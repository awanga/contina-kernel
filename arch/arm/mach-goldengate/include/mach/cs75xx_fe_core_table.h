/*
 *
 * Copyright (c) 2010 Cortina Systems, Inc.
 * All Rights Reserved.
 *
 */

#ifndef __CS_FE_CORE_TABLE_H__
#define __CS_FE_CORE_TABLE_H__
//#include <linux/spinlock.h>
//#include <asm/atomic.h>
#include <linux/types.h>

/* Virtual Table */
#define CS_L2_RULE_BASE		0
#define CS_L2_FLOW_BASE		1
#define CS_L3_RULE_BASE		2
#define CS_L4_FLOW_BASE		3

#define	GE_PORT0			0
#define	GE_PORT1			1
#define	GE_PORT2			2
#define CPU_PORT			3
#define ENCRYPTION_PORT		4
#define PE0_PORT		    4
#define ENCAPSULATION_PORT	5
#define PE1_PORT	        5
#define MCAST_PORT			6
#define MIRROR_PORT			7
#define SW_WIRELESS_PORT	8

#define	GE_PORT0_VOQ_BASE	0
#define	GE_PORT1_VOQ_BASE	8
#define	GE_PORT2_VOQ_BASE	16
#define	ENCRYPTION_VOQ_BASE	24
#define	ENCAPSULATION_VOQ_BASE	32
#define	ROOT_PORT_VOQ_BASE	40
#define	CPU_PORT0_VOQ_BASE	48
#define	CPU_PORT1_VOQ_BASE	56
#define	CPU_PORT2_VOQ_BASE 	64
#define	CPU_PORT3_VOQ_BASE	72
#define	CPU_PORT4_VOQ_BASE	80
#define	CPU_PORT5_VOQ_BASE 	88
#define	CPU_PORT6_VOQ_BASE	96
#define	CPU_PORT7_VOQ_BASE 	104

#ifdef CONFIG_CS75XX_WFO
#define	WFO_GE2PE_P0_OFFSET  7
#define	WFO_GE2PE_P1_OFFSET  6
#define	WFO_GE2PE_P2_OFFSET  5
#define	WFO_GE2PE_P3_OFFSET  4
#define	WFO_GE2PE_P4_OFFSET  3
#define	WFO_GE2PE_P5_OFFSET  2
#define	WFO_GE2PE_P6_OFFSET  1
#endif

/* Logic source port id */
#define CS_FE_LSPID_0		0
#define CS_FE_LSPID_1		1
#define CS_FE_LSPID_2		2
#define CS_FE_LSPID_3		3
#define CS_FE_LSPID_4		4
#define CS_FE_LSPID_5		5
#define CS_FE_LSPID_6		6
#define CS_FE_LSPID_7		7
#define CS_FE_LSPID_8		8
#define CS_FE_LSPID_9		9
#define CS_FE_LSPID_10		10
#define CS_FE_LSPID_11		11
#define CS_FE_LSPID_12		12
#define CS_FE_LSPID_13		13
#define CS_FE_LSPID_14		14
#define CS_FE_LSPID_15		15

/* Forward Type */
#define CS_FWD_NORMAL	0
#define CS_FWD_CPU		1
#define CS_FWD_MC		4
#define CS_FWD_BC		5
#define CS_FWD_UM		6
#define CS_FWD_UU		7
#define CS_FWD_MIRROR	8
#define CS_FWD_BYPASS	12
#define CS_FWD_DROP		13

#define CS_NOT_DROP		0
#define CS_ACL_DISABLE	0
#define CS_DEC_TTL		0
#define CS_VOQ_POL_DEFAULT		0
#define CS_MASK_PTR_DEFAULT		0xF


/*
 *[ingress qos]--add by ethan for ingress qos
 * Ingress QOS Table Entry
 *
 */
typedef struct fe_ingress_qos_table_entry_s {

	__u8 mask;
	__u16	vid;
	__u8	_8021p;
	__u8	dscp;
	__u32 sip[4];
	__u32 dip[4];
	__u8	voq;
	__u8  lspid;
	__u8  del;
} fe_ingress_qos_table_entry_t;

/*
 *[ingress qos]--add by ethan for ingress qos
 * Ingress QOS SHAPER Voq Table Entry
 *
 */
typedef struct fe_ingress_qos_shaper_voq_entry_s {

	__u8	voq_id;
	__u8	sp_drr; //0:SP or 1:DRR
	__u16	rate;   //0: default value, if user didn't set up this field.
} fe_ingress_qos_shaper_voq_entry_t;

/*
 *[ingress qos]--add by ethan for ingress qos
 * Ingress QOS SHAPER Port Table Entry
 *
 */
typedef struct fe_ingress_qos_shaper_port_entry_s {

	__u8	port_id;
	__u16	st_rate;
	__u16	lt_rate;
	__u16	burst_size;
} fe_ingress_qos_shaper_port_entry_t;


/*
 * FE AN/BNG MAC Addr Table Entry
 * Total 15 entreis.
 *
 */
typedef struct fe_an_bng_mac_entry_s {
	__u8	mac[6];
	__u8	sa_da;	/* this entry MAC is SA or DA */
	__u8	pspid;	/* physical source port ID */
	__u8	pspid_mask;	/* mask of physical source port ID */
	__u8	valid;	/* entry valid bit */
} fe_an_bng_mac_entry_t;
/* FE_AN_BNG_MAC_ACCESS is defined in register.h */

/*
 * Port Range Table Entry
 * Total 32 entries. First 16 for L4 SP, next 16 for L4 DP.
 * Incoming packet's SP/DP are compared against ranges programmed in the CAM.
 *
 */
typedef struct fe_port_range_entry_s {
	__u16	sp_dp_high;
	__u16	sp_dp_low;
	__u8	valid;
} fe_port_range_entry_t;


/*
 * Ethertype Indirect CAM Table Entry
 * Total 63 entries.
 *
 */
typedef struct fe_eth_type_entry_s {
	__u16	ether_type;
	__u8	valid;
} fe_eth_type_entry_t;


/*
 * PE VOQ Drop Entry
 * Total 32 * 4 = 128 Entries.
 */
typedef struct fe_pe_voq_drp_entry_s {
	__u16	voq_id;
	__u8	f_drop_enbl;
} fe_pe_voq_drp_entry_t;


/*
 * LLC Header Indirect CAM Table Entry
 * Total 3 entries.
 *
 */
typedef struct fe_llc_hdr_entry_s {
	__u32	llc_hdr;
	__u8	valid;
} fe_llc_hdr_entry_t;


/*
 * LPB (Logical Port Behavior) Table Entry
 *
 * Total 8 entries. Accessed via PSPID of header A from parser.
 * Each entry associates with one physical port.
 *
 */
typedef struct fe_lpb_entry_s {
	__u8	lspid;
	__u16	pvid;
	__u8	pvid_tpid_enc;
	__u8	olspid_en;
	__u8	olspid;
	__u8	olspid_preserve_en;
	__u8	parity;
} fe_lpb_entry_t;


/*
 * VLAN Table Entry
 * Total 4096 entries, accessed with topmost vlan of the ingress packet
 * (PVID if untagged), and topmost vlan of the egress packet.
 *
 */
typedef struct fe_vlan_entry_s {
	/* 6 bits. port membership bit vector */
	__u8	vlan_member;

	__u8	vlan_egress_untagged;
	__u8	vlan_fid;
	__u8	vlan_first_vlan_cmd;
	__u16	vlan_first_vid;
	__u8	vlan_first_tpid_enc;

	__u8	vlan_second_vlan_cmd;
	__u16	vlan_second_vid;
	__u8	vlan_second_tpid_enc;
	__u16	vlan_mcgid;
	__u8	parity;
} fe_vlan_entry_t;


/*
 * Classifier Table Entry
 * Total 64 entries.
 *
 */

typedef struct fe_class_entry_port_s {
	__u8	lspid;
	__u8	hdr_a_orig_lspid;
	__u8	fwd_type;
	__u8	hdr_a_flags_crcerr;
	__u8	l3_csum_err;
	__u8	l4_csum_err;
	__u8	not_hdr_a_flags_stsvld;

	__u8	lspid_mask;
	__u8	hdr_a_orig_lspid_mask;
	__u8	fwd_type_mask;
	__u8	hdr_a_flags_crcerr_mask;
	__u8	l3_csum_err_mask;
	__u8	l4_csum_err_mask;
	__u8	not_hdr_a_flags_stsvld_mask;
	__u16	mcgid;
	__u16	mcgid_mask;
} fe_class_entry_port_t;

typedef struct fe_class_entry_l2_s {
	__u8	tpid_enc_1;
	__u16	vid_1;
	__u8	_8021p_1;
	__u8	tpid_enc_2;
	__u16	vid_2;
	__u8	_8021p_2;
	__u8	tpid_enc_1_msb_mask;
	__u8	tpid_enc_1_lsb_mask;
	__u8	vid_1_mask;
	__u8	_8021p_1_mask;
	__u8	tpid_enc_2_msb_mask;
	__u8	tpid_enc_2_lsb_mask;
	__u8	vid_2_mask;
	__u8	_8021p_2_mask;

	__u8	da_an_mac_sel;
	__u8	da_an_mac_hit;
	__u8	sa_bng_mac_sel;
	__u8	sa_bng_mac_hit;
	__u8	da_an_mac_sel_mask;
	__u8	da_an_mac_hit_mask;
	__u8	sa_bng_mac_sel_mask;
	__u8	sa_bng_mac_hit_mask;

	__u8	ethertype_enc;
	__u8	ethertype_enc_mask;

	__u8	da[6];
	__u8	sa[6];
	__u8	da_mask;	/* 6 bits */
	__u8	sa_mask;	/* 6 bits */

	__u8	mcast_da;
	__u8	bcast_da;
	__u8	mcast_da_mask;
	__u8	bcast_da_mask;

	__u8	len_encoded;
	__u8	len_encoded_mask;
} fe_class_entry_l2_t;

typedef struct fe_class_entry_l3_s {
	__u8	dscp;
	__u8	ecn;
	__u8	ip_prot;

	__u32	sa[4];
	__u32	da[4];

	__u8	ip_valid;
	__u8	ip_ver;
	__u8	ip_frag;

	__u8	dscp_mask;
	__u8	ecn_mask;
	__u8	ip_prot_mask;

	__u16	ip_sa_mask;		/* 9 bits */
	__u16	ip_da_mask;		/* 9 bits */

	__u8	ip_valid_mask;
	__u8	ip_ver_mask;
	__u8	ip_frag_mask;

	__u32	spi;
	__u8	spi_valid;
	__u8	spi_mask;
	__u8	spi_valid_mask;
} fe_class_entry_l3_t;

typedef struct fe_class_entry_l4_s {
	__u16	l4_sp;
	__u16	l4_dp;
	__u8	l4_valid;

	__u8	l4_port_mask;
	__u8	l4_valid_mask;
} fe_class_entry_l4_t;

typedef struct fe_class_entry_s {
	__u8	sdb_idx;
	__u8	rule_priority;
	__u8	entry_valid;
	__u8	parity;
	fe_class_entry_port_t	port;
	fe_class_entry_l2_t		l2;
	fe_class_entry_l3_t		l3;
	fe_class_entry_l4_t		l4;
} fe_class_entry_t;


/*
 * SDB Table Entry
 * Total 64 entries.
 *
 */

typedef struct fe_sdb_tuple_entry_s {
	__u8		mask_ptr;
	__u8		priority;
	__u8		enable;
} fe_sdb_tuple_entry_t;

typedef struct fe_sdb_lpm_entry_s {
	__u8		start_ptr;
	__u8		end_ptr;
	__u8		lpm_ptr_en;
} fe_sdb_lpm_entry_t;

typedef struct fe_sdb_pvid_field_s {
	__u16		pvid;
	__u8		pvid_tpid_enc;
	__u8		pvid_en;
} fe_sdb_pvid_field_t;

typedef struct fe_sdb_vlan_entry_s {
	__u8		vlan_ingr_membership_en;
	__u8		vlan_egr_membership_en;
	__u8		vlan_egr_untag_chk_en;
} fe_sdb_vlan_entry_t;

typedef struct fe_sdb_entry_misc_s {
	__u8		use_egrlen_pkttype_policer;
	__u8		use_egrlen_src_policer;
	__u8		use_egrlen_flow_policer;
	__u8		ttl_hop_limit_zero_discard_en;
	__u8		key_rule;
	__u16		uu_flowidx;
	__u8		hash_sts_update_ctrl;
	__u16		bc_flowidx;
	__u16		um_flowidx;
	__u8		rsvd_202;
	__u8		drop;
	__u8		egr_vln_ingr_mbrshp_en;
	__u8		acl_dsbl;
} fe_sdb_entry_misc_t;

typedef struct fe_sdb_entry_s {
	fe_sdb_tuple_entry_t	sdb_tuple[8];
	__u8					lpm_en;
	fe_sdb_lpm_entry_t		sdb_lpm_v4[2];
	fe_sdb_lpm_entry_t		sdb_lpm_v6[2];
	fe_sdb_pvid_field_t		pvid;
	fe_sdb_vlan_entry_t		vlan;
	fe_sdb_entry_misc_t		misc;
	__u8					parity;
} fe_sdb_entry_t;

/*
 * Hash Mask Table Entry
 * Total 64 entries.
 */
typedef struct fe_hash_mask_entry_s {
	__u8		mac_da_mask;
	__u8		mac_sa_mask;
	__u8		ethertype_mask;
	__u8		llc_type_enc_msb_mask;
	__u8		llc_type_enc_lsb_mask;
	__u8		tpid_enc_1_msb_mask;
	__u8		tpid_enc_1_lsb_mask;
	__u8		_8021p_1_mask;
	__u8		dei_1_mask;
	__u8		vid_1_mask;
	__u8		tpid_enc_2_msb_mask;
	__u8		tpid_enc_2_lsb_mask;
	__u8		_8021p_2_mask;
	__u8		dei_2_mask;
	__u8		vid_2_mask;
	__u16		ip_da_mask;
	__u16		ip_sa_mask;
	__u8		ip_prot_mask;
	__u8		dscp_mask;
	__u8		ecn_mask;
	__u8		ip_fragment_mask;
	__u8		keygen_poly_sel;
	__u8		ipv6_flow_lbl_mask;
	__u8		ip_ver_mask;
	__u8		ip_vld_mask;
	__u8		l4_ports_rngd;
	__u16		l4_dp_mask;
	__u16		l4_sp_mask;
	__u8		tcp_ctrl_mask;
	__u8		tcp_ecn_mask;
	__u8		l4_vld_mask;
	__u8		lspid_mask;
	__u8		fwdtype_mask;
	__u8		pppoe_session_id_vld_mask;
	__u8		pppoe_session_id_mask;
	__u8		rsvd_109;
	__u8		recirc_idx_mask;
	__u8		mcidx_mask;
	__u8		mc_da_mask;
	__u8		bc_da_mask;
	__u8		da_an_mac_sel_mask;
	__u8		da_an_mac_hit_mask;
	__u8		orig_lspid_mask;
	__u8		l7_field_mask;
	__u8		l7_field_vld_mask;
	__u8		hdr_a_flags_crcerr_mask;
	__u8		l3_chksum_err_mask;
	__u8		l4_chksum_err_mask;
	__u8		not_hdr_a_flags_stsvld_mask;
	__u8		hash_fid_mask;
	__u8		l7_field_sel;
	__u8		sa_bng_mac_sel_mask;
	__u8		sa_bng_mac_hit_mask;
	__u8		spi_vld_mask;
	__u8		spi_mask;
	__u8		ipv6_ndp_mask;
	__u8		ipv6_hbh_mask;
	__u8		ipv6_rh_mask;
	__u8		ipv6_doh_mask;
	__u8		ppp_protocol_vld_mask;
	__u8		ppp_protocol_mask;
	__u8		pktlen_rng_match_vector_mask;
	__u16		mcgid_mask;
	__u8		parity;
} fe_hash_mask_entry_t;


/*
 * Hash Match Table Entry
 * Total 24k entries.
 * Hash result, Result table index, mask ptr, etc.
 */

typedef struct fe_hash_hash_entry_s {
	__u32	crc32_0;
	__u32	crc32_1;
	__u16	result_index0;	/* index to result table */
	__u16	result_index1;
	__u8	mask_ptr0;
	__u8	mask_ptr1;
	/* values below are maintained by sw only */
	__u8	entry0_valid;
	__u8	entry1_valid;
	__u16	crc16_0;	/* used for double check */
	__u16	crc16_1;	/* used for double check */
} fe_hash_hash_entry_t;

/*
 * Hash Super Hash Entry
 * 1 entry only.
 */

typedef struct fe_hash_super_hash {
	__u8	mask_ptr;
	__u8	enable;
} fe_hash_super_hash_s;


/* Hash Match Table Overflow Entry */
typedef struct fe_hash_overflow_entry_s {
	__u32	crc32;
	__u16	crc16;
	__u16	result_index;
	__u8	mask_ptr;
} fe_hash_overflow_entry_t;

/* Hash Match Table Status Entry */
typedef struct fe_hash_status_entry_s {
	__u64	data;
} fe_hash_status_entry_t;

/*
 * Main Forwarding Results Table
 * Total 8k entries.
 *
 */

typedef struct fe_fwd_result_l2_s {
	__u8	mac_sa_replace_en;
	__u8	mac_da_replace_en;
	__u16	l2_index;		/* index to MAC table */
	__u16	mcgid;
	__u8	mcgid_valid;

	__u8	flow_vlan_op_en;
	__u16	flow_vlan_index;	/* index to flow vlan table entry */

	__u8	pppoe_encap_en;
	__u8	pppoe_decap_en;

} fe_fwd_result_l2_t;

typedef struct fe_fwd_result_l3_s {
	__u8	ip_sa_replace_en;
	__u8	ip_da_replace_en;

	__u16	ip_sa_index;
	__u16	ip_da_index;

	__u8	decr_ttl_hoplimit;

} fe_fwd_result_l3_t;

typedef struct fe_fwd_result_l4_s {
	__u8	sp_replace_en;
	__u8	dp_replace_en;
	__u16	sp;
	__u16	dp;
} fe_fwd_result_l4_t;

typedef struct fe_fwd_result_dest_s {
	__u8	pol_policy;
	__u8	voq_policy;
	__u16	voq_pol_table_index;
} fe_fwd_result_dest_t;

typedef struct fe_fwd_result_act_s {
	__u8	fwd_type_valid;
	__u8	fwd_type;
	__u8	drop;
} fe_fwd_result_act_t;

typedef struct fe_fwd_result_entry_s {
	fe_fwd_result_l2_t	l2;
	fe_fwd_result_l3_t	l3;
	fe_fwd_result_l4_t	l4;
	fe_fwd_result_dest_t	dest;
	fe_fwd_result_act_t		act;
	__u8	acl_dsbl;
	__u8	parity;
} fe_fwd_result_entry_t;

typedef struct fe_fwd_result_param_s {
	__u8	mac[12];
	__u8	first_vlan_cmd;
	__u16	first_vid;
	__u8	first_tpid_enc;
	__u8	second_vlan_cmd;
	__u16	second_vid;
	__u8	second_tpid_enc;
	bool	is_v6;
	__u32	src_ip[4];
	__u32	dst_ip[4];
	__u16	voq_id;
	__u16	pol_id;
	__u16	pppoe_session_id;	/* pppoe session ID to be encaped */
	bool	has_sw_action_id;
	__u32	sw_action_id;
	__u16	result_index;
} fe_fwd_result_param_t;


/*
 * QoS Result Eable Entry
 * Total 128 entries.
 *
 */
typedef struct fe_qos_result_entry_s {
	__u8	wred_cos;
	__u8	voq_cos;
	__u8	pol_cos;
	__u8	premark;
	__u8	change_dscp_en;
	__u8	dscp;
	__u8	dscp_markdown_en;
	__u8	marked_down_dscp;
	__u8	ecn_en;
	__u8	top_802_1p;
	__u8	marked_down_top_802_1p;
	__u8	top_8021p_markdown_en;
	__u8	top_dei;
	__u8	marked_down_top_dei;
	__u8	inner_802_1p;
	__u8	marked_down_inner_802_1p;
	__u8	inner_8021p_markdown_en;
	__u8	inner_dei;
	__u8	marked_down_inner_dei;
	__u8	change_8021p_1_en;
	__u8	change_dei_1_en;
	__u8	change_8021p_2_en;
	__u8	change_dei_2_en;
	__u8	parity;
} fe_qos_result_entry_t;



/*
 * VoQ & Policer Table Entry
 * Total 512 entries.
 */

typedef struct fe_voq_pol_entry_s {
	__u8	voq_base;		/* base destination voq of the packet */
	__u8	pol_base;		/* base destinaiton policer id of the packet */
	__u8	cpu_pid;		/* CPU sub port ID. */
	__u8	ldpid;			/* logical destination port that voq maps to */
	__u16	pppoe_session_id;	/* pppoe session ID to be encaped */
	__u8	cos_nop;
	__u8	parity;
} fe_voq_pol_entry_t;


/*
 * Flow VLAN Table Entry
 * Total 512 entries.
 *
 */
typedef struct fe_flow_vlan_entry_s {
	__u8	first_vlan_cmd;
	__u16	first_vid;
	__u8	first_tpid_enc;
	__u8	second_vlan_cmd;
	__u16	second_vid;
	__u8	second_tpid_enc;
	__u8	parity;
} fe_flow_vlan_entry_t;


/*
 * L2 Address Table Entry
 * Total 512 entries.
 */

typedef struct fe_l2_addr_pair_entry_s {
	__u8	mac_sa[6];
	__u16	sa_count;
	__u8	mac_da[6];
	__u16	da_count;
} fe_l2_addr_pair_entry_t;


/*
 * L3 Address Table Entry
 * Total 1k entries
 */

typedef struct fe_l3_addr_entry_s {
	__u32	ip_addr[4];
	__u16	count[4];
} fe_l3_addr_entry_t;


/*
 * ACL Rule Table Entry
 * Total
 *
 */
typedef struct fe_acl_rule_l2_entry_s {
	__u8	l2_mac_da[6];
	__u8	l2_mac_sa[6];
	__u16	eth_type;
	__u8	len_encoded;

	__u8	tpid_1_vld;
	__u8	tpid_enc_1;
	__u16	vid_1;
	__u8	_8021p_1;
	__u8	dei_1;

	__u8	tpid_2_vld;
	__u8	tpid_enc_2;
	__u16	vid_2;
	__u8	_8021p_2;
	__u8	dei_2;

	__u8	l2_mac_da_mask;
	__u8	l2_mac_sa_mask;
	__u8	ethertype_mask;
	__u8	len_encoded_mask;

	__u8	tpid_1_vld_mask;
	__u8	tpid_enc_1_mask;
	__u8	vid_1_mask;
	__u8	_8021p_1_mask;
	__u8	dei_1_mask;
	__u8	tpid_2_vld_mask;
	__u8	tpid_enc_2_mask;
	__u8	vid_2_mask;
	__u8	_8021p_2_mask;
	__u8	dei_2_mask;

	__u8	da_an_mac_sel;
	__u8	da_an_mac_sel_mask;
	__u8	da_an_mac_hit;
	__u8	da_an_mac_hit_mask;
	__u8	sa_bng_mac_sel;
	__u8	sa_bng_mac_sel_mask;
	__u8	sa_bng_mac_hit;
	__u8	sa_bng_mac_hit_mask;

	__u8	pppoe_session_id_vld;
	__u8	pppoe_session_id_vld_mask;
	__u16	pppoe_session_id;
	__u8	pppoe_session_id_mask;

	__u8	ppp_protocol_vld;
	__u8	ppp_protocol_vld_mask;
	__u16	ppp_protocol;
	__u8	ppp_protocol_mask;
} fe_acl_rule_l2_entry_t;

typedef struct fe_acl_rule_l3_entry_s {
	__u8	ip_vld;
	__u8	ip_ver;
	__u32	da[4];
	__u32	sa[4];
	__u8	dscp;
	__u8	ecn;
	__u8	proto;
	__u8	fragment;
	__u8	options;
	__u32	ipv6_flow_label;
	__u8	ttl_hoplimit;

	__u8	ip_vld_mask;
	__u8	ip_ver_mask;
	__u16	ip_da_mask;
	__u16	ip_sa_mask;
	__u8	dscp_mask;
	__u8	ecn_mask;
	__u8	ip_proto_mask;
	__u8	ip_fragment_mask;
	__u8	ip_options_mask;
	__u8	ipv6_flow_label_mask;
	__u8	ttl_hoplimit_mask;

	__u32	spi;
	__u8	spi_mask;
	__u8	spi_vld;
	__u8	spi_vld_mask;

	__u8	ipv6_ndp;
	__u8	ipv6_ndp_mask;
	__u8	ipv6_hbh;
	__u8	ipv6_hbh_mask;
	__u8	ipv6_rh;
	__u8	ipv6_rh_mask;
	__u8	ipv6_doh;
	__u8	ipv6_doh_mask;

} fe_acl_rule_l3_entry_t;

#define FE_ACL_RULE_L4_MASK_DP_LO	0x0001
#define FE_ACL_RULE_L4_MASK_DP_HI	0x0002
#define FE_ACL_RULE_L4_MASK_SP_LO	0x0004
#define FE_ACL_RULE_L4_MASK_SP_HI	0x0008

typedef struct fe_acl_rule_l4_entry_s {
	__u8	l4_valid;
	__u16	dp_lo;
	__u16	dp_hi;
	__u16	sp_lo;
	__u16	sp_hi;

	__u8	l4_valid_mask;
	__u8	l4_mask;

} fe_acl_rule_l4_entry_t;

#define FE_ACL_RULE_PARSER_L700		0x0000	// 4 bytes starting MAC DA
#define FE_ACL_RULE_PARSER_L701		0x0001	// 4 bytes after TCP/UDP hdr
#define FE_ACL_RULE_PARSER_L702		0x0002	// 4 bytes after IP header
#define FE_ACL_RULE_PARSER_L703		0x0003	// 4 bytes after Ether header
#define FE_ACL_RULE_PARSER_L707		0x0007	// Masked or field not in use.

#define FE_ACL_RULE_FLAGS_BC		0x0000
#define FE_ACL_RULE_FLAGS_MC		0x0001
#define FE_ACL_RULE_FLAGS_UNOKNOWN_OP	0x0002
#define FE_ACL_RULE_FLAGS_OAM		0x0004
#define FE_ACL_RULE_FLAGS_PAUSE		0x0008
#define FE_ACL_RULE_FLAGS_CRC		0x0010
#define FE_ACL_RULE_FLAGS_OVERSIZE	0x0020
#define FE_ACL_RULE_FLAGS_RUNT		0x0040
#define FE_ACL_RULE_FLAGS_LINK_STATUS	0x0080
#define FE_ACL_RULE_FLAGS_JUMBO_FRAME	0x0100
#define FE_ACL_RULE_FLAGS_INVLD_CTRL	0x0200
#define FE_ACL_RULE_FLAGS_STATUS_VLD	0x0400
#define FE_ACL_RULE_FLAGS_DROP_ON_ERR	0x0800
#define FE_ACL_RULE_FLAGS_PTP_FROM_HDR	0x1000
#define FE_ACL_RULE_FLAGS_L3_CSUM_ERR	0x2000
#define FE_ACL_RULE_FLAGS_L4_CSUM_ERR	0x4000

/* Not Equal Comparison */
#define FE_ACL_RULE_NE_MAC_DA_ENABLE	0x0001
#define FE_ACL_RULE_NE_MAC_SA_ENABLE	0x0002
#define FE_ACL_RULE_NE_IP_DA_ENABLE		0x0004
#define FE_ACL_RULE_NE_IP_SA_ENABLE		0x0008
#define FE_ACL_RULE_NE_L4_DP_ENABLE		0x0010
#define FE_ACL_RULE_NE_L4_SP_ENABLE		0x0020
#define FE_ACL_RULE_NE_IP_PROT_ENABLE	0x0040
#define FE_ACL_RULE_NE_LSPID_ENABLE		0x0080
#define FE_ACL_RULE_NE_ORIG_LSPID_ENABLE	0x0100
#define FE_ACL_RULE_NE_L7_FIELD_ENABLE	0x0200
#define FE_ACL_RULE_NE_VID_1_ENABLE		0x0400
#define FE_ACL_RULE_NE_VID_2_ENABLE		0x0800
#define FE_ACL_RULE_NE_ETHTYPE_ENABLE	0x1000
#define FE_ACL_RULE_NE_CLASS_SVIDX_ENABLE	0x2000
#define FE_ACL_RULE_NE_RECIRC_IDX_ENABLE	0x4000
#define FE_ACL_RULE_NE_TTL_HOP_LMT_ENABLE	0x8000


typedef struct fe_acl_rule_misc_entry_s {
	__u8	lspid;
	__u8	orig_lspid;
	__u8	fwd_type;
	__u16	spl_pkt_vec;
	__u8	class_hit;
	__u8	class_svidx;
	__u8	lpm_hit;
	__u8	lpm_hit_idx;
	__u8	hash_hit;
	__u16	hash_hit_idx;
	__u32	l7_field;

	__u8	lspid_mask;
	__u8	orig_lspid_mask;
	__u8	fwd_type_mask;
	__u16	spl_pkt_vec_mask;
	__u8	class_hit_mask;
	__u8	class_svidx_mask;
	__u8	lpm_hit_mask;
	__u8	lpm_hit_idx_mask;
	__u8	hash_hit_mask;
	__u8	hash_hit_idx_mask;
	__u8	l7_field_mask;

	__u16	flags_vec;
	__u16	flags_vec_mask;
	__u8	flags_vec_or;

	__u8	spl_pkt_vec_or;

	__u16	recirc_idx;
	__u8	recirc_idx_mask;

	__u16	ne_vec;
	__u8	mc_idx;
	__u8	mc_idx_mask;
	__u8	sdb_drop;
	__u8	sdb_drop_mask;
	__u8	fwd_drop;
	__u8	fwd_drop_mask;

	__u8	pktlen_rng_vec;
	__u8	pktlen_rng_vec_mask;

	__u16	rsvd_879_878;
} fe_acl_rule_misc_entry_t;

typedef struct fe_acl_rule_entry_s {
	__u8	rule_valid;
	fe_acl_rule_l2_entry_t		l2;
	fe_acl_rule_l3_entry_t		l3;
	fe_acl_rule_l4_entry_t		l4;
	fe_acl_rule_misc_entry_t	misc;
	__u8	parity;
} fe_acl_rule_entry_t;

typedef struct fe_acl_action_l2_entry_s {
	__u8	_8021p_1_vld;
	__u8	_8021p_1;
	__u8	_8021p_1_pri;
	__u8	dei_1_vld;
	__u8	dei_1;
	__u8	dei_1_pri;
	__u8	_8021p_2_vld;
	__u8	_8021p_2;
	__u8	_8021p_2_pri;
	__u8	dei_2_vld;
	__u8	dei_2;
	__u8	dei_2_pri;
	__u8	first_vlan_cmd_vld;
	__u8	first_vlan_cmd;
	__u16	first_vid;
	__u8	first_tpid_enc;
	__u8	first_vlan_cmd_pri;
	__u8	second_vlan_cmd_vld;
	__u8	second_vlan_cmd;
	__u16	second_vid;
	__u8	second_tpid_enc;
	__u8	second_vlan_cmd_pri;
	__u8	mac_da_sa_replace_en_vld;
	__u8	mac_da_replace_en;
	__u8	mac_sa_replace_en;
	__u8	mac_da_sa_replace_en_pri;
	__u16	l2_idx;
	__u8	change_8021p_1_en;
	__u8	change_dei_1_en;
	__u8	change_8021p_2_en;
	__u8	change_dei_2_en;
} fe_acl_action_l2_entry_t;

typedef struct fe_acl_action_l3_entry_s {
	__u8	dscp_vld;
	__u8	dscp;
	__u8	dscp_pri;
	__u8	ip_sa_replace_en_vld;
	__u8	ip_sa_replace_en;
	__u8	ip_sa_replace_en_pri;
	__u8	ip_da_replace_en_vld;
	__u8	ip_da_replace_en;
	__u8	ip_da_replace_en_pri;
	__u16	ip_sa_idx;
	__u16	ip_da_idx;
	__u8	change_dscp_en;
	__u8	decr_ttl_hoplimit_vld;
	__u8	decr_ttl_hoplimit;
	__u8	decr_ttl_hoplimit_pri;
} fe_acl_action_l3_entry_t;

typedef struct fe_acl_action_l4_entry_s {
	__u8	sp_replace_en_vld;
	__u8	sp_replace_en;
	__u16	sp;
	__u8	sp_replace_en_pri;
	__u8	dp_replace_en_vld;
	__u8	dp_replace_en;
	__u16	dp;
	__u8	dp_replace_en_pri;
} fe_acl_action_l4_entry_t;

typedef struct fe_acl_action_misc_entry_s {
	__u8	voq_vld;
	__u8	voq_pri;
	__u8	voq;
	__u8	ldpid;	/* logical destination port id */
	__u8	cpucopy;
	__u8	cpucopy_voq;
	__u8	cpucopy_pri;
	__u8	mirror_vld;
	__u16	mirror_id;
	__u8	mirror_id_pri;
	__u8	wred_cos_vld;
	__u8	wred_cos;
	__u8	wred_cos_pri;
	__u8	pre_mark_vld;
	__u8	pre_mark;
	__u8	pre_mark_pri;
	__u8	policer_id_vld;
	__u8	policer_id;
	__u8	policer_id_pri;
	__u8	drop_permit_vld;
	__u8	drop;
	__u8	permit;
	__u8	drop_permit_pri;
	__u8	fwdtype_vld;
	__u8	fwdtype;
	__u8	fwdtype_pri;
	__u8	mcgid_vld;
	__u16	mcgid;
	__u8	mcdid_pri;
	__u8	keep_ts_vld;
	__u8	keep_ts;
	__u8	keep_ts_pri;
	__u8	voq_cpupid;
	__u8	cpucopy_cpupid;
} fe_acl_action_misc_entry_t;

typedef struct fe_acl_action_entry_s {
	fe_acl_action_l2_entry_t	l2;
	fe_acl_action_l3_entry_t	l3;
	fe_acl_action_l4_entry_t	l4;
	fe_acl_action_misc_entry_t	misc;
	__u8	parity;
} fe_acl_action_entry_t;

typedef struct fe_acl_entry_s {
	fe_acl_rule_entry_t rule;
	fe_acl_action_entry_t action;
} fe_acl_entry_t;

/* PKTLEN RNGS Table Entry */
typedef struct fe_pktlen_rngs_entry_s {
	__u16	low;
	__u16	high;
	__u8	valid;
} fe_pktlen_rngs_entry_t;

/*
 * LPM Table Entry
 *
 */
typedef struct fe_lpm_entry_s {
	__u32		ip_addr[4];
	__u8		mask;	/* Ipv4 6 bits mask each , Ipv6 8 bits mask*/
	__u8		priority;	/* 6 bits priority */
	__u16		result_idx;	/* 13 bits to result table */
	__u8		ipv6;
} fe_lpm_entry_t;


/* Table Size */
#define FE_AN_BNG_MAC_ENTRY_MAX		15
#define FE_PORT_RANGE_ENTRY_MAX		32
#define FE_ETH_CAM_ENTRY_MAX		63
#define FE_LLC_ENTRY_MAX			3
#define FE_LPB_ENTRY_MAX			8

#define FE_CLASS_ENTRY_MAX			64
#define FE_SDB_ENTRY_MAX			64
#define FE_VLAN_ENTRY_MAX			4096
#define FE_FWD_RESULT_ENTRY_MAX		8192
#define FE_QOS_RESULT_ENTRY_MAX		128

#define FE_VOQ_POL_ENTRY_MAX		512
#define FE_FVLAN_ENTRY_MAX			512
#define FE_L2_ADDR_PAIR_ENTRY_MAX	512
#define FE_L3_ADDR_ENTRY_MAX		1024
#define FE_PE_VOQ_DROP_ENTRY_MAX	32
#define FE_ACL_ENTRY_MAX			128
#define FE_PKTLEN_RANGE_ENTRY_MAX	4

#define FE_LPM_ENTRY_MAX			64	/* FIXME! confirm this number!! */

/* 4096 buckets, with 6 hash entries each bucket */
#define FE_SW_HASH_ENTRY_MAX		4096
#define FE_HASH_ENTRY_MAX			12288
#define FE_HASH_OVERFLOW_ENTRY_MAX	64
#define FE_HASH_STATUS_ENTRY_MAX	385
#define FE_HASH_MASK_ENTRY_MAX		64
#define FE_HASH_DBG_FIFO_ENTRY_MAX	32
#define FE_HASH_CHECK_MEM_ENTRY_MAX	8192

#define FE_MAX_ENTRY_SIZE		512	/* assume fe_xx_entry_t max size of is not over this */


#if 0
typedef struct {
	void		*p_entry;
//	atomic_t	users;
	__u16	users;
//	__u16	use_count;
} fe_table_entry_s;
#endif

/*
 * Hash Match Table Entry
 * Total 24k entries.
 * Hash result, Result table index, mask ptr, etc.
 * Defined for Forwarding Engine Spec. Table 1-7
 */
typedef struct fe_sw_hash_s {
	__u8   	mac_da[6];
	__u8   	mac_sa[6];
	__u16  	eth_type;
	__u8   	llc_type_enc;
	__u8	ip_frag;
	__u8   	revd_115;
	__u8   	tpid_enc_1;
	__u8   	_8021p_1;
	__u8   	dei_1;
	__u16  	vid_1;
	__u8   	revd_135;
	__u8   	tpid_enc_2;
	__u8   	_8021p_2;
	__u8   	dei_2;
	__u16  	vid_2;
	__u32	da[4];
	__u32	sa[4];
	__u8   	ip_prot;
	__u8   	dscp;
	__u8   	ecn;
	__u8	pktlen_rng_match_vector;
	__u32	ipv6_flow_label;    /* IPv6: Flow label, IPv4: Set to 20'b0 */
	__u8	ip_version;		    /* 0: ipv4;		1: ipv6 */
	__u8	ip_valid;
	__u16	l4_dp;
	__u16	l4_sp;
	__u8	tcp_ctrl_flags;
	__u8	tcp_ecn_flags;
	__u8	l4_valid;
	__u8	sdbid;
	__u8	lspid;
	__u8	fwdtype;
	__u8	pppoe_session_id_valid;
	__u16	pppoe_session_id;
	__u8	mask_ptr_0_7;
	__u16	mcgid;
	__u8	mc_idx;
	__u8	da_an_mac_sel;
	__u8	da_an_mac_hit;
	__u8	sa_bng_mac_sel;
	__u8	sa_bng_mac_hit;
	__u8	orig_lspid;
	__u32	recirc_idx;
	__u32	l7_field;
	__u8	l7_field_valid;
	__u8	hdr_a_flags_crcerr;
	__u8	l3_csum_err;
	__u8	l4_csum_err;
	__u8	not_hdr_a_flags_stsvld;
	__u8	hash_fid;
	__u8	mc_da;
	__u8	bc_da;
	__u8	spi_vld;
	__u32	spi_idx;
	__u8	ipv6_ndp;
	__u8	ipv6_hbh;
	__u8	ipv6_rh;
	__u8	ipv6_doh;
	__u8	ppp_protocol_vld;
	__u16	ppp_protocol;
} fe_sw_hash_t;

typedef enum {
	FE_HASH_DA,
	FE_HASH_SA,
	FE_HASH_ETHERTYPE_RAW,
	FE_HASH_LLC_TYPE_ENC,
	FE_HASH_IP_FRAGMENT,
	FE_HASH_TPID_ENC_1,
	FE_HASH_8021P_1,
	FE_HASH_DEI_1,
	FE_HASH_VID_1,
	FE_HASH_RSVD_135,
	FE_HASH_TPID_ENC_2,
	FE_HASH_8021P_2,
	FE_HASH_DEI_2,
	FE_HASH_VID_2,
	FE_HASH_IP_DA,
	FE_HASH_IP_SA,
	FE_HASH_IP_PROT,
	FE_HASH_DSCP,
	FE_HASH_ECN,
	FE_HASH_PKTLEN_RNG_MATCH_VECTOR,
	FE_HASH_IPV6_FLOW_LBL,
	FE_HASH_IP_VER,
	FE_HASH_IP_VLD,
	FE_HASH_L4_DP_EXACT,
	FE_HASH_L4_SP_EXACT,
	FE_HASH_TCP_CTRL,
	FE_HASH_TCP_ECN,
	FE_HASH_L4_VLD,
	FE_HASH_SDB_KEYRULE,
	FE_HASH_LSPID,
	FE_HASH_FWDTYPE,
	FE_HASH_PPPOE_SESSION_ID_VLD,
	FE_HASH_PPPOE_SESSION_ID,
	FE_SDB_HTUPL_MASK_PTR_0_7,
	FE_HASH_MCGID,
	FE_HASH_MCIDX,
	FE_HASH_DA_AN_MAC_SEL,
	FE_HASH_DA_AN_MAC_HIT,
	FE_HASH_SA_BNG_MAC_SEL,
	FE_HASH_SA_BNG_MAC_HIT,
	FE_HASH_ORIG_LSPID,
	FE_HASH_RECIRC_IDX,
	FE_HASH_L7_FIELD,
	FE_HASH_L7_FIELD_VLD,
	FE_HASH_HDR_A_FLAGS_CRCERR,
	FE_HASH_L3_CHKSUM_ERR,
	FE_HASH_L4_CHKSUM_ERR,
	FE_HASH_NOT_HDR_A_FLAGS_STSVLD,
	FE_HASH_FID,
	FE_HASH_MC_DA,
	FE_HASH_BC_DA,
	FE_HASH_SPI_VLD,
	FE_HASH_SPI_IDX,
	FE_HASH_IPV6_NDP,
	FE_HASH_IPV6_HBH,
	FE_HASH_IPV6_RH,
	FE_HASH_IPV6_DOH,
	FE_HASH_PPP_PROTOCOL_VLD,
	FE_HASH_PPP_PROTOCOL,
	FE_HASH_MAX,
} HASH_HASH_FIELD_DEF;

#define FE_HASH_BYTE_CRC	1

#ifdef FE_HASH_BYTE_CRC
	#define FE_HASH_HASH_BITS  672
#else
	#define FE_HASH_HASH_BITS  667
#endif
	#define FE_HASH_HASH_BYTES (FE_HASH_HASH_BITS / 8) + ((FE_HASH_HASH_BITS % 8) ? 1 : 0)


typedef struct {
	__u32      field_type;
	__u16      start_pos;
	__u16      total_width;
} cs_fe_hash_hash_bits_field_s;

static const cs_fe_hash_hash_bits_field_s cs_fe_hash_hash_fields[FE_HASH_MAX] = {
	{FE_HASH_DA,                        0, 48},
	{FE_HASH_SA,                       48, 48},
	{FE_HASH_ETHERTYPE_RAW,            96, 16},
	{FE_HASH_LLC_TYPE_ENC,            112,  3},
	{FE_HASH_IP_FRAGMENT,             115,  1},
	{FE_HASH_TPID_ENC_1,              116,  3},
	{FE_HASH_8021P_1,                 119,  3},
	{FE_HASH_DEI_1,                   122,  1},
	{FE_HASH_VID_1,                   123, 12},
	{FE_HASH_RSVD_135,                135,  1},
	{FE_HASH_TPID_ENC_2,              136,  3},
	{FE_HASH_8021P_2,                 139,  3},
	{FE_HASH_DEI_2,                   142,  1},
	{FE_HASH_VID_2,                   143, 12},
	{FE_HASH_IP_DA,                   155,128},
	{FE_HASH_IP_SA,                   283,128},
	{FE_HASH_IP_PROT,                 411,  8},
	{FE_HASH_DSCP,                    419,  6},
	{FE_HASH_ECN,                     425,  2},
	{FE_HASH_PKTLEN_RNG_MATCH_VECTOR, 427,  4},
	{FE_HASH_IPV6_FLOW_LBL,           431, 20},
	{FE_HASH_IP_VER,                  451,  1},
	{FE_HASH_IP_VLD,                  452,  1},
	{FE_HASH_L4_DP_EXACT,             453, 16},
	{FE_HASH_L4_SP_EXACT,             469, 16},
	{FE_HASH_TCP_CTRL,                485,  6},
	{FE_HASH_TCP_ECN,                 491,  3},
	{FE_HASH_L4_VLD,                  494,  1},
	{FE_HASH_SDB_KEYRULE,             495,  6},
	{FE_HASH_LSPID,                   501,  4},
	{FE_HASH_FWDTYPE,                 505,  4},
	{FE_HASH_PPPOE_SESSION_ID_VLD,    509,  1},
	{FE_HASH_PPPOE_SESSION_ID,        510, 16},
	{FE_SDB_HTUPL_MASK_PTR_0_7,       526,  6},
	{FE_HASH_MCGID,                   532,  9},
	{FE_HASH_MCIDX,                   541,  5},
	{FE_HASH_DA_AN_MAC_SEL,           546,  4},
	{FE_HASH_DA_AN_MAC_HIT,           550,  1},
	{FE_HASH_SA_BNG_MAC_SEL,          551,  4},
	{FE_HASH_SA_BNG_MAC_HIT,          555,  1},
	{FE_HASH_ORIG_LSPID,              556,  4},
	{FE_HASH_RECIRC_IDX,              560, 10},
	{FE_HASH_L7_FIELD,                570, 32},
	{FE_HASH_L7_FIELD_VLD,            602,  1},
	{FE_HASH_HDR_A_FLAGS_CRCERR,      603,  1},
	{FE_HASH_L3_CHKSUM_ERR,           604,  1},
	{FE_HASH_L4_CHKSUM_ERR,           605,  1},
	{FE_HASH_NOT_HDR_A_FLAGS_STSVLD,  606,  1},
	{FE_HASH_FID,                     607,  4},
	{FE_HASH_MC_DA,                   611,  1},
	{FE_HASH_BC_DA,                   612,  1},
	{FE_HASH_SPI_VLD,                 613,  1},
	{FE_HASH_SPI_IDX,                 614, 32},
	{FE_HASH_IPV6_NDP,                646,  1},
	{FE_HASH_IPV6_HBH,                647,  1},
	{FE_HASH_IPV6_RH,                 648,  1},
	{FE_HASH_IPV6_DOH,                649,  1},
	{FE_HASH_PPP_PROTOCOL_VLD,        650,  1},
	{FE_HASH_PPP_PROTOCOL,            651, 16},
};

typedef struct fe_hash_check_entry_s {
    __u8        check_l4_sp_en;
    __u8        check_l4_dp_en;
    __u8        check_mac_sa_en;
    __u8        check_mac_da_en;
    __u8        check_ip_sa_en;
    __u8        check_ip_da_en;
    __u16       check_l4_sp_to_be_chk;
    __u16       check_l4_dp_to_be_chk;
    __u16       check_l2_check_idx;
    __u16       check_ip_sa_check_idx;
    __u16       check_ip_da_check_idx;
    __u8        check_parity;
    __u32       check_reserved;
} fe_hash_check_entry_t;



typedef union
{
//	__u64 bits32_e;
	struct bit_header_e
	{
#if 0
    	__u64 resevred				 : 13;	/* 51 ~ 63 */
    	__u64 linux_buffer_number	 : 3;	/* 48 ~ 50 */
    	__u64 time_stamped_pkt		 : 1;	/* 47      */
    	__u64 lspid           		 : 4;	/* 43 ~ 46 */
    	__u64 spare           		 : 4;	/* 39 ~ 42 */
    	__u64 cpu_ptp_flag    		 : 1;	/* 38      */
    	__u64 cpu_header_exist		 : 1;	/* 37      */
    	__u64 acl_replaced_voq_valid : 1;	/* 36      */
    	__u64 cpu_header      		 : 1;	/* 35      */
    	__u64 mark    				 : 1;	/* 34      */
    	__u64 cos    				 : 3;	/* 31 ~ 33 */
		__u64 mc_grp_id				 : 9;	/* 22 ~ 30 */
    	__u64 fwd_pkt_info    		 : 4;	/* 18 ~ 21 */
    	__u64 fwd_type        		 : 4;	/* 14 ~ 17 */
    	__u64 pkt_size        		 : 14;	/*  0 ~ 13 */
#else
    	__u64 pkt_size					: 14;	/*  0 ~ 13 */
    	__u64 fwd_type					: 4;	/* 14 ~ 17 */
    	__u64 fwd_pkt_info				: 4;	/* 18 ~ 21 */
		__u64 mc_grp_id					: 9;	/* 22 ~ 30 */
    	__u64 cos						: 3;	/* 31 ~ 33 */
    	__u64 mark						: 1;	/* 34      */
    	__u64 acl_replaced_voq_valid	: 1;	/* 35      */
    	__u64 cpu_header				: 1;	/* 36      */
    	__u64 cpu_ptp_flag				: 1;	/* 38      */
    	__u64 spare						: 4;	/* 39 ~ 42 */
    	__u64 lspid						: 4;	/* 43 ~ 46 */
    	__u64 time_stamped_pkt			: 1;	/* 47      */
    	__u64 resevred					: 13;	/* 51 ~ 63 */
    	__u64 linux_buffer_number		: 3;	/* 48 ~ 50 */
#endif
    } bits;
	__u32 bits32[2];
}  HEADER_E_T;

typedef union
{
/* CS_LITTLE_ENDIAN */
	__u64 bits64;
	struct bit_cpu0
	{
#if 0
		__u64 fwd_valids		: 2;	/* 62 ~ 63 */
		__u64 lpm_result_idx	: 6;	/* 56 ~ 61 */
		__u64 l2l3_flags		: 4;	/* 52 ~ 55 */
		__u64 acl_id			: 6;	/* 46 ~ 51 */
		__u64 dst_voq			: 8;	/* 38 ~ 45 */
		__u64 hash_result_idx	: 13;	/* 25 ~ 37 */
		__u64 pspid				: 4;	/* 21 ~ 24 */
		__u64 flags				: 13;	/*  8 ~ 20 */
		__u64 acl_vld_dvoq_cpu	: 1;	/*  7      */
		__u64 cpuhdr_voq		: 7;	/*  0 ~ 6  */
#else
		__u64 cpuhdr_voq		: 7;	/*  0 ~ 6  */
		__u64 acl_vld_dvoq_cpu	: 1;	/*  7      */
		__u64 flags				: 13;	/*  8 ~ 20 */
		__u64 pspid				: 4;	/* 21 ~ 24 */
		__u64 hash_result_idx	: 13;	/* 25 ~ 37 */
		__u64 dst_voq			: 8;	/* 38 ~ 45 */
		__u64 acl_id			: 6;	/* 46 ~ 51 */
		__u64 l2l3_flags		: 4;	/* 52 ~ 55 */
		__u64 lpm_result_idx	: 6;	/* 56 ~ 61 */
		__u64 fwd_valids		: 2;	/* 62 ~ 63 */
#endif
	} bits;
	__u32 bits32[2];
}  CPU_HEADER0_T;

typedef union
{
	__u64 bits64;
	struct bit_cpu1
	{
#if 0
		__u64 input_l4_start	: 8;	/* 56 ~ 63 */
		__u64 class_match		: 1;	/* 55      */
		__u64 svidx				: 6;	/* 49 ~ 54 */
		__u64 superhash_vld		: 1;	/* 48      */
		__u64 superhash			: 16;	/* 32 ~ 47 */
		__u64 spare				: 1;	/* 31      */
		__u64 cpucopy_valid		: 1;	/* 30      */
		__u64 l4_csum_err		: 1;	/* 29      */
		__u64 ipv4_csum_err		: 1;	/* 28      */
		__u64 input_l3_start	: 8;	/* 20 ~ 27 */
		__u64 sw_action			: 20;	/*  0 ~ 19 */
#else
		__u64 sw_action			: 20;	/*  0 ~ 19 */
		__u64 input_l3_start	: 8;	/* 20 ~ 27 */
		__u64 ipv4_csum_err		: 1;	/* 28      */
		__u64 l4_csum_err		: 1;	/* 29      */
		__u64 cpucopy_valid		: 1;	/* 30      */
		__u64 spare				: 1;	/* 31      */
		__u64 superhash			: 16;	/* 32 ~ 47 */
		__u64 superhash_vld		: 1;	/* 48      */
		__u64 svidx				: 6;	/* 49 ~ 54 */
		__u64 class_match		: 1;	/* 55      */
		__u64 input_l4_start	: 8;	/* 56 ~ 63 */
#endif
	} bits;
	__u32 bits32[2];
}  CPU_HEADER1_T;


/* QoS Mapping Table API */
int cs_fe_qos_mapping_table_init(void);
int cs_fe_set_qos_mapping_table(int8_t i_pri, int8_t i_dscp,
	int8_t e_pri, int8_t e_dscp);
int cs_fe_get_qos_mapping_result(int8_t i_pri, int8_t i_dscp,
	int8_t *e_pri, int8_t *e_dscp);
int cs_fe_print_qos_mapping_table(void);

#endif /* __CS_FE_CORE_TABLE_H__ */
