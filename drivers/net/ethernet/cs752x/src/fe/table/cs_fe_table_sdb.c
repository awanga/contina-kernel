/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_sdb.c
 *
 * $Id: cs_fe_table_sdb.c,v 1.5 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains SDB Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_sdb_table_type;

#define FE_SDB_TABLE_PTR	(cs_fe_sdb_table_type.content_table)
#define FE_SDB_LOCK		&(cs_fe_sdb_table_type.lock)

static int fe_sdb_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_sdb_table_type, rslt_idx,
			start_offset);
} /* fe_sdb_alloc_entry */

static int fe_sdb_convert_sw_to_hw_data(void *sw_entry, __u32 *p_data_array,
		unsigned int size)
{
	__u32 value;
	fe_sdb_entry_t *entry = (fe_sdb_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->sdb_tuple[0].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_0, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[0].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_0, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[0].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_0, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[1].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_1, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[1].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_1, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[1].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_1, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[2].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_2, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[2].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_2, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[2].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_2, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[3].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_3, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[3].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_3, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[3].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_3, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[4].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_4, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[4].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_4, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[4].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_4, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[5].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_5, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[5].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_5, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[5].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_5, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[6].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_6, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[6].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_6, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[6].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_6, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[7].enable;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_EN_7, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[7].mask_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_MASK_PTR_7, &value, p_data_array, size);

	value = (__u32)entry->sdb_tuple[7].priority;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTUPL_PRI_7, &value, p_data_array, size);

	value = (__u32)entry->lpm_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_EN, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v4[0].start_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV4_START_PTR0, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v4[0].end_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV4_END_PTR0, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v4[1].start_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV4_START_PTR1, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v4[1].end_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV4_END_PTR1, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v6[0].start_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV6_START_PTR0, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v6[0].end_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV6_END_PTR0, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v6[1].start_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV6_START_PTR1, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v6[1].end_ptr;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV6_END_PTR1, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v4[0].lpm_ptr_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV4_PTR0_EN, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v4[1].lpm_ptr_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV4_PTR1_EN, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v6[0].lpm_ptr_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV6_PTR0_EN, &value, p_data_array, size);

	value = (__u32)entry->sdb_lpm_v6[1].lpm_ptr_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_LPM_IPV6_PTR1_EN, &value, p_data_array, size);

	value = (__u32)entry->pvid.pvid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_PVID, &value, p_data_array, size);

	value = (__u32)entry->pvid.pvid_tpid_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_PVID_TPID_ENC, &value, p_data_array, size);

	value = (__u32)entry->pvid.pvid_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_PVID_EN, &value, p_data_array, size);

	value = (__u32)entry->vlan.vlan_ingr_membership_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_VLAN_INGR_MBRSHP_EN, &value, p_data_array, size);

	value = (__u32)entry->vlan.vlan_egr_membership_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_VLAN_EGR_MBRSHP_EN, &value, p_data_array, size);

	value = (__u32)entry->vlan.vlan_egr_untag_chk_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_VLAN_EGRUNTAG_CHK_EN, &value, p_data_array,
			size);

	value = (__u32)entry->misc.use_egrlen_pkttype_policer;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_USE_EGRLEN_PKTTYPE_POLICER, &value, p_data_array,
			size);

	value = (__u32)entry->misc.use_egrlen_src_policer;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_USE_EGRLEN_SRC_POLICER, &value, p_data_array,
			size);

	value = (__u32)entry->misc.use_egrlen_flow_policer;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_USE_EGRLEN_FLOW_POLICER, &value, p_data_array,
			size);

	value = (__u32)entry->misc.ttl_hop_limit_zero_discard_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_TTL_HOPLIMIT_ZERO_DISCARD_EN, &value,
			p_data_array, size);

	value = (__u32)entry->misc.key_rule;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_KEYRULE, &value, p_data_array, size);

	value = (__u32)entry->misc.uu_flowidx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_UU_FLOWIDX, &value, p_data_array, size);

	value = (__u32)entry->misc.hash_sts_update_ctrl;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_HTPL_STSUPDT_CNTL, &value, p_data_array, size);

	value = (__u32)entry->misc.bc_flowidx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_BC_FLOWIDX, &value, p_data_array, size);

	value = (__u32)entry->misc.um_flowidx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_UM_FLOWIDX, &value, p_data_array, size);

	value = (__u32)entry->misc.rsvd_202;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_RSVD_202, &value, p_data_array, size);

	value = (__u32)entry->misc.drop;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_DROP, &value, p_data_array, size);

	value = (__u32)entry->misc.egr_vln_ingr_mbrshp_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_EGRVLN_INGR_MBRSHP_EN, &value, p_data_array,
			size);

	value = (__u32)entry->misc.acl_dsbl;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_ACL_DISABLE, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_SDB,
			FE_SDB_MEM_PARITY, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_sdb_convert_sw_to_hw_data */

static int fe_sdb_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_sdb_table_type, idx, entry);
} /* fe_sdb_set_entry */

static int fe_sdb_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_sdb_table_type, entry, rslt_idx);
} /* fe_sdb_add_entry */

static int fe_sdb_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_sdb_table_type, idx, f_force);
} /* fe_sdb_del_entry_by_idx */

static int fe_sdb_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_sdb_table_type, entry, f_force);
} /* fe_sdb_del_entry */

static int fe_sdb_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_sdb_table_type, entry, rslt_idx,
			start_offset);
} /* fe_sdb_find_entry */

static int fe_sdb_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_sdb_table_type, idx, entry);
} /* fe_sdb_get_entry */

static int fe_sdb_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_sdb_table_type, idx);
} /* fe_sdb_inc_entry_refcnt */

static int fe_sdb_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_sdb_table_type, idx);
} /* fe_sdb_dec_entry_refcnt */

static int fe_sdb_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_sdb_table_type, idx, p_cnt);
} /* fe_sdb_get_entry_refcnt */

static int fe_sdb_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_sdb_entry_t *p_sdb_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_sdb_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_SDB_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_SDB_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_SDB_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_sdb_table_type);
		spin_unlock_irqrestore(FE_SDB_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_sdb_entry = (fe_sdb_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_sdb_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_SDB_LOCK, flags);
	switch (field) {
	case FE_SDB_HTUPL_EN_0:
		p_sdb_entry->sdb_tuple[0].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_0:
		p_sdb_entry->sdb_tuple[0].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_0:
		p_sdb_entry->sdb_tuple[0].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_1:
		p_sdb_entry->sdb_tuple[1].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_1:
		p_sdb_entry->sdb_tuple[1].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_1:
		p_sdb_entry->sdb_tuple[1].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_2:
		p_sdb_entry->sdb_tuple[2].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_2:
		p_sdb_entry->sdb_tuple[2].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_2:
		p_sdb_entry->sdb_tuple[2].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_3:
		p_sdb_entry->sdb_tuple[3].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_3:
		p_sdb_entry->sdb_tuple[3].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_3:
		p_sdb_entry->sdb_tuple[3].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_4:
		p_sdb_entry->sdb_tuple[4].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_4:
		p_sdb_entry->sdb_tuple[4].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_4:
		p_sdb_entry->sdb_tuple[4].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_5:
		p_sdb_entry->sdb_tuple[5].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_5:
		p_sdb_entry->sdb_tuple[5].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_5:
		p_sdb_entry->sdb_tuple[5].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_6:
		p_sdb_entry->sdb_tuple[6].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_6:
		p_sdb_entry->sdb_tuple[6].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_6:
		p_sdb_entry->sdb_tuple[6].priority = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_EN_7:
		p_sdb_entry->sdb_tuple[7].enable = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_MASK_PTR_7:
		p_sdb_entry->sdb_tuple[7].mask_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_HTUPL_PRI_7:
		p_sdb_entry->sdb_tuple[7].priority = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_EN:
		p_sdb_entry->lpm_en = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV4_START_PTR0:
		p_sdb_entry->sdb_lpm_v4[0].start_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV4_END_PTR0:
		p_sdb_entry->sdb_lpm_v4[0].end_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV4_START_PTR1:
		p_sdb_entry->sdb_lpm_v4[1].start_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV4_END_PTR1:
		p_sdb_entry->sdb_lpm_v4[1].end_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV6_START_PTR0:
		p_sdb_entry->sdb_lpm_v6[0].start_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV6_END_PTR0:
		p_sdb_entry->sdb_lpm_v6[0].end_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV6_START_PTR1:
		p_sdb_entry->sdb_lpm_v6[1].start_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV6_END_PTR1:
		p_sdb_entry->sdb_lpm_v6[1].end_ptr = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV4_PTR0_EN:
		p_sdb_entry->sdb_lpm_v4[0].lpm_ptr_en = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV4_PTR1_EN:
		p_sdb_entry->sdb_lpm_v4[1].lpm_ptr_en = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV6_PTR0_EN:
		p_sdb_entry->sdb_lpm_v6[0].lpm_ptr_en = (__u8)p_value[0];
		break;
	case FE_SDB_LPM_IPV6_PTR1_EN:
		p_sdb_entry->sdb_lpm_v6[1].lpm_ptr_en = (__u8)p_value[0];
		break;
	case FE_SDB_PVID:
		p_sdb_entry->pvid.pvid = (__u16)p_value[0];
		break;
	case FE_SDB_PVID_TPID_ENC:
		p_sdb_entry->pvid.pvid_tpid_enc = (__u8)p_value[0];
		break;
	case FE_SDB_PVID_EN:
		p_sdb_entry->pvid.pvid_en = (__u8)p_value[0];
		break;
	case FE_SDB_VLAN_INGR_MBRSHP_EN:
		p_sdb_entry->vlan.vlan_ingr_membership_en = (__u8)p_value[0];
		break;
	case FE_SDB_VLAN_EGR_MBRSHP_EN:
		p_sdb_entry->vlan.vlan_egr_membership_en = (__u8)p_value[0];
		break;
	case FE_SDB_VLAN_EGRUNTAG_CHK_EN:
		p_sdb_entry->vlan.vlan_egr_untag_chk_en = (__u8)p_value[0];
		break;
	case FE_SDB_USE_EGRLEN_PKTTYPE_POLICER:
		p_sdb_entry->misc.use_egrlen_pkttype_policer = (__u8)p_value[0];
		break;
	case FE_SDB_USE_EGRLEN_SRC_POLICER:
		p_sdb_entry->misc.use_egrlen_src_policer = (__u8)p_value[0];
		break;
	case FE_SDB_USE_EGRLEN_FLOW_POLICER:
		p_sdb_entry->misc.use_egrlen_flow_policer = (__u8)p_value[0];
		break;
	case FE_SDB_TTL_HOPLIMIT_ZERO_DISCARD_EN:
		p_sdb_entry->misc.ttl_hop_limit_zero_discard_en =
			(__u8)p_value[0];
		break;
	case FE_SDB_KEYRULE:
		p_sdb_entry->misc.key_rule = (__u8)p_value[0];
		break;
	case FE_SDB_UU_FLOWIDX:
		p_sdb_entry->misc.uu_flowidx = (__u16)p_value[0];
		break;
	case FE_SDB_HTPL_STSUPDT_CNTL:
		p_sdb_entry->misc.hash_sts_update_ctrl = (__u8)p_value[0];
		break;
	case FE_SDB_BC_FLOWIDX:
		p_sdb_entry->misc.bc_flowidx = (__u16)p_value[0];
		break;
	case FE_SDB_UM_FLOWIDX:
		p_sdb_entry->misc.um_flowidx = (__u16)p_value[0];
		break;
	case FE_SDB_RSVD_202:
		p_sdb_entry->misc.rsvd_202 = (__u8)p_value[0];
		break;
	case FE_SDB_DROP:
		p_sdb_entry->misc.drop = (__u8)p_value[0];
		break;
	case FE_SDB_EGRVLN_INGR_MBRSHP_EN:
		p_sdb_entry->misc.egr_vln_ingr_mbrshp_en = (__u8)p_value[0];
		break;
	case FE_SDB_ACL_DISABLE:
		p_sdb_entry->misc.acl_dsbl = (__u8)p_value[0];
		break;
	case FE_SDB_MEM_PARITY:
		p_sdb_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_SDB_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_SDB, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_SDB_LOCK, flags);

	return status;
} /* fe_sdb_set_field */

static int fe_sdb_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_sdb_entry_t *p_sdb_entry;
	__u32 hw_value=0;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_sdb_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_SDB_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_SDB_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_sdb_entry = (fe_sdb_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_sdb_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_SDB_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, field,
			&hw_value);
	spin_unlock_irqrestore(FE_SDB_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_SDB_LOCK, flags);
	switch (field) {
	case FE_SDB_HTUPL_EN_0:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[0].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_0:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[0].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_0:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[0].priority;
		break;
	case FE_SDB_HTUPL_EN_1:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[1].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_1:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[1].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_1:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[1].priority;
		break;
	case FE_SDB_HTUPL_EN_2:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[2].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_2:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[2].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_2:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[2].priority;
		break;
	case FE_SDB_HTUPL_EN_3:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[3].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_3:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[3].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_3:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[3].priority;
		break;
	case FE_SDB_HTUPL_EN_4:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[4].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_4:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[4].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_4:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[4].priority;
		break;
	case FE_SDB_HTUPL_EN_5:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[5].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_5:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[5].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_5:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[5].priority;
		break;
	case FE_SDB_HTUPL_EN_6:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[6].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_6:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[6].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_6:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[6].priority;
		break;
	case FE_SDB_HTUPL_EN_7:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[7].enable;
		break;
	case FE_SDB_HTUPL_MASK_PTR_7:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[7].mask_ptr;
		break;
	case FE_SDB_HTUPL_PRI_7:
		p_value[0] = (__u32)p_sdb_entry->sdb_tuple[7].priority;
		break;
	case FE_SDB_LPM_EN:
		p_value[0] = (__u32)p_sdb_entry->lpm_en;
		break;
	case FE_SDB_LPM_IPV4_START_PTR0:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v4[0].start_ptr;
		break;
	case FE_SDB_LPM_IPV4_END_PTR0:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v4[0].end_ptr;
		break;
	case FE_SDB_LPM_IPV4_START_PTR1:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v4[1].start_ptr;
		break;
	case FE_SDB_LPM_IPV4_END_PTR1:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v4[1].end_ptr;
		break;
	case FE_SDB_LPM_IPV6_START_PTR0:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v6[0].start_ptr;
		break;
	case FE_SDB_LPM_IPV6_END_PTR0:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v6[0].end_ptr;
		break;
	case FE_SDB_LPM_IPV6_START_PTR1:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v6[1].start_ptr;
		break;
	case FE_SDB_LPM_IPV6_END_PTR1:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v6[1].end_ptr;
		break;
	case FE_SDB_LPM_IPV4_PTR0_EN:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v4[0].lpm_ptr_en;
		break;
	case FE_SDB_LPM_IPV4_PTR1_EN:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v4[1].lpm_ptr_en;
		break;
	case FE_SDB_LPM_IPV6_PTR0_EN:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v6[0].lpm_ptr_en;
		break;
	case FE_SDB_LPM_IPV6_PTR1_EN:
		p_value[0] = (__u32)p_sdb_entry->sdb_lpm_v6[1].lpm_ptr_en;
		break;
	case FE_SDB_PVID:
		p_value[0] = (__u32)p_sdb_entry->pvid.pvid;
		break;
	case FE_SDB_PVID_TPID_ENC:
		p_value[0] = (__u32)p_sdb_entry->pvid.pvid_tpid_enc;
		break;
	case FE_SDB_PVID_EN:
		p_value[0] = (__u32)p_sdb_entry->pvid.pvid_en;
		break;
	case FE_SDB_VLAN_INGR_MBRSHP_EN:
		p_value[0] = (__u32)p_sdb_entry->vlan.vlan_ingr_membership_en;
		break;
	case FE_SDB_VLAN_EGR_MBRSHP_EN:
		p_value[0] = (__u32)p_sdb_entry->vlan.vlan_egr_membership_en;
		break;
	case FE_SDB_VLAN_EGRUNTAG_CHK_EN:
		p_value[0] = (__u32)p_sdb_entry->vlan.vlan_egr_untag_chk_en;
		break;
	case FE_SDB_USE_EGRLEN_PKTTYPE_POLICER:
		p_value[0] = (__u32)p_sdb_entry->misc.use_egrlen_pkttype_policer;
		break;
	case FE_SDB_USE_EGRLEN_SRC_POLICER:
		p_value[0] = (__u32)p_sdb_entry->misc.use_egrlen_src_policer;
		break;
	case FE_SDB_USE_EGRLEN_FLOW_POLICER:
		p_value[0] = (__u32)p_sdb_entry->misc.use_egrlen_flow_policer;
		break;
	case FE_SDB_TTL_HOPLIMIT_ZERO_DISCARD_EN:
		p_value[0] = (__u32)
			p_sdb_entry->misc.ttl_hop_limit_zero_discard_en;
		break;
	case FE_SDB_KEYRULE:
		p_value[0] = (__u32)p_sdb_entry->misc.key_rule;
		break;
	case FE_SDB_UU_FLOWIDX:
		p_value[0] = (__u32)p_sdb_entry->misc.uu_flowidx;
		break;
	case FE_SDB_HTPL_STSUPDT_CNTL:
		p_value[0] = (__u32)p_sdb_entry->misc.hash_sts_update_ctrl;
		break;
	case FE_SDB_BC_FLOWIDX:
		p_value[0] = (__u32)p_sdb_entry->misc.bc_flowidx;
		break;
	case FE_SDB_UM_FLOWIDX:
		p_value[0] = (__u32)p_sdb_entry->misc.um_flowidx;
		break;
	case FE_SDB_RSVD_202:
		p_value[0] = (__u32)p_sdb_entry->misc.rsvd_202;
		break;
	case FE_SDB_DROP:
		p_value[0] = (__u32)p_sdb_entry->misc.drop;
		break;
	case FE_SDB_EGRVLN_INGR_MBRSHP_EN:
		p_value[0] = (__u32)p_sdb_entry->misc.egr_vln_ingr_mbrshp_en;
		break;
	case FE_SDB_ACL_DISABLE:
		p_value[0] = (__u32)p_sdb_entry->misc.acl_dsbl;
		break;
	case FE_SDB_MEM_PARITY:
		p_value[0] = (__u32)p_sdb_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_SDB_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_SDB_LOCK, flags);

	if (hw_value != p_value[0])
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);

	return FE_TABLE_OK;
} /* fe_sdb_get_field */

static int fe_sdb_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_sdb_table_type);
} /* fe_sdb_flush_table */

static int fe_sdb_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_sdb_table_type);
} /* fe_sdb_get_avail_count */

static void _fe_sdb_print_title(void)
{
	printk("\n\n ------------------ SDB Table -------------------------\n");
	printk("!----------------------------------------------------------\n");
} /* _fe_sdb_print_title */

static void _fe_sdb_print_entry(unsigned int idx)
{
	fe_sdb_entry_t sdb_entry, *p_sdb;
	__u32 value;
	int status;
	unsigned int count;

	status = fe_sdb_get_entry(idx, &sdb_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_sdb_get_entry_refcnt(idx, &count);

	p_sdb = &sdb_entry;
	printk("| index: %04d | refcnt: %d\n", idx, count);

	/* Tuple#0 */
	printk(" |- TUPLE#0_EN: %01x (HW ", p_sdb->sdb_tuple[0].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_0,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_0, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[0].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[0].priority);

	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_0, &value);
	printk("%01x).\n", value);

	/* Tuple#1 */
	printk(" |- TUPLE#1_EN: %01x (HW ", p_sdb->sdb_tuple[1].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_1,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_1, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[1].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[1].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_1, &value);
	printk("%01x).\n", value);

	/* Tuple#2 */
	printk(" |- TUPLE#2_EN: %01x (HW ", p_sdb->sdb_tuple[2].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_2,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_2, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[2].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[2].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_2, &value);
	printk("%01x).\n", value);

	/* Tuple#3 */
	printk(" |- TUPLE#3_EN: %01x (HW ", p_sdb->sdb_tuple[3].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_3,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_3, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[3].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[3].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_3, &value);
	printk("%01x).\n", value);

	/* Tuple#4 */
	printk(" |- TUPLE#4_EN: %01x (HW ", p_sdb->sdb_tuple[4].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_4,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_4, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[4].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[4].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_4, &value);
	printk("%01x).\n", value);

	/* Tuple#5 */
	printk(" |- TUPLE#5_EN: %01x (HW ", p_sdb->sdb_tuple[5].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_5,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_5, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[5].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[5].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_5, &value);
	printk("%01x).\n", value);

	/* Tuple#6 */
	printk(" |- TUPLE#6_EN: %01x (HW ", p_sdb->sdb_tuple[6].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_6,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_6, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[6].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[6].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_6, &value);
	printk("%06x).\n", value);

	/* Tuple#7 */
	printk(" |- TUPLE#7_EN: %01x (HW ", p_sdb->sdb_tuple[7].enable);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_HTUPL_EN_7,
			&value);
	printk("%01x). MASK_PTR: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_MASK_PTR_7, &value);
	printk("%04x (HW %04x). ", p_sdb->sdb_tuple[7].mask_ptr, value);
	printk("PRI: %01x (HW ", p_sdb->sdb_tuple[7].priority);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTUPL_PRI_7, &value);
	printk("%06x).\n", value);

	/* LPM */
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_LPM_EN,
			&value);
	printk(" |- LPM EN: %01x (HW %01x).\n", p_sdb->lpm_en, value);
	/* IPv4 Ptr#0 */
	printk("  |- IPv4 PTR#0, EN: %01x (", p_sdb->sdb_lpm_v4[0].lpm_ptr_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV4_PTR0_EN, &value);
	printk("HW %01x). Start: %02x", value, p_sdb->sdb_lpm_v4[0].start_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV4_START_PTR0, &value);
	printk(" (HW %02x). End: %02x (", value, p_sdb->sdb_lpm_v4[0].end_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV4_END_PTR0, &value);
	printk("HW %02x).\n", value);
	/* IPv4 Ptr#1 */
	printk("  |- IPv4 PTR#1, EN: %01x (", p_sdb->sdb_lpm_v4[1].lpm_ptr_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV4_PTR1_EN, &value);
	printk("HW %01x). Start: %02x", value, p_sdb->sdb_lpm_v4[1].start_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV4_START_PTR1, &value);
	printk(" (HW %02x). End: %02x (", value, p_sdb->sdb_lpm_v4[1].end_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV4_END_PTR1, &value);
	printk("HW %02x).\n", value);
	/* IPv6 Ptr#0 */
	printk("  |- IPv4 PTR#0, EN: %01x (", p_sdb->sdb_lpm_v6[0].lpm_ptr_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV6_PTR0_EN, &value);
	printk("HW %01x). Start: %02x", value, p_sdb->sdb_lpm_v6[0].start_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV6_START_PTR0, &value);
	printk(" (HW %02x). End: %02x (", value, p_sdb->sdb_lpm_v6[0].end_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV6_END_PTR0, &value);
	printk("HW %02x).\n", value);
	/* IPv6 Ptr#1 */
	printk("  |- IPv6 PTR#1, EN: %01x (", p_sdb->sdb_lpm_v6[1].lpm_ptr_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV6_PTR1_EN, &value);
	printk("HW %01x). Start: %02x", value, p_sdb->sdb_lpm_v6[1].start_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV6_START_PTR1, &value);
	printk(" (HW %02x). End: %02x (", value, p_sdb->sdb_lpm_v6[1].end_ptr);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_LPM_IPV6_END_PTR1, &value);
	printk("HW %02x).\n", value);

	/* PVID */
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_PVID, &value);
	printk(" |- PVID, PVID: %04x (HW %04x). ", p_sdb->pvid.pvid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_PVID_EN,
			&value);
	printk("EN: %01x (HW %01x). ", p_sdb->pvid.pvid_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_PVID_TPID_ENC, &value);
	printk("TPID ENC: %01x (HW %01x).\n", p_sdb->pvid.pvid_tpid_enc, value);

	/* VLAN */
	printk(" |- VLAN: INGR_MBRSHP_EN: ");
	printk("%01x (HW ", p_sdb->vlan.vlan_ingr_membership_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_VLAN_INGR_MBRSHP_EN, &value);
	printk("%04x). EGR_MBRSHP_EN: ", value);
	printk("%01x (HW ", p_sdb->vlan.vlan_egr_membership_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_VLAN_EGR_MBRSHP_EN, &value);
	printk("%04x). EGR_UNTAG_CHK_EN: ", value);
	printk("%01x (HW ", p_sdb->vlan.vlan_egr_untag_chk_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_VLAN_EGRUNTAG_CHK_EN, &value);
	printk("%04x).\n", value);

	/* Miscell */
	printk(" |- MISC: PKTTYPE_POLICER: ");
	printk("%01x (HW", p_sdb->misc.use_egrlen_pkttype_policer);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_USE_EGRLEN_PKTTYPE_POLICER, &value);
	printk(" %01x). SRC_POLICER: ", value);
	printk("%01x (HW", p_sdb->misc.use_egrlen_src_policer);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_USE_EGRLEN_SRC_POLICER, &value);
	printk(" %01x). FLOW_POLICER: ", value);
	printk("%01x (HW", p_sdb->misc.use_egrlen_flow_policer);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_USE_EGRLEN_FLOW_POLICER, &value);
	printk(" %01x).\n", value);
	printk("HOPLIMIT_ZERO_DISCARD_EN: ");
	printk("%01x ", p_sdb->misc.ttl_hop_limit_zero_discard_en);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_TTL_HOPLIMIT_ZERO_DISCARD_EN, &value);
	printk("(HW %01x)\nSDB Key Rule: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_KEYRULE, &value);
	printk("%02x (HW %02x).\n", p_sdb->misc.key_rule, value);
	printk("UU FlowIdx: %04x ", p_sdb->misc.uu_flowidx);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_UU_FLOWIDX, &value);
	printk(" (HW %04x). BC FlowIdx: ", value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_BC_FLOWIDX, &value);
	printk("%04x (HW %04x). ", p_sdb->misc.bc_flowidx, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_UM_FLOWIDX, &value);
	printk("UM FlowIdx: %04x (HW %04x).\n", p_sdb->misc.um_flowidx, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_HTPL_STSUPDT_CNTL, &value);
	printk("Hash Status Update Ctrl: %02x (HW %02x). ",
			p_sdb->misc.hash_sts_update_ctrl, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_RSVD_202, &value);
	printk("RSVD_202: %01x (HW %01x). ", p_sdb->misc.rsvd_202, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_DROP, &value);
	printk("DROP: %01x (HW %01x).\n", p_sdb->misc.drop, value);
	printk("EGRVLN_INGR_MBRSHP_EN: ");
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_EGRVLN_INGR_MBRSHP_EN, &value);
	printk("%01x (HW%01x). ", p_sdb->misc.egr_vln_ingr_mbrshp_en,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx,
			FE_SDB_ACL_DISABLE, &value);
	printk("ACL DISABLE: %01x (HW %01x). ", p_sdb->misc.acl_dsbl, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_SDB, idx, FE_SDB_MEM_PARITY,
			&value);
	printk("MEM PARITY: %01x (HW %01x).\n", p_sdb->parity, value);
	printk("|---------------------------------------------------|\n");
} /* _fe_sdb_print_entry */

static void fe_sdb_print_entry(unsigned int idx)
{
	if (idx >= FE_SDB_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_sdb_print_title();
	_fe_sdb_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_sdb_print_entry */

static void fe_sdb_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_SDB_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_sdb_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_sdb_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_sdb_print_range */

static void fe_sdb_print_table(void)
{
	unsigned int i;

	_fe_sdb_print_title();
	for (i = 0; i < cs_fe_sdb_table_type.max_entry; i++) {
		_fe_sdb_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_sdb_print_table */


static cs_fe_table_t cs_fe_sdb_table_type = {
	.type_id = FE_TABLE_SDB,
	.max_entry = FE_SDB_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_sdb_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_sdb_convert_sw_to_hw_data,
		.alloc_entry = fe_sdb_alloc_entry,
		.set_entry = fe_sdb_set_entry,
		.add_entry = fe_sdb_add_entry,
		.del_entry_by_idx = fe_sdb_del_entry_by_idx,
		.del_entry = fe_sdb_del_entry,
		.find_entry = fe_sdb_find_entry,
		.get_entry = fe_sdb_get_entry,
		.inc_entry_refcnt = fe_sdb_inc_entry_refcnt,
		.dec_entry_refcnt = fe_sdb_dec_entry_refcnt,
		.get_entry_refcnt = fe_sdb_get_entry_refcnt,
		.set_field = fe_sdb_set_field,
		.get_field = fe_sdb_get_field,
		.flush_table = fe_sdb_flush_table,
		.get_avail_count = fe_sdb_get_avail_count,
		.print_entry = fe_sdb_print_entry,
		.print_range = fe_sdb_print_range,
		.print_table = fe_sdb_print_table,
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

int cs_fe_ioctl_sdb(struct net_device *dev, void *pdata, void *cmd)
{
	fe_sdb_entry_t *p_rslt = (fe_sdb_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_sdb_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_sdb_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_sdb_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** SDB entry 0 ~ %d ***\n", FE_SDB_ENTRY_MAX - 1);
		fe_sdb_print_range(fe_cmd_hdr->idx_start, fe_cmd_hdr->idx_end);
		break;
	case CMD_REPLACE:	/* ignore */
		break;
	case CMD_INIT:	/* ignore */
		break;
	default:
		return -1;
	}

	return status;
} /* cs_fe_ioctl_sdb */

/* this API will initialize sdb table */
int cs_fe_table_sdb_init(void)
{
	int ret;
	unsigned int index;

	spin_lock_init(FE_SDB_LOCK);

	cs_fe_sdb_table_type.content_table = cs_table_alloc(
			cs_fe_sdb_table_type.max_entry);
	if (cs_fe_sdb_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_sdb_table_type.type_id,
			&cs_fe_sdb_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register SDB table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_sdb_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_sdb_flush_table();

	/* Reserve a default entry */
	// FIXME!! do we want to move the following reserve to somehwere else?
	ret = fe_sdb_alloc_entry(&index, 0);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to allocate default SDB entry\n", __func__,
				__LINE__);
		return -1;
	}
	if (index != 0)
		printk("%s:%d:the default index is not 0 (%d)!\n:", __func__,
				__LINE__, index);

	return FE_TABLE_OK;
} /* cs_fe_table_sdb_init */
EXPORT_SYMBOL(cs_fe_table_sdb_init);

void cs_fe_table_sdb_exit(void)
{
	fe_sdb_flush_table();

	if (cs_fe_sdb_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_sdb_table_type.content_table);
	cs_fe_table_unregister(cs_fe_sdb_table_type.type_id);
} /* cs_fe_table_sdb_exit */
EXPORT_SYMBOL(cs_fe_table_sdb_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_sdb_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_sdb_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_sdb_alloc_entry_ut);

int fe_sdb_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_sdb_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_sdb_convert_sw_to_hw_data_ut);

int fe_sdb_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_sdb_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_sdb_set_entry_ut);

int fe_sdb_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_sdb_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_sdb_add_entry_ut);

int fe_sdb_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_sdb_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_sdb_del_entry_by_idx_ut);

int fe_sdb_del_entry_ut(void *entry, bool f_force)
{
	return fe_sdb_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_sdb_del_entry_ut);

int fe_sdb_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_sdb_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_sdb_find_entry_ut);

int fe_sdb_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_sdb_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_sdb_get_entry_ut);

int fe_sdb_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_sdb_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_sdb_inc_entry_refcnt_ut);

int fe_sdb_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_sdb_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_sdb_dec_entry_refcnt_ut);

int fe_sdb_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_sdb_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_sdb_get_entry_refcnt_ut);

int fe_sdb_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_sdb_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_sdb_set_field_ut);

int fe_sdb_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_sdb_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_sdb_get_field_ut);

int fe_sdb_flush_table_ut(void)
{
	return fe_sdb_flush_table();
}
EXPORT_SYMBOL(fe_sdb_flush_table_ut);

int fe_sdb_get_avail_count_ut(void)
{
	return fe_sdb_get_avail_count();
}
EXPORT_SYMBOL(fe_sdb_get_avail_count_ut);

void fe_sdb_print_entry_ut(unsigned int idx)
{
	fe_sdb_print_entry(idx);
}
EXPORT_SYMBOL(fe_sdb_print_entry_ut);

void fe_sdb_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_sdb_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_sdb_print_range_ut);

void fe_sdb_print_table_ut(void)
{
	fe_sdb_print_table();
}
EXPORT_SYMBOL(fe_sdb_print_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

