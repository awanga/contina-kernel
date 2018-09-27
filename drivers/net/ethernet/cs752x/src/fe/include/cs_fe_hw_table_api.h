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
/*
 * cs_fe_hw_table_api.h
 *
 * $Id: cs_fe_hw_table_api.h,v 1.2 2011/05/14 02:49:36 whsu Exp $
 *
 * It contains API defintions for hardware Table Management.
 */

#ifndef __CS_FE_HW_TABLE_API_H__
#define __CS_FE_HW_TABLE_API_H__

#include "linux/types.h"
#include "cs_fe_head_table.h"

#define TABLE_TRY_TIMEOUT	1000

int cs_fe_hw_table_init(void);

/* Generic table management APIs to control the HW FE tables */
int cs_fe_hw_table_set_field_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int field, __u32 *p_value);
int cs_fe_hw_table_clear_entry(cs_fe_hw_table_e table_type,
		unsigned int idx);
int cs_fe_hw_table_get_field_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int field, __u32 *p_value);
int cs_fe_hw_table_flush_table(cs_fe_hw_table_e table_type);

int cs_fe_hw_table_set_field_value_to_sw_data(cs_fe_hw_table_e table_type,
		unsigned int field, __u32 *p_value, __u32 *p_data_array,
		unsigned int array_size);

int cs_fe_hw_table_get_entry_value(cs_fe_hw_table_e table_type,
		unsigned int idx, __u32 *p_data_array,
		unsigned int *p_data_cnt);
int cs_fe_hw_table_set_entry_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int data_cnt, __u32 *p_data_array);

/* Specific table management APIs for FE L3 Result Table */
int cs_fe_hw_table_set_rslt_l3_ipv4(unsigned int idx, __u32 ip_addr,
		bool parity);
int cs_fe_hw_table_set_rslt_l3_ipv6(unsigned int idx, __u32 *p_ip_addr,
		bool parity);
int cs_fe_hw_table_get_rslt_l3_ipv4(unsigned int idx, __u32 *p_ip_addr,
		bool *p_parity);
int cs_fe_hw_table_get_rslt_l3_ipv6(unsigned int idx, __u32 *p_ip_addr,
		bool *p_parity);
/* share clear_entry and flush_table with the generic APIs, but idx needs to be
 * right-shifted by 2. */

#endif /* __CS_FE_HW_TABLE_API_H__ */

