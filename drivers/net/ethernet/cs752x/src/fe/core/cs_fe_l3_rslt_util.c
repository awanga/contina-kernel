/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_l3_rslt_util.c
 *
 * $Id: cs_fe_l3_rslt_util.c,v 1.9 2012/03/28 23:38:28 whsu Exp $
 *
 * It contains the assistance API for L3 IP result table.  The following
 * implementation perform crc16 on the value given for table search
 * efficiency.
 */
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include "cs_fe.h"
#include "crc.h"
#include "cs_mut.h"

typedef struct fe_l3_rslt_lookup_entry_s {
	u16 crc16;
	unsigned int rslt_index;
	bool is_v6;
	u32 ip_addr[4];
	struct list_head list;
	atomic_t users;
} fe_l3_rslt_lookup_entry_t;

static struct list_head fe_l3_rslt_lookup_table_base[FE_L3_ADDR_ENTRY_MAX << 2];
static spinlock_t fe_l3_rslt_lookup_table_lock;
// this is l3 result sw lookup index and hw entry index mapping table
//     15   14   13   12   11   10   9   8   7   6   5   4   3   2   1   0
//   +----+----+----+----+----+----+---+---+---+---+---+---+---+---+---+---+
//   | v6 |              |                      sw lookup index            |
//   +----+----+----+----+----+----+---+---+---+---+---+---+---+---+---+---+
// v6: 0 - ipv4
//     1 - ipv6
#define L3_MAP_TBL_IP_MASK          0x8000
#define L3_MAP_TBL_IP_V4            0x0000
#define L3_MAP_TBL_IP_V6            0x8000
static u16 fe_l3_rslt_lookup_and_entry_map_tbl[FE_L3_ADDR_ENTRY_MAX << 2];

int cs_fe_l3_result_alloc(u32 *ip_addr, bool is_v6, unsigned int *return_idx)
{
	int status = FE_TABLE_ENTRYNOTFOUND;
	u8 *data = (u8*)ip_addr;
	u16 crc16 = 0, crc_idx, maptbl_isv6 = 0;
	unsigned int l3_idx;
	u32 ip_addr_tmp[4];
	int i = 0;
	fe_l3_rslt_lookup_entry_t *entry;
	struct list_head *table;

	memset(ip_addr_tmp, 0, sizeof(ip_addr_tmp));

	for (i = 0; i < 4; i++) {
		crc16 = update_crc_ccitt(crc16, *data);
		data++;
	}
	crc_idx = crc16 >> 4;
	ip_addr_tmp[0] = *ip_addr;

	if (is_v6 == true) {
		for (i = 0; i < 12; i++) {
			crc16 = update_crc_ccitt(crc16, *data);
			data++;
		}
		crc_idx = crc16 >> 4;
		maptbl_isv6 = L3_MAP_TBL_IP_V6;
		memcpy(ip_addr_tmp, ip_addr, sizeof(ip_addr_tmp));
	}

	spin_lock(&fe_l3_rslt_lookup_table_lock);
	table = &fe_l3_rslt_lookup_table_base[crc_idx];
	if (!list_empty(table)) {
		list_for_each_entry(entry, table, list) {
			if ((entry->crc16 == crc16) &&
					(entry->is_v6 == is_v6) &&
					(memcmp(entry->ip_addr,
						ip_addr_tmp, 16) == 0)) {
				*return_idx = entry->rslt_index;
				atomic_inc(&entry->users);
				status = FE_TABLE_OK;
				goto exit;
			}
		}
	}

	/* if reach here, we will need to create a new entry for this
	 * allocation */
	entry = cs_zalloc(sizeof(fe_l3_rslt_lookup_entry_t), GFP_ATOMIC);
	if (entry == NULL) {
		status = FE_TABLE_ENOMEM;
		goto exit;
	}

	status = cs_fe_table_add_l3_ip(ip_addr, &l3_idx, is_v6);
	if (status != FE_TABLE_OK) {
		cs_free(entry);
		goto exit;
	}

	list_add_tail(&entry->list, table);
	memcpy(entry->ip_addr, ip_addr_tmp, sizeof(entry->ip_addr));
	entry->crc16 = crc16;
	entry->rslt_index = l3_idx;
	entry->is_v6 = is_v6;
	/* set mapping table */
	fe_l3_rslt_lookup_and_entry_map_tbl[l3_idx] = maptbl_isv6 | crc_idx;
	atomic_set(&entry->users, 1);
	*return_idx = l3_idx;
	status = FE_TABLE_OK;
exit:
	spin_unlock(&fe_l3_rslt_lookup_table_lock);
	return status;
}/* cs_fe_l3_result_alloc */

int cs_fe_l3_result_dealloc(u16 l3_idx)
{
	u16 crc_idx = 0, maptbl_isv6 = 0;
	int status = FE_TABLE_ENTRYNOTFOUND;
	struct list_head *table;
	fe_l3_rslt_lookup_entry_t *entry = NULL;
	bool is_v6 = false;

	crc_idx = fe_l3_rslt_lookup_and_entry_map_tbl[l3_idx] & 0x0FFF;
	maptbl_isv6 = fe_l3_rslt_lookup_and_entry_map_tbl[l3_idx] &
		L3_MAP_TBL_IP_MASK;
	if (maptbl_isv6 == L3_MAP_TBL_IP_V6)
		is_v6 = true;

	table = &fe_l3_rslt_lookup_table_base[crc_idx];
	if (list_empty(table))
		return status;

	spin_lock(&fe_l3_rslt_lookup_table_lock);

	list_for_each_entry(entry, table, list) {
		if ((entry->rslt_index == l3_idx) && (entry->is_v6 == is_v6)) {
			status = FE_TABLE_OK;
			if (atomic_dec_and_test(&entry->users)) {
				status = cs_fe_table_del_l3_ip(l3_idx, is_v6);
				if (status == FE_TABLE_OK) {
					list_del(&entry->list);
					cs_free(entry);
					fe_l3_rslt_lookup_and_entry_map_tbl[
						l3_idx] = 0;
				}
			}
			goto exit;
		}
	}

exit:
	spin_unlock(&fe_l3_rslt_lookup_table_lock);
	return status;
}/* cs_fe_l3_result_dealloc */

int fe_l3_rslt_lookup_table_init(void)
{
	int i;

	for (i = 0; i < (FE_L3_ADDR_ENTRY_MAX << 2); i++)
		INIT_LIST_HEAD(&fe_l3_rslt_lookup_table_base[i]);
	memset(fe_l3_rslt_lookup_and_entry_map_tbl, 0,
			FE_L3_ADDR_ENTRY_MAX << 3);
	spin_lock_init(&fe_l3_rslt_lookup_table_lock);
	return FE_TABLE_OK;
} /* fe_l3_rslt_lookup_table_init */
EXPORT_SYMBOL(fe_l3_rslt_lookup_table_init);

void cs_fe_l3_result_print_counter(void)
{
	int i, c_ipv4 = 0, c_ipv6 = 0;
	struct list_head *table;
	unsigned long start_time = jiffies;
	fe_l3_rslt_lookup_entry_t *entry = NULL;

	spin_lock(&fe_l3_rslt_lookup_table_lock);
	for (i = 0; i < (FE_L3_ADDR_ENTRY_MAX << 2); i++) {
		table = &fe_l3_rslt_lookup_table_base[i];
		if (!list_empty(table)) {
			list_for_each_entry(entry, table, list) {
				if (entry->is_v6 == true)
					c_ipv6++;
				else
					c_ipv4++;
			}
		}
	}
	spin_unlock(&fe_l3_rslt_lookup_table_lock);
	start_time = jiffies - start_time;
	printk("\t\t\t\tipv4 = %d, ipv6 = %d, scan period (%ld)\n", c_ipv4,
			c_ipv6, start_time);
}/* fe_l3_rslt_lookup_table_counter */
EXPORT_SYMBOL(cs_fe_l3_result_print_counter);

//int cs_fe_l3_result_dealloc_all(void)
//{
//	int i;
//	spin_lock(&L3_IP_LOCK);
//	for (i=0; i<4096; i++) {
//		if (fe_l3_rslt_lookup_table_base[i])
//			cs_free(fe_l3_rslt_lookup_table_base[i]);
//	}
//	spin_unlock(&L3_IP_LOCK);
//	return FE_STATUS_OK;
//}/* cs_fe_l3_result_dealloc_all */

