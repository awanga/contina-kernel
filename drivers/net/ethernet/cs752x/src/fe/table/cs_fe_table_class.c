/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_class.c
 *
 * $Id: cs_fe_table_class.c,v 1.5 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains Classifier Table Management APIs Implementation.
 */

/* NOTES: IMPORTANT!! Read it before you use the following APIs:
 * The mask in FE Classifier is different than the usual mask.
 * When a mask is set to 0, it is ENABLED.  To disable it, you actually
 * have to set it to 1.  This applies to all the masks besides
 * FE_CLASS_IP_SA_MASK and FE_CLASS_IP_DA_MASK.  Watch out when you
 * use classifier API. */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_class_table_type;

#define FE_CLASS_TABLE_PTR	(cs_fe_class_table_type.content_table)
#define FE_CLASS_LOCK		&(cs_fe_class_table_type.lock)

static void fe_class_init_entry(unsigned int idx)
{
	__u32 value = 0xffffffff;

	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_LSPID_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_LSPID_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_HDR_A_ORIG_LSPID_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_FWDTYPE_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_TPID_ENC_1_MSB_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_TPID_ENC_1_LSB_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_VID_1_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_8021P_1_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_TPID_ENC_2_MSB_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_TPID_ENC_2_LSB_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_VID_2_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_8021P_2_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_DA_AN_MAC_SEL_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_DA_AN_MAC_HIT_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_SA_BNG_MAC_SEL_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_SA_BNG_MAC_HIT_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_DSCP_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_ECN_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_ETHERTYPE_ENC_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_IP_PROT_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_L4_PORT_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_IP_VLD_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_IP_VER_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_IP_FRAGMENT_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_MC_DA_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_BC_DA_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_LEN_ENCODED_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx,
			FE_CLASS_HDR_A_FLAGS_CRCERR_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_L3_CHKSUM_ERR_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_L4_CHKSUM_ERR_MASK,
			&value);
	cs_fe_class_table_type.op.set_field(idx,
			FE_CLASS_NOT_HDR_A_FLAGS_STSVLD_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_SPI_VLD_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_SPI_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_MCGID_MASK, &value);
	cs_fe_class_table_type.op.set_field(idx, FE_CLASS_RSVD_621_606, &value);
} /* fe_class_init_entry */

static int fe_class_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	int ret;

	ret = fe_table_alloc_entry(&cs_fe_class_table_type, rslt_idx,
			start_offset);
	if (ret != FE_TABLE_OK) return ret;

	/* need to initialize the entry properly */
	fe_class_init_entry(*rslt_idx);
	return ret;
} /* fe_class_alloc_entry */

static int fe_class_convert_sw_to_hw_data(void *sw_entry, __u32 *p_data_array,
		unsigned int size)
{
	__u32 value;
	__u8 mac_addr[6], i;
	fe_class_entry_t *p_class = (fe_class_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = p_class->sdb_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SVIDX, &value, p_data_array, size);
	value = p_class->rule_priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_RULE_PRI, &value, p_data_array, size);
	value = p_class->entry_valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_ENTRY_VLD, &value, p_data_array, size);
	value = p_class->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_MEM_PARITY, &value, p_data_array, size);

	/* port */
	value = p_class->port.lspid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_LSPID, &value, p_data_array, size);
	value = p_class->port.hdr_a_orig_lspid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_HDR_A_ORIG_LSPID, &value, p_data_array, size);
	value = p_class->port.fwd_type;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_FWDTYPE, &value, p_data_array, size);
	value = p_class->port.hdr_a_flags_crcerr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_HDR_A_FLAGS_CRCERR, &value, p_data_array,
			size);
	value = p_class->port.l3_csum_err;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L3_CHKSUM_ERR, &value, p_data_array, size);
	value = p_class->port.l4_csum_err;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_CHKSUM_ERR, &value, p_data_array, size);
	value = p_class->port.not_hdr_a_flags_stsvld;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_NOT_HDR_A_FLAGS_STSVLD, &value, p_data_array,
			size);
	value = p_class->port.lspid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_LSPID_MASK, &value, p_data_array, size);
	value = p_class->port.hdr_a_orig_lspid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_HDR_A_ORIG_LSPID_MASK, &value, p_data_array,
			size);
	value = p_class->port.fwd_type_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_FWDTYPE_MASK, &value, p_data_array, size);
	value = p_class->port.hdr_a_flags_crcerr_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_HDR_A_FLAGS_CRCERR_MASK, &value, p_data_array,
			size);
	value = p_class->port.l3_csum_err_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L3_CHKSUM_ERR_MASK, &value, p_data_array,
			size);
	value = p_class->port.l4_csum_err_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_CHKSUM_ERR_MASK, &value, p_data_array,
			size);
	value = p_class->port.not_hdr_a_flags_stsvld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_NOT_HDR_A_FLAGS_STSVLD_MASK, &value,
			p_data_array, size);

	/* L2 */
	value = p_class->l2.tpid_enc_1;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_TPID_ENC_1, &value, p_data_array, size);
	value = p_class->l2.vid_1;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_VID_1, &value, p_data_array, size);
	value = p_class->l2._8021p_1;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_8021P_1, &value, p_data_array, size);
	value = p_class->l2.tpid_enc_2;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_TPID_ENC_2, &value, p_data_array, size);
	value = p_class->l2.vid_2;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_VID_2, &value, p_data_array, size);
	value = p_class->l2._8021p_2;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_8021P_2, &value, p_data_array, size);
	value = p_class->l2.tpid_enc_1_msb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_TPID_ENC_1_MSB_MASK, &value, p_data_array,
			size);
	value = p_class->l2.tpid_enc_1_lsb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_TPID_ENC_1_LSB_MASK, &value, p_data_array,
			size);
	value = p_class->l2.vid_1_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_VID_1_MASK, &value, p_data_array, size);
	value = p_class->l2._8021p_1_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_8021P_1_MASK, &value, p_data_array, size);
	value = p_class->l2.tpid_enc_2_msb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_TPID_ENC_2_MSB_MASK, &value, p_data_array,
			size);
	value = p_class->l2.tpid_enc_2_lsb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_TPID_ENC_2_LSB_MASK, &value, p_data_array,
			size);
	value = p_class->l2.vid_2_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_VID_2_MASK, &value, p_data_array, size);
	value = p_class->l2._8021p_2_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_8021P_2_MASK, &value, p_data_array, size);
	value = p_class->l2.da_an_mac_sel;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DA_AN_MAC_SEL, &value, p_data_array, size);
	value = p_class->l2.da_an_mac_hit;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DA_AN_MAC_HIT, &value, p_data_array, size);
	value = p_class->l2.sa_bng_mac_sel;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SA_BNG_MAC_SEL, &value, p_data_array, size);
	value = p_class->l2.sa_bng_mac_hit;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SA_BNG_MAC_HIT, &value, p_data_array, size);
	value = p_class->l2.da_an_mac_sel_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DA_AN_MAC_SEL_MASK, &value, p_data_array,
			size);
	value = p_class->l2.da_an_mac_hit_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DA_AN_MAC_HIT_MASK, &value, p_data_array,
			size);
	value = p_class->l2.sa_bng_mac_sel_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SA_BNG_MAC_SEL_MASK, &value, p_data_array,
			size);
	value = p_class->l2.sa_bng_mac_hit_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SA_BNG_MAC_HIT_MASK, &value, p_data_array,
			size);
	value = p_class->l2.ethertype_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_ETHERTYPE_ENC, &value, p_data_array, size);
	value = p_class->l2.ethertype_enc_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_ETHERTYPE_ENC_MASK, &value, p_data_array,
			size);
	for (i = 0; i < 6; i++) mac_addr[0] = p_class->l2.da[0];
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DA, (__u32*)p_class->l2.da, p_data_array,
			size);
	for (i = 0; i < 6; i++) mac_addr[0] = p_class->l2.sa[0];
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SA, (__u32*)p_class->l2.sa, p_data_array,
			size);
	value = p_class->l2.da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DA_MASK, &value, p_data_array, size);
	value = p_class->l2.sa_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SA_MASK, &value, p_data_array, size);
	value = p_class->l2.mcast_da;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_MC_DA, &value, p_data_array, size);
	value = p_class->l2.bcast_da;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_BC_DA, &value, p_data_array, size);
	value = p_class->l2.mcast_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_MC_DA_MASK, &value, p_data_array, size);
	value = p_class->l2.bcast_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_BC_DA_MASK, &value, p_data_array, size);
	value = p_class->l2.len_encoded;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_LEN_ENCODED, &value, p_data_array, size);
	value = p_class->l2.len_encoded_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_LEN_ENCODED_MASK, &value, p_data_array, size);

	/* L3 */
	value = p_class->l3.dscp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DSCP, &value, p_data_array, size);
	value = p_class->l3.ecn;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_ECN, &value, p_data_array, size);
	value = p_class->l3.ip_prot;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_PROT, &value, p_data_array, size);
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_SA, p_class->l3.sa, p_data_array, size);
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_DA, p_class->l3.da, p_data_array, size);
	value = p_class->l3.ip_valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_VLD, &value, p_data_array, size);
	value = p_class->l3.ip_ver;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_VER, &value, p_data_array, size);
	value = p_class->l3.ip_frag;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_FRAGMENT, &value, p_data_array, size);
	value = p_class->l3.dscp_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_DSCP_MASK, &value, p_data_array, size);
	value = p_class->l3.ecn_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_ECN_MASK, &value, p_data_array, size);
	value = p_class->l3.ip_prot_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_PROT_MASK, &value, p_data_array, size);
	value = p_class->l3.ip_sa_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_SA_MASK, &value, p_data_array, size);
	value = p_class->l3.ip_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_DA_MASK, &value, p_data_array, size);
	value = p_class->l3.ip_valid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_VLD_MASK, &value, p_data_array, size);
	value = p_class->l3.ip_ver_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_VER_MASK, &value, p_data_array, size);
	value = p_class->l3.ip_frag_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_IP_FRAGMENT_MASK, &value, p_data_array, size);
	value = p_class->l3.spi;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SPI, &value, p_data_array, size);
	value = p_class->l3.spi_valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SPI_VLD, &value, p_data_array, size);
	value = p_class->l3.spi_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SPI_MASK, &value, p_data_array, size);
	value = p_class->l3.spi_valid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_SPI_VLD_MASK, &value, p_data_array, size);
	value = p_class->port.mcgid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_MCGID, &value, p_data_array, size);
	value = p_class->port.mcgid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_MCGID_MASK, &value, p_data_array, size);

	/* L4 */
	value = p_class->l4.l4_sp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_SP, &value, p_data_array, size);
	value = p_class->l4.l4_dp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_DP, &value, p_data_array, size);
	value = p_class->l4.l4_valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_VLD, &value, p_data_array, size);
	value = p_class->l4.l4_port_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_PORT_MASK, &value, p_data_array, size);
	value = p_class->l4.l4_valid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_CLASS,
			FE_CLASS_L4_VLD_MASK, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_class_convert_sw_to_hw_data */

static int fe_class_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_class_table_type, idx, entry);
} /* fe_class_set_entry */

static int fe_class_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_class_table_type, entry, rslt_idx);
} /* fe_class_add_entry */

static int fe_class_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_class_table_type, idx, f_force);
} /* fe_class_del_entry_by_idx */

static int fe_class_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_class_table_type, entry, f_force);
} /* fe_class_del_entry */

static int fe_class_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_class_table_type, entry, rslt_idx,
			start_offset);
} /* fe_class_find_entry */

static int fe_class_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_class_table_type, idx, entry);
} /* fe_class_get_entry */

static int fe_class_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_class_table_type, idx);
} /* fe_class_inc_entry_refcnt */

static int fe_class_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_class_table_type, idx);
} /* fe_class_dec_entry_refcnt */

static int fe_class_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_class_table_type, idx, p_cnt);
} /* fe_class_get_entry_refcnt */

static int fe_class_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_class_entry_t *p_class_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_class_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_CLASS_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_CLASS_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_CLASS_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_class_table_type);
		spin_unlock_irqrestore(FE_CLASS_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_class_entry = (fe_class_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_class_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_CLASS_LOCK, flags);
	switch (field) {
	case FE_CLASS_SVIDX:
		p_class_entry->sdb_idx = (__u8)p_value[0];
		break;
	case FE_CLASS_LSPID:
		p_class_entry->port.lspid = (__u8)p_value[0];
		break;
	case FE_CLASS_HDR_A_ORIG_LSPID:
		p_class_entry->port.hdr_a_orig_lspid = (__u8)p_value[0];
		break;
	case FE_CLASS_FWDTYPE:
		p_class_entry->port.fwd_type = (__u8)p_value[0];
		break;
	case FE_CLASS_TPID_ENC_1:
		p_class_entry->l2.tpid_enc_1 = (__u8)p_value[0];
		break;
	case FE_CLASS_VID_1:
		p_class_entry->l2.vid_1 = (__u16)p_value[0];
		break;
	case FE_CLASS_8021P_1:
		p_class_entry->l2._8021p_1 = (__u8)p_value[0];
		break;
	case FE_CLASS_TPID_ENC_2:
		p_class_entry->l2.tpid_enc_2 = (__u8)p_value[0];
		break;
	case FE_CLASS_VID_2:
		p_class_entry->l2.vid_2 = (__u16)p_value[0];
		break;
	case FE_CLASS_8021P_2:
		p_class_entry->l2._8021p_2 = (__u8)p_value[0];
		break;
	case FE_CLASS_DA_AN_MAC_SEL:
		p_class_entry->l2.da_an_mac_sel = (__u8)p_value[0];
		break;
	case FE_CLASS_DA_AN_MAC_HIT:
		p_class_entry->l2.da_an_mac_hit = (__u8)p_value[0];
		break;
	case FE_CLASS_SA_BNG_MAC_SEL:
		p_class_entry->l2.sa_bng_mac_sel = (__u8)p_value[0];
		break;
	case FE_CLASS_SA_BNG_MAC_HIT:
		p_class_entry->l2.sa_bng_mac_hit = (__u8)p_value[0];
		break;
	case FE_CLASS_DSCP:
		p_class_entry->l3.dscp = (__u8)p_value[0];
		break;
	case FE_CLASS_ECN:
		p_class_entry->l3.ecn = (__u8)p_value[0];
		break;
	case FE_CLASS_ETHERTYPE_ENC:
		p_class_entry->l2.ethertype_enc = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_PROT:
		p_class_entry->l3.ip_prot = (__u8)p_value[0];
		break;
	case FE_CLASS_L4_SP:
		p_class_entry->l4.l4_sp = (__u16)p_value[0];
		break;
	case FE_CLASS_L4_DP:
		p_class_entry->l4.l4_dp = (__u16)p_value[0];
		break;
	case FE_CLASS_DA:
		p_class_entry->l2.da[0] = ((__u8*)p_value)[0];
		p_class_entry->l2.da[1] = ((__u8*)p_value)[1];
		p_class_entry->l2.da[2] = ((__u8*)p_value)[2];
		p_class_entry->l2.da[3] = ((__u8*)p_value)[3];
		p_class_entry->l2.da[4] = ((__u8*)p_value)[4];
		p_class_entry->l2.da[5] = ((__u8*)p_value)[5];
		break;
	case FE_CLASS_SA:
		p_class_entry->l2.sa[0] = ((__u8*)p_value)[0];
		p_class_entry->l2.sa[1] = ((__u8*)p_value)[1];
		p_class_entry->l2.sa[2] = ((__u8*)p_value)[2];
		p_class_entry->l2.sa[3] = ((__u8*)p_value)[3];
		p_class_entry->l2.sa[4] = ((__u8*)p_value)[4];
		p_class_entry->l2.sa[5] = ((__u8*)p_value)[5];
		break;
	case FE_CLASS_IP_DA:
		p_class_entry->l3.da[0] = p_value[0];
		p_class_entry->l3.da[1] = p_value[1];
		p_class_entry->l3.da[2] = p_value[2];
		p_class_entry->l3.da[3] = p_value[3];
		break;
	case FE_CLASS_IP_SA:
		p_class_entry->l3.sa[0] = p_value[0];
		p_class_entry->l3.sa[1] = p_value[1];
		p_class_entry->l3.sa[2] = p_value[2];
		p_class_entry->l3.sa[3] = p_value[3];
		break;
	case FE_CLASS_IP_VLD:
		p_class_entry->l3.ip_valid = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_VER:
		p_class_entry->l3.ip_ver = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_FRAGMENT:
		p_class_entry->l3.ip_frag = (__u8)p_value[0];
		break;
	case FE_CLASS_L4_VLD:
		p_class_entry->l4.l4_valid = (__u8)p_value[0];
		break;
	case FE_CLASS_MC_DA:
		p_class_entry->l2.mcast_da = (__u8)p_value[0];
		break;
	case FE_CLASS_BC_DA:
		p_class_entry->l2.bcast_da = (__u8)p_value[0];
		break;
	case FE_CLASS_LEN_ENCODED:
		p_class_entry->l2.len_encoded = (__u8)p_value[0];
		break;
	case FE_CLASS_HDR_A_FLAGS_CRCERR:
		p_class_entry->port.hdr_a_flags_crcerr = (__u8)p_value[0];
		break;
	case FE_CLASS_L3_CHKSUM_ERR:
		p_class_entry->port.l3_csum_err = (__u8)p_value[0];
		break;
	case FE_CLASS_L4_CHKSUM_ERR:
		p_class_entry->port.l4_csum_err = (__u8)p_value[0];
		break;
	case FE_CLASS_NOT_HDR_A_FLAGS_STSVLD:
		p_class_entry->port.not_hdr_a_flags_stsvld = (__u8)p_value[0];
		break;
	case FE_CLASS_LSPID_MASK:
		p_class_entry->port.lspid_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_HDR_A_ORIG_LSPID_MASK:
		p_class_entry->port.hdr_a_orig_lspid_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_FWDTYPE_MASK:
		p_class_entry->port.fwd_type_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_TPID_ENC_1_MSB_MASK:
		p_class_entry->l2.tpid_enc_1_msb_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_TPID_ENC_1_LSB_MASK:
		p_class_entry->l2.tpid_enc_1_lsb_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_VID_1_MASK:
		p_class_entry->l2.vid_1_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_8021P_1_MASK:
		p_class_entry->l2._8021p_1_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_TPID_ENC_2_MSB_MASK:
		p_class_entry->l2.tpid_enc_2_msb_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_TPID_ENC_2_LSB_MASK:
		p_class_entry->l2.tpid_enc_2_lsb_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_VID_2_MASK:
		p_class_entry->l2.vid_2_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_8021P_2_MASK:
		p_class_entry->l2._8021p_2_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_DA_AN_MAC_SEL_MASK:
		p_class_entry->l2.da_an_mac_sel_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_DA_AN_MAC_HIT_MASK:
		p_class_entry->l2.da_an_mac_hit_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_SA_BNG_MAC_SEL_MASK:
		p_class_entry->l2.sa_bng_mac_sel_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_SA_BNG_MAC_HIT_MASK:
		p_class_entry->l2.sa_bng_mac_hit_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_DSCP_MASK:
		p_class_entry->l3.dscp_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_ECN_MASK:
		p_class_entry->l3.ecn_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_ETHERTYPE_ENC_MASK:
		p_class_entry->l2.ethertype_enc_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_PROT_MASK:
		p_class_entry->l3.ip_prot_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_L4_PORT_MASK:
		p_class_entry->l4.l4_port_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_DA_MASK:
		p_class_entry->l2.da_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_SA_MASK:
		p_class_entry->l2.sa_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_DA_MASK:
		p_class_entry->l3.ip_da_mask = (__u16)p_value[0];
		break;
	case FE_CLASS_IP_SA_MASK:
		p_class_entry->l3.ip_sa_mask = (__u16)p_value[0];
		break;
	case FE_CLASS_IP_VLD_MASK:
		p_class_entry->l3.ip_valid_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_VER_MASK:
		p_class_entry->l3.ip_ver_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_IP_FRAGMENT_MASK:
		p_class_entry->l3.ip_frag_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_L4_VLD_MASK:
		p_class_entry->l4.l4_valid_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_MC_DA_MASK:
		p_class_entry->l2.mcast_da_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_BC_DA_MASK:
		p_class_entry->l2.bcast_da_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_LEN_ENCODED_MASK:
		p_class_entry->l2.len_encoded_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_HDR_A_FLAGS_CRCERR_MASK:
		p_class_entry->port.hdr_a_flags_crcerr_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_L3_CHKSUM_ERR_MASK:
		p_class_entry->port.l3_csum_err_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_L4_CHKSUM_ERR_MASK:
		p_class_entry->port.l4_csum_err_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_NOT_HDR_A_FLAGS_STSVLD_MASK:
		p_class_entry->port.not_hdr_a_flags_stsvld_mask =
			(__u8)p_value[0];
		break;
	case FE_CLASS_SPI_VLD:
		p_class_entry->l3.spi_valid = (__u8)p_value[0];
		break;
	case FE_CLASS_SPI_VLD_MASK:
		p_class_entry->l3.spi_valid_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_SPI:
		p_class_entry->l3.spi = p_value[0];
		break;
	case FE_CLASS_SPI_MASK:
		p_class_entry->l3.spi_mask = (__u8)p_value[0];
		break;
	case FE_CLASS_MCGID:
		p_class_entry->port.mcgid = (__u16)p_value[0];
		break;
	case FE_CLASS_MCGID_MASK:
		p_class_entry->port.mcgid_mask = (__u16)p_value[0];
		break;
	case FE_CLASS_RULE_PRI:
		p_class_entry->rule_priority = (__u8)p_value[0];
		break;
	case FE_CLASS_ENTRY_VLD:
		p_class_entry->entry_valid = (__u8)p_value[0];
		break;
	case FE_CLASS_MEM_PARITY:
		p_class_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_CLASS_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_CLASS, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_CLASS_LOCK, flags);

	return status;
} /* fe_class_set_field */

static int fe_class_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_class_entry_t *p_class_entry;
	__u32 hw_value=0, *p_hw_value;
	__u8 hw_mac_value[6] = { 0, 0, 0, 0, 0, 0 };
	__u32 hw_ip_value[4] = { 0, 0, 0, 0 };
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_class_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_CLASS_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_CLASS_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_class_entry = (fe_class_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_class_entry == NULL)
		return FE_TABLE_ENULLPTR;

	switch (field) {
	case FE_CLASS_DA:
	case FE_CLASS_SA:
		p_hw_value = (__u32 *) hw_mac_value;
		break;
	case FE_CLASS_IP_DA:
	case FE_CLASS_IP_SA:
		p_hw_value = hw_ip_value;
		break;
	default:
		p_hw_value = &hw_value;
	}
	spin_lock_irqsave(FE_CLASS_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx, field,
			p_hw_value);
	spin_unlock_irqrestore(FE_CLASS_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_CLASS_LOCK, flags);
	switch (field) {
	case FE_CLASS_SVIDX:
		p_value[0] = (__u32)p_class_entry->sdb_idx;
		break;
	case FE_CLASS_LSPID:
		p_value[0] = (__u32)p_class_entry->port.lspid;
		break;
	case FE_CLASS_HDR_A_ORIG_LSPID:
		p_value[0] = (__u32)p_class_entry->port.hdr_a_orig_lspid;
		break;
	case FE_CLASS_FWDTYPE:
		p_value[0] = (__u32)p_class_entry->port.fwd_type;
		break;
	case FE_CLASS_TPID_ENC_1:
		p_value[0] = (__u32)p_class_entry->l2.tpid_enc_1;
		break;
	case FE_CLASS_VID_1:
		p_value[0] = (__u32)p_class_entry->l2.vid_1;
		break;
	case FE_CLASS_8021P_1:
		p_value[0] = (__u32)p_class_entry->l2._8021p_1;
		break;
	case FE_CLASS_TPID_ENC_2:
		p_value[0] = (__u32)p_class_entry->l2.tpid_enc_2;
		break;
	case FE_CLASS_VID_2:
		p_value[0] = (__u32)p_class_entry->l2.vid_2;
		break;
	case FE_CLASS_8021P_2:
		p_value[0] = (__u32)p_class_entry->l2._8021p_2;
		break;
	case FE_CLASS_DA_AN_MAC_SEL:
		p_value[0] = (__u32)p_class_entry->l2.da_an_mac_sel;
		break;
	case FE_CLASS_DA_AN_MAC_HIT:
		p_value[0] = (__u32)p_class_entry->l2.da_an_mac_hit;
		break;
	case FE_CLASS_SA_BNG_MAC_SEL:
		p_value[0] = (__u32)p_class_entry->l2.sa_bng_mac_sel;
		break;
	case FE_CLASS_SA_BNG_MAC_HIT:
		p_value[0] = (__u32)p_class_entry->l2.sa_bng_mac_hit;
		break;
	case FE_CLASS_DSCP:
		p_value[0] = (__u32)p_class_entry->l3.dscp;
		break;
	case FE_CLASS_ECN:
		p_value[0] = (__u32)p_class_entry->l3.ecn;
		break;
	case FE_CLASS_ETHERTYPE_ENC:
		p_value[0] = (__u32)p_class_entry->l2.ethertype_enc;
		break;
	case FE_CLASS_IP_PROT:
		p_value[0] = (__u32)p_class_entry->l3.ip_prot;
		break;
	case FE_CLASS_L4_SP:
		p_value[0] = (__u32)p_class_entry->l4.l4_sp;
		break;
	case FE_CLASS_L4_DP:
		p_value[0] = (__u32)p_class_entry->l4.l4_dp;
		break;
	case FE_CLASS_DA:
		((__u8*)p_value)[0] = p_class_entry->l2.da[0];
		((__u8*)p_value)[1] = p_class_entry->l2.da[1];
		((__u8*)p_value)[2] = p_class_entry->l2.da[2];
		((__u8*)p_value)[3] = p_class_entry->l2.da[3];
		((__u8*)p_value)[4] = p_class_entry->l2.da[4];
		((__u8*)p_value)[5] = p_class_entry->l2.da[5];
		break;
	case FE_CLASS_SA:
		((__u8*)p_value)[0] = p_class_entry->l2.sa[0];
		((__u8*)p_value)[1] = p_class_entry->l2.sa[1];
		((__u8*)p_value)[2] = p_class_entry->l2.sa[2];
		((__u8*)p_value)[3] = p_class_entry->l2.sa[3];
		((__u8*)p_value)[4] = p_class_entry->l2.sa[4];
		((__u8*)p_value)[5] = p_class_entry->l2.sa[5];
		break;
	case FE_CLASS_IP_DA:
		p_value[0] = p_class_entry->l3.da[0];
		p_value[1] = p_class_entry->l3.da[1];
		p_value[2] = p_class_entry->l3.da[2];
		p_value[3] = p_class_entry->l3.da[3];
		break;
	case FE_CLASS_IP_SA:
		p_value[0] = p_class_entry->l3.sa[0];
		p_value[1] = p_class_entry->l3.sa[1];
		p_value[2] = p_class_entry->l3.sa[2];
		p_value[3] = p_class_entry->l3.sa[3];
		break;
	case FE_CLASS_IP_VLD:
		p_value[0] = (__u32)p_class_entry->l3.ip_valid;
		break;
	case FE_CLASS_IP_VER:
		p_value[0] = (__u32)p_class_entry->l3.ip_ver;
		break;
	case FE_CLASS_IP_FRAGMENT:
		p_value[0] = (__u32)p_class_entry->l3.ip_frag;
		break;
	case FE_CLASS_L4_VLD:
		p_value[0] = (__u32)p_class_entry->l4.l4_valid;
		break;
	case FE_CLASS_MC_DA:
		p_value[0] = (__u32)p_class_entry->l2.mcast_da;
		break;
	case FE_CLASS_BC_DA:
		p_value[0] = (__u32)p_class_entry->l2.bcast_da;
		break;
	case FE_CLASS_LEN_ENCODED:
		p_value[0] = (__u32)p_class_entry->l2.len_encoded;
		break;
	case FE_CLASS_HDR_A_FLAGS_CRCERR:
		p_value[0] = (__u32)p_class_entry->port.hdr_a_flags_crcerr;
		break;
	case FE_CLASS_L3_CHKSUM_ERR:
		p_value[0] = (__u32)p_class_entry->port.l3_csum_err;
		break;
	case FE_CLASS_L4_CHKSUM_ERR:
		p_value[0] = (__u32)p_class_entry->port.l4_csum_err;
		break;
	case FE_CLASS_NOT_HDR_A_FLAGS_STSVLD:
		p_value[0] = (__u32)p_class_entry->port.not_hdr_a_flags_stsvld;
		break;
	case FE_CLASS_LSPID_MASK:
		p_value[0] = (__u32)p_class_entry->port.lspid_mask;
		break;
	case FE_CLASS_HDR_A_ORIG_LSPID_MASK:
		p_value[0] = (__u32)p_class_entry->port.hdr_a_orig_lspid_mask;
		break;
	case FE_CLASS_FWDTYPE_MASK:
		p_value[0] = (__u32)p_class_entry->port.fwd_type_mask;
		break;
	case FE_CLASS_TPID_ENC_1_MSB_MASK:
		p_value[0] = (__u32)p_class_entry->l2.tpid_enc_1_msb_mask;
		break;
	case FE_CLASS_TPID_ENC_1_LSB_MASK:
		p_value[0] = (__u32)p_class_entry->l2.tpid_enc_1_lsb_mask;
		break;
	case FE_CLASS_VID_1_MASK:
		p_value[0] = (__u32)p_class_entry->l2.vid_1_mask;
		break;
	case FE_CLASS_8021P_1_MASK:
		p_value[0] = (__u32)p_class_entry->l2._8021p_1_mask;
		break;
	case FE_CLASS_TPID_ENC_2_MSB_MASK:
		p_value[0] = (__u32)p_class_entry->l2.tpid_enc_2_msb_mask;
		break;
	case FE_CLASS_TPID_ENC_2_LSB_MASK:
		p_value[0] = (__u32)p_class_entry->l2.tpid_enc_2_lsb_mask;
		break;
	case FE_CLASS_VID_2_MASK:
		p_value[0] = (__u32)p_class_entry->l2.vid_2_mask;
		break;
	case FE_CLASS_8021P_2_MASK:
		p_value[0] = (__u32)p_class_entry->l2._8021p_2_mask;
		break;
	case FE_CLASS_DA_AN_MAC_SEL_MASK:
		p_value[0] = (__u32)p_class_entry->l2.da_an_mac_sel_mask;
		break;
	case FE_CLASS_DA_AN_MAC_HIT_MASK:
		p_value[0] = (__u32)p_class_entry->l2.da_an_mac_hit_mask;
		break;
	case FE_CLASS_SA_BNG_MAC_SEL_MASK:
		p_value[0] = (__u32)p_class_entry->l2.sa_bng_mac_sel_mask;
		break;
	case FE_CLASS_SA_BNG_MAC_HIT_MASK:
		p_value[0] = (__u32)p_class_entry->l2.sa_bng_mac_hit_mask;
		break;
	case FE_CLASS_DSCP_MASK:
		p_value[0] = (__u32)p_class_entry->l3.dscp_mask;
		break;
	case FE_CLASS_ECN_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ecn_mask;
		break;
	case FE_CLASS_ETHERTYPE_ENC_MASK:
		p_value[0] = (__u32)p_class_entry->l2.ethertype_enc_mask;
		break;
	case FE_CLASS_IP_PROT_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ip_prot_mask;
		break;
	case FE_CLASS_L4_PORT_MASK:
		p_value[0] = (__u32)p_class_entry->l4.l4_port_mask;
		break;
	case FE_CLASS_DA_MASK:
		p_value[0] = (__u32)p_class_entry->l2.da_mask;
		break;
	case FE_CLASS_SA_MASK:
		p_value[0] = (__u32)p_class_entry->l2.sa_mask;
		break;
	case FE_CLASS_IP_DA_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ip_da_mask;
		break;
	case FE_CLASS_IP_SA_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ip_sa_mask;
		break;
	case FE_CLASS_IP_VLD_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ip_valid_mask;
		break;
	case FE_CLASS_IP_VER_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ip_ver_mask;
		break;
	case FE_CLASS_IP_FRAGMENT_MASK:
		p_value[0] = (__u32)p_class_entry->l3.ip_frag_mask;
		break;
	case FE_CLASS_L4_VLD_MASK:
		p_value[0] = (__u32)p_class_entry->l4.l4_valid_mask;
		break;
	case FE_CLASS_MC_DA_MASK:
		p_value[0] = (__u32)p_class_entry->l2.mcast_da_mask;
		break;
	case FE_CLASS_BC_DA_MASK:
		p_value[0] = (__u32)p_class_entry->l2.bcast_da_mask;
		break;
	case FE_CLASS_LEN_ENCODED_MASK:
		p_value[0] = (__u32)p_class_entry->l2.len_encoded_mask;
		break;
	case FE_CLASS_HDR_A_FLAGS_CRCERR_MASK:
		p_value[0] = (__u32)p_class_entry->port.hdr_a_flags_crcerr_mask;
		break;
	case FE_CLASS_L3_CHKSUM_ERR_MASK:
		p_value[0] = (__u32)p_class_entry->port.l3_csum_err_mask;
		break;
	case FE_CLASS_L4_CHKSUM_ERR_MASK:
		p_value[0] = (__u32)p_class_entry->port.l4_csum_err_mask;
		break;
	case FE_CLASS_NOT_HDR_A_FLAGS_STSVLD_MASK:
		p_value[0] = (__u32)p_class_entry->port.not_hdr_a_flags_stsvld_mask;
		break;
	case FE_CLASS_SPI_VLD:
		p_value[0] = (__u32)p_class_entry->l3.spi_valid;
		break;
	case FE_CLASS_SPI_VLD_MASK:
		p_value[0] = (__u32)p_class_entry->l3.spi_valid_mask;
		break;
	case FE_CLASS_SPI:
		p_value[0] = (__u32)p_class_entry->l3.spi;
		break;
	case FE_CLASS_SPI_MASK:
		p_value[0] = (__u32)p_class_entry->l3.spi_mask;
		break;
	case FE_CLASS_MCGID:
		p_value[0] = (__u32)p_class_entry->port.mcgid;
		break;
	case FE_CLASS_MCGID_MASK:
		p_value[0] = (__u32)p_class_entry->port.mcgid_mask;
		break;
	case FE_CLASS_RULE_PRI:
		p_value[0] = (__u32)p_class_entry->rule_priority;
		break;
	case FE_CLASS_ENTRY_VLD:
		p_value[0] = (__u32)p_class_entry->entry_valid;
		break;
	case FE_CLASS_MEM_PARITY:
		p_value[0] = (__u32)p_class_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_CLASS_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_CLASS_LOCK, flags);

	if (hw_value != p_value[0]) {
		printk("%s:%d:SW value (0x%X) doesn't not match with HW value "
				"(0x%X)\n", __func__, __LINE__, p_value[0], 
				hw_value);
	}
	switch (field) {
	case FE_CLASS_DA:
	case FE_CLASS_SA:
		if ((hw_mac_value[0] != p_value[0]) ||
				(hw_mac_value[1] != p_value[1]) ||
				(hw_mac_value[2] != p_value[2]) ||
				(hw_mac_value[3] != p_value[3]) ||
				(hw_mac_value[4] != p_value[4]) ||
				(hw_mac_value[5] != p_value[5])) {
			printk("%s:%d:SW value doesn't not match with HW "
					"value\n", __func__, __LINE__);
		}
		break;
	case FE_CLASS_IP_DA:
	case FE_CLASS_IP_SA:
		if ((hw_ip_value[0] != p_value[0]) ||
				(hw_ip_value[1] != p_value[1]) ||
				(hw_ip_value[2] != p_value[2]) ||
				(hw_ip_value[3] != p_value[3])) {
			printk("%s:%d:SW value doesn't not match with HW "
					"value\n", __func__, __LINE__);
		}
		break;
	default:
		if (hw_value != p_value[0]) {
			printk("%s:%d:SW value doesn't not match with HW "
					"value\n", __func__, __LINE__);
		}
	}

	return FE_TABLE_OK;
} /* fe_class_get_field */

static int fe_class_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_class_table_type);
} /* fe_class_flush_table */

static int fe_class_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_class_table_type);
} /* fe_class_get_avail_count */

static void _fe_class_print_title(void)
{
	printk("\n\n ----------------- CLASS Table ----------------------\n");
	printk("|--------------------------------------------------------\n");
} /* _fe_class_print_title */

static void _fe_class_print_entry(unsigned int idx)
{
	fe_class_entry_t class_entry, *p_class;
	__u32 value;
	__u8 value_mac[6];
	__u32 value_ip[4];
	int status;
	unsigned int count;

	status = fe_class_get_entry(idx, &class_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_class_get_entry_refcnt(idx, &count);

	p_class = &class_entry;

	printk("| index: %04d | refcnt: %d ", idx, count);

	/* General */
	printk("  |- General: SDB Idx %02x (HW ", p_class->sdb_idx);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SVIDX, &value);
	printk("%02x)\n", value);
	printk("\trule_priority %01x ", p_class->rule_priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_RULE_PRI, &value);
	printk("(HW %01x)\n", value);
	printk("\tentry_valid %01x ", p_class->entry_valid);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_ENTRY_VLD, &value);
	printk("(HW %01x)\n", value);
	printk("\tParity %01x ", p_class->parity);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_MEM_PARITY, &value);
	printk("(HW %01x)\n", value);

	/* Port */
	printk("  |- Port: LSPID %02x (HW ", p_class->port.lspid);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_LSPID, &value);
	printk("%02x). Mask %02x (HW ", value, p_class->port.lspid_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_LSPID_MASK, &value);
	printk("%02x)\n", value);
	printk("\tHDR_A_ORIG_LSPID %02x ", p_class->port.hdr_a_orig_lspid);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_HDR_A_ORIG_LSPID, &value);
	printk("(HW %02x). ", value);
	printk("Mask %02x ", p_class->port.hdr_a_orig_lspid_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_HDR_A_ORIG_LSPID_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tFE_TYPE %02x ", p_class->port.fwd_type);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_FWDTYPE, &value);
	printk("(HW %02x). ", value);
	printk("Mask %02x ", p_class->port.fwd_type_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_FWDTYPE_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tHDR_A_FLAGS_CRCERR %02x ", p_class->port.hdr_a_flags_crcerr);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_HDR_A_FLAGS_CRCERR, &value);
	printk("(HW %02x). ", value);
	printk("Mask %02x ", p_class->port.hdr_a_flags_crcerr_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_HDR_A_FLAGS_CRCERR_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tL3_CSUM_ERR %02x ", p_class->port.l3_csum_err);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L3_CHKSUM_ERR, &value);
	printk("(HW %02x). ", value);
	printk("Mask %02x ", p_class->port.l3_csum_err_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L3_CHKSUM_ERR_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tL4_CSUM_ERR %02x ", p_class->port.l4_csum_err);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_CHKSUM_ERR, &value);
	printk("(HW %02x). ", value);
	printk("Mask %02x ", p_class->port.l4_csum_err_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_CHKSUM_ERR_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tNOT HDR A FLAGS STSVLD %02x ",
			p_class->port.not_hdr_a_flags_stsvld);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_NOT_HDR_A_FLAGS_STSVLD, &value);
	printk("(HW %02x). ", value);
	printk("Mask %02x ", p_class->port.not_hdr_a_flags_stsvld_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_NOT_HDR_A_FLAGS_STSVLD_MASK, &value);
	printk("(HW %02x)\n", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_MCGID, &value);
	printk("(HW %02x)\n", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_MCGID_MASK, &value);
	printk("(HW %02x)\n", value);

	/* L2 */
	printk("  |- L2: TPID#1 %02x (HW ", p_class->l2.tpid_enc_1);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_TPID_ENC_1, &value);
	printk("%02x). ", value);
	printk("MSB Mask %02x ", p_class->l2.tpid_enc_1_msb_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_TPID_ENC_1_MSB_MASK, &value);
	printk("(HW %02x). ", value);
	printk("LSB Mask %02x ", p_class->l2.tpid_enc_1_lsb_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_TPID_ENC_1_LSB_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tVID#1 %04x (HW ", p_class->l2.vid_1);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_VID_1, &value);
	printk("%04x). ", value);
	printk("MASK %02x ", p_class->l2.vid_1_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_VID_1_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\t8021P#1 %02x (HW ", p_class->l2._8021p_1);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_8021P_1, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2._8021p_1_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_8021P_1_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tTPID#2 %02x (HW ", p_class->l2.tpid_enc_2);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_TPID_ENC_2, &value);
	printk("%02x). ", value);
	printk("MSB Mask %02x ", p_class->l2.tpid_enc_2_msb_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_TPID_ENC_2_MSB_MASK, &value);
	printk("(HW %02x). ", value);
	printk("LSB Mask %02x ", p_class->l2.tpid_enc_2_lsb_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_TPID_ENC_2_LSB_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tVID#2 %04x (HW ", p_class->l2.vid_2);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_VID_2, &value);
	printk("%04x). ", value);
	printk("MASK %02x ", p_class->l2.vid_2_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_VID_2_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\t8021P#2 %02x (HW ", p_class->l2._8021p_2);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_8021P_2, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2._8021p_2_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_8021P_2_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tDA AN MAC SEL %02x (HW ", p_class->l2.da_an_mac_sel);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DA_AN_MAC_SEL, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2.da_an_mac_sel_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DA_AN_MAC_SEL_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tDA AN MAC HIT %02x (HW ", p_class->l2.da_an_mac_hit);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DA_AN_MAC_HIT, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2.da_an_mac_hit_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DA_AN_MAC_HIT_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tSA BNG MAC SEL %02x (HW ", p_class->l2.sa_bng_mac_sel);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SA_BNG_MAC_SEL, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2.sa_bng_mac_sel_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SA_BNG_MAC_SEL_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tSA BNG MAC HIT %02x (HW ", p_class->l2.sa_bng_mac_hit);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SA_BNG_MAC_HIT, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2.sa_bng_mac_hit_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SA_BNG_MAC_HIT_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tEtherType_enc %02x (HW ", p_class->l2.ethertype_enc);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_ETHERTYPE_ENC, &value);
	printk("%02x). ", value);
	printk("MASK %02x ", p_class->l2.ethertype_enc_mask);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_ETHERTYPE_ENC_MASK, &value);
	printk("(HW %02x)\n", value);
	printk("\tDA %02x.%02x.", p_class->l2.da[0], p_class->l2.da[1]);
	printk("%02x.%02x.", p_class->l2.da[2], p_class->l2.da[3]);
	printk("%02x.%02x ", p_class->l2.da[4], p_class->l2.da[5]);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DA, (__u32*)value_mac);
	printk("(HW %02x.%02x.%02x.", value_mac[0], value_mac[1], value_mac[2]);
	printk("%02x.%02x.%02x). ", value_mac[3], value_mac[4], value_mac[5]);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DA_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l2.da_mask, value);
	printk("\tSA %02x.%02x.", p_class->l2.sa[0], p_class->l2.sa[1]);
	printk("%02x.%02x.", p_class->l2.sa[2], p_class->l2.sa[3]);
	printk("%02x.%02x ", p_class->l2.sa[4], p_class->l2.sa[5]);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SA, (__u32*)value_mac);
	printk("(HW %02x.%02x.%02x.", value_mac[0], value_mac[1], value_mac[2]);
	printk("%02x.%02x.%02x). ", value_mac[3], value_mac[4], value_mac[5]);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SA_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l2.sa_mask, value);

	printk("\tMCAST DA %02x ", p_class->l2.mcast_da);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_MC_DA, &value);
	printk("(HW %02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_MC_DA_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l2.mcast_da_mask, value);
	printk("\tBCAST DA %02x ", p_class->l2.bcast_da);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_BC_DA, &value);
	printk("(HW %02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_BC_DA_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l2.bcast_da_mask, value);
	printk("\tLEN encoded %02x ", p_class->l2.len_encoded);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_LEN_ENCODED, &value);
	printk("(HW %02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_LEN_ENCODED_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l2.len_encoded_mask, value);

	/* L3 */
	printk("  |- L3: DSCP %02x (HW ", p_class->l3.dscp);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DSCP, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_DSCP_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.dscp_mask, value);
	printk("\tECN %02x (HW ", p_class->l3.ecn);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_ECN, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_ECN_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.ecn_mask, value);
	printk("\tIP Prot %02x (HW ", p_class->l3.ip_prot);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_PROT, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_PROT_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.ip_prot_mask, value);
	printk("\tSA %08x.%08x.", p_class->l3.sa[0], p_class->l3.sa[1]);
	printk("%08x.%08x (HW ", p_class->l3.sa[2], p_class->l3.sa[3]);
	memset(value_ip, 0, 16);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_SA, value_ip);
	printk("%08x.%08x.%08x.", value_ip[0], value_ip[1], value_ip[2]);
	printk("%08x). ", value_ip[3]);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_SA_MASK, &value);
	printk("Mask %04x (HW %04x).\n", p_class->l3.ip_sa_mask, value);
	printk("\tDA %08x.%08x.", p_class->l3.da[0], p_class->l3.da[1]);
	printk("%08x.%08x (HW ", p_class->l3.da[2], p_class->l3.da[3]);
	memset(value_ip, 0, 16);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_DA, value_ip);
	printk("%08x.%08x.%08x.", value_ip[0], value_ip[1], value_ip[2]);
	printk("%08x). ", value_ip[3]);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_DA_MASK, &value);
	printk("Mask %04x (HW %04x).\n", p_class->l3.ip_da_mask, value);
	printk("\tIP Valid %02x (HW ", p_class->l3.ip_valid);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_VLD, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_VLD_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.ip_valid_mask, value);
	printk("\tIP Ver %02x (HW ", p_class->l3.ip_ver);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_VER, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_VER_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.ip_ver_mask, value);
	printk("\tIP Frag %02x (HW ", p_class->l3.ip_frag);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_FRAGMENT, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_IP_FRAGMENT_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.ip_frag_mask, value);
	printk("\tSPI %08x (HW ", p_class->l3.spi);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SPI, &value);
	printk("%08x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SPI_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.spi_mask, value);
	printk("\tSPI Valid %02x (HW ", p_class->l3.spi_valid);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SPI_VLD, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_SPI_VLD_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l3.spi_valid_mask, value);

	/* L4 */
	printk("  |- L4: Src Port %04x (HW ", p_class->l4.l4_sp);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_SP, &value);
	printk("%04x). ", value);
	printk("Dst Port %04x (HW ", p_class->l4.l4_dp);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_DP, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_PORT_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l4.l4_port_mask, value);
	printk("\tL4 Valid %02x (HW ", p_class->l4.l4_valid);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_VLD, &value);
	printk("%02x). ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_CLASS, idx,
			FE_CLASS_L4_VLD_MASK, &value);
	printk("Mask %02x (HW %02x).\n", p_class->l4.l4_valid_mask, value);
} /* _fe_class_print_entry */

static void fe_class_print_entry(unsigned int idx)
{
	if (idx >= FE_CLASS_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_class_print_title();
	_fe_class_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_class_print_entry */

static void fe_class_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_CLASS_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_class_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_class_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_class_print_range */

static void fe_class_print_table(void)
{
	unsigned int i;

	_fe_class_print_title();
	for (i = 0; i < cs_fe_class_table_type.max_entry; i++) {
		_fe_class_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_class_print_table */


static cs_fe_table_t cs_fe_class_table_type = {
	.type_id = FE_TABLE_CLASS,
	.max_entry = FE_CLASS_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_class_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_class_convert_sw_to_hw_data,
		.alloc_entry = fe_class_alloc_entry,
		.set_entry = fe_class_set_entry,
		.add_entry = fe_class_add_entry,
		.del_entry_by_idx = fe_class_del_entry_by_idx,
		.del_entry = fe_class_del_entry,
		.find_entry = fe_class_find_entry,
		.get_entry = fe_class_get_entry,
		.inc_entry_refcnt = fe_class_inc_entry_refcnt,
		.dec_entry_refcnt = fe_class_dec_entry_refcnt,
		.get_entry_refcnt = fe_class_get_entry_refcnt,
		.set_field = fe_class_set_field,
		.get_field = fe_class_get_field,
		.flush_table = fe_class_flush_table,
		.get_avail_count = fe_class_get_avail_count,
		.print_entry = fe_class_print_entry,
		.print_range = fe_class_print_range,
		.print_table = fe_class_print_table,
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
		.enbl_voqdrp = NULL,
		.dsbl_voqdrp = NULL,
		.get_voqdrp = NULL,
	},
	.content_table = NULL,
};

int cs_fe_ioctl_class(struct net_device *dev, void *pdata, void *cmd)
{
	fe_class_entry_t *p_rslt = (fe_class_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_class_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_class_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_class_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** CLASS entry 0 ~ %d ***\n", FE_CLASS_ENTRY_MAX - 1);
		fe_class_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_class */

/* this API will initialize class table */
int cs_fe_table_class_init(void)
{
	int ret;
	unsigned int index;

	spin_lock_init(FE_CLASS_LOCK);

	cs_fe_class_table_type.content_table = cs_table_alloc(
			cs_fe_class_table_type.max_entry);
	if (cs_fe_class_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_class_table_type.type_id,
			&cs_fe_class_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register CLASS table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_class_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_class_flush_table();

	/* Reserve a default entry */
	ret = fe_class_alloc_entry(&index, 0);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to allocate default CLASS entry\n",
				__func__, __LINE__);
		return -1;
	}
	if (index != 0) {
		printk("%s:%d:the default index is not 0 (%d)!\n",
				__func__, __LINE__, index);
	}
	return FE_TABLE_OK;
} /* cs_fe_table_class_init */
EXPORT_SYMBOL(cs_fe_table_class_init);

void cs_fe_table_class_exit(void)
{
	fe_class_flush_table();

	if (cs_fe_class_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_class_table_type.content_table);
	cs_fe_table_unregister(cs_fe_class_table_type.type_id);
} /* cs_fe_table_class_exit */
EXPORT_SYMBOL(cs_fe_table_class_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_class_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_class_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_class_alloc_entry_ut);

int fe_class_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{

	return fe_class_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_class_convert_sw_to_hw_data_ut);

int fe_class_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_class_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_class_set_entry_ut);

int fe_class_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_class_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_class_add_entry_ut);

int fe_class_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_class_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_class_del_entry_by_idx_ut);

int fe_class_del_entry_ut(void *entry, bool f_force)
{
	return fe_class_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_class_del_entry_ut);

int fe_class_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_class_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_class_find_entry_ut);

int fe_class_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_class_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_class_get_entry_ut);

int fe_class_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_class_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_class_inc_entry_refcnt_ut);

int fe_class_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_class_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_class_dec_entry_refcnt_ut);

int fe_class_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_class_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_class_get_entry_refcnt_ut);

int fe_class_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_class_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_class_set_field_ut);

int fe_class_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{

	return fe_class_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_class_get_field_ut);

int fe_class_flush_table_ut(void)
{
	return fe_class_flush_table();
}
EXPORT_SYMBOL(fe_class_flush_table_ut);

int fe_class_get_avail_count_ut(void)
{
	return fe_class_get_avail_count();
}
EXPORT_SYMBOL(fe_class_get_avail_count_ut);

void fe_class_print_entry_ut(unsigned int idx)
{
	fe_class_print_entry(idx);
}
EXPORT_SYMBOL(fe_class_print_entry_ut);

void fe_class_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_class_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_class_print_range_ut);

void fe_class_print_table_ut(void)
{
	fe_class_print_table();
}
EXPORT_SYMBOL(fe_class_print_table_ut);



#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */


