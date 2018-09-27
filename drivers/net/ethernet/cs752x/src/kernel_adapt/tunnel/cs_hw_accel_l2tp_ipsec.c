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
 * cs_hw_accel_l2tp_ipsec.c
 *
 * $Id$
 *
 * This file contains the implementation for CS L2TP/IPsec Offload Kernel Module.
 */

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/esp.h>
#include <net/ah.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/crypto.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include <mach/cs_route_api.h>
#include <mach/cs_rule_hash_api.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include "cs_fe.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_hw_accel_sa_id.h"
#include "cs_mut.h"
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
#include "cs_hw_accel_ip_translate.h"
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#else
#define DBG(x)	{ }
#endif /* CONFIG_CS752X_PROC */

#ifdef CS_IPC_ENABLED
#include <mach/cs_vpn_tunnel_ipc.h>
#endif

#if defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL) || \
	defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL)
#endif

#define ERR(x)	(x)
#define SKIP(x) { }

cs_l2tp_cb_t cs_tunnel_cb;
spinlock_t cs_l2tp_lock;

/* external variables and functions */
extern cs_status_t cs_l3_nexthop_update_by_sa_id(cs_uint32_t old_sa_id,
				cs_uint32_t new_sa_id, cs_uint8_t *source_mac);

/* static variables and functions */


/* utilities */
static cs_status_t cs_l2tp_add_ipc_send(
	CS_IN cs_tunnel_entry_t				*t,
	CS_IN cs_session_id_t				session_id)
{
	cs_ipsec_policy_node_t p_node;
	cs_uint8_t protocol;
	int ret;
	cs_sa_id_direction_t dir = -1;

	switch (t->tunnel_cfg.dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
	case CS_TUNNEL_DIR_INBOUND:
		dir = t->tunnel_cfg.dir;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		DBG(printk("%s:%d invalid dir %d\n",
			__func__, __LINE__, t->tunnel_cfg.dir));
		return CS_E_CONFLICT;
	}

	/* get policy and sa from p_node */

	if (t->tunnel_cfg.type == CS_L2TP_IPSEC ||
		t->tunnel_cfg.type == CS_L2TPV3_IPSEC) {
		if (t->tunnel_cfg.tunnel.l2tp.ipsec_policy == 0) {
			ERR(printk("%s:%d invalid ipsec_policy\n",
						__func__, __LINE__));
			return CS_E_CONFLICT;
		}



		/* get IPSec policy node */
		ret = cs_ipsec_policy_node_get(t->device_id,
				dir,
				t->tunnel_cfg.tunnel.l2tp.ipsec_policy,
				&p_node);
		if (ret != CS_OK) {
			ERR(printk("%s:%d can't get IPSec policy and sa\n",
						__func__, __LINE__));
			return CS_E_CONFLICT;
		}
#ifndef CONFIG_CS75XX_HW_ACCEL_L2TPV3_IPSEC
		/* make sure it is IPSec transport mode */
		if (p_node.sa.tunnel != 1) {
			ERR(printk("%s:%d not IPSec transport mode\n",
						__func__, __LINE__));
			return CS_E_CONFLICT;
		}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TPV3_IPSEC */

#ifdef CS_IPC_ENABLED
		/* send IPCs */
		cs_ipsec_ipc_send_set_sadb(dir, &p_node, t->sa_id);
#endif /* CS_IPC_ENABLED */
		if (p_node.sa.tunnel == 0 /* Tunnel mode */)
			protocol = CS_IPPROTO_L2TP;
		else if (p_node.sa.is_natt != 0)
			protocol = IPPROTO_UDP;
		else
			protocol = p_node.sa.proto ? IPPROTO_AH : IPPROTO_ESP;

		if (t->tunnel_cfg.type == CS_L2TPV3_IPSEC)
			session_id = t->tunnel_cfg.tunnel.l2tp.session_id;

#ifdef CS_IPC_ENABLED
		cs_l2tp_ipc_send_set_entry(dir, &t->tunnel_cfg,
				session_id, protocol, t->sa_id);
#endif /* CS_IPC_ENABLED */
		/* create hash */
		/*cs_l2tp_ipsec_tunnel_hash_create(t);*/

	} else if (t->tunnel_cfg.type == CS_L2TP) {
		/* send IPCs */
#ifdef CS_IPC_ENABLED
		cs_l2tp_ipc_send_set_entry(dir, &t->tunnel_cfg,
				session_id, IPPROTO_UDP, t->sa_id);
#endif /* CS_IPC_ENABLED */
		/* create hash */
	} else if (t->tunnel_cfg.type == CS_L2TPV3) {
		session_id = t->tunnel_cfg.tunnel.l2tp.session_id;
		/* send IPCs */
#ifdef CS_IPC_ENABLED
		if (t->tunnel_cfg.tunnel.l2tp.encap_type == 1 /* IP */) {
			cs_l2tp_ipc_send_set_entry(dir, &t->tunnel_cfg,
					session_id, CS_IPPROTO_L2TP, t->sa_id);
		} else { /* UDP */
			cs_l2tp_ipc_send_set_entry(dir, &t->tunnel_cfg,
					session_id, IPPROTO_UDP, t->sa_id);
		}
#endif /* CS_IPC_ENABLED */
	}

	return CS_E_NONE;
}

static cs_status_t cs_l2tp_del_ipc_send(
	CS_IN	cs_tunnel_entry_t			*t)
{
	cs_sa_id_direction_t dir = -1;

	switch (t->tunnel_cfg.dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
	case CS_TUNNEL_DIR_INBOUND:
		dir = t->tunnel_cfg.dir;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		DBG(printk("%s:%d invalid dir %d\n",
			__func__, __LINE__, t->tunnel_cfg.dir));
		return CS_E_CONFLICT;
	}

#ifdef CS_IPC_ENABLED
	/* delete hash */
	cs_l2tp_ipsec_tunnel_hash_del(t);
	/* send IPCs */
	cs_l2tp_ipc_send_del_entry(dir, t->sa_id);

	if (t->tunnel_cfg.type == CS_L2TP_IPSEC ||
		t->tunnel_cfg.type == CS_L2TPV3_IPSEC)
		cs_ipsec_ipc_send_del_entry(dir, t->sa_id);
#endif /* CS_IPC_ENABLED */
	return CS_E_NONE;
}

cs_status_t cs_l2tp_ipsec_set_src_mac(
	CS_IN	cs_l3_nexthop_t 			*nexthop,
	CS_OUT	char 					*src_mac,
	CS_OUT	cs_uint16_t				*sa_id)
{
	cs_tunnel_entry_t *t;
	cs_uint8_t mac[CS_ETH_ADDR_LEN];
	int crc32, i;
	unsigned long flags;

	if (nexthop == NULL || src_mac == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if ((nexthop->nhid.nhop_type != CS_L3_NEXTHOP_TUNNEL_L2TP_IPSEC) &&
		(nexthop->nhid.nhop_type != CS_L3_NEXTHOP_TUNNEL_L2TP) &&
		(nexthop->nhid.nhop_type != CS_L3_NEXTHOP_TUNNEL_IPSEC)) {
		ERR(printk("%s:%d invalid nexthop type %d\n",
			__func__, __LINE__, nexthop->nhid.nhop_type));
		return CS_E_CONFLICT;
	}
	if ((nexthop->id.tunnel_id < TID_L2TP_IPSEC_BASE) ||
		(nexthop->id.tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d invalid tunnel_id %d\n",
			__func__, __LINE__, nexthop->id.tunnel_id));
		return CS_E_CONFLICT;
	}

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == nexthop->id.tunnel_id) {
			/* hit */
			/* fill the source MAC with the host order */
			mac[1] = t->tunnel_cfg.type & 0xFF;
			mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
			mac[3] = t->sa_id & 0xFF;
			mac[4] = (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND) ? 0 : 1;
			mac[5] = 0; /* reserved */


			/* Calculate CRC over the MAC SA */
			crc32 = ~(calc_crc(~0, &mac[1], 5));
			/* Store 8 bits of the crc in the src MAC */
			mac[0] = crc32 & 0xFF;

			for (i = 0; i < CS_ETH_ADDR_LEN; i++)
				src_mac[i] = mac[(CS_ETH_ADDR_LEN - 1) - i];

			*sa_id = t->sa_id;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_OK;
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_l2tp_ipsec_set_src_mac);

/* update sa_id in tunnel node */
cs_status_t cs_l2tp_sa_id_set(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_uint16_t				sa_id)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	DBG(printk("%s:%d tunnel_id = %d, sa_id = %d\n",
		__func__, __LINE__, tunnel_id, sa_id));

	if ((tunnel_id < TID_L2TP_IPSEC_BASE) || (tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d invalid tunnel id\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	/* L2TP tunnel or L2TP over IPSec */
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			t->sa_id = sa_id;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_OK;
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;

}

/* update state in tunnel node */
cs_status_t cs_tunnel_state_set(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_tunnel_state_t			state)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	DBG(printk("%s:%d tunnel_id = %d, state = %d\n",
		__func__, __LINE__, tunnel_id, state));

	if ((tunnel_id < TID_L2TP_IPSEC_BASE) || (tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d invalid tunnel id\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	/* L2TP tunnel or L2TP over IPSec */
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			t->state = state;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_OK;
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;

}

/* update hash index in tunnel node */
cs_status_t cs_tunnel_hash_index_set(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_tunnel_hash_type_t			type,
	CS_IN	cs_uint32_t				voq_pol_idx,
	CS_IN	cs_uint32_t				vlan_rslt_idx,
	CS_IN	cs_uint32_t				fwd_rslt_idx,
	CS_IN	cs_uint16_t				hash_idx)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	DBG(printk("%s:%d tunnel_id = %d, type = %d,\n\t"
		"voq_pol_idx %d, vlan_rslt_idx %d, "
		"fwd_rslt_idx %d, hash_idx %d\n",
		__func__, __LINE__,
		tunnel_id, type,
		voq_pol_idx, vlan_rslt_idx, fwd_rslt_idx, hash_idx));

	if ((tunnel_id < TID_L2TP_IPSEC_BASE) || (tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d invalid tunnel id %d\n",
			__func__, __LINE__, tunnel_id));
		return CS_E_NOT_FOUND;
	}

	if (type >= CS_TUNNEL_HASH_TYPE_MAX) {
		ERR(printk("%s:%d invalid tunnel type %d\n",
			__func__, __LINE__, type));
		return CS_E_NOT_FOUND;
	}

	/* L2TP tunnel or L2TP over IPSec */
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			t->voq_pol_idx[type] = voq_pol_idx;
			t->vlan_rslt_idx[type] = vlan_rslt_idx;
			t->fwd_rslt_idx[type] = fwd_rslt_idx;
			t->fwd_hash_idx[type] = hash_idx;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_OK;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;

}


cs_status_t cs_l2tp_session_id_get(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_OUT	cs_uint32_t				*session_id)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	SKIP(printk("%s:%d tunnel_id = %d\n",
		__func__, __LINE__, tunnel_id));

	if ((tunnel_id < TID_L2TP_IPSEC_BASE) || (tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d invalid tunnel id\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	*session_id = 0;

	/* L2TP tunnel or L2TP over IPSec */
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			if (t->tunnel_cfg.type == CS_L2TP ||
					t->tunnel_cfg.type == CS_L2TP_IPSEC) {
				if (t->se_cnt == 0 || t->se_h == NULL) {
					DBG(printk("%s:%d: tunnel_id %d,"
						" se_cnt %d, se_h 0x%p\n",
						__func__, __LINE__,
						tunnel_id, t->se_cnt, t->se_h));
					spin_unlock_irqrestore(&cs_l2tp_lock, flags);
					return CS_E_CONFLICT;
				}
				*session_id = t->se_h->session_id;
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				DBG(printk("%s:%d tunnel_id = %d, sid = 0x%x\n",
					__func__, __LINE__,
					tunnel_id, *session_id));
				return CS_OK;
			} else {
				ERR(printk("%s:%d invalid tunnel type %d\n",
					__func__, __LINE__,
					t->tunnel_cfg.type));
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				return CS_E_CONFLICT;
			}
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;

}


cs_status_t cs_tunnel_get_by_sa_id(
	CS_IN	cs_uint16_t				sa_id,
	CS_IN	cs_sa_id_direction_t			dir,
	CS_OUT	cs_tunnel_entry_t  			*p_tunnel)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	SKIP(printk("%s:%d sa_id = %d, dir = %d, p_tunnel = 0x%p\n",
		__func__, __LINE__, sa_id, dir, p_tunnel));

	if (p_tunnel == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	if (dir > DOWN_STREAM) {
		ERR(printk("%s:%d invalid direction %u\n",
			__func__, __LINE__, dir));
		return CS_E_DIR;
	}

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id and direciton */
		if ((t->sa_id == sa_id) &&
			(t->tunnel_cfg.dir == (cs_tunnel_dir_t) dir)) {
			*p_tunnel = *t;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			SKIP(printk("%s:%d Got type %d, dir %d, tunnel_id = %d,"
				" sa_id = %d\n",
				__func__, __LINE__,
				t->tunnel_cfg.type, t->tunnel_cfg.dir,
				t->tunnel_id, t->sa_id));
			return CS_OK;
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_tunnel_get_by_sa_id);

cs_status_t cs_l2tp_tunnel_get_by_tid(
	CS_IN	cs_tunnel_dir_t				dir,
	CS_IN	cs_uint32_t				tid,
	CS_OUT	cs_tunnel_entry_t  			*p_tunnel)
{
	cs_tunnel_entry_t *t;
	cs_tunnel_type_t type;
	unsigned long flags;

	SKIP(printk("%s:%d dir = %d, tid = 0x%x\n",
		__func__, __LINE__, dir, tid));

	if (p_tunnel == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		type = t->tunnel_cfg.type;
		/* search by L2TP tid */
		if ((type == CS_L2TP || type == CS_L2TPV3 ||
			type == CS_L2TP_IPSEC || type == CS_L2TPV3_IPSEC) &&
			(t->tunnel_cfg.dir == dir) &&
			(t->tunnel_cfg.tunnel.l2tp.tid == tid)) {

			*p_tunnel = *t;
			DBG(printk("%s:%d Got type %d, dir %d, tid 0x%x,"
				"tunnel_id = %d, sa_id = %d\n",
				__func__, __LINE__,
				type, dir, tid,
				t->tunnel_id, t->sa_id));
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_OK;
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_l2tp_tunnel_get_by_tid);


cs_status_t cs_l2tp_ipsec_sa_id_get_by_tunnel_id(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_tunnel_type_t			tunnel_type,
	CS_IN	cs_tunnel_dir_t				dir,
	CS_OUT	cs_uint16_t				*sa_id)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	SKIP(printk("%s:%d tunnel_id = %d, tunnel_type = %d, dir = %d, "
		"&sa_id = 0x%p\n",
		__func__, __LINE__, tunnel_id, tunnel_type, dir, sa_id));

	if (sa_id == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id &&
			t->tunnel_cfg.type == tunnel_type) {
			if (t->tunnel_cfg.dir != dir) {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				return CS_E_DIR;
			}

			*sa_id = t->sa_id;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			SKIP(printk("%s:%d tunnel_id = %d, tunnel_type = %d,"
				" dir = %d, sa_id = %d\n",
				__func__, __LINE__,
				tunnel_id, tunnel_type, dir, *sa_id));
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}

cs_status_t cs_l2tp_tunnel_handle(
	struct sk_buff 					*skb)
{
	struct ethhdr *eth;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_uint16_t sa_id;
	cs_tunnel_entry_t *t, t_node;
	cs_status_t ret;
	cs_sa_id_direction_t dir;

	t = &t_node;
	eth = eth_hdr(skb);
	sa_id = eth->h_source[3];
	dir = eth->h_source[4];


	/* h_source[4] DIR = 1(WAN to LAN)  0(LAN to WAN) */
	/* only handle h_source[4] DIR = 1 esle drop the packet */
	if (dir == DOWN_STREAM) {
		DBG(printk("%s:%d New inbound L2TP flow, len = %d, sa_id = %d\n",
			__func__, __LINE__, skb->len, sa_id));
		DBG(cs_dump_data((void *)eth, skb->len));
	} else {
		DBG(printk("%s:%d Receive a upstream packet. "
			"Can't hit PE to WAN hash\n",
			__func__, __LINE__));
		DBG(cs_dump_data((void *)eth, skb->len));
		goto SKB_HANDLED;
	}

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL
	if (cs_l2tp_ctrl_enable() == FALSE)
		return CS_OK;

	ret = cs_tunnel_get_by_sa_id(sa_id, dir, t);

	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d Can't find valid tunnel entry\n",
			__func__, __LINE__));
		goto SKB_HANDLED;;
	}

	if (t->state != CS_TUNNEL_ACCELERATED) {
		DBG(printk("%s:%d L2TP tunnel is not ready, "
			"tunnel id = %d\n",
			__func__,__LINE__, t->tunnel_id));
		goto SKB_HANDLED;;
	}

	if (cs_cb) {
		cs_cb->common.module_mask |= CS_MOD_MASK_L2TP;
		cs_cb->common.vpn_dir = dir;
		cs_cb->common.tunnel_id = t->tunnel_cfg.tunnel.l2tp.tid;

		if (t->se_cnt > 0 && t->se_h != NULL)
			cs_cb->common.session_id = t->se_h->session_id;
	} else {
		DBG(printk("%s:%d: No CS_CB\n",
			__func__,__LINE__));
		goto SKB_HANDLED;
	}

#endif

	return CS_OK;

SKB_HANDLED:
	dev_kfree_skb(skb);
	return CS_E_ERROR;
}



cs_status_t cs_l2tp_ipsec_tunnel_add(
	CS_IN cs_dev_id_t				device_id,
	CS_IN cs_tunnel_cfg_t				*p_tunnel_cfg,
	CS_OUT cs_tunnel_id_t				*p_tunnel_id)
{
	cs_status_t ret;
	cs_ipsec_policy_node_t p_node;
	cs_uint32_t *policy_handle_p = NULL;
	cs_boolean_t flag;
	cs_tunnel_entry_t *tunnel_node, *t, *t2;
	int found = 0;
	unsigned long flags;

	DBG(printk("%s:%d device_id=%d, p_tunnel_cfg = 0x%p,"
		" p_tunnel_id = 0x%p\n",
		__func__, __LINE__, device_id, p_tunnel_cfg, p_tunnel_id));

	tunnel_node = cs_zalloc(sizeof(cs_tunnel_entry_t), GFP_KERNEL);
	if (tunnel_node == NULL) {
		ERR(printk("%s:%d out of memory\n",
			__func__, __LINE__));
		return CS_E_MEM_ALLOC;
	}
	tunnel_node->device_id = device_id;
	tunnel_node->state = CS_TUNNEL_INIT;

	/* Generate an unique tunnel_id.
	 * The resource of PE and system RAM is restricted,
	 * so it is impossible to use all 768 L2TP_IPSEC tunnel IDs nor
	 * the following do-while becomes infinite loop.
	 */
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	do {
		++cs_tunnel_cb.curr_tu_id;
		if (cs_tunnel_cb.curr_tu_id >= TID_L2TP_IPSEC_MAX)
			cs_tunnel_cb.curr_tu_id = TID_L2TP_IPSEC_BASE;
		tunnel_node->tunnel_id = cs_tunnel_cb.curr_tu_id;

		found = 1;
		t = cs_tunnel_cb.tu_h;
		while (t) {
			/* search by tunnel_id */
			if (t->tunnel_id == tunnel_node->tunnel_id) {
				found = 0;
				break;
			}

			t = t->next;
		}
	} while (found == 0);
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	t = NULL;

	*p_tunnel_id = tunnel_node->tunnel_id;
	memcpy(&tunnel_node->tunnel_cfg, p_tunnel_cfg,
					sizeof(cs_tunnel_cfg_t));

	/* check direction */
	switch (tunnel_node->tunnel_cfg.dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		DBG(printk("%s:%d upstream\n", __func__, __LINE__));
		break;
	case CS_TUNNEL_DIR_INBOUND:
		DBG(printk("%s:%d downstream\n", __func__, __LINE__));
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		ERR(printk("%s:%d unknown direction %d\n",
					__func__, __LINE__,
					tunnel_node->tunnel_cfg.dir));
		cs_free(tunnel_node);
		return CS_E_CONFLICT;
	}

	/* apply or inherit a sa_id */
	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));
	switch (tunnel_node->tunnel_cfg.type) {
	case CS_L2TP:
	case CS_L2TPV3:
		/* apply a sa_id */
		ret = cs_sa_id_alloc(tunnel_node->tunnel_cfg.dir,
					0,
					0,
					tunnel_node->tunnel_cfg.type,
					&tunnel_node->sa_id);

		if (ret != CS_OK) {
			ERR(printk("%s:%d can't apply sa_id\n",
						__func__, __LINE__));
			cs_free(tunnel_node);
			return CS_E_CONFLICT;
		}
		break;
	case CS_IPSEC:
		policy_handle_p =
			&p_tunnel_cfg->tunnel.ipsec.ipsec_policy;
		break;
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
		policy_handle_p =
			&p_tunnel_cfg->tunnel.l2tp.ipsec_policy;
		break;
	default:
		ERR(printk("%s:%d unknown tunnel type %d\n",
					__func__, __LINE__,
					tunnel_node->tunnel_cfg.type));
		cs_free(tunnel_node);
		return CS_E_CONFLICT;
	}


	if (policy_handle_p) { /* type = CS_IPSEC or CS_L2TP_IPSEC */
		if (*policy_handle_p > 0) {
			/* get IPSec policy node */
			ret = cs_ipsec_policy_node_get(device_id,
					tunnel_node->tunnel_cfg.dir,
					*policy_handle_p,
					&p_node);
			if (ret != CS_OK) {
				ERR(printk("%s:%d can't get IPSec "
						"policy node\n",
						__func__, __LINE__));
				cs_free(tunnel_node);
				return CS_E_CONFLICT;
			}

			/* check if SA is valid. */
			if (p_node.valid != CS_TRUE ||
				p_node.sa_valid != CS_TRUE) {
				ERR(printk("%s:%d invalid policy node "
					"(%d) or invalid sa (%d)\n",
					__func__, __LINE__,
					p_node.valid, p_node.sa_valid));
				cs_free(tunnel_node);
				return CS_E_CONFLICT;
			}

			if (tunnel_node->tunnel_cfg.type == CS_L2TP_IPSEC ||
				tunnel_node->tunnel_cfg.type == CS_L2TPV3_IPSEC) {
				/* crosscheck between L2TP IP and
				   selectors of IPSec policy */
				cs_ipsec_sel_ip_check_2(
					&tunnel_node->tunnel_cfg,
					&p_node.policy,
					&flag);
				if (flag == CS_FALSE) {
					ERR(printk("%s:%d tunnel IP and"
						" IPSec selectors are "
						"mismatched\n",
						__func__, __LINE__));
					cs_free(tunnel_node);
					return CS_E_CONFLICT;
				}
			}

			/* inherit a sa_id */
			tunnel_node->sa_id = p_node.sa_handle - 1;

			/* set tunnel_id to policy node */
			ret = cs_ipsec_policy_tunnel_link(device_id,
					tunnel_node->tunnel_cfg.dir,
					*policy_handle_p,
					tunnel_node->tunnel_id,
					&tunnel_node->tunnel_cfg.src_addr,
					&tunnel_node->tunnel_cfg.dest_addr);
			if (ret != CS_OK) {
				ERR(printk("%s:%d can't update tunnel_"
						"id to IPSec policy\n",
						__func__, __LINE__));
				cs_free(tunnel_node);
				return CS_E_CONFLICT;
			}


		} else {
			/* invalid policy_handle */
			ERR(printk("%s:%d invalid IPSec policy_handle\n",
						__func__, __LINE__));
			cs_free(tunnel_node);
			return CS_E_CONFLICT;
		}
	}

	tunnel_node->sw_id = CS_TUNNEL_SWID(tunnel_node->tunnel_cfg.type,
					    tunnel_node->tunnel_cfg.dir,
					    tunnel_node->sa_id);
	/* insert tunnel into list */
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	if (t == NULL) {
		cs_tunnel_cb.tu_h = tunnel_node;
		cs_tunnel_cb.tu_cnt = 1;
	} else {
		while (t) {
			t2 = t;
			t = t->next;
		}
		t2->next = tunnel_node;
		tunnel_node->prev = t2;
		cs_tunnel_cb.tu_cnt++;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);

	/* If it is IPSec tunnel, then we send IPCs and create hash */
	switch (p_tunnel_cfg->type) {
	case CS_IPSEC:
		/* make sure it is tunnel mode */
		if (p_node.sa.tunnel != 0) {
			ERR(printk("%s:%d not IPSec tunnel mode\n",
						__func__, __LINE__));
			t = tunnel_node;
			spin_lock_irqsave(&cs_l2tp_lock, flags);
			if (t == cs_tunnel_cb.tu_h)
				cs_tunnel_cb.tu_h = NULL;
			else
				t->prev->next = t->next;

			if (t->next != NULL)
				t->next->prev = t->prev;

			cs_tunnel_cb.tu_cnt--;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);

			cs_free(tunnel_node);
			return CS_E_CONFLICT;
		}

		/* send IPCs */
#ifdef CS_IPC_ENABLED
		cs_ipsec_ipc_send_set_sadb(tunnel_node->tunnel_cfg.dir, &p_node,
						tunnel_node->sa_id);
#endif /* CS_IPC_ENABLED */

		break;
	case CS_L2TPV3:
	case CS_L2TPV3_IPSEC:
#ifdef CS_IPC_ENABLED
		/* send IPCs and create hash */
		cs_l2tp_add_ipc_send(tunnel_node, 0);
#endif /* CS_IPC_ENABLED */
		break;
	default:
		break;
	}

	return CS_E_NONE;

}

cs_status_t cs_l2tp_tunnel_delete(
	CS_IN cs_dev_id_t       			device_id,
	CS_IN cs_tunnel_cfg_t   			*p_tunnel_cfg)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tid */
		if (t->tunnel_cfg.tunnel.l2tp.tid ==
			p_tunnel_cfg->tunnel.l2tp.tid) {
			if (t->se_cnt > 0) {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				ERR(printk("%s:%d still have %d sessions\n",
					__func__, __LINE__,
					t->se_cnt));
				return CS_E_CONFLICT;
			}

			t->state = CS_TUNNEL_INIT;

			if (t == cs_tunnel_cb.tu_h)
				cs_tunnel_cb.tu_h = t->next;
			else
				t->prev->next = t->next;

			if (t->next != NULL)
				t->next->prev = t->prev;

			cs_tunnel_cb.tu_cnt--;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);

			if (p_tunnel_cfg->type == CS_L2TPV3 ||
				p_tunnel_cfg->type == CS_L2TPV3_IPSEC) {
				/* send IPCs and delete hash */
				cs_l2tp_del_ipc_send(t);
			}

			cs_free(t);
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}

cs_status_t cs_ipsec_tunnel_delete(
	CS_IN cs_dev_id_t				device_id,
	CS_IN cs_tunnel_cfg_t				*p_tunnel_cfg)
{
	cs_sa_id_direction_t dir = -1;
	cs_tunnel_entry_t *t;
	unsigned long flags;

	if (p_tunnel_cfg->dir >= CS_TUNNEL_DIR_TWO_WAY ||
		p_tunnel_cfg->tunnel.ipsec.ipsec_policy == 0) {

		DBG(printk("%s:%d invalid tunnel configuration\n",
			__func__, __LINE__));
		return CS_E_CONFLICT;
	}

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by ipsec_policy*/
		if ((t->tunnel_cfg.type == CS_IPSEC) &&
			(t->tunnel_cfg.dir == p_tunnel_cfg->dir) &&
			(t->tunnel_cfg.tunnel.ipsec.ipsec_policy ==
			 p_tunnel_cfg->tunnel.ipsec.ipsec_policy)) {
			if (t->se_cnt > 0) {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				ERR(printk("%s:%d still have %d sessions\n",
					__func__, __LINE__,
					t->se_cnt));
				return CS_E_CONFLICT;
			}

			t->state = CS_TUNNEL_INIT;

			if (t == cs_tunnel_cb.tu_h)
				cs_tunnel_cb.tu_h = t->next;
			else
				t->prev->next = t->next;

			if (t->next != NULL)
				t->next->prev = t->prev;

			cs_tunnel_cb.tu_cnt--;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);

			switch (t->tunnel_cfg.dir) {
			case CS_TUNNEL_DIR_OUTBOUND:
			case CS_TUNNEL_DIR_INBOUND:
				dir = t->tunnel_cfg.dir;
				break;
			case CS_TUNNEL_DIR_TWO_WAY:
			default:
				DBG(printk("%s:%d invalid dir %d\n",
					__func__, __LINE__,
					t->tunnel_cfg.dir));
				return CS_E_CONFLICT;
			}
			/* delete hash */
			cs_l2tp_ipsec_tunnel_hash_del(t);
			/* send IPCs */
#ifdef CS_IPC_ENABLED
			cs_ipsec_ipc_send_del_entry(dir, t->sa_id);
#endif /* CS_IPC_ENABLED */
			cs_free(t);
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;

}

cs_status_t cs_l2tp_ipsec_tunnel_delete_by_idx(
	CS_IN cs_dev_id_t				device_id,
	CS_IN cs_tunnel_id_t				tunnel_id)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d\n",
		__func__, __LINE__, device_id, tunnel_id));

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			if (t->se_cnt > 0) {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				ERR(printk("%s:%d still have %d sessions\n",
					__func__, __LINE__,
					t->se_cnt));
				return CS_E_CONFLICT;
			}

			t->state = CS_TUNNEL_INIT;

			if (t == cs_tunnel_cb.tu_h)
				cs_tunnel_cb.tu_h = t->next;
			else
				t->prev->next = t->next;

			if (t->next != NULL)
				t->next->prev = t->prev;

			cs_tunnel_cb.tu_cnt--;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);

			/* check if it is IPSec tunnel */
			if (t->tunnel_cfg.type == CS_IPSEC) {
				cs_sa_id_direction_t dir = -1;

				switch (t->tunnel_cfg.dir) {
				case CS_TUNNEL_DIR_OUTBOUND:
				case CS_TUNNEL_DIR_INBOUND:
					dir = t->tunnel_cfg.dir;
					break;
				case CS_TUNNEL_DIR_TWO_WAY:
				default:
					DBG(printk("%s:%d invalid dir %d\n",
						__func__, __LINE__,
						t->tunnel_cfg.dir));
					return CS_E_CONFLICT;
				}

				/* delete hash */
				cs_l2tp_ipsec_tunnel_hash_del(t);
				/* send IPCs */
#ifdef CS_IPC_ENABLED
				cs_ipsec_ipc_send_del_entry(dir, t->sa_id);
#endif /* CS_IPC_ENABLED*/
			} else if (t->tunnel_cfg.type == CS_L2TPV3 ||
					t->tunnel_cfg.type == CS_L2TPV3_IPSEC) {
				/* send IPCs and delete hash */
				cs_l2tp_del_ipc_send(t);
			}

			cs_free(t);
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;

}

cs_status_t cs_l2tp_tunnel_get(
	CS_IN  cs_dev_id_t				device_id,
	CS_IN  cs_tunnel_id_t				tunnel_id,
	CS_OUT cs_tunnel_cfg_t				*p_tunnel_cfg)
{
	cs_tunnel_entry_t *t;
	unsigned long flags;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, p_tunnel_cfg = 0x%p\n",
		__func__, __LINE__, device_id, tunnel_id, p_tunnel_cfg));

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			*p_tunnel_cfg = t->tunnel_cfg;
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}

cs_status_t cs_l2tp_ipsec_session_add(
	CS_IN cs_dev_id_t				device_id,
	CS_IN cs_tunnel_id_t				tunnel_id,
	CS_IN cs_session_id_t				session_id)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*session_node, *s, *s2;
	unsigned long flags;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x\n",
		__func__, __LINE__, device_id, tunnel_id, session_id));

	session_node = cs_zalloc(sizeof(cs_l2tp_session_entry_t), GFP_KERNEL);
	if (session_node == NULL) {
		ERR(printk("%s:%d out of memory\n", __func__, __LINE__));
		return CS_E_MEM_ALLOC;
	}
	session_node->session_id = session_id;

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			if (t->tunnel_cfg.type == CS_L2TP ||
				t->tunnel_cfg.type == CS_L2TP_IPSEC) {
				s = s2 = t->se_h;

				/* go through whole list,
				   and get the last node in s2 */
				while (s) {
					/* check for duplicate session ID */
					if (s->session_id == session_id) {
						spin_unlock_irqrestore(
							&cs_l2tp_lock, flags);
						ERR(printk("%s:%d duplicate "
							"session ID\n",
							__func__, __LINE__));
						cs_free(session_node);
						return CS_E_CONFLICT;
					}
					s2 = s;
					s = s->next;
				}

				if (t->se_h == NULL) {
					t->se_h = session_node;
					t->se_cnt = 1;
				} else {
					spin_unlock_irqrestore(
						&cs_l2tp_lock, flags);
					printk("##L2TP## only support "
						"one session per tunnel\n");
					cs_free(session_node);
					return CS_E_CONFLICT;
				}
				session_node->tunnel = t;
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);

				/* send IPCs and create hash */
				cs_l2tp_add_ipc_send(t, session_id);

			} else {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				ERR(printk("%s:%d invalid tunnel type (%d)\n",
					__func__, __LINE__,
					t->tunnel_cfg.type));
				cs_free(session_node);
				return CS_E_CONFLICT;
			}

			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}

cs_status_t cs_l2tp_ipsec_session_delete(
	CS_IN cs_dev_id_t				device_id,
	CS_IN cs_tunnel_id_t				tunnel_id,
	CS_IN cs_session_id_t				session_id)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;
	unsigned long flags;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x\n",
		__func__, __LINE__, device_id, tunnel_id, session_id));

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			if (t->tunnel_cfg.type == CS_L2TP ||
				t->tunnel_cfg.type == CS_L2TP_IPSEC) {
				s = t->se_h;

				/* go through whole list,
				   and get the last node in s2 */
				while (s) {
					/* search by session ID */
					if (s->session_id == session_id) {
						if (s == t->se_h)
							t->se_h = s->next;
						else
							s->prev->next = s->next;

						if (s->next != NULL)
							s->next->prev = s->prev;

						t->se_cnt--;
						spin_unlock_irqrestore(
							&cs_l2tp_lock, flags);

						/* send IPCs and delete hash */
						cs_l2tp_del_ipc_send(t);

						cs_free(s);
						return CS_E_NONE;
					}
					s = s->next;
				}
			} else {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				ERR(printk("%s:%d invalid tunnel type (%d)\n",
					__func__, __LINE__,
					t->tunnel_cfg.type));
				return CS_E_CONFLICT;
			}
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}

cs_status_t cs_l2tp_ipsec_session_get(
	CS_IN  cs_dev_id_t				device_id,
	CS_IN  cs_tunnel_id_t				tunnel_id,
	CS_IN  cs_session_id_t				session_id,
	CS_OUT cs_boolean_t				*is_present)
{
	cs_tunnel_entry_t *t;
	cs_l2tp_session_entry_t	*s;
	unsigned long flags;

	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x,"
		" is_present = 0x%p\n", __func__, __LINE__,
		device_id, tunnel_id, session_id, is_present));
	if (is_present == NULL)
		return CS_ERROR;
	*is_present = FALSE;

	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			if (t->tunnel_cfg.type == CS_L2TP ||
				t->tunnel_cfg.type == CS_L2TP_IPSEC) {
				s = t->se_h;

				/* go through whole list,
				   and get the last node in s2 */
				while (s) {
					/* search by session ID */
					if (s->session_id == session_id) {
						*is_present = TRUE;
						spin_unlock_irqrestore(
							&cs_l2tp_lock, flags);
						return CS_E_NONE;
					}
					s = s->next;
				}
			} else {
				spin_unlock_irqrestore(&cs_l2tp_lock, flags);
				ERR(printk("%s:%d invalid tunnel type (%d)\n",
					__func__, __LINE__,
					t->tunnel_cfg.type));
				return CS_E_CONFLICT;
			}
			spin_unlock_irqrestore(&cs_l2tp_lock, flags);
			return CS_E_NONE;
		}

		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);
	return CS_E_NOT_FOUND;
}

/* for CS_L2TP, CS_L2TP_IPSEC and CS_IPSEC */
int cs_l2tp_ipsec_tunnel_hash_create(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	unsigned int crc32;
	int ret = 0;
	cs_l3_nexthop_t nexthop;
	cs_ipsec_policy_node_t p_node;
	cs_uint32_t *policy_handle_p = NULL;
	cs_dev_id_t device_id;
	cs_rule_hash_t wan2pe_rule_hash;
	cs_rule_hash_t pe2wan_rule_hash;
	unsigned short old_hash_idx = 0;
	cs_tunnel_type_t type;
	cs_sa_id_direction_t dir = -1;
	cs_uint16_t sa_id = 0, old_sa_id = 0;
	cs_uint8_t src_mac[ETH_ALEN];

	cs_uint8_t      is_natt;
	cs_l3_ip_addr   *p_da_ip, *p_sa_ip;
	cs_uint8_t      ip_version;
	cs_uint16_t     pppoe_session_id = 0;
	cs_uint16_t     hash_index;
	cs_uint16_t     vlan_id = 0;
	cs_uint8        *p_sa_mac, *p_da_mac;
	cs_uint32_t     spi_idx;
	cs_uint16_t     natt_ingress_src_port;
	cs_uint16_t     natt_egress_dest_port;

	if (tunnel_entry == NULL) {
		ERR(printk("%s:%d NULL pointer\n", __func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	type = tunnel_entry->tunnel_cfg.type;

	switch (tunnel_entry->tunnel_cfg.dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
	case CS_TUNNEL_DIR_INBOUND:
		dir = tunnel_entry->tunnel_cfg.dir;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		DBG(printk("%s:%d invalid dir %d\n",
			__func__, __LINE__,
			tunnel_entry->tunnel_cfg.dir));
		return CS_E_CONFLICT;
	}

	memset(&wan2pe_rule_hash, 0, sizeof(cs_rule_hash_t));
	memset(&pe2wan_rule_hash, 0, sizeof(cs_rule_hash_t));

	DBG(printk("%s:%d tunnel_entry = 0x%p\n", __func__, __LINE__,
		tunnel_entry));

	device_id = tunnel_entry->device_id;

	/* get nexthop */
	memset(&nexthop, 0, sizeof(cs_l3_nexthop_t));
	ret = cs_l3_nexthop_get(device_id,
				tunnel_entry->tunnel_cfg.nexthop_id,
				&nexthop);
	if (ret != CS_OK) {
		ERR(printk("%s:%d can't get nexthop %d\n",
					__func__, __LINE__,
					tunnel_entry->tunnel_cfg.nexthop_id));
		return CS_E_CONFLICT;
	}
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
	if (nexthop.nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE) {
		/*
		 * Don't need to create tunnel hash for next hop is IP_TRANLATE
		 * tunnel hash will be created in cs_ip_translate_add()
		 * But need to inform PE for its next hop sa_id
		 */

		DBG(printk("%s:%d tunnel_entry = 0x%p nexthop is IP_TRANSLATE \n", __func__, __LINE__,
			tunnel_entry));
#ifdef CS_IPC_ENABLED
		return cs_ip_translate_nexthop_set(dir, tunnel_entry->tunnel_cfg.type,
			tunnel_entry->tunnel_id, nexthop.id.tunnel_id);
#else
		return CS_E_CONFLICT;
#endif

	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE */
	if (nexthop.nhid.nhop_type != CS_L3_NEXTHOP_DIRECT) {
		ERR(printk("%s:%d unexpected nexthop type %d\n",
					__func__, __LINE__,
					nexthop.nhid.nhop_type));
		return CS_E_CONFLICT;
	}

	/* get policy node */
	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));
	switch (tunnel_entry->tunnel_cfg.type) {
	case CS_IPSEC:
		policy_handle_p =
			&tunnel_entry->tunnel_cfg.tunnel.ipsec.ipsec_policy;
		break;
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
		policy_handle_p =
			&tunnel_entry->tunnel_cfg.tunnel.l2tp.ipsec_policy;
		break;
	default:
		break;
	}
	if (policy_handle_p) {	/* CS_IPSEC or CS_L2TP_IPSEC */
		if (*policy_handle_p > 0) {
			/* get IPSec policy node */
			ret = cs_ipsec_policy_node_get(device_id,
					dir,
					*policy_handle_p,
					&p_node);
			if (ret != CS_OK) {
				ERR(printk("%s:%d can't get IPSec policy node\n",
							__func__, __LINE__));
				return CS_E_CONFLICT;
			}
			if (p_node.valid != CS_TRUE ||
				p_node.sa_valid != CS_TRUE) {
				ERR(printk("%s:%d invalid policy node "
					"(%d) or invalid sa (%d)\n",
					__func__, __LINE__,
					p_node.valid, p_node.sa_valid));
				return CS_E_CONFLICT;
			}

			/* check if rekey */
			if (p_node.previous_sa_handle > 0) {
				sa_id = p_node.sa_handle - 1;
				old_sa_id = p_node.previous_sa_handle - 1;

				/* Backup old hash index */
				old_hash_idx =
					tunnel_entry->fwd_hash_idx[dir];
			}
		} else {
			/* invalid policy_handle */
			ERR(printk("%s:%d invalid IPSec policy_handle\n",
						__func__, __LINE__));
			return CS_E_CONFLICT;
		}
	}

	if (dir == UP_STREAM /* upstream */) {

		is_natt = p_node.sa.is_natt;

		if (tunnel_entry->tunnel_cfg.type == CS_L2TPV3_IPSEC &&
			p_node.sa.tunnel == 0 /* tunnel */) {
			p_da_ip = &(p_node.sa.tunnel_daddr);
			p_sa_ip = &(p_node.sa.tunnel_saddr);
			ip_version = p_node.sa.ip_ver;
		} else {
			p_da_ip = &(tunnel_entry->tunnel_cfg.dest_addr.ip_addr);
			p_sa_ip = &(tunnel_entry->tunnel_cfg.src_addr.ip_addr);
			ip_version = tunnel_entry->tunnel_cfg.dest_addr.afi;
		}
		if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
			pppoe_session_id = nexthop.encap.port_encap.port_encap.pppoe.pppoe_session_id;
		}
		if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
			vlan_id = nexthop.encap.port_encap.port_encap.pppoe.tag[0] & VLAN_VID_MASK;
		} else if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_ETH_1Q_E ||
			nexthop.encap.port_encap.type == CS_PORT_ENCAP_ETH_QinQ_E) {
			vlan_id = nexthop.encap.port_encap.port_encap.eth.tag[0] & VLAN_VID_MASK;
		}
		p_da_mac = &(nexthop.da_mac[0]);
		p_sa_mac = &(nexthop.encap.port_encap.port_encap.eth.src_mac[0]);
		spi_idx = p_node.sa.spi;
		natt_egress_dest_port = p_node.sa.natt_egress_dest_port;

		ret = cs_tunnel_pe2wan_rule_hash_add(&(tunnel_entry->tunnel_cfg), is_natt, p_da_ip, p_sa_ip, ip_version,
				pppoe_session_id, vlan_id, p_da_mac, p_sa_mac, spi_idx, natt_egress_dest_port, &pe2wan_rule_hash, &hash_index);

		if (ret != CS_OK) {
			printk("%s: cs_tunnel_pe2wan_rule_hash_add(type=%d) failed !!, ret=%d\n", __func__, tunnel_entry->tunnel_cfg.type, ret);
			return CS_E_ERROR;
		}

		cs_tunnel_hash_index_set(tunnel_entry->tunnel_id,
					CS_TUNNEL_HASH_PE_TO_WAN,
					pe2wan_rule_hash.voq_pol_idx,
					pe2wan_rule_hash.vlan_rslt_idx,
					pe2wan_rule_hash.fwd_rslt_idx,
					pe2wan_rule_hash.hash_index);
		DBG(printk("\t PE to WAN: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
			"VLAN cmd = %d\n",
			pe2wan_rule_hash.hash_index,
			pe2wan_rule_hash.voq_pol_idx,
			pe2wan_rule_hash.fwd_rslt_idx,
			pe2wan_rule_hash.vlan_rslt_idx));

	} else { /* dir == DOWN_STREAM */

		is_natt = p_node.sa.is_natt;

		if (tunnel_entry->tunnel_cfg.type == CS_L2TPV3_IPSEC &&
			p_node.sa.tunnel == 0 /* tunnel */) {
			p_da_ip = &(p_node.sa.tunnel_daddr);
			p_sa_ip = &(p_node.sa.tunnel_saddr);
			ip_version = p_node.sa.ip_ver;
		} else {
			p_da_ip = &(tunnel_entry->tunnel_cfg.dest_addr.ip_addr);
			p_sa_ip = &(tunnel_entry->tunnel_cfg.src_addr.ip_addr);
			ip_version = tunnel_entry->tunnel_cfg.dest_addr.afi;
		}

		if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
			pppoe_session_id = nexthop.encap.port_encap.port_encap.pppoe.pppoe_session_id;
		}
		sa_id = tunnel_entry->sa_id;
		spi_idx = p_node.sa.spi;
		natt_ingress_src_port = p_node.sa.natt_ingress_src_port;
		ret = cs_tunnel_wan2pe_rule_hash_add(&(tunnel_entry->tunnel_cfg), is_natt, p_da_ip, p_sa_ip, ip_version, pppoe_session_id,
					sa_id, spi_idx, natt_ingress_src_port, &wan2pe_rule_hash);
		if (ret != CS_OK) {
			printk("%s: cs_tunnel_wan2pe_rule_hash_add(type=%d) failed !!, ret=%d\n", __func__, tunnel_entry->tunnel_cfg.type, ret);
			return CS_E_ERROR;
		}

		cs_tunnel_hash_index_set(tunnel_entry->tunnel_id,
					CS_TUNNEL_HASH_WAN_TO_PE,
					wan2pe_rule_hash.voq_pol_idx,
					wan2pe_rule_hash.vlan_rslt_idx,
					wan2pe_rule_hash.fwd_rslt_idx,
					wan2pe_rule_hash.hash_index);
		DBG(printk("\t WAN to PE: hash = %d, VoQ policer = %d, fwd_rslt = %d, "
			"VLAN cmd = %d\n",
			wan2pe_rule_hash.hash_index,
			wan2pe_rule_hash.voq_pol_idx,
			wan2pe_rule_hash.fwd_rslt_idx,
			wan2pe_rule_hash.vlan_rslt_idx));
	}

	/* check if rekey */
	switch (type) {
	case CS_IPSEC:
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
		if (p_node.previous_sa_handle > 0) {
			if (dir == UP_STREAM) {
				/* Update LPM rule (SA MAC (sa_id) in fwdrslt) */

				/* fill the source MAC with the host order */
				src_mac[4] = type & 0xFF;
				src_mac[3] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
				src_mac[2] = sa_id & 0xFF;
				src_mac[1] = 1; /* 0: dec, 1: enc */
				src_mac[0] = 0; /* reserved */


				/* Calculate CRC over the MAC SA */
				crc32 = ~(calc_crc(~0, &src_mac[0], 5));
				/* Store 8 bits of the crc in the src MAC */
				src_mac[5] = crc32 & 0xFF;

				DBG(printk("%s:%d old sa_id = %d, new sa_id = %d, SA MAC = %pM\n",
					__func__, __LINE__, old_sa_id, sa_id, src_mac));
				cs_l3_nexthop_update_by_sa_id(old_sa_id, sa_id, src_mac);
			}

			/* Send IPC to delete the old tunnel */
			/* delete hash */
			if ((ret = cs_rule_hash_delete_by_hash_index(
					device_id, old_hash_idx)) != CS_E_OK) {
				ERR(printk("%s:%d fail to delete hash, ret = %d\n",
					__func__, __LINE__, ret));
				return ret;
			}
			/* send IPCs */
#ifdef CS_IPC_ENABLED
			if (type == CS_L2TP_IPSEC || type == CS_L2TPV3_IPSEC)
				cs_l2tp_ipc_send_del_entry(dir,
						p_node.previous_sa_handle - 1);

			cs_ipsec_ipc_send_del_entry(dir,
						p_node.previous_sa_handle - 1);
#endif /* CS_IPC_ENABLED*/
		}
	default:
		break;
	}

	cs_tunnel_state_set(tunnel_entry->tunnel_id, CS_TUNNEL_ACCELERATED);
	return CS_OK;
}

int cs_l2tp_ipsec_tunnel_hash_del(
		cs_tunnel_entry_t *tunnel_entry
		)
{
	int ret;
	int idx;
	cs_l3_nexthop_t nexthop;

	if (tunnel_entry == NULL) {
		ERR(printk("%s:%d NULL pointer\n", __func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	DBG(printk("%s:%d tunnel_entry = 0x%p, dir = %d\n",
		__func__, __LINE__,
		tunnel_entry, tunnel_entry->tunnel_cfg.dir));

#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
	/* get nexthop */
	memset(&nexthop, 0, sizeof(cs_l3_nexthop_t));
	ret = cs_l3_nexthop_get(tunnel_entry->device_id,
				tunnel_entry->tunnel_cfg.nexthop_id,
				&nexthop);
	if (ret != CS_OK) {
		ERR(printk("%s:%d can't get nexthop %d\n",
					__func__, __LINE__,
					tunnel_entry->tunnel_cfg.nexthop_id));
		return CS_E_CONFLICT;
	}

	if (nexthop.nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE) {
		/*
		 * Don't need to delete tunnel hash for next hop is IP_TRANLATE
		 * But need to inform PE for its next hop sa_id
		 */
		DBG(printk("%s:%d tunnel_entry = 0x%p nexthop is IP_TRANSLATE \n", __func__, __LINE__,
			tunnel_entry));
#ifdef CS_IPC_ENABLED
		return cs_ip_translate_nexthop_del(tunnel_entry->tunnel_cfg.dir,
			tunnel_entry->tunnel_cfg.type,
			tunnel_entry->tunnel_id, nexthop.id.tunnel_id);
#else
		return CS_E_CONFLICT;
#endif

	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE */
	tunnel_entry->state = CS_TUNNEL_INIT;

	switch (tunnel_entry->tunnel_cfg.dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		idx = CS_TUNNEL_HASH_PE_TO_WAN;
		break;
	case CS_TUNNEL_DIR_INBOUND:
		idx = CS_TUNNEL_HASH_WAN_TO_PE;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		DBG(printk("%s:%d invalid dir %d\n",
			__func__, __LINE__,
			tunnel_entry->tunnel_cfg.dir));
		return CS_E_CONFLICT;
	}

	DBG(printk("\t %s: hash = %d, VoQ policer = %d,"
		" fwd_rslt = %d, VLAN cmd = %d\n",
		(idx == CS_TUNNEL_DIR_INBOUND) ? "WAN to PE" : "PE to WAN",
		tunnel_entry->fwd_hash_idx[idx],
		tunnel_entry->voq_pol_idx[idx],
		tunnel_entry->fwd_rslt_idx[idx],
		tunnel_entry->vlan_rslt_idx[idx]));

	if ((ret = cs_rule_hash_delete_by_hash_index(
			tunnel_entry->device_id,
			tunnel_entry->fwd_hash_idx[idx])) != CS_E_OK) {
		ERR(printk("%s:%d fail to delete hash, ret = %d\n",
			__func__, __LINE__, ret));
		return ret;
	}

	cs_tunnel_hash_index_set(tunnel_entry->tunnel_id, idx, 0, 0, 0, 0);

	return CS_OK;
}

/* exported APIs */

#ifdef CS_IPC_ENABLED
/*===== L2TP  =====*/
/*CS_L2TP_IPC_PE_SET_ENTRY*/
cs_status_t
cs_l2tp_ipc_send_set_entry(
		cs_sa_id_direction_t direction,
		cs_tunnel_cfg_t *p_tunnel_cfg,
		cs_uint32_t session_id,
		cs_uint8_t protocol, /*protocol in tunnel ip address*/
		cs_uint16 sa_id
		)
{
	g2_ipc_pe_l2tp_set_entry_t 	l2tp_entry_msg;
	cs_l2tp_tunnel_cfg_t *p_l2tp_cfg;

	if (p_tunnel_cfg == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	p_l2tp_cfg = &p_tunnel_cfg->tunnel.l2tp;
	if (p_l2tp_cfg == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	memset(&l2tp_entry_msg, 0, sizeof(g2_ipc_pe_l2tp_set_entry_t));
	/*TODO: fill l2tp information*/
	l2tp_entry_msg.ver = p_l2tp_cfg->ver;
	l2tp_entry_msg.len = p_l2tp_cfg->len;
	l2tp_entry_msg.tid = p_l2tp_cfg->tid;
	l2tp_entry_msg.session_id = session_id;
	l2tp_entry_msg.dest_port = p_l2tp_cfg->dest_port;
	l2tp_entry_msg.src_port = p_l2tp_cfg->src_port;
	l2tp_entry_msg.calc_udp_csum = p_l2tp_cfg->calc_udp_csum;

	l2tp_entry_msg.encap_type = p_l2tp_cfg->encap_type;
	l2tp_entry_msg.l2specific_len = p_l2tp_cfg->l2specific_len;
	l2tp_entry_msg.l2specific_type = p_l2tp_cfg->l2specific_type;
	l2tp_entry_msg.send_seq = p_l2tp_cfg->send_seq;
	l2tp_entry_msg.ns = p_l2tp_cfg->ns;
	l2tp_entry_msg.cookie_len = p_l2tp_cfg->cookie_len;
	l2tp_entry_msg.offset = p_l2tp_cfg->offset;
	memcpy(l2tp_entry_msg.cookie, p_l2tp_cfg->cookie, 8);
	memcpy(l2tp_entry_msg.l2tp_src_mac, p_l2tp_cfg->l2tp_src_mac, 6);
	memcpy(l2tp_entry_msg.peer_l2tp_src_mac, p_l2tp_cfg->peer_l2tp_src_mac, 6);

	l2tp_entry_msg.sa_idx = sa_id;

	l2tp_entry_msg.ip_ver = p_tunnel_cfg->dest_addr.afi;
	memcpy(&l2tp_entry_msg.tunnel_daddr, &p_tunnel_cfg->dest_addr.ip_addr, sizeof(cs_l3_ip_addr));
	memcpy(&l2tp_entry_msg.tunnel_saddr, &p_tunnel_cfg->src_addr.ip_addr, sizeof(cs_l3_ip_addr));

	if(l2tp_entry_msg.ip_ver == CS_IPV4)
	{
		l2tp_entry_msg.ip_checksum = cs_calc_ipv4_checksum(
								l2tp_entry_msg.tunnel_saddr.ipv4_addr,
								l2tp_entry_msg.tunnel_daddr.ipv4_addr,
								protocol);
	}

	DBG(printk("%s:%d ver = 0x%x\n", __func__, __LINE__, l2tp_entry_msg.ver));
	DBG(printk("%s:%d len = %d\n", __func__, __LINE__, l2tp_entry_msg.len));
	DBG(printk("%s:%d tid = 0x%x\n", __func__, __LINE__, l2tp_entry_msg.tid));
	DBG(printk("%s:%d sid = 0x%x\n", __func__, __LINE__, l2tp_entry_msg.session_id));
	DBG(printk("%s:%d dest_port = 0x%x\n", __func__, __LINE__, l2tp_entry_msg.dest_port));
	DBG(printk("%s:%d src_port = 0x%x\n", __func__, __LINE__, l2tp_entry_msg.src_port));
	DBG(printk("%s:%d sa_idx = %d\n", __func__, __LINE__, l2tp_entry_msg.sa_idx));
	DBG(printk("%s:%d ip_checksum = %d\n", __func__, __LINE__, l2tp_entry_msg.ip_checksum));
	DBG(printk("tunnel_saddr dump:\n"));
	DBG(cs_dump_data(&l2tp_entry_msg.tunnel_saddr, sizeof(cs_l3_ip_addr)));
	DBG(printk("\n"));
	DBG(printk("tunnel_daddr dump:\n"));
	DBG(cs_dump_data(&l2tp_entry_msg.tunnel_daddr, sizeof(cs_l3_ip_addr)));
	DBG(printk("\n"));
	if (p_tunnel_cfg->type == CS_L2TPV3 || p_tunnel_cfg->type == CS_L2TPV3_IPSEC) {
		DBG(printk("\t encap_type = %d\n", l2tp_entry_msg.encap_type));
		DBG(printk("\t l2specific_len = %d\n", l2tp_entry_msg.l2specific_len));
		DBG(printk("\t l2specific_type = %d\n", l2tp_entry_msg.l2specific_type));
		DBG(printk("\t send_seq = %d\n", l2tp_entry_msg.send_seq));
		DBG(printk("\t ns = 0x%x\n", l2tp_entry_msg.ns));
		DBG(printk("\t cookie_len = %d\n", l2tp_entry_msg.cookie_len));
		DBG(printk("\t cookie = %02x %02x %02x %02x %02x %02x %02x %02x\n",
			l2tp_entry_msg.cookie[0], l2tp_entry_msg.cookie[1],
			l2tp_entry_msg.cookie[2], l2tp_entry_msg.cookie[3],
			l2tp_entry_msg.cookie[4], l2tp_entry_msg.cookie[5],
			l2tp_entry_msg.cookie[6], l2tp_entry_msg.cookie[7]));
		DBG(printk("\t offset = %d\n", l2tp_entry_msg.offset));
		DBG(printk("\t l2tp_src_mac = %02x %02x %02x %02x %02x %02x\n",
			l2tp_entry_msg.l2tp_src_mac[0],
			l2tp_entry_msg.l2tp_src_mac[1],
			l2tp_entry_msg.l2tp_src_mac[2],
			l2tp_entry_msg.l2tp_src_mac[3],
			l2tp_entry_msg.l2tp_src_mac[4],
			l2tp_entry_msg.l2tp_src_mac[5]));
		DBG(printk("\t peer_l2tp_src_mac = %02x %02x %02x %02x %02x %02x\n",
			l2tp_entry_msg.peer_l2tp_src_mac[0],
			l2tp_entry_msg.peer_l2tp_src_mac[1],
			l2tp_entry_msg.peer_l2tp_src_mac[2],
			l2tp_entry_msg.peer_l2tp_src_mac[3],
			l2tp_entry_msg.peer_l2tp_src_mac[4],
			l2tp_entry_msg.peer_l2tp_src_mac[5]));
	}
	return cs_tunnel_ipc_send(direction, CS_L2TP_IPC_PE_SET_ENTRY, &l2tp_entry_msg,
					sizeof(g2_ipc_pe_l2tp_set_entry_t));
}

/*CS_L2TP_IPC_PE_DEL_ENTRY*/
cs_status_t
cs_l2tp_ipc_send_del_entry(
		cs_sa_id_direction_t direction,
		cs_uint16 sa_id
		)
{
	g2_ipc_pe_l2tp_del_entry_t l2tp_entry_msg;

	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));

	memset(&l2tp_entry_msg, 0, sizeof(g2_ipc_pe_l2tp_del_entry_t));

	l2tp_entry_msg.sa_idx = sa_id;

	return cs_tunnel_ipc_send(direction, CS_L2TP_IPC_PE_DEL_ENTRY, &l2tp_entry_msg,
					sizeof(g2_ipc_pe_l2tp_del_entry_t));
}

cs_status_t cs_l2tp_ipc_rcv_set_entry_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_l2tp_set_entry_ack_t *msg;
	cs_tunnel_entry_t *t, t_node;;
	cs_uint16_t sa_id;
	cs_status_t ret;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	t = &t_node;
	msg = (g2_ipc_pe_l2tp_set_entry_ack_t * )msg_data;
	sa_id = msg->sa_idx;

	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<L2TP>: Receive set entry ack from PE%d\n", peer.cpu_id - 1));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));

	ret = cs_tunnel_get_by_sa_id(sa_id,
			(peer.cpu_id == CPU_RCPU0) ? DOWN_STREAM : UP_STREAM,
			t);

	if(ret != CS_OK) {
		ERR(printk("%s:%d Can't find valid tunnel entry\n",
			__func__, __LINE__));
		return ret;
	}

	/* create hash */
	switch (t->tunnel_cfg.type) {
	case CS_L2TP:
	case CS_L2TP_IPSEC:
	case CS_L2TPV3:
	case CS_L2TPV3_IPSEC:
		ret = cs_l2tp_ipsec_tunnel_hash_create(t);

		if(ret != CS_OK) {
			ERR(printk("%s:%d Can't create hash for ipsec tunnel.\n",
				__func__, __LINE__));
			return ret;
		}
		break;
	default:
		break;
	}

	return CS_OK;
}

cs_status_t cs_l2tp_ipc_rcv_del_entry_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_l2tp_del_entry_ack_t *msg;
	cs_uint16_t sa_id;
	cs_uint16_t ofld_seq;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_l2tp_del_entry_ack_t * )msg_data;
	sa_id = msg->sa_idx;
	ofld_seq = msg->ofld_seq;

	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<L2TP>: Receive del entry ack from PE%d", peer.cpu_id - 1));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));
	printk("%s:%d ##### ofld_seq = %d #####\n", __func__, __LINE__, ofld_seq);

	return CS_OK;
}

#endif /* CS_IPC_ENABLED */

void cs_hw_accel_l2tp_ipsec_init(void)
{
	memset(&cs_tunnel_cb, 0, sizeof(cs_l2tp_cb_t));
	cs_tunnel_cb.curr_tu_id = TID_L2TP_IPSEC_BASE;
	spin_lock_init(&cs_l2tp_lock);

#if defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL) || \
	defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL)
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL

	/* hw accel_manger */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_L2TP_CTRL,
						cs_l2tp_proc_callback);


#endif
}

void cs_hw_accel_l2tp_ipsec_exit(void)
{
}


