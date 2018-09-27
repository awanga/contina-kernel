/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_lpb.c
 *
 * $Id: cs_fe_table_lpb.c,v 1.6 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains Logic Port Behavior Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_lpb_table_type;

#define FE_LPB_TABLE_PTR	(cs_fe_lpb_table_type.content_table)
#define FE_LPB_LOCK		&(cs_fe_lpb_table_type.lock)

static int fe_lpb_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_lpb_table_type, rslt_idx,
			start_offset);
} /* fe_lpb_alloc_entry */

static int fe_lpb_convert_sw_to_hw_data(void *sw_entry, __u32 *p_data_array,
		unsigned int size)
{
	__u32 value;
	fe_lpb_entry_t *entry = (fe_lpb_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->lspid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_LSPID, &value, p_data_array, size);

	value = (__u32)entry->pvid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_PVID, &value, p_data_array, size);

	value = (__u32)entry->pvid_tpid_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_PVID_TPID_ENC, &value, p_data_array, size);

	value = (__u32)entry->olspid_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_OLSPID_EN, &value, p_data_array, size);

	value = (__u32)entry->olspid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_OLSPID, &value, p_data_array, size);

	value = (__u32)entry->olspid_preserve_en;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_OLSPID_PRESERVE_EN, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_LPB,
			FE_LPB_MEM_PARITY, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_lpb_convert_sw_to_hw_data */

static int fe_lpb_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_lpb_table_type, idx, entry);
} /* fe_lpb_set_entry */

static int fe_lpb_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_lpb_table_type, idx, f_force);
} /* fe_lpb_del_entry_by_idx */

static int fe_lpb_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_lpb_table_type, entry, rslt_idx,
			start_offset);
} /* fe_lpb_find_entry */

static int fe_lpb_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_lpb_table_type, idx, entry);
} /* fe_lpb_get_entry */

static int fe_lpb_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_lpb_entry_t *p_rslt_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_lpb_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_LPB_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_LPB_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_LPB_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_lpb_table_type);
		spin_unlock_irqrestore(FE_LPB_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_rslt_entry = (fe_lpb_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_LPB_LOCK, flags);
	switch (field) {
	case FE_LPB_LSPID:
		p_rslt_entry->lspid = (__u8)p_value[0];
		break;
	case FE_LPB_PVID:
		p_rslt_entry->pvid = (__u16)p_value[0];
		break;
	case FE_LPB_PVID_TPID_ENC:
		p_rslt_entry->pvid_tpid_enc = (__u8)p_value[0];
		break;
	case FE_LPB_OLSPID_EN:
		p_rslt_entry->olspid_en = (__u8)p_value[0];
		break;
	case FE_LPB_OLSPID:
		p_rslt_entry->olspid = (__u8)p_value[0];
		break;
	case FE_LPB_OLSPID_PRESERVE_EN:
		p_rslt_entry->olspid_preserve_en = (__u8)p_value[0];
		break;
	case FE_LPB_MEM_PARITY:
		p_rslt_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_LPB_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_LPB, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_LPB_LOCK, flags);

	return status;
} /* fe_lpb_set_field */

static int fe_lpb_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_lpb_entry_t *p_rslt_entry;
	__u32 hw_value=0;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_lpb_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_LPB_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_LPB_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_rslt_entry = (fe_lpb_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_LPB_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx, field,
			&hw_value);
	spin_unlock_irqrestore(FE_LPB_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_LPB_LOCK, flags);
	switch (field) {
	case FE_LPB_LSPID:
		p_value[0] = (__u32)p_rslt_entry->lspid;
		break;
	case FE_LPB_PVID:
		p_value[0] = (__u32)p_rslt_entry->pvid;
		break;
	case FE_LPB_PVID_TPID_ENC:
		p_value[0] = (__u32)p_rslt_entry->pvid_tpid_enc;
		break;
	case FE_LPB_OLSPID_EN:
		p_value[0] = (__u32)p_rslt_entry->olspid_en;
		break;
	case FE_LPB_OLSPID:
		p_value[0] = (__u32)p_rslt_entry->olspid;
		break;
	case FE_LPB_OLSPID_PRESERVE_EN:
		p_value[0] = (__u32)p_rslt_entry->olspid_preserve_en;
		break;
	case FE_LPB_MEM_PARITY:
		p_value[0] = (__u32)p_rslt_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_LPB_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_LPB_LOCK, flags);

	if (hw_value != p_value[0]) {
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);
	}

	return FE_TABLE_OK;
} /* fe_lpb_get_field */

static int fe_lpb_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_lpb_table_type);
} /* fe_lpb_flush_table */

static void _fe_lpb_print_title(void)
{
	printk("\n\n ------------------- LPB Table -----------------\n");
	printk("|------------------------------------------------------\n");
} /* _fe_lpb_print_title */

static void _fe_lpb_print_entry(unsigned int idx)
{
	fe_lpb_entry_t lpb_entry, *p_lpb;
	__u32 value;
	int status;

	status = fe_lpb_get_entry(idx, &lpb_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	p_lpb = &lpb_entry;

	printk("| index: %04d ", idx);

	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx, FE_LPB_LSPID, &value);
	printk("| LSPID: %02x  (HW %02x)\n", p_lpb->lspid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx, FE_LPB_PVID, &value);
	printk("| PVID: %04x  (HW %04x)\n", p_lpb->pvid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx,
			FE_LPB_PVID_TPID_ENC, &value);
	printk("| PVID_TPID_ENC: %02x  (HW %02x)\n", p_lpb->pvid_tpid_enc,
			value);
	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx, FE_LPB_OLSPID_EN,
			&value);
	printk("| OLSPID_EN: %02x  (HW %02x)\n", p_lpb->olspid_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx, FE_LPB_OLSPID,
			&value);
	printk("| OLSPID: %02x  (HW %02x)\n", p_lpb->olspid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx,
			FE_LPB_OLSPID_PRESERVE_EN, &value);
	printk("| OLSPID_PRESERVE_EN: %02x  (HW %02x)\n",
			p_lpb->olspid_preserve_en, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_LPB, idx, FE_LPB_MEM_PARITY,
			&value);
	printk("| MEM_PARITY: %02x  (HW %02x)\n", p_lpb->parity, value);
} /* _fe_lpb_print_entry */

static void fe_lpb_print_entry(unsigned int idx)
{
	if (idx >= FE_LPB_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_lpb_print_title();
	_fe_lpb_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_lpb_print_entry */

static void fe_lpb_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_LPB_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_lpb_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_lpb_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_lpb_print_range */

static void fe_lpb_print_table(void)
{
	unsigned int i;

	_fe_lpb_print_title();
	for (i = 0; i < cs_fe_lpb_table_type.max_entry; i++) {
		_fe_lpb_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_lpb_print_table */


static cs_fe_table_t cs_fe_lpb_table_type = {
	.type_id = FE_TABLE_LPB,
	.max_entry = FE_LPB_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_lpb_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_lpb_convert_sw_to_hw_data,
		.alloc_entry = fe_lpb_alloc_entry,
		.set_entry = fe_lpb_set_entry,
		.add_entry = NULL, /* does not support add */
		.del_entry_by_idx = fe_lpb_del_entry_by_idx,
		.del_entry = NULL, /* does not support delete */
		.find_entry = fe_lpb_find_entry,
		.get_entry = fe_lpb_get_entry,
		.inc_entry_refcnt = NULL,	/* doesn't care refcnt */
		.dec_entry_refcnt = NULL,	/* doesn't care refcnt */
		.get_entry_refcnt = NULL,	/* doesn't care refcnt */
		.set_field = fe_lpb_set_field,
		.get_field = fe_lpb_get_field,
		.flush_table = fe_lpb_flush_table,
		.get_avail_count = NULL,	/* doesn't care it */
		.print_entry = fe_lpb_print_entry,
		.print_range = fe_lpb_print_range,
		.print_table = fe_lpb_print_table,
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

int cs_fe_ioctl_lpb(struct net_device *dev, void *pdata, void *cmd)
{
	fe_lpb_entry_t *p_rslt = (fe_lpb_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_lpb_set_entry(fe_cmd_hdr->idx_start, p_rslt);
		break;
	case CMD_DELETE:
		/* does not support */
		break;
	case CMD_FLUSH:
		fe_lpb_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** LPB entry 0 ~ %d ***\n", FE_LPB_ENTRY_MAX - 1);
		fe_lpb_print_range(fe_cmd_hdr->idx_start, fe_cmd_hdr->idx_end);
		break;
	case CMD_REPLACE:	/* ignore */
		break;
	case CMD_INIT:	/* ignore */
		break;
	default:
		return -1;
	}

	return status;
} /* cs_fe_ioctl_lpb */

/* this API will initialize lpb table */
int cs_fe_table_lpb_init(void)
{
	int ret;
	unsigned int i, index;

	spin_lock_init(FE_LPB_LOCK);

	cs_fe_lpb_table_type.content_table = cs_table_alloc(
			cs_fe_lpb_table_type.max_entry);
	if (cs_fe_lpb_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_lpb_table_type.type_id,
			&cs_fe_lpb_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register LPB table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_lpb_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_lpb_flush_table();

	/* allocate all the entries in LPB table, since the allocation of an
	 * empty entry is not required here */
	for (i = 0; i < FE_LPB_ENTRY_MAX; i++)
		fe_lpb_alloc_entry(&index, i);

	return FE_TABLE_OK;
} /* cs_fe_table_lpb_init */
EXPORT_SYMBOL(cs_fe_table_lpb_init);

void cs_fe_table_lpb_exit(void)
{
	fe_lpb_flush_table();

	if (cs_fe_lpb_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_lpb_table_type.content_table);
	cs_fe_table_unregister(cs_fe_lpb_table_type.type_id);
} /* cs_fe_table_lpb_exit */
EXPORT_SYMBOL(cs_fe_table_lpb_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_lpb_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_lpb_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_lpb_alloc_entry_ut);

int fe_lpb_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_lpb_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_lpb_convert_sw_to_hw_data_ut);

int fe_lpb_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_lpb_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_lpb_set_entry_ut);

int fe_lpb_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_lpb_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_lpb_del_entry_by_idx_ut);

int fe_lpb_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_lpb_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_lpb_find_entry_ut);

int fe_lpb_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_lpb_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_lpb_get_entry_ut);

int fe_lpb_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_lpb_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_lpb_set_field_ut);

int fe_lpb_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_lpb_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_lpb_get_field_ut);

int fe_lpb_flush_table_ut(void)
{
	return fe_lpb_flush_table();
}
EXPORT_SYMBOL(fe_lpb_flush_table_ut);

void fe_lpb_print_entry_ut(unsigned int idx)
{
	fe_lpb_print_entry(idx);
}
EXPORT_SYMBOL(fe_lpb_print_entry_ut);

void fe_lpb_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_lpb_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_lpb_print_range_ut);

void fe_lpb_print_table_ut(void)
{
	fe_lpb_print_table();
}
EXPORT_SYMBOL(fe_lpb_print_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

