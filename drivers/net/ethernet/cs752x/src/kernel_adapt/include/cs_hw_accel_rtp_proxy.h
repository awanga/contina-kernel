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
 *  cs_hw_accel_rtp_proxy.h
 *
 *
 * This header file defines the data structures and APIs for CS RTP proxy
 * Acceleration.
 */

#ifndef __CS_HW_ACCEL_RTP_PROXY_H__
#define __CS_HW_ACCEL_RTP_PROXY_H__

#include "cs_hw_accel_ipc.h"
#include "cs_hw_accel_sa_id.h"

#include <mach/cs_flow_api.h>
#include <mach/cs_rtp_proxy.h>

#ifdef CS_IPC_ENABLED
#include <mach/g2cpu_ipc.h>
#endif

/* TODO: ID management */
#define CS_RTP_ID_BASE	0
#define CS_RTP_PE0_STATS_BASE	0xF6A3E000
#define CS_RTP_PE1_STATS_BASE	0xF6A3F000

typedef struct cs_rtp_entry_s {
	cs_rtp_id_t     rtp_id;
	cs_rtp_cfg_t   rtp_cfg;
	cs_rtp_dir_t    dir;
	cs_uint16_t    sa_id;
	cs_flow_t       flow_entry_to_pe; /* LAN to PE or WAN to PE flow hash */
	cs_flow_t       flow_entry_from_pe; /* PE to LAN or PE to WAN flow hash */
	struct cs_rtp_entry_s	*prev;
	struct cs_rtp_entry_s	*next;
} cs_rtp_entry_t;

typedef struct cs_rtp_proxy_cb_s {
	cs_dev_id_t	device_id;
	cs_rtp_entry_t	*rtp_h;	/* rtp list head */
	cs_uint16_t		rtp_cnt;		/* rtp counter */
	/* increase curr_rtp_id to generate an unique rtp_id */
	cs_uint16_t		curr_rtp_id;
	struct cs_rtp_proxy_cb_s *next;
} cs_rtp_proxy_cb_t;

/* IPC functions */
#ifdef CS_IPC_ENABLED
cs_status_t
cs_rtp_ipc_send_set_entry(
		cs_sa_id_direction_t direction,
		cs_rtp_cfg_t *p_rtp_cfg,
		cs_uint8_t *egress_sa_mac,
		cs_uint16_t sa_id
		);

cs_status_t
cs_rtp_ipc_send_del_entry(
		cs_sa_id_direction_t direction,
		cs_uint16_t sa_id
		);

cs_status_t 
cs_rtp_ipc_rcv_set_entry_ack(
		struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context
		);

cs_status_t 
cs_rtp_ipc_rcv_del_entry_ack(
		struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context
		);
#endif /* CS_IPC_ENABLED */

/* Internal functions*/

cs_status_t construct_flow_entries(
	CS_IN	cs_rtp_entry_t *p_rtp_node,
	CS_IN	cs_flow_t *p_flow_entry);

cs_status_t cs_rtp_entry_get_by_sa_id(
	CS_IN	cs_uint16_t				sa_id,
	CS_IN	cs_sa_id_direction_t			dir,
	CS_OUT	cs_rtp_entry_t  			**p_rtp);

cs_status_t cs_rtp_translate_sa_id_get_by_rtp_id(
	CS_IN	cs_rtp_id_t				rtp_id,
	CS_OUT	cs_uint16_t				*sa_id);

void cs_hw_accel_rtp_proxy_init(void);
void cs_hw_accel_rtp_proxy_exit(void);


#endif /* __CS_HW_ACCEL_RTP_PROXY_H__ */
