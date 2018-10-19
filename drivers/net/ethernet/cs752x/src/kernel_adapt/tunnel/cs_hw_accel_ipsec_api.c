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

#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
#include "cs_core_hmu.h"

extern struct xfrm_mgr cs_ipsec_xfrm_mgr;
#endif

#define ERR(x)	(x)
#define SKIP(x) { }

extern cs_l2tp_cb_t cs_tunnel_cb;
extern spinlock_t cs_l2tp_lock;
cs_ipsec_spd_t cs_ipsec_spd[CS_IPSEC_SPD_MAX];
spinlock_t cs_ipsec_lock;


/* utilities */
cs_status_t cs_ipsec_policy_node_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_OUT	cs_ipsec_policy_node_t			*p_node)
{
	int idx;
	cs_ipsec_policy_node_t *p;
	unsigned long flags;

	SKIP(printk("%s:%d device id = %d, spd_handle = %d, "
		"policy_handle = %d, p_node= 0x%p\n",
		__func__, __LINE__,
		device_id, spd_handle, policy_handle, p_node));

	if (p_node == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	
	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}
	
	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->policy_handle == policy_handle) {
			/* hit */
			*p_node = *p;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}

cs_status_t cs_ipsec_policy_node_get_by_sa_id(
	CS_IN	cs_sa_id_direction_t			dir,
	CS_IN	cs_uint16_t				sa_id,
	CS_OUT	cs_ipsec_policy_node_t			*p_node)
{
	int idx;
	cs_uint32_t sa_handle;
	cs_ipsec_policy_node_t *p;
	unsigned long flags;

	DBG(printk("%s:%d dir = %d, sa_id = %d, p_node= 0x%p\n",
		__func__, __LINE__,
		dir, sa_id, p_node));

	if (p_node == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	
	if (dir > DOWN_STREAM) {
		ERR(printk("%s:%d invalid dir %d\n",
			__func__, __LINE__, dir));
		return CS_E_NOT_FOUND;
	}
	idx = dir;
	sa_handle = sa_id + 1;
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}
	
	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->valid == CS_TRUE &&
			p->sa_valid == CS_TRUE &&
			p->sa_handle == sa_handle) {
			/* hit */
			*p_node = *p;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}

cs_status_t cs_ipsec_policy_node_get_by_spi (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				spi,
	CS_OUT	cs_ipsec_policy_node_t			*p_node)
{
	int idx;
	cs_ipsec_policy_node_t *p;
	unsigned long flags;

	SKIP(printk("%s:%d device id = %d, spd_handle = %d, "
		"spi = 0x%x, p_node = 0x%p\n",
		__func__, __LINE__,
		device_id, spd_handle, spi, p_node));

	if (p_node == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		SKIP(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}

	while (p) {
		if (p->valid == CS_TRUE &&
			p->sa_valid == CS_TRUE &&
			spi == p->sa.spi) {
			/* hit */
			*p_node = *p;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
			
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	DBG(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}

cs_status_t cs_ipsec_policy_tunnel_link(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_IN	cs_uint32_t				tunnel_id,
	CS_IN	cs_ip_address_t				*src_ip,
	CS_IN	cs_ip_address_t				*dest_ip)
{
	int idx;
	cs_ipsec_policy_node_t *p;
	int i;
	unsigned long flags;

	SKIP(printk("%s:%d device id = %d, spd_handle = %d, "
		"policy_handle = %d, tunnel_id= %d\n",
		__func__, __LINE__,
		device_id, spd_handle, policy_handle, tunnel_id));

	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;

	if (src_ip == NULL || dest_ip == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	
	if ((tunnel_id < TID_L2TP_IPSEC_BASE) || (tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d tunnel_id %d is invalid.\n",
			__func__, __LINE__, tunnel_id));
		return CS_E_NOT_FOUND;
	}
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->policy_handle == policy_handle) {
			/* hit */
			p->tunnel_id = tunnel_id;
#ifndef CONFIG_CS75XX_HW_ACCEL_L2TPV3_IPSEC			
			p->sa.ip_ver = dest_ip->afi;
			if (p->sa.ip_ver == 0 /* IPv4 */) {
				p->sa.tunnel_saddr.addr[0] =
					src_ip->ip_addr.ipv4_addr;
				p->sa.tunnel_daddr.addr[0] =
					dest_ip->ip_addr.ipv4_addr;
			} else {	/* IPv6 */
				for (i = 0; i < 4; i++ ) {
					p->sa.tunnel_saddr.addr[i] =
						src_ip->ip_addr.ipv6_addr[i];
					p->sa.tunnel_daddr.addr[i] =
						dest_ip->ip_addr.ipv6_addr[i];
				}
			}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TPV3_IPSEC */			
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}

cs_status_t cs_ipsec_policy_xfrm_link(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_IN	void					*xfrm_state)
{
	int idx;
	cs_ipsec_policy_node_t *p;
	unsigned long flags;

	SKIP(printk("%s:%d device id = %d, spd_handle = %d, "
		"policy_handle = %d, xfrm_state= 0x%p\n",
		__func__, __LINE__,
		device_id, spd_handle, policy_handle, xfrm_state));

	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;

	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->policy_handle == policy_handle) {
			/* hit */
			p->xfrm_state = xfrm_state;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}


static cs_status_t cs_ipsec_sa_rekey (
	cs_sa_id_direction_t 				dir,
	cs_ipsec_policy_node_t				*p)
{
	cs_tunnel_entry_t *t, t_node;
	cs_tunnel_type_t type = CS_TUN_TYPE_MAX;
	cs_uint8_t protocol;
	cs_uint16_t sa_id, old_sa_id;
	cs_uint32_t session_id;

	DBG(printk("%s:%d dir = %d, p = 0x%p\n",
		__func__, __LINE__, dir, p));

	/************************************************
	
	 UP:
	 (1)	Send IPC to PE to arrange a new tunnel
		 1.1   call cs_ipsec_ipc_send_set_sadb()
		 1.2   call cs_l2tp_ipc_send_set_entry() if it is L2TP over IPSec.
	 (2)	Backup old hash index and add a rule hash from PE to WAN port (SPI)
	 (3)	Update LPM rule (SA MAC (sa_id) in fwdrslt) (need new API)
	 (4)	Send IPC to delete the old tunnel
		 4.1   call cs_l2tp_ipc_send_del_entry() if it is L2TP over IPSec.
		 4.2   call cs_ipsec_ipc_send_del_entry()
	 (6)	Delete the old rule hash
	 
	 DOWN:
	 (1)	Send IPC to PE to arrange a new tunnel
		 1.1   call cs_ipsec_ipc_send_set_sadb()
		 1.2   call cs_l2tp_ipc_send_set_entry() if it is L2TP over IPSec.
	 (2)	Backup old hash index and add rule hash from WAN port to PE(SPI, SA MAC (sa_id))
	 (3)	Send IPC to delete the old tunnel
		 3.1   call cs_l2tp_ipc_send_del_entry() if it is L2TP over IPSec.
		 3.2   call cs_ipsec_ipc_send_del_entry()
	 (4)	Delete the old rule hash

	 ************************************************/

	sa_id = p->sa_handle - 1;
	old_sa_id = p->previous_sa_handle - 1;
	t = &t_node;
	
	/* check if it is IPSec tunnel or L2TP over IPSec */
	if (cs_tunnel_get_by_sa_id(sa_id, dir, t) == CS_OK) {
		type = t->tunnel_cfg.type;
	} else {
		ERR(printk("%s:%d invalid tunnel id %d, sa_id %d\n",
			__func__, __LINE__, p->tunnel_id, sa_id));
		return CS_E_CONFLICT;
	}

	if (type == CS_L2TP_IPSEC) {
		if (cs_l2tp_session_id_get(t->tunnel_id, &session_id) != CS_OK) {
			ERR(printk("%s:%d no valid session id\n",
				__func__, __LINE__));
			return CS_E_CONFLICT;
		}
	} else if (type == CS_L2TPV3_IPSEC) {
		session_id = t->tunnel_cfg.tunnel.l2tp.session_id;
	}
#ifdef CS_IPC_ENABLED
	/* Send IPC to PE to arrange a new tunnel */
	cs_ipsec_ipc_send_set_sadb(dir, p, sa_id);
#endif /* CS_IPC_ENABLED */

	if (type == CS_L2TP_IPSEC || type == CS_L2TPV3_IPSEC) {
		if (p->sa.tunnel == 0 /* Tunnel mode */)
			protocol = CS_IPPROTO_L2TP;
		else if (p->sa.is_natt != 0)
			protocol = IPPROTO_UDP;
		else
			protocol = p->sa.proto ? IPPROTO_AH : IPPROTO_ESP;

#ifdef CS_IPC_ENABLED
		cs_l2tp_ipc_send_set_entry(dir, &t->tunnel_cfg,
				session_id, protocol, sa_id);
#endif /* CS_IPC_ENABLED */
	}

	/* cs_l2tp_ipsec_tunnel_hash_create() is called in IPC ACK handler */
	/* cs_rule_hash_delete_by_hash_index(), 
	 * cs_l2tp_ipc_send_del_entry(),
	 * cs_ipsec_ipc_send_del_entry(),
	 * and cs_l3_nexthop_update_by_sa_id() are called by 
	 * cs_l2tp_ipsec_tunnel_hash_create().
	 */
	return CS_OK;	
}

cs_status_t cs_ipsec_sel_ip_check(
	CS_IN cs_uint32_t				tunnel_id,
	CS_IN cs_ip_address_t				*ip_prefix,
	CS_OUT cs_boolean_t 				*is_valid)
{
	cs_tunnel_entry_t *t, t_node;
	cs_ipsec_policy_node_t p_node;
	int ret;
	int i, j;
	cs_ipsec_selector_t *s;
	unsigned long flags;

	if (ip_prefix == NULL || is_valid == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	*is_valid = CS_FALSE;

	if ((tunnel_id < TID_L2TP_IPSEC_BASE) || (tunnel_id >= TID_L2TP_IPSEC_MAX)) {
		ERR(printk("%s:%d invalid tunnel_id %d\n",
			__func__, __LINE__, tunnel_id));
		return CS_E_CONFLICT;
	}

	memset(&t_node, 0, sizeof(cs_tunnel_entry_t));
	spin_lock_irqsave(&cs_l2tp_lock, flags);
	t = cs_tunnel_cb.tu_h;
	while (t) {
		/* search by tunnel_id */
		if (t->tunnel_id == tunnel_id) {
			/* hit */
			t_node = *t;
			break;
		}
		t = t->next;
	}
	spin_unlock_irqrestore(&cs_l2tp_lock, flags);

	if (t == NULL) {
		ERR(printk("%s:%d invalid tunnel_id %d\n",
			__func__, __LINE__, tunnel_id));
		return CS_E_NOT_FOUND;
	}
	t = &t_node; /* use local copy instead of the tree node */

	if (t->tunnel_cfg.type == CS_IPSEC) {
	
		if (t->tunnel_cfg.tunnel.ipsec.ipsec_policy == 0) {
			ERR(printk("%s:%d invalid ipsec_policy\n",
						__func__, __LINE__));
			return CS_E_CONFLICT;
		}

		switch (t->tunnel_cfg.dir) {
		case CS_TUNNEL_DIR_OUTBOUND:
			/* check selectors of outbound IPSec */
			
			/* get IPSec policy node */
			ret = cs_ipsec_policy_node_get(t->device_id,
					0,
					t->tunnel_cfg.tunnel.ipsec.ipsec_policy,
					&p_node);
			if (ret != CS_OK) {
				ERR(printk("%s:%d can't get IPSec policy and sa\n",
							__func__, __LINE__));
				return CS_E_CONFLICT;
			}
			/* make sure it is IPSec tunnel mode */
			if (p_node.sa.tunnel != 0) {
				ERR(printk("%s:%d not IPSec tunnel mode\n",
							__func__, __LINE__));
				return CS_E_CONFLICT;
			}

			i = p_node.policy.selector_count;
			s = &p_node.policy.selector_array[0];
			for (j = 0; j < i; j++ ) {
				if (cs_ip_addr_cmp(&s[j].dst_ip, ip_prefix) >= 0) {
					*is_valid = CS_TRUE;
					return CS_OK;
				}
			}
			return CS_OK;
		case CS_TUNNEL_DIR_INBOUND:
			/* ignore selectors of inbound IPSec */
			*is_valid = CS_TRUE;
			return CS_OK;
		case CS_TUNNEL_DIR_TWO_WAY:
		default:
			DBG(printk("%s:%d invalid dir %d\n",
				__func__, __LINE__, t->tunnel_cfg.dir));
			return CS_E_CONFLICT;
		}
	} else {
		ERR(printk("%s:%d unexpected tunnel type %d of tunnel id %d\n",
				__func__, __LINE__,
				t->tunnel_cfg.type,
				t->tunnel_id));
		return CS_E_CONFLICT;
	}


}
EXPORT_SYMBOL(cs_ipsec_sel_ip_check);

cs_status_t cs_ipsec_sel_ip_check_2(
	CS_IN cs_tunnel_cfg_t				*t_cfg,
	CS_IN cs_ipsec_policy_t 			*p,
	CS_OUT cs_boolean_t 				*is_valid)
{
	int i;
	*is_valid = CS_FALSE;

	for (i = 0; i < p->selector_count; i++) {
		if (((p->selector_array[i].protocol == 0) || (p->selector_array[i].protocol == IPPROTO_UDP) || (p->selector_array[i].protocol == CS_IPPROTO_L2TP)) &&
			((p->selector_array[i].src_port == 0) || (p->selector_array[i].src_port == t_cfg->tunnel.l2tp.src_port)) &&
			((p->selector_array[i].dst_port == 0) || (p->selector_array[i].dst_port == t_cfg->tunnel.l2tp.dest_port)) &&
			(cs_ip_addr_cmp(&p->selector_array[i].src_ip, &t_cfg->src_addr) >= 0) &&
			(cs_ip_addr_cmp(&p->selector_array[i].dst_ip, &t_cfg->dest_addr) >= 0)) {
			*is_valid = CS_TRUE;
			return CS_OK;
		}
	}
	return CS_OK;
}

cs_uint8_t  ipsec_ni_enc_selector_check(cs_ipsec_policy_node_t *p_node,struct sk_buff *skb) 
{
	int i, j;
	int match = 1;
	cs_uint8_t *ip_ptr = skb->data;
	cs_uint32_t selector_ipv4addr, src_ipaddr;
	cs_uint16_t port = 0;
	cs_uint8_t  selector_protocol;

	if((NULL == p_node)||(NULL == skb)){
		printk("%s:%d error in NULL pointer. \r\n",__func__, __LINE__);
		return 0; /*not match!!*/	
	}
			
	for (i = 0; i < p_node->policy.selector_count; i++) {
		match = 1;
		selector_protocol = (p_node->policy.selector_array[i].protocol); 
		
		if (CS_IPV4 == p_node->policy.selector_array[i].src_ip.afi) {
			selector_ipv4addr = (p_node->policy.selector_array[i].src_ip.ip_addr.ipv4_addr);
			if ((p_node->src_ip_mask[i].mask[0] != 0) || (selector_ipv4addr!= 0))
			{		
				src_ipaddr = (ip_ptr[12]&0xff)|((ip_ptr[13]<<8)&0xff00)
				             |((ip_ptr[14]<<16)&0xff0000)|((ip_ptr[15]<<24)&0xff000000);	
				src_ipaddr = src_ipaddr & p_node->src_ip_mask[i].mask[0];
				if ( selector_ipv4addr != src_ipaddr) {
					match = 0;
				}
			}	

			if ((match == 1) && (selector_protocol != 0)) {
				if (selector_protocol != ip_ptr[9])
					match = 0;
			}

			if ((match == 1) && (p_node->policy.selector_array[i].src_port != 0)) {
				if ((ip_ptr[9] == 0x11) || (ip_ptr[9] == 0x6))  /*UDP/TCP*/
					port = ip_ptr[20] * 16 + ip_ptr[21];
				else
					match = 0;

				if (p_node->policy.selector_array[i].src_port != port)
					match = 0;
			}

			if ((match == 1) && (p_node->policy.selector_array[i].dst_port != 0)) {
				if ((ip_ptr[9] == 0x11) || (ip_ptr[9] == 0x6))  /*UDP/TCP*/
					port = ip_ptr[22] * 16 + ip_ptr[23];
				else
					match = 0;

				if (p_node->policy.selector_array[i].dst_port != port)
					match = 0;
			}

			if (match == 1)
				return 1;											
		}else{
			// IPV6
			if ((p_node->src_ip_mask[i].mask[0] != 0) 
			    || 
			    (p_node->policy.selector_array[i].src_ip.ip_addr.ipv6_addr[0]!= 0))
			{
				for (j = 0; j < 4; j ++) {
					src_ipaddr = ((ip_ptr[8+j*4])|(ip_ptr[9+j*4]<<8)|(ip_ptr[10+j*4]<<16)|(ip_ptr[11+j*4]<<24))
					              &
						     (p_node->src_ip_mask[i].mask[j]);
					if (p_node->policy.selector_array[i].src_ip.ip_addr.ipv6_addr[j] != src_ipaddr) {
						match = 0;
						break;
					}	
				}				
			}
			
			if ((match == 1) && (selector_protocol != 0)) {
				if (selector_protocol != ip_ptr[6]) // ? ip_ptr[9]
					match = 0;
			}

			if ((match == 1) && (p_node->policy.selector_array[i].src_port != 0)) {
				if ((ip_ptr[9] == 0x11) || (ip_ptr[9] == 0x6))  /*UDP/TCP*/
					port = ip_ptr[40] * 16 + ip_ptr[41];
				else
					match = 0;

				if (p_node->policy.selector_array[i].src_port != port)
					match = 0;
			}

			if ((match == 1) && (p_node->policy.selector_array[i].dst_port != 0)) {
				if ((ip_ptr[9] == 0x11) || (ip_ptr[9] == 0x6))  /*UDP/TCP*/
					port = ip_ptr[42] * 16 + ip_ptr[43];
				else
					match = 0;

				if (p_node->policy.selector_array[i].dst_port != port)
					match = 0;
			}
			if (match == 1)
				return 1;							
		}		
	}

	return 0; /*not match!!*/		
}

cs_status_t cs_ipsec_tunnel_handle(
	struct sk_buff 					*skb)
{
	struct ethhdr *eth;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_uint16_t sa_id;
	cs_tunnel_entry_t *t, t_node;
	cs_ipsec_policy_node_t p_node;
	cs_status_t ret;
	cs_sa_id_direction_t dir;

	eth = eth_hdr(skb);
	sa_id = eth->h_source[3];
	dir = eth->h_source[4];
	t = &t_node;

	/* h_source[4] DIR = 1(WAN to LAN)  0(LAN to WAN) */
	/* only handle h_source[4] DIR = 1 esle drop the packet */
	if (dir == DOWN_STREAM) {
		DBG(printk("%s:%d New inbound IPSec flow, len = %d, sa_id = %d\n",
			__func__, __LINE__, skb->len, sa_id));
		DBG(cs_dump_data((void *)eth, skb->len));
	} else {
		DBG(printk("%s:%d Receive a upstream packet. "
			"Can't hit PE to WAN hash\n",
			__func__, __LINE__));
		DBG(cs_dump_data((void *)eth, skb->len));
		goto SKB_HANDLED;
	}


#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
	if (cs_ipsec_ctrl_enable() == FALSE)
		return CS_OK;

	ret = cs_tunnel_get_by_sa_id(sa_id, dir, t);

	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d Can't find valid tunnel entry\n",
			__func__, __LINE__));
		goto SKB_HANDLED;;	
	}			 

	if (t->state != CS_TUNNEL_ACCELERATED) {
		DBG(printk("%s:%d IPSec tunnel is not ready, "
			"tunnel id = %d\n",
			__func__,__LINE__, t->tunnel_id));
		goto SKB_HANDLED;;
	}
	
	ret = cs_ipsec_policy_node_get_by_sa_id(dir,sa_id,&p_node);
	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d can not find policy node\n",
			__func__,__LINE__));
		goto SKB_HANDLED;;
	}

	if (cs_cb) {
		cs_cb->common.module_mask |= CS_MOD_MASK_IPSEC;

		/* record SPI, IP version, and dir in cs_cb */
		cs_cb->common.vpn_dir = dir;
		cs_cb->common.vpn_ip_ver = p_node.sa.ip_ver;
		cs_cb->input.vpn_h.ah_esp.spi = p_node.sa.spi;
	} else {
		DBG(printk("%s:%d: No CS_CB\n",
			__func__,__LINE__));
		goto SKB_HANDLED;
	}

	/* need to fill sec_path in skb,
	 * such that the Kernel could realize
	 * this packet when we do netif_rx */
	ret = cs_ipsec_insert_sec_path(skb, &p_node);
	if (ret != CS_E_OK) {
		DBG(printk("%s:%d: Can't insert sec_path\n",
			__func__,__LINE__));
		goto SKB_HANDLED;
	}

	
#else
	ret = cs_tunnel_get_by_sa_id(sa_id, dir, t);

	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d Can't find valid tunnel entry\n",
			__func__, __LINE__));
		goto SKB_HANDLED;;	
	}			 

	if (t->state != CS_TUNNEL_ACCELERATED) {
		DBG(printk("%s:%d IPSec tunnel is not ready, "
			"tunnel id = %d\n",
			__func__,__LINE__, t->tunnel_id));
		goto SKB_HANDLED;;
	}

	ret = cs_ipsec_policy_node_get_by_sa_id(dir,sa_id,&p_node);
	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d can not find policy node\n",
			__func__,__LINE__));
		goto SKB_HANDLED;;
	}

	if (CS_IPSEC_POLICY_ACTION_DISCARD ==
			p_node.policy.policy_action) {		
		/* Discard */
		DBG(printk("%s:%d CS_IPSEC_POLICY_ACTION_DISCARD\n",
			__func__,__LINE__));
		if(ipsec_ni_enc_selector_check(&p_node,skb)) {
			goto SKB_HANDLED;
		}
	}


#endif /* CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL */

	return CS_OK;
	
SKB_HANDLED:	
	dev_kfree_skb(skb);
	return CS_E_ERROR;
}

cs_status_t cs_l2tp_ipsec_tunnel_handle(
	struct sk_buff 					*skb)
{
	struct ethhdr *eth;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_uint16_t sa_id;
	cs_tunnel_entry_t *t, t_node;
	cs_ipsec_policy_node_t p_node;
	cs_status_t ret;
	cs_sa_id_direction_t dir;

	eth = eth_hdr(skb);
	sa_id = eth->h_source[3];
	dir = eth->h_source[4];
	t = &t_node;

	/* h_source[4] DIR = 1(WAN to LAN)  0(LAN to WAN) */
	/* only handle h_source[4] DIR = 1 esle drop the packet */
	if (dir == DOWN_STREAM) {
		DBG(printk("%s:%d New inbound L2TP over IPSec flow,"
			" len = %d, sa_id = %d\n",
			__func__, __LINE__, skb->len, sa_id));
		DBG(cs_dump_data((void *)eth, skb->len));
	} else {
		DBG(printk("%s:%d Receive a upstream packet. "
			"Can't hit PE to WAN hash\n",
			__func__, __LINE__));
		DBG(cs_dump_data((void *)eth, skb->len));
		goto SKB_HANDLED;
	}


#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) &&\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
	if (cs_ipsec_ctrl_enable() == FALSE || cs_l2tp_ctrl_enable() == FALSE)
		return CS_OK;

	ret = cs_tunnel_get_by_sa_id(sa_id, dir, t);

	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d Can't find valid tunnel entry\n",
			__func__, __LINE__));
		goto SKB_HANDLED;;	
	}			 

	if (t->state != CS_TUNNEL_ACCELERATED) {
		DBG(printk("%s:%d IPSec tunnel is not ready, "
			"tunnel id = %d\n",
			__func__,__LINE__, t->tunnel_id));
		goto SKB_HANDLED;;
	}
	
	ret = cs_ipsec_policy_node_get_by_sa_id(dir,sa_id,&p_node);
	if (CS_E_NOT_FOUND == ret) {
		DBG(printk("%s:%d can not find policy node\n",
			__func__,__LINE__));
		goto SKB_HANDLED;;
	}

	if (cs_cb) {
		cs_cb->common.module_mask |= 
					CS_MOD_MASK_IPSEC | CS_MOD_MASK_L2TP;

		/* record SPI, IP version, and dir in cs_cb */
		cs_cb->common.vpn_dir = dir;
		cs_cb->common.vpn_ip_ver = p_node.sa.ip_ver;
		cs_cb->input.vpn_h.ah_esp.spi = p_node.sa.spi;
		cs_cb->common.tunnel_id = t->tunnel_cfg.tunnel.l2tp.tid;

		cs_l2tp_session_id_get(t->tunnel_id, &cs_cb->common.session_id);
	} else {
		DBG(printk("%s:%d: No CS_CB\n",
			__func__,__LINE__));
		goto SKB_HANDLED;
	}

	/* need to fill sec_path in skb,
	 * such that the Kernel could realize
	 * this packet when we do netif_rx */
	ret = cs_ipsec_insert_sec_path(skb, &p_node);
	if (ret != CS_E_OK) {
		DBG(printk("%s:%d: Can't insert sec_path\n",
			__func__,__LINE__));
		goto SKB_HANDLED;
	}

	
#endif /* CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL && 
	  CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL */

	return CS_OK;
	
SKB_HANDLED:	
	dev_kfree_skb(skb);
	return CS_E_ERROR;
}



/* exported APIs */

cs_status_t cs_ipsec_init (
	CS_IN 	cs_dev_id_t				device_id)
{
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
	cs_uint32_t tmp;
	cs_core_hmu_t cs_ipsec_hmu_entry;
	cs_core_hmu_value_t cs_ipsec_hmu_value;
#endif
	
	DBG(printk("%s:%d device id = %d\n",
		__func__, __LINE__, device_id));
	
	memset(&cs_ipsec_spd, 0, sizeof(cs_ipsec_spd_t) * CS_IPSEC_SPD_MAX);
	spin_lock_init(&cs_ipsec_lock);

#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
	cs_l3_route_init_0(device_id);
	cs_ipsec_spd_add(device_id, CS_IPSEC_SPD_OUTBOUND, 0, &tmp);
	cs_ipsec_spd_add(device_id, CS_IPSEC_SPD_INBOUND, 1, &tmp);

	/* delete IPSec tunnel according to xfrm event */
	xfrm_register_km(&cs_ipsec_xfrm_mgr);

	
	/* hw accel_manger */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPSEC_CTRL,
						cs_ipsec_proc_callback);
	
	/*for core hmu */
	memset(&cs_ipsec_hmu_entry, 0, sizeof(cs_core_hmu_t));
	memset(&cs_ipsec_hmu_value, 0, sizeof(cs_core_hmu_value_t));
	cs_ipsec_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	cs_ipsec_hmu_value.mask = 0x08;
	cs_ipsec_hmu_value.value.swid64 = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPSEC);
	cs_ipsec_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;

	cs_ipsec_hmu_entry.value_mask = &cs_ipsec_hmu_value;
	cs_ipsec_hmu_entry.callback = NULL;
	cs_core_hmu_register_watch(&cs_ipsec_hmu_entry);
#endif

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_ipsec_init);

cs_status_t cs_ipsec_spd_add (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_ipsec_spd_direction_t		direction,
	CS_IN	cs_uint32_t				spd_id,
	CS_OUT	cs_uint32_t				*spd_handle)
{
	int idx;
	unsigned long flags;
	
	DBG(printk("%s:%d device id = %d, direction = %d, "
		"spd_id = %d, spd_handle = 0x%p\n",
		__func__, __LINE__, device_id, direction, spd_id, spd_handle));

	if (spd_handle == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	
	if (direction > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid direction\n",
			__func__, __LINE__));
		return CS_E_DIR;
	}
	idx = direction; /* direction is 0 or 1 */

	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_TRUE) {
		*spd_handle = idx;
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		DBG(printk("%s:%d index = %d, spd already exists.\n",
			__func__, __LINE__, idx));
		return CS_E_NONE;
	}

	memset(&cs_ipsec_spd[idx], 0, sizeof(cs_ipsec_spd_t));

	cs_ipsec_spd[idx].valid = CS_TRUE;
	cs_ipsec_spd[idx].device_id = device_id;
	cs_ipsec_spd[idx].direction = direction;
	cs_ipsec_spd[idx].spd_id = spd_id;
	cs_ipsec_spd[idx].curr_policy_handle = 0;
	*spd_handle = idx;
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_ipsec_spd_add);


cs_status_t cs_ipsec_spd_delete (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle)
{
	int idx;
	unsigned long flags;
	
	DBG(printk("%s:%d device id = %d, spd_handle = %d\n",
		__func__, __LINE__, device_id, spd_handle));

	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle; /* spd_handle is assigned as direction (0 or 1) */
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is already empty.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}


	if (cs_ipsec_spd[idx].p_h != NULL) {
		ERR(printk("%s:%d still have %d policies\n",
			__func__, __LINE__, cs_ipsec_spd[idx].policy_cnt));
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		return CS_E_CONFLICT;
	}

	cs_ipsec_spd[idx].valid = CS_FALSE;
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_ipsec_spd_delete);



cs_status_t cs_ipsec_policy_add (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_ipsec_policy_t			*policy,
	CS_OUT	cs_uint32_t				*policy_handle)
{
	int idx;
	cs_ipsec_policy_node_t *p_node, *p, *pre_p;
	int i;
	unsigned long flags;

	DBG(printk("%s:%d device id = %d, spd_handle = %d, "
		"policy = 0x%p, policy_handle = 0x%p\n",
		__func__, __LINE__,
		device_id, spd_handle, policy, policy_handle));

	if (policy == NULL || policy_handle == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;
	
	p_node = cs_zalloc(sizeof(cs_ipsec_policy_node_t), GFP_KERNEL);
	if (p_node == NULL) {
		ERR(printk("%s:%d out of memory\n", __func__, __LINE__));
		return CS_E_RESOURCE;
	}

	p_node->valid = CS_TRUE;
	p_node->device_id = device_id;
	p_node->spd_handle = spd_handle;
	memcpy(&p_node->policy, policy, sizeof(cs_ipsec_policy_t));

	/* convert from policy.selector_array[].src_ip to src_ip_mask */
	/* convert from policy.selector_array[].dst_ip to dst_ip_mask */
	for (i = 0; i < policy->selector_count; i++) {
		cs_ip2mask(&policy->selector_array[i].src_ip,
					&p_node->src_ip_mask[i]);
		cs_ip2mask(&policy->selector_array[i].dst_ip,
					&p_node->dst_ip_mask[i]);
	}

	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		cs_free(p_node);
		return CS_E_NOT_FOUND;
	}
	
	/* curr_policy_handle is a increasing integer which starts from 1.
	 * curr_policy_handle: 0: invalid, 
	 *                     1,2,3,....: an unique ID
	 */
	p_node->policy_handle = ++cs_ipsec_spd[idx].curr_policy_handle;

	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL) {
		cs_ipsec_spd[idx].p_h = p_node;
		cs_ipsec_spd[idx].policy_cnt = 1;
	} else {
		while (p) {
			pre_p = p;
			p = p->next;
		}
		pre_p->next = p_node;
		cs_ipsec_spd[idx].policy_cnt++;
	}

	*policy_handle = p_node->policy_handle;
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	
	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_ipsec_policy_add);

cs_status_t cs_ipsec_policy_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle)
{
	int idx;
	cs_ipsec_policy_node_t *p, *pre_p;
	unsigned long flags;

	DBG(printk("%s:%d device id = %d, spd_handle = %d, "
		"policy_handle = %d\n",
		__func__, __LINE__,
		device_id, spd_handle, policy_handle));

	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}
	
	pre_p = p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->policy_handle == policy_handle) {
			/* hit */
			if (p->sa_valid == CS_TRUE) {
				spin_unlock_irqrestore(&cs_ipsec_lock, flags);
				ERR(printk("%s:%d still have valid sa\n",
					__func__, __LINE__));
				return CS_E_CONFLICT;
			
			}

			if (p == cs_ipsec_spd[idx].p_h) {
				/* the first entry */
				cs_ipsec_spd[idx].p_h = p->next;
			} else {
				pre_p->next = p->next;
			}
			cs_ipsec_spd[idx].policy_cnt--;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			cs_free(p);
			return CS_E_NONE;
		}
		pre_p = p;
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_ipsec_policy_delete);

cs_status_t cs_ipsec_policy_get (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_OUT	cs_ipsec_policy_t			*policy)
{
	int idx;
	cs_ipsec_policy_node_t *p;
	unsigned long flags;

	DBG(printk("%s:%d device id = %d, spd_handle = %d, "
		"policy = 0x%p, policy_handle = %d\n",
		__func__, __LINE__,
		device_id, spd_handle, policy, policy_handle));

	if (policy == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->valid == CS_TRUE && p->policy_handle == policy_handle) {
			/* hit */
			*policy = p->policy;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_ipsec_policy_get);

cs_status_t cs_ipsec_policy_get_by_spi (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				spi,
	CS_OUT	cs_ipsec_policy_t			*policy,
	CS_OUT	cs_uint32_t				*policy_handle)
{
	int idx;
	cs_ipsec_policy_node_t *p;
	unsigned long flags;

	DBG(printk("%s:%d device id = %d, spd_handle = %d, "
		"spi = 0x%x, policy = 0x%p, policy_handle = 0x%p\n",
		__func__, __LINE__,
		device_id, spd_handle, spi, policy, policy_handle));

	if (policy == NULL || policy_handle == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}

	spi = ntohl(spi);
	while (p) {
		if (p->valid == CS_TRUE && 
			p->sa_valid == CS_TRUE &&
			spi == p->sa.spi) {
			/* hit */
			*policy = p->policy;
			*policy_handle = p->policy_handle;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			return CS_E_NONE;
		}
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_ipsec_policy_get_by_spi);

cs_status_t cs_ipsec_sa_add (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_IN	cs_uint8_t				sa_count,
	CS_IN	cs_ipsec_sa_t				*sa_array,
	CS_IN	cs_uint32_t				previous_sa_handle,
	CS_OUT	cs_uint32_t				*sa_handle)
{
	int idx, ret;
	cs_ipsec_policy_node_t *p, *pre_p, p_node;
	cs_uint16_t sa_id;
	cs_uint32_t spi;
	unsigned long flags;

	DBG(printk("%s:%d device id = %d, spd_handle = %d, \n\t"
		"policy_handle = %d, sa_count = %d, sa_array = 0x%p, "
		"previous_sa_handle = %d, &sa_handle = 0x%p\n",
		__func__, __LINE__,
		device_id, spd_handle, policy_handle, sa_count,
		sa_array, previous_sa_handle, sa_handle));

	if (sa_array == NULL || sa_handle == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;

	if ((sa_array->is_natt != 0) &&
			(sa_array->natt_ingress_src_port == 0) &&
			(sa_array->natt_egress_dest_port == 0)) {
		ERR(printk("%s:%d src port of ingress NAT-T and dest port of egress NAT-T can't be both zero.\n",
			__func__, __LINE__));
		return CS_E_PARAM;
	}

	if (sa_count != 1) {
		ERR(printk("%s:%d only support the case of sa_count = 1\n",
			__func__, __LINE__));
		return CS_E_CONFLICT;
	}
	
	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	pre_p = p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}

	/* avoid duplicate SPI */
	spi = htonl(sa_array[0].spi);
	while (p) {
		if (p->sa.spi == spi) {
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);
			ERR(printk("%s:%d duplicate SA, SPI = 0x%x\n",
				__func__, __LINE__, spi));
			return CS_E_CONFLICT;
		}
		p = p->next;
	}
	
	p = cs_ipsec_spd[idx].p_h;
	while (p) {
		if (p->policy_handle == policy_handle) {
			/* hit */
			if (p->sa_valid == CS_TRUE &&
				previous_sa_handle > 0 &&
				p->sa_handle == previous_sa_handle) {
				/* need to rekey later */
				p->previous_sa_handle = previous_sa_handle;
			}
			else if (p->sa_valid == CS_TRUE) {
				spin_unlock_irqrestore(&cs_ipsec_lock, flags);
				ERR(printk("%s:%d still have valid sa\n",
					__func__, __LINE__));
				return CS_E_CONFLICT;
			
			} else {
				/* don't rekey */
				p->previous_sa_handle = 0; /* invalid */
			}

			/* apply a sa_id for the encrypted tunnel */
			ret = cs_sa_id_alloc(sa_array[0].sa_dir,
						1,
						0,
						CS_IPSEC,
						&sa_id);
			if (ret != CS_OK) {
				spin_unlock_irqrestore(&cs_ipsec_lock, flags);
				ERR(printk("%s:%d can't get sa_id\n",
							__func__, __LINE__));
				return CS_E_CONFLICT;
			}

			/* Store sa_handle = sa_id + 1 */
			p->sa_handle = *sa_handle = sa_id + 1;

			/* check for rekey or not */
			if (p->previous_sa_handle > 0 &&
				p->tunnel_id > TID_L2TP_IPSEC_BASE) {
				/* store ekey, akey, spi, seq_num */
				memcpy(&p->sa.ekey, &sa_array[0].ekey,
						CS_IPSEC_MAX_ENC_KEY_LEN);
				memcpy(&p->sa.akey, &sa_array[0].akey,
						CS_IPSEC_MAX_AUTH_KEY_LEN);
				p->sa.spi = spi;
				p->sa.seq_num = sa_array[0].seq_num;
				
				/* update sa_id in tunnel node */
				cs_l2tp_sa_id_set(p->tunnel_id, sa_id);
				p_node = *p;
				spin_unlock_irqrestore(&cs_ipsec_lock, flags);

				/* rekey */
				/* use local copy instead of the tree node */
				p = &p_node;
				ret = cs_ipsec_sa_rekey(spd_handle, p);
				if (ret != CS_OK) {
					ERR(printk("%s:%d SA rekey fail\n",
							__func__, __LINE__));
					return CS_E_CONFLICT;
				}
				return CS_E_NONE;
			} else {
				memcpy(&p->sa, &sa_array[0],
							sizeof(cs_ipsec_sa_t));
				p->sa.spi = spi;
			}
			p->sa_valid = CS_TRUE;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);

			/* change to send IPC in cs_l2tp_tunnel_add() */
			return CS_E_NONE;
		}
		pre_p = p;
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	return CS_E_NOT_FOUND;	
}
EXPORT_SYMBOL(cs_ipsec_sa_add);


cs_status_t cs_ipsec_sa_delete (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_IN	cs_uint32_t				sa_handle)
{
	int idx;
	cs_ipsec_policy_node_t *p, *pre_p;
	unsigned long flags;

	DBG(printk("%s:%d device id = %d, spd_handle = %d, \n\t"
		"policy_handle = %d, sa_handle = %d\n",
		__func__, __LINE__,
		device_id, spd_handle, policy_handle, sa_handle));

	if (spd_handle > CS_IPSEC_SPD_INBOUND) {
		ERR(printk("%s:%d invalid spd_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	idx = spd_handle;

	spin_lock_irqsave(&cs_ipsec_lock, flags);
	if (cs_ipsec_spd[idx].valid == CS_FALSE) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d index = %d, spd is invalid.\n",
			__func__, __LINE__, idx));
		return CS_E_NOT_FOUND;
	}

	pre_p = p = cs_ipsec_spd[idx].p_h;
	if (p == NULL || cs_ipsec_spd[idx].policy_cnt == 0) {
		spin_unlock_irqrestore(&cs_ipsec_lock, flags);
		ERR(printk("%s:%d can't find policy_handle\n",
			__func__, __LINE__));
		return CS_E_NOT_FOUND;
	}
	while (p) {
		if (p->policy_handle == policy_handle) {
			/* hit */
			if (p->sa_valid == CS_FALSE) {
				spin_unlock_irqrestore(&cs_ipsec_lock, flags);
				ERR(printk("%s:%d invalid sa\n",
					__func__, __LINE__));
				return CS_E_NOT_FOUND;
			}

			if (p->sa_handle != sa_handle) {
				spin_unlock_irqrestore(&cs_ipsec_lock, flags);
				ERR(printk("%s:%d mismatched sa_handle = %d\n",
					__func__, __LINE__, p->sa_handle));
				return CS_E_NOT_FOUND;
			}

		
			/* change to send IPC in cs_ipsec_tunnel_delete() */

			p->sa_valid = CS_FALSE;
			spin_unlock_irqrestore(&cs_ipsec_lock, flags);

			/* release the sa_id here */
			cs_sa_id_free(spd_handle, sa_handle - 1);
			return CS_E_NONE;
		}
		pre_p = p;
		p = p->next;
	}
	spin_unlock_irqrestore(&cs_ipsec_lock, flags);
	return CS_E_NOT_FOUND;

}
EXPORT_SYMBOL(cs_ipsec_sa_delete);

#ifdef CS_IPC_ENABLED
/*===== L2TP  =====*/
/*CS_IPSEC_IPC_PE_SET_SADB*/
cs_status_t
cs_ipsec_ipc_send_set_sadb(
		cs_sa_id_direction_t direction,
		cs_ipsec_policy_node_t * p_policy_node,
		cs_uint16 sa_id
		)
{
	g2_ipc_pe_ipsec_set_entry_t sadb_entry;
	cs_status_t status;
	cs_uint32_t index;
	cs_uint8_t protocol;
	cs_ipsec_selector_t * p_selector;
	cs_uint32_t selector_count = p_policy_node->policy.selector_count;
	cs_ipsec_sa_t * sa = &p_policy_node->sa;
	
	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));

	if (p_policy_node == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	memset(&sadb_entry, 0, sizeof(g2_ipc_pe_ipsec_set_entry_t));
	
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));
	sadb_entry.sa_idx = sa_id;
		
	DBG(printk("sa information dump:\n"));
	sadb_entry.replay_window = sa->replay_window;
	DBG(printk("  sa->replay_window = %d\n", sa->replay_window));
	sadb_entry.spi = sa->spi;
	DBG(printk("  sa->spi=0x%x\n", sa->spi));
	sadb_entry.seq_num = sa->seq_num;
	DBG(printk("  sa->seq_num = %d\n", sa->seq_num));
	sadb_entry.ip_ver = sa->ip_ver; /* 0=IPv4, 1=IPv6 */
	DBG(printk("  sa->ip_ver = %d\n", sa->ip_ver));
	sadb_entry.proto = sa->proto;  /* 0=ESP; 1=AH */
	DBG(printk("  sa->proto = %d\n", sa->proto));
	sadb_entry.tunnel = sa->tunnel; /* 0=Tunnel; 1=Transport */
	DBG(printk("  sa->tunnel = %d\n", sa->tunnel));
	sadb_entry.sa_dir = sa->sa_dir; /* 0=outbound; 1=inbound */
	DBG(printk("  sa->sa_dir = %d\n", sa->sa_dir));
	
	/* encryption mode based on elliptic */
	sadb_entry.ealg = sa->ealg; /* cs_ipsec_ciph_alg_t */
	DBG(printk("  sa->ealg = %d\n", sa->ealg));
	sadb_entry.ealg_mode = sa->ealg_mode; /* cs_ipsec_ciph_mode_t */
	DBG(printk("  sa->ealg_mode = %d\n", sa->ealg_mode));
	sadb_entry.iv_len = sa->iv_len; /* (n * 4) bytes for CBC */
	DBG(printk("  sa->iv_len = %d\n", sa->iv_len));

	/* authentication mode based on elliptic */
	sadb_entry.aalg = sa->aalg; /* cs_ipsec_hash_alg_t */
	DBG(printk("  sa->aalg = %d\n", sa->aalg));
	sadb_entry.icv_trunclen = sa->icv_trunclen; /* (n * 4) bytes */
	DBG(printk("  sa->icv_trunclen = %d\n", sa->icv_trunclen));
	sadb_entry.etherIP = sa->etherIP;
	DBG(printk("  sa->etherIP = %d\n", sa->etherIP));

	/* If tunnel mode the outer IP header template is sent for reconstructing */
	memcpy(&sadb_entry.tunnel_saddr, &sa->tunnel_saddr, sizeof(cs_l3_ip_addr));
	DBG(printk("tunnel_saddr dump:\n"));
	DBG(cs_dump_data(&sadb_entry.tunnel_saddr, sizeof(cs_l3_ip_addr)));
	DBG(printk("\n"));
	memcpy(&sadb_entry.tunnel_daddr, &sa->tunnel_daddr, sizeof(cs_l3_ip_addr));
	DBG(printk("tunnel_daddr dump:\n"));
	DBG(cs_dump_data(&sadb_entry.tunnel_daddr, sizeof(cs_l3_ip_addr)));
	DBG(printk("\n"));	
	sadb_entry.lifetime_bytes = sa->lifetime_bytes;
	DBG(printk("  sa->lifetime_bytes = %d\n", sa->lifetime_bytes));
	sadb_entry.bytes_count = sa->bytes_count;
	DBG(printk("  sa->bytes_count = %d\n", sa->bytes_count));
	sadb_entry.lifetime_packets = sa->lifetime_packets;
	DBG(printk("  sa->lifetime_packets = %d\n", sa->lifetime_packets));
	sadb_entry.packets_count = sa->packets_count;
	DBG(printk("  sa->packets_count = %d\n", sa->packets_count));
	sadb_entry.is_natt = sa->is_natt;
	DBG(printk("  sa->is_natt = %d\n", sa->is_natt));
	sadb_entry.copy_ip_id = sa->copy_ip_id;
	DBG(printk("  sa->copy_ip_id = %d\n", sa->copy_ip_id));
	sadb_entry.copy_tos = sa->copy_tos;
	DBG(printk("  sa->copy_tos = %d\n", sa->copy_tos));

	/*calc ip_checksum if IPV4*/
	if(sadb_entry.ip_ver == CS_IPV4)
	{
		if (sa->is_natt != 0)
			protocol = IPPROTO_UDP;
		else
			protocol = sa->proto ? IPPROTO_AH : IPPROTO_ESP;

		sadb_entry.ip_checksum = cs_calc_ipv4_checksum(
								sadb_entry.tunnel_saddr.ipv4_addr,
								sadb_entry.tunnel_daddr.ipv4_addr, 
								protocol);
	}
	DBG(printk("  sadb_entry.ip_checksum = %d\n", sadb_entry.ip_checksum));
	
	DBG(printk("sadb_entry dump:\n"));
	DBG(cs_dump_data(&sadb_entry, sizeof(g2_ipc_pe_ipsec_set_entry_t)));
	DBG(printk("\n"));
	
	status = cs_tunnel_ipc_send(direction, CS_IPSEC_IPC_PE_SET_SADB, &sadb_entry,
					sizeof(g2_ipc_pe_ipsec_set_entry_t));
	
	if (status != G2_IPC_OK) {
		ERR(printk("%s::Failed to send IPC for set sadb\n",
			__func__));
		return CS_E_ERROR;
	}
	
	/* Set selector */
	for(index = 0; index < selector_count; index++)
	{
		p_selector = &p_policy_node->policy.selector_array[index];
		status = cs_ipsec_ipc_send_sadb_selector(direction, index, p_selector, sa_id);
		if (status != G2_IPC_OK) {
			ERR(printk("%s::Failed to send IPC for set sadb selector\n",
				__func__));
			return CS_E_ERROR;
		}
	}

	/* Set KEY */
	status = cs_ipsec_ipc_send_sadb_key(direction,
										sa->auth_keylen,
										sa->enc_keylen,
										sa->akey,
										sa->ekey,
										sa_id);
	if (status != G2_IPC_OK) {
		ERR(printk("%s::Failed to send IPC for set sadb key\n",
			__func__));
		return CS_E_ERROR;
	}

	return status;
}

/*CS_IPSEC_IPC_PE_DEL_SADB*/
cs_status_t
cs_ipsec_ipc_send_del_entry(
		cs_sa_id_direction_t direction,
		cs_uint16 sa_id
		)
{
	g2_ipc_pe_ipsec_del_sadb_t sadb_entry_msg;
	
	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));
	memset(&sadb_entry_msg, 0, sizeof(g2_ipc_pe_ipsec_del_sadb_t));
	
	sadb_entry_msg.sa_idx = sa_id;
	
	return cs_tunnel_ipc_send(direction, CS_IPSEC_IPC_PE_DEL_SADB, &sadb_entry_msg,
					sizeof(g2_ipc_pe_ipsec_del_sadb_t));
}

/*CS_IPSEC_IPC_PE_SET_SADB_KEY*/
cs_status_t
cs_ipsec_ipc_send_sadb_key(
		cs_sa_id_direction_t direction,
		cs_uint8_t	auth_keylen,
		cs_uint8_t	enc_keylen,
		cs_uint8_t * 	akey,
		cs_uint8_t * 	ekey,
		cs_uint16_t sa_id
		)
{
	g2_ipc_pe_ipsec_set_sadb_key_t sadb_key_msg;
	
	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));
	DBG(printk("%s:%d enc_keylen = %d\n", __func__, __LINE__, enc_keylen));
	DBG(printk("%s:%d auth_keylen = %d\n", __func__, __LINE__, auth_keylen));
	
	if(auth_keylen > 8 || enc_keylen > 8)
	{
		ERR(printk("%s::Wrong sadb key length\n",
			__func__));
		return CS_E_ERROR;
	}
	
	memset(&sadb_key_msg, 0, sizeof(g2_ipc_pe_ipsec_set_sadb_key_t));
		
	sadb_key_msg.sa_idx = sa_id;
	sadb_key_msg.auth_keylen = auth_keylen;
	sadb_key_msg.enc_keylen = enc_keylen;
	
	memcpy(sadb_key_msg.ekey, ekey, enc_keylen * 4); /* (n * 4) bytes */
	memcpy(sadb_key_msg.akey, akey, auth_keylen * 4); /* (n * 4) bytes */

	DBG(printk("ekey dump:\n"));
	DBG(cs_dump_data(sadb_key_msg.ekey, enc_keylen * 4));
	DBG(printk("\n"));
	DBG(printk("akey dump:\n"));
	DBG(cs_dump_data(sadb_key_msg.akey, auth_keylen * 4));
	DBG(printk("\n"));
	
	return cs_tunnel_ipc_send(direction, CS_IPSEC_IPC_PE_SET_SADB_KEY, &sadb_key_msg,
					sizeof(g2_ipc_pe_ipsec_set_sadb_key_t));
}

/* CS_IPSEC_IPC_PE_SET_SADB_SELECTOR */
cs_status_t
cs_ipsec_ipc_send_sadb_selector(
		cs_sa_id_direction_t direction,
		cs_uint16_t selector_idx,
		cs_ipsec_selector_t * p_selector,
		cs_uint16_t sa_id
		)
{
	g2_ipc_pe_ipsec_set_sadb_selector_t sadb_selector_msg;

	if (p_selector == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}
	
	DBG(printk("%s:%d direction = %d\n", __func__, __LINE__, direction));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));
	DBG(printk("%s:%d selector_idx = %d\n", __func__, __LINE__, selector_idx));

	memset(&sadb_selector_msg, 0, sizeof(g2_ipc_pe_ipsec_set_sadb_selector_t));
	sadb_selector_msg.sa_idx = sa_id;
	sadb_selector_msg.selector_idx = selector_idx;
	memcpy(&sadb_selector_msg.selector, p_selector, sizeof(cs_ipsec_selector_t));
	
	DBG(printk("Selector dump:\n"));
	DBG(cs_dump_data(&sadb_selector_msg.selector, sizeof(cs_ipsec_selector_t)));
	DBG(printk("\n"));
	
	return cs_tunnel_ipc_send(direction, CS_IPSEC_IPC_PE_SET_SADB_SELECTOR, &sadb_selector_msg,
					sizeof(g2_ipc_pe_ipsec_set_sadb_selector_t));
}

cs_status_t cs_ipsec_ipc_rcv_set_sadb_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_ipsec_set_sadb_ack_t *msg;
	cs_uint16_t sa_id;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_ipsec_set_sadb_ack_t * )msg_data;
	sa_id = msg->sa_idx;
	
	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<IPSEC>: Receive set sadb ack from PE%d", peer.cpu_id - 1));
	DBG(printk("%s:%d sa_id = %d\n", __func__, __LINE__, sa_id));
	
	return CS_OK;
}

cs_status_t cs_ipsec_ipc_rcv_del_sadb_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_ipsec_del_sadb_ack_t *msg;
	cs_uint16_t sa_id;
	cs_uint16_t ofld_seq;
	
	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_ipsec_del_sadb_ack_t * )msg_data;
	sa_id = msg->sa_idx;
	ofld_seq = msg->ofld_seq;

	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<IPSEC>: Receive del sadb ack from PE%d", peer.cpu_id - 1));
	DBG(printk("%s:%d sa_id=%d\n", __func__, __LINE__, sa_id));
	printk("%s:%d ##### ofld_seq = %d #####\n", __func__, __LINE__, ofld_seq);
	
	return CS_OK;
}

cs_status_t cs_ipsec_ipc_rcv_set_sadb_key_ack(struct ipc_addr peer,
			      unsigned short msg_no, const void *msg_data,
			      unsigned short msg_size,
			      struct ipc_context *context)
{
	g2_ipc_pe_ipsec_set_sadb_key_ack_t *msg;
	cs_tunnel_entry_t *t, t_node;
	cs_uint16_t sa_id;
	cs_status_t ret;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_ipsec_set_sadb_key_ack_t * )msg_data;
	sa_id = msg->sa_idx;
	t = &t_node;
	
	DBG(printk("%s:%d\n", __func__, __LINE__));
	DBG(printk("<IPSEC>: Receive set sadb key ack from PE%d", peer.cpu_id - 1));
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
	if(t->tunnel_cfg.type == CS_IPSEC) {
		ret = cs_l2tp_ipsec_tunnel_hash_create(t);
		if(ret != CS_OK) {
			ERR(printk("%s:%d Can't create hash for ipsec tunnel.\n",
				__func__, __LINE__));
		}
	}

	
	return ret;
}
#endif /* CS_IPC_ENABLED */



