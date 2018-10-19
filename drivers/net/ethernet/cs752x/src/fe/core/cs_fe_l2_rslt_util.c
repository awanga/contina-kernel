/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_l2_rslt_util.c
 *
 * $Id: cs_fe_l2_rslt_util.c,v 1.6 2011/12/15 23:23:07 whsu Exp $
 *
 * It contains the assistance API for L2 MAC result table.  The
 * following implementation perform crc16 on the value given for table
 * search efficiency.
 */
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include "cs_fe.h"
#include "crc.h"
#include "cs_mut.h"

typedef struct fe_l2_rslt_lookup_entry_s {
	u16 crc16;
	unsigned int rslt_index;
	u8 mac_addr[12];
	struct list_head list;
	atomic_t users;
} fe_l2_rslt_lookup_entry_t;

static struct list_head fe_l2_rslt_sa_lookup_base[FE_L2_ADDR_PAIR_ENTRY_MAX];
static struct list_head fe_l2_rslt_da_lookup_base[FE_L2_ADDR_PAIR_ENTRY_MAX];
static struct list_head fe_l2_rslt_pair_lookup_base[FE_L2_ADDR_PAIR_ENTRY_MAX];

static spinlock_t fe_l2_rslt_sa_lookup_table_lock;
static spinlock_t fe_l2_rslt_da_lookup_table_lock;
static spinlock_t fe_l2_rslt_pair_lookup_table_lock;

static u16 fe_l2_rslt_sa_lookup_map_tbl[FE_L2_ADDR_PAIR_ENTRY_MAX];
static u16 fe_l2_rslt_da_lookup_map_tbl[FE_L2_ADDR_PAIR_ENTRY_MAX];
static u16 fe_l2_rslt_pair_lookup_map_tbl[FE_L2_ADDR_PAIR_ENTRY_MAX];

/* this is l2 result sw lookup index and hw entry index mapping table
 *     15   14   13   12   11   10   9   8   7   6   5   4   3   2   1   0
 *   +----+----+----+----+----+----+---+---+---+---+---+---+---+---+---+---+
 *   |                        |  type  |          sw lookup index          |
 *   +----+----+----+----+----+----+---+---+---+---+---+---+---+---+---+---+
 * type: 11 - fe_l2_rslt_pair_lookup_base
 *       01 - fe_l2_rslt_da_lookup_base
 *       10 - fe_l2_rslt_sa_lookup_base */
#define L2_MAP_TBL_TYPE_MASK        0x0600
#define L2_MAP_TBL_TYPE_PAIR        0x0600
#define L2_MAP_TBL_TYPE_DA          0x0200
#define L2_MAP_TBL_TYPE_SA          0x0400

int cs_fe_l2_result_alloc(unsigned char *mac_addr, unsigned char type,
		unsigned int *return_idx)
{
	u16 crc16 = 0, idx = 0, maptbl_type = 0;
	unsigned int l2_idx;
	int status = FE_TABLE_ENTRYNOTFOUND;
	int i, length;
	unsigned char mac_combo[12], *mac_sa, *mac_da;
	spinlock_t *table_lock;
	struct list_head *table;
	fe_l2_rslt_lookup_entry_t *entry;
	u16 *map_table;

	memset(mac_combo, 0, sizeof(mac_combo));

	switch (type) {
	case L2_LOOKUP_TYPE_PAIR:
		length = 12;
		table = &fe_l2_rslt_pair_lookup_base[0];
		map_table = &fe_l2_rslt_pair_lookup_map_tbl[0];
		table_lock = &fe_l2_rslt_pair_lookup_table_lock;
		memcpy(mac_combo, mac_addr, length);
		maptbl_type = L2_MAP_TBL_TYPE_PAIR;
		mac_sa = mac_addr + 6;
		mac_da = mac_addr;
		break;
	case L2_LOOKUP_TYPE_SA:
		length = 6;
		table = &fe_l2_rslt_sa_lookup_base[0];
		map_table = &fe_l2_rslt_sa_lookup_map_tbl[0];
		table_lock = &fe_l2_rslt_sa_lookup_table_lock;
		memcpy(mac_combo, mac_addr, length);
		maptbl_type = L2_MAP_TBL_TYPE_SA;
		mac_sa = mac_addr;
		mac_da = NULL;
		break;
	case L2_LOOKUP_TYPE_DA:
		length = 6;
		table = &fe_l2_rslt_da_lookup_base[0];
		map_table = &fe_l2_rslt_da_lookup_map_tbl[0];
		table_lock = &fe_l2_rslt_da_lookup_table_lock;
		memcpy(mac_combo, mac_addr, length);
		maptbl_type = L2_MAP_TBL_TYPE_DA;
		mac_sa = NULL;
		mac_da = mac_addr;
		break;
	default:
		return FE_TABLE_EOPNOTSUPP;
	}

	for (i = 0; i < length; i++)
		crc16 = update_crc_ccitt(crc16, *(mac_addr + i));

	idx = crc16 & (FE_L2_ADDR_PAIR_ENTRY_MAX - 1);
	spin_lock(table_lock);
	table += idx;

	if (!list_empty(table)) {
		list_for_each_entry(entry, table, list) {
			if ((entry->crc16 == crc16) &&
					(memcmp(entry->mac_addr, mac_combo,
						length) == 0)) {
				*return_idx = entry->rslt_index;
				atomic_inc(&entry->users);
				status = FE_TABLE_OK;
				goto exit;
			}
		}
	}

	/* if reach here, we will need to create a new entry for this
	 * allocation */
	entry = cs_zalloc(sizeof(fe_l2_rslt_lookup_entry_t), GFP_ATOMIC);
	if (entry == NULL) {
		status = FE_TABLE_ENOMEM;
		goto exit;
	}

	status = cs_fe_table_add_l2_mac(mac_sa, mac_da, &l2_idx);
	if (status != FE_TABLE_OK) {
		cs_free(entry);
		goto exit;
	}

	list_add_tail(&entry->list, table);
	entry->crc16 = crc16;
	entry->rslt_index = l2_idx;
	memcpy(entry->mac_addr, mac_combo, length);
	/* set mapping table */
	map_table[l2_idx] = maptbl_type | idx;
	atomic_set(&entry->users, 1);
	*return_idx = l2_idx;
	status = FE_TABLE_OK;

exit:
	spin_unlock(table_lock);
	return status;
}/* cs_fe_l2_result_alloc */


int cs_fe_l2_result_dealloc(unsigned int l2_idx, unsigned char type)
{
	unsigned int crc_idx = 0, maptbl_type = 0;
	int status = FE_TABLE_ENTRYNOTFOUND;
	int length;
	spinlock_t *table_lock;
	struct list_head *table;
	fe_l2_rslt_lookup_entry_t *entry = NULL;
	bool f_sa, f_da;
	u16 *map_table;

	switch (type) {
	case L2_LOOKUP_TYPE_PAIR:
		crc_idx = fe_l2_rslt_pair_lookup_map_tbl[l2_idx] &
			(FE_L2_ADDR_PAIR_ENTRY_MAX - 1);
		maptbl_type = fe_l2_rslt_pair_lookup_map_tbl[l2_idx] &
			L2_MAP_TBL_TYPE_MASK;
		if (maptbl_type != L2_MAP_TBL_TYPE_PAIR)
			return FE_TABLE_ENTRYNOTFOUND;
		map_table = &fe_l2_rslt_pair_lookup_map_tbl[0];
		length = 12;
		table = &fe_l2_rslt_pair_lookup_base[crc_idx];
		table_lock = &fe_l2_rslt_pair_lookup_table_lock;
		f_sa = f_da = true;
		break;
	case L2_LOOKUP_TYPE_SA:
		crc_idx = fe_l2_rslt_sa_lookup_map_tbl[l2_idx] &
			(FE_L2_ADDR_PAIR_ENTRY_MAX - 1);
		maptbl_type = fe_l2_rslt_sa_lookup_map_tbl[l2_idx] &
			L2_MAP_TBL_TYPE_MASK;
		if (maptbl_type != L2_MAP_TBL_TYPE_SA)
			return FE_TABLE_ENTRYNOTFOUND;
		map_table = &fe_l2_rslt_sa_lookup_map_tbl[0];
		length = 6;
		table = &fe_l2_rslt_sa_lookup_base[crc_idx];
		table_lock = &fe_l2_rslt_sa_lookup_table_lock;
		f_sa = true;
		f_da = false;
		break;
	case L2_LOOKUP_TYPE_DA:
		crc_idx = fe_l2_rslt_da_lookup_map_tbl[l2_idx] &
			(FE_L2_ADDR_PAIR_ENTRY_MAX - 1);
		maptbl_type = fe_l2_rslt_da_lookup_map_tbl[l2_idx] &
			L2_MAP_TBL_TYPE_MASK;
		if (maptbl_type != L2_MAP_TBL_TYPE_DA)
			return FE_TABLE_ENTRYNOTFOUND;
		map_table = &fe_l2_rslt_da_lookup_map_tbl[0];
		length = 6;
		table = &fe_l2_rslt_da_lookup_base[crc_idx];
		table_lock = &fe_l2_rslt_da_lookup_table_lock;
		f_sa = false;
		f_da = true;
		break;
	default:
		return FE_TABLE_EOPNOTSUPP;
	}

	spin_lock(table_lock);
	if (list_empty(table))
		goto exit;

	list_for_each_entry(entry, table, list) {
		if (entry->rslt_index == l2_idx) {
			status = FE_TABLE_OK;
			if (atomic_dec_and_test(&entry->users)) {
				status = cs_fe_table_del_l2_mac(l2_idx, f_sa,
						f_da);
				if (status == FE_TABLE_OK) {
					list_del(&entry->list);
					map_table[l2_idx] = 0;
					cs_free(entry);
				}
			}
			goto exit;
		}
	}

exit:
	spin_unlock(table_lock);
    return status;
}/* cs_fe_l2_result_dealloc */

int fe_l2_rslt_lookup_table_init(void)
{
	int i;

	for (i = 0; i < FE_L2_ADDR_PAIR_ENTRY_MAX; i++) {
		INIT_LIST_HEAD(&fe_l2_rslt_sa_lookup_base[i]);
		INIT_LIST_HEAD(&fe_l2_rslt_da_lookup_base[i]);
		INIT_LIST_HEAD(&fe_l2_rslt_pair_lookup_base[i]);
	}

	spin_lock_init(&fe_l2_rslt_sa_lookup_table_lock);
	spin_lock_init(&fe_l2_rslt_da_lookup_table_lock);
	spin_lock_init(&fe_l2_rslt_pair_lookup_table_lock);

	memset(fe_l2_rslt_sa_lookup_map_tbl, 0, FE_L2_ADDR_PAIR_ENTRY_MAX << 1);
	memset(fe_l2_rslt_da_lookup_map_tbl, 0, FE_L2_ADDR_PAIR_ENTRY_MAX << 1);
	memset(fe_l2_rslt_pair_lookup_map_tbl, 0,
			FE_L2_ADDR_PAIR_ENTRY_MAX << 1);

	return FE_STATUS_OK;
} /* fe_l2_rslt_lookup_table_init */
EXPORT_SYMBOL(fe_l2_rslt_lookup_table_init);

/* FIXME!! any release implementation is needed? */

//cs_status fe_rslt_lookup_table_release(fe_rslt_lookup_bucket_s* table_base[],
//	cs_uint16 size)
//{
//	int i;
//	for (i=0; i<size; i++) {
//		if(table_base[i])
//			cs_free(table_base[i]);
//	}
//	return FE_STATUS_OK;
//}
//
//cs_status cs_fe_l2_result_dealloc_all(void)
//{
//	spin_lock(&fe_l2_rslt_sa_lookup_table_lock);
//	fe_rslt_lookup_table_release(fe_l2_rslt_sa_lookup_base, FE_L2_ADDR_PAIR_ENTRY_MAX);
//	spin_unlock(&fe_l2_rslt_sa_lookup_table_lock);
//
//	spin_lock(&fe_l2_rslt_da_lookup_table_lock);
//	fe_rslt_lookup_table_release(fe_l2_rslt_da_lookup_base, FE_L2_ADDR_PAIR_ENTRY_MAX);
//	spin_unlock(&fe_l2_rslt_da_lookup_table_lock);
//
//	spin_lock(&fe_l2_rslt_pair_lookup_table_lock);
//	fe_rslt_lookup_table_release(fe_l2_rslt_pair_lookup_base, FE_L2_ADDR_PAIR_ENTRY_MAX);
//	spin_unlock(&fe_l2_rslt_pair_lookup_table_lock);
//
//	return FE_STATUS_OK;
//}/* cs_fe_l2_result_dealloc_all */


