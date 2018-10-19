/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_hashmask.c
 *
 * $Id: cs_fe_table_hashmask.c,v 1.5 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains HASH MASK Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_hashmask_table_type;

#define FE_HASHMASK_TABLE_PTR	(cs_fe_hashmask_table_type.content_table)
#define FE_HASHMASK_LOCK	&(cs_fe_hashmask_table_type.lock)

static int fe_hashmask_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_hashmask_table_type, rslt_idx,
			start_offset);
} /* fe_hashmask_alloc_entry */

static int fe_hashmask_convert_sw_to_hw_data(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	__u32 value;
	fe_hash_mask_entry_t *entry = (fe_hash_mask_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->mac_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_MAC_DA_MASK, &value, p_data_array, size);

	value = (__u32)entry->mac_sa_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_MAC_SA_MASK, &value, p_data_array, size);

	value = (__u32)entry->ethertype_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_ETHERTYPE_MASK, &value, p_data_array, size);

	value = (__u32)entry->llc_type_enc_msb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_LLC_TYPE_ENC_MSB_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->llc_type_enc_lsb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_LLC_TYPE_ENC_LSB_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->tpid_enc_1_msb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_TPID_ENC_1_MSB_MASK, &value, p_data_array, size);

	value = (__u32)entry->tpid_enc_1_lsb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_TPID_ENC_1_LSB_MASK, &value, p_data_array, size);

	value = (__u32)entry->_8021p_1_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_8021P_1_MASK, &value, p_data_array, size);

	value = (__u32)entry->dei_1_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_DEI_1_MASK, &value, p_data_array, size);

	value = (__u32)entry->vid_1_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_VID_1_MASK, &value, p_data_array, size);

	value = (__u32)entry->tpid_enc_2_msb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_TPID_ENC_2_MSB_MASK, &value, p_data_array, size);

	value = (__u32)entry->tpid_enc_2_lsb_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_TPID_ENC_2_LSB_MASK, &value, p_data_array, size);

	value = (__u32)entry->_8021p_2_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_8021P_2_MASK, &value, p_data_array, size);

	value = (__u32)entry->dei_2_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_DEI_2_MASK, &value, p_data_array, size);

	value = (__u32)entry->vid_2_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_VID_2_MASK, &value, p_data_array, size);

	value = (__u32)entry->ip_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IP_DA_MASK, &value, p_data_array, size);

	value = (__u32)entry->ip_sa_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IP_SA_MASK, &value, p_data_array, size);

	value = (__u32)entry->ip_prot_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IP_PROT_MASK, &value, p_data_array, size);

	value = (__u32)entry->dscp_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_DSCP_MASK, &value, p_data_array, size);

	value = (__u32)entry->ecn_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_ECN_MASK, &value, p_data_array, size);

	value = (__u32)entry->ip_fragment_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IP_FRAGMENT_MASK, &value, p_data_array, size);

	value = (__u32)entry->keygen_poly_sel;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_KEYGEN_POLY_SEL, &value, p_data_array, size);

	value = (__u32)entry->ipv6_flow_lbl_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IPV6_FLOW_LBL_MASK, &value, p_data_array, size);

	value = (__u32)entry->ip_ver_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IP_VER_MASK, &value, p_data_array, size);

	value = (__u32)entry->ip_vld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IP_VLD_MASK, &value, p_data_array, size);

	value = (__u32)entry->l4_ports_rngd;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L4_PORTS_RNGD, &value, p_data_array, size);

	value = (__u32)entry->l4_dp_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L4_DP_MASK, &value, p_data_array, size);

	value = (__u32)entry->l4_sp_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L4_SP_MASK, &value, p_data_array, size);

	value = (__u32)entry->tcp_ctrl_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_TCP_CTRL_MASK, &value, p_data_array, size);

	value = (__u32)entry->tcp_ecn_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_TCP_ECN_MASK, &value, p_data_array, size);

	value = (__u32)entry->l4_vld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L4_VLD_MASK, &value, p_data_array, size);

	value = (__u32)entry->lspid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_LSPID_MASK, &value, p_data_array, size);

	value = (__u32)entry->fwdtype_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_FWDTYPE_MASK, &value, p_data_array, size);

	value = (__u32)entry->pppoe_session_id_vld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_PPPOE_SESSION_ID_VLD_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->pppoe_session_id_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_PPPOE_SESSION_ID_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->rsvd_109;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_RSVD_109, &value, p_data_array, size);

	value = (__u32)entry->recirc_idx_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_RECIRC_IDX_MASK, &value, p_data_array, size);

	value = (__u32)entry->mcidx_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_MCIDX_MASK, &value, p_data_array, size);

	value = (__u32)entry->mc_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_MC_DA_MASK, &value, p_data_array, size);

	value = (__u32)entry->bc_da_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_BC_DA_MASK, &value, p_data_array, size);

	value = (__u32)entry->da_an_mac_sel_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_DA_AN_MAC_SEL_MASK, &value, p_data_array, size);

	value = (__u32)entry->da_an_mac_hit_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_DA_AN_MAC_HIT_MASK, &value, p_data_array, size);

	value = (__u32)entry->orig_lspid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_ORIG_LSPID_MASK, &value, p_data_array, size);

	value = (__u32)entry->l7_field_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L7_FIELD_MASK, &value, p_data_array, size);

	value = (__u32)entry->l7_field_vld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L7_FIELD_VLD_MASK, &value, p_data_array, size);

	value = (__u32)entry->hdr_a_flags_crcerr_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_HDR_A_FLAGS_CRCERR_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->l3_chksum_err_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L3_CHKSUM_ERR_MASK, &value, p_data_array, size);

	value = (__u32)entry->l4_chksum_err_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L4_CHKSUM_ERR_MASK, &value, p_data_array, size);

	value = (__u32)entry->not_hdr_a_flags_stsvld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_NOT_HDR_A_FLAGS_STSVLD_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->hash_fid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_HASH_FID_MASK, &value, p_data_array, size);

	value = (__u32)entry->l7_field_sel;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_L7_FIELD_SEL, &value, p_data_array, size);

	value = (__u32)entry->sa_bng_mac_sel_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_SA_BNG_MAC_SEL_MASK, &value, p_data_array, size);

	value = (__u32)entry->sa_bng_mac_hit_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_SA_BNG_MAC_HIT_MASK, &value, p_data_array, size);

	value = (__u32)entry->spi_vld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_SPI_VLD_MASK, &value, p_data_array, size);

	value = (__u32)entry->spi_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_SPI_MASK, &value, p_data_array, size);

	value = (__u32)entry->ipv6_ndp_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IPV6_NDP_MASK, &value, p_data_array, size);

	value = (__u32)entry->ipv6_hbh_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IPV6_HBH_MASK, &value, p_data_array, size);

	value = (__u32)entry->ipv6_rh_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IPV6_RH_MASK, &value, p_data_array, size);

	value = (__u32)entry->ipv6_doh_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_IPV6_DOH_MASK, &value, p_data_array, size);

	value = (__u32)entry->ppp_protocol_vld_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_PPP_PROTOCOL_VLD_MASK, &value, p_data_array,
			size);

	value = (__u32)entry->ppp_protocol_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_PPP_PROTOCOL_MASK, &value, p_data_array, size);

	value = (__u32)entry->pktlen_rng_match_vector_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_PKTLEN_RNG_MATCH_VECTOR_MASK, &value,
			p_data_array, size);

	value = (__u32)entry->mcgid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_MCGID_MASK, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_MASK,
			FE_HM_PARITY, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_hashmask_convert_sw_to_hw_data */

static int fe_hashmask_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_hashmask_table_type, idx, entry);
} /* fe_hashmask_set_entry */

static int fe_hashmask_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_hashmask_table_type, entry, rslt_idx);
} /* fe_hashmask_add_entry */

static int fe_hashmask_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_hashmask_table_type, idx,
			f_force);
} /* fe_hashmask_del_entry_by_idx */

static int fe_hashmask_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_hashmask_table_type, entry, f_force);
} /* fe_hashmask_del_entry */

static int fe_hashmask_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_hashmask_table_type, entry, rslt_idx,
			start_offset);
} /* fe_hashmask_find_entry */

static int fe_hashmask_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_hashmask_table_type, idx, entry);
} /* fe_hashmask_get_entry */

static int fe_hashmask_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_hashmask_table_type, idx);
} /* fe_hashmask_inc_entry_refcnt */

static int fe_hashmask_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_hashmask_table_type, idx);
} /* fe_hashmask_dec_entry_refcnt */

static int fe_hashmask_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_hashmask_table_type, idx,
			p_cnt);
} /* fe_hashmask_get_entry_refcnt */

static int fe_hashmask_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_hash_mask_entry_t *p_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_hashmask_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_HASHMASK_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_HASHMASK_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_HASHMASK_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_hashmask_table_type);
		spin_unlock_irqrestore(FE_HASHMASK_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_entry = (fe_hash_mask_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_HASHMASK_LOCK, flags);
	switch (field) {
	case FE_HM_MAC_DA_MASK:
		p_entry->mac_da_mask = (__u8)p_value[0];
		break;
	case FE_HM_MAC_SA_MASK:
		p_entry->mac_sa_mask = (__u8)p_value[0];
		break;
	case FE_HM_ETHERTYPE_MASK:
		p_entry->ethertype_mask = (__u8)p_value[0];
		break;
	case FE_HM_LLC_TYPE_ENC_MSB_MASK:
		p_entry->llc_type_enc_msb_mask = (__u8)p_value[0];
		break;
	case FE_HM_LLC_TYPE_ENC_LSB_MASK:
		p_entry->llc_type_enc_lsb_mask = (__u16)p_value[0];
		break;
	case FE_HM_TPID_ENC_1_MSB_MASK:
		p_entry->tpid_enc_1_msb_mask = (__u8)p_value[0];
		break;
	case FE_HM_TPID_ENC_1_LSB_MASK:
		p_entry->tpid_enc_1_lsb_mask = (__u8)p_value[0];
		break;
	case FE_HM_8021P_1_MASK:
		p_entry->_8021p_1_mask = (__u16)p_value[0];
		break;
	case FE_HM_DEI_1_MASK:
		p_entry->dei_1_mask = (__u8)p_value[0];
		break;
	case FE_HM_VID_1_MASK:
		p_entry->vid_1_mask = (__u16)p_value[0];
		break;
	case FE_HM_TPID_ENC_2_MSB_MASK:
		p_entry->tpid_enc_2_msb_mask = (__u8)p_value[0];
		break;
	case FE_HM_TPID_ENC_2_LSB_MASK:
		p_entry->tpid_enc_2_lsb_mask = (__u8)p_value[0];
		break;
	case FE_HM_8021P_2_MASK:
		p_entry->_8021p_2_mask = (__u8)p_value[0];
		break;
	case FE_HM_DEI_2_MASK:
		p_entry->dei_2_mask = (__u8)p_value[0];
		break;
	case FE_HM_VID_2_MASK:
		p_entry->vid_2_mask = (__u8)p_value[0];
		break;
	case FE_HM_IP_DA_MASK:
		p_entry->ip_da_mask = (__u16)p_value[0];
		break;
	case FE_HM_IP_SA_MASK:
		p_entry->ip_sa_mask = (__u16)p_value[0];
		break;
	case FE_HM_IP_PROT_MASK:
		p_entry->ip_prot_mask = (__u8)p_value[0];
		break;
	case FE_HM_DSCP_MASK:
		p_entry->dscp_mask = (__u8)p_value[0];
		break;
	case FE_HM_ECN_MASK:
		p_entry->ecn_mask = (__u8)p_value[0];
		break;
	case FE_HM_IP_FRAGMENT_MASK:
		p_entry->ip_fragment_mask = (__u8)p_value[0];
		break;
	case FE_HM_KEYGEN_POLY_SEL:
		p_entry->keygen_poly_sel = (__u8)p_value[0];
		break;
	case FE_HM_IPV6_FLOW_LBL_MASK:
		p_entry->ipv6_flow_lbl_mask = (__u8)p_value[0];
		break;
	case FE_HM_IP_VER_MASK:
		p_entry->ip_ver_mask = (__u8)p_value[0];
		break;
	case FE_HM_IP_VLD_MASK:
		p_entry->ip_vld_mask = (__u8)p_value[0];
		break;
	case FE_HM_L4_PORTS_RNGD:
		p_entry->l4_ports_rngd = (__u8)p_value[0];
		break;
	case FE_HM_L4_DP_MASK:
		p_entry->l4_dp_mask = (__u16)p_value[0];
		break;
	case FE_HM_L4_SP_MASK:
		p_entry->l4_sp_mask = (__u16)p_value[0];
		break;
	case FE_HM_TCP_CTRL_MASK:
		p_entry->tcp_ctrl_mask = (__u8)p_value[0];
		break;
	case FE_HM_TCP_ECN_MASK:
		p_entry->tcp_ecn_mask = (__u8)p_value[0];
		break;
	case FE_HM_L4_VLD_MASK:
		p_entry->l4_vld_mask = (__u8)p_value[0];
		break;
	case FE_HM_LSPID_MASK:
		p_entry->lspid_mask = (__u8)p_value[0];
		break;
	case FE_HM_FWDTYPE_MASK:
		p_entry->fwdtype_mask = (__u8)p_value[0];
		break;
	case FE_HM_PPPOE_SESSION_ID_VLD_MASK:
		p_entry->pppoe_session_id_vld_mask = (__u8)p_value[0];
		break;
	case FE_HM_PPPOE_SESSION_ID_MASK:
		p_entry->pppoe_session_id_mask = (__u8)p_value[0];
		break;
	case FE_HM_RSVD_109:
		p_entry->rsvd_109 = (__u8)p_value[0];
		break;
	case FE_HM_RECIRC_IDX_MASK:
		p_entry->recirc_idx_mask = (__u8)p_value[0];
		break;
	case FE_HM_MCIDX_MASK:
		p_entry->mcidx_mask = (__u8)p_value[0];
		break;
	case FE_HM_MC_DA_MASK:
		p_entry->mc_da_mask = (__u8)p_value[0];
		break;
	case FE_HM_BC_DA_MASK:
		p_entry->bc_da_mask = (__u8)p_value[0];
		break;
	case FE_HM_DA_AN_MAC_SEL_MASK:
		p_entry->da_an_mac_sel_mask = (__u8)p_value[0];
		break;
	case FE_HM_DA_AN_MAC_HIT_MASK:
		p_entry->da_an_mac_hit_mask = (__u8)p_value[0];
		break;
	case FE_HM_ORIG_LSPID_MASK:
		p_entry->orig_lspid_mask = (__u8)p_value[0];
		break;
	case FE_HM_L7_FIELD_MASK:
		p_entry->l7_field_mask = (__u8)p_value[0];
		break;
	case FE_HM_L7_FIELD_VLD_MASK:
		p_entry->l7_field_vld_mask = (__u8)p_value[0];
		break;
	case FE_HM_HDR_A_FLAGS_CRCERR_MASK:
		p_entry->hdr_a_flags_crcerr_mask = (__u8)p_value[0];
		break;
	case FE_HM_L3_CHKSUM_ERR_MASK:
		p_entry->l3_chksum_err_mask = (__u8)p_value[0];
		break;
	case FE_HM_L4_CHKSUM_ERR_MASK:
		p_entry->l4_chksum_err_mask = (__u8)p_value[0];
		break;
	case FE_HM_NOT_HDR_A_FLAGS_STSVLD_MASK:
		p_entry->not_hdr_a_flags_stsvld_mask = (__u8)p_value[0];
		break;
	case FE_HM_HASH_FID_MASK:
		p_entry->hash_fid_mask = (__u8)p_value[0];
		break;
	case FE_HM_L7_FIELD_SEL:
		p_entry->l7_field_sel = (__u8)p_value[0];
		break;
	case FE_HM_SA_BNG_MAC_SEL_MASK:
		p_entry->sa_bng_mac_sel_mask = (__u8)p_value[0];
		break;
	case FE_HM_SA_BNG_MAC_HIT_MASK:
		p_entry->sa_bng_mac_hit_mask = (__u8)p_value[0];
		break;
	case FE_HM_SPI_VLD_MASK:
		p_entry->spi_vld_mask = (__u8)p_value[0];
		break;
	case FE_HM_SPI_MASK:
		p_entry->spi_mask = (__u8)p_value[0];
		break;
	case FE_HM_IPV6_NDP_MASK:
		p_entry->ipv6_ndp_mask = (__u8)p_value[0];
		break;
	case FE_HM_IPV6_HBH_MASK:
		p_entry->ipv6_hbh_mask = (__u8)p_value[0];
		break;
	case FE_HM_IPV6_RH_MASK:
		p_entry->ipv6_rh_mask = (__u8)p_value[0];
		break;
	case FE_HM_IPV6_DOH_MASK:
		p_entry->ipv6_doh_mask = (__u8)p_value[0];
		break;
	case FE_HM_PPP_PROTOCOL_VLD_MASK:
		p_entry->ppp_protocol_vld_mask = (__u8)p_value[0];
		break;
	case FE_HM_PPP_PROTOCOL_MASK:
		p_entry->ppp_protocol_mask = (__u8)p_value[0];
		break;
	case FE_HM_PKTLEN_RNG_MATCH_VECTOR_MASK:
		p_entry->pktlen_rng_match_vector_mask = (__u8)p_value[0];
		break;
	case FE_HM_MCGID_MASK:
		p_entry->mcgid_mask = (__u16)p_value[0];
		break;
	case FE_HM_PARITY:
		p_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_HASHMASK_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_HASH_MASK, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_HASHMASK_LOCK, flags);

	return status;
} /* fe_hashmask_set_field */

static int fe_hashmask_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_hash_mask_entry_t *p_entry;
	__u32 hw_value=0;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_hashmask_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_HASHMASK_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_HASHMASK_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_entry = (fe_hash_mask_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_HASHMASK_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_HASH_MASK, idx, field,
			&hw_value);
	spin_unlock_irqrestore(FE_HASHMASK_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_HASHMASK_LOCK, flags);
	switch (field) {
	case FE_HM_MAC_DA_MASK:
		p_value[0] = (__u32)p_entry->mac_da_mask;
		break;
	case FE_HM_MAC_SA_MASK:
		p_value[0] = (__u32)p_entry->mac_sa_mask;
		break;
	case FE_HM_ETHERTYPE_MASK:
		p_value[0] = (__u32)p_entry->ethertype_mask;
		break;
	case FE_HM_LLC_TYPE_ENC_MSB_MASK:
		p_value[0] = (__u32)p_entry->llc_type_enc_msb_mask;
		break;
	case FE_HM_LLC_TYPE_ENC_LSB_MASK:
		p_value[0] = (__u32)p_entry->llc_type_enc_lsb_mask;
		break;
	case FE_HM_TPID_ENC_1_MSB_MASK:
		p_value[0] = (__u32)p_entry->tpid_enc_1_msb_mask;
		break;
	case FE_HM_TPID_ENC_1_LSB_MASK:
		p_value[0] = (__u32)p_entry->tpid_enc_1_lsb_mask;
		break;
	case FE_HM_8021P_1_MASK:
		p_value[0] = (__u32)p_entry->_8021p_1_mask;
		break;
	case FE_HM_DEI_1_MASK:
		p_value[0] = (__u32)p_entry->dei_1_mask;
		break;
	case FE_HM_VID_1_MASK:
		p_value[0] = (__u32)p_entry->vid_1_mask;
		break;
	case FE_HM_TPID_ENC_2_MSB_MASK:
		p_value[0] = (__u32)p_entry->tpid_enc_2_msb_mask;
		break;
	case FE_HM_TPID_ENC_2_LSB_MASK:
		p_value[0] = (__u32)p_entry->tpid_enc_2_lsb_mask;
		break;
	case FE_HM_8021P_2_MASK:
		p_value[0] = (__u32)p_entry->_8021p_2_mask;
		break;
	case FE_HM_DEI_2_MASK:
		p_value[0] = (__u32)p_entry->dei_2_mask;
		break;
	case FE_HM_VID_2_MASK:
		p_value[0] = (__u32)p_entry->vid_2_mask;
		break;
	case FE_HM_IP_DA_MASK:
		p_value[0] = (__u32)p_entry->ip_da_mask;
		break;
	case FE_HM_IP_SA_MASK:
		p_value[0] = (__u32)p_entry->ip_sa_mask;
		break;
	case FE_HM_IP_PROT_MASK:
		p_value[0] = (__u32)p_entry->ip_prot_mask;
		break;
	case FE_HM_DSCP_MASK:
		p_value[0] = (__u32)p_entry->dscp_mask;
		break;
	case FE_HM_ECN_MASK:
		p_value[0] = (__u32)p_entry->ecn_mask;
		break;
	case FE_HM_IP_FRAGMENT_MASK:
		p_value[0] = (__u32)p_entry->ip_fragment_mask;
		break;
	case FE_HM_KEYGEN_POLY_SEL:
		p_value[0] = (__u32)p_entry->keygen_poly_sel;
		break;
	case FE_HM_IPV6_FLOW_LBL_MASK:
		p_value[0] = (__u32)p_entry->ipv6_flow_lbl_mask;
		break;
	case FE_HM_IP_VER_MASK:
		p_value[0] = (__u32)p_entry->ip_ver_mask;
		break;
	case FE_HM_IP_VLD_MASK:
		p_value[0] = (__u32)p_entry->ip_vld_mask;
		break;
	case FE_HM_L4_PORTS_RNGD:
		p_value[0] = (__u32)p_entry->l4_ports_rngd;
		break;
	case FE_HM_L4_DP_MASK:
		p_value[0] = (__u32)p_entry->l4_dp_mask;
		break;
	case FE_HM_L4_SP_MASK:
		p_value[0] = (__u32)p_entry->l4_sp_mask;
		break;
	case FE_HM_TCP_CTRL_MASK:
		p_value[0] = (__u32)p_entry->tcp_ctrl_mask;
		break;
	case FE_HM_TCP_ECN_MASK:
		p_value[0] = (__u32)p_entry->tcp_ecn_mask;
		break;
	case FE_HM_L4_VLD_MASK:
		p_value[0] = (__u32)p_entry->l4_vld_mask;
		break;
	case FE_HM_LSPID_MASK:
		p_value[0] = (__u32)p_entry->lspid_mask;
		break;
	case FE_HM_FWDTYPE_MASK:
		p_value[0] = (__u32)p_entry->fwdtype_mask;
		break;
	case FE_HM_PPPOE_SESSION_ID_VLD_MASK:
		p_value[0] = (__u32)p_entry->pppoe_session_id_vld_mask;
		break;
	case FE_HM_PPPOE_SESSION_ID_MASK:
		p_value[0] = (__u32)p_entry->pppoe_session_id_mask;
		break;
	case FE_HM_RSVD_109:
		p_value[0] = (__u32)p_entry->rsvd_109;
		break;
	case FE_HM_RECIRC_IDX_MASK:
		p_value[0] = (__u32)p_entry->recirc_idx_mask;
		break;
	case FE_HM_MCIDX_MASK:
		p_value[0] = (__u32)p_entry->mcidx_mask;
		break;
	case FE_HM_MC_DA_MASK:
		p_value[0] = (__u32)p_entry->mc_da_mask;
		break;
	case FE_HM_BC_DA_MASK:
		p_value[0] = (__u32)p_entry->bc_da_mask;
		break;
	case FE_HM_DA_AN_MAC_SEL_MASK:
		p_value[0] = (__u32)p_entry->da_an_mac_sel_mask;
		break;
	case FE_HM_DA_AN_MAC_HIT_MASK:
		p_value[0] = (__u32)p_entry->da_an_mac_hit_mask;
		break;
	case FE_HM_ORIG_LSPID_MASK:
		p_value[0] = (__u32)p_entry->orig_lspid_mask;
		break;
	case FE_HM_L7_FIELD_MASK:
		p_value[0] = (__u32)p_entry->l7_field_mask;
		break;
	case FE_HM_L7_FIELD_VLD_MASK:
		p_value[0] = (__u32)p_entry->l7_field_vld_mask;
		break;
	case FE_HM_HDR_A_FLAGS_CRCERR_MASK:
		p_value[0] = (__u32)p_entry->hdr_a_flags_crcerr_mask;
		break;
	case FE_HM_L3_CHKSUM_ERR_MASK:
		p_value[0] = (__u32)p_entry->l3_chksum_err_mask;
		break;
	case FE_HM_L4_CHKSUM_ERR_MASK:
		p_value[0] = (__u32)p_entry->l4_chksum_err_mask;
		break;
	case FE_HM_NOT_HDR_A_FLAGS_STSVLD_MASK:
		p_value[0] = (__u32)p_entry->not_hdr_a_flags_stsvld_mask;
		break;
	case FE_HM_HASH_FID_MASK:
		p_value[0] = (__u32)p_entry->hash_fid_mask;
		break;
	case FE_HM_L7_FIELD_SEL:
		p_value[0] = (__u32)p_entry->l7_field_sel;
		break;
	case FE_HM_SA_BNG_MAC_SEL_MASK:
		p_value[0] = (__u32)p_entry->sa_bng_mac_sel_mask;
		break;
	case FE_HM_SA_BNG_MAC_HIT_MASK:
		p_value[0] = (__u32)p_entry->sa_bng_mac_hit_mask;
		break;
	case FE_HM_SPI_VLD_MASK:
		p_value[0] = (__u32)p_entry->spi_vld_mask;
		break;
	case FE_HM_SPI_MASK:
		p_value[0] = (__u32)p_entry->spi_mask;
		break;
	case FE_HM_IPV6_NDP_MASK:
		p_value[0] = (__u32)p_entry->ipv6_ndp_mask;
		break;
	case FE_HM_IPV6_HBH_MASK:
		p_value[0] = (__u32)p_entry->ipv6_hbh_mask;
		break;
	case FE_HM_IPV6_RH_MASK:
		p_value[0] = (__u32)p_entry->ipv6_rh_mask;
		break;
	case FE_HM_IPV6_DOH_MASK:
		p_value[0] = (__u32)p_entry->ipv6_doh_mask;
		break;
	case FE_HM_PPP_PROTOCOL_VLD_MASK:
		p_value[0] = (__u32)p_entry->ppp_protocol_vld_mask;
		break;
	case FE_HM_PPP_PROTOCOL_MASK:
		p_value[0] = (__u32)p_entry->ppp_protocol_mask;
		break;
	case FE_HM_PKTLEN_RNG_MATCH_VECTOR_MASK:
		p_value[0] = (__u32)p_entry->pktlen_rng_match_vector_mask;
		break;
	case FE_HM_MCGID_MASK:
		p_value[0] = (__u32)p_entry->mcgid_mask;
		break;
	case FE_HM_PARITY:
		p_value[0] = (__u32)p_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_HASHMASK_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_HASHMASK_LOCK, flags);

	if (hw_value != p_value[0]) {
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);
	}

	return FE_TABLE_OK;
} /* fe_hashmask_get_field */

static int fe_hashmask_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_hashmask_table_type);
} /* fe_hashmask_flush_table */

static int fe_hashmask_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_hashmask_table_type);
} /* fe_hashmask_get_avail_count */

static void _fe_hashmask_print_title(void)
{
	printk("\n\n ------------------- Hash Mask Table -----------------\n");
	printk("|------------------------------------------------------\n");
} /* _fe_hashmask_print_title */

static void _fe_hashmask_print_entry(unsigned int idx)
{
	fe_hash_mask_entry_t entry;
	int status;
	unsigned int count;

	status = fe_hashmask_get_entry(idx, &entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_hashmask_get_entry_refcnt(idx, &count);

	printk("| index: %04d | refcnt: %d ", idx, count);

	printk("mac_da_mask\t%x\tmac_sa_mask\t%x\tethtype_mask\t%x\n",
			entry.mac_da_mask, entry.mac_sa_mask,
			entry.ethertype_mask);

	printk("llc_type_enc_msb\t%x\tlsb\t%x\n",
			entry.llc_type_enc_msb_mask,
			entry.llc_type_enc_lsb_mask);

	printk("tpid_enc_1_msb_mask\t%x\ttpid_enc_1_lsb_mask\t%x\n",
			entry.tpid_enc_1_msb_mask, entry.tpid_enc_1_lsb_mask);
	printk("802_1p_1_mask\t%x\tdei_1_mask\t%x\tvid_1_mask\t%x\n",
			entry._8021p_1_mask, entry.dei_1_mask,
			entry.vid_1_mask);
	printk("tpid_enc_2_msb_mask\t%x\ttpid_enc_2_lsb_mask\t%x\n",
			entry.tpid_enc_2_msb_mask, entry.tpid_enc_2_lsb_mask);
	printk("802_1p_2_mask\t%x\tdei_2_mask %x\tvid_2_mask\t%x\n\n",
			entry._8021p_2_mask, entry.dei_2_mask,
			entry.vid_2_mask);

	printk("ip_da_mask\t%x\tip_sa_mask\t%x\tip_prot_mask\t%x\n",
			entry.ip_da_mask, entry.ip_sa_mask, entry.ip_prot_mask);
	printk("dscp mask\t%x\tecn_mask\t%x\tip_fragment_mask\t%x\n",
			entry.dscp_mask, entry.ecn_mask,
			entry.ip_fragment_mask);

	printk("keygen_pol_sel\t%x\tipv6_flow_lbl_mask\t%x\t",
			entry.keygen_poly_sel, entry.ipv6_flow_lbl_mask);
	printk("ip_ver_mask\t%x\tip_vld_mask\t%x\n",
			entry.ip_ver_mask, entry.ip_vld_mask);

	printk("l4_port_rngd\t%x\tl4_dp_mask\t%x\tl4_sp_mask\t%x\n",
			entry.l4_ports_rngd, entry.l4_dp_mask,
			entry.l4_sp_mask);
	printk("tcp_ctrl_mask\t%x\ttcp_ecn_mask\t%x\tl4_vld_mask\t%x\n",
			entry.tcp_ctrl_mask, entry.tcp_ecn_mask,
			entry.l4_vld_mask);

	printk("lspid_mask\t%x\tfwd_type_mask\t%x\n",
			entry.lspid_mask, entry.fwdtype_mask);
	printk("pppoe_session_id_vld_mask\t%x\tpppoe_session_id_mask\t%x\n",
			entry.pppoe_session_id_vld_mask,
			entry.pppoe_session_id_mask);
	printk("mcgid_mask\t%x\trecirc_idx_mask\t%x\tmcidx_mask\t%x\n\n",
			entry.mcgid_mask, entry.recirc_idx_mask,
			entry.mcidx_mask);

	printk("mc_da_mask\t%x\tbc_da_mask\t%x\t",
		entry.mc_da_mask, entry.bc_da_mask);
	printk("da_an_mac_sel_mask\t%x\tda_an_mac_hit_mask\t%x\n",
			entry.da_an_mac_sel_mask, entry.da_an_mac_hit_mask);
	printk("orig_lspid_mask\t%x\tl7_field_vld_mask\t%x\t",
			entry.orig_lspid_mask, entry.l7_field_vld_mask);
	printk("hdr_a_flags_crcerr_mask\t%x\n", entry.hdr_a_flags_crcerr_mask);
	printk("l3_csum_err_mask\t%x\tl4_csum_err_mask\t%x\t",
			entry.l3_chksum_err_mask, entry.l4_chksum_err_mask);
	printk("not_hdr_a_flags_stsvld_mask\t%x\n",
			entry.not_hdr_a_flags_stsvld_mask);

	printk("hash_fid_mask\t%x\tl7_field_sel\t%x\t",
			entry.hash_fid_mask, entry.l7_field_sel);
	printk("sa_bng_mac_sel_mask\t%x\tsa_bng_mac_hit_mask\t%x\n",
			entry.sa_bng_mac_sel_mask, entry.sa_bng_mac_hit_mask);
	printk("spi_vld_mask\t%x\tspi_mask\t%x\t",
			entry.spi_vld_mask, entry.spi_mask);
	printk("ipv6_ndp_mask\t%x\tipv6_hbh_mask\t%x\n",
			entry.ipv6_ndp_mask, entry.ipv6_hbh_mask);
	printk("ipv6_rh_mask\t%x\tipv6_doh_mask\t%x\t",
			entry.ipv6_rh_mask, entry.ipv6_doh_mask);
	printk("ppp_protocol_vld_mask\t%x\tppp_protocol_mask\t%x\n",
			entry.ppp_protocol_vld_mask, entry.ppp_protocol_mask);
	printk("pktlen_rng_match_vec_mask\t%x\tmcgid_mask\t%x\t",
			entry.pktlen_rng_match_vector_mask, entry.mcgid_mask);
	printk("parity\t%x\n", entry.parity);
} /* _fe_hashmask_print_entry */

static void fe_hashmask_print_entry(unsigned int idx)
{
	if (idx >= FE_HASH_MASK_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_hashmask_print_title();
	_fe_hashmask_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_hashmask_print_entry */

static void fe_hashmask_print_range(unsigned int start_idx,
		unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_HASH_MASK_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_hashmask_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_hashmask_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_hashmask_print_range */

static void fe_hashmask_print_table(void)
{
	unsigned int i;

	_fe_hashmask_print_title();
	for (i = 0; i < cs_fe_hashmask_table_type.max_entry; i++) {
		_fe_hashmask_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_hashmask_print_table */


static cs_fe_table_t cs_fe_hashmask_table_type = {
	.type_id = FE_TABLE_HASH_MASK,
	.max_entry = FE_HASH_MASK_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_hash_mask_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_hashmask_convert_sw_to_hw_data,
		.alloc_entry = fe_hashmask_alloc_entry,
		.set_entry = fe_hashmask_set_entry,
		.add_entry = fe_hashmask_add_entry,
		.del_entry_by_idx = fe_hashmask_del_entry_by_idx,
		.del_entry = fe_hashmask_del_entry,
		.find_entry = fe_hashmask_find_entry,
		.get_entry = fe_hashmask_get_entry,
		.inc_entry_refcnt = fe_hashmask_inc_entry_refcnt,
		.dec_entry_refcnt = fe_hashmask_dec_entry_refcnt,
		.get_entry_refcnt = fe_hashmask_get_entry_refcnt,
		.set_field = fe_hashmask_set_field,
		.get_field = fe_hashmask_get_field,
		.flush_table = fe_hashmask_flush_table,
		.get_avail_count = fe_hashmask_get_avail_count,
		.print_entry = fe_hashmask_print_entry,
		.print_range = fe_hashmask_print_range,
		.print_table = fe_hashmask_print_table,
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

int cs_fe_ioctl_hashmask(struct net_device *dev, void *pdata, void *cmd)
{
	fe_hash_mask_entry_t *p_rslt = (fe_hash_mask_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_hashmask_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_hashmask_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_hashmask_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** HASHMASK entry 0 ~ %d ***\n",
				FE_HASH_MASK_ENTRY_MAX - 1);
		fe_hashmask_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_hashmask */

/* this API will initialize hashmask table */
int cs_fe_table_hashmask_init(void)
{
	int ret;
	unsigned int index;

	spin_lock_init(FE_HASHMASK_LOCK);

	cs_fe_hashmask_table_type.content_table = cs_table_alloc(
			cs_fe_hashmask_table_type.max_entry);
	if (cs_fe_hashmask_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_hashmask_table_type.type_id,
			&cs_fe_hashmask_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register HASHMASK table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_hashmask_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_hashmask_flush_table();

	/* Reserve a default entry */
	ret = fe_hashmask_alloc_entry(&index, 0);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to allocate default HashMask entry\n",
				__func__, __LINE__);
		return -1;
	}
	if (index != 0)
		printk("%s:%d:the default index is not 0 (%d)!\n",
				__func__, __LINE__, index);

	return FE_TABLE_OK;
} /* cs_fe_table_hashmask_init */
EXPORT_SYMBOL(cs_fe_table_hashmask_init);

void cs_fe_table_hashmask_exit(void)
{
	fe_hashmask_flush_table();

	if (cs_fe_hashmask_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_hashmask_table_type.content_table);
	cs_fe_table_unregister(cs_fe_hashmask_table_type.type_id);
} /* cs_fe_table_hashmask_exit */
EXPORT_SYMBOL(cs_fe_table_hashmask_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_hashmask_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_hashmask_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_hashmask_alloc_entry_ut);

int fe_hashmask_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_hashmask_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_hashmask_convert_sw_to_hw_data_ut);

int fe_hashmask_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_hashmask_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_hashmask_set_entry_ut);

int fe_hashmask_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_hashmask_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_hashmask_add_entry_ut);

int fe_hashmask_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_hashmask_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_hashmask_del_entry_by_idx_ut);

int fe_hashmask_del_entry_ut(void *entry, bool f_force)
{
	return fe_hashmask_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_hashmask_del_entry_ut);

int fe_hashmask_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_hashmask_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_hashmask_find_entry_ut);

int fe_hashmask_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_hashmask_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_hashmask_get_entry_ut);

int fe_hashmask_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_hashmask_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_hashmask_inc_entry_refcnt_ut);

int fe_hashmask_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_hashmask_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_hashmask_dec_entry_refcnt_ut);

int fe_hashmask_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_hashmask_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_hashmask_get_entry_refcnt_ut);

int fe_hashmask_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_hashmask_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_hashmask_set_field_ut);

int fe_hashmask_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_hashmask_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_hashmask_get_field_ut);

int fe_hashmask_flush_table_ut(void)
{
	return fe_hashmask_flush_table();
}
EXPORT_SYMBOL(fe_hashmask_flush_table_ut);

int fe_hashmask_get_avail_count_ut(void)
{
	return fe_hashmask_get_avail_count();
}
EXPORT_SYMBOL(fe_hashmask_get_avail_count_ut);

void fe_hashmask_print_entry_ut(unsigned int idx)
{
	fe_hashmask_print_entry(idx);
}
EXPORT_SYMBOL(fe_hashmask_print_entry_ut);

void fe_hashmask_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_hashmask_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_hashmask_print_range_ut);

void fe_hashmask_print_table_ut(void)
{
	fe_hashmask_print_table();
}
EXPORT_SYMBOL(fe_hashmask_print_table_ut);


#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

