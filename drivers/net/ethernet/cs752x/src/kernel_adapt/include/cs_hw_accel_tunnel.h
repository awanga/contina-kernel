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
 * cs_iplip.h
 *
 *
 * This header file defines the data structures and APIs for CS Tunnel
 * Acceleration.
 * Currently supported:
 * 	IPv6 over PPP over L2TP over IPv4 over PPPoE (IPLIP) Kernel Module.
 */
#ifndef __CS_HW_ACCEL_TUNNEL_H__
#define __CS_HW_ACCEL_TUNNEL_H__

#include <linux/ip.h>
#include <linux/udp.h>
#include <mach/cs_tunnel.h>
#include <mach/cs_route_api.h>
#include <mach/cs_rule_hash_api.h>
#include "cs_hw_accel_sa_id.h"
#include "cs_hw_accel_ipc.h"

#define CONFIG_CS75XX_HW_ACCEL_L2TPV3_IPSEC

/*******************************************************************/

/* Tunnel ID (cs_tunnel_id_t) range for each tunnel type */
#define TID_IPLIP_BASE		0x00000000
#define TID_IPLIP_MAX		0x000000FF

#define TID_L2TP_IPSEC_BASE	0x00000200
#define TID_L2TP_IPSEC_MAX	0x000004FF

#define TID_PPTP_BASE		0x00000500
#define TID_PPTP_MAX		0x000007FF

#define TID_INVALID		0xFFFFFFFF

/*******************************************************************/

#define CS_IPLIP_TBL_SIZE	64
#define CS_TUNNEL_TBL_SIZE	64
#define CS_IPPROTO_L2TP	115

struct cs_iplip_hdr1_s {
	struct ethhdr		ethh;
	cs_pppoe_hdr_t		pppoeh;
	cs_ppp2_pro_t		ppp2h;
	struct iphdr		iph;
	struct udphdr		udph;
	cs_l2tp_hdr1_t		l2tph;
	cs_ppp_hdr_t		ppph;
} __attribute__((packed));

struct cs_iplip_hdr2_s {
	struct ethhdr		ethh;
	cs_pppoe_hdr_t		pppoeh;
	cs_ppp2_pro_t		ppp2h;
	struct iphdr		iph;
	struct udphdr		udph;
	cs_l2tp_hdr2_t		l2tph;
	cs_ppp_hdr_t		ppph;
} __attribute__((packed));

struct cs_iplip_hdr3_s {
	struct ethhdr		ethh;
	cs_pppoe_hdr_t		pppoeh;
	cs_ppp2_pro_t		ppp2h;
	struct iphdr		iph;
	struct udphdr		udph;
	cs_l2tp_hdr3_t		l2tph;
	cs_ppp_hdr_t		ppph;
} __attribute__((packed));

struct cs_iplip_hdr4_s {
	struct ethhdr		ethh;
	cs_pppoe_hdr_t		pppoeh;
	cs_ppp2_pro_t		ppp2h;
	struct iphdr		iph;
	struct udphdr		udph;
	cs_l2tp_hdr4_t		l2tph;
	cs_ppp_hdr_t		ppph;
} __attribute__((packed));

typedef struct iplip_entry1_s {
	unsigned int		crc32;		/* crc32 of iplip_hdr */
	union {
		struct cs_iplip_hdr1_s iplip_hdr;
		unsigned char	      iplip_octet[sizeof(struct cs_iplip_hdr1_s)];
	};
	unsigned char		valid;	/*0: invalid 1: valid*/
	unsigned char		dir;	/* 0: unknown, 1: upstream, 2: downstream */
} __attribute__((packed)) cs_iplip_entry1_t;

typedef struct iplip_entry2_s {
	unsigned int		crc32;		/* crc32 of iplip_hdr */
	union {
		struct cs_iplip_hdr2_s iplip_hdr;
		unsigned char	      iplip_octet[sizeof(struct cs_iplip_hdr2_s)];
	};
	unsigned char		valid;	/*0: invalid 1: valid*/
	unsigned char		dir;	/* 0: unknown, 1: upstream, 2: downstream */
} __attribute__((packed)) cs_iplip_entry2_t;

typedef struct iplip_entry3_s {
	unsigned int		crc32;		/* crc32 of iplip_hdr */
	union {
		struct cs_iplip_hdr3_s iplip_hdr;
		unsigned char	      iplip_octet[sizeof(struct cs_iplip_hdr3_s)];
	};
	unsigned char		valid;	/*0: invalid 1: valid*/
	unsigned char		dir;	/* 0: unknown, 1: upstream, 2: downstream */
} __attribute__((packed)) cs_iplip_entry3_t;

typedef struct iplip_entry4_s {
	unsigned int		crc32;		/* crc32 of iplip_hdr */
	union {
		struct cs_iplip_hdr4_s iplip_hdr;
		unsigned char	      iplip_octet[sizeof(struct cs_iplip_hdr4_s)];
	};
	unsigned char		valid;	/*0: invalid 1: valid*/
	unsigned char		dir;	/* 0: unknown, 1: upstream, 2: downstream */
} __attribute__((packed)) cs_iplip_entry4_t;

typedef cs_iplip_entry2_t cs_iplip_entry_t;

/* structures for IPC */
#define CS_IPLIP_IPC_CLNT_ID		0x5

/* CPU send to PE */
#define CS_IPLIP_IPC_PE_RESET		0x01
#define CS_IPLIP_IPC_PE_STOP		0x02
#define CS_IPLIP_IPC_PE_SET_ENTRY	0x03
#define CS_IPLIP_IPC_PE_DEL_ENTRY	0x04
#define CS_IPLIP_IPC_PE_DUMP_TBL	0x05
#define CS_IPLIP_IPC_PE_ECHO		0x06
#define CS_IPLIP_IPC_PE_MIB_EN		0x07

#define CS_IPLIP_IPC_PE_MAX		CS_IPLIP_IPC_PE_MIB_EN

/* PE send to CPU */
#define CS_IPLIP_IPC_PE_RESET_ACK	0x11
#define CS_IPLIP_IPC_PE_STOP_ACK	0x12
#define CS_IPLIP_IPC_PE_SET_ACK		0x13
#define CS_IPLIP_IPC_PE_DEL_ACK		0x14
#define CS_IPLIP_IPC_PE_ECHO_ACK	0x15

#define CS_IPLIP_IPC_PE_MAX_ACK		CS_IPLIP_IPC_PE_ECHO_ACK

/* CS_IPLIP_IPC_PE_INIT */
/* CS_IPLIP_IPC_PE_STOP */
/* CS_IPLIP_IPC_PE_DUMP_TBL */
/* No data */

/* CS_IPLIP_IPC_PE_SET_ENTRY */
typedef struct cs_iplip_ipc_msg_set1_s {
	unsigned char		idx;
	unsigned char		l2tp_type; /* cs_l2tp_type_t */
	unsigned char		resvd[2];
	cs_iplip_entry1_t	iplip_entry;
} __attribute__ ((__packed__)) cs_iplip_ipc_msg_set1_t;

typedef struct cs_iplip_ipc_msg_set2_s {
	unsigned char		idx;
	unsigned char		l2tp_type; /* cs_l2tp_type_t */
	unsigned char		resvd[2];
	cs_iplip_entry2_t	iplip_entry;
} __attribute__ ((__packed__)) cs_iplip_ipc_msg_set2_t;

typedef struct cs_iplip_ipc_msg_set3_s {
	unsigned char		idx;
	unsigned char		l2tp_type; /* cs_l2tp_type_t */
	unsigned char		resvd[2];
	cs_iplip_entry3_t	iplip_entry;
} __attribute__ ((__packed__)) cs_iplip_ipc_msg_set3_t;

typedef struct cs_iplip_ipc_msg_set4_s {
	unsigned char		idx;
	unsigned char		l2tp_type; /* cs_l2tp_type_t */
	unsigned char		resvd[2];
	cs_iplip_entry4_t	iplip_entry;
} __attribute__ ((__packed__)) cs_iplip_ipc_msg_set4_t;


/* CS_IPLIP_IPC_PE_DEL_ENTRY */
typedef struct cs_iplip_ipc_msg_del_s {
	unsigned char		idx;
	unsigned char		resvd[3];
} __attribute__ ((__packed__)) cs_iplip_ipc_msg_del_t;

/* CS_IPLIP_IPC_PE_MIB_EN_ENTRY */
typedef struct cs_iplip_ipc_msg_en_s {
	unsigned char		enbl;	/* 0: disable, 1: enable */
	unsigned char		resvd[3];
} __attribute__ ((__packed__)) cs_iplip_ipc_msg_en_t;


#define CS_TUNNEL_TID(pppoe_port_id, index) ((pppoe_port_id << 16) | index & 0xFFFF)
#define CS_TUNNEL_GET_PID_FROM_TID(tunnel_id) (tunnel_id >> 16)
#define CS_TUNNEL_GET_IDX_FROM_TID(tunnel_id) (tunnel_id & 0xFFFF)

/* internal structure */
typedef struct cs_ip_address_entry_s {
	cs_ip_address_t		ip;
	struct cs_ip_address_entry_s	*prev;
	struct cs_ip_address_entry_s	*next;
	struct cs_l2tp_session_entry_s	*session;	/* parent */

	/* LAN --> PE hash information */
	unsigned int voq_pol_idx;
	unsigned int fwd_rslt_idx;
	unsigned short fwd_hash_idx;
} cs_ip_address_entry_t;

typedef struct cs_l2tp_session_entry_s {
	cs_session_id_t		session_id;
	cs_ip_address_entry_t	*ip_h;	/* ip list head */
	unsigned int		ip_cnt;	/* ip entry counter */
	unsigned char		iplip_idx;	/* index in IPLIP table */
	struct cs_l2tp_session_entry_s	*prev;
	struct cs_l2tp_session_entry_s	*next;
	struct cs_tunnel_entry_s	*tunnel;	/* parent */
} cs_l2tp_session_entry_t;

typedef enum {
	CS_TUNNEL_INIT 		= 0,
	CS_TUNNEL_ACCELERATED 	= 1,
	CS_TUNNEL_INVALID	= 0xFFFFFFFF,
} cs_tunnel_state_t;

typedef enum {
	CS_TUNNEL_HASH_PE_TO_WAN 	= 0,
	CS_TUNNEL_HASH_WAN_TO_PE 	= 1,
	CS_TUNNEL_HASH_PE1_TO_CPU	= 2,
	CS_TUNNEL_HASH_TYPE_MAX		= 3,
} cs_tunnel_hash_type_t;

/* Only use the lower 32-bit of swid due to limitation of cs_flow_add() */
#define CS_TUNNEL_SWID(tunnel_type, dir, sa_id)	\
	(CS_SWID64_MASK(CS_SWID64_MOD_ID_IPSEC) | ((u64)0xEC << 24) | \
	((u64)tunnel_type << 16) | ((u64)dir << 8) | (sa_id))

typedef struct cs_tunnel_entry_s {
	cs_dev_id_t		device_id;
	cs_tunnel_id_t		tunnel_id;
	cs_tunnel_cfg_t 	tunnel_cfg;
	cs_l2tp_session_entry_t	*se_h;	/* session list head */
	unsigned char		se_cnt;	/* session counter */
	unsigned int		ti_cnt;	/* total IPv6 prefix counter */
	cs_uint16_t		sa_id;
	cs_uint64		sw_id;
	cs_tunnel_state_t	state;
	struct cs_tunnel_entry_s	*prev;
	struct cs_tunnel_entry_s	*next;
	struct cs_pppoe_port_entry_s	*pppoe_port;	/* parent */

	/* hash information */
	/* 0: PE --> WAN
	   1: WAN --> PE
	   2: PE1 --> CPU */
	unsigned int voq_pol_idx[CS_TUNNEL_HASH_TYPE_MAX];
	unsigned int vlan_rslt_idx[CS_TUNNEL_HASH_TYPE_MAX];
	unsigned int fwd_rslt_idx[CS_TUNNEL_HASH_TYPE_MAX];
	unsigned short fwd_hash_idx[CS_TUNNEL_HASH_TYPE_MAX];
} cs_tunnel_entry_t;

typedef struct cs_pppoe_port_entry_s {
	cs_dev_id_t	device_id;
	cs_port_id_t	port_id;
	cs_port_id_t	pppoe_port_id;
	int		pppoe_ifindex;	/* identify tunnel direction */
	int		ppp_ifindex;	/* identify IPv6 routing destination */
	cs_pppoe_port_cfg_t	p_cfg;
	cs_tunnel_entry_t	*tu_h;	/* tunnel list head */
	unsigned char		tu_cnt;		/* tunnel counter */
	//unsigned short		tu_idx;		/* increasing index to generate unique tunnel_id */
	struct cs_pppoe_port_entry_s *next;
} cs_pppoe_port_entry_t;

typedef struct cs_pppoe_port_list_s {
	cs_pppoe_port_entry_t *port_list_hdr;
	unsigned int	pppoe_port_cnt;		/* pppoe_port counter */
	unsigned char	tt_cnt;			/* total tunnel counter */
	unsigned char	ts_cnt;			/* total session counter */
	cs_tunnel_entry_t *tu_list[CS_TUNNEL_TBL_SIZE]; /* tunnel list */
} cs_pppoe_port_list_t;

typedef struct cs_iplip_tbl_s {
	cs_iplip_entry_t entry[CS_IPLIP_TBL_SIZE];
	cs_l2tp_session_entry_t	*session_ptr[CS_IPLIP_TBL_SIZE];
} cs_iplip_tbl_t;

typedef union {
	unsigned char octet[ETH_ALEN];
	struct {
		unsigned char index;
		unsigned char rsvd;
		unsigned int crc32;
	} __attribute__ ((__packed__));
} cs_iplip_mac_t;

typedef struct cs_l2tp_cb_s {
	cs_tunnel_entry_t	*tu_h;	/* tunnel list head */
	unsigned char		tu_cnt;		/* tunnel counter */
	/* increase curr_tu_id to generate an unique tunnel_id */
	unsigned short		curr_tu_id;
	struct cs_l2tp_cb_s *next;
} cs_l2tp_cb_t;

typedef struct cs_ip_mask_s {
	cs_uint32_t				mask[4];
	cs_uint32_t				result[4];
} cs_ip_mask_t;

/*******************************************************************/

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
/*********************IPSec*********************************/
/*
 * spd_handle: array index of cs_ipsec_spd[]
 *	0: encryption
 *	1: decryption
 */
#define CS_IPSEC_SPD_MAX	2


#define CS_IPSEC_SA_CNT_MAX	3

#define CS_IPSEC_SA_HANDLE_TO_ID(x) (x - 1)
#define CS_IPSEC_SA_ID_TO_HANDLE(x) (x + 1)

typedef struct cs_ipsec_policy_node_s {
	cs_boolean_t				valid;
	cs_dev_id_t				device_id;
	cs_uint32_t				spd_handle;
	cs_ipsec_policy_t			policy;
	cs_uint32_t				policy_handle;

	/* convert from policy.selector_array[].src_ip */
	cs_ip_mask_t 				src_ip_mask[CS_IPSEC_SEL_MAX];
	/* convert from policy.selector_array[].dst_ip */
	cs_ip_mask_t 				dst_ip_mask[CS_IPSEC_SEL_MAX];

	/* Currently we only support one SA per policy
	cs_uint8_t				sa_count;
	cs_ipsec_sa_t				*sa_array;
	*/
	cs_boolean_t				sa_valid;
	cs_ipsec_sa_t 				sa;

	void					*xfrm_state;
	/* sa_handle = sa_id + 1
	 * sa_handle	0: invalid
	 *           1~24: denote the entry occupies a resource of crypto engine
	 */
	cs_uint32_t				sa_handle;
	cs_uint32_t				previous_sa_handle;
	/* tunnel_id is returned from cs_tunnel_add().
	   Valid tunnel_id is started from CS_IPSEC_TUNNEL_ID_BASE */
	cs_uint32_t				tunnel_id;
	struct cs_ipsec_policy_node_s		*next;
} cs_ipsec_policy_node_t;

typedef struct cs_ipsec_spd_s {
	cs_boolean_t				valid;
	cs_dev_id_t				device_id;
	cs_ipsec_spd_direction_t		direction;
	cs_uint32_t				spd_id;

	/* curr_policy_handle: 0 is invalid. It starts from 1 */
	cs_uint32_t				curr_policy_handle;
	cs_uint32_t				policy_cnt;
	cs_ipsec_policy_node_t 			*p_h;
} cs_ipsec_spd_t;

#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

/*******************************************************************/

/* exported APIs */
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
void cs_hw_accel_iplip_init(void);
void cs_hw_accel_iplip_exit(void);
int cs_is_ppp_tunnel_traffic(struct sk_buff *skb);
void cs_lan2pe_hash_add(struct sk_buff *skb);
int cs_iplip_ppp_ifindex_set(int pppoe_port, int ppp_ifindex);
int cs_iplip_pppoe_ifindex_set(int pppoe_port, int pppoe_ifindex);

cs_status_t cs_iplip_tunnel_add(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_cfg_t				*p_tunnel_cfg,
	CS_OUT	cs_tunnel_id_t				*p_tunnel_id);

cs_status_t cs_iplip_tunnel_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_cfg_t				*p_tunnel_cfg);

cs_status_t cs_iplip_tunnel_delete_by_idx(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id);

cs_status_t cs_iplip_tunnel_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_OUT	cs_tunnel_cfg_t				*p_tunnel_cfg);

cs_status_t cs_iplip_l2tp_session_add(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id);

cs_status_t cs_iplip_l2tp_session_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id);

cs_status_t cs_iplip_l2tp_session_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id,
	CS_OUT	cs_boolean_t				*is_present);




/* IPC handlers */

int
cs_iplip_ipc_send_reset(void);

int
cs_iplip_ipc_send_stop(void);

int
cs_iplip_ipc_send_set_entry(
		unsigned char		idx,
		cs_iplip_entry_t	*iplip_entry
		);

int
cs_iplip_ipc_send_del_entry(
		unsigned char		idx
		);

int
cs_iplip_ipc_send_dump(void);

int
cs_iplip_ipc_send_echo(void);

int
cs_iplip_ipc_send_mib_en(
		unsigned char		enbl
		);
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
void cs_hw_accel_l2tp_ipsec_init(void);
void cs_hw_accel_l2tp_ipsec_exit(void);

cs_status_t cs_l2tp_ipsec_set_src_mac(
	CS_IN	cs_l3_nexthop_t 			*nexthop,
	CS_OUT	char 					*src_mac,
	CS_OUT	cs_uint16_t				*sa_id);

cs_status_t cs_ipsec_sel_ip_check(
	CS_IN cs_uint32_t				tunnel_id,
	CS_IN cs_ip_address_t				*ip_prefix,
	CS_OUT cs_boolean_t 				*is_valid);

cs_status_t cs_ipsec_sel_ip_check_2(
	CS_IN cs_tunnel_cfg_t				*t_cfg,
	CS_IN cs_ipsec_policy_t 			*p,
	CS_OUT cs_boolean_t 				*is_valid);

cs_status_t cs_l2tp_ipsec_tunnel_add(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_cfg_t				*p_tunnel_cfg,
	CS_OUT	cs_tunnel_id_t				*p_tunnel_id);

cs_status_t cs_l2tp_tunnel_delete(
	CS_IN	cs_dev_id_t       			device_id,
	CS_IN	cs_tunnel_cfg_t   			*p_tunnel_cfg);

cs_status_t cs_ipsec_tunnel_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_cfg_t				*p_tunnel_cfg);

cs_status_t cs_l2tp_ipsec_tunnel_delete_by_idx(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id);

cs_status_t cs_l2tp_tunnel_get(
	CS_IN  cs_dev_id_t				device_id,
	CS_IN  cs_tunnel_id_t				tunnel_id,
	CS_OUT cs_tunnel_cfg_t				*p_tunnel_cfg);

cs_status_t cs_l2tp_ipsec_session_add(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id);

cs_status_t cs_l2tp_ipsec_session_delete(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_session_id_t				session_id);

cs_status_t cs_l2tp_ipsec_session_get(
	CS_IN  cs_dev_id_t				device_id,
	CS_IN  cs_tunnel_id_t				tunnel_id,
	CS_IN  cs_session_id_t				session_id,
	CS_OUT cs_boolean_t				*is_present);

cs_status_t cs_ipsec_policy_node_get(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_OUT	cs_ipsec_policy_node_t			*p_node);

cs_status_t cs_ipsec_policy_node_get_by_sa_id(
	CS_IN	cs_sa_id_direction_t			dir,
	CS_IN	cs_uint16_t				sa_id,
	CS_OUT	cs_ipsec_policy_node_t			*p_node);

cs_status_t cs_ipsec_policy_node_get_by_spi (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				spi,
	CS_OUT	cs_ipsec_policy_node_t			*p_node);

cs_status_t cs_ipsec_policy_get (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_OUT	cs_ipsec_policy_t			*policy);

cs_status_t cs_ipsec_policy_get_by_spi (
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				spi,
	CS_OUT	cs_ipsec_policy_t			*policy,
	CS_OUT	cs_uint32_t				*policy_handle);

cs_status_t cs_ipsec_policy_tunnel_link(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_IN	cs_uint32_t				tunnel_id,
	CS_IN	cs_ip_address_t				*src_ip,
	CS_IN	cs_ip_address_t				*dest_ip);

cs_status_t cs_ipsec_policy_xfrm_link(
	CS_IN	cs_dev_id_t				device_id,
	CS_IN	cs_uint32_t				spd_handle,
	CS_IN	cs_uint32_t				policy_handle,
	CS_IN	void					*xfrm_state);

cs_status_t cs_l2tp_sa_id_set(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_uint16_t				sa_id);

cs_status_t cs_l2tp_session_id_get(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_OUT	cs_uint32_t				*session_id);

cs_status_t cs_tunnel_state_set(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_tunnel_state_t			state);

cs_status_t cs_tunnel_hash_index_set(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_tunnel_hash_type_t			type,
	CS_IN	cs_uint32_t				voq_pol_idx,
	CS_IN	cs_uint32_t				vlan_rslt_idx,
	CS_IN	cs_uint32_t				fwd_rslt_idx,
	CS_IN	cs_uint16_t				hash_idx);

cs_status_t cs_tunnel_get_by_sa_id(
	CS_IN	cs_uint16_t				sa_id,
	CS_IN	cs_sa_id_direction_t			dir,
	CS_OUT	cs_tunnel_entry_t  			*p_tunnel);

cs_status_t cs_l2tp_tunnel_get_by_tid(
	CS_IN	cs_tunnel_dir_t				dir,
	CS_IN	cs_uint32_t				tid,
	CS_OUT	cs_tunnel_entry_t  			*p_tunnel);

cs_status_t cs_l2tp_ipsec_sa_id_get_by_tunnel_id(
	CS_IN	cs_tunnel_id_t				tunnel_id,
	CS_IN	cs_tunnel_type_t			tunnel_type,
	CS_IN	cs_tunnel_dir_t				dir,
	CS_OUT	cs_uint16_t				*sa_id);

cs_status_t cs_l2tp_tunnel_handle(
	struct sk_buff 					*skb);

int cs_l2tp_ipsec_tunnel_hash_create(
	cs_tunnel_entry_t 				*tunnel_entry);

int cs_l2tp_ipsec_tunnel_hash_del(
	cs_tunnel_entry_t				*tunnel_entry);
cs_status_t cs_ipsec_tunnel_handle(
	struct sk_buff 					*skb);
cs_status_t cs_l2tp_ipsec_tunnel_handle(
	struct sk_buff 					*skb);


#ifdef CS_IPC_ENABLED
cs_status_t
cs_l2tp_ipc_send_set_entry(
		cs_sa_id_direction_t direction,
		cs_tunnel_cfg_t *p_tunnel_cfg,
		cs_uint32_t session_id,
		cs_uint8_t protocol, /*protocol in tunnel ip address*/
		cs_uint16 sa_id);

cs_status_t
cs_l2tp_ipc_send_del_entry(
		cs_sa_id_direction_t direction,
		cs_uint16 sa_id);

cs_status_t
cs_ipsec_ipc_send_set_sadb(
		cs_sa_id_direction_t direction,
		cs_ipsec_policy_node_t * p_policy_node,
		cs_uint16 sa_id);

cs_status_t
cs_ipsec_ipc_send_del_entry(
		cs_sa_id_direction_t direction,
		cs_uint16 sa_id);

cs_status_t
cs_ipsec_ipc_send_sadb_key(
		cs_sa_id_direction_t direction,
		cs_uint8_t	auth_keylen,
		cs_uint8_t	enc_keylen,
		cs_uint8_t * 	akey,
		cs_uint8_t * 	ekey,
		cs_uint16_t sa_id);

cs_status_t
cs_ipsec_ipc_send_sadb_selector(
		cs_sa_id_direction_t direction,
		cs_uint16_t selector_idx,
		cs_ipsec_selector_t * p_selector,
		cs_uint16_t sa_id);

cs_status_t
cs_l2tp_ipc_rcv_set_entry_ack(struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context);

cs_status_t
cs_l2tp_ipc_rcv_del_entry_ack(struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context);

cs_status_t
cs_ipsec_ipc_rcv_set_sadb_ack(struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context);

cs_status_t
cs_ipsec_ipc_rcv_del_sadb_ack(struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context);

cs_status_t
cs_ipsec_ipc_rcv_set_sadb_key_ack(struct ipc_addr peer,
		unsigned short msg_no, const void *msg_data,
		unsigned short msg_size,
		struct ipc_context *context);

#endif /* CS_IPC_ENABLED */
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
#define MAX_ENC_KEY_LEN		32
#define MAX_AUTH_KEY_LEN	32

/* CS IPsec Offload supported cipher algorithm */
#define CS_IPSEC_CIPHER_NULL	0
#define CS_IPSEC_DES		1
#define CS_IPSEC_3DES		1
#define CS_IPSEC_AES		2

#define CS_IPSEC_CIPHER_ECB		0
#define CS_IPSEC_CIPHER_CBC		1
#define CS_IPSEC_CIPHER_CTR		2
#define CS_IPSEC_CIPHER_CCM		3
#define CS_IPSEC_CIPHER_GCM		5
#define CS_IPSEC_CIPHER_OFB		7
#define CS_IPSEC_CIPHER_CFB		8

/* CS IPsec Offload supported authentication algorithm */
#define CS_IPSEC_AUTH_NULL	0
#define CS_IPSEC_MD5		1
#define CS_IPSEC_SHA1		2
#define CS_IPSEC_SHA224		3
#define CS_IPSEC_SHA256		4


#ifndef MIN
#define MIN(a,b) (((a) <= (b)) ? (a):(b))
#endif

#define CS_IPSEC_RETRY_CNT_MAX	3
#define CS_IPSEC_RETRY_INTERVAL	(HZ)
#define CS_IPSEC_MIN_ETHER_SIZE 60

void cs_ipsec_proc_callback(unsigned long notify_event, unsigned long value);
cs_status_t cs_ipsec_insert_sec_path(struct sk_buff *skb,
						cs_ipsec_policy_node_t *p);
int cs_ipsec_hw_accel_add(struct sk_buff *skb);
int cs_ipsec_ctrl_enable(void);
void cs_ipsec_tunnel_chk(cs_tunnel_entry_t *t, cs_uint32 spi);

#endif /* CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL */

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL
#define CS_L2TP_RETRY_CNT_MAX	3
#define CS_L2TP_RETRY_INTERVAL	(HZ)
#define CS_L2TP_MIN_ETHER_SIZE 60

void cs_l2tp_proc_callback(unsigned long notify_event, unsigned long value);
int cs_l2tp_hw_accel_add(struct sk_buff *skb);
void cs_l2tp_free(u32 tid, u32 peer_tid);
int cs_l2tp_ctrl_enable(void);

#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL */

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) &&\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)

int cs_l2tp_ipsec_hw_accel_add(struct sk_buff *skb);

#endif /* CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL &&
	  CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL */

/* utilities */
cs_uint16_t cs_calc_ipv4_checksum(cs_uint32_t saddr, cs_uint32_t daddr,
							cs_uint8_t protocol) ;
void cs_dump_data(void *p, int len);
extern unsigned int calc_crc(u32 crc, u8 const *p, u32 len);
int cs_ip_addr_cmp(cs_ip_address_t *addr1, cs_ip_address_t *addr2);
int cs_ip2mask(cs_ip_address_t *ip, cs_ip_mask_t *ip_mask);

#ifdef CS_IPC_ENABLED
#define cs_tunnel_ipc_send(w,x,y,z) cs_pe_ipc_send(w,x,y,z)
#endif /* CS_IPC_ENABLED */

cs_status_t
cs_sa_id_get_by_tunnel_id(
	CS_IN	cs_tunnel_id_t		tunnel_id,
	CS_IN	cs_tunnel_type_t	tunnel_type,
	CS_IN	cs_tunnel_dir_t		dir,
	CS_OUT	cs_uint16_t		*sa_id);

cs_status_t
cs_tunnel_tx_frame_to_pe(
	CS_IN cs_uint8_t * dptr, /* pointer of packet payload (from L2) */
	CS_IN cs_uint32_t pkt_len, /* pkt_len of packet payload (from L2) */
	CS_IN cs_tunnel_id_t tunnel_id,
	CS_IN cs_tunnel_type_t tunnel_type,
	CS_IN cs_tunnel_dir_t dir);

/* global */
int cs_hw_accel_tunnel_handle(int voq, struct sk_buff *skb);
int cs_hw_accel_tunnel_ctrl_handle(int voq, struct sk_buff *skb);
int __cs_hw_accel_tunnel_handle(struct sk_buff *skb);
int __cs_hw_accel_tunnel_ctrl_handle(int voq, struct sk_buff *skb);

/* module API */
void cs_hw_accel_tunnel_init(void);
void cs_hw_accel_tunnel_exit(void);

cs_status_t cs_tunnel_wan2pe_rule_hash_add(
        cs_tunnel_cfg_t *p_tunnel_cfg,
        cs_uint8_t      is_natt,
        cs_l3_ip_addr   *p_da_ip,
        cs_l3_ip_addr   *p_sa_ip,
        cs_uint8_t      ip_version,
        cs_uint16_t     pppoe_session_id,
        cs_uint16_t     sa_id,
        cs_uint32_t     spi_idx,
	cs_uint16_t     natt_ingress_src_port,
        cs_rule_hash_t  *p_rule_hash);

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
	cs_uint16_t	natt_egress_dest_port,
        cs_rule_hash_t  *p_rule_hash,
        cs_uint16_t     *p_hash_index);

#endif /* __CS_HW_ACCEL_TUNNEL_H__ */
