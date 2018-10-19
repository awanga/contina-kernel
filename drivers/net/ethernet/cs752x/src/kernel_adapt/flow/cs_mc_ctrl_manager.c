/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
//#include <linux/export.h>
#include <mach/cs_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_fe_table_api.h>
#include <cs_fe_head_table.h>
#include <cs_core_logic_data.h>
#include <cs_core_vtable.h>
#include <cs_fe_util_api.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include "cs_fe.h"
#include "cs_core_rule_hmu.h"
#include "cs_core_hmu.h"
#include <mach/cs_rule_hash_api.h>

#define CS_MAX_MC_CLIENT 15
#define CS_MAX_MC_ENTRY 128

int ni_special_start_xmit_none_bypass_ne(u16 recirc_idx, u32 buf0, int len0, u32 buf1, int len1, struct sk_buff *skb);

typedef struct cs_mc_ctrl_entry_s {
	cs_uint8_t	used;
	cs_uint8_t	mc_ref;
	cs_ip_address_t	mc_ip;
	cs_uint32_t	l7_4_bytes;
	void * data;
} cs_mc_ctrl_entry_t;


typedef struct cs_mc_sw_ip_entry_s {
	cs_uint8_t	ref;
	cs_ip_address_t	mc_ip;
	cs_uint32_t hash_idx;
} cs_mc_sw_ip_entry_t;

typedef struct cs_mc_client_entry_s{
	cs_uint8_t      client_used;
	cs_uint8_t		client_ref;
	cs_uint8_t		client_mac[CS_ETH_ADDR_LEN];
} cs_mc_client_entry_t;

cs_mc_ctrl_entry_t cs_mc_ctrl_entry[CS_MAX_MC_ENTRY];
cs_mc_sw_ip_entry_t cs_mc_sw_ip_entry[CS_MAX_MC_ENTRY];
cs_mc_client_entry_t cs_mc_client_entry[CS_MAX_MC_CLIENT];

void cs_mc_ctrl_handle(struct sk_buff *skb) {
	/* Parse UDP first bytes */
	struct ethhdr *eth;
	char *tmp;
	eth = skb->data;
	if (eth->h_proto == 0x0008) {
		tmp = (char*)eth + 14 + 20 + 8;
	} else {
		tmp = (char*)eth + 18 + 20 + 8;
	}
	void * data_ptr = dma_map_single(NULL, (void *)skb->data,
			SMP_CACHE_BYTES,
			DMA_TO_DEVICE);
	ni_special_start_xmit_none_bypass_ne(*tmp, data_ptr, skb->len, NULL, 0, skb);
}

void cs_mc_ctrl_manager_init(void)
{
	memset(0, cs_mc_ctrl_entry, sizeof(cs_mc_ctrl_entry_t) * CS_MAX_MC_ENTRY);
}

cs_int16_t cs_mc_ctrl_manager_get_sw_ip_idx(cs_ip_address_t group_ip)
{
	int i;
	for (i = 0; i < CS_MAX_MC_ENTRY; i++) {
		if (cs_mc_sw_ip_entry[i].ref != 0) {
			if (memcmp(&cs_mc_sw_ip_entry[i].mc_ip, &group_ip, sizeof(cs_ip_address_t)) == 0) {
				return i;
			}
		}
	}
	return -1;
}

cs_int16_t cs_mc_ctrl_manager_allocate_sw_ip_idx(cs_ip_address_t group_ip)
{
	int i;
	/* allocate a new entry*/
	for (i = 0; i < CS_MAX_MC_ENTRY; i++) {
		if (cs_mc_sw_ip_entry[i].ref == 0) {
			cs_mc_sw_ip_entry[i].mc_ip = group_ip;
			cs_mc_sw_ip_entry[i].ref = 1;
			return i;
		}
	}
	return -1;
}

cs_int16_t cs_mc_ctrl_manager_set_hash_idx(cs_int8_t group_idx, cs_int16_t hash_idx)
{
	int i;
	cs_mc_sw_ip_entry_t * entry;
	if (group_idx >=CS_MAX_MC_ENTRY)
		return -1;

	entry = &cs_mc_sw_ip_entry[group_idx];
	if (entry->ref == 0)
		return -1;
	entry->hash_idx = hash_idx;
	return 0;
}

cs_uint16_t cs_mc_ctrl_manager_get_hash_idx(cs_int8_t group_idx)
{
	cs_mc_sw_ip_entry_t * entry;
	if (group_idx >=CS_MAX_MC_ENTRY)
		return 0;

	entry = &cs_mc_sw_ip_entry[group_idx];
	if (entry->ref == 0)
		return 0;
	return entry->hash_idx;
}

cs_int16_t cs_mc_ctrl_manager_remove_reference(cs_int8_t group_idx)
{
	int i;
	cs_mc_sw_ip_entry_t * entry;
	if (group_idx >=CS_MAX_MC_ENTRY)
		return -1;

	entry = &cs_mc_sw_ip_entry[group_idx];
	if (entry->ref == 0)
		return -1;
	entry->ref--;
	return 0;
}

cs_int16_t cs_mc_ctrl_manager_increase_reference(cs_int8_t group_idx)
{
	int i;
	cs_mc_sw_ip_entry_t * entry;
	if (group_idx >=CS_MAX_MC_ENTRY)
		return -1;

	entry = &cs_mc_sw_ip_entry[group_idx];
	if (entry->ref == 0)
		return -1;
	entry->ref++;
	return 0;
}

cs_int16_t cs_mc_ctrl_manager_get_reference(cs_ip_address_t group_ip)
{
	int i;
	for (i = 0; i < CS_MAX_MC_ENTRY; i++) {
		if (cs_mc_sw_ip_entry[i].ref != 0) {
			if (memcmp(&cs_mc_sw_ip_entry[i].mc_ip, &group_ip, sizeof(cs_ip_address_t)) == 0) {
				return cs_mc_sw_ip_entry[i].ref;
			}
		}
	}
	return -1;
}

cs_int16_t cs_mc_ctrl_manager_get_group_idx(cs_ip_address_t group_ip, cs_uint32_t l7_4_bytes)
{
	int i;
	for (i = 0; i < CS_MAX_MC_ENTRY; i++) {
		if (cs_mc_ctrl_entry[i].used == 1) {
			if ((memcmp(&cs_mc_ctrl_entry[i].mc_ip, &group_ip, sizeof(cs_ip_address_t)) == 0) &&
				(cs_mc_ctrl_entry[i].l7_4_bytes == l7_4_bytes)) {
				cs_mc_ctrl_entry[i].mc_ref++;
				return i;
			}
		}
	}

	/* allocate a new entry*/
	for (i = 0; i < CS_MAX_MC_ENTRY; i++) {
		if (cs_mc_ctrl_entry[i].used == 0) {
			cs_mc_ctrl_entry[i].mc_ip = group_ip;
			cs_mc_ctrl_entry[i].l7_4_bytes = l7_4_bytes;
			cs_mc_ctrl_entry[i].used = 1;
			cs_mc_ctrl_entry[i].mc_ref = 1;
			return i;
		}
	}
	return -1;
}

cs_int16_t cs_mc_ctrl_manager_get_client_idx(cs_uint8_t * client_mac)
{
	int i;

	for (i = 0; i < CS_MAX_MC_CLIENT; i++) {
		if (cs_mc_client_entry[i].client_used == 1) {
			if (memcmp(&cs_mc_client_entry[i].client_mac,  client_mac, CS_ETH_ADDR_LEN) == 0) {
				cs_mc_client_entry[i].client_ref++;
				return i;
			}
		}
	}

	/* allocate a new entry*/
	for (i = 0; i < CS_MAX_MC_CLIENT; i++) {
		if (cs_mc_client_entry[i].client_used == 0) {
			memcpy(&cs_mc_client_entry[i].client_mac[0], &client_mac[0], CS_ETH_ADDR_LEN);
			cs_mc_client_entry[i].client_used = 1;
			cs_mc_client_entry[i].client_ref = 1;
			return i;
		}
	}
	return -1;
}

cs_int16_t cs_mc_ctrl_manager_del_by_group_idx(cs_int8_t group_idx)
{
	int i;
	cs_mc_ctrl_entry_t * mc_ctrl_entry;
	if (group_idx >=CS_MAX_MC_ENTRY)
		return -1;

	mc_ctrl_entry = &cs_mc_ctrl_entry[group_idx];

	if (mc_ctrl_entry->used == 0)
		return -1;

	if (mc_ctrl_entry->mc_ref == 0) {
			printk("%s why mc ref count is 0??? \n", __func__);
			return -1;
	}

	mc_ctrl_entry->mc_ref--;
	if (mc_ctrl_entry->mc_ref == 0)
		mc_ctrl_entry->used = 0;

	return 0;
}

cs_int16_t cs_mc_ctrl_manager_del_client_by_idx(cs_int8_t client_idx)
{
	if (client_idx >= CS_MAX_MC_CLIENT)
		return -1;
	if (cs_mc_client_entry[client_idx].client_ref == 0) {
		printk("%s why client ref count is 0??? \n", __func__);
		return -1;
	}
	cs_mc_client_entry[client_idx].client_ref--;
	if (cs_mc_client_entry[client_idx].client_ref == 0)
		cs_mc_client_entry[client_idx].client_used = 0;
	return 0;
}

