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
#include <linux/in.h>
#include <linux/if_ether.h>
#include <mach/cs_types.h>
#include <mach/cs_network_types.h>
#include <mach/cs_rule_hash_api.h>
#include <mach/cs_flow_api.h>

#include "cs_core_vtable.h"
#include "cs_core_hmu.h"

#include <cs752x_proc.h>
extern cs_uint32_t cs_adapt_debug;

cs_status_t cs_flow_hash_add(cs_rule_hash_t *p_rule_hash, cs_flow_t *p_flow);
cs_status_t cs_flow_qos_hash_add(cs_rule_hash_t *p_rule_hash, cs_flow_t *p_flow);
cs_status_t cs_flow_hash_get(cs_uint16_t hash_index, cs_flow_t *p_flow);
cs_status_t cs_flow_hash_delete(cs_uint16_t hash_index);
cs_status_t cs_flow_hash_get_lastuse_tickcount(cs_uint16_t flow_id, cs_uint32_t *lastuse_tickcount);

static void cs_flow_dump(cs_flow_t *p_flow)
{
#ifdef CONFIG_CS752X_PROC
        int i;

        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("=== cs_flow_t ===\n");
		printk("%s: flow_id=%d\n", __func__, p_flow->flow_id);
		printk("%s: flow_type=%d\n", __func__, p_flow->flow_type);
		
		printk("=== cs_pkt_info_t for ingress ===\n");
		printk("%s: phy_port=%d\n", __func__, p_flow->ingress_pkt.phy_port);
		printk("%s: da_mac=", __func__);
		for (i = 0; i < 6; i++) {
                        printk("[0x%x]-", (unsigned int)(p_flow->ingress_pkt.da_mac[i]));
                }
                printk("\n");
		
		printk("%s: sa_mac=", __func__);
                for (i = 0; i < 6; i++) {
                        printk("[0x%x]-", (unsigned int)(p_flow->ingress_pkt.sa_mac[i]));
                }
                printk("\n");

		printk("%s: tpid_encap_type[0]=0x%x\n", __func__, p_flow->ingress_pkt.tag[0].tpid_encap_type);
		printk("%s: vlan_id[0]=0x%x\n", __func__, p_flow->ingress_pkt.tag[0].vlan_id);
		printk("%s: priority[0]=0x%x\n", __func__, (unsigned int)(p_flow->ingress_pkt.tag[0].priority));

		printk("%s: tpid_encap_type[1]=0x%x\n", __func__, p_flow->ingress_pkt.tag[1].tpid_encap_type);
                printk("%s: vlan_id[1]=0x%x\n", __func__, p_flow->ingress_pkt.tag[1].vlan_id);
                printk("%s: priority[1]=0x%x\n", __func__, (unsigned int)(p_flow->ingress_pkt.tag[1].priority));

                printk("%s: pppoe_session_id=0x%x\n", __func__, p_flow->ingress_pkt.pppoe_session_id);
		
                printk("%s: DA afi=0x%x\n", __func__, p_flow->ingress_pkt.da_ip.afi);
                printk("%s: DA ip_addr[0]=0x%x\n", __func__, p_flow->ingress_pkt.da_ip.ip_addr.addr[0]);
                printk("%s: DA ip_addr[1]=0x%x\n", __func__, p_flow->ingress_pkt.da_ip.ip_addr.addr[1]);
                printk("%s: DA ip_addr[2]=0x%x\n", __func__, p_flow->ingress_pkt.da_ip.ip_addr.addr[2]);
                printk("%s: DA ip_addr[3]=0x%x\n", __func__, p_flow->ingress_pkt.da_ip.ip_addr.addr[3]);
                printk("%s: DA addr_len=%d\n", __func__, (unsigned int)(p_flow->ingress_pkt.da_ip.addr_len));
	
                printk("%s: SA afi=0x%x\n", __func__, p_flow->ingress_pkt.sa_ip.afi);
                printk("%s: SA ip_addr[0]=0x%x\n", __func__, p_flow->ingress_pkt.sa_ip.ip_addr.addr[0]);
                printk("%s: SA ip_addr[1]=0x%x\n", __func__, p_flow->ingress_pkt.sa_ip.ip_addr.addr[1]);
                printk("%s: SA ip_addr[2]=0x%x\n", __func__, p_flow->ingress_pkt.sa_ip.ip_addr.addr[2]);
                printk("%s: SA ip_addr[3]=0x%x\n", __func__, p_flow->ingress_pkt.sa_ip.ip_addr.addr[3]);
                printk("%s: SA addr_len=%d\n", __func__, (unsigned int)(p_flow->ingress_pkt.sa_ip.addr_len));	

                printk("%s: tos=0x%x\n", __func__, (unsigned int)(p_flow->ingress_pkt.tos));	
                printk("%s: protocol=0x%x\n", __func__, (unsigned int)(p_flow->ingress_pkt.protocol));	
                printk("%s: spi_idx=0x%x\n", __func__, (unsigned int)(p_flow->ingress_pkt.l4_header.esp.spi_idx));	

                printk("%s: natt_4_bytes=0x%x\n", __func__, (unsigned int)(p_flow->ingress_pkt.natt_4_bytes));	
	
		printk("=== cs_pkt_info_t for egress ===\n");
                printk("%s: phy_port=%d\n", __func__, p_flow->egress_pkt.phy_port);
                printk("%s: da_mac=", __func__);
                for (i = 0; i < 6; i++) {
                        printk("[0x%x]-", (unsigned int)(p_flow->egress_pkt.da_mac[i]));
                }
		printk("\n");

                printk("%s: sa_mac=", __func__);
                for (i = 0; i < 6; i++) {
                        printk("[0x%x]-", (unsigned int)(p_flow->egress_pkt.sa_mac[i]));
                }
                printk("\n");

                printk("%s: tpid_encap_type[0]=0x%x\n", __func__, p_flow->egress_pkt.tag[0].tpid_encap_type);
                printk("%s: vlan_id[0]=0x%x\n", __func__, p_flow->egress_pkt.tag[0].vlan_id);
                printk("%s: priority[0]=0x%x\n", __func__, (unsigned int)(p_flow->egress_pkt.tag[0].priority));

                printk("%s: tpid_encap_type[1]=0x%x\n", __func__, p_flow->egress_pkt.tag[1].tpid_encap_type);
                printk("%s: vlan_id[1]=0x%x\n", __func__, p_flow->egress_pkt.tag[1].vlan_id);
                printk("%s: priority[1]=0x%x\n", __func__, (unsigned int)(p_flow->egress_pkt.tag[1].priority));

                printk("%s: pppoe_session_id=0x%x\n", __func__, p_flow->egress_pkt.pppoe_session_id);

                printk("%s: DA afi=0x%x\n", __func__, p_flow->egress_pkt.da_ip.afi);
                printk("%s: DA ip_addr[0]=0x%x\n", __func__, p_flow->egress_pkt.da_ip.ip_addr.addr[0]);
                printk("%s: DA ip_addr[1]=0x%x\n", __func__, p_flow->egress_pkt.da_ip.ip_addr.addr[1]);
                printk("%s: DA ip_addr[2]=0x%x\n", __func__, p_flow->egress_pkt.da_ip.ip_addr.addr[2]);
                printk("%s: DA ip_addr[3]=0x%x\n", __func__, p_flow->egress_pkt.da_ip.ip_addr.addr[3]);
                printk("%s: DA addr_len=%d\n", __func__, (unsigned int)(p_flow->egress_pkt.da_ip.addr_len));

                printk("%s: SA afi=0x%x\n", __func__, p_flow->egress_pkt.sa_ip.afi);
                printk("%s: SA ip_addr[0]=0x%x\n", __func__, p_flow->egress_pkt.sa_ip.ip_addr.addr[0]);
                printk("%s: SA ip_addr[1]=0x%x\n", __func__, p_flow->egress_pkt.sa_ip.ip_addr.addr[1]);
                printk("%s: SA ip_addr[2]=0x%x\n", __func__, p_flow->egress_pkt.sa_ip.ip_addr.addr[2]);
                printk("%s: SA ip_addr[3]=0x%x\n", __func__, p_flow->egress_pkt.sa_ip.ip_addr.addr[3]);
                printk("%s: SA addr_len=%d\n", __func__, (unsigned int)(p_flow->egress_pkt.sa_ip.addr_len));

                printk("%s: tos=0x%x\n", __func__, (unsigned int)(p_flow->egress_pkt.tos));
                printk("%s: protocol=0x%x\n", __func__, (unsigned int)(p_flow->egress_pkt.protocol));
                printk("%s: spi_idx=0x%x\n", __func__, (unsigned int)(p_flow->egress_pkt.l4_header.esp.spi_idx));

                printk("%s: natt_4_bytes=0x%x\n", __func__, (unsigned int)(p_flow->egress_pkt.natt_4_bytes));	

                printk("%s: dec_ttl=%d\n", __func__, p_flow->dec_ttl);

                printk("%s: voq_offset=0x%x\n", __func__, (unsigned int)(p_flow->voq_offset));

                printk("%s: life_time=0x%x\n", __func__, (unsigned int)(p_flow->life_time));

		for (i = 0; i < CS_FLOW_SWID_MAX; i++) {
			printk("%s: swid_array[%d]=0x%x\n", __func__, i, p_flow->swid_array[i]);
		}
	}
#endif
}

/* ATTENTION:
 * Never set any unnecessary value in cs_flow_t if the hash mask (flow type) doesn't need that value.
 * Otherwise, hash entry might be wrong, packet can't hit the hash.
 */
cs_status_t cs_flow_add(CS_IN cs_dev_id_t device_id, CS_IN_OUT cs_flow_t *p_flow)
{
	cs_rule_hash_t rule_hash;
	cs_int16_t tpid_encap_type, tpid_encap_type1;
	cs_uint16_t vlan_id, vlan_id1;
	int ret;

	cs_flow_dump(p_flow);	

	memset(&rule_hash, 0 , sizeof(cs_rule_hash_t));

	/* fill cs_rule_hash_t */
	switch (p_flow->flow_type) {
		case CS_FLOW_TYPE_L2:
			rule_hash.apptype = CORE_FWD_APP_TYPE_L2_FLOW;
			break;
		case CS_FLOW_TYPE_L4:
			if (p_flow->ingress_pkt.eth_type == ETH_P_IPV6)
				rule_hash.apptype = CORE_FWD_APP_TYPE_L3_GENERIC_WITH_CHKSUM;
			else
				rule_hash.apptype = CORE_FWD_APP_TYPE_L3_GENERIC;
			break;
		case CS_FLOW_TYPE_L3_MC:
			rule_hash.apptype = CORE_FWD_APP_TYPE_L3_MCAST;
			break;
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
		case CS_FLOW_TYPE_L4_NATT:
			rule_hash.apptype = CORE_FWD_APP_TYPE_L7_GENERIC;
			break;
#endif
#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS) || defined(CONFIG_CS75XX_HW_ACCEL_L2TP_PASS)
		case CS_FLOW_TYPE_L2_PASS:
			rule_hash.apptype = CORE_FWD_APP_TYPE_L2_PASS;
			break;
#endif
		case CS_FLOW_TYPE_MC_MEMBER:
			rule_hash.apptype = CORE_FWD_APP_TYPE_MCAST_CTRL_IP_SA;
			break;
		case CS_FLOW_TYPE_MC_L7_FILTER:
			if (p_flow->ingress_pkt.natt_4_bytes == 0)
				rule_hash.apptype = CORE_FWD_APP_TYPE_MCAST_CTRL_IPTV;
			else
				rule_hash.apptype = CORE_FWD_APP_TYPE_MCAST_L7_FILTER;
			break;
		case CS_FLOW_TYPE_MC_HASH:
			if (p_flow->ingress_pkt.natt_4_bytes == 0)
				rule_hash.apptype = CORE_FWD_APP_TYPE_MCAST_CTRL_IPTV;
			else
				rule_hash.apptype = CORE_FWD_APP_TYPE_MCAST_L7_FILTER;
			break;
		default:
			printk("%s: unknown flow type=%d!!\n", __func__, p_flow->flow_type);
			return CS_ERROR;
	}
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
                printk("%s: rule_hash.apptype=0x%x\n", __func__, rule_hash.apptype);
        }
#endif

	memcpy(&(rule_hash.key.mac_da[0]), &(p_flow->ingress_pkt.da_mac[0]), CS_ETH_ADDR_LEN);
	memcpy(&(rule_hash.key.mac_sa[0]), &(p_flow->ingress_pkt.sa_mac[0]), CS_ETH_ADDR_LEN);
	if ((p_flow->flow_type != CS_FLOW_TYPE_MC_MEMBER) &&
		(p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
		(p_flow->flow_type != CS_FLOW_TYPE_MC_HASH)) {
		rule_hash.key.eth_type = p_flow->ingress_pkt.eth_type;
	}

	tpid_encap_type = p_flow->ingress_pkt.tag[0].tpid_encap_type;
	if (tpid_encap_type == CS_VLAN_TPID_8100 || tpid_encap_type == CS_VLAN_TPID_9100 ||
	    tpid_encap_type == CS_VLAN_TPID_88A8 || tpid_encap_type == CS_VLAN_TPID_9200) {
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: tpid_encap_type1=0x%x\n", __func__, tpid_encap_type);
		}
#endif
		if ((p_flow->flow_type != CS_FLOW_TYPE_MC_MEMBER) &&
			(p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
			(p_flow->flow_type != CS_FLOW_TYPE_MC_HASH)) {
			rule_hash.key.tpid_enc_1 = 0x4 | tpid_encap_type;   /* only need to turn on the MSB of TPID */
			rule_hash.key._8021p_1 = p_flow->ingress_pkt.tag[0].priority & 0x7;
			rule_hash.key.vid_1 = p_flow->ingress_pkt.tag[0].vlan_id;
		}
	}

	tpid_encap_type = p_flow->ingress_pkt.tag[1].tpid_encap_type;
	if (tpid_encap_type == CS_VLAN_TPID_8100 || tpid_encap_type == CS_VLAN_TPID_9100 ||
	    tpid_encap_type == CS_VLAN_TPID_88A8 || tpid_encap_type == CS_VLAN_TPID_9200) {
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: tpid_encap_type2=0x%x\n", __func__, tpid_encap_type);
		}
#endif
		if ((p_flow->flow_type != CS_FLOW_TYPE_MC_MEMBER) &&
			(p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
			(p_flow->flow_type != CS_FLOW_TYPE_MC_HASH)) {
			rule_hash.key.tpid_enc_2 = 0x4 | tpid_encap_type;   /* only need to turn on the MSB of TPID */
			rule_hash.key._8021p_2 = p_flow->ingress_pkt.tag[1].priority & 0x7;
			rule_hash.key.vid_2 = p_flow->ingress_pkt.tag[1].vlan_id;
		}
	}

	if (rule_hash.apptype != CORE_FWD_APP_TYPE_L2_FLOW) {
		rule_hash.key.ip_valid = 1;
		memcpy(&(rule_hash.key.da[0]), &(p_flow->ingress_pkt.da_ip.ip_addr.addr[0]), sizeof(cs_l3_ip_addr));
		memcpy(&(rule_hash.key.sa[0]), &(p_flow->ingress_pkt.sa_ip.ip_addr.addr[0]), sizeof(cs_l3_ip_addr));
		rule_hash.key.ip_prot = p_flow->ingress_pkt.protocol;

		rule_hash.key.ip_version = p_flow->ingress_pkt.sa_ip.afi;

		switch (p_flow->ingress_pkt.protocol) {
			case IPPROTO_TCP:
				rule_hash.key.l4_valid = 1;
				rule_hash.key.l4_dp = p_flow->ingress_pkt.l4_header.tcp.dport;
				rule_hash.key.l4_sp = p_flow->ingress_pkt.l4_header.tcp.sport;
				break;
			case IPPROTO_UDP:
				rule_hash.key.l4_valid = 1;
				rule_hash.key.l4_dp = p_flow->ingress_pkt.l4_header.udp.dport;
                        	rule_hash.key.l4_sp = p_flow->ingress_pkt.l4_header.udp.sport;
				break;
			case IPPROTO_ESP:
				rule_hash.key.spi_vld = 1;
				rule_hash.key.spi_idx = htonl(p_flow->ingress_pkt.l4_header.esp.spi_idx);
				break;
		}
	}
	else {
		rule_hash.key.ip_valid = 0;
		if (p_flow->ingress_pkt.eth_type == ETH_P_IP || p_flow->ingress_pkt.eth_type == ETH_P_IPV6 || p_flow->ingress_pkt.eth_type == ETH_P_PPP_SES) {
			rule_hash.key.ip_valid = 1;
		}
	}
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	if (rule_hash.apptype == CORE_FWD_APP_TYPE_TUNNEL_L4_L7) 
		rule_hash.key.lspid = p_flow->ingress_pkt.phy_port;
#endif

	if ((p_flow->flow_type != CS_FLOW_TYPE_MC_MEMBER) &&
		(p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
		(p_flow->flow_type != CS_FLOW_TYPE_MC_HASH)) {
	rule_hash.key.dscp = (p_flow->ingress_pkt.tos >> 2) & 0x3f;
	rule_hash.key.pppoe_session_id_valid = p_flow->ingress_pkt.pppoe_session_id_valid;
	rule_hash.key.pppoe_session_id = p_flow->ingress_pkt.pppoe_session_id;
	}

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	if (rule_hash.apptype == CORE_FWD_APP_TYPE_L7_GENERIC) {
		rule_hash.key.l7_field_valid = 1;
		rule_hash.key.l7_field = p_flow->ingress_pkt.natt_4_bytes;
	}
#endif
	if (rule_hash.apptype == CORE_FWD_APP_TYPE_MCAST_L7_FILTER) {
		rule_hash.key.l7_field_valid = 1;
		rule_hash.key.l7_field = p_flow->ingress_pkt.natt_4_bytes;
	}

	/* prepare the forward result */
	if (memcmp(&(p_flow->ingress_pkt.da_mac[0]), &(p_flow->egress_pkt.da_mac[0]), CS_ETH_ADDR_LEN) != 0) {
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: Forward Result L2 DA MAC\n", __func__);
		}
#endif
		rule_hash.fwd_result.l2.mac_da_replace_en = 1;
		memcpy(&(rule_hash.fwd_result.l2.mac_da[0]), &(p_flow->egress_pkt.da_mac[0]), CS_ETH_ADDR_LEN);
	}

	if (memcmp(&(p_flow->ingress_pkt.sa_mac[0]), &(p_flow->egress_pkt.sa_mac[0]), CS_ETH_ADDR_LEN) != 0) {
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: Forward Result L2 SA MAC\n", __func__);
		}
#endif
		rule_hash.fwd_result.l2.mac_sa_replace_en = 1;
		memcpy(&(rule_hash.fwd_result.l2.mac_sa[0]), &(p_flow->egress_pkt.sa_mac[0]), CS_ETH_ADDR_LEN);
	}

	if (p_flow->ingress_pkt.pppoe_session_id != p_flow->egress_pkt.pppoe_session_id) {
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: Forward Result PPPoE session ID\n", __func__);
		}
#endif
		/* do not support replace PPPoE sseion id */
		if (p_flow->ingress_pkt.pppoe_session_id_valid != 0 && p_flow->egress_pkt.pppoe_session_id_valid != 0) {
			printk("%s: Does not support PPPoE session id replacement!!\n", __func__);
			return CS_ERROR;
		}

		if (p_flow->ingress_pkt.pppoe_session_id_valid == 0 && p_flow->egress_pkt.pppoe_session_id_valid != 0) {
			/* add encap PPPoE session */
			rule_hash.fwd_result.l2.pppoe_encap_en = 1;
                	rule_hash.voq_pol.pppoe_session_id = p_flow->egress_pkt.pppoe_session_id;
		}
		else if (p_flow->ingress_pkt.pppoe_session_id_valid != 0 && p_flow->egress_pkt.pppoe_session_id_valid == 0) {
			 /* add decap PPPoE session */
                        rule_hash.fwd_result.l2.pppoe_decap_en = 1;
                        rule_hash.voq_pol.pppoe_session_id = p_flow->egress_pkt.pppoe_session_id;
		}
	}

	tpid_encap_type = p_flow->ingress_pkt.tag[0].tpid_encap_type;
	tpid_encap_type1 = p_flow->egress_pkt.tag[0].tpid_encap_type;
	vlan_id = p_flow->ingress_pkt.tag[0].vlan_id;
	vlan_id1 = p_flow->egress_pkt.tag[0].vlan_id;
	if (tpid_encap_type != tpid_encap_type1 || vlan_id != vlan_id1) {
        	if (tpid_encap_type == CS_VLAN_TPID_8100 || tpid_encap_type == CS_VLAN_TPID_9100 ||
            		tpid_encap_type == CS_VLAN_TPID_88A8 || tpid_encap_type == CS_VLAN_TPID_9200) {
#ifdef CONFIG_CS752X_PROC
                	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
                		printk("%s: first tpid_encap_type=%d, tpid_encap_type1=%d, vlan_id=0x%x, vlan_id1=0x%x\n",
					__func__, tpid_encap_type, tpid_encap_type1, vlan_id, vlan_id1);
			}
#endif

		  	rule_hash.fwd_result.l2.flow_vlan_op_en = 1;
			if (tpid_encap_type1 == CS_VLAN_TPID_NONE) {
				/* remove vlan tag */
				rule_hash.fvlan.first_vlan_cmd = CS_FE_VLAN_CMD_POP_B;
				rule_hash.fvlan.first_vid = vlan_id;
				rule_hash.fvlan.first_tpid_enc = tpid_encap_type;
			}
			else {
				/* swap the vlan tag */
                                rule_hash.fvlan.first_vlan_cmd = CS_FE_VLAN_CMD_SWAP_B;
                                rule_hash.fvlan.first_vid = vlan_id1;
                                rule_hash.fvlan.first_tpid_enc = tpid_encap_type1;
			}
		}
		else {
#ifdef CONFIG_CS752X_PROC
                	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: first tpid_encap_type=%d, tpid_encap_type1=%d, vlan_id=0x%x, vlan_id1=0x%x\n",
                                	__func__, tpid_encap_type, tpid_encap_type1, vlan_id, vlan_id1);
			}
#endif

                        if (tpid_encap_type1 != CS_VLAN_TPID_NONE) {
                                /* insert vlan tag */
                        	rule_hash.fwd_result.l2.flow_vlan_op_en = 1;
                                rule_hash.fvlan.first_vlan_cmd = 7 /*CS_FE_VLAN_CMD_PUSH_C*/;
                                rule_hash.fvlan.first_vid = vlan_id1;
                                rule_hash.fvlan.first_tpid_enc = tpid_encap_type1;
                        }
		}
        }

	tpid_encap_type = p_flow->ingress_pkt.tag[1].tpid_encap_type;
        tpid_encap_type1 = p_flow->egress_pkt.tag[1].tpid_encap_type;
        vlan_id = p_flow->ingress_pkt.tag[1].vlan_id;
        vlan_id1 = p_flow->egress_pkt.tag[1].vlan_id;
        if (tpid_encap_type != tpid_encap_type1 || vlan_id != vlan_id1) {
                if (tpid_encap_type == CS_VLAN_TPID_8100 || tpid_encap_type == CS_VLAN_TPID_9100 ||
                        tpid_encap_type == CS_VLAN_TPID_88A8 || tpid_encap_type == CS_VLAN_TPID_9200) {
#ifdef CONFIG_CS752X_PROC
                	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
                        	printk("%s: second tpid_encap_type=%d, tpid_encap_type1=%d, vlan_id=0x%x, vlan_id1=0x%x\n",
                                	__func__, tpid_encap_type, tpid_encap_type1, vlan_id, vlan_id1);
			}
#endif
                        rule_hash.fwd_result.l2.flow_vlan_op_en = 1;
                        if (tpid_encap_type1 == CS_VLAN_TPID_NONE) {
                                /* remove vlan tag */
				rule_hash.fvlan.second_vlan_cmd = CS_FE_VLAN_CMD_POP_B;
#if 0
				if (rule_hash.fvlan.first_vlan_cmd == CS_FE_VLAN_CMD_POP_B)
					rule_hash.fvlan.second_vlan_cmd = CS_FE_VLAN_CMD_POP_B; /*POP ALL*/
				else
					rule_hash.fvlan.second_vlan_cmd = CS_FE_VLAN_CMD_POP_D; /*POP INNER*/
#endif
				rule_hash.fvlan.second_vid = vlan_id;
				rule_hash.fvlan.second_tpid_enc = tpid_encap_type;
                        }
                        else {
                                /* swap the vlan tag */
                                rule_hash.fvlan.second_vlan_cmd = CS_FE_VLAN_CMD_SWAP_B;
                                rule_hash.fvlan.second_vid = vlan_id1;
                                rule_hash.fvlan.second_tpid_enc = tpid_encap_type1;
                        }
                }
                else {
#ifdef CONFIG_CS752X_PROC
                	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
                        	printk("%s: first tpid_encap_type=%d, tpid_encap_type1=%d, vlan_id=0x%x, vlan_id1=0x%x\n",
                                	__func__, tpid_encap_type, tpid_encap_type1, vlan_id, vlan_id1);
			}
#endif

                        if (tpid_encap_type1 != CS_VLAN_TPID_NONE) {
                                /* insert vlan tag */
                                rule_hash.fwd_result.l2.flow_vlan_op_en = 1;
                                rule_hash.fvlan.second_vlan_cmd = CS_FE_VLAN_CMD_PUSH_B;
                                rule_hash.fvlan.second_vid = vlan_id1;
                                rule_hash.fvlan.second_tpid_enc = tpid_encap_type1;
                        }
                }
        }

	if (memcmp(&(p_flow->ingress_pkt.da_ip.ip_addr.addr[0]), &(p_flow->egress_pkt.da_ip.ip_addr.addr[0]), sizeof(cs_l3_ip_addr)) != 0) {
#ifdef CONFIG_CS752X_PROC
               	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: Forward Result L3 DA IP\n", __func__);
		}
#endif
               	rule_hash.fwd_result.l3.ip_da_replace_en = 1;
               	memcpy(&(rule_hash.fwd_result.l3.ip_da[0]), &(p_flow->egress_pkt.da_ip.ip_addr.addr[0]), sizeof(cs_l3_ip_addr));
       	}

	if (memcmp(&(p_flow->ingress_pkt.sa_ip.ip_addr.addr[0]), &(p_flow->egress_pkt.sa_ip.ip_addr.addr[0]), sizeof(cs_l3_ip_addr)) != 0) {
#ifdef CONFIG_CS752X_PROC
               	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: Forward Result L3 SA IP\n", __func__);
		}
#endif
               	rule_hash.fwd_result.l3.ip_sa_replace_en = 1;
               	memcpy(&(rule_hash.fwd_result.l3.ip_sa[0]), &(p_flow->egress_pkt.sa_ip.ip_addr.addr[0]), sizeof(cs_l3_ip_addr));
       	}

	rule_hash.fwd_result.l3.decr_ttl_hoplimit = p_flow->dec_ttl;

	if (p_flow->ingress_pkt.protocol == IPPROTO_TCP) {
		if (p_flow->ingress_pkt.l4_header.tcp.dport != p_flow->egress_pkt.l4_header.tcp.dport) {
#ifdef CONFIG_CS752X_PROC
               		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: Forward Result L4 TCP dport\n", __func__);
			}
#endif
			rule_hash.fwd_result.l4.dp_replace_en = 1;
			rule_hash.fwd_result.l4.dp = p_flow->egress_pkt.l4_header.tcp.dport;
		}
		if (p_flow->ingress_pkt.l4_header.tcp.sport != p_flow->egress_pkt.l4_header.tcp.sport) {
#ifdef CONFIG_CS752X_PROC
               		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: Forward Result L4 TCP sport\n", __func__);
			}
#endif
                       	rule_hash.fwd_result.l4.sp_replace_en = 1;
                       	rule_hash.fwd_result.l4.sp = p_flow->egress_pkt.l4_header.tcp.sport;
               	}
	}

	if (p_flow->ingress_pkt.protocol == IPPROTO_UDP) {
		if (p_flow->ingress_pkt.l4_header.udp.dport != p_flow->egress_pkt.l4_header.udp.dport) {
#ifdef CONFIG_CS752X_PROC
               		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: Forward Result L4 UDP dport\n", __func__);
			}
#endif
               		rule_hash.fwd_result.l4.dp_replace_en = 1;
                       	rule_hash.fwd_result.l4.dp = p_flow->egress_pkt.l4_header.udp.dport;
               	}
               	if (p_flow->ingress_pkt.l4_header.udp.sport != p_flow->egress_pkt.l4_header.udp.sport) {
#ifdef CONFIG_CS752X_PROC
               		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: Forward Result L4 UDP sport\n", __func__);
			}
#endif
                       	rule_hash.fwd_result.l4.sp_replace_en = 1;
                       	rule_hash.fwd_result.l4.sp = p_flow->egress_pkt.l4_header.udp.sport;
               	}
       	}

	if ((p_flow->flag & FLOW_FLAG_QOS_POL) == FLOW_FLAG_QOS_POL) {
		rule_hash.fwd_result.dest.voq_policy = 1;
	}

	/* if the phy_port of egress packet is 0xff, it means the packet should be dropped */
	if (p_flow->egress_pkt.phy_port == 0xff) {
#ifdef CONFIG_CS752X_PROC
        	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: phy_port of egress packet is 0xff, packet will be dropped!!!\n", __func__);
		}
#endif
		rule_hash.fwd_result.act.drop = 1;
	}
	else {
		switch (p_flow->egress_pkt.phy_port) {
		case CS_PORT_GMAC0:
		case CS_PORT_GMAC1:
		case CS_PORT_GMAC2:
		case CS_PORT_OFLD0:
		case CS_PORT_OFLD1:
		case CS_PORT_CPU:
			rule_hash.voq_pol.voq_base = (p_flow->egress_pkt.phy_port * 8) + p_flow->voq_offset;
			break;
		case CS_PORT_CUSTOM0:
		case CS_PORT_CUSTOM1:
		case CS_PORT_CUSTOM2:
			rule_hash.voq_pol.voq_base = p_flow->voq_offset; /* absolute voq */
			break;
		}
	}

	if (p_flow->sw_action_id != 0) {
		rule_hash.fwd_result.l3.ip_sa_replace_en = 0;
		rule_hash.fwd_result.l3.ip_da_replace_en = 0;
		rule_hash.fwd_result.l3.ip_sa_index = p_flow->sw_action_id & 0x0fff;
		rule_hash.fwd_result.l3.ip_da_index = (p_flow->sw_action_id >> 12) & 0x00ff;
	}

	ret = cs_flow_hash_add(&rule_hash, p_flow);
	if (ret != CS_OK) {
                //printk("%s: cs_flow_hash_add() failed, ret=%d\n", __func__, ret);
                return CS_ERROR;
        }

	p_flow->flow_id = rule_hash.hash_index;

	if (rule_hash.fwd_result.act.drop == 0) {
		ret = cs_flow_qos_hash_add(&rule_hash, p_flow);
		if (ret != CS_OK) {
			cs_flow_hash_delete(rule_hash.hash_index);
			printk("%s: cs_flow_qos_hash_add() failed, ret=%d\n", __func__, ret);
			return CS_ERROR;
		}
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_add);

cs_status_t cs_flow_get(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t hash_index, CS_OUT cs_flow_t *p_flow)
{
	int ret;

	ret = cs_flow_hash_get(hash_index, p_flow);

	if (ret != CS_OK) {
		printk("%s: cs_flow_hash_get(hash_index=%d) failed, ret=%d\n", __func__, hash_index, ret);
		return CS_ERROR;	
	}

	cs_flow_dump(p_flow);

	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_get);

cs_status_t cs_flow_delete(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t flow_id)
{
	int ret;

        ret = cs_flow_hash_delete(flow_id);

        if (ret != CS_OK) {
                printk("%s: cs_flow_hash_delete(hash_index=%d) failed, ret=%d\n", __func__, flow_id, ret);
                return CS_ERROR;
        }

	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_delete);

cs_status_t cs_flow_get_lastuse_tickcount(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t flow_id, CS_OUT cs_uint32_t *lastuse_tickcount)
{
        int ret;

        ret = cs_flow_hash_get_lastuse_tickcount(flow_id, lastuse_tickcount);

        if (ret != CS_OK) {
                printk("%s: cs_flow_hash_get_lastuse_tickcount(hash_index=%d) failed, ret=%d\n", __func__, flow_id, ret);
                return CS_ERROR;
        }

        return CS_OK;
}
EXPORT_SYMBOL(cs_flow_get_lastuse_tickcount);

