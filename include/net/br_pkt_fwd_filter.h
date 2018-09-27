/*
 *	Packet forwarding filter for ethernet bridge
 *
 * This file contains unpublished documentation and software           
 * proprietary to Cortina Systems Incorporated. Any use or disclosure, 
 * in whole or in part, of the information in this file without a      
 * written consent of an officer of Cortina Systems Incorporated is    
 * strictly prohibited.                                                
 * Copyright (c) 2010 by Cortina Systems Incorporated.                 
 */

#ifndef _BR_PACKET_FORWARD_FILTER_H
#define _BR_PACKET_FORWARD_FILTER_H

#define BR_PKT_FWD_FT_DRIVER_NAME	"br_pkt_fwd_filter"
#define BR_PKT_FWD_FT_VLAN_ARRAY_LEN	(4096/8)
#define BR_PKT_FWD_FT_NAME_LEN		16

/* IOCTL command of packet forwarding filter */
typedef enum
{
	PKT_FWD_FT_RESET = 1,
	PKT_FWD_FT_RAW_SET,
	PKT_FWD_FT_RAW_GET,
	PKT_FWD_FT_ETH_GRP_SET,
	PKT_FWD_FT_ETH_GRP_GET,
	PKT_FWD_FT_ETH_VLAN_SET,
	PKT_FWD_FT_ETH_VLAN_GET,
	PKT_FWD_FT_MAX
} BR_FWD_PKT_FT_CMD_DEF_T;

struct br_fwd_fwd_ft_vlan {
	unsigned short vid;
	unsigned char mode; /* 1: forward, 0: drop */
};

/* structure of parameters for IOCTL command */
typedef struct {
	unsigned short cmd;         /* refer to BR_FWD_PKT_FT_CMD_DEF_T */

	char netdev_name[BR_PKT_FWD_FT_NAME_LEN];

	unsigned char fwd_grp;

	union {
		unsigned char vid_map[BR_PKT_FWD_FT_VLAN_ARRAY_LEN];
		struct br_fwd_fwd_ft_vlan vlan_mode;
	};
	
} BR_FWD_PKT_FT_CMD_T;


#endif
