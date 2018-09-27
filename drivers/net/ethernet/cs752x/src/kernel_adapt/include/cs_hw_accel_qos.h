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
 * cs_hw_accel_qos.h
 *
 * $Id: cs_hw_accel_qos.h,v 1.13 2012/09/05 22:59:18 whsu Exp $
 *
 * This header file defines the data structures and APIs for CS QoS.
 */

#ifndef __CS_HW_ACCEL_QOS_H__
#define __CS_HW_ACCEL_QOS_H__
#ifdef CONFIG_NET_SCHED
#include <net/sch_generic.h>
#endif
#include <mach/cs_types.h>
#include <net/pkt_sched.h>

int __init cs_qos_init(void);
void __exit cs_qos_exit(void);
void cs_qos_set_voq_id_to_skb_cs_cb(struct sk_buff *skb, struct net_device *dev,
		u8 queue_id);
/* Port-based APIs */
int cs_qos_set_port_param(u8 port_id, u16 burst_size, u32 rate);
int cs_qos_set_port_pol_cfg(u8 port_id, u8 enbl, u8 bypass_yellow,
		u8 bypass_red, u32 rate_bps, u32 cbs, u32 pbs);
int cs_qos_set_port_global_buffer(u8 port_id, u16 min, u16 max);
int cs_qos_get_port_global_buffer(u8 port_id, u16 *min, u16 *max);
int cs_qos_reset_port(u8 port_id);

/* WRED configuration APIs */
int cs_qos_set_wred_mode(u8 wred_mode, u8 wred_adj_range_idx);
int cs_qos_get_wred_mode(u8 *wred_mode, u8 *wred_adj_range_idx);

/* VOQ-based APIs */
int cs_qos_set_voq_param(u8 port_id, u8 voq_id, u8 priority, u32 weight,
		u32 rsrv_size, u32 max_size, u32 rate,
		struct tc_wredspec *p_wred);
int cs_qos_set_voq_depth(u8 port_id, u8 voq_id, u16 min_depth, u16 max_depth);
int cs_qos_get_voq_depth(u8 port_id, u8 voq_id, u16 *p_min_depth,
		u16 *p_max_depth);
int cs_qos_set_voq_scheduler(u8 port_id, u8 voq_id, u8 priority, u32 weight,
		u32 rate);
int cs_qos_get_voq_scheduler(u8 port_id, u8 voq_id, u8 *priority, u32 *weight,
		u32 *rate);
int cs_qos_reset_voq(u8 port_id, u8 voq_id);

void cs_qos_enbl_filter_policer(u8 *p_pol_id, u32 cir, u32 cbs, u32 pir, u32 pbs);
void cs_qos_dsbl_filter_policer(u8 pol_id);

#ifdef CONFIG_CS752X_HW_ACCELERATION
void cs_qos_set_skb_sw_only(struct sk_buff *skb);
void cs_qos_check_qos_fields(struct sk_buff *skb, int offset, u32 mask);

#ifdef CONFIG_NET_ACT_POLICE
void cs_qos_set_skb_pol_id(struct sk_buff *skb, u8 pol_id);
#endif
#endif /* CONFIG_CS752X_HW_ACCELERATION */

#ifdef CONFIG_NET_SCH_MULTIQ
int cs_qos_multiq_init(void);
void cs_qos_multiq_exit(void);
int cs_qos_check_and_steal_multiq_skb(struct Qdisc *qdisc, struct sk_buff *skb);
void cs_qos_set_multiq_attribute(struct Qdisc *qdisc, u16 burst_size,
		u32 rate_bps, u16 min_global_buffer, u16 max_global_buffer,
		u8 wred_mode, u8 wred_adj_range_idx);
int cs_qos_set_multisubq_attribute(struct Qdisc *qdisc, u8 band_id,
		u8 priority, u32 weight, u32 rsrv_size, u32 max_size,
		u32 rate_bps, void *p_wred);
int cs_qos_get_multisubq_depth(struct Qdisc *qdisc, u8 band_id,
		u16 *p_min_depth, u16 *p_max_depth);
void cs_qos_reset_multiq(struct Qdisc *qdisc);
void cs_qos_reset_multisubq(struct Qdisc *qdisc, u8 band_id);
#endif /* CONFIG_NET_SCH_MULTIQ */

int cs_qos_ingress_init(void);
void cs_qos_ingress_exit(void);
#ifdef CONFIG_NET_SCH_INGRESS
void cs_qos_set_pol_cfg_ingress_qdisc(struct Qdisc *qdisc, u8 enbl,
		u8 bypass_yellow, u8 bypass_red, u32 rate_bps, u32 cbs, u32 pbs);
#endif /* CONFIG_NET_SCH_INGRESS */

typedef enum {
	CS_QOS_INGRESS_MODE_DSCP,
	CS_QOS_INGRESS_MODE_8021P,
	CS_QOS_INGRESS_MODE_MAX
} cs_qos_ingress_mode_t;

int cs_qos_ingress_set_mode(u8 mode);
int cs_qos_ingress_get_mode(u8 *mode);
void cs_qos_ingress_print_mode(void);

typedef enum {
	CS_QOS_INGRESS_PORT_CPU0,
	CS_QOS_INGRESS_PORT_CPU1,
	CS_QOS_INGRESS_PORT_CPU2,
	CS_QOS_INGRESS_PORT_CPU3,
	CS_QOS_INGRESS_PORT_CPU4,
	CS_QOS_INGRESS_PORT_CPU5,
	CS_QOS_INGRESS_PORT_CPU6,
	CS_QOS_INGRESS_PORT_CPU7,
	CS_QOS_INGRESS_PORT_MAX,
} cs_qos_ingress_port_id_t;

int cs_qos_ingress_set_port_param(u8 port_id, u16 burst_size, u32 rate);
int cs_qos_ingress_get_port_param(u8 port_id, u16 *burst_size, u32 *rate);
void cs_qos_ingress_print_port_param(u8 port_id);

int cs_qos_ingress_set_queue_scheduler(u8 port_id, u8 queue_id, u8 priority,
		u32 weight, u32 rate);
int cs_qos_ingress_get_queue_scheduler(u8 port_id, u8 queue_id, u8 *priority,
		u32 *weight, u32 *rate);
void cs_qos_ingress_print_queue_scheduler(u8 port_id, u8 queue_id);
void cs_qos_ingress_print_queue_scheduler_of_port(u8 port_id);
int cs_qos_ingress_set_queue_size(u8 port_id, u8 queue_id, u32 rsrv_size,
		u32 max_size);
int cs_qos_ingress_get_queue_size(u8 port_id, u8 queue_id, u32 *rsrv_size,
		u32 *max_size);
void cs_qos_ingress_print_queue_size(u8 port_id, u8 queue_id);
void cs_qos_ingress_print_queue_size_of_port(u8 port_id);

int cs_qos_ingress_set_value_queue_mapping(u8 value, u8 queue_id);
int cs_qos_ingress_get_value_queue_mapping(u8 value, u8 *queue_id);
void cs_qos_ingress_print_value_queue_mapping(void);

int cs_qos_ingress_ioctl(struct net_device *dev, void *pdata, void *cmd);

int cs_qos_pkt_type_pol_init(void);
void cs_qos_pkt_type_pol_exit(void);
int cs_qos_set_pkt_type_pol(u8 port_id, u8 pkt_type, u32 cir, u32 cbs,
		u32 pir, u32 pbs);
int cs_qos_reset_pkt_type_pol(u8 port_id, u8 pkt_type);
int cs_qos_get_pkt_type_pol(u8 port_id, u8 pkt_type, u32 *cir, u32 *cbs,
		u32 *pir, u32 *pbs);
void cs_qos_print_pkt_type_pol(u8 port_id, u8 pkt_type);
void cs_qos_print_pkt_type_pol_per_port(u8 port_id);
void cs_qos_print_all_pkt_type_pol(void);

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS 
int cs_offset_based_qos_init(void);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS 

#endif /* __CS_HW_ACCEL_QOS_H__ */
