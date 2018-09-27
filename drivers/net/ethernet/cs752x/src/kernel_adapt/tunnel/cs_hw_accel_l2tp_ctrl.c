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
 * cs_hw_accel_l2tp_ctrl.c
 *
 * $Id$
 *
 * This file contains control plane for CS L2TP data plane APIs.
 */

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/esp.h>
#include <net/ah.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/crypto.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include <mach/cs_route_api.h>
#include <mach/cs_rule_hash_api.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include "cs_fe.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_hw_accel_sa_id.h"
#include <mach/cs_flow_api.h>
#include "cs_core_hmu.h"
#include <linux/socket.h>
#include <linux/l2tp.h>
#include <linux/in.h>
#include <linux/ppp_defs.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include "l2tp_core.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#else
#define DBG(x)	{ }
#endif /* CONFIG_CS752X_PROC */

#define SKIP(x) { }
#define ERR(x)	(x)

extern struct net_device *ni_get_device(unsigned char port_id);

int cs_l2tp_ctrl_enable(void)
{
	if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_L2TP_CTRL) > 0)
		return TRUE;
	else
		return FALSE;
}

static cs_status_t cs_l2tp_nexthop_construct(struct sk_buff *skb,
	cs_tunnel_dir_t dir, cs_l3_nexthop_t *n)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	/* Only MAC address and IP address should be network order.
	   Other fields are all host order */
	n->nhid.nhop_type = CS_L3_NEXTHOP_DIRECT;

	if (dir == CS_TUNNEL_DIR_OUTBOUND) {
		/* WAN ifindex */
		n->nhid.intf_id = skb->dev->ifindex;
		
		/* L3 IP */
		if (cs_cb->output.l3_nh.iph.ver == 4) {
			n->nhid.addr.afi = CS_IPV4;
			n->nhid.addr.ip_addr.ipv4_addr = cs_cb->output.l3_nh.iph.dip;
			n->nhid.addr.addr_len = 32;
		} else if (cs_cb->output.l3_nh.iph.ver == 6) {
			n->nhid.addr.afi = CS_IPV6;
			n->nhid.addr.ip_addr.ipv6_addr[0] = cs_cb->output.l3_nh.ipv6h.dip[0];
			n->nhid.addr.ip_addr.ipv6_addr[1] = cs_cb->output.l3_nh.ipv6h.dip[1];
			n->nhid.addr.ip_addr.ipv6_addr[2] = cs_cb->output.l3_nh.ipv6h.dip[2];
			n->nhid.addr.ip_addr.ipv6_addr[3] = cs_cb->output.l3_nh.ipv6h.dip[3];
			n->nhid.addr.addr_len = 128;
		} else {
			printk("%s:%d: Unsupported IP ver %d\n",
				__func__, __LINE__, cs_cb->output.l3_nh.iph.ver);
			return CS_E_NOT_SUPPORT;
		}

		/* DA MAC */
		memcpy(n->da_mac, cs_cb->output.raw.da, CS_ETH_ADDR_LEN);

		/* port ID */
		n->id.port_id = cs_cb->common.egress_port_id;


		/* port encap */
		if (cs_cb->common.module_mask & CS_MOD_MASK_PPPOE) {
			n->encap.port_encap.type = CS_PORT_ENCAP_PPPOE_E;

			n->encap.port_encap.port_encap.pppoe.pppoe_session_id =
				ntohs(cs_cb->output.raw.pppoe_frame);

			n->encap.port_encap.port_encap.pppoe.tag[0] = 
				cs_cb->output.raw.vlan_tci & VLAN_VID_MASK;

			n->encap.port_encap.port_encap.pppoe.tag[1] = 
				cs_cb->output.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.pppoe.dest_mac,
				cs_cb->output.raw.da, CS_ETH_ADDR_LEN);
			memcpy(n->encap.port_encap.port_encap.pppoe.src_mac,
				cs_cb->output.raw.sa, CS_ETH_ADDR_LEN);
				
		} else {
			if (cs_cb->output.raw.vlan_tci_2 > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_QinQ_E;
			else if (cs_cb->output.raw.vlan_tci > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_1Q_E;
			else
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_E;
			
			n->encap.port_encap.port_encap.eth.tag[0] =
				cs_cb->output.raw.vlan_tci & VLAN_VID_MASK;
			n->encap.port_encap.port_encap.eth.tag[1] =
				cs_cb->output.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.eth.src_mac,
				cs_cb->output.raw.sa, CS_ETH_ADDR_LEN);
		}
	} else if (dir == CS_TUNNEL_DIR_INBOUND) {
		/* WAN ifindex */
		n->nhid.intf_id = cs_cb->common.in_ifindex;
		
		/* L3 IP */
		if (cs_cb->input.l3_nh.iph.ver == 4) {
			n->nhid.addr.afi = CS_IPV4;
			n->nhid.addr.ip_addr.ipv4_addr = cs_cb->input.l3_nh.iph.sip;
			n->nhid.addr.addr_len = 32;
		} else if (cs_cb->input.l3_nh.iph.ver == 6) {
			n->nhid.addr.afi = CS_IPV6;
			n->nhid.addr.ip_addr.ipv6_addr[0] = cs_cb->input.l3_nh.ipv6h.sip[0];
			n->nhid.addr.ip_addr.ipv6_addr[1] = cs_cb->input.l3_nh.ipv6h.sip[1];
			n->nhid.addr.ip_addr.ipv6_addr[2] = cs_cb->input.l3_nh.ipv6h.sip[2];
			n->nhid.addr.ip_addr.ipv6_addr[3] = cs_cb->input.l3_nh.ipv6h.sip[3];
			n->nhid.addr.addr_len = 128;
		} else {
			printk("%s:%d: Unsupported IP ver %d\n",
				__func__, __LINE__, cs_cb->input.l3_nh.iph.ver);
			return CS_E_NOT_SUPPORT;
		}

		/* DA MAC */
		memcpy(n->da_mac, cs_cb->input.raw.sa, CS_ETH_ADDR_LEN);

		/* port ID */
		n->id.port_id = cs_cb->common.ingress_port_id;


		/* port encap */
		if (cs_cb->common.module_mask & CS_MOD_MASK_PPPOE) {
			n->encap.port_encap.type = CS_PORT_ENCAP_PPPOE_E;

			n->encap.port_encap.port_encap.pppoe.pppoe_session_id =
				ntohs(cs_cb->input.raw.pppoe_frame);

			n->encap.port_encap.port_encap.pppoe.tag[0] = 
				cs_cb->input.raw.vlan_tci & VLAN_VID_MASK;

			n->encap.port_encap.port_encap.pppoe.tag[1] = 
				cs_cb->input.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.pppoe.dest_mac,
				cs_cb->input.raw.sa, CS_ETH_ADDR_LEN);
			memcpy(n->encap.port_encap.port_encap.pppoe.src_mac,
				cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
				
		} else {
			if (cs_cb->input.raw.vlan_tci_2 > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_QinQ_E;
			else if (cs_cb->input.raw.vlan_tci > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_1Q_E;
			else
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_E;
			
			n->encap.port_encap.port_encap.eth.tag[0] =
				cs_cb->input.raw.vlan_tci & VLAN_VID_MASK;
			n->encap.port_encap.port_encap.eth.tag[1] =
				cs_cb->input.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.eth.src_mac,
				cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
		}
	} else {
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return CS_E_NOT_SUPPORT;
	}

	return CS_OK;
}

static cs_status_t cs_l2tp_t_cfg_construct(
	cs_kernel_accel_cb_t				*cs_cb,
	cs_tunnel_type_t				type,
	cs_tunnel_dir_t					dir,
	cs_uint32_t					nexthop_id,
	cs_uint32_t					policy_handle,
	struct l2tp_session				*session,
	cs_tunnel_cfg_t					*t_cfg)
{
	struct cb_network_field *cb_net;
	struct l2tp_tunnel *tunnel = session->tunnel;
	int i;

	t_cfg->type = type;
	t_cfg->tx_port = 0;
	t_cfg->nexthop_id = nexthop_id;
	t_cfg->dir = dir;
	t_cfg->tunnel.l2tp.ipsec_policy = policy_handle;
	t_cfg->tunnel.l2tp.ver = tunnel->version | 0x4000;
	
	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		cb_net = &cs_cb->output;
		t_cfg->tunnel.l2tp.tid = tunnel->peer_tunnel_id;
		break;
	case CS_TUNNEL_DIR_INBOUND:
		cb_net = &cs_cb->input;
		t_cfg->tunnel.l2tp.tid = tunnel->tunnel_id;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return CS_E_NOT_SUPPORT;
	}

	DBG(printk("%s:%d dir %d, tid 0x%x\n",
		__func__, __LINE__, dir, t_cfg->tunnel.l2tp.tid));

	if ((t_cfg->tunnel.l2tp.ver & 0xF) == 3) {
		/* L2TPv3 */
		t_cfg->tunnel.l2tp.session_id = 
			(dir == CS_TUNNEL_DIR_OUTBOUND) ? 
				ntohs(session->peer_session_id) : 
				ntohs(session->session_id);
		t_cfg->tunnel.l2tp.encap_type = (cs_uint16_t) tunnel->encap;
		t_cfg->tunnel.l2tp.l2specific_len = session->l2specific_len;
		t_cfg->tunnel.l2tp.l2specific_type = session->l2specific_type;
		t_cfg->tunnel.l2tp.send_seq = session->send_seq;
		t_cfg->tunnel.l2tp.ns = session->ns;
		t_cfg->tunnel.l2tp.cookie_len = session->cookie_len;
		for (i = 0; i < 8; i++)
			t_cfg->tunnel.l2tp.cookie[i] = session->cookie[i];
		t_cfg->tunnel.l2tp.offset = session->offset;
		
		for (i = 0; i < ETH_ALEN; i++) {
			t_cfg->tunnel.l2tp.l2tp_src_mac[i] = cs_cb->common.vpn_sa[i];
			t_cfg->tunnel.l2tp.peer_l2tp_src_mac[i] = cs_cb->common.vpn_da[i];
		}
	}
	

	/* IP address should be network order */
	if (cb_net->l3_nh.iph.ver == 4) {
		t_cfg->dest_addr.afi = CS_IPV4;
		t_cfg->dest_addr.ip_addr.ipv4_addr = cb_net->l3_nh.iph.dip;
		t_cfg->dest_addr.addr_len = 32;
	
		t_cfg->src_addr.afi = CS_IPV4;
		t_cfg->src_addr.ip_addr.ipv4_addr = cb_net->l3_nh.iph.sip;
		t_cfg->src_addr.addr_len = 32;
	} else if (cb_net->l3_nh.iph.ver == 6) {
		t_cfg->dest_addr.afi = CS_IPV6;
		t_cfg->dest_addr.ip_addr.ipv6_addr[0] = cb_net->l3_nh.ipv6h.dip[0];
		t_cfg->dest_addr.ip_addr.ipv6_addr[1] = cb_net->l3_nh.ipv6h.dip[1];
		t_cfg->dest_addr.ip_addr.ipv6_addr[2] = cb_net->l3_nh.ipv6h.dip[2];
		t_cfg->dest_addr.ip_addr.ipv6_addr[3] = cb_net->l3_nh.ipv6h.dip[3];
		t_cfg->dest_addr.addr_len = 128;
	
		t_cfg->src_addr.afi = CS_IPV6;
		t_cfg->src_addr.ip_addr.ipv6_addr[0] = cb_net->l3_nh.ipv6h.sip[0];
		t_cfg->src_addr.ip_addr.ipv6_addr[1] = cb_net->l3_nh.ipv6h.sip[1];
		t_cfg->src_addr.ip_addr.ipv6_addr[2] = cb_net->l3_nh.ipv6h.sip[2];
		t_cfg->src_addr.ip_addr.ipv6_addr[3] = cb_net->l3_nh.ipv6h.sip[3];
		t_cfg->src_addr.addr_len = 128;
	} else {
		printk("%s:%d: Unsupported IP ver %d\n",
			__func__, __LINE__, cb_net->l3_nh.iph.ver);
		return CS_E_NOT_SUPPORT;
	}

	/* L4 UDP ports */	
	t_cfg->tunnel.l2tp.dest_port = ntohs(cb_net->l4_h.uh.dport);
	t_cfg->tunnel.l2tp.src_port = ntohs(cb_net->l4_h.uh.sport);

	return CS_OK;
}

static void cs_l2tp_insert_mac_hdr_on_skb(
	struct sk_buff					*skb,
	cs_tunnel_entry_t 				*t)
{
	struct ethhdr *p_eth;
	struct iphdr *p_ip = (struct iphdr *)skb->data;
	struct net_device *dev;

	/* Current skb is an IP packet without proper L2 info.
	 * To be able to send it to L2TP Offload Engine,
	 * we need to construct a L2 ETH header.
	 */
	skb_push(skb, ETH_HLEN);

	/* (new skb->data) DA(6):SA(6):Ethertype(2): (old skb->data) */
	p_eth = (struct ethhdr *)skb->data;

	if (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND)
		dev = ni_get_device(CS_PORT_GMAC1);
	else
		dev = ni_get_device(CS_PORT_GMAC0);
		
	if (dev) {
		DBG(printk("%s:%d: new DA MAC = %pM\n",
			__func__, __LINE__, dev->dev_addr));
		memcpy(p_eth->h_dest, dev->dev_addr, ETH_HLEN);
	}

	if (p_ip->version == 4)
		p_eth->h_proto = __constant_htons(ETH_P_IP);
	else if (p_ip->version == 6)
		p_eth->h_proto = __constant_htons(ETH_P_IPV6);

	/* move the MAC header to current skb->data */
	skb_reset_mac_header(skb);

}

static void cs_l2tp_tx_skb_to_pe(
	struct sk_buff					*skb,
	cs_tunnel_entry_t 				*t)
{
	int ret;
	int len;
	cs_tunnel_dir_t dir;


	/* if this skb is hijacked from kernel, the new ip header has been
	 * inserted, so we will have to remove it! */

	if (skb == NULL || t == NULL)
		return;
	dir = t->tunnel_cfg.dir;

	
	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		/* PPP header: FF 03 00 21 */
		skb_pull(skb, PPP_HDRLEN);
		break;
	case CS_TUNNEL_DIR_INBOUND:
		/* PPP header: 00 21 */
		skb_pull(skb, 2);
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return;
	}
	/* Now skb->data points to inner IP header */


	SKIP(printk("%s:%d: dir = %d, data = 0x%p, len = %d, network ptr = 0x%p,"
		" network hdr len = %d, mac ptr = 0x%p, mac_len = %d\n",
		__func__, __LINE__,
		dir,
		skb->data, skb->len,
		skb_network_header(skb), skb_network_header_len(skb),
		skb_mac_header(skb), skb->mac_len));
	SKIP(cs_dump_data(skb->data,skb->len));

	/* convert the packet to an ethernet packet and send it to PE */
	cs_l2tp_insert_mac_hdr_on_skb(skb, t);

	len = skb->len;
	
	ret = cs_tunnel_tx_frame_to_pe(skb_mac_header(skb), len,
			t->tunnel_id, t->tunnel_cfg.type, t->tunnel_cfg.dir);

	if (ret != CS_OK) {
		ERR(printk("%s:%d: can't send pkt to PE\n", __func__, __LINE__));
	}
	return;
}

static void cs_l2tp_lan2pe_flow_hash_add(
	cs_kernel_accel_cb_t 				*cs_cb,
	cs_tunnel_entry_t 				*t)
{
	cs_flow_t lan2pe_flow;
	cs_pkt_info_t *in_pkt, *out_pkt;
	int crc32;
	int ret;

	/* 1. Redirect packets to PE#1.
	 * 2. Replace SA MAC with extra information.
	 * 3. Keep other fields, including VLAN
	 */
	/* Only MAC address and IP address should be network order.
	   Other fields are all host order */
	memset(&lan2pe_flow, 0, sizeof(cs_flow_t));
	in_pkt = &lan2pe_flow.ingress_pkt;
	out_pkt = &lan2pe_flow.egress_pkt;
	
	/* 1. construct lan2pe_flow */
	lan2pe_flow.flow_type = CS_FLOW_TYPE_L4;

	/* L2 */
	/* CS_HM_MAC_DA_MASK | CS_HM_MAC_SA_MASK | CS_HM_ETHERTYPE_MASK */
	memcpy(in_pkt->sa_mac, cs_cb->input.raw.sa, CS_ETH_ADDR_LEN);
	memcpy(in_pkt->da_mac, cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
	in_pkt->eth_type = ntohs(cs_cb->input.raw.eth_protocol);

	/* VLAN */
	/* CS_HM_VID_1_MASK | CS_HM_VID_2_MASK | 
	   CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK | 
	   CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK | 
	   CS_HM_8021P_1_MASK | CS_HM_8021P_2_MASK |
	   CS_HM_DEI_1_MASK | CS_HM_DEI_2_MASK
	 */
	switch (cs_cb->input.raw.vlan_tpid) {
	case ETH_P_8021Q:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[0].vlan_id = cs_cb->input.raw.vlan_tci & 0xFFF;

	switch (cs_cb->input.raw.vlan_tpid_2) {
	case ETH_P_8021Q:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[1].vlan_id = cs_cb->input.raw.vlan_tci_2 & 0xFFF;

	/* PPPoE */
	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	in_pkt->pppoe_session_id_valid = cs_cb->input.raw.pppoe_frame_vld;
	in_pkt->pppoe_session_id = ntohs(cs_cb->input.raw.pppoe_frame);

	/* IP */
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK | CS_HM_IP_PROT_MASK | 
	   CS_HM_IP_VER_MASK | CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |
	   CS_HM_DSCP_MASK  | CS_HM_IP_FRAGMENT_MASK | 
	   CS_HM_IPV6_RH_MASK | CS_HM_IPV6_DOH_MASK | CS_HM_IPV6_NDP_MASK | 
	   CS_HM_IPV6_HBH_MASK
	 */
	if (cs_cb->input.l3_nh.iph.ver == 6) {
		in_pkt->sa_ip.afi = CS_IPV6;
		in_pkt->sa_ip.ip_addr.addr[0] = cs_cb->input.l3_nh.ipv6h.sip[0];
		in_pkt->sa_ip.ip_addr.addr[1] = cs_cb->input.l3_nh.ipv6h.sip[1];
		in_pkt->sa_ip.ip_addr.addr[2] = cs_cb->input.l3_nh.ipv6h.sip[2];
		in_pkt->sa_ip.ip_addr.addr[3] = cs_cb->input.l3_nh.ipv6h.sip[3];

		in_pkt->da_ip.ip_addr.addr[0] = cs_cb->input.l3_nh.ipv6h.dip[0];
		in_pkt->da_ip.ip_addr.addr[1] = cs_cb->input.l3_nh.ipv6h.dip[1];
		in_pkt->da_ip.ip_addr.addr[2] = cs_cb->input.l3_nh.ipv6h.dip[2];
		in_pkt->da_ip.ip_addr.addr[3] = cs_cb->input.l3_nh.ipv6h.dip[3];
	} else {
		in_pkt->sa_ip.afi = CS_IPV4;
		in_pkt->sa_ip.ip_addr.ipv4_addr = cs_cb->input.l3_nh.iph.sip;
		in_pkt->da_ip.ip_addr.ipv4_addr = cs_cb->input.l3_nh.iph.dip;
	}
	
	in_pkt->protocol = cs_cb->input.l3_nh.iph.protocol;
	in_pkt->tos = cs_cb->input.l3_nh.iph.tos;
	
	/* L4 */
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK | 
	   CS_HM_TCP_CTRL_MASK |
	   CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK
	 */
	switch (in_pkt->protocol) {
	case IPPROTO_TCP:
		in_pkt->l4_header.tcp.dport = ntohs(cs_cb->input.l4_h.th.dport); 
		in_pkt->l4_header.tcp.sport = ntohs(cs_cb->input.l4_h.th.sport);
		break;
	case IPPROTO_UDP:
		in_pkt->l4_header.udp.dport = ntohs(cs_cb->input.l4_h.uh.dport);
		in_pkt->l4_header.udp.sport = ntohs(cs_cb->input.l4_h.uh.sport);
		break;
	default:
		DBG(printk("%s:%d: unexpected protocol = %d\n",
			__func__, __LINE__, in_pkt->protocol));
		return;
	}

	/* set egress packet contents */
	memcpy(out_pkt, in_pkt, sizeof(cs_pkt_info_t));

	/* physical port */
	in_pkt->phy_port = cs_cb->common.ingress_port_id;
	out_pkt->phy_port = CS_PORT_OFLD1;

	/* fill the source MAC with the host order */
	out_pkt->sa_mac[1] = t->tunnel_cfg.type & 0xFF;
	out_pkt->sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	out_pkt->sa_mac[3] = t->sa_id & 0xFF;
	out_pkt->sa_mac[4] = (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND) ? 0 : 1;
	out_pkt->sa_mac[5] = 0; /* reserved */


	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &out_pkt->sa_mac[1], 5));
	/* Store 8 bits of the crc in the src MAC */
	out_pkt->sa_mac[0] = crc32 & 0xFF;

	/* set sw_id */
	if (cs_cb->input.l3_nh.iph.ver == 6) {
		lan2pe_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD];
	} else {
		lan2pe_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD];
	}

	lan2pe_flow.swid_array[CS_FLOW_SWID_VPN] = t->sw_id;
	
	/* 2. create flow hash */
	ret = cs_flow_add(t->device_id, &lan2pe_flow);
	DBG(printk("%s:%d: type %d, dir %d, sa_id %d, sw_id = 0x%0llx\n",
		__func__, __LINE__,
		t->tunnel_cfg.type, t->tunnel_cfg.dir, t->sa_id,t->sw_id));

	if (ret != CS_OK) {
		DBG(printk("%s:%d: cs_flow_add ret = %d\n",
			__func__, __LINE__, ret));
	}
		
	return;
}

static void cs_l2tp_pe2lan_flow_hash_add(
	cs_kernel_accel_cb_t 				*cs_cb,
	cs_tunnel_entry_t 				*t)
{
	cs_flow_t pe2lan_flow;
	cs_pkt_info_t *in_pkt, *out_pkt;
	int crc32;
	int ret;

	/* 1. WAN2PE hash will remove PPPoE header if it exists,
	 *    and we don't take care PPPoE header here.
	 * 2. SRC MAC will be modified by WAN2PE hash, but PE will keep it.
	 *    Therefore, we need to set the modified SRC MAC as key.
	 * 3. Other L2 and VLAN header will not modified by WAN2PE or PE.
	 *    Therefore, we also need to handle it.
	 */
	/* Only MAC address and IP address should be network order.
	   Other fields are all host order */
	memset(&pe2lan_flow, 0, sizeof(cs_flow_t));
	in_pkt = &pe2lan_flow.ingress_pkt;
	out_pkt = &pe2lan_flow.egress_pkt;
	
	/* 1. construct pe2lan_flow */
	pe2lan_flow.flow_type = CS_FLOW_TYPE_L4;

	/* L2 */
	/* CS_HM_MAC_DA_MASK | CS_HM_MAC_SA_MASK | CS_HM_ETHERTYPE_MASK */

	/* fill the source MAC with the host order */
	in_pkt->sa_mac[1] = t->tunnel_cfg.type & 0xFF;
	in_pkt->sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	in_pkt->sa_mac[3] = t->sa_id & 0xFF;
	in_pkt->sa_mac[4] = (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND) ? 0 : 1;
	in_pkt->sa_mac[5] = 0; /* reserved */

	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &in_pkt->sa_mac[1], 5));
	/* Store 8 bits of the crc in the src MAC */
	in_pkt->sa_mac[0] = crc32 & 0xFF;

	memcpy(in_pkt->da_mac, cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
	in_pkt->eth_type = ntohs(cs_cb->input.raw.eth_protocol);

	/* VLAN */
	/* CS_HM_VID_1_MASK | CS_HM_VID_2_MASK | 
	   CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK | 
	   CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK | 
	   CS_HM_8021P_1_MASK | CS_HM_8021P_2_MASK |
	   CS_HM_DEI_1_MASK | CS_HM_DEI_2_MASK
	 */
	switch (cs_cb->input.raw.vlan_tpid) {
	case ETH_P_8021Q:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[0].vlan_id = cs_cb->input.raw.vlan_tci & 0xFFF;

	switch (cs_cb->input.raw.vlan_tpid_2) {
	case ETH_P_8021Q:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[1].vlan_id = cs_cb->input.raw.vlan_tci_2 & 0xFFF;

	/* PPPoE */
	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	/* in_pkt->pppoe_session_id_valid =  0; */
	/* in_pkt->pppoe_session_id = 0; */

	/* IP */
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK | CS_HM_IP_PROT_MASK | 
	   CS_HM_IP_VER_MASK | CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |
	   CS_HM_DSCP_MASK  | CS_HM_IP_FRAGMENT_MASK | 
	   CS_HM_IPV6_RH_MASK | CS_HM_IPV6_DOH_MASK | CS_HM_IPV6_NDP_MASK | 
	   CS_HM_IPV6_HBH_MASK
	 */
	if (cs_cb->output.l3_nh.iph.ver == 6) {
		in_pkt->sa_ip.afi = CS_IPV6;
		in_pkt->sa_ip.ip_addr.addr[0] = cs_cb->output.l3_nh.ipv6h.sip[0];
		in_pkt->sa_ip.ip_addr.addr[1] = cs_cb->output.l3_nh.ipv6h.sip[1];
		in_pkt->sa_ip.ip_addr.addr[2] = cs_cb->output.l3_nh.ipv6h.sip[2];
		in_pkt->sa_ip.ip_addr.addr[3] = cs_cb->output.l3_nh.ipv6h.sip[3];

		in_pkt->da_ip.ip_addr.addr[0] = cs_cb->output.l3_nh.ipv6h.dip[0];
		in_pkt->da_ip.ip_addr.addr[1] = cs_cb->output.l3_nh.ipv6h.dip[1];
		in_pkt->da_ip.ip_addr.addr[2] = cs_cb->output.l3_nh.ipv6h.dip[2];
		in_pkt->da_ip.ip_addr.addr[3] = cs_cb->output.l3_nh.ipv6h.dip[3];	
	} else {
		in_pkt->sa_ip.afi = CS_IPV4;
		in_pkt->sa_ip.ip_addr.ipv4_addr = cs_cb->output.l3_nh.iph.sip;
		in_pkt->da_ip.ip_addr.ipv4_addr = cs_cb->output.l3_nh.iph.dip;
	}
	
	in_pkt->protocol = cs_cb->output.l3_nh.iph.protocol;
	in_pkt->tos = cs_cb->output.l3_nh.iph.tos;
	
	/* L4 */
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK | 
	   CS_HM_TCP_CTRL_MASK |
	   CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK
	 */

	switch (in_pkt->protocol) {
	case IPPROTO_TCP:
		in_pkt->l4_header.tcp.dport = ntohs(cs_cb->output.l4_h.th.dport); 
		in_pkt->l4_header.tcp.sport = ntohs(cs_cb->output.l4_h.th.sport);
		break;
	case IPPROTO_UDP:
		in_pkt->l4_header.udp.dport = ntohs(cs_cb->output.l4_h.uh.dport);
		in_pkt->l4_header.udp.sport = ntohs(cs_cb->output.l4_h.uh.sport);
		break;
	default:
		DBG(printk("%s:%d: unexpected protocol = %d\n",
			__func__, __LINE__, in_pkt->protocol));
		return;
	}

	/* set egress packet contents */
	memcpy(out_pkt, in_pkt, sizeof(cs_pkt_info_t));

	/* L2 */
	memcpy(out_pkt->sa_mac, cs_cb->output.raw.sa, CS_ETH_ADDR_LEN);
	memcpy(out_pkt->da_mac, cs_cb->output.raw.da, CS_ETH_ADDR_LEN);
	/* out_pkt->eth_type = cs_cb->output.raw.eth_protocol; */

	/* VLAN */
	switch (cs_cb->output.raw.vlan_tpid) {
	case ETH_P_8021Q:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	out_pkt->tag[0].vlan_id = cs_cb->output.raw.vlan_tci & 0xFFF;

	switch (cs_cb->output.raw.vlan_tpid_2) {
	case ETH_P_8021Q:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	out_pkt->tag[1].vlan_id = cs_cb->output.raw.vlan_tci_2 & 0xFFF;

	/* physical port */
	in_pkt->phy_port = CS_PORT_OFLD0;
	out_pkt->phy_port = cs_cb->common.egress_port_id;

	/* set sw_id */
	if (cs_cb->output.l3_nh.iph.ver == 6) {
		pe2lan_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD];
	} else {
		pe2lan_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD];
	}

	pe2lan_flow.swid_array[CS_FLOW_SWID_VPN] = t->sw_id;
	DBG(printk("%s:%d: type %d, dir %d, sa_id %d, sw_id = 0x%0llx\n",
		__func__, __LINE__,
		t->tunnel_cfg.type, t->tunnel_cfg.dir, t->sa_id,t->sw_id));
	
	/* 2. create flow hash */
	ret = cs_flow_add(t->device_id, &pe2lan_flow);
	if (ret != CS_OK) {
		DBG(printk("%s:%d: cs_flow_add ret = %d\n",
			__func__, __LINE__, ret));
	}

	return;
}


/************************ main handlers *********************/
/* this function returns 0 when it is ok for Kernel to continue
 * its original task.  CS_DONE means Kernel does not have to
 * handle anymore.
 * parameters:
 *	skb
 *	dir:		0: outbound, 1: inbound
 * return value:
 *	0		Linux original path
 *	1 (CS_DONE)	free skb
 */
int cs_l2tp_ctrl(struct sk_buff *skb, struct l2tp_session *s, unsigned char dir)
{
	int ret;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_tunnel_state_t state = CS_TUNNEL_INVALID;
	cs_tunnel_entry_t *t, t_node;

	struct l2tp_tunnel *tunnel = s->tunnel;
	int i;
	cs_uint32_t tid = 0, sid = 0;
	cs_uint32_t ppp_hdr_4;
	cs_uint16_t ppp_hdr_2;

	/* skip PPP control messages and they will go through SW path */
	if ((cs_cb == NULL) || (cs_cb->common.sw_only == CS_SWONLY_STATE))
		return 0;

	DBG(printk("%s:%d: skb 0x%p, len %d, dir %d, skb->sp 0x%p,"
		" cs_cb 0x%p, mode %d, enable %d\n",
		__func__, __LINE__,
		skb, skb->len, dir, skb->sp, cs_cb,
		cs_cb ? cs_cb->common.sw_only : 0xFF,
		cs_l2tp_ctrl_enable()));

	if (cs_l2tp_ctrl_enable() == FALSE)
		return 0;
	t = &t_node;
	/*
	 * For inbound L2TP, both control and data packets have cs_cb.
	 * skb->data points to L2TP header.
	 * skb->network_header points to the outer IP header.
	 * skb->mac_header points to ether header.
	 * We should change skb->data and skb->len to correct value before
	 * sending it to PE.
	 */
	/* now I change the hook pointer to let skb->data point to 
	 * PPP header (v2) or L2TP ether header (v3)
	 */

	/*
	 * For outbound L2TP, only data packets have cs_cb.
	 * skb->data points to PPP header.
	 * skb->network_header points to the original IP header.
	 * skb->mac_header points to ether header, but the content is partial
	 * overwritten by PPP header.
	 * We should modify it before sending it to PE.
	 * 1. change skb->data and skb->len to correct value.
	 * 2. recover ether header.
	 */
	
	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		break;
	case CS_TUNNEL_DIR_INBOUND:
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return 0;
	}

	/* Linux sometimes routes a inbound packet to outbound tunnel
	 * for unknown reason. We need to avoid creating wrong hashes.
	 */
	if ((dir == CS_TUNNEL_DIR_OUTBOUND) &&
			(cs_cb->common.ingress_port_id == GE_PORT0 ||
			cs_cb->common.ingress_port_id == PE0_PORT)) {
		SKIP(printk("%s:%d: port %d loops back to WAN port\n",
			__func__, __LINE__, cs_cb->common.ingress_port_id));
		return CS_DONE; /* free skb */
	}

	if (tunnel->version == 2 /* L2TPv2 */) {
		ppp_hdr_4 = get_unaligned_be32(skb->data);
		ppp_hdr_2 = get_unaligned_be16(skb->data);
		if (dir == CS_TUNNEL_DIR_OUTBOUND &&
				skb_network_offset(skb) == PPP_HDRLEN &&
				(ppp_hdr_4 == 0xFF030021 ||
				 ppp_hdr_4 == 0xFF030057)) {
			/* PPP header: FF 03 00 21 (IPv4)
			 *             FF 03 00 57 (IPv6)
			 */
			
			tid = tunnel->peer_tunnel_id;
			sid = s->peer_session_id;

		} else if (dir == CS_TUNNEL_DIR_INBOUND &&
			(ppp_hdr_2 == 0x0021 || ppp_hdr_2 == 0x0057)) {
			/* PPP header: 00 21 (IPv4)
			 *             00 57 (IPv6)
			 */

			tid = tunnel->tunnel_id;
			sid = s->session_id;
			
		} else {
			
			/* PPP control packet or others */
			/* PPP header: FF 03 C0 21 */
			SKIP(printk("%s:%d: Only PPP data packet is supported."
				" Len = %d\n",
				__func__, __LINE__, skb->len));
			SKIP(cs_dump_data(skb->data, 4));
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return 0;
		}
	} else {
		/* L2TPv3 */
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return 0;
	}



	/* try to get tunnel state */
	ret = cs_l2tp_tunnel_get_by_tid((cs_tunnel_dir_t) dir, tid, t);

	if (ret == CS_OK)
		state = t->state;

	if (state == CS_TUNNEL_ACCELERATED /* HW tunnel exists */) {
		DBG(printk("%s:%d: dir %d, tunnel state = CS_TUNNEL_ACCELERATED"
			", type %d, tid 0x%x, mode %d\n",
			__func__, __LINE__,
			dir, t->tunnel_cfg.type, tunnel->tunnel_id, cs_cb->common.sw_only));

		/* workaround to handle rekey case of L2TP over IPSec
		 * Here tid is kept, but SPI is changed
		 */
		switch (t->tunnel_cfg.type) {
		case CS_L2TP:
			if ((dir == CS_TUNNEL_DIR_OUTBOUND) &&
				(cs_cb->input.l3_nh.iph.ver == 6)) {
				SKIP(printk("%s:%d: L2TPv2 doesn't support IPv6 over PPP (IPv4)\n",
						__func__, __LINE__));
				cs_cb->common.sw_only = CS_SWONLY_STATE;
				return 0;
			}
		case CS_L2TPV3:
			if (dir == CS_TUNNEL_DIR_OUTBOUND) {
				/* create flow hash for an outbound packet */
				cs_l2tp_lan2pe_flow_hash_add(cs_cb, &t_node);
			}
			break;
		case CS_IPSEC:
		case CS_L2TP_IPSEC:
		case CS_L2TPV3_IPSEC:
			/* Outbound L2TP over IPSec is handled in IPSec module */
			if (dir == CS_TUNNEL_DIR_OUTBOUND) {
				DBG(printk("%s:%d: ignore for tunnel type %d\n",
					__func__, __LINE__, t->tunnel_cfg.type));
				return 0;
			} else {
				/* Inbound L2TP over IPSec should not go through here */
				/* When rekey occurs, SPI is changed, but tid is kept.
				 * We should delete the old tunnel.
				 */
				DBG(printk("%s:%d: Get strange inbound packet "
					"in L2TP module: dir %d, type %d, "
					"SPI 0x%x, tunnel ID %d",
					__func__, __LINE__,
					dir, t->tunnel_cfg.type,
					cs_cb->input.vpn_h.ah_esp.spi,
					t->tunnel_id));
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
				/* If SPI is changed, delete the tunnel,
				   hashes and the PE entry */
				cs_ipsec_tunnel_chk(t,
						cs_cb->input.vpn_h.ah_esp.spi);
#endif
				cs_cb->common.sw_only = CS_SWONLY_STATE;
				return 0;
			}
			break;
		default:
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return 0;
		}


		/* send to PE */
		cs_l2tp_tx_skb_to_pe(skb, &t_node);
		return CS_DONE; /* free skb */
	} else if (state == CS_TUNNEL_INIT /* HW tunnel is under construction */) {
		DBG(printk("%s:%d: dir %d, tunnel state = CS_TUNNEL_INIT"
			", type %d, tid 0x%x\n",
			__func__, __LINE__,
			dir, t->tunnel_cfg.type, tunnel->tunnel_id));
		if (t->tunnel_cfg.type == CS_L2TP ||
					t->tunnel_cfg.type == CS_L2TPV3) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return 0;
		}
		/* drop packets */
		return CS_DONE; /* free skb */
	} else {
		cs_cb->common.module_mask |= CS_MOD_MASK_L2TP;
		cs_cb->common.vpn_dir = dir;
		cs_cb->common.tunnel_id = tunnel->tunnel_id;
		cs_cb->common.session_id = s->session_id;

		if ((tunnel->version & 0xF) == 3) {
			/* L2TPv3 */
			/* Collect L2TP MAC addr */
			if (dir == 0 /* outbound */) {
				/* skb->data points to L2TP MAC DA */
				for (i = 0; i < ETH_ALEN; i++)
					cs_cb->common.vpn_da[i] = skb->data[i];
				for (i = 0; i < ETH_ALEN; i++)
					cs_cb->common.vpn_sa[i] = skb->data[i + ETH_ALEN];
			} else {
				/* inbound */
				/* we don't care it in inbound case */
				/* skb->data points to L2TP MAC DA */
				for (i = 0; i < ETH_ALEN; i++)
					cs_cb->common.vpn_sa[i] = skb->data[i];
				for (i = 0; i < ETH_ALEN; i++)
					cs_cb->common.vpn_da[i] = skb->data[i + ETH_ALEN];
			}
		}
		DBG(printk("%s:%d: dir %d, tid 0x%x, sid 0x%x, "
			"module_mask = 0x%x, HW/SW mode %d\n",
			__func__, __LINE__,
			dir, tunnel->tunnel_id, s->session_id,
			cs_cb->common.module_mask, cs_cb->common.sw_only));
	}

	return 0;
}

EXPORT_SYMBOL(cs_l2tp_ctrl);


/*
 * Based on given skb and its control block info, create L2TP tunnel.
 * Now skb->data points to MAC header.
 */
int cs_l2tp_hw_accel_add(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_tunnel_dir_t dir;
	struct l2tp_session *session = NULL;
	struct l2tp_tunnel *tunnel = NULL;
	struct sock *sk = NULL;
	struct inet_sock *inet = NULL;
	cs_status_t ret;
	cs_dev_id_t dev_id;
	cs_l3_nexthop_t nexthop;
	cs_uint32_t nexthop_id;
	cs_tunnel_cfg_t	t_cfg;
	cs_tunnel_id_t tunnel_id;
	cs_uint16_t sa_id = G2_INVALID_SA_ID;
	cs_tunnel_entry_t *t, t_node;
	cs_uint32_t policy_handle = 0;
	cs_tunnel_type_t type = CS_L2TP;
	cs_uint32_t tid, sid;
	
	/*
	 * For inbound L2TP, only data packets should be accelerated.
	 * skb->data points to ether header.
	 * skb->network_header points to IP header, but network header len = 0.
	 * skb->mac_header and skb->transport_header are invalid.
	 */

	/*
	 * For outbound L2TP, only data packets should be accelerated.
	 * skb->data points to ether header.
	 * skb->network_header points to the outer IP header.
	 * skb->transport_header points to the UDP header. (L2TPv2)
	 * skb->mac_header is invalid.
	 */


	dir = (cs_tunnel_dir_t) cs_cb->common.vpn_dir;
	t = &t_node;

	DBG(printk("%s:%d: skb 0x%p, len %d, dir %d, skb->sp 0x%p,"
		" cs_cb 0x%p, mode %d, module_mask = 0x%x\n",
		__func__, __LINE__,
		skb, skb->len, dir, skb->sp, cs_cb,
		cs_cb->common.sw_only,
		cs_cb->common.module_mask));

	/******************** lookup tunnel ***********************/
	tunnel = l2tp_tunnel_find(&init_net, cs_cb->common.tunnel_id);
	if (tunnel == NULL) {
		DBG(printk("%s:%d: Can't find tunnel by tunnel_id 0x%x\n",
				__func__, __LINE__,
				cs_cb->common.tunnel_id));
		goto EXIT;
	} else {
		DBG(printk("%s:%d: Got tunnel by tunnel_id 0x%x\n",
				__func__, __LINE__,
				cs_cb->common.tunnel_id));
	}
	sk = tunnel->sock;
	
	/* L2TPv2 doesn't support IPv6 over PPP (IPv4) */
	if (tunnel->version == 2 &&
		cs_cb->input.l3_nh.iph.ver != cs_cb->output.l3_nh.iph.ver) {
		SKIP(printk("%s:%d: L2TPv2 doesn't support IPv6 over PPP (IPv4)\n",
				__func__, __LINE__));
		goto EXIT;
	}

	/******************** lookup session ***********************/
	session = l2tp_session_find(&init_net, tunnel, cs_cb->common.session_id);
	if (session == NULL) {
		DBG(printk("%s:%d: Can't find session by session_id 0x%x\n",
				__func__, __LINE__,
				cs_cb->common.session_id));
		goto EXIT;
	} else {
		DBG(printk("%s:%d: Got session by session_id 0x%x\n",
				__func__, __LINE__,
				cs_cb->common.session_id));
	}

	if (session->tunnel != tunnel) {
		DBG(printk("%s:%d: tunnel mismatch for tunnel_id 0x%x "
				"and session_id 0x%x\n",
				__func__, __LINE__,
				cs_cb->common.tunnel_id,
				cs_cb->common.session_id));
		goto EXIT;
	}

	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		tid = tunnel->peer_tunnel_id;
		sid = session->peer_session_id;
		break;
	case CS_TUNNEL_DIR_INBOUND:
		tid = tunnel->tunnel_id;
		sid = session->session_id;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		DBG(printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir));
		return CS_ACCEL_HASH_DONT_CARE;
	}

	DBG(printk("%s:%d: Dir %d, tid 0x%x, sid 0x%x\n",
 			__func__, __LINE__, dir, tid, sid));
	

	inet = inet_sk(sk);

	DBG(printk("%s:%d: SIP %pI4, DIP %pI4, sport %d, dport %d\n",
		__func__, __LINE__,
		&inet->inet_saddr, &inet->inet_daddr,
		ntohs(inet->inet_sport), ntohs(inet->inet_dport)));
	DBG(printk("%s:%d: cb OUTPUT SIP %pI4, DIP %pI4, sport %d, dport %d\n",
		__func__, __LINE__,
		&cs_cb->output.l3_nh.iph.sip, &cs_cb->output.l3_nh.iph.dip,
		ntohs(cs_cb->output.l4_h.uh.sport),
		ntohs(cs_cb->output.l4_h.uh.dport)));

	DBG(printk("%s:%d: cb INPUT SIP %pI4, DIP %pI4, sport %d, dport %d\n",
		__func__, __LINE__,
		&cs_cb->input.l3_nh.iph.sip, &cs_cb->input.l3_nh.iph.dip,
		ntohs(cs_cb->input.l4_h.uh.sport),
		ntohs(cs_cb->input.l4_h.uh.dport)));

	memset(&t_cfg, 0, sizeof(cs_tunnel_cfg_t));

	/* avoid duplicate tunnel */
	dev_id = 1;
	ret = cs_l2tp_tunnel_get_by_tid(dir, tid, t);

	if (ret == CS_OK) {
		/* we already create L2TP tunnel for it */
		
		sa_id = t->sa_id;
		DBG(printk("%s:%d: we already create L2TP tunnel for it. tid 0x%x\n",
			__func__, __LINE__, tid));
		goto FLOW_HASH;
	}

	/******************** create tunnel hash ***********************/
	/* add nexthop of L2TP tunnel */
	ret = cs_l2tp_nexthop_construct(skb, dir, &nexthop);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct L2TP nexthop, ret = %d\n",
			__func__, __LINE__, ret);
		goto EXIT;
	}

	/* nexthop debug print */
	DBG(printk("%s:%d: nexthop ID\n\t\t type = %d\n",
		__func__, __LINE__,
		nexthop.nhid.nhop_type));

	DBG(printk("\t\t IF ID = %d\n", nexthop.nhid.intf_id));

	if (nexthop.nhid.addr.afi == CS_IPV4) {
		DBG(printk("\t\t IP = %pI4\n", nexthop.nhid.addr.ip_addr.addr));
	} else {
		/* IPv6 */
		DBG(printk("\t\t IP = %pI6\n", nexthop.nhid.addr.ip_addr.addr));
	}
	DBG(printk("\t DA MAC = %pM\n", nexthop.da_mac));
	DBG(printk("\t ID = %d\n", nexthop.id.port_id));
	DBG(printk("\t encap\n\t\t type = %d\n", nexthop.encap.port_encap.type));
	DBG(printk("\t\t SA MAC = %pM\n", nexthop.encap.port_encap.port_encap.eth.src_mac));
	DBG(printk("\t\t VID[0] = %d\n", nexthop.encap.port_encap.port_encap.eth.tag[0]));
	DBG(printk("\t\t VID[1] = %d\n", nexthop.encap.port_encap.port_encap.eth.tag[1]));

	ret = cs_l3_nexthop_add(dev_id, &nexthop, &nexthop_id);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add L2TP nexthop, ret = %d\n",
			__func__, __LINE__, ret);
		goto EXIT;
	}

	/* add L2TP tunnel */
	ret = cs_l2tp_t_cfg_construct(cs_cb, type, dir, nexthop_id,
					policy_handle, session, &t_cfg);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct L2TP tunnel, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_NEXTHOP;
	}

	/* tunnel debug print */
	DBG(printk("%s:%d: tunnel_cfg\n\t type = %d\n",
		__func__, __LINE__,
		t_cfg.type));

	if (t_cfg.dest_addr.afi == CS_IPV4) {
		DBG(printk("\t dest IP = %pI4\n", t_cfg.dest_addr.ip_addr.addr));
		DBG(printk("\t src IP = %pI4\n", t_cfg.src_addr.ip_addr.addr));
	} else {
		/* IPv6 */
		DBG(printk("\t dest IP = %pI6\n", t_cfg.dest_addr.ip_addr.addr));
		DBG(printk("\t src IP = %pI6\n", t_cfg.src_addr.ip_addr.addr));
	}
	DBG(printk("\t tx port = %d\n", t_cfg.tx_port));
	DBG(printk("\t nexthop_id = %d\n", t_cfg.nexthop_id));
	DBG(printk("\t dir = %d\n", t_cfg.dir));
	switch (t_cfg.type) {
	case CS_IPSEC:
		DBG(printk("\t IPSec tunnel\n"));
		break;
	case CS_L2TP:
	case CS_L2TPV3:
		DBG(printk("\t L2TP tunnel\n"));
		break;
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
		DBG(printk("\t L2TP over IPSec tunnel\n"));
		break;
	default:
		DBG(printk("\t Unexpected tunnel, type %d\n", t_cfg.type));
	}

	if (t_cfg.type == CS_IPSEC) {
		DBG(printk("\t\t policy ID = %d\n", t_cfg.tunnel.ipsec.ipsec_policy));
	} else {
		DBG(printk("\t\t ver = 0x%x\n", t_cfg.tunnel.l2tp.ver));
		DBG(printk("\t\t len = 0x%x\n", t_cfg.tunnel.l2tp.len));
		DBG(printk("\t\t tid = 0x%x\n", t_cfg.tunnel.l2tp.tid));
		DBG(printk("\t\t UDP dest port = 0x%x\n", t_cfg.tunnel.l2tp.dest_port));
		DBG(printk("\t\t UDP src port = 0x%x\n", t_cfg.tunnel.l2tp.src_port));
		DBG(printk("\t\t policy ID = %d\n", t_cfg.tunnel.l2tp.ipsec_policy));
	}

	ret = cs_tunnel_add(dev_id, &t_cfg, &tunnel_id);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add L2TP tunnel, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_NEXTHOP;
	}

	/* add L2TP session if it is L2TPv2 */
	if (tunnel->version == 2) {
		ret = cs_l2tp_session_add(dev_id, tunnel_id, sid);
		
		if (ret != CS_OK) {
			printk("%s:%d: failed to add L2TP session, ret = %d\n",
				__func__, __LINE__, ret);
			goto FREE_TUNNEL;
		}
	}

	/* get tunnel entry back */
	ret = cs_l2tp_tunnel_get_by_tid((cs_tunnel_dir_t) dir, tid, t);

	if (ret == CS_OK) {
		sa_id = t->sa_id;
	} else {
		printk("%s:%d: failed to get tunnel entry, ret = %d\n",
			__func__, __LINE__, ret);
		if (tunnel->version == 2)
			goto FREE_SESSION;
		else
			goto FREE_TUNNEL;
	}

	printk("VPN HW acceleration for type %d, dir %d, tunnel id %d,"
		" tid 0x%x, sid 0x%x\n",
		type, dir, tunnel_id, tid, sid);


FLOW_HASH:
	/******************** create flow hash ***********************/
	if (t->state == CS_TUNNEL_ACCELERATED) {
		if (dir == CS_TUNNEL_DIR_OUTBOUND)
			cs_l2tp_lan2pe_flow_hash_add(cs_cb, t);
		else if (dir == CS_TUNNEL_DIR_INBOUND)
			cs_l2tp_pe2lan_flow_hash_add(cs_cb, t);
	}
	return CS_ACCEL_HASH_SUCCESS;

FREE_SESSION:
	cs_l2tp_session_delete(dev_id, tunnel_id, sid);
FREE_TUNNEL:
	cs_tunnel_delete(dev_id, &t_cfg);
FREE_NEXTHOP:
	cs_l3_nexthop_delete(dev_id, &nexthop);

EXIT:

	return CS_ACCEL_HASH_DONT_CARE;
}

int cs_l2tp_hw_accel_delete(cs_tunnel_entry_t *t)
{
	cs_dev_id_t dev_id;
	cs_tunnel_id_t tunnel_id;
	cs_uint32_t nexthop_id;
	cs_l3_nexthop_t nexthop;
	cs_tunnel_dir_t dir;
	cs_uint32_t ver, sid;


	dev_id = t->device_id;
	tunnel_id = t->tunnel_id;
	nexthop_id = t->tunnel_cfg.nexthop_id;
	dir = t->tunnel_cfg.dir;
	ver = t->tunnel_cfg.tunnel.l2tp.ver & 0xF;

	if (ver == 2 /* L2TPv2 */) {
		if (cs_l2tp_session_id_get(tunnel_id, &sid) == CS_OK)
			cs_l2tp_ipsec_session_delete(dev_id, tunnel_id, sid);
	}
		
	cs_tunnel_delete_by_idx(dev_id, tunnel_id);
	if (cs_l3_nexthop_get(dev_id, nexthop_id, &nexthop) == CS_OK)
		cs_l3_nexthop_delete(dev_id, &nexthop);

	return CS_OK;
}

int cs_l2tp_flow_hash_delete_by_sw_id(cs_uint64 sw_id)
{
	cs_core_hmu_value_t hmu_value;

	memset(&hmu_value, 0, sizeof(cs_core_hmu_value_t));
	hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	hmu_value.value.swid64 = sw_id;
	hmu_value.mask = 0x08;

	return cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64, &hmu_value);

}

void cs_l2tp_free(u32 tid, u32 peer_tid)
{
	cs_status_t ret;
	cs_tunnel_entry_t *t, t_node;

	DBG(printk("%s:%d: Del tunnel by tid 0x%x, peer_tid 0x%x\n",
			__func__, __LINE__,
			tid, peer_tid));
	t = &t_node;

	/* remove outbound L2TP tunnel */
	ret = cs_l2tp_tunnel_get_by_tid(CS_TUNNEL_DIR_OUTBOUND, peer_tid, t);

	if (ret == CS_OK) {
		/* L2TP over IPSec is handled in IPSec module */
		if (t->tunnel_cfg.type != CS_L2TP)
			return;

		cs_l2tp_flow_hash_delete_by_sw_id(t->sw_id);

		cs_l2tp_hw_accel_delete(&t_node);
		
	}

	/* remove inbound L2TP tunnel */
	ret = cs_l2tp_tunnel_get_by_tid(CS_TUNNEL_DIR_INBOUND, tid, t);

	if (ret == CS_OK) {
		/* L2TP over IPSec is handled in IPSec module */
		if (t->tunnel_cfg.type != CS_L2TP)
			return;

		cs_l2tp_flow_hash_delete_by_sw_id(t->sw_id);

		cs_l2tp_hw_accel_delete(&t_node);
		
	}
}

void cs_l2tp_proc_callback(unsigned long notify_event,
					   unsigned long value)
{
	DBG(printk("%s notify_event 0x%lx value 0x%lx\n",
	     __func__, notify_event, value));
	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
			DBG(printk("%s:%d: CS_HAM_ACTION_MODULE_DISABLE\n",
				__func__, __LINE__));
			break;
		case CS_HAM_ACTION_MODULE_ENABLE:
			DBG(printk("%s:%d: CS_HAM_ACTION_MODULE_ENABLE\n",
				__func__, __LINE__));
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			break;
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			break;
	}

}


