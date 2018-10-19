/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
#include <asm/io.h>
#include <mach/cs_types.h>
#include <mach/registers.h>
#include "cs75xx_tm_pm.h"

static int cs_tm_pm_wait_access_done(unsigned int reg)
{
	unsigned int retry = CS_TM_PM_RETRY_TIMES;
	unsigned int access;

	while (retry--) {
		access = readl(reg);
		if ((access & CS_TM_PM_ACCESS_BIT) == 0)
			break;
	}

	if ((access & CS_TM_PM_ACCESS_BIT) == 1)
		return -1;
	return 0;
}

int cs_tm_pm_set_cfg(cs_tm_pm_cfg_t *cfg)
{
	TM_PM_CONFIG_0_t pm_cfg;
	TM_PM_STATUS_0_t status;

	status.wrd = readl(TM_PM_STATUS_0);
	if (status.bf.init_done == 0)
		return -1;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.disable = cfg->disable;
	pm_cfg.bf.bypass_disable_byte_cntrs = cfg->bypass_disable_byte_cntrs;
	pm_cfg.bf.mark_mode = cfg->mark_mode;
	pm_cfg.bf.cnt_mode = cfg->cnt_mode;
	pm_cfg.bf.auto_clear_on_read_mode = cfg->auto_clear_on_read_mode;
	pm_cfg.bf.init = cfg->init;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);
	return 0;
}

int cs_tm_pm_get_cfg(cs_tm_pm_cfg_t *cfg)
{
	TM_PM_CONFIG_0_t pm_cfg;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	cfg->disable = pm_cfg.bf.disable;
	cfg->bypass_disable_byte_cntrs = pm_cfg.bf.bypass_disable_byte_cntrs;
	cfg->mark_mode = pm_cfg.bf.mark_mode;
	cfg->cnt_mode = pm_cfg.bf.cnt_mode;
	cfg->auto_clear_on_read_mode = pm_cfg.bf.auto_clear_on_read_mode;
	cfg->init = pm_cfg.bf.init;
	return 0;
}

int cs_tm_pm_get_cntr_mem_status(unsigned char *err_cntr,
		unsigned short *err_addr)
{
	TM_PM_CNTR_MEM_STATUS_t status;

	status.wrd = readl(TM_PM_CNTR_MEM_STATUS);
	*err_cntr = status.bf.err_cntr;
	*err_addr = status.bf.err_addr;
	return 0;
}

int cs_tm_pm_get_glb_cntr_mem_status(unsigned int *err_addr)
{
	TM_PM_GLB_CNTR_MEM_STATUS_t status;

	status.wrd = readl(TM_PM_GLB_CNTR_MEM_STATUS);
	*err_addr = status.bf.err_addr;
	return 0;
}

int cs_tm_pm_get_interrupt_0( unsigned int *int_status)
{
	*int_status = readl(TM_PM_INTERRUPT_0);
	return 0;
}

int cs_tm_pm_set_interrupt_0(unsigned int int_status)
{
	writel(int_status, TM_PM_INTERRUPT_0);
	return 0;
}

int cs_tm_pm_get_intenable_0( unsigned int *int_enable)
{
	*int_enable = readl(TM_PM_INTENABLE_0);
	return 0;
}

int cs_tm_pm_set_intenable_0( unsigned int int_enable)
{
	writel(int_enable, TM_PM_INTENABLE_0);
	return 0;
}

int cs_tm_pm_get_interrupt_1( unsigned int *int_status)
{
	*int_status = readl(TM_PM_INTERRUPT_1);
	return 0;
}

int cs_tm_pm_set_interrupt_1(unsigned int int_status)
{
	writel(int_status, TM_PM_INTERRUPT_1);
	return 0;
}

int cs_tm_pm_get_intenable_1( unsigned int *int_enable)
{
	*int_enable = readl(TM_PM_INTENABLE_1);
	return 0;
}

int cs_tm_pm_set_intenable_1( unsigned int int_enable)
{
	writel(int_enable, TM_PM_INTENABLE_1);
	return 0;
}

static int cs_tm_pm_get_cntr_mem(unsigned int start_adr, unsigned int size,
		void *counter)
{
	TM_PM_CONFIG_0_t pm_cfg;
	TM_PM_CNTR_MEM_ACCESS_t access;
	TM_PM_CNTR_MEM_DATA6_t data6;
	TM_PM_CNTR_MEM_DATA5_t data5;
	TM_PM_CNTR_MEM_DATA4_t data4;
	TM_PM_CNTR_MEM_DATA3_t data3;
	TM_PM_CNTR_MEM_DATA2_t data2;
	TM_PM_CNTR_MEM_DATA1_t data1;
	TM_PM_CNTR_MEM_DATA0_t data0;
	cs_tm_pm_cntr_t *pm_cntr;
	unsigned int i;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	pm_cntr = (cs_tm_pm_cntr_t *)counter;

	for (i = start_adr; i < (start_adr + size); i++) {
		access.bf.rbw = CS_OP_READ;
		access.bf.addr = i;
		access.bf.access = CS_ENABLE;
		writel(access.wrd, TM_PM_CNTR_MEM_ACCESS);
		if (cs_tm_pm_wait_access_done(TM_PM_CNTR_MEM_ACCESS) == -1)
			return (-1);

		data0.wrd = readl(TM_PM_CNTR_MEM_DATA0);
		data1.wrd = readl(TM_PM_CNTR_MEM_DATA1);
		data2.wrd = readl(TM_PM_CNTR_MEM_DATA2);
		data3.wrd = readl(TM_PM_CNTR_MEM_DATA3);
		data4.wrd = readl(TM_PM_CNTR_MEM_DATA4);
		data5.wrd = readl(TM_PM_CNTR_MEM_DATA5);
		data6.wrd = readl(TM_PM_CNTR_MEM_DATA6);
		pm_cntr->pkts = data0.wrd;
		pm_cntr->pkts_mark = data1.wrd;
		pm_cntr->pkts_drop = data2.wrd;
		pm_cntr->bytes = data4.wrd & 0x0f;
		pm_cntr->bytes = (pm_cntr->bytes << 32) + data3.wrd;
		pm_cntr->bytes_mark = data5.wrd & 0xff;
		pm_cntr->bytes_mark = (pm_cntr->bytes_mark << 28) +
			(data4.wrd >> 4);
		pm_cntr->bytes_drop = data6.wrd;
		pm_cntr->bytes_drop = (pm_cntr->bytes_drop << 24) +
			(data5.wrd >> 8);
		pm_cntr++;
	}

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	return 0;
}

static int cs_tm_pm_reset_cntr_mem(unsigned int start_adr, unsigned int size)
{
	unsigned int i;
	TM_PM_CONFIG_0_t pm_cfg;
	TM_PM_CNTR_MEM_ACCESS_t access;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	for (i = start_adr; i < (start_adr + size); i++) {
		writel(0, TM_PM_CNTR_MEM_DATA0);
		writel(0, TM_PM_CNTR_MEM_DATA1);
		writel(0, TM_PM_CNTR_MEM_DATA2);
		writel(0, TM_PM_CNTR_MEM_DATA3);
		writel(0, TM_PM_CNTR_MEM_DATA4);
		writel(0, TM_PM_CNTR_MEM_DATA5);
		writel(0, TM_PM_CNTR_MEM_DATA6);

		access.bf.rbw = CS_OP_WRITE;
		access.bf.addr = i;
		access.bf.access = CS_ENABLE;
		writel(access.wrd, TM_PM_CNTR_MEM_ACCESS);
		if (cs_tm_pm_wait_access_done(TM_PM_CNTR_MEM_ACCESS) == -1)
			return (-1);
	}

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	return 0;
}

static int cs_tm_pm_get_glb_cntr_mem(cs_tm_pm_glb_blk_t *counter)
{
	TM_PM_CONFIG_0_t pm_cfg;
	TM_PM_GLB_CNTR_MEM_ACCESS_t access;
	TM_PM_GLB_CNTR_MEM_DATA2_t data2;
	TM_PM_GLB_CNTR_MEM_DATA1_t data1;
	TM_PM_GLB_CNTR_MEM_DATA0_t data0;
	unsigned int i;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	for (i = CS_TM_PM_GLB_RX_ALL; i <= CS_TM_PM_GLB_CE; i++) {
		access.bf.rbw = CS_OP_READ;
		access.bf.addr = i;
		access.bf.access = CS_ENABLE;
		writel(access.wrd, TM_PM_GLB_CNTR_MEM_ACCESS);
		if (cs_tm_pm_wait_access_done(TM_PM_GLB_CNTR_MEM_ACCESS) == -1)
			return (-1);

		data0.wrd = readl(TM_PM_GLB_CNTR_MEM_DATA0);
		data1.wrd = readl(TM_PM_GLB_CNTR_MEM_DATA1);
		data2.wrd = readl(TM_PM_GLB_CNTR_MEM_DATA2);
		counter->glb_cntr[i].pkts = data0.wrd;
		counter->glb_cntr[i].bytes = data2.wrd;
		counter->glb_cntr[i].bytes = (counter->glb_cntr[i].bytes << 32)
			+ data1.wrd;
	}

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	return 0;
}

int cs_tm_pm_get_cntr(cs_tm_pm_cntr_id_t cntr_type, unsigned int start_adr,
		unsigned int size, void *counter)
{
	unsigned int start_id = start_adr;

	switch (cntr_type) {
	case CS_TM_PM_FLOW_CNTR:
		start_id += CS_TM_PM_FLOW_0;
		break;
	case CS_TM_PM_SPID_CNTR:
		start_id += CS_TM_PM_SPID_0;
		break;
	case CS_TM_PM_VOQ_CNTR:
		start_id += CS_TM_PM_VOQ_0;
		break;
	case CS_TM_PM_CPU_CNTR:
		start_id += CS_TM_PM_CPU_0;
		break;
	case CS_TM_PM_PKT_TYPE_CNTR:
		start_id += CS_TM_PM_PKT_TYPE_0;
		break;
	default:
		return -1;
	}

	return cs_tm_pm_get_cntr_mem(start_id, size, counter);
} /* cs_tm_pm_get_cnt */

int cs_tm_pm_reset_cntr(cs_tm_pm_cntr_id_t cntr_type, unsigned int start_adr,
		unsigned int size)
{
	unsigned int start_id = start_adr;

	switch (cntr_type) {
	case CS_TM_PM_FLOW_CNTR:
		start_id += CS_TM_PM_FLOW_0;
		break;
	case CS_TM_PM_SPID_CNTR:
		start_id += CS_TM_PM_SPID_0;
		break;
	case CS_TM_PM_VOQ_CNTR:
		start_id += CS_TM_PM_VOQ_0;
		break;
	case CS_TM_PM_CPU_CNTR:
		start_id += CS_TM_PM_CPU_0;
		break;
	case CS_TM_PM_PKT_TYPE_CNTR:
		start_id += CS_TM_PM_PKT_TYPE_0;
		break;
	default:
		return -1;
	}

	return cs_tm_pm_reset_cntr_mem(start_id, size);
} /* cs_tm_pm_reset_cnt */

int cs_tm_pm_get_cntr_by_type(cs_tm_pm_cntr_id_t cntr_type, void *cntr_mem)
{
	cs_tm_pm_glb_blk_t *tm_glb_blk;
	cs_tm_pm_t *tm_pm_blk;

	switch (cntr_type) {
	case CS_TM_PM_FLOW_CNTR:
		cs_tm_pm_get_cntr_mem(CS_TM_PM_FLOW_0, CS_TM_PM_FLOW_SIZE,
				cntr_mem);
		break;

	case CS_TM_PM_SPID_CNTR:
		cs_tm_pm_get_cntr_mem(CS_TM_PM_SPID_0, CS_TM_PM_SPID_SIZE,
				cntr_mem);
		break;

	case CS_TM_PM_VOQ_CNTR:
		cs_tm_pm_get_cntr_mem(CS_TM_PM_VOQ_0, CS_TM_PM_VOQ_SIZE,
				cntr_mem);
		break;

	case CS_TM_PM_CPU_CNTR:
		cs_tm_pm_get_cntr_mem(CS_TM_PM_CPU_0, CS_TM_PM_CPU_SIZE,
				cntr_mem);
		break;

	case CS_TM_PM_PKT_TYPE_CNTR:
		cs_tm_pm_get_cntr_mem(CS_TM_PM_PKT_TYPE_0,
				CS_TM_PM_PKT_TYPE_SIZE, cntr_mem);
		break;

	case CS_TM_PM_CPU_COPY_CNTR:
		cs_tm_pm_get_cntr_mem(CS_TM_PM_CPU_COPY_0,
				CS_TM_PM_CPU_COPY_SIZE, cntr_mem);
		break;

	case CS_TM_PM_GLB_CNTR:
		tm_glb_blk = (cs_tm_pm_glb_blk_t *)cntr_mem;
		cs_tm_pm_get_glb_cntr_mem(tm_glb_blk);
		break;

	case CS_TM_PM_ALL_CNTR:
		tm_pm_blk = (cs_tm_pm_t *)cntr_mem;
		tm_glb_blk = (cs_tm_pm_glb_blk_t *)&tm_pm_blk->tm_glb_blk;
		cs_tm_pm_get_cntr_mem(CS_TM_PM_FLOW_0, CS_TM_PM_FLOW_SIZE,
				&tm_pm_blk->flow_blk);
		cs_tm_pm_get_cntr_mem(CS_TM_PM_SPID_0, CS_TM_PM_SPID_SIZE,
				&tm_pm_blk->spid_blk);
		cs_tm_pm_get_cntr_mem(CS_TM_PM_VOQ_0, CS_TM_PM_VOQ_SIZE,
				&tm_pm_blk->voq_blk);
		cs_tm_pm_get_cntr_mem(CS_TM_PM_CPU_0, CS_TM_PM_CPU_SIZE,
				&tm_pm_blk->cpu_blk);
		cs_tm_pm_get_cntr_mem(CS_TM_PM_PKT_TYPE_0,
				CS_TM_PM_PKT_TYPE_SIZE,
				&tm_pm_blk->pkt_type_blk);
		cs_tm_pm_get_cntr_mem(CS_TM_PM_CPU_COPY_0,
				CS_TM_PM_CPU_COPY_SIZE,
				&tm_pm_blk->cpu_copy_blk);
		cs_tm_pm_get_glb_cntr_mem(tm_glb_blk);
		break;
	}

	return 0;
}

static int cs_tm_pm_get_msb_cntr_mem(unsigned int start_adr, unsigned int size,
		void *counter)
{
	TM_PM_CONFIG_0_t pm_cfg;
	TM_PM_CNTR_MEM_ACCESS_t access;
//	TM_PM_CNTR_MEM_DATA6_t data6;
//	TM_PM_CNTR_MEM_DATA5_t data5;
//	TM_PM_CNTR_MEM_DATA4_t data4;
//	TM_PM_CNTR_MEM_DATA3_t data3;
//	TM_PM_CNTR_MEM_DATA2_t data2;
//	TM_PM_CNTR_MEM_DATA1_t data1;
	TM_PM_CNTR_MEM_DATA0_t data0;
	cs_tm_pm_cntr_msb_t *pm_msb_cntr;
	unsigned int i;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	pm_msb_cntr = (cs_tm_pm_cntr_msb_t *)counter;

	for (i = start_adr; i < (start_adr+size); i++) {
		access.bf.rbw = CS_OP_READ;
		access.bf.addr = i;
		access.bf.access = CS_ENABLE;
		writel(access.wrd, TM_PM_CNTR_MEM_ACCESS);
		if (cs_tm_pm_wait_access_done(TM_PM_CNTR_MEM_ACCESS) == -1)
			return (-1);

		data0.wrd = readl(TM_PM_CNTR_MEM_DATA0);
		pm_msb_cntr->bf.msb_pkt = data0.wrd & 0x0001;
		pm_msb_cntr->bf.msb_pkt_mark = (data0.wrd>>1) & 0x0001;
		pm_msb_cntr->bf.msb_pkt_drop = (data0.wrd>>2) & 0x0001;
		pm_msb_cntr->bf.msb_byte = (data0.wrd>>3) & 0x0001;
		pm_msb_cntr->bf.msb_byte_mark = (data0.wrd>>4) & 0x0001;
		pm_msb_cntr->bf.msb_byte_drop = (data0.wrd>>5) & 0x0001;
		pm_msb_cntr++;
	}

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	return 0;
}

static int cs_tm_pm_get_glb_msb_cntr_mem(cs_tm_pm_glb_msb_blk_t *counter)
{
	TM_PM_CONFIG_0_t pm_cfg;
	TM_PM_GLB_CNTR_MEM_ACCESS_t access;
//	TM_PM_GLB_CNTR_MEM_DATA2_t data2;
//	TM_PM_GLB_CNTR_MEM_DATA1_t data1;
	TM_PM_GLB_CNTR_MEM_DATA0_t data0;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	access.bf.rbw = CS_OP_READ;
	access.bf.addr = CS_TM_PM_GLB_MSB;
	access.bf.access = CS_ENABLE;
	writel(access.wrd, TM_PM_GLB_CNTR_MEM_ACCESS);
	if (cs_tm_pm_wait_access_done(TM_PM_GLB_CNTR_MEM_ACCESS) == -1)
		return (-1);

	data0.wrd = readl(TM_PM_GLB_CNTR_MEM_DATA0);
	counter->wrd = data0.wrd;

	pm_cfg.wrd = readl(TM_PM_CONFIG_0);
	pm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pm_cfg.wrd, TM_PM_CONFIG_0);

	return 0;
}

int cs_tm_pm_get_msb_counter(cs_tm_pm_cntr_id_t cntr_type, void *cntr_mem)
{
	cs_tm_pm_glb_msb_blk_t *tm_glb_msb_blk;
	cs_tm_pm_msb_t *tm_pm_msb_blk;

	switch (cntr_type ) {
	case CS_TM_PM_FLOW_CNTR:
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_FLOW_MSB_0,
				CS_TM_PM_FLOW_SIZE, cntr_mem);
		break;

	case CS_TM_PM_SPID_CNTR:
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_SPID_MSB_0,
				CS_TM_PM_SPID_SIZE, cntr_mem);
		break;

	case CS_TM_PM_VOQ_CNTR:
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_VOQ_MSB_0,
				CS_TM_PM_VOQ_SIZE, cntr_mem);
		break;

	case CS_TM_PM_CPU_CNTR:
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_CPU_MSB_0,
				CS_TM_PM_CPU_SIZE, cntr_mem);
		break;

	case CS_TM_PM_PKT_TYPE_CNTR:
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_PKT_TYPE_MSB_0,
				CS_TM_PM_PKT_TYPE_SIZE, cntr_mem);
		break;

	case CS_TM_PM_CPU_COPY_CNTR:
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_CPU_COPY_MSB_0,
				CS_TM_PM_CPU_COPY_SIZE, cntr_mem);
		break;

	case CS_TM_PM_GLB_CNTR:
		tm_glb_msb_blk = (cs_tm_pm_glb_msb_blk_t *)cntr_mem;
		cs_tm_pm_get_glb_msb_cntr_mem(tm_glb_msb_blk);
		break;

	case CS_TM_PM_ALL_CNTR:
		tm_pm_msb_blk = (cs_tm_pm_msb_t *)cntr_mem;
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_FLOW_MSB_0,
				CS_TM_PM_FLOW_MSB_127,
				&tm_pm_msb_blk->flow_blk);
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_SPID_MSB_0,
				CS_TM_PM_SPID_MSB_7, &tm_pm_msb_blk->spid_blk);
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_VOQ_MSB_0,
				CS_TM_PM_VOQ_MSB_31, &tm_pm_msb_blk->voq_blk);
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_CPU_MSB_0,
				CS_TM_PM_CPU_MSB_9, &tm_pm_msb_blk->cpu_blk);
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_PKT_TYPE_MSB_0,
				CS_TM_PM_PKT_TYPE_MSB_19,
				&tm_pm_msb_blk->pkt_type_blk);
		cs_tm_pm_get_msb_cntr_mem(CS_TM_PM_CPU_COPY_MSB_0,
				CS_TM_PM_CPU_COPY_MSB_63,
				&tm_pm_msb_blk->cpu_copy_blk);
		cs_tm_pm_get_glb_msb_cntr_mem(&tm_pm_msb_blk->tm_glb_blk);
		break;
	}

	return 0;
}

int cs_tm_pm_init(void)
{
	cs_tm_pm_cfg_t pm_cfg;

	cs_tm_pm_get_cfg(&pm_cfg);
	pm_cfg.disable = 0;
	pm_cfg.auto_clear_on_read_mode = CS_TM_PM_READ_MODE_CLEAR_ALL;
	pm_cfg.init = 1;
	cs_tm_pm_set_cfg(&pm_cfg);
	pm_cfg.init = 0;
	cs_tm_pm_set_cfg(&pm_cfg);

	return 0;
}

