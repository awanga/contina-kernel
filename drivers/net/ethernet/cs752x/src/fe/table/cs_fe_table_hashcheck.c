/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_hashcheck.c
 *
 * $Id: cs_fe_table_hashcheck.c,v 1.3 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains the implementation for HW FE Hash Check Table Management.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

/*
 * Hash Check table is directly related to FWDRSLT idx that should've been
 * obtained before enabling HashCheck.  Therefore, there is no entry
 * addition.  There is only setting the value to a specific entry.
 */

static cs_fe_table_t cs_fe_hashcheck_table_type;

#define FE_HASHCHECK_TABLE_PTR	(cs_fe_hashcheck_table_type.content_table)
#define FE_HASHCHECK_LOCK	&(cs_fe_hashcheck_table_type.lock)

static int fe_hashcheck_convert_sw_to_hw_data(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	fe_hash_check_entry_t *hashcheck_entry;
	__u32 value;
	memset(p_data_array, 0x0, size << 2);

	hashcheck_entry = (fe_hash_check_entry_t*)sw_entry;
	value = hashcheck_entry->check_l4_sp_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_L4_SP_EN, &value, p_data_array, size);

	value = hashcheck_entry->check_l4_dp_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_L4_DP_EN, &value, p_data_array, size);

	value = hashcheck_entry->check_mac_sa_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_MAC_SA_EN, &value, p_data_array, size);

	value = hashcheck_entry->check_mac_da_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_MAC_DA_EN, &value, p_data_array, size);

	value = hashcheck_entry->check_ip_sa_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_IP_SA_EN, &value, p_data_array, size);

	value = hashcheck_entry->check_ip_da_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_IP_DA_EN, &value, p_data_array, size);

	value = hashcheck_entry->check_l4_sp_to_be_chk;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_L4_SP_TO_BE_CHK, &value, p_data_array,
			size);

	value = hashcheck_entry->check_l4_dp_to_be_chk;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_L4_DP_TO_BE_CHK, &value, p_data_array,
			size);

	value = hashcheck_entry->check_l2_check_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_L2_CHECK_IDX, &value, p_data_array, size);

	value = hashcheck_entry->check_ip_sa_check_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_IP_SA_CHECK_IDX, &value, p_data_array,
			size);

	value = hashcheck_entry->check_ip_da_check_idx;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_IP_DA_CHECK_IDX, &value, p_data_array,
			size);

	value = hashcheck_entry->check_parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_PARITY, &value, p_data_array, size);

	value = hashcheck_entry->check_reserved;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_HASH_CHECK_MEM,
			FE_HASH_CHECK_RSVD, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_hashcheck_convert_sw_to_hw_data */

static int fe_hashcheck_set_entry(unsigned int idx, void *entry)
{
	cs_table_entry_t *p_sw_entry;
	__u32 data_array[28];
	unsigned int table_size;
	int ret;
	unsigned long flags;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;

	if (idx >= cs_fe_hashcheck_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;

	if (FE_HASHCHECK_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_HASHCHECK_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	p_sw_entry->local_data |= FE_TABLE_ENTRY_USED;

	/* Deal with HW table first */
	ret = cs_fe_hw_table_get_entry_value(FE_TABLE_HASH_CHECK_MEM, idx, 
			data_array, &table_size);
	if (ret != 0)
		return ret;

	/* Generate the data register value based on the given entry */
	ret = fe_hashcheck_convert_sw_to_hw_data(entry, data_array, table_size);
	if (ret != 0)
		return ret;

	spin_lock_irqsave(FE_HASHCHECK_LOCK, flags);
	/* set it to HW indirect access table */
	ret = cs_fe_hw_table_set_entry_value(FE_TABLE_HASH_CHECK_MEM, idx,
			table_size, data_array);
	spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);
	if (ret != 0)
		return ret;

	/* store the SW entry in SW table */
	spin_lock_irqsave(FE_HASHCHECK_LOCK, flags);
	if (p_sw_entry->data == NULL)
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_hashcheck_table_type);
	if (p_sw_entry->data == NULL) {
		spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);
		return -ENOMEM;
	}
	memcpy(((fe_table_entry_t*)p_sw_entry->data)->p_entry, entry,
			cs_fe_hashcheck_table_type.entry_size);
	spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_hashcheck_set_entry */

static int fe_hashcheck_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_hashcheck_table_type, idx,
			f_force);
} /* fe_hashcheck_del_entry_by_idx */

static int fe_hashcheck_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_hashcheck_table_type, entry, f_force);
} /* fe_hashcheck_del_entry */

static int fe_hashcheck_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_hashcheck_table_type, entry, rslt_idx,
			start_offset);
} /* fe_hashcheck_find_entry */

static int fe_hashcheck_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_hashcheck_table_type, idx, entry);
} /* fe_hashcheck_get_entry */

static int fe_hashcheck_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_hash_check_entry_t *p_rslt_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_hashcheck_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_HASHCHECK_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_HASHCHECK_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_HASHCHECK_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_hashcheck_table_type);
		spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_rslt_entry = (fe_hash_check_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_HASHCHECK_LOCK, flags);
	switch (field) {
	case FE_HASH_CHECK_L4_SP_EN:
		p_rslt_entry->check_l4_sp_en = (__u8)p_value[0];
		break;
	case FE_HASH_CHECK_L4_DP_EN:
		p_rslt_entry->check_l4_dp_en = (__u8)p_value[0];
		break;
	case FE_HASH_CHECK_MAC_SA_EN:
		p_rslt_entry->check_mac_sa_en = (__u8)p_value[0];
		break;
	case FE_HASH_CHECK_MAC_DA_EN:
		p_rslt_entry->check_mac_da_en = (__u8)p_value[0];
		break;
	case FE_HASH_CHECK_IP_SA_EN:
		p_rslt_entry->check_ip_sa_en = (__u8)p_value[0];
		break;
	case FE_HASH_CHECK_IP_DA_EN:
		p_rslt_entry->check_ip_da_en = (__u8)p_value[0];
		break;
	case FE_HASH_CHECK_L4_SP_TO_BE_CHK:
		p_rslt_entry->check_l4_sp_to_be_chk = (__u16)p_value[0];
		break;
	case FE_HASH_CHECK_L4_DP_TO_BE_CHK:
		p_rslt_entry->check_l4_dp_to_be_chk = (__u16)p_value[0];
		break;
	case FE_HASH_CHECK_L2_CHECK_IDX:
		p_rslt_entry->check_l2_check_idx = (__u16)p_value[0];
		break;
	case FE_HASH_CHECK_IP_SA_CHECK_IDX:
		p_rslt_entry->check_ip_sa_check_idx = (__u16)p_value[0];
		break;
	case FE_HASH_CHECK_IP_DA_CHECK_IDX:
		p_rslt_entry->check_ip_da_check_idx = (__u16)p_value[0];
		break;
	case FE_HASH_CHECK_PARITY:
		p_rslt_entry->check_parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			field, p_value);
	spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);

	return status;
} /* fe_hashcheck_set_field */

static int fe_hashcheck_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_hash_check_entry_t *p_rslt_entry;
	__u32 hw_value;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_hashcheck_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_HASHCHECK_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_HASHCHECK_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_rslt_entry = (fe_hash_check_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_HASHCHECK_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			field, &hw_value);
	spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_HASHCHECK_LOCK, flags);
	switch (field) {
	case FE_HASH_CHECK_L4_SP_EN:
		p_value[0] = (__u32)p_rslt_entry->check_l4_sp_en;
		break;
	case FE_HASH_CHECK_L4_DP_EN:
		p_value[0] = (__u32)p_rslt_entry->check_l4_dp_en;
		break;
	case FE_HASH_CHECK_MAC_SA_EN:
		p_value[0] = (__u32)p_rslt_entry->check_mac_sa_en;
		break;
	case FE_HASH_CHECK_MAC_DA_EN:
		p_value[0] = (__u32)p_rslt_entry->check_mac_da_en;
		break;
	case FE_HASH_CHECK_IP_SA_EN:
		p_value[0] = (__u32)p_rslt_entry->check_ip_sa_en;
		break;
	case FE_HASH_CHECK_IP_DA_EN:
		p_value[0] = (__u32)p_rslt_entry->check_ip_da_en;
		break;
	case FE_HASH_CHECK_L4_SP_TO_BE_CHK:
		p_value[0] = (__u32)p_rslt_entry->check_l4_sp_to_be_chk;
		break;
	case FE_HASH_CHECK_L4_DP_TO_BE_CHK:
		p_value[0] = (__u32)p_rslt_entry->check_l4_dp_to_be_chk;
		break;
	case FE_HASH_CHECK_L2_CHECK_IDX:
		p_value[0] = (__u32)p_rslt_entry->check_l2_check_idx;
		break;
	case FE_HASH_CHECK_IP_SA_CHECK_IDX:
		p_value[0] = (__u32)p_rslt_entry->check_ip_sa_check_idx;
		break;
	case FE_HASH_CHECK_IP_DA_CHECK_IDX:
		p_value[0] = (__u32)p_rslt_entry->check_ip_da_check_idx;
		break;
	case FE_HASH_CHECK_PARITY:
		p_value[0] = (__u32)p_rslt_entry->check_parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_HASHCHECK_LOCK, flags);

	if (hw_value != p_value[0]) {
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);
	}

	return FE_TABLE_OK;
} /* fe_hashcheck_get_field */

static int fe_hashcheck_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_hashcheck_table_type);
} /* fe_hashcheck_flush_table */

static void _fe_hashcheck_print_title(void)
{
	printk("|------------------ Hash Check Table ---------------------|\n");
} /* _fe_hash_check_print_title */

static void _fe_hashcheck_print_entry(unsigned int idx)
{
	fe_hash_check_entry_t hashcheck_entry, *p_entry;
	__u32 value;
	int status;

	status = fe_hashcheck_get_entry(idx, &hashcheck_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND)) {
		printk("| index %04d | NOT USED\n", idx);
	}
	if (status != FE_TABLE_OK)
		return;

	p_entry = &hashcheck_entry;

	printk("| index %04d | ", idx);

	/* L2 */
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_MAC_SA_EN, &value);
	printk("L2: SA_EN %01x (hw %01x), ", p_entry->check_mac_sa_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_MAC_DA_EN, &value);
	printk("DA_EN %01x (hw %01x), ", p_entry->check_mac_da_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_L2_CHECK_IDX, &value);
	printk("IDX %04d (hw %04d) |\n", p_entry->check_l2_check_idx, value);

	/* L3 */
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_IP_SA_EN, &value);
	printk("\t|L3: SA_EN %01x (hw %01x), ", p_entry->check_ip_sa_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_IP_SA_CHECK_IDX, &value);
	printk("SA_IDX %04d (hw %04d), ", p_entry->check_ip_sa_check_idx,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_IP_DA_EN, &value);
	printk("DA_EN %01x(hw %01x), ", p_entry->check_ip_da_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_IP_DA_CHECK_IDX, &value);
	printk(" DA_IDX %04d (hw %04d) |\n", p_entry->check_ip_da_check_idx,
			value);

	/* L4 */
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_L4_SP_EN, &value);
	printk("\t|L4: SP_EN %01x (hw %01x), ", p_entry->check_l4_sp_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_L4_SP_TO_BE_CHK, &value);
	printk("SP %04d (hw %04d), ", p_entry->check_l4_sp_to_be_chk, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_L4_DP_EN, &value);
	printk("DP_EN %01x (hw %01x), ", p_entry->check_l4_dp_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_L4_DP_TO_BE_CHK, &value);
	printk("DP %04d (hw %04d) | ", p_entry->check_l4_dp_to_be_chk, value);

	/* Other */
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_PARITY, &value);
	printk("Parity %01x (hw %01x) | ", p_entry->check_parity, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_HASH_CHECK_MEM, idx,
			FE_HASH_CHECK_RSVD, &value);
	printk("Rsvd %04x (hw %04x) |\n", p_entry->check_reserved, value);

	printk("|--------------------------------------------------------|\n");
} /* _fe_hashcheck_print_entry */

static void fe_hashcheck_print_entry(unsigned int idx)
{
	if (idx >= FE_HASH_CHECK_MEM_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_hashcheck_print_title();
	_fe_hashcheck_print_entry(idx);
	printk("|----------------------------------------------|\n");
} /* fe_hashcheck_print_entry */

static void fe_hashcheck_print_range(unsigned int start_idx,
		unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_HASH_CHECK_MEM_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_hashcheck_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_hashcheck_print_entry(i);
		cond_resched();
	}

	printk("|------------------------------------------------------\n");
} /* fe_hashcheck_print_range */

static void fe_hashcheck_print_table(void)
{
	unsigned int i;

	_fe_hashcheck_print_title();
	for (i = 0; i < FE_HASH_CHECK_MEM_ENTRY_MAX; i++) {
		_fe_hashcheck_print_entry(i);
		cond_resched();
	}

	printk("|------------------------------------------------------\n");
} /* fe_hashcheck_print_table */

static cs_fe_table_t cs_fe_hashcheck_table_type = {
	.type_id = FE_TABLE_HASH_CHECK_MEM,
	.max_entry = FE_HASH_CHECK_MEM_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_hash_check_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_hashcheck_convert_sw_to_hw_data,
		.alloc_entry = NULL, /* does not support alloc */
		.set_entry = fe_hashcheck_set_entry,
		.add_entry = NULL, /* does not support add */
		.del_entry_by_idx = fe_hashcheck_del_entry_by_idx,
		.del_entry = fe_hashcheck_del_entry,
		.find_entry = fe_hashcheck_find_entry,
		.get_entry = fe_hashcheck_get_entry,
		.inc_entry_refcnt = NULL, /* doesn't care about refcnt */
		.dec_entry_refcnt = NULL, /* doesn't care about refcnt */
		.get_entry_refcnt = NULL, /* doesn't care about refcnt */
		.set_field = fe_hashcheck_set_field,
		.get_field = fe_hashcheck_get_field,
		.flush_table = fe_hashcheck_flush_table,
		.get_avail_count = NULL, /* doesn't care about it */
		.print_entry = fe_hashcheck_print_entry,
		.print_range = fe_hashcheck_print_range,
		.print_table = fe_hashcheck_print_table,
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

int cs_fe_ioctl_hashcheck(struct net_device *dev, void *pdata, void *cmd)
{
	fe_hash_check_entry_t *p_rslt = (fe_hash_check_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	int status;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_hashcheck_set_entry(fe_cmd_hdr->idx_start, p_rslt);
		break;
	case CMD_DELETE:
		fe_hashcheck_del_entry_by_idx(fe_cmd_hdr->idx_start, false);
		break;
	case CMD_FLUSH:
		fe_hashcheck_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		fe_hashcheck_print_range(fe_cmd_hdr->idx_start,
				fe_cmd_hdr->idx_end);
		break;
	case CMD_REPLACE:	/* ignore */
		break;
	case CMD_INIT:	/* ignore */
		break;
	default:
		return -1;
	}

	return FE_TABLE_OK;
} /* cs_fe_ioctl_hashcheck */

/* this API will initialize hashcheck table */
int cs_fe_table_hashcheck_init(void)
{
	int ret;

	spin_lock_init(FE_HASHCHECK_LOCK);

	cs_fe_hashcheck_table_type.content_table = cs_table_alloc(
			cs_fe_hashcheck_table_type.max_entry);
	if (cs_fe_hashcheck_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_hashcheck_table_type.type_id,
			&cs_fe_hashcheck_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register HASHCHECK table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_hashcheck_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_hashcheck_flush_table();

	return CS_OK;
} /* cs_fe_table_hashcheck_init */
EXPORT_SYMBOL(cs_fe_table_hashcheck_init);

void cs_fe_table_hashcheck_exit(void)
{
	fe_hashcheck_flush_table();

	if (cs_fe_hashcheck_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_hashcheck_table_type.content_table);
	cs_fe_table_unregister(cs_fe_hashcheck_table_type.type_id);
} /* cs_fe_table_hashcheck_exit */
EXPORT_SYMBOL(cs_fe_table_hashcheck_exit);

