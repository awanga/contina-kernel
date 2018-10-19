/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2002 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_network_types.h
 *
 * Include file containing some network data types and defines
 * used by kernel, drivers, modules and applications.
 *
 * $Id$
*/
#ifndef __CS_NETWORK_TYPES_H__
#define __CS_NETWORK_TYPES_H__

#include "cs_types.h"

typedef enum {
	CS_IPV4 = 0,
	CS_IPV6 = 1
} cs_ip_afi_t; /* (to be defined newly if doesn't exist) */

typedef enum {
	CS_PORT_GMAC0 		= 0,
	CS_PORT_GMAC1	 	= 1,
	CS_PORT_GMAC2		= 2,
	CS_PORT_OFLD0		= 3, /* PE#0 */
	CS_PORT_OFLD1		= 4, /* PE#1 */
	CS_PORT_CPU		= 5, /* indicate port id of CPU port */
	CS_PORT_CUSTOM0		= 6, /* customer defined interface#0 */
	CS_PORT_CUSTOM1		= 7, /* customer defined interface#1 */
	CS_PORT_CUSTOM2		= 8, /* customer defined interface#2 */
} cs_phy_port_id_t;

typedef enum {
	CS_IPSEC_FLOW_BASED	= 0, /* for previous IPSEC design */
	CS_L2TP			= 1, /* L2TPv2 server (plaintext) */
	CS_GRE			= 2, /* pure GRE tunnel */
	CS_GRE_IPSEC		= 3,
	CS_IPSEC		= 4,
	CS_L2TP_IPSEC		= 5, /* L2TPv2/IPSEC server */
	CS_PPTP			= 6, /* PPTP tunnel */
	CS_RTP			= 7,
	CS_IP_TRANSLATE	= 8,
	CS_L2TPV3		= 9, /* L2TPv3 (plaintext) */
	CS_L2TPV3_IPSEC		= 10, /* L2TPv3 over ipsec */
	CS_TUN_TYPE_MAX
} cs_tunnel_type_t;


typedef enum {
	CS_L2TP_TYPE_1 = 1,	/* L=0, O=0 */
	CS_L2TP_TYPE_2 = 2,	/* L=1, O=0 */
	CS_L2TP_TYPE_3 = 3,	/* L=0, O=1, Offset=0 */
	CS_L2TP_TYPE_4 = 4	/* L=1, O=1, Offset=0 */
} cs_l2tp_type_t;


typedef union {
	cs_uint32_t	addr[4];
	cs_uint32_t	ipv4_addr;
	cs_uint32_t	ipv6_addr[4];
} __attribute__((packed)) cs_l3_ip_addr;

typedef struct cs_ip_address_s {
	cs_ip_afi_t 	afi; /* address family identifier */
	cs_l3_ip_addr	ip_addr;
	cs_uint8_t	addr_len; /* length in bits */
} __attribute__((packed)) cs_ip_address_t;


#endif /* __CS_NETWORK_TYPES_H__ */

