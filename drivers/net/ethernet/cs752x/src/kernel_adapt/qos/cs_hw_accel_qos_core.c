/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <wen.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * cs_hw_accel_qos_core.c
 *
 * $Id: cs_hw_accel_qos_core.c,v 1.20 2012/09/06 07:49:52 ewang Exp $
 *
 * This file contains the core implementation for CS QoS implementation.
 */
#include <linux/netdevice.h>
#include <linux/export.h>
#include <mach/cs75xx_fe_core_table.h>
#include "cs_core_logic.h"
#include "cs_core_hmu.h"
#include "cs_hw_accel_qos.h"
#include "cs_hw_accel_qos_data.h"
#include "cs75xx_tm.h"

static cs_qos_voq_param_t voq_darray[CS_QOS_PORT_NUM][CS_QOS_VOQ_PER_PORT];

extern u8 cs_ni_get_port_id(struct net_device *dev);

/* Internal APIs */
static inline u8 phy_port_id_to_tm_port_id(u8 phy_port_id)
{
	switch (phy_port_id) {
	case CS_SCH_ETH_PORT0:
	case CS_SCH_ETH_PORT1:
	case CS_SCH_ETH_PORT2:
		return phy_port_id;
	case CS_SCH_ETH_CRYPT:
		return ENCRYPTION_PORT;
	case CS_SCH_ETH_ENCAP:
		return ENCAPSULATION_PORT;
	case CS_SCH_ROOT_PORT:
		return MCAST_PORT;	/* or MIRROR_PORT? */
	default:
		if ((phy_port_id >= CS_SCH_CPU_PORT0) &&
				(phy_port_id <= CS_SCH_CPU_PORT7))
			return CPU_PORT;
		return 0xFF;
	};
} /* phy_port_id_to_tm_port_id */

static int cs_qos_check_queue_grouping(u8 voq_id,
		cs_qos_voq_type_e type, u8 priority)
{
	u8 port_idx, queue_idx, iii, curr_priority;

	port_idx = voq_id >> 3;
	queue_idx = voq_id & 0x07;

	/* if setting VOQ to SP mode, we have to check the following VOQ group
	 * conditions:
	 * 1) All the previous VOQs must be in SP mode.
	 * 2) The VOQ with highest priority is in the earliest position. meaning
	 * the priority of the previous VOQ must be larger than the current VOQ.
	 */
	if (type == CS_QOS_VOQ_TYPE_SP) {
		curr_priority = 0xff;
		for (iii = 0; iii < queue_idx; iii++) {
			if (voq_darray[port_idx][iii].type !=
					CS_QOS_VOQ_TYPE_SP)
				return -1;
			if ((iii == 0) && (curr_priority < voq_darray[
						port_idx][iii].priority))
				return -1;
			if ((iii != 0) && (curr_priority <= voq_darray[
						port_idx][iii].priority))
				return -1;
			curr_priority = voq_darray[port_idx][iii].priority;
		}
		/* if reach this point, we need to check whether it's ok to
		 * insert/update the priority of this SP VOQ here.  Conditions
		 * are:
		 * 1) the new priority is smaller than the previous VOQ's
		 * 	priority.
		 * 2) the new priority assigned is larger then the next queue if
		 * 	the next VOQ is running in SP mode. */
		if (priority >= curr_priority)
			return -1;

		if (queue_idx < (CS_QOS_VOQ_PER_PORT - 1)) {
			/* don't have to check if we are modifying the last
			 * VOQ */
			if (voq_darray[port_idx][queue_idx + 1].type ==
					CS_QOS_VOQ_TYPE_SP) {
				/* only perform the check when the next VOQ is
				 * also running in SP mode. */
				if (priority <= voq_darray[port_idx][queue_idx +
					       	1].priority)
					return -1;
			}
		}
	} else {	/* if (CS_QOS_VOQ_TYPE_DRR == type) */
		/* all we need to check is that the later VOQs are running in
		 * DRR mode. */
		for (iii = (queue_idx + 1); iii < CS_QOS_VOQ_PER_PORT; iii++) {
			if (voq_darray[port_idx][iii].type !=
					CS_QOS_VOQ_TYPE_DRR)
				return -1;
		}
	}

	/* if reach here.. everything has been checked ok. */
	return 0;
} /* cs_qos_check_queue_grouping */

/* this internal API is to find the lower rate in the DRR VOQ group that this
 * VOQ with given voq_id is belonged to. */
static int cs_qos_get_drr_group_rate(u8 voq_id, u32 new_rate, u32 *p_rslt_rate)
{
	u8 port_idx, queue_idx, iii;

	port_idx = voq_id >> 3;
	queue_idx = voq_id & 0x07;

	if (new_rate != 0)
		*p_rslt_rate = new_rate;
	else
		*p_rslt_rate = 0xffffffff;

	for (iii = 0; iii < CS_QOS_VOQ_PER_PORT; iii++) {
		if ((voq_darray[port_idx][iii].type == CS_QOS_VOQ_TYPE_DRR) &&
				(iii != queue_idx) &&
				(voq_darray[port_idx][iii].rate != 0) &&
				(voq_darray[port_idx][iii].rate < (*p_rslt_rate)))
			*p_rslt_rate = voq_darray[port_idx][iii].rate;
	}

	if (*p_rslt_rate == 0xffffffff)
		*p_rslt_rate = 0;

	return 0;
} /* cs_qos_drr_group_rate */

/* Module APIs */
void cs_qos_set_voq_id_to_skb_cs_cb(struct sk_buff *skb, struct net_device *dev,
		u8 queue_id)
{
	u8 port_id = cs_ni_get_port_id(dev);
	u8 qid = queue_id;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (cs_cb == NULL)
		return;
	if (qid >= 8)
		qid = 0;
	cs_cb->action.voq_pol.d_voq_id =
		CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(port_id, qid);
} /* cs_qos_set_voq_id_to_skb_cs_cb */

/* Port-based APIs */
/* This API is to set up port parameters. burst_size is in bytes.
 * rate is in bps.  Won't do anything if the value is 0. */
int cs_qos_set_port_param(u8 port_id, u16 burst_size, u32 rate)
{
	if (cs752x_sch_set_port_burst_size(port_id, burst_size) != 0)
		return -1;

	if (cs752x_sch_set_port_rate_lt(port_id, rate) != 0)
		return -1;

	return 0;
} /* cs_qos_set_port_param */

int cs_qos_set_port_pol_cfg(u8 port_id, u8 enbl,
		u8 bypass_yellow, u8 bypass_red, u32 rate,
		u32 cbs, u32 pbs)
{
	cs_tm_pol_profile_mem_t pol_profile;
	cs_tm_pol_freq_select_t freq_sel;
	u16 cir_credit;

	memset(&pol_profile, 0, sizeof(pol_profile));

	if (enbl == 1) {
		if (cs_tm_pol_convert_rate_to_hw_value(rate, FALSE, &freq_sel,
					&cir_credit) != 0)
			return -1;

		pol_profile.policer_type = CS_TM_POL_RFC_2697;
		pol_profile.range = freq_sel >> 1;
		pol_profile.cir_credit = cir_credit;
		pol_profile.cir_max_credit = cbs >> 7;
		pol_profile.pir_max_credit = pbs >> 7;
		pol_profile.bypass_yellow = bypass_yellow;
		pol_profile.bypass_red = bypass_red;
	} else {
		pol_profile.policer_type = CS_TM_POL_DISABLE;
	}

	if (cs_tm_pol_set_profile_mem(CS_TM_POL_SPID_PROFILE_MEM, port_id,
				&pol_profile) != 0)
		return -1;

	return 0;
} /* cs_qos_set_port_pol_cfg */

int cs_qos_set_port_global_buffer(u8 port_id, u16 min, u16 max)
{
	u8 tm_port_id = phy_port_id_to_tm_port_id(port_id);
	cs_tm_bm_dest_port_mem_t dest_port;

	if (tm_port_id == 0xff)
		return -1;

	dest_port.dest_port_min_global_buffers = min;
	dest_port.dest_port_max_global_buffers = max;

	return cs_tm_bm_set_dest_port_mem(tm_port_id, &dest_port);
} /* cs_qos_set_port_global_buffer */

int cs_qos_get_port_global_buffer(u8 port_id, u16 *min, u16 *max)
{
	u8 tm_port_id = phy_port_id_to_tm_port_id(port_id);
	cs_tm_bm_dest_port_mem_t dest_port;
	int ret;

	if (tm_port_id == 0xff)
		return -1;

	ret = cs_tm_bm_get_dest_port_mem(tm_port_id, &dest_port);
	if (unlikely(ret != 0))
		return ret;

	*min = dest_port.dest_port_min_global_buffers;
	*max = dest_port.dest_port_max_global_buffers;

	return ret;
} /* cs_qos_set_port_global_buffer */

int cs_qos_set_wred_mode(u8 wred_mode, u8 wred_adj_range_idx)
{
	cs_tm_bm_wred_cfg_t wred_cfg;

	if (cs_tm_bm_get_wred_cfg(&wred_cfg))
		return -1;

	wred_cfg.wred_mode = wred_mode;
	wred_cfg.wred_adjust_range_index = wred_adj_range_idx;

	return cs_tm_bm_set_wred_cfg(&wred_cfg);
} /* cs_qos_set_wred_mode */

int cs_qos_get_wred_mode(u8 *wred_mode, u8 *wred_adj_range_idx)
{
	cs_tm_bm_wred_cfg_t wred_cfg;

	if (cs_tm_bm_get_wred_cfg(&wred_cfg))
		return -1;

	if (wred_mode != NULL)
		*wred_mode = wred_cfg.wred_mode;

	if (wred_adj_range_idx != NULL)
		*wred_adj_range_idx = wred_cfg.wred_adjust_range_index;

	return 0;
} /* cs_qos_get_wred_mode */

int cs_qos_reset_port(u8 port_id)
{
	u8 tm_port_id = phy_port_id_to_tm_port_id(port_id);
	int iii;

	if (tm_port_id == 0xff)
		return -1;

	/* reset all the VOQs under this port first,
	 * need to go from last queue to first */
	for (iii = 7; iii >= 0; iii--)
		cs_qos_reset_voq(port_id, (port_id << 3) + iii);

	if (cs_tm_port_reset(tm_port_id) != 0)
		return -1;

	if (cs752x_sch_reset_port(port_id) != 0)
		return -1;

	return 0;
} /* cs_qos_reset_port */


/* VOQ-base APIs */
/* This API is to set up VoQ parameters, such as whether this VOQ is
 * running in SP mode or DRR mode.
 * Acceptable range for priority is 0 to 7.
 * VOQ will be running in SP mode if (priority != 0) || (weight == 0).
 * If (priority == 0) && (weight != 0), VOQ will be running in DRR more.
 * Rate is in bps. */
int cs_qos_set_voq_param(u8 port_id, u8 voq_id, u8 priority, u32 weight,
		u32 rsrv_size, u32 max_size, u32 rate,
		struct tc_wredspec *p_wred)
{
	if (cs_qos_set_voq_scheduler(port_id, voq_id, priority, weight, rate)
			!= 0)
		return -1;

	if (cs_tm_bm_set_voq_profile(voq_id, rsrv_size, max_size, p_wred) != 0)
		return -1;

	return 0;
} /* cs_qos_set_voq_param */

int cs_qos_set_voq_depth(u8 port_id, u8 voq_id, u16 min_depth, u16 max_depth)
{
	return cs_tm_bm_set_voq_profile(voq_id, min_depth, max_depth, NULL);
} /* cs_qos_set_voq_depth */

int cs_qos_set_voq_scheduler(u8 port_id, u8 voq_id, u8 priority, u32 weight,
		u32 rate)
{
	u8 port_idx, queue_idx;

	port_idx = ((voq_id >> 3) & 0x1f);
	queue_idx = (voq_id & 0x07);

	if (priority != 0) {
		/* turning this VOQ into SP VOQ */
		if (priority > CS_QOS_VOQ_PRIORITY_MAX)
			return -1;

		/* need to make sure all the SP and DRR queues are grouping
		 * properly. */
		if (cs_qos_check_queue_grouping(voq_id, CS_QOS_VOQ_TYPE_SP,
					priority) != 0)
			return -1;

		if (cs752x_sch_set_queue_sp(port_idx, queue_idx, rate) != 0)
			return -1;

		/* update SW values */
		voq_darray[port_idx][queue_idx].type = CS_QOS_VOQ_TYPE_SP;
		voq_darray[port_idx][queue_idx].priority = priority;
		/* well in SP mode, weight is not needed. */
		voq_darray[port_idx][queue_idx].weight = 0;
		voq_darray[port_idx][queue_idx].rate = rate;
	} else {
		u32 real_rate;

		/* need to make sure all the SP and DRR queues are grouping
		 * properly. */
		if (cs_qos_check_queue_grouping(voq_id, CS_QOS_VOQ_TYPE_DRR,
					0) != 0)
			return -1;

		/* in DRR case, the real rate is the lowest rate of all the
		 * DRR VOQ in the group. */
		if (cs_qos_get_drr_group_rate(voq_id, rate, &real_rate) != 0)
			return -1;

		if (cs752x_sch_set_queue_drr(port_idx, queue_idx, weight,
					real_rate) != 0)
			return -1;

		/* update SW values */
		voq_darray[port_idx][queue_idx].type = CS_QOS_VOQ_TYPE_DRR;
		/* == 0 */
		voq_darray[port_idx][queue_idx].priority = 0;
		voq_darray[port_idx][queue_idx].weight = weight;
		voq_darray[port_idx][queue_idx].rate = rate;
	}
	return 0;
} /* cs_qos_set_voq_scheduler */

int cs_qos_get_voq_scheduler(u8 port_id, u8 voq_id, u8 *priority, u32 *weight,
		u32 *rate)
{
	u8 port_idx, queue_idx;

	port_idx = ((voq_id >> 3) & 0x1f);
	queue_idx = (voq_id & 0x07);

	if (priority != NULL)
		*priority = voq_darray[port_idx][queue_idx].priority;
	if (weight != NULL)
		*weight = voq_darray[port_idx][queue_idx].weight;
	if (rate != NULL)
		*rate = voq_darray[port_idx][queue_idx].rate;
	return 0;
} /* cs_qos_get_voq_scheduler */

/* this API is used to retrieve the current depth value store in the
 * hardware table */
int cs_qos_get_voq_depth(u8 port_id, u8 voq_id, u16 *p_min_depth,
		u16 *p_max_depth)
{
	return cs_tm_bm_get_voq_depth(voq_id, p_min_depth, p_max_depth);
} /* cs_qos_get_voq_depth */

int cs_qos_reset_voq(u8 port_id, u8 voq_id)
{
	if (cs_tm_voq_reset(voq_id) != 0)
		return -1;
	if (cs752x_sch_reset_queue(port_id, (voq_id & 0x07)) != 0)
		return -1;
	return 0;
} /* cs_qos_reset_voq */

static inline void cs_qos_delete_by_pol_id(u8 pol_id)
{
	cs_core_hmu_value_t hmu_value;

	hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	hmu_value.mask = 0x08;
	hmu_value.value.swid64 = pol_id |
		CS_SWID64_MASK(CS_SWID64_MOD_ID_FLOW_POLICER);
	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64, &hmu_value);
	return;
}

void cs_qos_enbl_filter_policer(u8 *p_pol_id, u32 cir, u32 cbs, u32 pir,
		u32 pbs)
{
	cs_tm_pol_profile_mem_t pol_profile;
	int status;
	u8 f_get_new = 1, pol_id;

	memset(&pol_profile, 0, sizeof(pol_profile));

	pol_profile.bypass_yellow = 0;
	pol_profile.bypass_red = 0;
	pol_profile.pir_max_credit = pbs >> 7;
	/* it's in a unit of 128 bytes */
	pol_profile.cir_max_credit = cbs >> 7;
	/* it's in a unit of 128 bytes */

	/* if pir != 0, set up dual-rate.. if not, set up single-rate */
	if (pir != 0) {
		// FIXME right policer RFC rule?
		pol_profile.policer_type = CS_TM_POL_RFC_2698;
		if (cs_tm_pol_convert_rate_to_hw_value(pir, TRUE,
					&pol_profile.range,
					&pol_profile.pir_credit) != 0)
			return;
		if (cs_tm_pol_rate_divisor_to_credit(cir, pol_profile.range,
					&pol_profile.cir_credit) != 0)
			return;
	} else {
		pol_profile.policer_type = CS_TM_POL_RFC_2697;
		if (cs_tm_pol_convert_rate_to_hw_value(cir, TRUE,
					&pol_profile.range,
					&pol_profile.cir_credit) != 0)
			return;
		if (pol_profile.pir_max_credit == 0)
			pol_profile.pir_max_credit =
				pol_profile.cir_max_credit + 1;
	}

	if (*p_pol_id != 0) {
		u32 count;
		if (cs_tm_pol_get_flow_policer_used_count(*p_pol_id, &count)
				!= 0)
			return;

		if (count <= 1)
			f_get_new = 0;
	}

	if (f_get_new == 1) {
		status = cs_tm_pol_get_avail_flow_policer(&pol_id);
		if (status != 0) 
			return;
		if (*p_pol_id != 0)
			cs_tm_pol_del_flow_policer(*p_pol_id);
		*p_pol_id = pol_id;
	}

	/* at this point, we should've found an available flow policer that we
	 * can modify. It is either free or currently used by this filter
	 * policer. */
	status = cs_tm_pol_set_flow_policer(*p_pol_id, &pol_profile);
	if (status != 0)
		goto fail_delete_policer;

	if (f_get_new == 1)
		cs_tm_pol_inc_flow_policer_used_count(*p_pol_id);

	return;

fail_delete_policer:
	cs_tm_pol_del_flow_policer(*p_pol_id);
	*p_pol_id = 0;
	return;
} /* cs_qos_enbl_filter_policer */
EXPORT_SYMBOL(cs_qos_enbl_filter_policer);

void cs_qos_dsbl_filter_policer(u8 pol_id)
{
	cs_qos_delete_by_pol_id(pol_id);
	cs_tm_pol_del_flow_policer(pol_id);
} /* cs_qos_dsbl_filter_policre */
EXPORT_SYMBOL(cs_qos_dsbl_filter_policer);

#ifdef CONFIG_CS752X_HW_ACCELERATION
/* Jump table APIs */
void cs_qos_check_qos_fields(struct sk_buff *skb, int offset, u32 mask)
{
	cs_kernel_accel_cb_t *cs_cb;
	int org_offset;

	if (skb == NULL)
		return;

	cs_cb = CS_KERNEL_SKB_CB(skb);
	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	org_offset = (u32)skb_network_header(skb) - (u32)skb_mac_header(skb);
	/* DSCP field! */
	if ((offset == org_offset) && (mask & 0xff00))
		cs_cb->common.module_mask |= CS_MOD_MASK_WITH_QOS;

	// TODO: What is the offset for 8021p? Do we want to support double-tag?
} /* cs_qos_check_qos_fields */
EXPORT_SYMBOL(cs_qos_check_qos_fields);

void cs_qos_set_skb_sw_only(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (cs_cb == NULL)
		return;

	cs_cb->common.sw_only = CS_SWONLY_STATE;
} /* cs_qos_set_skb_sw_only */
EXPORT_SYMBOL(cs_qos_set_skb_sw_only);

#ifdef CONFIG_NET_ACT_POLICE
void cs_qos_set_skb_pol_id(struct sk_buff *skb, u8 pol_id)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u64 swid = 0;

	if (cs_cb == NULL)
		return;

	if (cs_cb->action.voq_pol.d_pol_id != 0) {
		/* check if there is a previously existing pol_id assigned on
		 * this skb/flow, if so, mark the skb sw-only and reassign the
		 * pol_id to 0 on this skb. */
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		cs_cb->action.voq_pol.d_pol_id = 0;

		return;
	}

	swid = pol_id | CS_SWID64_MASK(CS_SWID64_MOD_ID_FLOW_POLICER);
	cs_core_logic_add_swid64(cs_cb, swid);

	cs_cb->action.voq_pol.d_pol_id = pol_id;

	return;
} /* cs_qos_set_skb_pol_id */
EXPORT_SYMBOL(cs_qos_set_skb_pol_id);

#endif /* CONFIG_NET_ACT_POLICE */
#endif /* CONFIG_CS752X_HW_ACCELERATION */

int __init cs_qos_init(void)
{
	u8 iii, jjj;
#ifdef CONFIG_NET_ACT_POLICE
	cs_core_hmu_t flow_pol_hmu_entry;
	cs_core_hmu_value_t flow_pol_hmu_value;
#endif

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS 
    // change all port to Strict Priority by default except CPU Port 6
	for (iii = 0; iii < CS_QOS_PORT_NUM; iii++) {
		for (jjj = 0; jjj < CS_QOS_VOQ_PER_PORT; jjj++) {
			voq_darray[iii][jjj].type = CS_QOS_VOQ_TYPE_SP;
			voq_darray[iii][jjj].priority = (CS_QOS_VOQ_PER_PORT - 1 - iii);
			voq_darray[iii][jjj].rate = 0;
		}
	}
	// set CS_SCH_CPU_PORT6 to DRR
	for (jjj = 0; jjj < CS_QOS_VOQ_PER_PORT; jjj++) {
		voq_darray[CS_SCH_CPU_PORT6][jjj].type = CS_QOS_VOQ_TYPE_DRR;
		voq_darray[CS_SCH_CPU_PORT6][jjj].weight = CS_QOS_VOQ_DEFAULT_QUANTA;
		voq_darray[CS_SCH_CPU_PORT6][jjj].priority = 0;
		voq_darray[CS_SCH_CPU_PORT6][jjj].rate = 0;
	}
#else
	/* FIXME!! do we need to initialize all or some of the HW VOQs here? */
	for (iii = 0; iii < CS_QOS_PORT_NUM; iii++) {
		for (jjj = 0; jjj < CS_QOS_VOQ_PER_PORT; jjj++) {
			voq_darray[iii][jjj].type = CS_QOS_VOQ_TYPE_DRR;
			voq_darray[iii][jjj].weight = CS_QOS_VOQ_DEFAULT_QUANTA;
			voq_darray[iii][jjj].priority = 0;
			voq_darray[iii][jjj].rate = 0;
		}
	}
#endif //CS75XX_VOQ_REASSIGN

#ifdef CONFIG_NET_SCH_MULTIQ
	cs_qos_multiq_init();
#endif
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS 
	cs_offset_based_qos_init();
#else
	cs_qos_ingress_init();
#endif //CS75XX_VOQ_REASSIGN
	cs_qos_pkt_type_pol_init();

#ifdef CONFIG_NET_ACT_POLICE
	/* register HMU entry for flow policer */
	memset(&flow_pol_hmu_entry, 0, sizeof(flow_pol_hmu_entry));
	memset(&flow_pol_hmu_value, 0, sizeof(flow_pol_hmu_value));

	flow_pol_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	flow_pol_hmu_value.mask = 0x08;
	flow_pol_hmu_value.value.swid64 =
		CS_SWID64_MASK(CS_SWID64_MOD_ID_FLOW_POLICER);

	flow_pol_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;
	flow_pol_hmu_entry.value_mask = &flow_pol_hmu_value;
	flow_pol_hmu_entry.callback = NULL;

	cs_core_hmu_register_watch(&flow_pol_hmu_entry);
#endif

	return 0;
} /* cs_qos_init */

void __exit cs_qos_exit(void)
{
#ifdef CONFIG_NET_SCH_MULTIQ
	cs_qos_multiq_exit();
#endif
	cs_qos_ingress_exit();
	cs_qos_pkt_type_pol_exit();

	return;
} /* cs_qos_exit */

module_init(cs_qos_init);
module_exit(cs_qos_exit);

