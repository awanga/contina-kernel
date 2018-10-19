/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_an_bng_mac.c
 *
 * $Id: cs_fe_table_an_bng_mac.c,v 1.5 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains AN BNG MAC Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_an_bng_mac_table_type;

#define FE_AN_BNG_MAC_TABLE_PTR	(cs_fe_an_bng_mac_table_type.content_table)
#define FE_AN_BNG_MAC_LOCK	&(cs_fe_an_bng_mac_table_type.lock)

static int fe_an_bng_mac_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_an_bng_mac_table_type, rslt_idx,
			start_offset);
} /* fe_an_bng_mac_alloc_entry */

static int fe_an_bng_mac_convert_sw_to_hw_data(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	__u32 value, *p_value;
	fe_an_bng_mac_entry_t *entry = (fe_an_bng_mac_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	p_value = (__u32*)entry->mac;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_AN_BNG_MAC,
			FE_AN_BNG_MAC_ADDR, p_value, p_data_array, size);

	value = (__u32)entry->sa_da;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_AN_BNG_MAC,
			FE_AN_BNG_MAC_SA_DA, &value, p_data_array, size);

	value = (__u32)entry->pspid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_AN_BNG_MAC,
			FE_AN_BNG_MAC_PSPID, &value, p_data_array, size);

	value = (__u32)entry->pspid_mask;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_AN_BNG_MAC,
			FE_AN_BNG_MAC_PSPID_MASK, &value, p_data_array, size);

	value = (__u32)entry->valid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_AN_BNG_MAC,
			FE_AN_BNG_MAC_VALID, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_an_bng_mac_convert_sw_to_hw_data */

static int fe_an_bng_mac_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_an_bng_mac_table_type, idx, entry);
} /* fe_an_bng_mac_set_entry */

static int fe_an_bng_mac_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_an_bng_mac_table_type, entry,
			rslt_idx);
} /* fe_an_bng_mac_add_entry */

static int fe_an_bng_mac_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_an_bng_mac_table_type, idx,
			f_force);
} /* fe_an_bng_mac_del_entry_by_idx */

static int fe_an_bng_mac_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_an_bng_mac_table_type, entry, f_force);
} /* fe_an_bng_mac_del_entry */

static int fe_an_bng_mac_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_an_bng_mac_table_type, entry,
			rslt_idx, start_offset);
} /* fe_an_bng_mac_find_entry */

static int fe_an_bng_mac_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_an_bng_mac_table_type, idx, entry);
} /* fe_an_bng_mac_get_entry */

static int fe_an_bng_mac_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_an_bng_mac_table_type, idx);
} /* fe_an_bng_mac_inc_entry_refcnt */

static int fe_an_bng_mac_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_an_bng_mac_table_type, idx);
} /* fe_an_bng_mac_dec_entry_refcnt */

static int fe_an_bng_mac_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_an_bng_mac_table_type, idx,
			p_cnt);
} /* fe_an_bng_mac_get_entry_refcnt */

static int fe_an_bng_mac_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_an_bng_mac_entry_t *p_rslt_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_an_bng_mac_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_AN_BNG_MAC_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_AN_BNG_MAC_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_AN_BNG_MAC_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_an_bng_mac_table_type);
		spin_unlock_irqrestore(FE_AN_BNG_MAC_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_rslt_entry = (fe_an_bng_mac_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_AN_BNG_MAC_LOCK, flags);
	switch (field) {
	case FE_AN_BNG_MAC_ADDR:
		p_rslt_entry->mac[0] = ((__u8*)p_value)[0];
		p_rslt_entry->mac[1] = ((__u8*)p_value)[1];
		p_rslt_entry->mac[2] = ((__u8*)p_value)[2];
		p_rslt_entry->mac[3] = ((__u8*)p_value)[3];
		p_rslt_entry->mac[4] = ((__u8*)p_value)[4];
		p_rslt_entry->mac[5] = ((__u8*)p_value)[5];
		break;
	case FE_AN_BNG_MAC_SA_DA:
		p_rslt_entry->sa_da = (__u8)p_value[0];
		break;
	case FE_AN_BNG_MAC_PSPID:
		p_rslt_entry->pspid = (__u8)p_value[0];
		break;
	case FE_AN_BNG_MAC_PSPID_MASK:
		p_rslt_entry->pspid_mask = (__u8)p_value[0];
		break;
	case FE_AN_BNG_MAC_VALID:
		p_rslt_entry->valid = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_AN_BNG_MAC_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_AN_BNG_MAC, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_AN_BNG_MAC_LOCK, flags);

	return status;
} /* fe_an_bng_mac_set_field */

static int fe_an_bng_mac_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_an_bng_mac_entry_t *p_rslt_entry;
	__u32 hw_value=0, *p_hw_value;
	__u8 hw_mac_value[6] = { 0, 0, 0, 0, 0, 0 };
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_an_bng_mac_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_AN_BNG_MAC_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_AN_BNG_MAC_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_rslt_entry = (fe_an_bng_mac_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	if (field == FE_AN_BNG_MAC_ADDR)
		p_hw_value = (__u32*)hw_mac_value;
	else p_hw_value = &hw_value;
	spin_lock_irqsave(FE_AN_BNG_MAC_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_AN_BNG_MAC, idx, field,
			p_hw_value);
	spin_unlock_irqrestore(FE_AN_BNG_MAC_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_AN_BNG_MAC_LOCK, flags);
	switch (field) {
	case FE_AN_BNG_MAC_ADDR:
		((__u8*)p_value)[0] = p_rslt_entry->mac[0];
		((__u8*)p_value)[1] = p_rslt_entry->mac[1];
		((__u8*)p_value)[2] = p_rslt_entry->mac[2];
		((__u8*)p_value)[3] = p_rslt_entry->mac[3];
		((__u8*)p_value)[4] = p_rslt_entry->mac[4];
		((__u8*)p_value)[5] = p_rslt_entry->mac[5];
		break;
	case FE_AN_BNG_MAC_SA_DA:
		p_value[0] = (__u32)p_rslt_entry->sa_da;
		break;
	case FE_AN_BNG_MAC_PSPID:
		p_value[0] = (__u32)p_rslt_entry->pspid;
		break;
	case FE_AN_BNG_MAC_PSPID_MASK:
		p_value[0] = (__u32)p_rslt_entry->pspid_mask;
		break;
	case FE_AN_BNG_MAC_VALID:
		p_value[0] = (__u32)p_rslt_entry->valid;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_AN_BNG_MAC_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_AN_BNG_MAC_LOCK, flags);

	if (field == FE_AN_BNG_MAC_ADDR) {
		if ((hw_mac_value[0] != p_value[0]) ||
				(hw_mac_value[1] != p_value[1]) ||
				(hw_mac_value[2] != p_value[2]) ||
				(hw_mac_value[3] != p_value[3]) ||
				(hw_mac_value[4] != p_value[4]) ||
				(hw_mac_value[5] != p_value[5]))
			printk("%s:%d:SW value doesn't not match with HW "
					"value\n", __func__, __LINE__);
	} else {
		if (hw_value != p_value[0])
			printk("%s:%d:SW value doesn't not match with HW "
					"value\n", __func__, __LINE__);
	}

	return FE_TABLE_OK;
} /* fe_an_bng_mac_get_field */

static int fe_an_bng_mac_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_an_bng_mac_table_type);
} /* fe_an_bng_mac_flush_table */

static int fe_an_bng_mac_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_an_bng_mac_table_type);
} /* fe_an_bng_mac_get_avail_count */

static void _fe_an_bng_mac_print_title(void)
{
	printk("\n\n ------------------- AN_BNG_MAC Table -----------------\n");
	printk("|------------------------------------------------------\n");
} /* _fe_an_bng_mac_print_title */

static void _fe_an_bng_mac_print_entry(unsigned int idx)
{
	fe_an_bng_mac_entry_t an_bng_mac_entry, *p_abm;
	__u32 value;
	__u8 value_mac[6];
	int status;
	unsigned int count;

	status = fe_an_bng_mac_get_entry(idx, &an_bng_mac_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_an_bng_mac_get_entry_refcnt(idx, &count);

	p_abm = &an_bng_mac_entry;

	printk("| index: %03d | refcnt: %d ", idx, count);

	cs_fe_hw_table_get_field_value(FE_TABLE_AN_BNG_MAC, idx,
			FE_AN_BNG_MAC_ADDR, (__u32*)value_mac);
	printk("| MAC: %02x.%02x.", p_abm->mac[0], p_abm->mac[1]);
	printk("%02x.%02x.", p_abm->mac[2], p_abm->mac[3]);
	printk("%02x.%02x ", p_abm->mac[4], p_abm->mac[5]);
	printk("(HW %02x.%02x.", value_mac[0], value_mac[1]);
	printk("%02x.%02x.", value_mac[2], value_mac[3]);
	printk("%02x.%02x).\n", value_mac[4], value_mac[5]);

	cs_fe_hw_table_get_field_value(FE_TABLE_AN_BNG_MAC, idx,
			FE_AN_BNG_MAC_SA_DA, &value);
	printk("| SA_DA: %02x (HW %02x)\n", p_abm->sa_da, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_AN_BNG_MAC, idx,
			FE_AN_BNG_MAC_PSPID, &value);
	printk("| PSPID: %02x (HW %02x)\n", p_abm->pspid, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_AN_BNG_MAC, idx,
			FE_AN_BNG_MAC_PSPID_MASK, &value);
	printk("| PSPID MASK: %02x (HW %02x)\n", p_abm->pspid_mask, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_AN_BNG_MAC, idx,
			FE_AN_BNG_MAC_VALID, &value);
	printk("| Valid: %02x (HW %02x)\n", p_abm->valid, value);
} /* _fe_an_bng_mac_print_entry */

static void fe_an_bng_mac_print_entry(unsigned int idx)
{
	if (idx >= FE_AN_BNG_MAC_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_an_bng_mac_print_title();
	_fe_an_bng_mac_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_an_bng_mac_print_entry */

static void fe_an_bng_mac_print_range(unsigned int start_idx,
		unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_AN_BNG_MAC_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_an_bng_mac_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_an_bng_mac_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_an_bng_mac_print_range */

static void fe_an_bng_mac_print_table(void)
{
	unsigned int i;

	_fe_an_bng_mac_print_title();
	for (i = 0; i < cs_fe_an_bng_mac_table_type.max_entry; i++) {
		_fe_an_bng_mac_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_an_bng_mac_print_table */


static cs_fe_table_t cs_fe_an_bng_mac_table_type = {
	.type_id = FE_TABLE_AN_BNG_MAC,
	.max_entry = FE_AN_BNG_MAC_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_an_bng_mac_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_an_bng_mac_convert_sw_to_hw_data,
		.alloc_entry = fe_an_bng_mac_alloc_entry,
		.set_entry = fe_an_bng_mac_set_entry,
		.add_entry = fe_an_bng_mac_add_entry,
		.del_entry_by_idx = fe_an_bng_mac_del_entry_by_idx,
		.del_entry = fe_an_bng_mac_del_entry,
		.find_entry = fe_an_bng_mac_find_entry,
		.get_entry = fe_an_bng_mac_get_entry,
		.inc_entry_refcnt = fe_an_bng_mac_inc_entry_refcnt,
		.dec_entry_refcnt = fe_an_bng_mac_dec_entry_refcnt,
		.get_entry_refcnt = fe_an_bng_mac_get_entry_refcnt,
		.set_field = fe_an_bng_mac_set_field,
		.get_field = fe_an_bng_mac_get_field,
		.flush_table = fe_an_bng_mac_flush_table,
		.get_avail_count = fe_an_bng_mac_get_avail_count,
		.print_entry = fe_an_bng_mac_print_entry,
		.print_range = fe_an_bng_mac_print_range,
		.print_table = fe_an_bng_mac_print_table,
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

int cs_fe_ioctl_an_bng_mac(struct net_device *dev, void *pdata, void *cmd)
{
	fe_an_bng_mac_entry_t *p_rslt = (fe_an_bng_mac_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_an_bng_mac_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_an_bng_mac_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_an_bng_mac_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** AN_BNG_MAC entry 0 ~ %d ***\n",
				FE_AN_BNG_MAC_ENTRY_MAX - 1);
		fe_an_bng_mac_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_an_bng_mac */

/* this API will initialize an_bng_mac table */
int cs_fe_table_an_bng_mac_init(void)
{
	int ret;

	spin_lock_init(FE_AN_BNG_MAC_LOCK);

	cs_fe_an_bng_mac_table_type.content_table = cs_table_alloc(
			cs_fe_an_bng_mac_table_type.max_entry);
	if (cs_fe_an_bng_mac_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_an_bng_mac_table_type.type_id,
			&cs_fe_an_bng_mac_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register AN_BNG_MAC table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_an_bng_mac_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_an_bng_mac_flush_table();

	return FE_TABLE_OK;
} /* cs_fe_table_an_bng_mac_init */
EXPORT_SYMBOL(cs_fe_table_an_bng_mac_init);

void cs_fe_table_an_bng_mac_exit(void)
{
	fe_an_bng_mac_flush_table();

	if (cs_fe_an_bng_mac_table_type.content_table == NULL) return;

	cs_table_dealloc(cs_fe_an_bng_mac_table_type.content_table);
	cs_fe_table_unregister(cs_fe_an_bng_mac_table_type.type_id);
} /* cs_fe_table_an_bng_mac_exit */
EXPORT_SYMBOL(cs_fe_table_an_bng_mac_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_an_bng_mac_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_an_bng_mac_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_an_bng_mac_alloc_entry_ut);

int fe_an_bng_mac_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{

	return fe_an_bng_mac_convert_sw_to_hw_data(sw_entry, p_data_array,
			size);
}
EXPORT_SYMBOL(fe_an_bng_mac_convert_sw_to_hw_data_ut);

int fe_an_bng_mac_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_an_bng_mac_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_an_bng_mac_set_entry_ut);

int fe_an_bng_mac_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_an_bng_mac_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_an_bng_mac_add_entry_ut);

int fe_an_bng_mac_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_an_bng_mac_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_an_bng_mac_del_entry_by_idx_ut);

int fe_an_bng_mac_del_entry_ut(void *entry, bool f_force)
{
	return fe_an_bng_mac_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_an_bng_mac_del_entry_ut);

int fe_an_bng_mac_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_an_bng_mac_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_an_bng_mac_find_entry_ut);

int fe_an_bng_mac_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_an_bng_mac_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_an_bng_mac_get_entry_ut);

int fe_an_bng_mac_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_an_bng_mac_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_an_bng_mac_inc_entry_refcnt_ut);

int fe_an_bng_mac_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_an_bng_mac_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_an_bng_mac_dec_entry_refcnt_ut);

int fe_an_bng_mac_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_an_bng_mac_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_an_bng_mac_get_entry_refcnt_ut);

int fe_an_bng_mac_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_an_bng_mac_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_an_bng_mac_set_field_ut);

int fe_an_bng_mac_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{

	return fe_an_bng_mac_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_an_bng_mac_get_field_ut);

int fe_an_bng_mac_flush_table_ut(void)
{
	return fe_an_bng_mac_flush_table();
}
EXPORT_SYMBOL(fe_an_bng_mac_flush_table_ut);

int fe_an_bng_mac_get_avail_count_ut(void)
{
	return fe_an_bng_mac_get_avail_count();
}
EXPORT_SYMBOL(fe_an_bng_mac_get_avail_count_ut);

void fe_an_bng_mac_print_entry_ut(unsigned int idx)
{
	fe_an_bng_mac_print_entry(idx);
}
EXPORT_SYMBOL(fe_an_bng_mac_print_entry_ut);

void fe_an_bng_mac_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_an_bng_mac_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_an_bng_mac_print_range_ut);

void fe_an_bng_mac_print_table_ut(void)
{
	fe_an_bng_mac_print_table();
}
EXPORT_SYMBOL(fe_an_bng_mac_print_table_ut);

#endif  /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || 
	 * defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */



