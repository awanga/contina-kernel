/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_core_logic.c
 *
 * $Id$
 *
 * It contains the Main Control Unit for core logic.
 */

#include <asm-generic/cputime.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <net/ipv6.h>
#include "cs_core_vtable.h"
#include "cs_core_hmu.h"
#include "cs_core_logic.h"
#include <mach/cs75xx_fe_core_table.h>
#include "cs_fe.h"
#include "cs_core_fastnet.h"
#include "cs_hw_accel_forward.h"
#include "cs_hw_accel_manager.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_core_rule_hmu.h"
#include "cs752x_eth.h"
#include <mach/cs_mcast.h>
#include "cs_mut.h"
#ifdef CONFIG_CS75XX_MTU_CHECK
#include <mach/cs_mtu.h>
extern cs_port_id_t cs_wan_port_id;
#endif
#define MAX_FWD_HASH_CNT	4

#ifdef CONFIG_CS752X_ACCEL_KERNEL
extern u32 cs_ne_default_lifetime;
#else
u32 cs_ne_default_lifetime = 0;
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ne_core_logic_debug;
#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
extern u32 cs_fe_double_chk;
#endif
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_MCU) x;}
#else
u32 cs_fe_double_chk = 0;

#define DBG(x) { }
#endif

#define PROTO_IGMP	0x02
#define PROTO_ICMPV6	0x3A

extern u8 cs_ni_get_port_id(struct net_device *dev);
void cs_dhcp_init(void);
bool set_dhcp_port_hash(u16 port,int ieth,u8 dhcp_dport_hm_idx,unsigned int dhcp_fwdrslt_idx[GE_PORT_NUM]);
extern cs_vtable_t *vtable_list[CORE_VTABLE_TYPE_MAX];
extern u32 cs_hw_ipsec_offload_mode;
extern u32 cs_vpn_offload_mode;

extern int cs_mcast_init(void);

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
#include "cs_hw_accel_ipsec.h"
#endif

#ifdef CONFIG_CS752X_ACCEL_KERNEL
#include "cs_hw_accel_vlan.h"
#include "cs_hw_accel_manager.h"
#include "cs_hw_accel_pppoe.h"
#include "cs_hw_accel_dscp.h"   //Bug#40322
#else

u32 cs_accel_hw_accel_enable(u32 mod_mask)
{
	return 0;
}

u32 cs_accel_fastnet_enable(u32 mod_mask)
{
	return 0;
}
#endif

typedef enum {
	CS_CORE_HW_EXCEPT_TTL_EQUAL_1 = 0,
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	CS_CORE_HW_EXCEPT_MC_PKT_TTL_EQUAL_255,
#endif
	CS_CORE_HW_EXCEPT_IP_OPTIONS,
	CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC0,
	CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC1,
	CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC2,
	CS_CORE_HW_MCIDX2,
	CS_CORE_HW_MCIDX7,
	CS_CORE_HW_EXCEPT_MAX,
} cs_core_hw_exception_rules_e;

#define ACL_RULE_IDX_INVALID 0xFF
unsigned int cs_acl_rule_idx[CS_CORE_HW_EXCEPT_MAX];

void set_fwd_hash_result_index_to_cs_cb(cs_kernel_accel_cb_t *cs_cb,
		cs_fwd_hash_t *fwd_hash);

fe_fwd_result_entry_t l4port_fwdrslt_entry[GE_PORT_NUM];
fe_voq_pol_entry_t l4port_voqpol_entry[GE_PORT_NUM];
unsigned int l4port_fwdrslt_idx[GE_PORT_NUM],  l4port_voqpol_idx[GE_PORT_NUM];
__u8 l4_sport_hm_idx = ~(0x0);
__u8 l4_dport_hm_idx = ~(0x0);

typedef struct {
	u16 l4port;
	u16 hash_index[GE_PORT_NUM*2];
} cs_l4port_hash_t;

cs_l4port_hash_t tcp_l4port_hash_tbl[MAX_PORT_LIST_SIZE];
cs_l4port_hash_t udp_l4port_hash_tbl[MAX_PORT_LIST_SIZE];

/* control block operation APIs */
/* initialize CB at the input */
int cs_core_logic_input_set_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb;
	struct ethhdr *eth;
	int i;

	if (skb == NULL)
		return -1;

	cs_cb = CS_KERNEL_SKB_CB(skb);
	if (cs_cb == NULL)
		return -1;

	memset(cs_cb, 0x0, sizeof(cs_kernel_accel_cb_t));

	cs_cb->common.tag = CS_CB_TAG;
	cs_cb->common.sw_only = CS_SWONLY_DONTCARE;
	cs_cb->common.module_mask = 0;
	cs_cb->common.input_dev = skb->dev;
	/* the setting is moved to ni_complete_rx_instance */
//	cs_cb->common.ingress_port_id = cs_ni_get_port_id(skb->dev);
	cs_cb->common.sw_action_id = 0;
	cs_cb->common.dec_ttl = 0;

	for (i = 0; i < CS_SWID64_MOD_MAX; i++)
		cs_cb->common.swid[i] = CS_INVALID_SWID64;
	cs_cb->common.swid_cnt = 0;

	cs_cb->lifetime = secs_to_cputime(cs_ne_default_lifetime);

	eth = (struct ethhdr *)skb->data;
	memcpy(cs_cb->input.raw.da, eth->h_dest, ETH_ALEN);
	memcpy(cs_cb->input.raw.sa, eth->h_source, ETH_ALEN);
	cs_cb->input.raw.eth_protocol = eth->h_proto;

	cs_cb->key_misc.mcidx = 0;
	cs_cb->key_misc.hw_fwd_type = CS_FWD_NORMAL;
	cs_cb->key_misc.mcgid = 0;
	// mcgid may change per vtable settings.

	cs_cb->action.acl_dsbl = false;

	cs_cb->action.voq_pol.d_voq_id = CS_DEFAULT_VOQ;
	cs_cb->action.voq_pol.d_pol_id = 0;
	cs_cb->action.voq_pol.cpu_pid = 0;
	cs_cb->action.voq_pol.cos_nop = 0;
	cs_cb->action.voq_pol.pppoe_session_id = 0;
	cs_cb->action.voq_pol.voq_policer_parity = false;

#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_vlan_set_input_cb(skb);
	cs_pppoe_kernel_set_input_cb(skb);
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS	
	cs_dscp_set_input_cb(skb);  //Bug#40322
#endif	
#endif

	return 0;
} /* cs_core_logic_input_set_cb */


/*
 * Identify forward application type based on flags in cs_cb.
 * We may update this function for netfilter/firewall function by
 * rule vtables.
 */
inline unsigned int get_app_type_from_module_type(unsigned int mod_mask)
{
	if (mod_mask & (CS_MOD_MASK_FROM_RE0 | CS_MOD_MASK_FROM_RE1))
		return CORE_FWD_APP_TYPE_IPSEC_FROM_RE;
	else if (mod_mask & (CS_MOD_MASK_IPV4_MULTICAST |
					CS_MOD_MASK_IPV6_MULTICAST))
		return CORE_FWD_APP_TYPE_L3_MCAST;
	else if (mod_mask & CS_MOD_MASK_L2_MULTICAST)
		return CORE_FWD_APP_TYPE_L2_MCAST;
	else if (mod_mask & CS_MOD_MASK_ETHERIP)
		return CORE_FWD_APP_TYPE_L2_FLOW;
	else if (mod_mask & CS_MOD_MASK_IPSEC)
		return CORE_FWD_APP_TYPE_L3_IPSEC;
	else if (mod_mask & CS_MOD_MASK_IPV6_ROUTING)
		return CORE_FWD_APP_TYPE_L3_GENERIC_WITH_CHKSUM;
	else if (mod_mask & (CS_MOD_MASK_IPV4_ROUTING |
				CS_MOD_MASK_IPV4_PROTOCOL | CS_MOD_MASK_NAT))
		return CORE_FWD_APP_TYPE_L3_GENERIC;
	else if (mod_mask & (CS_MOD_MASK_BRIDGE |
				CS_MOD_MASK_BRIDGE_NETFILTER))
		return CORE_FWD_APP_TYPE_L2_FLOW;
	else
		return CORE_FWD_APP_TYPE_NONE;
}

int cs_core_set_hash_key(u64 flag, cs_kernel_accel_cb_t *cb,
		fe_sw_hash_t *key, bool is_from_re)
{
	int i;
	struct cb_network_field * cb_key_info;
	if (is_from_re)
		cb_key_info = &cb->output;
	else
		cb_key_info = &cb->input;
	/* L2 */
	if (flag & CS_HM_MAC_SA_MASK) {
		for (i = 0; i < 6; i++)
			key->mac_sa[i] = cb_key_info->raw.sa[5 - i];
	}

	if (flag & CS_HM_MAC_DA_MASK) {
		for (i = 0; i < 6; i++)
			key->mac_da[i] = cb_key_info->raw.da[5 - i];
	}

	if (flag & CS_HM_ETHERTYPE_MASK)
		key->eth_type = ntohs(cb_key_info->raw.eth_protocol);

	if (flag & CS_HM_TPID_ENC_1_LSB_MASK) {
		switch (cb_key_info->raw.vlan_tpid) {
		case ETH_P_8021Q:
			key->tpid_enc_1 = 0;
			break;
		case 0x9100:
			key->tpid_enc_1 = 1;
			break;
		case 0x88a8:
			key->tpid_enc_1 = 2;
			break;
		case 0x9200:
			key->tpid_enc_1 = 3;
			break;
		default:
			key->tpid_enc_1 = 0;
		}
	}

	if (flag & CS_HM_TPID_ENC_1_MSB_MASK) {
		if ((cb_key_info->raw.vlan_tpid == ETH_P_8021Q) ||
			(cb_key_info->raw.vlan_tpid == 0x9100) ||
			(cb_key_info->raw.vlan_tpid == 0x88a8) ||
			(cb_key_info->raw.vlan_tpid == 0x9200))
			key->tpid_enc_1 = key->tpid_enc_1 | 4;
	}

	if (flag & CS_HM_TPID_ENC_2_LSB_MASK) {
		switch (cb_key_info->raw.vlan_tpid_2) {
		case ETH_P_8021Q:
			key->tpid_enc_2 = 0;
			break;
		case 0x9100:
			key->tpid_enc_2 = 1;
			break;
		case 0x88a8:
			key->tpid_enc_2 = 2;
			break;
		case 0x9200:
			key->tpid_enc_2 = 3;
			break;
		default:
			key->tpid_enc_2 = 0;
		}
	}

	if (flag & CS_HM_TPID_ENC_2_MSB_MASK) {
		if ((cb_key_info->raw.vlan_tpid_2 == ETH_P_8021Q) ||
			(cb_key_info->raw.vlan_tpid_2 == 0x9100) ||
			(cb_key_info->raw.vlan_tpid_2 == 0x88a8) ||
			(cb_key_info->raw.vlan_tpid_2 == 0x9200))
			key->tpid_enc_2 = key->tpid_enc_2 | 4;
	}

	if (flag & CS_HM_VID_1_MASK)
		key->vid_1 = cb_key_info->raw.vlan_tci & VLAN_VID_MASK;

	if (flag & CS_HM_VID_2_MASK)
		key->vid_2 = cb_key_info->raw.vlan_tci_2 & VLAN_VID_MASK;

	if (flag & CS_HM_8021P_1_MASK)
		key->_8021p_1 = (cb_key_info->raw.vlan_tci & VLAN_PRIO_MASK) >>
			VLAN_PRIO_SHIFT;

	if (flag & CS_HM_DEI_1_MASK)
		key->dei_1 = (cb_key_info->raw.vlan_tci >> 12) & 0x01;

	if (flag & CS_HM_8021P_2_MASK)
		key->_8021p_2 = (cb_key_info->raw.vlan_tci_2 & VLAN_PRIO_MASK) >>
			VLAN_PRIO_SHIFT;

	if (flag & CS_HM_DEI_2_MASK)
		key->dei_2 = (cb_key_info->raw.vlan_tci_2 >> 12) & 0x01;

	if ((flag & CS_HM_PPPOE_SESSION_ID_VLD_MASK) &&
			(flag & CS_HM_PPPOE_SESSION_ID_MASK)) {
		if (cb->common.module_mask & CS_MOD_MASK_PPPOE) {
			if (is_from_re) {
				if (cb->action.l2.pppoe_op_en ==
						CS_PPPOE_OP_INSERT) {
					key->pppoe_session_id_valid = 0;
					key->pppoe_session_id = 0;
				} else if (cb->action.l2.pppoe_op_en ==
						CS_PPPOE_OP_REMOVE) {
					key->pppoe_session_id_valid = 1;
					key->pppoe_session_id =
						ntohs(cb_key_info->raw.pppoe_frame);
				}
			} else {
				key->pppoe_session_id_valid =
					cb_key_info->raw.pppoe_frame_vld;
				key->pppoe_session_id =
					ntohs(cb_key_info->raw.pppoe_frame);
			}
		} else {
			key->pppoe_session_id_valid = 0;
			key->pppoe_session_id = 0;
		}
	}

	/* L3 */
	/* We will assume all the hash masks that is layer 3 or higher
	 * should have CS_HM_IP_VER_MASK in it. */
	if (flag & CS_HM_IP_VER_MASK) {
		if (cb_key_info->l3_nh.iph.ver == 4)
			key->ip_version = 0;
		else if (cb_key_info->l3_nh.iph.ver == 6)
			key->ip_version = 1;
	}

	if (flag & CS_HM_IP_DA_MASK) {
		if (cb_key_info->l3_nh.iph.ver == 4) {
			key->da[0] = ntohl(cb_key_info->l3_nh.iph.dip);
		} else if (cb_key_info->l3_nh.iph.ver == 6) {
			key->da[0] = ntohl(cb_key_info->l3_nh.ipv6h.dip[3]);
			key->da[1] = ntohl(cb_key_info->l3_nh.ipv6h.dip[2]);
			key->da[2] = ntohl(cb_key_info->l3_nh.ipv6h.dip[1]);
			key->da[3] = ntohl(cb_key_info->l3_nh.ipv6h.dip[0]);
		}
	}

	if (flag & CS_HM_IP_SA_MASK) {
		if (cb_key_info->l3_nh.iph.ver == 4) {
			key->sa[0] = ntohl(cb_key_info->l3_nh.iph.sip);
		} else if (cb_key_info->l3_nh.iph.ver == 6) {
			key->sa[0] = ntohl(cb_key_info->l3_nh.ipv6h.sip[3]);
			key->sa[1] = ntohl(cb_key_info->l3_nh.ipv6h.sip[2]);
			key->sa[2] = ntohl(cb_key_info->l3_nh.ipv6h.sip[1]);
			key->sa[3] = ntohl(cb_key_info->l3_nh.ipv6h.sip[0]);
		}
	}

	if (flag & CS_HM_IP_PROT_MASK) {
		if (cb_key_info->l3_nh.iph.ver == 4)
			key->ip_prot = cb_key_info->l3_nh.iph.protocol;
		else if (cb_key_info->l3_nh.iph.ver == 6)
			key->ip_prot = cb_key_info->l3_nh.ipv6h.protocol;
	}

	if (flag & CS_HM_IP_FRAGMENT_MASK)
		key->ip_frag = cb_key_info->l3_nh.iph.frag;

	/* assuming we only create hash that rejects checksum error */
	if (flag & CS_HM_L3_CHKSUM_ERR_MASK)
		key->l3_csum_err = 0;

	/* We only support IPv4 and IPv6, if we don't know the version,
	 * then it's not an IP packet */
	if (flag & CS_HM_IP_VLD_MASK) {
		if ((cb_key_info->l3_nh.iph.ver == 4) ||
				(cb_key_info->l3_nh.iph.ver == 6))
			key->ip_valid = 1;
	}

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
	if (flag & CS_HM_DSCP_MASK)
		key->dscp = (cb_key_info->l3_nh.iph.tos >> 2) & 0x3e;
#else
	if (flag & CS_HM_DSCP_MASK)
		key->dscp = (cb_key_info->l3_nh.iph.tos >> 2) & 0x3f;
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
	/* IPv4 and IPv6 should share the same data structure for
	 * IPv4->tos and IPv6->tc */

	if (flag & CS_HM_ECN_MASK)
		key->ecn = cb_key_info->l3_nh.iph.tos & 0x03;
	/* IPv4 and IPv6 should share the same data structure for
	 * IPv4->tos and IPv6->tc */

	/* IPv6 */
#if 0	// FIXME!! implement later.. for IPv6
	if (flag & CS_HM_IPV6_NDP_MASK)
		key->ipv6_ndp = cb->input.l3_nh.iph.;
#endif

	if ((flag & CS_HM_IPV6_HBH_MASK) &&
			(cb_key_info->l3_nh.iph.ver == 6) &&
			(cb_key_info->l3_nh.ipv6h.protocol == NEXTHDR_HOP))
		key->ipv6_hbh = 1;

	if ((flag & CS_HM_IPV6_RH_MASK) &&
			(cb_key_info->l3_nh.iph.ver == 6) &&
			(cb_key_info->l3_nh.ipv6h.protocol == NEXTHDR_ROUTING))
		key->ipv6_rh = 1;

	if ((flag & CS_HM_IPV6_DOH_MASK) &&
			(cb_key_info->l3_nh.iph.ver == 6) &&
			(cb_key_info->l3_nh.ipv6h.protocol == NEXTHDR_DEST))
		key->ipv6_doh = 1;

	if ((flag & CS_HM_IPV6_FLOW_LBL_MASK) &&
			(cb_key_info->l3_nh.iph.ver == 6))
		key->ipv6_flow_label = cb_key_info->l3_nh.ipv6h.flow_lbl[2] |
				(cb_key_info->l3_nh.ipv6h.flow_lbl[1] << 8) |
				((cb_key_info->l3_nh.ipv6h.flow_lbl[0] & 0x0f) << 16);

	/* L4 */
	if (flag & CS_HM_L4_DP_MASK) {
		// FIXME!! if flag & CS_HM_PORTS_RNGD case...
		if (cb_key_info->l3_nh.iph.ver == 4) {
			if (cb_key_info->l3_nh.iph.protocol == IPPROTO_TCP)
				key->l4_dp = ntohs(cb_key_info->l4_h.th.dport);
			else if (cb_key_info->l3_nh.iph.protocol == IPPROTO_UDP)
				key->l4_dp = ntohs(cb_key_info->l4_h.uh.dport);
		} else if (cb_key_info->l3_nh.iph.ver == 6) {
			if (cb_key_info->l3_nh.ipv6h.protocol == IPPROTO_TCP)
				key->l4_dp = ntohs(cb_key_info->l4_h.th.dport);
			else if (cb_key_info->l3_nh.ipv6h.protocol == IPPROTO_UDP)
				key->l4_dp = ntohs(cb_key_info->l4_h.uh.dport);
		}
	}

	if (flag & CS_HM_L4_SP_MASK) {
		// FIXME!! if flag & CS_HM_PORTS_RNGD case...
		if (cb_key_info->l3_nh.iph.ver == 4) {
			if (cb_key_info->l3_nh.iph.protocol == IPPROTO_TCP)
				key->l4_sp = ntohs(cb_key_info->l4_h.th.sport);
			else if (cb_key_info->l3_nh.iph.protocol == IPPROTO_UDP)
				key->l4_sp = ntohs(cb_key_info->l4_h.uh.sport);
		} else if (cb_key_info->l3_nh.iph.ver == 6) {
			if (cb_key_info->l3_nh.ipv6h.protocol == IPPROTO_TCP)
				key->l4_sp = ntohs(cb_key_info->l4_h.th.sport);
			else if (cb_key_info->l3_nh.ipv6h.protocol == IPPROTO_UDP)
				key->l4_sp = ntohs(cb_key_info->l4_h.uh.sport);
		}
	}

	if (flag & CS_HM_L4_VLD_MASK) {
		if ((cb_key_info->l3_nh.iph.ver == 4) &&
				((cb_key_info->l3_nh.iph.protocol == IPPROTO_TCP)
				 || (cb_key_info->l3_nh.iph.protocol ==
					 IPPROTO_UDP)))
			key->l4_valid = 1;
		else if ((cb_key_info->l3_nh.iph.ver == 6) &&
				((cb_key_info->l3_nh.ipv6h.protocol == IPPROTO_TCP)
				 || (cb_key_info->l3_nh.ipv6h.protocol ==
					 IPPROTO_UDP)))
			key->l4_valid = 1;
	}

#if 0	// FIXME!! how is this used?
	if (flag & CS_HM_L4_PORTS_RNGD)
		key->afds = afsa;
#endif

	/* IPsec */
	if ((flag & CS_HM_SPI_VLD_MASK) && (flag & CS_HM_SPI_MASK)) {
		if ((cb_key_info->l3_nh.iph.protocol == IPPROTO_ESP) ||
				(cb_key_info->l3_nh.iph.protocol == IPPROTO_AH)) {
			key->spi_idx = ntohl(cb_key_info->vpn_h.ah_esp.spi);
			key->spi_vld = 1;
		}
	}

	/* Miscellaneous */
	if (flag & CS_HM_ORIG_LSPID_MASK)
		key->orig_lspid = cb->key_misc.orig_lspid;
	if (flag & CS_HM_LSPID_MASK)
		key->lspid = cb->key_misc.lspid;
#ifdef CONFIG_CS75XX_MTU_CHECK
	key->pktlen_rng_match_vector = 0;
	if (flag & CS_HM_PKTLEN_RNG_MATCH_VECTOR_B0_MASK)
		key->pktlen_rng_match_vector |= 0x1;
	if (flag & CS_HM_PKTLEN_RNG_MATCH_VECTOR_B1_MASK)
		key->pktlen_rng_match_vector |= (0x1 << 1);	
	if (flag & CS_HM_PKTLEN_RNG_MATCH_VECTOR_B2_MASK)
		key->pktlen_rng_match_vector |= (0x1 << 2);	
	if (flag & CS_HM_PKTLEN_RNG_MATCH_VECTOR_B3_MASK)
		key->pktlen_rng_match_vector |= (0x1 << 3);
#endif
	if (flag & CS_HM_RECIRC_IDX_MASK)
		key->recirc_idx = cb->key_misc.recirc_idx;
	if (flag & CS_HM_MCIDX_MASK)
		key->mc_idx = cb->key_misc.mcidx;
	if (flag & CS_HM_MCGID_MASK)
		key->mcgid = cb->key_misc.mcgid;
#ifdef CONFIG_CS752X_ACCEL_KERNEL
	if (flag & CS_HM_L7_FIELD_MASK)
                key->l7_field = cb->key_misc.l7_field;
	if (flag & CS_HM_L7_FIELD_VLD_MASK)
                key->l7_field_valid = 1;
#endif

	// TODO: If there is any new Hask Mask that is used for
	// hash value calculation, please put it here!!

	return 0;
} /* cs_core_set_hash_key */

int set_fwd_hash_result(cs_kernel_accel_cb_t *cb,
		cs_fwd_hash_t *fwd_hash)
{
	fe_fwd_result_entry_t *action = &fwd_hash->action;
	fe_fwd_result_param_t *result = &fwd_hash->param;
	int i = 0;

	if (cb->output_mask & CS_HM_MAC_DA_MASK) {
		for (i = 0; i < 6; i++)
			result->mac[i] = cb->output.raw.da[5 - i];
		action->l2.mac_da_replace_en = CS_RESULT_ACTION_ENABLE;
	}
	if (cb->output_mask & CS_HM_MAC_SA_MASK) {
		for (i = 0; i < 6; i++)
			result->mac[6 + i] = cb->output.raw.sa[5 - i];
		action->l2.mac_sa_replace_en = CS_RESULT_ACTION_ENABLE;
	}

	// FIXME! modify VLAN later...
		if (cb->output_mask & CS_HM_VID_1_MASK) {
		result->first_vid = cb->output.raw.vlan_tci & VLAN_VID_MASK;

		if ((cb->input.raw.vlan_tpid == 0) &&
			(cb->output.raw.vlan_tpid != 0))
			/*INSERT VLAN*/
			result->first_vlan_cmd =
				CS_FE_VLAN_CMD_PUSH_C;
		else if ((cb->input.raw.vlan_tpid != 0) &&
				 (cb->output.raw.vlan_tpid == 0)) {

			if ((cb->input.raw.vlan_tci & VLAN_VID_MASK) == 0) {
				/*REMOVE Priority tag*/
				result->first_vlan_cmd =
					CS_FE_VLAN_CMD_SWAP_B;
				result->second_vlan_cmd =
					CS_FE_VLAN_CMD_POP_B;
				result->first_vid = 0xa; //set a fake vid for remove
			}
			else {
				/*REMOVE VLAN*/
				result->first_vlan_cmd =
					CS_FE_VLAN_CMD_POP_B;
			}
		}
		else {
			/*REPLACE VLAN*/
			result->first_vlan_cmd =
				CS_FE_VLAN_CMD_SWAP_B;
		}


		switch (cb->output.raw.vlan_tpid) {
			case ETH_P_8021Q:
				result->first_tpid_enc = 0;
				break;
			case 0x9100:
				result->first_tpid_enc = 1;
				break;
			case 0x88a8:
				result->first_tpid_enc = 2;
				break;
			case 0x9200:
				result->first_tpid_enc = 3;
				break;
		}
	}

	if (cb->output_mask & CS_HM_VID_2_MASK) {
		result->second_vid = cb->output.raw.vlan_tci_2 & VLAN_VID_MASK;
		switch (cb->output.raw.vlan_tpid_2) {
			case ETH_P_8021Q:
				result->second_tpid_enc = 0;
				break;
			case 0x9100:
				result->second_tpid_enc = 1;
				break;
			case 0x88a8:
				result->second_tpid_enc = 2;
				break;
			case 0x9200:
				result->second_tpid_enc = 3;
				break;
		}

		if ((cb->input.raw.vlan_tpid_2 == 0) &&
			(cb->output.raw.vlan_tpid_2 != 0)) {
			if ((cb->input.raw.vlan_tpid != 0) &&
				((cb->input.raw.vlan_tci & VLAN_VID_MASK) == 0) &&
				((cb->output.raw.vlan_tci_2 & VLAN_VID_MASK) == 0)) {
					/*INSERT 1st tag before priority tag*/
					result->first_vlan_cmd =
						CS_FE_VLAN_CMD_PUSH_C;
					result->second_vlan_cmd =
						0;
			} else {
				/*INSERT 2nd VLAN*/
				i = result->first_vid;
				result->first_vid = result->second_vid;
				result->second_vid = i;

				i = result->first_tpid_enc;
				result->first_tpid_enc = result->second_tpid_enc;
				result->second_tpid_enc = i;

				result->second_vlan_cmd =
						CS_FE_VLAN_CMD_PUSH_C;
			}
		} else if ((cb->input.raw.vlan_tpid_2 != 0) &&
				 (cb->output.raw.vlan_tpid_2 == 0)) {
			/*REMOVE 2nd VLAN*/
			if ((cb->input.raw.vlan_tci_2 & VLAN_VID_MASK) != 0) {
				if (cb->output.raw.vlan_tpid == 0) {
					/*POP OUTER */
					result->second_vlan_cmd =
						CS_FE_VLAN_CMD_POP_B;
				} else {
					/*POP INNER */
					result->second_vlan_cmd =
						CS_FE_VLAN_CMD_POP_D;
				}
			} else {
				if (cb->output.raw.vlan_tpid == 0) {
					/*POP OUTER and POP p-tag*/
					result->first_vlan_cmd =
						CS_FE_VLAN_CMD_POP_D;
					result->second_vlan_cmd =
						CS_FE_VLAN_CMD_POP_B;
				} else {
					/*POP OUTER and change tag*/
					result->first_vlan_cmd =
						CS_FE_VLAN_CMD_POP_B;
					result->second_vid = cb->output.raw.vlan_tci & VLAN_VID_MASK;
					result->second_vlan_cmd =
						CS_FE_VLAN_CMD_SWAP_B;

				}
			}
		} else {
			if ((cb->input.raw.vlan_tci_2 & VLAN_VID_MASK) == 0) {
				/*REPLACE 2nd priority VLAN*/
				result->second_vlan_cmd =
					CS_FE_VLAN_CMD_SWAP_B;
			} else {
				/*REPLACE 2nd VLAN*/
				result->second_vlan_cmd =
					CS_FE_VLAN_CMD_SWAP_D;
			}
		}
	}

	if (cb->output_mask & CS_HM_IP_SA_MASK) {
		action->l3.ip_sa_replace_en = CS_RESULT_ACTION_ENABLE;
		if (cb->output.l3_nh.iph.ver == 6) {
			result->src_ip[0] = ntohl(
					cb->output.l3_nh.ipv6h.sip[3]);
			result->src_ip[1] = ntohl(
					cb->output.l3_nh.ipv6h.sip[2]);
			result->src_ip[2] = ntohl(
					cb->output.l3_nh.ipv6h.sip[1]);
			result->src_ip[3] = ntohl(
					cb->output.l3_nh.ipv6h.sip[0]);
			result->is_v6 = true;
		} else {
			result->src_ip[0] = ntohl(cb->output.l3_nh.iph.sip);
		}
	}
	if (cb->output_mask & CS_HM_IP_DA_MASK) {
		action->l3.ip_da_replace_en = CS_RESULT_ACTION_ENABLE;
		if (cb->output.l3_nh.iph.ver == 6) {
			result->dst_ip[0] = ntohl(
					cb->output.l3_nh.ipv6h.dip[3]);
			result->dst_ip[1] = ntohl(
					cb->output.l3_nh.ipv6h.dip[2]);
			result->dst_ip[2] = ntohl(
					cb->output.l3_nh.ipv6h.dip[1]);
			result->dst_ip[3] = ntohl(
					cb->output.l3_nh.ipv6h.dip[0]);
			result->is_v6 = true;
		} else {
			result->dst_ip[0] = ntohl(cb->output.l3_nh.iph.dip);
		}
	}
	if (cb->common.dec_ttl == CS_DEC_TTL_ENABLE)
		action->l3.decr_ttl_hoplimit = CS_RESULT_ACTION_ENABLE;

	if (cb->output_mask & CS_HM_L4_SP_MASK) {
		action->l4.sp_replace_en = CS_RESULT_ACTION_ENABLE;
		if (cb->output.l3_nh.iph.protocol == IPPROTO_TCP)
			action->l4.sp = ntohs(cb->output.l4_h.th.sport);
		else if (cb->output.l3_nh.iph.protocol == IPPROTO_UDP)
			action->l4.sp = ntohs(cb->output.l4_h.uh.sport);
	}
	if (cb->output_mask & CS_HM_L4_DP_MASK) {
		action->l4.dp_replace_en = CS_RESULT_ACTION_ENABLE;
		if (cb->output.l3_nh.iph.protocol == IPPROTO_TCP)
			action->l4.dp = ntohs(cb->output.l4_h.th.dport);
		else if (cb->output.l3_nh.iph.protocol == IPPROTO_UDP)
			action->l4.dp = ntohs(cb->output.l4_h.uh.dport);
	}

	if (cb->common.module_mask & CS_MOD_MASK_PPPOE) {
		if (cb->action.l2.pppoe_op_en == CS_PPPOE_OP_INSERT) {
			result->pppoe_session_id =
				ntohs(cb->output.raw.pppoe_frame);
			action->l2.pppoe_encap_en = 1;
			action->l2.pppoe_decap_en = 0;
		} else if (cb->action.l2.pppoe_op_en ==
				CS_PPPOE_OP_REMOVE) {
			result->pppoe_session_id =
				ntohs(cb->input.raw.pppoe_frame);
			action->l2.pppoe_encap_en = 0;
			action->l2.pppoe_decap_en = 1;
		}

	}

	if (cb->common.sw_action_id != 0) {
		result->has_sw_action_id = true;
		result->sw_action_id = cb->common.sw_action_id;
	}

	if (cb->output_mask & CS_HM_MCGID_MASK) {
		action->l2.mcgid_valid = CS_RESULT_ACTION_ENABLE;
		action->l2.mcgid = cb->action.misc.mcgid;
	}

	if (unlikely(fwd_hash->fwd_app_type == CORE_FWD_APP_TYPE_SA_CHECK))
		result->voq_id = UU_DEF_CPU_VOQ;
	else
		result->voq_id = cb->action.voq_pol.d_voq_id;

	action->dest.pol_policy = cb->action.voq_pol.pol_policy;
	action->dest.voq_policy = cb->action.voq_pol.voq_policy;

	result->pol_id = cb->action.voq_pol.d_pol_id;

	// TODO: If there is any new Hask action or result that is also used,
	// please put it here!!

	return 0;
} /* set_fwd_hash_result */

int set_qos_hash_result(cs_kernel_accel_cb_t *cb,
		cs_qos_hash_t *qos_hash)
{
	fe_qos_result_entry_t *action = &qos_hash->action;

	if (cb->output_mask & CS_HM_8021P_1_MASK) {
		action->change_8021p_1_en = 1;
		action->top_802_1p = (cb->output.raw.vlan_tci & VLAN_PRIO_MASK)
			>> VLAN_PRIO_SHIFT;
	}

	if (cb->output_mask & CS_HM_DEI_1_MASK) {
		action->change_dei_1_en = 1;
		if (cb->output.raw.vlan_tci & VLAN_CFI_MASK)
	 		action->top_dei = 1;
	}

	if (cb->output_mask & CS_HM_8021P_2_MASK) {
		action->change_8021p_2_en = 1;
		action->inner_802_1p = (cb->output.raw.vlan_tci_2 & VLAN_PRIO_MASK)
			>> VLAN_PRIO_SHIFT;
	}

	if (cb->output_mask & CS_HM_DEI_2_MASK) {
		action->change_dei_2_en = 1;
		if (cb->output.raw.vlan_tci_2 & VLAN_CFI_MASK)
			action->inner_dei = 1;
	}

	if (cb->output_mask & CS_HM_DSCP_MASK) {
		action->change_dscp_en = 1;
		action->dscp = (cb->output.l3_nh.iph.tos >> 2) & 0x3f;
		/* IPv4 and IPv6 should share the same data structure for
		 * IPv4->tos and IPv6->tc */
	}

	return 0;
}

static int set_generic_fwd_hash(cs_fwd_hash_t *fwd_hash,
		cs_kernel_accel_cb_t *cb)
{
	u64 fwd_hm_flag;
	fe_sw_hash_t *key = &fwd_hash->key;
	int status;

	status = cs_core_vtable_get_hashmask_flag_from_apptype(
			fwd_hash->fwd_app_type, &fwd_hm_flag);
	if (status != 0)
		return status;

	status = cs_core_vtable_get_hashmask_index_from_apptype(
			fwd_hash->fwd_app_type,
			&fwd_hash->key.mask_ptr_0_7);
	if (status != 0)
		return status;

	/* Setting up Hash Field */
	status = cs_core_set_hash_key(fwd_hm_flag, cb, key, false);
	if (status != 0)
		return status;

	fwd_hash->lifetime = cb->lifetime;

	return set_fwd_hash_result(cb, fwd_hash);
} /* set_generic_fwd_hash */

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC

int set_ipsec_fwd_hash(cs_fwd_hash_t *fwd_hash, unsigned int *fwd_cnt,
		struct sk_buff *skb)
{
	u64 fwd_hm_flag;
	int status, i;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	/* The hash for RE to GE gets created first , then GE to RE */
	fe_sw_hash_t *key_dir0 = &fwd_hash[0].key;
	fe_sw_hash_t *key_dir1 = &fwd_hash[1].key;
	unsigned char table_id, func_id = 0, crc_hld[5];
	unsigned short sa_id;
	cs_ipsec_sadb_t *p_sadb;
	unsigned int crc32;

	if (!cs_cb)
		return -1;

	/* Create two hashes for the connection */
	sa_id = cs_cb->common.swid[CS_SWID64_MOD_ID_IPSEC] & 0x1FF;
	table_id = (cs_cb->common.swid[CS_SWID64_MOD_ID_IPSEC] >> 9 ) & 0x1;

	p_sadb = ipsec_sadb_get(table_id, sa_id);


	if (p_sadb->state == SADB_INIT)
		ipsec_ipc_timer_func(0); /* Let RE now the database location */

	*fwd_cnt = 0; /* Two hash entries needs to be created */
#ifdef CONFIG_CS752X_HW_ACCEL_ETHERIP
	if ((p_sadb->sa_dir) == CS_IPSEC_INBOUND &&
		(CS_MOD_MASK_ETHERIP & cs_cb->common.module_mask)) { /* Reset etherIP flag */
			cs_cb->common.module_mask &= ~CS_MOD_MASK_ETHERIP;
			p_sadb->etherip = 1;
			dma_map_single(NULL, (void *)p_sadb,
					sizeof(cs_ipsec_sadb_t), DMA_TO_DEVICE);
	}
#endif

	cs_cb->output_mask &= ~CS_HM_IP_SA_MASK;
	cs_cb->output_mask &= ~CS_HM_IP_DA_MASK;


	fwd_hash[1].fwd_app_type = get_app_type_from_module_type(
			cs_cb->common.module_mask);

#ifdef CONFIG_CS75XX_MTU_CHECK
	/* For the outbound streams (LAN -> PE) selects CORE_FWD_APP_TYPE_L3_MTU_IPSEC.
		For the inbound streams (WAN -> PE) selects CORE_FWD_APP_TYPE_L3_IPSEC.
	*/
	if (fwd_hash[1].fwd_app_type == CORE_FWD_APP_TYPE_L3_IPSEC &&
		(p_sadb->sa_dir) == CS_IPSEC_OUTBOUND) {
		if (cs_cb->input.l3_nh.iph.ver == 4)
			fwd_hash[1].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPSEC;
		else
			fwd_hash[1].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPSEC_WITH_CHKSUM;
	}
#endif

	fwd_hash[0].fwd_app_type = get_app_type_from_module_type(
				(CS_MOD_MASK_FROM_RE0 | CS_MOD_MASK_FROM_RE1));

	/* Populate the Mac address */


#ifdef CONFIG_CS752X_HW_ACCEL_ETHERIP
	if (!((p_sadb->sa_dir) == CS_IPSEC_INBOUND &&
		(CS_MOD_MASK_ETHERIP & cs_cb->common.module_mask))) {
#endif

	if (memcmp(cs_cb->input.raw.da,skb->data, ETH_ALEN) != 0) {
		cs_cb->output_mask |= CS_HM_MAC_DA_MASK;
		memcpy(cs_cb->output.raw.da, skb->data,ETH_ALEN);
	}
	if (memcmp(cs_cb->input.raw.sa,skb->data + ETH_ALEN,
				ETH_ALEN) != 0) {
		cs_cb->output_mask |= CS_HM_MAC_SA_MASK;
		memcpy(cs_cb->output.raw.sa,skb->data + ETH_ALEN, ETH_ALEN);
	}

#ifdef CONFIG_CS752X_HW_ACCEL_ETHERIP
	}
#endif
	if (cs_hw_ipsec_offload_mode != IPSEC_OFFLOAD_MODE_BOTH)
		cs_cb->action.voq_pol.voq_policy = 0;
	
	/* First hash to re-direct traffic from GE-RE */
	status = cs_core_vtable_get_hashmask_flag_from_apptype(
			fwd_hash[1].fwd_app_type, &fwd_hm_flag);

	if (status != 0)
		return status;

	status = cs_core_vtable_get_hashmask_index_from_apptype(
			fwd_hash[1].fwd_app_type,
			&fwd_hash[1].key.mask_ptr_0_7);

	if (status != 0)
		return status;

	if (p_sadb->proto == IPPROTO_AH)
		func_id += 1;

	if(CS_IPSEC_INBOUND == p_sadb->sa_dir)
		func_id += 2;
	/* Add ipsec specific fields to it */
	/* mark the sw action id */
	cs_cb->common.sw_action_id = CS_IPSEC_SW_ACTION_ID(func_id,
			cs_cb->action.voq_pol.d_voq_id, sa_id);
	/* Setting up Hash Field */
	status = cs_core_set_hash_key(fwd_hm_flag, cs_cb, key_dir1, false);
	if (status != 0)
		return status;

	if (p_sadb->sa_dir == CS_IPSEC_OUTBOUND)
		cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;

	set_fwd_hash_result(cs_cb, &fwd_hash[1]);
	fwd_hash[1].param.mac[6] = cs_cb->action.voq_pol.d_voq_id;
	/* Change the MAC SA to VoQ id as RE would use this to form recirc_id */
	/* The table_id and sa_id is required for new flow within a tunnel in
	 * the inbound case. As the decrypted packet reaching the CPU will not
	 * have any information about to which SA it belongs. The MAC SA can be
	 * used to determine that as the hash will not hit in this case and MAC
	 * SA content would be intact */
	fwd_hash[1].param.mac[7] = table_id;
	fwd_hash[1].param.mac[8] = sa_id & 0xFF;
	fwd_hash[1].param.mac[9] = (sa_id & 0x100) >> 1;

	/* When a invalid packet would come from RE, it may result into a kernel
	 * crash. This CRC check is there to make sure that any packet processed
	 * by Linux is valid */
	for (i = 0; i < 5; i++)
		crc_hld[i] = fwd_hash[1].param.mac[10 - i];
	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &crc_hld[0], 5));
	/* Store 8 bits of the crc in the forwarded MAC */
	fwd_hash[1].param.mac[11] = crc32 & 0xFF;
	/* RE populates back the original mac address from the p_sadb stored
	 * MAC. in case the tunnel end point MAC addresses change (very
	 * unlikely ) this will not work.  In that case change RE firmware to
	 * copy MAC address from incoming Packet and/or change kernel adapt to
	 * tear down previous hashes and create new ones notifying RE */
	/* IPsec specific results */
	/* Change the MAC SA to VoQ id as RE would use this to form recirc_id */
	/*
	 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH
	 * 	Send outbound IPSec packets to VoQ#32
	 * 	Send inbound IPSec packets to VoQ#24
	 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0
	 * 	Send outbound IPSec packets to VoQ#25
	 * 	Send inbound IPSec packets to VoQ#24
	 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1
	 * 	Send outbound IPSec packets to VoQ#33
	 * 	Send inbound IPSec packets to VoQ#32
	 */
	CS_IPSEC_RE_VOQ_ID_SET(fwd_hash[1].param.voq_id, table_id);

	/* printk("%s:%d: re_id = %d, CS_IPSEC_RE_VOQ_ID_SET = %d\n",
	   	__func__, __LINE__, table_id, fwd_hash[1].param.voq_id); */

	/* Disable TTL dec by default for all RE hashes. RE hash while creating
	 * will determine if this is required */
	cs_cb->common.dec_ttl = !CS_DEC_TTL_ENABLE;

	/* Second hash for RE to GE */

	status = cs_core_vtable_get_hashmask_flag_from_apptype(
			fwd_hash[0].fwd_app_type, &fwd_hm_flag);
	if (status != 0)
		return status;

	status = cs_core_vtable_get_hashmask_index_from_apptype(
			fwd_hash[0].fwd_app_type,
			&fwd_hash[0].key.mask_ptr_0_7);

	if (status != 0)
		return status;

	/* Populate the CB with recirc id so that the set_fwd_key creates the
	 * proper key */
	cs_cb->key_misc.recirc_idx = cs_cb->action.voq_pol.d_voq_id;
	/* Setting up Hash Field using outer cb information */
	status = cs_core_set_hash_key(fwd_hm_flag, cs_cb, key_dir0, true);
	if (status != 0)
		return status;
	/* In the decrypt path the MAC SA would be same as sent so need to
	 * convert it back */
	if (p_sadb->sa_dir == CS_IPSEC_INBOUND) {
		key_dir0->mac_sa[0] = cs_cb->action.voq_pol.d_voq_id;
		key_dir0->mac_sa[1] = table_id;
		key_dir0->mac_sa[2] = sa_id & 0xFF;
		key_dir0->mac_sa[3] = (sa_id & 0x100) >> 1;
		/* 8 bits of the crc in the in the key MAC */
		key_dir0->mac_sa[5] = crc32 & 0xFF;

		key_dir0->lspid = CS_IPSEC_IN_LSPID_FROM_RE;
		
		cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;

		fwd_hash[1].lifetime = 0;
		fwd_hash[0].lifetime = cs_cb->lifetime;


	} else {
		key_dir0->lspid = CS_IPSEC_OUT_LSPID_FROM_RE;

		fwd_hash[1].lifetime = cs_cb->lifetime;
		fwd_hash[0].lifetime = 0;
	}

	set_fwd_hash_result(cs_cb, &fwd_hash[0]);

	/*
	 * for tunnel hash, it should only watch by IPSec
	 */
	if (p_sadb->used == 0){
		*fwd_cnt = 0;
		return -1;
	}

	if (p_sadb->state == SADB_INIT) {
		u16 fwd_hash_idx;
		cs_kernel_accel_cb_t new_cs_cb;
		int tunnel_hash_idx;
		int ret;

		if (CS_IPSEC_INBOUND == p_sadb->sa_dir) {
			/* populate the ethernet MAC DA / SA and type fields */
			memcpy(&p_sadb->eth_addr[0], &cs_cb->input.raw.da[0], 6);
			memcpy(&p_sadb->eth_addr[6], &cs_cb->input.raw.sa[0], 6);
		}

		if (CS_IPSEC_OUTBOUND == p_sadb->sa_dir) {
			/* populate the ethernet MAC DA / SA and type fields */
			memcpy(&p_sadb->eth_addr[0], &cs_cb->output.raw.da[0], 6);
			memcpy(&p_sadb->eth_addr[6], &cs_cb->output.raw.sa[0], 6);
			ipsec_sadb_update_dest_dev(table_id, sa_id, cs_cb->common.output_dev);
		}

		p_sadb->seq_num = p_sadb->x_state->replay.oseq + 1;
		/* Flush the p_sadb so that RE gets correct data */
		dma_map_single(NULL, (void *)p_sadb,
			sizeof(cs_ipsec_sadb_t), DMA_TO_DEVICE);

		/*for remove other kernel adapt module watch*/
		memcpy(&new_cs_cb, cs_cb, sizeof(new_cs_cb));
		for (i = 0; i < CS_SWID64_MOD_MAX; i++)
				new_cs_cb.common.swid[i] = CS_INVALID_SWID64;

		/*for remove ARP watch*/
		for (i = 0; i < ETH_ALEN; i++) {
				new_cs_cb.input.raw.sa[i] = i;
				new_cs_cb.output.raw.da[i] = i;
		}

		new_cs_cb.common.swid_cnt = 0;
		cs_core_logic_add_swid64(&new_cs_cb, CS_IPSEC_GID(table_id, sa_id));

		if (p_sadb->sa_dir == CS_IPSEC_INBOUND)
			tunnel_hash_idx = 1;
		else
			tunnel_hash_idx = 0;

		ret = cs_core_add_fwd_hash(&fwd_hash[tunnel_hash_idx], &fwd_hash_idx);
		if ((ret == 0) || (ret == FE_TABLE_EDUPLICATE)) {
			set_fwd_hash_result_index_to_cs_cb(&new_cs_cb,
				&fwd_hash[tunnel_hash_idx]);

			cs_core_hmu_link_src_and_hash(&new_cs_cb, fwd_hash_idx, NULL);
		} else {
			*fwd_cnt = 0;
			return -1;
		}
		p_sadb->state = SADB_ACCELERATED;
		dma_map_single(NULL, (void *)p_sadb,
			sizeof(cs_ipsec_sadb_t), DMA_TO_DEVICE);

    }else if (p_sadb->state != SADB_ACCELERATED) {
		*fwd_cnt = 0;
		return -1;
    }

	/* Packets from RE to the orig destination Voq
	 * from the hash result itself */

	if (p_sadb->sa_dir == CS_IPSEC_OUTBOUND) {
		memcpy(&fwd_hash[0], &fwd_hash[1], sizeof(fwd_hash[0]));
	}
	*fwd_cnt = 1;

	if (0 == ipsec_sadb_check_skb_queue_empty(table_id, sa_id))
		ipsec_dequeue_skb_and_send_to_re(table_id, sa_id);


#ifdef CONFIG_CS752X_HW_ACCEL_ETHERIP
	/* hash lifetime 0 for etherip as , as we don't support new flow
	 * and so it should not time out.
	 * FIXME with when EtherIP is independent of IPSec */
	cs_cb->lifetime = 0;
#endif

	return 0;
} /* set_ipsec_fwd_hash */
#endif /* CONFIG_CS752X_HW_ACCELERATION_IPSEC */

int set_generic_qos_hash(cs_qos_hash_t *qos_hash, cs_kernel_accel_cb_t *cb)
{
	u64 qos_hm_flag;
	fe_sw_hash_t *key = &qos_hash->key;
	int status;

	status = cs_core_vtable_get_hashmask_flag_from_apptype(
			qos_hash->qos_app_type, &qos_hm_flag);
	if (status != 0)
		return status;

	status = cs_core_vtable_get_hashmask_index_from_apptype(
			qos_hash->qos_app_type,
			&qos_hash->key.mask_ptr_0_7);
	if (status != 0)
		return status;

	/* Setting up Hash Field */
	status = cs_core_set_hash_key(qos_hm_flag, cb, key, false);
	if (status != 0)
		return status;

	return set_qos_hash_result(cb, qos_hash);
} /* set_generic_qos_hash */

int set_l3_multicast_fwd_hash(cs_fwd_hash_t *fwd_hash, unsigned int *fwd_cnt,
		struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_kernel_accel_cb_t main_cb;
	u32 crc32;
	u16 crc16, rslt_idx;
	int ret;
	u16 curr_mcidx;
	unsigned int sw_id;
	fe_fwd_result_entry_t main_fwd_reslt;

	if (cs_cb == NULL)
		return 0;

	*fwd_cnt = 0;
	curr_mcidx = 1 << (cs_cb->common.egress_port_id);

	/*get app type*/
	fwd_hash[0].fwd_app_type = get_app_type_from_module_type(
					cs_cb->common.module_mask);

	fwd_hash[1].fwd_app_type = fwd_hash[0].fwd_app_type;
	memcpy(&main_cb, cs_cb , sizeof(main_cb));
	memset(&main_cb.action, 0, sizeof(main_cb.action));
	DBG(printk("%s input_port=%d, curr_mcidx = %d\n", __func__,
				main_cb.key_misc.orig_lspid, curr_mcidx));
	main_cb.key_misc.lspid = main_cb.key_misc.orig_lspid;
	main_cb.action.voq_pol.d_voq_id = ROOT_PORT_VOQ_BASE;
	main_cb.action.voq_pol.voq_policy = 1;
	main_cb.output_mask = CS_HM_MCGID_MASK;
	main_cb.action.misc.mcgid = curr_mcidx;

	main_cb.common.dec_ttl = ~CS_DEC_TTL_ENABLE;
	main_cb.common.sw_action_id = 0;

	set_generic_fwd_hash(&fwd_hash[1], &main_cb);

	ret = cs_fe_hash_calc_crc(&fwd_hash[1].key, &crc32, &crc16,
			CRC16_CCITT);
	if (ret != 0){
		printk("%s calculate crc err fail err=%d\n", __func__, ret);
		return 0;
	}

	ret = cs_fe_hash_get_hash_by_crc(crc32, crc16,
			fwd_hash[1].key.mask_ptr_0_7, &rslt_idx, &sw_id);


	if (ret == FE_TABLE_ENTRYNOTFOUND){
		/*there is no main fwdrslt to Root port so create it*/
		DBG(printk("%s hash entry not found by crc crc16=0x%x\n", __func__,
					crc16));
		*fwd_cnt = 1;
	} else {
		/*there is has main fwdrslt to Root port so modify mcgid*/
		DBG(printk("%s hash entry found by crc - crc16=0x%x," \
					"rslt_idx = 0x%x , sw_id = 0x%x\n", __func__,
					crc16, rslt_idx, sw_id));
		ret = cs_fe_table_get_entry(FE_TABLE_FWDRSLT,
					rslt_idx, &main_fwd_reslt);

		if (ret != FE_TABLE_OK){
			printk("%s get main_fwd_reslt fail err=%d\n",__func__, ret);
			return 0;
		}

//		if (main_fwd_reslt.l2.mcgid != curr_mcidx) {
		if ((main_fwd_reslt.l2.mcgid & curr_mcidx) == 0) {
			DBG(printk("%s update main_fwd_reslt (%d) mcgid(%d) to new mcidx = %d\n",
				__func__, fwd_hash[1].param.result_index, main_fwd_reslt.l2.mcgid,
				main_fwd_reslt.l2.mcgid | curr_mcidx));

			main_fwd_reslt.l2.mcgid |= curr_mcidx;
			ret = cs_fe_table_set_entry(FE_TABLE_FWDRSLT,
					rslt_idx, &main_fwd_reslt);

			if (ret != FE_TABLE_OK) {
				/* if update fwdrslt fail, should delete the
				 * whole hash tree, but will this happen?*/
				printk("%s update main_fwd_reslt mcgid fail err=%d\n",__func__, ret);
				return 0;
			}
		}
	}

	cs_cb->key_misc.mcidx = cs_cb->common.egress_port_id;
	cs_cb->key_misc.lspid = MCAST_PORT;

	set_generic_fwd_hash(&fwd_hash[0], cs_cb);
	(*fwd_cnt)++;
	DBG(printk("%s set fwd hash count %d\n", __func__,
						*fwd_cnt));

	return 0;
} /*set_l3_multicast_fwd_hash*/

int set_l3_multicast_sw_hash(cs_fwd_hash_t *fwd_hash, unsigned int *fwd_cnt,
                struct sk_buff *skb)
{
        cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
 
        if (cs_cb == NULL)
                return 0;
 
        /*get app type*/
        fwd_hash[0].fwd_app_type = get_app_type_from_module_type(
                                        cs_cb->common.module_mask);
 
        memset(&(cs_cb->action), 0, sizeof(cs_cb->action));
        cs_cb->key_misc.lspid = cs_cb->key_misc.orig_lspid;
        cs_cb->action.voq_pol.d_voq_id = CPU_PORT7_VOQ_BASE + 1;
 
        cs_cb->common.dec_ttl = ~CS_DEC_TTL_ENABLE;
        cs_cb->common.sw_action_id = 0;
 
        set_generic_fwd_hash(&fwd_hash[0], cs_cb);
 
        *fwd_cnt = 1;
        DBG(printk("%s set fwd hash count %d\n", __func__,
                                                *fwd_cnt));
        return 0;
} /*set_l3_multicast_sw_hash*/
 

static int setup_hash_entry(struct sk_buff *skb, cs_fwd_hash_t *fwd_hash,
		unsigned int *fwd_cnt, cs_qos_hash_t *qos_hash,
		unsigned int *qos_cnt)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	int fwd_app_type;
	
	if (!cs_cb)
		return -1;

	*fwd_cnt = *qos_cnt = 0;

	if (cs_cb->common.module_mask & (CS_MOD_MASK_IPV4_MULTICAST |
				CS_MOD_MASK_IPV6_MULTICAST |
				CS_MOD_MASK_L2_MULTICAST)) {
		// FIXME!!! Please implement this!!! All we know is the
		// fwd_type for the first fwd hash... the rest is TBD.
		// such as how do we construct multiple hash entries
		// from info in control block. the following is dummy
		// code for the first entry..

		if (cs_cb->common.module_mask & CS_MOD_MASK_L2_MULTICAST) {
			fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L2_MCAST;
		} else {
		    if (cs_cb->common.module_mask & CS_MOD_MASK_WFO_SW_MULTICAST) {
		        // MCAL full because reach the station limitation
		        // setup a special hash to kernel
		        // It means all the multicast packets goes through kernel
    			fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MCAST;
    			set_l3_multicast_sw_hash(fwd_hash, fwd_cnt, skb);
		    }else {
			fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MCAST;
			set_l3_multicast_fwd_hash(fwd_hash, fwd_cnt, skb);
			if (cs_cb->common.module_mask &
					CS_MOD_MASK_QOS_FIELD_CHANGE) {
				qos_hash->qos_app_type =
					CORE_QOS_APP_TYPE_L3_QOS_MULTICAST;
				*qos_cnt = 1;
			}
		}
		}
#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
	} else if (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) {
		set_ipsec_fwd_hash(fwd_hash, fwd_cnt, skb);
#endif
	} else {
		*fwd_cnt = 1;
		fwd_hash[0].fwd_app_type = get_app_type_from_module_type(
				cs_cb->common.module_mask);
		fwd_app_type = fwd_hash[0].fwd_app_type;

#ifdef CONFIG_CS75XX_MTU_CHECK
		if ((fwd_app_type == CORE_FWD_APP_TYPE_L3_GENERIC || fwd_app_type == CORE_FWD_APP_TYPE_L3_GENERIC_WITH_CHKSUM)
			&& (cs_cb->common.ingress_port_id == GE_PORT0 ||
				cs_cb->common.ingress_port_id == GE_PORT1 || cs_cb->common.ingress_port_id == GE_PORT2)) {
			/* check mtu for streams from downlink ports */
			if (cs_cb->common.ingress_port_id != cs_wan_port_id) {
				if (cs_cb->common.module_mask & CS_MOD_MASK_PPPOE) {
					/* check mtu for pppoe stream */
					if (cs_cb->input.l3_nh.iph.ver == 4)
						fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_PPPOE;
					else
						fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_PPPOE_WITH_CHKSUM;
					DBG(printk("fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_PPPOE\n"));
				} else if (cs_cb->common.module_mask & CS_MOD_MASK_TUNNEL) {
					/* check mtu for iplip stream */
					if (cs_cb->input.l3_nh.iph.ver == 4)
						fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPLIP;
					else
						fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPLIP_WITH_CHKSUM;
					DBG(printk("fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPLIP\n"));
				} else {
					/* check mtu for ipoe stream */
					if (cs_cb->input.l3_nh.iph.ver == 4)
						fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPOE;
					else
						fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPOE_WITH_CHKSUM;
					DBG(printk("fwd_hash[0].fwd_app_type = CORE_FWD_APP_TYPE_L3_MTU_IPOE\n"));
				}
			}
		}
#endif
#if 0				
		if ((fwd_hash[0].fwd_app_type ==
					CORE_FWD_APP_TYPE_L3_GENERIC) ||
				(fwd_hash[0].fwd_app_type ==
				 CORE_FWD_APP_TYPE_L2_FLOW)) {
			//FIXME: mac address need from kernel
			memcpy(cs_cb->output.raw.da, skb->data,
						ETH_ALEN);
			memcpy(cs_cb->output.raw.sa,
						skb->data + ETH_ALEN, ETH_ALEN);

			if (memcmp(cs_cb->input.raw.da,skb->data, ETH_ALEN)
					!= 0) {
				cs_cb->output_mask |= CS_HM_MAC_DA_MASK;
			}
			if (memcmp(cs_cb->input.raw.sa,skb->data + ETH_ALEN,
						ETH_ALEN) != 0) {
				cs_cb->output_mask |= CS_HM_MAC_SA_MASK;
			}
		}
#endif
		if (fwd_hash[0].fwd_app_type != CORE_FWD_APP_TYPE_NONE)
			set_generic_fwd_hash(&fwd_hash[0], cs_cb);

		if (cs_cb->common.module_mask & CS_MOD_MASK_QOS_FIELD_CHANGE) {
			if (fwd_hash[0].fwd_app_type ==
					CORE_FWD_APP_TYPE_L2_FLOW) {
				qos_hash[*qos_cnt].qos_app_type =
					CORE_QOS_APP_TYPE_L2_QOS_1;
				*qos_cnt = 1;
			} else if (fwd_app_type ==
					CORE_FWD_APP_TYPE_L3_GENERIC) {
				qos_hash[*qos_cnt].qos_app_type =
					CORE_QOS_APP_TYPE_L3_QOS_GENERIC;
				*qos_cnt = 1;
			} else if (fwd_app_type ==
					CORE_FWD_APP_TYPE_L3_GENERIC_WITH_CHKSUM) {
				qos_hash[*qos_cnt].qos_app_type =
					CORE_QOS_APP_TYPE_L3_QOS_GENERIC_WITH_CHKSUM;
				*qos_cnt = 1;
			}
		}
	}

	if ((*qos_cnt) != 0)
		set_generic_qos_hash(qos_hash, cs_cb);

#ifdef SA_CHECK_ENABLE
	if (*fwd_cnt != 0) {
		fwd_hash[*fwd_cnt].fwd_app_type = CORE_FWD_APP_TYPE_SA_CHECK;
		set_generic_fwd_hash(&fwd_hash[*fwd_cnt], cs_cb);
		(*fwd_cnt)++;
	}
#endif


#if 0
	if (cs_cb->common.module_mask & CS_MOD_MASK_QOS_FIELD_CHANGE) {
		if (cs_cb->common.module_mask & (CS_MOD_MASK_IPV6_ROUTING |
					CS_MOD_MASK_IPV6_MULTICAST |
					CS_MOD_MASK_IPV6_NETFILTER)) {
			*qos_cnt = 1;
			qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L3_QOS_GENERIC;
		} else if (cs_cb->common.module_mask &
				(CS_MOD_MASK_IPV4_ROUTING |
				 CS_MOD_MASK_IPV4_PROTOCOL |
	 			 CS_MOD_MASK_IPV4_NETFILTER)) {
			*qos_cnt = 1;
			qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L4_QOS_NAT;
		} else if (cs_cb->common.module_mask & (CS_MOD_MASK_BRIDGE |
					CS_MOD_MASK_BRIDGE_NETFILTER)) {
			*qos_cnt = 1;
			qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L2_QOS_1;
		} else {
			qos_hash->qos_app_type = CORE_QOS_APP_TYPE_NONE;
		}
	}
	if (*qos_cnt != 0)
		set_generic_qos_hash(qos_hash, cs_cb);

#endif

	if ((*fwd_cnt == 0) && (*qos_cnt == 0))
		return -1;
	else
		return 0;
} /* setup_hash_entry */

#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
/*
 * Enable double check mechanism
 * The HASH_CHECK_MEM index should be the same as FWDRSLT index.
 * It also stores L2 MAC addresses of input packet into FE L2 table.
 * It also stores L3 IP addresses of input packet into FE L3 table.
 */
int cs_core_double_chk_enbl(cs_fwd_hash_t *fwd_hash_entry, u16 fwdrslt_index)
{
	fe_sw_hash_t *key;
	fe_hash_check_entry_t hashcheck_entry;
	u8 mac[12];
	u32 l2_index = 0, l3_index;
	int ret = 0, i;
	u32 idx;
	u64 fwd_hm_flag;

	DBG(printk("%s:%d: fwdrslt_index = %d\n", __func__, __LINE__, 
			fwdrslt_index));

	ret = cs_core_vtable_get_hashmask_flag_from_apptype(
			fwd_hash_entry->fwd_app_type, &fwd_hm_flag);
	if (ret != 0)
		goto L2_EXIT;

	key = &(fwd_hash_entry->key);
	memset(&hashcheck_entry, 0, sizeof(hashcheck_entry));
	idx = fwdrslt_index;
	
	/* L2 MAC */
	if ((fwd_hm_flag & CS_HM_MAC_DA_MASK) &&
		(fwd_hm_flag & CS_HM_MAC_SA_MASK)) {
		for (i = 0; i < 6; i++)
			mac[i] = key->mac_da[i];
		for (i = 0; i < 6; i++)
			mac[6 + i] = key->mac_sa[i];

		ret = cs_fe_l2_result_alloc(mac, L2_LOOKUP_TYPE_PAIR,
				&l2_index);
	
		if (ret != 0)
			goto L2_EXIT;

		hashcheck_entry.check_mac_sa_en = 1;
		hashcheck_entry.check_mac_da_en = 1;
		hashcheck_entry.check_l2_check_idx = (u16) l2_index;
		DBG(printk("%s:%d: check_l2_check_idx = %d\n", __func__,
				__LINE__, l2_index));
	} else {
		DBG(printk("%s:%d: FIXME!\n"
				"Can't find valid SA MAC or DA MAC in key\n",
				__func__, __LINE__));
		ret = -1;
		goto L2_EXIT;
	}

	/* L3 SIP */
	if(fwd_hm_flag & CS_HM_IP_SA_MASK) {
		ret = cs_fe_l3_result_alloc(key->sa, key->ip_version,
				&l3_index);
		if (ret != 0)
			goto L3_SA_EXIT;
		hashcheck_entry.check_ip_sa_check_idx = (u16) l3_index;
		hashcheck_entry.check_ip_sa_en = 1;
		DBG(printk("%s:%d: check_ip_sa_check_idx = %d\n", __func__,
				__LINE__, l3_index));
	}
	
	/* L3 DIP */
	if(fwd_hm_flag & CS_HM_IP_DA_MASK) {
		ret = cs_fe_l3_result_alloc(key->da, key->ip_version,
				&l3_index);
		if (ret != 0)
			goto L3_DA_EXIT;
		hashcheck_entry.check_ip_da_check_idx = (u16) l3_index;
		hashcheck_entry.check_ip_da_en = 1;
		DBG(printk("%s:%d: check_ip_da_check_idx = %d\n", __func__,
				__LINE__, l3_index));
	}
	
	/* L4 SP */
	if(fwd_hm_flag & CS_HM_L4_SP_MASK) {
		hashcheck_entry.check_l4_sp_to_be_chk = key->l4_sp;
		hashcheck_entry.check_l4_sp_en = 1;
		DBG(printk("%s:%d: check_l4_sp_to_be_chk = %d\n", __func__,
				__LINE__, key->l4_sp));
	}
	
	/* L4 DP */
	if(fwd_hm_flag & CS_HM_L4_SP_MASK) {
		hashcheck_entry.check_l4_dp_to_be_chk = key->l4_dp;
		hashcheck_entry.check_l4_dp_en = 1;
		DBG(printk("%s:%d: check_l4_dp_to_be_chk = %d\n", __func__,
				__LINE__, key->l4_dp));
	}

	/* set FETOP_HASH_CHECK_MEM_ACCESS & FETOP_HASH_CHECK_MEM_DATA */

	ret = cs_fe_table_set_entry(FE_TABLE_HASH_CHECK_MEM, idx, 
			&hashcheck_entry);
	if (ret != 0)
		goto CHECK_MEM_EXIT;
	return 0;
	
CHECK_MEM_EXIT:
	cs_fe_l3_result_dealloc(hashcheck_entry.check_ip_da_check_idx);

L3_DA_EXIT:
	cs_fe_l3_result_dealloc(hashcheck_entry.check_ip_sa_check_idx);

L3_SA_EXIT:
	cs_fe_l2_result_dealloc(hashcheck_entry.check_l2_check_idx,
			L2_LOOKUP_TYPE_PAIR);
L2_EXIT:
	DBG(printk("%s:%d: ret = %d\n", __func__, __LINE__, ret));
	return ret;
}

/*
 * Disable double check mechanism
 * It also removes L2 MAC addresses of input packet from FE L2 table.
 * It also removes L3 IP addresses of input packet from FE L3 table.
 */
int cs_core_double_chk_dsbl(u16 fwdrslt_index)
{
	fe_hash_check_entry_t hashcheck_entry;
	int ret;

	DBG(printk("%s:%d: fwdrslt_index = %d\n", __func__, __LINE__,
			fwdrslt_index));
	ret = cs_fe_table_get_entry(FE_TABLE_HASH_CHECK_MEM, fwdrslt_index, 
			&hashcheck_entry);
	if (ret != 0)
		return ret;

	if (hashcheck_entry.check_mac_da_en == 1 && 
		hashcheck_entry.check_mac_sa_en == 1)
		cs_fe_l2_result_dealloc(hashcheck_entry.check_l2_check_idx,
				L2_LOOKUP_TYPE_PAIR);

	if (hashcheck_entry.check_ip_sa_en == 1)
		cs_fe_l3_result_dealloc(hashcheck_entry.check_ip_sa_check_idx);

	if (hashcheck_entry.check_ip_da_en == 1)
		cs_fe_l3_result_dealloc(hashcheck_entry.check_ip_da_check_idx);

	ret = cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_CHECK_MEM, 
			fwdrslt_index, false);

	DBG(printk("%s:%d: ret = %d\n", __func__, __LINE__, ret));

	return ret;
}

/*
 * Do double check by software
 * It compares the input packet with corresponding HASH_CHECK_MEM entry.
 */
int cs_core_double_chk_cmp(cs_fwd_hash_t *fwd_hash_entry, u16 fwdrslt_index, bool *match)
{
	fe_sw_hash_t *key;
	fe_hash_check_entry_t hashcheck_entry;
	u8 mac_da[6], mac_sa[6];
	int ret, i;
	u32 ip_sa[4], ip_da[4];
	u64 fwd_hm_flag;

	key = &(fwd_hash_entry->key);
	*match = false;
	
	DBG(printk("%s:%d: fwdrslt_index = %d\n", __func__, __LINE__, 
			fwdrslt_index));
	
	ret = cs_fe_table_get_entry(FE_TABLE_HASH_CHECK_MEM, fwdrslt_index, 
			&hashcheck_entry);
	if (ret != 0) {
		DBG(printk("%s:%d: can't get fwdrslt entry, ret = %d\n",
				__func__, __LINE__, ret));
		return ret;
	}

	DBG(cs_fe_table_print_entry(FE_TABLE_HASH_CHECK_MEM, fwdrslt_index));

	ret = cs_core_vtable_get_hashmask_flag_from_apptype(
			fwd_hash_entry->fwd_app_type, &fwd_hm_flag);
	if (ret != 0) {
		DBG(printk("%s:%d: unknown APP type %d, ret = %d\n",
				__func__, __LINE__,
				fwd_hash_entry->fwd_app_type, ret));
		return ret;
	}

	/* L2 Flags */
	if ((hashcheck_entry.check_mac_sa_en != 1) ||
		(hashcheck_entry.check_mac_da_en != 1) ||
		((fwd_hm_flag & CS_HM_MAC_DA_MASK) == 0) ||
		((fwd_hm_flag & CS_HM_MAC_SA_MASK) == 0)) {
		DBG(printk("%s:%d: Flags of MAC SA or DA don't match.\n",
				__func__, __LINE__));
		return 0;
	}

	/* L3 Flags */
	if (hashcheck_entry.check_ip_sa_en != 
		((fwd_hm_flag & CS_HM_IP_SA_MASK) > 0)) {
		DBG(printk("%s:%d: Flag of IP SA doesn't match.\n",
				__func__, __LINE__));
		return 0;
	}

	if (hashcheck_entry.check_ip_da_en != 
		((fwd_hm_flag & CS_HM_IP_DA_MASK) > 0)) {
		DBG(printk("%s:%d: Flag of IP DA doesn't match.\n",
				__func__, __LINE__));
		return 0;
	}

	/* L4 Flags */
	if (hashcheck_entry.check_l4_sp_en != 
		((fwd_hm_flag & CS_HM_L4_SP_MASK) > 0)) {
		DBG(printk("%s:%d: Flag of L4 SP doesn't match.\n",
				__func__, __LINE__));
		return 0;
	}

	if (hashcheck_entry.check_l4_dp_en != 
		((fwd_hm_flag & CS_HM_L4_DP_MASK) > 0)) {
		DBG(printk("%s:%d: Flag of L4 DP doesn't match.\n",
				__func__, __LINE__));
		return 0;
	}
		
	/* L4 SP */
	if (fwd_hm_flag & CS_HM_L4_SP_MASK) {
		if (hashcheck_entry.check_l4_sp_to_be_chk != key->l4_sp) {
			DBG(printk("%s:%d: L4 SP doesn't match\n",
					__func__, __LINE__));
			return 0;
		}
	}
	
	/* L4 DP */
	if (fwd_hm_flag & CS_HM_L4_DP_MASK) {
		if (hashcheck_entry.check_l4_dp_to_be_chk != key->l4_dp) {
			DBG(printk("%s:%d: L4 DP doesn't match\n",
					__func__, __LINE__));
			return 0;
		}
	}
	
	/* L3 SIP */
	if (fwd_hm_flag & CS_HM_IP_SA_MASK) {
		ret = cs_fe_table_get_l3_ip(
			hashcheck_entry.check_ip_sa_check_idx, ip_sa,
			key->ip_version); 
		if (ret != 0) {
			DBG(printk("%s:%d: can't get IP SA entry, ret = %d\n",
					__func__, __LINE__, ret));
			return ret;
		}
		
		if (key->ip_version == 1 /* IPv6 */) {
			for (i = 0; i < 4; i++) {
				if (ip_sa[i] != key->sa[i]) {
					DBG(printk(
						"%s:%d: IP SA doesn't match\n",
							__func__, __LINE__));
					return 0;
				}
			}
		} else {
			if (ip_sa[0] != key->sa[0]) {
				DBG(printk("%s:%d: IP SA doesn't match\n",
						__func__, __LINE__));
				return 0;
			}
		}
	}
	
	/* L3 DIP */
	if (fwd_hm_flag & CS_HM_IP_DA_MASK) {
		ret = cs_fe_table_get_l3_ip(
			hashcheck_entry.check_ip_da_check_idx, ip_da,
			key->ip_version); 
		if (ret != 0) {
			DBG(printk("%s:%d: can't get IP DA entry, ret = %d\n",
					__func__, __LINE__, ret));
			return ret;
		}
		
		if (key->ip_version == 1 /* IPv6 */) {
			for (i = 0; i < 4; i++) {
				if (ip_da[i] != key->da[i]) {
					DBG(printk(
						"%s:%d: IP SA doesn't match\n",
							__func__, __LINE__));
					return 0;
				}
			}
		} else {
			if (ip_da[0] != key->da[0]) {
				DBG(printk("%s:%d: IP SA doesn't match\n",
						__func__, __LINE__));
				return 0;
			}
		}
	}
	
	/* L2 MAC */
	ret = cs_fe_table_get_l2_mac(hashcheck_entry.check_l2_check_idx,
			mac_sa, mac_da);
	if (ret != 0) {
		DBG(printk("%s:%d: can't get L2 MAC entry, ret = %d\n",
				__func__, __LINE__, ret));
		return ret;
	}

	ret = memcmp(mac_sa, key->mac_sa, 6);
	if (ret != 0) {
		DBG(printk("%s:%d: MAC SA doesn't match, ret = %d\n",
				__func__, __LINE__, ret));
		return 0;
	}

	ret = memcmp(mac_da, key->mac_da, 6);
	if (ret != 0) {
		DBG(printk("%s:%d: MAC DA doesn't match, ret = %d\n",
				__func__, __LINE__, ret));
		return 0;
	}

	DBG(printk("%s:%d: done.\n", __func__, __LINE__));
	*match = true;
	return 0;
}
#endif

/* manually add a forwarding hash */
int cs_core_add_fwd_hash(cs_fwd_hash_t *fwd_hash_entry, u16 *fwd_hash_idx)
{
	int ret = 0;
	fe_voq_pol_entry_t voqpol_entry;
	fe_flow_vlan_entry_t fvlan_entry;
	unsigned int result_index, fwdrslt_index;
	unsigned int l2_index = 0, l3_index;
	u32 crc32;
	u16 hash_index, crc16;
	bool f_free_fwdrslt = false;

	if (fwd_hash_entry == NULL)
		return -1;
	/* get the result indices */
	/* get L2 table index */
	if ((fwd_hash_entry->action.l2.mac_sa_replace_en == 1) &&
			(fwd_hash_entry->action.l2.mac_da_replace_en == 1))
		ret = cs_fe_l2_result_alloc(fwd_hash_entry->param.mac,
				L2_LOOKUP_TYPE_PAIR, &l2_index);
	else if (fwd_hash_entry->action.l2.mac_sa_replace_en == 1)
		ret = cs_fe_l2_result_alloc(fwd_hash_entry->param.mac + 6,
				L2_LOOKUP_TYPE_SA, &l2_index);
	else if (fwd_hash_entry->action.l2.mac_da_replace_en == 1)
		ret = cs_fe_l2_result_alloc(fwd_hash_entry->param.mac,
				L2_LOOKUP_TYPE_DA, &l2_index);
	if (ret != 0)
		return ret;
	fwd_hash_entry->action.l2.l2_index = (__u16)l2_index;

	/* get the VLAN CMD info */
	if ((fwd_hash_entry->param.first_vlan_cmd != 0) ||
			(fwd_hash_entry->param.second_vlan_cmd != 0)) {
		memset(&fvlan_entry, 0, sizeof(fvlan_entry));
		fvlan_entry.first_vid = fwd_hash_entry->param.first_vid;
		fvlan_entry.first_vlan_cmd =
			fwd_hash_entry->param.first_vlan_cmd;
		fvlan_entry.first_tpid_enc =
			fwd_hash_entry->param.first_tpid_enc;
		fvlan_entry.second_vid = fwd_hash_entry->param.second_vid;
		fvlan_entry.second_vlan_cmd =
			fwd_hash_entry->param.second_vlan_cmd;
		fvlan_entry.second_tpid_enc =
			fwd_hash_entry->param.second_tpid_enc;

		ret = cs_fe_table_add_entry(FE_TABLE_FVLAN,
				&fvlan_entry, &result_index);
		fwd_hash_entry->action.l2.flow_vlan_index = (u16)result_index;

		if (ret != 0)
			goto QUIT_FREE_L2;
		fwd_hash_entry->action.l2.flow_vlan_op_en = 1;
	}

	/* get L3 table index */
	if (fwd_hash_entry->action.l3.ip_sa_replace_en == 1) {
		ret = cs_fe_l3_result_alloc(fwd_hash_entry->param.src_ip,
				fwd_hash_entry->param.is_v6, &l3_index);
		if (ret != 0)
			goto QUIT_FREE_FVLAN;
		fwd_hash_entry->action.l3.ip_sa_index = (__u16)l3_index;
	}

	if (fwd_hash_entry->action.l3.ip_da_replace_en == 1) {
		ret = cs_fe_l3_result_alloc(fwd_hash_entry->param.dst_ip,
				fwd_hash_entry->param.is_v6, &l3_index);
		if (ret != 0)
			goto QUIT_FREE_L3_SRC;
		fwd_hash_entry->action.l3.ip_da_index = (__u16)l3_index;
	}

	if ((fwd_hash_entry->param.has_sw_action_id == true) &&
			(fwd_hash_entry->action.l3.ip_da_replace_en == 0) &&
			(fwd_hash_entry->action.l3.ip_sa_replace_en == 0)) {
		fwd_hash_entry->action.l3.ip_sa_index = (u16)
			(fwd_hash_entry->param.sw_action_id & 0x0fff);
		fwd_hash_entry->action.l3.ip_da_index = (u16)
			((fwd_hash_entry->param.sw_action_id >> 12) & 0x00ff);
	}

	/* get voqpol table index */
	memset(&voqpol_entry, 0, sizeof(voqpol_entry));
	voqpol_entry.voq_base = fwd_hash_entry->param.voq_id;
	voqpol_entry.pol_base = fwd_hash_entry->param.pol_id;
	if (fwd_hash_entry->action.l2.pppoe_encap_en |
			fwd_hash_entry->action.l2.pppoe_decap_en) {
		voqpol_entry.pppoe_session_id =
			fwd_hash_entry->param.pppoe_session_id;
	}

	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voqpol_entry,
			&result_index);
	fwd_hash_entry->action.dest.voq_pol_table_index = (u16)result_index;

	if (ret != 0)
		goto QUIT_FREE_L3;

	/* get FWDRSLT index */
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwd_hash_entry->action,
			&fwdrslt_index);
	if (ret != 0)
		goto QUIT_FREE_VOQPOL;
	fwd_hash_entry->param.result_index = (u16)fwdrslt_index;

	/* calculate the crc values */
	ret = cs_fe_hash_calc_crc(&fwd_hash_entry->key, &crc32, &crc16,
			CRC16_CCITT);
	if (ret != 0)
		goto QUIT_FREE_FWDRSLT;

	/* add the hash to HashHash table */
	ret = cs_fe_hash_add_hash(crc32, crc16,
			fwd_hash_entry->key.mask_ptr_0_7, (u16)fwdrslt_index,
			&hash_index);
	if (fwd_hash_idx != NULL)
		*fwd_hash_idx = hash_index;

	/* if (ret == FE_TABLE_EDUPLICATE) {
		ret = 0;
		goto QUIT_FREE_FWDRSLT;
	} else */ if (ret != 0) {
		goto QUIT_FREE_FWDRSLT;
	}

#if defined(CONFIG_CS752X_PROC) && defined(CONFIG_CS75XX_DOUBLE_CHECK)
	/* do double check if necessary */
	if (cs_fe_double_chk > 0) {
		ret = cs_core_double_chk_enbl(fwd_hash_entry,
				(u16) fwdrslt_index);
		if (ret != 0)
			goto QUIT_FREE_FWDHASH;
	}
#endif

	ret = cs_core_hmu_add_hash(hash_index, fwd_hash_entry->lifetime, NULL);
	if (ret == 0)
		cs_core_hmu_set_result_idx(hash_index, fwdrslt_index, 0);
	else
		goto QUIT_FREE_FWDHASH;

	return ret;

QUIT_FREE_FWDHASH:
	cs_fe_hash_del_hash(hash_index);
	if (fwd_hash_idx != NULL)
		*fwd_hash_idx = 0;
	
QUIT_FREE_FWDRSLT:
	f_free_fwdrslt = true;

QUIT_FREE_VOQPOL:
	cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
			fwd_hash_entry->action.dest.voq_pol_table_index, false);

QUIT_FREE_L3:
	if (fwd_hash_entry->action.l3.ip_da_replace_en == 1)
		cs_fe_l3_result_dealloc(fwd_hash_entry->action.l3.ip_da_index);

QUIT_FREE_L3_SRC:
	if (fwd_hash_entry->action.l3.ip_sa_replace_en == 1)
		cs_fe_l3_result_dealloc(fwd_hash_entry->action.l3.ip_sa_index);

QUIT_FREE_FVLAN:
	if (fwd_hash_entry->action.l2.flow_vlan_op_en == 1)
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
				fwd_hash_entry->action.l2.flow_vlan_index,
				false);

QUIT_FREE_L2:
	if ((fwd_hash_entry->action.l2.mac_sa_replace_en == 1) &&
			(fwd_hash_entry->action.l2.mac_da_replace_en == 1))
		cs_fe_l2_result_dealloc(fwd_hash_entry->action.l2.l2_index,
				L2_LOOKUP_TYPE_PAIR);
	else if (fwd_hash_entry->action.l2.mac_sa_replace_en == 1)
		cs_fe_l2_result_dealloc(fwd_hash_entry->action.l2.l2_index,
				L2_LOOKUP_TYPE_SA);
	else if (fwd_hash_entry->action.l2.mac_da_replace_en == 1)
		cs_fe_l2_result_dealloc(fwd_hash_entry->action.l2.l2_index,
				L2_LOOKUP_TYPE_DA);

	if (f_free_fwdrslt == true)
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, fwdrslt_index,
				false);

	return ret;
} /* cs_core_add_fwd_hash */

/* manually add a QoS hash */
int cs_core_add_qos_hash(cs_qos_hash_t *qos_hash_entry, u16 *qos_hash_idx)
{
	int ret;
	unsigned int qosrslt_index;
	u32 crc32;
	u16 hash_index, crc16;

	if (qos_hash_entry == NULL)
		return -1;

	ret = cs_fe_table_add_entry(FE_TABLE_QOSRSLT,
			&qos_hash_entry->action, &qosrslt_index);
	if (ret != 0)
		return -1;

	ret = cs_fe_hash_calc_crc(&qos_hash_entry->key, &crc32, &crc16,
			CRC16_CCITT);
	if (ret != 0)
		goto QUIT_FREE_QOSRSLT;

	ret = cs_fe_hash_add_hash(crc32, crc16,
			qos_hash_entry->key.mask_ptr_0_7, qosrslt_index,
			&hash_index);
	if (qos_hash_idx != NULL)
		*qos_hash_idx = hash_index;
	qos_hash_entry->rslt_idx = qosrslt_index;
	if (ret == FE_TABLE_EDUPLICATE) {
		ret = 0;
		goto QUIT_FREE_QOSRSLT;
		/* it's duplicate. so we need to delete the created
		 * qosrslt for decrementing the user count */
		/* we are not going to increment the user count of this
		 * QoS hash entry, even though one QoS hash entry might
		 * be shared by various number of FWD hash entry. We are
		 * letting the upper software to handle this. */
	} else if (ret != 0) {
		goto QUIT_FREE_QOSRSLT;
	}

	/* QoS hash does not have valid life time */
	ret = cs_core_hmu_add_hash(hash_index, 0, NULL);
	if (ret == 0) {
		cs_core_hmu_set_result_idx(hash_index, qosrslt_index, 1);
	}

	return 0;

QUIT_FREE_QOSRSLT:
	cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT, qosrslt_index, false);

	return ret;
} /* cs_core_add_qos_hash */

/* manually add both FWD and QOS hashes and link both of them together */
int cs_core_add_hash(cs_fwd_hash_t *fwd_hash_entry, u16 *p_fwd_hash_idx,
		cs_qos_hash_t *qos_hash_entry, u16 *p_qos_hash_idx)
{
	int ret;
	u16 fwd_hash_index, qos_hash_index;

	if ((fwd_hash_entry == NULL) || (qos_hash_entry == NULL))
		return -1;

	ret = cs_core_add_fwd_hash(fwd_hash_entry, &fwd_hash_index);
	if (ret == FE_TABLE_EDUPLICATE && p_fwd_hash_idx != NULL)
		*p_fwd_hash_idx = fwd_hash_index;
	if (ret != 0)
		return ret;

	ret = cs_core_add_qos_hash(qos_hash_entry, &qos_hash_index);
	if (ret != 0) {
		cs_core_hmu_delete_hash_by_idx(fwd_hash_index);
		return ret;
	}

	ret = cs_core_hmu_link_fwd_and_qos_hash(fwd_hash_index, qos_hash_index, 1);
	if (ret != 0) {
		cs_core_hmu_delete_hash_by_idx(fwd_hash_index);
		cs_core_hmu_delete_hash_by_idx(qos_hash_index);
		return ret;
	}

	if (p_fwd_hash_idx != NULL)
		*p_fwd_hash_idx = fwd_hash_index;
	if (p_qos_hash_idx != NULL)
		*p_qos_hash_idx = qos_hash_index;

	return 0;
} /* cs_core_add_hash */

void set_fwd_hash_result_index_to_cs_cb(cs_kernel_accel_cb_t *cs_cb,
		cs_fwd_hash_t *fwd_hash)
{
#if 0
	if ((fwd_hash->action.l2.mac_sa_replace_en == 1) ||
			(fwd_hash->action.l2.mac_da_replace_en == 1)) {
		cs_cb->hw_rslt.has_l2rslt = 1;
		cs_cb->hw_rslt.l2rslt_idx = fwd_hash->action.l2.l2_index;
	}

	if ((fwd_hash->param.first_vlan_cmd != 0) ||
			(fwd_hash->param.second_vlan_cmd != 0)) {
		cs_cb->hw_rslt.has_fvlan = 1;
		cs_cb->hw_rslt.fvlan_idx =
			fwd_hash->action.l2.flow_vlan_index;
	}

	if (fwd_hash->action.l3.ip_sa_replace_en == 1) {
		cs_cb->hw_rslt.has_ip_sa = 1;
		cs_cb->hw_rslt.ip_sa_idx = fwd_hash->action.l3.ip_sa_index;
	}

	if (fwd_hash->action.l3.ip_da_replace_en == 1) {
		cs_cb->hw_rslt.has_ip_da = 1;
		cs_cb->hw_rslt.ip_da_idx = fwd_hash->action.l3.ip_da_index;
	}

	cs_cb->hw_rslt.has_voqpol = 1;
	cs_cb->hw_rslt.voqpol_idx = fwd_hash->action.dest.voq_pol_table_index;
#endif

	cs_cb->hw_rslt.has_fwdrslt = 1;
	cs_cb->hw_rslt.fwdrslt_idx = fwd_hash->param.result_index;
} /* set_fwd_hash_result_index_to_cs_cb */

static void set_qos_hash_result_index_to_cs_cb(cs_kernel_accel_cb_t *cs_cb,
		cs_qos_hash_t *qos_hash)
{
	cs_cb->hw_rslt.has_qosrslt = 1;
	cs_cb->hw_rslt.qosrslt_idx = qos_hash->rslt_idx;
} /* set_qos_hash_result_index_to_cs_cb */

/*
 * Based on given skb and its control block info, decide whether to create
 * hash entry(ies).  If yes, then create hash entry(ieS) according to its
 * module.  This API supports at most creating 4 FWD hash entries and 1
 * QOS hash entry at once.
 */
int cs_core_logic_add_hw_accel(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_fwd_hash_t *fwd_hash = NULL;
	cs_qos_hash_t qos_hash;
	u16 fwd_hash_idx[MAX_FWD_HASH_CNT], qos_hash_idx = 0;
	unsigned int fwd_cnt = 0, qos_cnt = 0;
	int ret, i, j;
	int result = CS_ACCEL_HASH_SUCCESS;
#if defined(CONFIG_CS752X_PROC) && defined(CONFIG_CS75XX_DOUBLE_CHECK)
	u32 crc32;
	u16 crc16, fwdrslt_idx;
	u8 mask_ptr;
	bool match;
#endif

	if (!cs_cb)
		return CS_ACCEL_HASH_DONT_CARE;

	fwd_hash = cs_zalloc(sizeof(cs_fwd_hash_t)*MAX_FWD_HASH_CNT, GFP_KERNEL);
	if (!fwd_hash)
		return CS_ACCEL_HASH_DONT_CARE;
	memset(&qos_hash, 0, sizeof(cs_qos_hash_t));

	ret = setup_hash_entry(skb, &fwd_hash[0], &fwd_cnt, &qos_hash,
			&qos_cnt);

	if (ret != 0) {
		result = CS_ACCEL_HASH_DONT_CARE;
		goto end;
	}

	if ((fwd_cnt == 1) && (qos_cnt == 0) &&
			(fwd_hash[0].fwd_app_type == CORE_FWD_APP_TYPE_NONE)) {
		result = CS_ACCEL_HASH_DONT_CARE;
		goto end;
	}

	if (cs_cb->common.module_mask & (CS_MOD_MASK_IPV4_NETFILTER |
				CS_MOD_MASK_IPV4_NETFILTER |
				CS_MOD_MASK_BRIDGE_NETFILTER))
		printk("%s::firewall enabled. Usually rule table hash "
				"creation function should use another API\n",
				__func__);	
	/* well this might get fixed later, because there might be cases
	 * when there are multiple passes, such as IPsec and MC, and we
	 * only need to apply QoS in the second pass, not first pass
	 * traffic.  We will need to adjust for that */
	for (i = 0; i < fwd_cnt; i++) {
		if (qos_cnt != 0)
			ret += cs_core_add_hash(&fwd_hash[i], &fwd_hash_idx[i],
					&qos_hash, &qos_hash_idx);
		else
			ret += cs_core_add_fwd_hash(&fwd_hash[i],
					&fwd_hash_idx[i]);

		switch (ret) {
		case FE_TABLE_EDUPLICATE:
#if defined(CONFIG_CS752X_PROC) && defined(CONFIG_CS75XX_DOUBLE_CHECK)
			if (cs_fe_double_chk > 0) {
				/* get original fwdrslt_index */
				ret = cs_fe_hash_get_hash(fwd_hash_idx[i],
						&crc32, &crc16, &mask_ptr,
						&fwdrslt_idx);
				if (ret != 0)
					goto FAIL_TO_ADD_HASH;

				/* do double check by software */
				ret = cs_core_double_chk_cmp(fwd_hash, fwdrslt_idx,
						&match);
				if (ret != 0)
					goto FAIL_TO_ADD_HASH;

				if (match == false) {
					/* double check fail */
					DBG(printk("%s:%d: double check fail\n",
						__func__, __LINE__)); 
					result = CS_ACCEL_HASH_DONT_CARE;
					goto end;
				} else {
					DBG(printk("%s:%d: double check ok\n",
						__func__, __LINE__)); 
				}
			}
			/* fall through */
#endif
		case 0:
			set_fwd_hash_result_index_to_cs_cb(cs_cb, &fwd_hash[i]);

			if ((i == 0) && (qos_cnt != 0))
				set_qos_hash_result_index_to_cs_cb(cs_cb,
						&qos_hash);
			cs_core_hmu_link_src_and_hash(cs_cb, fwd_hash_idx[i], NULL);
			if (qos_cnt > 0)
				qos_cnt--;
			break;
		default:
			goto FAIL_TO_ADD_HASH;
		}
	}

end:
	cs_free(fwd_hash);
	return result;

FAIL_TO_ADD_HASH:
	/*need to delete all the previous created hash*/

	for (j = i - 1; j >= 0; j--) {
		DBG(printk("%s:delete index=%d, fwd_hash_idx=0x%x, "
				"fwdrslt=0x%x\n", __func__, j, fwd_hash_idx[j],
				fwd_hash[j].param.result_index));
		/* cs_fe_hash_del_hash(fwd_hash_idx[j]); */
		/* cs_core_hmu_delete_hash_by_idx()
		 * 1. doesn't delete hw hash entry.
		 * 2. it would send timout notification to watch list (like
		 * 	fwdrslt, kernel adapt modules).
		 * So fwdrslt hw entry will be deleted in fwdrslt_hmu_callback()
		 */

		/* FIXME: for multicast, need to consider 2nd hash creation fails
		 * fastnet hash notification will arrive kernel adapt module at
		 * first then cs_core_hmu_delete_hash_by_idx notification:
		 * CS_CORE_HMU_RET_STATUS_DELETED_ALL */
		cs_core_hmu_delete_hash_by_idx(fwd_hash_idx[j]);
	}
	//memset(&cs_cb->hw_rslt, 0, sizeof(cb_result_index_t));
	cs_free(fwd_hash);
	return CS_ACCEL_HASH_FAIL;
}

static int create_logical_port_fwd_hash(void)
{
	fe_fwd_result_entry_t fwdrslt_entry;
	fe_voq_pol_entry_t voqpol_entry;
	fe_sw_hash_t key;
	u64 fwd_hm_flag;
	u32 crc32;
	u16 hash_index, crc16;
	unsigned int fwdrslt_idx, voqpol_idx;
	int  i;

	/* for GMAC ports: eth0, eth1, eth2 */
	memset(&fwdrslt_entry, 0x0, sizeof(fwdrslt_entry));
	memset(&voqpol_entry, 0x0, sizeof(voqpol_entry));
	memset(&key, 0x0, sizeof(key));
	fwdrslt_entry.dest.voq_policy = 1;
	for (i = 0; i < 3; i++) {
		if (cs_core_vtable_get_hashmask_flag_from_apptype(
					CORE_FWD_APP_TYPE_SEPARATE_LOGICAL_PORT,
					&fwd_hm_flag))
			continue;

		if (cs_core_vtable_get_hashmask_index_from_apptype(
					CORE_FWD_APP_TYPE_SEPARATE_LOGICAL_PORT,
					&key.mask_ptr_0_7))
			continue;

		key.lspid = GE_PORT0 + i;
		if (cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT))
			continue;

		voqpol_entry.voq_base = CPU_PORT0_VOQ_BASE + i * 8;
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voqpol_entry,
					&voqpol_idx))
			continue;

		fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwdrslt_entry,
					&fwdrslt_idx)) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
					voqpol_idx, false);
			continue;
		}


		if (cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
					(u16)fwdrslt_idx, &hash_index)) {
			cs_fe_fwdrslt_del_by_idx(fwdrslt_idx);
			continue;
		}

		/* we don't add these hashes to HMU, since these hashes should
		 * be there forever by default */
	}
	return 1;
}

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
static int create_pe_logical_port_fwd_hash(void)
{
        fe_fwd_result_entry_t fwdrslt_entry;
        fe_voq_pol_entry_t voqpol_entry;
        fe_sw_hash_t key;
        u64 fwd_hm_flag;
        u32 crc32;
        u16 hash_index, crc16;
        unsigned int fwdrslt_idx, voqpol_idx;
        int  i;

        /* for PE ports: PE0, PE1 */
        memset(&fwdrslt_entry, 0x0, sizeof(fwdrslt_entry));
        memset(&voqpol_entry, 0x0, sizeof(voqpol_entry));
        memset(&key, 0x0, sizeof(key));

        voqpol_entry.voq_base = CPU_PORT3_VOQ_BASE;
        if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voqpol_entry,
                        &voqpol_idx)) {
                return -1;
        }

        fwdrslt_entry.dest.voq_policy = 1;
        fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;

        /* create default for UUC from PE0*/
        for (i = 0; i < 16; i++) {
            if (cs_core_vtable_get_hashmask_flag_from_apptype(
                                    CORE_FWD_APP_TYPE_PE_RECIDX,
                                    &fwd_hm_flag)) {
                    continue;
            }

            if (cs_core_vtable_get_hashmask_index_from_apptype(
                                    CORE_FWD_APP_TYPE_PE_RECIDX,
                                    &key.mask_ptr_0_7)) {
                    continue;
            }

            key.lspid = ENCRYPTION_PORT;
            key.recirc_idx = (CS_FUNC_TYPE_WFO_11AC << 16) + i + 33;
            if (cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT)) {
                    continue;
            }

        	fwdrslt_entry.l3.ip_sa_index = (u16)(key.recirc_idx & 0x0fff);
        	fwdrslt_entry.l3.ip_da_index = (u16)((key.recirc_idx >> 12) & 0x00ff);
            if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwdrslt_entry,
                            &fwdrslt_idx)) {
                    cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
                        voqpol_idx, false);
                    continue;
            }

            if (cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
                            (u16)fwdrslt_idx, &hash_index)) {
                    cs_fe_fwdrslt_del_by_idx(fwdrslt_idx);
                    continue;
            }
        } /* for (i = 0; i < 16; i++) */

        /* create default for UUC from PE1*/
        for (i = 0; i < 16; i++) {
            if (cs_core_vtable_get_hashmask_flag_from_apptype(
                                    CORE_FWD_APP_TYPE_PE_RECIDX,
                                    &fwd_hm_flag)) {
                    continue;
            }

            if (cs_core_vtable_get_hashmask_index_from_apptype(
                                    CORE_FWD_APP_TYPE_PE_RECIDX,
                                    &key.mask_ptr_0_7)) {
                    continue;
            }

            key.lspid = ENCAPSULATION_PORT;
            key.recirc_idx = (CS_FUNC_TYPE_WFO_11N << 16) + i + 49;
            if (cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT)) {
                    continue;
            }

            if (cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
                            (u16)fwdrslt_idx, &hash_index)) {
                    cs_fe_fwdrslt_del_by_idx(fwdrslt_idx);
                    continue;
            }
        } /* for (i = 0; i < 16; i++) */

        return 1;
}
#else //CONFIG_CS75XX_OFFSET_BASED_QOS
/* BUG#39672: WFO NEC related features (Mutliple BSSID) */
static int create_pe_logical_port_fwd_hash(void)
{
        fe_fwd_result_entry_t fwdrslt_entry;
        fe_voq_pol_entry_t voqpol_entry;
        fe_sw_hash_t key;
        u64 fwd_hm_flag;
        u32 crc32;
        u16 hash_index, crc16;
        unsigned int fwdrslt_idx, voqpol_idx;
        int  i, j;

        /* for GMAC ports: eth0, eth1, eth2 */
        memset(&fwdrslt_entry, 0x0, sizeof(fwdrslt_entry));
        memset(&voqpol_entry, 0x0, sizeof(voqpol_entry));
        memset(&key, 0x0, sizeof(key));
        //fwdrslt_entry.dest.voq_policy = 1;
        for (i = 0; i < 2; i++) {
                for (j = 0; j < 4; j++) {
                        if (cs_core_vtable_get_hashmask_flag_from_apptype(
                                                CORE_FWD_APP_TYPE_PE_RECIDX,
                                                &fwd_hm_flag))
                                continue;

                        if (cs_core_vtable_get_hashmask_index_from_apptype(
                                                CORE_FWD_APP_TYPE_PE_RECIDX,
                                                &key.mask_ptr_0_7))
                                continue;

                        key.lspid = ENCRYPTION_PORT + i;
                        key.recirc_idx = j + 1;
                        if (cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT))
                                continue;

                        voqpol_entry.voq_base = CPU_PORT6_VOQ_BASE + i + j * 2;
                        if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &voqpol_entry,
                                        &voqpol_idx))
                                continue;

                        fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;
                        if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &fwdrslt_entry,
                                        &fwdrslt_idx)) {
                                cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
                                        voqpol_idx, false);
                                continue;
                        }

                        if (cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
                                        (u16)fwdrslt_idx, &hash_index)) {
                                cs_fe_fwdrslt_del_by_idx(fwdrslt_idx);
                                continue;
                        }

                }
        }
        return 1;
}
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

static int set_l4_port_hash_tbl(u16 port, u8 ip_prot, cs_l4port_hash_t *l4port_hash)
{
	cs_l4port_hash_t* l4port_hash_ptr = NULL, *entry_ptr = NULL;
	u16 entry;

	if (ip_prot == SOL_TCP)
		l4port_hash_ptr = tcp_l4port_hash_tbl;
	else if (ip_prot == SOL_UDP)
		l4port_hash_ptr = udp_l4port_hash_tbl;
	else
		return -1;
	
	/* find a entry that can be used */
	for (entry = 0; entry < MAX_PORT_LIST_SIZE; entry++, l4port_hash_ptr++) {
		if (l4port_hash_ptr->l4port == 0 && entry_ptr == NULL)
			entry_ptr = l4port_hash_ptr;

		if (l4port_hash_ptr->l4port == port) {
			entry_ptr = l4port_hash_ptr;
			break;
		}
	}
	/* all entries are used */
	if (entry_ptr == NULL)
		return -1;
	/* update new data */
	if (l4port_hash == NULL)
		memset(entry_ptr, 0, sizeof(cs_l4port_hash_t));
	else
		memcpy(entry_ptr, l4port_hash, sizeof(cs_l4port_hash_t));
	return 0;
}

static int get_l4_port_hash_tbl(u16 port, u8 ip_prot, cs_l4port_hash_t *l4port_hash)
{
	cs_l4port_hash_t* l4port_hash_ptr = NULL;
	u16 entry;

	if (l4port_hash == NULL)
		return -1;
	
	if (ip_prot == SOL_TCP)
		l4port_hash_ptr = tcp_l4port_hash_tbl;
	else if (ip_prot == SOL_UDP)
		l4port_hash_ptr = udp_l4port_hash_tbl;
	else
		return -1;
	
	/* find the entry */
	for (entry = 0; entry < MAX_PORT_LIST_SIZE; entry++, l4port_hash_ptr++) {
		if (l4port_hash_ptr->l4port != port) 
			continue;		
		memcpy(l4port_hash, l4port_hash_ptr, sizeof(cs_l4port_hash_t));
		return 0;
	}
	return -1;
}

int cs_core_logic_add_l4_port_fwd_hash(u16 port, u8 ip_prot)
{
	cs_l4port_hash_t l4port_hash;
	fe_sw_hash_t key;
	u16 hash_index;
	u32 crc32;
	u16 crc16;
	int ieth;
	int ret = 0;

	DBG(printk("%s: port=%u, ip_prot=%u\n", __func__, port, ip_prot));

	if ((ip_prot != SOL_TCP) && (ip_prot != SOL_UDP))
		return -1;

	memset(&l4port_hash, 0, sizeof(cs_l4port_hash_t));
	l4port_hash.l4port = port;

	for (ieth = 0; ieth < GE_PORT_NUM; ieth++) {
		/*** source port ***/
		memset(&key, 0x0, sizeof(fe_sw_hash_t));
		key.lspid = GE_PORT0 + ieth;
		key.ip_prot = ip_prot;
		key.l4_sp = port;
		key.mask_ptr_0_7 = l4_sport_hm_idx;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
			goto update;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, l4_sport_hm_idx,
			l4port_fwdrslt_idx[ieth], &hash_index);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
			goto update;
		}

		DBG(printk("%s: l4_sport_hm_idx=%u, l4port_fwdrslt_idx[%d]=%u, hash_index=%u\n",
				__func__, l4_sport_hm_idx, ieth, l4port_fwdrslt_idx[ieth], hash_index));

		cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT,
				l4port_fwdrslt_idx[ieth]);
		
		l4port_hash.hash_index[ieth*2] = hash_index;

		/*** dest port ***/
		memset(&key, 0x0, sizeof(fe_sw_hash_t));
		key.lspid = GE_PORT0 + ieth;
		key.ip_prot = ip_prot;
		key.l4_dp = port;
		key.mask_ptr_0_7 = l4_dport_hm_idx;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
			goto update;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, l4_dport_hm_idx,
			l4port_fwdrslt_idx[ieth], &hash_index);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
			goto update;
		}

		DBG(printk("%s: l4_dport_hm_idx=%u, l4port_fwdrslt_idx[%d]=%u, hash_index=%u\n",
				__func__, l4_dport_hm_idx, ieth, l4port_fwdrslt_idx[ieth], hash_index));

		cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT,
				l4port_fwdrslt_idx[ieth]);

		l4port_hash.hash_index[ieth*2+1] = hash_index;
	}

update:
	/* update hash indices to l4port_hash_tbl */
	if (l4port_hash.hash_index[0])
		set_l4_port_hash_tbl(port, ip_prot, &l4port_hash);

	return ret;
}

int cs_core_logic_del_l4_port_fwd_hash(u16 port, u8 ip_prot)
{
	u16 hash_index, toto_hash;
	cs_l4port_hash_t l4port_hash;

	DBG(printk("%s: port=%u, ip_prot=%u\n", __func__, port, ip_prot));

	if ((ip_prot != SOL_TCP) && (ip_prot != SOL_UDP))
		return -1;
	
	/* get hash indices from l4port_hash_tbl */
	if (get_l4_port_hash_tbl(port, ip_prot, &l4port_hash))
		return -1;
	
	toto_hash = GE_PORT_NUM*2;
	/* delete hashes */
	for (hash_index = 0; hash_index < toto_hash; hash_index++) {
		if (!l4port_hash.hash_index[hash_index])
			continue;
		cs_fe_hash_del_hash(l4port_hash.hash_index[hash_index]);
	}
	
	/* clear hash indices from l4port_hash_tbl */
	set_l4_port_hash_tbl(port, ip_prot, NULL);
	return 0;
}

static int create_l4_port_fwd_hash(void)
{
	int ieth, iport;
	u16 *port_ptr;
	u32 hash_tbl_size;
	
	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_L4_SPORT,
			&l4_sport_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_L4_SPORT\n", __func__);
		return -1;
	}

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_L4_DPORT,
			&l4_dport_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_L4_DPORT\n", __func__);
		return -1;
	}
	
	hash_tbl_size = MAX_PORT_LIST_SIZE*sizeof(cs_l4port_hash_t);
	memset(&tcp_l4port_hash_tbl, 0, hash_tbl_size);
	memset(&udp_l4port_hash_tbl, 0, hash_tbl_size);

	/* hash forward result for each GE port */
	for (ieth = 0; ieth < GE_PORT_NUM; ieth++) {
		memset(&l4port_voqpol_entry[ieth], 0x0, sizeof(fe_voq_pol_entry_t));
		l4port_voqpol_entry[ieth].voq_base = CPU_PORT0_VOQ_BASE + 5 + ieth * 8;	// CPU VoQ#5
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &l4port_voqpol_entry[ieth],
				&l4port_voqpol_idx[ieth])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		}

		memset(&l4port_fwdrslt_entry[ieth], 0x0, sizeof(fe_fwd_result_entry_t));
		l4port_fwdrslt_entry[ieth].dest.voq_policy = 0;
		l4port_fwdrslt_entry[ieth].dest.voq_pol_table_index = l4port_voqpol_idx[ieth];
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &l4port_fwdrslt_entry[ieth],
				&l4port_fwdrslt_idx[ieth])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
					l4port_voqpol_idx[ieth], false);
		}
	}

#ifdef CONFIG_CS752X_ACCEL_KERNEL
	/* create hashes */
	port_ptr = cs_forward_port_list_get(PRIORITY_LIST_TUPE_TCP);
	for (iport = 0; iport < MAX_PORT_LIST_SIZE; port_ptr++, iport++) {
		if (*port_ptr != 0) {
			cs_core_logic_add_l4_port_fwd_hash(*port_ptr, SOL_TCP);
		} else {
			break;
		}
	}

	port_ptr = cs_forward_port_list_get(PRIORITY_LIST_TUPE_UDP);
	for (iport = 0; iport < MAX_PORT_LIST_SIZE; port_ptr++, iport++) {
		if (*port_ptr != 0) {
			cs_core_logic_add_l4_port_fwd_hash(*port_ptr, SOL_UDP);
		} else {
			break;
		}
	}
#endif
	return 0;
}

int cs_core_hw_except_add_rule(int type)
{
	int ret;
	unsigned int result_index;
	fe_acl_entry_t acl_entry;
	cs_vtable_t *table = NULL;
	int mc_class_sidx = 3;

	memset(&acl_entry.rule, 0xff, sizeof(acl_entry.rule));
	memset(&acl_entry.action, 0, sizeof(acl_entry.action));

	if ((type >= CS_CORE_HW_EXCEPT_MAX) || 
		(cs_acl_rule_idx[type] != ACL_RULE_IDX_INVALID)) {
		DBG(printk("%s: ACL rule already exists.\n", __func__));
		return -1;
	}

	table = cs_core_vtable_get(CORE_VTABLE_TYPE_L3_MCAST_V4);
	if (table) {
		mc_class_sidx = table->class_index;
	}
	
	/* ACL rule */
	acl_entry.rule.misc.ne_vec = 0;
	acl_entry.rule.l3.ip_sa_mask = 0;
	acl_entry.rule.l3.ip_da_mask = 0;
	acl_entry.rule.rule_valid = 1;

	switch (type) {
	case CS_CORE_HW_MCIDX2:
		acl_entry.rule.misc.mc_idx = 0x2;
		acl_entry.rule.misc.mc_idx_mask = 0x0;
		acl_entry.rule.misc.class_svidx = mc_class_sidx;
		acl_entry.rule.misc.class_svidx_mask = 0x0;
		break;
	case CS_CORE_HW_MCIDX7:
		acl_entry.rule.misc.mc_idx = 0x7;
		acl_entry.rule.misc.mc_idx_mask = 0x0;
		acl_entry.rule.misc.class_svidx = mc_class_sidx;
		acl_entry.rule.misc.class_svidx_mask = 0x0;
		break;
	case CS_CORE_HW_EXCEPT_TTL_EQUAL_1:
		acl_entry.rule.l3.ttl_hoplimit = 1;
		acl_entry.rule.l3.ttl_hoplimit_mask = 0;
		acl_entry.rule.l3.ip_vld = 1;
		acl_entry.rule.l3.ip_vld_mask = 0;
		break;
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	case CS_CORE_HW_EXCEPT_MC_PKT_TTL_EQUAL_255:
		acl_entry.rule.l3.ttl_hoplimit = 255;
		acl_entry.rule.l3.ttl_hoplimit_mask = 0;
		acl_entry.rule.l3.ip_vld = 1;
		acl_entry.rule.l3.ip_vld_mask = 0;
		acl_entry.rule.misc.spl_pkt_vec = (1 << 0);		/* Multicast packet */
		acl_entry.rule.misc.spl_pkt_vec_mask = ~(1 << 0);
		break;
#endif
        case CS_CORE_HW_EXCEPT_IP_OPTIONS:
                acl_entry.rule.l3.options = 1;
                acl_entry.rule.l3.ip_options_mask = 0;
                break;
	case CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC0:
		acl_entry.rule.misc.class_hit = 1;
		acl_entry.rule.misc.class_hit_mask = 0;
		acl_entry.rule.misc.class_svidx =
			vtable_list[CORE_VTABLE_TYPE_L2_FLOW]->class_index;
		acl_entry.rule.misc.class_svidx_mask = 0;
		acl_entry.rule.misc.lspid = 0;
		acl_entry.rule.misc.lspid_mask = 0;
		break;
	case CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC1:
		acl_entry.rule.misc.class_hit = 1;
		acl_entry.rule.misc.class_hit_mask = 0;
		acl_entry.rule.misc.class_svidx =
			vtable_list[CORE_VTABLE_TYPE_L2_FLOW]->class_index;
		acl_entry.rule.misc.class_svidx_mask = 0;
		acl_entry.rule.misc.lspid = 1;
		acl_entry.rule.misc.lspid_mask = 0;
		break;
	case CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC2:
		acl_entry.rule.misc.class_hit = 1;
		acl_entry.rule.misc.class_hit_mask = 0;
		acl_entry.rule.misc.class_svidx =
			vtable_list[CORE_VTABLE_TYPE_L2_FLOW]->class_index;
		acl_entry.rule.misc.class_svidx_mask = 0;
		acl_entry.rule.misc.lspid = 2;
		acl_entry.rule.misc.lspid_mask = 0;
		break;
	default:
		DBG(printk("%s:unknown rule type = %d\n", __func__, type));
		return -1;
	}



	/* Action */
	/*
	 * Replace FwdRslt actions used in HashHash Table
	 * -- So we need to set all action priority as highest (7)
	 * -- to replace FwdRslt action (its priority defined in hash tuple)
	 */
	switch (type) {
	case CS_CORE_HW_EXCEPT_TTL_EQUAL_1:
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	case CS_CORE_HW_EXCEPT_MC_PKT_TTL_EQUAL_255:
#endif
        case CS_CORE_HW_EXCEPT_IP_OPTIONS:
		acl_entry.action.misc.ldpid = CPU_PORT;
		acl_entry.action.misc.voq_vld = 1;
		acl_entry.action.misc.voq_pri = 7;
		acl_entry.action.misc.voq = CPU_PORT7_VOQ_BASE;

		/* ACL action to replace drop result */
		acl_entry.action.misc.drop_permit_vld = 1;
		acl_entry.action.misc.drop = 0;
		acl_entry.action.misc.permit = 1;
		acl_entry.action.misc.drop_permit_pri = 7;

		acl_entry.action.l2._8021p_1_pri = 7;
		acl_entry.action.l2._8021p_1_vld = 1;
		acl_entry.action.l2._8021p_2_pri = 7;
		acl_entry.action.l2._8021p_2_vld = 1;
		acl_entry.action.l2.dei_1_pri = 7;
		acl_entry.action.l2.dei_1_vld = 1;
		acl_entry.action.l2.dei_2_pri = 7;
		acl_entry.action.l2.dei_2_vld = 1;
		acl_entry.action.l3.dscp_pri = 7;
		acl_entry.action.l3.dscp_vld = 1;
		acl_entry.action.l2.mac_da_sa_replace_en_pri = 7;
		acl_entry.action.l2.mac_da_sa_replace_en_vld = 1;
		acl_entry.action.l2.first_vlan_cmd_pri = 7;
		acl_entry.action.l2.first_vlan_cmd_vld = 1;
		acl_entry.action.l2.second_vlan_cmd_pri = 7;
		acl_entry.action.l2.second_vlan_cmd_vld = 1;
		acl_entry.action.l3.ip_sa_replace_en_pri = 7;
		acl_entry.action.l3.ip_sa_replace_en_vld = 1;
		acl_entry.action.l3.ip_da_replace_en_pri = 7;
		acl_entry.action.l3.ip_da_replace_en_vld = 1;
		acl_entry.action.l3.decr_ttl_hoplimit_pri = 7;
		acl_entry.action.l3.decr_ttl_hoplimit_vld = 1;
		acl_entry.action.l4.sp_replace_en_pri = 7;
		acl_entry.action.l4.sp_replace_en_vld = 1;
		acl_entry.action.l4.dp_replace_en_pri = 7;
		acl_entry.action.l4.dp_replace_en_vld = 1;
		break;
	case CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC0:
	case CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC1:
	case CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC2:
		acl_entry.action.misc.drop_permit_vld = 1;
		acl_entry.action.misc.drop_permit_pri = 7;
		acl_entry.action.misc.drop = 1;
		acl_entry.action.misc.permit = 0;
		break;
	case CS_CORE_HW_MCIDX2:
	case CS_CORE_HW_MCIDX7:
		acl_entry.action.l3.change_dscp_en = 0;
		acl_entry.action.l3.dscp_vld = 1;
		break;
	default:
		DBG(printk("%s:unknown action type = %d\n", __func__, type));
		return -1;
	}

	ret = cs_fe_table_add_entry(FE_TABLE_ACL_RULE, (void *)&acl_entry,
			&result_index);

	if (ret == 0)
		cs_acl_rule_idx[type] = result_index;
	DBG(printk("%s:ADD acl_rule of type %d at index = %d, ret = %d\n",
				__func__, type, result_index, ret));
	DBG(cs_fe_table_print_entry(FE_TABLE_ACL_RULE, result_index));
	return ret;
}

int cs_core_hw_except_del_rule(int type)
{
	int ret;
	
	if ((type >= CS_CORE_HW_EXCEPT_MAX) || 
		(cs_acl_rule_idx[type] == ACL_RULE_IDX_INVALID)) {
		DBG(printk("%s: ACL rule doesn't exist.\n", __func__));
		return -1;
	}

	ret = cs_fe_table_del_entry_by_idx(FE_TABLE_ACL_RULE, 
			cs_acl_rule_idx[type], false);
	DBG(printk("%s:DEL acl_rule of type %d at index = %d, ret = %d\n",
				__func__, type, cs_acl_rule_idx[type], ret));

	if (ret == 0)
		cs_acl_rule_idx[type] = ACL_RULE_IDX_INVALID;
	
	return ret;
}

int cs_core_hw_except_update_rule_state(int type, bool enbl)
{
	int ret;
	fe_acl_entry_t acl_entry;

	if ((type >= CS_CORE_HW_EXCEPT_MAX) || 
		(cs_acl_rule_idx[type] == ACL_RULE_IDX_INVALID)) {
		DBG(printk("%s: ACL rule doesn't exist.\n", __func__));
		return -1;
	}

	ret = cs_fe_table_get_entry(FE_TABLE_ACL_RULE, cs_acl_rule_idx[type],
			&acl_entry);

	if (ret) {
		DBG(printk("%s:Get acl_rule of type %d at index = %d, ret = %d\n",
				__func__, type, cs_acl_rule_idx[type], ret));
		goto exit;
	}

	if (acl_entry.rule.rule_valid == enbl) {
		DBG(printk("%s: ACL rule is not changed.\n", __func__));
		return 0;
	}
		
	acl_entry.rule.rule_valid = enbl;

	
	ret = cs_fe_table_set_entry(FE_TABLE_ACL_RULE, cs_acl_rule_idx[type],
			&acl_entry);

	DBG(printk("%s:Change acl_rule of type %d at index = %d, ret = %d\n",
				__func__, type, cs_acl_rule_idx[type], ret));
	DBG(cs_fe_table_print_entry(FE_TABLE_ACL_RULE, cs_acl_rule_idx[type]));
exit:	
	return ret;
}

int cs_core_hw_except_add_rules(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < CS_CORE_HW_EXCEPT_MAX; i++)
		cs_acl_rule_idx[i] = ACL_RULE_IDX_INVALID;
	
	for (i = 0; i < CS_CORE_HW_EXCEPT_MAX; i++)
		ret |= cs_core_hw_except_add_rule(i);

	return ret;
}

int cs_core_set_promiscuous_port(int port, int enbl)
{
	int type;
	int ret;
	
	switch (port) {
	case 0:
		type = CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC0;
		break;
	case 1:
		type = CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC1;
		break;
	case 2:
		type = CS_CORE_HW_EXCEPT_DA_MAC_NO_MATCH_GMAC2;
		break;
	default:
		DBG(printk("%s:invalid port = %d\n", __func__, port));
		return -1;
		
	}

	enbl = !enbl; /* invert enbl */
	
	cs_core_hw_except_update_rule_state(type, enbl);
	
	return ret;
}

int cs_core_logic_output_set_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	int ret = 0;
	if (cs_cb == NULL)
		return -1;

	if ((cs_cb->common.tag != CS_CB_TAG) ||
		(cs_cb->common.sw_only & (CS_SWONLY_DONTCARE |
					  CS_SWONLY_STATE)))
		return -1;

#ifdef CONFIG_CS752X_ACCEL_KERNEL
	if (cs_vlan_set_output_cb(skb) != 0) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		ret = -1;
	} else if (cs_pppoe_kernel_set_output_cb(skb) != 0) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		ret = -1;
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS		
//++Bug#40322
	} else if (cs_dscp_set_output_cb(skb) != 0) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		ret = -1;
//--Bug#40322
#endif
	}
	cs_cb->fill_ouput_done = 1;
#endif
	return ret;

}
EXPORT_SYMBOL(cs_core_logic_output_set_cb);

int cs_core_logic_add_connections(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	int ret = CS_ACCEL_HASH_DONT_CARE;;

	if (cs_cb == NULL)
		return CS_ACCEL_HASH_DONT_CARE;

	if ((cs_cb->common.tag != CS_CB_TAG) ||
			(cs_cb->common.sw_only & (CS_SWONLY_DONTCARE |
						  CS_SWONLY_STATE)))
		return CS_ACCEL_HASH_DONT_CARE;


#ifdef CONFIG_CS752X_ACCEL_KERNEL
	if (cs_cb->fill_ouput_done == 0) {
#if 0
		if (cs_vlan_set_output_cb(skb) != 0)
			return CS_ACCEL_HASH_DONT_CARE;
		if (cs_pppoe_kernel_set_output_cb(skb) != 0)
			return CS_ACCEL_HASH_DONT_CARE;
#else
		if (cs_core_logic_output_set_cb(skb) != 0)
			return CS_ACCEL_HASH_DONT_CARE;
#endif
	}

	if (cs_accel_hw_accel_enable(CS_MOD_MASK_QOS_FIELD_CHANGE) == 0)
		cs_cb->common.module_mask &= ~CS_MOD_MASK_QOS_FIELD_CHANGE;
#endif

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) ||\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
	
	if ((cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) && 
		(cs_cb->common.module_mask & CS_MOD_MASK_L2TP)) {
		/* L2TP over IPSec */
#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) &&\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
		DBG(printk("%s:%d: L2TP over IPSec\n", __func__, __LINE__));
		DBG(printk("%s:%d: data = 0x%p, len = %d, L4 ptr = 0x%p,\n"
			"\t network ptr = 0x%p, network hdr len = %d,\n"
			"\t mac ptr = 0x%p, mac_len = %d\n",
			__func__, __LINE__,
			skb->data, skb->len, skb_transport_header(skb),
			skb_network_header(skb), skb_network_header_len(skb),
			skb_mac_header(skb), skb->mac_len));
		DBG(cs_dump_data(skb->data, skb->len));

		if (cs_vpn_offload_mode & CS_VPN_OFFLOAD_L2TP_OVER_IPSEC)
			ret = cs_l2tp_ipsec_hw_accel_add(skb);
#endif		
		return ret;
	} else if (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) {
		/* IPSec tunnel mode */
#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL)
		DBG(printk("%s:%d: IPSec tunnel only\n", __func__, __LINE__));
		DBG(printk("%s:%d: data = 0x%p, len = %d, L4 ptr = 0x%p,\n"
			"\t network ptr = 0x%p, network hdr len = %d,\n"
			"\t mac ptr = 0x%p, mac_len = %d\n",
			__func__, __LINE__,
			skb->data, skb->len, skb_transport_header(skb),
			skb_network_header(skb), skb_network_header_len(skb),
			skb_mac_header(skb), skb->mac_len));
		DBG(cs_dump_data(skb->data, skb->len));

		if (cs_vpn_offload_mode & CS_VPN_OFFLOAD_IPSEC)
			ret = cs_ipsec_hw_accel_add(skb);
#endif
		return ret;
	} else if (cs_cb->common.module_mask & CS_MOD_MASK_L2TP) {
		/* L2TP tunnel mode */
#if defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
		DBG(printk("%s:%d: L2TP tunnel only\n", __func__, __LINE__));
		DBG(printk("%s:%d: data = 0x%p, len = %d, L4 ptr = 0x%p,\n"
			"\t network ptr = 0x%p, network hdr len = %d,\n"
			"\t mac ptr = 0x%p, mac_len = %d\n",
			__func__, __LINE__,
			skb->data, skb->len, skb_transport_header(skb),
			skb_network_header(skb), skb_network_header_len(skb),
			skb_mac_header(skb), skb->mac_len));
		DBG(cs_dump_data(skb->data, skb->len));

		if (cs_vpn_offload_mode & CS_VPN_OFFLOAD_L2TP)
			ret = cs_l2tp_hw_accel_add(skb);
#endif
		return ret;
	}
#endif
	if (cs_accel_hw_accel_enable(cs_cb->common.module_mask)) {
		DBG(printk("%s create hw accel hash \n", __func__));
		ret = cs_core_logic_add_hw_accel(skb);
	}

	if ((ret != CS_ACCEL_HASH_SUCCESS) &&
		 cs_accel_fastnet_enable(cs_cb->common.module_mask)) {
		DBG(printk("%s create fastnet accel hash\n", __func__));
		ret = cs_core_fastnet_add_fwd_hash(skb);

		/*FIXME:
		 * for multicast, need to consider 2nd hash creation fail.
		 * In such case, we cannot create fastnet hash
		 */
		/*FIXME: adding fastnet forwarding hash*/
	}

	if (ret != CS_ACCEL_HASH_DONT_CARE) {
		if (ret == CS_ACCEL_HASH_SUCCESS) {
			cs_core_hmu_callback_for_hash_creation_pass(cs_cb);
		} else {
			cs_core_hmu_callback_for_hash_creation_fail(cs_cb);
		}
	}

	return ret;
}


int cs_core_logic_set_lifetime(cs_kernel_accel_cb_t *cb,
		unsigned int lifetime_sec)
{
	unsigned long new_lifetime = msecs_to_jiffies(lifetime_sec * 1000);

	if (cb == NULL)
		return -1;

	if (new_lifetime < cb->lifetime)
		cb->lifetime = new_lifetime;

	return 0;
} /* cs_core_logic_set_lifetime */

int cs_core_logic_add_swid64(cs_kernel_accel_cb_t *cb, u64 swid64)
{
	unsigned int mod_id = CS_SWID64_TO_MOD_ID(swid64);

	if (cb == NULL)
		return -1;

	if (cb->common.swid[mod_id] != CS_INVALID_SWID64)
		return -1;

	cb->common.swid[mod_id] = swid64;
	cb->common.swid_cnt++;

	return 0;
} /* cs_core_logic_add_swid64 */

void cs_core_set_sw_only(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (cs_cb == NULL)
		return;

	cs_cb->common.sw_only = CS_SWONLY_STATE;
}
EXPORT_SYMBOL(cs_core_set_sw_only);

int cs_igmp_init(void)
{
	fe_fwd_result_entry_t igmp_fwdrslt_entry[GE_PORT_NUM];
	fe_voq_pol_entry_t igmp_voqpol_entry[GE_PORT_NUM];
	unsigned int igmp_fwdrslt_idx[GE_PORT_NUM], igmp_voqpol_idx[GE_PORT_NUM];
	__u8 igmp_hm_idx = ~(0x0);

	fe_fwd_result_entry_t icmpv6_fwdrslt_entry[GE_PORT_NUM];
	fe_voq_pol_entry_t icmpv6_voqpol_entry[GE_PORT_NUM];
	unsigned int icmpv6_fwdrslt_idx[GE_PORT_NUM], icmpv6_voqpol_idx[GE_PORT_NUM];
	__u8 icmpv6_hm_idx = ~(0x0);

	fe_sw_hash_t key;
	u16 hash_index;
	u32 crc32;
	u16 crc16;
	int i;
	int ret;

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_IP_PROT, &igmp_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_IP_PROT\n", __func__);
		return CS_E_INIT;
	}

	icmpv6_hm_idx = igmp_hm_idx;


	/*** IGMP ***/

	/* hash forward result for each GE port */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&igmp_voqpol_entry[i], 0x0, sizeof(fe_voq_pol_entry_t));
		igmp_voqpol_entry[i].voq_base = CPU_PORT0_VOQ_BASE + 0 + i * 8; // CPU VoQ#0
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &igmp_voqpol_entry[i],
				&igmp_voqpol_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		}

		memset(&igmp_fwdrslt_entry[i], 0x0, sizeof(fe_fwd_result_entry_t));
		igmp_fwdrslt_entry[i].dest.voq_policy = 0;
		igmp_fwdrslt_entry[i].dest.voq_pol_table_index = igmp_voqpol_idx[i];
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &igmp_fwdrslt_entry[i],
				&igmp_fwdrslt_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, igmp_voqpol_idx[i], false);
		}
	}

	/* hash entry for IGMP packet */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&key, 0x0, sizeof(fe_sw_hash_t));
		key.lspid = GE_PORT0 + i;
		key.ip_prot = PROTO_IGMP;
		key.ip_valid = 1;
		key.mask_ptr_0_7 = igmp_hm_idx;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
			return CS_E_INIT;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, igmp_hm_idx,
			igmp_fwdrslt_idx[i], &hash_index);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
			return CS_E_INIT;
		}

		DBG(printk(KERN_INFO "%s: igmp_hm_idx=%u, igmp_fwdrslt_idx[%d]=%u, hash_index=%u\n",
				__func__, igmp_hm_idx, i, igmp_fwdrslt_idx[i], hash_index));

		cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT, igmp_fwdrslt_idx[i]);
	}

	/*** ICMPv6 ***/

	/* hash forward result for each GE port; same forward result as IGMP */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&icmpv6_voqpol_entry[i], 0x0, sizeof(fe_voq_pol_entry_t));
		icmpv6_voqpol_entry[i].voq_base = CPU_PORT0_VOQ_BASE + 0 + i * 8; // CPU VoQ#0
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &icmpv6_voqpol_entry[i],
				&icmpv6_voqpol_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		}

		memset(&icmpv6_fwdrslt_entry[i], 0x0, sizeof(fe_fwd_result_entry_t));
		icmpv6_fwdrslt_entry[i].dest.voq_policy = 0;
		icmpv6_fwdrslt_entry[i].dest.voq_pol_table_index = icmpv6_voqpol_idx[i];
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &icmpv6_fwdrslt_entry[i],
				&icmpv6_fwdrslt_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, icmpv6_voqpol_idx[i], false);
		}
	}

	/* hash entry for ICMPv6 packet */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&key, 0x0, sizeof(fe_sw_hash_t));
		key.lspid = GE_PORT0 + i;
		key.ip_prot = PROTO_ICMPV6;
		key.ip_valid = 1;
		key.mask_ptr_0_7 = icmpv6_hm_idx;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
			return CS_E_INIT;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, icmpv6_hm_idx,
			icmpv6_fwdrslt_idx[i], &hash_index);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
			return CS_E_INIT;
		}

		DBG(printk(KERN_INFO "%s: icmpv6_hm_idx=%u, icmpv6_fwdrslt_idx[%d]=%u, hash_index=%u\n",
				__func__, icmpv6_hm_idx, i, icmpv6_fwdrslt_idx[i], hash_index));

		cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT, icmpv6_fwdrslt_idx[i]);
	}
	return 0;
}

bool set_dhcp_port_hash(u16 port,int ieth,u8 dhcp_dport_hm_idx,unsigned int dhcp_fwdrslt_idx[GE_PORT_NUM])
{
	u8 ip_prot = SOL_UDP;
	fe_sw_hash_t key;
	u32 crc32;
	u16 crc16;
	int ret = 0;
	u16 hash_index;

	/*** dest port ***/
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.lspid = GE_PORT0 + ieth;
	key.ip_prot = ip_prot;
	key.l4_dp = port;
	key.mask_ptr_0_7 = dhcp_dport_hm_idx;

	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
		return FALSE;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, dhcp_dport_hm_idx, dhcp_fwdrslt_idx[ieth], &hash_index);
	if (ret != 0) {
		printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
		return false;
	}

	DBG(printk("%s: dhcp_dport_hm_idx=%u, dhcp_fwdrslt_idx[%d]=%u, hash_index=%u\n",
			__func__, dhcp_dport_hm_idx, ieth, dhcp_fwdrslt_idx[ieth], hash_index));

	cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT,dhcp_fwdrslt_idx[ieth]);
	return true;
}


void cs_dhcp_init(void)
{
	int ieth;
	const u16 dhcpPort = 67;
	const u16 dhcpv6Port = 547;
	const u16 dhcpPortClient = 68;
	const u16 dhcpv6PortClient = 548;
	const unsigned int DHCPQueueSelection = 2;

	fe_fwd_result_entry_t dhcp_fwdrslt_entry[GE_PORT_NUM];
	fe_voq_pol_entry_t dhcp_voqpol_entry[GE_PORT_NUM];
	unsigned int dhcp_fwdrslt_idx[GE_PORT_NUM], dhcp_voqpol_idx[GE_PORT_NUM];
	__u8 dhcp_dport_hm_idx = ~(0x0);

	if (cs_core_vtable_get_hashmask_index_from_apptype( CORE_FWD_APP_TYPE_L4_DPORT,	&dhcp_dport_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_L4_DPORT\n", __func__);
		return;
	}

	for (ieth = 0; ieth < GE_PORT_NUM; ieth++) {
		/* hash forward result for each GE port */
		memset(&dhcp_voqpol_entry[ieth], 0x0, sizeof(fe_voq_pol_entry_t));
		dhcp_voqpol_entry[ieth].voq_base = CPU_PORT0_VOQ_BASE + DHCPQueueSelection + ieth * 8;	// CPU VoQ#2, 3rd high priority
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &dhcp_voqpol_entry[ieth], &dhcp_voqpol_idx[ieth])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		}

		memset(&dhcp_fwdrslt_entry[ieth], 0x0, sizeof(fe_fwd_result_entry_t));
		dhcp_fwdrslt_entry[ieth].dest.voq_policy = 0;
		dhcp_fwdrslt_entry[ieth].dest.voq_pol_table_index = dhcp_voqpol_idx[ieth];
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &dhcp_fwdrslt_entry[ieth], &dhcp_fwdrslt_idx[ieth])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, dhcp_voqpol_idx[ieth], false);
		}
	}

	//DHCP server configuration
	set_dhcp_port_hash( dhcpPort,GE_PORT2,dhcp_dport_hm_idx,dhcp_fwdrslt_idx);
	set_dhcp_port_hash( dhcpv6Port,GE_PORT2,dhcp_dport_hm_idx,dhcp_fwdrslt_idx);

	//DHCP client configuration
	set_dhcp_port_hash( dhcpPortClient,GE_PORT0,dhcp_dport_hm_idx,dhcp_fwdrslt_idx);
	set_dhcp_port_hash( dhcpv6PortClient,GE_PORT0,dhcp_dport_hm_idx,dhcp_fwdrslt_idx);
	set_dhcp_port_hash( dhcpPortClient,GE_PORT1,dhcp_dport_hm_idx,dhcp_fwdrslt_idx);
	set_dhcp_port_hash( dhcpv6PortClient,GE_PORT1,dhcp_dport_hm_idx,dhcp_fwdrslt_idx);
}

void gwr_set_mc_1p_dscp_mapping_qos_hash(void)
{
	fe_hash_mask_entry_t hash_mask;
	fe_qos_result_entry_t qos_rslt;
	fe_sw_hash_t key;
	unsigned int qosrslt_idx;
	u32 crc32;
	u16 crc16;
	int ret, i;
	u8 qos_hm_idx;
	u16 qos_hash_idx;

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_QOS_APP_TYPE_1P_DSCP_MAP,
			&qos_hm_idx) != 0) {
		printk("%s cannot get qos hm idx\n", __func__);
		return;
	}

	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.mask_ptr_0_7 = qos_hm_idx;
	key.lspid = MCAST_PORT;
	/* encoded TPID =
	 * 4:0x8100
	 * 5:0x9100
	 * 6:0x88a8
	 * 7:0x9200
	 */
	key.tpid_enc_1 = 4;

	for (i = 0; i < 8; i++) {
		memset(&qos_rslt, 0x0, sizeof(fe_qos_result_entry_t));
		key._8021p_1 = i;
		switch (i) {
			case 0:
				qos_rslt.voq_cos = 5;
				qos_rslt.dscp = 0x00;
				break;
			case 1:
				qos_rslt.voq_cos = 7;
				qos_rslt.dscp = 0x25;
				qos_rslt.change_dscp_en = 1;
				break;
			case 2:
				qos_rslt.voq_cos = 6;
				qos_rslt.dscp = 0x0;
				qos_rslt.change_dscp_en = 1;
				break;
			case 3:
				qos_rslt.voq_cos = 4;
				qos_rslt.dscp = 0x0;
				break;
			case 4:
				qos_rslt.voq_cos = 3;
				qos_rslt.dscp = 0x20;
				qos_rslt.change_dscp_en = 1;
				break;
			case 5:
				qos_rslt.voq_cos = 2;
				qos_rslt.dscp = 0x28;
				qos_rslt.change_dscp_en = 1;
				break;
			case 6:
				qos_rslt.voq_cos = 1;
				qos_rslt.dscp = 0x30;
				qos_rslt.change_dscp_en = 1;
				break;
			case 7:
				qos_rslt.voq_cos = 0;
				qos_rslt.dscp = 0x38;
				qos_rslt.change_dscp_en = 1;
				break;
			case 8:
				qos_rslt.voq_cos = 5;
				qos_rslt.dscp = 0x0;
				key.tpid_enc_1 = 0;
				key._8021p_1 = 0;
				break;
		}

		ret = cs_fe_table_add_entry(FE_TABLE_QOSRSLT, &qos_rslt, &qosrslt_idx);
		if (ret != 0) {
			printk("%s cannot add qos rslt ret=%d\n", __func__, ret);
			return ret;
		}
		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);

		if (ret != 0) {
			printk("%s cannot calc crc ret=%d\n", __func__, ret);
			return ret;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, qos_hm_idx,
				qosrslt_idx, &qos_hash_idx);

		if (ret != 0) {
			printk("%s cannot add hash ret=%d\n", __func__, ret);
			return ret;
		}
		printk("%s create hash %x for 1P = %d (hash_mask_idx=%d)\n", __func__, qos_hash_idx, i, qos_hm_idx);

	}
}

int cs_core_logic_init(void)
{
	cs_ne_default_lifetime = CS_DEFAULT_LIFETIME;
	cs_core_vtable_init();
	cs_core_hmu_init();
	cs_accel_cb_init();

	cs_core_rule_hmu_init();

#ifdef CONFIG_CS752X_FASTNET
	cs_core_fastnet_init();
#endif

#ifdef CONFIG_CS752X_HW_ACCELERATION
	cs_core_hw_except_add_rules();
#endif
	create_logical_port_fwd_hash();

	/* BUG#39672: WFO NEC related features (Mutliple BSSID) */
	create_pe_logical_port_fwd_hash();

//	commented out to disable priority changes not in requirements.
//	create_l4_port_fwd_hash(); // might conflict

#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	cs_mcast_init();
#else
	cs_igmp_init();
#endif

	cs_dhcp_init();

#ifdef CONFIG_CS75XX_MTU_CHECK
	cs_port_encap_ip_mtu_add(0, GE_PORT0, CS_ENCAP_IPOE, 1500);
	cs_port_encap_ip_mtu_add(0, GE_PORT0, CS_ENCAP_PPPOE, 1492);
	cs_port_encap_ip_mtu_add(0, GE_PORT0, CS_ENCAP_L2TP, 1452);	
	cs_port_encap_ip_mtu_add(0, GE_PORT0, CS_ENCAP_IPSEC, 1389);
#endif
	gwr_set_mc_1p_dscp_mapping_qos_hash();

	return 0;
} /* cs_core_logic_init */

void cs_core_logic_exit(void)
{
	cs_core_vtable_exit();
	cs_core_hmu_exit();
#ifdef CONFIG_CS752X_FASTNET
	cs_core_fastnet_exit();
#endif
	cs_accel_cb_exit();
} /* cs_core_logic_exit */

