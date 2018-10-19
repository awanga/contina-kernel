/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <wen.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CS_VTABLE_H__
#define __CS_VTABLE_H__

#include <mach/cs75xx_fe_core_table.h>
#include <linux/spinlock.h>

/*
 * One vtable maps to one type of application oriented classification. In
 * each of vtable, there could be multiple hash masks inserted in one SDB
 * for different applications.  However, one rule application type might
 * happen in different vtables.
 */
typedef struct cs_vtable_s {
	/* next and prev are another vtables of same application type that
	 * are adjacent to this vtable.  They have their own classifiers,
	 * SDBs, and etc. */
	struct cs_vtable_s	*next, *prev;

	/* vtable_type indicates what kind of application this vtable is
	 * about: bcast, mcast, L2 forwarding, L3 forwarding etc. */
	unsigned int	vtable_type;

	unsigned int	class_index;
	unsigned int	sdb_index;

	/* mcgid of this vtable, use to link vtables of the same chain. */
	unsigned int	mcgid;

	/* default flow index to forwarding result table */
	unsigned int	uuflow_idx;
	unsigned int	umflow_idx;
	unsigned int	bcflow_idx;

	spinlock_t	lock;
} cs_vtable_t;

#if 0


typedef enum {
	CS_VTABLE_DEF_ACT_GE0,
	CS_VTABLE_DEF_ACT_GE1,
	CS_VTABLE_DEF_ACT_GE2,
	CS_VTABLE_DEF_ACT_RE0,
	CS_VTABLE_DEF_ACT_RE1,
	CS_VTABLE_DEF_ACT_ROOT,
	CS_VTABLE_DEF_ACT_CPU0,
	CS_VTABLE_DEF_ACT_CPU1,
	CS_VTABLE_DEF_ACT_CPU2,
	CS_VTABLE_DEF_ACT_CPU3,
	CS_VTABLE_DEF_ACT_CPU4,
	CS_VTABLE_DEF_ACT_CPU5,
	CS_VTABLE_DEF_ACT_CPU6,
	CS_VTABLE_DEF_ACT_CPU7,
	CS_VTABLE_DEF_ACT_DROP,

	CS_VTABLE_DEF_ACT_MAX,
} cs_vtable_default_act_e;
#endif
/* allocate a vtable by allocating its Classifier and SDB.  Set up Classifier
 * according to the given classifier.  Set up the default action according to
 * the given default action.
 * Input: 1) Pointer to classifier
 * 	  2) default action type
 * Return: Pointer to vtable if succeeds.  Null, if otherwise. */
cs_vtable_t *cs_vtable_alloc(fe_class_entry_t *p_class, unsigned int def_act, unsigned int vtbl_type);

/* free vtable. Need to release the Classifier, SDB, hash masks, and the
 * forwarding results used for default actions */
int cs_vtable_free(cs_vtable_t *table);

#define CS_VTABLE_DEF_ACT_TYPE_UU	0x01
#define CS_VTABLE_DEF_ACT_TYPE_UM	0x02
#define CS_VTABLE_DEF_ACT_TYPE_BC	0x04
/* set up default action to the specific default action type */
int cs_vtable_set_def_action(cs_vtable_t *table, u8 def_act_type_mask,
		unsigned int def_act);

/* add given hashmask to the vtable with the given priority.
 * Return the hashmask index if succeed. Else otherwise. */
int cs_vtable_add_hashmask(cs_vtable_t *table, fe_hash_mask_entry_t *hm_ptr,
		unsigned int priority, bool is_qos);

/* find the matching hashmask from given vtable and delete it.
 * return 0 if succeeds.  Else otherwise */
int cs_vtable_del_hashmask(cs_vtable_t *table, fe_hash_mask_entry_t *hm_ptr,
		bool is_qos);

/* delete the hash mask with hm_idx from vtable.
 * return 0 if succeeds, else otherwise. */
int cs_vtable_del_hashmask_by_idx(cs_vtable_t *table, u32 hm_idx,
		bool is_qos);

/* delete all the hash mask associated with this vtable. */
int cs_vtable_del_hashmask_all(cs_vtable_t *table);

/* find the hashmask index that matches the given hash mask entry in vtable */
int cs_vtable_get_hashmask_idx(cs_vtable_t *table,
		fe_hash_mask_entry_t *hm_ptr, bool is_qos);

/* insert vtable to the chain by given the pointer to the new vtable, and the
 * previous table. It will insert the new vtable between previous vtable and
 * the next table of previous vtable */
int cs_vtable_insert_to_chain(cs_vtable_t *new_table, cs_vtable_t *prev_table);

/* remove the given vtable from the chain.  It will make sure the rest of the
 * vtables in the chain still keep their linkage properly. */
int cs_vtable_remove_from_chain(cs_vtable_t *table);

/* combine hash mask from src_table to dst_table */
int cs_vtable_combine_vtable_hashmask(cs_vtable_t *dst_table,
		cs_vtable_t *src_table);

/* return true, if there is still available space for inserting hashmask */
bool cs_vtable_has_avail_hashmask_space(cs_vtable_t *table, bool is_qos);

#endif /* __CS_VTABLE_H__ */
