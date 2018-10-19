/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_voqpol.c
 *
 * $Id: cs_fe_table_voqpol.c,v 1.6 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains VOQ Policer Table Management APIs.
 */

/* VOQ POLICER Table Management:
 * Each entry contains a lot of different fields; therefore, a generic
 * "cs_fe_update_voqpol" API is being implemented.  Caller has to specify
 * the field and the value to update the entry with given index. */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_voqpol_table_type;

#define FE_VOQPOL_TABLE_PTR	(cs_fe_voqpol_table_type.content_table)
#define FE_VOQPOL_LOCK		&(cs_fe_voqpol_table_type.lock)

static int fe_voqpol_alloc_entry(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_alloc_entry(&cs_fe_voqpol_table_type, rslt_idx,
			start_offset);
} /* fe_voqpol_alloc_entry */

static int fe_voqpol_convert_sw_to_hw_data(void *sw_entry, __u32 *p_data_array,
		unsigned int size)
{
	__u32 value;
	fe_voq_pol_entry_t *entry = (fe_voq_pol_entry_t*)sw_entry;
	memset(p_data_array, 0x0, size << 2);

	value = (__u32)entry->voq_base;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_VOQ_BASE, &value, p_data_array, size);

	value = (__u32)entry->pol_base;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_POL_BASE, &value, p_data_array, size);

	value = (__u32)entry->cpu_pid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_CPUPID, &value, p_data_array, size);

	value = (__u32)entry->ldpid;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_LDPID, &value, p_data_array, size);

	value = (__u32)entry->pppoe_session_id;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_PPPOE_SESSION_ID, &value, p_data_array,
			size);

	value = (__u32)entry->cos_nop;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_COS_NOP, &value, p_data_array, size);

	value = (__u32)entry->parity;
	cs_fe_hw_table_set_field_value_to_sw_data(FE_TABLE_VOQ_POLICER,
			FWD_VOQPOL_MEM_PARITY, &value, p_data_array, size);

	return FE_TABLE_OK;
} /* fe_voqpol_convert_sw_to_hw_data */

static int fe_voqpol_set_entry(unsigned int idx, void *entry)
{
	return fe_table_set_entry(&cs_fe_voqpol_table_type, idx, entry);
} /* fe_voqpol_set_entry */

static int fe_voqpol_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_table_add_entry(&cs_fe_voqpol_table_type, entry, rslt_idx);
} /* fe_voqpol_add_entry */

static int fe_voqpol_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_table_del_entry_by_idx(&cs_fe_voqpol_table_type, idx,
			f_force);
} /* fe_voqpol_del_entry_by_idx */

static int fe_voqpol_del_entry(void *entry, bool f_force)
{
	return fe_table_del_entry(&cs_fe_voqpol_table_type, entry, f_force);
} /* fe_voqpol_del_entry */

static int fe_voqpol_find_entry(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_table_find_entry(&cs_fe_voqpol_table_type, entry, rslt_idx,
			start_offset);
} /* fe_voqpol_find_entry */

static int fe_voqpol_get_entry(unsigned int idx, void *entry)
{
	return fe_table_get_entry(&cs_fe_voqpol_table_type, idx, entry);
} /* fe_voqpol_get_entry */

static int fe_voqpol_inc_entry_refcnt(unsigned int idx)
{
	return fe_table_inc_entry_refcnt(&cs_fe_voqpol_table_type, idx);
} /* fe_voqpol_inc_entry_refcnt */

static int fe_voqpol_dec_entry_refcnt(unsigned int idx)
{
	return fe_table_dec_entry_refcnt(&cs_fe_voqpol_table_type, idx);
} /* fe_voqpol_dec_entry_refcnt */

static int fe_voqpol_get_entry_refcnt(unsigned int idx, unsigned int *p_cnt)
{
	return fe_table_get_entry_refcnt(&cs_fe_voqpol_table_type, idx, p_cnt);
} /* fe_voqpol_get_entry_refcnt */

static int fe_voqpol_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_voq_pol_entry_t *p_rslt_entry;
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_voqpol_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_VOQPOL_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_VOQPOL_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;

	/*
	 * in the case that the user calls set_field() right after it allocates
	 * the resource.  The sw entity is not created yet.
	 */
	if (p_sw_entry->data == NULL) {
		spin_lock_irqsave(FE_VOQPOL_LOCK, flags);
		p_sw_entry->data = fe_table_malloc_table_entry(
				&cs_fe_voqpol_table_type);
		spin_unlock_irqrestore(FE_VOQPOL_LOCK, flags);
		if (p_sw_entry->data == NULL)
			return -ENOMEM;
	}

	p_rslt_entry = (fe_voq_pol_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_rslt_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_VOQPOL_LOCK, flags);
	switch (field) {
	case FWD_VOQPOL_VOQ_BASE:
		p_rslt_entry->voq_base = (__u8)p_value[0];
		break;
	case FWD_VOQPOL_POL_BASE:
		p_rslt_entry->pol_base = (__u8)p_value[0];
		break;
	case FWD_VOQPOL_CPUPID:
		p_rslt_entry->cpu_pid = (__u8)p_value[0];
		break;
	case FWD_VOQPOL_LDPID:
		p_rslt_entry->ldpid = (__u8)p_value[0];
		break;
	case FWD_VOQPOL_PPPOE_SESSION_ID:
		p_rslt_entry->pppoe_session_id = (__u16)p_value[0];
		break;
	case FWD_VOQPOL_COS_NOP:
		p_rslt_entry->cos_nop = (__u8)p_value[0];
		break;
	case FWD_VOQPOL_MEM_PARITY:
		p_rslt_entry->parity = (__u8)p_value[0];
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_VOQPOL_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}

	/* WRITE TO HARDWARE */
	status = cs_fe_hw_table_set_field_value(FE_TABLE_VOQ_POLICER, idx,
			field, p_value);
	spin_unlock_irqrestore(FE_VOQPOL_LOCK, flags);

	return status;
} /* fe_voqpol_set_field */

static int fe_voqpol_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	cs_table_entry_t *p_sw_entry;
	fe_voq_pol_entry_t *p_voqpol_entry;
	__u32 hw_value=0;
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_voqpol_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_VOQPOL_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_VOQPOL_TABLE_PTR, idx);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;
	if ((p_sw_entry->local_data & FE_TABLE_ENTRY_USED) == 0)
		return FE_TABLE_EENTRYNOTRSVD;
	if (p_sw_entry->data == NULL)
		return FE_TABLE_ENULLPTR;

	p_voqpol_entry = (fe_voq_pol_entry_t*)
		((fe_table_entry_t*)p_sw_entry->data)->p_entry;
	if (p_voqpol_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_VOQPOL_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			field, &hw_value);
	spin_unlock_irqrestore(FE_VOQPOL_LOCK, flags);
	if (status != FE_TABLE_OK)
		return status;

	spin_lock_irqsave(FE_VOQPOL_LOCK, flags);
	switch (field) {
	case FWD_VOQPOL_VOQ_BASE:
		p_value[0] = (__u32)p_voqpol_entry->voq_base;
		break;
	case FWD_VOQPOL_POL_BASE:
		p_value[0] = (__u32)p_voqpol_entry->pol_base;
		break;
	case FWD_VOQPOL_CPUPID:
		p_value[0] = (__u32)p_voqpol_entry->cpu_pid;
		break;
	case FWD_VOQPOL_LDPID:
		p_value[0] = (__u32)p_voqpol_entry->ldpid;
		break;
	case FWD_VOQPOL_PPPOE_SESSION_ID:
		p_value[0] = (__u32)p_voqpol_entry->pppoe_session_id;
		break;
	case FWD_VOQPOL_COS_NOP:
		p_value[0] = (__u32)p_voqpol_entry->cos_nop;
		break;
	case FWD_VOQPOL_MEM_PARITY:
		p_value[0] = (__u32)p_voqpol_entry->parity;
		break;
	default:
		/* unsupported field */
		spin_unlock_irqrestore(FE_VOQPOL_LOCK, flags);
		return FE_TABLE_EFIELDNOTSUPP;
	}
	spin_unlock_irqrestore(FE_VOQPOL_LOCK, flags);

	if (hw_value != p_value[0])
		printk("%s:%d:SW value doesn't not match with HW value\n",
				__func__, __LINE__);

	return FE_TABLE_OK;
} /* fe_voqpol_get_field */

static int fe_voqpol_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_voqpol_table_type);
} /* fe_voqpol_flush_table */

static int fe_voqpol_get_avail_count(void)
{
	return fe_table_get_avail_count(&cs_fe_voqpol_table_type);
} /* fe_voqpol_get_avail_count */

static void _fe_voqpol_print_title(void)
{
	printk("\n\n ----------------- VOQ POLICER Table ---------------\n");
	printk("|------------------------------------------------------\n");
} /* _fe_voqpol_print_title */

static void _fe_voqpol_print_entry(unsigned int idx)
{
	fe_voq_pol_entry_t voqpol_entry, *p_voqpol;
	__u32 value;
	int status;
	unsigned int count;

	status = fe_voqpol_get_entry(idx, &voqpol_entry);
	if ((status == FE_TABLE_EENTRYNOTRSVD) ||
			(status == FE_TABLE_ENTRYNOTFOUND))
		printk("| %04d | NOT USED\n", idx);
	if (status != FE_TABLE_OK)
		return;

	fe_voqpol_get_entry_refcnt(idx, &count);

	p_voqpol = &voqpol_entry;

	printk("| index %04d | SW count %d\n", idx, count);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_VOQ_BASE, &value);
	printk(" |- VOQ Base: %02x (HW %02x).\n", p_voqpol->voq_base, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_POL_BASE, &value);
	printk("\tPOL Base: %02x (HW %02x).\n", p_voqpol->pol_base, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_CPUPID, &value);
	printk("\tCPU PID: %02x (HW %02x).\n", p_voqpol->cpu_pid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_LDPID, &value);
	printk("\tLDPID: %02x (HW %02x).\n", p_voqpol->ldpid, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_PPPOE_SESSION_ID, &value);
	printk("\tPPPOE Session ID: %02x (HW %02x).\n",
			p_voqpol->pppoe_session_id, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_COS_NOP, &value);
	printk("\tCOS NOP: %02x (HW %02x).\n", p_voqpol->cos_nop, value);
	cs_fe_hw_table_get_field_value(FE_TABLE_VOQ_POLICER, idx,
			FWD_VOQPOL_MEM_PARITY, &value);
	printk("\tParity: %02x (HW %02x).\n", p_voqpol->parity, value);
	printk("|----------------------------------------------|\n");
} /* _fe_voqpol_print_entry */

static void fe_voqpol_print_entry(unsigned int idx)
{
	if (idx >= cs_fe_voqpol_table_type.max_entry) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_voqpol_print_title();
	_fe_voqpol_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_voqpol_print_entry */

static void fe_voqpol_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) ||
			(end_idx >= cs_fe_voqpol_table_type.max_entry)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_voqpol_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_voqpol_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_voqpol_print_range */

static void fe_voqpol_print_table(void)
{
	unsigned int i;

	_fe_voqpol_print_title();
	for (i = 0; i < cs_fe_voqpol_table_type.max_entry; i++) {
		_fe_voqpol_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_voqpol_print_table */


static cs_fe_table_t cs_fe_voqpol_table_type = {
	.type_id = FE_TABLE_VOQ_POLICER,
	.max_entry = FE_VOQ_POL_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_voq_pol_entry_t),
	.op = {
		.convert_sw_to_hw_data = fe_voqpol_convert_sw_to_hw_data,
		.alloc_entry = fe_voqpol_alloc_entry,
		.set_entry = fe_voqpol_set_entry,
		.add_entry = fe_voqpol_add_entry,
		.del_entry_by_idx = fe_voqpol_del_entry_by_idx,
		.del_entry = fe_voqpol_del_entry,
		.find_entry = fe_voqpol_find_entry,
		.get_entry = fe_voqpol_get_entry,
		.inc_entry_refcnt = fe_voqpol_inc_entry_refcnt,
		.dec_entry_refcnt = fe_voqpol_dec_entry_refcnt,
		.get_entry_refcnt = fe_voqpol_get_entry_refcnt,
		.set_field = fe_voqpol_set_field,
		.get_field = fe_voqpol_get_field,
		.flush_table = fe_voqpol_flush_table,
		.get_avail_count = fe_voqpol_get_avail_count,
		.print_entry = fe_voqpol_print_entry,
		.print_range = fe_voqpol_print_range,
		.print_table = fe_voqpol_print_table,
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

int cs_fe_ioctl_voqpol(struct net_device *dev, void *pdata, void *cmd)
{
	fe_voq_pol_entry_t *p_rslt = (fe_voq_pol_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_voqpol_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_voqpol_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_voqpol_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		printk("*** VOQPOL entry 0 ~ %d ***\n",
				FE_VOQ_POL_ENTRY_MAX - 1);
		fe_voqpol_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_voqpol */

/* this API will initialize voqpol table */
int cs_fe_table_voqpol_init(void)
{
	int ret;
#if 0	// Do we need to reserve default entry?
	unsigned int index;
#endif

	spin_lock_init(FE_VOQPOL_LOCK);

	cs_fe_voqpol_table_type.content_table = cs_table_alloc(
			cs_fe_voqpol_table_type.max_entry);
	if (cs_fe_voqpol_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_voqpol_table_type.type_id,
			&cs_fe_voqpol_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register VOQPOL table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_voqpol_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_voqpol_flush_table();

#if 0	// Do we need to reserve default entry?
	/* Reserve a default entry */
	ret = fe_voqpol_alloc_entry(&index, 0);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to allocate default VOQPOL entry\n",
				__func__, __LINE__);
		return -1;
	}
	if (index != 0) {
		printk("%s:%d:the default index is not 0 (%d)!\n",
				__func__, __LINE__, index);
	}
#endif

	return CS_OK;
} /* cs_fe_table_voqpol_init */
EXPORT_SYMBOL(cs_fe_table_voqpol_init);

void cs_fe_table_voqpol_exit(void)
{
	fe_voqpol_flush_table();

	if (cs_fe_voqpol_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_voqpol_table_type.content_table);
	cs_fe_table_unregister(cs_fe_voqpol_table_type.type_id);
} /* cs_fe_table_voqpol_exit */
EXPORT_SYMBOL(cs_fe_table_voqpol_exit);

#if 0	/* OLD.. don't know if it is going to be needed in the future */
cs_status cs_fe_voqpol_delete_by_voq_id(cs_uint8 voq_id)
{
	fe_table_entry_s *p_curr = GET_VOQPOL_TABLE;
	fe_voq_pol_entry_s *p_voqpol_entry;
	cs_uint16 idx;

	for (idx = 0; idx < FE_VOQ_POL_ENTRY_MAX; idx++) {
		if (p_curr->p_entry != NULL) {
			p_voqpol_entry =
				(fe_voq_pol_entry_s *)(p_curr->p_entry);
			if (p_voqpol_entry->voq_base == voq_id)
				cs_fe_del_voqpol(idx);
		}
		p_curr++;
	}
	return CS_OK;
} /* cs_fe_voqpol_delete_by_voq_id */

cs_status cs_fe_voqpol_delete_by_pol_id(cs_uint8 pol_id)
{
	fe_table_entry_s *p_curr = GET_VOQPOL_TABLE;
	fe_voq_pol_entry_s *p_voqpol_entry;
	cs_uint16 idx;

	for (idx = 0; idx < FE_VOQ_POL_ENTRY_MAX; idx++) {
		if (p_curr->p_entry != NULL) {
			p_voqpol_entry =
				(fe_voq_pol_entry_s *)(p_curr->p_entry);
			if (p_voqpol_entry->pol_base == pol_id)
				cs_fe_del_voqpol(idx);
		}
		p_curr++;
	}
	return CS_OK;
} /* cs_fe_voqpol_delete_by_pol_id */
#endif

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_voqpol_alloc_entry_ut(unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_voqpol_alloc_entry(rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_voqpol_alloc_entry_ut);

int fe_voqpol_convert_sw_to_hw_data_ut(void *sw_entry,
		__u32 *p_data_array, unsigned int size)
{
	return fe_voqpol_convert_sw_to_hw_data(sw_entry, p_data_array, size);
}
EXPORT_SYMBOL(fe_voqpol_convert_sw_to_hw_data_ut);

int fe_voqpol_set_entry_ut(unsigned int idx, void *entry)
{
	return fe_voqpol_set_entry(idx, entry);
}
EXPORT_SYMBOL(fe_voqpol_set_entry_ut);

int fe_voqpol_add_entry_ut(void *entry, unsigned int *rslt_idx)
{
	return fe_voqpol_add_entry(entry, rslt_idx);
}
EXPORT_SYMBOL(fe_voqpol_add_entry_ut);

int fe_voqpol_del_entry_by_idx_ut(unsigned int idx, bool f_force)
{
	return fe_voqpol_del_entry_by_idx(idx, f_force);
}
EXPORT_SYMBOL(fe_voqpol_del_entry_by_idx_ut);

int fe_voqpol_del_entry_ut(void *entry, bool f_force)
{
	return fe_voqpol_del_entry(entry, f_force);
}
EXPORT_SYMBOL(fe_voqpol_del_entry_ut);

int fe_voqpol_find_entry_ut(void *entry, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	return fe_voqpol_find_entry(entry, rslt_idx, start_offset);
}
EXPORT_SYMBOL(fe_voqpol_find_entry_ut);

int fe_voqpol_get_entry_ut(unsigned int idx, void *entry)
{
	return fe_voqpol_get_entry(idx, entry);
}
EXPORT_SYMBOL(fe_voqpol_get_entry_ut);

int fe_voqpol_inc_entry_refcnt_ut(unsigned int idx)
{
	return fe_voqpol_inc_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_voqpol_inc_entry_refcnt_ut);

int fe_voqpol_dec_entry_refcnt_ut(unsigned int idx)
{
	return fe_voqpol_dec_entry_refcnt(idx);
}
EXPORT_SYMBOL(fe_voqpol_dec_entry_refcnt_ut);

int fe_voqpol_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt)
{
	return fe_voqpol_get_entry_refcnt(idx, p_cnt);
}
EXPORT_SYMBOL(fe_voqpol_get_entry_refcnt_ut);

int fe_voqpol_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_voqpol_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_voqpol_set_field_ut);

int fe_voqpol_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_voqpol_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_voqpol_get_field_ut);

int fe_voqpol_flush_table_ut(void)
{
	return fe_voqpol_flush_table();
}
EXPORT_SYMBOL(fe_voqpol_flush_table_ut);

int fe_voqpol_get_avail_count_ut(void)
{
	return fe_voqpol_get_avail_count();
}
EXPORT_SYMBOL(fe_voqpol_get_avail_count_ut);

void fe_voqpol_print_entry_ut(unsigned int idx)
{
	fe_voqpol_print_entry(idx);
}
EXPORT_SYMBOL(fe_voqpol_print_entry_ut);

void fe_voqpol_print_range_ut(unsigned int start_idx, unsigned int end_idx)
{
	fe_voqpol_print_range(start_idx, end_idx);
}
EXPORT_SYMBOL(fe_voqpol_print_range_ut);

void fe_voqpol_print_table_ut(void)
{
	fe_voqpol_print_table();
}
EXPORT_SYMBOL(fe_voqpol_print_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */



