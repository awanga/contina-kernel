/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_l2mac.c
 *
 * $Id: cs_fe_table_l2mac.c,v 1.7 2011/12/20 01:52:09 whsu Exp $
 *
 * It contains L2 Result Table Management APIs.
 */

/* L2 Table Management:
 * Each entry in the table contains both SA and DA.  In the hash result,
 * it can specify to use one or both of the entry with given index. */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"
#include "cs_mut.h"

static cs_fe_table_t cs_fe_l2mac_table_type;

int L2_SA_PTR = 0, L2_DA_PTR = 0, L2_PAIR_PTR = 0;
#define FE_L2MAC_TABLE_PTR	(cs_fe_l2mac_table_type.content_table)
#define FE_L2MAC_LOCK		&(cs_fe_l2mac_table_type.lock)
#define FE_L2MAC_ENTRY_USED	(0x8000)

static int fe_l2mac_get_entry(unsigned int idx, void *entry);

static int fe_l2mac_add_entry(unsigned int idx, unsigned char *p_sa,
		unsigned char *p_da)
{
	cs_table_entry_t *p_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l2_addr_pair_entry_t *p_l2_entry;
	int ret;
	unsigned long flags;

	p_entry = cs_table_get_entry(FE_L2MAC_TABLE_PTR, idx);
	if (p_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	spin_lock_irqsave(FE_L2MAC_LOCK, flags);
	if (p_entry->data == NULL) {
		p_entry->data = fe_table_malloc_table_entry(
				&cs_fe_l2mac_table_type);
		if (p_entry->data == NULL) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return -ENOMEM;
		}
	}
	p_fe_entry = (fe_table_entry_t*)p_entry->data;
	p_l2_entry = (fe_l2_addr_pair_entry_t*)p_fe_entry->p_entry;

	if ((p_sa != NULL) && (p_l2_entry->sa_count & FE_L2MAC_ENTRY_USED)) {
		p_l2_entry->sa_count++;
		memcpy((&(p_l2_entry->mac_sa[0])), p_sa, 6);
		/* write to hardware */
		ret = cs_fe_hw_table_set_field_value(FE_TABLE_L2_MAC, idx,
				FWD_L2_MAC_SA, (__u32*)p_sa);
		if (ret != FE_TABLE_OK) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return ret;
		}
	}

	if ((p_da != NULL) && (p_l2_entry->da_count & FE_L2MAC_ENTRY_USED)) {
		p_l2_entry->da_count++;
		memcpy((&(p_l2_entry->mac_da[0])), p_da, 6);
		/* write to hardware */
		ret = cs_fe_hw_table_set_field_value(FE_TABLE_L2_MAC, idx,
				FWD_L2_MAC_DA, (__u32*)p_da);
		if (ret != FE_TABLE_OK) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return ret;
		}
	}
	atomic_inc(&p_fe_entry->users);
	spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l2mac_add_entry */

static int fe_l2mac_find_avail_index(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	cs_table_entry_t *p_entry;
	fe_l2_addr_pair_entry_t *p_l2mac;
	fe_table_entry_t *p_fe_entry;
	int f_sa_match, f_da_match, boundary;
	bool f_found = false;
	unsigned long flags;

	if (FE_L2MAC_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;
	if ((p_sa != NULL) && (p_da != NULL))
		boundary = L2_PAIR_PTR;
	else if (p_sa != NULL)
		boundary = L2_SA_PTR;
	else // if (p_da != NULL)
		boundary = L2_DA_PTR;

	spin_lock_irqsave(FE_L2MAC_LOCK, flags);
	*p_idx = boundary;
	do {
		if ((*p_idx) == cs_fe_l2mac_table_type.max_entry)
			*p_idx = 0;

		p_entry = cs_table_get_entry(FE_L2MAC_TABLE_PTR, *p_idx);
		if (p_entry == NULL) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return FE_TABLE_ENULLPTR;
		}
		if ((p_entry->local_data & FE_TABLE_ENTRY_USED) == 0) {
			p_entry->local_data |= FE_TABLE_ENTRY_USED;
			if (p_entry->data == NULL) {
				p_entry->data = fe_table_malloc_table_entry(
						&cs_fe_l2mac_table_type);
				if (p_entry->data == NULL) {
					spin_unlock_irqrestore(FE_L2MAC_LOCK,
							flags);
					return -ENOMEM;
				}
			}

			p_fe_entry = (fe_table_entry_t*)p_entry->data;
			p_l2mac = (fe_l2_addr_pair_entry_t*)p_fe_entry->p_entry;

			if ((p_sa != NULL) && (p_da != NULL)) {
				if (L2_PAIR_PTR == L2_SA_PTR)
					L2_SA_PTR = (*p_idx) + 1;
				if (L2_PAIR_PTR == L2_DA_PTR)
					L2_DA_PTR = (*p_idx) + 1;
				L2_PAIR_PTR = (*p_idx) + 1;
				p_l2mac->sa_count |= FE_L2MAC_ENTRY_USED;
				p_l2mac->da_count |= FE_L2MAC_ENTRY_USED;
			} else if (p_sa != NULL) {
				if (L2_PAIR_PTR == L2_SA_PTR)
					L2_PAIR_PTR = (*p_idx) + 1;
				L2_SA_PTR = (*p_idx) + 1;
				p_l2mac->sa_count |= FE_L2MAC_ENTRY_USED;
			} else if (p_da != NULL) {
				if (L2_PAIR_PTR == L2_DA_PTR)
					L2_PAIR_PTR = (*p_idx) + 1;
				L2_DA_PTR = (*p_idx) + 1;
				p_l2mac->da_count |= FE_L2MAC_ENTRY_USED;
			}
			cs_fe_l2mac_table_type.used_entry++;
			f_found = true;
		} else {
			/*
			 * when an entry has been used before, it could be just
			 * MAC_SA or MAC_DA. In this case, we can check the
			 * content, to see if we can share the entry.
			 */
			p_fe_entry = (fe_table_entry_t*)p_entry->data;
			if (p_fe_entry == NULL) {
				spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
				return FE_TABLE_ENULLPTR;
			}
			p_l2mac = (fe_l2_addr_pair_entry_t*)p_fe_entry->p_entry;
			if (p_l2mac == NULL) {
				spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
				return FE_TABLE_ENULLPTR;
			}
			if ((p_sa != NULL) && (p_da != NULL)) {
				f_sa_match = memcmp(&p_l2mac->mac_sa[0], p_sa,
						6);
				f_da_match = memcmp(&p_l2mac->mac_da[0], p_da,
						6);
				if ((f_sa_match == 0) &&
						(p_l2mac->da_count == 0)) {
					L2_SA_PTR = (*p_idx) + 1;
					L2_PAIR_PTR = (*p_idx) + 1;
					p_l2mac->da_count |=
						FE_L2MAC_ENTRY_USED;
					f_found = true;
					spin_unlock_irqrestore(FE_L2MAC_LOCK,
							flags);
					return FE_TABLE_OK;
				} else if ((f_da_match == 0) &&
						(p_l2mac->sa_count == 0)) {
					L2_DA_PTR = (*p_idx) + 1;
					L2_PAIR_PTR = (*p_idx) + 1;
					p_l2mac->sa_count |=
						FE_L2MAC_ENTRY_USED;
					f_found = true;
					spin_unlock_irqrestore(FE_L2MAC_LOCK,
							flags);
					return FE_TABLE_OK;
				}
			} else if (p_sa != NULL) {
				if (p_l2mac->sa_count == 0) {
					L2_SA_PTR = (*p_idx) + 1;
					p_l2mac->sa_count |=
						FE_L2MAC_ENTRY_USED;
					f_found = true;
					spin_unlock_irqrestore(FE_L2MAC_LOCK,
							flags);
					return FE_TABLE_OK;
				}
			} else if (p_da != NULL) {
				if (p_l2mac->da_count == 0) {
					L2_DA_PTR = (*p_idx) + 1;
					p_l2mac->da_count |=
						FE_L2MAC_ENTRY_USED;
					f_found = true;
					spin_unlock_irqrestore(FE_L2MAC_LOCK,
							flags);
					return FE_TABLE_OK;
				}
			}
			(*p_idx)++;

			if ((*p_idx) == boundary) {
				/*
				 * we have already gone around the world and
				 * found nothing available.
				 */
				(*p_idx) = 0;
				spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
				return FE_TABLE_ETBLFULL;
			}
		}
	} while (f_found == false);

	spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
	return FE_TABLE_OK;
} /* fe_l2mac_find_avail_index */


static int fe_l2mac_add(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	int ret;

	if (p_idx == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_sa == NULL) && (p_da == NULL))
		return FE_TABLE_ENULLPTR;

	ret = fe_l2mac_find_avail_index(p_sa, p_da, p_idx);
	if (ret != FE_TABLE_OK)
		return ret;

	return fe_l2mac_add_entry((*p_idx), p_sa, p_da);
} /* fe_l2mac_add */

static int fe_l2mac_del(unsigned int idx, bool f_sa, bool f_da)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l2_addr_pair_entry_t *p_l2_entry;
	int ret;
	unsigned long flags;

	if (idx >= cs_fe_l2mac_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_L2MAC_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_L2MAC_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't decrement the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	p_l2_entry = (fe_l2_addr_pair_entry_t*)p_fe_entry->p_entry;
	if (p_l2_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_L2MAC_LOCK, flags);
	/* since this table is maintained by software, the only thing we need
	 * to maintain in deletion is whether this SA or DA is enabled. */
	if ((f_sa == true) &&
			((p_l2_entry->sa_count & ~FE_L2MAC_ENTRY_USED) != 0))
		p_l2_entry->sa_count--;
	if ((f_da == true) &&
			((p_l2_entry->da_count & ~FE_L2MAC_ENTRY_USED) != 0))
		p_l2_entry->da_count--;

	atomic_dec(&p_fe_entry->users);

	if ((f_sa == true) && (p_l2_entry->sa_count == FE_L2MAC_ENTRY_USED)) {
		memset(&(p_l2_entry->mac_sa[0]), 0x00, 6);
		p_l2_entry->sa_count = 0;
		/* WRITE TO HARDWARE */
		ret = cs_fe_hw_table_set_field_value(FE_TABLE_L2_MAC, idx,
				FWD_L2_MAC_SA,
				(__u32 *)(&(p_l2_entry->mac_sa[0])));
		if (ret != FE_TABLE_OK) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return ret;
		}
	}

	if ((f_da == true) && (p_l2_entry->da_count == FE_L2MAC_ENTRY_USED)) {
		memset(&(p_l2_entry->mac_da[0]), 0x00, 6);
		p_l2_entry->da_count = 0;
		/* WRITE TO HARDWARE */
		ret = cs_fe_hw_table_set_field_value(FE_TABLE_L2_MAC, idx,
				FWD_L2_MAC_DA,
				(__u32 *)(&(p_l2_entry->mac_da[0])));
		if (ret != FE_TABLE_OK) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return ret;
		}
	}

	if ((p_l2_entry->sa_count == 0) && (p_l2_entry->da_count == 0)) {
		atomic_set(&p_fe_entry->users, 0);
		cs_free(p_fe_entry->p_entry);
		cs_free(p_sw_entry->data);
		p_sw_entry->data = NULL;
		cs_fe_l2mac_table_type.used_entry--;
		p_sw_entry->local_data &= ~FE_TABLE_ENTRY_USED;
		ret = cs_fe_hw_table_clear_entry(FE_TABLE_L2_MAC, idx);
		if (ret != 0) {
			spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);
			return ret;
		}
	}

	spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);

	return FE_TABLE_OK;	/* OK if you don't want to delete anything */
} /* fe_l2mac_del */

static int fe_l2mac_find(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	fe_l2_addr_pair_entry_t l2_entry;
	int ret;
	unsigned int iii;

	if (p_idx == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sa == NULL) && (p_da == NULL))
		return FE_TABLE_ENULLPTR;

	for (iii = 0; iii < cs_fe_l2mac_table_type.max_entry; iii++) {
		ret = fe_l2mac_get_entry(iii, &l2_entry);
		if (ret == FE_TABLE_OK) {
			if ((p_sa != NULL) && (p_da != NULL) &&
					(memcmp(&l2_entry.mac_sa[0], p_sa,
						6) == 0) &&
					(memcmp(&l2_entry.mac_da[0], p_da,
						6) == 0)) {
				*p_idx = iii;
				return FE_TABLE_OK;
			} else if ((p_sa != NULL) && (p_da == NULL) &&
					(memcmp(&l2_entry.mac_sa[0], p_sa,
						6) == 0)) {
				*p_idx = iii;
				return FE_TABLE_OK;
			} else if ((p_sa == NULL) && (p_da != NULL) &&
					(memcmp(&l2_entry.mac_da[0], p_da,
						6) == 0)) {
				*p_idx = iii;
				return FE_TABLE_OK;
			}
		}
	}

	return FE_TABLE_ENTRYNOTFOUND;
} /* fe_l2mac_find */

static int fe_l2mac_get(unsigned int idx, unsigned char *p_sa,
		unsigned char *p_da)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l2_addr_pair_entry_t *p_l2_entry;
	unsigned long flags;

	if (idx >= cs_fe_l2mac_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_L2MAC_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if ((p_sa == NULL) && (p_da == NULL))
		return FE_TABLE_ENULLPTR;

	p_sw_entry = cs_table_get_entry(FE_L2MAC_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	p_l2_entry = (fe_l2_addr_pair_entry_t*)p_fe_entry->p_entry;
	if (p_l2_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_L2MAC_LOCK, flags);
	if (p_sa != NULL) memcpy(p_sa, (&(p_l2_entry->mac_sa[0])), 6);
	if (p_da != NULL) memcpy(p_da, (&(p_l2_entry->mac_da[0])), 6);
	spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l2mac_get */

static int fe_l2mac_inc_refcnt(unsigned int idx, bool f_sa, bool f_da)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l2_addr_pair_entry_t *p_l2_entry;
	unsigned long flags;

	if (idx >= cs_fe_l2mac_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_L2MAC_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if ((f_sa == false) && (f_da == false))
		return FE_TABLE_OK;

	p_sw_entry = cs_table_get_entry(FE_L2MAC_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't increment the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	p_l2_entry = (fe_l2_addr_pair_entry_t*)p_fe_entry->p_entry;
	if (p_l2_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_L2MAC_LOCK, flags);
	if ((f_sa == true) && (p_l2_entry->sa_count & FE_L2MAC_ENTRY_USED))
		p_l2_entry->sa_count++;
	if ((f_da == true) && (p_l2_entry->da_count & FE_L2MAC_ENTRY_USED))
		p_l2_entry->da_count++;
	atomic_inc(&p_fe_entry->users);
	spin_unlock_irqrestore(FE_L2MAC_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l2mac_inc_refcnt */

static int fe_l2mac_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_l2mac_table_type, idx, f_force);
} /* fe_l2mac_del_entry_by_idx */

static int fe_l2mac_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_l2mac_table_type, idx, entry);
} /* fe_l2mac_get_entry */

static int fe_l2mac_flush_table(void)
{
	L2_SA_PTR = 0;
	L2_DA_PTR = 0;
	L2_PAIR_PTR = 0;
	return fe_table_flush_table(&cs_fe_l2mac_table_type);
} /* fe_l2mac_flush_table */

static int fe_l2mac_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_l2mac_table_type);
} /* fe_l2mac_get_avail_count */

static void _fe_l2mac_print_entry(unsigned int idx)
{
	fe_l2_addr_pair_entry_t l2_entry, *p_l2_entry;
	__u8 sa_array[6], da_array[6];
	__u8 parity;
	int status;
	unsigned int j;

	status = fe_l2mac_get_entry(idx, &l2_entry);
	if (status == FE_TABLE_EENTRYNOTRSVD) {
		printk("| %03d | NOT USED\n", idx);
		printk("|-------------------------------------------|\n");
	}
	if (status != FE_TABLE_OK) {
		//printk("%s:%d:got here, idx = %d, status = %d", __func__,
		//		__LINE__, idx, status);
		return;
	}

	p_l2_entry = &l2_entry;

	printk("| %03d | 0x", idx);
	for (j = 0; j < 6; j++) printk("%02x", p_l2_entry->mac_sa[j]);
	printk(" | %11d | 0x", p_l2_entry->sa_count & ~FE_L2MAC_ENTRY_USED);
	for (j = 0; j < 6; j++) printk("%02x", p_l2_entry->mac_da[j]);
	printk(" | %11d | 0x", p_l2_entry->da_count & ~FE_L2MAC_ENTRY_USED);

	/* read the value from HW */
	cs_fe_hw_table_get_field_value(FE_TABLE_L2_MAC, idx,
			FWD_L2_MAC_SA, (cs_uint32*)sa_array);
	cs_fe_hw_table_get_field_value(FE_TABLE_L2_MAC, idx,
			FWD_L2_MAC_DA, (cs_uint32*)da_array);
	cs_fe_hw_table_get_field_value(FE_TABLE_L2_MAC, idx,
			FWD_L2_MEM_PARITY, (cs_uint32*)&parity);

	/* done reading value from HW, output them! */
	printk("%02x%02x%02x%02x%02x%02x | ", sa_array[0], sa_array[1],
			sa_array[2], sa_array[3], sa_array[4], sa_array[5]);
	printk("0x%02x%02x%02x%02x%02x%02x |\n", da_array[0],
			da_array[1], da_array[2], da_array[3], da_array[4],
			da_array[5]);
	printk("|------------------------------------------------------|\n");
} /* _fe_l2mac_print_entry */

static void fe_l2mac_print_entry(unsigned int idx)
{
	if (idx >= FE_L2_ADDR_PAIR_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	printk("\n\n ------------------ L2 Result Table ---------------\n");
	printk("|------------------------------------------------------");
	printk("-----------------------------------------------|\n");
	printk("| idx |     SA (sw)    | SA cnt (sw) |     DA (sw)    |");
	printk(" DA cnt (sw) |     SA (hw)    |     DA (hw)    |\n");
	printk("|------------------------------------------------------");
	printk("-----------------------------------------------|\n");

	_fe_l2mac_print_entry(idx);
} /* fe_l2mac_print_entry */

static void fe_l2mac_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_L2_ADDR_PAIR_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	printk("\n\n ------------------ L2 Result Table ---------------\n");
	printk("|------------------------------------------------------");
	printk("-----------------------------------------------|\n");
	printk("| idx |     SA (sw)    | SA cnt (sw) |     DA (sw)    |");
	printk(" DA cnt (sw) |     SA (hw)    |     DA (hw)    |\n");
	printk("|------------------------------------------------------");
	printk("-----------------------------------------------|\n");

	for (i = start_idx; i <= end_idx; i++) {
		_fe_l2mac_print_entry(i);
		cond_resched();
	}
} /* fe_l2mac_print_range */

static void fe_l2mac_print_table(void)
{
	unsigned int i;

	printk("\n\n ------------------ L2 Result Table ---------------\n");
	printk("|------------------------------------------------------");
	printk("-----------------------------------------------|\n");
	printk("| idx |     SA (sw)    | SA cnt (sw) |     DA (sw)    |");
	printk(" DA cnt (sw) |     SA (hw)    |     DA (hw)    |\n");
	printk("|------------------------------------------------------");
	printk("-----------------------------------------------|\n");

	for (i = 0; i < FE_L2_ADDR_PAIR_ENTRY_MAX; i++) {
		_fe_l2mac_print_entry(i);
		cond_resched();
	}
} /* fe_l2mac_print_table */

static cs_fe_table_t cs_fe_l2mac_table_type = {
	.type_id = FE_TABLE_L2_MAC,
	.max_entry = FE_L2_ADDR_PAIR_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_l2_addr_pair_entry_t),
	.op = {
		.convert_sw_to_hw_data = NULL, /* L2MAC table does not support */
		.alloc_entry = NULL, /* L2MAC table does not support */
		.set_entry = NULL, /* L2MAC table does not support */
		.add_entry = NULL, /* L2MAC table does not support */
		.del_entry_by_idx = fe_l2mac_del_entry_by_idx,
		.del_entry = NULL, /* L2MAC table does not support */
		.find_entry = NULL, /* L2MAC table does not support */
		.get_entry = fe_l2mac_get_entry,
		.inc_entry_refcnt = NULL, /* L2MAC table does not support */
		.dec_entry_refcnt = NULL, /* L2MAC table does not support */
		.get_entry_refcnt = NULL, /* L2MAC table does not support */
		.set_field = NULL, /* L2MAC table does not support */
		.get_field = NULL, /* L2MAC table does not support */
		.flush_table = fe_l2mac_flush_table,
		.get_avail_count = fe_l2mac_get_avail_count,
		.print_entry = fe_l2mac_print_entry,
		.print_range = fe_l2mac_print_range,
		.print_table = fe_l2mac_print_table,
		.add_l2_mac = fe_l2mac_add,
		.del_l2_mac = fe_l2mac_del,
		.find_l2_mac = fe_l2mac_find,
		.get_l2_mac = fe_l2mac_get,
		.inc_l2_mac_refcnt = fe_l2mac_inc_refcnt,
		.add_l3_ip = NULL,
		.del_l3_ip = NULL,
		.find_l3_ip = NULL,
		.get_l3_ip = NULL,
		.inc_l3_ip_refcnt = NULL,
	},
	.content_table = NULL,
};

/* this API will initialize l2mac table */
int cs_fe_table_l2mac_init(void)
{
	int ret;

	spin_lock_init(FE_L2MAC_LOCK);

	cs_fe_l2mac_table_type.content_table = cs_table_alloc(
			cs_fe_l2mac_table_type.max_entry);
	if (cs_fe_l2mac_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_l2mac_table_type.type_id,
			&cs_fe_l2mac_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register L2MAC table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_l2mac_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_l2mac_flush_table();

	/* FIXME! any other initialization that needs to take place here? */

	return CS_OK;
} /* cs_fe_table_l2mac_init */
EXPORT_SYMBOL(cs_fe_table_l2mac_init);

void cs_fe_table_l2mac_exit(void)
{
	fe_l2mac_flush_table();

	if (cs_fe_l2mac_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_l2mac_table_type.content_table);
	cs_fe_table_unregister(cs_fe_l2mac_table_type.type_id);
} /* cs_fe_table_l2mac_exit */
EXPORT_SYMBOL(cs_fe_table_l2mac_exit);

int cs_fe_ioctl_l2mac(struct net_device *dev, void *pdata, void *cmd)
{
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;

	switch (fe_cmd_hdr->cmd) {
		case CMD_ADD:
			//cs_fe_add_entry_l2(p_l2_entry, &l2_idx);
			break;
		case CMD_DELETE:
			//cs_fe_delete_entry_l2(p_l2_entry);
			break;
		case CMD_FLUSH:
			break;
		case CMD_GET:	/* do it as print */
			printk("********* L2 FWDRSLT entry 0 ~ %d *********\n",
					FE_L2_ADDR_PAIR_ENTRY_MAX - 1);
			fe_l2mac_print_range(fe_cmd_hdr->idx_start,
					fe_cmd_hdr->idx_end);
			break;
		case CMD_REPLACE:	/* ignore */
			break;
		case CMD_INIT:	/* ignore */
			break;
		default:
			return -1;
	}

	return 0;
} /* cs_fe_l2mac_ioctl */

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_l2mac_add_entry_ut(unsigned int idx, unsigned char *p_sa,
		unsigned char *p_da)
{
	return fe_l2mac_add_entry(idx, p_sa, p_da);
}
EXPORT_SYMBOL(fe_l2mac_add_entry_ut);

int fe_l2mac_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_l2mac_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_l2mac_del_entry_by_idx_ut);

int fe_l2mac_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_l2mac_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_l2mac_get_entry_ut);

int fe_l2mac_flush_table_ut(void)
{
	return fe_l2mac_flush_table();
}
EXPORT_SYMBOL(fe_l2mac_flush_table_ut);

int fe_l2mac_get_avail_count_ut(void)
{
	return fe_l2mac_get_avail_count();
}
EXPORT_SYMBOL(fe_l2mac_get_avail_count_ut);

void fe_l2mac_print_entry_ut(unsigned int idx)
{
	fe_l2mac_print_entry(idx);
}
EXPORT_SYMBOL(fe_l2mac_print_entry_ut);

void fe_l2mac_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_l2mac_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_l2mac_print_range_ut);

void fe_l2mac_print_table_ut(void)
{
	fe_l2mac_print_table();
}
EXPORT_SYMBOL(fe_l2mac_print_table_ut);

int fe_l2mac_add_ut(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	return fe_l2mac_add(p_sa, p_da, p_idx);
}
EXPORT_SYMBOL(fe_l2mac_add_ut);

int fe_l2mac_del_ut(unsigned int idx, bool f_sa, bool f_da)
{
	return fe_l2mac_del(idx, f_sa, f_da);
}
EXPORT_SYMBOL(fe_l2mac_del_ut);

int fe_l2mac_find_ut(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	return fe_l2mac_find(p_sa, p_da, p_idx);
}
EXPORT_SYMBOL(fe_l2mac_find_ut);

int fe_l2mac_get_ut(unsigned int idx, unsigned char *p_sa, unsigned char *p_da)
{
	return fe_l2mac_get(idx, p_sa, p_da);
}
EXPORT_SYMBOL(fe_l2mac_get_ut);

int fe_l2mac_inc_refcnt_ut(unsigned int idx, bool f_sa, bool f_da)
{
	return fe_l2mac_inc_refcnt(idx, f_sa, f_da);
}
EXPORT_SYMBOL(fe_l2mac_inc_refcnt_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

