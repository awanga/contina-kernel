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

#ifndef __CS_FE_TABLE_EXT_H__
#define __CS_FE_TABLE_EXT_H__

#include <linux/errno.h>
#include "cs_fe_head_table.h"

/* FE Return Code */
#define FE_TABLE_OK			(0)
#define FE_TABLE_ENTRYNOTFOUND		(1)

/* Error code.. will need to revisit this */
#define FE_TABLE_ETBLNOTEXIST		(-2)	/* Table does not exist */
#define FE_TABLE_EOPNOTSUPP		(-3)	/* Operation is not supported */
#define FE_TABLE_ENULLPTR		(-4)	/* Accessing a NULL pointer */
#define FE_TABLE_ETBLFULL		(-5)	/* Table is full */
#define FE_TABLE_EOUTRANGE		(-6)	/* Out of range */
#define FE_TABLE_EENTRYNOTRSVD		(-7)	/* Entry is not reserved */
#define FE_TABLE_ENOMEM			(-ENOMEM)	/* no memory */
#define FE_TABLE_EFIELDNOTSUPP		(-8)
#define FE_TABLE_EDUPLICATE		(-9)
#define FE_TABLE_EACCESSINCOMP		(-10)
// FIXME!! more error code..

#define FE_TABLE_ENTRY_USED		(0xF0000000)

/* real APIs for table users */
/* Generic APIs that are usually applied for most of the tables. */
int cs_fe_table_alloc_entry(cs_fe_hw_table_e type_id, unsigned int *rslt_idx,
		unsigned int start_offset);
int cs_fe_table_set_entry(cs_fe_hw_table_e type_id, unsigned int idx,
		void *entry);
int cs_fe_table_add_entry(cs_fe_hw_table_e type_id, void *entry,
		unsigned int *rslt_idx);
int cs_fe_table_del_entry_by_idx(cs_fe_hw_table_e type_id, unsigned int idx,
		bool f_force);
int cs_fe_table_del_entry(cs_fe_hw_table_e type_id, void *entry, bool f_force);
int cs_fe_table_find_entry(cs_fe_hw_table_e type_id, void *entry,
		unsigned int *rslt_idx, unsigned int start_offset);
int cs_fe_table_get_entry(cs_fe_hw_table_e type_id, unsigned int idx,
		void *entry);
int cs_fe_table_inc_entry_refcnt(cs_fe_hw_table_e type_id, unsigned int idx);
int cs_fe_table_dec_entry_refcnt(cs_fe_hw_table_e type_id, unsigned int idx);
int cs_fe_table_get_entry_refcnt(cs_fe_hw_table_e type_id, unsigned int idx,
		unsigned int *p_cnt);
int cs_fe_table_set_field(cs_fe_hw_table_e type_id, unsigned int idx,
		unsigned int field, __u32 *p_value);
int cs_fe_table_get_field(cs_fe_hw_table_e type_id, unsigned int idx,
		unsigned int field, __u32 *p_value);
int cs_fe_table_flush_table(cs_fe_hw_table_e type_id);
int cs_fe_table_get_avail_count(cs_fe_hw_table_e type_id);
int cs_fe_table_print_entry(cs_fe_hw_table_e type_id, unsigned int idx);
int cs_fe_table_print_range(cs_fe_hw_table_e type_id, unsigned int start_idx,
		unsigned int end_idx);
int cs_fe_table_print_table(cs_fe_hw_table_e type_id);
int cs_fe_table_print_table_used_count(cs_fe_hw_table_e type_id);

/* L2 result table specific */
int cs_fe_table_add_l2_mac(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx);
int cs_fe_table_del_l2_mac(unsigned int idx, bool f_sa, bool f_da);
int cs_fe_table_find_l2_mac(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx);
int cs_fe_table_get_l2_mac(unsigned int idx, unsigned char *p_sa,
		unsigned char *p_da);
int cs_fe_table_inc_l2_mac_refcnt(unsigned int idx, bool f_sa, bool f_da);

/* L3 result table specific */
int cs_fe_table_add_l3_ip(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6);
int cs_fe_table_del_l3_ip(unsigned int idx, bool f_is_v6);
int cs_fe_table_find_l3_ip(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6);
int cs_fe_table_get_l3_ip(unsigned int idx, __u32 *p_ip_addr, bool f_is_v6);
int cs_fe_table_inc_l3_ip_refcnt(unsigned int idx, bool f_is_v6);

/* PE VOQDRP table specific */
int cs_fe_table_enbl_voqdrp(unsigned int voq_idx);
int cs_fe_table_dsbl_voqdrp(unsigned int voq_idx);
int cs_fe_table_get_voqdrp(unsigned int voq_idx, __u8 *p_enbl);

int cs_fe_table_init(void);
void cs_fe_table_exit(void);

#endif /* __CS_FE_TABLE_EXT_H__ */
