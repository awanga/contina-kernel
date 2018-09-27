/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_hash.c
 *
 * $Id: cs_fe_hash.c,v 1.18 2012/08/22 07:08:40 bhsieh Exp $
 *
 * It contains hash table management unit which handles insertion and
 * removal of hash entry to either Hash Hash table or Hash Overflow
 * table.  It also implements a timer fucntion that checks the hit
 * status of each inserted hash entry.
 */

#include <asm-generic/cputime.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include "cs_fe.h"
#include "cs_crc.h"
#include "crctable_32.h"
#include "crctable_ccitt16.h"
#include "crctable_poly_1.h"
#include "crctable_poly_2.h"
#include "crctable_poly_3.h"
#include "cs_hmu.h"

#define PFX	"CS_FE_HASH"
#define PRINT(format, args...) printk(KERN_WARNING PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)

#ifdef CONFIG_CS752X_ACCEL_KERNEL
extern u32 cs_ne_hash_timer_period;
#else
u32 cs_ne_hash_timer_period = 0;
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_fe_debug;
// FIXME!! should we use something else other than CS752X_FE_HASH_STATUS?
#define DBG(x) {if (cs_fe_debug & CS752X_FE_HASH_STATUS) x;}
#else
#define DBG(x) { }
#endif

struct timer_list cs_fe_hash_timer_obj;
static u64 cs_fe_hash_used_mask[FE_HASH_STATUS_ENTRY_MAX];
static spinlock_t cs_fe_hash_lock;

#define MIN(xxx, yyy)		(((xxx) <= (yyy))? (xxx): (yyy))
#define UNLOCK_RETURN(lock, flag, ret_val) \
{spin_unlock_irqrestore(lock, flag); return ret_val;}

#define HASH_TRY_TIMEOUT	1000

/* Hash Calculation Variables */
unsigned long HHBitMask[] = {0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF,
	0x000007FF, 0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
	0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF,
	0x007FFFFF, 0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF};

extern void ni_dm_byte(u32 location, int length);

void cs_fe_hash_print_counter(void)
{
	u16 hash_idx ;
	u32 hw_idx;
	unsigned int count;
	int ret, i;
	unsigned long flags;
	unsigned long hash_counter = 0;
	unsigned long start_time = jiffies;
	/*
	 * Get the current hash usage of all the 6 buckets with the same
	 * hash_idx that this newly introduced hash entry might be using.
	 */
	spin_lock_irqsave(&cs_fe_hash_lock, flags);
	for (hash_idx = 0; hash_idx < FE_SW_HASH_ENTRY_MAX; hash_idx++)
		for (i = 0; i < 3; i++) {
			hw_idx = (i) << 12 | hash_idx;
			ret = cs_fe_table_get_entry_refcnt(FE_TABLE_HASH_HASH,
				hw_idx, &count);
			if (ret == FE_TABLE_OK)
				hash_counter += count;
		}
	spin_unlock_irqrestore(&cs_fe_hash_lock, flags);
	start_time = jiffies - start_time;
	printk("\t\t\t\t hash counter = %ld, scan period (%ld)\n",
			hash_counter, start_time);
}
EXPORT_SYMBOL(cs_fe_hash_print_counter);

int cs_fe_hash_add_hash(u32 crc32, u16 crc16, u8 mask_ptr,
		u16 rslt_idx, u16 *return_idx)
{
	u16 hash_idx = crc16 >> 4;
	u32 hw_idx, new_idx, free_idx;
	//unsigned int hash_idx = crc16 >> 4, hw_idx, new_idx, free_idx;
	unsigned int mask_addr, mask_offset;
	fe_hash_hash_entry_t hash_entry;
	fe_hash_overflow_entry_t overflow_entry;
	unsigned int count[3];
	int ret[3], i;
	unsigned long flags;

	DBG(printk("%s:%d:crc32 = %x, crc16 = %x, mask_ptr = %d, rslt_idx "
				"= %d\n", __func__, __LINE__, crc32, crc16,
				mask_ptr, rslt_idx));
	/*
	 * 4 different cases might happen here:
	 * 1) none of these 6 is used: so we can use the first one.
	 * 2) some/all of these 6 are used, and there is exact the same entry,
	 *    so do nothing.
	 * 3) some of these 6 are used, and there is no exact the same entry, so
	 *    we are going to create this entry on the empty slot.
	 * 4) all of these 6 are used, but there is no more slot. Check overflow
	 *    to see if there is a matching entry in overflow, if not add it.
	 */

	/*
	 * Get the current hash usage of all the 6 buckets with the same
	 * hash_idx that this newly introduced hash entry might be using.
	 */
	for (i = 0; i < 3; i++) {
		hw_idx = (i) << 12 | hash_idx;
		ret[i] = cs_fe_table_get_entry_refcnt(FE_TABLE_HASH_HASH,
				hw_idx, &count[i]);
		/* check all the return value, make sure there is no error. */
		if ((ret[i] != FE_TABLE_OK) &&
				(ret[i] != FE_TABLE_EENTRYNOTRSVD))
			return ret[i];
	}

	spin_lock_irqsave(&cs_fe_hash_lock, flags);
	/* case 1) none of these 6 are used. */
	if ((ret[0] == FE_TABLE_EENTRYNOTRSVD) &&
			(ret[1] == FE_TABLE_EENTRYNOTRSVD) &&
			(ret[2] == FE_TABLE_EENTRYNOTRSVD)) {
		hw_idx = hash_idx;
		memset(&hash_entry, 0x0, sizeof(hash_entry));
		hash_entry.entry0_valid = 1;
		hash_entry.crc32_0 = crc32;
		hash_entry.crc16_0 = crc16;
		hash_entry.result_index0 = rslt_idx;
		hash_entry.mask_ptr0 = mask_ptr;
		ret[0] = cs_fe_table_alloc_entry(FE_TABLE_HASH_HASH, &new_idx,
				hw_idx);
		if (ret[0] != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[0]);
		if (new_idx != hw_idx) {
			DBG(PRINT("new_idx %x != request_idx %x\n", new_idx,
						hw_idx));
			cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_HASH,
					new_idx, false);
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, -1);
		}
		*return_idx = (hash_idx << 4);
		ret[0] = cs_fe_table_set_entry(FE_TABLE_HASH_HASH, hw_idx,
				&hash_entry);
		if (ret[0] != FE_TABLE_OK) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_HASH,
					new_idx, false);
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[0]);
		}
		ret[0] = cs_fe_table_inc_entry_refcnt(FE_TABLE_HASH_HASH,
				hw_idx);
		if (ret[0] != FE_TABLE_OK) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_HASH,
					new_idx, false);
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[0]);
		}
		goto ADD_HASH_COMPLETE;
	}

	/* case 2) & 3), some/all buckets are used. inspect them */
	free_idx = 6;
	for (i = 0; i < 3; i++) {
		if (ret[i] == FE_TABLE_EENTRYNOTRSVD)
			free_idx = MIN(i << 1, free_idx);
		else {
			hw_idx = i << 12 | hash_idx;
			ret[i] = cs_fe_table_get_entry(FE_TABLE_HASH_HASH,
					hw_idx, &hash_entry);
			if (ret[i] != FE_TABLE_OK)
				UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[i]);
			if (hash_entry.entry0_valid == 0) {
				free_idx = MIN(i << 1, free_idx);
			} else {
				if ((hash_entry.crc32_0 == crc32) &&
						(hash_entry.crc16_0 == crc16)) {
					*return_idx = (hash_idx << 4) |
						(i << 1);
					DBG(PRINT("duplicate hash!\n"));
					UNLOCK_RETURN(&cs_fe_hash_lock, flags,
							FE_TABLE_EDUPLICATE);
				}
			}
			if (hash_entry.entry1_valid == 0) {
				free_idx = MIN((i << 1) + 1, free_idx);
			} else {
				if ((hash_entry.crc32_1 == crc32) &&
						(hash_entry.crc16_1 == crc16)) {
					*return_idx = ((hash_idx << 4) |
							(i << 1)) + 1;
					DBG(PRINT("duplicate hash!\n"));
					UNLOCK_RETURN(&cs_fe_hash_lock, flags,
							FE_TABLE_EDUPLICATE);
				}
			}
		}
	}

	/* case 2) doesn't happen, so let's check case 3) by checking free_idx.
	 * It should be some numbers smaller than 6 if there are some available
	 * spaces. */
	if (free_idx != 6) {
		i = (free_idx & 0x06) >> 1;
		hw_idx = i << 12 | hash_idx;

		if (ret[i] == FE_TABLE_EENTRYNOTRSVD) {
			memset(&hash_entry, 0x0, sizeof(hash_entry));
			hash_entry.entry0_valid = 1;
			hash_entry.crc32_0 = crc32;
			hash_entry.crc16_0 = crc16;
			hash_entry.result_index0 = rslt_idx;
			hash_entry.mask_ptr0 = mask_ptr;
			ret[i] = cs_fe_table_alloc_entry(FE_TABLE_HASH_HASH,
					&new_idx, hw_idx);
			if (ret[i] != FE_TABLE_OK)
				UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[i]);
			if (new_idx != hw_idx) {
				DBG(PRINT("new_idx %x != request_idx %x\n",
							new_idx, hw_idx));
				cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_HASH,
						new_idx, false);
				UNLOCK_RETURN(&cs_fe_hash_lock, flags, -1);
			}
		} else {
			ret[i] = cs_fe_table_get_entry(FE_TABLE_HASH_HASH,
					hw_idx, &hash_entry);
			if (ret[i] != FE_TABLE_OK)
				UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[i]);
			if ((free_idx & 0x01) == 0) {
				hash_entry.entry0_valid = 1;
				hash_entry.crc32_0 = crc32;
				hash_entry.crc16_0 = crc16;
				hash_entry.result_index0 = rslt_idx;
				hash_entry.mask_ptr0 = mask_ptr;
			} else {
				hash_entry.entry1_valid = 1;
				hash_entry.crc32_1 = crc32;
				hash_entry.crc16_1 = crc16;
				hash_entry.result_index1 = rslt_idx;
				hash_entry.mask_ptr1 = mask_ptr;
			}
		}
		*return_idx = (hash_idx << 4) | (free_idx & 0x07);
		ret[i] = cs_fe_table_set_entry(FE_TABLE_HASH_HASH, hw_idx,
				&hash_entry);
		if (ret[i] != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[i]);
		ret[i] = cs_fe_table_inc_entry_refcnt(FE_TABLE_HASH_HASH,
				hw_idx);
		if (ret[i] != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[i]);
		ret[0] = ret[i];
		goto ADD_HASH_COMPLETE;
	}

	/*
	 * reach here.. then it's case 4). check & see if we can use hash
	 * overflow table.
	 */
	overflow_entry.crc32 = crc32;
	overflow_entry.crc16 = hash_idx;
	overflow_entry.result_index = rslt_idx;
	overflow_entry.mask_ptr = mask_ptr;
	new_idx = 0;
	/*
	 * the following function will check duplicate entry and add the entry
	 * if there is any available space.
	 */
	ret[0] = cs_fe_table_add_entry(FE_TABLE_HASH_OVERFLOW, &overflow_entry,
			&new_idx);
	*return_idx = (new_idx << 4) | HASH_OVERFLOW_INDEX_MASK;

	if (ret[0] != FE_TABLE_OK)
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[0]);

ADD_HASH_COMPLETE:
	/* turn on the bit in the MASK table */
	if (IS_OVERFLOW_ENTRY(*return_idx)) {
		mask_addr = FE_HASH_STATUS_ENTRY_MAX - 1;
		mask_offset = ((*return_idx) >> 4) & 0x3f;
	} else {
		hw_idx = hash_idx * 6 + ((*return_idx) & 0x07);
		mask_addr = hw_idx >> 6;
		mask_offset = hw_idx & 0x3f;
	}
	cs_fe_hash_used_mask[mask_addr] |= (((u64)0x01) << mask_offset);

	/* TODO! Wen: if we need to implement HashCheck, do it here */
	UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret[0]);
} /* cs_fe_hash_add_hash */

int cs_fe_hash_del_hash(unsigned int sw_idx)
{
	unsigned int hw_idx = HASH_INDEX_SW2HW(sw_idx), tmp_idx;
	unsigned int mask_addr, mask_offset;
	fe_hash_hash_entry_t hash_entry;
	int ret;
	unsigned long flags;
	bool f_del_hash = false;

	/* if this sw_idx belongs to overflow table, we delete the entry
	 * from hash overflow table */
	if (IS_OVERFLOW_ENTRY(sw_idx)) {
		spin_lock_irqsave(&cs_fe_hash_lock, flags);
		hw_idx &= 0x3f;
		ret = cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_OVERFLOW,
				hw_idx, false);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
		mask_addr = FE_HASH_STATUS_ENTRY_MAX - 1;
		mask_offset = hw_idx;
		goto DEL_HASH_COMPLETE;
	}

	spin_lock_irqsave(&cs_fe_hash_lock, flags);

	ret = cs_fe_table_get_entry(FE_TABLE_HASH_HASH, hw_idx, &hash_entry);
	if (ret != FE_TABLE_OK)
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);

	if ((sw_idx & 0x01) == 0) {
		if (hash_entry.entry0_valid == 1) {
			hash_entry.entry0_valid = 0;
			hash_entry.crc32_0 = 0;
			hash_entry.crc16_0 = 0;
			hash_entry.result_index0 = 0;
			hash_entry.mask_ptr0 = 0;
		}
		if (hash_entry.entry1_valid == 0)
			f_del_hash = true;
	} else {
		if (hash_entry.entry1_valid == 1) {
			hash_entry.entry1_valid = 0;
			hash_entry.crc32_1 = 0;
			hash_entry.crc16_1 = 0;
			hash_entry.result_index1 = 0;
			hash_entry.mask_ptr1 = 0;
		}
		if (hash_entry.entry0_valid == 0)
			f_del_hash = true;
	}

	if (f_del_hash == true) {
		ret = cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_HASH, hw_idx,
				false);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
	} else {
		ret = cs_fe_table_set_entry(FE_TABLE_HASH_HASH, hw_idx,
				&hash_entry);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
		ret = cs_fe_table_dec_entry_refcnt(FE_TABLE_HASH_HASH, hw_idx);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
	}

	tmp_idx = (hw_idx & 0xfff) * 6 + (sw_idx & 0x07);
	mask_addr = tmp_idx >> 6;
	mask_offset = tmp_idx & 0x3f;

DEL_HASH_COMPLETE:
	/* turn off the bit in the MASK table */
	cs_fe_hash_used_mask[mask_addr] &= ~((u64)0x01 << mask_offset);

	/* TODO! Wen: if we need to implement HashCheck, do it here */

	UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
} /* cs_fe_hash_del_hash */

int cs_fe_hash_get_hash(unsigned int sw_idx, u32 *crc32, u16 *crc16,
		u8 *mask_ptr, u16 *rslt_idx)
{
	unsigned int hw_idx = HASH_INDEX_SW2HW(sw_idx);
	fe_hash_hash_entry_t hash_entry;
	fe_hash_overflow_entry_t overflow_entry;
	unsigned long flags;
	int ret;

	if ((crc32 == NULL) || (crc16 == NULL) ||
			(mask_ptr == NULL) || (rslt_idx == NULL))
		return FE_TABLE_ENULLPTR;

	/* if this sw_idx belongs to overflow table, we delete the entry
	 * from hash overflow table */
	if (IS_OVERFLOW_ENTRY(sw_idx)) {
		spin_lock_irqsave(&cs_fe_hash_lock, flags);
		hw_idx &= 0x3f;
		ret = cs_fe_table_get_entry(FE_TABLE_HASH_OVERFLOW, hw_idx,
				&overflow_entry);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
		*crc32 = overflow_entry.crc32;
		*crc16 = overflow_entry.crc16;
		*rslt_idx = overflow_entry.result_index;
		*mask_ptr = overflow_entry.mask_ptr;
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
	}

	spin_lock_irqsave(&cs_fe_hash_lock, flags);

	ret = cs_fe_table_get_entry(FE_TABLE_HASH_HASH, hw_idx, &hash_entry);
	if (ret != FE_TABLE_OK)
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);

	if ((sw_idx & 0x01) == 0) {
		if (hash_entry.entry0_valid == 1) {
			*crc32 = hash_entry.crc32_0;
			*crc16 = hash_entry.crc16_0;
			*rslt_idx = hash_entry.result_index0;
			*mask_ptr = hash_entry.mask_ptr0;
		}
	} else {
		if (hash_entry.entry1_valid == 1) {
			*crc32 = hash_entry.crc32_1;
			*crc16 = hash_entry.crc16_1;
			*rslt_idx = hash_entry.result_index1;
			*mask_ptr = hash_entry.mask_ptr1;
		}
	}
	UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
} /* cs_fe_hash_get_hash */

int cs_fe_hash_update_hash(unsigned int sw_idx, u32 crc32, u16 crc16,
		u8 mask_ptr, u16 rslt_idx)
{
	unsigned int hw_idx = HASH_INDEX_SW2HW(sw_idx);
	fe_hash_hash_entry_t hash_entry;
	fe_hash_overflow_entry_t overflow_entry;
	unsigned long flags;
	int ret;

	/* if this sw_idx belongs to overflow table, we delete the entry
	 * from hash overflow table */
	if (IS_OVERFLOW_ENTRY(sw_idx)) {
		spin_lock_irqsave(&cs_fe_hash_lock, flags);
		hw_idx &= 0x3f;
		ret = cs_fe_table_get_entry(FE_TABLE_HASH_OVERFLOW, hw_idx,
				&overflow_entry);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
		overflow_entry.crc32 = crc32;
		overflow_entry.crc16 = crc16;
		overflow_entry.result_index = rslt_idx;
		overflow_entry.mask_ptr = mask_ptr;
		ret = cs_fe_table_set_entry(FE_TABLE_HASH_OVERFLOW, hw_idx,
				&overflow_entry);
		if (ret != FE_TABLE_OK)
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
	}

	spin_lock_irqsave(&cs_fe_hash_lock, flags);

	ret = cs_fe_table_get_entry(FE_TABLE_HASH_HASH, hw_idx, &hash_entry);
	if (ret != FE_TABLE_OK)
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);

	if ((sw_idx & 0x01) == 0) {
		if (hash_entry.entry0_valid == 1) {
			hash_entry.crc32_0 = crc32;
			hash_entry.crc16_0 = crc16;
			hash_entry.result_index0 = rslt_idx;
			hash_entry.mask_ptr0 = mask_ptr;
		}
	} else {
		if (hash_entry.entry1_valid == 1) {
			hash_entry.crc32_1 = crc32;
			hash_entry.crc16_1 = crc16;
			hash_entry.result_index1 = rslt_idx;
			hash_entry.mask_ptr1 = mask_ptr;
		}
	}
	ret = cs_fe_table_set_entry(FE_TABLE_HASH_HASH, hw_idx, &hash_entry);
	if (ret != FE_TABLE_OK)
		UNLOCK_RETURN(&cs_fe_hash_lock, flags, ret);
	UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
} /* cs_fe_hash_update_hash */

int cs_fe_hash_get_hash_by_crc(u32 crc32, u16 crc16, u8 mask_ptr,
		u16 *fwd_rslt_idx, unsigned int *sw_idx)
{
	u16 hash_idx = crc16 >> 4;
	u32 hw_idx;
	fe_hash_hash_entry_t hash_entry;
	fe_hash_overflow_entry_t overflow_entry;
	int ret, i;
	unsigned long flags;

	DBG(printk("%s:%d:crc32 = %x, crc16 = %x, mask_ptr = %d \n"
				, __func__, __LINE__, crc32, crc16,
				mask_ptr));

	spin_lock_irqsave(&cs_fe_hash_lock, flags);

	for (i = 0; i < 3; i++) {
		hw_idx = (i) << 12 | hash_idx;

		ret = cs_fe_table_get_entry(FE_TABLE_HASH_HASH,
			hw_idx, &hash_entry);
		if (ret == FE_TABLE_OK) {
			if ((hash_entry.entry0_valid == 1) &&
					(hash_entry.crc32_0 == crc32) &&
					(hash_entry.crc16_0 == crc16) &&
					(hash_entry.mask_ptr0 == mask_ptr)) {
				*fwd_rslt_idx = hash_entry.result_index0;
				*sw_idx = (hash_idx << 4) | (i << 1);
				UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
			} else 	if ((hash_entry.entry1_valid == 1) &&
					(hash_entry.crc32_1 == crc32) &&
					(hash_entry.crc16_1 == crc16) &&
					(hash_entry.mask_ptr1 == mask_ptr)){
				*fwd_rslt_idx = hash_entry.result_index1;
				*sw_idx = ((hash_idx << 4) | (i << 1)) + 1;
				UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
			}
		}
	}


	for (i = 0; i < FE_HASH_OVERFLOW_ENTRY_MAX; i++) {
		cs_fe_table_get_entry(FE_TABLE_HASH_OVERFLOW, i, &overflow_entry);
		if ((overflow_entry.crc32 == crc32) &&
			(overflow_entry.crc16 == hash_idx) &&
			(overflow_entry.mask_ptr == mask_ptr)) {
			*fwd_rslt_idx = overflow_entry.result_index;
			*sw_idx = (i << 4) | HASH_OVERFLOW_INDEX_MASK;
			UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_OK);
		}
	}
	UNLOCK_RETURN(&cs_fe_hash_lock, flags, FE_TABLE_ENTRYNOTFOUND);
} /* cs_fe_hash_get_hash_by_crc */


void cs_fe_hash_flush(void)
{
	cs_fe_table_flush_table(FE_TABLE_HASH_HASH);
	cs_fe_table_flush_table(FE_TABLE_HASH_OVERFLOW);
	cs_fe_table_flush_table(FE_TABLE_HASH_CHECK_MEM);
	cs_fe_table_flush_table(FE_TABLE_HASH_STATUS);
	memset(cs_fe_hash_used_mask, 0x0, FE_HASH_STATUS_ENTRY_MAX * 8);
} /* cs_fe_hash_flush */

#if 0	// Sample Code
int cs_fe_hash_add_entry(fe_sw_hash_t *swhash, u8 mask_ptr, u16 rslt_idx,
		u16 *return_idx)
{
	u32 crc32;
	u16 crc16;
	int ret;

	/* allocate mask_ptr and rslt_idx should be done out there */

	if ((swhash == NULL) || (return_idx == NULL)) return FE_TABLE_ENULLPTR;

	ret = cs_fe_hash_calc_crc(swhash, &crc32, &crc16, CRC16_CCITT);
	if (ret != FE_TABLE_OK) return ret;

	return cs_fe_hash_add_hash(crc32, crc16, mask_ptr, rslt_idx, return_idx);
} /* cs_fe_hash_add_entry */
#endif

void cs_fe_hash_timer_func(unsigned long data)
{
	int i, j;
	u16 hw_idx, tbl_idx;
	//u16 hash_index;
	u64 tmp64, tmp_dyn;
	u32 value[2];
	int status;
	int update_timer = 0;
	bool is_used;

	for (i = 0; i < FE_HASH_STATUS_ENTRY_MAX; i++) {
		/* If some hash are enabled, poll it */
		if (cs_fe_hash_used_mask[i] != 0) {
			update_timer = 1;
			status = cs_fe_table_get_field(FE_TABLE_HASH_STATUS, i,
					FE_HASH_STATUS_MEM_DATA_DATA, value);

			if (status != FE_TABLE_OK)
				goto next_time;

			tmp64 = value[0] | (((u64)value[1]) << 32);
			/* Clear status bits and write back */
			value[0] = value[1] = 0;
			cs_fe_table_set_field(FE_TABLE_HASH_STATUS, i,
					FE_HASH_STATUS_MEM_DATA_DATA, value);

			tmp_dyn = cs_fe_hash_used_mask[i];
			for (j = 0; (j < 64) && (tmp_dyn != 0); j++, tmp_dyn >>= 1) {
				if ((tmp_dyn & 1)) {
#if 0
					DBG(printk("Decrease hash timer %d"
								"(i=%d j=%d)\n",
								i * 64 + j, i,
								j ));
#endif
					if (i == (FE_HASH_STATUS_ENTRY_MAX - 1)) {
						hw_idx = (j << 4) |
							HASH_OVERFLOW_INDEX_MASK;
					} else {
						tbl_idx = (i << 6) + j;
						hw_idx = (((tbl_idx) / 6) << 4 )
							| ((tbl_idx) % 6);
					}
					if (tmp64 & (((u64)0x1) << j))
						is_used = true;
					else
						is_used = false;
					cs_hmu_hash_update_last_use(hw_idx,
							is_used);
				}
			/*
			 * FIXME!! getting some weird warning message!!
			 * sleeping function called from invalid context
			 */
			//cond_resched();
			}
		}
		/*
		 * FIXME!! getting some weird warning message!! sleeping
		 * function called from invalid context
		 */
		//cond_resched();
	}

next_time:
	cs_hmu_hash_table_scan();

	mod_timer(&cs_fe_hash_timer_obj, jiffies +
			secs_to_cputime(cs_ne_hash_timer_period));

} /* cs_fe_hash_timer_func */

void cs_fe_hash_timer_init(void)
{
	init_timer(&cs_fe_hash_timer_obj);
	cs_ne_hash_timer_period = CS_HASH_TIMER_PERIOD;
	cs_fe_hash_timer_obj.expires = jiffies +
			secs_to_cputime(cs_ne_hash_timer_period);
	cs_fe_hash_timer_obj.data = (unsigned long)&cs_fe_hash_timer_obj;
	cs_fe_hash_timer_obj.function = (void *)&cs_fe_hash_timer_func;
	add_timer(&cs_fe_hash_timer_obj);
} /* cs_fe_hash_timer_init */


#ifdef FE_HASH_BYTE_CRC
typedef int TYPE;
TYPE reverse_n(TYPE x, int bits);
TYPE reverse_n(TYPE x, int bits)
{
	TYPE m = ~0;
	switch (bits) {
	case 64:
		x = (x & 0xFFFFFFFF00000000 & m) >> 16 |
			(x & 0x00000000FFFFFFFF & m) << 16;
	case 32:
		x = (x & 0xFFFF0000FFFF0000 & m) >> 16 |
			(x & 0x0000FFFF0000FFFF & m) << 16;
	case 16:
		x = (x & 0xFF00FF00FF00FF00 & m) >> 8 |
			(x & 0x00FF00FF00FF00FF & m) << 8;
	case 8:
		x = (x & 0xF0F0F0F0F0F0F0F0 & m) >> 4 |
			(x & 0x0F0F0F0F0F0F0F0F & m) << 4;
		x = (x & 0xCCCCCCCCCCCCCCCC & m) >> 2 |
			(x & 0x3333333333333333 & m) << 2;
		x = (x & 0xAAAAAAAAAAAAAAAA & m) >> 1 |
			(x & 0x5555555555555555 & m) << 1;
	}
	return x;
}
#endif


/******************************************************************************
 *
 *                       Hard hash calculate functions
 *
 *****************************************************************************/

void fe_tbl_HashHash_Fill_Fields(unsigned char *pDstBuf, unsigned char *pSrcBuf,
		unsigned short StartPos, unsigned short TotalWidth)
{
	unsigned short StartByte = (unsigned short)StartPos >> 3;
	unsigned short StartBit = (unsigned short)StartPos % 8;
	unsigned char RemainBits = 0, MoveBitsCount = 0;
	unsigned char *pDstByte = NULL, *pSrcByte = NULL;
	int i = 0;

	RemainBits = 8 - StartBit;
	pDstByte = (unsigned char*)(pDstBuf + StartByte);
	pSrcByte = (unsigned char*)pSrcBuf;

	if (TotalWidth <= 8) {
		if (RemainBits >= TotalWidth) {
			*pDstByte |= ((*pSrcByte) & HHBitMask[TotalWidth - 1])
				<< StartBit;
		} else {
			*pDstByte |= ((*pSrcByte) << StartBit);
			TotalWidth -= RemainBits;
			*(pDstByte + 1) |= (((*pSrcByte) >> RemainBits) &
					HHBitMask[TotalWidth - 1]);
		}
	} else if (TotalWidth <= 64) {
		while (TotalWidth) {
			if (TotalWidth >= 8)
				MoveBitsCount = 8;
			else
				MoveBitsCount = TotalWidth;

			*pDstByte |= ((*pSrcByte) << StartBit);
			*(pDstByte + 1) |= (((*pSrcByte) >> RemainBits) &
					HHBitMask[TotalWidth-RemainBits - 1]);
			TotalWidth -= MoveBitsCount;

			pDstByte++;
			pSrcByte++;
		} /* end while(TotalWidth) */
	} else if (TotalWidth == 128) { /* 128 bits */
		for (i = 0; i < 16; i++) {
			*pDstByte |= ((*pSrcByte) << StartBit);
			*(pDstByte + 1) |= (((*pSrcByte) >> RemainBits) &
					HHBitMask[StartBit]);
			pDstByte++;
			pSrcByte++;
		}
	} /* end if(TotalWidth <= 8) */

	return;
} /* fe_tbl_HashHash_Fill_Fields */


void fe_tbl_HashHash_do_bits_table(unsigned char * HHBuffer, fe_sw_hash_t *swhash)
{
	memset(HHBuffer, 0, FE_HASH_HASH_BYTES);

	fe_tbl_HashHash_Fill_Fields(HHBuffer, swhash->mac_da,
			cs_fe_hash_hash_fields[FE_HASH_DA].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_DA].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer, swhash->mac_sa,
			cs_fe_hash_hash_fields[FE_HASH_SA].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_SA].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->eth_type)),
			cs_fe_hash_hash_fields[FE_HASH_ETHERTYPE_RAW].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_ETHERTYPE_RAW].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->llc_type_enc)),
			cs_fe_hash_hash_fields[FE_HASH_LLC_TYPE_ENC].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_LLC_TYPE_ENC].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ip_frag)),
			cs_fe_hash_hash_fields[FE_HASH_IP_FRAGMENT].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IP_FRAGMENT].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->tpid_enc_1)),
			cs_fe_hash_hash_fields[FE_HASH_TPID_ENC_1].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_TPID_ENC_1].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->_8021p_1)),
			cs_fe_hash_hash_fields[FE_HASH_8021P_1].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_8021P_1].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->dei_1)),
			cs_fe_hash_hash_fields[FE_HASH_DEI_1].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_DEI_1].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->vid_1)),
			cs_fe_hash_hash_fields[FE_HASH_VID_1].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_VID_1].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->revd_135)),
			cs_fe_hash_hash_fields[FE_HASH_RSVD_135].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_RSVD_135].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->tpid_enc_2)),
			cs_fe_hash_hash_fields[FE_HASH_TPID_ENC_2].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_TPID_ENC_2].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->_8021p_2)),
			cs_fe_hash_hash_fields[FE_HASH_8021P_2].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_8021P_2].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->dei_2)),
			cs_fe_hash_hash_fields[FE_HASH_DEI_2].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_DEI_2].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->vid_2)),
			cs_fe_hash_hash_fields[FE_HASH_VID_2].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_VID_2].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer, (unsigned char*)&swhash->da[0],
			cs_fe_hash_hash_fields[FE_HASH_IP_DA].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IP_DA].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer, (unsigned char*)swhash->sa,
			cs_fe_hash_hash_fields[FE_HASH_IP_SA].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IP_SA].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ip_prot)),
			cs_fe_hash_hash_fields[FE_HASH_IP_PROT].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IP_PROT].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer, (unsigned char*)(&(swhash->dscp)),
			cs_fe_hash_hash_fields[FE_HASH_DSCP].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_DSCP].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer, (unsigned char*)(&(swhash->ecn)),
			cs_fe_hash_hash_fields[FE_HASH_ECN].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_ECN].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->pktlen_rng_match_vector)),
			cs_fe_hash_hash_fields[FE_HASH_PKTLEN_RNG_MATCH_VECTOR].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_PKTLEN_RNG_MATCH_VECTOR].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ipv6_flow_label)),
			cs_fe_hash_hash_fields[FE_HASH_IPV6_FLOW_LBL].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IPV6_FLOW_LBL].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ip_version)),
			cs_fe_hash_hash_fields[FE_HASH_IP_VER].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IP_VER].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ip_valid)),
			cs_fe_hash_hash_fields[FE_HASH_IP_VLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IP_VLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l4_dp)),
			cs_fe_hash_hash_fields[FE_HASH_L4_DP_EXACT].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L4_DP_EXACT].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l4_sp)),
			cs_fe_hash_hash_fields[FE_HASH_L4_SP_EXACT].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L4_SP_EXACT].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->tcp_ctrl_flags)),
			cs_fe_hash_hash_fields[FE_HASH_TCP_CTRL].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_TCP_CTRL].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->tcp_ecn_flags)),
			cs_fe_hash_hash_fields[FE_HASH_TCP_ECN].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_TCP_ECN].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l4_valid)),
			cs_fe_hash_hash_fields[FE_HASH_L4_VLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L4_VLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->sdbid)),
			cs_fe_hash_hash_fields[FE_HASH_SDB_KEYRULE].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_SDB_KEYRULE].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->lspid)),
			cs_fe_hash_hash_fields[FE_HASH_LSPID].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_LSPID].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->fwdtype)),
			cs_fe_hash_hash_fields[FE_HASH_FWDTYPE].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_FWDTYPE].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->pppoe_session_id_valid)),
			cs_fe_hash_hash_fields[FE_HASH_PPPOE_SESSION_ID_VLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_PPPOE_SESSION_ID_VLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->pppoe_session_id)),
			cs_fe_hash_hash_fields[FE_HASH_PPPOE_SESSION_ID].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_PPPOE_SESSION_ID].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->mask_ptr_0_7)),
			cs_fe_hash_hash_fields[FE_SDB_HTUPL_MASK_PTR_0_7].start_pos,
			cs_fe_hash_hash_fields[FE_SDB_HTUPL_MASK_PTR_0_7].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->mcgid)),
			cs_fe_hash_hash_fields[FE_HASH_MCGID].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_MCGID].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->mc_idx)),
			cs_fe_hash_hash_fields[FE_HASH_MCIDX].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_MCIDX].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->da_an_mac_sel)),
			cs_fe_hash_hash_fields[FE_HASH_DA_AN_MAC_SEL].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_DA_AN_MAC_SEL].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->da_an_mac_hit)),
			cs_fe_hash_hash_fields[FE_HASH_DA_AN_MAC_HIT].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_DA_AN_MAC_HIT].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->sa_bng_mac_sel)),
			cs_fe_hash_hash_fields[FE_HASH_SA_BNG_MAC_SEL].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_SA_BNG_MAC_SEL].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->sa_bng_mac_hit)),
			cs_fe_hash_hash_fields[FE_HASH_SA_BNG_MAC_HIT].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_SA_BNG_MAC_HIT].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->orig_lspid)),
			cs_fe_hash_hash_fields[FE_HASH_ORIG_LSPID].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_ORIG_LSPID].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->recirc_idx)),
			cs_fe_hash_hash_fields[FE_HASH_RECIRC_IDX].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_RECIRC_IDX].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l7_field)),
			cs_fe_hash_hash_fields[FE_HASH_L7_FIELD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L7_FIELD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l7_field_valid)),
			cs_fe_hash_hash_fields[FE_HASH_L7_FIELD_VLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L7_FIELD_VLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->hdr_a_flags_crcerr)),
			cs_fe_hash_hash_fields[FE_HASH_HDR_A_FLAGS_CRCERR].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_HDR_A_FLAGS_CRCERR].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l3_csum_err)),
			cs_fe_hash_hash_fields[FE_HASH_L3_CHKSUM_ERR].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L3_CHKSUM_ERR].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->l4_csum_err)),
			cs_fe_hash_hash_fields[FE_HASH_L4_CHKSUM_ERR].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_L4_CHKSUM_ERR].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->not_hdr_a_flags_stsvld)),
			cs_fe_hash_hash_fields[FE_HASH_NOT_HDR_A_FLAGS_STSVLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_NOT_HDR_A_FLAGS_STSVLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->hash_fid)),
			cs_fe_hash_hash_fields[FE_HASH_FID].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_FID].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->mc_da)),
			cs_fe_hash_hash_fields[FE_HASH_MC_DA].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_MC_DA].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->bc_da)),
			cs_fe_hash_hash_fields[FE_HASH_BC_DA].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_BC_DA].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->spi_vld)),
			cs_fe_hash_hash_fields[FE_HASH_SPI_VLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_SPI_VLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->spi_idx)),
			cs_fe_hash_hash_fields[FE_HASH_SPI_IDX].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_SPI_IDX].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ipv6_ndp)),
			cs_fe_hash_hash_fields[FE_HASH_IPV6_NDP].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IPV6_NDP].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ipv6_hbh)),
			cs_fe_hash_hash_fields[FE_HASH_IPV6_HBH].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IPV6_HBH].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ipv6_rh)),
			cs_fe_hash_hash_fields[FE_HASH_IPV6_RH].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IPV6_RH].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ipv6_doh)),
			cs_fe_hash_hash_fields[FE_HASH_IPV6_DOH].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_IPV6_DOH].total_width);

 	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ppp_protocol_vld)),
			cs_fe_hash_hash_fields[FE_HASH_PPP_PROTOCOL_VLD].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_PPP_PROTOCOL_VLD].total_width);

	fe_tbl_HashHash_Fill_Fields(HHBuffer,
			(unsigned char*)(&(swhash->ppp_protocol)),
			cs_fe_hash_hash_fields[FE_HASH_PPP_PROTOCOL].start_pos,
			cs_fe_hash_hash_fields[FE_HASH_PPP_PROTOCOL].total_width );
	return;
} /* fe_tbl_HashHash_do_bits_table */

/* This API is used to calculate CRC16 based on given polynomial.
 * 0 : x^16+x^12+x^5+1 (also known as CCITT CRC-16).
 * 1 : x^14+x^4+1
 * 2 : x^14+x^8+x^5+x^4+1
 * 3 : x^14+x^11+x^9+x^8+x^5+x^4+1 */
u16 cs_fe_hash_keygen_crc16(u8 polynomial, unsigned char *buff, u16 size)
{
	u16 crc16 = 0x3FFF;
	u16 i;
#ifndef FE_HASH_BYTE_CRC
	u8 data;
#endif

	if (polynomial == CRC16_CCITT)
		crc16 = 0xFFFF;

#ifdef FE_HASH_BYTE_CRC
	switch (polynomial) {
	case CRC16_CCITT:
	default:
		for (i = 0; i < size; i++)
			crc16 = crctable_ccitt16[(crc16 ^
					((unsigned char)buff[i])) & 0xFFL] ^
				(crc16 >> 8);
		break;
	case CRC16_14_1:
		for (i = 0; i < size; i++)
			crc16 = crctable_poly_1[(crc16 ^
					(unsigned char)buff[i]) & 0xFFL] ^
				(crc16 >> 8);
		break;
	case CRC16_14_2:
		for (i = 0; i < size; i++)
			crc16 = crctable_poly_2[(crc16 ^
					(unsigned char)buff[i]) & 0xFFL] ^
				(crc16 >> 8);
		break;
	case CRC16_14_3:
		for (i = 0; i < size; i++)
			crc16 = crctable_poly_3[(crc16 ^
					(unsigned char)buff[i]) & 0xFFL] ^
				(crc16 >> 8);
		break;
	} /* end switch */

	crc16 = reverse_n(crc16, 16);
#else
	for (i = FE_HASH_HASH_BITS; i > 0; i--) {
		data = cs_getBit(buff, i - 1);
		switch (polynomial) {
		case CRC16_CCITT:
			crc16 = cs_update_crc_ccitt(crc16, data);
			break;
		case CRC16_14_1:
			crc16 = cs_update_crc_14_1(crc16, data);
			break;
		case CRC16_14_2:
			crc16 = cs_update_crc_14_2(crc16, data);
			break;
		case CRC16_14_3:
			crc16 = cs_update_crc_14_3(crc16, data);
			break;
		default:
			crc16 = cs_update_crc_ccitt(crc16, data);
			break;
		} /* end switch */
	}
#endif

	return crc16;
} /* cs_fe_hash_keygen_crc16 */

u32 cs_fe_hash_keygen_crc32(unsigned char *buff, u16 bitNumber)
{
	u32 crc32 = 0xFFFFFFFF;
	u16 i;
#ifndef FE_HASH_BYTE_CRC
	u8 data;
#else
	u8 size;
#endif

#ifdef FE_HASH_BYTE_CRC
	size = bitNumber / 8;
	for (i = 0; i < size; i++) {
		crc32 = crctable_32[(crc32 ^ (unsigned char)buff[i]) & 0xFFL] ^
			(crc32 >> 8);
	}
	crc32 = reverse_n(crc32, 32);
#else
	for (i = bitNumber; i > 0; i--) {
		data = cs_getBit(buff, i - 1);
		crc32 = cs_update_crc_32(crc32, data);
	}
#endif
	return crc32;
}

/* For debugging purpose */
static void print_HHBuffer(unsigned char* HHBuffer)
{
	int i;

	printk("\n\n");
	printk("****************** hwBitsHash: HHBuffer ****************\n");
	printk("0x");
	for (i = 0; i < FE_HASH_HASH_BYTES; i++) {
		printk("%2.2x", HHBuffer[FE_HASH_HASH_BYTES - i - 1]);
	}
	printk("\n");
	printk("********************* End HHBuffer ***********************\n");
} /* print_HHBuffer */

/* For Debugging purpose.  It doesn't print all the keys */
static void print_sw_hash_key(fe_sw_hash_t *swhash)
{
	int i;
	printk("******************** Start Hash Key **********************\n");
	if ((swhash->mac_da[0] != 0) || (swhash->mac_da[1] != 0) ||
			(swhash->mac_da[2] != 0) || (swhash->mac_da[3] != 0) ||
			(swhash->mac_da[4] != 0) || (swhash->mac_da[5] != 0)) {
		printk("MAC DA: ");
		for (i = 0; i < 6; i++)
			printk("%02X ", swhash->mac_da[5 - i]);
		printk("\n");
	}

	if ((swhash->mac_sa[0] != 0) || (swhash->mac_sa[1] != 0) ||
			(swhash->mac_sa[2] != 0) || (swhash->mac_sa[3] != 0) ||
			(swhash->mac_sa[4] != 0) || (swhash->mac_sa[5] != 0)) {
		printk("MAC SA: ");
		for (i = 0; i < 6; i++)
			printk("%02x ", swhash->mac_sa[5 - i]);
		printk("\n");
	}

	if (swhash->eth_type != 0)
		printk("eth_type: 0x%x \n", swhash->eth_type);

	if (swhash->tpid_enc_1 != 0)
		printk("tpid_enc_1: 0x%x \n", swhash->tpid_enc_1);

	if (swhash->_8021p_1 != 0)
		printk("_8021p_1: 0x%x \n", swhash->_8021p_1);

	if (swhash->dei_1 != 0)
		printk("dei_1: 0x%x \n", swhash->dei_1);

	if (swhash->vid_1 != 0)
		printk("vid_1: 0x%x \n", swhash->vid_1);

	if (swhash->tpid_enc_2 != 0)
		printk("tpid_enc_2: 0x%x \n", swhash->tpid_enc_2);

	if (swhash->_8021p_2 != 0)
		printk("_8021p_2: 0x%x \n", swhash->_8021p_2);

	if (swhash->dei_2 != 0)
		printk("dei_2: 0x%x \n", swhash->dei_2);

	if (swhash->vid_2 != 0)
		printk("vid_2: 0x%x \n", swhash->vid_2);

	if (swhash->da[0] != 0)
		printk("L3 DIP: 0x%x \n", swhash->da[0]);
	if (swhash->da[1] != 0)
		printk("L3 DIP_1: 0x%x \n", swhash->da[1]);
	if (swhash->da[2] != 0)
		printk("L3 DIP_2: 0x%x \n", swhash->da[2]);
	if (swhash->da[3] != 0)
		printk("L3 DIP_3: 0x%x \n", swhash->da[3]);

	if (swhash->sa[0] != 0)
		printk("L3 SIP: 0x%x \n", swhash->sa[0]);
	if (swhash->sa[1] != 0)
		printk("L3 SIP_1: 0x%x \n", swhash->sa[1]);
	if (swhash->sa[2] != 0)
		printk("L3 SIP_2: 0x%x \n", swhash->sa[2]);
	if (swhash->sa[3] != 0)
		printk("L3 SIP_3: 0x%x \n", swhash->sa[3]);

	if (swhash->ip_prot != 0)
		printk("IP Protocol: 0x%x \n", swhash->ip_prot);

	if (swhash->dscp != 0)
		printk("DSCP: 0x%x \n", swhash->dscp);

	if (swhash->pktlen_rng_match_vector != 0)
		printk("pktlen_rng_match_vector: 0x%x \n",
				swhash->pktlen_rng_match_vector);

	if (swhash->ipv6_flow_label != 0)
		printk("IPv6 flow label: 0x%x \n", swhash->ipv6_flow_label);

	if (swhash->ip_version != 0)
		printk("IP Version: 0x%x \n", swhash->ip_version);

	if (swhash->ip_valid != 0)
		printk("IP Valid: 0x%x \n", swhash->ip_valid);

	if (swhash->l4_sp != 0)
		printk("L4 sport: 0x%04x \n", swhash->l4_sp);

	if (swhash->l4_dp != 0)
		printk("L4 dport: 0x%04x \n", swhash->l4_dp);

	if (swhash->tcp_ctrl_flags != 0)
		printk("tcp_ctrl_flags: 0x%x \n", swhash->tcp_ctrl_flags);

	if (swhash->l4_valid != 0)
		printk("L4 Valid: 0x%x \n", swhash->l4_valid);

	if (swhash->sdbid != 0)
		printk("SDB INDEX: %d \n", swhash->sdbid);

	if (swhash->lspid != 0)
		printk("LSPID: %d \n", swhash->lspid);

	if (swhash->fwdtype != 0)
		printk("FWD TYPE: %d \n", swhash->fwdtype);

	if (swhash->pppoe_session_id_valid != 0)
		printk("pppoe_session_id_valid: %d \n",
				swhash->pppoe_session_id_valid);

	if (swhash->pppoe_session_id != 0)
		printk("PPPoE session id: 0x%02x \n", swhash->pppoe_session_id);

	printk("mask_ptr_0_7: %d \n", swhash->mask_ptr_0_7);

	if (swhash->mcgid != 0)
		printk("MCGID: %d \n", swhash->mcgid);

	if (swhash->mc_idx != 0)
		printk("MC INDEX: %d \n", swhash->mc_idx);

	if (swhash->orig_lspid != 0)
		printk("Orig LSPID: %d \n", swhash->orig_lspid);

	if (swhash->recirc_idx != 0)
		printk("recirc_idx: 0x%x \n", swhash->recirc_idx);

	if (swhash->spi_idx != 0)
			printk("spi_idx: 0x%x \n", swhash->spi_idx);

	if (swhash->spi_vld != 0)
			printk("spi_vld: 0x%x \n", swhash->spi_vld);

	if (swhash->l7_field_valid != 0)
			printk("l7_field: 0x%08x\n", swhash->l7_field);

	printk("********************* End Hash Key ***********************\n");
} /* print_sw_hash_key */

int cs_fe_hash_calc_crc(fe_sw_hash_t *swhash, u32 *pCrc32, u16 *pCrc16,
		u8 crc16_polynomial)
{
	unsigned char HHBuffer[FE_HASH_HASH_BYTES];

	fe_tbl_HashHash_do_bits_table(HHBuffer, swhash);

	DBG(print_sw_hash_key(swhash));
	DBG(print_HHBuffer(HHBuffer));

	*pCrc32 = cs_fe_hash_keygen_crc32(HHBuffer, FE_HASH_HASH_BITS);
	*pCrc16 = cs_fe_hash_keygen_crc16(crc16_polynomial ,HHBuffer,
			sizeof(HHBuffer));
	return 0;
} /* cs_fe_hash_calc_crc */

int cs_fe_hash_init(void)
{
	FETOP_HASH_STATUS_t hash_status_reg;
	FETOP_HASH_INIT_t hash_init_reg;
	unsigned int try_loop = 0;

	/* check if ready to initialize */
	do {
		hash_status_reg.wrd = readl(FETOP_HASH_STATUS);
		udelay(1000);
	} while ((hash_status_reg.bf.ready_for_init == 0) &&
			(try_loop++ < HASH_TRY_TIMEOUT));

	if (try_loop >= HASH_TRY_TIMEOUT) {
		printk("hash not ready to init:0x%x\n", hash_status_reg.wrd);
		return -1;
	}

	/* initialization */
	hash_init_reg.bf.init = 1;
	writel(hash_init_reg.wrd, FETOP_HASH_INIT);

	/* check if initialization complete */
	try_loop = 0;
	do {
		hash_status_reg.wrd = readl(FETOP_HASH_STATUS);
		udelay(1000);
	} while ((hash_status_reg.bf.init_in_progress == 1) &&
			(try_loop++ < HASH_TRY_TIMEOUT));

	if (try_loop >= HASH_TRY_TIMEOUT) {
		printk("hash init fail (Timeout):0%x\n", hash_status_reg.wrd);
		return -1;
	}

	spin_lock_init(&cs_fe_hash_lock);
	memset(cs_fe_hash_used_mask, 0x0, FE_HASH_STATUS_ENTRY_MAX * 8);
	cs_fe_hash_timer_init();
	// FIXME!! more?
	return FE_TABLE_OK;
} /* cs_fe_hash_init */
EXPORT_SYMBOL(cs_fe_hash_init);

void cs_fe_hash_exit(void)
{
	del_timer_sync(&cs_fe_hash_timer_obj);
	// FIXME!! more??
} /* cs_fe_hash_exit */
EXPORT_SYMBOL(cs_fe_hash_exit);

