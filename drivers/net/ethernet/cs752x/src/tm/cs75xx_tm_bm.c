/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
#include <asm/io.h>
#include <linux/types.h>
#include <mach/cs_types.h>
#include <mach/registers.h>
#include <linux/string.h>
#include <mach/cs75xx_fe_core_table.h>

#include "cs75xx_tm_bm.h"

cs_tm_bm_voq_profile_mem_t voq_profile_tbl[32];
cs_wred_profile_t wred_profile_tbl[32];
bm_voq_info_t voq_qos_tbl[CS_MAX_VOQ_NO];

extern int use_internal_buff;

static int cs_tm_bm_wait_access_done(unsigned int reg)
{
	unsigned int retry = CS_TM_BM_RETRY_TIMES;
	unsigned int access;

	while (retry--) {
		access = readl(reg);
		if ((access & CS_TM_BM_ACCESS_BIT) == 0)
			break;
	}

	if ((access & CS_TM_BM_ACCESS_BIT) == 1)
		return -1;

	return 0;
}

int cs_tm_bm_set_cfg(cs_tm_bm_cfg_t *cfg)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_STATUS_0_t status;

	status.wrd = readl(TM_BM_STATUS_0);
	if (status.bf.init_done == 0)
		return -1;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.disable = cfg->disable;
	bm_cfg.bf.recalculate_global_buffers_used =
		cfg->recalculate_global_buffers_used;
	bm_cfg.bf.bm2_bypass_ca_process = cfg->bm2_bypass_ca_process;
	bm_cfg.bf.bm2_bypass_dest_port_process =
		cfg->bm2_bypass_dest_port_process;
	bm_cfg.bf.bm2_bypass_voq_process = cfg->bm2_bypass_voq_process;
	bm_cfg.bf.bm1_bypass_ca_process = cfg->bm1_bypass_ca_process;
	bm_cfg.bf.bm1_bypass_dest_port_process =
		cfg->bm1_bypass_dest_port_process;
	bm_cfg.bf.bm1_bypass_voq_process = cfg->bm1_bypass_voq_process;
	bm_cfg.bf.enque_hdr_adjust = cfg->enque_hdr_adjust;
	bm_cfg.bf.init = cfg->init;
	writel(bm_cfg.wrd,TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_get_cfg(cs_tm_bm_cfg_t *cfg)
{
	TM_BM_CONFIG_0_t bm_cfg;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	cfg->disable = bm_cfg.bf.disable;
	cfg->recalculate_global_buffers_used =
		bm_cfg.bf.recalculate_global_buffers_used;
	cfg->bm2_bypass_ca_process = bm_cfg.bf.bm2_bypass_ca_process;
	cfg->bm2_bypass_dest_port_process =
		bm_cfg.bf.bm2_bypass_dest_port_process;
	cfg->bm2_bypass_voq_process = bm_cfg.bf.bm2_bypass_voq_process;
	cfg->bm1_bypass_ca_process = bm_cfg.bf.bm1_bypass_ca_process;
	cfg->bm1_bypass_dest_port_process =
		bm_cfg.bf.bm1_bypass_dest_port_process;
	cfg->bm1_bypass_voq_process = bm_cfg.bf.bm1_bypass_voq_process;
	cfg->enque_hdr_adjust = bm_cfg.bf.enque_hdr_adjust;
	cfg->init = bm_cfg.bf.init;

	return 0;
}

int cs_tm_bm_get_recal_glb_buf_active(unsigned char *active)
{
	TM_BM_STATUS_0_t status;

	status.wrd = readl(TM_BM_STATUS_0);
	*active = status.bf.recalculate_global_buffers_used_active;

	return 0;
}

int cs_tm_bm_get_free_buf_info(unsigned int *total_buf, unsigned int *free_buf)
{
	TM_BM_STATUS_1_t status;

	status.wrd = readl(TM_BM_STATUS_1);
	*total_buf = status.bf.buffers;
	*free_buf = status.bf.free_buffers;

	return 0;
}


int cs_tm_bm_get_glb_free_buf_info(unsigned int *total_buf,
		unsigned int *free_buf)
{
	TM_BM_STATUS_2_t status;

	status.wrd = readl(TM_BM_STATUS_2);
	*total_buf = status.bf.global_buffers;
	*free_buf = status.bf.free_global_buffers;

	return 0;
}

int cs_tm_bm_set_reserve_buffers(cs_tm_bm_reserve_buffer_t *rsv_buf)
{
	TM_BM_RESERVE_BUFFERS_t bufs;
	TM_BM_RESERVE_CPU_BUFFERS_t cpu_bufs;
	TM_BM_RESERVE_LINUX_BUFFERS_0_t linux_bufs_0;
	TM_BM_RESERVE_LINUX_BUFFERS_1_t linux_bufs_1;
	TM_BM_RESERVE_LINUX_BUFFERS_2_t linux_bufs_2;
	TM_BM_RESERVE_LINUX_BUFFERS_3_t linux_bufs_3;

	bufs.bf.reserve_buffers = rsv_buf->reserve_buffers;
	bufs.bf.reserve_buffers_inflight_pkts =
		rsv_buf->reserve_buffers_inflight_pkts;
	cpu_bufs.bf.reserve_cpu_buffers = rsv_buf->reserve_cpu_buffers;
	linux_bufs_0.bf.reserve_linux0_buffers =
		rsv_buf->reserve_linux0_buffers;
	linux_bufs_0.bf.reserve_linux1_buffers =
		rsv_buf->reserve_linux1_buffers;
	linux_bufs_1.bf.reserve_linux2_buffers =
		rsv_buf->reserve_linux2_buffers;
	linux_bufs_1.bf.reserve_linux3_buffers =
		rsv_buf->reserve_linux3_buffers;
	linux_bufs_2.bf.reserve_linux4_buffers =
		rsv_buf->reserve_linux4_buffers;
	linux_bufs_2.bf.reserve_linux5_buffers =
		rsv_buf->reserve_linux5_buffers;
	linux_bufs_3.bf.reserve_linux6_buffers =
		rsv_buf->reserve_linux6_buffers;
	linux_bufs_3.bf.reserve_linux7_buffers =
		rsv_buf->reserve_linux7_buffers;

	writel(bufs.wrd,TM_BM_RESERVE_BUFFERS);
	writel(cpu_bufs.wrd,TM_BM_RESERVE_CPU_BUFFERS);
	writel(linux_bufs_0.wrd,TM_BM_RESERVE_LINUX_BUFFERS_0);
	writel(linux_bufs_1.wrd,TM_BM_RESERVE_LINUX_BUFFERS_1);
	writel(linux_bufs_2.wrd,TM_BM_RESERVE_LINUX_BUFFERS_2);
	writel(linux_bufs_3.wrd,TM_BM_RESERVE_LINUX_BUFFERS_3);
	return 0;
}

int cs_tm_bm_get_reserve_buffers(cs_tm_bm_reserve_buffer_t *rsv_buf)
{
	TM_BM_RESERVE_BUFFERS_t bufs;
	TM_BM_RESERVE_CPU_BUFFERS_t cpu_bufs;
	TM_BM_RESERVE_LINUX_BUFFERS_0_t linux_bufs_0;
	TM_BM_RESERVE_LINUX_BUFFERS_1_t linux_bufs_1;
	TM_BM_RESERVE_LINUX_BUFFERS_2_t linux_bufs_2;
	TM_BM_RESERVE_LINUX_BUFFERS_3_t linux_bufs_3;

	bufs.wrd = readl(TM_BM_RESERVE_BUFFERS);
	cpu_bufs.wrd = readl(TM_BM_RESERVE_CPU_BUFFERS);
	linux_bufs_0.wrd = readl(TM_BM_RESERVE_LINUX_BUFFERS_0);
	linux_bufs_1.wrd = readl(TM_BM_RESERVE_LINUX_BUFFERS_1);
	linux_bufs_2.wrd = readl(TM_BM_RESERVE_LINUX_BUFFERS_2);
	linux_bufs_3.wrd = readl(TM_BM_RESERVE_LINUX_BUFFERS_3);

	rsv_buf->reserve_buffers = bufs.bf.reserve_buffers;
	rsv_buf->reserve_buffers_inflight_pkts =
		bufs.bf.reserve_buffers_inflight_pkts;
	rsv_buf->reserve_cpu_buffers = cpu_bufs.bf.reserve_cpu_buffers;
	rsv_buf->reserve_linux0_buffers =
		linux_bufs_0.bf.reserve_linux0_buffers;
	rsv_buf->reserve_linux1_buffers =
		linux_bufs_0.bf.reserve_linux1_buffers;
	rsv_buf->reserve_linux2_buffers =
		linux_bufs_1.bf.reserve_linux2_buffers;
	rsv_buf->reserve_linux3_buffers =
		linux_bufs_1.bf.reserve_linux3_buffers;
	rsv_buf->reserve_linux4_buffers =
		linux_bufs_2.bf.reserve_linux4_buffers;
	rsv_buf->reserve_linux5_buffers =
		linux_bufs_2.bf.reserve_linux5_buffers;
	rsv_buf->reserve_linux6_buffers =
		linux_bufs_3.bf.reserve_linux6_buffers;
	rsv_buf->reserve_linux7_buffers =
		linux_bufs_3.bf.reserve_linux7_buffers;
	return 0;
}

int cs_tm_bm_set_wred_cfg(cs_tm_bm_wred_cfg_t *cfg)
{
	TM_BM_WRED_CONFIG_0_t wred_cfg_0;
	TM_BM_WRED_CONFIG_1_t wred_cfg_1;

	wred_cfg_0.bf.wred_ad_update_period = cfg->wred_ad_update_period;
	wred_cfg_1.bf.wred_buffer_alloc_only = cfg->wred_buffer_alloc_only;
	wred_cfg_1.bf.wred_agbd_weight = cfg->wred_agbd_weight;
	wred_cfg_1.bf.wred_drop_mark_mode = cfg->wred_drop_mark_mode;
	wred_cfg_1.bf.wred_profile_select_mode = cfg->wred_profile_select_mode;
	wred_cfg_1.bf.wred_ad_update_mode = cfg->wred_ad_update_mode;
	wred_cfg_1.bf.wred_mode = cfg->wred_mode;
	wred_cfg_1.bf.wred_adjust_range_index = cfg->wred_adjust_range_index;

	writel(wred_cfg_0.wrd, TM_BM_WRED_CONFIG_0);
	writel(wred_cfg_1.wrd, TM_BM_WRED_CONFIG_1);
	return 0;
}

int cs_tm_bm_get_wred_cfg(cs_tm_bm_wred_cfg_t *cfg)
{
	TM_BM_WRED_CONFIG_0_t wred_cfg_0;
	TM_BM_WRED_CONFIG_1_t wred_cfg_1;

	wred_cfg_0.wrd = readl(TM_BM_WRED_CONFIG_0);
	wred_cfg_1.wrd = readl(TM_BM_WRED_CONFIG_1);

	cfg->wred_ad_update_period = wred_cfg_0.bf.wred_ad_update_period;
	cfg->wred_buffer_alloc_only = wred_cfg_1.bf.wred_buffer_alloc_only;
	cfg->wred_agbd_weight = wred_cfg_1.bf.wred_agbd_weight;
	cfg->wred_drop_mark_mode = wred_cfg_1.bf.wred_drop_mark_mode;
	cfg->wred_profile_select_mode = wred_cfg_1.bf.wred_profile_select_mode;
	cfg->wred_ad_update_mode = wred_cfg_1.bf.wred_ad_update_mode;
	cfg->wred_mode = wred_cfg_1.bf.wred_mode;
	cfg->wred_adjust_range_index = wred_cfg_1.bf.wred_adjust_range_index;
	return 0;
}

int cs_tm_bm_set_copy_wred_profile(cs_tm_bm_wred_profile_t *profile)
{
	TM_BM_COPY_WRED_PROFILE_t wred_profile;

	wred_profile.bf.wred_aqd_min_pct_baseline =
		profile->wred_aqd_min_pct_baseline;
	wred_profile.bf.wred_aqd_max_pct_baseline =
		profile->wred_aqd_max_pct_baseline;
	wred_profile.bf.wred_aqd_min_pct_buffers0 =
		profile->wred_aqd_min_pct_buffers0;
	wred_profile.bf.wred_aqd_max_pct_buffers0 =
		profile->wred_aqd_max_pct_buffers0;
	wred_profile.bf.wred_aqd_max_drop_probability =
		profile->wred_aqd_max_drop_probability;
	writel(wred_profile.wrd, TM_BM_COPY_WRED_PROFILE);
	return 0;
}

int cs_tm_bm_get_copy_wred_profile(cs_tm_bm_wred_profile_t *profile)
{
	TM_BM_COPY_WRED_PROFILE_t wred_profile;

	wred_profile.wrd = readl(TM_BM_COPY_WRED_PROFILE);
	profile->wred_aqd_min_pct_baseline =
		wred_profile.bf.wred_aqd_min_pct_baseline;
	profile->wred_aqd_max_pct_baseline =
		wred_profile.bf.wred_aqd_max_pct_baseline;
	profile->wred_aqd_min_pct_buffers0 =
		wred_profile.bf.wred_aqd_min_pct_buffers0;
	profile->wred_aqd_max_pct_buffers0 =
		wred_profile.bf.wred_aqd_max_pct_buffers0;
	profile->wred_aqd_max_drop_probability =
		wred_profile.bf.wred_aqd_max_drop_probability;
	return 0;
}

int cs_tm_bm_set_copy_dest_port(unsigned char port)
{
	TM_BM_COPY_DEST_PORT_t dest_port;

	dest_port.bf.dest_port_addr = port;
	writel(dest_port.wrd, TM_BM_COPY_DEST_PORT);
	return 0;
}

int cs_tm_bm_get_copy_dest_port(unsigned char *port)
{
	TM_BM_COPY_DEST_PORT_t dest_port;

	dest_port.wrd = readl(TM_BM_COPY_DEST_PORT);
	*port = dest_port.bf.dest_port_addr;
	return 0;
}

int cs_tm_bm_set_traffic_control(cs_tm_bm_traffic_control_t *tc_cfg)
{
	TM_BM_TRAFFIC_CONTROL_0_t tc0;
	TM_BM_TRAFFIC_CONTROL_1_t tc1;

	tc0.bf.warning_threshold = tc_cfg->warning_threshold;
	tc1.bf.dying_gasp_threshold = tc_cfg->dying_gasp_threshold;
	tc1.bf.living_threshold = tc_cfg->living_threshold;
	writel(tc0.wrd, TM_BM_TRAFFIC_CONTROL_0);
	writel(tc1.wrd, TM_BM_TRAFFIC_CONTROL_1);
	return 0;
}

int cs_tm_bm_get_traffic_control(cs_tm_bm_traffic_control_t *tc_cfg)
{
	TM_BM_TRAFFIC_CONTROL_0_t tc0;
	TM_BM_TRAFFIC_CONTROL_1_t tc1;

	tc0.wrd = readl(TM_BM_TRAFFIC_CONTROL_0);
	tc1.wrd = readl(TM_BM_TRAFFIC_CONTROL_1);
	tc_cfg->warning_threshold = tc0.bf.warning_threshold;
	tc_cfg->dying_gasp_threshold= tc1.bf.dying_gasp_threshold;
	tc_cfg->living_threshold = tc1.bf.living_threshold;
	return 0;
}

int cs_tm_bm_set_traffic_control_int(cs_tm_bm_traffic_control_t *tc_cfg)
{
	TM_BM_TRAFFIC_CONTROL_2_t tc2;
	TM_BM_TRAFFIC_CONTROL_3_t tc3;

	tc2.bf.warning_threshold_int = tc_cfg->warning_threshold;
	tc3.bf.dying_gasp_threshold_int = tc_cfg->dying_gasp_threshold;
	tc3.bf.living_threshold_int = tc_cfg->living_threshold;
	writel(tc2.wrd, TM_BM_TRAFFIC_CONTROL_2);
	writel(tc3.wrd, TM_BM_TRAFFIC_CONTROL_3);
	return 0;
}

int cs_tm_bm_get_traffic_control_int(cs_tm_bm_traffic_control_t *tc_cfg)
{
	TM_BM_TRAFFIC_CONTROL_2_t tc2;
	TM_BM_TRAFFIC_CONTROL_3_t tc3;

	tc2.wrd = readl(TM_BM_TRAFFIC_CONTROL_2);
	tc3.wrd = readl(TM_BM_TRAFFIC_CONTROL_3);
	tc_cfg->warning_threshold = tc2.bf.warning_threshold_int;
	tc_cfg->dying_gasp_threshold= tc3.bf.dying_gasp_threshold_int;
	tc_cfg->living_threshold = tc3.bf.living_threshold_int;
	return 0;
}

int cs_tm_bm_get_mem_status(cs_tm_bm_mem_type_t mem_type,
		unsigned char *err_addr, unsigned char *err_correct_addr)
{
	TM_BM_VOQ_MEM_STATUS_t voq_mem;
	TM_BM_VOQ_STATUS_MEM_STATUS_t voq_status_mem;
	TM_BM_DEST_PORT_MEM_STATUS_t dest_port_mem;
	TM_BM_DEST_PORT_STATUS_MEM_STATUS_t dest_port_status_mem;
	TM_BM_VOQ_PROFILE_MEM_STATUS_t voq_profile_mem;
	TM_BM_WRED_PROFILE_MEM_STATUS_t wred_profile_mem;

	switch (mem_type) {
	case CS_TM_BM_VOQ_MEM:
		voq_mem.wrd = readl(TM_BM_VOQ_MEM_STATUS);
		*err_addr = voq_mem.bf.err_addr;
		*err_correct_addr = voq_mem.bf.err_correct_addr;
		break;

	case CS_TM_BM_VOQ_STATUS_MEM:
		voq_status_mem.wrd = readl(TM_BM_VOQ_STATUS_MEM_STATUS);
		*err_addr = voq_status_mem.bf.err_addr;
		*err_correct_addr = voq_status_mem.bf.err_correct_addr;
		break;

	case CS_TM_BM_DEST_PORT_MEM:
		dest_port_mem.wrd = readl(TM_BM_DEST_PORT_MEM_STATUS);
		*err_addr = dest_port_mem.bf.err_addr;
		*err_correct_addr = dest_port_mem.bf.err_correct_addr;
		break;

	case CS_TM_BM_DEST_PORT_STATUS_MEM:
		dest_port_status_mem.wrd =
			readl(TM_BM_DEST_PORT_STATUS_MEM_STATUS);
		*err_addr = dest_port_status_mem.bf.err_addr;
		*err_correct_addr = dest_port_status_mem.bf.err_correct_addr;
		break;

	case CS_TM_BM_VOQ_PROFILE_MEM:
		voq_profile_mem.wrd = readl(TM_BM_VOQ_PROFILE_MEM_STATUS);
		*err_addr = voq_profile_mem.bf.err_addr;
		*err_correct_addr = voq_profile_mem.bf.err_correct_addr;
		break;

	case CS_TM_BM_WRED_PROFILE_MEM:
		wred_profile_mem.wrd = readl(TM_BM_WRED_PROFILE_MEM_STATUS);
		*err_addr = wred_profile_mem.bf.err_addr;
		*err_correct_addr = wred_profile_mem.bf.err_correct_addr;
		break;
	}
	return 0;
}

int cs_tm_bm_get_interrupt_0( unsigned int *int_status)
{
	*int_status = readl(TM_BM_INTERRUPT_0);
	return 0;
}

int cs_tm_bm_set_interrupt_0(unsigned int int_status)
{
	writel(int_status, TM_BM_INTERRUPT_0);
	return 0;
}

int cs_tm_bm_get_intenable_0( unsigned int *int_enable)
{
	*int_enable = readl(TM_BM_INTENABLE_0);
	return 0;
}

int cs_tm_bm_set_intenable_0( unsigned int int_enable)
{
	writel(int_enable, TM_BM_INTENABLE_0);
	return 0;
}

int cs_tm_bm_get_interrupt_1( unsigned int *int_status)
{
	*int_status = readl(TM_BM_INTERRUPT_1);
	return 0;
}

int cs_tm_bm_set_interrupt_1(unsigned int int_status)
{
	writel(int_status, TM_BM_INTERRUPT_1);
	return 0;
}

int cs_tm_bm_get_intenable_1( unsigned int *int_enable)
{
	*int_enable = readl(TM_BM_INTENABLE_1);
	return 0;
}

int cs_tm_bm_set_intenable_1( unsigned int int_enable)
{
	writel(int_enable, TM_BM_INTENABLE_1);
	return 0;
}

int cs_tm_bm_get_voq_mem(unsigned char addr, cs_tm_bm_voq_mem_t *voq)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_VOQ_MEM_ACCESS_t voq_access;
	TM_BM_VOQ_MEM_DATA_t voq_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	voq_access.bf.addr = addr;
	voq_access.bf.access= CS_ENABLE;
	voq_access.bf.rbw = CS_OP_READ;

	writel(voq_access.wrd, TM_BM_VOQ_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_VOQ_MEM_ACCESS) == -1)
		return (-1);


	voq_data.wrd = readl(TM_BM_VOQ_MEM_DATA);
	voq->voq_profile = voq_data.bf.voq_profile;
	voq->wred_profile = voq_data.bf.wred_profile;
	voq->dest_port = voq_data.bf.dest_port;
	voq->voq_cntr_enable= voq_data.bf.voq_cntr_enable;
	voq->voq_cntr = voq_data.bf.voq_cntr;
	voq->wred_enable = voq_data.bf.wred_enable;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_set_voq_mem(unsigned char addr, cs_tm_bm_voq_mem_t *voq)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_VOQ_MEM_ACCESS_t voq_access;
	TM_BM_VOQ_MEM_DATA_t voq_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	voq_access.bf.addr = addr;
	voq_access.bf.access= CS_ENABLE;
	voq_access.bf.rbw = CS_OP_WRITE;

	voq_data.bf.voq_profile = voq->voq_profile;
	voq_data.bf.wred_profile = voq->wred_profile;
	voq_data.bf.dest_port = voq->dest_port;
	voq_data.bf.voq_cntr_enable = voq->voq_cntr_enable;
	voq_data.bf.voq_cntr = voq->voq_cntr;
	voq_data.bf.wred_enable = voq->wred_enable;
	writel(voq_data.wrd, TM_BM_VOQ_MEM_DATA);

	writel(voq_access.wrd, TM_BM_VOQ_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_VOQ_MEM_ACCESS) == -1)
		return (-1);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_get_voq_status_mem(unsigned char addr,
		cs_tm_bm_voq_status_mem_t *voq_status)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_VOQ_STATUS_MEM_ACCESS_t voq_access;
	TM_BM_VOQ_STATUS_MEM_DATA0_t voq_data0;
	TM_BM_VOQ_STATUS_MEM_DATA1_t voq_data1;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	voq_access.bf.addr = addr;
	voq_access.bf.access= CS_ENABLE;
	voq_access.bf.rbw = CS_OP_READ;

	writel(voq_access.wrd, TM_BM_VOQ_STATUS_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_VOQ_STATUS_MEM_ACCESS) == -1)
		return (-1);

	voq_data0.wrd = readl(TM_BM_VOQ_STATUS_MEM_DATA0);
	voq_data1.wrd = readl(TM_BM_VOQ_STATUS_MEM_DATA1);

	voq_status->voq_write_ptr = voq_data0.bf.voq_write_ptr;
	voq_status->voq_depth = voq_data0.bf.voq_depth;
	voq_status->voq_aqd = voq_data0.bf.voq_aqd +
		(voq_data1.bf.voq_aqd << 2);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_set_voq_status_mem(unsigned char addr,
		cs_tm_bm_voq_status_mem_t *voq_status)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_VOQ_STATUS_MEM_ACCESS_t voq_access;
	TM_BM_VOQ_STATUS_MEM_DATA0_t voq_data0;
	TM_BM_VOQ_STATUS_MEM_DATA1_t voq_data1;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	voq_access.bf.addr = addr;
	voq_access.bf.access= CS_ENABLE;
	voq_access.bf.rbw = CS_OP_WRITE;

	voq_data0.bf.voq_write_ptr = voq_status->voq_write_ptr;
	voq_data0.bf.voq_depth = voq_status->voq_depth;
	voq_data0.bf.voq_aqd = voq_status->voq_aqd & 0x0003;
	voq_data1.bf.voq_aqd = voq_status->voq_aqd >> 2;
	writel(voq_data0.wrd, TM_BM_VOQ_STATUS_MEM_DATA0);
	writel(voq_data1.wrd, TM_BM_VOQ_STATUS_MEM_DATA1);

	writel(voq_access.wrd, TM_BM_VOQ_STATUS_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_VOQ_STATUS_MEM_ACCESS) == -1)
		return (-1);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_get_dest_port_mem(unsigned char addr,
		cs_tm_bm_dest_port_mem_t *dest_port)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_DEST_PORT_MEM_ACCESS_t dest_port_access;
	TM_BM_DEST_PORT_MEM_DATA_t dest_port_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	dest_port_access.bf.addr = addr;
	dest_port_access.bf.access = CS_ENABLE;
	dest_port_access.bf.rbw = CS_OP_READ;

	writel(dest_port_access.wrd, TM_BM_DEST_PORT_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_DEST_PORT_MEM_ACCESS) == -1)
		return (-1);

	dest_port_data.wrd = readl(TM_BM_DEST_PORT_MEM_DATA);

	dest_port->dest_port_min_global_buffers =
		dest_port_data.bf.dest_port_min_global_buffers;

	dest_port->dest_port_max_global_buffers =
		dest_port_data.bf.dest_port_max_global_buffers;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_set_dest_port_mem(unsigned char addr,
		cs_tm_bm_dest_port_mem_t *dest_port)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_DEST_PORT_MEM_ACCESS_t dest_port_access;
	TM_BM_DEST_PORT_MEM_DATA_t dest_port_data;
	int ret = 0;

	/* we don't allow CPU_PORT's port parameter changed */
	if (addr == CPU_PORT)
		return 0;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	dest_port_access.bf.addr = addr;
	dest_port_access.bf.access = CS_ENABLE;
	dest_port_access.bf.rbw = CS_OP_WRITE;

	dest_port_data.bf.dest_port_min_global_buffers =
		dest_port->dest_port_min_global_buffers;

	dest_port_data.bf.dest_port_max_global_buffers =
		dest_port->dest_port_max_global_buffers;

	writel(dest_port_data.wrd, TM_BM_DEST_PORT_MEM_DATA);

	writel(dest_port_access.wrd, TM_BM_DEST_PORT_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_DEST_PORT_MEM_ACCESS) == -1)
		ret = (-1);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return ret;
}

int cs_tm_bm_get_dest_port_status_mem(unsigned char addr,
		unsigned short *dest_port_global_buffers)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_DEST_PORT_STATUS_MEM_ACCESS_t dest_port_access;
	TM_BM_DEST_PORT_STATUS_MEM_DATA_t dest_port_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	dest_port_access.bf.addr = addr;
	dest_port_access.bf.access = CS_ENABLE;
	dest_port_access.bf.rbw = CS_OP_READ;

	writel(dest_port_access.wrd, TM_BM_DEST_PORT_STATUS_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_DEST_PORT_STATUS_MEM_ACCESS) == -1)
		return (-1);

	dest_port_data.wrd = readl(TM_BM_DEST_PORT_STATUS_MEM_DATA);

	*dest_port_global_buffers = dest_port_data.wrd;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_set_dest_port_status_mem(unsigned char addr,
		unsigned short dest_port_global_buffers)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_DEST_PORT_STATUS_MEM_ACCESS_t dest_port_access;
	TM_BM_DEST_PORT_STATUS_MEM_DATA_t dest_port_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	dest_port_access.bf.addr = addr;
	dest_port_access.bf.access = CS_ENABLE;
	dest_port_access.bf.rbw = CS_OP_WRITE;

	dest_port_data.wrd = dest_port_global_buffers;
	writel(dest_port_data.wrd, TM_BM_DEST_PORT_STATUS_MEM_DATA);

	writel(dest_port_access.wrd, TM_BM_DEST_PORT_STATUS_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_DEST_PORT_STATUS_MEM_ACCESS) == -1)
		return (-1);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_get_voq_profile_mem(unsigned char addr,
		cs_tm_bm_voq_profile_mem_t *voq_profile)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_VOQ_PROFILE_MEM_ACCESS_t voq_profile_access;
	TM_BM_VOQ_PROFILE_MEM_DATA0_t voq_profile_data0;
	TM_BM_VOQ_PROFILE_MEM_DATA1_t voq_profile_data1;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	voq_profile_access.bf.addr = addr;
	voq_profile_access.bf.access= CS_ENABLE;
	voq_profile_access.bf.rbw = CS_OP_READ;

	writel(voq_profile_access.wrd, TM_BM_VOQ_PROFILE_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_VOQ_PROFILE_MEM_ACCESS) == -1)
		return (-1);

	voq_profile_data0.wrd = readl(TM_BM_VOQ_PROFILE_MEM_DATA0);
	voq_profile_data1.wrd = readl(TM_BM_VOQ_PROFILE_MEM_DATA1);
	voq_profile->voq_min_depth = voq_profile_data0.bf.voq_min_depth;
	voq_profile->voq_max_depth = voq_profile_data0.bf.voq_max_depth;
	voq_profile->wred_adjust_range_index =
		voq_profile_data1.bf.wred_adjust_range_index;
	voq_profile->wred_aqd_weight = voq_profile_data1.bf.wred_aqd_weight;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_set_voq_profile_mem(unsigned char addr,
		cs_tm_bm_voq_profile_mem_t *voq_profile)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_VOQ_PROFILE_MEM_ACCESS_t voq_profile_access;
	TM_BM_VOQ_PROFILE_MEM_DATA0_t voq_profile_data0;
	TM_BM_VOQ_PROFILE_MEM_DATA1_t voq_profile_data1;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	voq_profile_access.bf.addr = addr;
	voq_profile_access.bf.access= CS_ENABLE;
	voq_profile_access.bf.rbw = CS_OP_WRITE;

	voq_profile_data0.bf.voq_min_depth = voq_profile->voq_min_depth;
	voq_profile_data0.bf.voq_max_depth = voq_profile->voq_max_depth;
	voq_profile_data1.bf.wred_adjust_range_index =
		voq_profile->wred_adjust_range_index;
	voq_profile_data1.bf.wred_aqd_weight = voq_profile->wred_aqd_weight;

	writel(voq_profile_data0.wrd, TM_BM_VOQ_PROFILE_MEM_DATA0);
	writel(voq_profile_data1.wrd, TM_BM_VOQ_PROFILE_MEM_DATA1);

	writel(voq_profile_access.wrd, TM_BM_VOQ_PROFILE_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_VOQ_PROFILE_MEM_ACCESS) == -1)
		return (-1);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
}

int cs_tm_bm_get_wred_profile_mem(unsigned char addr,
		cs_tm_bm_wred_profile_t *wred_profile)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_WRED_PROFILE_MEM_ACCESS_t wred_profile_access;
	TM_BM_WRED_PROFILE_MEM_DATA_t wred_profile_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	wred_profile_access.bf.addr = addr;
	wred_profile_access.bf.access = CS_ENABLE;
	wred_profile_access.bf.rbw = CS_OP_READ;
	writel(wred_profile_access.wrd, TM_BM_WRED_PROFILE_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_WRED_PROFILE_MEM_ACCESS) == -1)
		return (-1);

	wred_profile_data.wrd = readl(TM_BM_WRED_PROFILE_MEM_DATA);
	wred_profile->wred_aqd_min_pct_baseline =
		wred_profile_data.bf.wred_aqd_min_pct_baseline;
	wred_profile->wred_aqd_max_pct_baseline =
		wred_profile_data.bf.wred_aqd_max_pct_baseline;
	wred_profile->wred_aqd_min_pct_buffers0 =
		wred_profile_data.bf.wred_aqd_min_pct_buffers0;
	wred_profile->wred_aqd_max_pct_buffers0 =
		wred_profile_data.bf.wred_aqd_max_pct_buffers0;
	wred_profile->wred_aqd_max_drop_probability =
		wred_profile_data.bf.wred_aqd_max_drop_probability;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
} /* cs_tm_bm_get_wred_profile_mem */

int cs_tm_bm_set_wred_profile_mem(unsigned char addr,
		cs_tm_bm_wred_profile_t *wred_profile)
{
	TM_BM_CONFIG_0_t bm_cfg;
	TM_BM_WRED_PROFILE_MEM_ACCESS_t wred_profile_access;
	TM_BM_WRED_PROFILE_MEM_DATA_t wred_profile_data;

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	wred_profile_access.bf.addr = addr;
	wred_profile_access.bf.access = CS_ENABLE;
	wred_profile_access.bf.rbw = CS_OP_WRITE;

	wred_profile_data.bf.wred_aqd_min_pct_baseline =
		wred_profile->wred_aqd_min_pct_baseline;
	wred_profile_data.bf.wred_aqd_max_pct_baseline =
		wred_profile->wred_aqd_max_pct_baseline;
	wred_profile_data.bf.wred_aqd_min_pct_buffers0 =
		wred_profile->wred_aqd_min_pct_buffers0;
	wred_profile_data.bf.wred_aqd_max_pct_buffers0 =
		wred_profile->wred_aqd_max_pct_buffers0;
	wred_profile_data.bf.wred_aqd_max_drop_probability =
		wred_profile->wred_aqd_max_drop_probability;

	writel(wred_profile_data.wrd, TM_BM_WRED_PROFILE_MEM_DATA);

	writel(wred_profile_access.wrd, TM_BM_WRED_PROFILE_MEM_ACCESS);
	if (cs_tm_bm_wait_access_done(TM_BM_WRED_PROFILE_MEM_ACCESS) == -1)
		return (-1);

	bm_cfg.wrd = readl(TM_BM_CONFIG_0);
	bm_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(bm_cfg.wrd, TM_BM_CONFIG_0);

	return 0;
} /* cs_tm_bm_set_wred_profile_mem */

int cs_tm_bm_init(void)
{
	unsigned char i;
	cs_tm_bm_cfg_t bm_config;
	cs_tm_bm_voq_mem_t voq_mem;
	cs_tm_bm_traffic_control_t traff_ctrl;
	cs_tm_bm_wred_cfg_t wred_cfg;
	cs_tm_bm_voq_profile_mem_t voq_prof_mem;

	/* Init BM */
	/* config wred config */
	cs_tm_bm_get_wred_cfg(&wred_cfg);
	/* using dirct mapping voq to wred profile */
	wred_cfg.wred_profile_select_mode = 2;
	cs_tm_bm_set_wred_cfg(&wred_cfg);

	/* the main BM cfg */
	memset(&bm_config, 0, sizeof(cs_tm_bm_cfg_t));
	bm_config.bm1_bypass_ca_process = 1;
	bm_config.bm2_bypass_dest_port_process = 1;
	bm_config.bm2_bypass_voq_process = 1;
	bm_config.init = 1;
	cs_tm_bm_set_cfg(&bm_config);

	bm_config.bm1_bypass_ca_process = 0;
	bm_config.bm2_bypass_dest_port_process = 0;
	bm_config.bm2_bypass_voq_process = 0;
	bm_config.init = 0;
	cs_tm_bm_set_cfg(&bm_config);

	/* config dying gasp */
	memset(&traff_ctrl, 0, sizeof(cs_tm_bm_traffic_control_t));
	traff_ctrl.dying_gasp_threshold = CS_TM_BM_DEF_DYING_GASP_INT;
	traff_ctrl.living_threshold = CS_TM_BM_DEF_LIVING_THSHLD_INT;
	cs_tm_bm_set_traffic_control_int(&traff_ctrl);

	/* reset all the tables used in tm bm */
	memset(voq_profile_tbl, 0, sizeof(voq_profile_tbl));
	memset(wred_profile_tbl, 0, sizeof(wred_profile_tbl));
	memset(voq_qos_tbl, 0, sizeof(voq_qos_tbl));

	/* init default voq profile */
	memset(&voq_prof_mem, 0, sizeof(cs_tm_bm_voq_profile_mem_t));
	voq_prof_mem.voq_min_depth = CS_TM_BM_DEF_VOQ_MIN_DEPTH;
	voq_profile_tbl[0].voq_min_depth = CS_TM_BM_DEF_VOQ_MIN_DEPTH;
	if (use_internal_buff == 1) {
		voq_prof_mem.voq_max_depth = CS_TM_BM_DEF_VOQ_MAX_DEPTH_INT_BUFF;
		voq_profile_tbl[0].voq_max_depth = CS_TM_BM_DEF_VOQ_MAX_DEPTH_INT_BUFF;
	} else {
		voq_prof_mem.voq_max_depth = CS_TM_BM_DEF_VOQ_MAX_DEPTH_EXT_BUFF;
		voq_profile_tbl[0].voq_max_depth = CS_TM_BM_DEF_VOQ_MAX_DEPTH_EXT_BUFF;
	}
	cs_tm_bm_set_voq_profile_mem(0, &voq_prof_mem);

	memset(&voq_prof_mem, 0, sizeof(cs_tm_bm_voq_profile_mem_t));
	voq_prof_mem.voq_min_depth = CS_TM_BM_DEF_CPU_VOQ_MIN_DEPTH;
	voq_prof_mem.voq_max_depth = CS_TM_BM_DEF_CPU_VOQ_MAX_DEPTH;
	voq_profile_tbl[1].voq_min_depth = CS_TM_BM_DEF_CPU_VOQ_MIN_DEPTH;
	voq_profile_tbl[1].voq_max_depth = CS_TM_BM_DEF_CPU_VOQ_MAX_DEPTH;
	voq_profile_tbl[1].ref_cnt += 24;
	cs_tm_bm_set_voq_profile_mem(1, &voq_prof_mem);

	/* performance monitor disabled by default */
	voq_mem.voq_cntr_enable = 0;
	voq_mem.voq_cntr = 0;
	voq_mem.wred_enable = 0 ;
	for(i = 0; i < CS_MAX_VOQ_NO; i++) {
		voq_profile_tbl[0].ref_cnt++;
		voq_qos_tbl[i].wred_prof_idx = i;
		voq_mem.wred_profile = i;
		voq_mem.dest_port = (i >> 3);
		voq_qos_tbl[i].dest_port = (i >> 3);
		if ((i >= CPU_PORT0_VOQ_BASE) && (i < CPU_PORT3_VOQ_BASE)) {
			voq_mem.voq_profile = 1;
			voq_qos_tbl[i].voq_prof_idx = 1;
		} else {
			voq_mem.voq_profile = 0;
			voq_qos_tbl[i].voq_prof_idx = 0;
		}
		cs_tm_bm_set_voq_mem(i, &voq_mem);
	}

	return 0;
} /* cs_tm_bm_init */

int cs_tm_voq_reset(unsigned char voq_id)
{
	cs_tm_bm_voq_profile_mem_t *p_vp;
	cs_wred_profile_t *p_wp;
	cs_tm_bm_voq_mem_t voq_mem;

	/*
	 * ========================== VOQ profile ==============================
	 * De-associate voq profile if point to any.
	 */
	p_vp = &voq_profile_tbl[voq_qos_tbl[voq_id].voq_prof_idx];
	if (voq_qos_tbl[voq_id].voq_prof_idx != 0) {
		if (p_vp->ref_cnt > 0)
			p_vp->ref_cnt--;
		voq_qos_tbl[voq_id].voq_prof_idx = 0;
		voq_profile_tbl[0].ref_cnt++;
	}

	if (voq_qos_tbl[voq_id].wred_enbl == 1) {
		p_wp = &wred_profile_tbl[voq_qos_tbl[voq_id].wred_prof_idx];
		if (p_wp->ref_cnt > 0)
			p_wp->ref_cnt--;
	}
	voq_qos_tbl[voq_id].wred_enbl = 0;
	/*
	 * wred_prof_idx is equal to voq_id, bcuz it's being configured to run
	 * in 1-to-1 mapping WRED select mode.
	 */
	voq_qos_tbl[voq_id].wred_prof_idx = voq_id;

	/* Update voq memory */
	voq_mem.voq_profile = voq_qos_tbl[voq_id].voq_prof_idx;
	voq_mem.wred_profile = voq_qos_tbl[voq_id].wred_prof_idx;
	voq_mem.dest_port = (voq_id >> 3);
	voq_mem.voq_cntr_enable = voq_qos_tbl[voq_id].voq_cnt_enbl;
	voq_mem.voq_cntr = voq_qos_tbl[voq_id].voq_cnt_idx;
	voq_mem.wred_enable = voq_qos_tbl[voq_id].wred_enbl;
	cs_tm_bm_set_voq_mem(voq_id, &voq_mem);

	return 0;
} /* cs_tm_voq_reset */

int cs_tm_port_reset(unsigned char port_id)
{
	cs_tm_bm_dest_port_mem_t dest_port;

	memset(&dest_port, 0x0, sizeof(dest_port));
	return cs_tm_bm_set_dest_port_mem(port_id, &dest_port);
} /* cs_tm_port_reset */

static unsigned char cal_pct_in_base_31(unsigned char pct_base)
{
	unsigned char rslt_idx = 0;

	if (pct_base > 100)
		return 0;

	rslt_idx = (unsigned char)(((unsigned long)pct_base) * 100 / 315);
	if (rslt_idx > 31)
		rslt_idx = 31;
	return rslt_idx;
} /* cal_pct_in_base_31 */

static unsigned char cal_prob_in_base_8(unsigned char drop_prob)
{
	unsigned char rslt_val = 0;

	if (drop_prob > 100)
		return 7;

	rslt_val = (unsigned char)(((unsigned long)drop_prob) * 8 / 100);
	if (rslt_val > 7)
		rslt_val = 7;
	return rslt_val;
} /* cal_prob_in_base_8 */

static int cs_tm_bm_find_profile_entity(unsigned char voq_id,
		unsigned short min_depth, unsigned short max_depth,
		void *wred_args)
{
	cs_tm_bm_voq_profile_mem_t *p_vp;
	unsigned short i;
	int rc = 0;
	struct tc_wredspec *p_wred = (struct tc_wredspec *)wred_args;

	/* =========================== VOQ profile ========================== */
	/* Find one and associate */
	for (i = 0; i < CS_MAX_VOQ_PROFILE; i++) {
		p_vp = &voq_profile_tbl[i];
		/* Check if same rule of voq profile */
		if ((p_vp->voq_min_depth == min_depth) &&
				(p_vp->voq_max_depth == max_depth) &&
				((p_wred == NULL) || ((p_wred != NULL) &&
					((p_wred->enbl == 0) || ((p_wred->enbl == 1) &&
						(p_vp->wred_aqd_weight ==
						 p_wred->aqd_lp_filter_const)))))
				) {
			p_vp->ref_cnt++;
			/* De-associate previous voq profile*/
			voq_profile_tbl[voq_qos_tbl[voq_id].voq_prof_idx].ref_cnt--;
			voq_qos_tbl[voq_id].voq_prof_idx = i;
			break;
		}
	}

	if (i == CS_MAX_VOQ_PROFILE) {	/* No matching rule, pick empty one */
		i = voq_qos_tbl[voq_id].voq_prof_idx;
		if ((voq_profile_tbl[i].ref_cnt > 1) || (i == 0)) {
			for (i = 1; i < CS_MAX_VOQ_PROFILE; i++) {
				p_vp = &voq_profile_tbl[i];
				if (p_vp->ref_cnt == 0) {
					p_vp->voq_min_depth = min_depth;
					p_vp->voq_max_depth = max_depth;
					if ((p_wred != NULL) && (p_wred->enbl == 1))
						p_vp->wred_aqd_weight = p_wred->aqd_lp_filter_const;;
					p_vp->ref_cnt++;
					/* De-associate previous voq profile*/
					voq_profile_tbl[voq_qos_tbl[voq_id].voq_prof_idx].ref_cnt--;
					voq_qos_tbl[voq_id].voq_prof_idx = i;
					break;
				}
			}
		} else {
			p_vp = &voq_profile_tbl[i];
			p_vp->voq_min_depth = min_depth;
			p_vp->voq_max_depth = max_depth;
			if ((p_wred != NULL) && (p_wred->enbl == 1))
				p_vp->wred_aqd_weight = p_wred->aqd_lp_filter_const;
		}
		if (i == CS_MAX_VOQ_PROFILE) /* Can't find unused profile :( */
			rc = -1;
	}

	/* since WRED profile is now 1-to-1 mapped to VOQ_ID, we do not have to
	 * look for available WRED profile */

	return rc;
} /* cs_tm_bm_find_profile_entity */

int cs_tm_bm_set_voq_profile(unsigned char voq_id, unsigned short min_depth,
		unsigned short max_depth, void *wred_args)
{
	int rc;
	cs_tm_bm_voq_profile_mem_t voq_profile;
	cs_tm_bm_voq_mem_t voq_mem;
	struct tc_wredspec *p_wred = (struct tc_wredspec *)wred_args;

	/* Compare rule or allocate new one */
	rc = cs_tm_bm_find_profile_entity(voq_id, min_depth, max_depth,
			wred_args);
	if (rc == -1)
		return rc;

	/* update voq profile */
	voq_profile.voq_min_depth = min_depth;
	voq_profile.voq_max_depth = max_depth;
	voq_profile.wred_adjust_range_index =
		voq_profile_tbl[voq_id].wred_adjust_range_index;
	voq_profile.wred_aqd_weight = voq_profile_tbl[voq_id].wred_aqd_weight;

	/*
	 * if WRED is going to be disabled, then we need to make sure the
	 * SW-keep data structure is aware of that.
	 */
	if ((p_wred != NULL) && (p_wred->enbl == 0))
		voq_qos_tbl[voq_id].wred_enbl = 0;

	/* update wred profile */
	if ((p_wred != NULL) && (p_wred->enbl == 1)) {
		cs_tm_bm_wred_profile_t bm_wred_profile;

		voq_qos_tbl[voq_id].wred_enbl = 1;
		bm_wred_profile.wred_aqd_min_pct_baseline =
			cal_pct_in_base_31(p_wred->min_pct_base);
		bm_wred_profile.wred_aqd_max_pct_baseline =
			cal_pct_in_base_31(p_wred->max_pct_base);
		bm_wred_profile.wred_aqd_min_pct_buffers0 =
			cal_pct_in_base_31(p_wred->min_pct_buffer);
		bm_wred_profile.wred_aqd_max_pct_buffers0 =
			cal_pct_in_base_31(p_wred->max_pct_buffer);
		bm_wred_profile.wred_aqd_max_drop_probability =
			cal_prob_in_base_8(p_wred->drop_prob);
		cs_tm_bm_set_wred_profile_mem(voq_qos_tbl[voq_id].wred_prof_idx,
				&bm_wred_profile);
		voq_profile.wred_aqd_weight = p_wred->aqd_lp_filter_const;
	}
	cs_tm_bm_set_voq_profile_mem(voq_qos_tbl[voq_id].voq_prof_idx,
			&voq_profile);

	/* Update voq memory */
	voq_mem.voq_profile = voq_qos_tbl[voq_id].voq_prof_idx;
	voq_mem.wred_profile = voq_qos_tbl[voq_id].wred_prof_idx;
	voq_mem.dest_port = voq_qos_tbl[voq_id].dest_port;
	voq_mem.voq_cntr_enable = voq_qos_tbl[voq_id].voq_cnt_enbl;
	voq_mem.voq_cntr = voq_qos_tbl[voq_id].voq_cnt_idx;
	voq_mem.wred_enable = voq_qos_tbl[voq_id].wred_enbl;
	cs_tm_bm_set_voq_mem(voq_id, &voq_mem);

	return 0;
} /* cs_tm_bm_set_voq_profile */

int cs_tm_bm_set_voq_cntr(unsigned char voq_id, unsigned char voq_cntr_enbl,
		unsigned char voq_cntr_id)
{
	cs_tm_bm_voq_mem_t voq_mem;

	voq_qos_tbl[voq_id].voq_cnt_enbl = voq_cntr_enbl;
	voq_qos_tbl[voq_id].voq_cnt_idx = voq_cntr_id;

	voq_mem.voq_profile = voq_qos_tbl[voq_id].voq_prof_idx;
	voq_mem.wred_profile = voq_qos_tbl[voq_id].wred_prof_idx;
	voq_mem.dest_port = voq_qos_tbl[voq_id].dest_port;
	voq_mem.voq_cntr_enable = voq_qos_tbl[voq_id].voq_cnt_enbl;
	voq_mem.voq_cntr = voq_qos_tbl[voq_id].voq_cnt_idx;
	voq_mem.wred_enable = voq_qos_tbl[voq_id].wred_enbl;
	cs_tm_bm_set_voq_mem(voq_id, &voq_mem);

	return 0;
} /* cs_tm_bm_set_voq_cntr */

int cs_tm_bm_get_voq_depth(unsigned char voq_id, unsigned short *p_min_depth,
		unsigned short *p_max_depth)
{
	cs_tm_bm_voq_profile_mem_t *p_vp;

	if (voq_id >= CS_MAX_VOQ_NO)
		return -1;
	p_vp = &voq_profile_tbl[voq_qos_tbl[voq_id].voq_prof_idx];

	if (p_min_depth != NULL)
		*p_min_depth = p_vp->voq_min_depth;
	if (p_max_depth != NULL)
		*p_max_depth = p_vp->voq_max_depth;

	return 0;
} /* cs_tm_bm_get_voq_depth */
