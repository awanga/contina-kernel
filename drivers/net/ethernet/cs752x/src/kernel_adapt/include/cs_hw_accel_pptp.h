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
#ifndef __CS_HW_ACCEL_PPTP_H__
#define __CS_HW_ACCEL_PPTP_H__

#include <linux/ip.h>
#include <linux/netfilter/nf_conntrack_proto_gre.h>
#include <mach/cs_tunnel.h>
#include <mach/cs_route_api.h>
#include "cs_hw_accel_sa_id.h"

#define MAX_PPTP_SESSION	32	/* max number of PPTP sessions to offload, restricted by PE memory size */

struct pptp_db {
	int valid;

	cs_tunnel_id_t tunnel_id;
	cs_pptp_sa_t session;

	cs_uint16_t hash_idx;	/* hash index of norrmal PPTP packet */
	cs_uint16_t hash_idx2;	/* hash index of GRE ACK packet */
};

struct tunnel_db {
	int valid;

	int idx_up_db;		/* index of PPTP session in upstream DB */
	int idx_down_db;	/* index of PPTP session in downstream DB */
};

/**************************************************************************/

/* structures for IPC */
#define CS_PPTP_IPC_CLNT_ID		0x2
#define CS_PPTP_TBL_SIZE		64
/* largest key length (128-bit) */
#define CS_MPPE_MAX_KEY_LEN		16

/* CPU send to PE */
#define CS_PPTP_IPC_PE_RESET		0x01
#define CS_PPTP_IPC_PE_STOP		0x02
#define CS_PPTP_IPC_PE_DUMP_TBL		0x05
#define CS_PPTP_IPC_PE_ECHO		0x06
#define CS_PPTP_IPC_PE_MIB_EN		0x07
#define CS_PPTP_IPC_PE_MAX		CS_PPTP_IPC_PE_MIB_EN

/* PE send to CPU */
#define CS_PPTP_IPC_PE_RESET_ACK	0x11
#define CS_PPTP_IPC_PE_STOP_ACK		0x12
#define CS_PPTP_IPC_PE_SET_ACK		0x13
#define CS_PPTP_IPC_PE_DEL_ACK		0x14
#define CS_PPTP_IPC_PE_ECHO_ACK		0x15
#define CS_PPTP_IPC_PE_MAX_ACK		CS_PPTP_IPC_PE_ECHO_ACK

typedef struct pptp_entry_s {
	unsigned int crc32;	/* crc32 of pptp_tunnel_hdr */
	union {
		struct cs_pptp_tunnel_hdr_s {
			struct ethhdr ethh;
			struct iphdr iph;
#ifdef gre_hdr_pptp
			struct gre_hdr_pptp gre;
#else
			struct gre_full_hdr gre;
#endif
		} __attribute__((packed)) cs_pptp_tunnel_hdr;
		unsigned char pptp_octet[sizeof(struct cs_pptp_tunnel_hdr_s)];
	};

	unsigned char valid;	/* 0: invalid 1: valid */

	unsigned char dir;	/* 0: unknown, 1: encrypt, 2: decrypt */
	char start_key[CS_MPPE_MAX_KEY_LEN];

	/* key length in bytes, SPACC only support 40-bit, 128-bit keys */
	unsigned char keylen;	
} __attribute__((packed)) cs_pptp_entry_t;

/* CS_PPTP_IPC_PE_SET_ENTRY */
typedef struct cs_pptp_ipc_msg_set_s {
	unsigned char idx;
	unsigned char resvd[3];
	cs_pptp_entry_t	pptp_entry;
} __attribute__ ((__packed__)) cs_pptp_ipc_msg_set_t;

/* CS_PPTP_IPC_PE_DEL_ENTRY */
typedef struct cs_pptp_ipc_msg_del_s {
	unsigned char idx;
	unsigned char resvd[3];
	unsigned int crc32;
} __attribute__ ((__packed__)) cs_pptp_ipc_msg_del_t;

/* CS_PPTP_IPC_PE_MIB_EN_ENTRY */
typedef struct cs_pptp_ipc_msg_en_s {
	unsigned char enbl;	/* 0: disable, 1: enable */
	unsigned char resvd[3];
} __attribute__ ((__packed__)) cs_pptp_ipc_msg_en_t;



/* internal structure */


#ifdef CS_IPC_ENABLED
/* IPC handlers */
cs_status_t cs_pptp_ipc_send_reset(cs_sa_id_direction_t dir);
cs_status_t cs_pptp_ipc_send_stop(cs_sa_id_direction_t dir);
cs_status_t cs_pptp_ipc_send_set_entry(cs_sa_id_direction_t dir, cs_tunnel_cfg_t *p_tunnel_cfg);
cs_status_t cs_pptp_ipc_send_del_entry(cs_sa_id_direction_t dir, cs_uint16_t sa_idx);
cs_status_t cs_pptp_ipc_set_generic_sa_idx(cs_sa_id_direction_t dir);
cs_status_t cs_pptp_ipc_send_mib_en(cs_sa_id_direction_t dir, cs_uint8_t enable);
cs_status_t cs_pptp_ipc_inform_lost_pkt(cs_sa_id_direction_t dir);
cs_status_t cs_pptp_ipc_key_change(cs_uint16_t sa_idx);
cs_status_t cs_pptp_ipc_send_dump(cs_sa_id_direction_t dir, cs_uint8_t sa_id);
cs_status_t cs_pptp_ipc_send_echo(cs_sa_id_direction_t dir);

/* IPC callback handlers */
cs_status_t cs_pptp_ipc_del_entry_ack(struct ipc_addr peer,
				unsigned short msg_no, const void *msg_data,
				unsigned short msg_size,
				struct ipc_context *context);

cs_status_t cs_pptp_ipc_inform_lost_pkt_ack(struct ipc_addr peer,
				unsigned short msg_no, const void *msg_data,
				unsigned short msg_size,
				struct ipc_context *context);
#endif /* CS_IPC_ENABLED */


cs_status_t cs_pptp_tunnel_add(cs_tunnel_cfg_t *p_tunnel_cfg);
cs_status_t cs_pptp_tunnel_delete(cs_tunnel_cfg_t *p_tunnel_cfg);
cs_status_t cs_pptp_tunnel_delete_by_idx(cs_tunnel_id_t tunnel_id);

cs_status_t cs_pptp_set_src_mac(cs_l3_nexthop_t *nexthop, char *src_mac);

cs_status_t cs_pptp_tunnel_id_to_sa_id(cs_tunnel_id_t tunnel_id, cs_tunnel_dir_t dir, cs_uint16_t *sa_id);

/* module API */
cs_status_t cs_hw_accel_pptp_init(void);
cs_status_t cs_hw_accel_pptp_exit(void);


#endif /* __CS_HW_ACCEL_PPTP_H__ */
