/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2012 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cs_ne_ioctl.h>
#include "cs75xx_tm.h"
#include "cs752x_voq_cntr.h"

#define VOQ_CNTR_STATUS_ERROR	0xff
#define VOQ_CNTR_MASK_ALL	0xffffffff

typedef struct {
	bool enbl;
	u8 voq_cntr_id;
} cs_diag_voq_cntr_entry_t;

typedef struct {
	cs_diag_voq_cntr_entry_t voq_cntr_tbl[CS_MAX_VOQ_NO];
	/* we know CS_TM_PM_VOQ_SIZE == 32, so we only need a u32 mask */
	u32 voq_cntr_mask;
} cs_diag_voq_cntr_db_t;

cs_diag_voq_cntr_db_t voq_cntr_db;

static u8 cs_diag_alloc_voq_cntr(void)
{
	u8 iii;

	if (voq_cntr_db.voq_cntr_mask != VOQ_CNTR_MASK_ALL) {
		for (iii = 0; iii < 32; iii++) {
			if (!(voq_cntr_db.voq_cntr_mask & (0x01 << iii))) {
				voq_cntr_db.voq_cntr_mask |= 0x01 << iii;
				return iii;
			}
		}
	}

	return VOQ_CNTR_STATUS_ERROR;
} /* cs_diag_alloc_voq_cntr */

static void cs_diag_free_voq_cntr(u8 voq_cntr_id)
{
	if (voq_cntr_id >= CS_TM_PM_VOQ_SIZE)
		return;

	voq_cntr_db.voq_cntr_mask &= ~(0x01 << voq_cntr_id);
	return;
} /* cs_diag_free_voq_cntr */

int cs_diag_add_voq_cntr(u8 voq_id)
{
	int status;
	u8 voq_cntr_id;

	if (voq_id >= CS_MAX_VOQ_NO)
		return -1;

	if (voq_cntr_db.voq_cntr_tbl[voq_id].enbl == true)
		return 1;

	voq_cntr_id = cs_diag_alloc_voq_cntr();
	if (voq_cntr_id == VOQ_CNTR_STATUS_ERROR)
		return -1;

	status = cs_tm_bm_set_voq_cntr(voq_id, 1, voq_cntr_id);
	if (status != 0) {
		cs_diag_free_voq_cntr(voq_cntr_id);
		return -1;
	}

	voq_cntr_db.voq_cntr_tbl[voq_id].enbl = true;
	voq_cntr_db.voq_cntr_tbl[voq_id].voq_cntr_id = voq_cntr_id;

	return 0;
} /* cs_diag_add_voq_cntr */

int cs_diag_del_voq_cntr(u8 voq_id)
{
	if (voq_id >= CS_MAX_VOQ_NO)
		return -1;

	if (voq_cntr_db.voq_cntr_tbl[voq_id].enbl == false)
		return 0;

	/* since we are deleting the counter, we might as well clean it up */
	cs_tm_pm_reset_cntr(CS_TM_PM_VOQ_CNTR,
			voq_cntr_db.voq_cntr_tbl[voq_id].voq_cntr_id, 1);

	cs_tm_bm_set_voq_cntr(voq_id, 0, 0);

	cs_diag_free_voq_cntr(voq_cntr_db.voq_cntr_tbl[voq_id].voq_cntr_id);

	voq_cntr_db.voq_cntr_tbl[voq_id].enbl = false;
	voq_cntr_db.voq_cntr_tbl[voq_id].voq_cntr_id = 0;

	return 0;
} /* cs_diag_del_voq_cntr */

int cs_diag_get_voq_cntr(u8 voq_id, cs_tm_pm_cntr_t *p_tm_pm_cntr)
{
	if (voq_id >= CS_MAX_VOQ_NO)
		return -1;

	if (p_tm_pm_cntr == NULL)
		return -1;

	if (voq_cntr_db.voq_cntr_tbl[voq_id].enbl == false)
		return -1;

	return cs_tm_pm_get_cntr(CS_TM_PM_VOQ_CNTR,
			voq_cntr_db.voq_cntr_tbl[voq_id].voq_cntr_id,
			1, p_tm_pm_cntr);
} /* cs_diag_get_voq_cntr */

void cs_diag_print_voq_cntr(u8 voq_id)
{
	cs_tm_pm_cntr_t tm_pm_cntr;
	int status = cs_diag_get_voq_cntr(voq_id, &tm_pm_cntr);

	if (status != 0) {
		printk("voq_id = %d does not have counter\n", voq_id);
		return;
	}

	printk("voq_id = %d:\n\tpkt_cnt = %d, pkt_mark = %d, pkt_drop = %d\n\t"
			"bytes = %lld, bytes_mark = %lld, bytes_drop = %lld\n",
			voq_id, tm_pm_cntr.pkts, tm_pm_cntr.pkts_mark,
			tm_pm_cntr.pkts_drop, tm_pm_cntr.bytes,
			tm_pm_cntr.bytes_mark, tm_pm_cntr.bytes_drop);
	return;
} /* cs_diag_print_voq_cntr */

int cs_diag_voq_cntr_set_read_mode(unsigned char mode)
{
	cs_tm_pm_cfg_t pm_cfg;

	if (mode >= CS_TM_PM_READ_MODE_MAX)
		return -1;

	cs_tm_pm_get_cfg(&pm_cfg);
	if (pm_cfg.auto_clear_on_read_mode == mode)
		return 0;

	pm_cfg.auto_clear_on_read_mode = mode;
	pm_cfg.init = 1;
	cs_tm_pm_set_cfg(&pm_cfg);
	pm_cfg.init = 0;
	cs_tm_pm_set_cfg(&pm_cfg);
	
	return 0;
} /* cs_diag_voq_cntr_set_read_mode */

int cs_diag_voq_cntr_get_read_mode(unsigned char *mode)
{
	cs_tm_pm_cfg_t pm_cfg;

	if (mode == NULL)
		return -1;

	cs_tm_pm_get_cfg(&pm_cfg);
	*mode = pm_cfg.auto_clear_on_read_mode;
	return 0;
} /* cs_diag_voq_cntr_get_read_mode */

void cs_diag_voq_cntr_print_read_mode(void)
{
	cs_tm_pm_cfg_t pm_cfg;

	cs_tm_pm_get_cfg(&pm_cfg);

	printk("Read mode is ");
	switch (pm_cfg.auto_clear_on_read_mode) {
	case CS_TM_PM_READ_MODE_CLEAR_ALL:
		printk("Clear All on read!\n");
		break;
	case CS_TM_PM_READ_MODE_CLEAR_MSB:
		printk("Clear only MSB on read!\n");
		break;
	default:
		printk("No clear on read!\n");
		break;
	};
	return;
} /* cs_diag_voq_cntr_print_read_mode */

int cs_diag_voq_cntr_ioctl(struct net_device *dev, void *pdata, void *cmd)
{
	cs_voq_counter_api_entry_t *entry = (cs_voq_counter_api_entry_t *)pdata;
	cs_tm_pm_cntr_t tm_pm_cntr;

	switch (entry->sub_cmd) {
	case CS_VOQ_COUNTER_ADD:
		entry->ret = cs_diag_add_voq_cntr(entry->param.voq_id);
		break;
	case CS_VOQ_COUNTER_DELETE:
		entry->ret = cs_diag_del_voq_cntr(entry->param.voq_id);
		break;
	case CS_VOQ_COUNTER_GET:
		entry->ret = cs_diag_get_voq_cntr(entry->param.voq_id,
				&tm_pm_cntr);
		entry->param.pkts = tm_pm_cntr.pkts;
		entry->param.pkts_mark = tm_pm_cntr.pkts_mark;
		entry->param.pkts_drop = tm_pm_cntr.pkts_drop;
		entry->param.bytes = tm_pm_cntr.bytes;
		entry->param.bytes_mark = tm_pm_cntr.bytes_mark;
		entry->param.bytes_drop = tm_pm_cntr.bytes_drop;
		break;
	case CS_VOQ_COUNTER_PRINT:
		cs_diag_print_voq_cntr(entry->param.voq_id);
		entry->ret = 0;
		break;
	case CS_VOQ_COUNTER_SET_READ_MODE:
		entry->ret = cs_diag_voq_cntr_set_read_mode(entry->read_mode);
		break;
	case CS_VOQ_COUNTER_GET_READ_MODE:
		entry->ret = cs_diag_voq_cntr_get_read_mode(&entry->read_mode);
		break;
	case CS_VOQ_COUNTER_PRINT_READ_MODE:
		cs_diag_voq_cntr_print_read_mode();
		entry->ret = 0;
		break;
	default:
		entry->ret = -EPERM;
	}

	return 0;
} /* cs_diag_voq_cntr_ioctl */

int cs_diag_voq_cntr_init(void)
{
	memset(&voq_cntr_db, 0x0, sizeof(voq_cntr_db));
	return 0;
} /* cs_diag_voq_cntr_init */

void cs_diag_voq_cntr_exit(void)
{
	/* TODO: Do we want to clean all the VOQ_CNTR? */
} /* cs_diag_voq_cntr_exit */

