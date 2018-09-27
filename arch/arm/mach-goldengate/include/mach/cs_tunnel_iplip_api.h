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
#ifndef __CS_TUNNEL_IPLIP_API_H__
#define __CS_TUNNEL_IPLIP_API_H__

#include "cs_types.h"
#include "cs_tunnel.h"

typedef struct {
        cs_uint32   sub_cmd; /* refer to cs_tunnel_iplip_ioctl_sub_cmd_e */
        /* parameters for commands */
        union {
                /* cs_pppoe_port_add (cs_dev_id_t device_id, cs_port_id_t port_id, cs_port_id_t pppoe_port_id) */
                /* cs_pppoe_port_delete (cs_dev_id_t device_id, cs_port_id_t pppoe_port_id) */
                struct cs_tunnel_iplip_pppoe_port_add_delete_param {
                        cs_dev_id_t  device_id;
                        cs_port_id_t port_id;
                        cs_port_id_t pppoe_port_id;
                } pppoe_port_add_delete_param;

                /*cs_pppoe_port_encap_set (cs_dev_id_t device_id, cs_port_id_t pppoe_port_id, cs_pppoe_port_cfg_t *p_cfg)
                  cs_pppoe_port_encap_get (cs_dev_id_t device_id, cs_port_id_t pppoe_port_id, cs_pppoe_port_cfg_t *p_cfg)*/
                struct cs_tunnel_iplip_pppoe_port_encap_set_get_param {
                        cs_dev_id_t         device_id;
                        cs_port_id_t        pppoe_port_id;
                        cs_pppoe_port_cfg_t pppoe_port_cfg;
                } pppoe_port_encap_param;

                /*cs_tunnel_add(cs_dev_id_t device_id, cs_tunnel_cfg_t *p_tunnel_cfg, cs_tunnel_id_t *p_tunnel_id)
                  cs_tunnel_delete(cs_dev_id_t device_id, cs_tunnel_cfg_t *p_tunnel_cfg)
                  cs_tunnel_delete_by_idx (cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id)
                  cs_tunnel_get(cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, cs_tunnel_cfg_t *p_tunnel_cfg) */
                 struct cs_tunnel_iplip_tunnel_param {
                        cs_dev_id_t     device_id;
                        cs_tunnel_id_t tunnel_id;
                        cs_tunnel_cfg_t tunnel_cfg;
                } tunnel_param;

                /*cs_l2tp_session_add(cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, uint16 session_id)
                  cs_l2tp_session_delete(cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, uint16 session_id)
                  cs_l2tp_session_get (cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, uint16 session_id, u8 *is_present)*/
                struct cs_l2tp_session_param {
                        cs_dev_id_t     device_id;
                        cs_tunnel_id_t tunnel_id;
                        cs_session_id_t session_id;
                        cs_uint8        is_present;
                } l2tp_session_param;

                /*cs_ipv6_over_l2tp_add(cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, cs_session_id_t session_id, cs_ip_address *ipv6_prefix)
                  cs_ipv6_over_l2tp_delete (cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, cs_session_id_t session_id, cs_ip_address *ipv6_prefix)
                  cs_ipv6_over_l2tp_getnext (cs_dev_id_t device_id, cs_tunnel_id_t tunnel_id, cs_session_id_t session_id, cs_ip_address *ipv6_prefix)*/
                struct cs_ipv6_over_l2tp_param {
                        cs_dev_id_t     device_id;
                        cs_tunnel_id_t tunnel_id;
                        cs_session_id_t session_id;
                        cs_ip_address_t ipv6_prefix;
                } ipv6_over_l2tp_param;
        };

        /* return value from TUNNEL IPLIP APIs */
        cs_int32 ret;
} cs_tunnel_iplip_api_entry_t;

#endif /* __CS_TUNNEL_IPLIP_API_H__ */
