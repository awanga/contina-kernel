/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_l3ip.c
 *
 * $Id: cs_fe_table_l3ip.c,v 1.13 2011/11/23 02:10:06 whsu Exp $
 *
 * It contains L3 Result Table Management APIs implementation.
 */

/*
 * L3 Table Management:
 * Each row in the table contains 4 4-bytes block, and there are 1024 rows.
 * Each row can be used to contain either four IPv4 addresses or one
 * IPv6 address. Even though it sounds confusing, but this table has 1024
 * entries, and its index range is 0 to 4095.  IPv6 index must be 4's
 * multiple.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"
#include "cs_fe_util_api.h"
#include "cs_mut.h"

static cs_fe_table_t cs_fe_l3ip_table_type;

#define FE_L3IP_TABLE_PTR	(cs_fe_l3ip_table_type.content_table)
#define FE_L3IP_LOCK		&(cs_fe_l3ip_table_type.lock)
#define FE_L3IP_ENTRY_USED	(0x8000)

static int fe_l3ip_add_entry(unsigned int idx, __u32 *p_ip_addr,
		bool f_is_v6)
{
	cs_table_entry_t *p_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l3_addr_entry_t *p_l3_entry;
	unsigned long flags;
	int status;

	p_entry = cs_table_get_entry(FE_L3IP_TABLE_PTR, idx >> 2);
	if (p_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	spin_lock_irqsave(FE_L3IP_LOCK, flags);
	if (p_entry->data == NULL) {
		p_entry->data = fe_table_malloc_table_entry(
				&cs_fe_l3ip_table_type);
		if (p_entry->data == NULL) {
			spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
			return -ENOMEM;
		}
	}
	p_fe_entry = (fe_table_entry_t*)p_entry->data;
	p_l3_entry = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;

	if ((f_is_v6 == true) &&
			(p_l3_entry->count[0] & FE_L3IP_ENTRY_USED) &&
			(p_l3_entry->count[1] == 0xffff) &&
			(p_l3_entry->count[2] == 0xffff) &&
			(p_l3_entry->count[3] == 0xffff)) {
		p_l3_entry->count[0]++;
		p_l3_entry->ip_addr[0] = p_ip_addr[0];
		p_l3_entry->ip_addr[1] = p_ip_addr[1];
		p_l3_entry->ip_addr[2] = p_ip_addr[2];
		p_l3_entry->ip_addr[3] = p_ip_addr[3];

		status = cs_fe_hw_table_set_rslt_l3_ipv6(idx, p_ip_addr, false);
	} else if ((f_is_v6 == false) &&
			(p_l3_entry->count[idx & 0x03] == FE_L3IP_ENTRY_USED)) {
		p_l3_entry->count[idx & 0x03]++;
		p_l3_entry->ip_addr[idx & 0x03] = p_ip_addr[0];

		status = cs_fe_hw_table_set_rslt_l3_ipv4(idx, p_ip_addr[0],
				false);
	} else {
		status = -1;
	}
	if (status == FE_TABLE_OK)
		atomic_inc(&p_fe_entry->users);
	spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
	return FE_TABLE_OK;
} /* fe_l3ip_add_entry */

static int fe_l3ip_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_l3ip_table_type, idx >> 2,
			f_force);
} /* fe_l3ip_del_entry_by_idx */

static int fe_l3ip_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_l3ip_table_type, idx >> 2, entry);
} /* fe_l3ip_get_entry */

static int fe_l3ip_find_avail_index(unsigned int *p_idx, bool f_is_v6)
{
	cs_table_entry_t *p_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l3_addr_entry_t *p_l3ip;
	int ret;
	bool f_found = false;
	unsigned long flags;
	unsigned int tbl_idx, final_tbl_idx;

	/* assuming p_idx is not NULL.. the caller API should check that */

	/* deal with IPv6 first */
	if (f_is_v6 == true) {
		ret = fe_table_alloc_entry(&cs_fe_l3ip_table_type, &tbl_idx, 0);
		if (ret == FE_TABLE_OK) {
			spin_lock_irqsave(FE_L3IP_LOCK, flags);
			*p_idx = tbl_idx << 2;
			p_entry = cs_table_get_entry(FE_L3IP_TABLE_PTR,
					tbl_idx);
			if (p_entry == NULL) {
				spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
				return FE_TABLE_ENULLPTR;
			}
			if (p_entry->data == NULL) {
				p_entry->data = fe_table_malloc_table_entry(
						&cs_fe_l3ip_table_type);
				if (p_entry->data == NULL) {
					spin_unlock_irqrestore(FE_L3IP_LOCK,
							flags);
					return -ENOMEM;
				}
			}
			p_fe_entry = (fe_table_entry_t*)p_entry->data;
			p_l3ip = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;
			p_l3ip->count[0] |= FE_L3IP_ENTRY_USED;
			p_l3ip->count[1] = 0xffff;
			p_l3ip->count[2] = 0xffff;
			p_l3ip->count[3] = 0xffff;
			spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
		}
		return ret;
	}

	/* IPv4 requires special handling.. */
	if (FE_L3IP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	spin_lock_irqsave(FE_L3IP_LOCK, flags);
	/* instead of searching table from the curr_ptr, we go 1 step back by
	 * searching table from the previous entry; however, if the curr_ptr is
	 * at the entry#0, we will just start from there. */
	if (cs_fe_l3ip_table_type.curr_ptr != 0)
		tbl_idx = cs_fe_l3ip_table_type.curr_ptr - 1;
	else
		tbl_idx = 0;
	final_tbl_idx = tbl_idx;

	do {
		/* wrap around */
		if (tbl_idx == cs_fe_l3ip_table_type.max_entry)
			tbl_idx = 0;

		p_entry = cs_table_get_entry(FE_L3IP_TABLE_PTR, tbl_idx);
		if (p_entry == NULL) {
			spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
			return FE_TABLE_ENULLPTR;
		}

		if ((p_entry->local_data & FE_TABLE_ENTRY_USED) == 0) {
			p_entry->local_data |= FE_TABLE_ENTRY_USED;
			cs_fe_l3ip_table_type.used_entry++;
			cs_fe_l3ip_table_type.curr_ptr = tbl_idx + 1;
			*p_idx = tbl_idx << 2;
			if (p_entry->data == NULL) {
				p_entry->data = fe_table_malloc_table_entry(
						&cs_fe_l3ip_table_type);
				if (p_entry->data == NULL) {
					spin_unlock_irqrestore(FE_L3IP_LOCK,
							flags);
					return -ENOMEM;
				}
			}
			p_fe_entry = (fe_table_entry_t*)p_entry->data;
			p_l3ip = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;
			p_l3ip->count[0] |= FE_L3IP_ENTRY_USED;
			f_found = true;
		} else {
			p_fe_entry = (fe_table_entry_t*)p_entry->data;
			if (p_fe_entry == NULL)
				printk("%s:%d:NULL PTR!!\n", __func__,
						__LINE__);
			p_l3ip = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;
			if (p_l3ip == NULL)
				printk("%s:%d:NULL PTR!!\n", __func__,
						__LINE__);
			if ((p_fe_entry != NULL) && (p_l3ip != NULL) &&
					(atomic_read(&p_fe_entry->users))) {
				if (p_l3ip->count[0] == 0) {
					*p_idx = tbl_idx << 2;
					p_l3ip->count[0] |= FE_L3IP_ENTRY_USED;
					f_found = true;
				} else if (p_l3ip->count[1] == 0) {
					*p_idx = (tbl_idx << 2) + 1;
					p_l3ip->count[1] |= FE_L3IP_ENTRY_USED;
					f_found = true;
				} else if (p_l3ip->count[2] == 0) {
					*p_idx = (tbl_idx << 2) + 2;
					p_l3ip->count[2] |= FE_L3IP_ENTRY_USED;
					f_found = true;
				} else if (p_l3ip->count[3] == 0) {
					*p_idx = (tbl_idx << 2) + 3;
					p_l3ip->count[3] |= FE_L3IP_ENTRY_USED;
					f_found = true;
				}
				if (f_found == true) {
					spin_unlock_irqrestore(FE_L3IP_LOCK,
							flags);
					return FE_TABLE_OK;
				}
			}
			tbl_idx++;
			if (tbl_idx == final_tbl_idx) {
				/*
				 * we have already gone around the world and
				 * found nothing available.
				 */
				*p_idx = 0;
				spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
				return FE_TABLE_ETBLFULL;
			}
		}
	} while (f_found == false);

	spin_unlock_irqrestore(FE_L3IP_LOCK, flags);
	return FE_TABLE_OK;
} /* fe_l3ip_find_avail_index */

static int fe_l3ip_add(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6)
{
	int ret;

	if ((p_idx == NULL) || (p_ip_addr == NULL))
		return FE_TABLE_ENULLPTR;

	ret = fe_l3ip_find_avail_index(p_idx, f_is_v6);
	if (ret != FE_TABLE_OK)
		return ret;

	return fe_l3ip_add_entry((*p_idx), p_ip_addr, f_is_v6);
} /* fe_l3ip_add */

static int fe_l3ip_del(unsigned int idx, bool f_is_v6)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l3_addr_entry_t *p_l3_entry;
	unsigned long flags;
	__u32 zero_ip_addr[4] = {0, 0, 0, 0};

	/* index for IPv6 should be 4-divisible */
	if ((f_is_v6 == true) && (idx & 0x03))
		return FE_TABLE_EOUTRANGE;
	if ((FE_L3_ADDR_ENTRY_MAX << 2) <= idx)
		return FE_TABLE_EOUTRANGE;
	if (FE_L3IP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_L3IP_TABLE_PTR, idx >> 2);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't decrement the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;
	p_l3_entry = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;
	if (p_l3_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((f_is_v6 == true) &&
			(((p_l3_entry->count[0] & FE_L3IP_ENTRY_USED) == 0) ||
			(p_l3_entry->count[1] != 0xffff) ||
			(p_l3_entry->count[2] != 0xffff) ||
			(p_l3_entry->count[3] != 0xffff)))
		return -1;	// FIXME! need to define an error code for this?

	if ((f_is_v6 == false) &&
			(!(p_l3_entry->count[idx & 0x03] & FE_L3IP_ENTRY_USED)))
		return -1;	// FIXME! need to define an error code for this?

	spin_lock_irqsave(FE_L3IP_LOCK, flags);
	p_l3_entry->count[idx & 0x03]--;
	if (p_l3_entry->count[idx & 0x03] == FE_L3IP_ENTRY_USED) {
		p_l3_entry->count[idx & 0x03] &= ~FE_L3IP_ENTRY_USED;
		if (f_is_v6 == true) {
			/* clean IPv6 flags */
			p_l3_entry->count[1] = 0;
			p_l3_entry->count[2] = 0;
			p_l3_entry->count[3] = 0;

			cs_fe_hw_table_set_rslt_l3_ipv6(idx, zero_ip_addr,
					false);
		} else {
			cs_fe_hw_table_set_rslt_l3_ipv4(idx, zero_ip_addr[0],
					false);
		}
		atomic_dec(&p_fe_entry->users);
		if (atomic_read(&p_fe_entry->users) == 0) {
			cs_free(p_fe_entry->p_entry);
			p_fe_entry->p_entry = NULL;
			cs_free(p_sw_entry->data);
			p_sw_entry->data = NULL;
			cs_fe_l3ip_table_type.used_entry--;
			p_sw_entry->local_data &= ~FE_TABLE_ENTRY_USED;
		}
	}
	spin_unlock_irqrestore(FE_L3IP_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l3ip_del */

static bool fe_l3ip_ipv6_match(fe_l3_addr_entry_t *p_entry, __u32 *p_ip_addr)
{
	if ((p_entry->count[0] != 0) &&
			(p_entry->count[1] == 0xffff) &&
			(p_entry->count[2] == 0xffff) &&
			(p_entry->count[3] == 0xffff) &&
			(p_entry->ip_addr[0] == p_ip_addr[0]) &&
			(p_entry->ip_addr[1] == p_ip_addr[1]) &&
			(p_entry->ip_addr[2] == p_ip_addr[2]) &&
			(p_entry->ip_addr[3] == p_ip_addr[3]))
		return true;
	else
		return false;
} /* fe_l3ip_ipv6_match */

static int fe_l3ip_find(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6)
{
	fe_l3_addr_entry_t l3ip_entry;
	int ret;
	unsigned int iii;

	if ((p_idx == NULL) || (p_ip_addr == NULL))
		return FE_TABLE_ENULLPTR;

	for (iii = 0; iii < cs_fe_l3ip_table_type.max_entry; iii++) {
		ret = fe_l3ip_get_entry(iii << 2, &l3ip_entry);
		if (ret == FE_TABLE_OK) {
			if (f_is_v6 == true) {
				if (fe_l3ip_ipv6_match(&l3ip_entry, p_ip_addr)
						== true) {
					*p_idx = iii << 2;
					return FE_TABLE_OK;
				}
			} else {	/* (f_is_v6 == false) */
				if ((l3ip_entry.count[0] != 0) &&
						(l3ip_entry.ip_addr[0] ==
						 p_ip_addr[0])) {
					*p_idx = (iii << 2);
					return FE_TABLE_OK;
				} else if ((l3ip_entry.count[1] != 0) &&
						(l3ip_entry.ip_addr[1] ==
						 p_ip_addr[0])) {
					*p_idx = (iii << 2) + 1;
					return FE_TABLE_OK;
				} else if ((l3ip_entry.count[2] != 0) &&
						(l3ip_entry.ip_addr[2] ==
						 p_ip_addr[0])) {
					*p_idx = (iii << 2) + 2;
					return FE_TABLE_OK;
				} else if ((l3ip_entry.count[3] != 0) &&
						(l3ip_entry.ip_addr[3] ==
						 p_ip_addr[0])) {
					*p_idx = (iii << 2) + 3;
					return FE_TABLE_OK;
				}
			}
		}
	}
	return FE_TABLE_ENTRYNOTFOUND;
} /* fe_l3ip_find */

static int fe_l3ip_get(unsigned int idx, __u32 *p_ip_addr, bool f_is_v6)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l3_addr_entry_t *p_l3_entry;
	unsigned long flags;

	/* index for IPv6 should be 4-divisible */
	if ((f_is_v6 == true) && (idx & 0x03))
		return FE_TABLE_EOUTRANGE;
	if (idx >= (FE_L3_ADDR_ENTRY_MAX << 2))
		return FE_TABLE_EOUTRANGE;
	if (p_ip_addr == NULL)
		return FE_TABLE_ENULLPTR;
	if (FE_L3IP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_L3IP_TABLE_PTR, idx >> 2);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	p_l3_entry = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;
	if (p_l3_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((f_is_v6 == true) &&
			(((p_l3_entry->count[0] & FE_L3IP_ENTRY_USED) == 0) ||
			(p_l3_entry->count[1] != 0xffff) ||
			(p_l3_entry->count[2] != 0xffff) ||
			(p_l3_entry->count[3] != 0xffff)))
		return -1;	// FIXME! need to define an error code for this?

	if ((f_is_v6 == false) &&
			(!(p_l3_entry->count[idx & 0x03] & FE_L3IP_ENTRY_USED)))
		return -1;	// FIXME! need to define an error code for this?

	spin_lock_irqsave(FE_L3IP_LOCK, flags);
	if (f_is_v6 == true) {
		p_ip_addr[0] = p_l3_entry->ip_addr[0];
		p_ip_addr[1] = p_l3_entry->ip_addr[1];
		p_ip_addr[2] = p_l3_entry->ip_addr[2];
		p_ip_addr[3] = p_l3_entry->ip_addr[3];
	} else	/* if (f_is_v6 == false) */
		p_ip_addr[0] = p_l3_entry->ip_addr[idx & 0x03];
	spin_unlock_irqrestore(FE_L3IP_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l3ip_get */

static int fe_l3ip_inc_refcnt(unsigned int idx, bool f_is_v6)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	fe_l3_addr_entry_t *p_l3_entry;
	unsigned long flags;

	/* index for IPv6 should be 4-divisible */
	if ((f_is_v6 == true) && (idx & 0x03))
		return FE_TABLE_EOUTRANGE;
	if (idx >= (FE_L3_ADDR_ENTRY_MAX << 2))
		return FE_TABLE_EOUTRANGE;
	if (FE_L3IP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_L3IP_TABLE_PTR, idx >> 2);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	p_l3_entry = (fe_l3_addr_entry_t*)p_fe_entry->p_entry;
	if (p_l3_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((f_is_v6 == true) &&
			(((p_l3_entry->count[0] & FE_L3IP_ENTRY_USED) == 0) ||
			(p_l3_entry->count[1] != 0xffff) ||
			(p_l3_entry->count[2] != 0xffff) ||
			(p_l3_entry->count[3] != 0xffff)))
		return -1;	// FIXME! need to define an error code for this?

	if ((f_is_v6 == false) &&
			(!(p_l3_entry->count[idx & 0x03] & FE_L3IP_ENTRY_USED)))
		return -1;	// FIXME! need to define an error code for this?

	spin_lock_irqsave(FE_L3IP_LOCK, flags);
	p_l3_entry->count[idx & 0x03]++;
	spin_unlock_irqrestore(FE_L3IP_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l3ip_inc_refcnt */

static int fe_l3ip_flush_table(void)
{
	unsigned int i;
	unsigned long flags;

	for (i = 0; i < FE_L3_ADDR_ENTRY_MAX; i++)
		fe_l3ip_del_entry_by_idx(i << 2, true);

	/*
	 * purposedly do the following again to make sure the HW table
	 * is really flushed.
	 */
	spin_lock_irqsave(FE_L3IP_LOCK, flags);
	cs_fe_hw_table_flush_table(FE_TABLE_L3_IP);
	cs_fe_l3ip_table_type.curr_ptr = 0;
	cs_fe_l3ip_table_type.used_entry = 0;
	spin_unlock_irqrestore(FE_L3IP_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_l3ip_flush_table */

static int fe_l3ip_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_l3ip_table_type);
} /* fe_l3ip_get_avail_count */

static void _fe_l3ip_print_title(void)
{
	printk("\n\n ----------------- L3 Result Table -------------\n");
	printk("|----------------------------------------------\n");
	printk("| idx |  ver | SW IP addr  (cnt) | HW IP addr |\n");
} /* _fe_l3ip_print_title */

static void _fe_l3ip_print_entry(unsigned int idx)
{
	fe_l3_addr_entry_t l3ip_entry, *p_l3_entry;
	int status;
	__u32 ip_addr_buffer[4];
	bool parity;

	status = fe_l3ip_get_entry(idx, &l3ip_entry);
	if (status == FE_TABLE_EENTRYNOTRSVD) {
		printk("| %03d | NOT USED\n", idx);
		printk("|-------------------------------------------|\n");
	}
	if (status != FE_TABLE_OK)
		return;

	status = cs_fe_hw_table_get_rslt_l3_ipv6(idx, ip_addr_buffer, &parity);
	if (status != FE_TABLE_OK)
		return;
	/* done reading value from HW. */

	p_l3_entry = &l3ip_entry;

	if (p_l3_entry->count[idx & 0x03] == 0) {
		printk("| %03d | NOT USED\n", idx);
		printk("|-------------------------------------------|\n");
		return;
	}

	printk("| %03d | ", idx);
	if (p_l3_entry->count[idx & 0x03] == 0xffff)
		printk("IPv6 | ");
	else if (((idx & 0x03) == 0) && (p_l3_entry->count[1] == 0xffff))
		printk("IPv6 | ");
	else
		printk("IPv4 | ");

	printk("0x%08x ", p_l3_entry->ip_addr[idx & 0x03]);

	if (p_l3_entry->count[idx & 0x03] == 0xffff)
		printk("(----) | ");
	else
		printk("(%4d) | ", (p_l3_entry->count[idx & 0x03] &
			~FE_L3IP_ENTRY_USED));

	printk("0x%08x |\n", ip_addr_buffer[idx & 0x03]);
} /* _fe_l3ip_print_entry */

static void fe_l3ip_print_entry(unsigned int idx)
{
	if (idx >= (FE_L3_ADDR_ENTRY_MAX << 2)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}
	_fe_l3ip_print_title();
	_fe_l3ip_print_entry(idx);
	printk("|---------------------------------------------|\n\n\n");
} /* fe_l3ip_print_entry */

static void fe_l3ip_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= (FE_L3_ADDR_ENTRY_MAX << 2))) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_l3ip_print_title();

	for (i = start_idx; i <= end_idx; i++) {
		_fe_l3ip_print_entry(i);
		cond_resched();
	}
	printk("|---------------------------------------------|\n\n\n");
} /* fe_l3ip_print_range */

static void fe_l3ip_print_table(void)
{
	unsigned int i;

	_fe_l3ip_print_title();

	for (i = 0; i <= (FE_L3_ADDR_ENTRY_MAX << 2); i++) {
		_fe_l3ip_print_entry(i);
		cond_resched();
	}
	printk("|---------------------------------------------|\n\n\n");
} /* fe_l3ip_print_table */

static cs_fe_table_t cs_fe_l3ip_table_type = {
	.type_id = FE_TABLE_L3_IP,
	.max_entry = FE_L3_ADDR_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_l3_addr_entry_t),
	.op = {
		.convert_sw_to_hw_data = NULL, /* L3IP table does not support */
		.alloc_entry = NULL, /* L3IP table does not support */
		.set_entry = NULL, /* L3IP table does not support */
		.add_entry = NULL, /* L3IP table has different parameters */
		.del_entry_by_idx = fe_l3ip_del_entry_by_idx,
		.del_entry = NULL, /* L3IP table does not support */
		.find_entry = NULL, /* L3IP table does not support */
		.get_entry = fe_l3ip_get_entry,
		.inc_entry_refcnt = NULL, /* L3IP table does not support */
		.dec_entry_refcnt = NULL, /* L3IP table does not support */
		.get_entry_refcnt = NULL, /* L3IP table does not support */
		.set_field = NULL, /* L3IP table does not support */
		.get_field = NULL, /* L3IP table does not support */
		.flush_table = fe_l3ip_flush_table,
		.get_avail_count = fe_l3ip_get_avail_count,
		.print_entry = fe_l3ip_print_entry,
		.print_range = fe_l3ip_print_range,
		.print_table = fe_l3ip_print_table,
		.add_l2_mac = NULL,
		.del_l2_mac = NULL,
		.find_l2_mac = NULL,
		.get_l2_mac = NULL,
		.inc_l2_mac_refcnt = NULL,
		.add_l3_ip = fe_l3ip_add,
		.del_l3_ip = fe_l3ip_del,
		.find_l3_ip = fe_l3ip_find,
		.get_l3_ip = fe_l3ip_get,
		.inc_l3_ip_refcnt = fe_l3ip_inc_refcnt,
	},
	.content_table = NULL,
};

/* this API will initialize l3ip table */
int cs_fe_table_l3ip_init(void)
{
	int ret;

	spin_lock_init(FE_L3IP_LOCK);

	cs_fe_l3ip_table_type.content_table = cs_table_alloc(
			cs_fe_l3ip_table_type.max_entry);
	if (cs_fe_l3ip_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_l3ip_table_type.type_id,
			&cs_fe_l3ip_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register L3IP table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_l3ip_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_l3ip_flush_table();

	/* FIXME! any other initialization that needs to take place here? */

	return CS_OK;
} /* cs_fe_table_l3ip_init */
EXPORT_SYMBOL(cs_fe_table_l3ip_init);

void cs_fe_table_l3ip_exit(void)
{
	fe_l3ip_flush_table();

	if (cs_fe_l3ip_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_l3ip_table_type.content_table);
	cs_fe_table_unregister(cs_fe_l3ip_table_type.type_id);
} /* cs_fe_table_l3ip_exit */
EXPORT_SYMBOL(cs_fe_table_l3ip_exit);

int cs_fe_ioctl_l3ip(struct net_device *dev, void *pdata, void *cmd)
{
	//unsigned int l3_idx;
	//fe_l3_addr_entry_t *p_l3_entry = (fe_l3_addr_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;

	switch (fe_cmd_hdr->cmd) {
		case CMD_ADD:
			//cs_fe_add_entry_l3(p_l3_entry, &l3_idx);
			break;
		case CMD_DELETE:
			//cs_fe_delete_entry_l3(p_l3_entry);
			break;
		case CMD_FLUSH:
			break;
		case CMD_GET:	/* do it as print */
			printk("********* L3 FWDRSLT entry 0 ~ %d *********\n",
					FE_L3_ADDR_ENTRY_MAX - 1);
			fe_l3ip_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_l3ip_ioctl */

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_l3ip_add_entry_ut(unsigned int idx, __u32 *p_ip_addr, bool f_is_v6)
{
	return fe_l3ip_add_entry(idx, p_ip_addr, f_is_v6);
}
EXPORT_SYMBOL(fe_l3ip_add_entry_ut);

int fe_l3ip_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_l3ip_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_l3ip_del_entry_by_idx_ut);

int fe_l3ip_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_l3ip_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_l3ip_get_entry_ut);

int fe_l3ip_flush_table_ut(void)
{
	return fe_l3ip_flush_table();
}
EXPORT_SYMBOL(fe_l3ip_flush_table_ut);

int fe_l3ip_get_avail_count_ut(void)
{
	return fe_l3ip_get_avail_count();
}
EXPORT_SYMBOL(fe_l3ip_get_avail_count_ut);

void fe_l3ip_print_entry_ut(unsigned int idx)
{
	fe_l3ip_print_entry(idx);
}
EXPORT_SYMBOL(fe_l3ip_print_entry_ut);

void fe_l3ip_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_l3ip_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_l3ip_print_range_ut);

void fe_l3ip_print_table_ut(void)
{
	fe_l3ip_print_table();
}
EXPORT_SYMBOL(fe_l3ip_print_table_ut);

int fe_l3ip_add_ut(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6)
{
	return fe_l3ip_add(p_ip_addr, p_idx, f_is_v6);
}
EXPORT_SYMBOL(fe_l3ip_add_ut);

int fe_l3ip_del_ut(unsigned int idx, bool f_is_v6)
{
	return fe_l3ip_del(idx, f_is_v6);
}
EXPORT_SYMBOL(fe_l3ip_del_ut);

int fe_l3ip_find_ut(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6)
{
	return fe_l3ip_find(p_ip_addr, p_idx, f_is_v6);
}
EXPORT_SYMBOL(fe_l3ip_find_ut);

int fe_l3ip_get_ut(unsigned int idx, __u32 *p_ip_addr, bool f_is_v6)
{
	return fe_l3ip_get(idx, p_ip_addr, f_is_v6);
}
EXPORT_SYMBOL(fe_l3ip_get_ut);

int fe_l3ip_inc_refcnt_ut(unsigned int idx, bool f_is_v6)
{
	return fe_l3ip_inc_refcnt(idx, f_is_v6);
}
EXPORT_SYMBOL(fe_l3ip_inc_refcnt_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

