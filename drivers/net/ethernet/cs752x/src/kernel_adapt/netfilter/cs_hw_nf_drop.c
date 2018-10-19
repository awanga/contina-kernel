#include <mach/cs75xx_fe_core_table.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <asm-generic/cputime.h>    //Bug#40544
#include "cs_core_logic.h"
#include "cs_hw_nf_drop.h"
#include "cs_fe.h"
#include "cs_hw_accel_manager.h"
#include "cs_core_vtable.h"
#include "cs_core_hmu.h"
#include "cs_mut.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;

#define DBG(x) {if (cs_adapt_debug & CS752X_ADAPT_NF_DROP) x;}
#else
#define DBG(x) {}
#endif

#define CS_MOD_NF_DROP_SWID (CS_SWID64_MASK(CS_SWID64_MOD_ID_NF_DROP) | 0x000000000001)

static int cs_hw_nf_drop_hmu_callback(u32 watch_bitmask,
				cs_core_hmu_value_t * value, u32 status);

static cs_core_hmu_value_t nf_drop_hmu_value = {
	.type = CS_CORE_HMU_WATCH_SWID64,
	.mask = 0xFF,
	.value.swid64 = CS_MOD_NF_DROP_SWID,
};

static cs_core_hmu_t nf_drop_hmu_entry = {
	.watch_bitmask = CS_CORE_HMU_WATCH_SWID64,
	.value_mask = &nf_drop_hmu_value,
	.callback = cs_hw_nf_drop_hmu_callback,
};

static spinlock_t nf_drop_lock;
struct list_head nf_drop_check_tbl;
typedef struct cs_hw_nf_drop_check_s {
	struct list_head node;
	u32 crc32;
	u16 crc16;
	u16 triggered;
	unsigned long last_use;
} cs_hw_nf_drop_check_t;

extern u32 cs_hw_nf_drop_check_try, cs_hw_nf_drop_check_life;

struct timer_list cs_hw_drop_timer_obj;
#define CS_HW_DROP_TIMER_PERIOD		(10)	/* seconds */

static void hw_nf_create_hash(cs_kernel_accel_cb_t *cb)
{
	u64 fwd_hm_flag;
	fe_sw_hash_t key;
	fe_fwd_result_entry_t fwdrslt_action;
	unsigned int fwdrslt_index;
	u32 crc32;
	u16 hash_index, crc16;
	int ret;
	memset(&fwdrslt_action, 0x0, sizeof(fwdrslt_action));
	memset(&key, 0x0, sizeof(key));

	/* use different hashmask based on L2 or L3 */
	if (cb->common.module_mask & CS_MOD_MASK_L3_RELATED) {
		DBG(printk("%s:using FWD_APP_TYPE_L3_GENERIC\n", __func__));
		if (cs_core_vtable_get_hashmask_flag_from_apptype(
					CORE_FWD_APP_TYPE_L3_GENERIC,
					&fwd_hm_flag))
			return;

		if (cs_core_vtable_get_hashmask_index_from_apptype(
					CORE_FWD_APP_TYPE_L3_GENERIC,
					&key.mask_ptr_0_7))
			return;
	} else {
		DBG(printk("%s:using FWD_APP_TYPE_L2_FLOW\n", __func__));
		if (cs_core_vtable_get_hashmask_flag_from_apptype(
					CORE_FWD_APP_TYPE_L2_FLOW,
					&fwd_hm_flag))
			return;

		if (cs_core_vtable_get_hashmask_index_from_apptype(
					CORE_FWD_APP_TYPE_L2_FLOW,
					&key.mask_ptr_0_7))
			return;
	}

	if (cs_core_set_hash_key(fwd_hm_flag, cb, &key, false))
		return;

	if (cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT))
		return;

	fwdrslt_action.act.drop = 1;
	if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwdrslt_action,
				&fwdrslt_index))
		return;

	if (cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
				(u16)fwdrslt_index, &hash_index)) {
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				fwdrslt_index, false);
		return;
	}

	ret = cs_core_hmu_add_hash(hash_index, 0, NULL);
	if (ret == 0) {
		cs_core_hmu_set_result_idx(hash_index, fwdrslt_index, 0);
	}
	/* watch this resource in HMU table */
	cs_core_hmu_link_src_and_hash(cb, hash_index, NULL);
	// any case for error handling?

	DBG(printk("%s:created a hash for this packet\n", __func__));

	return;
} /* hw_nf_create_hash */

static bool hw_nf_can_create_hash(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cb = CS_KERNEL_SKB_CB(skb);
	u64 fwd_hm_flag;
	fe_sw_hash_t key;
	u32 crc32;
	u16 crc16;
	cs_hw_nf_drop_check_t *nf_drop_check;

	if (cs_hw_nf_drop_check_try <= 1)
		return true;

	memset(&key, 0x0, sizeof(key));

	/* use different hashmask based on L2 or L3 */
	if (cb->common.module_mask & CS_MOD_MASK_L3_RELATED) {
		DBG(printk("%s:using FWD_APP_TYPE_L3_GENERIC\n", __func__));
		if (cs_core_vtable_get_hashmask_flag_from_apptype(
					CORE_FWD_APP_TYPE_L3_GENERIC,
					&fwd_hm_flag))
			return false;

		if (cs_core_vtable_get_hashmask_index_from_apptype(
					CORE_FWD_APP_TYPE_L3_GENERIC,
					&key.mask_ptr_0_7))
			return false;
	} else {
		DBG(printk("%s:using FWD_APP_TYPE_L2_FLOW\n", __func__));
		if (cs_core_vtable_get_hashmask_flag_from_apptype(
					CORE_FWD_APP_TYPE_L2_FLOW,
					&fwd_hm_flag))
			return false;

		if (cs_core_vtable_get_hashmask_index_from_apptype(
					CORE_FWD_APP_TYPE_L2_FLOW,
					&key.mask_ptr_0_7))
			return false;
	}

	if (cs_core_set_hash_key(fwd_hm_flag, cb, &key, false))
		return false;

	if (cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT))
		return false;

	spin_lock_bh(&nf_drop_lock);
	list_for_each_entry(nf_drop_check, &nf_drop_check_tbl, node) {
		if ((nf_drop_check->crc32 == crc32) &&
				(nf_drop_check->crc16 == crc16)) {
			/* found it.. check the number of trigger */
			if (nf_drop_check->triggered <
					(cs_hw_nf_drop_check_try - 1)) {
				nf_drop_check->triggered++;
				nf_drop_check->last_use = jiffies;
				spin_unlock_bh(&nf_drop_lock);
				return false;
			} else {
				/* we can now return true to create the
				 * hash for this NF_DROP; however, we need
				 * to clean the list node first */
				list_del_init(&nf_drop_check->node);
				spin_unlock_bh(&nf_drop_lock);
				cs_free(nf_drop_check);
				return true;
			}
		}
	}

	/* nothing has been found, so we are creating a node and insert
	 * it to the list */
	nf_drop_check = (cs_hw_nf_drop_check_t *)
		cs_zalloc(sizeof(cs_hw_nf_drop_check_t), GFP_ATOMIC);
	if (!nf_drop_check) {
		spin_unlock_bh(&nf_drop_lock);
		return false;
	}
	nf_drop_check->crc32 = crc32;
	nf_drop_check->crc16 = crc16;
	nf_drop_check->triggered = 1;
	nf_drop_check->last_use = jiffies;
	list_add_tail(&nf_drop_check->node, &nf_drop_check_tbl);
	spin_unlock_bh(&nf_drop_lock);

	return false;
} /* hw_nf_can_create_hash */

void cs_hw_nf_drop_handler(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_NF_DROP) == 0)
		return;

	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	if (hw_nf_can_create_hash(skb)) {
		cs_core_logic_add_swid64(cs_cb, CS_MOD_NF_DROP_SWID);

		hw_nf_create_hash(cs_cb);
	}

	return;
} /* cs_hw_nf_drop_handler */
EXPORT_SYMBOL(cs_hw_nf_drop_handler);

static void cs_hw_nf_drop_check_table_scan(void)
{
	cs_hw_nf_drop_check_t *nf_drop_check, *n;
	unsigned long life_in_jiffies;

	life_in_jiffies = secs_to_cputime(cs_hw_nf_drop_check_life);

	spin_lock_bh(&nf_drop_lock);
	list_for_each_entry_safe(nf_drop_check, n, &nf_drop_check_tbl, node) {
		if ((cs_hw_nf_drop_check_life != 0) &&
				(!time_in_range(jiffies, nf_drop_check->last_use,
						nf_drop_check->last_use +
						life_in_jiffies))) {
			list_del_init(&nf_drop_check->node);
			cs_free(nf_drop_check);
		}
	}
	spin_unlock_bh(&nf_drop_lock);

	mod_timer(&cs_hw_drop_timer_obj, jiffies +
			secs_to_cputime(CS_HW_DROP_TIMER_PERIOD));

} /* cs_hw_nf_drop_check_table_scan */

static void cs_hw_nf_timer_init(void)
{
	init_timer(&cs_hw_drop_timer_obj);
	cs_hw_drop_timer_obj.expires = jiffies +
		secs_to_cputime(CS_HW_DROP_TIMER_PERIOD);
	cs_hw_drop_timer_obj.data = (unsigned long)&cs_hw_drop_timer_obj;
	cs_hw_drop_timer_obj.function = (void *)&cs_hw_nf_drop_check_table_scan;
	add_timer(&cs_hw_drop_timer_obj);
} /* cs_hw_nf_timer_init */

void cs_hw_nf_drop_check_table_clean(void)
{
	cs_hw_nf_drop_check_t *nf_drop_check, *n;

	spin_lock_bh(&nf_drop_lock);
	list_for_each_entry_safe(nf_drop_check, n, &nf_drop_check_tbl, node) {
		list_del_init(&nf_drop_check->node);
		cs_free(nf_drop_check);
	}
	spin_unlock_bh(&nf_drop_lock);
	printk("\ndone cleaning NF_DROP check table!\n");
} /* cs_hw_nf_drop_check_table_clean */

void cs_hw_nf_drop_check_table_dump(void)
{
	cs_hw_nf_drop_check_t *nf_drop_check;
	int i = 1;

	printk("check try is %d times\n", cs_hw_nf_drop_check_try);
	printk("check life is %d seconds\n", cs_hw_nf_drop_check_life);

	spin_lock_bh(&nf_drop_lock);
	list_for_each_entry(nf_drop_check, &nf_drop_check_tbl, node)
		printk("entry#%d\n\tcrc32 = 0x%x, crc16 = 0x%04x, triggered "
				"%d times\n", i++, nf_drop_check->crc32,
				nf_drop_check->crc16, nf_drop_check->triggered);
	spin_unlock_bh(&nf_drop_lock);

	if (i == 1)
		printk("the table is empty\n");
	return;
} /* cs_hw_nf_drop_check_table_dump */

static void cs_hw_nf_drop_callback_hma(unsigned long notify_event,
		unsigned long value)
{
	DBG(printk("%s:%d:notify_event = 0x%lx, value = 0x%lx\n", __func__,
				__LINE__, notify_event, value));

	switch (notify_event) {
	case CS_HAM_ACTION_MODULE_DISABLE:
	case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
		cs_core_hmu_clean_watch(&nf_drop_hmu_entry);
		break;
	case CS_HAM_ACTION_MODULE_REMOVE:
		/*
		 * Do nothing.
		 * cs_hw_nf_drop_handler() will check
		 * CS752X_ADAPT_ENABLE_NF_DROP directly.
		 */
		break;
	case CS_HAM_ACTION_MODULE_INSERT:
		/*
		 * Do nothing.
		 * cs_hw_nf_drop_handler() will check
		 * CS752X_ADAPT_ENABLE_NF_DROP directly.
		 */
		break;
	}
} /* cs_hw_nf_drop_callback_hma */

static int cs_hw_nf_drop_hmu_callback(u32 watch_bitmask,
						cs_core_hmu_value_t * value,
						u32 status)
{
	/* we don't really need callback here!! */
	if (watch_bitmask == CS_CORE_HMU_WATCH_SWID64) {
		DBG(printk("%s() core_hmu_notify status=%x \n", __func__,
				status));

		if (value != NULL)
			DBG(printk("%s() type=%x, swid64=0x%016llx ", __func__,
					value->type, value->value.swid64));
		DBG(printk("\n"));
	}
	return 0;
}

int cs_hw_nf_drop_init(void)
{
	cs_core_hmu_register_watch(&nf_drop_hmu_entry);

	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_NF_DROP,
			cs_hw_nf_drop_callback_hma);
	spin_lock_init(&nf_drop_lock);
	INIT_LIST_HEAD(&nf_drop_check_tbl);
	cs_hw_nf_timer_init();

	return 0;
} /* cs_hw_nf_drop_init */

void cs_hw_nf_drop_exit(void)
{
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_NF_DROP,
			NULL);

	cs_core_hmu_unregister_watch(&nf_drop_hmu_entry);

	return;
} /* cs_hw_nf_drop_exit */

