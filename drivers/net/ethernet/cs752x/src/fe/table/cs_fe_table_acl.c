/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_acl.c
 *
 * $Id: cs_fe_table_acl.c,v 1.6 2012/10/30 08:54:26 ewang Exp $
 *
 * It contains the implementation for HW FE ACL Table Resource Management.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"
#include "cs_mut.h"

static cs_fe_table_t cs_fe_acl_table_type;

#define FE_ACL_TABLE_PTR	(cs_fe_acl_table_type.content_table)
#define FE_ACL_LOCK		&(cs_fe_acl_table_type.lock)

#define PRINT_RULE_ENTRY_U32(fmt, sw_field, hw_field, mask) { \
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx, hw_field, \
			&value); \
	printk(fmt, sw_field & mask, value & mask); \
}

#define PRINT_RULE_ENTRY_MAC(name, sw_mac, hw_mac) { \
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx, hw_mac, \
			(__u32*)value_mac); \
	printk("\t%s %02x:%02x:%02x:%02x:%02x:%02x" \
		" (HW %02x:%02x:%02x:%02x:%02x:%02x)\n", \
			name, \
			sw_mac[0], sw_mac[1], sw_mac[2], \
			sw_mac[3], sw_mac[4], sw_mac[5], \
			value_mac[0], value_mac[1], value_mac[2], \
			value_mac[3], value_mac[4], value_mac[5]); \
}


#define PRINT_RULE_ENTRY_IP(name, sw_ip, hw_ip) { \
	memset(value_ip, 0, 16); \
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx, hw_ip, \
			value_ip); \
	printk("\tIP %s %x.%x.%x.%x (HW %x.%x.%x.%x)\n", \
			name, \
			(__u32) sw_ip[0], (__u32) sw_ip[1], \
			(__u32) sw_ip[2], (__u32) sw_ip[3], \
			(__u32) value_ip[0], (__u32) value_ip[1], \
			(__u32) value_ip[2], (__u32) value_ip[3]); \
}

#define PRINT_ACTION_ENTRY_U32(fmt, sw_field, hw_field, mask) { \
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx, hw_field, \
			&value); \
	printk(fmt, sw_field & mask, value & mask); \
}

static int fe_acl_alloc_entry(unsigned int *rslt_idx, unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_acl_table_type, rslt_idx,
			start_offset);
} /* fe_acl_alloc_entry */

static int convert_sw_acl_rule_to_data_register(fe_acl_rule_entry_t *entry,
		__u32 *p_data_array, unsigned int size)
{
	__u32 value[4] = {0, 0, 0, 0};
	memset(p_data_array, 0x0, size * 4);

	if (entry->rule_valid == 1) {
		((__u8*)value)[0] = entry->l2.l2_mac_da[0];
		((__u8*)value)[1] = entry->l2.l2_mac_da[1];
		((__u8*)value)[2] = entry->l2.l2_mac_da[2];
		((__u8*)value)[3] = entry->l2.l2_mac_da[3];
		((__u8*)value)[4] = entry->l2.l2_mac_da[4];
		((__u8*)value)[5] = entry->l2.l2_mac_da[5];
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L2_MAC_DA, value, p_data_array, size);

		((__u8*)value)[0] = entry->l2.l2_mac_sa[0];
		((__u8*)value)[1] = entry->l2.l2_mac_sa[1];
		((__u8*)value)[2] = entry->l2.l2_mac_sa[2];
		((__u8*)value)[3] = entry->l2.l2_mac_sa[3];
		((__u8*)value)[4] = entry->l2.l2_mac_sa[4];
		((__u8*)value)[5] = entry->l2.l2_mac_sa[5];
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L2_MAC_SA, value, p_data_array, size);

		value[0] = (__u32)entry->l2.eth_type;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_ETHERTYPE, value, p_data_array, size);

		value[0] = (__u32)entry->l2.len_encoded;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LEN_ENCODED, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.tpid_1_vld;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_1_VLD, value, p_data_array, size);

		value[0] = (__u32)entry->l2.tpid_enc_1;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_ENC_1, value, p_data_array, size);

		value[0] = (__u32)entry->l2.vid_1;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_VID_1, value, p_data_array, size);

		value[0] = (__u32)entry->l2._8021p_1;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_8021P_1, value, p_data_array, size);

		value[0] = (__u32)entry->l2.dei_1;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DEI_1, value, p_data_array, size);

		value[0] = (__u32)entry->l2.tpid_2_vld;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_2_VLD, value, p_data_array, size);

		value[0] = (__u32)entry->l2.tpid_enc_2;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_ENC_2, value, p_data_array, size);

		value[0] = (__u32)entry->l2.vid_2;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_VID_2, value, p_data_array, size);

		value[0] = (__u32)entry->l2._8021p_2;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_8021P_2, value, p_data_array, size);

		value[0] = (__u32)entry->l2.dei_2;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DEI_2, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ip_vld;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_VLD, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ip_ver;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_VER, value, p_data_array, size);

		value[0] = (__u32)entry->l3.da[0];
		value[1] = (__u32)entry->l3.da[1];
		value[2] = (__u32)entry->l3.da[2];
		value[3] = (__u32)entry->l3.da[3];
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_DA, value, p_data_array, size);

		value[0] = (__u32)entry->l3.sa[0];
		value[1] = (__u32)entry->l3.sa[1];
		value[2] = (__u32)entry->l3.sa[2];
		value[3] = (__u32)entry->l3.sa[3];
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_SA, value, p_data_array, size);

		value[0] = (__u32)entry->l3.dscp;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DSCP, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ecn;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_ECN, value, p_data_array, size);

		value[0] = (__u32)entry->l3.proto;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_PROT, value, p_data_array, size);

		value[0] = (__u32)entry->l3.fragment;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_FRAGMENT, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.options;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_OPTIONS, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ipv6_flow_label;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_FLOW_LBL, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ttl_hoplimit;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TTL_HOPLMT, value, p_data_array, size);

		value[0] = (__u32)entry->l4.l4_valid;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_VLD, value, p_data_array, size);

		value[0] = (__u32)entry->l4.dp_lo;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_DPLO, value, p_data_array, size);

		value[0] = (__u32)entry->l4.dp_hi;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_DPHI, value, p_data_array, size);

		value[0] = (__u32)entry->l4.sp_lo;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_SPLO, value, p_data_array, size);

		value[0] = (__u32)entry->l4.sp_hi;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_SPHI, value, p_data_array, size);

		value[0] = (__u32)entry->misc.lspid;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LSPID, value, p_data_array, size);

		value[0] = (__u32)entry->misc.orig_lspid;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_ORIG_LSPID, value, p_data_array, size);

		value[0] = (__u32)entry->misc.fwd_type;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FWDTYPE, value, p_data_array, size);

		value[0] = (__u32)entry->misc.spl_pkt_vec;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPL_PKT_VEC, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.class_hit;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_CLASS_HIT, value, p_data_array, size);

		value[0] = (__u32)entry->misc.class_svidx;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_CLASS_SVIDX, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.lpm_hit;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LPM_HIT, value, p_data_array, size);

		value[0] = (__u32)entry->misc.lpm_hit_idx;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LPM_HIT_IDX, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.hash_hit;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_HASH_HIT, value, p_data_array, size);

		value[0] = (__u32)entry->misc.hash_hit_idx;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_HASH_HIT_IDX, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.da_an_mac_hit;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DA_AN_MAC_HIT, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.da_an_mac_hit_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DA_AN_MAC_HIT_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->misc.l7_field;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L7_FIELD, value, p_data_array, size);

		value[0] = (__u32)entry->l2.l2_mac_da_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L2_MAC_DA_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.l2_mac_sa_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L2_MAC_SA_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.ethertype_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_ETHERTYPE_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.len_encoded_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LEN_ENCODED_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.tpid_1_vld_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_1_VLD_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.tpid_enc_1_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_ENC_1_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.vid_1_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_VID_1_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l2._8021p_1_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_8021P_1_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.dei_1_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DEI_1_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l2.tpid_2_vld_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_2_VLD_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.tpid_enc_2_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TPID_ENC_2_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.vid_2_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_VID_2_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l2._8021p_2_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_8021P_2_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.dei_2_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DEI_2_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ip_vld_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_VLD_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ip_ver_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_VER_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ip_da_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_DA_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ip_sa_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_SA_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l3.dscp_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DSCP_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ecn_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_ECN_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ip_proto_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_PROT_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ip_fragment_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_FRAGMENT_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ip_options_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IP_OPTIONS_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ipv6_flow_label_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_FLOW_LBL_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->l3.ttl_hoplimit_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_TTL_HOPLMT_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l4.l4_valid_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_VLD_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l4.l4_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L4_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->misc.lspid_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LSPID_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->misc.orig_lspid_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_ORIG_LSPID_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.fwd_type_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FWDTYPE_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.spl_pkt_vec_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPL_PKT_VEC_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.class_hit_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_CLASS_HIT_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.class_svidx_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_CLASS_SVIDX_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.lpm_hit_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LPM_HIT_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.lpm_hit_idx_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_LPM_HIT_IDX_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.hash_hit_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_HASH_HIT_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.hash_hit_idx_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_HASH_HIT_IDX_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.l7_field_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_L7_FIELD_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.da_an_mac_sel;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DA_AN_MAC_SEL, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.da_an_mac_sel_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_DA_AN_MAC_SEL_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->l2.sa_bng_mac_sel;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SA_BNG_MAC_SEL, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.sa_bng_mac_sel_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SA_BNG_MAC_SEL_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->l2.sa_bng_mac_hit;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SA_BNG_MAC_HIT, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.sa_bng_mac_hit_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SA_BNG_MAC_HIT_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->misc.flags_vec;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FLAGS_VEC, value, p_data_array, size);

		value[0] = (__u32)entry->misc.flags_vec_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FLAGS_VEC_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.flags_vec_or;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FLAGS_VEC_OR, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.spl_pkt_vec_or;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPL_PKT_VEC_OR, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.recirc_idx;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_RECIRC_IDX, value, p_data_array, size);

		value[0] = (__u32)entry->misc.recirc_idx_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_RECIRC_IDX_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.ne_vec;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_NE_VEC, value, p_data_array, size);

		value[0] = (__u32)entry->misc.mc_idx;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_MCIDX, value, p_data_array, size);

		value[0] = (__u32)entry->misc.mc_idx_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_MCIDX_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->misc.sdb_drop;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SDB_DROP, value, p_data_array, size);

		value[0] = (__u32)entry->misc.sdb_drop_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SDB_DROP_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.fwd_drop;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FWD_DROP, value, p_data_array, size);

		value[0] = (__u32)entry->misc.fwd_drop_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_FWD_DROP_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.pppoe_session_id_vld;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPPOE_SESSION_ID_VLD, value,
				p_data_array, size);

		value[0] = (__u32)entry->l2.pppoe_session_id_vld_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPPOE_SESSION_ID_VLD_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->l2.pppoe_session_id;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPPOE_SESSION_ID, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.pppoe_session_id_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPPOE_SESSION_ID_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->l3.spi_vld;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPI_VLD, value, p_data_array, size);

		value[0] = (__u32)entry->l3.spi_vld_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPI_VLD_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.spi;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPI, value, p_data_array, size);

		value[0] = (__u32)entry->l3.spi_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_SPI_MASK, value, p_data_array, size);

		value[0] = (__u32)entry->l2.ppp_protocol_vld;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPP_PROTOCOL_VLD, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.ppp_protocol_vld_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPP_PROTOCOL_VLD_MASK, value,
				p_data_array, size);

		value[0] = (__u32)entry->l2.ppp_protocol;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPP_PROTOCOL, value, p_data_array,
				size);

		value[0] = (__u32)entry->l2.ppp_protocol_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PPP_PROTOCOL_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ipv6_ndp;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_NDP, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ipv6_ndp_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_NDP_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ipv6_hbh;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_HBH, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ipv6_hbh_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_HBH_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ipv6_rh;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_RH, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ipv6_rh_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_RH_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->l3.ipv6_doh;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_DOH, value, p_data_array, size);

		value[0] = (__u32)entry->l3.ipv6_doh_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_IPV6_DOH_MASK, value, p_data_array,
				size);

		value[0] = (__u32)entry->misc.pktlen_rng_vec;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PKTLEN_RNG_MATCH_VECTOR, value,
				p_data_array, size);

		value[0] = (__u32)entry->misc.pktlen_rng_vec_mask;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_PKTLEN_RNG_MATCH_VECTOR_MASK, value,
				p_data_array,
				size);

		value[0] = (__u32)entry->misc.rsvd_879_878;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_RSVD_879_878, value, p_data_array,
				size);

		value[0] = (__u32)entry->parity;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_MEM_PARITY, value, p_data_array, size);

		value[0] = 1;
		cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_RULE,
				ACL_RULE_VLD, value, p_data_array, size);
	}

	return 0;
} /* convert_sw_acl_rule_to_data_register */

static int convert_sw_acl_action_to_data_register(fe_acl_action_entry_t *entry,
		__u32 *p_data_array, unsigned int size)
{
	__u32 value;
	memset(p_data_array, 0x0, size * 4);

	value = (__u32)entry->misc.voq_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_VOQ_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.voq_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_VOQ_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.voq;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_VOQ, &value, p_data_array, size);

	value = (__u32)entry->misc.ldpid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_LDPID, &value, p_data_array, size);

	value = (__u32)entry->misc.cpucopy;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CPUCOPY, &value, p_data_array, size);

	value = (__u32)entry->misc.cpucopy_voq;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CPUCOPY_VOQ, &value, p_data_array, size);

	value = (__u32)entry->misc.cpucopy_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CPUCOPY_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.mirror_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MIRROR_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.mirror_id;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MIRROR_ID, &value, p_data_array, size);

	value = (__u32)entry->misc.mirror_id_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MIRROR_ID_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.wred_cos_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_WRED_COS_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.wred_cos;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_WRED_COS, &value, p_data_array, size);

	value = (__u32)entry->misc.wred_cos_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_WRED_COS_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.pre_mark_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_PRE_MARK_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.pre_mark;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_PRE_MARK, &value, p_data_array, size);

	value = (__u32)entry->misc.pre_mark_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_PRE_MARK_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.policer_id_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_POLICER_ID_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.policer_id;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_POLICER_ID, &value, p_data_array, size);

	value = (__u32)entry->misc.policer_id_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_POLICER_ID_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.drop_permit_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DROP_PERMIT_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.drop;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DROP, &value, p_data_array, size);

	value = (__u32)entry->misc.permit;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_PERMIT, &value, p_data_array, size);

	value = (__u32)entry->misc.drop_permit_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DROP_PERMIT_PRI, &value, p_data_array, size);

	value = (__u32)entry->l2._8021p_1_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_8021P_1_VLD, &value, p_data_array, size);

	value = (__u32)entry->l2._8021p_1;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_8021P_1, &value, p_data_array, size);

	value = (__u32)entry->l2._8021p_1_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_8021P_1_PRI, &value, p_data_array, size);

	value = (__u32)entry->l2.dei_1_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DEI_1_VLD, &value, p_data_array, size);

	value = (__u32)entry->l2.dei_1;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DEI_1, &value, p_data_array, size);

	value = (__u32)entry->l2.dei_1_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DEI_1_PRI, &value, p_data_array, size);

	value = (__u32)entry->l2._8021p_2_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_8021P_2_VLD, &value, p_data_array, size);

	value = (__u32)entry->l2._8021p_2;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_8021P_2, &value, p_data_array, size);

	value = (__u32)entry->misc.keep_ts_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_KEEP_TS_VLD, &value, p_data_array, size);

	value = (__u32)entry->l2._8021p_2_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_8021P_2_PRI, &value, p_data_array, size);

	value = (__u32)entry->l2.dei_2_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DEI_2_VLD, &value, p_data_array, size);

	value = (__u32)entry->l2.dei_2;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DEI_2, &value, p_data_array, size);

	value = (__u32)entry->l2.dei_2_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DEI_2_PRI, &value, p_data_array, size);

	value = (__u32)entry->l3.dscp_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DSCP_VLD, &value, p_data_array, size);

	value = (__u32)entry->l3.dscp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DSCP, &value, p_data_array, size);

	value = (__u32)entry->l3.dscp_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DSCP_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.fwdtype_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FWDTYPE_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.fwdtype;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FWDTYPE, &value, p_data_array, size);

	value = (__u32)entry->misc.fwdtype_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FWDTYPE_PRI, &value, p_data_array, size);

	value = (__u32)entry->misc.mcgid_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MCGID_VLD, &value, p_data_array, size);

	value = (__u32)entry->misc.mcgid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MCGID, &value, p_data_array, size);

	value = (__u32)entry->misc.mcdid_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MCGID_PRI, &value, p_data_array, size);

	value = (__u32)entry->l2.first_vlan_cmd_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FIRST_VLAN_CMD_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l2.first_vlan_cmd;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FIRST_VLAN_CMD, &value, p_data_array, size);

	value = (__u32)entry->l2.first_vid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FIRST_VID, &value, p_data_array, size);

	value = (__u32)entry->l2.first_tpid_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FIRST_TPID_ENC, &value, p_data_array, size);

	value = (__u32)entry->l2.first_vlan_cmd_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_FIRST_VLAN_CMD_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->l2.second_vlan_cmd_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_SECOND_VLAN_CMD_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l2.second_vlan_cmd;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_SECOND_VLAN_CMD, &value, p_data_array, size);

	value = (__u32)entry->l2.second_vid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_SECOND_VID, &value, p_data_array, size);

	value = (__u32)entry->l2.second_tpid_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_SECOND_TPID_ENC, &value, p_data_array, size);

	value = (__u32)entry->l2.second_vlan_cmd_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_SECOND_VLAN_CMD_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->misc.keep_ts;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_KEEP_TS, &value, p_data_array, size);

	value = (__u32)entry->misc.keep_ts_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_KEEP_TS_PRI, &value, p_data_array, size);

	value = (__u32)entry->l3.ip_sa_replace_en_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_SA_REPLACE_EN_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l3.ip_sa_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_SA_REPLACE_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l3.ip_sa_replace_en_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_SA_REPLACE_EN_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->l3.ip_da_replace_en_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_DA_REPLACE_EN_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l3.ip_da_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_DA_REPLACE_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l3.ip_da_replace_en_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_DA_REPLACE_EN_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->l4.sp_replace_en_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_SP_REPLACE_EN_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l4.sp_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_SP_REPLACE_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l4.sp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_SP, &value, p_data_array, size);

	value = (__u32)entry->l4.sp_replace_en_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_SP_REPLACE_EN_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->l4.dp_replace_en_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_DP_REPLACE_EN_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l4.dp_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_DP_REPLACE_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l4.dp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_DP, &value, p_data_array, size);

	value = (__u32)entry->l4.dp_replace_en_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L4_DP_REPLACE_EN_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->l3.ip_sa_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_SA_IDX, &value, p_data_array, size);

	value = (__u32)entry->l3.ip_da_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_IP_DA_IDX, &value, p_data_array, size);

	value = (__u32)entry->l2.mac_da_sa_replace_en_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MAC_DA_SA_REPLACE_EN_VLD, &value,
			p_data_array, size);

	value = (__u32)entry->l2.mac_da_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MAC_DA_REPLACE_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l2.mac_sa_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MAC_SA_REPLACE_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l2.mac_da_sa_replace_en_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MAC_DA_SA_REPLACE_EN_PRI, &value,
			p_data_array, size);

	value = (__u32)entry->l2.l2_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_L2_IDX, &value, p_data_array, size);

	value = (__u32)entry->l3.change_dscp_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CHANGE_DSCP_EN, &value, p_data_array, size);

	value = (__u32)entry->l3.decr_ttl_hoplimit_vld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DECR_TTL_HOPLIMIT_VLD, &value, p_data_array,
			size);

	value = (__u32)entry->l3.decr_ttl_hoplimit;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DECR_TTL_HOPLIMIT, &value, p_data_array,
			size);

	value = (__u32)entry->l3.decr_ttl_hoplimit_pri;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_DECR_TTL_HOPLIMIT_PRI, &value, p_data_array,
			size);

	value = (__u32)entry->misc.voq_cpupid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_VOQ_CPUPID, &value, p_data_array, size);

	value = (__u32)entry->misc.cpucopy_cpupid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CPUCOPY_CPUPID, &value, p_data_array, size);

	value = (__u32)entry->l2.change_8021p_1_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CHANGE_8021P_1_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l2.change_dei_1_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CHANGE_DEI_1_EN, &value, p_data_array, size);

	value = (__u32)entry->l2.change_8021p_2_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CHANGE_8021P_2_EN, &value, p_data_array,
			size);

	value = (__u32)entry->l2.change_dei_2_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_CHANGE_DEI_2_EN, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_ACL_ACTION,
			ACL_ACTION_MEM_PARITY, &value, p_data_array, size);

	return 0;
} /* convert_sw_acl_action_to_data_register */

static int fe_acl_set_entry(unsigned int idx, void *entry)
{
	fe_acl_entry_t *acl_entry = (fe_acl_entry_t *)entry;
	cs_table_entry_t *p_sw_entry;
	__u32 data_array[30];
	unsigned int table_size;
	int ret;
	unsigned long flags;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;

	if (idx >= cs_fe_acl_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;

	if (FE_ACL_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_ACL_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/* Deal with ACL Rule table first */
	ret = cs_fe_hw_table_get_entry_value(FE_TABLE_ACL_RULE, idx, data_array,
			&table_size);
	if (ret != 0)
		return ret;

	/* Generate the data register value based on the given entry */
	ret = convert_sw_acl_rule_to_data_register(&acl_entry->rule, data_array,
			table_size);
	if (ret != 0)
		return ret;

	spin_lock_irqsave(FE_ACL_LOCK, flags);
	/* set it to HW indirect access table */
	ret = cs_fe_hw_table_set_entry_value(FE_TABLE_ACL_RULE, idx, table_size,
			data_array);
	spin_unlock_irqrestore(FE_ACL_LOCK, flags);
	if (ret != 0)
		return ret;

	/* Deal with ACL Action table */
	ret = cs_fe_hw_table_get_entry_value(FE_TABLE_ACL_ACTION, idx,
			data_array, &table_size);
	if (ret != 0)
		return ret;

	/* Generate the data register value based on the given entry */
	ret = convert_sw_acl_action_to_data_register(&acl_entry->action,
			data_array, table_size);
	if (ret != 0)
		return ret;

	/* set it to HW indirect access table */
	spin_lock_irqsave(FE_ACL_LOCK, flags);
	ret = cs_fe_hw_table_set_entry_value(FE_TABLE_ACL_ACTION, idx,
			table_size, data_array);
	spin_unlock_irqrestore(FE_ACL_LOCK, flags);
	if (ret != 0)
		return ret;

	/* store the ACL entry in SW table */
	spin_lock_irqsave(FE_ACL_LOCK, flags);
	if (p_sw_entry->data == NULL)
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_acl_table_type);
	if (p_sw_entry->data == NULL) {
		spin_unlock_irqrestore(FE_ACL_LOCK, flags);
		return -ENOMEM;
	}
	memcpy(((fe_table_entry_t*)p_sw_entry->data)->p_entry, entry,
			sizeof(fe_acl_entry_t));
	spin_unlock_irqrestore(FE_ACL_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_acl_set_entry */

static int fe_acl_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_acl_table_type, idx);
} /* fe_acl_inc_entry_refcnt */

static int fe_acl_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_acl_table_type, idx);
} /* fe_acl_dec_entry_refcnt */

static int fe_acl_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_acl_table_type, idx, p_cnt);
} /* fe_acl_get_entry_refcnt */

static int fe_acl_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_acl_table_type, entry, rslt_idx);
} /* fe_acl_add_entry */

static int fe_acl_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_acl_table_type, entry, rslt_idx,
			start_offset);
} /* fe_acl_find_entry */

/* cannot use fe_table_del_entry_by_idx, because ACL requires
 * cleaning 2 HW tables at once.  ACL_RULE and ACL_ACTION */
static int fe_acl_del_entry_by_idx(unsigned int idx, bool f_force)
{
	cs_table_entry_t *p_sw_entry;
	fe_table_entry_t *p_fe_entry;
	bool f_clean_entry = false;
	unsigned long flags;
	int ret;

	if (idx >= cs_fe_acl_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;

	if (FE_ACL_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	spin_lock_irqsave(FE_ACL_LOCK, flags);

	p_sw_entry = cs_table_get_entry(FE_ACL_TABLE_PTR, idx);
	if (p_sw_entry == NULL) {
		spin_unlock_irqrestore(FE_ACL_LOCK, flags);
		return FE_TABLE_ENULLPTR;
	}
	/* We don't decrement the reference count if the entry is not used */
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0) {
		spin_unlock_irqrestore(FE_ACL_LOCK, flags);
		return FE_TABLE_EENTRYNOTRSVD;
	}

	p_fe_entry = (fe_table_entry_t*)p_sw_entry->data;
	if (p_fe_entry == NULL) {
		spin_unlock_irqrestore(FE_ACL_LOCK, flags);
		return FE_TABLE_ENULLPTR;
	}

	if (f_force == true) atomic_set(&p_fe_entry->users, 0);

	/* if it is already 0, just return it */
	if (atomic_read(&p_fe_entry->users) == 0) f_clean_entry = true;

	if ((f_clean_entry == false) &&
			(atomic_dec_and_test(&p_fe_entry->users)))
		f_clean_entry = true;

	if (f_clean_entry == true) {
		cs_free(p_fe_entry->p_entry);
		p_fe_entry->p_entry = NULL;
		cs_free(p_sw_entry->data);
		p_sw_entry->data = NULL;
		cs_fe_acl_table_type.used_entry--;
		ret = cs_fe_hw_table_clear_entry(FE_TABLE_ACL_RULE, idx);
		if (ret != 0) {
			spin_unlock_irqrestore(FE_ACL_LOCK, flags);
			return ret;
		}
		ret = cs_fe_hw_table_clear_entry(FE_TABLE_ACL_ACTION, idx);
		if (ret != 0) {
			spin_unlock_irqrestore(FE_ACL_LOCK, flags);
			return ret;
		}
		p_sw_entry->local_data &= ~FE_TABLE_ENTRY_USED;
	}

	spin_unlock_irqrestore(FE_ACL_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_acl_del_entry_by_idx */

static int fe_acl_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_acl_table_type, entry, f_force);
} /* fe_acl_del_entry */

static int fe_acl_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_acl_table_type, idx, entry);
} /* fe_acl_get_entry */

static int fe_acl_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_acl_table_type);
} /* fe_acl_flush_table */

static void _fe_acl_print_entry(unsigned int idx)
{
	fe_acl_entry_t acl_entry, *p_acl;
	__u32 value;
	__u8 value_mac[6];
	__u32 value_ip[4];
	int status;
	unsigned int count;

	status = fe_acl_get_entry(idx, &acl_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_acl_get_entry_refcnt(idx, &count);

	p_acl = &acl_entry;

	printk("| index: %04d | refcnt: %u\n", idx, count);

	/* Rule */
	printk("  |- ACL Rule:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_VLD, &value);
	printk("\tRule_Valid %u (HW %u)\n", p_acl->rule.rule_valid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			ACL_RULE_MEM_PARITY, &value);
	printk("\tParity %01x (HW %01x)\n", p_acl->rule.parity, (__u8) value);

	/* L2 rule */
	printk("    |- L2 Rule:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L2_MAC_DA, (__u32*)value_mac);
	printk(" \tDA %02x:%02x:%02x:%02x:%02x:%02x"
		" (HW %02x:%02x:%02x:%02x:%02x:%02x)\n",
		p_acl->rule.l2.l2_mac_da[0],	p_acl->rule.l2.l2_mac_da[1],
		p_acl->rule.l2.l2_mac_da[2],	p_acl->rule.l2.l2_mac_da[3],
		p_acl->rule.l2.l2_mac_da[4],	p_acl->rule.l2.l2_mac_da[5],
		value_mac[0], value_mac[1], value_mac[2],
		value_mac[3], value_mac[4], value_mac[5]);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L2_MAC_DA_MASK, &value);
	printk("\tDA MAC Mask 0x%x (HW 0x%x)\n", p_acl->rule.l2.l2_mac_da_mask,
			(__u8) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L2_MAC_SA, (__u32*)value_mac);
	printk("\tSA %02x:%02x:%02x:%02x:%02x:%02x"
		" (HW %02x:%02x:%02x:%02x:%02x:%02x)\n",
		p_acl->rule.l2.l2_mac_sa[0],	p_acl->rule.l2.l2_mac_sa[1],
		p_acl->rule.l2.l2_mac_sa[2],	p_acl->rule.l2.l2_mac_sa[3],
		p_acl->rule.l2.l2_mac_sa[4],	p_acl->rule.l2.l2_mac_sa[5],
		value_mac[0], value_mac[1], value_mac[2],
		value_mac[3], value_mac[4], value_mac[5]);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L2_MAC_SA_MASK, &value);
	printk("\tSA MAC Mask 0x%x (HW 0x%x)\n", p_acl->rule.l2.l2_mac_sa_mask,
			(__u8) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_ETHERTYPE, &value);
	printk("\tEthertype 0x%04x (HW 0x%04x)", p_acl->rule.l2.eth_type,
			(__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_ETHERTYPE_MASK, &value);
	printk("\tEthertype Mask %u (HW %u)\n", p_acl->rule.l2.ethertype_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LEN_ENCODED, &value);
	printk("\tLEN encoded %u (HW %u)", p_acl->rule.l2.len_encoded, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LEN_ENCODED_MASK, &value);
	printk("\tLEN Encoded Mask %u (HW %u)\n",
			p_acl->rule.l2.len_encoded_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_1_VLD, &value);
	printk("\tTPID_1 Valid %u (HW %u)", p_acl->rule.l2.tpid_1_vld, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_1_VLD_MASK, &value);
	printk("\tTPID_1 Valid Mask %u (HW %u)\n",
			p_acl->rule.l2.tpid_1_vld_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_ENC_1, &value);
	printk("\tEncoded TPID_1 %u (HW %u)", p_acl->rule.l2.tpid_enc_1, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_ENC_1_MASK, &value);
	printk("\tEncoded TPID_1 Mask %u (HW %u)\n",
			p_acl->rule.l2.tpid_enc_1_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_VID_1, &value);
	printk("\tVID_1 %u (HW %u)", p_acl->rule.l2.vid_1, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_VID_1_MASK, &value);
	printk("\tVID_1 Mask %u (HW %u)\n", p_acl->rule.l2.vid_1_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_8021P_1, &value);
	printk("\t802.1p_1 %u (HW %u)", p_acl->rule.l2._8021p_1, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_8021P_1_MASK, &value);
	printk("\t802.1p_1 Mask %u (HW %u)\n", p_acl->rule.l2._8021p_1_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DEI_1, &value);
	printk("\tDEI_1 %u (HW %u)", p_acl->rule.l2.dei_1, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DEI_1_MASK, &value);
	printk("\tDEI_1 Mask %u (HW %u)\n", p_acl->rule.l2.dei_1_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_2_VLD, &value);
	printk("\tTPID_2 Valid %u (HW %u)", p_acl->rule.l2.tpid_2_vld, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_2_VLD_MASK, &value);
	printk("\tTPID_2 Valid Mask %u (HW %u)\n",
			p_acl->rule.l2.tpid_2_vld_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_ENC_2, &value);
	printk("\tEncoded TPID_2 %u (HW %u)\n", p_acl->rule.l2.tpid_enc_2,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TPID_ENC_2_MASK, &value);
	printk("\tEncoded TPID_2 Mask %u (HW %u)\n",
			p_acl->rule.l2.tpid_enc_2_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_VID_2, &value);
	printk("\tVID_2 %u (HW %u)", p_acl->rule.l2.vid_2, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_VID_2_MASK, &value);
	printk("\tVID_2 Mask %u (HW %u)\n", p_acl->rule.l2.vid_2_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_8021P_2, &value);
	printk("\t802.1p_2 %u (HW %u)", p_acl->rule.l2._8021p_2, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_8021P_2_MASK, &value);
	printk("\t802.1p_2 Mask %u (HW %u)\n", p_acl->rule.l2._8021p_2_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DEI_2, &value);
	printk("\tDEI_2 %u (HW %u)", p_acl->rule.l2.dei_2, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DEI_2_MASK, &value);
	printk("\tDEI_2 Mask %u (HW %u)\n", p_acl->rule.l2.dei_2_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DA_AN_MAC_HIT, &value);
	printk("\tDA AN MAC Hit %u (HW %u)", p_acl->rule.l2.da_an_mac_hit,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DA_AN_MAC_HIT_MASK, &value);
	printk("\tDA AN MAC Hit Mask %u (HW %u)\n",
			p_acl->rule.l2.da_an_mac_hit_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DA_AN_MAC_SEL, &value);
	printk("\tDA AN MAC SEL %u (HW %u)", p_acl->rule.l2.da_an_mac_sel,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DA_AN_MAC_SEL_MASK, &value);
	printk("\tDA AN MAC SEL Mask %u (HW %u)\n",
			p_acl->rule.l2.da_an_mac_sel_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SA_BNG_MAC_SEL, &value);
	printk("\tSA BNG MAC SEL %u (HW %u)", p_acl->rule.l2.sa_bng_mac_sel,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SA_BNG_MAC_SEL_MASK, &value);
	printk("\tSA BNG MAC SEL Mask %u (HW %u)\n",
			p_acl->rule.l2.sa_bng_mac_sel_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SA_BNG_MAC_HIT, &value);
	printk("\tSA BNG MAC Hit %u (HW %u)", p_acl->rule.l2.sa_bng_mac_hit,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SA_BNG_MAC_HIT_MASK, &value);
	printk("\tSA BNG MAC Hit Mask %u (HW %u)\n",
			p_acl->rule.l2.sa_bng_mac_hit_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPPOE_SESSION_ID_VLD, &value);
	printk("\tPPPoE Session ID Valid %u (HW %u)\n",
			p_acl->rule.l2.pppoe_session_id_vld, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPPOE_SESSION_ID_VLD_MASK, &value);
	printk("\tPPPoE Session ID Valid Mask %u (HW %u)\n",
			p_acl->rule.l2.pppoe_session_id_vld_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPPOE_SESSION_ID, &value);
	printk("\tPPPoE Session ID 0x%04x (HW 0x%04x)\n",
			p_acl->rule.l2.pppoe_session_id, (__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPPOE_SESSION_ID_MASK, &value);
	printk("\tPPPoE Session ID Mask %u (HW %u)\n",
			p_acl->rule.l2.pppoe_session_id_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPP_PROTOCOL_VLD, &value);
	printk("\tPPP Protocol Valid %u (HW %u)",
			p_acl->rule.l2.ppp_protocol_vld, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPP_PROTOCOL_VLD_MASK, &value);
	printk("\tPPP Protocol Valid Mask %u (HW %u)\n",
			p_acl->rule.l2.ppp_protocol_vld_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPP_PROTOCOL, &value);
	printk("\tPPP Protocol 0x%04x (HW 0x%04x)", p_acl->rule.l2.ppp_protocol,
			(__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PPP_PROTOCOL_MASK, &value);
	printk("\tPPP Protocol Mask %u (HW %u)\n",
			p_acl->rule.l2.ppp_protocol_mask, value);

	/* L3 rule */
	printk("    |- L3 Rule:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_VLD, &value);
	printk("\tIP Valid %u (HW %u)", p_acl->rule.l3.ip_vld, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_VLD_MASK, &value);
	printk("\tIP Valid Mask %u (HW %u)\n", p_acl->rule.l3.ip_vld_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_VER, &value);
	printk("\tIP Ver %u (HW %u)", p_acl->rule.l3.ip_ver, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_VER_MASK, &value);
	printk("\tIP Ver Mask %u (HW %u)\n", p_acl->rule.l3.ip_ver_mask, value);

	memset(value_ip, 0, 16);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_DA, value_ip);
	printk("\tIP DA %x.%x.%x.%x (HW %x.%x.%x.%x)\n",
		(__u32) p_acl->rule.l3.da[0], (__u32) p_acl->rule.l3.da[1],
		(__u32) p_acl->rule.l3.da[2], (__u32) p_acl->rule.l3.da[3],
		(__u32) value_ip[0], (__u32) value_ip[1],
		(__u32) value_ip[2], (__u32) value_ip[3]);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_DA_MASK, &value);
	printk("\tIP DA Mask 0x%03x (HW 0x%03x)\n", p_acl->rule.l3.ip_da_mask,
			value & 0x1FF);

	memset(value_ip, 0, 16);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_SA, value_ip);
	printk("\tIP SA %u.%u.%u.%u (HW %u.%u.%u.%u)\n",
		(__u8) p_acl->rule.l3.sa[0], (__u8) p_acl->rule.l3.sa[1],
		(__u8) p_acl->rule.l3.sa[2], (__u8) p_acl->rule.l3.sa[3],
		(__u8) value_ip[0], (__u8) value_ip[1],
		(__u8) value_ip[2], (__u8) value_ip[3]);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_SA_MASK, &value);
	printk("\tIP SA Mask 0x%03x (HW 0x%03x)\n", p_acl->rule.l3.ip_sa_mask,
			value & 0x1FF);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DSCP, &value);
	printk("\tDSCP 0x%02x (HW 0x%02x)", p_acl->rule.l3.dscp, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_DSCP_MASK, &value);
	printk("\tDSCP Mask 0x%02x (HW 0x%02x)\n", p_acl->rule.l3.dscp_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_ECN, &value);
	printk("\tECN %u (HW %u)", p_acl->rule.l3.ecn, (__u8) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_ECN_MASK, &value);
	printk("\tECN Mask %u (HW %u)\n", p_acl->rule.l3.ecn_mask,
			(__u8) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_PROT, &value);
	printk("\tProtocol %u (HW %u)", p_acl->rule.l3.proto, (__u8) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_PROT_MASK, &value);
	printk("\tProtocol Mask %u (HW %u)\n", p_acl->rule.l3.ip_proto_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_FRAGMENT, &value);
	printk("\tFrag %u (HW %u)", p_acl->rule.l3.fragment, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_FRAGMENT_MASK, &value);
	printk("\tFrag Mask %u (HW %u)\n", p_acl->rule.l3.ip_fragment_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_OPTIONS, &value);
	printk("\tOptions %u (HW %u)", p_acl->rule.l3.options, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IP_OPTIONS_MASK, &value);
	printk("\tOptions Mask %u (HW %u)\n", p_acl->rule.l3.ip_options_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_FLOW_LBL, &value);
	printk("\tIPv6 Flow Label 0x%x (HW 0x%x)",
			p_acl->rule.l3.ipv6_flow_label, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_FLOW_LBL_MASK, &value);
	printk("\tIPv6 Flow Level Mask %u (HW %u)\n",
			p_acl->rule.l3.ipv6_flow_label_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TTL_HOPLMT, &value);
	printk("\tTTL/HOP Limit %u (HW %u)", p_acl->rule.l3.ttl_hoplimit,
			(__u8) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_TTL_HOPLMT_MASK, &value);
	printk("\tTTL/Hop Limit Mask %u (HW %u)\n",
			p_acl->rule.l3.ttl_hoplimit_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPI_VLD, &value);
	printk("\tSPI Valid %u (HW %u)", p_acl->rule.l3.spi_vld, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPI_VLD_MASK, &value);
	printk("\tSPI Valid Mask %u (HW %u)\n", p_acl->rule.l3.spi_vld_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPI, &value);
	printk("\tSPI 0x%08x (HW 0x%08x)", p_acl->rule.l3.spi, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPI_MASK, &value);
	printk("\tSPI Mask %u (HW %u)\n", p_acl->rule.l3.spi_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_NDP, &value);
	printk("\tIPv6 NDP %u (HW %u)", p_acl->rule.l3.ipv6_ndp, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_NDP_MASK, &value);
	printk("\tIPv6 NDP Mask %u (HW %u)\n", p_acl->rule.l3.ipv6_ndp_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_HBH, &value);
	printk("\tIPv6 HBH %u (HW %u)", p_acl->rule.l3.ipv6_hbh, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_HBH_MASK, &value);
	printk("\tIPv6 HBH Mask %u (HW %u)\n", p_acl->rule.l3.ipv6_hbh_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_RH, &value);
	printk("\tIPv6 RH %u (HW %u)", p_acl->rule.l3.ipv6_rh, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_RH_MASK, &value);
	printk("\tIPv6 RH Mask %u (HW %u)\n", p_acl->rule.l3.ipv6_rh_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_DOH, &value);
	printk("\tIPv6 DOH %u (HW %u)", p_acl->rule.l3.ipv6_doh, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_IPV6_DOH_MASK, &value);
	printk("\tIPv6 DOH Mask %u (HW %u)\n", p_acl->rule.l3.ipv6_doh_mask,
			value);

	/* L4 rule */
	printk("    |- L4 Rule:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_VLD, &value);
	printk("\tL4 Valid %u (HW %u)", p_acl->rule.l4.l4_valid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_VLD_MASK, &value);
	printk("\tL4 Valid Mask %u (HW %u)\n", p_acl->rule.l4.l4_valid_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_DPLO, &value);
	printk("\tDP Lo 0x%04x (HW 0x%04x)", p_acl->rule.l4.dp_lo,
			(__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_DPHI, &value);
	printk("\tDP Hi 0x%04x (HW 0x%04x)\n", p_acl->rule.l4.dp_hi,
			(__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_SPLO, &value);
	printk("\tSP Lo 0x%04x (HW 0x%04x)", p_acl->rule.l4.sp_lo,
			(__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_SPHI, &value);
	printk("\tSP Hi 0x%04x (HW 0x%04x)\n", p_acl->rule.l4.sp_hi,
			(__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L4_MASK, &value);
	printk("\tL4 Mask %u (HW %u)\n", p_acl->rule.l4.l4_mask, (__u8) value);


	/* Misc rule */
	printk("    |- Misc Rule:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LSPID, &value);
	printk("\tLSPID %u (HW %u)", p_acl->rule.misc.lspid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LSPID_MASK, &value);
	printk("\tLSPID Mask %u (HW %u)\n", p_acl->rule.misc.lspid_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_ORIG_LSPID, &value);
	printk("\tOrig LSPID %u (HW %u)", p_acl->rule.misc.orig_lspid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_ORIG_LSPID_MASK, &value);
	printk("\tOrig LSPID Mask %u (HW %u)\n",
			p_acl->rule.misc.orig_lspid_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FWDTYPE, &value);
	printk("\tFWD Type %u (HW %u)", p_acl->rule.misc.fwd_type, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FWDTYPE_MASK, &value);
	printk("\tFWD Type Mask 0x%x (HW 0x%x)\n",
			p_acl->rule.misc.fwd_type_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPL_PKT_VEC, &value);
	printk("\tSPL PKT VEC 0x%04x (HW 0x%04x)\n",
			p_acl->rule.misc.spl_pkt_vec, (__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPL_PKT_VEC_MASK, &value);
	printk("\tSPL PKT VEC Mask 0x%04x (HW 0x%04x)\n",
			p_acl->rule.misc.spl_pkt_vec_mask, (__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_CLASS_HIT, &value);
	printk("\tClass Hit %u (HW %u)", p_acl->rule.misc.class_hit, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_CLASS_HIT_MASK, &value);
	printk("\tClass Hit Mask %u (HW %u)\n", p_acl->rule.misc.class_hit_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_CLASS_SVIDX, &value);
	printk("\tClass Idx %u (HW %u)", p_acl->rule.misc.class_svidx, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_CLASS_SVIDX_MASK, &value);
	printk("\tClass Idx Mask %u (HW %u)\n",
			p_acl->rule.misc.class_svidx_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LPM_HIT, &value);
	printk("\tLPM Hit %u (HW %u)", p_acl->rule.misc.lpm_hit, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LPM_HIT_MASK, &value);
	printk("\tLPM Hit Mask %u (HW %u)\n", p_acl->rule.misc.lpm_hit_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LPM_HIT_IDX, &value);
	printk("\tLPM Hit Idx %u (HW %u)", p_acl->rule.misc.lpm_hit_idx, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_LPM_HIT_IDX_MASK, &value);
	printk("\tLPM Hit Idx Mask %u (HW %u)\n",
			p_acl->rule.misc.lpm_hit_idx_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_HASH_HIT, &value);
	printk("\tHash Hit %u (HW %u)", p_acl->rule.misc.hash_hit, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_HASH_HIT_MASK, &value);
	printk("\tHash Hit Mask %u (HW %u)\n", p_acl->rule.misc.hash_hit_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_HASH_HIT_IDX, &value);
	printk("\tHash Hit Idx %u (HW %u)", p_acl->rule.misc.hash_hit_idx,
			(__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_HASH_HIT_IDX_MASK, &value);
	printk("\tHash Hit Idx Mask %u (HW %u)\n",
			p_acl->rule.misc.hash_hit_idx_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L7_FIELD, &value);
	printk("\tL7 Field 0x%08x (HW 0x%08x)", p_acl->rule.misc.l7_field,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_L7_FIELD_MASK, &value);
	printk("\tL7 Field Mask %u (HW %u)\n", p_acl->rule.misc.l7_field_mask,
			(__u8) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FLAGS_VEC, &value);
	printk("\tFlags VEC 0x%04x (HW 0x%04x)", p_acl->rule.misc.flags_vec,
			(__u16) value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FLAGS_VEC_MASK, &value);
	printk("\tFlags VEC Mask 0x%04x (HW 0x%04x)\n",
			p_acl->rule.misc.flags_vec_mask, (__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FLAGS_VEC_OR, &value);
	printk("\tFlags VEC Or %u (HW %u)\n", p_acl->rule.misc.flags_vec_or,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SPL_PKT_VEC_OR, &value);
	printk("\tSPL Pkt VEC Or %u (HW %u)\n", p_acl->rule.misc.spl_pkt_vec_or,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_RECIRC_IDX, &value);
	printk("\tRecirc Idx %u (HW %u)", p_acl->rule.misc.recirc_idx, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_RECIRC_IDX_MASK, &value);
	printk("\tRecirc Idx Mask %u (HW %u)\n",
			p_acl->rule.misc.recirc_idx_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_NE_VEC, &value);
	printk("\tNE VEC 0x%04x (HW 0x%04x)\n", p_acl->rule.misc.ne_vec,
			(__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_MCIDX, &value);
	printk("\tMCIDX %u (HW %u)", p_acl->rule.misc.mc_idx, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_MCIDX_MASK, &value);
	printk("\tMCIDX Mask %u (HW %u)\n", p_acl->rule.misc.mc_idx_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SDB_DROP, &value);
	printk("\tSDB Drop %u (HW %u)", p_acl->rule.misc.sdb_drop, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_SDB_DROP_MASK, &value);
	printk("\tSDB Drop Mask %u (HW %u)\n", p_acl->rule.misc.sdb_drop_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FWD_DROP, &value);
	printk("\tFWD Drop %u (HW %u)", p_acl->rule.misc.fwd_drop, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_FWD_DROP_MASK, &value);
	printk("\tFWD Drop Mask %u (HW %u)\n", p_acl->rule.misc.fwd_drop_mask,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PKTLEN_RNG_MATCH_VECTOR, &value);
	printk("\tPkt Range VEC %u (HW %u)", p_acl->rule.misc.pktlen_rng_vec,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_PKTLEN_RNG_MATCH_VECTOR_MASK, &value);
	printk("\tPkt Range VEC Mask %u (HW %u)\n",
			p_acl->rule.misc.pktlen_rng_vec_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_RULE, idx,
			ACL_RULE_RSVD_879_878, &value);
	printk("\tReserved Bits %u (HW %u)\n", p_acl->rule.misc.rsvd_879_878,
			value);

	/* Action */
	printk("  |- Action:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MEM_PARITY, &value);
	printk("\tParity %u (HW %u)\n", p_acl->action.parity, value);


	/* L2 action */
	printk("    |- L2 Action:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_8021P_1, &value);
	printk("\t802.1p_1 %u (HW %u)", p_acl->action.l2._8021p_1, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_8021P_1_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l2._8021p_1_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_8021P_1_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2._8021p_1_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CHANGE_8021P_1_EN, &value);
	printk("\tChange 802.1p_1 Enable %u (HW %u)\n",
			p_acl->action.l2.change_8021p_1_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DEI_1, &value);
	printk("\tDEI_1 %u (HW %u)", p_acl->action.l2.dei_1, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DEI_1_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l2.dei_1_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DEI_1_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2.dei_1_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CHANGE_DEI_1_EN, &value);
	printk("\tChange DEI_1 Enable %u (HW %u)\n",
			p_acl->action.l2.change_dei_1_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_8021P_2, &value);
	printk("\t802.1p_2 %u (HW %u)", p_acl->action.l2._8021p_2, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_8021P_2_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l2._8021p_2_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_8021P_2_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2._8021p_2_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CHANGE_8021P_2_EN, &value);
	printk("\tChange 802.1p_2 Enable %u (HW %u)\n",
			p_acl->action.l2.change_8021p_2_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DEI_2, &value);
	printk("\tDEI_2 %u (HW %u)", p_acl->action.l2.dei_2, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DEI_2_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l2.dei_2_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DEI_2_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2.dei_2_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CHANGE_DEI_2_EN, &value);
	printk("\tChange DEI_2 Enable %u (HW %u)\n",
			p_acl->action.l2.change_dei_2_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FIRST_VLAN_CMD, &value);
	printk("\t1st VLAN Cmd %u (HW %u)", p_acl->action.l2.first_vlan_cmd,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FIRST_VLAN_CMD_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l2.first_vlan_cmd_vld,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FIRST_VLAN_CMD_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2.first_vlan_cmd_pri,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FIRST_VID, &value);
	printk("\t1st VID %u (HW %u)", p_acl->action.l2.first_vid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FIRST_TPID_ENC, &value);
	printk("\t1st TPID ENC %u (HW %u)\n", p_acl->action.l2.first_tpid_enc,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_SECOND_VLAN_CMD, &value);
	printk("\t2nd VLAN Cmd %u (HW %u)", p_acl->action.l2.second_vlan_cmd,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_SECOND_VLAN_CMD_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l2.second_vlan_cmd_vld,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_SECOND_VLAN_CMD_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2.second_vlan_cmd_pri,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_SECOND_VID, &value);
	printk("\t2nd VID %u (HW %u)", p_acl->action.l2.second_vid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_SECOND_TPID_ENC, &value);
	printk("\t2nd TPID ENC %u (HW %u)\n", p_acl->action.l2.second_tpid_enc,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MAC_DA_SA_REPLACE_EN_VLD, &value);
	printk("\tMAC DA SA Replace Enable Valid %u (HW %u)",
			p_acl->action.l2.mac_da_sa_replace_en_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MAC_DA_SA_REPLACE_EN_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l2.mac_da_sa_replace_en_pri,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MAC_DA_REPLACE_EN, &value);
	printk("\tMAC DA Replace Enable %u (HW %u)",
			p_acl->action.l2.mac_da_replace_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MAC_SA_REPLACE_EN, &value);
	printk("\tMAC SA Replace Enable %u (HW %u)\n",
			p_acl->action.l2.mac_sa_replace_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L2_IDX, &value);
	printk("\tL2 Idx %u (HW %u)\n", p_acl->action.l2.l2_idx, value & 0x1FF);



	/* L3 action */
	printk("    |- L3 Action:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DSCP, &value);
	printk("\tDSCP %u (HW %u)", p_acl->action.l3.dscp, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DSCP_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l3.dscp_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DSCP_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l3.dscp_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CHANGE_DSCP_EN, &value);
	printk("\tChange DSCP Enable %u (HW %u)\n",
			p_acl->action.l3.change_dscp_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_SA_REPLACE_EN, &value);
	printk("\tIP SA Replace Enable %u (HW %u)",
			p_acl->action.l3.ip_sa_replace_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_SA_REPLACE_EN_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l3.ip_sa_replace_en_vld,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_SA_REPLACE_EN_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l3.ip_sa_replace_en_pri,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_SA_IDX, &value);
	printk("\tIP SA Idx %u (HW %u)\n", p_acl->action.l3.ip_sa_idx,
			value & 0xFFF);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_DA_REPLACE_EN, &value);
	printk("\tIP DA Replace Enable %u (HW %u)",
			p_acl->action.l3.ip_da_replace_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_DA_REPLACE_EN_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l3.ip_da_replace_en_vld,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_DA_REPLACE_EN_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l3.ip_da_replace_en_pri,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_IP_DA_IDX, &value);
	printk("\tIP DA Idx %u (HW %u)\n", p_acl->action.l3.ip_da_idx,
			value & 0xFFF);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DECR_TTL_HOPLIMIT, &value);
	printk("\tDecrement TTL/Hop Limit %u (HW %u)",
			p_acl->action.l3.decr_ttl_hoplimit, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DECR_TTL_HOPLIMIT_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l3.decr_ttl_hoplimit_vld,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DECR_TTL_HOPLIMIT_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l3.decr_ttl_hoplimit_pri,
			value);


	/* L4 Action */
	printk("    |- L4 Action:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_SP_REPLACE_EN, &value);
	printk("\tIP SP Replace Enable %u (HW %u)",
			p_acl->action.l4.sp_replace_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_SP_REPLACE_EN_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l4.sp_replace_en_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_SP_REPLACE_EN_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l4.sp_replace_en_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_SP, &value);
	printk("\tIP SP %u (HW %u)\n", p_acl->action.l4.sp, (__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_DP_REPLACE_EN, &value);
	printk("\tIP DP Replace Enable %u (HW %u)",
			p_acl->action.l4.dp_replace_en, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_DP_REPLACE_EN_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.l4.dp_replace_en_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_DP_REPLACE_EN_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.l4.dp_replace_en_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_L4_DP, &value);
	printk("\tIP DP %u (HW %u)\n", p_acl->action.l4.dp, (__u16) value);


	/* Misc action */
	printk("    |- Misc Action:\n");
	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_VOQ, &value);
	printk("\tVoQ %u (HW %u)", p_acl->action.misc.voq, (__u8) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_VOQ_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.voq_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_VOQ_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.voq_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_VOQ_CPUPID, &value);
	printk("\tCPU Port ID %u (HW %u)\n",
			p_acl->action.misc.voq_cpupid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_LDPID, &value);
	printk("\tLDPID %u (HW %u)\n", p_acl->action.misc.ldpid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CPUCOPY_CPUPID, &value);
	printk("\tCPU Copy CPU Port ID %u (HW %u)\n",
			p_acl->action.misc.cpucopy_cpupid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CPUCOPY, &value);
	printk("\tCPU Copy %u (HW %u)", p_acl->action.misc.cpucopy, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CPUCOPY_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.cpucopy_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_CPUCOPY_VOQ, &value);
	printk("\tCPU Copy VoQ %u (HW %u)\n", p_acl->action.misc.cpucopy_voq,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MIRROR_ID, &value);
	printk("\tMirror ID %u (HW %u)", p_acl->action.misc.mirror_id,
			(__u16) value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MIRROR_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.mirror_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MIRROR_ID_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.mirror_id_pri,
			value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_WRED_COS, &value);
	printk("\tWRED COS %u (HW %u)", p_acl->action.misc.wred_cos, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_WRED_COS_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.wred_cos_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_WRED_COS_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.wred_cos_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_PRE_MARK, &value);
	printk("\tPre-mark %u (HW %u)", p_acl->action.misc.pre_mark, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_PRE_MARK_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.pre_mark_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_PRE_MARK_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.pre_mark_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_POLICER_ID, &value);
	printk("\tPolicer ID %u (HW %u)", p_acl->action.misc.policer_id,
			value & 0xFF);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_POLICER_ID_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.policer_id_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_POLICER_ID_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.policer_id_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DROP_PERMIT_VLD, &value);
	printk("\tDrop/Permit Valid %u (HW %u)",
			p_acl->action.misc.drop_permit_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DROP_PERMIT_PRI, &value);
	printk("\tDrop/Permit Pri %u (HW %u)\n",
			p_acl->action.misc.drop_permit_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_DROP, &value);
	printk("\tDrop %u (HW %u)", p_acl->action.misc.drop, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_PERMIT, &value);
	printk("\tPermit %u (HW %u)\n", p_acl->action.misc.permit, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_KEEP_TS, &value);
	printk("\tKeep TS %u (HW %u)", p_acl->action.misc.keep_ts, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_KEEP_TS_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.keep_ts_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_KEEP_TS_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.keep_ts_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FWDTYPE, &value);
	printk("\tFwd Type %u (HW %u)", p_acl->action.misc.fwdtype, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FWDTYPE_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.fwdtype_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_FWDTYPE_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.fwdtype_pri, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MCGID, &value);
	printk("\tMCGID %u (HW %u)", p_acl->action.misc.mcgid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MCGID_VLD, &value);
	printk("\tValid %u (HW %u)", p_acl->action.misc.mcgid_vld, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_ACL_ACTION, idx,
			ACL_ACTION_MCGID_PRI, &value);
	printk("\tPri %u (HW %u)\n", p_acl->action.misc.mcdid_pri, value);

} /* _fe_acl_print_entry */

static void _fe_acl_print_entry_valid(unsigned int idx)
{
	fe_acl_entry_t acl_entry, *p_acl;
	__u32 value;
	__u8 value_mac[6];
	__u32 value_ip[4];
	int status;
	unsigned int count;

	status = fe_acl_get_entry(idx, &acl_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_acl_get_entry_refcnt(idx, &count);

	p_acl = &acl_entry;

	printk("| index: %04d | refcnt: %u\n", idx, count);

	/* Rule */
	printk("  |- ACL Rule:\n");
	PRINT_RULE_ENTRY_U32("\tRule_Valid %u (HW %u)\n",
			p_acl->rule.rule_valid,
			ACL_RULE_VLD, 0x1);

	if (p_acl->rule.rule_valid == 0)
		return;

	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			ACL_RULE_MEM_PARITY, &value);
	printk("\tParity %01x (HW %01x)\n", p_acl->rule.parity, (__u8) value);

	/* L2 rule */
	printk("    |- L2 Rule:\n");
	if ((p_acl->rule.l2.l2_mac_da_mask & 0x3F) != 0x3F) {
		PRINT_RULE_ENTRY_MAC("DA", p_acl->rule.l2.l2_mac_da,
				ACL_RULE_L2_MAC_DA);
		PRINT_RULE_ENTRY_U32("\tDA MAC Mask 0x%x (HW 0x%x)\n",
				p_acl->rule.l2.l2_mac_da_mask,
				ACL_RULE_L2_MAC_DA_MASK, 0x3F);
	}

	if ((p_acl->rule.l2.l2_mac_sa_mask & 0x3F) != 0x3F) {
		PRINT_RULE_ENTRY_MAC("SA", p_acl->rule.l2.l2_mac_sa,
				ACL_RULE_L2_MAC_SA);
		PRINT_RULE_ENTRY_U32("\tSA MAC Mask 0x%x (HW 0x%x)\n",
				p_acl->rule.l2.l2_mac_sa_mask,
				ACL_RULE_L2_MAC_SA_MASK, 0x3F);
	}


	if ((p_acl->rule.l2.ethertype_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tEthertype 0x%04x (HW 0x%04x)",
				p_acl->rule.l2.eth_type,
				ACL_RULE_ETHERTYPE, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tEthertype Mask %u (HW %u)\n",
				p_acl->rule.l2.ethertype_mask,
				ACL_RULE_ETHERTYPE_MASK, 0x1);
	}

	if ((p_acl->rule.l2.len_encoded_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tLEN encoded %u (HW %u)",
				p_acl->rule.l2.len_encoded,
				ACL_RULE_LEN_ENCODED, 0x1);
		PRINT_RULE_ENTRY_U32("\tLEN Encoded Mask %u (HW %u)\n",
				p_acl->rule.l2.len_encoded_mask,
				ACL_RULE_LEN_ENCODED_MASK, 0x1);
	}


	if ((p_acl->rule.l2.tpid_1_vld_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tTPID_1 Valid %u (HW %u)",
				p_acl->rule.l2.tpid_1_vld,
				ACL_RULE_TPID_1_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tTPID_1 Valid Mask %u (HW %u)\n",
				p_acl->rule.l2.tpid_1_vld_mask,
				ACL_RULE_TPID_1_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l2.tpid_enc_1_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tEncoded TPID_1 %u (HW %u)",
				p_acl->rule.l2.tpid_enc_1,
				ACL_RULE_TPID_ENC_1, 0x3);
		PRINT_RULE_ENTRY_U32("\tEncoded TPID_1 Mask %u (HW %u)\n",
				p_acl->rule.l2.tpid_enc_1_mask,
				ACL_RULE_TPID_ENC_1_MASK, 0x1);
	}

	if ((p_acl->rule.l2.vid_1_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tVID_1 %u (HW %u)",
				p_acl->rule.l2.vid_1,
				ACL_RULE_VID_1, 0xFFF);
		PRINT_RULE_ENTRY_U32("\tVID_1 Mask %u (HW %u)\n",
				p_acl->rule.l2.vid_1_mask,
				ACL_RULE_VID_1_MASK, 0x1);
	}

	if ((p_acl->rule.l2._8021p_1_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\t802.1p_1 %u (HW %u)",
				p_acl->rule.l2._8021p_1,
				ACL_RULE_8021P_1, 0x7);
		PRINT_RULE_ENTRY_U32("\t802.1p_1 Mask %u (HW %u)\n",
				p_acl->rule.l2._8021p_1_mask,
				ACL_RULE_8021P_1_MASK, 0x1);
	}

	if ((p_acl->rule.l2.dei_1_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tDEI_1 %u (HW %u)",
				p_acl->rule.l2.dei_1,
				ACL_RULE_DEI_1, 0x1);
		PRINT_RULE_ENTRY_U32("\tDEI_1 Mask %u (HW %u)\n",
				p_acl->rule.l2.dei_1_mask,
				ACL_RULE_DEI_1_MASK, 0x1);
	}

	if ((p_acl->rule.l2.tpid_2_vld_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tTPID_2 Valid %u (HW %u)",
				p_acl->rule.l2.tpid_2_vld,
				ACL_RULE_TPID_2_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tTPID_2 Valid Mask %u (HW %u)\n",
				p_acl->rule.l2.tpid_2_vld_mask,
				ACL_RULE_TPID_2_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l2.tpid_enc_2_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tEncoded TPID_2 %u (HW %u)\n",
				p_acl->rule.l2.tpid_enc_2,
				ACL_RULE_TPID_ENC_2, 0x3);
		PRINT_RULE_ENTRY_U32("\tEncoded TPID_2 Mask %u (HW %u)\n",
				p_acl->rule.l2.tpid_enc_2_mask,
				ACL_RULE_TPID_ENC_2_MASK, 0x1);
	}

	if ((p_acl->rule.l2.vid_2_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tVID_2 %u (HW %u)",
				p_acl->rule.l2.vid_2,
				ACL_RULE_VID_2, 0xFFF);
		PRINT_RULE_ENTRY_U32("\tVID_2 Mask %u (HW %u)\n",
				p_acl->rule.l2.vid_2_mask,
				ACL_RULE_VID_2_MASK, 0x1);
	}

	if ((p_acl->rule.l2._8021p_2_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\t802.1p_2 %u (HW %u)",
				p_acl->rule.l2._8021p_2,
				ACL_RULE_8021P_2, 0x7);
		PRINT_RULE_ENTRY_U32("\t802.1p_2 Mask %u (HW %u)\n",
				p_acl->rule.l2._8021p_2_mask,
				ACL_RULE_8021P_2_MASK, 0x1);
	}

	if ((p_acl->rule.l2.dei_2_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tDEI_2 %u (HW %u)",
				p_acl->rule.l2.dei_2,
				ACL_RULE_DEI_2, 0x1);
		PRINT_RULE_ENTRY_U32("\tDEI_2 Mask %u (HW %u)\n",
				p_acl->rule.l2.dei_2_mask,
				ACL_RULE_DEI_2_MASK, 0x1);
	}

	if ((p_acl->rule.l2.da_an_mac_hit_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tDA AN MAC Hit %u (HW %u)",
				p_acl->rule.l2.da_an_mac_hit,
				ACL_RULE_DA_AN_MAC_HIT, 0x1);
		PRINT_RULE_ENTRY_U32("\tDA AN MAC Hit Mask %u (HW %u)\n",
				p_acl->rule.l2.da_an_mac_hit_mask,
				ACL_RULE_DA_AN_MAC_HIT_MASK, 0x1);
	}

	if ((p_acl->rule.l2.da_an_mac_sel_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tDA AN MAC SEL %u (HW %u)",
				p_acl->rule.l2.da_an_mac_sel,
				ACL_RULE_DA_AN_MAC_SEL, 0xF);
		PRINT_RULE_ENTRY_U32("\tDA AN MAC SEL Mask %u (HW %u)\n",
				p_acl->rule.l2.da_an_mac_sel_mask,
				ACL_RULE_DA_AN_MAC_SEL_MASK, 0x1);
	}

	if ((p_acl->rule.l2.sa_bng_mac_sel_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tSA BNG MAC SEL %u (HW %u)",
				p_acl->rule.l2.sa_bng_mac_sel,
				ACL_RULE_SA_BNG_MAC_SEL_MASK, 0xF);
		PRINT_RULE_ENTRY_U32("\tSA BNG MAC SEL Mask %u (HW %u)\n",
				p_acl->rule.l2.sa_bng_mac_sel_mask,
				ACL_RULE_SA_BNG_MAC_SEL_MASK, 0x1);
	}

	if ((p_acl->rule.l2.sa_bng_mac_hit_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tSA BNG MAC Hit %u (HW %u)",
				p_acl->rule.l2.sa_bng_mac_hit,
				ACL_RULE_SA_BNG_MAC_HIT, 0x1);
		PRINT_RULE_ENTRY_U32("\tSA BNG MAC Hit Mask %u (HW %u)\n",
				p_acl->rule.l2.sa_bng_mac_hit_mask,
				ACL_RULE_SA_BNG_MAC_HIT_MASK, 0x1);
	}

	if ((p_acl->rule.l2.pppoe_session_id_vld_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tPPPoE Session ID Valid %u (HW %u)\n",
				p_acl->rule.l2.pppoe_session_id_vld,
				ACL_RULE_PPPOE_SESSION_ID_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tPPPoE Session ID Valid Mask %u (HW %u)\n",
				p_acl->rule.l2.pppoe_session_id_vld_mask,
				ACL_RULE_PPPOE_SESSION_ID_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l2.pppoe_session_id_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tPPPoE Session ID 0x%04x (HW 0x%04x)\n",
				p_acl->rule.l2.pppoe_session_id,
				ACL_RULE_PPPOE_SESSION_ID, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tPPPoE Session ID Mask %u (HW %u)\n",
				p_acl->rule.l2.pppoe_session_id_mask,
				ACL_RULE_PPPOE_SESSION_ID_MASK, 0x1);
	}

	if ((p_acl->rule.l2.ppp_protocol_vld_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tPPP Protocol Valid %u (HW %u)",
				p_acl->rule.l2.ppp_protocol_vld,
				ACL_RULE_PPP_PROTOCOL_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tPPP Protocol Valid Mask %u (HW %u)\n",
				p_acl->rule.l2.ppp_protocol_vld_mask,
				ACL_RULE_PPP_PROTOCOL_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l2.pppoe_session_id_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tPPP Protocol 0x%04x (HW 0x%04x)",
				p_acl->rule.l2.ppp_protocol,
				ACL_RULE_PPP_PROTOCOL, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tPPP Protocol Mask %u (HW %u)\n",
				p_acl->rule.l2.ppp_protocol_mask,
				ACL_RULE_PPP_PROTOCOL_MASK, 0x1);
	}

	/* L3 rule */
	printk("    |- L3 Rule:\n");
	if ((p_acl->rule.l3.ip_vld_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIP Valid %u (HW %u)",
				p_acl->rule.l3.ip_vld,
				ACL_RULE_IP_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tIP Valid Mask %u (HW %u)\n",
				p_acl->rule.l3.ip_vld_mask,
				ACL_RULE_IP_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ip_ver_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIP Ver %u (HW %u)",
				p_acl->rule.l3.ip_ver,
				ACL_RULE_IP_VER, 0x1);
		PRINT_RULE_ENTRY_U32("\tIP Ver Mask %u (HW %u)\n",
				p_acl->rule.l3.ip_ver_mask,
				ACL_RULE_IP_VER_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ip_da_mask & 0x1FF) != 0) {
		PRINT_RULE_ENTRY_IP("DA", p_acl->rule.l3.da, ACL_RULE_IP_DA);
		PRINT_RULE_ENTRY_U32("\tIP DA Mask 0x%03x (HW 0x%03x)\n",
				p_acl->rule.l3.ip_da_mask,
				ACL_RULE_IP_DA_MASK, 0x1FF);
	}

	if ((p_acl->rule.l3.ip_sa_mask & 0x1FF) != 0) {
		PRINT_RULE_ENTRY_IP("SA", p_acl->rule.l3.sa, ACL_RULE_IP_SA);
		PRINT_RULE_ENTRY_U32("\tIP SA Mask 0x%03x (HW 0x%03x)\n",
				p_acl->rule.l3.ip_sa_mask,
				ACL_RULE_IP_SA_MASK, 0x1FF);
	}

	if ((p_acl->rule.l3.dscp_mask & 0x3F) != 0x3F) {
		PRINT_RULE_ENTRY_U32("\tDSCP 0x%02x (HW 0x%02x)",
				p_acl->rule.l3.dscp,
				ACL_RULE_DSCP, 0x3F);
		PRINT_RULE_ENTRY_U32("\tDSCP Mask 0x%02x (HW 0x%02x)\n",
				p_acl->rule.l3.dscp_mask,
				ACL_RULE_DSCP_MASK, 0x3F);
	}

	if ((p_acl->rule.l3.ecn_mask & 0x3) != 0x3) {
		PRINT_RULE_ENTRY_U32("\tECN %u (HW %u)",
				p_acl->rule.l3.ecn,
				ACL_RULE_ECN, 0x3);
		PRINT_RULE_ENTRY_U32("\tECN Mask %u (HW %u)\n",
				p_acl->rule.l3.ecn_mask,
				ACL_RULE_ECN_MASK, 0x3);
	}

	if ((p_acl->rule.l3.ip_proto_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tProtocol %u (HW %u)",
				p_acl->rule.l3.proto,
				ACL_RULE_IP_PROT, 0xFF);
		PRINT_RULE_ENTRY_U32("\tProtocol Mask %u (HW %u)\n",
				p_acl->rule.l3.ip_proto_mask,
				ACL_RULE_IP_PROT_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ip_fragment_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tFrag %u (HW %u)",
				p_acl->rule.l3.fragment,
				ACL_RULE_IP_FRAGMENT, 0x1);
		PRINT_RULE_ENTRY_U32("\tFrag Mask %u (HW %u)\n",
				p_acl->rule.l3.ip_fragment_mask,
				ACL_RULE_IP_FRAGMENT_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ip_options_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tOptions %u (HW %u)",
				p_acl->rule.l3.options,
				ACL_RULE_IP_OPTIONS, 0x1);
		PRINT_RULE_ENTRY_U32("\tOptions Mask %u (HW %u)\n",
				p_acl->rule.l3.ip_options_mask,
				ACL_RULE_IP_OPTIONS_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ipv6_flow_label_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIPv6 Flow Label 0x%x (HW 0x%x)",
				p_acl->rule.l3.ipv6_flow_label,
				ACL_RULE_IPV6_FLOW_LBL, 0xFFFFF);
		PRINT_RULE_ENTRY_U32("\tIPv6 Flow Level Mask %u (HW %u)\n",
				p_acl->rule.l3.ipv6_flow_label_mask,
				ACL_RULE_IPV6_FLOW_LBL_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ttl_hoplimit_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tTTL/HOP Limit %u (HW %u)",
				p_acl->rule.l3.ttl_hoplimit,
				ACL_RULE_TTL_HOPLMT, 0xFF);
		PRINT_RULE_ENTRY_U32("\tTTL/Hop Limit Mask %u (HW %u)\n",
				p_acl->rule.l3.ttl_hoplimit_mask,
				ACL_RULE_TTL_HOPLMT_MASK, 0x1);
	}

	if ((p_acl->rule.l3.spi_vld_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tSPI Valid %u (HW %u)",
				p_acl->rule.l3.spi_vld,
				ACL_RULE_SPI_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tSPI Valid Mask %u (HW %u)\n",
				p_acl->rule.l3.spi_vld_mask,
				ACL_RULE_SPI_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l3.spi_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tSPI 0x%08x (HW 0x%08x)",
				p_acl->rule.l3.spi,
				ACL_RULE_SPI, 0xFFFFFFFF);
		PRINT_RULE_ENTRY_U32("\tSPI Mask %u (HW %u)\n",
				p_acl->rule.l3.spi_mask,
				ACL_RULE_SPI_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ipv6_ndp_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIPv6 NDP %u (HW %u)",
				p_acl->rule.l3.ipv6_ndp,
				ACL_RULE_IPV6_NDP, 0x1);
		PRINT_RULE_ENTRY_U32("\tIPv6 NDP Mask %u (HW %u)\n",
				p_acl->rule.l3.ipv6_ndp_mask,
				ACL_RULE_IPV6_NDP_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ipv6_hbh_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIPv6 HBH %u (HW %u)",
				p_acl->rule.l3.ipv6_hbh,
				ACL_RULE_IPV6_HBH, 0x1);
		PRINT_RULE_ENTRY_U32("\tIPv6 HBH Mask %u (HW %u)\n",
				p_acl->rule.l3.ipv6_hbh_mask,
				ACL_RULE_IPV6_HBH_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ipv6_rh_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIPv6 RH %u (HW %u)",
				p_acl->rule.l3.ipv6_rh,
				ACL_RULE_IPV6_RH, 0x1);
		PRINT_RULE_ENTRY_U32("\tIPv6 RH Mask %u (HW %u)\n",
				p_acl->rule.l3.ipv6_rh_mask,
				ACL_RULE_IPV6_RH_MASK, 0x1);
	}

	if ((p_acl->rule.l3.ipv6_doh_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tIPv6 DOH %u (HW %u)",
				p_acl->rule.l3.ipv6_doh,
				ACL_RULE_IPV6_DOH, 0x1);
		PRINT_RULE_ENTRY_U32("\tIPv6 DOH Mask %u (HW %u)\n",
				p_acl->rule.l3.ipv6_doh_mask,
				ACL_RULE_IPV6_DOH_MASK, 0x1);
	}

	/* L4 rule */
	printk("    |- L4 Rule:\n");
	if ((p_acl->rule.l4.l4_valid_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tL4 Valid %u (HW %u)",
				p_acl->rule.l4.l4_valid,
				ACL_RULE_L4_VLD, 0x1);
		PRINT_RULE_ENTRY_U32("\tL4 Valid Mask %u (HW %u)\n",
				p_acl->rule.l4.l4_valid_mask,
				ACL_RULE_L4_VLD_MASK, 0x1);
	}

	if ((p_acl->rule.l4.l4_mask & 0x1) != 0x1)
		PRINT_RULE_ENTRY_U32("\tDP Lo 0x%04x (HW 0x%04x)\n",
				p_acl->rule.l4.dp_lo,
				ACL_RULE_L4_DPLO, 0xFFFF);

	if ((p_acl->rule.l4.l4_mask & 0x2) != 0x2)
		PRINT_RULE_ENTRY_U32("\tDP Hi 0x%04x (HW 0x%04x)\n",
				p_acl->rule.l4.dp_hi,
				ACL_RULE_L4_DPHI, 0xFFFF);

	if ((p_acl->rule.l4.l4_mask & 0x4) != 0x4)
		PRINT_RULE_ENTRY_U32("\tSP Lo 0x%04x (HW 0x%04x)\n",
				p_acl->rule.l4.sp_lo,
				ACL_RULE_L4_SPLO, 0xFFFF);

	if ((p_acl->rule.l4.l4_mask & 0x8) != 0x8)
		PRINT_RULE_ENTRY_U32("\tSP Hi 0x%04x (HW 0x%04x)\n",
				p_acl->rule.l4.sp_hi,
				ACL_RULE_L4_SPHI, 0xFFFF);

	if ((p_acl->rule.l4.l4_mask & 0xF) != 0xF)
		PRINT_RULE_ENTRY_U32("\tL4 Mask %u (HW %u)\n",
				p_acl->rule.l4.l4_mask,
				ACL_RULE_L4_MASK, 0xF);

	/* Misc rule */
	printk("    |- Misc Rule:\n");
	if ((p_acl->rule.misc.lspid_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tLSPID %u (HW %u)",
				p_acl->rule.misc.lspid,
				ACL_RULE_LSPID, 0xF);
		PRINT_RULE_ENTRY_U32("\tLSPID Mask %u (HW %u)\n",
				p_acl->rule.misc.lspid_mask,
				ACL_RULE_LSPID_MASK, 0x1);
	}

	if ((p_acl->rule.misc.orig_lspid_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tOrig LSPID %u (HW %u)",
				p_acl->rule.misc.orig_lspid,
				ACL_RULE_ORIG_LSPID, 0xF);
		PRINT_RULE_ENTRY_U32("\tOrig LSPID Mask %u (HW %u)\n",
				p_acl->rule.misc.orig_lspid_mask,
				ACL_RULE_ORIG_LSPID_MASK, 0x1);
	}


	if ((p_acl->rule.misc.fwd_type_mask & 0xF) != 0xF) {
		PRINT_RULE_ENTRY_U32("\tFWD Type %u (HW %u)",
				p_acl->rule.misc.fwd_type,
				ACL_RULE_FWDTYPE, 0xF);
		PRINT_RULE_ENTRY_U32("\tFWD Type Mask 0x%x (HW 0x%x)\n",
				p_acl->rule.misc.fwd_type_mask,
				ACL_RULE_FWDTYPE_MASK, 0xF);
	}

	if ((p_acl->rule.misc.spl_pkt_vec_mask & 0xFFFF) != 0xFFFF) {
		PRINT_RULE_ENTRY_U32("\tSPL PKT VEC 0x%04x (HW 0x%04x)\n",
				p_acl->rule.misc.spl_pkt_vec,
				ACL_RULE_SPL_PKT_VEC, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tSPL PKT VEC Mask 0x%04x (HW 0x%04x)\n",
				p_acl->rule.misc.spl_pkt_vec_mask,
				ACL_RULE_SPL_PKT_VEC_MASK, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tSPL Pkt VEC Or %u (HW %u)\n",
				p_acl->rule.misc.spl_pkt_vec_or,
				ACL_RULE_SPL_PKT_VEC_OR, 0x1);
	}

	if ((p_acl->rule.misc.class_hit_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tClass Hit %u (HW %u)",
				p_acl->rule.misc.class_hit,
				ACL_RULE_CLASS_HIT, 0x1);
		PRINT_RULE_ENTRY_U32("\tClass Hit Mask %u (HW %u)\n",
				p_acl->rule.misc.class_hit_mask,
				ACL_RULE_CLASS_HIT_MASK, 0x1);
	}

	if ((p_acl->rule.misc.class_svidx_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tClass Idx %u (HW %u)",
				p_acl->rule.misc.class_svidx,
				ACL_RULE_CLASS_SVIDX, 0x3F);
		PRINT_RULE_ENTRY_U32("\tClass Idx Mask %u (HW %u)\n",
				p_acl->rule.misc.class_svidx_mask,
				ACL_RULE_CLASS_SVIDX_MASK, 0x1);
	}

	if ((p_acl->rule.misc.lpm_hit_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tLPM Hit %u (HW %u)",
				p_acl->rule.misc.lpm_hit,
				ACL_RULE_LPM_HIT, 0x1);
		PRINT_RULE_ENTRY_U32("\tLPM Hit Mask %u (HW %u)\n",
				p_acl->rule.misc.lpm_hit_mask,
				ACL_RULE_LPM_HIT_MASK, 0x1);
	}

	if ((p_acl->rule.misc.lpm_hit_idx_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tLPM Hit Idx %u (HW %u)",
				p_acl->rule.misc.lpm_hit_idx,
				ACL_RULE_LPM_HIT_IDX, 0x3F);
		PRINT_RULE_ENTRY_U32("\tLPM Hit Idx Mask %u (HW %u)\n",
				p_acl->rule.misc.lpm_hit_idx_mask,
				ACL_RULE_LPM_HIT_IDX_MASK, 0x1);
	}

	if ((p_acl->rule.misc.hash_hit_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tHash Hit %u (HW %u)",
				p_acl->rule.misc.hash_hit,
				ACL_RULE_HASH_HIT, 0x1);
		PRINT_RULE_ENTRY_U32("\tHash Hit Mask %u (HW %u)\n",
				p_acl->rule.misc.hash_hit_mask,
				ACL_RULE_HASH_HIT_MASK, 0x1);
	}

	if ((p_acl->rule.misc.hash_hit_idx_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tHash Hit Idx %u (HW %u)",
				p_acl->rule.misc.hash_hit_idx,
				ACL_RULE_HASH_HIT_IDX, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tHash Hit Idx Mask %u (HW %u)\n",
				p_acl->rule.misc.hash_hit_idx_mask,
				ACL_RULE_HASH_HIT_IDX_MASK, 0x1);
	}

	if ((p_acl->rule.misc.l7_field_mask & 0x7) != 0x7) {
		PRINT_RULE_ENTRY_U32("\tL7 Field 0x%08x (HW 0x%08x)",
				p_acl->rule.misc.l7_field,
				ACL_RULE_L7_FIELD, 0xFFFFFFFF);
		PRINT_RULE_ENTRY_U32("\tL7 Field Mask %u (HW %u)\n",
				p_acl->rule.misc.l7_field_mask,
				ACL_RULE_L7_FIELD_MASK, 0x7);
	}

	if ((p_acl->rule.misc.flags_vec_mask & 0xFFFF) != 0xFFFF) {
		PRINT_RULE_ENTRY_U32("\tFlags VEC 0x%04x (HW 0x%04x)",
				p_acl->rule.misc.flags_vec,
				ACL_RULE_FLAGS_VEC, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tFlags VEC Mask 0x%04x (HW 0x%04x)\n",
				p_acl->rule.misc.flags_vec_mask,
				ACL_RULE_FLAGS_VEC_MASK, 0xFFFF);
		PRINT_RULE_ENTRY_U32("\tFlags VEC Or %u (HW %u)\n",
				p_acl->rule.misc.flags_vec_or,
				ACL_RULE_FLAGS_VEC_OR, 0x1);
	}

	if ((p_acl->rule.misc.recirc_idx_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tRecirc Idx %u (HW %u)",
				p_acl->rule.misc.recirc_idx,
				ACL_RULE_RECIRC_IDX, 0x3FF);
		PRINT_RULE_ENTRY_U32("\tRecirc Idx Mask %u (HW %u)\n",
				p_acl->rule.misc.recirc_idx_mask,
				ACL_RULE_RECIRC_IDX_MASK, 0x1);
	}

	if ((p_acl->rule.misc.ne_vec & 0xFFFF) != 0)
		PRINT_RULE_ENTRY_U32("\tNE VEC 0x%04x (HW 0x%04x)\n",
				p_acl->rule.misc.ne_vec,
				ACL_RULE_NE_VEC, 0xFFFF);

	if ((p_acl->rule.misc.mc_idx_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tMCIDX %u (HW %u)",
				p_acl->rule.misc.mc_idx,
				ACL_RULE_MCIDX, 0x1F);
		PRINT_RULE_ENTRY_U32("\tMCIDX Mask %u (HW %u)\n",
				p_acl->rule.misc.mc_idx_mask,
				ACL_RULE_MCIDX_MASK, 0x1);
	}

	if ((p_acl->rule.misc.sdb_drop_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tSDB Drop %u (HW %u)",
				p_acl->rule.misc.sdb_drop,
				ACL_RULE_SDB_DROP, 0x1);
		PRINT_RULE_ENTRY_U32("\tSDB Drop Mask %u (HW %u)\n",
				p_acl->rule.misc.sdb_drop_mask,
				ACL_RULE_SDB_DROP_MASK, 0x1);
	}

	if ((p_acl->rule.misc.fwd_drop_mask & 0x1) != 0x1) {
		PRINT_RULE_ENTRY_U32("\tFWD Drop %u (HW %u)",
				p_acl->rule.misc.fwd_drop,
				ACL_RULE_FWD_DROP, 0x1);
		PRINT_RULE_ENTRY_U32("\tFWD Drop Mask %u (HW %u)\n",
				p_acl->rule.misc.fwd_drop_mask,
				ACL_RULE_FWD_DROP_MASK, 0x1);
	}

	if ((p_acl->rule.misc.pktlen_rng_vec_mask & 0xF) != 0xF) {
		PRINT_RULE_ENTRY_U32("\tPkt Range VEC %u (HW %u)",
				p_acl->rule.misc.pktlen_rng_vec,
				ACL_RULE_PKTLEN_RNG_MATCH_VECTOR, 0xF);
		PRINT_RULE_ENTRY_U32("\tPkt Range VEC Mask %u (HW %u)\n",
				p_acl->rule.misc.pktlen_rng_vec_mask,
				ACL_RULE_PKTLEN_RNG_MATCH_VECTOR_MASK, 0xF);
	}

	if ((p_acl->rule.misc.rsvd_879_878 & 0x3) != 0x3)
		PRINT_RULE_ENTRY_U32("\tReserved Bits %u (HW %u)\n",
				p_acl->rule.misc.rsvd_879_878,
				ACL_RULE_RSVD_879_878, 0x3);


	/* Action */
	printk("  |- Action:\n");
	PRINT_ACTION_ENTRY_U32("\tParity %u (HW %u)\n",
			p_acl->action.parity,
			ACL_ACTION_MEM_PARITY, 0x1);


	/* L2 action */
	printk("    |- L2 Action:\n");
	if ((p_acl->action.l2._8021p_1_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\t802.1p_1 %u (HW %u)",
				p_acl->action.l2._8021p_1,
				ACL_ACTION_8021P_1, 0x7);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l2._8021p_1_vld,
				ACL_ACTION_8021P_1_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2._8021p_1_pri,
				ACL_ACTION_8021P_1_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tChange 802.1p_1 Enable %u (HW %u)\n",
				p_acl->action.l2.change_8021p_1_en,
				ACL_ACTION_CHANGE_8021P_1_EN, 0x1);
	}

	if ((p_acl->action.l2.dei_1_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tDEI_1 %u (HW %u)",
				p_acl->action.l2.dei_1,
				ACL_ACTION_DEI_1, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l2.dei_1_vld,
				ACL_ACTION_DEI_1_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2.dei_1_pri,
				ACL_ACTION_DEI_1_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tChange DEI_1 Enable %u (HW %u)\n",
				p_acl->action.l2.change_dei_1_en,
				ACL_ACTION_CHANGE_DEI_1_EN, 0x1);
	}

	if ((p_acl->action.l2._8021p_2_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\t802.1p_2 %u (HW %u)",
				p_acl->action.l2._8021p_2,
				ACL_ACTION_8021P_2, 0x7);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l2._8021p_2_vld,
				ACL_ACTION_8021P_2_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2._8021p_2_pri,
				ACL_ACTION_8021P_2_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tChange 802.1p_2 Enable %u (HW %u)\n",
				p_acl->action.l2.change_8021p_2_en,
				ACL_ACTION_CHANGE_8021P_2_EN, 0x1);
	}

	if ((p_acl->action.l2.dei_2_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tDEI_2 %u (HW %u)",
				p_acl->action.l2.dei_2,
				ACL_ACTION_DEI_2, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l2.dei_2_vld,
				ACL_ACTION_DEI_2_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2.dei_2_pri,
				ACL_ACTION_DEI_2_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tChange DEI_2 Enable %u (HW %u)\n",
				p_acl->action.l2.change_dei_2_en,
				ACL_ACTION_CHANGE_DEI_2_EN, 0x1);
	}

	if ((p_acl->action.l2.first_vlan_cmd_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\t1st VLAN Cmd %u (HW %u)",
				p_acl->action.l2.first_vlan_cmd,
				ACL_ACTION_FIRST_VLAN_CMD, 0x1F);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l2.first_vlan_cmd_vld,
				ACL_ACTION_FIRST_VLAN_CMD_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2.first_vlan_cmd_pri,
				ACL_ACTION_FIRST_VLAN_CMD_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\t1st VID %u (HW %u)",
				p_acl->action.l2.first_vid,
				ACL_ACTION_FIRST_VID, 0xFFF);
		PRINT_ACTION_ENTRY_U32("\t1st TPID ENC %u (HW %u)\n",
				p_acl->action.l2.first_tpid_enc,
				ACL_ACTION_FIRST_TPID_ENC, 0x3);
	}

	if ((p_acl->action.l2.second_vlan_cmd_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\t2nd VLAN Cmd %u (HW %u)",
				p_acl->action.l2.second_vlan_cmd,
				ACL_ACTION_SECOND_VLAN_CMD, 0x1F);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l2.second_vlan_cmd_vld,
				ACL_ACTION_SECOND_VLAN_CMD_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2.second_vlan_cmd_pri,
				ACL_ACTION_SECOND_VLAN_CMD_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\t2nd VID %u (HW %u)",
				p_acl->action.l2.second_vid,
				ACL_ACTION_SECOND_VID, 0xFFF);
		PRINT_ACTION_ENTRY_U32("\t2nd TPID ENC %u (HW %u)\n",
				p_acl->action.l2.second_tpid_enc,
				ACL_ACTION_SECOND_TPID_ENC, 0x3);
	}

	if ((p_acl->action.l2.mac_da_sa_replace_en_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tMAC DA SA Replace Enable Valid %u (HW %u)",
				p_acl->action.l2.mac_da_sa_replace_en_vld,
				ACL_ACTION_MAC_DA_SA_REPLACE_EN_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l2.mac_da_sa_replace_en_pri,
				ACL_ACTION_MAC_DA_SA_REPLACE_EN_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tMAC DA Replace Enable %u (HW %u)",
				p_acl->action.l2.mac_da_replace_en,
				ACL_ACTION_MAC_DA_REPLACE_EN, 0x1);
		PRINT_ACTION_ENTRY_U32("\tMAC SA Replace Enable %u (HW %u)\n",
				p_acl->action.l2.mac_sa_replace_en,
				ACL_ACTION_MAC_SA_REPLACE_EN, 0x1);
		PRINT_ACTION_ENTRY_U32("\tL2 Idx %u (HW %u)\n",
				p_acl->action.l2.l2_idx,
				ACL_ACTION_L2_IDX, 0x1FF);
	}



	/* L3 action */
	printk("    |- L3 Action:\n");
	if ((p_acl->action.l3.dscp_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tDSCP %u (HW %u)",
				p_acl->action.l3.dscp,
				ACL_ACTION_DSCP, 0x3F);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l3.dscp_vld,
				ACL_ACTION_DSCP_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l3.dscp_pri,
				ACL_ACTION_DSCP_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tChange DSCP Enable %u (HW %u)\n",
				p_acl->action.l3.change_dscp_en,
				ACL_ACTION_CHANGE_DSCP_EN, 0x1);
	}

	if ((p_acl->action.l3.ip_sa_replace_en_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tIP SA Replace Enable %u (HW %u)",
				p_acl->action.l3.ip_sa_replace_en,
				ACL_ACTION_IP_SA_REPLACE_EN, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l3.ip_sa_replace_en_vld,
				ACL_ACTION_IP_SA_REPLACE_EN_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l3.ip_sa_replace_en_pri,
				ACL_ACTION_IP_SA_REPLACE_EN_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tIP SA Idx %u (HW %u)\n",
				p_acl->action.l3.ip_sa_idx,
				ACL_ACTION_IP_SA_IDX, 0xFFF);
	}

	if ((p_acl->action.l3.ip_da_replace_en_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tIP DA Replace Enable %u (HW %u)",
				p_acl->action.l3.ip_da_replace_en,
				ACL_ACTION_IP_DA_REPLACE_EN, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l3.ip_da_replace_en_vld,
				ACL_ACTION_IP_DA_REPLACE_EN_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l3.ip_da_replace_en_pri,
				ACL_ACTION_IP_DA_REPLACE_EN_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tIP DA Idx %u (HW %u)\n",
				p_acl->action.l3.ip_da_idx,
				ACL_ACTION_IP_DA_IDX, 0xFFF);
	}

	if ((p_acl->action.l3.decr_ttl_hoplimit_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tDecrement TTL/Hop Limit %u (HW %u)",
				p_acl->action.l3.decr_ttl_hoplimit,
				ACL_ACTION_DECR_TTL_HOPLIMIT, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l3.decr_ttl_hoplimit_vld,
				ACL_ACTION_DECR_TTL_HOPLIMIT_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l3.decr_ttl_hoplimit_pri,
				ACL_ACTION_DECR_TTL_HOPLIMIT_PRI, 0xF);
	}


	/* L4 Action */
	printk("    |- L4 Action:\n");
	if ((p_acl->action.l4.sp_replace_en_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tIP SP Replace Enable %u (HW %u)",
				p_acl->action.l4.sp_replace_en,
				ACL_ACTION_L4_SP_REPLACE_EN, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l4.sp_replace_en_vld,
				ACL_ACTION_L4_SP_REPLACE_EN_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l4.sp_replace_en_pri,
				ACL_ACTION_L4_SP_REPLACE_EN_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tIP SP %u (HW %u)\n",
				p_acl->action.l4.sp,
				ACL_ACTION_L4_SP, 0xFFFF);
	}

	if ((p_acl->action.l4.dp_replace_en_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tIP DP Replace Enable %u (HW %u)",
				p_acl->action.l4.dp_replace_en,
				ACL_ACTION_L4_DP_REPLACE_EN, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.l4.dp_replace_en_vld,
				ACL_ACTION_L4_DP_REPLACE_EN_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.l4.dp_replace_en_pri,
				ACL_ACTION_L4_DP_REPLACE_EN_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tIP DP %u (HW %u)\n",
				p_acl->action.l4.dp,
				ACL_ACTION_L4_DP, 0xFFFF);
	}


	/* Misc action */
	printk("    |- Misc Action:\n");
	if ((p_acl->action.misc.voq_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tVoQ %u (HW %u)",
				p_acl->action.misc.voq,
				ACL_ACTION_VOQ, 0xFF);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.voq_vld,
				ACL_ACTION_VOQ_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.voq_pri,
				ACL_ACTION_VOQ_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tLDPID %u (HW %u)\n",
				p_acl->action.misc.ldpid,
				ACL_ACTION_LDPID, 0xF);
	}

	if ((p_acl->action.misc.cpucopy & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tCPU Copy CPU Port ID %u (HW %u)\n",
				p_acl->action.misc.cpucopy_cpupid,
				ACL_ACTION_CPUCOPY_CPUPID, 0x7);
		PRINT_ACTION_ENTRY_U32("\tCPU Copy %u (HW %u)",
				p_acl->action.misc.cpucopy,
				ACL_ACTION_CPUCOPY, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.cpucopy_pri,
				ACL_ACTION_CPUCOPY_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tCPU Copy VoQ %u (HW %u)\n",
				p_acl->action.misc.cpucopy_voq,
				ACL_ACTION_CPUCOPY_VOQ, 0x3F);
	}

	if (((p_acl->action.misc.voq_vld & 0x1) == 0x1) ||
		((p_acl->action.misc.cpucopy & 0x1) == 0x1))
		PRINT_ACTION_ENTRY_U32("\tCPU Port ID %u (HW %u)\n",
				p_acl->action.misc.voq_cpupid,
				ACL_ACTION_VOQ_CPUPID, 0x7);

	if ((p_acl->action.misc.mirror_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tMirror ID %u (HW %u)",
				p_acl->action.misc.mirror_id,
				ACL_ACTION_MIRROR_ID, 0x1FF);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.mirror_vld,
				ACL_ACTION_MIRROR_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.mirror_id_pri,
				ACL_ACTION_MIRROR_ID_PRI, 0xF);
	}

	if ((p_acl->action.misc.wred_cos_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tWRED COS %u (HW %u)",
				p_acl->action.misc.wred_cos,
				ACL_ACTION_WRED_COS, 0x7);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.wred_cos_vld,
				ACL_ACTION_WRED_COS_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.wred_cos_pri,
				ACL_ACTION_WRED_COS_PRI, 0xF);
	}

	if ((p_acl->action.misc.pre_mark_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tPre-mark %u (HW %u)",
				p_acl->action.misc.pre_mark,
				ACL_ACTION_PRE_MARK, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.pre_mark_vld,
				ACL_ACTION_PRE_MARK_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.pre_mark_pri,
				ACL_ACTION_PRE_MARK_PRI, 0xF);
	}

	if ((p_acl->action.misc.policer_id_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tPolicer ID %u (HW %u)",
				p_acl->action.misc.policer_id,
				ACL_ACTION_POLICER_ID, 0xFF);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.policer_id_vld,
				ACL_ACTION_POLICER_ID_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.policer_id_pri,
				ACL_ACTION_POLICER_ID_PRI, 0xF);
	}

	if ((p_acl->action.misc.drop_permit_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tDrop/Permit Valid %u (HW %u)",
				p_acl->action.misc.drop_permit_vld,
				ACL_ACTION_DROP_PERMIT_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.drop_permit_pri,
				ACL_ACTION_DROP_PERMIT_PRI, 0xF);
		PRINT_ACTION_ENTRY_U32("\tDrop %u (HW %u)",
				p_acl->action.misc.drop,
				ACL_ACTION_DROP, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPermit %u (HW %u)\n",
				p_acl->action.misc.permit,
				ACL_ACTION_PERMIT, 0x1);
	}

	if ((p_acl->action.misc.keep_ts_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tKeep TS %u (HW %u)",
				p_acl->action.misc.keep_ts,
				ACL_ACTION_KEEP_TS, 0x1);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.keep_ts_vld,
				ACL_ACTION_KEEP_TS_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.keep_ts_pri,
				ACL_ACTION_KEEP_TS_PRI, 0xF);
	}

	if ((p_acl->action.misc.fwdtype_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tFwd Type %u (HW %u)",
				p_acl->action.misc.fwdtype,
				ACL_ACTION_FWDTYPE, 0xF);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.fwdtype_vld,
				ACL_ACTION_FWDTYPE_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.fwdtype_pri,
				ACL_ACTION_FWDTYPE_PRI, 0xF);
	}

	if ((p_acl->action.misc.mcgid_vld & 0x1) == 0x1) {
		PRINT_ACTION_ENTRY_U32("\tMCGID %u (HW %u)",
				p_acl->action.misc.mcgid,
				ACL_ACTION_MCGID, 0x1FF);
		PRINT_ACTION_ENTRY_U32("\tValid %u (HW %u)",
				p_acl->action.misc.mcgid_vld,
				ACL_ACTION_MCGID_VLD, 0x1);
		PRINT_ACTION_ENTRY_U32("\tPri %u (HW %u)\n",
				p_acl->action.misc.mcdid_pri,
				ACL_ACTION_MCGID_PRI, 0xF);
	}



} /* _fe_acl_print_entry */

static void fe_acl_print_entry(unsigned int idx)
{
	if (idx >= FE_ACL_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	printk("\n\n ------------------- ACL Table --------------------\n");
	printk("|------------------------------------------------------\n");

	_fe_acl_print_entry_valid(idx);
} /* fe_acl_print_entry */

static void fe_acl_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_ACL_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	printk("\n\n ------------------- ACL Table --------------------\n");
	printk("|------------------------------------------------------\n");

	for (i = start_idx; i <= end_idx; i++) {
		_fe_acl_print_entry_valid(i);
		cond_resched();
	}

	printk("|------------------------------------------------------\n");
} /* fe_acl_print_range */

static void fe_acl_print_table(void)
{
	unsigned int i;

	printk("\n\n ------------------- ACL Table --------------------\n");
	printk("|------------------------------------------------------\n");

	for (i = 0; i < cs_fe_acl_table_type.max_entry; i++) {
		_fe_acl_print_entry_valid(i);
		cond_resched();
	}

	printk("|------------------------------------------------------\n");
} /* fe_acl_print_table */

static int fe_acl_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_acl_table_type);
} /* fe_acl_get_avail_count */

static cs_fe_table_t cs_fe_acl_table_type = {
	.type_id = FE_TABLE_ACL_RULE,	/* We combine writing to both ACL rule
					 * and action table into 1, since they
					 * are most likely 1-to-1 mapping */
	.max_entry = FE_ACL_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_acl_entry_t),
	.op = {
		.convert_sw_to_hw_data = NULL,
		.alloc_entry = fe_acl_alloc_entry,
		.set_entry = fe_acl_set_entry,
		.add_entry = fe_acl_add_entry,
		.del_entry_by_idx = fe_acl_del_entry_by_idx,
		.del_entry = fe_acl_del_entry,
		.find_entry = fe_acl_find_entry,
		.get_entry = fe_acl_get_entry,
		.inc_entry_refcnt = fe_acl_inc_entry_refcnt,
		.dec_entry_refcnt = fe_acl_dec_entry_refcnt,
		.get_entry_refcnt = fe_acl_get_entry_refcnt,
		.set_field = NULL,	/* ACL does not support set/get a */
		.get_field = NULL,	/* field, because it is controlling */
					/* 2 HW tables at once. */
		.flush_table = fe_acl_flush_table,
		.get_avail_count = fe_acl_get_avail_count,
		.print_entry = fe_acl_print_entry,
		.print_range = fe_acl_print_range,
		.print_table = fe_acl_print_table,
		/* all the rests are NULL */
		.add_l2_mac = NULL,
		.del_l2_mac = NULL,
		.find_l2_mac = NULL,
		.get_l2_mac = NULL,
		.inc_l2_mac_refcnt = NULL,
		.add_l3_ip = NULL,
		.del_l3_ip = NULL,
		.find_l3_ip = NULL,
		.get_l3_ip = NULL,
		.inc_l3_ip_refcnt = NULL,
	},
	.content_table = NULL,
};

int cs_fe_ioctl_acl(struct net_device *dev, void *pdata, void *cmd)
{
	fe_acl_entry_t *p_rslt = (fe_acl_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_acl_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_acl_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_acl_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** CLASS entry 0 ~ %d ***\n", FE_ACL_ENTRY_MAX - 1);
		fe_acl_print_range(fe_cmd_hdr->idx_start,
				fe_cmd_hdr->idx_end);
		break;
	case CMD_REPLACE:	/* ignore */
		break;
	case CMD_INIT:	/* ignore */
		break;
	default:
		return -1;
	}

	return status;
} /* cs_fe_ioctl_acl */

/* this API will initialize ACL Rule/Action table */
int cs_fe_table_acl_init(void)
{
	int ret;

	spin_lock_init(FE_ACL_LOCK);

	cs_fe_acl_table_type.content_table = cs_table_alloc(
			cs_fe_acl_table_type.max_entry);
	if (cs_fe_acl_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_acl_table_type.type_id,
			&cs_fe_acl_table_type);
	if (ret != FE_TABLE_OK) {
		cs_table_dealloc(cs_fe_acl_table_type.content_table);
		return -1;
	}

	/* ACL does not need to reserve a default entry */

	/* FIXME! any other initialization that needs to take place here? */

	return CS_OK;
} /* cs_fe_table_acl_init */
EXPORT_SYMBOL(cs_fe_table_acl_init);

void cs_fe_table_acl_exit(void)
{
	fe_acl_flush_table();

	if (cs_fe_acl_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_acl_table_type.content_table);
	cs_fe_table_unregister(cs_fe_acl_table_type.type_id);
} /* cs_fe_table_acl_exit */
EXPORT_SYMBOL(cs_fe_table_acl_exit);

