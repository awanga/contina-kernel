/*
 * Copyright (c) Cortina-Systems Limited 2013.  All rights reserved.
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
 *  cs_hw_accel_ip_translate.h
 *
 *
 * This header file defines the data structures and APIs for CS IP Translate
 * Acceleration.
 */

#ifndef __CS_HW_ACCEL_IP_TRANSLATE_H__
#define __CS_HW_ACCEL_IP_TRANSLATE_H__

#include "cs_hw_accel_ipc.h"
#include "cs_hw_accel_sa_id.h"
#include <mach/cs_ip_translate.h>
#include <mach/cs_flow_api.h>
#include <mach/cs_route_api.h>

#ifdef CS_IPC_ENABLED
#include <mach/g2cpu_ipc.h>
#endif

typedef struct cs_ip_translate_flow_s {
	struct cs_ip_translate_flow_s * next;
	struct cs_ip_translate_flow_s * prev;
	cs_flow_t				flow_entry;
	cs_flow_t       		flow_entry_to_pe; /* LAN to PE flow hash */
	cs_flow_t       		flow_entry_from_pe; /* PE to LAN flow hash */
	cs_flow_t       		over_size_entry_from_pe; /* PE-> LAN# CPU# port flow hash for over-size packets */
}cs_ip_translate_flow_t;

typedef struct cs_ip_translate_tunnel_s {
	cs_dev_id_t 			device_id;
	cs_ip_translate_cfg_t	ip_translate_cfg;
	cs_ip_translate_flow_t *napt_flow;
	cs_ip_translate_id_t	ip_translate_id; /*equal to egress_sa_id*/
	cs_flow_t       		tunnel_entry_to_pe; /* WAN to PE tunnel hash */
	cs_flow_t       		tunnel_entry_from_pe; /* PE to WAN tunnel hash */
	cs_uint16_t				inbound_next_func_id;
	cs_uint16_t				inbound_next_said;
	cs_uint16_t				outbound_next_func_id;
	cs_uint16_t				outbound_next_said;
} cs_ip_translate_tunnel_t;


cs_status_t cs_ip_translate_sa_id_get_by_ip_translate_id(
	CS_IN	cs_ip_translate_id_t	ip_translate_id,
	CS_IN	cs_uint16_t	dir,
	CS_OUT	cs_uint16_t	*sa_id);

cs_status_t cs_ip_translate_set_src_mac(
	CS_IN	cs_l3_nexthop_t *nexthop,
	CS_OUT	char *src_mac);

cs_status_t cs_ip_translate_tunnel_handle(
	struct sk_buff 					*skb);

/* IPC functions */
#ifdef CS_IPC_ENABLED
cs_status_t
__cs_ip_translate_ipc_add_entry(cs_ip_translate_tunnel_t *tunnel);

cs_status_t
__cs_ip_translate_ipc_del_entry(cs_ip_translate_tunnel_t *tunnel);

cs_status_t
__cs_ip_translate_ipc_set_portset(void);

cs_status_t
cs_ip_translate_ipc_rcv_set_entry_ack(
		struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context
		);

cs_status_t
cs_ip_translate_ipc_rcv_del_entry_ack(
		struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context
		);

cs_status_t
cs_ip_translate_nexthop_set(cs_uint8_t dir, cs_uint8_t tunnel_type, cs_uint16_t tunnel_id,
	cs_uint16_t next_tunnel_id);

cs_status_t
cs_ip_translate_nexthop_del(cs_uint8_t dir, cs_uint8_t tunnel_type, cs_uint16_t tunnel_id,
	cs_uint16_t next_tunnel_id);

#endif /* CS_IPC_ENABLED */

void cs_hw_accel_ip_translate_init(void);
void cs_hw_accel_ip_translate_exit(void);


#endif /* __CS_HW_ACCEL_IP_TRANSLATE_H__ */
