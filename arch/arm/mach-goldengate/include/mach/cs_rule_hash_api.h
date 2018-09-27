#ifndef G2_RULE_HASH_API_H
#define G2_RULE_HASH_API_H

#include "cs_common_api.h"


typedef struct cs_rule_hash_key_s {
        cs_uint8_t    mac_da[CS_ETH_ADDR_LEN];
        cs_uint8_t    mac_sa[CS_ETH_ADDR_LEN];
        cs_uint16_t   eth_type;
        cs_uint8_t    tpid_enc_1;
	cs_uint8_t    _8021p_1;
        cs_uint16_t   vid_1;
        cs_uint8_t    tpid_enc_2;
	cs_uint8_t    _8021p_2;
        cs_uint16_t   vid_2;
        cs_uint32_t   da[4];
        cs_uint32_t   sa[4];
        cs_uint8_t    ip_prot;
        cs_uint8_t    dscp;
        cs_uint8_t    ip_version;                 /* 0: ipv4;         1: ipv6 */
        cs_uint8_t    ip_valid;
        cs_uint16_t   l4_dp;
        cs_uint16_t   l4_sp;
        cs_uint8_t    l4_valid;
        cs_uint8_t    lspid;
        cs_uint8_t    fwdtype;
        cs_uint8_t    pppoe_session_id_valid;
        cs_uint16_t   pppoe_session_id;
	cs_uint8_t    mask_ptr_0_7;
        cs_uint16_t   mcgid;
        cs_uint8_t    mc_idx;
        cs_uint8_t    orig_lspid;
        cs_uint32_t   recirc_idx;
        cs_uint32_t   l7_field;
        cs_uint8_t    l7_field_valid;
	cs_uint8_t    ppp_protocol_vld;
        cs_uint16_t   ppp_protocol;	
	cs_uint8_t    spi_vld;
        cs_uint32_t   spi_idx;
} cs_rule_hash_key_t;

/*
 * Flow VLAN Table Entry
 * Total 512 entries.
 *
 */
typedef struct cs_rule_hash_fvlan_s {
        cs_uint8_t    first_vlan_cmd;
        cs_uint16_t   first_vid;
        cs_uint8_t    first_tpid_enc;
        cs_uint8_t    second_vlan_cmd;
        cs_uint16_t   second_vid;
        cs_uint8_t    second_tpid_enc;
        cs_uint8_t    parity;
} cs_rule_hash_fvlan_t;

/*
 * VoQ & Policer Table Entry
 * Total 512 entries.
 */

typedef struct cs_rule_hash_voq_pol_s {
        cs_uint8_t    voq_base;               /* base destination voq of the packet */
        cs_uint8_t    pol_base;               /* base destinaiton policer id of the packet */
        cs_uint8_t    cpu_pid;                /* CPU sub port ID. */
        cs_uint8_t    ldpid;                  /* logical destination port that voq maps to */
        cs_uint16_t   pppoe_session_id;       /* pppoe session ID to be encaped */
        cs_uint8_t    cos_nop;
        cs_uint8_t    parity;
} cs_rule_hash_voq_pol_t;

/*
 * Main Forwarding Results Table
 * Total 8k entries.
 *
 */

typedef struct cs_rule_hash_fwd_result_l2_s {
        cs_uint8_t    mac_sa_replace_en;
        cs_uint8_t    mac_da_replace_en;
        cs_uint16_t   l2_index;               /* index to MAC table */
        cs_uint16_t   mcgid;
        cs_uint8_t    mcgid_valid;

        cs_uint8_t    flow_vlan_op_en;
        cs_uint16_t   flow_vlan_index;        /* index to flow vlan table entry */

        cs_uint8_t    pppoe_encap_en;
        cs_uint8_t    pppoe_decap_en;

	cs_uint8_t    mac_da[CS_ETH_ADDR_LEN];
        cs_uint8_t    mac_sa[CS_ETH_ADDR_LEN];
} cs_rule_hash_fwd_result_l2_t;

typedef struct cs_rule_hash_fwd_result_l3_s {
        cs_uint8_t    ip_sa_replace_en;
        cs_uint8_t    ip_da_replace_en;

        cs_uint16_t   ip_sa_index;
        cs_uint16_t   ip_da_index;

        cs_uint8_t    decr_ttl_hoplimit;

	cs_uint32_t   ip_da[4];
        cs_uint32_t   ip_sa[4];
} cs_rule_hash_fwd_result_l3_t;

typedef struct cs_rule_hash_fwd_result_l4_s {
        cs_uint8_t    sp_replace_en;
        cs_uint8_t    dp_replace_en;
        cs_uint16_t   sp;
        cs_uint16_t   dp;
} cs_rule_hash_fwd_result_l4_t;

typedef struct cs_rule_hash_fwd_result_dest_s {
        cs_uint8_t    pol_policy;
        cs_uint8_t    voq_policy;
        cs_uint16_t   voq_pol_table_index;
} cs_rule_hash_fwd_result_dest_t;

typedef struct cs_rule_hash_fwd_result_act_s {
        cs_uint8_t    fwd_type_valid;
        cs_uint8_t    fwd_type;
        cs_uint8_t    drop;
} cs_rule_hash_fwd_result_act_t;

typedef struct cs_rule_hash_fwd_result_s {
        cs_rule_hash_fwd_result_l2_t      l2;
        cs_rule_hash_fwd_result_l3_t      l3;
        cs_rule_hash_fwd_result_l4_t      l4;
        cs_rule_hash_fwd_result_dest_t    dest;
        cs_rule_hash_fwd_result_act_t             act;
        cs_uint8_t    acl_dsbl;
        cs_uint8_t    parity;
} cs_rule_hash_fwd_result_t;

/* Please refer to ./drivers/net/cs752x/src/include/cs_core_vtable.h for apptype definition */
typedef struct cs_rule_hash_s {
	cs_uint32_t			apptype;
	cs_uint32_t  			mac_type;
	cs_rule_hash_key_t		key;	
	cs_rule_hash_fvlan_t		fvlan;
	cs_rule_hash_voq_pol_t		voq_pol;
	cs_rule_hash_fwd_result_t	fwd_result;	
	cs_uint16_t                     hash_index;
	cs_uint32_t                     voq_pol_idx;
	cs_uint32_t                     fwd_rslt_idx;
	cs_uint32_t			vlan_rslt_idx;
	cs_uint16_t			mcgid;
	cs_uint16_t			mcidx;
} cs_rule_hash_t;

cs_status_t cs_rule_hash_add(CS_IN cs_dev_id_t device_id, CS_IN_OUT cs_rule_hash_t *p_rule_hash);
cs_status_t cs_rule_hash_get_by_hash_index(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t hash_index, CS_OUT cs_rule_hash_t *p_rule_hash);
cs_status_t cs_rule_hash_delete_by_hash_index(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t hash_index);

#endif /* G2_RULE_HASH_API_H */

