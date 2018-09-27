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
 * cs_hw_accel_qos_multiq.c
 *
 * $Id: cs_hw_accel_qos_multiq.c,v 1.11 2012/09/27 02:16:08 ewang Exp $
 *
 * This file contains the implementation for CS QoS implementation
 * based on MultiQ.
 */
#ifdef CONFIG_NET_SCH_MULTIQ
#include "cs_hw_accel_qos.h"
#include "cs_hw_accel_qos_data.h"
#include "cs_core_logic.h"

extern u8 cs_ni_get_port_id(struct net_device *dev);

#ifdef CONFIG_CS752X_HW_ACCELERATION
static int cs_qos_handle_dev_cpu_collision(struct sk_buff *skb,
		struct netdev_queue *dev_queue)
{
        int ret = 0;
#ifndef CONFIG_PREEMPT_NONE
        get_cpu();
#endif
	if (unlikely(dev_queue->xmit_lock_owner == smp_processor_id())) {
		/* same cpu holding the lock. It may be a transient
		 * configuration error, when hard_start_xmit() recurses. We
		 * detect it by checking xmit owner and drop the packet when
		 * deadloop is detected. Return 0 to try the next skb */
		kfree_skb(skb);
		if (net_ratelimit())
			printk(KERN_WARNING "Dead loop on netdeivce %s, fix it"
					"urgently!\n", dev_queue->dev->name);
	} else {
		ret = CS_ERROR;
	}
#ifndef CONFIG_PREEMPT_NONE
	put_cpu();
#endif
	return ret;
} /* cs_qos_handle_dev_cpu_collision */

/* check if the qdisc is a multiq. (well.. most likely is) if HW
 * acceleration is turned on.. check if this SKB is marked sw_only.
 * If skb is qos_sw_only.. don't do anything. if skb can be hw accelerated,
 * then it will be placed into its corresponding HW queue for transmitting */
int cs_qos_check_and_steal_multiq_skb(struct Qdisc *qdisc,
		struct sk_buff *skb)
{
	struct netdev_queue *txq;
	struct  net_device *dev;
	int ret = NETDEV_TX_BUSY;

	/* if this queue is not supported by HW, just return anything but NOT
	 * NET_XMIT_SUCCESS. */
	if (!(CS_QOS_HWQ_MAP & qdisc->cs_handle))
		return NET_XMIT_MASK;

	qdisc_bstats_update(qdisc, skb);

	dev = qdisc_dev(qdisc);
	txq = netdev_get_tx_queue(dev, skb_get_queue_mapping(skb));

	cs_qos_set_voq_id_to_skb_cs_cb(skb, dev, skb_get_queue_mapping(skb));

#ifndef CONFIG_PREEMPT_NONE
	get_cpu();
#endif
	HARD_TX_LOCK(dev, txq, smp_processor_id());
	if (!netif_xmit_frozen_or_stopped(txq))
		ret = dev_hard_start_xmit(skb, dev, txq);
	HARD_TX_UNLOCK(dev, txq);
#ifndef CONFIG_PREEMPT_NONE
	put_cpu();
#endif

	switch (ret) {
		case NETDEV_TX_OK:
			return NET_XMIT_SUCCESS;
		case NETDEV_TX_LOCKED:
			/* driver try lock failed */
			if (0 == cs_qos_handle_dev_cpu_collision(skb, txq))
				return NET_XMIT_SUCCESS;
			else return NET_XMIT_MASK;
		default:
			/* driver returned NETDEV_TX_BUSY.. we will just let
			 * the rest of SW QoS to re-queue the skb */
			return NET_XMIT_MASK;
	};

	return NET_XMIT_MASK;
} /* cs_qos_check_and_steal_multiq_skb */
EXPORT_SYMBOL(cs_qos_check_and_steal_multiq_skb);
#endif /* CONFIG_CS752X_HW_ACCELERATION */

/* burst size is in the range of 16 to 256 */
void cs_qos_set_multiq_attribute(struct Qdisc *qdisc,
		u16 burst_size, u32 rate_bps, u16 min_global_buffer,
		u16 max_global_buffer, u8 wred_mode, u8 wred_adj_range_idx)
{
	struct net_device *dev;
	u8 port_id;
	int status;

	dev = qdisc_dev(qdisc);
	if (dev == NULL)
		return;

	if ((burst_size != 0) && ((burst_size < 16) || (burst_size > 256)))
		return;

	port_id = cs_ni_get_port_id(dev);

	status = cs_qos_set_port_param(port_id, burst_size, rate_bps);
	if (status != 0)
		return;

	status = cs_qos_set_port_global_buffer(port_id, min_global_buffer,
			max_global_buffer);
	if (status != 0)
		return;

	status = cs_qos_set_wred_mode(wred_mode, wred_adj_range_idx);
	if (status != 0)
		return;

	/* FIXME! do we want to introduce error code? */
} /* cs_qos_set_multiq_attribute */
EXPORT_SYMBOL(cs_qos_set_multiq_attribute);

extern int use_internal_buff;
int cs_qos_set_multisubq_attribute(struct Qdisc *qdisc, u8 band_id,
		u8 priority, u32 weight, u32 rsrv_size,
		u32 max_size, u32 rate_bps, void *p_wred)
{
	struct net_device *dev;
	u8 port_id;
	int status;
	int limit;

#define CS_QOS_MAX_SIZE_OF_INT_VOQ	2048
#define CS_QOS_MAX_SIZE_OF_EXT_VOQ	65535
	dev = qdisc_dev(qdisc);
	if (dev == NULL)
		return -EINVAL;

	if (use_internal_buff == 1)
		limit = CS_QOS_MAX_SIZE_OF_INT_VOQ;
	else
		limit = CS_QOS_MAX_SIZE_OF_EXT_VOQ;
	
	if ((max_size > limit) || (max_size <= rsrv_size)) {
		printk("Need to assert %d >= max_size (%d) > rsrv_size (%d)\n", 
			limit, max_size, rsrv_size);
		return -EINVAL;
	}

	port_id = cs_ni_get_port_id(dev);

	status = cs_qos_set_voq_param(port_id,
			CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(port_id, band_id),
			priority, weight, rsrv_size, max_size, rate_bps,
			(struct tc_wredspec *)p_wred);
	if (status != 0)
		return -EINVAL;

	return 0;
} /* cs_qos_set_multisubq_attribute */
EXPORT_SYMBOL(cs_qos_set_multisubq_attribute);

int cs_qos_get_multisubq_depth(struct Qdisc *qdisc, u8 band_id,
		u16 *p_min_depth, u16 *p_max_depth)
{
	struct net_device *dev;
	u8 port_id;
	int status;

	dev = qdisc_dev(qdisc);
	if (dev == NULL)
		return -EINVAL;

	port_id = cs_ni_get_port_id(dev);

	status = cs_qos_get_voq_depth(port_id,
			CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(port_id, band_id),
			p_min_depth, p_max_depth);
	if (status != 0)
		return -EINVAL;

	return 0;
} /* cs_qos_get_multisubq_depth */
EXPORT_SYMBOL(cs_qos_get_multisubq_depth);

void cs_qos_reset_multiq(struct Qdisc *qdisc)
{
	struct net_device *dev;
	u8 port_id;
	int status;

	dev = qdisc_dev(qdisc);
	if (dev == NULL)
		return;

	port_id = cs_ni_get_port_id(dev);

	status = cs_qos_reset_port(port_id);
	if (status != 0)
		return;

	/* FIXME! do we want to introduce error code? */
} /* cs_qos_reset_multiq */
EXPORT_SYMBOL(cs_qos_reset_multiq);

void cs_qos_reset_multisubq(struct Qdisc *qdisc, u8 band_id)
{
	struct net_device *dev;
	u8 port_id;
	int status;

	dev = qdisc_dev(qdisc);
	if (dev == NULL)
		return;

	port_id = cs_ni_get_port_id(dev);

	status = cs_qos_reset_voq(port_id,
			CS_QOS_VOQ_ID_FROM_PORT_QUEUE_ID(port_id, band_id));
	if (status != 0)
		return;

	/* FIXME! do we want to introduce error code? */
} /* cs_qos_reset_multisubq */
EXPORT_SYMBOL(cs_qos_reset_multisubq);

int cs_qos_multiq_init(void)
{
	return 0;
} /* cs_qos_multiq_init */

void cs_qos_multiq_exit(void)
{
	return;
} /* cs_qos_multiq_exit */
#endif /* CONFIG_NET_SCH_MULTIQ */
