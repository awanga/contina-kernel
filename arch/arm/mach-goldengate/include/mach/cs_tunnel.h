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
#ifndef __CS_TUNNEL_H__
#define __CS_TUNNEL_H__

#include "cs_types.h"
#include "cs_network_types.h"


typedef enum {
	CS_TUNNEL_DIR_OUTBOUND 		= 0,
	CS_TUNNEL_DIR_INBOUND		= 1,
	CS_TUNNEL_DIR_TWO_WAY		= 2,
} cs_tunnel_dir_t;

typedef union cs_tunnel_union_overlay {
	cs_uint32_t			enc_policy;		/* Policy handle for L2TP encryptionn - required by cs_ipsec_policy_add */
	cs_uint16_t			pptp_sa_id;		/* pptp sa_id */
} cs_tunnel_union_overlay_t;

typedef struct cs_l2tp_tunnel_cfg_s {
	cs_uint16_t			ver;			/* Version and flags */
	cs_uint16_t 			len;			/* Optional Length */ /* need caculate */
	cs_uint16_t 			tid;			/* Tunnel ID */
	cs_uint16_t			ipv4_id;		/* ID of IPv4 hdr */ /* increasement */ /* obsolete */
	cs_uint16_t			dest_port;		/* UDP port of L2TP hdr */
	cs_uint16_t			src_port;		/* UDP port of L2TP hdr */

	cs_uint32_t			ipsec_policy;		/* 0: unused, otherwise: encrypted */

	/* For L2TPv3 */
	cs_uint32_t			session_id;		/* Session ID */
	cs_uint16_t			encap_type;		/* 1: ip, 0: udp */
	cs_uint16_t			l2specific_len;		/* default is 4 */
	cs_uint16_t			l2specific_type;	/* 0: NONE, 1: DEFAULT */
	cs_uint8_t			send_seq;		/* 0: Not send sequence */
	cs_uint8_t			calc_udp_csum;          /* For IPv6 outer IP, calc UDP checksum of L2TP header. 0: UDP checksum is zero 1: calculate UDP checksum */
	cs_uint32_t			ns;			/* If send_seq, set the start of Session NR state */

	cs_uint16_t			cookie_len;
	cs_uint8_t			cookie[8];
	cs_uint16_t			offset;			/* offset from the end of L2TP header to beginning of data*/
	cs_uint8_t			l2tp_src_mac[6];
	cs_uint8_t			peer_l2tp_src_mac[6];
} cs_l2tp_tunnel_cfg_t;

typedef struct cs_gre_tunnel_cfg_s {
	cs_tunnel_id_t		tunnel_id;		/* returned by cs_tunnel_add */

	cs_uint16_t		up_call_id;		/* upstream call ID */
	cs_uint16_t		down_call_id;		/* upstream call ID */

	cs_tunnel_union_overlay_t	overlay_tunnel_ingress;
	cs_tunnel_union_overlay_t	overlay_tunnel_egress;
} cs_gre_tunnel_cfg_t;

typedef struct cs_ipsec_tunnel_cfg_s {
	cs_uint32_t			ipsec_policy;	/* 0: unused, otherwise: encrypted */
} cs_ipsec_tunnel_cfg_t;

typedef union cs_tunnel_cfg_union_s {
	cs_l2tp_tunnel_cfg_t	l2tp;
	cs_gre_tunnel_cfg_t	gre;
	cs_ipsec_tunnel_cfg_t	ipsec;
} cs_tunnel_cfg_union_t;

typedef struct cs_tunnel_cfg_s {
	cs_tunnel_type_t		type;			/* tunnel type */
	cs_ip_address_t			dest_addr;		/* tunnel destination IP */
	cs_ip_address_t			src_addr;		/* tunnel src IP */
	cs_port_id_t			tx_port;		/* out bound port -
								   can be physical port - such as WAN port
								   or logical ports such as PPPoE ports created by data
								   plane and returned back to the caller
								 */
	cs_uint32_t			nexthop_id;
	cs_tunnel_dir_t			dir;
	cs_tunnel_cfg_union_t		tunnel;
} cs_tunnel_cfg_t;

/*************************************************************************/

typedef struct cs_pppoe_hdr_s {
	cs_uint8_t 	    type:4; /* Type = 0x1 */
	cs_uint8_t 	    ver:4;  /* Version = 0x1 */
	cs_uint8_t 	    code;  	/* Code = 0x00 */
	cs_uint16_t     sid;  	/* Session ID */
	cs_uint16_t	    len;	/* Size of data in bytes */
}  __attribute__((packed)) cs_pppoe_hdr_t;


typedef struct cs_ppp_hdr_s {
	cs_uint8_t 	    addr;  /* Address = 0xff */
	cs_uint8_t 	    ctrl;  /* Control = 0x03 */
	cs_uint16_t 	    pro;  /* Protocol = 0x0057 */
}  __attribute__((packed)) cs_ppp_hdr_t;

typedef struct cs_l2tp_ver_s {
	cs_uint16_t version : 4; /* bits 3:0 */
	cs_uint16_t rsvd1   : 4; /* bits 7:4 */
	cs_uint16_t p       : 1; /* bit 8 */
	cs_uint16_t o       : 1; /* bit 9 */
	cs_uint16_t rsvd2   : 1; /* bit 10 */
	cs_uint16_t s       : 1; /* bit 11 */
	cs_uint16_t rsvd3   : 2; /* bits 13:12 */
	cs_uint16_t l       : 1; /* bit 14 */
	cs_uint16_t t       : 1; /* bit 15 */
} cs_l2tp_ver_t_bits;

typedef union {
	cs_uint16_t  u16;
	cs_l2tp_ver_t_bits bits;
}  __attribute__((packed)) cs_l2tp_ver_t;

typedef struct cs_l2tp_hdr1_s {
	cs_uint16_t	ver;  /* Version and flags */
	cs_uint16_t 	tid;  /* Tunnel ID */
	cs_uint16_t 	sid;  /* Session ID */
}  __attribute__((packed)) cs_l2tp_hdr1_t;

typedef struct cs_l2tp_hdr2_s {
	cs_uint16_t	ver;  /* Version and flags */
	cs_uint16_t 	len;  /* Optional length */
	cs_uint16_t 	tid;  /* Tunnel ID */
	cs_uint16_t 	sid;  /* Session ID */
}  __attribute__((packed)) cs_l2tp_hdr2_t;

typedef struct cs_l2tp_hdr3_s {
	cs_uint16_t	ver;  /* Version and flags */
	cs_uint16_t 	tid;  /* Tunnel ID */
	cs_uint16_t 	sid;  /* Session ID */
	cs_uint16_t 	offset;  /* Optional offset */
}  __attribute__((packed)) cs_l2tp_hdr3_t;

typedef struct cs_l2tp_hdr4_s {
	cs_uint16_t	ver;  /* Version and flags */
	cs_uint16_t 	len;  /* Optional length */
	cs_uint16_t 	tid;  /* Tunnel ID */
	cs_uint16_t 	sid;  /* Session ID */
	cs_uint16_t 	offset;  /* Optional offset */
}  __attribute__((packed)) cs_l2tp_hdr4_t;

typedef cs_l2tp_hdr2_t cs_l2tp_hdr_t;

typedef struct cs_pppoe_port_cfg_s {
	cs_uint8_t		src_mac[6];
	cs_uint8_t		dest_mac[6];
	cs_uint16_t		pppoe_session_id;
	cs_uint32_t		vlan_tag;		/* 0: untagged, 1~4094: normal tag */
	cs_port_id_t		tx_phy_port;		/* Port number corresponding to
							   WAN Port on which PPPoE
							   session is established */
} cs_pppoe_port_cfg_t;

/****************************************************************************/

#define CS_IPSEC_SEL_MAX     		4
#define CS_IPSEC_MAX_ENC_KEY_LEN	32
#define CS_IPSEC_MAX_AUTH_KEY_LEN	32


typedef enum {
	CS_IPSEC_SPD_OUTBOUND = 0,
	CS_IPSEC_SPD_ENCRYPT = CS_IPSEC_SPD_OUTBOUND,
	CS_IPSEC_SPD_INBOUND = 1,
	CS_IPSEC_SPD_DECRYPT = CS_IPSEC_SPD_INBOUND,
} cs_ipsec_spd_direction_t;

typedef enum {
	CS_IPSEC_POLICY_ACTION_IPSEC 	= 1, /* Apply IPSec */
	CS_IPSEC_POLICY_ACTION_DISCARD 	= 2, /* Discard / Drop */
	CS_IPSEC_POLICY_ACTION_BYPASS 	= 3, /* Allow to pass */
} cs_ipsec_policy_action_t;

typedef enum cs_ipsec_ciph_alg {
	CS_IPSEC_CIPH_NULL = 0,
	CS_IPSEC_CIPH_DES = 1,
	CS_IPSEC_CIPH_AES = 2,
} cs_ipsec_ciph_alg_t;

typedef enum cs_ipsec_ciph_mode {
	CS_IPSEC_CIPH_MODE_ECB = 0,
	CS_IPSEC_CIPH_MODE_CBC = 1,
	CS_IPSEC_CIPH_MODE_CTR = 2,
	CS_IPSEC_CIPH_MODE_CCM = 3,
	CS_IPSEC_CIPH_MODE_GCM = 5,
	CS_IPSEC_CIPH_MODE_OFB = 7,
	CS_IPSEC_CIPH_MODE_CFB = 8,
} cs_ipsec_ciph_mode_t;

typedef enum cs_ipsec_hash_alg {
	CS_IPSEC_HASH_NULL 	= 0,
	CS_IPSEC_HASH_MD5 	= 1,
	CS_IPSEC_HASH_SHA1 	= 2,
} cs_ipsec_hash_alg_t;

typedef struct 
{ 
	cs_uint8_t 			protocol;  /* IP Transp. Protocol or OPAQUE */ 
	cs_ip_address_t 		src_ip;    /* Src Address - can left as zero  */ 
	cs_ip_address_t 		dst_ip;    /* Dst Address */ 
	cs_uint16_t			src_port;  /* TCP/UDP Src Port-not in first phase */ 
	cs_uint16_t			dst_port;  /* TCP/UDP Dst Port-not in first phase */ 
} cs_ipsec_selector_t; /* not all fields of a selector may be used */

typedef struct
{
	cs_uint32_t 			policy_id; 
	cs_ipsec_policy_action_t 	policy_action; 
	cs_uint32_t 			selector_count; 
	cs_ipsec_selector_t 		selector_array[CS_IPSEC_SEL_MAX]; 
} cs_ipsec_policy_t;

typedef struct cs_ipsec_sa
{
	cs_uint16_t	replay_window;
	cs_uint32_t	spi;
	cs_uint32_t	seq_num;
	cs_uint8_t	ekey[CS_IPSEC_MAX_ENC_KEY_LEN];
	cs_uint8_t	akey[CS_IPSEC_MAX_AUTH_KEY_LEN];
	cs_uint32_t	ip_ver:1;   /* 0=IPv4, 1=IPv6 */
	cs_uint32_t	proto:1;    /* 0=ESP; 1=AH */
	cs_uint32_t	tunnel:1;   /* 0=Tunnel; 1=Transport */
	cs_uint32_t	sa_dir:1;   /* 0=outbound; 1=inbound */

	/* encryption mode based on elliptic */
	cs_uint32_t	ealg:2; /* cs_ipsec_ciph_alg_t */
	cs_uint32_t	ealg_mode:3; /* cs_ipsec_ciph_mode_t */
	cs_uint32_t	enc_keylen:4;   /* (n * 4) bytes */
	cs_uint32_t	iv_len:4;   /* (n * 4) bytes for CBC */
	/* authentication mode based on elliptic */
	cs_uint32_t	aalg:3; /* cs_ipsec_hash_alg_t */
	cs_uint32_t	auth_keylen:4;
	cs_uint32_t	icv_trunclen:4; /* (n * 4) bytes */
	cs_uint32_t	etherIP:1;
	cs_uint32_t	copy_ip_id:1;
	cs_uint32_t	copy_tos:1;
	cs_uint32_t	reserved:1;
	/* If tunnel mode the outer IP header template is sent for reconstructing */
	cs_l3_ip_addr	tunnel_saddr;
	cs_l3_ip_addr	tunnel_daddr;
	cs_uint32_t	lifetime_bytes;
	cs_uint32_t	bytes_count;
	cs_uint32_t	lifetime_packets;
	cs_uint32_t	packets_count;

	cs_uint8_t	is_natt; /* NAT-Traversal */
	cs_uint16_t	natt_ingress_src_port; /* src port of ingress NAT-T tunnel flow */
	cs_uint16_t	natt_egress_dest_port; /* dest port of egress NAT-T tunnel flow */
} cs_ipsec_sa_t;

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
cs_status_t cs_ipsec_init (
	CS_IN 	cs_dev_id_t				device_id);

cs_status_t cs_ipsec_spd_add (
	CS_IN 	cs_dev_id_t				device_id,
	CS_IN 	cs_ipsec_spd_direction_t		direction,
	CS_IN 	cs_uint32_t				spd_id,
	CS_OUT 	cs_uint32_t				*spd_handle);


cs_status_t cs_ipsec_spd_delete (
	CS_IN 	cs_dev_id_t				device_id,
	CS_IN 	cs_uint32_t				spd_handle);



cs_status_t cs_ipsec_policy_add (
	CS_IN 	cs_dev_id_t				device_id,
	CS_IN 	cs_uint32_t				spd_handle,
	CS_IN 	cs_ipsec_policy_t			*policy,
	CS_OUT 	cs_uint32_t				*policy_handle);

cs_status_t cs_ipsec_policy_delete(
	CS_IN 	cs_dev_id_t				device_id,
	CS_IN 	cs_uint32_t				spd_handle,
	CS_IN 	cs_uint32_t				policy_handle);


cs_status_t cs_ipsec_sa_add (
	CS_IN 	cs_dev_id_t				device_id,
	CS_IN   cs_uint32_t				spd_handle,
	CS_IN 	cs_uint32_t				policy_handle,
	CS_IN 	cs_uint8_t				sa_count,
	CS_IN 	cs_ipsec_sa_t				*sa_array,
	CS_IN	cs_uint32_t				previous_sa_handle,
	CS_OUT 	cs_uint32_t				*sa_handle);


cs_status_t cs_ipsec_sa_delete (
	CS_IN 	cs_dev_id_t				device_id,
	CS_IN   cs_uint32_t				spd_handle,
	CS_IN 	cs_uint32_t				policy_handle,
	CS_IN 	cs_uint32_t				sa_handle);
#endif

/*************************************************************************/

/* exported APIs */
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
cs_status_t
cs_pppoe_port_add(
		CS_IN cs_dev_id_t	device_id,
 		CS_IN cs_port_id_t	port_id,
		CS_IN cs_port_id_t	pppoe_port_id
		);

cs_status_t
cs_pppoe_port_delete(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_port_id_t	pppoe_port_id
		);

cs_status_t
cs_pppoe_port_encap_set(
		CS_IN cs_dev_id_t		device_id,
		CS_IN cs_port_id_t		pppoe_port_id,
		CS_IN cs_pppoe_port_cfg_t	*p_cfg
		);

cs_status_t
cs_pppoe_port_config_get(
		CS_IN  cs_dev_id_t		device_id,
		CS_IN  cs_port_id_t		pppoe_port_id,
		CS_OUT cs_pppoe_port_cfg_t	*p_cfg
		);
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

cs_status_t 
cs_tunnel_add(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_cfg_t	*p_tunnel_cfg,
		CS_OUT cs_tunnel_id_t	*p_tunnel_id
		);

cs_status_t 
cs_tunnel_delete(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_cfg_t	*p_tunnel_cfg
		);

cs_status_t 
cs_tunnel_delete_by_idx(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_id_t	tunnel_id
		);

cs_status_t 
cs_tunnel_get(
		CS_IN  cs_dev_id_t	device_id,
		CS_IN  cs_tunnel_id_t	tunnel_id,
		CS_OUT cs_tunnel_cfg_t	*p_tunnel_cfg
		);

cs_status_t 
cs_l2tp_session_add(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_id_t	tunnel_id,
		CS_IN cs_session_id_t	session_id
		);

cs_status_t 
cs_l2tp_session_delete(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_id_t	tunnel_id,
		CS_IN cs_session_id_t	session_id
		);

cs_status_t 
cs_l2tp_session_get(
		CS_IN  cs_dev_id_t	device_id,
		CS_IN  cs_tunnel_id_t	tunnel_id,
		CS_IN  cs_session_id_t	session_id,
		CS_OUT cs_boolean_t	*is_present
		);

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
cs_status_t 
cs_ipv6_over_l2tp_add(
		CS_IN cs_dev_id_t	device_id,
		CS_IN cs_tunnel_id_t	tunnel_id,
		CS_IN cs_session_id_t	session_id,
		CS_IN cs_ip_address_t	*ipv6_prefix
		);

cs_status_t 
cs_ipv6_over_l2tp_delete(
		CS_IN cs_dev_id_t	device_id,
   		CS_IN cs_tunnel_id_t	tunnel_id,
		CS_IN cs_session_id_t	session_id,
		CS_IN cs_ip_address_t	*ipv6_prefix
		);

cs_status_t 
cs_ipv6_over_l2tp_getnext(
		CS_IN  cs_dev_id_t	device_id,
   		CS_IN  cs_tunnel_id_t	tunnel_id,
		CS_IN  cs_session_id_t	session_id,
		CS_OUT cs_ip_address_t	*ipv6_prefix
		);
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */
/*************************************************************************/

typedef enum {
	CS_CRYPTO_INVALID = -1,
	CS_CRYPTO_NONE = 0,
	CS_CRYPTO_MPPE40,
	/* CS_CRYPTO_MPPE56, */	// crypto engine does not support
	CS_CRYPTO_MPPE128,
} cs_pptp_crypto_type_t;

typedef enum {
	CS_PPTP_STATELESS = 0,
	CS_PPTP_STATEFUL,
} cs_pptp_state_type_t;

typedef enum {
	CS_PPTP_DIR_UPSTREAM = 0,
	CS_PPTP_DIR_DOWNSTREAM,
} cs_pptp_direction_t;

typedef struct cs_pptp_sa_s {
	cs_uint16_t		sa_id;			/* PPTP Security Association ID */

	cs_pptp_direction_t	dir;			/* direction */
	cs_uint16_t		call_id;		/* local call ID */

	cs_ip_address_t		local_addr;		/* local PPP address */
	cs_ip_address_t		remote_addr;		/* remote PPP address */

	cs_pptp_state_type_t	state;			/* MPPE stateless / stateful mode */
	cs_pptp_crypto_type_t	crypto_type;

	cs_uint8_t		key[16];		/* Start Key */
	cs_uint8_t		keylen;

	cs_uint32_t		seq_num;		/* sequence number */
} cs_pptp_sa_t;

/* exported PPTP API */
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
cs_status_t cs_pptp_session_add(
	CS_IN cs_dev_id_t		dev_id,
	CS_IN_OUT cs_pptp_sa_t		*session);

cs_status_t cs_pptp_session_delete(
	CS_IN cs_dev_id_t		dev_id,
	CS_IN cs_uint16_t		sa_id,
	CS_IN cs_uint8_t		direction);

cs_status_t cs_pptp_session_get(
	CS_IN cs_dev_id_t		dev_id,
	CS_IN_OUT cs_pptp_sa_t		*session);

cs_status_t cs_pptp_session_clear_all(
	CS_IN cs_dev_id_t		dev_id);

cs_status_t cs_pptp_key_change(
	CS_IN cs_dev_id_t		dev_id,
	CS_IN cs_uint16_t		sa_id);
#endif

/*************************************************************************/

/* MTU */
typedef struct cs_tunnel_mtu_s {
	cs_tunnel_type_t	tunnel_type;
	cs_tunnel_id_t		tunnel_id;
	cs_uint16_t		mtu;		/* not include CRC */
} cs_tunnel_mtu_t;

cs_status_t cs_tunnel_mtu_set(
	CS_IN		cs_dev_id_t			device_id,
	CS_IN		cs_tunnel_mtu_t			*p_mtu);

cs_status_t cs_tunnel_mtu_get(
	CS_IN		cs_dev_id_t			device_id,
	CS_IN_OUT	cs_tunnel_mtu_t			*p_mtu);

/*************************************************************************/

#endif /* __CS_TUNNEL_H__ */
