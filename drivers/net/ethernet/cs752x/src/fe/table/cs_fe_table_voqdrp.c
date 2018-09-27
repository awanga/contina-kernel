/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_voqdrp.c
 *
 * $Id: cs_fe_table_voqdrp.c,v 1.3 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains Packet Editor VOQ Drop Table Management APIs.
 */

/* Packet Editor VOQ Table Management:
 * This table management is different than most of other tables. 
 * In hardware table, VOQ_BIT[6:2] are used as the address, and
 * VOQ_BIT[1:0] are used as the data.  However, in the SW table
 * management, each entry represents one VOQ. */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_voqdrp_table_type;

#define FE_VOQDRP_TABLE_PTR	(cs_fe_voqdrp_table_type.content_table)
#define FE_VOQDRP_LOCK		&(cs_fe_voqdrp_table_type.lock)

int fe_voqdrp_enbl(unsigned int voq_idx)
{
	cs_table_entry_t *p_sw_entry;
	unsigned int voq_addr;
	__u32 voq_data;
	int status;
	unsigned long flags;

	voq_addr = voq_idx >> 2;

	if (voq_idx >= cs_fe_voqdrp_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_VOQDRP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_VOQDRP_TABLE_PTR, voq_idx >> 5);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_VOQDRP_LOCK, flags);
	/* Update SW table */
	p_sw_entry->local_data |= BIT_MASK(voq_idx & 0x001f);

	/* Update HW table */
	status = cs_fe_hw_table_get_field_value(FE_TABLE_PE_VOQ_DROP, voq_addr,
			FE_PE_VOQ_DRP_DROP_CFG_DATA, &voq_data);
	if (status != FE_TABLE_OK) {
		spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
		return status;
	}

	voq_data |= BIT_MASK(voq_idx & 0x03);

	status = cs_fe_hw_table_set_field_value(FE_TABLE_PE_VOQ_DROP, voq_addr,
			FE_PE_VOQ_DRP_DROP_CFG_DATA, &voq_data);
	spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
	return status;
} /* fe_voqdrp_enbl */

int fe_voqdrp_dsbl(unsigned int voq_idx)
{
	cs_table_entry_t *p_sw_entry;
	unsigned int voq_addr;
	__u32 voq_data;
	int status;
	unsigned long flags;

	voq_addr = voq_idx >> 2;

	if (voq_idx >= cs_fe_voqdrp_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_VOQDRP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	p_sw_entry = cs_table_get_entry(FE_VOQDRP_TABLE_PTR, voq_idx >> 5);
	if (p_sw_entry == NULL)
		return FE_TABLE_ENULLPTR;

	spin_lock_irqsave(FE_VOQDRP_LOCK, flags);
	/* Update SW table */
	p_sw_entry->local_data &= ~(BIT_MASK(voq_idx & 0x001f));

	/* Update HW table */
	status = cs_fe_hw_table_get_field_value(FE_TABLE_PE_VOQ_DROP, voq_addr,
			FE_PE_VOQ_DRP_DROP_CFG_DATA, &voq_data);
	if (status != FE_TABLE_OK) {
		spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
		return status;
	}

	voq_data &= ~BIT_MASK(voq_idx & 0x03);

	status = cs_fe_hw_table_set_field_value(FE_TABLE_PE_VOQ_DROP, voq_addr,
			FE_PE_VOQ_DRP_DROP_CFG_DATA, &voq_data);
	spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
	return status;
} /* fe_voqdrp_dsbl */

cs_status fe_voqdrp_get(unsigned int voq_idx, __u8 *p_enbl)
{
	cs_table_entry_t *p_sw_entry;
	unsigned int voq_addr = voq_idx >> 2;
	__u32 voq_data;
	int status;
	__u8 hw_rslt;
	unsigned long flags;

	if (p_enbl == NULL)
		return FE_TABLE_ENULLPTR;
	if (voq_idx >= cs_fe_voqdrp_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;
	if (FE_VOQDRP_TABLE_PTR == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	/* Obtain HW result */
	spin_lock_irqsave(FE_VOQDRP_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_PE_VOQ_DROP, voq_addr,
			FE_PE_VOQ_DRP_DROP_CFG_DATA, &voq_data);
	if (status != FE_TABLE_OK) {
		spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
		return status;
	}

	if (voq_data & BIT_MASK(voq_idx & 0x03))
		hw_rslt = 1;
	else
		hw_rslt = 0;

	/* Obtain SW result */
	p_sw_entry = cs_table_get_entry(FE_VOQDRP_TABLE_PTR, voq_idx >> 5);
	if (p_sw_entry == NULL) {
		spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
		return FE_TABLE_ENULLPTR;
	}

	if (p_sw_entry->local_data & BIT_MASK(voq_idx & 0x001f))
		*p_enbl = 1;
	else
		*p_enbl = 0;

	spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);

	if (hw_rslt != *p_enbl) {
		printk("%s:%d:HW value does not match with SW!!\n", __func__,
				__LINE__);
		return -1;
	}

	return FE_TABLE_OK;
} /* fe_voqdrp_get */

/* since PE VOQDRP table is different from others, it will ignore
 * the argument "idx." */
static int fe_voqdrp_set_entry(unsigned int idx, void *entry)
{
	fe_pe_voq_drp_entry_t *voqdrp_entry = (fe_pe_voq_drp_entry_t*)entry;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;

	if (voqdrp_entry->f_drop_enbl == 1)
		return fe_voqdrp_enbl(voqdrp_entry->voq_id);
	else
		return fe_voqdrp_dsbl(voqdrp_entry->voq_id);
} /* fe_voqdrp_set_entry */

/* since PE VOQDRP table is different from others, it will ignore
 * the argument "rslt_idx." */
static int fe_voqdrp_add_entry(void *entry, unsigned int *rslt_idx)
{
	return fe_voqdrp_set_entry(0, entry);
} /* fe_voqdrp_add_entry */

/* del_entry is used as similar to fe_voqpol_dsbl */
static int fe_voqdrp_del_entry_by_idx(unsigned int idx, bool f_force)
{
	return fe_voqdrp_dsbl(idx);
} /* fe_voqdrp_del_entry_by_idx */

static int fe_voqdrp_del_entry(void *entry, bool f_force)
{
	fe_pe_voq_drp_entry_t *voqdrp_entry = (fe_pe_voq_drp_entry_t*)entry;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;
	return fe_voqdrp_dsbl(voqdrp_entry->voq_id);
} /* fe_voqdrp_del_entry */

static int fe_voqdrp_get_entry(unsigned int idx, void *entry)
{
	fe_pe_voq_drp_entry_t *voqdrp_entry = (fe_pe_voq_drp_entry_t*)entry;

	if (entry == NULL)
		return FE_TABLE_ENULLPTR;
	voqdrp_entry->voq_id = idx;
	return fe_voqdrp_get(idx, &voqdrp_entry->f_drop_enbl);
} /* fe_voqdrp_get_entry */

static int fe_voqdrp_flush_table(void)
{
	return fe_table_flush_table(&cs_fe_voqdrp_table_type);
} /* fe_voqdrp_flush_table */

static void _fe_voqdrp_print_title(void)
{
	printk("\n\n ------------------- VOQDRP Table -----------------\n");
	printk("|------------------------------------------------------\n");
	printk("| VOQ IDX | DRP_ENBL (HW) |\n");
} /* _fe_voqdrp_print_title */

static void _fe_voqdrp_print_entry(unsigned int idx)
{
	cs_table_entry_t *p_sw_entry;
	unsigned int voq_addr = idx >> 2;
	__u32 voq_data;
	int status;
	__u8 rslt;
	unsigned long flags;

	if (FE_VOQDRP_TABLE_PTR == NULL)
		return;

	/* print SW result */
	p_sw_entry = cs_table_get_entry(FE_VOQDRP_TABLE_PTR, idx >> 5);
	if (p_sw_entry == NULL)
		return;

	spin_lock_irqsave(FE_VOQDRP_LOCK, flags);
	if (p_sw_entry->local_data & BIT_MASK(idx & 0x001f))
		rslt = 1;
	else
		rslt = 0;
	printk("|  %03d  |  %01x  ", idx, rslt);

	/* Print HW result */
	status = cs_fe_hw_table_get_field_value(FE_TABLE_PE_VOQ_DROP, voq_addr,
			FE_PE_VOQ_DRP_DROP_CFG_DATA, &voq_data);
	spin_unlock_irqrestore(FE_VOQDRP_LOCK, flags);
	if (status != FE_TABLE_OK)
		return;

	if (voq_data & BIT_MASK(idx & 0x03))
		rslt = 1;
	else
		rslt = 0;
	printk(" (HW: %01x)  |\n", rslt);
} /* _fe_voqdrp_print_entry */

static void fe_voqdrp_print_entry(unsigned int idx)
{
	if (idx >= cs_fe_voqdrp_table_type.max_entry) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_voqdrp_print_title();
	_fe_voqdrp_print_entry(idx);
	printk("|--------------------------------------------------|\n\n");
} /* fe_voqdrp_print_entry */

static void fe_voqdrp_print_range(unsigned int start_idx, unsigned int end_idx)
{
	unsigned int i;

	if ((start_idx > end_idx) ||
			(end_idx >= cs_fe_voqdrp_table_type.max_entry)) {
		printk("%s::Range not acceptable!\n", __func__);
		return;
	}

	_fe_voqdrp_print_title();
	for (i = start_idx; i <= end_idx; i++) {
		_fe_voqdrp_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_voqdrp_print_range */

static void fe_voqdrp_print_table(void)
{
	unsigned int i;

	_fe_voqdrp_print_title();
	for (i = 0; i < cs_fe_voqdrp_table_type.max_entry; i++) {
		_fe_voqdrp_print_entry(i);
		cond_resched();
	}

	printk("|--------------------------------------------------|\n\n");
} /* fe_voqdrp_print_table */


static cs_fe_table_t cs_fe_voqdrp_table_type = {
	.type_id = FE_TABLE_PE_VOQ_DROP,
	.max_entry = (FE_PE_VOQ_DROP_ENTRY_MAX << 2),
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = sizeof(fe_pe_voq_drp_entry_t),
	.op = {
		.convert_sw_to_hw_data = NULL,
		.alloc_entry = NULL,
		.set_entry = fe_voqdrp_set_entry,
		.add_entry = fe_voqdrp_add_entry,
		.del_entry_by_idx = fe_voqdrp_del_entry_by_idx,
		.del_entry = fe_voqdrp_del_entry,
		.find_entry = NULL,
		.get_entry = fe_voqdrp_get_entry,
		.inc_entry_refcnt = NULL,
		.dec_entry_refcnt = NULL,
		.get_entry_refcnt = NULL,
		.set_field = NULL,
		.get_field = NULL,
		.flush_table = fe_voqdrp_flush_table,
		.get_avail_count = NULL,
		.print_entry = fe_voqdrp_print_entry,
		.print_range = fe_voqdrp_print_range,
		.print_table = fe_voqdrp_print_table,
		.enbl_voqdrp = fe_voqdrp_enbl,
		.dsbl_voqdrp = fe_voqdrp_dsbl,
		.get_voqdrp = fe_voqdrp_get,
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

int cs_fe_ioctl_voqdrp(struct net_device *dev, void *pdata, void *cmd)
{
	fe_pe_voq_drp_entry_t *p_rslt = (fe_pe_voq_drp_entry_t *)pdata;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	unsigned int index;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		status = fe_voqdrp_add_entry(p_rslt, &index);
		break;
	case CMD_DELETE:
		fe_voqdrp_del_entry(p_rslt, false);
		break;
	case CMD_FLUSH:
		fe_voqdrp_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		fe_voqdrp_print_range(fe_cmd_hdr->idx_start,
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
} /* cs_fe_ioctl_voqdrp */

/* this API will initialize voqdrp table */
int cs_fe_table_voqdrp_init(void)
{
	int ret;

	spin_lock_init(FE_VOQDRP_LOCK);

	cs_fe_voqdrp_table_type.content_table = cs_table_alloc(
			cs_fe_voqdrp_table_type.max_entry >> 5);
	if (cs_fe_voqdrp_table_type.content_table == NULL)
		return -ENOMEM;

	ret = cs_fe_table_register(cs_fe_voqdrp_table_type.type_id,
			&cs_fe_voqdrp_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register VOQDRP table!\n", __func__,
				__LINE__);
		cs_table_dealloc(cs_fe_voqdrp_table_type.content_table);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_voqdrp_flush_table();

	return FE_TABLE_OK;
} /* cs_fe_table_voqdrp_init */
EXPORT_SYMBOL(cs_fe_table_voqdrp_init);

void cs_fe_table_voqdrp_exit(void)
{
	fe_voqdrp_flush_table();

	if (cs_fe_voqdrp_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_voqdrp_table_type.content_table);
	cs_fe_table_unregister(cs_fe_voqdrp_table_type.type_id);
} /* cs_fe_table_voqdrp_exit */
EXPORT_SYMBOL(cs_fe_table_voqdrp_exit);

