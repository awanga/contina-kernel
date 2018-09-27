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
#include "cs75xx_tm_tc.h"

cs_status cs_tm_tc_set_intenable_0(cs_tm_tc_int_t *int_cfg)
{
	TM_TC_INTENABLE_0_t tc_int_mask;

	if (int_cfg == NULL)
		return CS_ERROR;

	tc_int_mask.wrd = readl(TM_TC_INTENABLE_0);
	tc_int_mask.bf.BMe = int_cfg->bm_int;
	tc_int_mask.bf.POLe = int_cfg->pol_int;
	tc_int_mask.bf.PMe = int_cfg->pm_int;
	writel(tc_int_mask.wrd, TM_TC_INTENABLE_0);
	return CS_OK;
}

cs_status cs_tm_tc_get_intenable_0(cs_tm_tc_int_t *int_cfg)
{
	TM_TC_INTENABLE_0_t tc_int_mask;

	if (int_cfg == NULL)
		return CS_ERROR;

	tc_int_mask.wrd = readl(TM_TC_INTENABLE_0);
	int_cfg->bm_int = tc_int_mask.bf.BMe;
	int_cfg->pol_int = tc_int_mask.bf.POLe;
	int_cfg->pm_int = tc_int_mask.bf.PMe;
	return CS_OK;
}

cs_status cs_tm_tc_set_interrupt_0(cs_tm_tc_int_t *int_status_ptr)
{
	TM_TC_INTERRUPT_0_t data;

	if (int_status_ptr == NULL)
		return CS_ERROR;

	data.bf.BMi = int_status_ptr->bm_int;
	data.bf.POLi = int_status_ptr->pol_int;
	data.bf.PMi = int_status_ptr->pm_int;
	writel(data.wrd, TM_TC_INTERRUPT_0);
	return CS_OK;
}

cs_status cs_tm_tc_get_interrupt_0(cs_tm_tc_int_t *int_status_ptr)
{
	TM_TC_INTERRUPT_0_t tc_int_status;

	if (int_status_ptr == NULL)
		return CS_ERROR;

	tc_int_status.wrd = readl(TM_TC_INTERRUPT_0);
	int_status_ptr->bm_int = tc_int_status.bf.BMi;
	int_status_ptr->pol_int = tc_int_status.bf.POLi;
	int_status_ptr->pm_int = tc_int_status.bf.PMi;
	return CS_OK;
}

cs_status cs_tm_tc_set_intenable_1(cs_tm_tc_int_t *int_cfg)
{
	TM_TC_INTENABLE_1_t tc_int_mask;

	if (int_cfg == NULL)
		return CS_ERROR;

	tc_int_mask.wrd = readl(TM_TC_INTENABLE_1);
	tc_int_mask.bf.BMe = int_cfg->bm_int;
	tc_int_mask.bf.POLe = int_cfg->pol_int;
	tc_int_mask.bf.PMe = int_cfg->pm_int;
	writel(tc_int_mask.wrd, TM_TC_INTENABLE_1);
	return CS_OK;
}

cs_status cs_tm_tc_get_intenable_1(cs_tm_tc_int_t *int_cfg)
{
	TM_TC_INTENABLE_1_t tc_int_mask;

	if (int_cfg == NULL)
		return CS_ERROR;

	tc_int_mask.wrd = readl(TM_TC_INTENABLE_1);
	int_cfg->bm_int = tc_int_mask.bf.BMe;
	int_cfg->pol_int = tc_int_mask.bf.POLe;
	int_cfg->pm_int = tc_int_mask.bf.PMe;
	return CS_OK;
}

cs_status cs_tm_tc_set_interrupt_1(cs_tm_tc_int_t *int_status_ptr)
{
	TM_TC_INTERRUPT_1_t data;

	if (int_status_ptr == NULL)
		return CS_ERROR;

	data.bf.BMi = int_status_ptr->bm_int;
	data.bf.POLi = int_status_ptr->pol_int;
	data.bf.PMi = int_status_ptr->pm_int;
	writel(data.wrd, TM_TC_INTERRUPT_1);
	return CS_OK;
}

cs_status cs_tm_tc_get_interrupt_1(cs_tm_tc_int_t *int_status_ptr)
{
	TM_TC_INTERRUPT_1_t tc_int_status;

	if (int_status_ptr == NULL)
		return CS_ERROR;

	tc_int_status.wrd = readl(TM_TC_INTERRUPT_1);
	int_status_ptr->bm_int = tc_int_status.bf.BMi;
	int_status_ptr->pol_int = tc_int_status.bf.POLi;
	int_status_ptr->pm_int = tc_int_status.bf.PMi;
	return CS_OK;
}

cs_status cs_tm_tc_set_port_pause_frame(cs_port_id_t port_id,
		cs_tm_tc_param_t *p_param)
{
	TM_TC_PAUSE_FRAME_PORT_t tc_cfg;

	if (p_param == NULL)
		return CS_ERROR;

	tc_cfg.wrd = readl(TM_TC_PAUSE_FRAME_PORT + port_id * 4);
	tc_cfg.bf.logical_spid = port_id;
	tc_cfg.bf.clear_cnt_on_read = p_param->clear_cnt_on_read;
	tc_cfg.bf.pol_enable = p_param->pol_enable;
	tc_cfg.bf.pol_mode = p_param->pol_mode;
	tc_cfg.bf.bm_warning_enable = p_param->bm_warning_enable;
	tc_cfg.bf.bm_dying_gasp_enable = p_param->bm_dying_gasp_enable;
	writel(tc_cfg.wrd, TM_TC_PAUSE_FRAME_PORT + port_id * 4);
	return CS_OK;
}

cs_status cs_tm_tc_get_port_pause_frame(cs_port_id_t port_id,
		cs_tm_tc_status_t *status_ptr)
{
	TM_TC_PAUSE_FRAME_PORT_t tc_status;

	if (status_ptr == NULL)
		return CS_ERROR;

	tc_status.wrd = readl(TM_TC_PAUSE_FRAME_PORT + port_id * 4);
	status_ptr->clear_cnt_on_read = tc_status.bf.clear_cnt_on_read;
	status_ptr->pol_enable = tc_status.bf.pol_enable;
	status_ptr->pol_mode = tc_status.bf.pol_mode;
	status_ptr->pol_status = tc_status.bf.pol_status;
	status_ptr->pol_pause_cnt = tc_status.bf.pol_pause_cnt;
	status_ptr->bm_warning_enable = tc_status.bf.bm_warning_enable;
	status_ptr->bm_dying_gasp_enable = tc_status.bf.bm_dying_gasp_enable;
	status_ptr->bm_status = tc_status.bf.bm_status;
	status_ptr->bm_pause_cnt = tc_status.bf.bm_pause_cnt;
	return CS_OK;
}

/* Initialization */
cs_status cs_tm_tc_init(void)
{
	/* FIXME! anything that needs to be initialized here? */
	return CS_OK;
} /* cs_tm_tc_init */

