#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <asm/cputype.h>
#include <mach/platform.h>
#include <mach/hardware.h>
#include <mach/registers.h>
#include <mach/global_timer.h>

#define GOLDENGATE_GLOBAL_TIMER_BASE 0xF8000200

#define GT_COUNTER0	0x00
#define GT_COUNTER1	0x04

#define GT_CONTROL	0x08
#define GT_CONTROL_TIMER_ENABLE		BIT(0)  /* this bit is NOT banked */
#define GT_CONTROL_COMP_ENABLE		BIT(1)	/* banked */
#define GT_CONTROL_IRQ_ENABLE		BIT(2)	/* banked */

#define GT_INT_STATUS	0x0c
#define GT_INT_STATUS_EVENT_FLAG	BIT(0)

#define GT_COMP0	0x10
#define GT_COMP1	0x14

#define STATE_INACTIVE	0x00
#define STATE_ENQUEUED  0x01
#define STATE_CALLBACK  0x02

struct cs75xx_global_timer_base boot_bases;
static DEFINE_PER_CPU(struct cs75xx_global_timer_base*, global_timer_bases) =
				&boot_bases;

/*
 * We are expecting to be clocked by the ARM peripheral clock.
 *
 * Note: it is assumed we are using a prescaler value of zero, so this is
 * the units for all operations.
 */
static void __iomem *gt_base;
static unsigned int gt_clk_rate;
static int gt_ppi;
static struct clock_event_device __percpu *gt_evt;

#ifdef CONFIG_DEBUG_FS
static struct dentry *debug_root;
#endif

/*
 * timer_is_hold - test if the timer is still queued.
 * @timer: the timer to be tested.
*/
static inline int timer_is_hold(const struct cs75xx_global_timer *timer)
{
	return timer->node.next != NULL;
}

/*
 * lock_base - grasp the timer base lock with the same 'timer->base'.
 * @timer: tries to lock the timer.
 * @flags: used to save the interrupt flags.
 *
 * 'timer->base' is per-cpu, the lock has multiple copies on each cpu,
 * exit with the lock of 'timer->base'.
 */
static struct cs75xx_global_timer_base *lock_base(
			struct cs75xx_global_timer *timer,
						unsigned long *flags)
{
	for (;;) {
		struct cs75xx_global_timer_base *prelock_base = timer->base;
		if (likely(prelock_base != NULL)) {
			spin_lock_irqsave(&prelock_base->lock, *flags);
			if (likely(prelock_base == timer->base))
				return prelock_base;
			/* The timer has been migrated to another CPU */
			spin_unlock_irqrestore(&prelock_base->lock, *flags);
		}
		cpu_relax();
	}
}

/*
 * To get the value from the Global Timer Counter register proceed as follows:
 * 1. Read the upper 32-bit timer counter register
 * 2. Read the lower 32-bit timer counter register
 * 3. Read the upper 32-bit timer counter register again. If the value is
 *  different to the 32-bit upper value read previously, go back to step 2.
 *  Otherwise the 64-bit timer counter value is correct.
 */
static u64 gt_counter_read(void)
{
	u64 counter;
	u32 lower;
	u32 upper, old_upper;

	upper = readl_relaxed(gt_base + GT_COUNTER1);
	do {
		old_upper = upper;
		lower = readl_relaxed(gt_base + GT_COUNTER0);
		upper = readl_relaxed(gt_base + GT_COUNTER1);
	} while (upper != old_upper);

	counter = upper;
	counter <<= 32;
	counter |= lower;
	return counter;
}

/**
 * To ensure that updates to comparator value register do not set the
 * Interrupt Status Register proceed as follows:
 * 1. Clear the Comp Enable bit in the Timer Control Register.
 * 2. Write the lower 32-bit Comparator Value Register.
 * 3. Write the upper 32-bit Comparator Value Register.
 * 4. Set the Comp Enable bit and, if necessary, the IRQ enable bit.
 */
static void gt_compare_set(unsigned long delta)
{
	u64 counter = gt_counter_read();
	unsigned long ctrl;

	counter += delta;
	ctrl = GT_CONTROL_TIMER_ENABLE;
	writel(ctrl, gt_base + GT_CONTROL);
	writel(lower_32_bits(counter), gt_base + GT_COMP0);
	writel(upper_32_bits(counter), gt_base + GT_COMP1);
	ctrl |= GT_CONTROL_COMP_ENABLE | GT_CONTROL_IRQ_ENABLE;
	writel(ctrl, gt_base + GT_CONTROL);
}

/*
 * program_next_event - Program the banked comparator of Global timer device.
 * @base: per-cpu base on this cpu.
 * @expires: next expire. it's absolute time based on the Global timer counter.
*/
static int program_next_event(struct cs75xx_global_timer_base *base,
			ktime_t expires)
{
	unsigned long cyc;
	int64_t delta;

	delta = ktime_to_ns(ktime_sub(expires, ktime_get()));
	delta = min_t(int64_t, delta, (int64_t) base->max_delta_ns);
	delta = max_t(int64_t, delta, (int64_t) base->min_delta_ns);
	cyc = (delta * base->mult) >> base->shift;
	gt_compare_set(cyc);
	return 0;
}

void cs75xx_timer_init(struct cs75xx_global_timer *timer)
{
	timer->node.next = NULL;
	timer->mode = GT_NORESTART;
	timer->cnt = 0;
	timer->base = raw_cpu_read(global_timer_bases);
}
EXPORT_SYMBOL(cs75xx_timer_init);

/*
 * __add_timer - queue the timer to the sorted list.
 * @timer: the timer to be queued.

 * return true if the queued timer is the first node in the sorted list,
 * or else return false.
*/
static inline int __add_timer(struct cs75xx_global_timer *timer,
		struct cs75xx_global_timer_base *base)
{
	struct list_head *entry;
	struct cs75xx_global_timer *tmp;

	entry = &base->list;

	list_for_each_entry(tmp, &base->list, node) {
		if (timer->expires.tv64 > tmp->expires.tv64)
			entry = &tmp->node;
		else
			break;
	}

	list_add(&timer->node, entry);
	return (&timer->node == base->list.next);
}

static int __remove_timer(struct cs75xx_global_timer *timer)
{
	struct list_head *node = &timer->node;

	if (!timer_is_hold(timer))
		return 0;

	__list_del(node->prev, node->next);
	node->next = NULL;
	node->prev = LIST_POISON2;
	return 1;
}

/*
 * _run_timer - Run all the expired timers on this cpu.
 * @base: the timer base to be processed.
*/
static inline void _run_timer(struct cs75xx_global_timer *timer,
		struct cs75xx_global_timer_base *base)
{
	int restart;

	__remove_timer(timer);

	spin_unlock(&base->lock);
	restart = timer->callback(timer->data);
	spin_lock(&base->lock);

	timer->cnt++;
	timer->cpu = smp_processor_id();

	/* For periodic timer. */
	if (restart == GT_RESTART) {
		timer->mode = GT_RESTART;
		__add_timer(timer, base);
	}
}

/*
 * gt_interrupt - per cpu global timer interrupt handler.
 * @irq: global timer interrupt number.
 * @dev_id: per cpu arg.
*/
static irqreturn_t gt_interrupt(int irq, void *dev_id)
{
	struct cs75xx_global_timer_base *base;
	unsigned long flags;
	ktime_t expires_next, now, expire;
	struct list_head *entry, *n;
	struct cs75xx_global_timer *timer;

	base = per_cpu(global_timer_bases, smp_processor_id());

	if (!(readl_relaxed(gt_base + GT_INT_STATUS) &
				GT_INT_STATUS_EVENT_FLAG))
		return IRQ_NONE;

	/**
	 * ERRATA 740657( Global Timer can send 2 interrupts for
	 * the same event in single-shot mode)
	 * Workaround:
	 *	Either disable single-shot mode.
	 *	Or
	 *	Modify the Interrupt Handler to avoid the
	 *	offending sequence. This is achieved by clearing
	 *	the Global Timer flag _after_ having incremented
	 *	the Comparator register	value to a higher value.
	 */
	gt_compare_set(ULONG_MAX);

	/*Clear the interrupt event.*/
	writel_relaxed(GT_INT_STATUS_EVENT_FLAG, gt_base + GT_INT_STATUS);

	spin_lock_irqsave(&base->lock, flags);
	expires_next.tv64 = KTIME_MAX;
	now = ktime_get();

rescan:
	list_for_each_safe(entry, n, &base->list) {
		timer = list_entry(entry, struct cs75xx_global_timer, node);
		base->running_timer = timer;

		if (now.tv64 < timer->expires.tv64) {
			expire = timer->expires;
			if (expire.tv64 < 0)
				expire.tv64 = KTIME_MAX;

			if (expire.tv64 < expires_next.tv64)
				expires_next = expire;

			break;
		}

		/*
		 * The timer list is sorted,
		 * so rescan has no performance impact.
		 */
		_run_timer(timer, base);
		goto rescan;
	}

	base->running_timer = NULL;
	base->next_timer = expires_next;

	/* Dynamic interrupt. */
	if (expires_next.tv64 == KTIME_MAX ||
		!program_next_event(base, base->next_timer)) {
		spin_unlock_irqrestore(&base->lock, flags);
		return IRQ_HANDLED;
	}

	pr_info("Unexpected interrupt.\n");
	spin_unlock_irqrestore(&base->lock, flags);
	return IRQ_HANDLED;
}

/*
 * cs75xx_timer_add - start the timer on this cpu.
 * @timer: the timer to be added.

 * rturn 0 for success, other value for fail.
*/
int cs75xx_timer_add(struct cs75xx_global_timer *timer)
{
	struct cs75xx_global_timer_base *base, *new_base;
	unsigned long flags;
	int ret = 0 , cpu, min_expire;

	base = lock_base(timer, &flags);

	if (timer_is_hold(timer))
		__remove_timer(timer);

	cpu = smp_processor_id();
	new_base = per_cpu(global_timer_bases, cpu);

	/* Migrate the timer on this cpu to avoid dead lock. */
	if (base != new_base) {
		if (likely(base->running_timer != timer)) {
			timer->base = NULL;
			spin_unlock(&base->lock);
			base = new_base;
			spin_lock(&base->lock);
			timer->base = base;
		}
	}

	min_expire = __add_timer(timer, base);

	if (min_expire) {
		/* If the added timer is the next 'latest' timer,
		 * need to reprogram the comparator.
		 */
		base->next_timer = timer->expires;
		program_next_event(base, base->next_timer);
	}

	spin_unlock_irqrestore(&base->lock, flags);
	return ret;
}
EXPORT_SYMBOL(cs75xx_timer_add);

static void __cs75xx_timer_add_on_cpu(void *p)
{
	struct cs75xx_global_timer *timer = (struct cs75xx_global_timer*)p; 

	cs75xx_timer_add(timer);
}

/*
 * cs75xx_timer_add_on_cpu - start the timer on the desinated cpu.
 * @timer: the timer to be added.
 * @cpu: the cpu running the timer. 

 * rturn 0 for success, other value for fail.
*/
int cs75xx_timer_add_on_cpu(struct cs75xx_global_timer *timer,
			unsigned int cpu)
{

	timer->cpumask = cpumask_of(cpu);
	on_each_cpu_mask(timer->cpumask, __cs75xx_timer_add_on_cpu, (void*)timer, 1);

	return 0;
}
EXPORT_SYMBOL(cs75xx_timer_add_on_cpu);

/*
 * cs75xx_timer_del - Cancel the timer from the queue.
 * @timer: the timer to be canceld.
 *
 * This function tries to cancel the timer from the queue, after exit, the timer
 * is not running on any cpu.
 * callers must prevent restarting of the timer after this function return.
*/
int cs75xx_timer_del(struct cs75xx_global_timer *timer)
{
	struct cs75xx_global_timer_base *base;
	struct cs75xx_global_timer *t;
	ktime_t next_expire;
	unsigned long flags;
	int ret;

	next_expire.tv64 = KTIME_MAX;

	for (;;) {
		ret = -1;
		base = lock_base(timer, &flags);

		/* Protect the running timer to be destroyed. */
		if (base->running_timer != timer) {
			ret = __remove_timer(timer);
			if (!list_empty(&base->list)) {
				t = list_first_entry(&base->list,
				  struct cs75xx_global_timer, node);
				next_expire = t->expires;
			}

			/* Adjust the next timer interrupt. */
			program_next_event(base, next_expire);
		}

		spin_unlock_irqrestore(&base->lock, flags);

		if (ret >= 0)
			return ret;

		cpu_relax();
	}
}
EXPORT_SYMBOL(cs75xx_timer_del);

static u64 gt_delta2ns(unsigned long latch,
		 struct cs75xx_global_timer_base *base)
{
	u64 clc = (u64) latch << base->shift;

	if (unlikely(!base->mult)) {
		base->mult = 1;
		WARN_ON(1);
	}

	do_div(clc, base->mult);
	if (clc < 1000)
		clc = 1000;
	if (clc > KTIME_MAX)
		clc = KTIME_MAX;

	return clc;
}

/*
 * Copy from clocksource.
 * @base: per-cpu timer base.
 * @freq: Global timer frequency.
 */
static void gt_config(struct cs75xx_global_timer_base *base, u32 freq)
{
	u64 sec;

	/*
	 * Calculate the maximum number of seconds we can sleep. Limit
	 * to 10 minutes for hardware which can program more than
	 * 32bit ticks so we still get reasonable conversion values.
	 */
	sec = base->max_delta_ticks;
	do_div(sec, freq);
	if (!sec)
		sec = 1;
	else if (sec > 600 && base->max_delta_ticks > UINT_MAX)
		sec = 600;

	clocks_calc_mult_shift(&base->mult, &base->shift, NSEC_PER_SEC,
							freq, sec);
	base->min_delta_ns = gt_delta2ns(base->min_delta_ticks, base);
	base->max_delta_ns = gt_delta2ns(base->max_delta_ticks, base);
}

static int gt_starting_cpu(unsigned int cpu)
{
	struct cs75xx_global_timer_base *base;
	static char base_done[NR_CPUS];
	static char boot_done;

	if (!base_done[cpu]) {
		if (boot_done) {
			base = kmalloc_node(sizeof(*base),
						GFP_KERNEL | __GFP_ZERO,
						cpu_to_node(cpu));
			if (!base)
				return -ENOMEM;

			per_cpu(global_timer_bases, cpu) = base;
		} else {
			boot_done = 1;
			base = &boot_bases;
		}
		spin_lock_init(&base->lock);
		base_done[cpu] = 1;
	} else
		base = per_cpu(global_timer_bases, cpu);

	INIT_LIST_HEAD(&base->list);
	base->next_timer.tv64 = KTIME_MAX;
	base->min_delta_ticks = 1;
	base->max_delta_ticks = 0xffffffff;
	gt_config(base, gt_clk_rate);
	pr_info("Global timer enabled on cpu %d.\n", cpu);
	enable_percpu_irq(gt_ppi, 0);
	return 0;
}

static int gt_dying_cpu(unsigned int cpu)
{
	disable_percpu_irq(gt_ppi);
	return 0;
}

static cycle_t gt_clocksource_read(struct clocksource *cs)
{
	return gt_counter_read();
}

static struct clocksource gt_clocksource = {
	.name	= "arm_global_timer",
	.rating	= 600,
	.read	= gt_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init gt_clocksource_init(void)
{
	writel(0, gt_base + GT_CONTROL);
	writel(0, gt_base + GT_COUNTER0);
	writel(0, gt_base + GT_COUNTER1);
	/* enables timer on all the cores */
	writel(GT_CONTROL_TIMER_ENABLE, gt_base + GT_CONTROL);
	clocksource_register_hz(&gt_clocksource, gt_clk_rate);
}

void global_timer_register(void)
{
	int err = 0;
	struct platform_clk clk;

	/*
	 * In r2p0 the comparators for each processor with the global timer
	 * fire when the timer value is greater than or equal to. In previous
	 * revisions the comparators fired when the timer value was equal to.
	 */
	if ((read_cpuid_id() & 0xf0000f) < 0x200000) {
		pr_warn("global-timer: non support for this cpu version.\n");
		return;
	}

	gt_ppi = 27;

	gt_base = __io_address(GOLDENGATE_GLOBAL_TIMER_BASE);

	get_platform_clk(&clk);
	gt_clk_rate = clk.apb_clk;
	pr_info("Global timer clk rate: %u\n", gt_clk_rate);

	gt_evt = alloc_percpu(struct clock_event_device);
	if (!gt_evt) {
		pr_warn("global-timer: can't allocate memory\n");
		err = -ENOMEM;
		goto out_clk;
	}

	err = request_percpu_irq(gt_ppi, gt_interrupt,
				 "gt", gt_evt);
	if (err) {
		pr_warn("global-timer: can't register interrupt %d (%d)\n",
			gt_ppi, err);
		goto out_free;
	}

	err = cpuhp_setup_state(CPUHP_AP_ARM_G2_TIMER_STARTING,
				"AP_ARM_G2_TIMER_STARTING",
				gt_starting_cpu,
				gt_dying_cpu);
	if (err) {
		pr_warn("global-timer: unable to setup hotplug state and timer.\n");
		goto out_irq;
	}

	/* Immediately configure the timer on the boot CPU */
	gt_clocksource_init();

	/*
	 * Must not register this Global Timer as a clock event device,
	 * or else the system schedule tick timer must be taken over.
	*/
	if (gt_starting_cpu(smp_processor_id()) != 0) {
		pr_info("per cpu base init fail.\n");
		goto out_irq;
	}

	return;

out_irq:
	free_percpu_irq(gt_ppi, gt_evt);
out_free:
	free_percpu(gt_evt);
out_clk:
	WARN(err, "ARM Global timer register failed (%d)\n", err);
}

#ifdef CONFIG_DEBUG_FS
static int debug_timer_show(struct seq_file *s, void *v)
{
	unsigned int cpu;
	struct cs75xx_global_timer_base *base;
	struct cs75xx_global_timer *entry;
	ktime_t now;
	unsigned long flags;

	now = ktime_get();

	for_each_online_cpu(cpu) {
		base = per_cpu(global_timer_bases, cpu);
		if (!base) {
			pr_info("No timer info on cpu %d.\n", cpu);
			return -1;
		}

		pr_info("now               expire            cpu   mode  count\n");

		spin_lock_irqsave(&base->lock, flags);

		list_for_each_entry(entry, &base->list, node)
	    pr_info("%-16lld  %-16lld  %-4d  %-4d  %-16lld.\n",
			  ktime_to_ns(now), ktime_to_ns(entry->expires), cpu,
			  entry->mode, entry->cnt);

		spin_unlock_irqrestore(&base->lock, flags);
	}

	return 0;
}

static int debug_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, debug_timer_show, inode->i_private);
}

static const struct file_operations debug_timer_fops = {
	.open = debug_timer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static int __init timer_debug_init(void)
{
	debug_root = debugfs_create_dir("arm_global_timer", NULL);
	if (!debug_root) {
		pr_info("Can't create the debugfs dir.\n");
		goto Exit;
	}

	if (!debugfs_create_file("timer_stat", S_IRUGO,
			debug_root, NULL, &debug_timer_fops)) {
			pr_info("Can't create the debugfs node.\n");
		debugfs_remove_recursive(debug_root);
	}

Exit:
	return 0;
}

late_initcall(timer_debug_init);
#endif
