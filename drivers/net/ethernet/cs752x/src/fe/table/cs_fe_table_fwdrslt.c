/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_fwdrslt.c
 *
 * $Id: cs_fe_table_fwdrslt.c,v 1.6 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains the implementation for HW FE Forwarding Result Table Management.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

int cs_fe_fwdrslt_debug = 0 ;
#define DBG(x)  if(cs_fe_fwdrslt_debug>=1) x

static cs_fe_table_t cs_fe_fwdrslt_table_type;

#define FE_FWDRSLT_TABLE_PTR	(cs_fe_fwdrslt_table_type.content_table)
#define FE_FWDRSLT_LOCK		&(cs_fe_fwdrslt_table_type.lock)

static int fe_fwdrslt_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_fwdrslt_table_type, rslt_idx,
			start_offset);
} /* fe_fwdrslt_alloc_entry */

static int fe_fwdrslt_convert_sw_to_hw_data(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	fe_fwd_result_entry_t *entry = (fe_fwd_result_entry_t *)sw_entry;
	__u32 value;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->dest.voq_policy;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_VOQ_POLICY, &value, p_data_array, size);

	value = (__u32)entry->dest.pol_policy;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_POL_POLICY, &value, p_data_array, size);

	value = (__u32)entry->dest.voq_pol_table_index;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_VOQ_POL_TBL_IDX, &value, p_data_array, size);

	value = (__u32)entry->l3.ip_sa_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_IP_SA_REPLACE_EN, &value, p_data_array, size);

	value = (__u32)entry->l3.ip_sa_index;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_IP_SA_IDX, &value, p_data_array, size);

	value = (__u32)entry->l3.ip_da_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_IP_DA_REPLACE_EN, &value, p_data_array, size);

	value = (__u32)entry->l3.ip_da_index;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_IP_DA_IDX, &value, p_data_array, size);

	value = (__u32)entry->l4.sp_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_L4_SP_REPLACE_EN, &value, p_data_array, size);

	value = (__u32)entry->l4.sp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_L4_SP, &value, p_data_array, size);

	value = (__u32)entry->l4.dp_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_L4_DP_REPLACE_EN, &value, p_data_array, size);

	value = (__u32)entry->l4.dp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_L4_DP, &value, p_data_array, size);

	value = (__u32)entry->l2.mac_sa_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_MAC_SA_REPLACE_EN, &value, p_data_array, size);

	value = (__u32)entry->l2.mac_da_replace_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_MAC_DA_REPLACE_EN, &value, p_data_array, size);

	value = (__u32)entry->l2.l2_index;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_L2_IDX, &value, p_data_array, size);

	value = (__u32)entry->l2.mcgid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_MCGID, &value, p_data_array, size);

	value = (__u32)entry->l2.mcgid_valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_MCGID_VLD, &value, p_data_array, size);

	value = (__u32)entry->acl_dsbl;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_ACL_DISABLE, &value, p_data_array, size);

	value = (__u32)entry->act.fwd_type_valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_FWDTYPE_VLD, &value, p_data_array, size);

	value = (__u32)entry->act.fwd_type;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_FWDTYPE, &value, p_data_array, size);

	value = (__u32)entry->l3.decr_ttl_hoplimit;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_DECR_TTL_HOPLIMIT, &value, p_data_array, size);

	value = (__u32)entry->l2.flow_vlan_op_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_FLOW_VLAN_OP_EN, &value, p_data_array, size);

	value = (__u32)entry->l2.flow_vlan_index;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_FLOW_VLANTBL_IDX, &value, p_data_array, size);

	value = (__u32)entry->act.drop;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_DROP, &value, p_data_array, size);

	value = (__u32)entry->l2.pppoe_encap_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_PPPOE_ENCAP_EN, &value, p_data_array, size);

	value = (__u32)entry->l2.pppoe_decap_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_PPPOE_DECAP_EN, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FWDRSLT,
			FWD_MEM_PARITY, &value, p_data_array, size);

	return 0;
} /* fe_fwdrslt_convert_sw_to_hw_data */

static int fe_fwdrslt_set_entry(unsigned int idx, void *entry)
{
	/* should not set to default entry */
	if (idx == 0)
		return FE_TABLE_OK;

	return fe_table_set_entry(&cs_fe_fwdrslt_table_type, idx, entry);
} /* fe_fwdrslt_set_entry */

static int fe_fwdrslt_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_fwd_result_entry_t *p_rslt_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_fwdrslt_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_FWDRSLT_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_FWDRSLT_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_FWDRSLT_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_fwdrslt_table_type);
		spin_unlock_irqrestore(FE_FWDRSLT_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_rslt_entry = (fe_fwd_result_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_FWDRSLT_LOCK, flags);
	/* update the sw table */
	switch (field) {
	case FWD_VOQ_POLICY:
		p_rslt_entry->dest.voq_policy = (__u8)p_value[0];
		break;
	case FWD_POL_POLICY:
		p_rslt_entry->dest.pol_policy = (__u8)p_value[0];
		break;
	case FWD_VOQ_POL_TBL_IDX:
		p_rslt_entry->dest.voq_pol_table_index = (__u16)p_value[0];
		break;
	case FWD_IP_SA_REPLACE_EN:
		p_rslt_entry->l3.ip_sa_replace_en = (__u8)p_value[0];
		break;
	case FWD_IP_SA_IDX:
		p_rslt_entry->l3.ip_sa_index = (__u16)p_value[0];
		break;
	case FWD_IP_DA_REPLACE_EN:
		p_rslt_entry->l3.ip_da_replace_en = (__u8)p_value[0];
		break;
	case FWD_IP_DA_IDX:
		p_rslt_entry->l3.ip_da_index = (__u16)p_value[0];
		break;
	case FWD_L4_SP_REPLACE_EN:
		p_rslt_entry->l4.sp_replace_en = (__u8)p_value[0];
		break;
	case FWD_L4_SP:
		p_rslt_entry->l4.sp = (__u16)p_value[0];
		break;
	case FWD_L4_DP_REPLACE_EN:
		p_rslt_entry->l4.dp_replace_en = (__u8)p_value[0];
		break;
	case FWD_L4_DP:
		p_rslt_entry->l4.dp = (__u16)p_value[0];
		break;
	case FWD_MAC_SA_REPLACE_EN:
		p_rslt_entry->l2.mac_sa_replace_en = (__u8)p_value[0];
		break;
	case FWD_MAC_DA_REPLACE_EN:
		p_rslt_entry->l2.mac_da_replace_en = (__u8)p_value[0];
		break;
	case FWD_L2_IDX:
		p_rslt_entry->l2.l2_index = (__u16)p_value[0];
		break;
	case FWD_MCGID:
		p_rslt_entry->l2.mcgid = (__u16)p_value[0];
		break;
	case FWD_MCGID_VLD:
		p_rslt_entry->l2.mcgid_valid = (__u8)p_value[0];
		break;
	case FWD_ACL_DISABLE:
		p_rslt_entry->acl_dsbl = (cs_boolean)p_value[0];
		break;
	case FWD_FWDTYPE_VLD:
		p_rslt_entry->act.fwd_type_valid = (__u8)p_value[0];
		break;
	case FWD_FWDTYPE:
		p_rslt_entry->act.fwd_type = (__u8)p_value[0];
		break;
	case FWD_DECR_TTL_HOPLIMIT:
		p_rslt_entry->l3.decr_ttl_hoplimit = (__u8)p_value[0];
		break;
	case FWD_FLOW_VLAN_OP_EN:
		p_rslt_entry->l2.flow_vlan_op_en = (__u8)p_value[0];
		break;
	case FWD_FLOW_VLANTBL_IDX:
		p_rslt_entry->l2.flow_vlan_index = (__u16)p_value[0];
		break;
	case FWD_DROP:
		p_rslt_entry->act.drop = (__u8)p_value[0];
		break;
	case FWD_PPPOE_ENCAP_EN:
		p_rslt_entry->l2.pppoe_encap_en = (__u8)p_value[0];
		break;
	case FWD_PPPOE_DECAP_EN:
		p_rslt_entry->l2.pppoe_decap_en = (__u8)p_value[0];
		break;
	case FWD_MEM_PARITY:
		p_rslt_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_FWDRSLT_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_FWDRSLT, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_FWDRSLT_LOCK, flags);

	return status;
} /* fe_fwdrslt_set_field */

static int fe_fwdrslt_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_fwd_result_entry_t *p_rslt_entry;
	__u32 hw_value=0;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_fwdrslt_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_FWDRSLT_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_FWDRSLT_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_rslt_entry = (fe_fwd_result_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_FWDRSLT_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, field,
			&hw_value);
	spin_unlock_irqrestore(FE_FWDRSLT_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_FWDRSLT_LOCK, flags);
	switch (field) {
	case FWD_VOQ_POLICY:
		p_value[0] = (__u32)p_rslt_entry->dest.voq_policy;
		break;
	case FWD_POL_POLICY:
		p_value[0] = (__u32)p_rslt_entry->dest.pol_policy;
		break;
	case FWD_VOQ_POL_TBL_IDX:
		p_value[0] = (__u32)p_rslt_entry->dest.voq_pol_table_index;
		break;
	case FWD_IP_SA_REPLACE_EN:
		p_value[0] = (__u32)p_rslt_entry->l3.ip_sa_replace_en;
		break;
	case FWD_IP_SA_IDX:
		p_value[0] = (__u32)p_rslt_entry->l3.ip_sa_index;
		break;
	case FWD_IP_DA_REPLACE_EN:
		p_value[0] = (__u32)p_rslt_entry->l3.ip_da_replace_en;
		break;
	case FWD_IP_DA_IDX:
		p_value[0] = (__u32)p_rslt_entry->l3.ip_da_index;
		break;
	case FWD_L4_SP_REPLACE_EN:
		p_value[0] = (__u32)p_rslt_entry->l4.sp_replace_en;
		break;
	case FWD_L4_SP:
		p_value[0] = (__u32)p_rslt_entry->l4.sp;
		break;
	case FWD_L4_DP_REPLACE_EN:
		p_value[0] = (__u32)p_rslt_entry->l4.dp_replace_en;
		break;
	case FWD_L4_DP:
		p_value[0] = (__u32)p_rslt_entry->l4.dp;
		break;
	case FWD_MAC_SA_REPLACE_EN:
		p_value[0] = (__u32)p_rslt_entry->l2.mac_sa_replace_en;
		break;
	case FWD_MAC_DA_REPLACE_EN:
		p_value[0] = (__u32)p_rslt_entry->l2.mac_da_replace_en;
		break;
	case FWD_L2_IDX:
		p_value[0] = (__u32)p_rslt_entry->l2.l2_index;
		break;
	case FWD_MCGID:
		p_value[0] = (__u32)p_rslt_entry->l2.mcgid;
		break;
	case FWD_MCGID_VLD:
		p_value[0] = (__u32)p_rslt_entry->l2.mcgid_valid;
		break;
	case FWD_ACL_DISABLE:
		p_value[0] = (__u32)p_rslt_entry->acl_dsbl;
		break;
	case FWD_FWDTYPE_VLD:
		p_value[0] = (__u32)p_rslt_entry->act.fwd_type_valid;
		break;
	case FWD_FWDTYPE:
		p_value[0] = (__u32)p_rslt_entry->act.fwd_type;
		break;
	case FWD_DECR_TTL_HOPLIMIT:
		p_value[0] = (__u32)p_rslt_entry->l3.decr_ttl_hoplimit;
		break;
	case FWD_FLOW_VLAN_OP_EN:
		p_value[0] = (__u32)p_rslt_entry->l2.flow_vlan_op_en;
		break;
	case FWD_FLOW_VLANTBL_IDX:
		p_value[0] = (__u32)p_rslt_entry->l2.flow_vlan_index;
		break;
	case FWD_DROP:
		p_value[0] = (__u32)p_rslt_entry->act.drop;
		break;
	case FWD_PPPOE_ENCAP_EN:
		p_value[0] = (__u32)p_rslt_entry->l2.pppoe_encap_en;
		break;
	case FWD_PPPOE_DECAP_EN:
		p_value[0] = (__u32)p_rslt_entry->l2.pppoe_decap_en;
		break;
	case FWD_MEM_PARITY:
		p_value[0] = (__u32)p_rslt_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_FWDRSLT_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_FWDRSLT_LOCK, flags);

	if (hw_value != p_value[0]) {
		DBG(printk("%s:%d:SW value doesn't not match with HW value\n",
					__func__, __LINE__));
	}

	return FE_TABLE_OK;
} /* fe_fwdrslt_get_field */

static int fe_fwdrslt_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_fwdrslt_table_type, idx);
} /* fe_fwdrslt_inc_entry_refcnt */

static int fe_fwdrslt_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_fwdrslt_table_type, idx);
} /* fe_fwdrslt_dec_entry_refcnt */

static int fe_fwdrslt_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_fwdrslt_table_type, idx, p_cnt);
} /* fe_fwdrslt_get_entry_refcnt */

static int fe_fwdrslt_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_fwdrslt_table_type, entry, rslt_idx);
} /* fe_fwdrslt_add_entry */

static int fe_fwdrslt_del_entry_by_idx(unsigned int idx, bool f_force)
{
	/* should not delete default entry */
	if (idx == 0 && f_force == false)
		return FE_TABLE_OK;

	return fe_table_del_entry_by_idx(&cs_fe_fwdrslt_table_type, idx,
			f_force);
} /* fe_fwdrslt_del_entry_by_idx */

static int fe_fwdrslt_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_fwdrslt_table_type, entry, f_force);
} /* fe_fwdrslt_del_entry */

static int fe_fwdrslt_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_fwdrslt_table_type, entry, rslt_idx,
			start_offset);
} /* fe_fwdrslt_find_entry */

static int fe_fwdrslt_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_fwdrslt_table_type, idx, entry);
} /* fe_fwdrslt_get_entry */

static int fe_fwdrslt_flush_table(void)
{
	int ret;
	unsigned int index;

	ret = fe_table_flush_table(&cs_fe_fwdrslt_table_type);
	if (ret != FE_TABLE_OK)
		return ret;

	/* Reserve a default entry */
	ret = fe_fwdrslt_alloc_entry(&index, 0);
	if (ret != FE_TABLE_OK)
		printk("%s:%d:Unable to allocate default FWDRSLT entry\n",
				__func__, __LINE__);

	if (index != 0)
		printk("%s:%d:the default index is not 0 (%d)!\n:", __func__,
				__LINE__, index);

	return ret;
} /* fe_fwdrslt_flush_table */

static void _fe_fwdrslt_print_entry(unsigned int idx)
{
	fe_fwd_result_entry_t fwdrslt_entry, *p_rslt;
	__u32 value;
	int status;
	unsigned int count;

	status = fe_fwdrslt_get_entry(idx, &fwdrslt_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND)) {
		printk("| index %04d | NOT USED\n", idx);
		printk("|----------------------------------------------|\n");
	}
	if (status != FE_TABLE_OK)
		return;

	fe_fwdrslt_get_entry_refcnt(idx, &count);

	p_rslt = &fwdrslt_entry;

	printk("| index %04d | Sw count %d\n", idx, count);
	printk("  |- ACL Dsbl %01x ", p_rslt->acl_dsbl);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_ACL_DISABLE,
			&value);
	printk("(hw %01x)\n", value);

	/* L2 */
	printk("  |- L2 Info SA_EN: %01x (HW ", p_rslt->l2.mac_sa_replace_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_MAC_SA_REPLACE_EN, (void*)&value);
	printk("%01x).  DA_EN: ", value);
	printk("%01x (HW ", p_rslt->l2.mac_da_replace_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_MAC_DA_REPLACE_EN, &value);
	printk("%01x).  L2 IDX: ",value);
	printk("%x (HW ", p_rslt->l2.l2_index);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_L2_IDX,
			&value);
	printk("%x)\n", value);
	printk("MCG_ID: %x ", p_rslt->l2.mcgid);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_MCGID,
			&value);
	printk("(HW %x).  MCG_ID_VLD: ", value);
	printk("%01x (HW ", p_rslt->l2.mcgid_valid);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_MCGID_VLD,
			&value);
	printk("%01x).  FVLAN_OP_EN: ", value);
	printk("%01x (HW ", p_rslt->l2.flow_vlan_op_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_FLOW_VLAN_OP_EN, &value);
	printk("%01x).  FVLAN_IDX: ", value);
	printk("%x (HW ", p_rslt->l2.flow_vlan_index);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_FLOW_VLANTBL_IDX, &value);
	printk("%x).  PPPoE_Encap_en: ", value);
	printk("%01x (HW ", p_rslt->l2.pppoe_encap_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_PPPOE_ENCAP_EN, &value);
	printk("%01x). PPPoE_Decap_en: ", value);
	printk("%01x (HW ", p_rslt->l2.pppoe_decap_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_PPPOE_DECAP_EN, &value);
	printk("%01x).\n", value);

	/* L3 */
	printk(" |- L3 Info decr_ttl: ");
	printk("%01x (HW ", p_rslt->l3.decr_ttl_hoplimit);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_DECR_TTL_HOPLIMIT, &value);
	printk("%01x). IP_SA Rep_EN: ", value);
	printk("%01x (HW ", p_rslt->l3.ip_sa_replace_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_IP_SA_REPLACE_EN, &value);
	printk("%01x). Index: ", value);
	printk("%x (HW ", p_rslt->l3.ip_sa_index);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_IP_SA_IDX,
			&value);
	printk("%x).  IP_DA Rep_EN: ", value);
	printk("%01x (HW ", p_rslt->l3.ip_da_replace_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_IP_DA_REPLACE_EN, &value);
	printk("%01x). Index: ", value);
	printk("%x (HW ", p_rslt->l3.ip_da_index);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_IP_DA_IDX,
			&value);
	printk("%x).\n", value);

	/* L4 */
	printk(" |- L4 Info SP_Rep_EN: ");
	printk("%01x (HW ", p_rslt->l4.sp_replace_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_L4_SP_REPLACE_EN, &value);
	printk("%01x). SrcPort: ", value);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_L4_SP,
			&value);
	printk("%x (HW %x). ", p_rslt->l4.sp, value);
	printk("DP_Rep_EN: %01x (HW ", p_rslt->l4.dp_replace_en);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_L4_DP_REPLACE_EN, &value);
	printk("%01x). DstPort: ", value);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_L4_DP,
			&value);
	printk("%x (HW %x).\n", p_rslt->l4.dp, value);

	/* Dest */
	printk(" |- Destination Info: Pol_policy: ");
	printk("%01x (HW ", p_rslt->dest.pol_policy);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_POL_POLICY,
			&value);
	printk("%01x). VOQ_Pol: ", value);
	printk("%01x (HW ", p_rslt->dest.voq_policy);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_VOQ_POLICY,
			&value);
	printk("%01x). VOQ_Pol_Table_index: ", value);
	printk("%x (HW ", p_rslt->dest.voq_pol_table_index);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx,
			FWD_VOQ_POL_TBL_IDX, &value);
	printk("%x)\n", value);

	/* Action */
	printk(" |- Action Info: Fwd_type_valid: ");
	printk("%01x (HW ", p_rslt->act.fwd_type_valid);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_FWDTYPE_VLD,
			&value);
	printk("%01x). Fwd_type: ", value);
	printk("%x (HW ", p_rslt->act.fwd_type);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_FWDTYPE,
			&value);
	printk("%x). Drop: ", value);
	printk("%01x (HW ", p_rslt->act.drop);
	value = 0;
	cs_fe_hw_table_get_field_value(FE_TABLE_FWDRSLT, idx, FWD_DROP, &value);
	printk("%01x).\n", value);
	printk("|----------------------------------------------|\n");
} /* _fe_fwdrslt_print_entry */

static void fe_fwdrslt_print_entry(unsigned int idx)
{
	if (idx >= FE_FWD_RESULT_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	printk("\n\n ----------------- FWD Result Table ---------------\n");
	printk("|------------------------------------------------------\n");

	_fe_fwdrslt_print_entry(idx);
} /* fe_fwdrslt_print_entry */

static void fe_fwdrslt_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_FWD_RESULT_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	printk("\n\n ----------------- FWD Result Table ---------------\n");
	printk("|------------------------------------------------------\n");

	for (i = start_idx; i <= end_idx; i++) {
		_fe_fwdrslt_print_entry(i);
		cond_resched();
	}

	printk("|------------------------------------------------------\n");
} /* fe_fwdrslt_print_range */

static void fe_fwdrslt_print_table(void)
{
	unsigned int i;

	printk("\n\n ----------------- FWD Result Table ---------------\n");
	printk("|------------------------------------------------------\n");

	for (i = 0; i < cs_fe_fwdrslt_table_type.max_entry; i++) {
		_fe_fwdrslt_print_entry(i);
		cond_resched();
	}

	printk("|------------------------------------------------------\n");
} /* fe_fwdrslt_print_table */

static int fe_fwdrslt_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_fwdrslt_table_type);
} /* fe_fwdrslt_get_avail_count */

static cs_fe_table_t cs_fe_fwdrslt_table_type = {
	.type_id = FE_TABLE_FWDRSLT,
	.max_entry = FE_FWD_RESULT_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_fwd_result_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_fwdrslt_convert_sw_to_hw_data,
		.alloc_entry = fe_fwdrslt_alloc_entry,
		.set_entry = fe_fwdrslt_set_entry,
		.add_entry = fe_fwdrslt_add_entry,
		.del_entry_by_idx = fe_fwdrslt_del_entry_by_idx,
		.del_entry = fe_fwdrslt_del_entry,
		.find_entry = fe_fwdrslt_find_entry,
		.get_entry = fe_fwdrslt_get_entry,
		.inc_entry_refcnt = fe_fwdrslt_inc_entry_refcnt,
		.dec_entry_refcnt = fe_fwdrslt_dec_entry_refcnt,
		.get_entry_refcnt = fe_fwdrslt_get_entry_refcnt,
		.set_field = fe_fwdrslt_set_field,
		.get_field = fe_fwdrslt_get_field,
		.flush_table = fe_fwdrslt_flush_table,
		.get_avail_count = fe_fwdrslt_get_avail_count,
		.print_entry = fe_fwdrslt_print_entry,
		.print_range = fe_fwdrslt_print_range,
		.print_table = fe_fwdrslt_print_table,
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

/* do we need to include / implement ioctl? */
int cs_fe_ioctl_fwdrslt(struct net_device *dev, void *pdata, void * cmd)
{
	fe_fwd_result_entry_t *p_rslt = (fe_fwd_result_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_fwdrslt_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_fwdrslt_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_fwdrslt_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		//cs_fe_print_fwdrslt_tbl(0, FE_FWD_RESULT_ENTRY_MAX - 1);
		printk("*** FWDRSLT entry 0 ~ %d ***\n",
				FE_FWD_RESULT_ENTRY_MAX - 1);
		fe_fwdrslt_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_fwdrslt */

/* this API will initialize fwdrslt table */
int cs_fe_table_fwdrslt_init(void)
{
	int ret;

	spin_lock_init(FE_FWDRSLT_LOCK);

	cs_fe_fwdrslt_table_type.content_table = cs_table_alloc(
			cs_fe_fwdrslt_table_type.max_entry);
	if (cs_fe_fwdrslt_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_fwdrslt_table_type.type_id,
			&cs_fe_fwdrslt_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register FWDRSLT table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_fwdrslt_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_fwdrslt_flush_table();


	return CS_OK;
} /* cs_fe_table_fwdrslt_init */
EXPORT_SYMBOL(cs_fe_table_fwdrslt_init);

void cs_fe_table_fwdrslt_exit(void)
{
	fe_fwdrslt_flush_table();

	if (cs_fe_fwdrslt_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_fwdrslt_table_type.content_table);
	cs_fe_table_unregister(cs_fe_fwdrslt_table_type.type_id);
} /* cs_fe_table_fwdrslt_exit */
EXPORT_SYMBOL(cs_fe_table_fwdrslt_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_fwdrslt_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_fwdrslt_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_fwdrslt_alloc_entry_ut);

int fe_fwdrslt_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_fwdrslt_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_fwdrslt_convert_sw_to_hw_data_ut);

int fe_fwdrslt_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_fwdrslt_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_fwdrslt_set_entry_ut);

int fe_fwdrslt_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_fwdrslt_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_fwdrslt_add_entry_ut);

int fe_fwdrslt_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_fwdrslt_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_fwdrslt_del_entry_by_idx_ut);

int fe_fwdrslt_del_entry_ut(void *entry, bool f_force)
{
	return fe_fwdrslt_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_fwdrslt_del_entry_ut);

int fe_fwdrslt_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_fwdrslt_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_fwdrslt_find_entry_ut);

int fe_fwdrslt_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_fwdrslt_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_fwdrslt_get_entry_ut);

int fe_fwdrslt_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_fwdrslt_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_fwdrslt_inc_entry_refcnt_ut);

int fe_fwdrslt_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_fwdrslt_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_fwdrslt_dec_entry_refcnt_ut);

int fe_fwdrslt_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_fwdrslt_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_fwdrslt_get_entry_refcnt_ut);

int fe_fwdrslt_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_fwdrslt_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_fwdrslt_set_field_ut);

int fe_fwdrslt_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_fwdrslt_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_fwdrslt_get_field_ut);

int fe_fwdrslt_flush_table_ut(void)
{
	return fe_fwdrslt_flush_table();
}
EXPORT_SYMBOL(fe_fwdrslt_flush_table_ut);

int fe_fwdrslt_get_avail_count_ut(void)
{
	return fe_fwdrslt_get_avail_count();
}
EXPORT_SYMBOL(fe_fwdrslt_get_avail_count_ut);

void fe_fwdrslt_print_entry_ut(unsigned int idx)
{
	fe_fwdrslt_print_entry(idx);
}
EXPORT_SYMBOL(fe_fwdrslt_print_entry_ut);

void fe_fwdrslt_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_fwdrslt_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_fwdrslt_print_range_ut);

void fe_fwdrslt_print_table_ut(void)
{
	fe_fwdrslt_print_table();
}
EXPORT_SYMBOL(fe_fwdrslt_print_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

