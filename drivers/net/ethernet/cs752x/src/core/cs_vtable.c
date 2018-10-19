/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_vtable.c
 *
 * $Id$
 *
 * It contains the implementation of vtable framework, which is abstract
 * entity that is used to maintain several FE entities at once.
 */

#include <linux/spinlock.h>
#include "cs_core_vtable.h"
#include "cs_vtable.h"
#include "cs_fe.h"
#include "cs_fe_mc.h"
#include "cs_mut.h"

/* allocate a vtable by allocating its Classifier and SDB.  Set up Classifier
 * according to the given classifier.  Set up the default action according to
 * the given default action.
 * Input: 1) Pointer to classifier
 * 	  2) default action type
 * Return: Pointer to vtable if succeeds.  Null, if otherwise. */
cs_vtable_t *cs_vtable_alloc(fe_class_entry_t *p_class, unsigned int def_act, unsigned int vtbl_type)
{
	cs_vtable_t *new_table;
	fe_sdb_entry_t sdb_entry;
	int ret;

	new_table = cs_zalloc(sizeof(cs_vtable_t), GFP_ATOMIC);
	if (new_table == NULL)
		return NULL;

	new_table->uuflow_idx = 0xffff;
	new_table->bcflow_idx = 0xffff;
	new_table->umflow_idx = 0xffff;
	new_table->mcgid = MCG_INIT_MCGID;
	new_table->vtable_type = vtbl_type;
	spin_lock_init(&new_table->lock);

	/* alloc classifier and sdb entry */
	ret = cs_fe_table_alloc_entry(FE_TABLE_SDB, &new_table->sdb_index, 0);
	if (ret != 0) {
		cs_free(new_table);
		return NULL;
	}

	memset((void *)&sdb_entry, 0x0, sizeof(sdb_entry));
	ret = cs_fe_table_set_entry(FE_TABLE_SDB, new_table->sdb_index,
			&sdb_entry);
	if (ret != 0)
		goto EXIT_FREE_SDB;

	ret = cs_fe_table_inc_entry_refcnt(FE_TABLE_SDB, new_table->sdb_index);
	if (ret != 0)
		goto EXIT_FREE_SDB;

	ret = cs_vtable_set_def_action(new_table, (CS_VTABLE_DEF_ACT_TYPE_UU |
				CS_VTABLE_DEF_ACT_TYPE_UM |
				CS_VTABLE_DEF_ACT_TYPE_BC), def_act);
	if (ret != 0)
		goto EXIT_FREE_SDB;

	/* Done setting SDB.. now move on to classifier */
	p_class->sdb_idx = new_table->sdb_index;
	ret = cs_fe_table_alloc_entry(FE_TABLE_CLASS, &new_table->class_index,
			0);
	if (ret != 0)
		goto EXIT_FREE_SDB;

	ret = cs_fe_table_set_entry(FE_TABLE_CLASS, new_table->class_index,
			p_class);
	if (ret != 0)
		goto EXIT_FREE_CLASS;

	ret = cs_fe_table_inc_entry_refcnt(FE_TABLE_CLASS,
			new_table->class_index);
	if (ret != 0)
		goto EXIT_FREE_CLASS;

	return new_table;

EXIT_FREE_CLASS:
	cs_fe_table_del_entry_by_idx(FE_TABLE_CLASS, new_table->class_index,
			false);

EXIT_FREE_SDB:
	cs_fe_table_del_entry_by_idx(FE_TABLE_SDB, new_table->sdb_index, false);

	cs_free(new_table);

	return NULL;
} /* cs_vtable_alloc */

/* free vtable. Need to release the Classifier, SDB, hash masks, and the
 * forwarding results used for default actions */
int cs_vtable_free(cs_vtable_t *table)
{
	unsigned int uuflow_idx = 0xffff, bcflow_idx = 0xffff;
	int ret, i;
	fe_sdb_entry_t sdb_entry;

	if (table == NULL)
		return -1;

	spin_lock(&table->lock);

	ret = cs_fe_table_del_entry_by_idx(FE_TABLE_CLASS, table->class_index,
			false);
	if (ret != 0)
		printk("%s:%d:failed at deleting CLASS entry\n", __func__,
				__LINE__);

	ret = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0)
		printk("%s:%d:failed at getting CLASS entry\n", __func__,
				__LINE__);

	for (i = 0; i < 8; i++) {
		if (sdb_entry.sdb_tuple[i].enable == 1) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK,
					sdb_entry.sdb_tuple[i].mask_ptr, false);
			sdb_entry.sdb_tuple[i].enable = 0;
			sdb_entry.sdb_tuple[i].mask_ptr = 0;
		}
	}

	ret = cs_fe_table_del_entry_by_idx(FE_TABLE_SDB, table->sdb_index,
			false);
	if (ret != 0) {
		printk("%s:%d:failed at deleting SDB entry\n", __func__,
				__LINE__);
		spin_unlock(&table->lock);
		return ret;
	}

	if (table->uuflow_idx != 0xffff) {
		uuflow_idx = table->uuflow_idx;
		ret = cs_fe_fwdrslt_del_by_idx(table->uuflow_idx);
		if (ret != 0)
			printk("%s:%d:failed to delete uuflow idx %d\n",
					__func__, __LINE__, table->uuflow_idx);
	}

	if ((table->bcflow_idx != 0xffff) &&
			(table->bcflow_idx != uuflow_idx)) {
		bcflow_idx = table->bcflow_idx;
		ret = cs_fe_fwdrslt_del_by_idx(table->bcflow_idx);
		if (ret != 0)
			printk("%s:%d:failed to delete bcflow idx %d\n",
					__func__, __LINE__, table->bcflow_idx);
	}

	if ((table->umflow_idx != 0xffff) &&
			(table->umflow_idx != uuflow_idx) &&
			(table->umflow_idx != bcflow_idx)) {
		ret = cs_fe_fwdrslt_del_by_idx(table->umflow_idx);
		if (ret != 0)
			printk("%s:%d:failed to delete umflow idx %d\n",
					__func__, __LINE__, table->umflow_idx);
	}

	if (IS_ARBITRARY_REPLICATION_MODE(table->mcgid))
		cs_fe_free_mcg_vtable_id(MCG_VTABLE_ID(table->mcgid));

	spin_unlock(&table->lock);

	return 0;
} /* cs_vtable_free */

extern cs_core_vtable_def_hashmask_info_t vtable_def_hm_info[];
/* set up default action to the specific default action type */
int cs_vtable_set_def_action(cs_vtable_t *table, u8 def_act_type_mask,
		unsigned int def_act)
{
	unsigned int voq_id = def_act;
	unsigned int fwdrslt_idx = 0, voqpol_idx = 0;
	unsigned int um_fwdrslt_idx = 0, um_voqpol_idx = 0;
	int ret;
	fe_fwd_result_entry_t fwdrslt_entry, um_fwdrslt_entry;
	fe_voq_pol_entry_t voqpol_entry, um_voqpol_entry;
	fe_sdb_entry_t sdb_entry;
	cs_core_vtable_def_hashmask_info_t *vtable_info;

	if (table == NULL)
		return -1;

	ret = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0)
		return ret;

	memset(&fwdrslt_entry, 0, sizeof(fwdrslt_entry));
	memset(&voqpol_entry, 0, sizeof(voqpol_entry));

	voqpol_entry.voq_base = voq_id;
	voqpol_entry.cos_nop = 0;
	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voqpol_entry,
			&voqpol_idx);
	if (ret != 0)
		return ret;

	fwdrslt_entry.dest.voq_policy = 1;
	fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwdrslt_entry,
			&fwdrslt_idx);
	if (ret != 0) {
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
				voqpol_idx, false);
		return ret;
	}

	/* default action of umflow is to drop */
	memset(&um_fwdrslt_entry, 0, sizeof(um_fwdrslt_entry));
	memset(&um_voqpol_entry, 0, sizeof(um_voqpol_entry));
	if (table->vtable_type == CORE_VTABLE_TYPE_ICMPV6) {
		um_fwdrslt_entry.dest.voq_policy = 1;
		um_fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;
	} else {
		um_fwdrslt_entry.act.drop = 1;
	}
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &um_fwdrslt_entry,
			&um_fwdrslt_idx);
	if (ret != 0) {
//		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
//				um_voqpol_idx, false);
		return ret;
	}

	if (def_act_type_mask & CS_VTABLE_DEF_ACT_TYPE_UU) {
		sdb_entry.misc.uu_flowidx = fwdrslt_idx;
		table->uuflow_idx = fwdrslt_idx;
	}
	if (def_act_type_mask & CS_VTABLE_DEF_ACT_TYPE_UM) {
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
		sdb_entry.misc.um_flowidx = um_fwdrslt_idx;
		table->umflow_idx = um_fwdrslt_idx;
#else
		sdb_entry.misc.um_flowidx = fwdrslt_idx;
		table->umflow_idx = fwdrslt_idx;
#endif
	}
	if (def_act_type_mask & CS_VTABLE_DEF_ACT_TYPE_BC) {
		sdb_entry.misc.bc_flowidx = fwdrslt_idx;
		table->bcflow_idx = fwdrslt_idx;
	}

	sdb_entry.misc.ttl_hop_limit_zero_discard_en = 1;

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS	
	if ((table->vtable_type == CORE_VTABLE_TYPE_CPU) 
		|| (table->vtable_type == CORE_VTABLE_TYPE_CPU_L3) 	
	){
		sdb_entry.misc.acl_dsbl = 1;
	}
#endif				
	/* turn on lpm_enable according to the hashmask information table */
        vtable_info = &vtable_def_hm_info[table->vtable_type];

        if (vtable_info->lpm_enable) {
                sdb_entry.lpm_en = 1;
                sdb_entry.sdb_lpm_v4[0].start_ptr = 0x00;
                sdb_entry.sdb_lpm_v4[0].end_ptr = 0x00;
                sdb_entry.sdb_lpm_v4[0].lpm_ptr_en = 0;
                sdb_entry.sdb_lpm_v4[1].start_ptr = 0x00;
                sdb_entry.sdb_lpm_v4[1].end_ptr = 0x00;
                sdb_entry.sdb_lpm_v4[1].lpm_ptr_en = 0;
                sdb_entry.sdb_lpm_v6[0].start_ptr = 0xf;
                sdb_entry.sdb_lpm_v6[0].end_ptr = 0xf;
                sdb_entry.sdb_lpm_v6[0].lpm_ptr_en = 0;
                sdb_entry.sdb_lpm_v6[1].start_ptr = 0xf;
                sdb_entry.sdb_lpm_v6[1].end_ptr = 0xf;
                sdb_entry.sdb_lpm_v6[1].lpm_ptr_en = 0;
                printk(KERN_DEBUG "%s: table->sdb_index=%d lpm is enabled\n", __func__, table->sdb_index);
        }

	
	ret = cs_fe_table_set_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0) {
		table->uuflow_idx = 0xffff;
		table->umflow_idx = 0xffff;
		table->bcflow_idx = 0xffff;
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, fwdrslt_idx,
				false);
	}

	return ret;
} /* cs_vtable_set_def_action */

/* add given hashmask to the vtable with the given priority.
 * Return the hashmask index if succeed. Else otherwise. */
int cs_vtable_add_hashmask(cs_vtable_t *table, fe_hash_mask_entry_t *hm_ptr,
		unsigned int priority, bool is_qos)
{
	fe_sdb_entry_t sdb_entry;
	unsigned int start_idx, end_idx, check_start_idx, check_end_idx;
	unsigned int hash_mask_idx, iii, dst_idx = 0;
	bool f_new_hashmask = false, find_match = false, find_slot = false;
	int ret = 0;
	if ((table == NULL) || (hm_ptr == NULL))
		return 0;

	if (is_qos == true) {
		start_idx = 6;
		end_idx = 7;
		check_start_idx = 0;
		check_end_idx = 5;
	} else {
		start_idx = 0;
		end_idx = 5;
		check_start_idx = 6;
		check_end_idx = 7;
	}

	/* 1) find or create a new hash mask */
	ret = cs_fe_table_find_entry(FE_TABLE_HASH_MASK, hm_ptr,
			&hash_mask_idx, 0);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		ret = cs_fe_table_add_entry(FE_TABLE_HASH_MASK, hm_ptr,
				&hash_mask_idx);
		if (ret != 0)
			return ret;
		f_new_hashmask = true;
	}
	if (ret != 0)
		return ret;

	/* 2) check if there is a place to insert in sdb */
	/* for tuple insertion, we have to take care of the case that when we
	 * are sharing the hashmask entry with another user. we have to make
	 * sure the shared user is not our own tuples of other type, such that
	 * same hash mask setting used in both FWD tuple and QoS tuple should
	 * have different hash_index. If that's the case, we will need to create
	 * a new one! Therefore, the step will be:
	 * 1) going through the vtable, make sure there is no previous entry of
	 * the same index and find the index where we are going to insert this
	 * hashmask to.
	 * 2) Then we compare this hashmask index with other hashmask index
	 * used in this vtable.*/
	spin_lock(&table->lock);
	ret = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0)
		goto EXIT_FREE_HASHMASK;

	for (iii = start_idx; iii <= end_idx; iii++) {
		if ((sdb_entry.sdb_tuple[iii].enable == 0) &&
				(find_slot == false)) {
			find_slot = true;
			dst_idx = iii;
		} else if ((sdb_entry.sdb_tuple[iii].enable == 1) &&
				(sdb_entry.sdb_tuple[iii].mask_ptr ==
				 hash_mask_idx)) {
			find_match = true;
			dst_idx = iii;
			break;
		}
	}

	if (find_match == true) {
		/* the same hash index has been inserted! If given priority is
		 * not 0, then all we do here is updating the priority. */
		if (priority != 0) {
			sdb_entry.sdb_tuple[dst_idx].priority = priority;
			ret = cs_fe_table_set_entry(FE_TABLE_SDB,
					table->sdb_index, &sdb_entry);
		}
		ret = (int)hash_mask_idx;
		goto EXIT_FREE_HASHMASK;
	}

	if (find_slot == false) {
		ret = -1;
		goto EXIT_FREE_HASHMASK;
	}

	/* at this point. we have already located the dst_index.
	 * Now we need to search through the tuple of other type in the same
	 * SDB to make sure we don't have duplicate.  For example, if we are
	 * inserting a hashmask index to tuple#0~5, we shouldn't have the same
	 * hashmask index in tuple#6~7, or the hash will get confused. */
	for (iii = check_start_idx; iii <= check_end_idx; iii++) {
		if ((f_new_hashmask == false) &&
				(sdb_entry.sdb_tuple[iii].enable == 1) &&
				(sdb_entry.sdb_tuple[iii].mask_ptr ==
				 hash_mask_idx)) {
			/* found a matching one. need to either find or create
			 * a new hash mask. */
			ret = cs_fe_table_find_entry(FE_TABLE_HASH_MASK,
					hm_ptr, &hash_mask_idx,
					hash_mask_idx + 1);
			if (ret == FE_TABLE_ENTRYNOTFOUND) {
				ret = cs_fe_table_alloc_entry(
						FE_TABLE_HASH_MASK,
						&hash_mask_idx, 0);
				if (ret != 0)
					goto EXIT_FREE_HASHMASK;

				f_new_hashmask = true;
				ret = cs_fe_table_set_entry(FE_TABLE_HASH_MASK,
						hash_mask_idx,
						(void*)hm_ptr);
				if (ret != 0)
					goto EXIT_FREE_HASHMASK;

				ret = cs_fe_table_inc_entry_refcnt(
						FE_TABLE_HASH_MASK,
						hash_mask_idx);
				if (ret != 0)
					goto EXIT_FREE_HASHMASK;
			}
		}
	}

	/* 3) update SDB */
	/* now we have the proper hash_mask_idx and dst_idx.
	 * We can perform the modification */
	sdb_entry.sdb_tuple[dst_idx].mask_ptr = hash_mask_idx;
	sdb_entry.sdb_tuple[dst_idx].priority = (priority == 0) ?
		dst_idx : priority;
	sdb_entry.sdb_tuple[dst_idx].enable = 1;

	ret = cs_fe_table_set_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0)
		goto EXIT_FREE_HASHMASK;

	/* one thing that we didn't care is tuple priority conflict in the
	 * same sdb.
	 * Also, this logic is exposed to an issue, where when we have the same
	 * hashmask entry in 2 different entries of table, there is a chance
	 * that we might have two or more different hash mask pointed to the
	 * same hash match info in the same tuple, but assuming when one
	 * calculates hash value, the hash mask index is different. */

	spin_unlock(&table->lock);

	if (f_new_hashmask == false)
		cs_fe_table_inc_entry_refcnt(FE_TABLE_HASH_MASK, hash_mask_idx);

	return (int)hash_mask_idx;

EXIT_FREE_HASHMASK:
	spin_unlock(&table->lock);

	if (f_new_hashmask == true)
		cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK, hash_mask_idx,
				false);

	return ret;
} /* cs_vtable_add_hashmask */

static inline int check_sdb_and_del_hashmask(unsigned int sdb_idx,
		unsigned int hm_idx, bool is_qos)
{
	unsigned int start_idx, end_idx, iii;
	fe_sdb_entry_t sdb_entry;
	int ret;

	if (is_qos == true) {
		start_idx = 6;
		end_idx = 7;
	} else {
		start_idx = 0;
		end_idx = 5;
	}

	/* get the SDB info */
	ret = cs_fe_table_get_entry(FE_TABLE_SDB, sdb_idx, &sdb_entry);
	if (ret != 0)
		return ret;

	for (iii = start_idx; iii <= end_idx; iii++) {
		if ((sdb_entry.sdb_tuple[iii].enable == 1) &&
				(sdb_entry.sdb_tuple[iii].mask_ptr == hm_idx)) {
			/* found a matching one.. need to remove it from SDB */
			sdb_entry.sdb_tuple[iii].enable = 0;
			sdb_entry.sdb_tuple[iii].mask_ptr = 0;
			sdb_entry.sdb_tuple[iii].priority = 0;
			ret = cs_fe_table_set_entry(FE_TABLE_SDB, sdb_idx,
					&sdb_entry);
			if (ret != 0)
				return ret;

			/* delete this hashmask entry */
			ret = cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK,
					hm_idx, false);
			if (ret != 0)
				return ret;
		}
	}

	return 0;
} /* check_sdb_and_del_hashmask */

/* find the matching hashmask from given vtable and delete it.
 * return 0 if succeeds (does not matter whether it really delets it or not,
 * but it makes sure there is no matching hash mask in the vtable).
 * Else otherwise */
int cs_vtable_del_hashmask(cs_vtable_t *table, fe_hash_mask_entry_t *hm_ptr,
		bool is_qos)
{
	unsigned int hash_mask_idx, offset = 0;
	int ret = 0;

	if ((table == NULL) || (hm_ptr == NULL))
		return -1;

	/* use a do-while look to find all the hash_mask_index with entries
	 * that match the given hm_ptr */
	do {
		ret = cs_fe_table_find_entry(FE_TABLE_HASH_MASK, hm_ptr,
				&hash_mask_idx, offset);
		if (ret == 0) {
			spin_lock(&table->lock);

			ret = check_sdb_and_del_hashmask(table->sdb_index,
					hash_mask_idx, is_qos);
			if (ret != 0)
				goto EXIT_DEL_TUPLE;

			spin_unlock(&table->lock);

			offset = hash_mask_idx + 1;
		}
	} while (ret == 0);

	return 0;

EXIT_DEL_TUPLE:
	spin_unlock(&table->lock);

	return ret;
} /* cs_vtable_del_hashmask */

/* delete the hash mask with hm_idx from vtable.
 * return 0 if succeeds, else otherwise. */
int cs_vtable_del_hashmask_by_idx(cs_vtable_t *table, u32 hm_idx,
		bool is_qos)
{
	int ret;

	if (table == NULL)
		return -1;

	spin_lock(&table->lock);

	ret = check_sdb_and_del_hashmask(table->sdb_index, hm_idx, is_qos);

	spin_unlock(&table->lock);

	return ret;
} /* cs_vtable_del_hashmask_by_idx */

/* delete all the hash mask associated with this vtable. */
int cs_vtable_del_hashmask_all(cs_vtable_t *table)
{
	fe_sdb_entry_t sdb_entry;
	unsigned int iii, ret;

	if (table == NULL)
		return -1;
	spin_lock(&table->lock);
	ret = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0) {
		spin_unlock(&table->lock);
		return ret;
	}

	for (iii = 0; iii <= 7; iii++) {
		if (sdb_entry.sdb_tuple[iii].enable == 1) {
			/* delete this hashmask entry */
			ret = cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK,
					sdb_entry.sdb_tuple[iii].mask_ptr,
					false);
			if (ret != 0) {
				/* we need to set SDB before quitting it,
				 * bacause we might've deleted some mask */
				cs_fe_table_set_entry(FE_TABLE_SDB,
						table->sdb_index, &sdb_entry);
				spin_unlock(&table->lock);
				return ret;
			}
			sdb_entry.sdb_tuple[iii].enable = 0;
			sdb_entry.sdb_tuple[iii].mask_ptr = 0;
			sdb_entry.sdb_tuple[iii].priority = 0;
		}
	}
	ret = cs_fe_table_set_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);

	spin_unlock(&table->lock);

	return ret;
} /* cs_vtable_del_hashmask_all */


/* find the hashmask index that matches the given hash mask entry in vtable */
int cs_vtable_get_hashmask_idx(cs_vtable_t *table,
		fe_hash_mask_entry_t *hm_ptr, bool is_qos)
{
	unsigned int start_idx, end_idx;
	fe_hash_mask_entry_t curr_hm_entry;
	fe_sdb_entry_t sdb_entry;
	int ret, iii;

	if (table == NULL)
		return -1;

	if (is_qos == true) {
		start_idx = 6;
		end_idx = 7;
	} else {
		start_idx = 0;
		end_idx = 5;
	}

	spin_lock(&table->lock);
	ret = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0) {
		spin_unlock(&table->lock);
		return ret;
	}

	for (iii = start_idx; iii <= end_idx; iii++) {
		if (sdb_entry.sdb_tuple[iii].enable == 1) {
			ret = cs_fe_table_get_entry(FE_TABLE_HASH_MASK,
					sdb_entry.sdb_tuple[iii].mask_ptr,
					&curr_hm_entry);
			if (ret != 0) {
				spin_unlock(&table->lock);
				return ret;
			}
			if (memcmp(hm_ptr, &curr_hm_entry,
						sizeof(fe_hash_mask_entry_t))
					== 0) {
				spin_unlock(&table->lock);
				return sdb_entry.sdb_tuple[iii].mask_ptr;
			}
		}
	}

	/* not found if reach here */
	spin_unlock(&table->lock);

	return -1;
} /* cs_vtable_get_hashmask_idx */

/* insert vtable to the chain by given the pointer to the new vtable, and the
 * previous table. It will insert the new vtable between previous vtable and
 * the next table of previous vtable */
int cs_vtable_insert_to_chain(cs_vtable_t *new_table, cs_vtable_t *prev_table)
{
	cs_vtable_t *next_table, *tmp_table;
	fe_fwd_result_entry_t fwdrslt_entry;
	fe_voq_pol_entry_t voqpol_entry;
	fe_sdb_entry_t prev_sdb_entry, new_sdb_entry;
	fe_class_entry_t new_class_entry;
	int ret;
	unsigned int voqpol_idx, fwdrslt_idx;
	unsigned int tmp_mcgid, i;
	unsigned short mcgid_chk_lst[MCGID_BITS][2] = {
		{0, 0x101}, {0, 0x102}, {0, 0x104}, {0, 0x108},
		{0, 0x110}, {0, 0x120}, {0, 0x140}, {0, 0x180}
	};

	if ((new_table == NULL) || (prev_table == NULL))
		return -1;

	next_table = prev_table->next;

	/* need to assign MCGID, update new_table's default flow forwarding */
	/*
	 * Update p_dst_class for MCGID.
	 * Only need to consider "next vtable is port replication mode" case,
	 * since arbitray replication mode is implemented in cs_vtable_alloc()
	 */
	if (new_table->mcgid == MCG_INIT_MCGID) {
		/* locate the first vtable in the chain */
		tmp_table = prev_table;
		while (tmp_table->prev != NULL) {
			tmp_table = tmp_table->prev;
		}

		while (tmp_table) {
			tmp_mcgid = tmp_table->mcgid;
			if (IS_PORT_REPLICATION_MODE(tmp_mcgid)) {
				for (i = 0; i < MCGID_BITS; i++) {
					if (tmp_mcgid == mcgid_chk_lst[i][1]) {
						mcgid_chk_lst[i][0] = 1;
						break;
					}
				}
			}
			tmp_table = tmp_table->next;
		}

		/* assign an unused MCGID */
		for (i = 0; i < MCGID_BITS; i++) {
			if (mcgid_chk_lst[i][0] == 0) {
				new_table->mcgid = mcgid_chk_lst[i][1];
				break;
			}
		}
		if (i == MCGID_BITS)
			return -EPERM;	/* no more unused MCGID */
	}

	spin_lock(&prev_table->lock);
	spin_lock(&new_table->lock);
	ret = cs_fe_table_get_entry(FE_TABLE_SDB, prev_table->sdb_index,
			&prev_sdb_entry);
	if (ret != 0) {
		spin_unlock(&prev_table->lock);
		spin_unlock(&new_table->lock);
		return ret;
	}

	ret = cs_fe_table_get_entry(FE_TABLE_SDB, new_table->sdb_index,
			&new_sdb_entry);
	if (ret != 0) {
		spin_unlock(&prev_table->lock);
		spin_unlock(&new_table->lock);
		return ret;
	}

	ret = cs_fe_table_get_entry(FE_TABLE_CLASS, new_table->class_index,
			&new_class_entry);
	if (ret != 0) {
		spin_unlock(&prev_table->lock);
		spin_unlock(&new_table->lock);
		return ret;
	}

	memset(&fwdrslt_entry, 0, sizeof(fwdrslt_entry));
	memset(&voqpol_entry, 0, sizeof(voqpol_entry));
	voqpol_entry.voq_base = ROOT_PORT_VOQ_BASE;
	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voqpol_entry,
			&voqpol_idx);
	if (ret != 0) {
		spin_unlock(&prev_table->lock);
		spin_unlock(&new_table->lock);
		return ret;
	}

	fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;
	fwdrslt_entry.l2.mcgid_valid = 1;
	fwdrslt_entry.l2.mcgid = new_table->mcgid;
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwdrslt_entry,
			&fwdrslt_idx);
	if (ret != 0)
		goto FAIL_CLEAR_VOQPOL;

	/* now chaining software entity */
	prev_table->next = new_table;
	new_table->next = prev_table;
	if (next_table != NULL) {
		new_table->next = next_table;
		next_table->prev = new_table;
	}

	/*
	 * chaining vtable by
	 * 1) set the new vtable's sdb->[uu/um/bc]_flowidx to the
	 * 	prev vtable's sdb->[uu/um/bc]_flowidx.
	 * 2) set the new vtable's classifier to include the MCGIC
	 * 3) update the prev vtable's sdb->[uu/um/bc]_flowidx to the
	 * 	new fwdrslt that's just created.
	 */
	/* step#1 */
	new_sdb_entry.misc.uu_flowidx = prev_sdb_entry.misc.uu_flowidx;
	new_sdb_entry.misc.um_flowidx = prev_sdb_entry.misc.um_flowidx;
	new_sdb_entry.misc.bc_flowidx = prev_sdb_entry.misc.bc_flowidx;
	new_table->uuflow_idx = prev_sdb_entry.misc.uu_flowidx;
	new_table->umflow_idx = prev_sdb_entry.misc.um_flowidx;
	new_table->bcflow_idx = prev_sdb_entry.misc.bc_flowidx;
	ret |= cs_fe_table_set_entry(FE_TABLE_SDB, new_table->sdb_index,
			&new_sdb_entry);

	/* step#2 */
	new_class_entry.port.mcgid = new_table->mcgid;
	ret |= cs_fe_table_set_entry(FE_TABLE_CLASS, new_table->class_index,
			&new_class_entry);

	/* step#3 */
	prev_sdb_entry.misc.uu_flowidx = fwdrslt_idx;
	prev_sdb_entry.misc.um_flowidx = fwdrslt_idx;
	prev_sdb_entry.misc.bc_flowidx = fwdrslt_idx;
	ret = cs_fe_table_set_entry(FE_TABLE_SDB, prev_table->sdb_index,
			&prev_sdb_entry);
	if (ret != 0) {
		prev_table->uuflow_idx = 0xffff;
		prev_table->umflow_idx = 0xffff;
		prev_table->bcflow_idx = 0xffff;
		goto FAIL_CLEAR_FWDRSLT;
	}
	prev_table->uuflow_idx = fwdrslt_idx;
	prev_table->umflow_idx = fwdrslt_idx;
	prev_table->bcflow_idx = fwdrslt_idx;

	return 0;

FAIL_CLEAR_FWDRSLT:
	cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, fwdrslt_idx, false);

FAIL_CLEAR_VOQPOL:
	cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, voqpol_idx, false);

	spin_unlock(&prev_table->lock);
	spin_unlock(&new_table->lock);

	return ret;
} /* cs_vtable_insert_to_chain */

/* remove the given vtable from the chain.  It will make sure the rest of the
 * vtables in the chain still keep their linkage properly.
 * The simple logic here is swapping the default flow indices of curr and prev
 * vtables. */
int cs_vtable_remove_from_chain(cs_vtable_t *table)
{
	cs_vtable_t *prev_table;
	int ret;
	fe_sdb_entry_t prev_sdb_entry, curr_sdb_entry;
	unsigned int uuflow_idx, umflow_idx, bcflow_idx;

	if ((table == NULL) || (table->prev == NULL))
		return -1;

	prev_table = table->prev;

	spin_lock(&prev_table->lock);
	spin_lock(&table->lock);

	ret = cs_fe_table_get_entry(FE_TABLE_SDB, prev_table->sdb_index,
			&prev_sdb_entry);
	ret |= cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index,
			&curr_sdb_entry);
	if (ret != 0) {
		spin_unlock(&prev_table->lock);
		spin_unlock(&table->lock);
		return ret;
	}

	/* swapping all the default flow indices */
	uuflow_idx = prev_sdb_entry.misc.uu_flowidx;
	umflow_idx = prev_sdb_entry.misc.um_flowidx;
	bcflow_idx = prev_sdb_entry.misc.bc_flowidx;

	prev_table->uuflow_idx = table->uuflow_idx;
	prev_table->umflow_idx = table->umflow_idx;
	prev_table->bcflow_idx = table->bcflow_idx;
	prev_sdb_entry.misc.uu_flowidx = curr_sdb_entry.misc.uu_flowidx;
	prev_sdb_entry.misc.um_flowidx = curr_sdb_entry.misc.um_flowidx;
	prev_sdb_entry.misc.bc_flowidx = curr_sdb_entry.misc.bc_flowidx;

	table->uuflow_idx = uuflow_idx;
	table->umflow_idx = umflow_idx;
	table->bcflow_idx = bcflow_idx;
	curr_sdb_entry.misc.uu_flowidx = uuflow_idx;
	curr_sdb_entry.misc.um_flowidx = umflow_idx;
	curr_sdb_entry.misc.bc_flowidx = bcflow_idx;

	/* fix the link */
	prev_table = table->next;
	if (table->next != NULL)
		table->next->prev = prev_table;
	table->next = table->prev = NULL;

	/* done swapping, now write it to FE table */
	ret = cs_fe_table_set_entry(FE_TABLE_SDB, table->sdb_index,
			&curr_sdb_entry);
	ret |= cs_fe_table_set_entry(FE_TABLE_SDB, prev_table->sdb_index,
			&prev_sdb_entry);

	spin_unlock(&prev_table->lock);
	spin_unlock(&table->lock);

	return ret;
} /* cs_vtable_remove_from_chain */

/* combine hash mask from src_table to dst_table.
 * Note: this API does not take care of duplicate hash mask indices
 * when merging. */
int cs_vtable_combine_vtable_hashmask(cs_vtable_t *dst_table,
		cs_vtable_t *src_table)
{
	fe_sdb_entry_t dst_sdb_entry, src_sdb_entry;
	unsigned int dst_fwd_cnt = 0, src_fwd_cnt = 0;
	unsigned int dst_qos_cnt = 0, src_qos_cnt = 0;
	unsigned int iii, jjj, last_jjj;
	int ret;

	if ((dst_table == NULL) || (src_table == NULL))
		return -1;

	spin_lock(&dst_table->lock);
	spin_lock(&src_table->lock);

	ret = cs_fe_table_get_entry(FE_TABLE_SDB, dst_table->sdb_index,
			&dst_sdb_entry);
	ret |= cs_fe_table_get_entry(FE_TABLE_SDB, src_table->sdb_index,
			&src_sdb_entry);
	if (ret != 0) {
		spin_unlock(&dst_table->lock);
		spin_unlock(&src_table->lock);
		return ret;
	}

	/* first count numbers of used hash mask tuple in both dst_table
	 * and src_table for both fwd and qos type. */
	for (iii = 0; iii <= 5; iii++) {
		if (dst_sdb_entry.sdb_tuple[iii].enable == 1)
			dst_fwd_cnt++;
		if (src_sdb_entry.sdb_tuple[iii].enable == 1)
			src_fwd_cnt++;
	}

	for (iii = 6; iii <= 7; iii++) {
		if (dst_sdb_entry.sdb_tuple[iii].enable == 1)
			dst_qos_cnt++;
		if (src_sdb_entry.sdb_tuple[iii].enable == 1)
			src_qos_cnt++;
	}

	if ((src_fwd_cnt > (6 - dst_fwd_cnt)) ||
			(src_qos_cnt > (6 - dst_qos_cnt))) {
		/* there isn't enough space */
		spin_unlock(&dst_table->lock);
		spin_unlock(&src_table->lock);
		return -1;
	}

	/* merge fwd hash mask tuple */
	last_jjj = 0;
	for (iii = 0; iii <= 5; iii++) {
		if (src_sdb_entry.sdb_tuple[iii].enable == 1) {
			for (jjj = last_jjj; jjj <= 5; jjj++) {
				if (dst_sdb_entry.sdb_tuple[jjj].enable == 0) {
					dst_sdb_entry.sdb_tuple[jjj].enable = 1;
					dst_sdb_entry.sdb_tuple[jjj].mask_ptr =
						src_sdb_entry.sdb_tuple[iii].
						mask_ptr;
					dst_sdb_entry.sdb_tuple[jjj].priority =
						src_sdb_entry.sdb_tuple[iii].
						priority;
					last_jjj = jjj + 1;
				}
			}
		}
	}

	/* merge qos hash mask tuple */
	last_jjj = 6;
	for (iii = 6; iii <= 7; iii++) {
		if (src_sdb_entry.sdb_tuple[iii].enable == 1) {
			for (jjj = last_jjj; jjj <= 7; jjj++) {
				if (dst_sdb_entry.sdb_tuple[jjj].enable == 0) {
					dst_sdb_entry.sdb_tuple[jjj].enable = 1;
					dst_sdb_entry.sdb_tuple[jjj].mask_ptr =
						src_sdb_entry.sdb_tuple[iii].
						mask_ptr;
					dst_sdb_entry.sdb_tuple[jjj].priority =
						src_sdb_entry.sdb_tuple[iii].
						priority;
					last_jjj = jjj + 1;
				}
			}
		}
	}

	spin_unlock(&dst_table->lock);
	spin_unlock(&src_table->lock);
	return 0;
} /* cs_vtable_combine_vtable_hashmask */

/* return true, if there is still available space for inserting hashmask */
bool cs_vtable_has_avail_hashmask_space(cs_vtable_t *table, bool is_qos)
{
	unsigned int start_idx, end_idx, iii;
	fe_sdb_entry_t sdb_entry;
	int ret;

	if (table == NULL)
		return -1;

	if (is_qos == true) {
		start_idx = 6;
		end_idx = 7;
	} else {
		start_idx = 0;
		end_idx = 5;
	}

	spin_lock(&table->lock);
	ret = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index, &sdb_entry);
	if (ret != 0) {
		spin_unlock(&table->lock);
		return ret;
	}

	for (iii = start_idx; iii <= end_idx; iii++) {
		if (sdb_entry.sdb_tuple[iii].enable == 0) {
			spin_unlock(&table->lock);
			return true;
		}
	}

	spin_unlock(&table->lock);
	return false;
} /* cs_vtable_has_avail_hashmask_space */

/* adjust the end pointer of LPM in SDBs */
void cs_lpm_update_sdb_end_ptr(int is_ipv6, int end_ptr, int is_lpm_ptr_en)
{
        int i, ret;
        cs_vtable_t *p_table;
        fe_sdb_entry_t sdb_entry;
        cs_core_vtable_def_hashmask_info_t *vtable_info;

        /* printk("%s: is_ipv6=%d, end_ptr=%d, is_lpm_ptr_en=%d\n", __func__, is_ipv6, end_ptr, is_lpm_ptr_en); */

        for (i = CORE_VTABLE_TYPE_BCAST; i < CORE_VTABLE_TYPE_MAX; i++) {
                p_table = cs_core_vtable_get(i);
                if (p_table == NULL) {
                        printk("%s:%d:failed to get vtable for table type %d\n", __func__, __LINE__, i);
                        continue;
                }
                /* printk("%s: table type=%d, p_table->sdb_index=%d\n", __func__, i, p_table->sdb_index); */

                vtable_info = &vtable_def_hm_info[p_table->vtable_type];

                if (!(vtable_info->lpm_enable)) {
                        continue;
                }

                ret = cs_fe_table_get_entry(FE_TABLE_SDB, p_table->sdb_index, &sdb_entry);
                if (ret != 0) {
                        printk("%s:  cs_fe_table_get_entry(FE_TABLE_SDB, p_table->sdb_index=%d) failed!!\n",
                                __func__, p_table->sdb_index);
                        continue;
                }
                if (is_ipv6 == 0) {
                        sdb_entry.sdb_lpm_v4[0].lpm_ptr_en = is_lpm_ptr_en;
                        sdb_entry.sdb_lpm_v4[0].end_ptr    = end_ptr;
                        sdb_entry.sdb_lpm_v4[1].lpm_ptr_en = is_lpm_ptr_en;
                        sdb_entry.sdb_lpm_v4[1].end_ptr    = end_ptr;
                }
                else {
                        sdb_entry.sdb_lpm_v6[0].lpm_ptr_en = is_lpm_ptr_en;
                       	sdb_entry.sdb_lpm_v6[0].end_ptr    = end_ptr / 4;
                        sdb_entry.sdb_lpm_v6[1].lpm_ptr_en = is_lpm_ptr_en;
                        sdb_entry.sdb_lpm_v6[1].end_ptr    = end_ptr / 4;
                }
	
                ret = cs_fe_table_set_entry(FE_TABLE_SDB, p_table->sdb_index, &sdb_entry);
                if (ret != 0) {
                        printk("%s: cs_fe_table_set_entry(FE_TABLE_SDB, p_table->sdb_index=%d) failed!!\n",
  				 __func__, p_table->sdb_index);
                }

        }
}

