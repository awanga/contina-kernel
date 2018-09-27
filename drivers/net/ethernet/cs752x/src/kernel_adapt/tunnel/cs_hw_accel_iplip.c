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
 * cs_hw_accel_iplip.c
 *
 * $Id$
 *
 * This file contains the implementation for CS Tunnel 
 * Acceleration.
 * Currently supported:
 * 	IPv6 over PPP over L2TP over IPv4 over PPPoE (IPLIP)
 */

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/net_namespace.h>
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include "cs_fe.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_mut.h"

#ifdef CS_IPC_ENABLED
#include <mach/g2cpu_ipc.h>
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#endif				/* CONFIG_CS752X_PROC */

/* general data base */
static cs_pppoe_port_list_t	cs_pppoe_cb;
static cs_iplip_tbl_t		cs_iplip_tbl;

#ifdef CS_IPC_ENABLED
static struct ipc_context *cs_ipc_iplip_ctxt;
#endif


#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#define ERR(x)	(x)

static int cs_iplip_down_tunnel_hash_create(cs_tunnel_entry_t *tunnel_entry);
static int cs_iplip_down_tunnel_hash_del(cs_tunnel_entry_t *tunnel_entry);
static int cs_iplip_flow_hash_create(cs_ip_address_entry_t *ip_entry);
static int cs_iplip_flow_hash_del(cs_ip_address_entry_t *ip_entry);


/* utilities */
static int is_ppp_ipv4_check(
		int pppoe_ifindex,
		unsigned int ipv4
		)
{
	struct net_device *dev;
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	unsigned int rev_ipv4;
	cs_boolean is_wan_ip_addr = FALSE;

	rev_ipv4 = htonl(ipv4);
	DBG(printk("%s:%d pppoe_ifindex = %d, ip = %pI4\n", __func__, __LINE__,
		pppoe_ifindex, &rev_ipv4));

	dev = dev_get_by_index(&init_net, pppoe_ifindex);
	if (dev == NULL) {
		ERR(printk("%s:%d can't find net device\n", __func__, __LINE__));
		return is_wan_ip_addr;
	}
	in_dev = in_dev_get(dev);

	for (ifa = (in_dev)->ifa_list; ifa; ifa = ifa->ifa_next){		

		DBG(printk("%s: ifa_local %pI4\n",
		       ifa->ifa_label, &ifa->ifa_local));
		if (rev_ipv4 == ifa->ifa_local) {
			DBG(printk("%s:%d It is WAN IP address\n",
				__func__, __LINE__));
			is_wan_ip_addr = TRUE;
			break;
		}
	}

	if (in_dev)
		in_dev_put(in_dev);
        
	if (dev)
		dev_put(dev);
        
	return is_wan_ip_addr;
}


int cs_is_ppp_tunnel_traffic(
		struct sk_buff *skb
		)
{
	cs_pppoe_port_entry_t *p;

	if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_TUNNEL) == 0)
		return 0;
	
	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		ERR(printk("%s:%d skb->dev->ifindex = %d, p->ppp_ifindex = %d\n",
			__func__, __LINE__, skb->dev->ifindex, p->ppp_ifindex));

		/* L2TP's PPP interface is newer than PPPoE interface */
		if (skb->dev->ifindex >= p->ppp_ifindex)
			return 1;
		p = p->next;
	}
	return 0;
}
EXPORT_SYMBOL(cs_is_ppp_tunnel_traffic);

/* redirect packets from LAN to PE#1, VoQ#33 */
void cs_lan2pe_hash_add(
		struct sk_buff *skb
		)
{
	cs_kernel_accel_cb_t *cs_cb;
	u64 tmp_mask = 0;

	cs_cb = CS_KERNEL_SKB_CB(skb);
	
	DBG(printk("%s:%d skb = 0x%p, cs_cb = 0x%p\n", __func__, __LINE__, skb, cs_cb));
	if ((cs_cb != NULL) && (cs_cb->common.tag == CS_CB_TAG) && 
			(cs_cb->common.sw_only == CS_SWONLY_HW_ACCEL) &&
			(cs_cb->common.ingress_port_id != GE_PORT0)&&
			(cs_cb->common.ingress_port_id != ENCAPSULATION_PORT)) {
		DBG(printk("%s:%d module_mask = 0x%x, output_mask = 0x%llx, sw_only = 0x%x\n",
			__func__, __LINE__,
			cs_cb->common.module_mask, cs_cb->output_mask, cs_cb->common.sw_only));
		cs_core_logic_output_set_cb(skb);

		if (cs_cb->output_mask & CS_HM_VID_1_MASK)
			tmp_mask |= CS_HM_VID_1_MASK;

		if (cs_cb->output_mask & CS_HM_VID_2_MASK)
			tmp_mask |= CS_HM_VID_2_MASK;

		cs_cb->output_mask = tmp_mask;
		cs_cb->common.module_mask |= CS_MOD_MASK_TUNNEL;
		cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;

		cs_cb->action.voq_pol.d_voq_id = ENCAPSULATION_VOQ_BASE + 1;

		cs_core_logic_add_connections(skb);

		/* this packet will not be used to create hash again */
		cs_cb->common.sw_only = CS_SWONLY_STATE;
	}
	
	return;
}
EXPORT_SYMBOL(cs_lan2pe_hash_add);

int cs_iplip_ppp_ifindex_set(
		int pppoe_port,
		int ppp_ifindex
		)
{
	cs_pppoe_port_entry_t *p;

	DBG(printk("%s:%d pppoe_port_id = %d, ppp_ifindex = %d\n",
		__func__, __LINE__, pppoe_port, ppp_ifindex));
	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == pppoe_port) {
			p->ppp_ifindex = ppp_ifindex;
			return 0;
		}
		p = p->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return -1;
}

int cs_iplip_pppoe_ifindex_set(
		int pppoe_port,
		int pppoe_ifindex
		)
{
	cs_pppoe_port_entry_t *p;

	DBG(printk("%s:%d pppoe_port_id = %d, pppoe_ifindex = %d\n",
		__func__, __LINE__, pppoe_port, pppoe_ifindex));
	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == pppoe_port) {
			p->pppoe_ifindex = pppoe_ifindex;
			return 0;
		}
		p = p->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return -1;
}



static int cs_iplip_entry_add(
		cs_l2tp_session_entry_t *session_entry
		)
{
	cs_pppoe_port_entry_t *pppoe_port;
	cs_tunnel_entry_t *tunnel;
	int i;

	DBG(printk("%s:%d session_entry = 0x%p\n", __func__, __LINE__,
		session_entry));
	tunnel = session_entry->tunnel;
	pppoe_port = tunnel->pppoe_port;
	
	for (i = 0; i < CS_IPLIP_TBL_SIZE; i++) {
		if (cs_iplip_tbl.entry[i].valid == 0) {
			/* Ethernet header */
			/* input MAC address is network order */
			memcpy(cs_iplip_tbl.entry[i].iplip_hdr.ethh.h_dest,
				pppoe_port->p_cfg.dest_mac, ETH_ALEN);
			memcpy(cs_iplip_tbl.entry[i].iplip_hdr.ethh.h_source,
				pppoe_port->p_cfg.src_mac, ETH_ALEN);
			cs_iplip_tbl.entry[i].iplip_hdr.ethh.h_proto =
				htons(ETH_P_PPP_SES);

			/* PPPoE header */
			cs_iplip_tbl.entry[i].iplip_hdr.pppoeh.type = 1;
			cs_iplip_tbl.entry[i].iplip_hdr.pppoeh.ver = 1;
			cs_iplip_tbl.entry[i].iplip_hdr.pppoeh.code = 0;
			cs_iplip_tbl.entry[i].iplip_hdr.pppoeh.sid =
				htons(pppoe_port->p_cfg.pppoe_session_id);
			cs_iplip_tbl.entry[i].iplip_hdr.pppoeh.len = 0; /* TBD */

			/* PPP2 header */
			cs_iplip_tbl.entry[i].iplip_hdr.ppp2h = htons(0x0021);

			/* IPv4 header */
			cs_iplip_tbl.entry[i].iplip_hdr.iph.version = 4;
			cs_iplip_tbl.entry[i].iplip_hdr.iph.ihl = 5;
			cs_iplip_tbl.entry[i].iplip_hdr.iph.tos = 0; 	/* TBD */
			cs_iplip_tbl.entry[i].iplip_hdr.iph.tot_len = 0; /* TBD */
			/* id: increment value by 1, form 0x8000 to 0xffff */
			cs_iplip_tbl.entry[i].iplip_hdr.iph.id = 0; 	/* TBD */
			cs_iplip_tbl.entry[i].iplip_hdr.iph.frag_off = 0x4000; /* don't fragment */
			cs_iplip_tbl.entry[i].iplip_hdr.iph.ttl = 64;
			cs_iplip_tbl.entry[i].iplip_hdr.iph.protocol = IPPROTO_UDP;
			cs_iplip_tbl.entry[i].iplip_hdr.iph.check = 0;	/* TBD */
			cs_iplip_tbl.entry[i].iplip_hdr.iph.saddr =
				htonl(tunnel->tunnel_cfg.src_addr.ip_addr.ipv4_addr);
			cs_iplip_tbl.entry[i].iplip_hdr.iph.daddr =
				htonl(tunnel->tunnel_cfg.dest_addr.ip_addr.ipv4_addr);

			/* UDP header */
			cs_iplip_tbl.entry[i].iplip_hdr.udph.source =
				htons(tunnel->tunnel_cfg.tunnel.l2tp.src_port);
			cs_iplip_tbl.entry[i].iplip_hdr.udph.dest =
				htons(tunnel->tunnel_cfg.tunnel.l2tp.dest_port);
			cs_iplip_tbl.entry[i].iplip_hdr.udph.len = 0; /* TBD */
			cs_iplip_tbl.entry[i].iplip_hdr.udph.check = 0;

			/* L2TP header */
			/* T=0, L=1, S=0, O=0, P=0, Ver=2 */
			cs_iplip_tbl.entry[i].iplip_hdr.l2tph.ver = 
				htons(tunnel->tunnel_cfg.tunnel.l2tp.ver);
			cs_iplip_tbl.entry[i].iplip_hdr.l2tph.len = 
				htons(tunnel->tunnel_cfg.tunnel.l2tp.len);
			cs_iplip_tbl.entry[i].iplip_hdr.l2tph.tid =
				htons(tunnel->tunnel_cfg.tunnel.l2tp.tid);
			cs_iplip_tbl.entry[i].iplip_hdr.l2tph.sid =
				htons(session_entry->session_id);
			
			/* PPP header */
			cs_iplip_tbl.entry[i].iplip_hdr.ppph.addr = 0xff;
			cs_iplip_tbl.entry[i].iplip_hdr.ppph.ctrl = 0x03;
			cs_iplip_tbl.entry[i].iplip_hdr.ppph.pro = htons(0x0057);

			cs_iplip_tbl.entry[i].crc32 =
				calc_crc(~0, cs_iplip_tbl.entry[i].iplip_octet,
						sizeof(struct cs_iplip_hdr2_s));
			DBG(printk("%s:%d iplip_entry[%d].crc32 = 0x%08x\n",
						__func__, __LINE__, i,
						cs_iplip_tbl.entry[i].crc32));

			cs_iplip_tbl.entry[i].valid = 1;
			switch (tunnel->tunnel_cfg.dir) {
			case CS_TUNNEL_DIR_OUTBOUND:
				cs_iplip_tbl.entry[i].dir = 1;
				break;
			case CS_TUNNEL_DIR_INBOUND:
				cs_iplip_tbl.entry[i].dir = 2;
				break;
			case CS_TUNNEL_DIR_TWO_WAY:
			default:
				cs_iplip_tbl.entry[i].dir = 0;
			}
			session_entry->iplip_idx = i;
			cs_iplip_tbl.session_ptr[i] = session_entry;
			
			DBG(printk("%s:%d iplip_entry[%d] = \n",
						__func__, __LINE__, i));
			DBG(cs_dump_data(cs_iplip_tbl.entry[i].iplip_octet,
						sizeof(struct cs_iplip_hdr2_s)));

#ifdef CS_IPC_ENABLED
			/* Send IPC message to add IPLIP entry */
			cs_iplip_ipc_send_set_entry(i, &cs_iplip_tbl.entry[i]);
#endif /* CS_IPC_ENABLED */		
			return CS_OK;
		}
	}

	ERR(printk("%s:%d no available entry is found\n", __func__, __LINE__));
	return CS_ERROR;
}

static int cs_iplip_entry_del(
		unsigned char index
		)
{
	DBG(printk("%s:%d index = %d\n", __func__, __LINE__, index));
	
	if (cs_iplip_tbl.entry[index].valid == TRUE) {
		cs_iplip_tbl.entry[index].valid = FALSE;
		cs_iplip_tbl.session_ptr[index] = NULL;

		DBG(printk("%s:%d iplip_entry[%d] = \n",
						__func__, __LINE__, index));
		DBG(cs_dump_data(cs_iplip_tbl.entry[index].iplip_octet,
						sizeof(struct cs_iplip_hdr2_s)));

#ifdef CS_IPC_ENABLED
		/* Send IPC message to delete IPLIP entry */
		cs_iplip_ipc_send_del_entry(index);
#endif /* CS_IPC_ENABLED */		
		return CS_OK;
	}

	ERR(printk("%s:%d entry#%d is an invalid entry\n", __func__, __LINE__,
		index));
	return CS_ERROR;
}

static int cs_iplip_down_tunnel_hash_create(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	cs_pppoe_port_entry_t *pppoe_port;
	unsigned long long fwd_hm_flag;
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	unsigned int voq_pol_idx, fwd_rslt_idx, vlan_rslt_idx;
	fe_sw_hash_t key;
	unsigned short hash_index;
	unsigned int crc32;
	unsigned short crc16;
	int ret = 0;
	int i;


	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));
	pppoe_port = tunnel_entry->pppoe_port;
	

	/*********************************************************/
	/* redirect packets from WAN to PE VoQ#32 for downstream */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	ret = cs_core_vtable_get_hashmask_flag_from_apptype(
	    					CORE_FWD_APP_TYPE_TUNNEL,
							      &fwd_hm_flag);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask flag, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	ret = cs_core_vtable_get_hashmask_index_from_apptype(
						CORE_FWD_APP_TYPE_TUNNEL,
						&key.mask_ptr_0_7);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask index, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	/* set key */
	
	/* CS_HM_LSPID_MASK */
	key.lspid = GE_PORT0;

	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	key.pppoe_session_id_valid = 1;
	key.pppoe_session_id = pppoe_port->p_cfg.pppoe_session_id;

	
	/*CS_HM_IP_VLD_MASK */
	key.ip_valid = 1;
	
	/* CS_HM_IP_VER_MASK */
	key.ip_version = tunnel_entry->tunnel_cfg.dest_addr.afi;
	
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK */
	if (key.ip_version == CS_IPV6) {
		/* IP address for hash key is host order */
		
		for (i = 0; i < 4; i++) {
			key.sa[i] = tunnel_entry->tunnel_cfg.src_addr.ip_addr.ipv6_addr[i];

			key.da[i] = tunnel_entry->tunnel_cfg.dest_addr.ip_addr.ipv6_addr[i];
		}
	} else {
		key.sa[0] = tunnel_entry->tunnel_cfg.src_addr.ip_addr.ipv4_addr;

		key.da[0] = tunnel_entry->tunnel_cfg.dest_addr.ip_addr.ipv4_addr;
	}

	/* CS_HM_IP_PROT_MASK */
	key.ip_prot = IPPROTO_UDP;

	/* CS_HM_IP_FRAGMENT_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L3_CHKSUM_ERR_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
	key.l4_valid = 1;
	key.l4_dp = tunnel_entry->tunnel_cfg.tunnel.l2tp.dest_port;
	key.l4_sp = tunnel_entry->tunnel_cfg.tunnel.l2tp.src_port;

	/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
	/* we use the default value 0 */
	
	/* set VoQ policer */
	memset(&voq_pol, 0x0, sizeof(fe_voq_pol_entry_t));
	voq_pol.voq_base = ENCAPSULATION_VOQ_BASE; /* VoQ 32 */

	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voq_pol,
							&voq_pol_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VoQ policer\n",
			__func__, __LINE__));
		return CS_ERROR;
	}
	tunnel_entry->voq_pol_idx[1] = voq_pol_idx;

	/* set FVLAN command */
 	memset(&fvlan_entry, 0, sizeof(fvlan_entry));
	fvlan_entry.first_vid = 0;
	fvlan_entry.first_vlan_cmd = CS_FE_VLAN_CMD_POP_A;
 	fvlan_entry.first_tpid_enc = 4;
	fvlan_entry.second_vid = 0;
	fvlan_entry.second_vlan_cmd = 0;
 	fvlan_entry.second_tpid_enc = 0;
	
	ret = cs_fe_table_add_entry(FE_TABLE_FVLAN,
			&fvlan_entry, &vlan_rslt_idx);
 	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VLAN result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}

	tunnel_entry->vlan_rslt_idx[1] = vlan_rslt_idx;
	
	/* set fwd result */
	memset(&fwd_rslt, 0x0, sizeof(fe_fwd_result_entry_t));
	fwd_rslt.dest.voq_policy = 0;
	fwd_rslt.dest.voq_pol_table_index = (u16)voq_pol_idx;
	fwd_rslt.l2.flow_vlan_index = (u16)vlan_rslt_idx;
	fwd_rslt.l2.flow_vlan_op_en = 1;
	/* remove outer VLAN if any */
	

	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwd_rslt, &fwd_rslt_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add forwarding result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}
	tunnel_entry->fwd_rslt_idx[1] = fwd_rslt_idx;


	/* create fwd hash */
	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get crc32, ret = 0x%x\n",
			__func__, __LINE__, ret));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
					fwd_rslt_idx, &hash_index);
	if ((ret != FE_TABLE_OK) && (ret != FE_TABLE_EDUPLICATE)) {
		ERR(printk("%s:%d Can't add forwarding hash, ret = %d\n",
			__func__, __LINE__, ret));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		/* ignore this for duplicate hash */
		/* FIXME!! there is something wrong when deleting hash */
		/*
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
		*/
	}

	tunnel_entry->fwd_hash_idx[1] = hash_index;

	DBG(printk("\t WAN to PE: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
		"VLAN cmd = %d\n",
		tunnel_entry->fwd_hash_idx[1],
		tunnel_entry->voq_pol_idx[1],
		tunnel_entry->fwd_rslt_idx[1],
		tunnel_entry->vlan_rslt_idx[1]));
	return CS_OK;
}

static int cs_iplip_down_tunnel_hash_del(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));
	DBG(printk("\t WAN to PE: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
		"VLAN cmd = %d\n",
		tunnel_entry->fwd_hash_idx[1],
		tunnel_entry->voq_pol_idx[1],
		tunnel_entry->fwd_rslt_idx[1],
		tunnel_entry->vlan_rslt_idx[1]));

	/* WAN --> PE */
	/* del hash at first to avoid junk pkt */
	cs_fe_hash_del_hash(tunnel_entry->fwd_hash_idx[1]);
	tunnel_entry->fwd_hash_idx[1] = 0;

	/* Delete Result table */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				tunnel_entry->fwd_rslt_idx[1], false);
	tunnel_entry->fwd_rslt_idx[1] = 0;

	/* Delete FVLAN command */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
				tunnel_entry->vlan_rslt_idx[1], false);
	tunnel_entry->vlan_rslt_idx[1] = 0;

	/* Delete VOQPOL */
	cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
				tunnel_entry->voq_pol_idx[1], false);
	tunnel_entry->voq_pol_idx[1] = 0;

	
	return CS_OK;
}

static int cs_iplip_up_tunnel_hash_create(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	cs_pppoe_port_entry_t *pppoe_port;
	unsigned long long fwd_hm_flag;
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	unsigned int voq_pol_idx, fwd_rslt_idx, vlan_rslt_idx;
	fe_sw_hash_t key;
	unsigned short hash_index;
	unsigned int crc32;
	unsigned short crc16;
	int ret = 0;
	int i;


	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));
	pppoe_port = tunnel_entry->pppoe_port;
	

	/*********************************************************/
	/* redirect packets from PE to WAN for upstream */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	ret = cs_core_vtable_get_hashmask_flag_from_apptype(
	    					CORE_FWD_APP_TYPE_TUNNEL,
							      &fwd_hm_flag);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask flag, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	ret = cs_core_vtable_get_hashmask_index_from_apptype(
						CORE_FWD_APP_TYPE_TUNNEL,
						&key.mask_ptr_0_7);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask index, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	/* set key */
	
	/* CS_HM_LSPID_MASK */
	key.lspid = ENCAPSULATION_PORT;

	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	key.pppoe_session_id_valid = 1;
	key.pppoe_session_id = pppoe_port->p_cfg.pppoe_session_id;

	
	/*CS_HM_IP_VLD_MASK */
	key.ip_valid = 1;
	
	/* CS_HM_IP_VER_MASK */
	key.ip_version = tunnel_entry->tunnel_cfg.dest_addr.afi;
	
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK */
	if (key.ip_version == CS_IPV6) {
		/* IP address for hash key is host order */
		
		for (i = 0; i < 4; i++) {
			key.sa[i] = tunnel_entry->tunnel_cfg.src_addr.ip_addr.ipv6_addr[i];

			key.da[i] = tunnel_entry->tunnel_cfg.dest_addr.ip_addr.ipv6_addr[i];
		}
	} else {
		key.sa[0] = tunnel_entry->tunnel_cfg.src_addr.ip_addr.ipv4_addr;

		key.da[0] = tunnel_entry->tunnel_cfg.dest_addr.ip_addr.ipv4_addr;
	}

	/* CS_HM_IP_PROT_MASK */
	key.ip_prot = IPPROTO_UDP;

	/* CS_HM_IP_FRAGMENT_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L3_CHKSUM_ERR_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
	key.l4_valid = 1;
	key.l4_dp = tunnel_entry->tunnel_cfg.tunnel.l2tp.dest_port;
	key.l4_sp = tunnel_entry->tunnel_cfg.tunnel.l2tp.src_port;

	/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
	/* we use the default value 0 */
	
	/* set VoQ policer */
	memset(&voq_pol, 0x0, sizeof(fe_voq_pol_entry_t));
	voq_pol.voq_base = GE_PORT0_VOQ_BASE; /* VoQ 0 */

	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voq_pol,
							&voq_pol_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VoQ policer\n",
			__func__, __LINE__));
		return CS_ERROR;
	}
	tunnel_entry->voq_pol_idx[0] = voq_pol_idx;
	
	/* set FVLAN command */
 	memset(&fvlan_entry, 0, sizeof(fvlan_entry));
	fvlan_entry.first_vid = pppoe_port->p_cfg.vlan_tag & VLAN_VID_MASK;
	if (fvlan_entry.first_vid > 0)
		fvlan_entry.first_vlan_cmd = CS_FE_VLAN_CMD_PUSH_B;
	else
		fvlan_entry.first_vlan_cmd = 0;
 	fvlan_entry.first_tpid_enc = 4;
	fvlan_entry.second_vid = 0;
	fvlan_entry.second_vlan_cmd = 0;
 	fvlan_entry.second_tpid_enc = 0;
	
	ret = cs_fe_table_add_entry(FE_TABLE_FVLAN,
			&fvlan_entry, &vlan_rslt_idx);
 	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VLAN result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}
	tunnel_entry->vlan_rslt_idx[0] = vlan_rslt_idx;
	
	/* set fwd result */
	memset(&fwd_rslt, 0x0, sizeof(fe_fwd_result_entry_t));
	fwd_rslt.dest.voq_policy = 1;	/* Enable QoS offset */
	fwd_rslt.dest.voq_pol_table_index = (u16)voq_pol_idx;
	fwd_rslt.l2.flow_vlan_index = (u16)vlan_rslt_idx;
	fwd_rslt.l2.flow_vlan_op_en = 1;
	
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwd_rslt, &fwd_rslt_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add forwarding result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}
	tunnel_entry->fwd_rslt_idx[0] = fwd_rslt_idx;


	/* create fwd hash */
	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get crc32, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
					fwd_rslt_idx, &hash_index);
	if ((ret != FE_TABLE_OK) && (ret != FE_TABLE_EDUPLICATE)) {
		ERR(printk("%s:%d Can't add forwarding hash, ret = %d\n",
			__func__, __LINE__, ret));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		/* ignore this for duplicate hash */
		/* FIXME!! there is something wrong when deleting hash */
		/*
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
		*/
	}

	tunnel_entry->fwd_hash_idx[0] = hash_index;

	DBG(printk("\t PE to WAN: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
		"VLAN cmd = %d\n",
		tunnel_entry->fwd_hash_idx[0],
		tunnel_entry->voq_pol_idx[0],
		tunnel_entry->fwd_rslt_idx[0],
		tunnel_entry->vlan_rslt_idx[0]));
	return CS_OK;
}

static int cs_iplip_up_tunnel_hash_del(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));
	DBG(printk("\t PE to WAN: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
		"VLAN cmd = %d\n",
		tunnel_entry->fwd_hash_idx[0],
		tunnel_entry->voq_pol_idx[0],
		tunnel_entry->fwd_rslt_idx[0],
		tunnel_entry->vlan_rslt_idx[0]));

	/* PE --> WAN */
	/* del hash at first to avoid junk pkt */
	cs_fe_hash_del_hash(tunnel_entry->fwd_hash_idx[0]);
	tunnel_entry->fwd_hash_idx[0] = 0;

	/* Delete Result table */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				tunnel_entry->fwd_rslt_idx[0], false);
	tunnel_entry->fwd_rslt_idx[0] = 0;

	/* Delete FVLAN command */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
				tunnel_entry->vlan_rslt_idx[0], false);
	tunnel_entry->vlan_rslt_idx[0] = 0;
	
	/* Delete VOQPOL */
	cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
				tunnel_entry->voq_pol_idx[0], false);
	tunnel_entry->voq_pol_idx[0] = 0;

	
	return CS_OK;
}

static int cs_iplip_pe2cpu_tunnel_hash_create(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	cs_pppoe_port_entry_t *pppoe_port;
	unsigned long long fwd_hm_flag;
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	unsigned int voq_pol_idx, fwd_rslt_idx, vlan_rslt_idx;
	fe_sw_hash_t key;
	unsigned short hash_index;
	unsigned int crc32;
	unsigned short crc16;
	int ret = 0;
	int i;


	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));
	pppoe_port = tunnel_entry->pppoe_port;
	

	/*********************************************************/
	/* redirect packets from PE1 to CPU VoQ#48~55 for control packets */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	ret = cs_core_vtable_get_hashmask_flag_from_apptype(
	    					CORE_FWD_APP_TYPE_TUNNEL,
							      &fwd_hm_flag);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask flag, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	ret = cs_core_vtable_get_hashmask_index_from_apptype(
						CORE_FWD_APP_TYPE_TUNNEL,
						&key.mask_ptr_0_7);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask index, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	/* set key */
	
	/* CS_HM_LSPID_MASK */
	key.lspid = ENCAPSULATION_PORT;

	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	key.pppoe_session_id_valid = 1;
	key.pppoe_session_id = pppoe_port->p_cfg.pppoe_session_id;

	
	/*CS_HM_IP_VLD_MASK */
	key.ip_valid = 1;
	
	/* CS_HM_IP_VER_MASK */
	key.ip_version = tunnel_entry->tunnel_cfg.dest_addr.afi;
	
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK */
	if (key.ip_version == CS_IPV6) {
		/* IP address for hash key is host order */
		
		for (i = 0; i < 4; i++) {
			key.sa[i] = tunnel_entry->tunnel_cfg.src_addr.ip_addr.ipv6_addr[i];

			key.da[i] = tunnel_entry->tunnel_cfg.dest_addr.ip_addr.ipv6_addr[i];
		}
	} else {
		key.sa[0] = tunnel_entry->tunnel_cfg.src_addr.ip_addr.ipv4_addr;

		key.da[0] = tunnel_entry->tunnel_cfg.dest_addr.ip_addr.ipv4_addr;
	}

	/* CS_HM_IP_PROT_MASK */
	key.ip_prot = IPPROTO_UDP;

	/* CS_HM_IP_FRAGMENT_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L3_CHKSUM_ERR_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
	key.l4_valid = 1;
	key.l4_dp = tunnel_entry->tunnel_cfg.tunnel.l2tp.dest_port;
	key.l4_sp = tunnel_entry->tunnel_cfg.tunnel.l2tp.src_port;

	/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
	/* we use the default value 0 */
	
	/* set VoQ policer */
	memset(&voq_pol, 0x0, sizeof(fe_voq_pol_entry_t));
	voq_pol.voq_base = CPU_PORT0_VOQ_BASE; /* VoQ 48 */

	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voq_pol,
							&voq_pol_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VoQ policer\n",
			__func__, __LINE__));
		return CS_ERROR;
	}
	tunnel_entry->voq_pol_idx[2] = voq_pol_idx;

	/* set FVLAN command */
 	memset(&fvlan_entry, 0, sizeof(fvlan_entry));
	fvlan_entry.first_vid = pppoe_port->p_cfg.vlan_tag & VLAN_VID_MASK;
	if (fvlan_entry.first_vid > 0)
		fvlan_entry.first_vlan_cmd = CS_FE_VLAN_CMD_PUSH_B;
	else
		fvlan_entry.first_vlan_cmd = 0;
 	fvlan_entry.first_tpid_enc = 4;
	fvlan_entry.second_vid = 0;
	fvlan_entry.second_vlan_cmd = 0;
 	fvlan_entry.second_tpid_enc = 0;
	
	ret = cs_fe_table_add_entry(FE_TABLE_FVLAN,
			&fvlan_entry, &vlan_rslt_idx);
 	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VLAN result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}

	tunnel_entry->vlan_rslt_idx[2] = vlan_rslt_idx;
	
	/* set fwd result */
	memset(&fwd_rslt, 0x0, sizeof(fe_fwd_result_entry_t));
	fwd_rslt.dest.voq_policy = 1;	/* Enable QoS offset */
	fwd_rslt.dest.voq_pol_table_index = (u16)voq_pol_idx;
	fwd_rslt.l2.flow_vlan_index = (u16)vlan_rslt_idx;
	fwd_rslt.l2.flow_vlan_op_en = 1;
	/* remove outer VLAN if any */
	

	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwd_rslt, &fwd_rslt_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add forwarding result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}
	tunnel_entry->fwd_rslt_idx[2] = fwd_rslt_idx;


	/* create fwd hash */
	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get crc32, ret = 0x%x\n",
			__func__, __LINE__, ret));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
					fwd_rslt_idx, &hash_index);
	if ((ret != FE_TABLE_OK) && (ret != FE_TABLE_EDUPLICATE)) {
		ERR(printk("%s:%d Can't add forwarding hash, ret = %d\n",
			__func__, __LINE__, ret));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
						vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		/* ignore this for duplicate hash */
		/* FIXME!! there is something wrong when deleting hash */
		/*
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
		*/
	}

	tunnel_entry->fwd_hash_idx[2] = hash_index;

	DBG(printk("\t PE1 to CPU: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
		"VLAN cmd = %d\n",
		tunnel_entry->fwd_hash_idx[2],
		tunnel_entry->voq_pol_idx[2],
		tunnel_entry->fwd_rslt_idx[2],
		tunnel_entry->vlan_rslt_idx[2]));
	return CS_OK;
}

static int cs_iplip_pe2cpu_tunnel_hash_del(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));
	DBG(printk("\t PE1 to CPU: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
		"VLAN cmd = %d\n",
		tunnel_entry->fwd_hash_idx[2],
		tunnel_entry->voq_pol_idx[2],
		tunnel_entry->fwd_rslt_idx[2],
		tunnel_entry->vlan_rslt_idx[2]));

	/* WAN --> PE */
	/* del hash at first to avoid junk pkt */
	cs_fe_hash_del_hash(tunnel_entry->fwd_hash_idx[2]);
	tunnel_entry->fwd_hash_idx[2] = 0;

	/* Delete Result table */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				tunnel_entry->fwd_rslt_idx[2], false);
	tunnel_entry->fwd_rslt_idx[2] = 0;

	/* Delete FVLAN command */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
				tunnel_entry->vlan_rslt_idx[2], false);
	tunnel_entry->vlan_rslt_idx[2] = 0;

	/* Delete VOQPOL */
	cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
				tunnel_entry->voq_pol_idx[2], false);
	tunnel_entry->voq_pol_idx[2] = 0;

	
	return CS_OK;
}



static int cs_iplip_flow_hash_create(
		cs_ip_address_entry_t	*ip_entry
		)
{
	cs_pppoe_port_entry_t *pppoe_port;
	cs_tunnel_entry_t *tunnel;
	cs_l2tp_session_entry_t *session;
	unsigned long long fwd_hm_flag;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	unsigned int voq_pol_idx, fwd_rslt_idx;
	fe_sw_hash_t key;
	unsigned short hash_index;
	unsigned int crc32;
	unsigned short crc16;
	int ret = 0;
	unsigned char index;
	int i;

	DBG(printk("%s:%d ip_entry = 0x%p\n", __func__, __LINE__, ip_entry));
	session = ip_entry->session;
	tunnel = session->tunnel;
	pppoe_port = tunnel->pppoe_port;
	index = session->iplip_idx;

	/* redirect packets from LAN to PE VoQ#33 for upstream */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	ret = cs_core_vtable_get_hashmask_flag_from_apptype(
	    					CORE_FWD_APP_TYPE_IPLIP_LAN,
							      &fwd_hm_flag);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask flag, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	ret = cs_core_vtable_get_hashmask_index_from_apptype(
						CORE_FWD_APP_TYPE_IPLIP_LAN,
						&key.mask_ptr_0_7);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get hashmask index, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	/* set key */
	/* CS_HM_LSPID_MASK */
	key.lspid = GE_PORT1;

	/* CS_HM_IP_VLD_MASK */
	key.ip_valid = 1;
	
	/* CS_HM_IP_VER_MASK */
	key.ip_version = ip_entry->ip.afi;

	/* CS_HM_IP_FRAGMENT_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_L3_CHKSUM_ERR_MASK */
	/* we use the default value 0 */
	
	/* CS_HM_IP_SA_MASK */
	/* IP address for hash key is host order */
	if (key.ip_version == CS_IPV6) {
		for (i = 0; i < 4; i++)
			key.sa[i] = ip_entry->ip.ip_addr.ipv6_addr[i];
	} else {
		key.sa[0] = ip_entry->ip.ip_addr.ipv4_addr;
	}

#ifdef CONFIG_CS75XX_MTU_CHECK
	key.pktlen_rng_match_vector = (0x1 << 2);
#endif

	/* set fwd result */
	memset(&voq_pol, 0x0, sizeof(fe_voq_pol_entry_t));
	voq_pol.voq_base = ENCAPSULATION_VOQ_BASE + 1; /* VoQ 33 */

	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voq_pol,
							&voq_pol_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add VoQ policer\n",
			__func__, __LINE__));
		return CS_ERROR;
	}
	ip_entry->voq_pol_idx = voq_pol_idx;
	
	memset(&fwd_rslt, 0x0, sizeof(fe_fwd_result_entry_t));
	fwd_rslt.dest.voq_policy = 0;
	fwd_rslt.dest.voq_pol_table_index = voq_pol_idx;
	fwd_rslt.l3.decr_ttl_hoplimit = 1;

	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwd_rslt, &fwd_rslt_idx);
	if (ret != FE_TABLE_OK) {
		ERR(printk("%s:%d Can't add forwarding result\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}
	ip_entry->fwd_rslt_idx = fwd_rslt_idx;


	/* create fwd hash */
	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		ERR(printk("%s:%d Can't get crc32, ret = 0x%x\n",
			__func__, __LINE__, ret));
		return CS_ERROR;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
					fwd_rslt_idx, &hash_index);
	if ((ret != FE_TABLE_OK) && (ret != FE_TABLE_EDUPLICATE)) {
		ERR(printk("%s:%d Can't add forwarding hash\n",
			__func__, __LINE__));
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
						fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voq_pol_idx, false);
		return CS_ERROR;
	}

	ip_entry->fwd_hash_idx = hash_index;

	DBG(printk("\t LAN to PE: hash = %d, VoQ policer = %d, fwd_rslt = %d\n",
		ip_entry->fwd_hash_idx,
		ip_entry->voq_pol_idx,
		ip_entry->fwd_rslt_idx));
	return CS_OK;
}

static int cs_iplip_flow_hash_del(
	cs_ip_address_entry_t	*ip_entry
		)
{
	DBG(printk("%s:%d ip_entry = 0x%p\n", __func__, __LINE__, ip_entry));
	DBG(printk("\t LAN to PE: hash = %d, VoQ policer = %d, fwd_rslt = %d\n",
		ip_entry->fwd_hash_idx,
		ip_entry->voq_pol_idx,
		ip_entry->fwd_rslt_idx));


	/* LAN --> PE */
	/* del hash at first to avoid junk pkt */
	cs_fe_hash_del_hash(ip_entry->fwd_hash_idx);
	ip_entry->fwd_hash_idx = 0;

	/* Delete VOQPOL */
	cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
				ip_entry->voq_pol_idx, false);
	ip_entry->voq_pol_idx = 0;

	/* Delete Result table */
	cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				ip_entry->fwd_rslt_idx, false);
	ip_entry->fwd_rslt_idx = 0;
	
	return CS_OK;
}

/* exported APIs */
cs_status_t
cs_pppoe_port_add(
		CS_IN cs_dev_id_t   device_id,
 		CS_IN cs_port_id_t  port_id,
		CS_IN cs_port_id_t  pppoe_port_id
		)
{
	cs_pppoe_port_entry_t *port_node, *p, *p2;

	DBG(printk("%s:%d device_id=%d, port_id=%d, pppoe_port_id = %d\n",
		__func__, __LINE__, device_id, port_id, pppoe_port_id));
	p = p2 = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* check for duplicate PPPoE port ID */
		if (p->pppoe_port_id == pppoe_port_id) {
			ERR(printk("%s:%d duplicate PPPoE port ID = %d\n",
				__func__, __LINE__, pppoe_port_id));
			return CS_E_PORT_ID;
		}
		p2 = p;
		p = p->next;
	}

	port_node = cs_zalloc(sizeof(cs_pppoe_port_entry_t), GFP_KERNEL);
	if (port_node == NULL) {
		ERR(printk("%s:%d out of memory\n", __func__, __LINE__));
		return CS_ERROR;
	}
	
	port_node->device_id = device_id;
	port_node->port_id = port_id;
	port_node->pppoe_port_id = pppoe_port_id;

	/* assume PPPoE ifindex is equal to PPPoE port ID */
	port_node->pppoe_ifindex = pppoe_port_id;
	/* assume L2TP's PPP port is next to original PPPoE port */
	port_node->ppp_ifindex = pppoe_port_id + 1;

	if (cs_pppoe_cb.port_list_hdr == NULL) {
		cs_pppoe_cb.port_list_hdr = port_node;
		cs_pppoe_cb.pppoe_port_cnt = 1;
	} else {
		p2->next = port_node;
		cs_pppoe_cb.pppoe_port_cnt++;
	}

	return CS_OK;
}
EXPORT_SYMBOL(cs_pppoe_port_add);

cs_status_t
cs_pppoe_port_delete(
		CS_IN cs_dev_id_t   device_id,
		CS_IN cs_port_id_t  pppoe_port_id
		)
{
	cs_pppoe_port_entry_t *p, *p2;

	DBG(printk("%s:%d device_id=%d, pppoe_port_id = %d\n", __func__,
		__LINE__, device_id, pppoe_port_id));
	p = p2 = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == pppoe_port_id) {
			if (p->tu_cnt > 0) {
				ERR(printk("%s:%d still have %d tunnels\n",
					__func__, __LINE__, p->tu_cnt));
				return CS_E_PORT_ID;
			}
			if (p == cs_pppoe_cb.port_list_hdr) {
				/* the first entry */
				cs_pppoe_cb.port_list_hdr = p->next;
			} else {
				p2->next = p->next;
			}
			
			cs_free(p);
			cs_pppoe_cb.pppoe_port_cnt--;
			return CS_OK;
		}
		p2 = p;
		p = p->next;
	}
	
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(cs_pppoe_port_delete);

cs_status_t
cs_pppoe_port_encap_set(
		CS_IN cs_dev_id_t           device_id,
		CS_IN cs_port_id_t          pppoe_port_id,
		CS_IN cs_pppoe_port_cfg_t   *p_cfg
		)
{
	cs_pppoe_port_entry_t *p;

	DBG(printk("%s:%d device_id=%d, pppoe_port_id = %d, p_cfg = 0x%p\n",
		__func__, __LINE__, device_id, pppoe_port_id, p_cfg));
	if (p_cfg == NULL)
		return CS_ERROR;

	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == pppoe_port_id) {
			memcpy(&p->p_cfg, p_cfg, sizeof(cs_pppoe_port_cfg_t));
			return CS_OK;
		}
		p = p->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(cs_pppoe_port_encap_set);

cs_status_t
cs_pppoe_port_config_get(
		CS_IN  cs_dev_id_t          device_id,
		CS_IN  cs_port_id_t         pppoe_port_id,
		CS_OUT cs_pppoe_port_cfg_t  *p_cfg
		)
{
	cs_pppoe_port_entry_t *p;

	DBG(printk("%s:%d device_id=%d, pppoe_port_id = %d, p_cfg = 0x%p\n",
		__func__, __LINE__, device_id, pppoe_port_id, p_cfg));
	if (p_cfg == NULL)
		return CS_ERROR;

	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == pppoe_port_id) {
			memcpy(p_cfg, &p->p_cfg, sizeof(cs_pppoe_port_cfg_t));
			return CS_OK;
		}
		p = p->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;

}
EXPORT_SYMBOL(cs_pppoe_port_config_get);

cs_status_t cs_iplip_tunnel_add(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_cfg_t				*p_tunnel_cfg,
	CS_OUT	cs_tunnel_id_t				*p_tunnel_id)
{
	cs_pppoe_port_entry_t *p;
	int i;
	cs_tunnel_entry_t *tunnel_node, *t, *t2;

	DBG(printk("%s:%d device_id=%d, p_tunnel_cfg = 0x%p,"
		" p_tunnel_id = 0x%p\n",
		__func__, __LINE__, device_id, p_tunnel_cfg, p_tunnel_id));
	if (p_tunnel_cfg == NULL)
		return CS_ERROR;

	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == p_tunnel_cfg->tx_port) {
			t = t2 = p->tu_h;
			while (t) {
				/* check for duplicate tunnel ID */
				if (t->tunnel_cfg.tunnel.l2tp.tid == 
					p_tunnel_cfg->tunnel.l2tp.tid) {
					ERR(printk("%s:%d duplicate tid = 0x%x\n",
						__func__, __LINE__,
						p_tunnel_cfg->tunnel.l2tp.tid));
					return CS_ERROR;
				}
				t2 = t;
				t = t->next;
			}

			if (cs_pppoe_cb.tt_cnt >= CS_TUNNEL_TBL_SIZE) {
				ERR(printk("%s:%d no available tunnel,"
					" tt_cnt = %d\n",
					__func__, __LINE__,
					cs_pppoe_cb.tt_cnt));
				return CS_ERROR;
			}
			
			tunnel_node = cs_zalloc(sizeof(cs_tunnel_entry_t),
						GFP_KERNEL);
			if (tunnel_node == NULL) {
				ERR(printk("%s:%d out of memory\n",
					__func__, __LINE__));
				return CS_ERROR;
			}

			tunnel_node->tunnel_id = TID_INVALID;
			for (i = 0; i < CS_TUNNEL_TBL_SIZE; i++) {
				if (cs_pppoe_cb.tu_list[i] == NULL) {
					tunnel_node->tunnel_id = i;
					cs_pppoe_cb.tu_list[i] = tunnel_node;
					break;
				}
			}

			if (tunnel_node->tunnel_id == TID_INVALID) {
				/* no free tunnel_id */
				ERR(printk("%s:%d no available tunnel\n",
					__func__, __LINE__));
				cs_free(tunnel_node);
				return CS_ERROR;
			}

			*p_tunnel_id = tunnel_node->tunnel_id;
			memcpy(&tunnel_node->tunnel_cfg, p_tunnel_cfg,
				sizeof(cs_tunnel_cfg_t));

			/* check direction */
			if (p_tunnel_cfg->src_addr.afi == CS_IPV4 &&
				is_ppp_ipv4_check(p->pppoe_ifindex,
				p_tunnel_cfg->src_addr.ip_addr.ipv4_addr) == TRUE) {
				tunnel_node->tunnel_cfg.dir = CS_TUNNEL_DIR_OUTBOUND;
				DBG(printk("%s:%d upstream\n",
					__func__, __LINE__));
			} else if (p_tunnel_cfg->dest_addr.afi == CS_IPV4 &&
				is_ppp_ipv4_check(p->pppoe_ifindex,
				p_tunnel_cfg->dest_addr.ip_addr.ipv4_addr) == TRUE) {
				tunnel_node->tunnel_cfg.dir = CS_TUNNEL_DIR_INBOUND;
				DBG(printk("%s:%d downstream\n",
					__func__, __LINE__));
			} else {
				tunnel_node->tunnel_cfg.dir = CS_TUNNEL_DIR_TWO_WAY;
				ERR(printk("%s:%d unknown direction\n",
					__func__, __LINE__));
			}
			if (p->tu_h == NULL) {
				p->tu_h = tunnel_node;
				p->tu_cnt = 1;
			} else {
				t2->next = tunnel_node;
				tunnel_node->prev = t2;
				p->tu_cnt++;
			}
			tunnel_node->pppoe_port = p;
			cs_pppoe_cb.tt_cnt++;

			DBG(printk("%s:%d tunnel ID = %d, %d tunnels in PPPoE"
				" port %d, total tunnels = %d\n",
				__func__, __LINE__, *p_tunnel_id, p->tu_cnt,
				p->pppoe_port_id, cs_pppoe_cb.tt_cnt));

			if (tunnel_node->tunnel_cfg.dir == CS_TUNNEL_DIR_INBOUND) {
				/* create tunnel hash */
				if (cs_iplip_down_tunnel_hash_create(tunnel_node) != CS_OK) {
					ERR(printk("%s:%d Can't create tunnel hash\n",
						__func__, __LINE__));
					return CS_ERROR;
				}
				/* create control tunnel hash */
				if (cs_iplip_pe2cpu_tunnel_hash_create(tunnel_node) != CS_OK) {
					ERR(printk("%s:%d Can't create tunnel hash\n",
						__func__, __LINE__));
					return CS_ERROR;
				}
			} else	if (tunnel_node->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND) {
				/* create tunnel hash */
				if (cs_iplip_up_tunnel_hash_create(tunnel_node) != CS_OK) {
					ERR(printk("%s:%d Can't create tunnel hash\n",
						__func__, __LINE__));
					return CS_ERROR;
				}
			}

			return CS_OK;
		}
		p = p->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;

}

cs_status_t cs_iplip_tunnel_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_cfg_t				*p_tunnel_cfg)
{
	cs_pppoe_port_entry_t *p;
	cs_tunnel_entry_t *t;

	DBG(printk("%s:%d device_id=%d, p_tunnel_cfg = 0x%p\n",
		__func__, __LINE__, device_id, p_tunnel_cfg));
	if (p_tunnel_cfg == NULL)
		return CS_ERROR;

	p = cs_pppoe_cb.port_list_hdr;

	while (p) {
		/* search by PPPoE port ID */
		if (p->pppoe_port_id == p_tunnel_cfg->tx_port) {
			t = p->tu_h;
			while (t) {
				/* search by tid */
				if (t->tunnel_cfg.tunnel.l2tp.tid ==
						p_tunnel_cfg->tunnel.l2tp.tid) {
					if (t->se_cnt > 0) {
						ERR(printk("%s:%d still have %d sessions\n",
							__func__, __LINE__,
							t->se_cnt));
						return CS_ERROR;
					}

					if (t == p->tu_h)
						p->tu_h = t->next;
					else
						t->prev->next = t->next;

					if (t->next != NULL)
						t->next->prev = t->prev;
					
					p->tu_cnt--;
					cs_pppoe_cb.tt_cnt--;
					cs_pppoe_cb.tu_list[t->tunnel_id] = NULL;

					
					/* delete tunnel hash */
					if (t->fwd_hash_idx[0])
						cs_iplip_up_tunnel_hash_del(t);
					if (t->fwd_hash_idx[1])
						cs_iplip_down_tunnel_hash_del(t);
					if (t->fwd_hash_idx[2])
						cs_iplip_pe2cpu_tunnel_hash_del(t);
					
					cs_free(t);
					return CS_OK;
				}
				t = t->next;
			}
			
			ERR(printk("%s:%d no entry is found\n",
				__func__, __LINE__));
			return CS_ERR_ENTRY_NOT_FOUND;
		}
		p = p->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}

cs_status_t cs_iplip_tunnel_delete_by_idx(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id)
{
	cs_pppoe_port_entry_t *p;
	cs_tunnel_entry_t *t;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d\n",
		__func__, __LINE__, device_id, tunnel_id));
	

	if (tunnel_id >= CS_TUNNEL_TBL_SIZE) {
		ERR(printk("%s:%d invalid tunnel ID\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	if (t->se_cnt > 0) {
		ERR(printk("%s:%d still have %d sessions\n",
			__func__, __LINE__, t->se_cnt));
		return CS_ERROR;
	}

	p = t->pppoe_port;
	if (t == p->tu_h)
		p->tu_h = t->next;
	else
		t->prev->next = t->next;
	
	if (t->next != NULL)
		t->next->prev = t->prev;
	
	p->tu_cnt--;
	cs_pppoe_cb.tt_cnt--;
	cs_pppoe_cb.tu_list[tunnel_id] = NULL;

	/* delete tunnel hash */
	if (t->fwd_hash_idx[0])
		cs_iplip_up_tunnel_hash_del(t);
	if (t->fwd_hash_idx[1])
		cs_iplip_down_tunnel_hash_del(t);
	if (t->fwd_hash_idx[2])
		cs_iplip_pe2cpu_tunnel_hash_del(t);

	cs_free(t);
	return CS_OK;
}

cs_status_t cs_iplip_tunnel_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_OUT	cs_tunnel_cfg_t				*p_tunnel_cfg)
{
	cs_tunnel_entry_t *t;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, p_tunnel_cfg = 0x%p\n",
		__func__, __LINE__, device_id, tunnel_id, p_tunnel_cfg));

	if (tunnel_id >= CS_TUNNEL_TBL_SIZE) {
		ERR(printk("%s:%d invalid tunnel ID\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	if (p_tunnel_cfg == NULL)
		return CS_ERROR;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	memcpy(p_tunnel_cfg, &t->tunnel_cfg, sizeof(cs_tunnel_cfg_t));

	return CS_OK;
}

cs_status_t cs_iplip_l2tp_session_add(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*session_node, *s, *s2;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x\n",
		__func__, __LINE__, device_id, tunnel_id, session_id));

	if (tunnel_id >= CS_TUNNEL_TBL_SIZE)
		return CS_ERR_ENTRY_NOT_FOUND;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	s = s2 = t->se_h;

	/* go through whole list, and get the last node in s2 */
	while (s) {
		/* check for duplicate session ID */
		if (s->session_id == session_id) {
			ERR(printk("%s:%d duplicate session ID\n",
				__func__, __LINE__));
			return CS_ERROR;
		}
		s2 = s;
		s = s->next;
	}

	session_node = cs_zalloc(sizeof(cs_l2tp_session_entry_t), GFP_KERNEL);
	if (session_node == NULL) {
		ERR(printk("%s:%d out of memory\n",
			__func__, __LINE__));
		return CS_ERROR;
	}
	
	session_node->session_id = session_id;
	if (t->se_h == NULL) {
		t->se_h = session_node;
		t->se_cnt = 1;
	} else {
		s2->next = session_node;
		session_node->prev = s2;
		t->se_cnt++;
	}
	session_node->tunnel = t;
	cs_pppoe_cb.ts_cnt++;

	/* set IPLIP entry and send IPC message to PE */
	if (cs_iplip_entry_add(session_node) != CS_OK) {
		ERR(printk("%s:%d Can't add IPLIP entry\n",
			__func__, __LINE__));
		return CS_ERROR;
	}
	
	return CS_OK;
}

cs_status_t cs_iplip_l2tp_session_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x\n",
		__func__, __LINE__, device_id, tunnel_id, session_id));

	if (tunnel_id >= CS_TUNNEL_TBL_SIZE)
		return CS_ERR_ENTRY_NOT_FOUND;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	s = t->se_h;

	/* go through whole list */
	while (s) {
		/* search by session ID */
		if (s->session_id == session_id) {
			if (s->ip_cnt > 0) {
				ERR(printk("%s:%d still have %d ip prefix\n",
					__func__, __LINE__, s->ip_cnt));
				return CS_ERROR;
			}
			
			if (s == t->se_h)
				t->se_h = s->next;
			else
				s->prev->next = s->next;
			
			if (s->next != NULL)
				s->next->prev = s->prev;
			
			t->se_cnt--;
			cs_pppoe_cb.ts_cnt--;

			/* delete IPLIP entry */
			if (cs_iplip_entry_del(s->iplip_idx) != CS_OK) {
				ERR(printk("%s:%d Can't delete IPLIP entry %d\n",
					__func__, __LINE__, s->iplip_idx));
				return CS_ERROR;
			}

			cs_free(s);
			return CS_OK;
		}
		
		s = s->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}

cs_status_t cs_iplip_l2tp_session_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id,
	CS_OUT	cs_boolean_t				*is_present)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x,"
		" is_present = 0x%p\n", __func__, __LINE__,
		device_id, tunnel_id, session_id, is_present));
	if (is_present == NULL)
		return CS_ERROR;
	*is_present = FALSE;

	if (tunnel_id >= CS_TUNNEL_TBL_SIZE)
		return CS_ERR_ENTRY_NOT_FOUND;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	s = t->se_h;

	/* go through whole list */
	while (s) {
		/* search by session ID */
		if (s->session_id == session_id) {
			*is_present = TRUE;
			return CS_OK;
		}
		
		s = s->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;

}

cs_status_t 
cs_ipv6_over_l2tp_add(
		CS_IN cs_dev_id_t       device_id,
		CS_IN cs_tunnel_id_t    tunnel_id,
		CS_IN cs_session_id_t   session_id,
		CS_IN cs_ip_address_t   *ipv6_prefix
		)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;
	cs_ip_address_entry_t *ip_node, *n, *n2;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x,"
		" ipv6_prefix = 0x%p\n", __func__, __LINE__,
		device_id, tunnel_id, session_id, ipv6_prefix));
	if (tunnel_id >= CS_TUNNEL_TBL_SIZE)
		return CS_ERR_ENTRY_NOT_FOUND;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	if (ipv6_prefix == NULL)
		return CS_ERROR;

	s = t->se_h;

	/* if session_id is 0, we assign to the 1st session_id */
	if ((s != NULL) && (session_id == 0))
		session_id = s->session_id;

	/* go through whole list */
	while (s) {
		/* search by session ID */
		if (s->session_id == session_id) {
			n = n2 = s->ip_h;
			
			/* go through whole list, and get the last node in n2 */
			while (n) {
				/* check for duplicate ip prefix */
				if (memcmp(&n->ip, ipv6_prefix,
						sizeof(cs_ip_address_t)) == 0) {
					ERR(printk("%s:%d duplicate ip prefix\n",
							__func__, __LINE__));
					return CS_ERROR;
				}
				n2 = n;
				n = n->next;
			}
			
			ip_node = cs_zalloc(sizeof(cs_ip_address_entry_t), GFP_KERNEL);
			if (ip_node == NULL) {
				ERR(printk("%s:%d out of memory\n",
					__func__, __LINE__));
				return CS_ERROR;
			}

			memcpy(&ip_node->ip, ipv6_prefix, sizeof(cs_ip_address_t));
			
			if (s->ip_h == NULL) {
				s->ip_h = ip_node;
				s->ip_cnt = 1;
			} else {
				n2->next = ip_node;
				ip_node->prev = n2;
				s->ip_cnt++;
			}
			ip_node->session = s;
			t->ti_cnt++;

			/* create flow hash */
			if (cs_iplip_flow_hash_create(ip_node) != CS_OK) {
				ERR(printk("%s:%d Can't create flow hash\n",
					__func__, __LINE__));
				return CS_ERROR;
			}

			return CS_OK;
		}
		
		s = s->next;
	}

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(cs_ipv6_over_l2tp_add);

cs_status_t 
cs_ipv6_over_l2tp_delete(
		CS_IN cs_dev_id_t       device_id,
   		CS_IN cs_tunnel_id_t    tunnel_id,
		CS_IN cs_session_id_t   session_id,
		CS_IN cs_ip_address_t   *ipv6_prefix
		)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;
	cs_ip_address_entry_t *n;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x,"
		" ipv6_prefix = 0x%p\n", __func__, __LINE__,
		device_id, tunnel_id, session_id, ipv6_prefix));
	if (tunnel_id >= CS_TUNNEL_TBL_SIZE)
		return CS_ERR_ENTRY_NOT_FOUND;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	if (ipv6_prefix == NULL)
		return CS_ERROR;

	s = t->se_h;

	/* if session_id is 0, we assign to the 1st session_id */
	if ((s != NULL) && (session_id == 0))
		session_id = s->session_id;

	/* go through whole list */
	while (s) {
		/* search by session ID */
		if (s->session_id == session_id) {
			n = s->ip_h;
			
			/* go through whole list */
			while (n) {
				/* search by ip prefix */
				if (memcmp(&n->ip, ipv6_prefix,
						sizeof(cs_ip_address_t)) == 0) {
					if (n == s->ip_h)
						s->ip_h = n->next;
					else
						n->prev->next = n->next;
					
					if (n->next != NULL)
						n->next->prev = n->prev;
					
					s->ip_cnt--;
					t->ti_cnt--;

					
					/* delete flow hash */
					if (cs_iplip_flow_hash_del(n) != CS_OK) {
						ERR(printk("%s:%d Can't delete flow hash for IPLIP index %d\n",
							__func__, __LINE__, s->iplip_idx));
						return CS_ERROR;
					}

					
					cs_free(n);
					return CS_OK;
				}
				n = n->next;
			}
			
			ERR(printk("%s:%d no such ip prefix\n",
				__func__, __LINE__));
			return CS_ERR_ENTRY_NOT_FOUND;
		}
		
		s = s->next;
	}

	ERR(printk("%s:%d no such session\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(cs_ipv6_over_l2tp_delete);

cs_status_t 
cs_ipv6_over_l2tp_getnext(
		CS_IN  cs_dev_id_t      device_id,
   		CS_IN  cs_tunnel_id_t   tunnel_id,
		CS_IN  cs_session_id_t  session_id,
		CS_OUT cs_ip_address_t  *ipv6_prefix
		)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;
	cs_ip_address_entry_t *n;
	cs_ip_address_t	ip_zero;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x,"
		" ipv6_prefix = 0x%p\n", __func__, __LINE__,
		device_id, tunnel_id, session_id, ipv6_prefix));
	if (tunnel_id >= CS_TUNNEL_TBL_SIZE)
		return CS_ERR_ENTRY_NOT_FOUND;

	t = cs_pppoe_cb.tu_list[tunnel_id];
	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel entry\n", __func__, __LINE__));
		return CS_ERR_ENTRY_NOT_FOUND;
	}

	if (ipv6_prefix == NULL)
		return CS_ERROR;

	s = t->se_h;

	/* if session_id is 0, we assign to the 1st session_id */
	if ((s != NULL) && (session_id == 0))
		session_id = s->session_id;

	memset(&ip_zero, 0, sizeof(cs_ip_address_t));

	/* go through whole list */
	while (s) {
		/* search by session ID */
		if (s->session_id == session_id) {
			n = s->ip_h;

			/* if the input ip prefix is all 0s,
			   we return the 1st ip prefix */
			if ((n != NULL) &&
				(memcmp(ipv6_prefix, &ip_zero, sizeof(cs_ip_address_t)) == 0)) {
				memcpy(ipv6_prefix, &n->ip, sizeof(cs_ip_address_t));
				return CS_OK;
			}
			
			/* go through whole list */
			while (n) {
				/* search by ip prefix */
				if (memcmp(ipv6_prefix, &n->ip,
						sizeof(cs_ip_address_t)) == 0) {
					if (n->next != NULL) {
						memcpy(ipv6_prefix,
							&n->next->ip,
							sizeof(cs_ip_address_t));
						return CS_OK;
					}
					ERR(printk("%s:%d already the last ip prefix\n",
							__func__, __LINE__));
					return CS_ERR_ENTRY_NOT_FOUND;
				}
				n = n->next;
			}
			
			ERR(printk("%s:%d can't find the ip prefix\n",
				__func__, __LINE__));
			return CS_ERR_ENTRY_NOT_FOUND;
		}
		
		s = s->next;
	}

	ERR(printk("%s:%d can't find the session\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(cs_ipv6_over_l2tp_getnext);


/* internal APIs*/
#ifdef CS_IPC_ENABLED
/* IPC related */
static int cs_iplip_ipc_send(
		unsigned short msg_type,
		const void *msg_data,
		unsigned short msg_size)
{
	int ret;

	ret = g2_ipc_send(cs_ipc_iplip_ctxt,
			 CPU_RCPU1,
			 CS_IPLIP_IPC_CLNT_ID,
			 G2_IPC_HPRIO,
			 msg_type,
			 msg_data,
			 msg_size);
	if (G2_IPC_OK != ret)
		return -1;

	return 0;
}

int
cs_iplip_ipc_send_reset(void)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_RESET, NULL, 0);
}

int
cs_iplip_ipc_send_stop(void)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_STOP, NULL, 0);
}


int
cs_iplip_ipc_send_set_entry1(
		unsigned char		idx,
		cs_iplip_entry1_t 	*iplip_entry
		)
{
	cs_iplip_ipc_msg_set1_t	msg;

	DBG(printk("%s:%d idx = %d, iplip_entry = 0x%p\n",
		__func__, __LINE__, idx, iplip_entry));
	memset(&msg, 0, sizeof(cs_iplip_ipc_msg_set1_t));
	msg.idx = idx;
	msg.l2tp_type = CS_L2TP_TYPE_1;
	memcpy(&msg.iplip_entry, iplip_entry, sizeof(cs_iplip_entry1_t));
	
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_SET_ENTRY, &msg,
				sizeof(cs_iplip_ipc_msg_set1_t));
}

int
cs_iplip_ipc_send_set_entry2(
		unsigned char		idx,
		cs_iplip_entry2_t 	*iplip_entry
		)
{
	cs_iplip_ipc_msg_set2_t	msg;

	DBG(printk("%s:%d idx = %d, iplip_entry = 0x%p\n",
		__func__, __LINE__, idx, iplip_entry));
	memset(&msg, 0, sizeof(cs_iplip_ipc_msg_set2_t));
	msg.idx = idx;
	msg.l2tp_type = CS_L2TP_TYPE_2;
	memcpy(&msg.iplip_entry, iplip_entry, sizeof(cs_iplip_entry2_t));
	
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_SET_ENTRY, &msg,
				sizeof(cs_iplip_ipc_msg_set2_t));
}

int
cs_iplip_ipc_send_set_entry3(
		unsigned char		idx,
		cs_iplip_entry3_t 	*iplip_entry
		)
{
	cs_iplip_ipc_msg_set3_t	msg;

	DBG(printk("%s:%d idx = %d, iplip_entry = 0x%p\n",
		__func__, __LINE__, idx, iplip_entry));
	memset(&msg, 0, sizeof(cs_iplip_ipc_msg_set3_t));
	msg.idx = idx;
	msg.l2tp_type = CS_L2TP_TYPE_3;
	memcpy(&msg.iplip_entry, iplip_entry, sizeof(cs_iplip_entry3_t));
	
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_SET_ENTRY, &msg,
				sizeof(cs_iplip_ipc_msg_set3_t));
}

int
cs_iplip_ipc_send_set_entry4(
		unsigned char		idx,
		cs_iplip_entry4_t 	*iplip_entry
		)
{
	cs_iplip_ipc_msg_set4_t	msg;

	DBG(printk("%s:%d idx = %d, iplip_entry = 0x%p\n",
		__func__, __LINE__, idx, iplip_entry));
	memset(&msg, 0, sizeof(cs_iplip_ipc_msg_set4_t));
	msg.idx = idx;
	msg.l2tp_type = CS_L2TP_TYPE_4;
	memcpy(&msg.iplip_entry, iplip_entry, sizeof(cs_iplip_entry4_t));
	
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_SET_ENTRY, &msg,
				sizeof(cs_iplip_ipc_msg_set4_t));
}

static int convert_iplip_entry_to_real_hdr(
		void			*dst,
		cs_iplip_entry_t 	*src,
		cs_l2tp_type_t		type
		)
{
	cs_iplip_entry1_t *dst1;
	cs_iplip_entry2_t *dst2;
	cs_iplip_entry3_t *dst3;
	cs_iplip_entry4_t *dst4;

	DBG(printk("%s:%d: dst = 0x%p, src = 0x%p, type = %d\n", __func__, __LINE__, dst, src, type));
	switch (type) {
	case CS_L2TP_TYPE_1:
		dst1 = (cs_iplip_entry1_t *)dst;
		memset(dst1, 0, sizeof(cs_iplip_entry1_t));
		dst1->crc32 = src->crc32;
		memcpy(&dst1->iplip_hdr.ethh, &src->iplip_hdr.ethh, sizeof(struct ethhdr));
		memcpy(&dst1->iplip_hdr.pppoeh, &src->iplip_hdr.pppoeh, sizeof(cs_pppoe_hdr_t));
		dst1->iplip_hdr.ppp2h = src->iplip_hdr.ppp2h;
		memcpy(&dst1->iplip_hdr.iph, &src->iplip_hdr.iph, sizeof(struct iphdr));
		memcpy(&dst1->iplip_hdr.udph, &src->iplip_hdr.udph, sizeof(struct udphdr));
		memcpy(&dst1->iplip_hdr.ppph, &src->iplip_hdr.ppph, sizeof(cs_ppp_hdr_t));
		dst1->valid = src->valid;
		dst1->dir = src->dir;

		dst1->iplip_hdr.l2tph.ver = src->iplip_hdr.l2tph.ver;
		dst1->iplip_hdr.l2tph.tid = src->iplip_hdr.l2tph.tid;
		dst1->iplip_hdr.l2tph.sid = src->iplip_hdr.l2tph.sid;
		return 0;
	case CS_L2TP_TYPE_2:
#if 1
		dst2 = (cs_iplip_entry2_t *)dst;
		/* cs_iplip_entry_t is exactly the same as cs_iplip_entry2_t */
		memcpy(dst2, src, sizeof(cs_iplip_entry2_t));
#else
		dst2 = (cs_iplip_entry2_t *)dst;
		memset(dst2, 0, sizeof(cs_iplip_entry2_t);
		dst2->crc32 = src->crc32;
		memcpy(&dst2->iplip_hdr.ethh, &src->iplip_hdr.ethh, sizeof(struct ethhdr));
		memcpy(&dst2->iplip_hdr.pppoeh, &src->iplip_hdr.pppoeh, sizeof(cs_pppoe_hdr_t));
		dst2->iplip_hdr.ppp2h = src->iplip_hdr.ppp2h;
		memcpy(&dst2->iplip_hdr.iph, &src->iplip_hdr.iph, sizeof(struct iphdr));
		memcpy(&dst2->iplip_hdr.udph, &src->iplip_hdr.udph, sizeof(struct udphdr));
		memcpy(&dst2->iplip_hdr.ppph, &src->iplip_hdr.ppph, sizeof(cs_ppp_hdr_t));
		dst2->valid = src->valid;
		dst2->dir = src->dir;

		dst2->iplip_hdr.l2tph.ver = src->iplip_hdr.l2tph.ver;
		dst2->iplip_hdr.l2tph.len = src->iplip_hdr.l2tph.len;
		dst2->iplip_hdr.l2tph.tid = src->iplip_hdr.l2tph.tid;
		dst2->iplip_hdr.l2tph.sid = src->iplip_hdr.l2tph.sid;
#endif		
		return 0;
	case CS_L2TP_TYPE_3:
		dst3 = (cs_iplip_entry3_t *)dst;
		memset(dst3, 0, sizeof(cs_iplip_entry3_t));
		dst3->crc32 = src->crc32;
		memcpy(&dst3->iplip_hdr.ethh, &src->iplip_hdr.ethh, sizeof(struct ethhdr));
		memcpy(&dst3->iplip_hdr.pppoeh, &src->iplip_hdr.pppoeh, sizeof(cs_pppoe_hdr_t));
		dst3->iplip_hdr.ppp2h = src->iplip_hdr.ppp2h;
		memcpy(&dst3->iplip_hdr.iph, &src->iplip_hdr.iph, sizeof(struct iphdr));
		memcpy(&dst3->iplip_hdr.udph, &src->iplip_hdr.udph, sizeof(struct udphdr));
		memcpy(&dst3->iplip_hdr.ppph, &src->iplip_hdr.ppph, sizeof(cs_ppp_hdr_t));
		dst3->valid = src->valid;
		dst3->dir = src->dir;

		dst3->iplip_hdr.l2tph.ver = src->iplip_hdr.l2tph.ver;
		dst3->iplip_hdr.l2tph.tid = src->iplip_hdr.l2tph.tid;
		dst3->iplip_hdr.l2tph.sid = src->iplip_hdr.l2tph.sid;
		dst3->iplip_hdr.l2tph.offset = 0;

		return 0;
	case CS_L2TP_TYPE_4:
		dst4 = (cs_iplip_entry4_t *)dst;
		memset(dst4, 0, sizeof(cs_iplip_entry4_t));
		dst4->crc32 = src->crc32;
		memcpy(&dst4->iplip_hdr.ethh, &src->iplip_hdr.ethh, sizeof(struct ethhdr));
		memcpy(&dst4->iplip_hdr.pppoeh, &src->iplip_hdr.pppoeh, sizeof(cs_pppoe_hdr_t));
		dst4->iplip_hdr.ppp2h = src->iplip_hdr.ppp2h;
		memcpy(&dst4->iplip_hdr.iph, &src->iplip_hdr.iph, sizeof(struct iphdr));
		memcpy(&dst4->iplip_hdr.udph, &src->iplip_hdr.udph, sizeof(struct udphdr));
		memcpy(&dst4->iplip_hdr.ppph, &src->iplip_hdr.ppph, sizeof(cs_ppp_hdr_t));
		dst4->valid = src->valid;
		dst4->dir = src->dir;

		dst4->iplip_hdr.l2tph.ver = src->iplip_hdr.l2tph.ver;
		dst4->iplip_hdr.l2tph.len = src->iplip_hdr.l2tph.len;
		dst4->iplip_hdr.l2tph.tid = src->iplip_hdr.l2tph.tid;
		dst4->iplip_hdr.l2tph.sid = src->iplip_hdr.l2tph.sid;
		dst4->iplip_hdr.l2tph.offset = 0;

		return 0;
	default:
		printk("%s:%d: unknown L2TP header type\n", __func__, __LINE__);
		return -1;
	}
	
}


int
cs_iplip_ipc_send_set_entry(
		unsigned char		idx,
		cs_iplip_entry_t 	*iplip_entry
		)
{
	cs_iplip_entry1_t iplip_entry1;
	cs_iplip_entry2_t iplip_entry2;
	cs_iplip_entry3_t iplip_entry3;
	cs_iplip_entry4_t iplip_entry4;
	cs_l2tp_ver_t l2tp_ver;

	DBG(printk("%s:%d: idx = %d, iplip_entry = 0x%p\n",
		__func__, __LINE__, idx, iplip_entry));

	//printk("iplip_hdr = 0x%p, offset = %d\n", &iplip_entry->iplip_hdr, ((int) &iplip_entry->iplip_hdr) - ((int)iplip_entry));
	//printk("l2tph = 0x%p, offset = %d\n", &iplip_entry->iplip_hdr.l2tph, ((int) &iplip_entry->iplip_hdr.l2tph) - ((int)iplip_entry));
	printk("%s:%d: ver = 0x%x\n", __func__, __LINE__, ntohs(iplip_entry->iplip_hdr.l2tph.ver));
	l2tp_ver.u16 = ntohs(iplip_entry->iplip_hdr.l2tph.ver);
	//l2tp_ver.u16 = 0x4002;

	/***********************
	 CS_L2TP_TYPE_1 = 1,	  L=0, O=0
	 CS_L2TP_TYPE_2 = 2,	  L=1, O=0
	 CS_L2TP_TYPE_3 = 3,	  L=0, O=1, Offset=0
	 CS_L2TP_TYPE_4 = 4,	  L=1, O=1, Offset=0
	 ***********************/
	if (l2tp_ver.bits.l == 0 && l2tp_ver.bits.o == 0) {
		convert_iplip_entry_to_real_hdr(&iplip_entry1, iplip_entry, CS_L2TP_TYPE_1);
		return cs_iplip_ipc_send_set_entry1(idx, &iplip_entry1);
	} else if (l2tp_ver.bits.l == 1 && l2tp_ver.bits.o == 0) {
		convert_iplip_entry_to_real_hdr(&iplip_entry2, iplip_entry, CS_L2TP_TYPE_2);
		return cs_iplip_ipc_send_set_entry2(idx, &iplip_entry2);
	} else if (l2tp_ver.bits.l == 0 && l2tp_ver.bits.o == 1) {
		convert_iplip_entry_to_real_hdr(&iplip_entry3, iplip_entry, CS_L2TP_TYPE_3);
		return cs_iplip_ipc_send_set_entry3(idx, &iplip_entry3);
	} else /* if (l2tp_ver.bits.l == 1 && l2tp_ver.bits.o == 1) */ {
		convert_iplip_entry_to_real_hdr(&iplip_entry4, iplip_entry, CS_L2TP_TYPE_4);
		return cs_iplip_ipc_send_set_entry4(idx, &iplip_entry4);
	}
}


int
cs_iplip_ipc_send_del_entry(
		unsigned char		idx
		)
{
	cs_iplip_ipc_msg_del_t	msg;

	DBG(printk("%s:%d idx = %d\n", __func__, __LINE__, idx));
	memset(&msg, 0, sizeof(cs_iplip_ipc_msg_del_t));
	msg.idx = idx;
	
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_DEL_ENTRY, &msg,
				sizeof(cs_iplip_ipc_msg_del_t));
}

int
cs_iplip_ipc_send_dump(void)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_DUMP_TBL, NULL, 0);
}


int
cs_iplip_ipc_send_echo(void)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_ECHO, NULL, 0);
}


int
cs_iplip_ipc_send_mib_en(
		unsigned char		enbl
		)
{
	cs_iplip_ipc_msg_en_t	msg;

	DBG(printk("%s:%d enbl = %d\n", __func__, __LINE__, enbl));
	memset(&msg, 0, sizeof(cs_iplip_ipc_msg_en_t));
	msg.enbl = enbl;
	
	return cs_iplip_ipc_send(CS_IPLIP_IPC_PE_MIB_EN, &msg,
				sizeof(cs_iplip_ipc_msg_en_t));
}


static int cs_iplip_ipc_rcv_reset_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return 0;
}

static int cs_iplip_ipc_rcv_stop_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return 0;
}

static int cs_iplip_ipc_rcv_set_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return 0;
}

static int cs_iplip_ipc_rcv_del_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	return 0;
}

static int cs_iplip_ipc_rcv_echo_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	DBG(printk("%s:%d\n", __func__, __LINE__));
	printk("<IPLIP>: Receive ECHO from PE %d", peer.cpu_id - 1);
	return 0;
}

struct g2_ipc_msg cs_iplip_ipc_procs[] = {
	{CS_IPLIP_IPC_PE_RESET_ACK, (unsigned long) cs_iplip_ipc_rcv_reset_ack},
	{CS_IPLIP_IPC_PE_STOP_ACK, (unsigned long) cs_iplip_ipc_rcv_stop_ack},
	{CS_IPLIP_IPC_PE_SET_ACK, (unsigned long)cs_iplip_ipc_rcv_set_ack},
	{CS_IPLIP_IPC_PE_DEL_ACK, (unsigned long)cs_iplip_ipc_rcv_del_ack},
	{CS_IPLIP_IPC_PE_ECHO_ACK, (unsigned long)cs_iplip_ipc_rcv_echo_ack}
};

static int cs_iplip_ipc_register(void)
{
	short status;

	status = g2_ipc_register(CS_IPLIP_IPC_CLNT_ID, cs_iplip_ipc_procs,
				 5, 0, NULL, &cs_ipc_iplip_ctxt);
	if (status != G2_IPC_OK) {
		printk("%s::Failed to register IPC for CS tunnel acceleration\n",
			__func__);
		return -1;
	} else
		printk("%s::successfully register IPC for CS tunnel acceleration\n",
			__func__);

	return 0;
}

static void cs_iplip_ipc_deregister(void)
{
	g2_ipc_deregister(cs_ipc_iplip_ctxt);
	printk("%s::Done deregister IPC for CS tunnel acceleration\n", __func__);
}

#endif /* CS_IPC_ENABLED */



void cs_hw_accel_iplip_init(void)
{
	
	memset(&cs_pppoe_cb, 0, sizeof(cs_pppoe_port_list_t));
	memset(&cs_iplip_tbl, 0, sizeof(cs_iplip_tbl_t));

#ifdef CS_IPC_ENABLED
	cs_iplip_ipc_register();
	cs_iplip_ipc_send_reset();
#endif
}

void cs_hw_accel_iplip_exit(void)
{
	/* stop all the accelerated tunnel */

	/* deregister the hook function from core */

#ifdef CS_IPC_ENABLED
	cs_iplip_ipc_send_stop();
	cs_iplip_ipc_deregister();
#endif
}



