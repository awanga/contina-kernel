#ifndef __ASM_ARCH_GLOBAL_TIMER_H
#define __ASM_ARCH_IRQS_GLOBAL_TIMER_H
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/compiler.h>

/*
 * @node: internal use, link to cpu base list.
 * @base: per cpu base.
 * @expires: timer expire, absolute value, in nano second unit.
 * @mask: cpumask.
 * @callback: timer callback, should return GT_RESTART in periodic timer,
 *			and should return GT_NORESTART in single-shot timer.
 * @data: parameter for the callback.
 */

struct cs75xx_global_timer {
	struct list_head node;
	struct cs75xx_global_timer_base *base;
	ktime_t expires;
	const struct cpumask *cpumask;
	int (*callback)(unsigned long);
	unsigned long data;

	int cpu;
	int mode;
	s64 cnt;
};

struct cs75xx_global_timer_base {
	spinlock_t lock;
	struct list_head list;
	ktime_t next_timer;
	struct cs75xx_global_timer *running_timer;
	u64			max_delta_ns;
	u64			min_delta_ns;
	unsigned long	min_delta_ticks;
	unsigned long	max_delta_ticks;
	u32 mult;
	u32 shift;
};

#define GT_RESTART 0x0
#define GT_NORESTART 0x1

extern void cs75xx_timer_init(struct cs75xx_global_timer *timer);
extern int cs75xx_timer_add(struct cs75xx_global_timer *timer);
extern int cs75xx_timer_add_on_cpu(struct cs75xx_global_timer *timer,
				unsigned int cpu);
extern int cs75xx_timer_del(struct cs75xx_global_timer *timer);
void global_timer_register(void);
#endif
