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

#ifndef __CS_FE_TABLE_INT_H__
#define __CS_FE_TABLE_INT_H__

#include <linux/spinlock.h>
#include "cs_table.h"

typedef struct cs_fe_table_op_s {
	/*
	 * convert SW entry value to HW value that can be used on writing to
	 * the HW register.
	 */
	int (*convert_sw_to_hw_data) (void *sw_entry, __u32 *p_data_array,
		unsigned int size);

	/*
	 * Get an available empty index from the start_offset.  In the
	 * implementation, one has to make sure this specific entry is reserved,
	 * so the next caller will not obtain this entry until it's free by any
	 * of the deleting API.
	 */
	int (*alloc_entry) (unsigned int *rslt_idx, unsigned int start_offset);
	/* Set the entry value to a given index in the table.  Must have
	 * valid OP assigned to (*convert_sw_to_hw_data) */
	int (*set_entry) (unsigned int idx, void *entry);

	/*
	 * the combined add API that will first find if a previous entry has
	 * been added to the table. If so, it will just increment the user count
	 * of the matching entry and return this index.  If not, it finds the
	 * first available index, set the entry value and increment the user
	 * count.  The implementation of this function should be an optimized
	 * function that combines (*find_entry), (*inc_entry_refcnt),
	 * (*alloc_entry), and (*set_entry).
	 */
	int (*add_entry) (void *entry, unsigned int *rslt_idx);

	/* delete the entry by index. It checks the user count. */
	int (*del_entry_by_idx) (unsigned int idx, bool f_force);
	/*
	 * find the first occurence of entry that matches the given one and
	 * delete it.
	 */
	int (*del_entry) (void *entry, bool f_force);

	/*
	 * find the first occurence of entry that matches the given one and
	 * return the index that points to it.
	 */
	int (*find_entry) (void *entry, unsigned int *rslt_idx,
		unsigned int start_offset);
	/*
	 * get the content of the entry of the given index and write it to
	 * the memory given by the user.
	 */
	int (*get_entry) (unsigned int idx, void *entry);

	/* Increment the user reference count on a given entry */
	int (*inc_entry_refcnt) (unsigned int idx);
	/* Decrement the user reference count on a given entry */
	int (*dec_entry_refcnt) (unsigned int idx);
	/* Get the user reference count on a given entry */
	int (*get_entry_refcnt) (unsigned int idx, unsigned int *p_cnt);

	/* Set the value to the field of a specific entry by the given index */
	int (*set_field) (unsigned int idx, unsigned int field, __u32 *p_value);
	/* Get the value to the field of a specific entry by the given index */
	int (*get_field) (unsigned int idx, unsigned int field, __u32 *p_value);

	/* flush the whole table */
	int (*flush_table) (void);
	/* Get the remaining available entry count of this table */
	int (*get_avail_count) (void);

	/* print the entry with the given index */
	void (*print_entry) (unsigned int idx);
	/* print the entry with a given range of index */
	void (*print_range) (unsigned int start_idx, unsigned int end_idx);
	/* print the whole table */
	void (*print_table) (void);

	/* L2 result table specific */
	/*
	 * add_l2_mac is a dumb adding mechanism. It will not check whether
	 * a matching entry existed or not. It will just add.  Management
	 * layer should be able to fully utilize it with faster search.
	 * If only adding MAC_SA, then give it NULL or MAC_DA.  If adding
	 * both MAC_SA and MAC_DA, both pointers cannot be NULL. It will
	 * return the index if it succeeds to add the entry.
	 */
	int (*add_l2_mac) (unsigned char *p_sa, unsigned char *p_da,
			unsigned int *p_idx);
	/*
	 * del_l2_mac will first decrement the refcnt, if it's 0, it will
	 * then delete the entry. Use f_sa and f_da to determine which
	 * MAC that the user wants to control.
	 */
	int (*del_l2_mac) (unsigned int idx, bool f_sa, bool f_da);
	/*
	 * find the index of a matching L2 MAC SA and/or DA address. Giving NULL
	 * in p_sa and/or p_da meaning the user is not matching the entity.
	 */
	int (*find_l2_mac) (unsigned char *p_sa, unsigned char *p_da,
			unsigned int *p_idx);
	/*
	 * to get the MAC SA and/or DA if given storage. Giving NULL in p_sa
	 * and/or p_da meaning the user is not obtaining those values.
	 */
	int (*get_l2_mac) (unsigned int idx, unsigned char *p_sa,
			unsigned char *p_da);
	/* increment the refcnt for a MAC_SA and/or MAC_DA with given index */
	int (*inc_l2_mac_refcnt) (unsigned int idx, bool f_sa, bool f_da);


	/* L3 result table specific */
	/*
	 * add_l3_ip is a dumb adding mechanism.  It will not check whether a
	 * matching entry existed or not.  It will just find an available spot
	 * and place the given IP address.
	 */
	int (*add_l3_ip) (__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6);
	/* this will delete the IP address with the given index. */
	int (*del_l3_ip) (unsigned int idx, bool f_is_v6);
	/* find the index of an entry with matching IP address */
	int (*find_l3_ip) (__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6);
	/* get the IP address store in the entry with given index */
	int (*get_l3_ip) (unsigned int idx, __u32 *p_ip_addr, bool f_is_v6);
	/* increment the refcnt of IP address with given index */
	int (*inc_l3_ip_refcnt) (unsigned int idx, bool f_is_v6);

	/* PE VOQDRP table specifc */
	/* enable VOQ drop on a VOQ with given VOQ index */
	int (*enbl_voqdrp) (unsigned int voq_idx);
	/* disable VOQ drop on a VOQ with given VOQ index */
	int (*dsbl_voqdrp) (unsigned int voq_idx);
	/* Get the VOQ drop status on a VOQ with given VOQ index */
	int (*get_voqdrp) (unsigned int voq_idx, __u8 *p_enbl);

	/* FIXME! LPM table specific */
} cs_fe_table_op_t;

typedef struct cs_fe_table_s {
	cs_fe_hw_table_e type_id;
	unsigned int max_entry;
	unsigned int used_entry;
	unsigned int curr_ptr;
	unsigned int entry_size;
	const cs_fe_table_op_t op;
	spinlock_t lock;
	cs_table_t *content_table;
} cs_fe_table_t;

/* Table manager that controls the registration of each table */
int cs_fe_table_register(cs_fe_hw_table_e type_id, cs_fe_table_t *fe_tbl);
int cs_fe_table_unregister(unsigned int type_id);
bool cs_fe_table_is_registered(cs_fe_hw_table_e type_id);
cs_fe_table_t *cs_fe_table_get(cs_fe_hw_table_e type_id);

#endif /* __CS_FE_TABLE_INT_H__ */
