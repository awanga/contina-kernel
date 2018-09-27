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
 * cs_hw_accel_ip_translate.c
 *
 * $Id$
 *
 * This file contains the implementation for CS IP Translate
 * Acceleration.
 */

#include "cs_core_logic_data.h"
#include "cs_hw_accel_ip_translate.h"
#include "cs_hw_accel_tunnel.h"

#include <mach/cs_network_types.h>
//#include <mach/cs75xx_fe_core_table.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#include "cs_mut.h"

#ifdef CS_IPC_ENABLED
#include <mach/cs_vpn_tunnel_ipc.h>
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#endif /* CONFIG_CS752X_PROC */

/* TODO:  add a RTP proxy cs_adapt_debug flag*/
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#define ERR(x)	(x)

#define IP_TRANSLATE_TUNNELS_ENTRY_MAX IP_TRANSLATE_TUNNELS_MAX
cs_ip_translate_tunnel_t * cs_ip_translate_entry[IP_TRANSLATE_TUNNELS_ENTRY_MAX];

typedef enum {
	CS_ENTRY_NOT_FOUND				= 1,
	CS_ENTRY_EGRESS_DUPLICATE		= 2,
	CS_ENTRY_INGRESS_DUPLICATE		= 3,
	CS_ENTRY_BOTH_DUPLICATE			= 4,
} cs_ip_entry_status_t;

/* for ipc message() */
wait_queue_head_t cs_ip_ts_wq;
int cs_ip_ts_wq_wakeup = 0;

spinlock_t cs_ip_ts_lock;

#define __CS_IP_TS_LOCK() spin_lock_bh(&cs_ip_ts_lock);
#define __CS_IP_TS_UNLOCK() spin_unlock_bh(&cs_ip_ts_lock);

/*global port set information*/
typedef struct cs_ip_translate_mape_rule_s {
	cs_uint16_t port_set_id;
	cs_uint16_t port_set_id_length;
	cs_uint16_t port_set_id_offset;
} cs_ip_translate_mape_rule_t;

cs_ip_translate_mape_rule_t	g_cs_ip_translate_mape_rule;

extern unsigned int calc_crc(u32 crc, u8 const *p, u32 len);

cs_ip_translate_flow_t * __cs_ip_translate_search_entry(
	CS_IN	cs_ip_translate_tunnel_t * tunnel,
	CS_IN	cs_flow_t	*p_flow_entry)
{
	cs_ip_translate_flow_t	*entry;
	entry = tunnel->napt_flow;
	while (entry != NULL) {
		if ((entry->flow_entry.ingress_pkt.da_ip.ip_addr.ipv4_addr == p_flow_entry->ingress_pkt.da_ip.ip_addr.ipv4_addr) &&
			(entry->flow_entry.ingress_pkt.sa_ip.ip_addr.ipv4_addr == p_flow_entry->ingress_pkt.sa_ip.ip_addr.ipv4_addr) &&
			(entry->flow_entry.ingress_pkt.protocol == p_flow_entry->ingress_pkt.protocol) &&
			(entry->flow_entry.ingress_pkt.l4_header.tcp.sport == p_flow_entry->ingress_pkt.l4_header.tcp.sport) &&
			(entry->flow_entry.ingress_pkt.l4_header.tcp.dport == p_flow_entry->ingress_pkt.l4_header.tcp.dport)) {
			/*
			 * check if user change the napt rule
			 */
			if ((entry->flow_entry.egress_pkt.da_ip.ip_addr.ipv4_addr != p_flow_entry->egress_pkt.da_ip.ip_addr.ipv4_addr) ||
				(entry->flow_entry.egress_pkt.sa_ip.ip_addr.ipv4_addr != p_flow_entry->egress_pkt.sa_ip.ip_addr.ipv4_addr) ||
				(entry->flow_entry.egress_pkt.protocol != p_flow_entry->egress_pkt.protocol) ||
				(entry->flow_entry.egress_pkt.l4_header.tcp.sport != p_flow_entry->egress_pkt.l4_header.tcp.sport) ||
				(entry->flow_entry.egress_pkt.l4_header.tcp.dport != p_flow_entry->egress_pkt.l4_header.tcp.dport)) {
					ERR(printk("%s:%d the setup conflict !! Please delete the old one and create new one!!\n",
						__func__, __LINE__));
			}
			return entry;
		}
		entry = entry->next;
	}

	return NULL;
}

cs_ip_entry_status_t __cs_ip_translate_search_tunnel(
	CS_IN	cs_ip_translate_cfg_t	*p_ip_translate_cfg,
	CS_OUT	cs_uint16	*p_entry_idx)
{
	cs_uint16 i;
	cs_ip_translate_tunnel_t * tunnel;
	for (i = 0 ; i < IP_TRANSLATE_TUNNELS_ENTRY_MAX; i++) {
		if (cs_ip_translate_entry[i] != NULL) {
			tunnel = cs_ip_translate_entry[i];
			if ((tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[0] == p_ip_translate_cfg->v6_pkt.da_ip.ip_addr.ipv6_addr[0]) &&
				(tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[1] == p_ip_translate_cfg->v6_pkt.da_ip.ip_addr.ipv6_addr[1]) &&
				(tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[2] == p_ip_translate_cfg->v6_pkt.da_ip.ip_addr.ipv6_addr[2]) &&
				(tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[3] == p_ip_translate_cfg->v6_pkt.da_ip.ip_addr.ipv6_addr[3]) &&
				(tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[0] == p_ip_translate_cfg->v6_pkt.sa_ip.ip_addr.ipv6_addr[0]) &&
				(tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[1] == p_ip_translate_cfg->v6_pkt.sa_ip.ip_addr.ipv6_addr[1]) &&
				(tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[2] == p_ip_translate_cfg->v6_pkt.sa_ip.ip_addr.ipv6_addr[2]) &&
				(tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[3] == p_ip_translate_cfg->v6_pkt.sa_ip.ip_addr.ipv6_addr[3])){
				*p_entry_idx = i;
				return CS_ENTRY_EGRESS_DUPLICATE;;
			}
		}
	}
	return CS_ENTRY_NOT_FOUND;
}


cs_status_t __cs_ip_translate_add_tunnel_hash(
	CS_IN	cs_uint16 dir,
	CS_IN	cs_uint16 sa_idx)
{
	int crc32;
	cs_status_t ret;
	cs_flow_t      *hash_flow;
	cs_ip_translate_tunnel_t *tunnel;

	if (sa_idx > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , sa_idx, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	tunnel = cs_ip_translate_entry[sa_idx];
	if (tunnel == NULL) {
		ERR(printk("%s:%d create hash for null entry [%d] direction %d \n",
			__func__, __LINE__ , sa_idx, dir));
		return CS_E_PARAM;
	}


	if (dir == CS_TUNNEL_DIR_OUTBOUND) {
		/* prepare PE->WAN flow entry information
		 * PE already construct IPv6 packet
		 * The hash will handle L2 converstion
		 */
		hash_flow = &tunnel->tunnel_entry_from_pe;
		memcpy(&hash_flow->ingress_pkt, &tunnel->ip_translate_cfg.v6_pkt, sizeof(cs_pkt_info_t));
		memcpy(&hash_flow->egress_pkt, &tunnel->ip_translate_cfg.v6_pkt, sizeof(cs_pkt_info_t));
		hash_flow->flow_type = CS_FLOW_TYPE_L4;
		hash_flow->dec_ttl = 0;
		hash_flow->ingress_pkt.sa_mac[1] = CS_FUNC_ID_MAPE & 0xFF;
		hash_flow->ingress_pkt.sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
		hash_flow->ingress_pkt.sa_mac[3] = tunnel->ip_translate_id & 0xFF;
		hash_flow->ingress_pkt.sa_mac[4] = 0;
		hash_flow->ingress_pkt.sa_mac[5] = tunnel->ip_translate_id >> 8;
		crc32 = ~(calc_crc(~0, (u8 const *) &hash_flow->ingress_pkt.sa_mac[1], 5));
		hash_flow->ingress_pkt.sa_mac[0] = crc32 & 0xFF;
		memcpy(&hash_flow->ingress_pkt.da_mac[0], &tunnel->ip_translate_cfg.v6_pkt.sa_mac[0], CS_ETH_ADDR_LEN);
		//after PE, it would be always no tag
		//memcpy(&hash_flow->ingress_pkt.tag[0], &entry->flow_entry.ingress_pkt.tag[0], sizeof(cs_vlan_t) * CS_VLAN_TAG_MAX);
		hash_flow->ingress_pkt.tag[0].tpid_encap_type = 0xffffffff;
		hash_flow->ingress_pkt.tag[1].tpid_encap_type = 0xffffffff;

		hash_flow->ingress_pkt.pppoe_session_id_valid = 0;
		hash_flow->ingress_pkt.pppoe_session_id = 0;
		ret = cs_flow_add(tunnel->device_id, hash_flow);
		if(ret != CS_OK) {
			ERR(printk("%s:%d Can't create tunnel hash(from PE) for IP Translate\n",
					__func__, __LINE__));
			return CS_E_ERROR;
		}
		return CS_E_OK;

	}else if (dir == CS_TUNNEL_DIR_INBOUND) {
		/*prepare WAN->PE flow entry information*/
		hash_flow = &tunnel->tunnel_entry_to_pe;
		memcpy(&hash_flow->ingress_pkt, &tunnel->ip_translate_cfg.v6_pkt, sizeof(cs_pkt_info_t));
		memcpy(&hash_flow->egress_pkt, &tunnel->ip_translate_cfg.v6_pkt, sizeof(cs_pkt_info_t));
		memcpy(&hash_flow->ingress_pkt.da_mac[0], &tunnel->ip_translate_cfg.v6_pkt.sa_mac[0], CS_ETH_ADDR_LEN);
		memcpy(&hash_flow->egress_pkt.da_mac[0], &tunnel->ip_translate_cfg.v6_pkt.sa_mac[0], CS_ETH_ADDR_LEN);

		memcpy(&hash_flow->ingress_pkt.sa_mac[0], &tunnel->ip_translate_cfg.v6_pkt.da_mac[0], CS_ETH_ADDR_LEN);

		memcpy(&hash_flow->ingress_pkt.da_ip.ip_addr.ipv6_addr[0],
				&tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[0], 16);
		memcpy(&hash_flow->ingress_pkt.sa_ip.ip_addr.ipv6_addr[0],
				&tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[0], 16);

		hash_flow->flow_type = CS_FLOW_TYPE_L4;
		hash_flow->dec_ttl = 0;
		hash_flow->egress_pkt.phy_port = CS_PORT_OFLD0;
		hash_flow->egress_pkt.sa_mac[1] = CS_FUNC_ID_MAPE & 0xFF;
		hash_flow->egress_pkt.sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
		hash_flow->egress_pkt.sa_mac[3] = tunnel->ip_translate_id & 0xFF;
		hash_flow->egress_pkt.sa_mac[4] = 1;
		hash_flow->egress_pkt.sa_mac[5] = tunnel->ip_translate_id >> 8;
		crc32 = ~(calc_crc(~0, &hash_flow->egress_pkt.sa_mac[1], 5));
		hash_flow->egress_pkt.sa_mac[0] = crc32 & 0xFF;

		/* To PE direction: Remove pppoe header if any */
		if (tunnel->ip_translate_cfg.v6_pkt.pppoe_session_id_valid != 0) {
			hash_flow->egress_pkt.pppoe_session_id_valid = 0;
			hash_flow->egress_pkt.pppoe_session_id = 0;
		}
		ret = cs_flow_add(tunnel->device_id, hash_flow);
		if(ret != CS_OK) {
			ERR(printk("%s:%d Can't create tunnel hash(from PE) for IP Translate\n",
					__func__, __LINE__));
			return CS_E_ERROR;
		}
		return CS_E_OK;
	}
	return CS_E_ERROR;
}

cs_status_t __cs_ip_translate_entry_add_hash(
	cs_ip_translate_tunnel_t *tunnel,
	cs_ip_translate_flow_t *entry)
{
	int crc32;
	cs_status_t ret;
	cs_flow_t	*hash_flow;

	/* prepare LAN->PE flow entry information
	 */
	hash_flow = &entry->flow_entry_to_pe;
	memcpy(&hash_flow->ingress_pkt, &entry->flow_entry.ingress_pkt, sizeof(cs_pkt_info_t));
	memcpy(&hash_flow->egress_pkt, &entry->flow_entry.egress_pkt, sizeof(cs_pkt_info_t));
	hash_flow->flow_type = CS_FLOW_TYPE_L4;
	hash_flow->dec_ttl = entry->flow_entry.dec_ttl;
	hash_flow->egress_pkt.phy_port = CS_PORT_OFLD1;
	hash_flow->life_time = 0;

	hash_flow->egress_pkt.sa_mac[1] = CS_FUNC_ID_MAPE & 0xFF;
	hash_flow->egress_pkt.sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	hash_flow->egress_pkt.sa_mac[3] = tunnel->ip_translate_id & 0xFF;
	hash_flow->egress_pkt.sa_mac[4] = 0;
	hash_flow->egress_pkt.sa_mac[5] = tunnel->ip_translate_id >> 8;
	crc32 = ~(calc_crc(~0, (u8 const *) &hash_flow->egress_pkt.sa_mac[1], 5));
	hash_flow->egress_pkt.sa_mac[0] = crc32 & 0xFF;

	ret = cs_flow_add(tunnel->device_id, hash_flow);
	if(ret != CS_OK) {
		ERR(printk("%s:%d Can't create flow hash(to PE) for IP Translate\n",
				__func__, __LINE__));
		return CS_E_ERROR;
	}

	/*prepare PE-> LAN flow entry information*/
	hash_flow = &entry->flow_entry_from_pe;
	memcpy(&hash_flow->ingress_pkt, &entry->flow_entry.egress_pkt, sizeof(cs_pkt_info_t));
	memcpy(&hash_flow->egress_pkt, &entry->flow_entry.ingress_pkt, sizeof(cs_pkt_info_t));
	memcpy(&hash_flow->ingress_pkt.da_mac[0], &tunnel->ip_translate_cfg.v6_pkt.sa_mac[0], CS_ETH_ADDR_LEN);
	memcpy(&hash_flow->egress_pkt.sa_mac[0], &entry->flow_entry.ingress_pkt.da_mac[0], CS_ETH_ADDR_LEN);
	memcpy(&hash_flow->egress_pkt.da_mac[0], &entry->flow_entry.ingress_pkt.sa_mac[0], CS_ETH_ADDR_LEN);

	hash_flow->ingress_pkt.sa_ip.ip_addr.ipv4_addr = entry->flow_entry.egress_pkt.da_ip.ip_addr.ipv4_addr;
	hash_flow->ingress_pkt.da_ip.ip_addr.ipv4_addr = entry->flow_entry.egress_pkt.sa_ip.ip_addr.ipv4_addr;
	hash_flow->ingress_pkt.l4_header.tcp.sport = entry->flow_entry.egress_pkt.l4_header.tcp.dport;
	hash_flow->ingress_pkt.l4_header.tcp.dport = entry->flow_entry.egress_pkt.l4_header.tcp.sport;

	hash_flow->egress_pkt.sa_ip.ip_addr.ipv4_addr = entry->flow_entry.ingress_pkt.da_ip.ip_addr.ipv4_addr;
	hash_flow->egress_pkt.da_ip.ip_addr.ipv4_addr = entry->flow_entry.ingress_pkt.sa_ip.ip_addr.ipv4_addr;
	hash_flow->egress_pkt.l4_header.tcp.sport = entry->flow_entry.ingress_pkt.l4_header.tcp.dport;
	hash_flow->egress_pkt.l4_header.tcp.dport = entry->flow_entry.ingress_pkt.l4_header.tcp.sport;

	hash_flow->flow_type = CS_FLOW_TYPE_L4;
	hash_flow->dec_ttl = entry->flow_entry.dec_ttl;
	hash_flow->ingress_pkt.phy_port = CS_PORT_OFLD0;
	hash_flow->life_time = 0;

	hash_flow->ingress_pkt.sa_mac[1] = CS_FUNC_ID_MAPE & 0xFF;
	hash_flow->ingress_pkt.sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	hash_flow->ingress_pkt.sa_mac[3] = tunnel->ip_translate_id & 0xFF;
	hash_flow->ingress_pkt.sa_mac[4] = 1;
	hash_flow->ingress_pkt.sa_mac[5] = tunnel->ip_translate_id >> 8;
	crc32 = ~(calc_crc(~0, &hash_flow->ingress_pkt.sa_mac[1], 5));
	hash_flow->ingress_pkt.sa_mac[0] = crc32 & 0xFF;

	ret = cs_flow_add(tunnel->device_id, hash_flow);
	if(ret != CS_OK) {
		cs_flow_delete(tunnel->device_id, entry->flow_entry_to_pe.flow_id);
		ERR(printk("%s:%d Can't create flow hash(from PE) for IP Translate\n",
				__func__, __LINE__));
		return CS_E_ERROR;
	}

	/*prepare PE-> LAN# CPU port flow entry for over-size packets*/
	hash_flow = &entry->over_size_entry_from_pe;
	memcpy(&hash_flow->ingress_pkt, &entry->flow_entry.egress_pkt, sizeof(cs_pkt_info_t));
	memcpy(&hash_flow->egress_pkt, &entry->flow_entry.ingress_pkt, sizeof(cs_pkt_info_t));

	hash_flow->egress_pkt.phy_port = entry->flow_entry.ingress_pkt.phy_port + 6;
	hash_flow->voq_offset = 7;
	hash_flow->life_time = 0;

	hash_flow->flow_type = CS_FLOW_TYPE_L4;
	hash_flow->dec_ttl = 0;
	memcpy(&hash_flow->ingress_pkt.sa_mac[0], &entry->flow_entry_to_pe.egress_pkt.sa_mac[0], CS_ETH_ADDR_LEN);

	ret = cs_flow_add(tunnel->device_id, hash_flow);
	if(ret != CS_OK) {
		cs_flow_delete(tunnel->device_id, entry->flow_entry_to_pe.flow_id);
		ERR(printk("%s:%d Can't create flow hash(from PE) for IP Translate\n",
				__func__, __LINE__));
		return CS_E_ERROR;
	}
	return CS_E_OK;
}

/* Exported APIs:
 *	cs_ip_translate_add()
 *	cs_ip_translate_del()
 *	cs_ip_translate_get()
 *  cs_ip_translate_entry_add()
 *  cs_ip_translate_entry_del()
 */
cs_status_t cs_ip_translate_add(
	CS_IN	cs_dev_id_t 			device_id,
	CS_IN	cs_ip_translate_cfg_t	*p_ip_translate_cfg,
	CS_OUT	cs_ip_translate_id_t	*p_ip_translate_id)
{
	cs_status_t ret;
	cs_ip_entry_status_t entry_status;
	cs_int32 	idx = -1;
	cs_uint16 i;
	cs_ip_translate_tunnel_t * tunnel;

	if (p_ip_translate_cfg == NULL) {
		ERR(printk("%s:%d p_ip_translate_cfg = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (p_ip_translate_id == NULL) {
		ERR(printk("%s:%d p_ip_translate_id = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (p_ip_translate_cfg->translate_type != CS_IP_TRANSLATE_TYPE_MAPE) {
		ERR(printk("%s:%d CS IP TRANSLATE doesn't support type %d \n",
			__func__, __LINE__ , p_ip_translate_cfg->translate_type));
		return CS_E_PARAM;
	}

	if ((p_ip_translate_cfg->v6_pkt.da_ip.afi != CS_IPV6) ||
		(p_ip_translate_cfg->v6_pkt.sa_ip.afi != CS_IPV6)) {
		ERR(printk("%s:%d p_flow_entry or p_ip_translate_cfg doesn't set the correct ip address\n",
			__func__, __LINE__));
		return CS_E_PARAM;
	}

	__CS_IP_TS_LOCK();

	entry_status = __cs_ip_translate_search_tunnel(p_ip_translate_cfg, &i);

	if (entry_status != CS_ENTRY_NOT_FOUND) {
		ERR(printk("%s:%d duplicate entry at %d entry for reason code : %d \n",
			__func__, __LINE__ , i, entry_status));
		*p_ip_translate_id = i;
		ret = CS_E_PARAM;
		goto add_exit;
	}

	for (i = 0 ; i < IP_TRANSLATE_TUNNELS_ENTRY_MAX; i++) {
		if (cs_ip_translate_entry[i] == NULL) {
			idx = i;
			break;
		}
	}
	if (i == IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d no more free ip translate entry \n",
			__func__, __LINE__));
			ret = CS_E_RESOURCE;
		goto add_exit;
	}


	if (cs_ip_translate_entry[idx] != NULL) {
		ERR(printk("%s:%d already has entry at idx %d\n",	__func__, __LINE__, idx));
		ret = CS_E_EXISTS;
		goto add_exit;
	}

	tunnel = cs_zalloc(sizeof(cs_ip_translate_tunnel_t), GFP_KERNEL);
	if (tunnel == NULL) {
		ERR(printk("%s:%d out of memory\n",	__func__, __LINE__));
		ret = CS_E_MEM_ALLOC;
		goto add_exit;
	}

	tunnel->device_id = device_id;
	tunnel->outbound_next_func_id = 0xffff;
	tunnel->inbound_next_func_id = 0xffff;
	//memcpy(&entry->flow_entry, p_flow_entry, sizeof(cs_flow_t));
	memcpy(&tunnel->ip_translate_cfg, p_ip_translate_cfg, sizeof(cs_ip_translate_cfg_t));
	tunnel->ip_translate_id = idx;
	//entry->next = cs_ip_translate_entry[ip_translate_id];
	cs_ip_translate_entry[idx] = tunnel;

	__CS_IP_TS_UNLOCK();

	ret = CS_E_ERROR;

#ifdef CS_IPC_ENABLED
	/* Send IPC to PE */
	ret = __cs_ip_translate_ipc_add_entry(tunnel);
#endif /* CS_IPC_ENABLED */

	if (ret != CS_E_OK) {
		cs_ip_translate_entry[idx] = NULL;
		cs_free(tunnel);
	} else {
		DBG(printk("%s:%d add entry at idx %d\n", __func__, __LINE__, idx));
		*p_ip_translate_id = idx;
	}

	if ((g_cs_ip_translate_mape_rule.port_set_id != p_ip_translate_cfg->port_set_id) ||
		(g_cs_ip_translate_mape_rule.port_set_id_length != p_ip_translate_cfg->port_set_id_length) ||
		(g_cs_ip_translate_mape_rule.port_set_id_offset != p_ip_translate_cfg->port_set_id_offset)) {
		g_cs_ip_translate_mape_rule.port_set_id = p_ip_translate_cfg->port_set_id;
		g_cs_ip_translate_mape_rule.port_set_id_length = p_ip_translate_cfg->port_set_id_length;
		g_cs_ip_translate_mape_rule.port_set_id_offset = p_ip_translate_cfg->port_set_id_offset;
		ret = __cs_ip_translate_ipc_set_portset();
		if (ret != CS_E_OK) {
			printk("%s:%d set mape rule fail at entry[%d]\n", __func__, __LINE__, idx);
		}
	}
	return ret;
add_exit:
	__CS_IP_TS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL(cs_ip_translate_add);

cs_status_t cs_ip_translate_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_ip_translate_id_t	ip_translate_id,
	CS_OUT	cs_ip_translate_cfg_t	*p_ip_translate_cfg,
	CS_OUT	cs_flow_t				*p_flow_entry)
{
	if (ip_translate_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , ip_translate_id, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	if ((p_ip_translate_cfg == NULL) || (p_flow_entry == NULL)) {
		ERR(printk("%s:%d p_ip_translate_cfg %p or p_flow_entry %p is NULL\n",
			__func__, __LINE__ , p_ip_translate_cfg, p_flow_entry));
		return CS_E_NULL_PTR;
	}
	__CS_IP_TS_LOCK();

	if (cs_ip_translate_entry[ip_translate_id] == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, ip_translate_id));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}

	memcpy(p_ip_translate_cfg, &cs_ip_translate_entry[ip_translate_id]->ip_translate_cfg, sizeof(cs_ip_translate_cfg_t));

	__CS_IP_TS_UNLOCK();
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_ip_translate_get);

cs_status_t cs_ip_translate_del(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_ip_translate_id_t	ip_translate_id)
{
	cs_ip_translate_tunnel_t *tunnel;
	cs_ip_translate_flow_t *entry;
	cs_ip_translate_flow_t *tmp;

	if (ip_translate_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , ip_translate_id, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}
	__CS_IP_TS_LOCK();

	if (cs_ip_translate_entry[ip_translate_id] == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, ip_translate_id));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}

	tunnel = cs_ip_translate_entry[ip_translate_id];
	entry = tunnel->napt_flow;

	while (entry != NULL) {
		cs_flow_delete(tunnel->device_id, entry->flow_entry_to_pe.flow_id);
		cs_flow_delete(tunnel->device_id, entry->flow_entry_from_pe.flow_id);
		cs_flow_delete(tunnel->device_id, entry->over_size_entry_from_pe.flow_id);
		tmp = entry;
		entry = entry->next;
		cs_free(tmp);
	}

	/*Remove PE->WAN and WAN->PE hash*/
	cs_flow_delete(tunnel->device_id, tunnel->tunnel_entry_to_pe.flow_id);
	cs_flow_delete(tunnel->device_id, tunnel->tunnel_entry_from_pe.flow_id);

#ifdef CS_IPC_ENABLED
	/* Send IPC to PE */
	__cs_ip_translate_ipc_del_entry(tunnel);
#endif /* CS_IPC_ENABLED */

	cs_ip_translate_entry[ip_translate_id] = NULL;
	cs_free(tunnel);

	__CS_IP_TS_UNLOCK();
	return CS_E_OK;

}
EXPORT_SYMBOL(cs_ip_translate_del);

cs_status_t cs_ip_translate_tunnel_handle(
	struct sk_buff 					*skb)
{
	struct ethhdr *eth;
	cs_uint16_t sa_id;
	cs_sa_id_direction_t dir;
	cs_ip_translate_tunnel_t *tunnel;

	eth = eth_hdr(skb);
	sa_id = eth->h_source[3] + eth->h_source[5] * 256;
	dir = eth->h_source[4];

	/* h_source[4] DIR = 1(WAN to LAN)  0(LAN to WAN) */
	/* only handle h_source[4] DIR = 1 esle drop the packet */
	if (dir == DOWN_STREAM) {
		DBG(printk("%s:%d New flow, len = %d, sa_id = %d\n",
			__func__, __LINE__, skb->len, sa_id));
		DBG(cs_dump_data((void *)eth, skb->len));

		if (sa_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
			ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
				__func__, __LINE__ , sa_id, IPSEC_TUNNELS_MAX));
			goto SKB_HANDLED;
		}

		tunnel = cs_ip_translate_entry[sa_id];
		if (tunnel == NULL) {
			ERR(printk("%s:%d entry[%d] is empty\n", __func__, __LINE__, sa_id));
			__CS_IP_TS_UNLOCK();
			goto SKB_HANDLED;
		}

		if (tunnel->inbound_next_func_id != 0xffff) {
			eth->h_source[1] = tunnel->inbound_next_func_id;
			eth->h_source[3] = tunnel->inbound_next_said;
			eth->h_source[5] = tunnel->inbound_next_said >> 8;
			return __cs_hw_accel_tunnel_handle(skb);
		}

	} else {
		DBG(printk("%s:%d Receive a upstream packet. "
			"Can't hit PE to WAN hash\n",
			__func__, __LINE__));
		DBG(cs_dump_data((void *)eth, skb->len));
		goto SKB_HANDLED;
	}

	return CS_OK;

SKB_HANDLED:
	dev_kfree_skb(skb);
	return CS_E_ERROR;
}

cs_status_t cs_ip_translate_flow_add(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_id_t	ip_translate_id,
CS_IN_OUT	cs_flow_t			*p_flow_entry)
{
	cs_status_t ret;
	cs_ip_translate_tunnel_t * tunnel;
	cs_ip_translate_flow_t * entry;

	if (p_flow_entry == NULL) {
		ERR(printk("%s:%d p_flow_entry = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (ip_translate_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , ip_translate_id, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	if ((p_flow_entry->ingress_pkt.da_ip.afi != CS_IPV4) ||
		(p_flow_entry->ingress_pkt.sa_ip.afi != CS_IPV4) ||
		(p_flow_entry->egress_pkt.da_ip.afi != CS_IPV4) ||
		(p_flow_entry->egress_pkt.sa_ip.afi != CS_IPV4)) {
		ERR(printk("%s:%d p_flow_entry doesn't set the correct ip afi (should be all 4)\n",
			__func__, __LINE__));
		return CS_E_PARAM;
	}
	__CS_IP_TS_LOCK();

	tunnel = cs_ip_translate_entry[ip_translate_id];

	if (tunnel == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, ip_translate_id));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}

	entry = __cs_ip_translate_search_entry(tunnel, p_flow_entry);

	if (entry != NULL) {
		ERR(printk("%s:%d duplicate entry at %d entry\n",
			__func__, __LINE__ , ip_translate_id));
		ret = CS_E_PARAM;
		goto add_exit;
	}

	entry = cs_zalloc(sizeof(cs_ip_translate_flow_t), GFP_KERNEL);
	if (entry == NULL) {
		ERR(printk("%s:%d out of memory\n", __func__, __LINE__));
		ret = CS_E_MEM_ALLOC;
		goto add_exit;
	}

	memcpy(&entry->flow_entry, p_flow_entry, sizeof(cs_flow_t));

	/*
	 * create LAN->PE / PE->LAN hash
	 */
	ret = __cs_ip_translate_entry_add_hash(tunnel, entry);

	if (ret != CS_E_OK) {
		cs_free(entry);
		goto add_exit;
	}

	DBG(printk("%s:%d add flow entry at idx %d\n", __func__, __LINE__, ip_translate_id));

	entry->next = tunnel->napt_flow;
	if (tunnel->napt_flow) {
		tunnel->napt_flow->prev = entry;
	}
	tunnel->napt_flow = entry;

	__CS_IP_TS_UNLOCK();
	return ret;
add_exit:
	__CS_IP_TS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL(cs_ip_translate_flow_add);


cs_status_t cs_ip_translate_flow_del(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_id_t	ip_translate_id,
CS_IN_OUT	cs_flow_t			*p_flow_entry)
{
	cs_status_t ret;
	cs_ip_translate_tunnel_t * tunnel;
	cs_ip_translate_flow_t * entry;

	if (p_flow_entry == NULL) {
		ERR(printk("%s:%d p_flow_entry = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (ip_translate_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , ip_translate_id, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	__CS_IP_TS_LOCK();

	tunnel = cs_ip_translate_entry[ip_translate_id];

	if (tunnel == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, ip_translate_id));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}

	entry = __cs_ip_translate_search_entry(tunnel, p_flow_entry);

	if (entry == NULL) {
		ERR(printk("%s:%d cannot find entry at %d entry\n",
			__func__, __LINE__ , ip_translate_id));
		ret = CS_E_PARAM;
		goto add_exit;
	}

	/*Remove PE->LAN and LAN->PE hash*/
	cs_flow_delete(tunnel->device_id, entry->flow_entry_to_pe.flow_id);
	cs_flow_delete(tunnel->device_id, entry->flow_entry_from_pe.flow_id);
	cs_flow_delete(tunnel->device_id, entry->over_size_entry_from_pe.flow_id);

	if (entry->prev) {
		entry->prev->next = entry->next;
	} else
		tunnel->napt_flow = entry->next;

	if (entry->next) {
			entry->next->prev = entry->prev;
	}
	cs_free(entry);
add_exit:
	__CS_IP_TS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL(cs_ip_translate_flow_del);


cs_status_t cs_ip_translate_set_src_mac(
	CS_IN	cs_l3_nexthop_t *nexthop,
	CS_OUT	char *src_mac)
{
	int crc32;
	int ip_translate_id;
	cs_ip_translate_tunnel_t *tunnel;
	cs_uint8_t mac[CS_ETH_ADDR_LEN];

	if (nexthop == NULL || src_mac == NULL) {
		printk(KERN_ERR "%s: ERROR NULL pointer.\n", __func__);
		return CS_E_NULL_PTR;
	}

	printk(KERN_DEBUG "%s: tunnel_id=%u\n", __func__, nexthop->id.tunnel_id);
	ip_translate_id = nexthop->id.tunnel_id;

	if (ip_translate_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , ip_translate_id, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	tunnel = cs_ip_translate_entry[ip_translate_id];
	if (tunnel == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, ip_translate_id));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}
	mac[5] = tunnel->ip_translate_id >> 8;
	mac[4] = 0x00; /* 0: upstream, 1: downstream */
	mac[3] = tunnel->ip_translate_id & 0xFF; /* SA ID */
	mac[2] = 0x00; /* 0: LAN->PE or WAN->PE, 1: CPU->PE */
	mac[1] = CS_FUNC_ID_MAPE; /* cs_tunnel_type_t */
	crc32 = ~(calc_crc(~0, (u8 const *) &mac[1], 5));
	mac[0] = crc32 & 0xff;

	src_mac[0] = mac[5];
	src_mac[1] = mac[4];
	src_mac[2] = mac[3];
	src_mac[3] = mac[2];
	src_mac[4] = mac[1];
	src_mac[5] = mac[0];

	return CS_E_NOT_FOUND;
}

/* IPC functions */
#ifdef CS_IPC_ENABLED

cs_status_t
cs_ip_translate_ipc_rcv_del_entry_ack(
		struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context
		)
{
	g2_ipc_pe_ip_translate_del_entry_ack_t *msg;
	cs_uint16_t sa_id;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_ip_translate_del_entry_ack_t * )msg_data;
	sa_id = msg->sa_idx;

	DBG(printk("%s %d RCV entry ack from PE%d sa_id=%d", __func__, __LINE__,
		peer.cpu_id - 1, sa_id));

	return CS_OK;
};

cs_status_t
cs_ip_translate_ipc_rcv_set_entry_ack(
		struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context
		)
{
	g2_ipc_pe_ip_translate_set_entry_ack_t *msg;
	cs_uint16_t sa_id;
	cs_status_t err;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_ip_translate_set_entry_ack_t * )msg_data;
	sa_id = msg->sa_idx;

	DBG(printk("%s %d RCV entry ack from PE%d sa_id=%d", __func__, __LINE__,
		peer.cpu_id - 1, sa_id));

	/*Create PE->WAN and WAN->PE hash*/
	err = __cs_ip_translate_add_tunnel_hash((peer.cpu_id == CPU_RCPU0) ?
			CS_TUNNEL_DIR_INBOUND: CS_TUNNEL_DIR_OUTBOUND, sa_id);
	if (err != CS_E_OK)
		cs_ip_ts_wq_wakeup = CS_ERROR;
	else
		cs_ip_ts_wq_wakeup = CS_DONE;

	return CS_E_OK;
};

cs_status_t
__cs_ip_translate_ipc_add_entry(cs_ip_translate_tunnel_t *tunnel)
{
	g2_ipc_pe_ip_translate_set_entry_t ip_translate_msg;
	cs_status_t err;
	int crc32;
	unsigned long  ms = 1000; /* timeout, unit is ms */
	int ret;

	memset(&ip_translate_msg, 0, sizeof(g2_ipc_pe_ip_translate_set_entry_t));
	ip_translate_msg.sa_idx = tunnel->ip_translate_id;
	ip_translate_msg.translate_type = tunnel->ip_translate_cfg.translate_type;

	ip_translate_msg.src_ipv6.ipv6_addr[0] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[0];
	ip_translate_msg.src_ipv6.ipv6_addr[1] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[1];
	ip_translate_msg.src_ipv6.ipv6_addr[2] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[2];
	ip_translate_msg.src_ipv6.ipv6_addr[3] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[3];
	ip_translate_msg.dst_ipv6.ipv6_addr[0] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[0];
	ip_translate_msg.dst_ipv6.ipv6_addr[1] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[1];
	ip_translate_msg.dst_ipv6.ipv6_addr[2] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[2];
	ip_translate_msg.dst_ipv6.ipv6_addr[3] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[3];

	ip_translate_msg.copy_tos = tunnel->ip_translate_cfg.copy_tos;
	ip_translate_msg.default_tos = tunnel->ip_translate_cfg.default_tos;
	ip_translate_msg.port_set_id = tunnel->ip_translate_cfg.port_set_id;
	ip_translate_msg.port_set_id_length = tunnel->ip_translate_cfg.port_set_id_length;
	ip_translate_msg.port_set_id_offset = tunnel->ip_translate_cfg.port_set_id_offset;
	memcpy(&ip_translate_msg.egress_da_mac[0], &tunnel->ip_translate_cfg.v6_pkt.sa_mac[0], CS_ETH_ADDR_LEN);
	ip_translate_msg.egress_sa_mac[1] = CS_FUNC_ID_MAPE & 0xFF;
	ip_translate_msg.egress_sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	ip_translate_msg.egress_sa_mac[3] = tunnel->ip_translate_id & 0xFF;
	ip_translate_msg.egress_sa_mac[4] = 1;
	ip_translate_msg.egress_sa_mac[5] = tunnel->ip_translate_id >> 8;
	crc32 = ~(calc_crc(~0, (u8 const *) &ip_translate_msg.egress_sa_mac[1], 5));
	ip_translate_msg.egress_sa_mac[0] = crc32 & 0xFF;

	cs_ip_ts_wq_wakeup = 0;

	err = cs_pe_ipc_send(CS_TUNNEL_DIR_INBOUND, CS_IP_TS_IPC_PE_SET_ENTRY, &ip_translate_msg,
						sizeof(g2_ipc_pe_ip_translate_set_entry_t));
	if (err != CS_E_OK) {
		printk("%s %d send inbound CS_IP_TS_IPC_PE_SET_ENTRY ipc fail", __func__, __LINE__);
		return err;
	}

	ret = wait_event_interruptible_timeout(cs_ip_ts_wq, cs_ip_ts_wq_wakeup != 0, ms * 100 / 1000);

	if (cs_ip_ts_wq_wakeup != CS_DONE) {
		printk("%s %d send inbound CS_IP_TS_IPC_PE_SET_ENTRY ipc timeout", __func__, __LINE__);
		return CS_E_ERROR;
	}

	cs_ip_ts_wq_wakeup = 0;
	ip_translate_msg.sa_idx = tunnel->ip_translate_id;
	ip_translate_msg.src_ipv6.ipv6_addr[0] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[0];
	ip_translate_msg.src_ipv6.ipv6_addr[1] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[1];
	ip_translate_msg.src_ipv6.ipv6_addr[2] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[2];
	ip_translate_msg.src_ipv6.ipv6_addr[3] = tunnel->ip_translate_cfg.v6_pkt.sa_ip.ip_addr.ipv6_addr[3];
	ip_translate_msg.dst_ipv6.ipv6_addr[0] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[0];
	ip_translate_msg.dst_ipv6.ipv6_addr[1] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[1];
	ip_translate_msg.dst_ipv6.ipv6_addr[2] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[2];
	ip_translate_msg.dst_ipv6.ipv6_addr[3] = tunnel->ip_translate_cfg.v6_pkt.da_ip.ip_addr.ipv6_addr[3];
	memcpy(&ip_translate_msg.egress_da_mac[0], &tunnel->ip_translate_cfg.v6_pkt.sa_mac[0], CS_ETH_ADDR_LEN);
	ip_translate_msg.egress_sa_mac[1] = CS_FUNC_ID_MAPE & 0xFF;
	ip_translate_msg.egress_sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	ip_translate_msg.egress_sa_mac[3] = tunnel->ip_translate_id & 0xFF;
	ip_translate_msg.egress_sa_mac[4] = 0;
	ip_translate_msg.egress_sa_mac[5] = tunnel->ip_translate_id >> 8;
	crc32 = ~(calc_crc(~0, (u8 const *) &ip_translate_msg.egress_sa_mac[1], 5));
	ip_translate_msg.egress_sa_mac[0] = crc32 & 0xFF;

	err = cs_pe_ipc_send(CS_TUNNEL_DIR_OUTBOUND, CS_IP_TS_IPC_PE_SET_ENTRY, &ip_translate_msg,
					sizeof(g2_ipc_pe_ip_translate_set_entry_t));
	if (err != CS_E_OK) {
		printk("%s %d send outbound CS_IP_TS_IPC_PE_SET_ENTRY ipc fail", __func__, __LINE__);
		return err;
	}

	ret = wait_event_interruptible_timeout(cs_ip_ts_wq, cs_ip_ts_wq_wakeup != 0, ms * 100 / 1000);

	if (cs_ip_ts_wq_wakeup != CS_DONE) {
		/*need to delete INBOUND Hash*/
		/* del flow hash */
		cs_flow_delete(tunnel->device_id, tunnel->tunnel_entry_to_pe.flow_id);
		printk("%s %d send outbound CS_IP_TS_IPC_PE_SET_ENTRY ipc timeout", __func__, __LINE__);
		return CS_E_ERROR;
	}
	return CS_E_OK;
}

cs_status_t
__cs_ip_translate_ipc_del_entry(cs_ip_translate_tunnel_t *tunnel)
{
	g2_ipc_pe_ip_translate_del_entry_t ip_translate_msg;
	cs_status_t err;

	ip_translate_msg.sa_idx = tunnel->ip_translate_id;
	err = cs_pe_ipc_send(CS_TUNNEL_DIR_OUTBOUND, CS_IP_TS_IPC_PE_DEL_ENTRY, &ip_translate_msg,
					sizeof(g2_ipc_pe_ip_translate_del_entry_t));
	if (err != CS_OK)
			return err;

	ip_translate_msg.sa_idx = tunnel->ip_translate_id;
	return cs_pe_ipc_send(CS_TUNNEL_DIR_INBOUND, CS_IP_TS_IPC_PE_DEL_ENTRY, &ip_translate_msg,
					sizeof(g2_ipc_pe_ip_translate_set_entry_t));

}

cs_status_t
__cs_ip_translate_ipc_set_portset(void)
{
	g2_ipc_pe_ip_translate_portset_set_t ip_translate_msg;
	cs_status_t err;

	ip_translate_msg.port_set_id = g_cs_ip_translate_mape_rule.port_set_id;
	ip_translate_msg.port_set_id_length = g_cs_ip_translate_mape_rule.port_set_id_length;
	ip_translate_msg.port_set_id_offset = g_cs_ip_translate_mape_rule.port_set_id_offset;

	err = cs_pe_ipc_send(CS_TUNNEL_DIR_OUTBOUND, CS_IPC_PE_IP_TS_PORTSET_SET, &ip_translate_msg,
					sizeof(g2_ipc_pe_ip_translate_portset_set_t));
	if (err != CS_OK)
			return err;

	return cs_pe_ipc_send(CS_TUNNEL_DIR_INBOUND, CS_IPC_PE_IP_TS_PORTSET_SET, &ip_translate_msg,
					sizeof(g2_ipc_pe_ip_translate_portset_set_t));

}

cs_status_t
cs_ip_translate_nexthop_set(cs_uint8_t dir, cs_uint8_t tunnel_type,
	cs_uint16_t tunnel_id, cs_uint16_t next_tunnel_id)
{
	g2_ipc_pe_next_hop_set_entry_t msg;
	cs_uint16_t next_said, orig_said;
	int ret;
	cs_ip_translate_tunnel_t *tunnel;

	if (tunnel_type >= CS_TUN_TYPE_MAX) {
		return CS_E_ERROR;
	}

	next_said = next_tunnel_id;
	if (next_said > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , next_said, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	tunnel = cs_ip_translate_entry[next_said];

	if (tunnel == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n", __func__, __LINE__, next_said));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}
	ret = cs_sa_id_get_by_tunnel_id(tunnel_id, tunnel_type,
						dir, &orig_said);
	if (ret != CS_OK)
		return ret;


	memset(&msg, 0, sizeof(g2_ipc_pe_next_hop_set_entry_t));

	if (dir == DOWN_STREAM) {
		msg.fun_id = CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE;
		msg.next_fun_id = tunnel_type;
		msg.sa_id = next_said;
		msg.next_sa_id = orig_said;
		tunnel->inbound_next_func_id = tunnel_type;
		tunnel->inbound_next_said = orig_said;
	} else {
		msg.fun_id = tunnel_type;
		msg.next_fun_id = CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE;
		msg.sa_id = orig_said;
		msg.next_sa_id = next_said;
		tunnel->outbound_next_func_id = tunnel_type;
		tunnel->outbound_next_said = orig_said;
	}
	ret = cs_pe_ipc_send(dir, CS_NEXT_HOP_IPC_PE_SET_ENTRY, &msg,
						sizeof(g2_ipc_pe_next_hop_set_entry_t));

	if (ret != CS_E_OK)
			return ret;


	return CS_E_OK;
}

cs_status_t
cs_ip_translate_nexthop_del(cs_uint8_t dir, cs_uint8_t tunnel_type,
	cs_uint16_t tunnel_id, cs_uint16_t next_tunnel_id)
{
	g2_ipc_pe_next_hop_del_entry_t msg;
	cs_uint16_t next_said, orig_said;
	int ret;
	cs_ip_translate_tunnel_t *tunnel;

	if (tunnel_type >= CS_TUN_TYPE_MAX) {
		return CS_E_ERROR;
	}

	next_said = next_tunnel_id;
	if (next_said > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , next_said, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	tunnel = cs_ip_translate_entry[next_said];

	if (tunnel == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, next_said));
		__CS_IP_TS_UNLOCK();
		return CS_E_NOT_FOUND;
	}
	ret = cs_sa_id_get_by_tunnel_id(tunnel_id, tunnel_type,
						dir, &orig_said);
	if (ret != CS_OK)
		return ret;


	memset(&msg, 0, sizeof(g2_ipc_pe_next_hop_set_entry_t));

	if (dir == DOWN_STREAM) {
		msg.fun_id = CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE;
		msg.next_fun_id = tunnel_type;
		msg.sa_id = next_said;
		msg.next_sa_id = orig_said;
		if ((tunnel->inbound_next_func_id != tunnel_type) ||
			(tunnel->inbound_next_said != orig_said))
		{
			return CS_E_NOT_FOUND;
		}
		tunnel->inbound_next_func_id = 0xffff;
	} else {
		msg.fun_id = tunnel_type;
		msg.next_fun_id = CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE;
		msg.sa_id = orig_said;
		msg.next_sa_id = next_said;
		tunnel->outbound_next_func_id = 0xffff;
	}

	ret = cs_pe_ipc_send(dir, CS_NEXT_HOP_IPC_PE_DEL_ENTRY, &msg,
						sizeof(g2_ipc_pe_next_hop_set_entry_t));

	if (ret != CS_E_OK)
			return ret;

	return CS_E_OK;
}

#endif /* CS_IPC_ENABLED */


cs_status_t cs_ip_translate_sa_id_get_by_ip_translate_id(
	CS_IN	cs_ip_translate_id_t	ip_translate_id,
	CS_IN	cs_uint16_t	dir,
	CS_OUT	cs_uint16_t	*sa_id)
{
	if (ip_translate_id > IP_TRANSLATE_TUNNELS_ENTRY_MAX) {
		ERR(printk("%s:%d ipsec tunnel entry [%d] over MAX size %d \n",
			__func__, __LINE__ , ip_translate_id, IPSEC_TUNNELS_MAX));
		return CS_E_PARAM;
	}

	if (cs_ip_translate_entry[ip_translate_id] == NULL) {
		ERR(printk("%s:%d entry[%d] is empty\n",	__func__, __LINE__, ip_translate_id));
		return CS_E_NOT_FOUND;
	}

	*sa_id = ip_translate_id;

	return CS_E_NOT_FOUND;
}

void cs_hw_accel_ip_translate_init(void)
{
	memset(cs_ip_translate_entry, 0,
		sizeof(cs_ip_translate_tunnel_t * ) * IP_TRANSLATE_TUNNELS_ENTRY_MAX);
	memset(&g_cs_ip_translate_mape_rule, 0,
		sizeof(cs_ip_translate_mape_rule_t));

	init_waitqueue_head(&cs_ip_ts_wq);
	spin_lock_init(&cs_ip_ts_lock);

}

void cs_hw_accel_ip_translate_exit(void)
{
}
