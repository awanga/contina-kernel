/*
 *  linux/arch/arm/mach-goldengate/core.c
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Jason Li <jason.li@cortina-systems.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <linux/smsc911x.h>
#include <linux/ata_platform.h>
#include <linux/amba/mmci.h>
#include <linux/export.h>

#include <asm/clkdev.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware/icst.h>

#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <mach/cs_cpu.h>
#include <mach/platform.h>
#include <mach/irqs.h>

#include "core.h"
#include "clock.h"

/*
 * Where is the timer (VA)?
 */
void __iomem *timer0_va_base;
void __iomem *timer1_va_base;

/*
 *  These are useconds NOT ticks.
 *
 */
#define mSEC_1		1000
#define mSEC_5		(mSEC_1 * 5)
#define mSEC_10		(mSEC_1 * 10)
#define mSEC_25		(mSEC_1 * 25)
#define SEC_1		(mSEC_1 * 1000)

/*
 * How long is the timer interval?
 */
#if CONFIG_HZ == 100
#define TIMER_INTERVAL	mSEC_10
#elif CONFIG_HZ == 1000
#define TIMER_INTERVAL  mSEC_1
#else
#error "Unsupported CONFIG_HZ value"
#endif

//#if (TIMER_INTERVAL >= 100000)
//#define TIMER_RELOAD  (TIMER_INTERVAL >> 10)
//#define TIMER_DIVISOR (CTRL_CLKSEL_DV_1024)
//#define TICKS2USECS(x)        (1024 * (x) / TICKS_PER_uSEC)
//#elif (TIMER_INTERVAL >= 10000)
//#define TIMER_RELOAD  (TIMER_INTERVAL>>6)
//#define TIMER_DIVISOR (CTRL_CLKSEL_DV_1024)
//#define TICKS2USECS(x)        (64 * (x) / TICKS_PER_uSEC)
//#else
#define TIMER_RELOAD	(TIMER_INTERVAL)
#define TIMER_DIVISOR	(CTRL_CLKSEL_DIRECT)
#define TICKS2USECS(x)	((x) / TICKS_PER_uSEC)
//#endif

struct platform_clk sys_clk_info;
static int got_clk_info = 0;
static irqreturn_t goldengate_timer_interrupt(int irq, void *dev_id);
static struct irqaction goldengate_timer_irq = {
	.name = "GoldenGate Timer Tick",
	.flags = IRQF_TIMER | IRQF_IRQPOLL,
	.handler = goldengate_timer_interrupt,
};

#if 0
//static
void timer_set_mode(enum clock_event_mode mode,
			   struct clock_event_device *clk)
{
	unsigned long ctrl;
	unsigned int reload;
	static int remove_timer_irq = 0;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		reload = (sys_clk_info.apb_clk / 1000000) * TIMER_INTERVAL;
		writel(reload, IO_ADDRESS(PER_TMR_LD1));
		ctrl = GPT_CTRL_RLMODE | GPT_CTRL_EN | TIMER_DIVISOR;
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		ctrl = GPT_CTRL_EN | TIMER_DIVISOR;
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		ctrl = 0;
		break;
	}

	writel(0x2, IO_ADDRESS(PER_TMR_LOADE));
	writel(ctrl, IO_ADDRESS(PER_TMR_CTRL1));
	writel(GPT_IE_EN, IO_ADDRESS(PER_TMR_IE1_0));	//Enable interrupt
}
#endif

//static
int timer_set_next_event(unsigned long evt,
				struct clock_event_device *unused)
{
	unsigned int ctrl = readl(IO_ADDRESS(PER_TMR_CTRL1));

	writel(evt, IO_ADDRESS(PER_TMR_LD1));
	writel(ctrl | GPT_CTRL_EN, IO_ADDRESS(PER_TMR_CTRL1));
	writel(0x2, IO_ADDRESS(PER_TMR_LOADE));
	return 0;
}

const cpumask_t cpu_0_mask = {CPU_BITS_CPU0};

static struct clock_event_device timer0_clockevent = {
	.name = "timer0",
	.shift = 32,
	.features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event = timer_set_next_event,
	.rating = 200,
	//.cpumask              = &cpu_all_mask,
	.cpumask = &cpu_0_mask,
};

static void __init goldengate_clockevents_init(unsigned int timer_irq)
{
	timer0_clockevent.irq = timer_irq;
	timer0_clockevent.mult =
	    div_sc(1000000, NSEC_PER_SEC, timer0_clockevent.shift);
	timer0_clockevent.max_delta_ns =
	    clockevent_delta2ns(0xFFFFFFFF, &timer0_clockevent);
	timer0_clockevent.min_delta_ns =
	    clockevent_delta2ns(0xF, &timer0_clockevent);

	clockevents_register_device(&timer0_clockevent);
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t goldengate_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &timer0_clockevent;

	/* clear the interrupt */
	writel(0x1, IO_ADDRESS(PER_TMR_INT1_0));

	evt->event_handler(evt);

	return IRQ_HANDLED;
}


static cycle_t goldengate_get_timer1_cycles(struct clocksource *cs)
{
	return ~readl(IO_ADDRESS(PER_TMR_CNT2));
}

static int goldengate_enable_timer1(struct clocksource *cs)
{
	unsigned int val;

	writel(0xFFFFFFFF, IO_ADDRESS(PER_TMR_LD2));
	writel(UPDATE_GPT1_LDVAL, IO_ADDRESS(PER_TMR_LOADE));
	val = readl(IO_ADDRESS(PER_TMR_CTRL2));
	val |= GPT_CTRL_EN | GPT_CTRL_RLMODE;
	writel(val, IO_ADDRESS(PER_TMR_CTRL2));

	return 0;
}

static void goldengate_disable_timer1(struct clocksource *cs)
{
	unsigned int val;

	writel(0xFFFFFFFF, IO_ADDRESS(PER_TMR_LD2));
	writel(UPDATE_GPT1_LDVAL, IO_ADDRESS(PER_TMR_LOADE));
	val = readl(IO_ADDRESS(PER_TMR_CTRL2));
	val &= ~GPT_CTRL_EN;
	writel(val, IO_ADDRESS(PER_TMR_CTRL2));

}

static struct clocksource clocksource_goldengate = {
	.name = "timer1",
	.rating = 300,
	.read = goldengate_get_timer1_cycles,
	.enable = goldengate_enable_timer1,
	.disable = goldengate_disable_timer1,
	.mask = CLOCKSOURCE_MASK(32),
	.shift = 20,
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init goldengate_clocksource_init(void)
{
	/* setup timer 1 as free-running clocksource */
	writel(0xFFFFFFFF, IO_ADDRESS(PER_TMR_LD2));
	writel(UPDATE_GPT1_LDVAL, IO_ADDRESS(PER_TMR_LOADE));
	writel(GPT_CTRL_EN | GPT_CTRL_RLMODE, IO_ADDRESS(PER_TMR_CTRL2));

	clocksource_register_khz(&clocksource_goldengate,
		sys_clk_info.apb_clk / 1000);
}

/*
 * Set up the clock source and clock events devices
 */
void __init goldengate_clock_init(unsigned int timer_irq)
{
	struct platform_clk clk;

	get_platform_clk(&clk);
	printk("APB Clock :%d\n",clk.apb_clk);
#if 0
	if (clk.apb_clk != APB_CLOCK){
		printk("Oops! APB Clock mismatch with HW strap pin![%d %d]\n",\
			 clk.apb_clk, APB_CLOCK);
		printk("You should change HW strap pin or Build option\n");
		WARN_ON(1);
	}
#endif
	/*
	 * Initialise to a known state (all timers off)
	 */
	writel(0, IO_ADDRESS(PER_TMR_CTRL1));
	writel(0, IO_ADDRESS(PER_TMR_CTRL2));

	/*
	 * Make irqs happen for the system timer
	 */
	setup_irq(timer_irq, &goldengate_timer_irq);

	goldengate_clocksource_init();
	goldengate_clockevents_init(timer_irq);

}


void get_platform_clk(struct platform_clk *clk)
{
#ifdef CONFIG_CORTINA_FPGA
	if (got_clk_info == 0){
		sys_clk_info.cpu_clk = 400 * 1000000;
		sys_clk_info.axi_clk = 104 * 1000000;
		sys_clk_info.apb_clk = 50 * 1000000;
		got_clk_info = 1;
	}

	clk->cpu_clk = sys_clk_info.cpu_clk;
	clk->apb_clk = sys_clk_info.apb_clk;
	clk->axi_clk = sys_clk_info.axi_clk;
#else
	unsigned int reg_v;

	if (got_clk_info == 1){
		clk->cpu_clk = sys_clk_info.cpu_clk;
		clk->apb_clk = sys_clk_info.apb_clk;
		clk->axi_clk = sys_clk_info.axi_clk;
		return ;
	}

	reg_v = readl(IO_ADDRESS(GLOBAL_STRAP));

	reg_v = (reg_v >> 1) & 0x07;
	switch (reg_v) {
	case 0:
		sys_clk_info.cpu_clk = 400 * 1000000;
		sys_clk_info.apb_clk = 100 * 1000000;
		sys_clk_info.axi_clk = 133333333;
		break;
	case 1:
		sys_clk_info.cpu_clk = 600 * 1000000;
		sys_clk_info.apb_clk = 100 * 1000000;
		sys_clk_info.axi_clk = 150 * 1000000;
		break;
	case 2:
		sys_clk_info.cpu_clk = 700 * 1000000;
		sys_clk_info.apb_clk = 100 * 1000000;
		sys_clk_info.axi_clk = 140 * 1000000;
		break;
	case 3:
		sys_clk_info.cpu_clk = 800 * 1000000;
		sys_clk_info.apb_clk = 100 * 1000000;
		sys_clk_info.axi_clk = 160 * 1000000;
		break;
	case 4:
		sys_clk_info.cpu_clk = 900 * 1000000;
		sys_clk_info.apb_clk = 100 * 1000000;
		sys_clk_info.axi_clk = 150 * 1000000;
		break;
	case 5:
		sys_clk_info.cpu_clk = 750 * 1000000;
		sys_clk_info.apb_clk = 150 * 1000000;
		sys_clk_info.axi_clk = 150 * 1000000;
		break;
	case 6:
		sys_clk_info.cpu_clk = 850 * 1000000;
		sys_clk_info.apb_clk = 170 * 1000000;
		sys_clk_info.axi_clk = 141666667;
		break;
	default:
		printk("Unknow strap pin for cpu clock");
		BUG_ON(1);
		break;
	}

	clk->cpu_clk = sys_clk_info.cpu_clk;
	clk->apb_clk = sys_clk_info.apb_clk;
	clk->axi_clk = sys_clk_info.axi_clk;

	got_clk_info = 0;

#endif
}
EXPORT_SYMBOL(get_platform_clk);

unsigned int cs_get_hw_timestamp(void)
{
	return readl(IO_ADDRESS(PER_TMR_CNT2));
}
EXPORT_SYMBOL(cs_get_hw_timestamp);

unsigned int cs_get_hw_timestamp_delta(unsigned int t1, unsigned int t2)
{
	unsigned int delta;

	if (t1 > t2)
		delta = t1 - t2;
	else
		delta = t1 + (~t2) + 1;

	return delta/(sys_clk_info.apb_clk/1000000) ;
}
EXPORT_SYMBOL(cs_get_hw_timestamp_delta);

unsigned int cs_get_soc_type(void)
{
	unsigned int soc_type;

	soc_type = (readl((volatile void __iomem *)GLOBAL_JTAG_ID) >> 12) & 0x0000000F;

	switch (soc_type) {
	case 0x0:
		soc_type = CS_SOC_CS7542_A0;
		break;
	case 0x8:
		soc_type = CS_SOC_CS7542_A1;
		break;
	case 0x1:
		soc_type = CS_SOC_CS7522_A0;
		break;
	case 0x9:
		soc_type = CS_SOC_CS7522_A1;
		break;
	default:
		soc_type = CS_SOC_UNKNOWN;
	}

	return soc_type;
}
EXPORT_SYMBOL(cs_get_soc_type);
