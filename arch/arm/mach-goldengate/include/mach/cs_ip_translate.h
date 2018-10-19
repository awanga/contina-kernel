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
 * cs_rtp_proxy.h
 *
 *
 * This header file defines the data structures and APIs for CS RTP PROXY
 * Acceleration.
 */
#ifndef __CS_IP_TRANSLATE_H__
#define __CS_IP_TRANSLATE_H__

#include "cs_types.h"
#include "cs_network_types.h"
#include "cs_flow_api.h"
#include "cs_tunnel.h"


typedef enum {
	CS_IP_TRANSLATE_TYPE_MAPE	= 0,
	CS_IP_TRANSLATE_TYPE_MAX
} cs_ip_translate_type_t;

typedef struct cs_ip_translate_cfg_s {
	cs_ip_translate_type_t translate_type;

	/* L3/L4 layer for tunnel ip header*/
	cs_pkt_info_t	v6_pkt;

	cs_uint8_t		copy_tos:1;
	cs_uint8_t		default_tos;

	/*port set information*/
	cs_uint16_t port_set_id;
	cs_uint16_t port_set_id_length;
	cs_uint16_t port_set_id_offset;
} cs_ip_translate_cfg_t;



/* Exported APIs
 *	cs_ip_translate_add()
 *	cs_ip_translate_delete()
 *	cs_ip_statistics_get()
 */
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
cs_status_t cs_ip_translate_add(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_cfg_t	*p_ip_translate_cfg,
CS_OUT	cs_ip_translate_id_t	*p_ip_translate_id);

cs_status_t cs_ip_translate_get(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_id_t	ip_translate_id,
CS_OUT	cs_ip_translate_cfg_t	*p_ip_translate_cfg,
CS_OUT	cs_flow_t				*p_flow_entry);

cs_status_t cs_ip_translate_del(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_id_t	ip_translate_id);

cs_status_t cs_ip_translate_flow_add(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_id_t	 ip_translate_id,
CS_IN_OUT	cs_flow_t				*p_flow_entry);

cs_status_t cs_ip_translate_flow_del(
CS_IN	cs_dev_id_t				device_id,
CS_IN	cs_ip_translate_id_t	ip_translate_id,
CS_IN_OUT	cs_flow_t			*p_flow_entry);


#endif /* CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE */

#endif /* __CS_RTP_PROXY_H__ */
