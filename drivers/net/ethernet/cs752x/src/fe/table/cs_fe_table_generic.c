/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_generic.c
 *
 * $Id: cs_fe_table_generic.c,v 1.8 2011/12/20 01:52:09 whsu Exp $
 *
 * It contains the generic resource management implementation for all HW
 * FE Tables.
 */

#include <linux/module.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs_mut.h"

int fe_table_inc_entry_refcnt(cs_fe_table_t *table_type_ptr, unsigned int idx)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	unsigned long flags;

	if (idx >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(table_type_ptr->content_table, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	atomic_inc(&p_fe_entry->users);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_inc_entry_refcnt */

int fe_table_dec_entry_refcnt(cs_fe_table_t *table_type_ptr, unsigned int idx)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	unsigned long flags;

	if (idx >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(table_type_ptr->content_table, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't decrement the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	/* if it is already 0, just return it */
	if (atomic_read(&p_fe_entry->users) == 0) {
		spin_unlock_irqrestore(&table_type_ptr->lock, flags);
		return FE_TABLE_OK;
	}

	atomic_dec(&p_fe_entry->users);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_dec_entry_refcnt */

int fe_table_get_entry_refcnt(cs_fe_table_t *table_type_ptr, unsigned int idx,
		unsigned int *p_cnt)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	unsigned long flags;

	if (p_cnt == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(table_type_ptr->content_table, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't decrement the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	*p_cnt = atomic_read(&p_fe_entry->users);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_get_entry_refcnt */

void *fe_table_malloc_table_entry(cs_fe_table_t *table_type_ptr)
{
	fe_table_entry_t *p_rslt = (fe_table_entry_t*) cs_zalloc(
			sizeof(fe_table_entry_t), GFP_ATOMIC);

	if (p_rslt == NULL)
		return NULL;
	atomic_set(&p_rslt->users, 0);
	p_rslt->p_entry = cs_zalloc(table_type_ptr->entry_size, GFP_ATOMIC);
	if (p_rslt->p_entry == NULL) {
		cs_free(p_rslt);
		return NULL;
	}

	return (void*)p_rslt;
} /* fe_table_malloc_table_entry */

int fe_table_alloc_entry(cs_fe_table_t *table_type_ptr, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	cs_table_entry_t *p_entry;
	bool f_found = false;
	unsigned long flags;
	unsigned int start_loc;

	if (rslt_idx == NULL)
		return FE_TABLE_ENULLPTR;
	if (start_offset >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (table_type_ptr->max_entry == table_type_ptr->used_entry)
		return FE_TABLE_ETBLFULL;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	start_loc = start_offset? start_offset : table_type_ptr->curr_ptr;
	*rslt_idx = start_loc;
	do {
		/* wrap around */
		if ((*rslt_idx) == table_type_ptr->max_entry) *rslt_idx = 0;

		p_entry = cs_table_get_entry(table_type_ptr->content_table,
				*rslt_idx);
		if (p_entry == NULL) {
			spin_unlock_irqrestore(&table_type_ptr->lock, flags);
			return FE_TABLE_ENULLPTR;
		}

		if ((p_entry->local_data & FE_TABLE_ENTRY_USED) == 0) {
			p_entry->local_data |= FE_TABLE_ENTRY_USED;
			table_type_ptr->used_entry++;
			table_type_ptr->curr_ptr = (*rslt_idx) + 1;
			f_found = true;
		} else {
			(*rslt_idx)++;
			if ((*rslt_idx) == start_loc) {
				/*
				 * we have already gone around the world and
				 * found nothing available.
				 */
				(*rslt_idx) = 0;
				spin_unlock_irqrestore(&table_type_ptr->lock,
						flags);
				return FE_TABLE_ETBLFULL;
			}
		}
	} while (f_found == false);

	spin_unlock_irqrestore(&table_type_ptr->lock, flags);
	return FE_TABLE_OK;
} /* fe_table_alloc_entry */

int fe_table_set_entry(cs_fe_table_t *table_type_ptr, unsigned int idx,
		void *entry)
{
	cs_table_entry_t *p_sw_entry;
	__u32 data_array[28];
	unsigned int table_size;
	int ret;
	unsigned long flags;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;
	if (table_type_ptr->op.convert_sw_to_hw_data == NULL)
		return FE_TABLE_EOPNOTSUPP;

	p_sw_entry = cs_table_get_entry(table_type_ptr->content_table, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/* Deal with HW table first */
	ret = cs_fe_hw_table_get_entry_value(table_type_ptr->type_id,
			idx, data_array, &table_size);
	if (ret != 0)
		return ret;

	//printk("%s:%d:table size = %d\n", __func__, __LINE__, table_size);
	/* Generate the data register value based on the given entry */
	ret = table_type_ptr->op.convert_sw_to_hw_data(entry, data_array,
			table_size);
	if (ret != 0)
		return ret;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	/* set it to HW indirect access table */
	ret = cs_fe_hw_table_set_entry_value(table_type_ptr->type_id,
			idx, table_size, data_array);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);
	if (ret != 0)
		return ret;

	/* store the SW entry in SW table */
	spin_lock_irqsave(&table_type_ptr->lock, flags);
	if (p_sw_entry->data == NULL)
		p_sw_entry->data = fe_table_malloc_table_entry(table_type_ptr);
	if (p_sw_entry->data == NULL) {
		spin_unlock_irqrestore(&table_type_ptr->lock, flags);
		return -ENOMEM;
	}
	memcpy(((fe_table_entry_t*)p_sw_entry->data)->p_entry, entry,
			table_type_ptr->entry_size);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_set_entry */

int fe_table_add_entry(cs_fe_table_t *table_type_ptr, void *entry,
		unsigned int *rslt_idx)
{
	int ret;

	if ((entry == NULL) || (rslt_idx == NULL))
		return FE_TABLE_ENULLPTR;
	if ((table_type_ptr->op.find_entry == NULL) ||
			(table_type_ptr->op.inc_entry_refcnt == NULL) ||
			(table_type_ptr->op.alloc_entry == NULL) ||
			(table_type_ptr->op.set_entry == NULL))
		return FE_TABLE_EOPNOTSUPP;

	ret = table_type_ptr->op.find_entry(entry, rslt_idx, 0);
	if (ret == FE_TABLE_OK)
		return table_type_ptr->op.inc_entry_refcnt(*rslt_idx);
	else if (ret == FE_TABLE_ENTRYNOTFOUND) {
		ret = table_type_ptr->op.alloc_entry(rslt_idx, 0);
		if (ret != 0)
		return ret;

		ret = table_type_ptr->op.set_entry(*rslt_idx, entry);
		if (ret != 0)
		return ret;

		ret = table_type_ptr->op.inc_entry_refcnt(*rslt_idx);
		if (ret != 0)
		return ret;
	}
	return ret;
} /* fe_table_add_entry */

int fe_table_find_entry(cs_fe_table_t *table_type_ptr, void *entry,
		unsigned int *rslt_idx, unsigned int start_offset)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	unsigned int i, remaining_used;

	if ((entry == NULL) || (rslt_idx == NULL))
		return FE_TABLE_ENULLPTR;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;
	if (start_offset >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;

	remaining_used = table_type_ptr->used_entry;

	for (i = start_offset; i < table_type_ptr->max_entry; i++) {
		if (remaining_used == 0)
		return FE_TABLE_ENTRYNOTFOUND;

		p_sw_entry = cs_table_get_entry(table_type_ptr->content_table,
				i);
		if ((p_sw_entry != NULL) &&
				(p_sw_entry->local_data & 
				 FE_TABLE_ENTRY_USED)) {
			remaining_used--;
			p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
			if ((p_fe_entry != NULL) &&
					(p_fe_entry->p_entry != NULL) &&
					(!memcmp(p_fe_entry->p_entry, entry,
						table_type_ptr->entry_size))) {
				*rslt_idx = i;
				return FE_TABLE_OK;
			}
		}
	}
	return FE_TABLE_ENTRYNOTFOUND;
} /* fe_table_find_entry */

int fe_table_del_entry_by_idx(cs_fe_table_t *table_type_ptr, unsigned int idx,
		bool f_force)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	bool f_clean_entry = false;
	unsigned long flags;
	int ret;

	if (idx >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(table_type_ptr->content_table, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't decrement the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL) {
		/* the entry is reserved, but not used */
		table_type_ptr->used_entry--;
		p_sw_entry->local_data &= ~FE_TABLE_ENTRY_USED;
		spin_unlock_irqrestore(&table_type_ptr->lock, flags);
		return FE_TABLE_OK;
	}

	if (f_force == true) atomic_set(&p_fe_entry->users, 0);

	/* if it is already 0, just
		return it */
	if (atomic_read(&p_fe_entry->users) == 0) f_clean_entry = true;

	if ((f_clean_entry == false) &&
			(atomic_dec_and_test(&p_fe_entry->users)))
		f_clean_entry = true;

	if (f_clean_entry == true) {
		cs_free(p_fe_entry->p_entry);
		p_fe_entry->p_entry = NULL;
		cs_free(p_sw_entry->data);
		p_sw_entry->data = NULL;
		table_type_ptr->used_entry--;
		p_sw_entry->local_data &= ~FE_TABLE_ENTRY_USED;
		ret = cs_fe_hw_table_clear_entry(table_type_ptr->type_id, idx);
		if (ret != 0) {
			spin_unlock_irqrestore(&table_type_ptr->lock, flags);
			return ret;
		}
	}

	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_del_entry_by_idx */

int fe_table_del_entry(cs_fe_table_t *table_type_ptr, void *entry, bool f_force)
{
	int ret;
	unsigned int index;

	ret = table_type_ptr->op.find_entry(entry, &index, 0);
	if (ret != FE_TABLE_OK)
		return ret;

	return table_type_ptr->op.del_entry_by_idx(index, f_force);
} /* fe_table_del_entry */

int fe_table_get_entry(cs_fe_table_t *table_type_ptr, unsigned int idx,
		void *entry)
{
	cs_table_entry_t *p_sw_entry;
	unsigned long flags;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= table_type_ptr->max_entry)
		return FE_TABLE_EOUTRANGE;
	if (table_type_ptr->content_table == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(table_type_ptr->content_table, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	/* We don't increment the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;
	if (((fe_table_entry_t*)p_sw_entry->data)->p_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(&table_type_ptr->lock, flags);
	memcpy(entry, ((fe_table_entry_t*)p_sw_entry->data)->p_entry,
			table_type_ptr->entry_size);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_get_entry */

int fe_table_flush_table(cs_fe_table_t *table_type_ptr)
{
	unsigned int i;
	unsigned long flags;

	if (table_type_ptr->op.del_entry_by_idx != NULL) {
		for (i = 0; i < table_type_ptr->max_entry; i++)
			table_type_ptr->op.del_entry_by_idx(i, true);
	}

	/* purposedly do the following again to make sure the HW table
	 * is really flushed */
	spin_lock_irqsave(&table_type_ptr->lock, flags);
	table_type_ptr->used_entry = 0;
	table_type_ptr->curr_ptr = 0;
	cs_fe_hw_table_flush_table(table_type_ptr->type_id);
	spin_unlock_irqrestore(&table_type_ptr->lock, flags);

	return FE_TABLE_OK;
} /* fe_table_flush_table */

int fe_table_get_avail_count(cs_fe_table_t *table_type_ptr)
{
	return (table_type_ptr->max_entry - table_type_ptr->used_entry);
} /* fe_table_get_avail_count */

