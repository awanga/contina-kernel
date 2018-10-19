/*
 *  linux/drivers/watchdog/g2_wdt.c
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <mach/g2-wdt.h>
#include <mach/platform.h>

struct g2_wdt {
	unsigned long	timer_alive;
	struct device	*dev;
	void __iomem	*base;
	int		irq;
	unsigned int	perturb;
	char		expect_close;
	struct g2_wdt * __percpu *pwdt;
};

static struct platform_device *g2_wdt_dev;
static spinlock_t wdt_lock;
static unsigned int g2_timer_rate = (100*1024*1024);

#define TIMER_MARGIN	60
static int g2_margin = TIMER_MARGIN;
module_param(g2_margin, int, 0);
MODULE_PARM_DESC(g2_margin,
	"G2 timer margin in seconds. (0 < g2_margin < 65536, default="
	__MODULE_STRING(TIMER_MARGIN) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

#define ONLY_TESTING	0
static int g2_noboot = ONLY_TESTING;
module_param(g2_noboot, int, 0);
MODULE_PARM_DESC(g2_noboot, "G2 watchdog action, "
	"set to 1 to ignore reboots, 0 to reboot (default="
	__MODULE_STRING(ONLY_TESTING) ")");

static void g2_wdt_timeout_reset_all(void)
{
	unsigned int	reg_v;
	struct platform_clk clk;

	get_platform_clk(&clk);

	g2_timer_rate = clk.apb_clk;
	
        /* Reset all block & subsystem */
        reg_v = readl(GLOBAL_GLOBAL_CONFIG);
        
        /* enable axi & L2 reset */
        reg_v &= ~0x00000300;

        /* wd_enable are exclusive with wd0_reset_subsys_enable */
        reg_v &= ~0x0000000E;

        /* reset remap, all block & subsystem */
        reg_v |= 0x000000F0;
        
        writel(reg_v, GLOBAL_GLOBAL_CONFIG);

	return;	
}

/*
 *	This is the interrupt handler.  Note that we only use this
 *	in testing mode, so don't actually do a reboot here.
 */
static irqreturn_t g2_wdt_fire(int irq, void *arg)
{
	const struct g2_wdt *wdt = *(void **)arg;

	/* Check it really was our interrupt */
	if (readl(wdt->base + G2_WDOG_INTSTAT)) {
		dev_printk(KERN_CRIT, wdt->dev,
			"Triggered - Reboot ignored.\n");
		/* Clear the interrupt on the watchdog */
		writel(1, wdt->base + G2_WDOG_INTSTAT);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

/*
 *	g2_wdt_keepalive - reload the timer
 *
 *	Note that the spec says a DIFFERENT value must be written to the reload
 *	register each time.  The "perturb" variable deals with this by adding 1
 *	to the count every other time the function is called.
 */
static void g2_wdt_keepalive(struct g2_wdt *wdt)
{
	unsigned int count=0;

	/* printk("%s : cpu id = %d ......\n",__func__,get_cpu()); */

	/* Assume prescale is set to 256 */
	count = (g2_timer_rate / 256) * g2_margin;

	/* Reload the counter */
	writel(count + wdt->perturb, wdt->base + G2_WDOG_LOAD);
	wdt->perturb = wdt->perturb ? 0 : 1;
}

static void g2_wdt_stop(struct g2_wdt *wdt)
{

	spin_lock(&wdt_lock);
	/* switch from watchdog mode to timer mode */
	writel(0x12345678, wdt->base + G2_WDOG_DISABLE);
	writel(0x87654321, wdt->base + G2_WDOG_DISABLE);
	/* watchdog is disabled */
	writel(0x0, wdt->base + G2_WDOG_CONTROL);
	spin_unlock(&wdt_lock);
}

static void g2_wdt_start(struct g2_wdt *wdt)
{

	spin_lock(&wdt_lock);

	/* This loads the count register but does NOT start the count yet */
	g2_wdt_keepalive(wdt);

	if (g2_noboot) {
		/* Enable watchdog - prescale=256, watchdog mode=0, enable=1 */
		writel(0x0000FF01, wdt->base + G2_WDOG_CONTROL);
	} else {
		/* Enable watchdog - prescale=256, watchdog mode=1, enable=1 */
		writel(0x0000FF09, wdt->base + G2_WDOG_CONTROL);
	}
	spin_unlock(&wdt_lock);
}

static int g2_wdt_set_heartbeat(int t)
{

	if (t < 0x0001 || t > 0xFFFF)
		return -EINVAL;

	g2_margin = t;
	return 0;
}

/*
 *	/dev/watchdog handling
 */
static int g2_wdt_open(struct inode *inode, struct file *file)
{
	struct g2_wdt *wdt = platform_get_drvdata(g2_wdt_dev);


	if (test_and_set_bit(0, &wdt->timer_alive))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	file->private_data = wdt;

	/*
	 *	Activate timer
	 */
   	smp_call_function(g2_wdt_start, wdt, 1);
	g2_wdt_start(wdt);

	return nonseekable_open(inode, file);
}

static int g2_wdt_release(struct inode *inode, struct file *file)
{
	struct g2_wdt *wdt = file->private_data;

	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we set nowayout
	 */
	if (wdt->expect_close == 42) {
   		smp_call_function(g2_wdt_stop, wdt, 1);
		g2_wdt_stop(wdt);
	} else {
   		smp_call_function(g2_wdt_keepalive, wdt, 1);
		g2_wdt_keepalive(wdt);
	}
	clear_bit(0, &wdt->timer_alive);
	wdt->expect_close = 0;
	return 0;
}

static ssize_t g2_wdt_write(struct file *file, const char *data,
	size_t len, loff_t *ppos)
{
	struct g2_wdt *wdt = file->private_data;

	/*
	 *	Refresh the timer.
	 */
	if (len) {
		if (!nowayout) {
			size_t i;

			/* In case it was set long ago */
			wdt->expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;

				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					wdt->expect_close = 42;
			}
		}
   		smp_call_function(g2_wdt_keepalive, wdt, 1);
		g2_wdt_keepalive(wdt);
	}
	return len;
}

static struct watchdog_info ident = {
	.options		= WDIOF_SETTIMEOUT |
				  WDIOF_KEEPALIVEPING |
				  WDIOF_MAGICCLOSE,
	.identity		= "G2 Watchdog",
};

static long g2_wdt_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct g2_wdt *wdt = file->private_data;
	int ret;
	union {
		struct watchdog_info ident;
		int i;
	} uarg;


	if (_IOC_DIR(cmd) && _IOC_SIZE(cmd) > sizeof(uarg))
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = copy_from_user(&uarg, (void __user *)arg, _IOC_SIZE(cmd));
		if (ret)
			return -EFAULT;
	}

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		uarg.ident = ident;
		ret = 0;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		uarg.i = 0;
		ret = 0;
		break;

	case WDIOC_SETOPTIONS:
		ret = -EINVAL;
		if (uarg.i & WDIOS_DISABLECARD) {
        		smp_call_function(g2_wdt_stop, wdt, 1);
			g2_wdt_stop(wdt);
			ret = 0;
		}
		if (uarg.i & WDIOS_ENABLECARD) {
        		smp_call_function(g2_wdt_start, wdt, 1);
			g2_wdt_start(wdt);
			ret = 0;
		}
		break;

	case WDIOC_KEEPALIVE:
       		smp_call_function(g2_wdt_keepalive, wdt, 1);
        	g2_wdt_keepalive(wdt);
		ret = 0;
		break;

	case WDIOC_SETTIMEOUT:
		ret = g2_wdt_set_heartbeat(uarg.i);
		if (ret)
			break;

        	smp_call_function(g2_wdt_keepalive, wdt, 1);
		g2_wdt_keepalive(wdt);
		break;
		
	case WDIOC_GETTIMEOUT:
		uarg.i = g2_margin;
		ret = 0;
		break;

	default:
		return -ENOTTY;
	}

	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) {
		ret = copy_to_user((void __user *)arg, &uarg, _IOC_SIZE(cmd));
		if (ret)
			ret = -EFAULT;
	}
	return ret;
}

/*
 *	System shutdown handler.  Turn off the watchdog if we're
 *	restarting or halting the system.
 */
static void g2_wdt_shutdown(struct platform_device *dev)
{
	struct g2_wdt *wdt = platform_get_drvdata(dev);

	if (system_state == SYSTEM_RESTART || system_state == SYSTEM_HALT) {
        	smp_call_function(g2_wdt_stop, wdt, 1);
		g2_wdt_stop(wdt);
	}
}

/*
 *	Kernel Interfaces
 */
static const struct file_operations g2_wdt_fops = {
	.owner			= THIS_MODULE,
	.llseek			= no_llseek,
	.write			= g2_wdt_write,
	.unlocked_ioctl		= g2_wdt_ioctl,
	.open			= g2_wdt_open,
	.release		= g2_wdt_release,
};

static struct miscdevice g2_wdt_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &g2_wdt_fops,
};

static int __devinit g2_wdt_probe(struct platform_device *dev)
{
	struct g2_wdt * __percpu *pwdt;
	struct g2_wdt *wdt;
	struct resource *res;
	int ret;
	int i;

	/* We only accept one device, and it must have an id of -1 */
/*	if (dev->id != -1)
		return -ENODEV;
*/

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto err_out;
	}

	pwdt = alloc_percpu(struct g2_wdt *);
	if (!pwdt) {
		ret = -ENOMEM;
		goto err_out;
	}

	wdt = kzalloc(sizeof(struct g2_wdt), GFP_KERNEL);
	if (!wdt) {
		ret = -ENOMEM;
		goto err_free_percpu;
	}

	wdt->pwdt = pwdt;
	wdt->dev = &dev->dev;
	wdt->irq = platform_get_irq(dev, 0);
	if (wdt->irq < 0) {
		ret = -ENXIO;
		goto err_free;
	}
	wdt->base = ioremap(res->start, res->end - res->start + 1);
	if (!wdt->base) {
		ret = -ENOMEM;
		goto err_free;
	}

	g2_wdt_miscdev.parent = &dev->dev;
	ret = misc_register(&g2_wdt_miscdev);
	if (ret) {
		dev_err(&dev->dev, "cannot register miscdev on minor=%d (%d)\n",
			WATCHDOG_MINOR, ret);
		goto err_misc;
	}

	for_each_possible_cpu(i)
		*per_cpu_ptr(pwdt, i) = wdt;

	ret = request_percpu_irq(wdt->irq, g2_wdt_fire, "g2_wdt", pwdt);
	if (ret) {
		dev_err(&dev->dev,
			"cannot register IRQ%d for watchdog\n", wdt->irq);
		goto err_irq;
	}

	printk("%s: base address =%x  IRQ=%d...\n",__func__,
		(unsigned int)wdt->base,wdt->irq);

	g2_wdt_stop(wdt);
	platform_set_drvdata(dev, wdt);
	g2_wdt_dev = dev;

	return 0;

err_irq:
	misc_deregister(&g2_wdt_miscdev);
err_misc:
	iounmap(wdt->base);
err_free:
	kfree(wdt);
err_free_percpu:
	free_percpu(pwdt);
err_out:
	return ret;
}

static int __devexit g2_wdt_remove(struct platform_device *dev)
{
	struct g2_wdt *wdt = platform_get_drvdata(dev);
	struct g2_wdt * __percpu *pwdt = wdt->pwdt;

	platform_set_drvdata(dev, NULL);

	misc_deregister(&g2_wdt_miscdev);

	g2_wdt_dev = NULL;

	free_percpu_irq(wdt->irq, pwdt);
	iounmap(wdt->base);
	kfree(wdt);
	free_percpu(pwdt);
	return 0;
}

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:g2_wdt");

static struct platform_driver g2_wdt_driver = {
	.probe		= g2_wdt_probe,
	.remove		= __devexit_p(g2_wdt_remove),
	.shutdown	= g2_wdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "g2-wdt",
	},
};

static char banner[] __initdata = KERN_INFO "G2 Watchdog Timer: 0.1. "
		"g2_noboot=%d g2_margin=%d sec (nowayout= %d)\n";

static int __init g2_wdt_init(void)
{
	/*
	 * Check that the margin value is within it's range;
	 * if not reset to the default
	 */
	if (g2_wdt_set_heartbeat(g2_margin)) {
		g2_wdt_set_heartbeat(TIMER_MARGIN);
		printk(KERN_INFO "g2_margin value must be 0<g2_margin<65536, \
			 using %d\n",TIMER_MARGIN);
	}

	g2_wdt_timeout_reset_all();

	spin_lock_init(&wdt_lock);

	printk(banner, g2_noboot, g2_margin, nowayout);

	return platform_driver_register(&g2_wdt_driver);
}

static void __exit g2_wdt_exit(void)
{
	platform_driver_unregister(&g2_wdt_driver);
}

module_init(g2_wdt_init);
module_exit(g2_wdt_exit);

MODULE_AUTHOR("Cortina-Systems Limited");
MODULE_DESCRIPTION("G2 Watchdog Device Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
