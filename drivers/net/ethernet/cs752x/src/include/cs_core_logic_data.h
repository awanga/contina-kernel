/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <whsu@cortina-systems.com>
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
#ifndef __CS_CORE_LOGIC_DATA_H__
#define __CS_CORE_LOGIC_DATA_H__

#include <mach/cs75xx_fe_core_table.h>
#include <net/xfrm.h>

#define CS_KERNEL_SKB_CB(skb) (cs_kernel_accel_cb_t *) (skb->cs_cb_loc)

#define CS_DEFAULT_VOQ		255
#define CS_DEFAULT_ROOT_VOQ	47
#define CS_DEC_TTL_ENABLE	1
#define CS_RESULT_ACTION_ENABLE	1
#define CS_DEFAULT_LIFETIME	0	/*change hash default lift time to 0 , means Core Logic never timeout a hash entry*/
#define UU_DEF_CPU_VOQ		(CPU_PORT0_VOQ_BASE + 7) /* Reserve 0~6 for
							    Ingress Qos
							  */

#define TCP_CTRL_MASK		0x38

/* Per Flow Control Block Info */
/* module_mask */
#define CS_MOD_MASK_BRIDGE			0x00000001
#define CS_MOD_MASK_PPPOE			0x00000002
#define CS_MOD_MASK_VLAN			0x00000004
#define CS_MOD_MASK_IPV4_ROUTING		0x00000008
#define CS_MOD_MASK_IPV4_PROTOCOL		0x00000010
#define CS_MOD_MASK_NAT				0x00000020
#define CS_MOD_MASK_IPSEC			0x00000040
#define CS_MOD_MASK_WITH_QOS			0x00000080
#define CS_MOD_MASK_IPV4_NETFILTER		0x00000100
#define CS_MOD_MASK_IPV4_MULTICAST		0x00000200
#define CS_MOD_MASK_IPV6_ROUTING		0x00000400
#define CS_MOD_MASK_IPV6_MULTICAST		0x00000800
#define CS_MOD_MASK_IPV6_NETFILTER		0x00001000
#define CS_MOD_MASK_TO_CPU			0x00002000
#define CS_MOD_MASK_NAT_PREROUTE		0x00004000
#define CS_MOD_MASK_TO_VTABLE			0x00008000
#define CS_MOD_MASK_BRIDGE_NETFILTER		0x00010000
#define CS_MOD_MASK_L2_MULTICAST		0x00020000
#define CS_MOD_MASK_FROM_RE0			0x00040000
#define CS_MOD_MASK_FROM_RE1			0x00080000
//#define CS_MOD_MASK_IPSEC_FROM_CPU		0x00100000
#define CS_MOD_MASK_QOS_FIELD_CHANGE		0x00200000
#define CS_MOD_MASK_ETHERIP			0x00400000
#define CS_MOD_MASK_WFO				0x00800000
#define CS_MOD_MASK_WFO_SW_MULTICAST    	0x01000000
#define CS_MOD_MASK_TUNNEL			0x02000000
#define CS_MOD_MASK_DSCP			0x04000000  //Bug#40322
#define CS_MOD_MASK_WIRELESS			0x08000000
#define CS_MOD_MASK_L2TP			0x10000000
#define CS_MOD_MASK_LOCAL_IN			0x20000000

#define CS_MOD_MASK_L3_RELATED		(CS_MOD_MASK_IPV4_ROUTING |\
		CS_MOD_MASK_IPV4_PROTOCOL | CS_MOD_MASK_NAT |\
		CS_MOD_MASK_IPV4_NETFILTER | CS_MOD_MASK_IPV4_MULTICAST |\
		CS_MOD_MASK_IPV6_ROUTING | CS_MOD_MASK_IPV6_MULTICAST |\
		CS_MOD_MASK_IPV6_NETFILTER | CS_MOD_MASK_NAT_PREROUTE)
// L2 rule?

/*
 * FE_VLAN_CMD
 */
 					/* Untagged 		Pri-tagged	Single		Duble		*/
					/****************************************************************/
#define CS_FE_VLAN_CMD_SWAP_A	1	/* NOP			NOP		SWAP		SWAP OUTER	*/
#define CS_FE_VLAN_CMD_SWAP_B	2	/* PUSH			SWAP		SWAP		SWAP OUTER	*/
#define CS_FE_VLAN_CMD_SWAP_D	4	/* NOP			NOP		NOP		SWAP INNER	*/
#define CS_FE_VLAN_CMD_PUSH_B	6	/* PUSH			SWAP		PUSH		NOP		*/
#define CS_FE_VLAN_CMD_PUSH_C	7	/* PUSH			PUSH		PUSH		PUSH		*/
#define CS_FE_VLAN_CMD_POP_A	15	/* NOP			NOP		POP OUTER	POP OUTER	*/
#define CS_FE_VLAN_CMD_POP_B	16	/* NOP			POP		POP OUTER	POP OUTER	*/
#define CS_FE_VLAN_CMD_POP_D	18	/* NOP			NOP		NOP		POP INNER	*/

/*
 * Create Hash Status
 */
#define CS_ACCEL_HASH_DONT_CARE		 1
#define CS_ACCEL_HASH_SUCCESS 		 0
#define CS_ACCEL_HASH_FAIL	 		-1


/* the first 8 bit of swid64 is reserved for module ID, please
 * insert one module ID here if one needs to use it.  Each
 * module will have its own 56 bits for maintaining */
typedef enum {
	CS_SWID64_MOD_ID_IPSEC,
	//CS_SWID64_MOD_ID_MCAST,
	CS_SWID64_MOD_ID_BRIDGE,
	CS_SWID64_MOD_ID_IPV4_FORWARD,
	CS_SWID64_MOD_ID_IPV6_FORWARD,
	//CS_SWID64_MOD_ID_PPPOE,
	CS_SWID64_MOD_ID_FLOW_POLICER,
	CS_SWID64_MOD_ID_NF_DROP,

	CS_SWID64_MOD_MAX, /* only support up to 255 */
} cs_swid64_mod_id_e;

#define CS_SWID64_MASK(mod_id)		((u64)mod_id << 56)
#define CS_SWID64_TO_MOD_ID(aa)		(aa >> 56)
#define CS_INVALID_SWID64		(0xffffffffffffffff)

/* protocol_attack */
#define CS_PROTOATT_ARP			0x00000001
#define CS_PROTOATT_ICMPV4		0x00000002
#define CS_PROTOATT_IPV4_FRAGMENT	0x00000004
#define CS_PROTOATT_ICMPV6		0x00000008
#define CS_PROTOATT_TCP_SYN		0x00000010
#define CS_PROTOATT_TCP_FIN		0x00000020

/* sw_only */
#define CS_SWONLY_HW_ACCEL		0x01	/* Build a HW hash entry */
#define CS_SWONLY_DONTCARE		0x02	/* Don't build HW hash entry */
#define CS_SWONLY_STATE			0x04	/* Don't build HW hash entry */
//#define CS_SWONLY_HW_ACCELL_DROP	0x08	/* What it means is that the
//						   hash needs to be build up but
//						   we do not need to transmit
//						   it. Only the bridge flooding
//						   code will take advantage of
//						   this value, other modules
//						   would not for now. */

#define CS_CB_TAG 	0x4C43 /* "CS" tag */

struct cb_common_field {
	u16	tag;		/* 	CS_CB_TAG */
	u8 	sw_only;
	u8	vtype;		/* vtable type */
	u32	module_mask;	/* NAT, bridge,VLAN */
	u64	swid[CS_SWID64_MOD_MAX];
	u16	swid_cnt;	/* Each module add swid[MODULE_SWID_NUM]
				   need add this counter */
	u8	ingress_port_id;
	u8	egress_port_id;
	struct net_device *input_dev;	/* input virtual ethernet interface */
	struct net_device *output_dev;	/* output virtual ethernet interface */
	u32	sw_action_id;

	u8	protocol_attack;
	u8	dec_ttl;
	u8 	uu_flow_enable;
	u8	state;		/* tcp connection state */
	u8	vpn_dir;	/* 0:outbound; 1:inbound */
	u8	vpn_ip_ver;	/* 0:IPv4, 1:IPv6 */
	u32	in_ifindex;	/* ingress dev->ifindex */
	u32	tunnel_id;	/* VPN tunnel ID */
	u32	session_id;	/* VPN session ID */
	u8	vpn_sa[6];	/* internal MAC used by L2TPv3 */
	u8	vpn_da[6];	/* internal MAC used by L2TPv3 */
};

struct cb_l2_mac {
	u8	sa[6];
	u8	da[6];
	u16	vlan_tci; 	/* priority, dei/cfi, vlan id - host order*/
	u16	vlan_tci_2; 	/* priority, dei/cfi, vlan id - host order*/
	u8	pppoe_frame_vld;
	u16	pppoe_frame;
	u8	ppp_interface;	/* 0:ppp0, 1:ppp1, 2:ppp2, ... */
	u8	l2_flood;
	u16	eth_protocol;	/*ipv4:0x0800, ipv6:0x86dd, pppoe:0x8864 - network order*/
	u16	vlan_tpid;		/*0x8100, 0x9100, 0x9200, 0x88a8 - host order*/
	u16	vlan_tpid_2;
};

struct cb_ip_hdr {
	u8	ver;
	u8	tos;		/* DSCP[7:2] + ECN[1:0]: Type of service */
	u8	protocol;	/* IP protocol, AH,ESP ... */
	u8	frag;		/* Not fragmented: 0; Fragmented: 1 */
	u32	sip;		/* source IP address */
	u32	dip;		/* destination IP address */
};

struct cb_ipv6_hdr {
	/* please make sure ver, tc, and proto are the first declarations in
	 * this given order matched with IPv4 header's definition */
	u8	ver;
	u8	tc;		/* traffic class, the TOS for IPv6 */
	u8	protocol;
	u8	ds_byte;
	u8	flow_lbl[3];
	u32	sip[4];		/* IP address is in network order */
	u32	dip[4];		/* IP address is in network order */
};

struct cb_arp_hdr
{
	u8	ar_sha[6];	/* sender hardware address	*/
	u8	ar_sip[4];	/* sender IP address		*/
	u8	ar_tha[6];	/* target hardware address	*/
	u8	ar_tip[4];	/* target IP address		*/
};

struct cb_tcp_hdr {
	u16	sport;
	u16	dport;
	u16	res1:4,
		doff:4,
		fin:1,
		syn:1,
		rst:1,
		psh:1,
		ack:1,
		urg:1,
		ece:1,
		cwr:1;
};

struct cb_udp_hdr {
	u16	sport;
	u16	dport;
};

struct cb_ip_ah_esp_hdr {
	u32 spi;
};

/* flow_vlan_op_en */
enum cs_vlan_operation {
	CS_VLAN_OP_NO_ENABLE,
	CS_VLAN_OP_INSERT,
	CS_VLAN_OP_REMOVE,
	CS_VLAN_OP_KEEP,
	CS_VLAN_OP_REPLACE,
};

/* flow_vlan_prio_op_en */
enum cs_vlan_prio_operation {
	CS_VLAN_PRIO_OP_NO_ENABLE,
	CS_VLAN_PRIO_OP_UPDATE_PRIO,
	CS_VLAN_PRIO_OP_UPDATE_DEI,
	CS_VLAN_PRIO_OP_UPDATE_PRIO_DEI,
};

/* pppoe_op_en */
enum cs_pppoe_operation {
	CS_PPPOE_OP_NO_ENABLE,
	CS_PPPOE_OP_INSERT,
	CS_PPPOE_OP_REMOVE,
	CS_PPPOE_OP_KEEP,
	CS_PPPOE_OP_REPLACE,
};

typedef struct cb_result_l2_s {
	u8	mac_sa_replace_en;
	u8	mac_da_replace_en;
	u8	flow_vlan_op_en;
	u8	flow_vlan_prio_op_en;
	u8	pppoe_op_en;
} cb_result_l2_t;

typedef struct cb_result_l3_s {
	u8	ip_sa_replace_en;
	u8	ip_da_replace_en;
	u8	decr_ttl_hoplimit;
} cb_result_l3_t;

typedef struct cb_result_l4_s {
	u8	sp_replace_en;
	u8	dp_replace_en;
} cb_result_l4_t;

typedef struct cb_result_dest_s {
	u8	d_voq_id;
	u8	d_pol_id;
	u8	cpu_pid;		/* CPU sub port ID. */
	u8	ldpid;			/* logical destination port that voq
					   maps to */
	u16	pppoe_session_id;	/* pppoe session ID to be encaped */
	u8	cos_nop : 1,
		voq_policy : 1,
		pol_policy : 1,
		voq_policer_parity : 5;
} cb_result_voq_pol_t;

typedef struct cb_result_act_s {
	u16	drop:1,
		mcgid_vaild:1,
		mcgid:14; /* only need 9 bits */
} cb_result_misc_t;

typedef struct cb_result_entry_s {
	cb_result_l2_t	l2;
	cb_result_l3_t	l3;
	cb_result_l4_t	l4;
	cb_result_voq_pol_t	voq_pol;
	cb_result_misc_t misc;
	bool	acl_dsbl;
} cb_hash_result_t;

typedef struct cb_key_misc_s {
	//u8	sdbid;
	u8	lspid;
	u8	mc_da;
	u8	bc_da;
	u16	mcgid;
	u16	mcidx;
	u8	hw_fwd_type;
	u8	orig_lspid;
	u32	recirc_idx;
	u8	mask_ptr_0_7;
	u8	l3_csum_err;
	u8	spi_vld;
	u32	spi_idx;
	u16	pkt_len_low;
	u16	pkt_len_high;
	u16	super_hash;
	u8	super_hash_vld;
	u32	l7_field;
} cb_key_misc_t;

typedef struct cb_result_index_s {
	u16	has_fwdrslt : 1,
		fwdrslt_idx : 15;
#if 0	// Not in use, but might be later on
	u16	has_l2rslt : 1,
		l2rslt_idx : 15;
	u16	has_ip_sa : 1,
		ip_sa_idx : 15;
	u16	has_ip_da : 1,
		ip_da_idx : 15;
	u16	has_voqpol : 1,
		voqpol_idx : 15;
	u16	has_fvlan : 1,
		fvlan_idx : 15;
#endif
	u16	has_qosrslt : 1,
		qosrslt_idx: 15;
} cb_result_index_t;

struct cb_network_field {
	struct cb_l2_mac 	raw;

	union {
		struct cb_ip_hdr	iph;
		struct cb_ipv6_hdr	ipv6h;
		struct cb_arp_hdr	arph;
	} l3_nh;

	union {
		struct cb_tcp_hdr	th;
		struct cb_udp_hdr	uh;
	} l4_h;

	union {
		struct cb_ip_ah_esp_hdr ah_esp;
	} vpn_h;
};

typedef struct cb_fastnet_field_s {
	u8 word3_valid;
	u32 word3;
} cb_fastnet_field_t;


/* input_mask & output_mask */
#define	CS_HM_MAC_DA_MASK 			0x0000000000000001LL
#define CS_HM_MAC_SA_MASK 			0x0000000000000002LL
#define CS_HM_ETHERTYPE_MASK 			0x0000000000000004LL
#define	CS_HM_LLC_TYPE_ENC_MSB_MASK 		0x0000000000000008LL
#define	CS_HM_LLC_TYPE_ENC_LSB_MASK 		0x0000000000000010LL
#define	CS_HM_TPID_ENC_1_MSB_MASK 		0x0000000000000020LL
#define	CS_HM_TPID_ENC_1_LSB_MASK 		0x0000000000000040LL
#define	CS_HM_8021P_1_MASK 			0x0000000000000080LL
#define	CS_HM_DEI_1_MASK 			0x0000000000000100LL
#define	CS_HM_VID_1_MASK 			0x0000000000000200LL
#define	CS_HM_TPID_ENC_2_MSB_MASK 		0x0000000000000400LL
#define	CS_HM_TPID_ENC_2_LSB_MASK 		0x0000000000000800LL
#define	CS_HM_8021P_2_MASK 			0x0000000000001000LL
#define	CS_HM_DEI_2_MASK 			0x0000000000002000LL
#define	CS_HM_VID_2_MASK 			0x0000000000004000LL
#define	CS_HM_IP_DA_MASK 			0x0000000000008000LL
#define	CS_HM_IP_SA_MASK 			0x0000000000010000LL
#define	CS_HM_IP_PROT_MASK 			0x0000000000020000LL
#define	CS_HM_DSCP_MASK 			0x0000000000040000LL
#define	CS_HM_ECN_MASK 				0x0000000000080000LL
#define	CS_HM_IP_FRAGMENT_MASK 			0x0000000000100000LL
#define	CS_HM_KEYGEN_POLY_SEL 			0x0000000000200000LL
#define	CS_HM_IPV6_FLOW_LBL_MASK 		0x0000000000400000LL
#define	CS_HM_IP_VER_MASK 			0x0000000000800000LL
#define	CS_HM_IP_VLD_MASK 			0x0000000001000000LL
#define	CS_HM_L4_PORTS_RNGD 			0x0000000002000000LL
#define	CS_HM_L4_DP_MASK 			0x0000000004000000LL
#define	CS_HM_L4_SP_MASK 			0x0000000008000000LL
#define	CS_HM_TCP_CTRL_MASK 			0x0000000010000000LL
#define	CS_HM_TCP_ECN_MASK 			0x0000000020000000LL
#define	CS_HM_L4_VLD_MASK 			0x0000000040000000LL
#define	CS_HM_LSPID_MASK 			0x0000000080000000LL
#define	CS_HM_FWDTYPE_MASK 			0x0000000100000000LL
#define	CS_HM_PPPOE_SESSION_ID_VLD_MASK 	0x0000000200000000LL
#define	CS_HM_PPPOE_SESSION_ID_MASK 		0x0000000400000000LL
#define	CS_HM_RECIRC_IDX_MASK			0x0000000800000000LL
#define	CS_HM_MCIDX_MASK 			0x0000001000000000LL
#define	CS_HM_MC_DA_MASK 			0x0000002000000000LL
#define	CS_HM_BC_DA_MASK 			0x0000004000000000LL

#define	CS_HM_L7_FIELD_MASK 			0x0000008000000000LL
#define	CS_HM_L7_FIELD_VLD_MASK 		0x0000010000000000LL

#define	CS_HM_ORIG_LSPID_MASK 			0x0000020000000000LL
/* 64-bit mask is full and temporarily mark off unused ones. */

#define	CS_HM_PKTLEN_RNG_MATCH_VECTOR_B1_MASK 	0x0000040000000000LL
#define	CS_HM_PKTLEN_RNG_MATCH_VECTOR_B2_MASK 	0x0000080000000000LL
#define	CS_HM_PKTLEN_RNG_MATCH_VECTOR_B3_MASK 	0x0000100000000000LL

#define	CS_HM_HDR_A_FLAGS_CRCERR_MASK 		0x0000200000000000LL
#define	CS_HM_L3_CHKSUM_ERR_MASK 		0x0000400000000000LL
#define	CS_HM_L4_CHKSUM_ERR_MASK 		0x0000800000000000LL

#define	CS_HM_L7_FIELD_SEL_HEADER_A 		0x0001000000000000LL
#define	CS_HM_L7_FIELD_SEL_TCP_UDP		0x0002000000000000LL
#define	CS_HM_L7_FIELD_SEL_IP			0x0004000000000000LL
#define	CS_HM_L7_FIELD_SEL_ETHERTYPE		0x0008000000000000LL

#define	CS_HM_SPI_VLD_MASK 			0x0010000000000000LL
#define	CS_HM_SPI_MASK 				0x0020000000000000LL
#define	CS_HM_IPV6_NDP_MASK 			0x0040000000000000LL
#define	CS_HM_IPV6_HBH_MASK 			0x0080000000000000LL
#define	CS_HM_IPV6_RH_MASK 			0x0100000000000000LL
#define	CS_HM_IPV6_DOH_MASK 			0x0200000000000000LL
#define	CS_HM_PPP_PROTOCOL_VLD_MASK 		0x0400000000000000LL
#define	CS_HM_PPP_PROTOCOL_MASK 		0x0800000000000000LL
#define	CS_HM_PKTLEN_RNG_MATCH_VECTOR_B0_MASK 	0x1000000000000000LL
#define	CS_HM_MCGID_MASK 			0x2000000000000000LL
#define	CS_HM_VTABLE_MASK 			0x4000000000000000LL //for vtable use only
#define CS_HM_IPV6_MASK				0x8000000000000000LL

#define CS_HM_QOS_TUPLE_MASK	(CS_HM_8021P_1_MASK | CS_HM_DEI_1_MASK | \
				CS_HM_8021P_2_MASK | CS_HM_DEI_2_MASK | \
				CS_HM_DSCP_MASK | CS_HM_ECN_MASK)

/* for cs_cb->input_tcp_flag_mask */
#define CS_HASH_MASK_TCP_URG_FLAG	0x00008000
#define CS_HASH_MASK_TCP_ACK_FLAG	0x00010000
#define CS_HASH_MASK_TCP_PSH_FLAG	0x00020000
#define CS_HASH_MASK_TCP_RST_FLAG	0x00040000
#define CS_HASH_MASK_TCP_SYN_FLAG	0x00080000
#define CS_HASH_MASK_TCP_FIN_FLAG	0x00100000

typedef struct {
	struct cb_common_field common;
	struct cb_network_field input;
	u64 output_mask;
	struct cb_network_field output;
	cb_key_misc_t key_misc;
	cb_hash_result_t action;
	cb_result_index_t hw_rslt;
	cb_fastnet_field_t fastnet;
	unsigned long lifetime;	/* in jiffies */
	u8 fill_ouput_done;
} cs_kernel_accel_cb_t;


/* Hash Data Structure */

/* when adding hash with only fwd_hash_entry or qos_hash_entry, it will
 * be created by itself individually w/o being linking together.
 * However, if adding fwd_hash_entry AND qos_hash_entry, they are linked
 * together.  When one is deleted, the other will be forced to be delete */
typedef struct {
	fe_sw_hash_t key;
	unsigned int fwd_app_type;
	fe_fwd_result_entry_t action;
	fe_fwd_result_param_t param;
	//u64	swid[CS_SWID64_MOD_MAX];
	//u8	swid_cnt;	/* Each module add swid[MODULE_SWID_NUM]
	//			   need add this counter */
	unsigned int lifetime;	/* in second, 0 means forever */
} cs_fwd_hash_t;

typedef struct {
	fe_sw_hash_t key;
	unsigned int qos_app_type;
	fe_qos_result_entry_t action;
	u16	rslt_idx;
	//u64	swid[CS_SWID64_MOD_MAX];
	//u8	swid_cnt;	/* Each module add swid[MODULE_SWID_NUM]
	//			   need add this counter */
} cs_qos_hash_t;


#endif	/* __CS_CORE_LOGIC_DATA_H__ */
