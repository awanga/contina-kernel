/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_qosrslt.c
 *
 * $Id: cs_fe_table_qosrslt.c,v 1.10 2012/03/28 23:38:28 whsu Exp $
 *
 * It contains QOS Result Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_qosrslt_table_type;

#define FE_QOSRSLT_TABLE_PTR	(cs_fe_qosrslt_table_type.content_table)
#define FE_QOSRSLT_LOCK		&(cs_fe_qosrslt_table_type.lock)

static int fe_qosrslt_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_qosrslt_table_type, rslt_idx,
			start_offset);
} /* fe_qosrslt_alloc_entry */

static int fe_qosrslt_convert_sw_to_hw_data(void *sw_entry, __u32 *p_data_array,
		unsigned int size)
{
	__u32 value;
	fe_qos_result_entry_t *entry = (fe_qos_result_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->wred_cos;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_WRED_COS, &value, p_data_array, size);

	value = (__u32)entry->voq_cos;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_VOQ_COS, &value, p_data_array, size);

	value = (__u32)entry->pol_cos;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_POL_COS, &value, p_data_array, size);

	value = (__u32)entry->premark;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_PREMARK, &value, p_data_array, size);

	value = (__u32)entry->change_dscp_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_CHANGE_DSCP_EN, &value, p_data_array, size);

	value = (__u32)entry->dscp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_DSCP, &value, p_data_array, size);

	value = (__u32)entry->dscp_markdown_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_DSCP_MARKDOWN_EN, &value, p_data_array, size);

	value = (__u32)entry->marked_down_dscp;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_MARKED_DOWN_DSCP, &value, p_data_array, size);

	value = (__u32)entry->ecn_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_ECN_ENABLE, &value, p_data_array, size);

	value = (__u32)entry->top_802_1p;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_TOP_802_1P, &value, p_data_array, size);

	value = (__u32)entry->marked_down_top_802_1p;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_MARKED_DOWN_TOP_802_1P, &value, p_data_array,
			size);

	value = (__u32)entry->top_8021p_markdown_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_TOP8021P_MARKDOWN_EN, &value, p_data_array,
			size);

	value = (__u32)entry->top_dei;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_TOP_DEI, &value, p_data_array, size);

	value = (__u32)entry->marked_down_top_dei;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_MARKED_DOWN_TOP_DEI, &value, p_data_array,
			size);

	value = (__u32)entry->inner_802_1p;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_INNER_802_1P, &value, p_data_array, size);

	value = (__u32)entry->marked_down_inner_802_1p;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_MARKED_DOWN_INNER_802_1P, &value, p_data_array,
			size);

	value = (__u32)entry->inner_8021p_markdown_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_INNER_8021P_MARKDOWN_EN, &value, p_data_array,
			size);

	value = (__u32)entry->inner_dei;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_INNER_DEI, &value, p_data_array, size);

	value = (__u32)entry->marked_down_inner_dei;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_MARKED_DOWN_INNER_DEI, &value, p_data_array,
			size);

	value = (__u32)entry->change_8021p_1_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_CHANGE_8021P_1_EN, &value, p_data_array, size);

	value = (__u32)entry->change_dei_1_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_CHANGE_DEI_1_EN, &value, p_data_array, size);

	value = (__u32)entry->change_8021p_2_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_CHANGE_8021P_2_EN, &value, p_data_array, size);

	value = (__u32)entry->change_dei_2_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_CHANGE_DEI_2_EN, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_QOSRSLT,
			FWD_QOS_MEM_PARITY, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_qosrslt_convert_sw_to_hw_data */

static int fe_qosrslt_set_entry(unsigned int idx, void *entry)
{
	/* should not set to default entry */
	if (idx == 0)
		return FE_TABLE_OK;

	return fe_table_set_entry(&cs_fe_qosrslt_table_type, idx, entry);
} /* fe_qosrslt_set_entry */

static int fe_qosrslt_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_qosrslt_table_type, entry, rslt_idx);
} /* fe_qosrslt_add_entry */

static int fe_qosrslt_del_entry_by_idx(unsigned int idx, bool f_force)
{
	/* should not delete the default entry */
	if (idx == 0 && f_force == false)
		return FE_TABLE_OK;

	return fe_table_del_entry_by_idx(&cs_fe_qosrslt_table_type, idx,
			f_force);
} /* fe_qosrslt_del_entry_by_idx */

static int fe_qosrslt_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_qosrslt_table_type, entry, f_force);
} /* fe_qosrslt_del_entry */

static int fe_qosrslt_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_qosrslt_table_type, entry, rslt_idx,
			start_offset);
} /* fe_qosrslt_find_entry */

static int fe_qosrslt_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_qosrslt_table_type, idx, entry);
} /* fe_qosrslt_get_entry */

static int fe_qosrslt_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_qosrslt_table_type, idx);
} /* fe_qosrslt_inc_entry_refcnt */

static int fe_qosrslt_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_qosrslt_table_type, idx);
} /* fe_qosrslt_dec_entry_refcnt */

static int fe_qosrslt_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_qosrslt_table_type, idx, p_cnt);
} /* fe_qosrslt_get_entry_refcnt */

static int fe_qosrslt_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_qos_result_entry_t *p_rslt_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_qosrslt_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_QOSRSLT_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_QOSRSLT_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_QOSRSLT_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_qosrslt_table_type);
		spin_unlock_irqrestore(FE_QOSRSLT_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_rslt_entry = (fe_qos_result_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_QOSRSLT_LOCK, flags);
	switch (field) {
	case FWD_QOS_WRED_COS:
		p_rslt_entry->wred_cos = (__u8)p_value[0];
		break;
	case FWD_QOS_VOQ_COS:
		p_rslt_entry->voq_cos = (__u8)p_value[0];
		break;
	case FWD_QOS_POL_COS:
		p_rslt_entry->pol_cos = (__u8)p_value[0];
		break;
	case FWD_QOS_PREMARK:
		p_rslt_entry->premark = (__u8)p_value[0];
		break;
	case FWD_QOS_CHANGE_DSCP_EN:
		p_rslt_entry->change_dscp_en = (__u8)p_value[0];
		break;
	case FWD_QOS_DSCP:
		p_rslt_entry->dscp = (__u8)p_value[0];
		break;
	case FWD_QOS_DSCP_MARKDOWN_EN:
		p_rslt_entry->dscp_markdown_en = (__u8)p_value[0];
		break;
	case FWD_QOS_MARKED_DOWN_DSCP:
		p_rslt_entry->marked_down_dscp = (__u8)p_value[0];
		break;
	case FWD_QOS_ECN_ENABLE:
		p_rslt_entry->ecn_en = (__u8)p_value[0];
		break;
	case FWD_QOS_TOP_802_1P:
		p_rslt_entry->top_802_1p = (__u8)p_value[0];
		break;
	case FWD_QOS_MARKED_DOWN_TOP_802_1P:
		p_rslt_entry->marked_down_top_802_1p = (__u8)p_value[0];
		break;
	case FWD_QOS_TOP8021P_MARKDOWN_EN:
		p_rslt_entry->top_8021p_markdown_en = (__u8)p_value[0];
		break;
	case FWD_QOS_TOP_DEI:
		p_rslt_entry->top_dei = (__u8)p_value[0];
		break;
	case FWD_QOS_MARKED_DOWN_TOP_DEI:
		p_rslt_entry->marked_down_top_dei = (__u8)p_value[0];
		break;
	case FWD_QOS_INNER_802_1P:
		p_rslt_entry->inner_802_1p = (__u8)p_value[0];
		break;
	case FWD_QOS_MARKED_DOWN_INNER_802_1P:
		p_rslt_entry->marked_down_inner_802_1p = (__u8)p_value[0];
		break;
	case FWD_QOS_INNER_8021P_MARKDOWN_EN:
		p_rslt_entry->inner_8021p_markdown_en = (__u8)p_value[0];
		break;
	case FWD_QOS_INNER_DEI:
		p_rslt_entry->inner_dei = (__u8)p_value[0];
		break;
	case FWD_QOS_MARKED_DOWN_INNER_DEI:
		p_rslt_entry->marked_down_inner_dei = (__u8)p_value[0];
		break;
	case FWD_QOS_CHANGE_8021P_1_EN:
		p_rslt_entry->change_8021p_1_en = (__u8)p_value[0];
		break;
	case FWD_QOS_CHANGE_DEI_1_EN:
		p_rslt_entry->change_dei_1_en = (__u8)p_value[0];
		break;
	case FWD_QOS_CHANGE_8021P_2_EN:
		p_rslt_entry->change_8021p_2_en = (__u8)p_value[0];
		break;
	case FWD_QOS_CHANGE_DEI_2_EN:
		p_rslt_entry->change_dei_2_en = (__u8)p_value[0];
		break;
	case FWD_QOS_MEM_PARITY:
		p_rslt_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_QOSRSLT_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_QOSRSLT, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_QOSRSLT_LOCK, flags);

	return status;
} /* fe_qosrslt_set_field */

static int fe_qosrslt_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_qos_result_entry_t *p_rslt_entry;
	__u32 hw_value;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_qosrslt_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_QOSRSLT_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_QOSRSLT_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_rslt_entry = (fe_qos_result_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_QOSRSLT_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx, field,
			&hw_value);
	spin_unlock_irqrestore(FE_QOSRSLT_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_QOSRSLT_LOCK, flags);
	switch (field) {
	case FWD_QOS_WRED_COS:
		p_value[0] = (__u32)p_rslt_entry->wred_cos;
		break;
	case FWD_QOS_VOQ_COS:
		p_value[0] = (__u32)p_rslt_entry->voq_cos;
		break;
	case FWD_QOS_POL_COS:
		p_value[0] = (__u32)p_rslt_entry->pol_cos;
		break;
	case FWD_QOS_PREMARK:
		p_value[0] = (__u32)p_rslt_entry->premark;
		break;
	case FWD_QOS_CHANGE_DSCP_EN:
		p_value[0] = (__u32)p_rslt_entry->change_dscp_en;
		break;
	case FWD_QOS_DSCP:
		p_value[0] = (__u32)p_rslt_entry->dscp;
		break;
	case FWD_QOS_DSCP_MARKDOWN_EN:
		p_value[0] = (__u32)p_rslt_entry->dscp_markdown_en;
		break;
	case FWD_QOS_MARKED_DOWN_DSCP:
		p_value[0] = (__u32)p_rslt_entry->marked_down_dscp;
		break;
	case FWD_QOS_ECN_ENABLE:
		p_value[0] = (__u32)p_rslt_entry->ecn_en;
		break;
	case FWD_QOS_TOP_802_1P:
		p_value[0] = (__u32)p_rslt_entry->top_802_1p;
		break;
	case FWD_QOS_MARKED_DOWN_TOP_802_1P:
		p_value[0] = (__u32)p_rslt_entry->marked_down_top_802_1p;
		break;
	case FWD_QOS_TOP8021P_MARKDOWN_EN:
		p_value[0] = (__u32)p_rslt_entry->top_8021p_markdown_en;
		break;
	case FWD_QOS_TOP_DEI:
		p_value[0] = (__u32)p_rslt_entry->top_dei;
		break;
	case FWD_QOS_MARKED_DOWN_TOP_DEI:
		p_value[0] = (__u32)p_rslt_entry->marked_down_top_dei;
		break;
	case FWD_QOS_INNER_802_1P:
		p_value[0] = (__u32)p_rslt_entry->inner_802_1p;
		break;
	case FWD_QOS_MARKED_DOWN_INNER_802_1P:
		p_value[0] = (__u32)p_rslt_entry->marked_down_inner_802_1p;
		break;
	case FWD_QOS_INNER_8021P_MARKDOWN_EN:
		p_value[0] = (__u32)p_rslt_entry->inner_8021p_markdown_en;
		break;
	case FWD_QOS_INNER_DEI:
		p_value[0] = (__u32)p_rslt_entry->inner_dei;
		break;
	case FWD_QOS_MARKED_DOWN_INNER_DEI:
		p_value[0] = (__u32)p_rslt_entry->marked_down_inner_dei;
		break;
	case FWD_QOS_CHANGE_8021P_1_EN:
		p_value[0] = (__u32)p_rslt_entry->change_8021p_1_en;
		break;
	case FWD_QOS_CHANGE_DEI_1_EN:
		p_value[0] = (__u32)p_rslt_entry->change_dei_1_en;
		break;
	case FWD_QOS_CHANGE_8021P_2_EN:
		p_value[0] = (__u32)p_rslt_entry->change_8021p_2_en;
		break;
	case FWD_QOS_CHANGE_DEI_2_EN:
		p_value[0] = (__u32)p_rslt_entry->change_dei_2_en;
		break;
	case FWD_QOS_MEM_PARITY:
		p_value[0] = (__u32)p_rslt_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_QOSRSLT_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_QOSRSLT_LOCK, flags);

	if (hw_value != p_value[0]) {
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);
	}

	return FE_TABLE_OK;
} /* fe_qosrslt_get_field */

static int fe_qosrslt_flush_table(void)
{
	int ret;
	unsigned int index;

	ret = fe_table_flush_table(&cs_fe_qosrslt_table_type);
	if (ret != FE_TABLE_OK)
		return ret;

	/* reserve a default entry */
	ret = fe_qosrslt_alloc_entry(&index, 0);
	if (ret != FE_TABLE_OK)
		printk("%s:%d:Unable to allocate default QOSRSLT ENTRY\n",
				__func__, __LINE__);

	if (index != 0)
		printk("%s:%d:the default index is not 0 (%d)!\n", __func__,
				__LINE__, index);

	return ret;
} /* fe_qosrslt_flush_table */

static int fe_qosrslt_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_qosrslt_table_type);
} /* fe_qosrslt_get_avail_count */

static void _fe_qosrslt_print_title(void)
{
	printk("\n\n ----------------- QOS Result Table ---------------\n");
	printk("|------------------------------------------------------\n");
} /* _fe_qosrslt_print_title */

static void _fe_qosrslt_print_entry(unsigned int idx)
{
	fe_qos_result_entry_t qosrslt_entry, *p_rslt;
	__u32 value;
	int status;
	unsigned int count;

	status = fe_qosrslt_get_entry(idx, &qosrslt_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_qosrslt_get_entry_refcnt(idx, &count);

	p_rslt = &qosrslt_entry;

	printk("| index: %04d | refcnt: %d ", idx, count);

	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_WRED_COS, &value);
	printk("  |- WRED COS: %02x (HW %02x).\n", p_rslt->wred_cos, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_VOQ_COS, &value);
	printk("\tVOQ COS: %02x (HW %02x).\n", p_rslt->voq_cos, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_POL_COS, &value);
	printk("\tPOL COS: %02x (HW %02x).\n", p_rslt->pol_cos, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_PREMARK, &value);
	printk("\tPremark: %02x (HW %02x).\n", p_rslt->voq_cos, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_CHANGE_DSCP_EN, &value);
	printk("\tChange DSCP EN: %02x (HW %02x).\n",
			p_rslt->change_dscp_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx, FWD_QOS_DSCP,
			&value);
	printk("\tDSCP: %02x (HW %02x).\n", p_rslt->dscp, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_DSCP_MARKDOWN_EN, &value);
	printk("\tDSCP Markdown EN: %02x (HW %02x).\n",
			p_rslt->dscp_markdown_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_MARKED_DOWN_DSCP, &value);
	printk("\tMarked Down DSCP: %02x (HW %02x).\n",
			p_rslt->marked_down_dscp, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_ECN_ENABLE, &value);
	printk("\tECN EN: %02x (HW %02x).\n", p_rslt->ecn_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_TOP_802_1P, &value);
	printk("\tTOP 802.1p: %02x (HW %02x).\n", p_rslt->top_802_1p, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_MARKED_DOWN_TOP_802_1P, &value);
	printk("\tMarked Down Top 802.1p: %02x (HW %02x).\n",
			p_rslt->marked_down_top_802_1p, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_TOP8021P_MARKDOWN_EN, &value);
	printk("\tTop 802.1p Markdown EN: %02x (HW %02x).\n",
			p_rslt->top_8021p_markdown_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_TOP_DEI, &value);
	printk("\tTop Dei: %02x (HW %02x).\n", p_rslt->top_dei, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_MARKED_DOWN_TOP_DEI, &value);
	printk("\tMarked Down Top Dei: %02x (HW %02x).\n",
			p_rslt->marked_down_top_dei, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_INNER_802_1P, &value);
	printk("\tInner 802.1p: %02x (HW %02x).\n", p_rslt->inner_802_1p,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_MARKED_DOWN_INNER_802_1P, &value);
	printk("\tMarked Down Inner 802.1p: %02x (HW %02x).\n",
			p_rslt->marked_down_inner_802_1p, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_INNER_8021P_MARKDOWN_EN, &value);
	printk("\tInner 802.1p Markdown EN: %02x (HW %02x).\n",
			p_rslt->inner_8021p_markdown_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_INNER_DEI, &value);
	printk("\tInner Dei: %02x (HW %02x).\n", p_rslt->inner_dei, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_MARKED_DOWN_INNER_DEI, &value);
	printk("\tMarked Down Inner Dei: %02x (HW %02x).\n",
			p_rslt->marked_down_inner_dei, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_CHANGE_8021P_1_EN, &value);
	printk("\tChange 802.1p 1 EN: %02x (HW %02x).\n",
			p_rslt->change_8021p_1_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_CHANGE_DEI_1_EN, &value);
	printk("\tChange Dei 1 EN: %02x (HW %02x).\n",
			p_rslt->change_dei_1_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_CHANGE_8021P_2_EN, &value);
	printk("\tChange 802.1p 2 EN: %02x (HW %02x).\n",
			p_rslt->change_8021p_2_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_CHANGE_DEI_2_EN, &value);
	printk("\tChange Dei 2 EN: %02x (HW %02x).\n",
			p_rslt->change_dei_2_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_QOSRSLT, idx,
			FWD_QOS_MEM_PARITY, &value);
	printk("\tMem Parity: %02x (HW %02x).\n", p_rslt->parity, value);
} /* _fe_qosrslt_print_entry */

static void fe_qosrslt_print_entry(unsigned int idx)
{
	if (idx >= FE_QOS_RESULT_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_qosrslt_print_title();
	_fe_qosrslt_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_qosrslt_print_entry */

static void fe_qosrslt_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_QOS_RESULT_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_qosrslt_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_qosrslt_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_qosrslt_print_range */

static void fe_qosrslt_print_table(void)
{
	unsigned int i;

	_fe_qosrslt_print_title();
	for (i = 0; i < cs_fe_qosrslt_table_type.max_entry; i++) {
		_fe_qosrslt_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_qosrslt_print_table */


static cs_fe_table_t cs_fe_qosrslt_table_type = {
	.type_id = FE_TABLE_QOSRSLT,
	.max_entry = FE_QOS_RESULT_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_qos_result_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_qosrslt_convert_sw_to_hw_data,
		.alloc_entry = fe_qosrslt_alloc_entry,
		.set_entry = fe_qosrslt_set_entry,
		.add_entry = fe_qosrslt_add_entry,
		.del_entry_by_idx = fe_qosrslt_del_entry_by_idx,
		.del_entry = fe_qosrslt_del_entry,
		.find_entry = fe_qosrslt_find_entry,
		.get_entry = fe_qosrslt_get_entry,
		.inc_entry_refcnt = fe_qosrslt_inc_entry_refcnt,
		.dec_entry_refcnt = fe_qosrslt_dec_entry_refcnt,
		.get_entry_refcnt = fe_qosrslt_get_entry_refcnt,
		.set_field = fe_qosrslt_set_field,
		.get_field = fe_qosrslt_get_field,
		.flush_table = fe_qosrslt_flush_table,
		.get_avail_count = fe_qosrslt_get_avail_count,
		.print_entry = fe_qosrslt_print_entry,
		.print_range = fe_qosrslt_print_range,
		.print_table = fe_qosrslt_print_table,
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

int cs_fe_ioctl_qosrslt(struct net_device *dev, void *pdata, void *cmd)
{
	fe_qos_result_entry_t *p_rslt = (fe_qos_result_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_qosrslt_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_qosrslt_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_qosrslt_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** QOSRSLT entry 0 ~ %d ***\n",
				FE_QOS_RESULT_ENTRY_MAX - 1);
		fe_qosrslt_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_qosrslt */

/* this API will initialize qosrslt table */
int cs_fe_table_qosrslt_init(void)
{
	int ret;

	spin_lock_init(FE_QOSRSLT_LOCK);

	cs_fe_qosrslt_table_type.content_table = cs_table_alloc(
			cs_fe_qosrslt_table_type.max_entry);
	if (cs_fe_qosrslt_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_qosrslt_table_type.type_id,
			&cs_fe_qosrslt_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register QOSRSLT table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_qosrslt_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_qosrslt_flush_table();

	return FE_TABLE_OK;
} /* cs_fe_table_qosrslt_init */
EXPORT_SYMBOL(cs_fe_table_qosrslt_init);

void cs_fe_table_qosrslt_exit(void)
{
	fe_qosrslt_flush_table();

	if (cs_fe_qosrslt_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_qosrslt_table_type.content_table);
	cs_fe_table_unregister(cs_fe_qosrslt_table_type.type_id);
} /* cs_fe_table_qosrslt_exit */
EXPORT_SYMBOL(cs_fe_table_qosrslt_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_qosrslt_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_qosrslt_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_qosrslt_alloc_entry_ut);

int fe_qosrslt_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_qosrslt_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_qosrslt_convert_sw_to_hw_data_ut);

int fe_qosrslt_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_qosrslt_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_qosrslt_set_entry_ut);

int fe_qosrslt_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_qosrslt_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_qosrslt_add_entry_ut);

int fe_qosrslt_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_qosrslt_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_qosrslt_del_entry_by_idx_ut);

int fe_qosrslt_del_entry_ut(void *entry, bool f_force)
{
	return fe_qosrslt_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_qosrslt_del_entry_ut);

int fe_qosrslt_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_qosrslt_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_qosrslt_find_entry_ut);

int fe_qosrslt_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_qosrslt_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_qosrslt_get_entry_ut);

int fe_qosrslt_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_qosrslt_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_qosrslt_inc_entry_refcnt_ut);

int fe_qosrslt_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_qosrslt_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_qosrslt_dec_entry_refcnt_ut);

int fe_qosrslt_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_qosrslt_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_qosrslt_get_entry_refcnt_ut);

int fe_qosrslt_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_qosrslt_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_qosrslt_set_field_ut);

int fe_qosrslt_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_qosrslt_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_qosrslt_get_field_ut);

int fe_qosrslt_flush_table_ut(void)
{
	return fe_qosrslt_flush_table();
}
EXPORT_SYMBOL(fe_qosrslt_flush_table_ut);

int fe_qosrslt_get_avail_count_ut(void)
{
	return fe_qosrslt_get_avail_count();
}
EXPORT_SYMBOL(fe_qosrslt_get_avail_count_ut);

void fe_qosrslt_print_entry_ut(unsigned int idx)
{
	fe_qosrslt_print_entry(idx);
}
EXPORT_SYMBOL(fe_qosrslt_print_entry_ut);

void fe_qosrslt_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_qosrslt_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_qosrslt_print_range_ut);

void fe_qosrslt_print_table_ut(void)
{
	fe_qosrslt_print_table();
}
EXPORT_SYMBOL(fe_qosrslt_print_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

