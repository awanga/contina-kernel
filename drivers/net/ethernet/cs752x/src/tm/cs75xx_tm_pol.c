/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
#include <asm/io.h>
#include <linux/slab.h>
#include <mach/cs_types.h>
#include <mach/registers.h>
#include "cs75xx_tm_pol.h"
#include "cs_mut.h"

static cs_tm_pol_flow_pol_db_t flow_pol_db;

static int cs_tm_pol_wait_access_done(unsigned int reg)
{
	unsigned int retry = CS_TM_POL_RETRY_TIMES;
	unsigned int access;

	while (retry--) {
		access = readl(reg);
		if ((access & CS_TM_POL_ACCESS_BIT) == 0)
			break;
	}

	if ((access & CS_TM_POL_ACCESS_BIT) == 1)
		return -1;

	return 0;
}

int cs_tm_pol_set_cfg(cs_tm_pol_cfg_t *cfg)
{
	TM_POL_CONFIG_0_t pol_cfg;
	TM_POL_STATUS_0_t status;

	if (cfg == NULL)
		return -1;

	status.wrd = readl(TM_POL_STATUS_0);
	if (status.bf.init_done == 0)
		return -1;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.pol_disable = cfg->pol_disable;
	pol_cfg.bf.pol_bypass_red = cfg->pol_bypass_red;
	pol_cfg.bf.pol_bypass_yellow = cfg->pol_bypass_yellow;
	pol_cfg.bf.pol_color_blind = cfg->pol_color_blind;
	pol_cfg.bf.init = cfg->init;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	return 0;
}

int cs_tm_pol_get_cfg(cs_tm_pol_cfg_t *cfg)
{
	TM_POL_CONFIG_0_t pol_cfg;

	if (cfg == NULL)
		return -1;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	cfg->pol_disable = pol_cfg.bf.pol_disable;
	cfg->pol_bypass_red = pol_cfg.bf.pol_bypass_red;
	cfg->pol_bypass_yellow = pol_cfg.bf.pol_bypass_yellow;
	cfg->pol_color_blind = pol_cfg.bf.pol_color_blind;
	cfg->init = pol_cfg.bf.init;

	return 0;
}

int cs_tm_pol_set_spid_cfg(cs_tm_pol_cfg_spid_t *cfg)
{
	TM_POL_CONFIG_SPID_t spid_cfg;

	if (cfg == NULL)
		return -1;

	spid_cfg.wrd = readl(TM_POL_CONFIG_SPID);
	spid_cfg.bf.bypass_yellow = cfg->bypass_yellow;
	spid_cfg.bf.bypass_red = cfg->bypass_red;
	spid_cfg.bf.disable = cfg->disable;
	spid_cfg.bf.color_blind = cfg->color_blind;
	spid_cfg.bf.bypass_yellow_define = cfg->bypass_yellow_define;
	spid_cfg.bf.update_mode = cfg->update_mode;
	spid_cfg.bf.nest_level = cfg->nest_level;
	spid_cfg.bf.commit_override_enable = cfg->commit_override_enable;
	writel(spid_cfg.wrd, TM_POL_CONFIG_SPID);

	return 0;
}

int cs_tm_pol_get_spid_cfg(cs_tm_pol_cfg_spid_t *cfg)
{
	TM_POL_CONFIG_SPID_t spid_cfg;

	if (cfg == NULL)
		return -1;

	spid_cfg.wrd = readl(TM_POL_CONFIG_SPID);
	cfg->bypass_yellow = spid_cfg.bf.bypass_yellow;
	cfg->bypass_red = spid_cfg.bf.bypass_red;
	cfg->disable = spid_cfg.bf.disable;
	cfg->color_blind = spid_cfg.bf.color_blind;
	cfg->bypass_yellow_define = spid_cfg.bf.bypass_yellow_define;
	cfg->update_mode = spid_cfg.bf.update_mode;
	cfg->nest_level = spid_cfg.bf.nest_level;
	cfg->commit_override_enable = spid_cfg.bf.commit_override_enable;

	return 0;
}

int cs_tm_pol_set_flow_cfg(cs_tm_pol_cfg_flow_t *cfg)
{
	TM_POL_CONFIG_FLOW_t flow_cfg;

	if (cfg == NULL)
		return -1;

	flow_cfg.wrd = readl(TM_POL_CONFIG_FLOW);
	flow_cfg.bf.bypass_yellow = cfg->bypass_yellow;
	flow_cfg.bf.bypass_red = cfg->bypass_red;
	flow_cfg.bf.disable = cfg->disable;
	flow_cfg.bf.color_blind = cfg->color_blind;
	flow_cfg.bf.bypass_yellow_define = cfg->bypass_yellow_define;
	flow_cfg.bf.update_mode = cfg->update_mode;
	flow_cfg.bf.nest_level = cfg->nest_level;
	flow_cfg.bf.commit_override_enable = cfg->commit_override_enable;
	writel(flow_cfg.wrd, TM_POL_CONFIG_FLOW);

	return 0;
}

int cs_tm_pol_get_flow_cfg(cs_tm_pol_cfg_flow_t *cfg)
{
	TM_POL_CONFIG_FLOW_t flow_cfg;

	if (cfg == NULL)
		return -1;

	flow_cfg.wrd = readl(TM_POL_CONFIG_FLOW);
	cfg->bypass_yellow = flow_cfg.bf.bypass_yellow;
	cfg->bypass_red = flow_cfg.bf.bypass_red;
	cfg->disable = flow_cfg.bf.disable;
	cfg->color_blind = flow_cfg.bf.color_blind;
	cfg->bypass_yellow_define = flow_cfg.bf.bypass_yellow_define;
	cfg->update_mode = flow_cfg.bf.update_mode;
	cfg->nest_level = flow_cfg.bf.nest_level;
	cfg->commit_override_enable = flow_cfg.bf.commit_override_enable;

	return 0;
}

int cs_tm_pol_set_pkt_type_cfg(cs_tm_pol_cfg_pkt_type_t *cfg)
{
	TM_POL_CONFIG_PKT_TYPE_t pkt_type_cfg;

	if (cfg == NULL)
		return -1;

	pkt_type_cfg.wrd = readl(TM_POL_CONFIG_PKT_TYPE);
	pkt_type_cfg.bf.bypass_yellow = cfg->bypass_yellow;
	pkt_type_cfg.bf.bypass_red = cfg->bypass_red;
	pkt_type_cfg.bf.disable = cfg->disable;
	pkt_type_cfg.bf.color_blind = cfg->color_blind;
	pkt_type_cfg.bf.bypass_yellow_define = cfg->bypass_yellow_define;
	pkt_type_cfg.bf.update_mode = cfg->update_mode;
	pkt_type_cfg.bf.nest_level = cfg->nest_level;
	pkt_type_cfg.bf.commit_override_enable = cfg->commit_override_enable;
	pkt_type_cfg.bf.lspid0 = cfg->lspid0;
	pkt_type_cfg.bf.lspid1 = cfg->lspid1;
	pkt_type_cfg.bf.lspid2 = cfg->lspid2;
	pkt_type_cfg.bf.lspid3 = cfg->lspid3;
	writel(pkt_type_cfg.wrd, TM_POL_CONFIG_PKT_TYPE);

	return 0;
}

int cs_tm_pol_get_pkt_type_cfg(cs_tm_pol_cfg_pkt_type_t *cfg)
{
	TM_POL_CONFIG_PKT_TYPE_t pkt_type_cfg;

	if (cfg == NULL)
		return -1;

	pkt_type_cfg.wrd = readl(TM_POL_CONFIG_PKT_TYPE);
	cfg->bypass_yellow = pkt_type_cfg.bf.bypass_yellow;
	cfg->bypass_red = pkt_type_cfg.bf.bypass_red;
	cfg->disable = pkt_type_cfg.bf.disable;
	cfg->color_blind = pkt_type_cfg.bf.color_blind;
	cfg->bypass_yellow_define = pkt_type_cfg.bf.bypass_yellow_define;
	cfg->update_mode = pkt_type_cfg.bf.update_mode;
	cfg->nest_level = pkt_type_cfg.bf.nest_level;
	cfg->commit_override_enable = pkt_type_cfg.bf.commit_override_enable;
	cfg->lspid0 = pkt_type_cfg.bf.lspid0;
	cfg->lspid1 = pkt_type_cfg.bf.lspid1;
	cfg->lspid2 = pkt_type_cfg.bf.lspid2;
	cfg->lspid3 = pkt_type_cfg.bf.lspid3;

	return 0;
}

int cs_tm_pol_set_cpu_cfg(cs_tm_pol_cfg_cpu_t *cfg)
{
	TM_POL_CONFIG_CPU_t cpu_cfg;

	if (cfg == NULL)
		return -1;

	cpu_cfg.wrd = readl(TM_POL_CONFIG_CPU);
	cpu_cfg.bf.bypass_yellow = cfg->bypass_yellow;
	cpu_cfg.bf.bypass_red = cfg->bypass_red;
	cpu_cfg.bf.disable = cfg->disable;
	cpu_cfg.bf.color_blind = cfg->color_blind;
	cpu_cfg.bf.bypass_yellow_define = cfg->bypass_yellow_define;
	cpu_cfg.bf.update_mode = cfg->update_mode;
	cpu_cfg.bf.nest_level = cfg->nest_level;
	cpu_cfg.bf.commit_override_enable = cfg->commit_override_enable;
	cpu_cfg.bf.voq0 = cfg->voq0;
	cpu_cfg.bf.voq1 = cfg->voq1;
	writel(cpu_cfg.wrd, TM_POL_CONFIG_CPU);

	return 0;
}

int cs_tm_pol_get_cpu_cfg(cs_tm_pol_cfg_cpu_t *cfg)
{
	TM_POL_CONFIG_CPU_t cpu_cfg;

	if (cfg == NULL)
		return -1;

	cpu_cfg.wrd = readl(TM_POL_CONFIG_CPU);
	cfg->bypass_yellow = cpu_cfg.bf.bypass_yellow;
	cfg->bypass_red = cpu_cfg.bf.bypass_red;
	cfg->disable = cpu_cfg.bf.disable;
	cfg->color_blind = cpu_cfg.bf.color_blind;
	cfg->bypass_yellow_define = cpu_cfg.bf.bypass_yellow_define;
	cfg->update_mode = cpu_cfg.bf.update_mode;
	cfg->nest_level = cpu_cfg.bf.nest_level;
	cfg->commit_override_enable = cpu_cfg.bf.commit_override_enable;
	cfg->voq0 = cpu_cfg.bf.voq0;
	cfg->voq1 = cpu_cfg.bf.voq1;

	return 0;
}

int cs_tm_pol_set_ipq_dest(unsigned char index, unsigned char ipq_val)
{
	TM_POL_IPG_DEST_t dest_ipg;

	if ((index < CS_TM_POL_IPG_IDX_MIN) || (index > CS_TM_POL_IPG_IDX_MAX))
		return -1;

	dest_ipg.wrd = readl(TM_POL_IPG_DEST);
	dest_ipg.wrd = dest_ipg.wrd | (ipq_val << ((index - 1) * 5));
	writel(dest_ipg.wrd, TM_POL_IPG_DEST);

	return 0;
}

int cs_tm_pol_get_ipq_dest(unsigned char index, unsigned char *ipg_val)
{
	TM_POL_IPG_DEST_t dest_ipg;

	if ((index < CS_TM_POL_IPG_IDX_MIN) || (index > CS_TM_POL_IPG_IDX_MAX))
		return -1;

	dest_ipg.wrd = readl(TM_POL_IPG_DEST);
	*ipg_val = (dest_ipg.wrd >> (index-1)*5) & 0x1f;

	return 0;
}

int cs_tm_pol_set_ipq_dest_map(unsigned char port_id, unsigned char index)
{
	TM_POL_IPG_DEST_MAP_t dest_ipg_map;

	if (port_id > CS_TM_POL_PORT_ID_MAX)
		return -1;

	dest_ipg_map.wrd = readl(TM_POL_IPG_DEST_MAP);
	dest_ipg_map.wrd = dest_ipg_map.wrd | (index << (port_id * 3));
	writel(dest_ipg_map.wrd, TM_POL_IPG_DEST_MAP);

	return 0;
}

int cs_tm_pol_get_ipq_dest_map(unsigned char port_id, unsigned char *index)
{
	TM_POL_IPG_DEST_MAP_t dest_ipg_map;

	if (port_id > CS_TM_POL_PORT_ID_MAX)
		return -1;

	dest_ipg_map.wrd = readl(TM_POL_IPG_DEST_MAP);
	*index = (dest_ipg_map.wrd >> (port_id * 3)) & 0x07;

	return 0;
}

int cs_tm_pol_set_ipq_src(unsigned char index, unsigned char ipq_val)
{
	TM_POL_IPG_SRC_t src_ipg;

	if ((index < CS_TM_POL_IPG_IDX_MIN) || (index > CS_TM_POL_IPG_IDX_MAX))
		return -1;

	src_ipg.wrd = readl(TM_POL_IPG_SRC);
	src_ipg.wrd = src_ipg.wrd | (ipq_val << ((index - 1) * 5));
	writel(src_ipg.wrd, TM_POL_IPG_SRC);

	return 0;
}

int cs_tm_pol_get_ipq_src(unsigned char index, unsigned char *ipg_val)
{
	TM_POL_IPG_SRC_t src_ipg;

	if ((index < CS_TM_POL_IPG_IDX_MIN) || (index > CS_TM_POL_IPG_IDX_MAX))
		return -1;

	src_ipg.wrd = readl(TM_POL_IPG_SRC);
	*ipg_val = (src_ipg.wrd >> (index - 1) * 5) & 0x1f;

	return 0;
}

int cs_tm_pol_set_ipq_src_map(unsigned char port_id, unsigned char index)
{
	TM_POL_IPG_SRC_MAP_t src_ipg_map;

	if (port_id > CS_TM_POL_PORT_ID_MAX)
		return -1;

	src_ipg_map.wrd = readl(TM_POL_IPG_SRC_MAP);
	src_ipg_map.wrd = src_ipg_map.wrd | (index << (port_id * 3));
	writel(src_ipg_map.wrd, TM_POL_IPG_SRC_MAP);

	return 0;
}

int cs_tm_pol_get_ipq_src_map(unsigned char port_id, unsigned char *index)
{
	TM_POL_IPG_SRC_MAP_t src_ipg_map;

	if (port_id > CS_TM_POL_PORT_ID_MAX)
		return -1;

	src_ipg_map.wrd = readl(TM_POL_IPG_SRC_MAP);
	*index = (src_ipg_map.wrd >> (port_id * 3)) & 0x07;

	return 0;
}

int cs_tm_pol_get_pol_mem_status(cs_tm_pol_mem_type_t type,
		unsigned char *err_addr, unsigned char *err_correct_addr)
{
	TM_POL_FLOW_PROFILE_MEM_STATUS_t flow_profile;
	TM_POL_FLOW_STATUS_MEM_STATUS_t flow_status;
	TM_POL_SPID_PROFILE_MEM_STATUS_t spid_profile;
	TM_POL_SPID_STATUS_MEM_STATUS_t spid_status;
	TM_POL_CPU_PROFILE_MEM_STATUS_t cpu_profile;
	TM_POL_CPU_STATUS_MEM_STATUS_t cpu_status;
	TM_POL_PKT_TYPE_PROFILE_MEM_STATUS_t pkt_type_profile;
	TM_POL_PKT_TYPE_STATUS_MEM_STATUS_t pkt_type_status;
	TM_POL_CONFIG_0_t pol_cfg;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	switch (type) {
	case CS_TM_POL_FLOW_PROFILE_MEM:
		flow_profile.wrd = readl(TM_POL_FLOW_PROFILE_MEM_STATUS);
		*err_addr = flow_profile.bf.err_addr;
		*err_correct_addr = flow_profile.bf.err_correct_addr;
		break;

	case CS_TM_POL_FLOW_STATUS_MEM:
		flow_status.wrd = readl(TM_POL_FLOW_STATUS_MEM_STATUS);
		*err_addr = flow_status.bf.err_addr;
		*err_correct_addr = flow_status.bf.err_correct_addr;
		break;

	case CS_TM_POL_SPID_PROFILE_MEM:
		spid_profile.wrd = readl(TM_POL_SPID_PROFILE_MEM_STATUS);
		*err_addr = spid_profile.bf.err_addr;
		*err_correct_addr = spid_profile.bf.err_correct_addr;
		break;

	case CS_TM_POL_SPID_STATUS_MEM:
		spid_status.wrd = readl(TM_POL_SPID_STATUS_MEM_STATUS);
		*err_addr = spid_status.bf.err_addr;
		*err_correct_addr = spid_status.bf.err_correct_addr;
		break;

	case CS_TM_POL_CPU_PROFILE_MEM:
		cpu_profile.wrd = readl(TM_POL_CPU_PROFILE_MEM_STATUS);
		*err_addr = cpu_profile.bf.err_addr;
		*err_correct_addr = cpu_profile.bf.err_correct_addr;
		break;

	case CS_TM_POL_CPU_STATUS_MEM:
		cpu_status.wrd = readl(TM_POL_CPU_STATUS_MEM_STATUS);
		*err_addr = cpu_status.bf.err_addr;
		*err_correct_addr = cpu_status.bf.err_correct_addr;
		break;

	case CS_TM_POL_PKT_TYPE_PROFILE_MEM:
		pkt_type_profile.wrd =
			readl(TM_POL_PKT_TYPE_PROFILE_MEM_STATUS);
		*err_addr = pkt_type_profile.bf.err_addr;
		*err_correct_addr = pkt_type_profile.bf.err_correct_addr;
		break;

	case CS_TM_POL_PKT_TYPE_STATUS_MEM:
		pkt_type_status.wrd = readl(TM_POL_PKT_TYPE_STATUS_MEM_STATUS);
		*err_addr = pkt_type_status.bf.err_addr;
		*err_correct_addr = pkt_type_status.bf.err_correct_addr;
		break;
	}

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	return 0;
}

int cs_tm_pol_get_interrupt_0(unsigned int *int_status)
{
	*int_status = readl(TM_POL_INTERRUPT_0);
	return 0;
}

int cs_tm_pol_set_interrupt_0(unsigned int int_status)
{
	writel(int_status, TM_POL_INTERRUPT_0);
	return 0;
}

int cs_tm_pol_get_intenable_0(unsigned int *int_enable)
{
	*int_enable = readl(TM_POL_INTENABLE_0);
	return 0;
}

int cs_tm_pol_set_intenable_0(unsigned int int_enable)
{
	writel(int_enable, TM_POL_INTENABLE_0);
	return 0;
}

int cs_tm_pol_get_interrupt_1(unsigned int *int_status)
{
	*int_status = readl(TM_POL_INTERRUPT_1);
	return 0;
}

int cs_tm_pol_set_interrupt_1(unsigned int int_status)
{
	writel(int_status, TM_POL_INTERRUPT_1);
	return 0;
}

int cs_tm_pol_get_intenable_1(unsigned int *int_enable)
{
	*int_enable = readl(TM_POL_INTENABLE_1);
	return 0;
}

int cs_tm_pol_set_intenable_1(unsigned int int_enable)
{
	writel(int_enable, TM_POL_INTENABLE_1);
	return 0;
}

int cs_tm_pol_get_status_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_status_mem_t *bucket)
{
	TM_POL_CONFIG_0_t pol_cfg;
	TM_POL_FLOW_STATUS_MEM_ACCESS_t flow_access;
	TM_POL_SPID_STATUS_MEM_ACCESS_t spid_access;
	TM_POL_CPU_STATUS_MEM_ACCESS_t cpu_access;
	TM_POL_PKT_TYPE_STATUS_MEM_ACCESS_t pkt_type_access;
	unsigned int reg_data0;
	unsigned int reg_data1;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	switch (type) {
	case CS_TM_POL_FLOW_STATUS_MEM:
		/* set access command register */
		flow_access.bf.addr = addr;
		flow_access.bf.access = CS_ENABLE;
		flow_access.bf.rbw = CS_OP_READ;
		writel(flow_access.wrd, TM_POL_FLOW_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_FLOW_STATUS_MEM_ACCESS) == -1)
			return (-1);

		/* read status memory */
		reg_data0 = readl(TM_POL_FLOW_STATUS_MEM_DATA0);
		reg_data1 = readl(TM_POL_FLOW_STATUS_MEM_DATA1);
		break;

	case CS_TM_POL_SPID_STATUS_MEM:
		/* set access command register */
		spid_access.bf.addr = addr;
		spid_access.bf.access = CS_ENABLE;
		spid_access.bf.rbw = CS_OP_READ;
		writel(spid_access.wrd, TM_POL_SPID_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_SPID_STATUS_MEM_ACCESS) == -1)
			return (-1);

		/* read status memory */
		reg_data0 = readl(TM_POL_SPID_STATUS_MEM_DATA0);
		reg_data1 = readl(TM_POL_SPID_STATUS_MEM_DATA1);
		break;

	case CS_TM_POL_CPU_STATUS_MEM:
		/* set access command register */
		cpu_access.bf.addr = addr;
		cpu_access.bf.access = CS_ENABLE;
		cpu_access.bf.rbw = CS_OP_READ;
		writel(cpu_access.wrd, TM_POL_CPU_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_CPU_STATUS_MEM_ACCESS) == -1)
			return (-1);

		/* read status memory */
		reg_data0 = readl(TM_POL_CPU_STATUS_MEM_DATA0);
		reg_data1 = readl(TM_POL_CPU_STATUS_MEM_DATA1);
		break;

	case CS_TM_POL_PKT_TYPE_STATUS_MEM:
		/* set access command register */
		pkt_type_access.bf.addr = addr;
		pkt_type_access.bf.access = CS_ENABLE;
		pkt_type_access.bf.rbw = CS_OP_READ;
		writel(pkt_type_access.wrd, TM_POL_PKT_TYPE_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_PKT_TYPE_STATUS_MEM_ACCESS) == -1)
			return (-1);

		/* read status memory */
		reg_data0 = readl(TM_POL_PKT_TYPE_STATUS_MEM_DATA0);
		reg_data1 = readl(TM_POL_PKT_TYPE_STATUS_MEM_DATA1);
		break;

	default:
		return -1;
	}

	bucket->cir_token_bucket = reg_data0 & 0x000fffff;
	bucket->pir_token_bucket = (reg_data0 >> 20) + (reg_data1 << 12);

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	return 0;
}

int cs_tm_pol_set_status_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_status_mem_t *bucket)
{
	TM_POL_CONFIG_0_t pol_cfg;
	TM_POL_FLOW_STATUS_MEM_ACCESS_t flow_access;
	TM_POL_SPID_STATUS_MEM_ACCESS_t spid_access;
	TM_POL_CPU_STATUS_MEM_ACCESS_t cpu_access;
	TM_POL_PKT_TYPE_STATUS_MEM_ACCESS_t pkt_type_access;
	unsigned int reg_data0;
	unsigned int reg_data1;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	reg_data0 = bucket->cir_token_bucket + (bucket->pir_token_bucket<<20);
	reg_data1 = bucket->pir_token_bucket >> 12;

	switch (type) {
	case CS_TM_POL_FLOW_STATUS_MEM:
		writel(reg_data0, TM_POL_FLOW_STATUS_MEM_DATA0);
		writel(reg_data1, TM_POL_FLOW_STATUS_MEM_DATA1);

		/* set access command register */
		flow_access.bf.addr = addr;
		flow_access.bf.access = CS_ENABLE;
		flow_access.bf.rbw = CS_OP_WRITE;
		writel(flow_access.wrd, TM_POL_FLOW_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_FLOW_STATUS_MEM_ACCESS) == -1)
			return (-1);
		break;

	case CS_TM_POL_SPID_STATUS_MEM:
		writel(reg_data0, TM_POL_SPID_STATUS_MEM_DATA0);
		writel(reg_data1, TM_POL_SPID_STATUS_MEM_DATA1);

		/* set access command register */
		spid_access.bf.addr = addr;
		spid_access.bf.access = CS_ENABLE;
		spid_access.bf.rbw = CS_OP_WRITE;
		writel(spid_access.wrd, TM_POL_SPID_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_SPID_STATUS_MEM_ACCESS) == -1)
			return (-1);
		break;

	case CS_TM_POL_CPU_STATUS_MEM:
		writel(reg_data0, TM_POL_CPU_STATUS_MEM_DATA0);
		writel(reg_data1, TM_POL_CPU_STATUS_MEM_DATA1);

		/* set access command register */
		cpu_access.bf.addr = addr;
		cpu_access.bf.access = CS_ENABLE;
		cpu_access.bf.rbw = CS_OP_WRITE;
		writel(cpu_access.wrd, TM_POL_CPU_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_CPU_STATUS_MEM_ACCESS) == -1)
			return (-1);
		break;

	case CS_TM_POL_PKT_TYPE_STATUS_MEM:
		writel(reg_data0, TM_POL_PKT_TYPE_STATUS_MEM_DATA0);
		writel(reg_data1, TM_POL_PKT_TYPE_STATUS_MEM_DATA1);

		/* set access command register */
		pkt_type_access.bf.addr = addr;
		pkt_type_access.bf.access = CS_ENABLE;
		pkt_type_access.bf.rbw = CS_OP_WRITE;
		writel(pkt_type_access.wrd, TM_POL_PKT_TYPE_STATUS_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_PKT_TYPE_STATUS_MEM_ACCESS) == -1)
			return (-1);
		break;

	default:
		return -1;
	}

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	return 0;
}

int cs_tm_pol_get_profile_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_profile_mem_t *profile)
{
	TM_POL_CONFIG_0_t pol_cfg;
	TM_POL_FLOW_PROFILE_MEM_ACCESS_t flow_access;
	TM_POL_SPID_PROFILE_MEM_ACCESS_t spid_access;
	TM_POL_CPU_PROFILE_MEM_ACCESS_t cpu_access;
	TM_POL_PKT_TYPE_PROFILE_MEM_ACCESS_t pkt_type_access;
//	TM_POL_FLOW_PROFILE_MEM_DATA0_t flow_data0;
//	TM_POL_FLOW_PROFILE_MEM_DATA1_t flow_data1;
//	TM_POL_SPID_PROFILE_MEM_DATA0_t spid_data0;
//	TM_POL_SPID_PROFILE_MEM_DATA1_t spid_data1;
//	TM_POL_CPU_PROFILE_MEM_DATA0_t cpu_data0;
//	TM_POL_CPU_PROFILE_MEM_DATA1_t cpu_data1;
//	TM_POL_PKT_TYPE_PROFILE_MEM_DATA0_t pkt_type_data0;
//	TM_POL_PKT_TYPE_PROFILE_MEM_DATA1_t pkt_type_data1;
	unsigned int reg_data0;
	unsigned int reg_data1;
	TM_POL_PROFILE_MEM_DATA0_t data0;
	TM_POL_PROFILE_MEM_DATA1_t data1;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	switch (type){
	case CS_TM_POL_FLOW_PROFILE_MEM:
		/* set access command register */
		flow_access.bf.addr = addr;
		flow_access.bf.access = CS_ENABLE;
		flow_access.bf.rbw = CS_OP_READ;
		writel(flow_access.wrd, TM_POL_FLOW_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_FLOW_PROFILE_MEM_ACCESS) == -1)
			return (-1);

		/* read profile memory */
		reg_data0 = readl(TM_POL_FLOW_PROFILE_MEM_DATA0);
		reg_data1 = readl(TM_POL_FLOW_PROFILE_MEM_DATA1);
		break;

	case CS_TM_POL_SPID_PROFILE_MEM:
		/* set access command register */
		spid_access.bf.addr = addr;
		spid_access.bf.access = CS_ENABLE;
		spid_access.bf.rbw = CS_OP_READ;
		writel(spid_access.wrd, TM_POL_SPID_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_SPID_PROFILE_MEM_ACCESS) == -1)
			return (-1);

		/* read profile memory */
		reg_data0 = readl(TM_POL_SPID_PROFILE_MEM_DATA0);
		reg_data1 = readl(TM_POL_SPID_PROFILE_MEM_DATA1);
		//??????????????
		break;

	case CS_TM_POL_CPU_PROFILE_MEM:
		/* set access command register */
		cpu_access.bf.addr = addr;
		cpu_access.bf.access = CS_ENABLE;
		cpu_access.bf.rbw = CS_OP_READ;
		writel(cpu_access.wrd, TM_POL_CPU_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_CPU_PROFILE_MEM_ACCESS) == -1)
			return (-1);

		/* read profile memory */
		reg_data0 = readl(TM_POL_CPU_PROFILE_MEM_DATA0);
		reg_data1 = readl(TM_POL_CPU_PROFILE_MEM_DATA1);
		break;

	case CS_TM_POL_PKT_TYPE_PROFILE_MEM:
		/* set access command register */
		pkt_type_access.bf.addr = addr;
		pkt_type_access.bf.access = CS_ENABLE;
		pkt_type_access.bf.rbw = CS_OP_READ;
		writel(pkt_type_access.wrd, TM_POL_PKT_TYPE_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_PKT_TYPE_PROFILE_MEM_ACCESS) == -1)
			return (-1);

		/* read profile memory */
		reg_data0 = readl(TM_POL_PKT_TYPE_PROFILE_MEM_DATA0);
		reg_data1 = readl(TM_POL_PKT_TYPE_PROFILE_MEM_DATA1);
		break;

	default:
		return -1;
	}

	data0 = (TM_POL_PROFILE_MEM_DATA0_t)(reg_data0);
	data1 = (TM_POL_PROFILE_MEM_DATA1_t)(reg_data1);

	profile->policer_type = data0.bf.policer_type;
	profile->range = data0.bf.range;
	profile->cir_credit = data0.bf.cir_credit;
	profile->cir_max_credit = data0.bf.cir_max_credit;
	profile->pir_credit = data0.bf.pir_credit + (data1.bf.pir_credit << 2);
	profile->pir_max_credit = data1.bf.pir_max_credit;
	profile->bypass_yellow = data1.bf.bypass_yellow;
	profile->bypass_red = data1.bf.bypass_red;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	return 0;
}

int cs_tm_pol_set_profile_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_profile_mem_t *profile)
{
	TM_POL_CONFIG_0_t pol_cfg;
	TM_POL_FLOW_PROFILE_MEM_ACCESS_t flow_access;
	TM_POL_SPID_PROFILE_MEM_ACCESS_t spid_access;
	TM_POL_CPU_PROFILE_MEM_ACCESS_t cpu_access;
	TM_POL_PKT_TYPE_PROFILE_MEM_ACCESS_t pkt_type_access;
	TM_POL_FLOW_PROFILE_MEM_DATA0_t flow_data0;
	TM_POL_FLOW_PROFILE_MEM_DATA1_t flow_data1;
	TM_POL_SPID_PROFILE_MEM_DATA0_t spid_data0;
	TM_POL_SPID_PROFILE_MEM_DATA1_t spid_data1;
	TM_POL_CPU_PROFILE_MEM_DATA0_t cpu_data0;
	TM_POL_CPU_PROFILE_MEM_DATA1_t cpu_data1;
	TM_POL_PKT_TYPE_PROFILE_MEM_DATA0_t pkt_type_data0;
	TM_POL_PKT_TYPE_PROFILE_MEM_DATA1_t pkt_type_data1;
//	unsigned int reg_data0;
//	unsigned int reg_data1;
//	void data0;
//	void data1;

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_ENABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	switch (type){
	case CS_TM_POL_FLOW_PROFILE_MEM:
		flow_data0.bf.policer_type = profile->policer_type;
		flow_data0.bf.range = profile->range;
		flow_data0.bf.cir_credit = profile->cir_credit;
		flow_data0.bf.cir_max_credit= profile->cir_max_credit;
		flow_data0.bf.pir_credit = profile->pir_credit & 0x0003;
		flow_data1.bf.pir_credit = profile->pir_credit >> 2;
		flow_data1.bf.pir_max_credit= profile->pir_max_credit;
		flow_data1.bf.bypass_yellow = profile->bypass_yellow;
		flow_data1.bf.bypass_red = profile->bypass_red;

		writel(flow_data0.wrd, TM_POL_FLOW_PROFILE_MEM_DATA0);
		writel(flow_data1.wrd, TM_POL_FLOW_PROFILE_MEM_DATA1);

		/* set access command register */
		flow_access.bf.addr = addr;
		flow_access.bf.access = CS_ENABLE;
		flow_access.bf.rbw = CS_OP_WRITE;
		writel(flow_access.wrd, TM_POL_FLOW_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_FLOW_PROFILE_MEM_ACCESS) == -1)
			return (-1);
		break;

	case CS_TM_POL_SPID_PROFILE_MEM:
		spid_data0.wrd = 0;
		spid_data1.wrd = 0;
		spid_data0.bf.policer_type = profile->policer_type;
		spid_data0.bf.range = profile->range;
		spid_data0.bf.cir_credit = profile->cir_credit;
		spid_data0.bf.cir_max_credit = profile->cir_max_credit;
		spid_data0.bf.pir_max_credit = profile->pir_max_credit & 0x0007;
		spid_data1.bf.pir_max_credit = profile->pir_max_credit >> 3;
		spid_data1.bf.bypass_yellow = profile->bypass_yellow;
		spid_data1.bf.bypass_red = profile->bypass_red;

		writel(spid_data0.wrd, TM_POL_SPID_PROFILE_MEM_DATA0);
		writel(spid_data1.wrd, TM_POL_SPID_PROFILE_MEM_DATA1);

		/* set access command register */
		spid_access.bf.addr = addr;
		spid_access.bf.access = CS_ENABLE;
		spid_access.bf.rbw = CS_OP_WRITE;
		writel(spid_access.wrd, TM_POL_SPID_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_SPID_PROFILE_MEM_ACCESS) == -1)
			return (-1);
		break;

	case CS_TM_POL_CPU_PROFILE_MEM:
		cpu_data0.bf.policer_type = profile->policer_type;
		cpu_data0.bf.range = profile->range;
		cpu_data0.bf.cir_credit = profile->cir_credit;
		cpu_data0.bf.cir_max_credit = profile->cir_max_credit;
		cpu_data0.bf.pir_credit = profile->pir_credit & 0x0003;
		cpu_data1.bf.pir_credit = profile->pir_credit >> 2;
		cpu_data1.bf.pir_max_credit = profile->pir_max_credit;
		cpu_data1.bf.bypass_yellow = profile->bypass_yellow;
		cpu_data1.bf.bypass_red = profile->bypass_red;

		writel(cpu_data0.wrd, TM_POL_CPU_PROFILE_MEM_DATA0);
		writel(cpu_data1.wrd, TM_POL_CPU_PROFILE_MEM_DATA1);

		/* set access command register */
		cpu_access.bf.addr = addr;
		cpu_access.bf.access = CS_ENABLE;
		cpu_access.bf.rbw = CS_OP_WRITE;
		writel(cpu_access.wrd, TM_POL_CPU_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_CPU_PROFILE_MEM_ACCESS) == -1)
			return (-1);
		break;

	case CS_TM_POL_PKT_TYPE_PROFILE_MEM:
		pkt_type_data0.bf.policer_type = profile->policer_type;
		pkt_type_data0.bf.range = profile->range;
		pkt_type_data0.bf.cir_credit = profile->cir_credit;
		pkt_type_data0.bf.cir_max_credit = profile->cir_max_credit;
		pkt_type_data0.bf.pir_credit = profile->pir_credit & 0x0003;
		pkt_type_data1.bf.pir_credit = profile->pir_credit >> 2;
		pkt_type_data1.bf.pir_max_credit = profile->pir_max_credit;
		pkt_type_data1.bf.bypass_yellow = profile->bypass_yellow;
		pkt_type_data1.bf.bypass_red = profile->bypass_red;

		writel(pkt_type_data0.wrd, TM_POL_PKT_TYPE_PROFILE_MEM_DATA0);
		writel(pkt_type_data1.wrd, TM_POL_PKT_TYPE_PROFILE_MEM_DATA1);

		/* set access command register */
		pkt_type_access.bf.addr = addr;
		pkt_type_access.bf.access = CS_ENABLE;
		pkt_type_access.bf.rbw = CS_OP_WRITE;
		writel(pkt_type_access.wrd, TM_POL_PKT_TYPE_PROFILE_MEM_ACCESS);
		if (cs_tm_pol_wait_access_done(
			TM_POL_PKT_TYPE_PROFILE_MEM_ACCESS) == -1)
			return (-1);
		break;

	default:
		return -1;
	}

	pol_cfg.wrd = readl(TM_POL_CONFIG_0);
	pol_cfg.bf.cpu_full_access = CS_DISABLE;
	writel(pol_cfg.wrd, TM_POL_CONFIG_0);

	return 0;
}

int cs_tm_pol_rate_divisor_to_credit(unsigned int rate_bps,
		cs_tm_pol_freq_select_t freq_sel, unsigned short *p_credit)
{
	unsigned int div;
	unsigned int rate_kbps = rate_bps / 1000, max_rate, min_rate;

	switch (freq_sel) {
	case CS_TM_POL_40_KHZ:
		div = CS_TM_POL_DIV_40_KHZ;
		max_rate = CS_TM_POL_RESOL1_MAX_RATE;
		min_rate = CS_TM_POL_RESOL1_MIN_RATE;
		break;
	case CS_TM_POL_10_KHZ:
		div = CS_TM_POL_DIV_10_KHZ;
		max_rate = CS_TM_POL_RESOL4_MAX_RATE;
		min_rate = CS_TM_POL_RESOL4_MIN_RATE;
		break;
	case CS_TM_POL_2_5_KHZ:
		div = CS_TM_POL_DIV_2_5_KHZ;
		max_rate = CS_TM_POL_RESOL16_MAX_RATE;
		min_rate = CS_TM_POL_RESOL16_MIN_RATE;
		break;
	case CS_TM_POL_0_625_KHZ:
		div = CS_TM_POL_DIV_0_625_KHZ;
		max_rate = CS_TM_POL_RESOL64_MAX_RATE;
		min_rate = CS_TM_POL_RESOL64_MIN_RATE;
		break;
	default:
		return -1;
		break;
	}

	if ((rate_kbps > max_rate) || (rate_kbps < min_rate))
		return -1;

	*p_credit = (rate_kbps * div) / CS_TM_POL_MAX_CYCLE_RATE;

	return 0;
} /* cs_tm_pol_rate_divisor_to_credit */

/* For SPID policer, f_4_modes should be FALSE, since HW only supports 2 ranges.
   For other policers, f_4_modes should be TRUE, since HW supports 4 ranges. */
int cs_tm_pol_convert_rate_to_hw_value(unsigned int rate_bps,
		cs_boolean f_4_modes, cs_tm_pol_freq_select_t *p_freq_sel,
		unsigned short *p_credit)
{
	unsigned int rate_kbps = rate_bps / 1000;
	unsigned int div;

	if (f_4_modes == TRUE) {
		/*
		 * if (rate_kbps <= 2.6Gbps && rate_kbps > 655 Mbps)
		 * 	then divisor = 1
		 * if (rate_kbps <= 655 Mbps && rate_kbps > 163 Mbps)
		 * 	then divisor = 4
		 * if (rate_kbps <= 163 Mbps && rate_kbps > 40 Mbps)
		 * 	then divisor = 16
		 * if (rate_kbps <= 40 Mbps)
		 * 	then divisor = 64
		 */
		if (rate_kbps <= CS_TM_POL_RESOL64_MAX_RATE) {
			*p_freq_sel = CS_TM_POL_0_625_KHZ;
			div = CS_TM_POL_DIV_0_625_KHZ;
			
			if (rate_kbps < CS_TM_POL_RESOL64_MIN_RATE)
				printk("Set policing rate as %dkbps\n"
					"Warning: the valid min. rate is 5kbps"
					", your setting is the same as 0bps.\n",
					rate_kbps);
		} else if ((rate_kbps <= CS_TM_POL_RESOL16_MAX_RATE) &&
				(rate_kbps > CS_TM_POL_RESOL64_MAX_RATE)) {
			*p_freq_sel = CS_TM_POL_2_5_KHZ;
			div = CS_TM_POL_DIV_2_5_KHZ;
		} else if ((rate_kbps <= CS_TM_POL_RESOL4_MAX_RATE) &&
				(rate_kbps > CS_TM_POL_RESOL16_MAX_RATE)) {
			*p_freq_sel = CS_TM_POL_10_KHZ;
			div = CS_TM_POL_DIV_10_KHZ;
		} else if ((rate_kbps <= CS_TM_POL_RESOL1_MAX_RATE) &&
				(rate_kbps > CS_TM_POL_RESOL4_MAX_RATE)) {
			*p_freq_sel = CS_TM_POL_40_KHZ;
			div = CS_TM_POL_DIV_40_KHZ;
		} else {
			return -1;
		}
	} else {
		/*
		 * if (rate_kbps < 2.6Gbps && rate_kbps > 163 Mbps)
		 * 	then divisor = 1
		 * if (rate_kbps < 163 Mbps)
		 * 	then divisor 16
		 */
		if (rate_kbps <= CS_TM_POL_RESOL16_MAX_RATE) {
			*p_freq_sel = CS_TM_POL_2_5_KHZ;
			div = CS_TM_POL_DIV_2_5_KHZ;
			
			if (rate_kbps < CS_TM_POL_RESOL16_MIN_RATE)
				printk("Set policing rate as %dkbps\n"
					"Warning: the valid min. rate is 20kbps"
					", your setting is the same as 0bps.\n",
					rate_kbps);
		} else if ((rate_kbps <= CS_TM_POL_RESOL1_MAX_RATE) &&
				(rate_kbps > CS_TM_POL_RESOL16_MAX_RATE)) {
			*p_freq_sel = CS_TM_POL_40_KHZ;
			div = CS_TM_POL_DIV_40_KHZ;
		} else {
			return -1;
		}
	}
	*p_credit = (rate_kbps * div) / CS_TM_POL_MAX_CYCLE_RATE;
	return 0;
} /* cs_tm_pol_convert_rate_to_hw_value */

static cs_tm_pol_flow_pol_t *find_flow_policer(unsigned char id)
{
	cs_tm_pol_flow_pol_t *p_curr_flow_pol = flow_pol_db.head;

	while (p_curr_flow_pol != NULL) {
		if (p_curr_flow_pol->id == id)
			return p_curr_flow_pol;
		p_curr_flow_pol = p_curr_flow_pol->next;
	};

	return NULL;
} /* find_flow_policer */

static int insert_flow_pol(cs_tm_pol_flow_pol_t *p_flow_pol)
{
	cs_tm_pol_flow_pol_t *prev = NULL, *next = NULL;

	p_flow_pol->next = NULL;

	if (flow_pol_db.head == NULL) {
		flow_pol_db.head = p_flow_pol;
		flow_pol_db.mask[p_flow_pol->id >> 5] |= 0x01 << (p_flow_pol->id & 0x1f);
		flow_pol_db.used_count++;
		return 0;
	}

	next = flow_pol_db.head;
	while ((next != NULL) && (next->id < p_flow_pol->id)) {
		prev = next;
		next = next->next;
	};

	if ((next != NULL) && (next->id == p_flow_pol->id))
		return -1;

	if (next == NULL) {
		prev->next = p_flow_pol;
	} else if (prev == NULL) {
		p_flow_pol->next = flow_pol_db.head;
		flow_pol_db.head = p_flow_pol;
	} else {
		prev->next = p_flow_pol;
		p_flow_pol->next = next;
	}

	flow_pol_db.mask[p_flow_pol->id >> 5] |= 0x01 << (p_flow_pol->id & 0x1f);
	flow_pol_db.used_count++;

	return 0;
} /* insert_flow_pol */

static int delete_flow_pol(unsigned char id)
{
	cs_tm_pol_flow_pol_t *prev = NULL, *next = flow_pol_db.head;

	while ((next != NULL) && (next->id != id)) {
		prev = next;
		next = next->next;
	};

	if (next == NULL)
		return -1;

	if (prev == NULL)
		flow_pol_db.head = next->next;
	else
		prev->next = next->next;

	cs_free(next);

	flow_pol_db.mask[id >> 5] &= ~(0x01 << (id & 0x1f));
	flow_pol_db.used_count--;

	return 0;
} /* delete_flow_pol */

static int delete_flow_pol_force(unsigned char id)
{
	int status;
	status = delete_flow_pol(id);

	return status;
} /* delete_flow_pol_force */

int cs_tm_pol_get_avail_flow_policer(unsigned char *p_id)
{
	int iii = 0, jjj;

	if (p_id == NULL)
		return -1;
	if (flow_pol_db.used_count > CS_TM_POL_FLOW_POL_MAX)
		return -1;

	do {
		if (flow_pol_db.mask[iii] != 0xffffffff) {
			jjj = 0;
			do {
				if (!(flow_pol_db.mask[iii] & (0x01 << jjj))) {
					*p_id = ((iii << 5) + jjj);
					return 0;
				}
				jjj++;
			}
			while (jjj < 32);
		}
		iii++;
	} while (iii < 4);
	return -1;
} /* cs_tm_pol_get_avail_flow_policer */

int cs_tm_pol_get_flow_policer_used_count(unsigned char id,
		unsigned int *p_count)
{
	cs_tm_pol_flow_pol_t *p_flow_pol;

	if (p_count == NULL)
		return -1;

	p_flow_pol = find_flow_policer(id);
	if (p_flow_pol == NULL)
		return -1;

	*p_count = p_flow_pol->ref_count;

	return 0;
} /* cs_tm_pol_get_flow_policer_used_count */

int cs_tm_pol_inc_flow_policer_used_count(unsigned char id)
{
	cs_tm_pol_flow_pol_t *p_flow_pol;

	p_flow_pol = find_flow_policer(id);
	if (p_flow_pol == NULL)
		return -1;


	p_flow_pol->ref_count++;
	

	return 0;
} /* cs_tm_pol_inc_flow_policer_used_count */

static bool flow_pol_cmp(cs_tm_pol_flow_pol_t *polflow,
		cs_tm_pol_profile_mem_t *polprof)
{
	if ((polflow->policer_type == polprof->policer_type) &&
			(polflow->range == polprof->range) &&
			(polflow->cir_credit == polprof->cir_credit) &&
			(polflow->cir_max_credit == polprof->cir_max_credit) &&
			(polflow->pir_credit == polprof->pir_credit) &&
			(polflow->pir_max_credit == polprof->pir_max_credit) &&
			(polflow->bypass_yellow == polprof->bypass_yellow) &&
			(polflow->bypass_red == polprof->bypass_red))
		return true;
	else
		return false;
} /* flow_pol_cmp */

int cs_tm_pol_find_flow_policer(cs_tm_pol_profile_mem_t *p_pol_prof,
		unsigned char *p_id)
{
	cs_tm_pol_flow_pol_t *p_curr_flow = flow_pol_db.head;

	while (p_curr_flow != NULL) {
		if (flow_pol_cmp(p_curr_flow, p_pol_prof) == true) {
			*p_id = p_curr_flow->id;
			return 0;
		}
		p_curr_flow = p_curr_flow->next;
	}

	return -1;
} /* cs_tm_pol_find_flow_policer */

int cs_tm_pol_set_flow_policer(unsigned char id,
		cs_tm_pol_profile_mem_t *p_pol_profile)
{
	cs_tm_pol_flow_pol_t *p_flow_pol;
	int status;

	p_flow_pol = find_flow_policer(id);
	if (p_flow_pol == NULL) {
		p_flow_pol = cs_malloc(sizeof(cs_tm_pol_flow_pol_t), GFP_KERNEL);
		if (p_flow_pol == NULL)
			return -1;
		p_flow_pol->id = id;
		status = insert_flow_pol(p_flow_pol);
		if (status != 0) {
			cs_free(p_flow_pol);
			return status;
		}
		p_flow_pol->ref_count = 0;
	}

	p_flow_pol->policer_type = p_pol_profile->policer_type;
	p_flow_pol->range = p_pol_profile->range;
	p_flow_pol->cir_credit = p_pol_profile->cir_credit;
	p_flow_pol->cir_max_credit = p_pol_profile->cir_max_credit;
	p_flow_pol->pir_credit = p_pol_profile->pir_credit;
	p_flow_pol->pir_max_credit = p_pol_profile->pir_max_credit;
	p_flow_pol->bypass_yellow = p_pol_profile->bypass_yellow;
	p_flow_pol->bypass_red = p_pol_profile->bypass_red;

	status = cs_tm_pol_set_profile_mem(CS_TM_POL_FLOW_PROFILE_MEM, id,
			p_pol_profile);
	if (status != 0) {
		delete_flow_pol_force(id);
		return status;
	}

	return 0;
} /* cs_tm_pol_set_flow_policer */

int cs_tm_pol_del_flow_policer(unsigned char id)
{
	cs_tm_pol_flow_pol_t *p_flow_pol;

	p_flow_pol = find_flow_policer(id);
	if (p_flow_pol == NULL)
		return -1;

	p_flow_pol->ref_count--;

	if (p_flow_pol->ref_count == 0)
		delete_flow_pol(id);

	return 0;
} /* cs_tm_pol_del_flow_policer */

int cs_tm_pol_init(void)
{
	int status;
	cs_tm_pol_cfg_t pol_cfg;
	cs_tm_pol_profile_mem_t pol_profile;
	cs_tm_pol_cfg_spid_t spid_cfg;
	cs_tm_pol_cfg_flow_t flow_cfg;
	cs_tm_pol_cfg_cpu_t cpu_cfg;
	cs_tm_pol_cfg_pkt_type_t pkt_cfg;

	/*
	 * initialize the nested mode for all 4 types of policer */
	/* the nested scheme will be Source Port -> Flow;
	 * In the current software design CPU and PacketType are not used.
	 */
	memset(&spid_cfg, 0, sizeof(spid_cfg));
	spid_cfg.nest_level = 0;
	status = cs_tm_pol_set_spid_cfg(&spid_cfg);
	if (status != 0) {
		printk("%s::error setting up SPID Policer Config\n", __func__);
		return status;
	}

	memset(&flow_cfg, 0, sizeof(flow_cfg));
	flow_cfg.nest_level = 1;
	status = cs_tm_pol_set_flow_cfg(&flow_cfg);
	if (status != 0) {
		printk("%s::error setting up Flow Policer Config\n", __func__);
		return status;
	}

	memset(&cpu_cfg, 0, sizeof(cpu_cfg));
	cpu_cfg.disable = 1;
	status = cs_tm_pol_set_cpu_cfg(&cpu_cfg);
	if (status != 0) {
		printk("%s::error setting up CPU Policer Config\n", __func__);
		return status;
	}

	memset(&pkt_cfg, 0, sizeof(pkt_cfg));
	pkt_cfg.nest_level = 2;
	pkt_cfg.lspid0 = 0;
	pkt_cfg.lspid1 = 1;
	pkt_cfg.lspid2 = 2;
	status = cs_tm_pol_set_pkt_type_cfg(&pkt_cfg);
	if (status != 0) {
		printk("%s::error setting up Packet Type Policer Config\n", __func__);
		return status;
	}

	memset(&pol_cfg, 0, sizeof(pol_cfg));
	pol_cfg.pol_disable = 0;
	pol_cfg.pol_bypass_red = 0;
	pol_cfg.pol_bypass_yellow = 0;
	pol_cfg.pol_color_blind = 0;
	pol_cfg.init = 1;
	status = cs_tm_pol_set_cfg(&pol_cfg);
	if (status != 0)
		return status;

	pol_cfg.init = 0;
	status = cs_tm_pol_set_cfg(&pol_cfg);
	if (status != 0)
		return status;

	/* initialize flow policer database */
	memset(&flow_pol_db, 0, sizeof(flow_pol_db));

	/* initialize default flow policer */
	memset(&pol_profile, 0, sizeof(pol_profile));
	pol_profile.policer_type = CS_TM_POL_DISABLE;
	cs_tm_pol_set_flow_policer(CS_TM_POL_DEF_FLOW_POL_ID, &pol_profile);
	cs_tm_pol_inc_flow_policer_used_count(CS_TM_POL_DEF_FLOW_POL_ID);

	return 0;
}

