/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_hashstatus.c
 *
 * $Id: cs_fe_table_hashstatus.c,v 1.3 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains HASHSTATUS Table Management APIs.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"

static cs_fe_table_t cs_fe_hashstatus_table_type;

#define FE_HASHSTATUS_LOCK	&(cs_fe_hashstatus_table_type.lock)

static int fe_hashstatus_set_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	unsigned long flags;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_hashstatus_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;

	/* WRITE TO HARDWARE */
	spin_lock_irqsave(FE_HASHSTATUS_LOCK, flags);
	status = cs_fe_hw_table_set_field_value(FE_TABLE_HASH_STATUS, idx,
			field, p_value);
	spin_unlock_irqrestore(FE_HASHSTATUS_LOCK, flags);

	return status;
} /* fe_hashstatus_set_field */

static int fe_hashstatus_get_field(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	int status;
	unsigned long flags;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (idx >= cs_fe_hashstatus_table_type.max_entry)
		return FE_TABLE_EOUTRANGE;

	/* Read from Hardware */
	spin_lock_irqsave(FE_HASHSTATUS_LOCK, flags);
	status = cs_fe_hw_table_get_field_value(FE_TABLE_HASH_STATUS, idx,
			field, p_value);
	spin_unlock_irqrestore(FE_HASHSTATUS_LOCK, flags);

	return status;
} /* fe_hashstatus_get_field */

static int fe_hashstatus_flush_table(void)
{
	unsigned long flags;

	spin_lock_irqsave(FE_HASHSTATUS_LOCK, flags);
	cs_fe_hw_table_flush_table(FE_TABLE_HASH_STATUS);
	spin_unlock_irqrestore(FE_HASHSTATUS_LOCK, flags);

	return FE_TABLE_OK;
} /* fe_hashstatus_flush_table */


static cs_fe_table_t cs_fe_hashstatus_table_type = {
	.type_id = FE_TABLE_HASH_STATUS,
	.max_entry = FE_HASH_STATUS_ENTRY_MAX,
	.used_entry = 0,
	.curr_ptr = 0,
	.entry_size = 0,
	.op = {
		.convert_sw_to_hw_data = NULL,
		.alloc_entry = NULL,
		.set_entry = NULL,
		.add_entry = NULL,
		.del_entry_by_idx = NULL,
		.del_entry = NULL,
		.find_entry = NULL,
		.get_entry = NULL,
		.inc_entry_refcnt = NULL,
		.dec_entry_refcnt = NULL,
		.get_entry_refcnt = NULL,
		.set_field = fe_hashstatus_set_field,
		.get_field = fe_hashstatus_get_field,
		.flush_table = fe_hashstatus_flush_table,
		.get_avail_count = NULL,
		.print_entry = NULL,
		.print_range = NULL,
		.print_table = NULL,
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

int cs_fe_ioctl_hashstatus(struct net_device *dev, void *pdata, void *cmd)
{
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	int status = FE_TABLE_OK;

	switch (fe_cmd_hdr->cmd) {
	case CMD_ADD:
		break;
	case CMD_DELETE:
		break;
	case CMD_FLUSH:
		fe_hashstatus_flush_table();
		break;
	case CMD_GET:	/* do it as print */
		break;
	case CMD_REPLACE:	/* ignore */
		break;
	case CMD_INIT:	/* ignore */
		break;
	default:
		return -1;
	}

	return status;
} /* cs_fe_ioctl_hashstatus */

/* this API will initialize hashstatus table */
int cs_fe_table_hashstatus_init(void)
{
	int ret;

	spin_lock_init(FE_HASHSTATUS_LOCK);

	ret = cs_fe_table_register(cs_fe_hashstatus_table_type.type_id,
			&cs_fe_hashstatus_table_type);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d:Unable to register HASHSTATUS table!\n", __func__,
				__LINE__);
		return -1;
	}

	/* to flush the table. make sure the table is clean by initialization */
	fe_hashstatus_flush_table();

	return FE_TABLE_OK;
} /* cs_fe_table_hashstatus_init */
EXPORT_SYMBOL(cs_fe_table_hashstatus_init);

void cs_fe_table_hashstatus_exit(void)
{
	fe_hashstatus_flush_table();

	if (cs_fe_hashstatus_table_type.content_table == NULL)
		return;

	cs_table_dealloc(cs_fe_hashstatus_table_type.content_table);
	cs_fe_table_unregister(cs_fe_hashstatus_table_type.type_id);
} /* cs_fe_table_hashstatus_exit */
EXPORT_SYMBOL(cs_fe_table_hashstatus_exit);

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

int fe_hashstatus_set_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{
	return fe_hashstatus_set_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_hashstatus_set_field_ut);

int fe_hashstatus_get_field_ut(unsigned int idx, unsigned int field,
		__u32 *p_value)
{

	return fe_hashstatus_get_field(idx, field, p_value);
}
EXPORT_SYMBOL(fe_hashstatus_get_field_ut);

int fe_hashstatus_flush_table_ut(void)
{
	return fe_hashstatus_flush_table();
}
EXPORT_SYMBOL(fe_hashstatus_flush_table_ut);

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

