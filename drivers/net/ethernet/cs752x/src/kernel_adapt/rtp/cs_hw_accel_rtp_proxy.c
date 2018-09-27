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
 * cs_hw_accel_rtp_proxy.c
 *
 * $Id$
 *
 * This file contains the implementation for CS RTP proxy
 * Acceleration.
 */

#include "cs_hw_accel_rtp_proxy.h"
#include "cs_hw_accel_qos_data.h"
#include "cs752x_sch.h"
#include <mach/cs_network_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#ifdef CS_IPC_ENABLED
#include <mach/cs_vpn_tunnel_ipc.h>
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#endif /* CONFIG_CS752X_PROC */

#include "cs_mut.h"

/* TODO:  add a RTP proxy cs_adapt_debug flag*/
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#define ERR(x)	(x)

/* static variables */
static cs_rtp_proxy_cb_t cs_rtp_proxy_cb;

/* Exported APIs:
 *	cs_rtp_translate_add()
 *	cs_rtp_translate_delete()
 *	cs_rtp_statistics_get()
 */
cs_status_t cs_rtp_translate_add(
		CS_IN	cs_dev_id_t		device_id,
		CS_IN	cs_rtp_cfg_t		*p_rtp_cfg,
		CS_IN	cs_flow_t		*p_flow_entry,
		CS_OUT	cs_rtp_id_t		*p_rtp_id
		)
{
	cs_status_t ret;
	cs_rtp_entry_t *p_rtp_node, *rtp_h, *rtp_tmp, *t;

	DBG(printk("%s:%d device_id=%d, p_rtp_cfg = 0x%p,"
		" p_flow_entry = 0x%p\n",
		__func__, __LINE__, device_id, p_rtp_cfg, p_flow_entry));

	if (cs_rtp_proxy_cb.device_id == 0) {
		cs_rtp_proxy_cb.device_id = device_id;
	} else if (cs_rtp_proxy_cb.device_id != device_id) {
		printk("RTP device_id (%d) is replaced with (%d)\n",
			cs_rtp_proxy_cb.device_id, device_id);
		cs_rtp_proxy_cb.device_id = device_id;
	}

	if (p_rtp_cfg == NULL) {
		ERR(printk("%s:%d p_rtp_cfg = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (p_flow_entry == NULL) {
		ERR(printk("%s:%d p_flow_entry = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}


	if (p_flow_entry->ingress_pkt.pppoe_session_id_valid != 0
		&& p_flow_entry->egress_pkt.pppoe_session_id_valid != 0){
		ERR(printk("%s:%d Does not support PPPoE session id replacement!!\n",
			__func__, __LINE__));
		return CS_E_PARAM;
	}

	if (p_rtp_id == NULL) {
		ERR(printk("%s:%d p_rtp_id = NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	/* Check if the RTP offload configuration has already existed */
	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		if (t->rtp_cfg.input_ssrc ==
			p_rtp_cfg->input_ssrc &&
			t->rtp_cfg.output_ssrc ==
			p_rtp_cfg->output_ssrc) {
			ERR(printk("%s:%d RTP offload configuration has "
				"already existed\n", __func__, __LINE__));
			return CS_E_CONFLICT;
		}
		t = t->next;
	}

	/* Check if ingress SA MAC is same as egress SA MAC*/
	if(memcmp(p_flow_entry->ingress_pkt.sa_mac,
			p_flow_entry->egress_pkt.sa_mac,
			CS_ETH_ADDR_LEN) == 0) {
		ERR(printk("%s:%d Ingress SA MAC can't be same as "
			"egress SA MAC\n", __func__, __LINE__));
		return CS_E_CONFLICT;
	}

	p_rtp_node = cs_zalloc(sizeof(cs_rtp_entry_t), GFP_KERNEL);

	if (p_rtp_node == NULL) {
		ERR(printk("%s:%d out of memory\n",
			__func__, __LINE__));
		return CS_E_MEM_ALLOC;
	}

	/* Check if the current rtp id is used */
	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		if (t->rtp_id == cs_rtp_proxy_cb.curr_rtp_id) {
			cs_rtp_proxy_cb.curr_rtp_id++;
		}
		t = t->next;
	}

	memcpy(&p_rtp_node->rtp_cfg, p_rtp_cfg,
					sizeof(cs_rtp_cfg_t));

	if(p_flow_entry->ingress_pkt.phy_port == CS_PORT_GMAC0) /*from eth0*/
		p_rtp_node->dir = CS_RTP_FROMWAN;
	else if(p_flow_entry->ingress_pkt.phy_port == CS_PORT_GMAC1) /*from eth1*/
		p_rtp_node->dir = CS_RTP_FROMLAN;
	else{
		printk("%s:%d invalid ingress_pkt phy_port = %d\n", __func__, __LINE__,
			p_flow_entry->ingress_pkt.phy_port);
		cs_free(p_rtp_node);
		return CS_E_PARAM;
	}

	/* Apply a sa_id */
	ret = cs_sa_id_alloc(p_rtp_node->dir,
				0,
				0,
				CS_RTP,
				&p_rtp_node->sa_id);

	if (ret != CS_OK) {
		ERR(printk("%s:%d can't apply sa_id\n",
					__func__, __LINE__));
		cs_free(p_rtp_node);
		return CS_E_CONFLICT;
	}

	ret = construct_flow_entries(p_rtp_node, p_flow_entry);

	if (ret != CS_OK) {
		ERR(printk("%s:%d construct flow entries failed\n",
					__func__, __LINE__));
		cs_free(p_rtp_node);
		return ret;
	}

	/* Insert RTP entry into cs_rtp_proxy_cb list */
	rtp_h = cs_rtp_proxy_cb.rtp_h;
	if (rtp_h == NULL) {
		cs_rtp_proxy_cb.rtp_h = p_rtp_node;
		cs_rtp_proxy_cb.rtp_cnt = 1;
	} else {
		while (rtp_h) {
			rtp_tmp = rtp_h;
			rtp_h = rtp_h->next;
		}
		rtp_tmp->next = p_rtp_node;
		p_rtp_node->prev = rtp_tmp;
		cs_rtp_proxy_cb.rtp_cnt++;
	}

	DBG(printk("%s:%d Current rtp_cnt=%u curr_rtp_id=%u\n",
			__func__, __LINE__,
			cs_rtp_proxy_cb.rtp_cnt, cs_rtp_proxy_cb.curr_rtp_id));

	p_rtp_node->rtp_id = cs_rtp_proxy_cb.curr_rtp_id++;

	*p_rtp_id = p_rtp_node->rtp_id;

#ifdef CS_IPC_ENABLED
	/* Send IPC to PE */
	cs_rtp_ipc_send_set_entry(p_rtp_node->dir, &p_rtp_node->rtp_cfg,
					p_flow_entry->egress_pkt.sa_mac, p_rtp_node->sa_id);
#endif /* CS_IPC_ENABLED */
	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_rtp_translate_add);

cs_status_t cs_rtp_translate_delete(
		CS_IN	cs_dev_id_t	device_id,
		CS_IN	cs_rtp_cfg_t	*p_rtp_cfg
		)
{
	cs_rtp_entry_t *t;
	cs_uint16_t flow_id_to_pe;
	cs_uint16_t flow_id_from_pe;

	DBG(printk("%s:%d device_id=%d, p_rtp_cfg = 0x%p,",
		__func__, __LINE__, device_id, p_rtp_cfg));

	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		/* search by input_ssrc & output_ssrc */
		if (t->rtp_cfg.input_ssrc ==
			p_rtp_cfg->input_ssrc &&
			t->rtp_cfg.output_ssrc ==
			p_rtp_cfg->output_ssrc) {

			flow_id_from_pe = t->flow_entry_from_pe.flow_id;
			flow_id_to_pe = t->flow_entry_to_pe.flow_id;

			if (t == cs_rtp_proxy_cb.rtp_h)
				cs_rtp_proxy_cb.rtp_h = t->next;
			else
				t->prev->next = t->next;

			if (t->next != NULL)
				t->next->prev = t->prev;

			cs_rtp_proxy_cb.rtp_cnt--;

			/* del flow hash */
			cs_flow_delete(device_id, flow_id_from_pe);

			cs_flow_delete(device_id, flow_id_to_pe);

			/* free sa id */
			cs_sa_id_free(t->dir, t->sa_id);
#ifdef CS_IPC_ENABLED
			cs_rtp_ipc_send_del_entry(t->dir, t->sa_id);
#endif /* CS_IPC_ENABLED */
			cs_free(t);
			return CS_E_NONE;
		}

		t = t->next;
	}

	return CS_E_NOT_FOUND;

}
EXPORT_SYMBOL(cs_rtp_translate_delete);

cs_status_t cs_rtp_translate_delete_by_id(
		CS_IN	cs_dev_id_t	device_id,
		CS_IN	cs_rtp_id_t	rtp_id
		)
{
	cs_rtp_entry_t *t;
	cs_uint16_t flow_id_to_pe;
	cs_uint16_t flow_id_from_pe;

	DBG(printk("%s:%d device_id=%d, rtp_id=%d\n",
		__func__, __LINE__, device_id, rtp_id));

	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		/* search by rtp_id */
		if (t->rtp_id == rtp_id) {

			flow_id_from_pe = t->flow_entry_from_pe.flow_id;
			flow_id_to_pe = t->flow_entry_to_pe.flow_id;

			if (t == cs_rtp_proxy_cb.rtp_h)
				cs_rtp_proxy_cb.rtp_h = t->next;
			else
				t->prev->next = t->next;

			if (t->next != NULL)
				t->next->prev = t->prev;

			cs_rtp_proxy_cb.rtp_cnt--;

			/* del flow hash */
			cs_flow_delete(device_id, flow_id_from_pe);

			cs_flow_delete(device_id, flow_id_to_pe);

			/* free sa id */
			cs_sa_id_free(t->dir, t->sa_id);
#ifdef CS_IPC_ENABLED
			cs_rtp_ipc_send_del_entry(t->dir, t->sa_id);
#endif /* CS_IPC_ENABLED */
			cs_free(t);
			return CS_E_NONE;
		}

		t = t->next;
	}
	ERR(printk("%s:%d Can't find rtp_id=%d\n", __func__, __LINE__, rtp_id));
	return CS_E_NOT_FOUND;

}
EXPORT_SYMBOL(cs_rtp_translate_delete_by_id);

cs_status_t cs_rtp_statistics_get(
		CS_IN	cs_rtp_id_t	rtp_id,
		CS_OUT	cs_rtp_stats_t	*data
	)
{
	cs_rtp_stats_t *pe_stats;
	cs_rtp_entry_t *t;

	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		if(t->rtp_id == rtp_id)
		{
			break;
		}
		t = t->next;
	}

	if(t == NULL){
		printk("%s:%d rtp_id(%d) does not exist\n", __func__, __LINE__, rtp_id);
		return CS_E_NOT_FOUND;
	}

	DBG(printk("%s:%d rtp_id=%d, sa_id=%d, dir=%d\n"
		" data = 0x%p\n",
		__func__, __LINE__, rtp_id, t->sa_id, t->dir, data));

	if(t->dir == CS_RTP_FROMLAN) { /* UPSTREAM */
		pe_stats = CS_RTP_PE1_STATS_BASE + t->sa_id * sizeof(cs_rtp_stats_t);
	}
	else if(t->dir == CS_RTP_FROMWAN){ /* DOWNSTREAM  */
		pe_stats = CS_RTP_PE0_STATS_BASE + t->sa_id * sizeof(cs_rtp_stats_t);
	}
	else{
		printk("%s:%d invalid direction = %d\n", __func__, __LINE__, t->dir);
		return CS_E_PARAM;
	}
	memcpy(data, pe_stats, sizeof(cs_rtp_stats_t));
	DBG(printk("%s:%d data->available = %u\n", __func__, __LINE__, data->available));
	DBG(printk("%s:%d data->seq_num = %u\n", __func__, __LINE__, data->seq_num));
	DBG(printk("%s:%d data->timestamp = %u\n", __func__, __LINE__, data->timestamp));
	DBG(printk("%s:%d data->pkt_cnt = %u\n", __func__, __LINE__, data->pkt_cnt));
	DBG(printk("%s:%d data->byte_cnt = %u\n", __func__, __LINE__, data->byte_cnt));

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_rtp_statistics_get);

/* IPC functions */
#ifdef CS_IPC_ENABLED
cs_status_t
cs_rtp_ipc_send_set_entry(
		cs_sa_id_direction_t direction,
		cs_rtp_cfg_t *p_rtp_cfg,
		cs_uint8_t *egress_sa_mac,
		cs_uint16_t sa_id
		)
{
	g2_ipc_pe_rtp_set_entry_t 	rtp_entry_msg;

	if (p_rtp_cfg == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	memset(&rtp_entry_msg, 0, sizeof(g2_ipc_pe_rtp_set_entry_t));
	rtp_entry_msg.seq_num_diff = p_rtp_cfg->seq_num_diff;
	rtp_entry_msg.timestamp_diff = p_rtp_cfg->timestamp_diff;
	rtp_entry_msg.input_ssrc = p_rtp_cfg->input_ssrc;
	rtp_entry_msg.output_ssrc = p_rtp_cfg->output_ssrc;
	rtp_entry_msg.sa_idx = sa_id;

	memcpy(rtp_entry_msg.egress_sa_mac, egress_sa_mac,
		CS_ETH_ADDR_LEN);

	DBG(printk("%s:%d sa_idx = %d\n", __func__, __LINE__, rtp_entry_msg.sa_idx));
	DBG(printk("%s:%d seq_num_diff = %d\n", __func__, __LINE__, rtp_entry_msg.seq_num_diff));
	DBG(printk("%s:%d timestamp_diff = %d\n", __func__, __LINE__, rtp_entry_msg.timestamp_diff));
	DBG(printk("%s:%d input_ssrc = %d\n", __func__, __LINE__, rtp_entry_msg.input_ssrc));
	DBG(printk("%s:%d output_ssrc = %d\n", __func__, __LINE__, rtp_entry_msg.output_ssrc));

	return cs_pe_ipc_send(direction, CS_RTP_IPC_PE_SET_ENTRY, &rtp_entry_msg,
					sizeof(g2_ipc_pe_rtp_set_entry_t));
}

/*CS_RTP_IPC_PE_DEL_ENTRY*/
cs_status_t
cs_rtp_ipc_send_del_entry(
		cs_sa_id_direction_t direction,
		cs_uint16_t sa_id
		)
{
	g2_ipc_pe_rtp_del_entry_t rtp_entry_msg;

	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));

	memset(&rtp_entry_msg, 0, sizeof(g2_ipc_pe_rtp_del_entry_t));

	rtp_entry_msg.sa_idx = sa_id;

	return cs_pe_ipc_send(direction, CS_RTP_IPC_PE_DEL_ENTRY, &rtp_entry_msg,
					sizeof(g2_ipc_pe_rtp_del_entry_t));
}

cs_status_t cs_rtp_ipc_rcv_set_entry_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_rtp_set_entry_ack_t *msg;
	cs_rtp_entry_t *rtp_entry;
	cs_uint16_t sa_id;
	cs_status_t ret;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_rtp_set_entry_ack_t * )msg_data;
	sa_id = msg->sa_idx;

	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<RTP>: Receive set entry ack from PE%d", peer.cpu_id - 1));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));

	/* find rtp by sa id */
	ret = cs_rtp_entry_get_by_sa_id(sa_id,
			(peer.cpu_id == CPU_RCPU0) ? DOWN_STREAM : UP_STREAM,
			 &rtp_entry);

	if(ret != CS_OK) {
		ERR(printk("%s:%d Can't find valid rtp entry\n",
			__func__, __LINE__));
		return ret;
	}

	if (rtp_entry == NULL) {
		ERR(printk("%s:%d rtp_entry is NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	ret = cs_flow_add(cs_rtp_proxy_cb.device_id, &rtp_entry->flow_entry_from_pe);

	if(ret != CS_OK) {
		ERR(printk("%s:%d Can't create flow hash(from PE) for RTP\n",
			__func__, __LINE__));
	}

	ret = cs_flow_add(cs_rtp_proxy_cb.device_id, &rtp_entry->flow_entry_to_pe);

	if(ret != CS_OK) {
		ERR(printk("%s:%d Can't create flow hash(to PE) for RTP\n",
			__func__, __LINE__));
	}

	return ret;
}

cs_status_t cs_rtp_ipc_rcv_del_entry_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_rtp_del_entry_ack_t *msg;
	cs_uint16_t sa_id;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_rtp_del_entry_ack_t * )msg_data;
	sa_id = msg->sa_idx;

	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<RTP>: Receive del entry ack from PE%d", peer.cpu_id - 1));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));

	return CS_OK;
}
#endif /* CS_IPC_ENABLED */

/* Internal functions*/
extern unsigned int calc_crc(u32 crc, u8 const *p, u32 len);

cs_status_t construct_flow_entries(
	CS_IN	cs_rtp_entry_t *p_rtp_node,
	CS_IN	cs_flow_t *p_flow_entry)
{
	int crc32;
	cs_rtp_dir_t dir = p_rtp_node->dir;
	cs_flow_t *flow_entry_from_pe = &p_rtp_node->flow_entry_from_pe;
	cs_flow_t *flow_entry_to_pe = &p_rtp_node->flow_entry_to_pe;

	if (p_rtp_node == NULL || p_flow_entry == NULL) {
		ERR(printk("%s:%d p_rtp NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if(dir != CS_RTP_FROMLAN && dir != CS_RTP_FROMWAN){
		ERR(printk("%s:%d invalid direction %u\n",
			__func__, __LINE__, dir));
		return CS_E_NULL_PTR;
	}

	/* Based on p_flow_entry input by upper layer, 2 flow hashes will be created.
		1. from PE->WAN(LAN) 2. from WAN(LAN) to PE */
	memcpy(flow_entry_from_pe, p_flow_entry,
					sizeof(cs_flow_t));

	memcpy(flow_entry_to_pe, p_flow_entry,
					sizeof(cs_flow_t));

	/* To PE. not to modify packet content. Only modify sa_mac*/
	memcpy(&flow_entry_to_pe->egress_pkt, &p_flow_entry->ingress_pkt,
				sizeof(cs_pkt_info_t));

	/* flow Hash from PE to WAN(LAN) direction */
	memcpy(flow_entry_from_pe->ingress_pkt.sa_mac, p_flow_entry->egress_pkt.sa_mac,
		CS_ETH_ADDR_LEN);

	if(p_flow_entry->dec_ttl != 0)
		flow_entry_to_pe->dec_ttl = 0;

	/* flow Hash to PE direction: Modify SA MAC and insert information for PE*/
	flow_entry_to_pe->egress_pkt.sa_mac[1] = CS_RTP & 0xFF;
	flow_entry_to_pe->egress_pkt.sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	flow_entry_to_pe->egress_pkt.sa_mac[3] = p_rtp_node->sa_id & 0xFF;
	/* DIR: 1 byte.
	0: WAN to LAN
	1: LAN to WAN */
	flow_entry_to_pe->egress_pkt.sa_mac[4] = (p_rtp_node->dir == CS_RTP_FROMLAN) ? 1 : 0;
	flow_entry_to_pe->egress_pkt.sa_mac[5] = 0; /* reserved */
	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &flow_entry_to_pe->egress_pkt.sa_mac[1], 5));
	/* Store 8 bits of the crc in the src MAC */
	flow_entry_to_pe->egress_pkt.sa_mac[0] = crc32 & 0xFF;

	/* To PE direction: Remove pppoe header if any */
	if (p_flow_entry->ingress_pkt.pppoe_session_id_valid != 0
		&& p_flow_entry->egress_pkt.pppoe_session_id_valid == 0) {
		flow_entry_to_pe->egress_pkt.pppoe_session_id_valid = 0;
		flow_entry_to_pe->egress_pkt.pppoe_session_id = 0;
		flow_entry_from_pe->ingress_pkt.eth_type = p_flow_entry->egress_pkt.eth_type;
		flow_entry_from_pe->ingress_pkt.pppoe_session_id_valid = 0;
		flow_entry_from_pe->ingress_pkt.pppoe_session_id = 0;
	}

 	/* Change fwd result voq */
	if(p_rtp_node->dir == CS_RTP_FROMLAN) {
		/* UPSTREAM: PE1 port = 4 */
		flow_entry_to_pe->egress_pkt.phy_port = CS_PORT_OFLD1;
	}
	else if((p_rtp_node->dir == CS_RTP_FROMWAN)) {
		/* DOWNSTREAM: PE0 port = 3 */
		flow_entry_to_pe->egress_pkt.phy_port = CS_PORT_OFLD0;
	}

	return CS_OK;
}

cs_status_t cs_rtp_entry_get_by_sa_id(
	CS_IN	cs_uint16_t		sa_id,
	CS_IN	cs_sa_id_direction_t	dir,
	CS_OUT	cs_rtp_entry_t  	**p_rtp)
{
	cs_rtp_entry_t *t;

	DBG(printk("%s:%d sa_id = %d, dir = %d, p_rtp = 0x%p\n",
		__func__, __LINE__, sa_id, dir, p_rtp));

	if (p_rtp == NULL) {
		ERR(printk("%s:%d p_rtp NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (dir != DOWN_STREAM && dir != UP_STREAM) {
		ERR(printk("%s:%d invalid direction %u\n",
			__func__, __LINE__, dir));
		return CS_E_DIR;
	}

	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		/* search by sa_id and direciton */
		if ((t->sa_id == sa_id) && (t->dir == dir)) {
			*p_rtp = t;
			return CS_OK;
		}

		t = t->next;
	}
	return CS_E_NOT_FOUND;
}

cs_status_t cs_rtp_translate_sa_id_get_by_rtp_id(
	CS_IN	cs_rtp_id_t	rtp_id,
	CS_OUT	cs_uint16_t	*sa_id)
{
	cs_rtp_entry_t *t;

	DBG(printk("%s:%d rtp_id = %d sa_id = 0x%p\n",
		__func__, __LINE__, rtp_id, sa_id));

	if (sa_id == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
			return CS_E_NULL_PTR;
	}

	t = cs_rtp_proxy_cb.rtp_h;
	while (t) {
		/* search by rtp_id */
		if (t->rtp_id == rtp_id) {
			*sa_id = t->sa_id;
			return CS_OK;
		}

		t = t->next;
	}
	return CS_E_NOT_FOUND;
}

void cs_hw_accel_rtp_proxy_init(void)
{
	cs_uint8_t i = 0, j = 0;
	memset(&cs_rtp_proxy_cb, 0, sizeof(cs_rtp_proxy_cb_t));
	cs_rtp_proxy_cb.curr_rtp_id = CS_RTP_ID_BASE;

	/* Set ETH0 ETH1 ETH2 PE0 PE1 all voq as SP voq
	 * SCH_MAX_ETH_PORT = 5
	 * CS_QOS_VOQ_PER_PORT = 8
	 */
	for(i = 0; i < SCH_MAX_ETH_PORT; i++){
		for (j = 0; j < CS_QOS_VOQ_PER_PORT; j++){
			cs752x_sch_set_queue_sp(i, j, 0);
		}
	}
}

void cs_hw_accel_rtp_proxy_exit(void)
{
}
