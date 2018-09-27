/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
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

#ifndef __CS_MCAST_H__
#define __CS_MCAST_H__

#include "cs_types.h"
#include "cs_network_types.h"

#define CS_DEFAULT_WAN_PORT_ID      0xFF

typedef enum {
	CS_MCAST_PORT_REPLICATION = 1,
	CS_MCAST_ARBITRARY_REPLICATION = 2,
} cs_mcast_mode;

typedef enum {
	CS_VLAN_TRANSPARENT,
	CS_VLAN_POP,
	CS_VLAN_SWAP,
} cs_vlan_cmd_t;

typedef enum {
	CS_MCAST_EXCLUDE = 0,
	CS_MCAST_INCLUDE,
} cs_mcast_filter_mode;


typedef struct cs_mcast_vlan_cfg {
	/* On LynxE named as mcast_vlan_id.
	 * vlan tag received from the network on the PON port.
	 */
	cs_uint16_t		in_vlan_id;

	cs_vlan_cmd_t		cs_vlan_cmd;

	/* On LynxE named as uni_vlan_id.
	 * fixed vlan_id used for the SWAP command when sending a
	 * packet to the UNI port.
	 */
	cs_uint16_t		out_vlan_id;
} cs_mcast_vlan_cfg_t;

typedef struct cs_mcast_entry_s {
	cs_port_id_t		sub_port;
	cs_uint16_t		mcast_vlan;
	cs_uint8_t		mac_addr[6];
} cs_mcast_entry_t;

#define CS_MCAST_MAX_ADDRESS	32
typedef struct cs_mcast_member_s {
	cs_ip_afi_t		afi;
	cs_uint16_t		sub_port;
	cs_uint16_t		mcast_vlan;

	/* if IPv4, only first four bytes are used */
	cs_ip_address_t		grp_addr;

	/* if CS_MCAST_EXCLUDE and src_num = 0, equal to IGMPv2 entry.
	 * if CS_MCAST_INCLUDE and src_num = 0, no entry will be set to HW.
	 */
	cs_mcast_filter_mode	mode;

	/* Unicast MAC address of joining host. Used by HW in arbitrary replication. */
	cs_uint8_t		dest_mac[6];

	cs_uint16_t		src_num;
	cs_ip_address_t		src_list[CS_MCAST_MAX_ADDRESS];
} cs_mcast_member_t;

#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST

/* Add IP multicast forward entry to HW, it can support IGMPv3/MLDv2.
 * To add multiple ports the function must be called multiple times.
 */
cs_status_t cs_l2_mcast_member_add(CS_IN cs_dev_id_t dev_id,
				    CS_IN cs_port_id_t port_id,
				    CS_IN cs_mcast_member_t *entry);

/* Delete one IP multicast entry on the port.
 * To delete multiple ports the function must be called multiple times.
 */
cs_status_t cs_l2_mcast_member_delete(CS_IN cs_dev_id_t dev_id,
				       CS_IN cs_port_id_t port_id,
				       CS_IN cs_mcast_member_t *entry);

/* Retrieve the number of multicast entries on one port. */
cs_status cs_l2_mcast_port_member_num_get(CS_IN cs_dev_id_t dev_id,
					   CS_IN cs_port_id_t port_id,
					   CS_OUT cs_uint16_t *num);

/* Retrieve at most 'num' amount of multicast entries on one port. */
cs_status cs_l2_mcast_port_member_entry_get(CS_IN cs_dev_id_t dev_id,
					     CS_IN cs_port_id_t port_id,
					     CS_IN_OUT cs_uint16_t *num,
					     CS_OUT cs_mcast_member_t *entry);

/* Delete all multicast entries on one port. */
cs_status_t cs_l2_mcast_port_member_clear(CS_IN cs_dev_id_t dev_id,
					   CS_IN cs_port_id_t port_id);

/* Clear all the HW accelerated multicast flows. */
cs_status_t cs_l2_mcast_member_all_clear(CS_IN cs_dev_id_t dev_id);

/* Set CS_MCAST_PORT_REPLICATION or CS_MCAST_ARBITRARY_REPLICATION.
 * Default is CS_MCAST_PORT_REPLICATION.
 *
 * When mode changes, the application must explicitly delete all the HW
 * accelerated multicast flows from the previous mode.
 */
cs_status_t cs_l2_mcast_mode_set(CS_IN cs_dev_id_t dev_id,
				 CS_IN cs_mcast_mode mode);

cs_status_t cs_l2_mcast_wan_port_id_set(CS_IN cs_dev_id_t dev_id,
					CS_IN cs_port_id_t port_id);

cs_status_t cs_l2_mcast_wan_port_id_get(CS_IN cs_dev_id_t dev_id,
					CS_OUT cs_port_id_t *port_id);

#else /* CONFIG_CS75XX_DATA_PLANE_MULTICAST */

#define cs_l2_mcast_member_add(d, p, e)		({ CS_OK; })
#define cs_l2_mcast_member_delete(d, p, e)		({ CS_OK; })
#define cs_l2_mcast_port_member_num_get(d, p, n)	({ CS_OK; })
#define cs_l2_mcast_port_member_entry_get(d, p, n, e)	({ CS_OK; })
#define cs_l2_mcast_port_member_clear(d, p)		({ CS_OK; })
#define cs_l2_mcast_member_all_clear(d)		({ CS_OK; })
#define cs_l2_mcast_mode_set(d, m)			({ CS_OK; })
#define cs_l2_mcast_wan_port_id_set(d, p)		({ CS_OK; })
#define cs_l2_mcast_wan_port_id_get(d, p)		({ CS_OK; })

#endif /* CONFIG_CS75XX_DATA_PLANE_MULTICAST */

#endif /* __CS_MCAST_H__ */

