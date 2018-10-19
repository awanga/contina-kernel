/*
 *  linux/drivers/rtc/rtc-g2.c
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Amos Lee <amos.lee@cortina-systems.com>
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
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/log2.h>

#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/rtc-g2.h>
#include <mach/registers.h>
#include <linux/slab.h>

static struct resource *g2_rtc_mem;

static void __iomem *g2_rtc_base;
static int g2_rtc_alarmno = NO_IRQ;
static int g2_rtc_tickno  = NO_IRQ;

static DEFINE_SPINLOCK(g2_rtc_pie_lock);

static int g2_rtc_readl(unsigned int addr)
{
	unsigned int	reg_val=0;

	reg_val = readl(g2_rtc_base + addr);
	return (reg_val);
}

static char g2_rtc_readb(unsigned int addr)
{
	unsigned int	reg_val=0;

	reg_val = readl(g2_rtc_base + addr) & 0x000000ff;
	return (reg_val);
}

static int g2_rtc_writel(unsigned int addr, unsigned int val, unsigned int bitmask)
{
	unsigned int	reg_val;

	reg_val = (readl(g2_rtc_base + addr) & (~bitmask) ) | (val & bitmask);
	writel(reg_val, g2_rtc_base+addr);
	return (0);
}

static int g2_rtc_write_enable(void)
{
	unsigned int	tmp;

	tmp = g2_rtc_readb(G2_RTC_RTCCON);
	tmp = tmp | G2_RTC_RTCCON_RTCEN | G2_RTC_RTCCON_STARTB;
	g2_rtc_writel(G2_RTC_RTCCON, tmp, 0xff);

	return 0;
}

static int g2_rtc_write_disable(void)
{
	unsigned int	tmp;

	tmp = g2_rtc_readb(G2_RTC_RTCCON);
	tmp = tmp & (~G2_RTC_RTCCON_STARTB) & (~G2_RTC_RTCCON_RTCEN);
	g2_rtc_writel(G2_RTC_RTCCON, tmp, 0xff);

	return 0;
}

/* IRQ Handlers */
static irqreturn_t g2_rtc_alarmirq(int irq, void *id)
{
	struct rtc_device *rdev = id;

	printk("\n Alarm IRQ...\n");

	g2_rtc_writel(G2_RTC_RTCPEND,0,0x000000ff);
	rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);

	return IRQ_HANDLED;
}

static irqreturn_t g2_rtc_tickirq(int irq, void *id)
{
	struct rtc_device *rdev = id;

	g2_rtc_writel(G2_RTC_PRIPEND,0,0x000000ff);
	rtc_update_irq(rdev, 1, RTC_PF | RTC_IRQF);

	return IRQ_HANDLED;
}

/* Update control registers */
static void g2_rtc_setaie(int to)
{
	unsigned int tmp;

	pr_debug("%s: aie=%d\n", __func__, to);

	g2_rtc_write_enable();

	tmp = g2_rtc_readl(G2_RTC_RTCALM);

	if (to)
		tmp |= G2_RTC_RTCALM_ALMEN;
	else
		tmp &= ~G2_RTC_RTCALM_ALMEN;

	g2_rtc_writel(G2_RTC_RTCALM,tmp,0x000000ff);

	g2_rtc_write_disable();

	pr_debug("\n%s : Setting G2_RTC_RTCALM to %08x\n",__func__, tmp);

	return;
}

/* Time read/write */
static int g2_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;

retry_get_time:
	rtc_tm->tm_min  = g2_rtc_readl(G2_RTC_RTCMIN);
	rtc_tm->tm_hour = g2_rtc_readl(G2_RTC_RTCHOUR);
	rtc_tm->tm_mday = g2_rtc_readl(G2_RTC_RTCDATE);
	rtc_tm->tm_mon  = g2_rtc_readl(G2_RTC_RTCMON);
	rtc_tm->tm_year = g2_rtc_readl(G2_RTC_RTCYEAR);
	rtc_tm->tm_sec  = g2_rtc_readl(G2_RTC_RTCSEC);

	/* the only way to work out wether the system was mid-update
	 * when we read it is to check the second counter, and if it
	 * is zero, then we re-try the entire read
	 */

	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_get_time;
	}

	pr_debug("read time %02x.%02x.%02x %02x/%02x/%02x\n",
		 rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		 rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	rtc_tm->tm_sec = bcd2bin(rtc_tm->tm_sec);
	rtc_tm->tm_min = bcd2bin(rtc_tm->tm_min);
	rtc_tm->tm_hour = bcd2bin(rtc_tm->tm_hour);
	rtc_tm->tm_mday = bcd2bin(rtc_tm->tm_mday);
	rtc_tm->tm_mon = bcd2bin(rtc_tm->tm_mon);
	rtc_tm->tm_year = bcd2bin(rtc_tm->tm_year & 0xff) + (bcd2bin(rtc_tm->tm_year >> 8) * 100);

	/* epoch == 1900 */
	rtc_tm->tm_year += 100;
	rtc_tm->tm_mon -= 1;

	return 0;
}

static int g2_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	unsigned int 	year = tm->tm_year - 100;

	pr_debug("set time %02d.%02d.%02d %02d/%02d/%02d\n",
		 tm->tm_year, tm->tm_mon, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* we get around y2k by simply not supporting it */

	if (year < 0 || year >= 100) {
		dev_err(dev, "rtc only supports 100 years\n");
		return -EINVAL;
	}

	g2_rtc_write_enable();

	g2_rtc_writel(G2_RTC_RTCSEC, bin2bcd(tm->tm_sec), 0xffffffff);
	g2_rtc_writel(G2_RTC_RTCMIN, bin2bcd(tm->tm_min), 0xffffffff);
	g2_rtc_writel(G2_RTC_RTCHOUR, bin2bcd(tm->tm_hour), 0xffffffff);
	g2_rtc_writel(G2_RTC_RTCDATE, bin2bcd(tm->tm_mday), 0xffffffff);
	g2_rtc_writel(G2_RTC_RTCMON, bin2bcd(tm->tm_mon + 1), 0xffffffff);
	g2_rtc_writel(G2_RTC_RTCYEAR, (bin2bcd(year/100)<<8) + bin2bcd(year%100), 0xffffffff);

	g2_rtc_write_disable();

	return 0;
}

static int g2_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alm_tm = &alrm->time;
	unsigned int alm_en;

	g2_rtc_write_enable();

	alm_tm->tm_sec  = g2_rtc_readb(G2_RTC_ALMSEC);
	alm_tm->tm_min  = g2_rtc_readb(G2_RTC_ALMMIN);
	alm_tm->tm_hour = g2_rtc_readb(G2_RTC_ALMHOUR);
	alm_tm->tm_mon  = g2_rtc_readb(G2_RTC_ALMMON);
	alm_tm->tm_mday = g2_rtc_readb(G2_RTC_ALMDATE);
	alm_tm->tm_year = g2_rtc_readb(G2_RTC_ALMYEAR);

	alm_en = g2_rtc_readb(G2_RTC_RTCALM);

	g2_rtc_write_disable();

	alrm->enabled = (alm_en & G2_RTC_RTCALM_ALMEN) ? 1 : 0;

	pr_debug("read alarm %02x %02x.%02x.%02x %02x/%02x/%02x\n",
		 alm_en,
		 alm_tm->tm_year, alm_tm->tm_mon, alm_tm->tm_mday,
		 alm_tm->tm_hour, alm_tm->tm_min, alm_tm->tm_sec);


	/* decode the alarm enable field */

	if (alm_en & G2_RTC_RTCALM_SECEN) {
		alm_tm->tm_sec = bcd2bin(alm_tm->tm_sec);
	} else {
		alm_tm->tm_sec = 0xff;
	}

	if (alm_en & G2_RTC_RTCALM_MINEN) {
		alm_tm->tm_min = bcd2bin(alm_tm->tm_min);
	} else {
		alm_tm->tm_min = 0xff;
	}

	if (alm_en & G2_RTC_RTCALM_HOUREN) {
		alm_tm->tm_hour = bcd2bin(alm_tm->tm_hour);
	} else {
		alm_tm->tm_hour = 0xff;
	}

	if (alm_en & G2_RTC_RTCALM_DAYEN) {
		alm_tm->tm_mday = bcd2bin(alm_tm->tm_mday);
	} else {
		alm_tm->tm_mday = 0xff;
	}

	if (alm_en & G2_RTC_RTCALM_MONEN) {
		alm_tm->tm_mon = bcd2bin(alm_tm->tm_mon);
		alm_tm->tm_mon -= 1;
	} else {
		alm_tm->tm_mon = 0xff;
	}

	if (alm_en & G2_RTC_RTCALM_YEAREN) {
		alm_tm->tm_year = bcd2bin(alm_tm->tm_year & 0xff) + (bcd2bin(alm_tm->tm_year >> 8) * 100);
	} else {
		alm_tm->tm_year = 0xffff;
	}

	return 0;
}

static int g2_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *tm = &alrm->time;
	unsigned int alrm_en;

	g2_rtc_write_enable();

	pr_debug("g2_rtc_setalarm: %d, %02x/%02x/%02x %02x.%02x.%02x\n",
		 alrm->enabled,
		 tm->tm_mday & 0xff, tm->tm_mon & 0xff, tm->tm_year & 0xff,
		 tm->tm_hour & 0xff, tm->tm_min & 0xff, tm->tm_sec);

	/* get RTC ALARM GLOBAL enable bit */
	alrm_en = g2_rtc_readb(G2_RTC_RTCALM) & G2_RTC_RTCALM_ALMEN;

	/* disable RTC alarm */
	g2_rtc_writel(G2_RTC_RTCALM, 0x00, 0xff);

	if (tm->tm_sec < 60 && tm->tm_sec >= 0) {
		alrm_en |= G2_RTC_RTCALM_SECEN;
		g2_rtc_writel(G2_RTC_ALMSEC, bin2bcd(tm->tm_sec), 0xff);
	}

	if (tm->tm_min < 60 && tm->tm_min >= 0) {
		alrm_en |= G2_RTC_RTCALM_MINEN;
		g2_rtc_writel(G2_RTC_ALMMIN, bin2bcd(tm->tm_min), 0xff);
	}

	if (tm->tm_hour < 24 && tm->tm_hour >= 0) {
		alrm_en |= G2_RTC_RTCALM_HOUREN;
		g2_rtc_writel(G2_RTC_ALMHOUR, bin2bcd(tm->tm_hour), 0xff);
	}

	alrm_en = (alrm->enabled ?
		  (alrm_en | G2_RTC_RTCALM_ALMEN) :
		  (alrm_en & ~G2_RTC_RTCALM_ALMEN));

	printk("\n%s : Setting G2_RTC_RTCALM to %08x\n",__func__, alrm_en);

	g2_rtc_writel(G2_RTC_RTCALM, alrm_en, 0xff);

	alrm_en = g2_rtc_readb(G2_RTC_RTCALM);
	printk("\n%s : Setting G2_RTC_RTCALM to %08x\n",__func__, alrm_en);

	g2_rtc_write_disable();

	g2_rtc_setaie(alrm->enabled);

	return 0;
}

static int g2_rtc_setalarmirq(struct device *dev, unsigned int enabled)
{
	g2_rtc_setaie(enabled);
	return 0;
}

/*
static int g2_rtc_proc(struct device *dev, struct seq_file *seq)
{
	unsigned int ticnt = readb(g2_rtc_base + G2_RTC_TICNT);

	seq_printf(seq, "periodic_IRQ\t: %s\n",
		     (ticnt & G2_RTC_TICNT_ENABLE) ? "yes" : "no" );
	return 0;
}
*/

static int g2_rtc_open(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_device *rtc_dev = platform_get_drvdata(pdev);
	int ret;

	ret = request_irq(g2_rtc_alarmno, g2_rtc_alarmirq,
			  IRQF_DISABLED,  "g2-rtc alarm", rtc_dev);

	if (ret) {
		dev_err(dev, "IRQ%d error %d\n", g2_rtc_alarmno, ret);
		return ret;
	}

	ret = request_irq(g2_rtc_tickno, g2_rtc_tickirq,
			  IRQF_DISABLED,  "g2-rtc tick", rtc_dev);

	if (ret) {
		dev_err(dev, "IRQ%d error %d\n", g2_rtc_tickno, ret);
		goto tick_err;
	}

	return ret;

tick_err:
	free_irq(g2_rtc_alarmno, rtc_dev);
	return ret;
}

static void g2_rtc_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_device *rtc_dev = platform_get_drvdata(pdev);

	/* do not clear AIE here, it may be needed for wake */
	free_irq(g2_rtc_alarmno, rtc_dev);
	free_irq(g2_rtc_tickno, rtc_dev);
}

static const struct rtc_class_ops g2_rtcops = {
	.open			= g2_rtc_open,
	.release		= g2_rtc_release,
	.read_time		= g2_rtc_gettime,
	.set_time		= g2_rtc_settime,
	.read_alarm		= g2_rtc_getalarm,
	.set_alarm		= g2_rtc_setalarm,
	.alarm_irq_enable 	= g2_rtc_setalarmirq,
	/*.proc	        	= g2_rtc_proc, */
};

static void g2_rtc_enable(struct platform_device *pdev, int en)
{
	unsigned int tmp;

	if (!en) {
		tmp = g2_rtc_readb(G2_RTC_RTCCON);
		g2_rtc_writel(G2_RTC_RTCCON, tmp & ~G2_RTC_RTCCON_RTCEN, 0xff);

/*		tmp = g2_rtc_readb(G2_RTC_TICNT);
		g2_rtc_writel(G2_RTC_TICNT, tmp & ~G2_RTC_TICNT_ENABLE, 0xff);
*/
	} else {
		/* re-enable the device, and check it is ok */

		if ((g2_rtc_readb(G2_RTC_RTCCON) & G2_RTC_RTCCON_RTCEN) == 0){
			dev_info(&pdev->dev, "rtc disabled, re-enabling\n");

			tmp = g2_rtc_readb(G2_RTC_RTCCON);
			g2_rtc_writel(G2_RTC_RTCCON, tmp | G2_RTC_RTCCON_RTCEN, 0xff);
		}

/*		if ((g2_rtc_readb(G2_RTC_RTCCON) & G2_RTC_RTCCON_CNTSEL)){
			dev_info(&pdev->dev, "removing RTCCON_CNTSEL\n");

			tmp = g2_rtc_readb(G2_RTC_RTCCON);
			g2_rtc_writel(G2_RTC_RTCCON, tmp & ~G2_RTC_RTCCON_CNTSEL, 0xff);
		}
*/
		if ((g2_rtc_readb(G2_RTC_RTCCON) & G2_RTC_RTCCON_CLKRST)){
			dev_info(&pdev->dev, "removing RTCCON_CLKRST\n");

			tmp = g2_rtc_readb(G2_RTC_RTCCON);
			g2_rtc_writel(G2_RTC_RTCCON, tmp & ~G2_RTC_RTCCON_CLKRST, 0xff);
		}

		g2_rtc_writel(G2_RTC_WKUPPEND, 0, 0xff);
		g2_rtc_writel(G2_RTC_RTCIM, 0x43, 0xff);
	}
}

static int __devexit g2_rtc_remove(struct platform_device *dev)
{
	struct rtc_device *rtc = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);
	rtc_device_unregister(rtc);

	g2_rtc_setaie(0);

	iounmap(g2_rtc_base);
	release_resource(g2_rtc_mem);
	kfree(g2_rtc_mem);

	return 0;
}

static int __devinit g2_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct resource *res;
	int ret;

	pr_debug("%s: probe=%p\n", __func__, pdev);

	/* find the IRQs */
	g2_rtc_tickno = platform_get_irq(pdev, 1);
	if (g2_rtc_tickno < 0) {
		dev_err(&pdev->dev, "no irq for rtc tick\n");
		return -ENOENT;
	}

	g2_rtc_alarmno = platform_get_irq(pdev, 0);
	if (g2_rtc_alarmno < 0) {
		dev_err(&pdev->dev, "no irq for alarm\n");
		return -ENOENT;
	}

	pr_debug("g2_rtc: tick irq %d, alarm irq %d\n",
		 g2_rtc_tickno, g2_rtc_alarmno);

	/* get the memory region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		return -ENOENT;
	}

	g2_rtc_mem = request_mem_region(res->start,
					 res->end-res->start+1,
					 pdev->name);

	if (g2_rtc_mem == NULL) {
		dev_err(&pdev->dev, "failed to reserve memory region\n");
		ret = -ENOENT;
		goto err_nores;
	}
	printk("%s: Physcial base address = %x ",__func__,res->start);

	g2_rtc_base = ioremap(res->start, res->end - res->start + 1);
	if (g2_rtc_base == NULL) {
		dev_err(&pdev->dev, "failed ioremap()\n");
		ret = -EINVAL;
		goto err_nomap;
	}
	printk("%s: Virtual base address =%x  Alarm IRQ=%d...\n",	\
		__func__,(unsigned int)g2_rtc_base,g2_rtc_alarmno);

	/* check to see if everything is setup correctly */
	g2_rtc_enable(pdev, 1);

	device_init_wakeup(&pdev->dev, 1);

	/* register RTC and exit */
	rtc = rtc_device_register("g2-rtc", &pdev->dev, &g2_rtcops, THIS_MODULE);

	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "cannot attach rtc\n");
		ret = PTR_ERR(rtc);
		goto err_nortc;
	}

	rtc->max_user_freq = 128;

	platform_set_drvdata(pdev, rtc);
	return 0;

 err_nortc:
	g2_rtc_enable(pdev, 0);
	iounmap(g2_rtc_base);

 err_nomap:
	release_resource(g2_rtc_mem);

 err_nores:
	return ret;
}

#ifdef CONFIG_PM

/* RTC Power management control */
#if DEBUG
static int ticnt_save;

static int g2_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* save TICNT for anyone using periodic interrupts */
	ticnt_save = readb(g2_rtc_base + G2_RTC_TICNT);
	g2_rtc_enable(pdev, 0);
	return 0;
}

static int g2_rtc_resume(struct platform_device *pdev)
{
	g2_rtc_enable(pdev, 1);
	writeb(ticnt_save, g2_rtc_base + G2_RTC_TICNT);
	return 0;
}
#endif
#else
#define g2_rtc_suspend NULL
#define g2_rtc_resume  NULL
#endif

static struct platform_driver g2_rtc_driver = {
	.probe		= g2_rtc_probe,
	.remove		= __devexit_p(g2_rtc_remove),
/*	.suspend	= g2_rtc_suspend, */
/*	.resume		= g2_rtc_resume,  */
	.driver		= {
		.name	= "g2-rtc",
		.owner	= THIS_MODULE,
	},
};

static char __initdata banner[] = "CS-XXXX RTC, (c) 2010 Cortina System\n";

static int __init g2_rtc_init(void)
{
	printk(banner);
	return platform_driver_register(&g2_rtc_driver);
}

static void __exit g2_rtc_exit(void)
{
	platform_driver_unregister(&g2_rtc_driver);
}

module_init(g2_rtc_init);
module_exit(g2_rtc_exit);

MODULE_DESCRIPTION("Cortina System RTC Driver");
MODULE_AUTHOR("<amos.lee@cortina-system.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc-g2");
