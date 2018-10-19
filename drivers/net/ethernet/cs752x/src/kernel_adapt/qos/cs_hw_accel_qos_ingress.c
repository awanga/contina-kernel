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
 * cs_hw_accel_qos_ingress.c
 *
 * $Id$
 *
 * This file contains the implementation for CS QoS implementation
 * for ingress traffic.
 */
#include "cs_fe.h"
#include <linux/cs_ne_ioctl.h>
#include "cs_hw_accel_qos.h"
#include "cs_hw_accel_qos_data.h"
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_core_hmu.h"
#include "cs75xx_tm.h"
#include "cs_mut.h"

static int ingress_mode = CS_QOS_INGRESS_MODE_MAX;
static unsigned char mode_name[CS_QOS_INGRESS_MODE_MAX + 1][8] = {
	{"DSCP"}, {"8021P"}, {"NONE"}};

typedef struct {
	u16 burst_size;
	u32 rate;
} cs_qos_ingress_port_cfg_t;

static cs_qos_ingress_port_cfg_t ingress_cpu_port_cfg[CS_QOS_INGRESS_PORT_MAX];

unsigned int ingress_qos_hm_idx = ~(0x0);

u16 *ingress_hash_table = NULL;
u8 *queue_mapping_table = NULL;
u8 table_max = 0;

typedef struct {
	u32 cir, cbs, pir, pbs;
} cs_qos_arp_policer_t;
static cs_qos_arp_policer_t cs_qos_arp_policer_db;

#ifdef CONFIG_NET_SCH_INGRESS
extern u8 cs_ni_get_port_id(struct net_device *dev);

void cs_qos_set_pol_cfg_ingress_qdisc(struct Qdisc *qdisc, u8 enbl,
		u8 bypass_yellow, u8 bypass_red, u32 rate_bps,
		u32 cbs, u32 pbs)
{
	struct net_device *dev;
	u8 port_id;

	dev = qdisc_dev(qdisc);
	if (dev == NULL)
		return;

	port_id = cs_ni_get_port_id(dev);

	cs_qos_set_port_pol_cfg(port_id, enbl, bypass_yellow, bypass_red,
			rate_bps, cbs, pbs);

	return;
} /* cs_qos_set_pol_cfg_ingress_qdisc */
EXPORT_SYMBOL(cs_qos_set_pol_cfg_ingress_qdisc);
#endif /* CONFIG_NET_SCH_INGRESS */

static int cs_qos_delete_qos_hash(u16 hash_idx)
{
	u32 crc32;
	u16 crc16, rslt_idx;
	u8 mask_ptr;
	int ret;

	ret = cs_fe_hash_get_hash(hash_idx, &crc32, &crc16, &mask_ptr,
			&rslt_idx);
	if (ret != 0)
		return ret;

	/* del hash at first to avoid junk pkt */
	ret = cs_fe_hash_del_hash(hash_idx);
	
	cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT, (unsigned int)rslt_idx,
			false);

	return ret;
} /* cs_qos_delete_qos_hash */

int cs_qos_ingress_set_mode(u8 mode)
{
	fe_hash_mask_entry_t hash_mask;
	fe_qos_result_entry_t qos_rslt;
	fe_sw_hash_t key;
	unsigned int bc_vtable_hm_idx = ~0x0, mc_vtable_hm_idx = ~0x0;
	unsigned int l2_vtable_hm_idx = ~0x0, l3_vtable_hm_idx = ~0x0;
	unsigned int qosrslt_idx;
	u32 crc32;
	u16 crc16;
	int ret, i;

	/* Setting up to the same mode.. no need to do anything */
	if (ingress_mode == mode)
		return 0;

	/* if ingress mode was set to certain mode, remove the old one */
	if (ingress_mode != CS_QOS_INGRESS_MODE_MAX) {
		/* delete all the hashes created */
		if (ingress_hash_table != NULL) {
			for (i = 0; i < table_max; i++)
				cs_qos_delete_qos_hash(ingress_hash_table[i]);
			cs_free(ingress_hash_table);
			ingress_hash_table = NULL;
			table_max = 0;
		}

		if (queue_mapping_table != NULL) {
			cs_free(queue_mapping_table);
			queue_mapping_table = NULL;
		}

		if (ingress_qos_hm_idx != ~0x0) {
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_BCAST,
					ingress_qos_hm_idx, true);
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L2_MCAST,
					ingress_qos_hm_idx, true);
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L3_MCAST_V4,
					ingress_qos_hm_idx, true);
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L3_MCAST_V6,
					ingress_qos_hm_idx, true);
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L2_FLOW,
					ingress_qos_hm_idx, true);
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L3_FLOW,
					ingress_qos_hm_idx, true);
#ifdef CS75XX_HW_ACCEL_TUNNEL
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L3_TUNNEL,
					ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS	
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L2_IPSEC,
					ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_PASS	
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_L2_L2TP,
					ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS,
					ingress_qos_hm_idx, true);
			cs_core_vtable_del_hashmask_by_idx(
					CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS,
					ingress_qos_hm_idx, true);
#endif
			ingress_qos_hm_idx = ~0x0;
		}
	}

	/* allocate the HashMask into the vtable that we care */
	memset(&hash_mask, 0xff, sizeof(fe_hash_mask_entry_t));
	hash_mask.keygen_poly_sel = 0;
	hash_mask.ip_sa_mask = 0;
	hash_mask.ip_da_mask = 0;
	switch (mode) {
	case CS_QOS_INGRESS_MODE_DSCP:
		hash_mask.dscp_mask = 0;
		table_max = 64;
		break;
	case CS_QOS_INGRESS_MODE_8021P:
		hash_mask.tpid_enc_1_msb_mask = 0;
		hash_mask._8021p_1_mask = 0;
		table_max = 8;
		break;
	default:
		ingress_mode = CS_QOS_INGRESS_MODE_MAX;
		return 0;
	}
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_BCAST, &hash_mask,
			2, true, &bc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_MCAST, &hash_mask,
			2, true, &mc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_MCAST_V4, &hash_mask,
			2, true, &mc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_MCAST_V6, &hash_mask,
			2, true, &mc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_FLOW, &hash_mask,
			2, true, &l2_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_FLOW, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#ifdef CS75XX_HW_ACCEL_TUNNEL
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_TUNNEL, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS	
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_IPSEC, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_PASS	
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_L2TP, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif

	/* supposedly all the hash mask indice should be the same */
	if ((bc_vtable_hm_idx != mc_vtable_hm_idx) &&
			(l2_vtable_hm_idx != l3_vtable_hm_idx) &&
			(l2_vtable_hm_idx != bc_vtable_hm_idx)) {
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_BCAST,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_MCAST,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_MCAST_V4,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_MCAST_V6,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_FLOW,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_FLOW,
				ingress_qos_hm_idx, true);
#ifdef CS75XX_HW_ACCEL_TUNNEL
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_TUNNEL,
				ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS	
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_IPSEC,
				ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_PASS	
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_L2TP,
				ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS,
				ingress_qos_hm_idx, true);
#endif
		table_max = 0;
		return -1;
	}
	ingress_qos_hm_idx = bc_vtable_hm_idx;

	/* create all the hashes */
	ingress_hash_table = cs_zalloc(table_max * 2, GFP_KERNEL);
	queue_mapping_table = cs_malloc(table_max, GFP_KERNEL);
	if ((ingress_hash_table == NULL) || (queue_mapping_table == NULL))
		return -1;

	memset(queue_mapping_table, 0x7, table_max);

	/* first.. get the default qosrslt_idx */
	memset(&qos_rslt, 0x0, sizeof(fe_qos_result_entry_t));
	qos_rslt.voq_cos = 7;
	ret = cs_fe_table_add_entry(FE_TABLE_QOSRSLT, &qos_rslt, &qosrslt_idx);
	if (ret != 0) {
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_BCAST,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_MCAST,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_MCAST_V4,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_MCAST_V6,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_FLOW,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_FLOW,
				ingress_qos_hm_idx, true);
#ifdef CS75XX_HW_ACCEL_TUNNEL
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L3_TUNNEL,
				ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS	
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_IPSEC,
				ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_PASS	
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_L2_L2TP,
				ingress_qos_hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS,
				ingress_qos_hm_idx, true);
		cs_core_vtable_del_hashmask_by_idx(
				CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS,
				ingress_qos_hm_idx, true);
#endif
		ingress_qos_hm_idx = ~0x0;
		cs_free(ingress_hash_table);
		cs_free(queue_mapping_table);
		return -1;
	}

	/* now.. create the hashes!! */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.mask_ptr_0_7 = ingress_qos_hm_idx;
	for (i = 0; i < table_max; i++) {
		switch (mode) {
		case CS_QOS_INGRESS_MODE_DSCP:
			key.dscp = i;
			break;
		case CS_QOS_INGRESS_MODE_8021P:
			/* encoded TPID =
			 * 4:0x8100
			 * 5:0x9100
			 * 6:0x88a8
			 * 7:0x9200
			 */
			key.tpid_enc_1 = 4;
			key._8021p_1 = i;
			break;
		}
		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0)
			return ret;

		ret = cs_fe_hash_add_hash(crc32, crc16, ingress_qos_hm_idx,
				qosrslt_idx, &ingress_hash_table[i]);
		if (ret != 0)
			return ret;

		if (likely(i > 0))
			cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
					qosrslt_idx);
#if 0		/* termporarily remove the engagement from core HMU table */
		ret = cs_core_hmu_add_hash(ingress_hash_table[i], 0);
		if (ret != 0)
			return ret;
#endif
	}

	ingress_mode = mode;
	return 0;
}

int cs_qos_ingress_get_mode(u8 *mode)
{
	if (mode == NULL)
		return -1;
	*mode = ingress_mode;
	return 0;
}

void cs_qos_ingress_print_mode(void)
{
	printk("Current Ingress QoS mode is on %s\n", mode_name[ingress_mode]);
}

/* port_id is CPU Port ID,
 * burst size in unit of Byte
 * rate is in unit of bps */
int cs_qos_ingress_set_port_param(u8 port_id, u16 burst_size, u32 rate)
{
	if (cs_qos_set_port_param(port_id + CS_SCH_CPU_PORT0, burst_size,
				rate) == 0) {
		ingress_cpu_port_cfg[port_id].burst_size = burst_size;
		ingress_cpu_port_cfg[port_id].rate = rate;
		return 0;
	}
	return -1;
}

int cs_qos_ingress_get_port_param(u8 port_id, u16 *burst_size, u32 *rate)
{
	if (port_id >= CS_QOS_INGRESS_PORT_MAX)
		return -1;
	if (burst_size != NULL)
		*burst_size = ingress_cpu_port_cfg[port_id].burst_size;
	if (rate != NULL)
		*rate = ingress_cpu_port_cfg[port_id].rate;
	return 0;
}

void cs_qos_ingress_print_port_param(u8 port_id)
{
	if (port_id >= CS_QOS_INGRESS_PORT_MAX) {
		printk("Invalid Port ID %d\n", port_id);
		return;
	}
	printk("CPU Port ID: %d\n\tBurst_size = %d bytes, Rate = %d bps\n",
			port_id, ingress_cpu_port_cfg[port_id].burst_size,
			ingress_cpu_port_cfg[port_id].rate);
}

int cs_qos_ingress_set_queue_scheduler(u8 port_id, u8 queue_id, u8 priority,
		u32 weight, u32 rate)
{
	return cs_qos_set_voq_scheduler(port_id,
			CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(
				port_id + CS_SCH_CPU_PORT0, queue_id),
			priority, weight, rate);
}

int cs_qos_ingress_get_queue_scheduler(u8 port_id, u8 queue_id, u8 *priority,
		u32 *weight, u32 *rate)
{
	return cs_qos_get_voq_scheduler(port_id,
			CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(
				port_id + CS_SCH_CPU_PORT0, queue_id),
			priority, weight, rate);
}

void cs_qos_ingress_print_queue_scheduler(u8 port_id, u8 queue_id)
{
	u8 priority;
	u32 weight, rate;
	int ret;

	ret = cs_qos_ingress_get_queue_scheduler(port_id, queue_id,
			&priority, &weight, &rate);
	printk("port_id = %d, queue_id = %d: ", port_id, queue_id);
	if (ret == 0)
		printk("priority = %d, weight = %d, rate = %d\n\n",
				priority, weight, rate);
	else
		printk("Unable to obtain queue size info!\n\n");
}

void cs_qos_ingress_print_queue_scheduler_of_port(u8 port_id)
{
	u8 priority;
	u32 weight, rate;
	int ret, i;

	if (port_id >= CS_QOS_INGRESS_PORT_MAX) {
		printk("Invalid Port ID %d\n", port_id);
		return;
	}

	printk("|  Port ID  |  Queue ID  |  Priority  |  Weight  |  Rate  |\n");
	for (i = 0; i < 8; i++) {
		printk("  %07d  |  %08d  |  ", port_id, i);
		ret = cs_qos_ingress_get_queue_scheduler(port_id, i,
				&priority, &weight, &rate);
		if (ret == 0)
			printk("%08d  |  %06d  |  %04d  |\n", priority, weight,
					rate);
		else
			printk("  N/A     |   N/A    |  N/A   |\n");
	}
	printk("\n");
}

int cs_qos_ingress_set_queue_size(u8 port_id, u8 queue_id, u32 rsrv_size,
		u32 max_size)
{
#define CS_QOS_MAX_SIZE_OF_EXT_VOQ	65535

	if (port_id >= CS_QOS_INGRESS_PORT_MAX || queue_id >= 8)
		return -EINVAL;
	if ((max_size > CS_QOS_MAX_SIZE_OF_EXT_VOQ) || (max_size <= rsrv_size)) {
		printk("Need to assert %d >= max_size (%d) > rsrv_size (%d)\n", 
			CS_QOS_MAX_SIZE_OF_EXT_VOQ, max_size, rsrv_size);
		return -EINVAL;
	}
	return cs_qos_set_voq_depth(port_id, CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(
				port_id + CS_SCH_CPU_PORT0, queue_id),
			rsrv_size, max_size);
}

int cs_qos_ingress_get_queue_size(u8 port_id, u8 queue_id, u32 *rsrv_size,
		u32 *max_size)
{
	u16 min_depth, max_depth;
	int ret;

	if (port_id >= CS_QOS_INGRESS_PORT_MAX || queue_id >= 8)
		return -1;

	ret = cs_qos_get_voq_depth(port_id,
			CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(
				port_id + CS_SCH_CPU_PORT0, queue_id),
			&min_depth, &max_depth);
	if (rsrv_size != NULL)
		*rsrv_size = min_depth;
	if (max_size != NULL)
		*max_size = max_depth;
	return ret;
}

void cs_qos_ingress_print_queue_size(u8 port_id, u8 queue_id)
{
	u32 rsrv_size, max_size;
	int ret;

	if (port_id >= CS_QOS_INGRESS_PORT_MAX || queue_id >= 8) {
		printk("Invalid Port ID %d, Queue ID %d\n", port_id, queue_id);
		return;
	}
	ret = cs_qos_ingress_get_queue_size(port_id, queue_id, &rsrv_size,
			&max_size);
	printk("port_id = %d, queue_id = %d: ", port_id, queue_id);
	if (ret == 0)
		printk("reserve size = %d, maximum size = %d\n\n", rsrv_size,
				max_size);
	else
		printk("Unable to obtain queue size info!\n\n");
}

void cs_qos_ingress_print_queue_size_of_port(u8 port_id)
{
	u32 rsrv_size, max_size;
	int i, ret;

	if (port_id >= CS_QOS_INGRESS_PORT_MAX) {
		printk("Invalid Port ID %d\n", port_id);
		return;
	}

	printk("|  Port ID  |  Queue ID  |  Reserve Size  |  Max Size  |\n");
	for (i = 0; i < 8; i++) {
		printk("  %07d  |  %08d  |  ", port_id, i);
		ret = cs_qos_ingress_get_queue_size(port_id, i, &rsrv_size,
				&max_size);
		if (ret == 0)
			printk("%12d  |  %8d  |\n", rsrv_size, max_size);
		else
			printk("    N/A       |    N/A     |\n");
	}
	printk("\n");
}

int cs_qos_ingress_set_value_queue_mapping(u8 value, u8 queue_id)
{
	u32 crc32;
	u16 crc16, rslt_idx;
	u8 mask_ptr;
	fe_qos_result_entry_t qos_rslt;
	unsigned int qosrslt_idx;
	int ret;

	if ((value >= table_max) || (queue_id >= 8) ||
			(queue_mapping_table == NULL) ||
			(ingress_hash_table == NULL))
		return -1;

	if (queue_mapping_table[value] == queue_id)
		return 0;

	ret = cs_fe_hash_get_hash(ingress_hash_table[value], &crc32, &crc16,
			&mask_ptr, &rslt_idx);
	if (ret != 0)
		return ret;

	memset(&qos_rslt, 0x0, sizeof(fe_qos_result_entry_t));
	qos_rslt.voq_cos = queue_id;
	ret = cs_fe_table_add_entry(FE_TABLE_QOSRSLT, &qos_rslt, &qosrslt_idx);
	if (ret != 0)
		return ret;

	ret = cs_fe_hash_update_hash(ingress_hash_table[value], crc32, crc16,
			mask_ptr, (u16)qosrslt_idx);
	if (ret != 0)
		return ret;

	cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT, (unsigned int)rslt_idx,
			false);
	queue_mapping_table[value] = queue_id;

	return 0;
}

int cs_qos_ingress_get_value_queue_mapping(u8 value, u8 *queue_id)
{
	if ((queue_id == NULL) || (queue_mapping_table == NULL))
		return -1;

	if (value >= table_max)
		return -1;

	*queue_id = queue_mapping_table[value];
	return 0;
}

void cs_qos_ingress_print_value_queue_mapping(void)
{
	int i;

	printk("Ingress Mode: %s\n", mode_name[ingress_mode]);
	printk("\t|  value  |  Queue  |\n");
	for (i = 0; i < table_max; i++) {
		printk("\t|  %5d  |  %5d  |\n", i, queue_mapping_table[i]);
		if ((i & 0x7) == 0x7)
			printk("---\n");
	}
	printk("\n\n");
}

/* Policer for ARP vtable */
int cs_qos_set_arp_policer(u32 cir, u32 cbs, u32 pir, u32 pbs)
{
	cs_vtable_t *table;
	unsigned int ref_cnt, status, new_fwdrslt_idx = 0, voqpol_idx = 0;
	fe_fwd_result_entry_t fwdrslt_entry;
	fe_voq_pol_entry_t voqpol_entry;
	fe_sdb_entry_t sdb_entry;

	table = cs_core_vtable_get(CORE_VTABLE_TYPE_ARP);
	if (table == NULL)
		return -1;

	/* we know at this point, all the UU, UM, BC of SDB all points to the
	 * same fwd_rslt */
	status = cs_fe_table_get_entry_refcnt(FE_TABLE_FWDRSLT,
			table->uuflow_idx, &ref_cnt);
	if (status)
		return status;
	status = cs_fe_table_get_entry(FE_TABLE_FWDRSLT, table->uuflow_idx,
			&fwdrslt_entry);
	if (status)
		return status;

	status = cs_fe_table_get_entry(FE_TABLE_VOQ_POLICER,
			fwdrslt_entry.dest.voq_pol_table_index,
			&voqpol_entry);
	if (status)
		return status;

	/* delete the old one first */
	if (voqpol_entry.pol_base != 0)
		cs_qos_dsbl_filter_policer(voqpol_entry.pol_base);
	voqpol_entry.pol_base = 0;

	/* create the policer based on the value given */
	if ((cir != 0) && (cbs != 0) && (pbs != 0))	/* pir can be 0 */
		cs_qos_enbl_filter_policer(&voqpol_entry.pol_base, cir, cbs,
				pir, pbs);

	if (ref_cnt > 1) {
		/* create a new voqpol entry */
		status = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER,
				&voqpol_entry, &voqpol_idx);
		if (status) 
			goto fail_delete_policer;
		fwdrslt_entry.dest.voq_pol_table_index = voqpol_idx;

		/* create a new fwdrslt entry */
		fwdrslt_entry.dest.voq_policy = 1;
		status = cs_fe_table_add_entry(FE_TABLE_FWDRSLT,
				&fwdrslt_entry, &new_fwdrslt_idx);
		if (status) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
					fwdrslt_entry.dest.voq_pol_table_index,
					false);
			goto fail_delete_policer;
		}

		/* update the SDB entry */
		status = cs_fe_table_get_entry(FE_TABLE_SDB, table->sdb_index,
				&sdb_entry);
		if (status)
			goto fail_delete_fwdrslt;

		sdb_entry.misc.uu_flowidx = new_fwdrslt_idx;
		sdb_entry.misc.um_flowidx = new_fwdrslt_idx;
		sdb_entry.misc.bc_flowidx = new_fwdrslt_idx;

		status = cs_fe_table_set_entry(FE_TABLE_SDB, table->sdb_index,
				&sdb_entry);
		if (status)
			goto fail_delete_fwdrslt;

		/* decrement the ref_cnt to the original one */
		cs_fe_fwdrslt_del_by_idx(table->uuflow_idx);

		/* update the vtable info */
		table->uuflow_idx = new_fwdrslt_idx;
		table->umflow_idx = new_fwdrslt_idx;
		table->bcflow_idx = new_fwdrslt_idx;
	} else {
		/* set the voqpol entry again */
		status = cs_fe_table_set_entry(FE_TABLE_VOQ_POLICER,
				fwdrslt_entry.dest.voq_pol_table_index,
				&voqpol_entry);
		if (status)
			return status;
	}

	cs_qos_arp_policer_db.cir = cir;
	cs_qos_arp_policer_db.cbs = cbs;
	cs_qos_arp_policer_db.pir = pir;
	cs_qos_arp_policer_db.pbs = pbs;

	return 0;

fail_delete_fwdrslt:
	cs_fe_fwdrslt_del_by_idx(new_fwdrslt_idx);

fail_delete_policer:
	cs_tm_pol_del_flow_policer(voqpol_entry.pol_base);

	return status;
} /* cs_qos_set_arp_policer */

int cs_qos_reset_arp_policer(void)
{
	cs_qos_arp_policer_db.cir = cs_qos_arp_policer_db.cbs = 0;
	cs_qos_arp_policer_db.pir = cs_qos_arp_policer_db.pbs = 0;

	return cs_qos_set_arp_policer(0, 0, 0, 0);
} /* cs_qos_reset_arp_policer */

int cs_qos_get_arp_policer(u32 *cir, u32 *cbs, u32 *pir, u32 *pbs)
{
	if ((cir == NULL) || (cbs == NULL) || (pir == NULL) || (pbs == NULL))
		return -1;

	*cir = cs_qos_arp_policer_db.cir;
	*cbs = cs_qos_arp_policer_db.cbs;
	*pir = cs_qos_arp_policer_db.pir;
	*pbs = cs_qos_arp_policer_db.pbs;

	return 0;
} /* cs_qos_get_arp_policer */

void cs_qos_print_arp_policer(void)
{
	printk("ARP policer: cir = %d, cbs = %d, pir = %d, pbs = %d\n",
			cs_qos_arp_policer_db.cir, cs_qos_arp_policer_db.cbs,
			cs_qos_arp_policer_db.pir, cs_qos_arp_policer_db.pbs);
} /* cs_qos_print_arp_policer */

int cs_qos_ingress_ioctl(struct net_device *dev, void *pdata, void *cmd)
{
	cs_qos_ingress_api_entry_t *entry = (cs_qos_ingress_api_entry_t *)pdata;
	//NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;

	switch (entry->sub_cmd) {
	case CS_QOS_INGRESS_SET_MODE:
		entry->ret = cs_qos_ingress_set_mode(entry->mode.mode);
		break;
	case CS_QOS_INGRESS_GET_MODE:
		entry->ret = cs_qos_ingress_get_mode(&entry->mode.mode);
		break;
	case CS_QOS_INGRESS_PRINT_MODE:
		cs_qos_ingress_print_mode();
		entry->ret = 0;
		break;

	case CS_QOS_INGRESS_SET_PORT_PARAM:
		entry->ret = cs_qos_ingress_set_port_param(
			entry->port_param.port_id,
			entry->port_param.burst_size,
			entry->port_param.rate);
		break;
	case CS_QOS_INGRESS_GET_PORT_PARAM:
		entry->ret = cs_qos_ingress_get_port_param(
			entry->port_param.port_id,
			&entry->port_param.burst_size,
			&entry->port_param.rate);
		break;
	case CS_QOS_INGRESS_PRINT_PORT_PARAM:
		cs_qos_ingress_print_port_param(entry->port_param.port_id);
		entry->ret = 0;
		break;

	case CS_QOS_INGRESS_SET_QUEUE_SCHEDULER:
		entry->ret = cs_qos_ingress_set_queue_scheduler(
			entry->queue_scheduler.port_id,
			entry->queue_scheduler.queue_id,
			entry->queue_scheduler.priority,
			entry->queue_scheduler.weight,
			entry->queue_scheduler.rate);
		break;
	case CS_QOS_INGRESS_GET_QUEUE_SCHEDULER:
		entry->ret = cs_qos_ingress_get_queue_scheduler(
			entry->queue_scheduler.port_id,
			entry->queue_scheduler.queue_id,
			&entry->queue_scheduler.priority,
			&entry->queue_scheduler.weight,
			&entry->queue_scheduler.rate);
		break;
	case CS_QOS_INGRESS_PRINT_QUEUE_SCHEDULER:
		cs_qos_ingress_print_queue_scheduler(
			entry->queue_scheduler.port_id,
			entry->queue_scheduler.queue_id);
		entry->ret = 0;
		break;
	case CS_QOS_INGRESS_PRINT_QUEUE_SCHEDULER_OF_PORT:
		cs_qos_ingress_print_queue_scheduler_of_port(
			entry->queue_scheduler.port_id);
		entry->ret = 0;
		break;

	case CS_QOS_INGRESS_SET_QUEUE_SIZE:
		entry->ret = cs_qos_ingress_set_queue_size(
			entry->queue_size.port_id,
			entry->queue_size.queue_id,
			entry->queue_size.rsrv_size,
			entry->queue_size.max_size);
		break;
	case CS_QOS_INGRESS_GET_QUEUE_SIZE:
		entry->ret = cs_qos_ingress_get_queue_size(
			entry->queue_size.port_id,
			entry->queue_size.queue_id,
			&entry->queue_size.rsrv_size,
			&entry->queue_size.max_size);
		break;
	case CS_QOS_INGRESS_PRINT_QUEUE_SIZE:
		cs_qos_ingress_print_queue_size(
			entry->queue_size.port_id,
			entry->queue_size.queue_id);
		entry->ret = 0;
		break;
	case CS_QOS_INGRESS_PRINT_QUEUE_SIZE_OF_PORT:
		cs_qos_ingress_print_queue_size_of_port(
			entry->queue_size.port_id);
		entry->ret = 0;
		break;

	case CS_QOS_INGRESS_SET_VALUE_QUEUE_MAPPING:
		entry->ret = cs_qos_ingress_set_value_queue_mapping(
			entry->queue_mapping.value,
			entry->queue_mapping.queue_id);
		break;
	case CS_QOS_INGRESS_GET_VALUE_QUEUE_MAPPING:
		entry->ret = cs_qos_ingress_get_value_queue_mapping(
			entry->queue_mapping.value,
			&entry->queue_mapping.queue_id);
		break;
	case CS_QOS_INGRESS_PRINT_VALUE_QUEUE_MAPPING:
		cs_qos_ingress_print_value_queue_mapping();
		entry->ret = 0;
		break;

	case CS_QOS_INGRESS_SET_ARP_POLICER:
		entry->ret = cs_qos_set_arp_policer(entry->arp_policer.cir,
				entry->arp_policer.cbs,
				entry->arp_policer.pir,
				entry->arp_policer.pbs);
		break;
	case CS_QOS_INGRESS_RESET_ARP_POLICER:
		entry->ret = cs_qos_reset_arp_policer();
		break;
	case CS_QOS_INGRESS_GET_ARP_POLICER:
		entry->ret = cs_qos_get_arp_policer(&entry->arp_policer.cir,
				&entry->arp_policer.cbs,
				&entry->arp_policer.pir,
				&entry->arp_policer.pbs);
		break;
	case CS_QOS_INGRESS_PRINT_ARP_POLICER:
		cs_qos_print_arp_policer();
		entry->ret = 0;
		break;
	case CS_QOS_INGRESS_SET_PKT_TYPE_POL:
		entry->ret = cs_qos_set_pkt_type_pol(
				entry->pkt_type_policer.port_id,
				entry->pkt_type_policer.pkt_type,
				entry->pkt_type_policer.cir,
				entry->pkt_type_policer.cbs,
				entry->pkt_type_policer.pir,
				entry->pkt_type_policer.pbs);
		break;
	case CS_QOS_INGRESS_RESET_PKT_TYPE_POL:
		entry->ret = cs_qos_reset_pkt_type_pol(
				entry->pkt_type_policer.port_id,
				entry->pkt_type_policer.pkt_type);
		break;
	case CS_QOS_INGRESS_GET_PKT_TYPE_POL:
		entry->ret = cs_qos_get_pkt_type_pol(
				entry->pkt_type_policer.port_id,
				entry->pkt_type_policer.pkt_type,
				&entry->pkt_type_policer.cir,
				&entry->pkt_type_policer.cbs,
				&entry->pkt_type_policer.pir,
				&entry->pkt_type_policer.pbs);
		break;
	case CS_QOS_INGRESS_PRINT_PKT_TYPE_POL:
		cs_qos_print_pkt_type_pol(
				entry->pkt_type_policer.port_id,
				entry->pkt_type_policer.pkt_type);
		entry->ret = 0;
		break;
	case CS_QOS_INGRESS_PRINT_PKT_TYPE_POL_PORT:
		cs_qos_print_pkt_type_pol_per_port(
				entry->pkt_type_policer.port_id);
		entry->ret = 0;
		break;
	case CS_QOS_INGRESS_PRINT_PKT_TYPE_POL_ALL:
		cs_qos_print_all_pkt_type_pol();
		entry->ret = 0;
		break;
	default:
		entry->ret = -EPERM;
	}

	return 0;
}

int cs_qos_ingress_init(void)
{
	int ret;

	memset(ingress_cpu_port_cfg, 0x0, sizeof(cs_qos_ingress_port_cfg_t) *
			CS_QOS_INGRESS_PORT_MAX);
	memset(&cs_qos_arp_policer_db, 0x0, sizeof(cs_qos_arp_policer_t));

	ret = cs_qos_ingress_set_mode(CS_QOS_INGRESS_MODE_DSCP);
	if (ret != 0)
		printk("%s:%d:unable to set up default ingress OoS mode to "
				"DSCP\n", __func__, __LINE__);

#if 0
	// FIXME!! Test code
	cs_qos_ingress_set_queue_scheduler(0, 0, 7, 0, 0);
	cs_qos_ingress_set_queue_scheduler(0, 1, 6, 0, 0);
	cs_qos_ingress_print_queue_scheduler_of_port(0);
	cs_qos_ingress_print_queue_size(0, 7);
	cs_qos_ingress_set_value_queue_mapping(0x38, 0);
	cs_qos_ingress_set_value_queue_mapping(0x30, 1);
	cs_qos_ingress_print_value_queue_mapping();
#endif

	return 0;
} /* cs_qos_ingress_init */

void cs_qos_ingress_exit(void)
{
	/* force the mode to be initialized back to DSCP <- default mode */
	cs_qos_ingress_set_mode(CS_QOS_INGRESS_MODE_8021P);
	cs_qos_ingress_set_mode(CS_QOS_INGRESS_MODE_DSCP);

	return;
} /* cs_qos_ingress_exit */
