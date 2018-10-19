/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_core_hmu.c
 *
 * $Id$
 *
 * It contains the core HMU implementation.
 */

#include <linux/list.h>
#include <linux/errno.h>
#include <linux/if_vlan.h>
#include <linux/sched.h>
#include "cs_hmu.h"
#include "cs_core_rule_hmu.h"
#include "cs_fe.h"
#include "cs_mut.h"

#define PFX     "CS_CORE_RULE_HMU"
#define PRINT(format, args...) printk(KERN_WARNING PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ne_core_logic_debug;
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_CORE_RULE_HMU) x;}
#else
#define DBG(x) { }
#endif

int hmu_get_key_by_hash_index(cs_hmu_table_t *p_hmu_tbl, u32 hash_index, char *p_key, void **data);

static cs_hmu_table_t *g_rule_hmu_table = NULL;
//static cs_core_rule_hmu_int_t *core_rule_hmu_list = NULL;

/* this internal free function only free value_mask, mask_in_str, and this
 * only data structure, but it doesn't free callback function list */
static void free_core_rule_hmu_int(void)
{
#if 0
	cs_core_hmu_value_t *v, *tmp;

	/* free internal data structure */
	v = int_core_hmu->value_mask;
	while (v != NULL) {
		tmp = v->next;
		cs_free(v);
		v = tmp;
	}

	if (int_core_hmu->mask_in_str != NULL)
		cs_free(int_core_hmu->mask_in_str);

	cs_free(int_core_hmu);
#endif
}

static int rule_hmu_callback(cs_hmu_table_t *table, unsigned char *key, u32 status)
{
	return CS_OK;
}

/* init and exit */
int cs_core_rule_hmu_init(void)
{
	cs_hmu_init();

	g_rule_hmu_table = cs_hmu_table_alloc(sizeof(cs_uint16_t), &rule_hmu_callback);
	if (g_rule_hmu_table == NULL) {
		printk("%s: Rule hash HMU table allocation failed!!\n", __func__);
		return -EPERM;
	}
	return CS_OK;
} /* cs_core_rule_hmu_init */

void cs_core_rule_hmu_exit(void)
{
	// FIXME!! implement
	//cs_rule_hmu_del_all_hash();
} /* cs_core_hmu_exit */

/* add the hash */
int cs_core_rule_hmu_add_hash(u32 hash_idx, void *data)
{
	return cs_hmu_add_hash(hash_idx, 0, data);  /* life time=0 means never timeout */
} /* cs_core_rule_hmu_add_hash */

/* delete hash based on the given value */
int cs_core_rule_hmu_delete_hash(cs_core_rule_hmu_data_t *p_data)
{
	int ret;

	ret = cs_hmu_del_hash_by_src(g_rule_hmu_table, (unsigned char *)p_data);
	//cs_free(key);

	return ret;
} /* cs_core_hmu_delete_hash */

/* delete the hash by hash_idx */
int cs_core_rule_hmu_delete_hash_by_idx(u32 hash_idx)
{
	return cs_hmu_del_hash_by_idx(hash_idx);
} /* cs_core_hmu_delete_hash_by_idx */

/* delete all the hashes */
int cs_core_rule_hmu_delete_all_hash(void)
{
	return cs_hmu_del_all_hash();
} /* cs_core_hmu_delete_all_hash */

int cs_core_rule_hmu_set_result_idx(u32 hash_idx, u32 result_idx, u8 result_type)
{
	return cs_hmu_set_result_idx(hash_idx, result_idx, result_type);
}

/* link HMU SRC based on info given in cb to hash of hash_idx */
int cs_core_rule_hmu_link_src_and_hash(u32 hash_idx, void *p_data)
{
	u8 *key;
	int ret;

	/* using hash_idx as the key of rule hash table */
	//key = (u8 *)p_data;
	key = (u8 *)&hash_idx;

	//debug_Aaron
	DBG(printk("%s: hash_idx=%d, key=0x%x\n", __func__, hash_idx, (unsigned int)key);)
 	ret = cs_hmu_create_link_src_and_hash(g_rule_hmu_table, key, hash_idx, p_data);

	DBG(cs_hmu_dump_table(g_rule_hmu_table, 1);)

	if (ret != CS_OK)
		printk("%s: Failed to link SRC and hash!!, ret=%d\n", __func__, ret);
	return ret;
} /* cs_core_rule_hmu_link_src_and_hash */

int cs_core_rule_hmu_get_data_by_hash_index(u32 hash_idx, unsigned char *p_key, void **p_data)
{
	int ret;
		
	ret = cs_hmu_get_hash_by_idx(hash_idx, p_data);

	if (ret != CS_OK) {
		printk("%s: Failed to get rule hmu data by hash index=%d\n", __func__, hash_idx);
		return ret;
	}

	return ret;
}
