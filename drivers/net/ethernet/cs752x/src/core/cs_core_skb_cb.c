/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_core_skb_cb.c
 *
 * $Id: cs_core_skb_cb.c,v 1.8 2012/05/21 21:47:06 whsu Exp $
 *
 * It contains the implementation of managing control block info that is
 * stored in the skb.
 */

/* implementation note:
 * In fact, to make the code more straightforward and mesh with how Linux
 * kernel implement list-type of table management, we could've used the
 * common list data structure to manage it.  However, there is a downside
 * with that implementation, which is it will require extra time for
 * data structure allocation and deallocation per packet.  We want to
 * avoid that.  That's why we decide to use a pre-allocated array for the
 * known data structure.  Expend it only when we run out of them.
 */
#include <linux/spinlock.h>
#include <linux/list.h>
#include "cs_core_logic.h"
#include "cs_mut.h"

/* after several tests, the following seems to be the best combinations:
 * SMP: define CONFIG_PER_CPU_CB_POOL, CONFIG_CB_POOL_LOCK, and
 * 	CONFIG_USED_CB_POOL, but do not define CONFIG_CB_POOL_LOCK.
 * Unicore: define CONFIG_USED_CB_POOL, but do not define CONFIG_PER_CPU_CB_POOL
 * 	nor CONFIG_CB_POOL_LOCK */
//#define CONFIG_PER_CPU_CB_POOL
#define CONFIG_CB_POOL_LOCK
#define CONFIG_USED_CB_POOL

#define CS_ACCEL_CB_DEFAULT_SIZE	2048
#define CS_ACCEL_CB_INCREASE_SIZE	256
#define CS_ACCEL_CB_MAX_SIZE		0x8000

typedef struct cs_accel_cb_db_s {
	struct list_head node;
	unsigned cpu_pool;
	cs_kernel_accel_cb_t content;
	struct sk_buff *skb;
} cs_accel_cb_db_t;

unsigned int accel_cb_max_size = CS_ACCEL_CB_MAX_SIZE;
unsigned int accel_cb_inc_size = CS_ACCEL_CB_INCREASE_SIZE;

struct list_head accel_cb_pool_0;
unsigned int accel_cb_curr_size_0 = CS_ACCEL_CB_DEFAULT_SIZE;
unsigned int accel_cb_cnt_0 = 0;
#ifdef CONFIG_CB_POOL_LOCK
spinlock_t cb_pool_lock_0;
#endif

#ifdef CONFIG_PER_CPU_CB_POOL
struct list_head accel_cb_pool_1;
unsigned int accel_cb_curr_size_1 = CS_ACCEL_CB_DEFAULT_SIZE;
unsigned int accel_cb_cnt_1 = 0;
#ifdef CONFIG_CB_POOL_LOCK
spinlock_t cb_pool_lock_1;
#endif
#endif

#ifdef CONFIG_USED_CB_POOL
struct list_head accel_cb_used;
spinlock_t cb_used_lock;
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ne_core_logic_debug;
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_SKB_CB) x;}
#define DBG_ERR(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_SKB_CB) x;}
#else
#define DBG(x) { }
#define DBG_ERR(x) { }

#endif


cs_accel_cb_db_t *get_next_avail_entry(void)
{
	cs_accel_cb_db_t *new_db;
	struct list_head *cpu_pool;
	unsigned int *cb_cnt_ptr;
#ifdef CONFIG_CB_POOL_LOCK
	spinlock_t *pool_lock;
#endif
#if defined(CONFIG_CB_POOL_LOCK) || defined(CONFIG_USED_CB_POOL)
	unsigned long flags;
#endif
	int cpu = get_cpu();
	put_cpu();

#ifdef CONFIG_PER_CPU_CB_POOL

	if (cpu == 0) {
		cpu_pool = &accel_cb_pool_0;
		cb_cnt_ptr = &accel_cb_cnt_0;
#ifdef CONFIG_CB_POOL_LOCK
		pool_lock = &cb_pool_lock_0;
#endif
	} else {
		cpu_pool = &accel_cb_pool_1;
		cb_cnt_ptr = &accel_cb_cnt_1;
#ifdef CONFIG_CB_POOL_LOCK
		pool_lock = &cb_pool_lock_1;
#endif
	}

#else
	cpu_pool = &accel_cb_pool_0;
	cb_cnt_ptr = &accel_cb_cnt_0;
#ifdef CONFIG_CB_POOL_LOCK
	pool_lock = &cb_pool_lock_0;
#endif
#endif

#ifdef CONFIG_CB_POOL_LOCK
	spin_lock_irqsave(pool_lock, flags);
#endif
	if (list_empty(cpu_pool)) {
#ifdef CONFIG_CB_POOL_LOCK
		spin_unlock_irqrestore(pool_lock, flags);
#endif
		return NULL;
	}

	new_db = list_first_entry(cpu_pool, cs_accel_cb_db_t, node);
	list_del(&new_db->node);
	(*cb_cnt_ptr)--;
#ifdef CONFIG_CB_POOL_LOCK
	spin_unlock_irqrestore(pool_lock, flags);
#endif

#ifdef CONFIG_USED_CB_POOL
	spin_lock_irqsave(&cb_used_lock, flags);
	list_add_tail(&new_db->node, &accel_cb_used);
	spin_unlock_irqrestore(&cb_used_lock, flags);
#endif
	return new_db;
}

cs_accel_cb_db_t *expand_table_and_get_entry(void)
{
	cs_accel_cb_db_t *new_cb_db;
	struct list_head *cpu_pool;
	unsigned int *cb_cnt_ptr, *cb_size_ptr;
	int i;
#ifdef CONFIG_CB_POOL_LOCK
	spinlock_t *pool_lock;
	unsigned long flags;
#endif
	int cpu = get_cpu();
	put_cpu();

#ifdef CONFIG_PER_CPU_CB_POOL

	if (cpu == 0) {
		if (accel_cb_curr_size_0 == accel_cb_max_size)
			return NULL;

		cpu_pool = &accel_cb_pool_0;
		cb_cnt_ptr = &accel_cb_cnt_0;
		cb_size_ptr = &accel_cb_curr_size_0;
#ifdef CONFIG_CB_POOL_LOCK
		pool_lock = &cb_pool_lock_0;
#endif
	} else {
		if (accel_cb_curr_size_1 == accel_cb_max_size)
			return NULL;

		cpu_pool = &accel_cb_pool_1;
		cb_cnt_ptr = &accel_cb_cnt_1;
		cb_size_ptr = &accel_cb_curr_size_1;
#ifdef CONFIG_CB_POOL_LOCK
		pool_lock = &cb_pool_lock_1;
#endif
	}
#else
	if (accel_cb_curr_size_0 == accel_cb_max_size)
		return NULL;

	cpu_pool = &accel_cb_pool_0;
	cb_cnt_ptr = &accel_cb_cnt_0;
	cb_size_ptr = &accel_cb_curr_size_0;
#ifdef CONFIG_CB_POOL_LOCK
	pool_lock = &cb_pool_lock_0;
#endif
#endif

	for (i = 0; i < accel_cb_inc_size; i++) {
		new_cb_db = cs_zalloc(sizeof(cs_accel_cb_db_t), GFP_ATOMIC);
		if (new_cb_db == NULL)
			break;
		new_cb_db->cpu_pool = cpu;
#ifdef CONFIG_CB_POOL_LOCK
		spin_lock_irqsave(pool_lock, flags);
#endif
		list_add_tail(&new_cb_db->node, cpu_pool);
		(*cb_cnt_ptr)++;
		(*cb_size_ptr)++;
#ifdef CONFIG_CB_POOL_LOCK
		spin_unlock_irqrestore(pool_lock, flags);
#endif
	}

	return get_next_avail_entry();
}

int cs_accel_cb_add(struct sk_buff *skb)
{
	cs_accel_cb_db_t *new_cb_db;
	cs_kernel_accel_cb_t *cb_ptr = CS_KERNEL_SKB_CB(skb);

	/* only allocate when the skb is a newly allocated one, since
	 * we are recycling used packet, we might not need to grab new
	 * cb for an existing packet buffer */
	if (cb_ptr != NULL)
		return 0;

	new_cb_db = get_next_avail_entry();
	if (unlikely(new_cb_db == NULL)) {
		/* we need to expand the table. and locate a new entry */
		new_cb_db = expand_table_and_get_entry();
		if (new_cb_db == NULL)
			return -1;
	}

	new_cb_db->skb = skb;
	skb->cs_cb_loc = (u32)&(new_cb_db->content);
	DBG(printk("%s skb=%p cb=%d cb_db=%p\n", __func__, skb, skb->cs_cb_loc, new_cb_db));
	return 0;
}

/* assuming skb is not NULL! */
int cs_accel_cb_del(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cb_ptr = CS_KERNEL_SKB_CB(skb);
	cs_accel_cb_db_t *cb_db_ptr;
	struct list_head *cpu_pool;
	unsigned int *cb_cnt_ptr;
#ifdef CONFIG_CB_POOL_LOCK
	spinlock_t *pool_lock;
#endif
#if defined(CONFIG_CB_POOL_LOCK) || defined(CONFIG_USED_CB_POOL)
	unsigned long flags;
#endif

	if (cb_ptr == NULL)
		return -1;

	cb_db_ptr = container_of(cb_ptr, struct cs_accel_cb_db_s, content);

	DBG(printk("%s skb=%p cb=%d cb_db=%p\n", __func__, skb, skb->cs_cb_loc, cb_db_ptr));

	skb->cs_cb_loc = 0;

	cb_ptr->common.tag = 0;
	cb_db_ptr->skb = NULL;

#ifdef CONFIG_USED_CB_POOL
	spin_lock_irqsave(&cb_used_lock, flags);
	list_del(&cb_db_ptr->node);
	spin_unlock_irqrestore(&cb_used_lock, flags);
#endif

#ifdef CONFIG_PER_CPU_CB_POOL
	if (cb_db_ptr->cpu_pool == 0) {
		cpu_pool = &accel_cb_pool_0;
		cb_cnt_ptr = &accel_cb_cnt_0;
#ifdef CONFIG_CB_POOL_LOCK
		pool_lock = &cb_pool_lock_0;
#endif
	} else {
		cpu_pool = &accel_cb_pool_1;
		cb_cnt_ptr = &accel_cb_cnt_1;
#ifdef CONFIG_CB_POOL_LOCK
		pool_lock = &cb_pool_lock_1;
#endif
	}
#else
	cpu_pool = &accel_cb_pool_0;
	cb_cnt_ptr = &accel_cb_cnt_0;
#ifdef CONFIG_CB_POOL_LOCK
	pool_lock = &cb_pool_lock_0;
#endif
#endif

#ifdef CONFIG_CB_POOL_LOCK
	spin_lock_irqsave(pool_lock, flags);
#endif
	list_add_tail(&cb_db_ptr->node, cpu_pool);
	(*cb_cnt_ptr)++;
#ifdef CONFIG_CB_POOL_LOCK
	spin_unlock_irqrestore(pool_lock, flags);
#endif

	return 0;
}

/* copy cb from src_skb to dst_skb */
int cs_accel_cb_copy(struct sk_buff *dst_skb, const struct sk_buff *src_skb)
{
	cs_kernel_accel_cb_t *src_cb_ptr = CS_KERNEL_SKB_CB(src_skb);
	cs_kernel_accel_cb_t *dst_cb_ptr = CS_KERNEL_SKB_CB(dst_skb);

	if ((src_cb_ptr == NULL) || (dst_cb_ptr == NULL))
		return -1;

	DBG(printk("%s src_skb=%p src_cb_ptr=%p  " \
			"--> dst_skb=%p dst_cb_ptr=%p  \n", __func__,
		dst_skb, dst_cb_ptr, src_skb, src_cb_ptr ));


	memcpy(dst_cb_ptr, src_cb_ptr, sizeof(cs_kernel_accel_cb_t));

	return 0;
}

int cs_accel_cb_reset(struct sk_buff *skb)
{
	skb->cs_cb_loc = 0;
	return 0;
}

int cs_accel_cb_reset_state(struct sk_buff *skb){
	cs_kernel_accel_cb_t *cb_ptr = CS_KERNEL_SKB_CB(skb);
	if (cb_ptr == NULL)
		return -1;

	/* we just have to make sure the tag is not CS_CB_TAG, so
	 * all the hardware acceleration processes will not get
	 * confused later on. */
	cb_ptr->common.tag = 0;

	return 0;
}

/* clone cb. will allocate a new cb for new skb if needed */
int cs_accel_cb_clone(struct sk_buff *new_skb, const struct sk_buff *old_skb)
{
	cs_kernel_accel_cb_t *cb_ptr = CS_KERNEL_SKB_CB(old_skb);

	DBG(printk("%s new_skb=%p cb=%p old_skb=%p\n", __func__, new_skb, cb_ptr, old_skb));

	/* the original skb does not contain cs_cb info */
	if (cb_ptr == NULL)
		return 0;

	if (cs_accel_cb_add(new_skb) != 0)
		return -1;

	return cs_accel_cb_copy(new_skb, old_skb);
}

void cs_accel_cb_print(void)
{
#ifdef CONFIG_USED_CB_POOL
	int used_count;
	cs_accel_cb_db_t *curr_cb_db, *m;
	cs_kernel_accel_cb_t *cb_ptr;
	unsigned long flags;
	int total_count, free_count;
#endif

	printk("\t*** CS Acceleration Control Block Info ***\n");
	printk("\t----------------------------------------------------------\n");

	printk("\tMax Size = %d, Increase Size = %d\n", accel_cb_max_size,
			accel_cb_inc_size);
	printk("\t----------------------------------------------------------\n");

	printk("\tQueue#0:");
	printk("curr_starting_max_size = %d, curr_count = %d\n",
			accel_cb_curr_size_0, accel_cb_cnt_0);
	printk("\t----------------------------------------------------------\n");
	total_count = accel_cb_curr_size_0;
	free_count = accel_cb_cnt_0;

#ifdef CONFIG_PER_CPU_CB_POOL
	printk("\tQueue#1:");
	printk("curr_starting_max_size = %d, curr_count = %d\n",
			accel_cb_curr_size_1, accel_cb_cnt_1);
	printk("\t----------------------------------------------------------\n");
	total_count += accel_cb_curr_size_1;
	free_count += accel_cb_cnt_1;
#endif

#ifdef CONFIG_USED_CB_POOL
	used_count = 0;
	printk("\tused cb and info:\n");
	spin_lock_irqsave(&cb_used_lock, flags);
	list_for_each_entry_safe(curr_cb_db, m, &accel_cb_used, node) {
//		printk("\t#%d:curr_cb_db@0x%08x, curr_cb_db->skb = 0x%08x\n",
//				used_count, curr_cb_db, curr_cb_db->skb);

		if (curr_cb_db->skb == NULL)
			printk("\t#%d is faulty: curr_cb_db@0x%08x, "
					"curr_cb_db->skb = 0x%08x\n",
					used_count, (u32)curr_cb_db,
					(u32)curr_cb_db->skb);
		else {
			cb_ptr = CS_KERNEL_SKB_CB(curr_cb_db->skb);
		}
		used_count++;
	}
	spin_unlock_irqrestore(&cb_used_lock, flags);
	printk("\ttotal used cb count = %d\n", used_count);
	printk("\t----------------------------------------------------------\n");
	if (total_count != used_count + free_count)
		printk("\t Warning: total_count(%d) != used_count(%d) + free_count(%d)\n",
			total_count, used_count, free_count);

#endif

	return;
}


int cs_accel_cb_init(void)
{
	int i;
	cs_accel_cb_db_t *new_cb_db;

	INIT_LIST_HEAD(&accel_cb_pool_0);
#ifdef CONFIG_CB_POOL_LOCK
	spin_lock_init(&cb_pool_lock_0);
#endif
#ifdef CONFIG_PER_CPU_CB_POOL
	INIT_LIST_HEAD(&accel_cb_pool_1);
#ifdef CONFIG_CB_POOL_LOCK
	spin_lock_init(&cb_pool_lock_1);
#endif
#endif
#ifdef CONFIG_USED_CB_POOL
	INIT_LIST_HEAD(&accel_cb_used);
	spin_lock_init(&cb_used_lock);
#endif

	for (i = 0; i < accel_cb_curr_size_0; i++) {
		new_cb_db = cs_zalloc(sizeof(cs_accel_cb_db_t), GFP_ATOMIC);
		if (new_cb_db == NULL)
			break;
		new_cb_db->cpu_pool = 0;
		list_add_tail(&new_cb_db->node, &accel_cb_pool_0);
		accel_cb_cnt_0++;
	}
	accel_cb_curr_size_0 = accel_cb_cnt_0;

#ifdef CONFIG_PER_CPU_CB_POOL
	for (i = 0; i < accel_cb_curr_size_1; i++) {
		new_cb_db = cs_zalloc(sizeof(cs_accel_cb_db_t), GFP_ATOMIC);
		if (new_cb_db == NULL)
			break;
		new_cb_db->cpu_pool = 1;
		list_add_tail(&new_cb_db->node, &accel_cb_pool_1);
		accel_cb_cnt_1++;
	}
	accel_cb_curr_size_1 = accel_cb_cnt_1;
#endif

	return 0;
}

void cs_accel_cb_exit(void)
{
	cs_accel_cb_db_t *cb_db_ptr;

#ifdef CONFIG_USED_CB_POOL
	while (!list_empty(&accel_cb_used)) {
		cb_db_ptr = list_first_entry(&accel_cb_used, cs_accel_cb_db_t,
				node);
		list_del(&cb_db_ptr->node);
		cb_db_ptr->skb->cs_cb_loc = 0;
		cs_free(cb_db_ptr);
	}
#endif

	while (!list_empty(&accel_cb_pool_0)) {
		cb_db_ptr = list_first_entry(&accel_cb_pool_0, cs_accel_cb_db_t,
				node);
		list_del(&cb_db_ptr->node);
		cs_free(cb_db_ptr);
	}
#ifdef CONFIG_PER_CPU_CB_POOL
	while (!list_empty(&accel_cb_pool_1)) {
		cb_db_ptr = list_first_entry(&accel_cb_pool_1, cs_accel_cb_db_t,
				node);
		list_del(&cb_db_ptr->node);
		cs_free(cb_db_ptr);
	}
#endif
}

EXPORT_SYMBOL(cs_accel_cb_add);
EXPORT_SYMBOL(cs_accel_cb_del);
EXPORT_SYMBOL(cs_accel_cb_copy);
EXPORT_SYMBOL(cs_accel_cb_clone);
