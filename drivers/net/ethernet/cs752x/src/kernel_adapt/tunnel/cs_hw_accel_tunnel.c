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
 * cs_hw_accel_tunnel.c
 *
 * $Id$
 *
 * This file contains the implementation for CS Tunnel
 * Acceleration.
 * Currently supported:
 * 	IPv6 over PPP over L2TP over IPv4 over PPPoE (IPLIP)
 *	L2TP tunnel
 *	L2TP over IPSec
 *	PPTP
 */

#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/net_namespace.h>
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/wait.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include <mach/cs_route_api.h>
#include <mach/cs_rule_hash_api.h>
#include "cs_fe.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_hw_accel_pptp.h"
#include "cs_hw_accel_sa_id.h"
#include "cs_hw_accel_rtp_proxy.h"
#include "cs_hw_accel_ip_translate.h"

#include "cs752x_eth.h"
#ifdef CS_IPC_ENABLED
#include <mach/cs_vpn_tunnel_ipc.h>
#endif /* CS_IPC_ENABLED */


#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#else
#define DBG(x)	{ }
#endif /* CONFIG_CS752X_PROC */

#define SKIP(x) { }
#define ERR(x)	(x)

spinlock_t cs_tunnel_rx_lock;

/* for cs_tunnel_type_mtu_get() */
wait_queue_head_t g_mtu_wq;
int g_mtu_wq_wakeup = 0;
int g_mtu = 0;

typedef struct cs_vpn_tasklet_s {
	struct tasklet_struct	tasklet;
	struct sk_buff_head	head; /* queue of VPN inbound packets */
} cs_vpn_tasklet_t;

static cs_vpn_tasklet_t cs_vpn_tasklet;


/* utilities */
/* cs_ip_addr_cmp
 *	return 		1	if addr1 range contains addr2 range
 *			0	if addr1 range is equal to addr2 range
 *			-1	otherwise
 */
int cs_ip_addr_cmp(cs_ip_address_t *addr1, cs_ip_address_t *addr2)
{
	int i;
	int idx, bits1, bits2, offset;
	cs_uint32_t mask;

	if (memcmp(addr1, addr2, sizeof(cs_ip_address_t)) == 0)
		return 0;

	if ((addr1->afi != addr2->afi) && (addr1->addr_len != 0))
		return -1;

	if (addr2->afi == CS_IPV4) {
		bits1 = (addr1->addr_len > 32) ? 32 : addr1->addr_len;
		bits2 = (addr2->addr_len > 32) ? 32 : addr2->addr_len;
		if ( bits1 > bits2)
			return -1;

		if (bits1 == 32) {
			if (addr1->ip_addr.ipv4_addr == addr2->ip_addr.ipv4_addr)
				return 0;
			else
				return -1;
		}
		mask = (1 << bits1) - 1;

		if ((addr1->ip_addr.ipv4_addr & mask) ==
					(addr2->ip_addr.ipv4_addr & mask)) {
			if (bits1 == bits2)
				return 0;
			else
				return 1;
		} else {
			return -1;
		}

	} else {	/* CS_IPV6 */
		bits1 = (addr1->addr_len > 128) ? 128 : addr1->addr_len;
		bits2 = (addr2->addr_len > 128) ? 128 : addr2->addr_len;
		if ( bits1 > bits2)
			return -1;

		if (bits1 == 128) {
			for (i = 0; i < 4; i++)
				if (addr1->ip_addr.ipv6_addr[i] !=
						addr2->ip_addr.ipv6_addr[i])
					return -1;
			return 0;
		}

		idx = bits1 / 32;

		offset = bits1 % 32;

		for (i = 0; i < idx; i++)
			if (addr1->ip_addr.ipv6_addr[i] !=
						addr2->ip_addr.ipv6_addr[i])
				return -1;

		mask = (1 << offset) - 1;

		if ((addr1->ip_addr.ipv6_addr[idx] & mask) ==
					(addr2->ip_addr.ipv6_addr[idx] & mask)) {
			if (bits1 == bits2)
				return 0;
			else
				return 1;
		} else {
			return -1;
		}

	}
}

/* cs_ip2mask		Convert cs_ip_address_t to {mask, result} pair
 *	return 		0	succeed
 *			-1	otherwise
 */
int cs_ip2mask(cs_ip_address_t *ip, cs_ip_mask_t *ip_mask) {
	int i, cnt;
	int all_bits, bits;
	cs_uint32_t mask;


	if(ip == NULL || ip_mask == NULL)
	{
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return -1;
	}

	memset(ip_mask, 0, sizeof(cs_ip_mask_t));
	cnt = (ip->afi == CS_IPV6) ? 4 : 1;
	all_bits = ip->addr_len;

	for (i = 0; i < cnt; i++) {
		bits = (all_bits >= 32) ? 32 : all_bits;
		all_bits -= bits;

#if 0
		if (bits == 32) {
			ip_mask->mask[i] = 0xffffffff;
			ip_mask->result[i] = ip->ip_addr.addr[i];
			continue;
		}

		if (bits == 0 )
			return 0;
#endif


		if (bits != 0 )
			mask = ((cs_uint32_t)0xffffffff<<(32-bits));
		else
			mask = 0;

		//Change to little endine format.
		mask = htonl(mask);
		ip_mask->mask[i] = mask;
		ip_mask->result[i] = ip->ip_addr.addr[i] & ip_mask->mask[i];
	}

#if 0
	for (i = 0; i < cnt; i++) {
		bits = (all_bits >= 32) ? 32 : all_bits;
		all_bits -= bits;

		if (bits == 32) {
			ip_mask->mask[i] = 0xffffffff;
			ip_mask->result[i] = ip->ip_addr.addr[i];
			continue;
		}

		if (bits == 0)
			return 0;

		ip_mask->mask[i] = (1 << bits) - 1;
		ip_mask->result[i] = ip->ip_addr.addr[i] & ip_mask->mask[i];
	}
#endif
	return 0;
}

cs_uint16_t cs_calc_ipv4_checksum(
	cs_uint32_t saddr,
	cs_uint32_t daddr,
	cs_uint8_t protocol
	)
{
	struct iphdr iph;

	iph.ihl = 5;
	iph.version = 4;
	iph.tos = 0;
	iph.tot_len = 0;
	iph.id = 0;
	iph.frag_off = 0;
	iph.ttl = 64;
	iph.protocol = protocol;
	iph.saddr = saddr;
	iph.daddr = daddr;
	ip_send_check(&iph);
	return iph.check;
}

void cs_dump_data(void *p, int len) {
	int i;

	unsigned char *ptr;

	ptr = (unsigned char *) p;
	for (i = 0; i < len; i++) {
		if ((i % 16) == 0)
			printk("\n%04X  ", i);
		if ((i % 8) == 0)
			printk("  ");

		printk("%02x ", ptr[i]);

	}
	printk("\n");
}

static cs_status cs_tunnel_cfg_check(
	cs_tunnel_cfg_t					*p_tunnel_cfg)
{
	cs_uint32_t tmp;

	switch (p_tunnel_cfg->type) {
	case CS_L2TP:
	case CS_IPSEC:
	case CS_L2TP_IPSEC:
	case CS_PPTP:
		break;

	case CS_L2TPV3:
	case CS_L2TPV3_IPSEC:
		tmp = p_tunnel_cfg->tunnel.l2tp.encap_type;
		if (tmp > 1) {
			ERR(printk("invalid encap_type %d\n", tmp));
			return CS_E_CONFLICT;
		}
		tmp = p_tunnel_cfg->tunnel.l2tp.cookie_len;
		if (tmp != 0 && tmp != 4 && tmp != 8) {
			ERR(printk("invalid cookie_len %d\n", tmp));
			return CS_E_CONFLICT;
		}
		break;
	default:
		ERR(printk("unknown tunnel type %d\n", p_tunnel_cfg->type));
		return CS_E_CONFLICT;
	}
	return CS_OK;
}


/*************************************************************************/
/* exported APIs */
#ifdef CS_IPC_ENABLED
cs_status_t
cs_tunnel_mtu_set(
	CS_IN cs_dev_id_t		device_id,
	CS_IN cs_tunnel_mtu_t		*p_mtu
	)
{
	g2_ipc_pe_mtu_set_t msg;
	cs_status_t ret;

	/* we only care outbound traffic for MTU limitation */

	if (p_mtu->tunnel_type >= CS_TUN_TYPE_MAX) {
		return CS_E_ERROR;
	}

	memset(&msg, 0, sizeof(g2_ipc_pe_mtu_set_t));
	ret = cs_sa_id_get_by_tunnel_id(p_mtu->tunnel_id, p_mtu->tunnel_type,
					CS_TUNNEL_DIR_OUTBOUND, &msg.sa_id);
	if (ret != CS_OK)
		return ret;

	msg.fun_id = p_mtu->tunnel_type;
	msg.mtu = p_mtu->mtu;

	ret = cs_tunnel_ipc_send(UP_STREAM, CS_IPC_PE_MTU_SET, &msg, sizeof(msg));
	if (ret != CS_E_OK)
		return ret;

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_tunnel_mtu_set);

cs_status_t
cs_tunnel_mtu_get(
	CS_IN cs_dev_id_t		device_id,
	CS_IN_OUT cs_tunnel_mtu_t	*p_mtu
	)
{
	g2_ipc_pe_mtu_get_t msg;
	unsigned long  ms = 1000; /* timeout, unit is ms */
	int ret;

	/* we only care outbound traffic for MTU limitation */

	g_mtu_wq_wakeup = 0;

	if (p_mtu->tunnel_type >= CS_TUN_TYPE_MAX) {
		return CS_E_ERROR;
	}

	memset(&msg, 0, sizeof(g2_ipc_pe_mtu_get_t));
	ret = cs_sa_id_get_by_tunnel_id(p_mtu->tunnel_id, p_mtu->tunnel_type,
					CS_TUNNEL_DIR_OUTBOUND, &msg.sa_id);
	if (ret != CS_OK)
		return ret;

	msg.fun_id = p_mtu->tunnel_type;

	if (cs_tunnel_ipc_send(UP_STREAM, CS_IPC_PE_MTU_GET, &msg, sizeof(msg)) != CS_E_OK) {
		printk(KERN_ERR "%s: Error! Fail to get MTU value for tunnel_type %d.\n", __func__, p_mtu->tunnel_type);
		return CS_E_ERROR;
	}

	/* wait till g_mtu_wq_wakeup is set or timeout elapsed */
	ret = wait_event_interruptible_timeout(g_mtu_wq, g_mtu_wq_wakeup != 0, ms * 100 / 1000);

	if (g_mtu == 0) {
		printk(KERN_ERR "%s: Timeout! Fail to get MTU value for tunnel_type %d.\n", __func__, p_mtu->tunnel_type);
		return CS_E_TIMEOUT;
	} else {
		p_mtu->mtu = g_mtu;
		g_mtu = 0;
	}

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_tunnel_mtu_get);

cs_status_t cs_tunnel_mtu_get_ack(
	struct ipc_addr		peer,
	unsigned short		msg_no,
	const void		*msg_data,
	unsigned short		msg_size,
	struct ipc_context	*context)
{
	g2_ipc_pe_mtu_get_ack_t *msg;

	if (msg_data == NULL) {
		printk(KERN_ERR "%s: ERROR! Null pointer.\n", __func__);
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_mtu_get_ack_t *) msg_data;

	if (msg->fun_id >= CS_TUN_TYPE_MAX) {
		return CS_E_ERROR;
	}

	g_mtu = msg->mtu;
	g_mtu_wq_wakeup = 1;
	wake_up_interruptible(&g_mtu_wq);

	return CS_E_OK;
}

#endif /* CS_IPC_ENABLED */

cs_status_t
cs_tunnel_add(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_cfg_t	*p_tunnel_cfg,
		CS_OUT cs_tunnel_id_t	*p_tunnel_id
		)
{
	cs_status_t ret;

	DBG(printk("%s:%d device_id=%d, p_tunnel_cfg = 0x%p,"
		" p_tunnel_id = 0x%p\n",
		__func__, __LINE__, device_id, p_tunnel_cfg, p_tunnel_id));
	if (p_tunnel_cfg == NULL)
		return CS_ERROR;

	if((ret = cs_tunnel_cfg_check(p_tunnel_cfg)) != CS_OK) {
		ERR(printk("Invalid tunnel configuration\n"));
		return ret;
	}

	DBG(printk("%s:%d tx_port=%d, p_tunnel_cfg->type = %d, dir = %d\n",
		__func__, __LINE__, p_tunnel_cfg->tx_port, p_tunnel_cfg->type,
		p_tunnel_cfg->dir));

	if (p_tunnel_cfg->tx_port == 0) {
		switch (p_tunnel_cfg->type) {
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		case CS_L2TP:
		case CS_IPSEC:
		case CS_L2TP_IPSEC:
		case CS_L2TPV3:
		case CS_L2TPV3_IPSEC:
			/* L2TP tunnel or L2TP over IPSec */
			return cs_l2tp_ipsec_tunnel_add(device_id, p_tunnel_cfg,
								p_tunnel_id);
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC*/
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
		case CS_PPTP:
			ret = cs_pptp_tunnel_add(p_tunnel_cfg);
			if (ret != CS_E_OK) {
				printk(KERN_ERR "%s: Fail to add PPTP tunnel\n",
								__func__);
			}
			*p_tunnel_id = p_tunnel_cfg->tunnel.gre.tunnel_id;
			return ret;
#endif /* CONFIG_CS75XX_HW_ACCEL_PPTP */
		default:
			break;
		}
	}

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	return cs_iplip_tunnel_add(device_id, p_tunnel_cfg, p_tunnel_id);
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;

}
EXPORT_SYMBOL(cs_tunnel_add);

cs_status_t
cs_tunnel_delete(
		CS_IN cs_dev_id_t       device_id,
		CS_IN cs_tunnel_cfg_t   *p_tunnel_cfg
		)
{
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
	cs_status_t ret;
#endif

	DBG(printk("%s:%d device_id=%d, p_tunnel_cfg = 0x%p\n",
		__func__, __LINE__, device_id, p_tunnel_cfg));
	if (p_tunnel_cfg == NULL)
		return CS_ERROR;

	if (p_tunnel_cfg->tx_port == 0) {
		switch (p_tunnel_cfg->type) {
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		case CS_L2TP:
		case CS_L2TP_IPSEC:
		case CS_L2TPV3:
		case CS_L2TPV3_IPSEC:
			/* L2TP tunnel or L2TP over IPSec */
			return cs_l2tp_tunnel_delete(device_id, p_tunnel_cfg);
		case CS_IPSEC:
			return cs_ipsec_tunnel_delete(device_id, p_tunnel_cfg);
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC*/
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
		case CS_PPTP:
			ret = cs_pptp_tunnel_delete(p_tunnel_cfg);
			if (ret != CS_E_OK) {
				printk(KERN_ERR "%s: Fail to delete PPTP tunnel\n",
								__func__);
			}
			return ret;
#endif /* CONFIG_CS75XX_HW_ACCEL_PPTP */
		default:
			break;
		}
	}

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	return cs_iplip_tunnel_delete(device_id, p_tunnel_cfg);
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;

}
EXPORT_SYMBOL(cs_tunnel_delete);

cs_status_t
cs_tunnel_delete_by_idx(
		CS_IN cs_dev_id_t       device_id,
		CS_IN cs_tunnel_id_t    tunnel_id
		)
{
	DBG(printk("%s:%d device_id=%d, tunnel_id = %d\n",
		__func__, __LINE__, device_id, tunnel_id));

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	if ((tunnel_id >= TID_L2TP_IPSEC_BASE) && (tunnel_id < TID_L2TP_IPSEC_MAX)) {
		/* L2TP tunnel or L2TP over IPSec */
		return cs_l2tp_ipsec_tunnel_delete_by_idx(device_id, tunnel_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
	if ((tunnel_id >= TID_PPTP_BASE) && (tunnel_id < TID_PPTP_MAX)) {
		return cs_pptp_tunnel_delete_by_idx(tunnel_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_PPTP */

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	if ((tunnel_id >= TID_IPLIP_BASE) && (tunnel_id < TID_IPLIP_MAX)) {
		return cs_iplip_tunnel_delete_by_idx(device_id, tunnel_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */
	return CS_OK;
}
EXPORT_SYMBOL(cs_tunnel_delete_by_idx);

cs_status_t
cs_tunnel_get(
		CS_IN  cs_dev_id_t      device_id,
		CS_IN  cs_tunnel_id_t   tunnel_id,
		CS_OUT cs_tunnel_cfg_t  *p_tunnel_cfg
		)
{
	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, p_tunnel_cfg = 0x%p\n",
		__func__, __LINE__, device_id, tunnel_id, p_tunnel_cfg));

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	if ((tunnel_id >= TID_L2TP_IPSEC_BASE) && (tunnel_id < TID_L2TP_IPSEC_MAX)) {
		/* L2TP tunnel or L2TP over IPSec */
		return cs_l2tp_tunnel_get(device_id, tunnel_id, p_tunnel_cfg);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	if ((tunnel_id >= TID_IPLIP_BASE) && (tunnel_id < TID_IPLIP_MAX)) {
		return cs_iplip_tunnel_get(device_id, tunnel_id, p_tunnel_cfg);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

	return CS_E_NOT_SUPPORT;
}
EXPORT_SYMBOL(cs_tunnel_get);

cs_status_t
cs_l2tp_session_add(
		CS_IN cs_dev_id_t       device_id,
		CS_IN cs_tunnel_id_t    tunnel_id,
		CS_IN cs_session_id_t   session_id
		)
{
	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x\n",
		__func__, __LINE__, device_id, tunnel_id, session_id));

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	if ((tunnel_id >= TID_L2TP_IPSEC_BASE) && (tunnel_id < TID_L2TP_IPSEC_MAX)) {
		/* L2TP tunnel or L2TP over IPSec */
		return cs_l2tp_ipsec_session_add(device_id, tunnel_id,
								session_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	if ((tunnel_id >= TID_IPLIP_BASE) && (tunnel_id < TID_IPLIP_MAX)) {
		return cs_iplip_l2tp_session_add(device_id, tunnel_id, session_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

	return CS_OK;
}
EXPORT_SYMBOL(cs_l2tp_session_add);

cs_status_t
cs_l2tp_session_delete(
		CS_IN cs_dev_id_t       device_id,
		CS_IN cs_tunnel_id_t    tunnel_id,
		CS_IN cs_session_id_t   session_id
		)
{
	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x\n",
		__func__, __LINE__, device_id, tunnel_id, session_id));

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	if ((tunnel_id >= TID_L2TP_IPSEC_BASE) && (tunnel_id < TID_L2TP_IPSEC_MAX)) {
		/* L2TP tunnel or L2TP over IPSec */
		return cs_l2tp_ipsec_session_delete(device_id, tunnel_id,
								session_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */


#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	if ((tunnel_id >= TID_IPLIP_BASE) && (tunnel_id < TID_IPLIP_MAX)) {
		return cs_iplip_l2tp_session_delete(device_id, tunnel_id, session_id);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(cs_l2tp_session_delete);

cs_status_t
cs_l2tp_session_get(
		CS_IN  cs_dev_id_t      device_id,
		CS_IN  cs_tunnel_id_t   tunnel_id,
		CS_IN  cs_session_id_t  session_id,
		CS_OUT cs_boolean_t     *is_present
		)
{
	DBG(printk("%s:%d device_id=%d, tunnel_id = %d, session_id = 0x%x,"
		" is_present = 0x%p\n", __func__, __LINE__,
		device_id, tunnel_id, session_id, is_present));
	if (is_present == NULL)
		return CS_ERROR;
	*is_present = FALSE;

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	if ((tunnel_id >= TID_L2TP_IPSEC_BASE) && (tunnel_id < TID_L2TP_IPSEC_MAX)) {
		/* L2TP tunnel or L2TP over IPSec */
		return cs_l2tp_ipsec_session_get(device_id, tunnel_id,
							session_id, is_present);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	if ((tunnel_id >= TID_IPLIP_BASE) && (tunnel_id < TID_IPLIP_MAX)) {
		return cs_iplip_l2tp_session_get(device_id, tunnel_id, session_id,
								is_present);
	}
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

	ERR(printk("%s:%d no entry is found\n", __func__, __LINE__));
	return CS_ERR_ENTRY_NOT_FOUND;

}
EXPORT_SYMBOL(cs_l2tp_session_get);

cs_status_t
cs_sa_id_get_by_tunnel_id(
	CS_IN cs_tunnel_id_t tunnel_id,
	CS_IN cs_tunnel_type_t tunnel_type,
	CS_IN cs_tunnel_dir_t dir,
	CS_OUT cs_uint16_t *sa_id)
{
	switch (tunnel_type) {
	case CS_L2TP:
	case CS_IPSEC:
	case CS_L2TP_IPSEC:
	case CS_L2TPV3:
	case CS_L2TPV3_IPSEC:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		return cs_l2tp_ipsec_sa_id_get_by_tunnel_id(tunnel_id,
						tunnel_type, dir, sa_id);
#else
		return CS_E_PARAM;
#endif

	case CS_PPTP:
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
		return cs_pptp_tunnel_id_to_sa_id(tunnel_id, dir, sa_id);
#else
		return CS_E_PARAM;
#endif

	case CS_RTP:
#ifdef CONFIG_CS75XX_HW_ACCEL_RTP_PROXY
		return cs_rtp_translate_sa_id_get_by_rtp_id(
				(cs_rtp_id_t) tunnel_id, sa_id);
#else
		return CS_E_PARAM;
#endif
	case CS_IP_TRANSLATE:
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
		return cs_ip_translate_sa_id_get_by_ip_translate_id(
				(cs_ip_translate_id_t) tunnel_id, dir, sa_id);
#else
		return CS_E_PARAM;
#endif

	case CS_GRE:
	case CS_GRE_IPSEC:

	default:
		return CS_E_PARAM;
	}
	return CS_E_OK;
}

cs_status_t
cs_tunnel_tx_frame_to_pe(
	CS_IN cs_uint8_t * dptr, /* pointer of packet payload (from L2) */
	CS_IN cs_uint32_t pkt_len, /* pkt_len of packet payload (from L2) */
	CS_IN cs_tunnel_id_t tunnel_id,
	CS_IN cs_tunnel_type_t tunnel_type,
	CS_IN cs_tunnel_dir_t dir
	)
{
	struct sk_buff *skb;
	struct ethhdr *p_eth;
	struct net_device * dev;
	cs_uint16_t sa_id = 0;
	cs_status_t ret;
	int crc32;
	u16 voq;

	ret = cs_sa_id_get_by_tunnel_id(tunnel_id, tunnel_type, dir, &sa_id);

	if (ret != CS_E_OK) {
		ERR(printk(KERN_ERR "%s: unable to get sa id.\n", __func__));
		return ret;
	}

	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		voq = ENCAPSULATION_VOQ_BASE;
		break;
	case CS_TUNNEL_DIR_INBOUND:
		voq = ENCRYPTION_VOQ_BASE;
		break;
	default:
		return CS_E_PARAM;
	}

	dev = ni_private_data.dev[0];

	skb = netdev_alloc_skb(dev, SKB_PKT_LEN + 0x100);

	/* first 256 bytes aligned address from skb->head */
	skb->data = (unsigned char *)((u32)
				(skb->head + 0x100) & 0xffffff00);

	memcpy(skb->data, dptr, pkt_len); /* pointer of packet payload (L2) */

	p_eth = (struct ethhdr *) skb->data;

	memset(p_eth->h_source, 0x0, ETH_ALEN);

	p_eth->h_source[1] = tunnel_type & 0xFF;
	p_eth->h_source[2] = 1; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	p_eth->h_source[3] = sa_id & 0xFF;
	p_eth->h_source[4] = (dir == CS_TUNNEL_DIR_OUTBOUND) ? 0 : 1;
	p_eth->h_source[5] = sa_id >> 8;

	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &p_eth->h_source[1], 5));
	/* Store 8 bits of the crc in the src MAC */
	p_eth->h_source[0] = crc32 & 0xFF;

	skb->len = pkt_len;
	skb->tail = skb->data + pkt_len;

	skb_shinfo(skb)->frag_list = 0;
	skb_shinfo(skb)->nr_frags = 0;

#if 0
	DBG(printk("%s:%d:sent the packet out via ni_special_start_xmit len=%d\n",
		   __func__, __LINE__, pkt_len));

	DBG(cs_dump_data(skb->data, pkt_len));
#endif
	skb_set_network_header(skb, ETH_HLEN);
	return ni_special_start_xmit(skb, NULL, voq);
}
EXPORT_SYMBOL(cs_tunnel_tx_frame_to_pe);

int __cs_hw_accel_tunnel_handle(struct sk_buff *skb)
{
	struct ethhdr *eth;
	cs_tunnel_type_t type;
	unsigned long flags;

	spin_lock_irqsave(&cs_tunnel_rx_lock, flags);

	eth = eth_hdr(skb);
	/* get function type from SA MAC[1] */
	type = eth->h_source[1];

	DBG(printk("%s:%d: function type %d, length %d\n",
		__func__, __LINE__,
		type, skb->len));

	switch (type) {
	case CS_IPSEC_FLOW_BASED:
		break;
	case CS_L2TP:
	case CS_L2TPV3:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		if(cs_l2tp_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif
		break;
	case CS_PPTP:
		break;
	case CS_IPSEC:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		if(cs_ipsec_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif
		break;
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		if(cs_l2tp_ipsec_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif
		break;
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
	case CS_FUNC_ID_MAPE:
		if(cs_ip_translate_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif /* CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE*/
	default:
		DBG(printk("%s:%d:: unknown function type %u\n",
			__func__, __LINE__, eth->h_source[1]));
	}

	netif_rx(skb);
	spin_unlock_irqrestore(&cs_tunnel_rx_lock, flags);
	return 0;

SKB_HANDLED:
	spin_unlock_irqrestore(&cs_tunnel_rx_lock, flags);
	DBG(printk("%s:%d: Terminate the packet\n",
		__func__, __LINE__));
	return 0;
}

int __cs_hw_accel_tunnel_ctrl_handle(
		int voq,
		struct sk_buff *skb)
{
	struct ethhdr *eth;
	cs_tunnel_type_t type;
	unsigned long flags;

	spin_lock_irqsave(&cs_tunnel_rx_lock, flags);

	eth = eth_hdr(skb);
	/* get function type from SA MAC[1] */
	type = eth->h_source[1];

	DBG(printk("%s:%d: function type %d, length %d\n",
		__func__, __LINE__,
		type, skb->len));

	switch (type) {
	case CS_IPSEC_FLOW_BASED:
	case CS_PPTP:
		break;

	case CS_L2TP:
	case CS_L2TPV3:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		if(cs_l2tp_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif
		break;
	case CS_IPSEC:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		if(cs_ipsec_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif
		break;

	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
		if(cs_l2tp_ipsec_tunnel_handle(skb) != CS_OK)
			goto SKB_HANDLED;
#endif
		break;
	default:
		DBG(printk("%s:%d:: unknown function type %u\n",
			__func__, __LINE__, skb->data[7]));
	}

	netif_rx(skb);
	spin_unlock_irqrestore(&cs_tunnel_rx_lock, flags);
	return 0;

SKB_HANDLED:
	spin_unlock_irqrestore(&cs_tunnel_rx_lock, flags);
	DBG(printk("%s:%d: Terminate the packet\n",
		__func__, __LINE__));
	return 0;
}

void cs_vpn_tasklet_func(unsigned long data)
{
	cs_vpn_tasklet_t *vt = &cs_vpn_tasklet;;
	unsigned long flags;
	struct sk_buff_head q;
	struct sk_buff *skb;

	__skb_queue_head_init(&q);
	spin_lock_irqsave(&vt->head.lock, flags);
	skb_queue_splice_init(&vt->head, &q);
	spin_unlock_irqrestore(&vt->head.lock, flags);

	while ((skb = __skb_dequeue(&q)))
		__cs_hw_accel_tunnel_handle(skb);

}

int cs_hw_accel_tunnel_handle(
		int voq,
		struct sk_buff *skb)
{
	skb_queue_tail(&cs_vpn_tasklet.head, skb);
	tasklet_schedule(&cs_vpn_tasklet.tasklet);
	return 0;
}

int cs_hw_accel_tunnel_ctrl_handle(
		int voq,
		struct sk_buff *skb)
{
	skb->protocol = eth_type_trans(skb, skb->dev);
	skb->pkt_type = PACKET_HOST;

	skb_queue_tail(&cs_vpn_tasklet.head, skb);
	tasklet_schedule(&cs_vpn_tasklet.tasklet);
	return 0;
}

static void cs_hw_accel_tunnel_callback_hma(unsigned long notify_event,
					   unsigned long value)
{
	DBG(printk("cs_hw_accel_tunnel_callback_hma notify_event 0x%lx value 0x%lx\n",
	     notify_event, value));
	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
#ifdef CS75XX_HW_ACCEL_TUNNEL
			/* disable CORE_VTABLE_TYPE_L3_TUNNEL vtable */
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_L3_TUNNEL, 0);
			/* disable CORE_VTABLE_TYPE_RE0_TUNNEL vtable */
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE0_TUNNEL, 0);
			/* disable CORE_VTABLE_TYPE_RE1_TUNNEL vtable */
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE1_TUNNEL, 0);
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_L3_TUNNEL_V6, 0);
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE0_TUNNEL_V6, 0);
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE1_TUNNEL_V6, 0);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
#ifdef CS_IPC_ENABLED
			cs_iplip_ipc_send_stop();
#endif /* CS_IPC_ENABLED */
#endif
			break;
		case CS_HAM_ACTION_MODULE_ENABLE:
#ifdef CS75XX_HW_ACCEL_TUNNEL
			/* enable CORE_VTABLE_TYPE_L3_TUNNEL vtable */
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_L3_TUNNEL, 1);
			/* enable CORE_VTABLE_TYPE_RE0_TUNNEL vtable */
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE0_TUNNEL, 1);
			/* enable CORE_VTABLE_TYPE_RE1_TUNNEL vtable */
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE1_TUNNEL, 1);
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_L3_TUNNEL_V6, 1);
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE0_TUNNEL_V6, 1);
			cs_core_vtable_set_entry_valid(CORE_VTABLE_TYPE_RE1_TUNNEL_V6, 1);
#endif
			break;
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			break;
	}

}

void cs_hw_accel_tunnel_init(void)
{
	cs_vpn_tasklet_t *vt = &cs_vpn_tasklet;;

	/* tasklet to handle rx packets */
	skb_queue_head_init(&vt->head);
	tasklet_init(&vt->tasklet, cs_vpn_tasklet_func, (unsigned long)vt);

	spin_lock_init(&cs_tunnel_rx_lock);

	/*cs_hw_accel_sa_id_init needs to move to cs_hw_accel_init()*/
	cs_hw_accel_sa_id_init();

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	cs_hw_accel_iplip_init();
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	cs_ipsec_init(1);
	cs_hw_accel_l2tp_ipsec_init();
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
	cs_hw_accel_ip_translate_init();
#endif /* CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE */

#ifdef CS_IPC_ENABLED
	/*cs_pe_ipc_register needs to move to cs_hw_accel_init()*/
	cs_pe_ipc_register();
#endif

	/* hw accel_manger */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_TUNNEL,
				       cs_hw_accel_tunnel_callback_hma);

	init_waitqueue_head(&g_mtu_wq);
}

void cs_hw_accel_tunnel_exit(void)
{
	/* tasklet to handle rx packets */
	tasklet_kill(&cs_vpn_tasklet.tasklet);

	/* hw accel_manger */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_TUNNEL, NULL);

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	cs_hw_accel_iplip_exit();
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	cs_hw_accel_l2tp_ipsec_exit();
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CS_IPC_ENABLED
	/*cs_pe_ipc_deregister needs to move to cs_hw_accel_exit()*/
	cs_pe_ipc_deregister();
#endif
}


/* common function used for tunnels to call cs_rule_hash_add() */
cs_status_t cs_tunnel_wan2pe_rule_hash_add(
	cs_tunnel_cfg_t *p_tunnel_cfg,
	cs_uint8_t      is_natt,
	cs_l3_ip_addr   *p_da_ip,
	cs_l3_ip_addr   *p_sa_ip,
	cs_uint8_t      ip_version,
	cs_uint16_t     pppoe_session_id,
	cs_uint16_t     sa_id,
	cs_uint32_t     spi_idx,
	cs_uint16_t	natt_ingress_src_port,
	cs_rule_hash_t  *p_rule_hash)
{
	unsigned int crc32;

	memset(p_rule_hash, 0, sizeof(cs_rule_hash_t));

	/* get IP SA and DA */
	memcpy(&p_rule_hash->key.sa[0], p_sa_ip, sizeof(cs_l3_ip_addr));
	memcpy(&p_rule_hash->key.da[0], p_da_ip, sizeof(cs_l3_ip_addr));

	switch (p_tunnel_cfg->type) {
		case CS_PPTP:
			p_rule_hash->key.ip_prot = 47; /* GRE */
			break;
		case CS_L2TP:
			/* CS_HM_IP_PROT_MASK */
			p_rule_hash->key.ip_prot = IPPROTO_UDP;

			/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
			p_rule_hash->key.l4_valid = 1;
			p_rule_hash->key.l4_dp = p_tunnel_cfg->tunnel.l2tp.dest_port;
			p_rule_hash->key.l4_sp = p_tunnel_cfg->tunnel.l2tp.src_port;

			/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
			/* we use the default value 0 */

			break;
		case CS_L2TPV3:
			if (p_tunnel_cfg->tunnel.l2tp.encap_type == 1 /* IP */) {
				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = CS_IPPROTO_L2TP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				/* we use the default value 0 */

				/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
				/* we use the default value 0 */

			} else { /* UDP */
				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = IPPROTO_UDP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				p_rule_hash->key.l4_valid = 1;
				p_rule_hash->key.l4_dp = p_tunnel_cfg->tunnel.l2tp.dest_port;
				p_rule_hash->key.l4_sp = p_tunnel_cfg->tunnel.l2tp.src_port;

				/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
				/* we use the default value 0 */

			}
			break;
		case CS_IPSEC:
		case CS_L2TP_IPSEC:
		case CS_L2TPV3_IPSEC:
			if (is_natt != 0) {
				/*** CS_HASHMASK_TUNNEL_L4_L7 ***/

				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = IPPROTO_UDP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				p_rule_hash->key.l4_valid = 1;
				p_rule_hash->key.l4_dp = 4500;
				p_rule_hash->key.l4_sp = natt_ingress_src_port;	/* Remote may not NAT-T device. */

				/* CS_HM_L7_FIELD_MASK | CS_HM_L7_FIELD_VLD_MASK */
				p_rule_hash->key.l7_field_valid = 1;
				p_rule_hash->key.l7_field = htonl(spi_idx);
			} else {
				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = IPPROTO_ESP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				/* we use the default value 0 */

				/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
				p_rule_hash->key.spi_vld = 1;
				p_rule_hash->key.spi_idx = spi_idx;
			}

			break;
		default:
			printk("%s:%d unknown hash key for tunnel type %d\n", __func__, __LINE__, p_tunnel_cfg->type);
			return CS_E_CONFLICT;
	}

	p_rule_hash->key.ip_version = ip_version;
	p_rule_hash->key.ip_valid = 1;
	p_rule_hash->key.lspid = GE_PORT0;
	p_rule_hash->key.vid_1 = 0; // TODO

	if (pppoe_session_id != 0) {
		p_rule_hash->key.pppoe_session_id_valid = 1;
		p_rule_hash->key.pppoe_session_id = pppoe_session_id;
	}

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	if (is_natt != 0)
		p_rule_hash->apptype = CORE_FWD_APP_TYPE_TUNNEL_L4_L7;
	else
#endif
		p_rule_hash->apptype = CORE_FWD_APP_TYPE_TUNNEL;

	/* set PPPoE session */
	if (pppoe_session_id != 0) {
		p_rule_hash->fwd_result.l2.pppoe_decap_en = 1;
		p_rule_hash->voq_pol.pppoe_session_id = pppoe_session_id;
	}

	p_rule_hash->voq_pol.voq_base = ENCRYPTION_VOQ_BASE;

	/* set fwd result */
	p_rule_hash->fwd_result.act.fwd_type = FE_CLASS_FWDTYPE_NORMAL;
	p_rule_hash->fwd_result.act.fwd_type_valid = 1;

	p_rule_hash->fwd_result.l2.mac_sa_replace_en = 1;

	if (p_tunnel_cfg->type == CS_PPTP) {
		p_rule_hash->fwd_result.l2.mac_sa[5] = GE_PORT0_VOQ_BASE;               /* dest_voq_id or sp_idx */
		p_rule_hash->fwd_result.l2.mac_sa[4] = 1;                               /* 0: enc, 1: dec */
		p_rule_hash->fwd_result.l2.mac_sa[3] = (cs_uint8_t) 0xff & p_tunnel_cfg->tunnel.gre.overlay_tunnel_egress.pptp_sa_id; /* SA ID */
		p_rule_hash->fwd_result.l2.mac_sa[2] = 0;                               /* 0: LAN->PE or WAN->PE, 1: CPU->PE */
		p_rule_hash->fwd_result.l2.mac_sa[1] = CS_PPTP;
	}
	else {
		p_rule_hash->fwd_result.l2.mac_sa[5] = 0;                               /* reserved */
		p_rule_hash->fwd_result.l2.mac_sa[4] = 1;                               /* 0: enc, 1: dec */
		p_rule_hash->fwd_result.l2.mac_sa[3] = sa_id & 0xFF;
		p_rule_hash->fwd_result.l2.mac_sa[2] = 0;                               /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
		p_rule_hash->fwd_result.l2.mac_sa[1] = p_tunnel_cfg->type & 0xFF;
	}
	crc32 = ~(calc_crc(~0, (u8 const *) &p_rule_hash->fwd_result.l2.mac_sa[1], 5));
	p_rule_hash->fwd_result.l2.mac_sa[0] = crc32 & 0xff;

	if (cs_rule_hash_add(0, p_rule_hash) != CS_E_OK) {
		printk(KERN_ERR "%s: ERROR! Fail to add hash for WAN to PE\n", __func__);
		return CS_E_ERROR;
	}

	//printk(KERN_DEBUG "%s: WAN2PE, hash_index=%u, voq_pol_idx=%u, fwd_rslt_idx=%u\n",
	//        __func__, p_rule_hash->hash_index, p_rule_hash->voq_pol_idx, p_rule_hash->fwd_rslt_idx);
	DBG(printk("%s: WAN2PE, hash_index=%u, voq_pol_idx=%u, fwd_rslt_idx=%u\n",
		__func__, p_rule_hash->hash_index, p_rule_hash->voq_pol_idx, p_rule_hash->fwd_rslt_idx));

	return CS_OK;
}

cs_status_t cs_tunnel_pe2wan_rule_hash_add(
	cs_tunnel_cfg_t *p_tunnel_cfg,
	cs_uint8_t      is_natt,
	cs_l3_ip_addr   *p_da_ip,
	cs_l3_ip_addr   *p_sa_ip,
	cs_uint8_t      ip_version,
	cs_uint16_t     pppoe_session_id,
	cs_uint16_t     vlan_id,
	cs_uint8_t      *p_da_mac,
	cs_uint8_t      *p_sa_mac,
	cs_uint32_t     spi_idx,
	cs_uint16_t     natt_egress_dest_port,
	cs_rule_hash_t  *p_rule_hash,
	cs_uint16_t     *p_hash_index)
{
	/***** PE1 to WAN *****/

	memset(p_rule_hash, 0, sizeof(cs_rule_hash_t));

	/* get IP SA and DA */
	memcpy(&(p_rule_hash->key.sa[0]), p_sa_ip, sizeof(cs_l3_ip_addr));
	memcpy(&(p_rule_hash->key.da[0]), p_da_ip, sizeof(cs_l3_ip_addr));

	if (p_tunnel_cfg->type == CS_PPTP) {
		p_rule_hash->key.ip_prot = 47; /* GRE */
	}

	switch (p_tunnel_cfg->type) {
		case CS_PPTP:
			p_rule_hash->key.ip_prot = 47; /* GRE */
			break;
		case CS_L2TP:
			/* CS_HM_IP_PROT_MASK */
			p_rule_hash->key.ip_prot = IPPROTO_UDP;

			/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
			p_rule_hash->key.l4_valid = 1;
			p_rule_hash->key.l4_dp = p_tunnel_cfg->tunnel.l2tp.dest_port;
			p_rule_hash->key.l4_sp = p_tunnel_cfg->tunnel.l2tp.src_port;

			/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
			/* we use the default value 0 */

			break;
		case CS_L2TPV3:
			if (p_tunnel_cfg->tunnel.l2tp.encap_type == 1 /* IP */) {
				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = CS_IPPROTO_L2TP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				/* we use the default value 0 */

				/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
				/* we use the default value 0 */
			} else { /* UDP */
				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = IPPROTO_UDP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				p_rule_hash->key.l4_valid = 1;
				p_rule_hash->key.l4_dp = p_tunnel_cfg->tunnel.l2tp.dest_port;
				p_rule_hash->key.l4_sp = p_tunnel_cfg->tunnel.l2tp.src_port;

				/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
				/* we use the default value 0 */
			}

			break;
		case CS_IPSEC:
		case CS_L2TP_IPSEC:
		case CS_L2TPV3_IPSEC:
			if (is_natt != 0) {
				/*** CS_HASHMASK_TUNNEL_L4_L7 ***/

				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = IPPROTO_UDP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				p_rule_hash->key.l4_valid = 1;
				p_rule_hash->key.l4_dp = 4500;
				p_rule_hash->key.l4_sp = 4500;

				/* CS_HM_L7_FIELD_MASK | CS_HM_L7_FIELD_VLD_MASK */
				p_rule_hash->key.l7_field_valid = 1;
				p_rule_hash->key.l7_field = htonl(spi_idx);

				/* Remote may not NAT-T device. Replace dest port. */
				p_rule_hash->fwd_result.l4.dp = natt_egress_dest_port;
				p_rule_hash->fwd_result.l4.dp_replace_en = 1;
			} else {
				/* CS_HM_IP_PROT_MASK */
				p_rule_hash->key.ip_prot = IPPROTO_ESP;

				/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK */
				/* we use the default value 0 */

				/* CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK */
				p_rule_hash->key.spi_vld = 1;
				p_rule_hash->key.spi_idx = spi_idx;
			}

			break;
		default:
			printk("%s:%d unknown hash key for tunnel type %d\n", __func__, __LINE__, p_tunnel_cfg->type);
			return CS_E_CONFLICT;
	}

	p_rule_hash->key.ip_version = ip_version;
	p_rule_hash->key.ip_valid = 1;
	p_rule_hash->key.lspid = ENCAPSULATION_PORT;  /* PE1 */

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	if (is_natt != 0)
		p_rule_hash->apptype = CORE_FWD_APP_TYPE_TUNNEL_L4_L7;
	else
#endif
		p_rule_hash->apptype = CORE_FWD_APP_TYPE_TUNNEL;

	/* set FVLAN command */
	p_rule_hash->fvlan.first_vid = vlan_id;

	if (p_rule_hash->fvlan.first_vid > 0) {
		p_rule_hash->fvlan.first_vlan_cmd = CS_FE_VLAN_CMD_SWAP_B;
		p_rule_hash->fwd_result.l2.flow_vlan_op_en = 1;
	} else {
		p_rule_hash->fvlan.first_vlan_cmd = CS_FE_VLAN_CMD_POP_A;
		p_rule_hash->fwd_result.l2.flow_vlan_op_en = 1;
	}

	p_rule_hash->fvlan.first_tpid_enc = 0;
	p_rule_hash->fvlan.second_vid = 0;
	p_rule_hash->fvlan.second_vlan_cmd = 0;
	p_rule_hash->fvlan.second_tpid_enc = 0;

	/* set PPPoE session */
	if (pppoe_session_id != 0) {
		p_rule_hash->fwd_result.l2.pppoe_encap_en = 1;
		p_rule_hash->voq_pol.pppoe_session_id = pppoe_session_id;
	}

	p_rule_hash->voq_pol.voq_base = GE_PORT0_VOQ_BASE;

	/* set fwd result */
	p_rule_hash->fwd_result.act.fwd_type = FE_CLASS_FWDTYPE_NORMAL;
	p_rule_hash->fwd_result.act.fwd_type_valid = 1;

	p_rule_hash->fwd_result.l2.mac_sa_replace_en = 1;
	memcpy(&(p_rule_hash->fwd_result.l2.mac_sa[0]), p_sa_mac, CS_ETH_ADDR_LEN);

	p_rule_hash->fwd_result.l2.mac_da_replace_en = 1;
	memcpy(&(p_rule_hash->fwd_result.l2.mac_da[0]), p_da_mac, CS_ETH_ADDR_LEN);

	if (cs_rule_hash_add(0, p_rule_hash) != CS_E_OK) {
		printk(KERN_ERR "%s: ERROR! Fail to add hash for PE to WAN\n", __func__);
		return CS_E_ERROR;
	}

	DBG(printk("%s: PE2WAN, hash_index=%u, voq_pol_idx=%u, fwd_rslt_idx=%u\n",
		__func__, p_rule_hash->hash_index, p_rule_hash->voq_pol_idx, p_rule_hash->fwd_rslt_idx));


	if (p_tunnel_cfg->type == CS_PPTP) {

		/***** PE0 to WAN: for GRE ACK *****/
		p_rule_hash->key.lspid = ENCRYPTION_PORT; /* PE0 for gre_ack pkt */
		if (cs_rule_hash_add(0, p_rule_hash) != CS_E_OK) {
			printk(KERN_ERR "%s: ERROR! Fail to add hash for PE0 to WAN\n", __func__);
			return CS_E_ERROR;
		}
		*p_hash_index = p_rule_hash->hash_index;

		printk("%s: PE2WAN for GRE ACK (from PE0), hash_index=%u, voq_pol_idx=%u, fwd_rslt_idx=%u\n",
			__func__, p_rule_hash->hash_index, p_rule_hash->voq_pol_idx, p_rule_hash->fwd_rslt_idx);
	}

	return CS_OK;
}

