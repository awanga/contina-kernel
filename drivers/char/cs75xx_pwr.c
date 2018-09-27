/*
 * FILE NAME cs75xx_pwr.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX Power Control device.
 *
 *  Copyright 2010 Cortina , Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/apm_bios.h>
#include <linux/pm.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>

struct cs75xx_pwr {
	struct device	*dev;
	void __iomem	*base;
	int		irq;
	spinlock_t	lock;
};

static struct platform_device *cs75xx_pwr_dev;
static unsigned long int_enable_time = 0;	/* power interrupt time */


#define cs75xx_pwr_read_reg(offset)		(readl(pwr->base+offset))
#define cs75xx_pwr_write_reg(offset, val)	(writel(val, pwr->base+offset))

/* Register Map */
#define	CS75XX_CIR_IP_ID		0x00
#define	CS75XX_CIR_INT_EN		0x10
#define	CS75XX_CIR_STATUS		0x0c

#define	CS75XX_PWR_CTRL0		0x24
#define	CS75XX_PWR_CTRL1		0x28
#define	CS75XX_PWR_INT_ST		0x2c
#define CS75XX_PWR_INT_EN		0x30

/* CS75XX_CIR_STATUS BIT */
#define	CIR_PWR_INT_CLEAR		BIT(0)

/* CS75XX_PWR_CTRL0 BIT */
#define	PWR_CTRL0_COUNT_MASK		0x00000003
#define	PWR_CTRL0_COUNT_MIN		3
#define	PWR_CTRL0_COUNT_MAX		6

/* CS75XX_PWR_CTRL1 BIT */
#define	PWR_CTRL1_SHUT_DOWN		BIT(0)
#define PWR_CTRL1_INIT_FINISH		BIT(1)
#define PWR_CTRL1_INT_CLEAR		BIT(2)

/* CS75XX_PWR_INT_ST, CS75XX_PWR_INT_EN BIT */
#define	PWR_INT_CIR			BIT(0)
#define	PWR_INT_RTC			BIT(1)
#define	PWR_INT_PUSH			BIT(2)


#define CS75XX_PWR_DEV_ID		((cs75xx_pwr_read_reg(CS75XX_CIR_IP_ID) & 0x00FFFF00) >> 8)
#define CS75XX_PWR_REV_ID		(cs75xx_pwr_read_reg(CS75XX_CIR_IP_ID) & 0x000000FF)
#define CS75XX_PWR_DEV_ID_VAL		0x000104


unsigned int Action = 0;
unsigned int pwr_src = 0;
wait_queue_head_t pwc_wait_q;

#ifndef PWR_MINOR
#define PWR_MINOR		241	/*  Documents/devices.txt suggest to use 240~255 for local driver!! */
#endif

#ifdef CONFIG_CORTINA_FPGA
extern void cs75xx_cir_power_on(void);
extern void cs75xx_cir_power_off(void);
#endif


static void cs75xx_pwr_power_off(void)
{
	struct cs75xx_pwr *pwr = platform_get_drvdata(cs75xx_pwr_dev);
	CIR_PWRCTRL_PWR_CTRL1_t reg_pwr_ctrl1;
	CIR_PWRCTRL_PWR_INT_ENABLE_t reg_pwr_inten;
	CIR_PWRCTRL_CIR_INT_ENABLE_t reg_cir_int_en;

	/* interval between interrupt and software shutdown must over 3 seconds */
	if (int_enable_time) {
		while ((long)jiffies - (long)int_enable_time < 0)
			mdelay(100);

		/* turn on Power Control power interrupt enable */
		reg_pwr_inten.wrd = 0;
		reg_pwr_inten.bf.cir_pwr_on_en = 1;
		reg_pwr_inten.bf.rtc_wake_en = 1;
		reg_pwr_inten.bf.push_btn_wake_en = 1;
		cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, reg_pwr_inten.wrd);

		/* turn on CIR power interrupt enable */
		reg_cir_int_en.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_INT_EN);
		reg_cir_int_en.bf.pwrkey_int_en = 1;
		cs75xx_pwr_write_reg(CS75XX_CIR_INT_EN, reg_cir_int_en.wrd);
	}

	reg_pwr_ctrl1.wrd = 0;
	reg_pwr_ctrl1.bf.swShutdnEn = 1;
	cs75xx_pwr_write_reg(CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);
}

static int cs75xx_pwr_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int cs75xx_pwr_release(struct inode *inode, struct file *file)
{
	return 0;
}

static irqreturn_t cs75xx_pwr_interrupt(int irq, void *dev_instance)
{
	struct cs75xx_pwr *pwr = (struct cs75xx_pwr *)dev_instance;
	CIR_PWRCTRL_CIR_INT_STATUS_t reg_cir_int;
	CIR_PWRCTRL_PWR_CTRL1_t reg_pwr_ctrl1;
	CIR_PWRCTRL_PWR_INT_ENABLE_t reg_pwr_inten;
	CIR_PWRCTRL_PWR_INT_STATUS_t reg_pwr_int;
	CIR_PWRCTRL_CIR_INT_ENABLE_t reg_cir_int_en;
	unsigned long flags;

	spin_lock_irqsave(&pwr->lock, flags);

#ifdef CONFIG_CORTINA_FPGA
	if (Action == POWEROFF) {
		dev_info(pwr->dev, "0x%08lX: POWER ON Power event by ", jiffies);
		/* power off to on --> take time to provide power */
		mdelay(3000);

		/* turn off CIR power interrupt enable */
		reg_cir_int_en.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_INT_EN);
		reg_cir_int_en.bf.pwrkey_int_en = 0;
		cs75xx_pwr_write_reg(CS75XX_CIR_INT_EN, reg_cir_int_en.wrd);

		/* turn off Power Control power interrupt enable */
		cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, 0);

		pwr_src = reg_pwr_int.wrd = cs75xx_pwr_read_reg(CS75XX_PWR_INT_ST);
		if (reg_pwr_int.bf.cir_pwr_on) {
			reg_cir_int.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_STATUS);
			reg_cir_int.bf.pwrkey_int_sts = 1;
			cs75xx_pwr_write_reg(CS75XX_CIR_STATUS, reg_cir_int.wrd);
			dev_info(pwr->dev, "CIR...\n");
		}
		else if (reg_pwr_int.bf.rtc_wake) {
			RTC_RTCIM_t reg_rtc_rtcim, tmp_rtcim;
			RTC_WKUPPEND_t reg_rtc_wkuppend;

			tmp_rtcim.wrd = reg_rtc_rtcim.wrd = readl(GOLDENGATE_RTC_BASE + 0x40);
			reg_rtc_wkuppend.wrd = readl(GOLDENGATE_RTC_BASE + 0x4C);

			reg_rtc_rtcim.bf.intmode = 0;
			writel(reg_rtc_rtcim.wrd, GOLDENGATE_RTC_BASE + 0x40);

			reg_rtc_wkuppend.bf.wkuppend = 0;
			writel(reg_rtc_wkuppend.wrd, GOLDENGATE_RTC_BASE + 0x4C);

			writel(tmp_rtcim.wrd, GOLDENGATE_RTC_BASE + 0x40);

			printk("RTC...\n");
		}
		else if (reg_pwr_int.bf.push_btn_wake) {
			dev_info(pwr->dev, "Button...\n");
		}
		else {
			dev_info(pwr->dev, "Unknow Source\n");
		}

		reg_pwr_ctrl1.wrd = 0;
		reg_pwr_ctrl1.bf.pwr_int_clear = 1;
		cs75xx_pwr_write_reg(CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

		reg_pwr_ctrl1.wrd = 0;
		reg_pwr_ctrl1.bf.sysInitFinish = 1;
		cs75xx_pwr_write_reg(CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

		/* turn on Power Control power interrupt enable */
		reg_pwr_inten.wrd = 0;
		reg_pwr_inten.bf.cir_pwr_on_en = 1;
		reg_pwr_inten.bf.rtc_wake_en = 1;
		reg_pwr_inten.bf.push_btn_wake_en = 1;
		cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, reg_pwr_inten.wrd);

		/* turn on CIR power interrupt enable */
		reg_cir_int_en.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_INT_EN);
		reg_cir_int_en.bf.pwrkey_int_en = 1;
		cs75xx_pwr_write_reg(CS75XX_CIR_INT_EN, reg_cir_int_en.wrd);

		cs75xx_cir_power_on();
		Action = 0;

		spin_unlock_irqrestore(&pwr->lock, flags);
		return IRQ_HANDLED;
	}
#endif
	dev_info(pwr->dev, "0x%08lx: Power event by ", jiffies);

	/* turn off CIR power interrupt enable */
	reg_cir_int_en.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_INT_EN);
	reg_cir_int_en.bf.pwrkey_int_en = 0;
	cs75xx_pwr_write_reg(CS75XX_CIR_INT_EN, reg_cir_int_en.wrd);

	/* turn off Power Control power interrupt enable */
	cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, 0);

	pwr_src = reg_pwr_int.wrd = cs75xx_pwr_read_reg(CS75XX_PWR_INT_ST);
	if (reg_pwr_int.bf.cir_pwr_on) {
		reg_cir_int.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_STATUS);
		reg_cir_int.bf.pwrkey_int_sts = 1;
		cs75xx_pwr_write_reg(CS75XX_CIR_STATUS, reg_cir_int.wrd);
		dev_info(pwr->dev, "CIR...\n");
	}
	else if (reg_pwr_int.bf.rtc_wake) {
		dev_info(pwr->dev, "RTC...\n");
	}
	else if (reg_pwr_int.bf.push_btn_wake) {
		dev_info(pwr->dev, "Button...\n");
	}
	else {
		dev_info(pwr->dev, "Unknow Source(0x%X)\n", reg_pwr_int.wrd);
	}

	reg_pwr_ctrl1.wrd = 0;
	reg_pwr_ctrl1.bf.pwr_int_clear = 1;
	cs75xx_pwr_write_reg(CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

	mdelay(1000);
	int_enable_time = jiffies + 2*HZ;

#if 0	/* not turn on interrrupt until software shutdown */
	/* turn on Power Control power interrupt enable */
	reg_pwr_inten.wrd = 0;
	reg_pwr_inten.bf.cir_pwr_on_en = 1;
	reg_pwr_inten.bf.rtc_wake_en = 1;
	reg_pwr_inten.bf.push_btn_wake_en = 1;
	cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, reg_pwr_inten.wrd);
#ifdef CONFIG_CORTINA_FPGA
	cs75xx_cir_power_on();
#endif

	/* turn on CIR power interrupt enable */
	reg_cir_int_en.wrd = cs75xx_pwr_read_reg(CS75XX_CIR_INT_EN);
	reg_cir_int_en.bf.pwrkey_int_en = 1;
	cs75xx_pwr_write_reg(CS75XX_CIR_INT_EN, reg_cir_int_en.wrd);
#endif

	Action = POWEROFF;

	wmb();

	wake_up(&pwc_wait_q);
	Action = POWEROFF;

#ifdef CONFIG_CORTINA_FPGA
	cs75xx_cir_power_off();
	cs75xx_pwr_power_off();
#endif

	spin_unlock_irqrestore(&pwr->lock, flags);

	return IRQ_HANDLED;
}

static int cs75xx_pwr_wait(void)
{
	interruptible_sleep_on(&pwc_wait_q);

	return pwr_src;
}

static int cs75xx_pwr_set_time(unsigned int sec)
{
	struct cs75xx_pwr *pwr = platform_get_drvdata(cs75xx_pwr_dev);
	CIR_PWRCTRL_PWR_CTRL0_t reg_ctrl0;

	if (sec < PWR_CTRL0_COUNT_MIN || sec > PWR_CTRL0_COUNT_MAX)
		return -EINVAL;

	sec -= PWR_CTRL0_COUNT_MIN;

	reg_ctrl0.wrd = cs75xx_pwr_read_reg(CS75XX_PWR_CTRL0);
	reg_ctrl0.bf.shut_dn_count = sec;
	cs75xx_pwr_write_reg(CS75XX_PWR_CTRL0, reg_ctrl0.wrd);

	return 0;
}

static int cs75xx_pwr_get_time(unsigned int *sec_p)
{
	struct cs75xx_pwr *pwr = platform_get_drvdata(cs75xx_pwr_dev);
	CIR_PWRCTRL_PWR_CTRL0_t reg_ctrl0;

	reg_ctrl0.wrd = cs75xx_pwr_read_reg(CS75XX_PWR_CTRL0);
	*sec_p = reg_ctrl0.bf.shut_dn_count + PWR_CTRL0_COUNT_MIN;

	return 0;
}

static long cs75xx_pwr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct pwc_ioctl_data data;

	switch (cmd) {
	case PWC_SET_SHUT_TIME:
		if (copy_from_user(&data, (struct pwc_ioctl_data *)arg, sizeof(data)))
			return -EFAULT;
		cs75xx_pwr_set_time(data.data);
		break;
	case PWC_GET_SHUT_TIME:
		cs75xx_pwr_get_time(&data.data);
		if (copy_to_user((struct pwc_ioctl_data *)arg, &data, sizeof(data)))
			return -EFAULT;
		break;
	case PWC_WAIT_BTN:
		if (Action == POWEROFF)		/* power button was pressed during booting time */
			ret = pwr_src ;
		else
			ret = cs75xx_pwr_wait();	/* Waiting Power Interrupt */

		data.data = ret ;
		data.action = Action;

		if (copy_to_user((struct pwc_ioctl_data *)arg, &data, sizeof(data)))
			return -EFAULT;
		break;
	case PWC_SHUTDOWN:
		/* reset SDRAM control to hald system */
		writel(0x10, 0xF0000004);
		break;
	default:
		return -EPERM;
	}

	return 0;
}

static struct file_operations cs75xx_pwr_fops =
{
	.owner          = THIS_MODULE,
	.open           = cs75xx_pwr_open,
	.release        = cs75xx_pwr_release,
	.unlocked_ioctl = cs75xx_pwr_ioctl,
};

static struct miscdevice cs75xx_pwr_miscdev =
{
	PWR_MINOR,
	"cs75xx-pwr",
	&cs75xx_pwr_fops
};

static int __devinit cs75xx_pwr_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct cs75xx_pwr *pwr = NULL;
	struct resource *res_mem;
	CIR_PWRCTRL_PWR_CTRL1_t reg_pwr_ctrl1;
	CIR_PWRCTRL_PWR_INT_ENABLE_t reg_pwr_inten;

	dev_info(&pdev->dev, "Function: %s, pdev->name = %s\n", __func__, pdev->name);
	printk(KERN_DEBUG "Function: %s, pdev->name = %s\n", __func__, pdev->name);

	pwr = kzalloc(sizeof(struct cs75xx_pwr), GFP_KERNEL);
	if (!pwr) {
		dev_err(&pdev->dev, "\nFunc: %s - can't allocate memory for %s device\n", __func__, "pwr");
		rc = -ENOMEM;
		goto fail;
	}
	pwr->dev = &pdev->dev;

	/* get the module base address */
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, "cir");
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "cir");
		goto fail;
	}
	pwr->base = ioremap(res_mem->start, resource_size(res_mem));
	if (!pwr->base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
		            __func__, "pwr", resource_size(res_mem));
		goto fail;
	}
	dev_info(&pdev->dev, "\tcir_base = 0x%08x, range = %d\n", (u32)pwr->base, resource_size(res_mem));

	pwr->irq = platform_get_irq_byname(pdev, "irq_pwr");
	if (pwr->irq == -ENXIO) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "irq_pwr");
		goto fail;
	}
	dev_info(&pdev->dev, "\tirq_pwr = %d\n", pwr->irq);

	/* init */
#ifndef CONFIG_CORTINA_FPGA
	/* fix bug# 27944 The "flash-NOR-USB-device-LCD888-PCIe-SATA-SPDIF-DAC_EXT-SMP-L2-UBOOT.bin"
	   image cannot boot to kernel. */
	dev_info(&pdev->dev, "CIR/PWC Device ID: %06x, rev: %02x\n", CS75XX_PWR_DEV_ID, CS75XX_PWR_REV_ID);

	if (CS75XX_PWR_DEV_ID != CS75XX_PWR_DEV_ID_VAL) {
		dev_err(&pdev->dev, "CS75XX CIR and PWC Module Not Found!!\n");
		return -ENODEV;
	}
#endif

	spin_lock_init(&pwr->lock);

	printk("Cortina CS75XX PWC Initialization\n");

	init_waitqueue_head(&pwc_wait_q);

	/* turn off Power Control power interrupt enable */
	reg_pwr_inten.wrd = 0;
	cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, reg_pwr_inten.wrd);

	/* clear interrupt */
	reg_pwr_ctrl1.wrd = 0;
	reg_pwr_ctrl1.bf.pwr_int_clear = 1;
	cs75xx_pwr_write_reg(CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

	reg_pwr_ctrl1.wrd = 0;
	reg_pwr_ctrl1.bf.sysInitFinish = 1;
	cs75xx_pwr_write_reg(CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

	/* interrupt signal is 32.768K clock domain, APB bus is 150Mhz
	   clock domain, interrupt signal must be sync from 32.768K -->
	   24Mhz --> 150Mhz domain, so it will take sometime to after sync. */
	mdelay(1);

	if ((rc = request_irq(pwr->irq, cs75xx_pwr_interrupt, IRQF_DISABLED, "Power Control", pwr)) != 0) {
		dev_err(&pdev->dev, "Error: Register IRQ for CS75XX Power Control failed %d\n", rc);
		goto fail;
	}

	/* turn on Power Control power interrupt enable */
	reg_pwr_inten.wrd = 0;
	reg_pwr_inten.bf.cir_pwr_on_en = 1;
	reg_pwr_inten.bf.rtc_wake_en = 1;
	reg_pwr_inten.bf.push_btn_wake_en = 1;
	cs75xx_pwr_write_reg(CS75XX_PWR_INT_EN, reg_pwr_inten.wrd);

	pm_power_off = cs75xx_pwr_power_off;
	misc_register(&cs75xx_pwr_miscdev);

	platform_set_drvdata(pdev, pwr);
	cs75xx_pwr_dev = pdev;

	Action = 0;
#ifdef CONFIG_CORTINA_FPGA
	Action = POWEROFF;
#endif

	return 0;

fail:
	if (pwr) {
		if (pwr->base)
			iounmap(pwr->base);
		kfree(pwr);
	}

	return rc;
}

static int __devexit cs75xx_pwr_remove(struct platform_device *pdev)
{
	struct cs75xx_pwr *pwr = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Function: %s, pdev->name = %s\n", __func__, pdev->name);

	platform_set_drvdata(pdev, NULL);

	misc_deregister(&cs75xx_pwr_miscdev);

	cs75xx_pwr_dev = NULL;

	free_irq(pwr->irq, pwr);
	iounmap(pwr->base);
	kfree(pwr);

	return 0;
}

static struct platform_driver cs75xx_pwr_platform_driver = {
	.probe	= cs75xx_pwr_probe,
	.remove	= __devexit_p(cs75xx_pwr_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name  = "cs75xx-pwr",
	},
};

static int __init cs75xx_pwr_init(void)
{
	printk("\n%s\n", __func__);

	return platform_driver_register(&cs75xx_pwr_platform_driver);
}

static void __exit cs75xx_pwr_exit(void)
{
	printk("\n%s\n", __func__);

	platform_driver_unregister(&cs75xx_pwr_platform_driver);
}

module_init(cs75xx_pwr_init);
module_exit(cs75xx_pwr_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX Power Control driver");

