/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_fvlan.c
 *
 * $Id: cs_fe_table_fvlan.c,v 1.5 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains Flow VLAN Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_fvlan_table_type;

#define FE_FVLAN_TABLE_PTR	(cs_fe_fvlan_table_type.content_table)
#define FE_FVLAN_LOCK		&(cs_fe_fvlan_table_type.lock)

static int fe_fvlan_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_fvlan_table_type, rslt_idx,
			start_offset);
} /* fe_fvlan_alloc_entry */

static int fe_fvlan_convert_sw_to_hw_data(void *sw_entry, __u32 *p_data_array,
		unsigned int size)
{
	__u32 value;
	fe_flow_vlan_entry_t *entry = (fe_flow_vlan_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->first_vlan_cmd;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_FIRST_VLAN_CMD, &value, p_data_array, size);

	value = (__u32)entry->first_vid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_FIRST_VID, &value, p_data_array, size);

	value = (__u32)entry->first_tpid_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_FIRST_TPID_ENC, &value, p_data_array, size);

	value = (__u32)entry->second_vlan_cmd;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_SECOND_VLAN_CMD, &value, p_data_array, size);

	value = (__u32)entry->second_vid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_SECOND_VID, &value, p_data_array, size);

	value = (__u32)entry->second_tpid_enc;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_SECOND_TPID_ENC, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_FVLAN,
			FWD_FVLAN_MEM_PARITY, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_fvlan_convert_sw_to_hw_data */

static int fe_fvlan_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_fvlan_table_type, idx, entry);
} /* fe_fvlan_set_entry */

static int fe_fvlan_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_fvlan_table_type, entry, rslt_idx);
} /* fe_fvlan_add_entry */

static int fe_fvlan_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_fvlan_table_type, idx, f_force);
} /* fe_fvlan_del_entry_by_idx */

static int fe_fvlan_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_fvlan_table_type, entry, f_force);
} /* fe_fvlan_del_entry */

static int fe_fvlan_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_fvlan_table_type, entry, rslt_idx,
			start_offset);
} /* fe_fvlan_find_entry */

static int fe_fvlan_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_fvlan_table_type, idx, entry);
} /* fe_fvlan_get_entry */

static int fe_fvlan_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_fvlan_table_type, idx);
} /* fe_fvlan_inc_entry_refcnt */

static int fe_fvlan_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_fvlan_table_type, idx);
} /* fe_fvlan_dec_entry_refcnt */

static int fe_fvlan_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_fvlan_table_type, idx, p_cnt);
} /* fe_fvlan_get_entry_refcnt */

static int fe_fvlan_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_flow_vlan_entry_t *p_fvlan_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_fvlan_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_FVLAN_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_FVLAN_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_FVLAN_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_fvlan_table_type);
		spin_unlock_irqrestore(FE_FVLAN_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_fvlan_entry = (fe_flow_vlan_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_fvlan_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_FVLAN_LOCK, flags);
	switch (field) {
	case FWD_FVLAN_FIRST_VLAN_CMD:
		p_fvlan_entry->first_vlan_cmd = (__u8)p_value[0];
		break;
	case FWD_FVLAN_FIRST_VID:
		p_fvlan_entry->first_vid = (__u16)p_value[0];
		break;
	case FWD_FVLAN_FIRST_TPID_ENC:
		p_fvlan_entry->first_tpid_enc = (__u8)p_value[0];
		break;
	case FWD_FVLAN_SECOND_VLAN_CMD:
		p_fvlan_entry->second_vlan_cmd = (__u8)p_value[0];
		break;
	case FWD_FVLAN_SECOND_VID:
		p_fvlan_entry->second_vid = (cs_uint16)p_value[0];
		break;
	case FWD_FVLAN_SECOND_TPID_ENC:
		p_fvlan_entry->second_tpid_enc = (__u8)p_value[0];
		break;
	case FWD_FVLAN_MEM_PARITY:
		p_fvlan_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_FVLAN_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_FVLAN, idx, field,
			p_value);
	spin_unlock_irqrestore(FE_FVLAN_LOCK, flags);

	return status;
} /* fe_fvlan_set_field */

static int fe_fvlan_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_flow_vlan_entry_t *p_fvlan_entry;
	__u32 hw_value;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_fvlan_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_FVLAN_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_FVLAN_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_fvlan_entry = (fe_flow_vlan_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_fvlan_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_FVLAN_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx, field,
			&hw_value);
	spin_unlock_irqrestore(FE_FVLAN_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_FVLAN_LOCK, flags);
	switch (field) {
	case FWD_FVLAN_FIRST_VLAN_CMD:
		p_value[0] = (__u32)p_fvlan_entry->first_vlan_cmd;
		break;
	case FWD_FVLAN_FIRST_VID:
		p_value[0] = (__u32)p_fvlan_entry->first_vid;
		break;
	case FWD_FVLAN_FIRST_TPID_ENC:
		p_value[0] = (__u32)p_fvlan_entry->first_tpid_enc;
		break;
	case FWD_FVLAN_SECOND_VLAN_CMD:
		p_value[0] = (__u32)p_fvlan_entry->second_vlan_cmd;
		break;
	case FWD_FVLAN_SECOND_VID:
		p_value[0] = (__u32)p_fvlan_entry->second_vid;
		break;
	case FWD_FVLAN_SECOND_TPID_ENC:
		p_value[0] = (__u32)p_fvlan_entry->second_tpid_enc;
		break;
	case FWD_FVLAN_MEM_PARITY:
		p_value[0] = (__u32)p_fvlan_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_FVLAN_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_FVLAN_LOCK, flags);

	if (hw_value != p_value[0]) {
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);
	}

	return FE_TABLE_OK;
} /* fe_fvlan_get_field */

static int fe_fvlan_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_fvlan_table_type);
} /* fe_fvlan_flush_table */

static int fe_fvlan_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_fvlan_table_type);
} /* fe_fvlan_get_avail_count */

static void _fe_fvlan_print_title(void)
{
	printk("\n\n ----------------- Flow VLAN Table -----------------\n");
	printk("|------------------------------------------------------\n");
} /* _fe_fvlan_print_title */

static void _fe_fvlan_print_entry(unsigned int idx)
{
	fe_flow_vlan_entry_t fvlan_entry, *p_fvlan;
	__u32 value;
	int status;
	unsigned int count;

	status = fe_fvlan_get_entry(idx, &fvlan_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_fvlan_get_entry_refcnt(idx, &count);

	p_fvlan = &fvlan_entry;

	printk("| index: %04d | refcnt: %d ", idx, count);

	/* 1st VLAN */
	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_FIRST_VLAN_CMD, &value);
	printk(" |- 1st VLAN cmd: %02x (HW %02x). ",
			p_fvlan->first_vlan_cmd, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_FIRST_VID, &value);
	printk("ID: %04x (HW %04x). ", p_fvlan->first_vid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_FIRST_TPID_ENC, &value);
	printk("TPID Enc: %02x (HW %02x).\n", p_fvlan->first_tpid_enc, value);

	/* 2nd VLAN */
	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_SECOND_VLAN_CMD, &value);
	printk(" |- 2nd VLAN cmd: %02x (HW %02x). ",
			p_fvlan->second_vlan_cmd, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_SECOND_VID, &value);
	printk("ID: %04x (HW %04x). ", p_fvlan->second_vid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_SECOND_TPID_ENC, &value);
	printk("TPID Enc: %02x (HW %02x).\n", p_fvlan->second_tpid_enc, value);

	cs_fe_hw_table_get_field_value(FE_TABLE_FVLAN, idx,
			FWD_FVLAN_MEM_PARITY, &value);
	printk(" |- Parity: %02x (HW %02x). ", p_fvlan->parity, value);
} /* _fe_fvlan_print_entry */

static void fe_fvlan_print_entry(unsigned int idx)
{
	if (idx >= FE_FVLAN_ENTRY_MAX) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_fvlan_print_title();
	_fe_fvlan_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_fvlan_print_entry */

static void fe_fvlan_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) || (end_idx >= FE_FVLAN_ENTRY_MAX)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_fvlan_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_fvlan_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_fvlan_print_range */

static void fe_fvlan_print_table(void)
{
	unsigned int i;

	_fe_fvlan_print_title();
	for (i = 0; i < cs_fe_fvlan_table_type.max_entry; i++) {
		_fe_fvlan_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_fvlan_print_table */


static cs_fe_table_t cs_fe_fvlan_table_type = {
	.type_id = FE_TABLE_FVLAN,
	.max_entry = FE_FVLAN_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_flow_vlan_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_fvlan_convert_sw_to_hw_data,
		.alloc_entry = fe_fvlan_alloc_entry,
		.set_entry = fe_fvlan_set_entry,
		.add_entry = fe_fvlan_add_entry,
		.del_entry_by_idx = fe_fvlan_del_entry_by_idx,
		.del_entry = fe_fvlan_del_entry,
		.find_entry = fe_fvlan_find_entry,
		.get_entry = fe_fvlan_get_entry,
		.inc_entry_refcnt = fe_fvlan_inc_entry_refcnt,
		.dec_entry_refcnt = fe_fvlan_dec_entry_refcnt,
		.get_entry_refcnt = fe_fvlan_get_entry_refcnt,
		.set_field = fe_fvlan_set_field,
		.get_field = fe_fvlan_get_field,
		.flush_table = fe_fvlan_flush_table,
		.get_avail_count = fe_fvlan_get_avail_count,
		.print_entry = fe_fvlan_print_entry,
		.print_range = fe_fvlan_print_range,
		.print_table = fe_fvlan_print_table,
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

int cs_fe_ioctl_fvlan(struct net_device *dev, void *pdata, void *cmd)
{
	fe_flow_vlan_entry_t *p_rslt = (fe_flow_vlan_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_fvlan_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_fvlan_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_fvlan_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** FVLAN entry 0 ~ %d ***\n", FE_FVLAN_ENTRY_MAX - 1);
		fe_fvlan_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_fvlan */

/* this API will initialize fvlan table */
int cs_fe_table_fvlan_init(void)
{
	int ret;

	spin_lock_init(FE_FVLAN_LOCK);

	cs_fe_fvlan_table_type.content_table = cs_table_alloc(
			cs_fe_fvlan_table_type.max_entry);
	if (cs_fe_fvlan_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_fvlan_table_type.type_id,
			&cs_fe_fvlan_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register FVLAN table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_fvlan_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_fvlan_flush_table();

	return FE_TABLE_OK;
} /* cs_fe_table_fvlan_init */
EXPORT_SYMBOL(cs_fe_table_fvlan_init);

void cs_fe_table_fvlan_exit(void)
{
	fe_fvlan_flush_table();

	if (cs_fe_fvlan_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_fvlan_table_type.content_table);
	cs_fe_table_unregister(cs_fe_fvlan_table_type.type_id);
} /* cs_fe_table_fvlan_exit */
EXPORT_SYMBOL(cs_fe_table_fvlan_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_fvlan_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_fvlan_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_fvlan_alloc_entry_ut);

int fe_fvlan_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_fvlan_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_fvlan_convert_sw_to_hw_data_ut);

int fe_fvlan_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_fvlan_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_fvlan_set_entry_ut);

int fe_fvlan_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_fvlan_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_fvlan_add_entry_ut);

int fe_fvlan_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_fvlan_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_fvlan_del_entry_by_idx_ut);

int fe_fvlan_del_entry_ut(void *entry, bool f_force)
{
	return fe_fvlan_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_fvlan_del_entry_ut);

int fe_fvlan_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_fvlan_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_fvlan_find_entry_ut);

int fe_fvlan_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_fvlan_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_fvlan_get_entry_ut);

int fe_fvlan_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_fvlan_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_fvlan_inc_entry_refcnt_ut);

int fe_fvlan_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_fvlan_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_fvlan_dec_entry_refcnt_ut);

int fe_fvlan_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_fvlan_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_fvlan_get_entry_refcnt_ut);

int fe_fvlan_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_fvlan_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_fvlan_set_field_ut);

int fe_fvlan_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_fvlan_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_fvlan_get_field_ut);

int fe_fvlan_flush_table_ut(void)
{
	return fe_fvlan_flush_table();
}
EXPORT_SYMBOL(fe_fvlan_flush_table_ut);

int fe_fvlan_get_avail_count_ut(void)
{
	return fe_fvlan_get_avail_count();
}
EXPORT_SYMBOL(fe_fvlan_get_avail_count_ut);

void fe_fvlan_print_entry_ut(unsigned int idx)
{
	fe_fvlan_print_entry(idx);
}
EXPORT_SYMBOL(fe_fvlan_print_entry_ut);

void fe_fvlan_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_fvlan_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_fvlan_print_range_ut);

void fe_fvlan_print_table_ut(void)
{
	fe_fvlan_print_table();
}
EXPORT_SYMBOL(fe_fvlan_print_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

