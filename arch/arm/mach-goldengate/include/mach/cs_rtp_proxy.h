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
#ifndef __CS_RTP_PROXY_H__
#define __CS_RTP_PROXY_H__

#include "cs_types.h"
#include "cs_network_types.h"
#include "cs_flow_api.h"

typedef struct cs_rtp_cfg_s {
	/* diff == output seq_num - input seq_num */
	cs_uint16_t			seq_num_diff;
	/* diff == output timestamp - input timestamp */
	cs_uint32_t			timestamp_diff;
	cs_uint32_t			input_ssrc;
	cs_uint32_t			output_ssrc;
} cs_rtp_cfg_t;

/*
 * RTP stats
 */
typedef struct {
	cs_uint16_t available;
	cs_uint16_t seq_num;
	cs_uint32_t timestamp;
	cs_uint32_t pkt_cnt;
	cs_uint32_t byte_cnt;
} cs_rtp_stats_t;

typedef enum {
	CS_RTP_FROMLAN = 0,
	CS_RTP_FROMWAN = 1
} cs_rtp_dir_t;

/* Exported APIs 
 *	cs_rtp_translate_add()
 *	cs_rtp_translate_delete()
 *	cs_rtp_statistics_get()
 */
#ifdef CONFIG_CS75XX_HW_ACCEL_RTP_PROXY
cs_status_t cs_rtp_translate_add(
CS_IN	cs_dev_id_t	device_id,
CS_IN	cs_rtp_cfg_t	*p_rtp_cfg,
CS_IN	cs_flow_t	*p_flow_entry,
CS_OUT	cs_rtp_id_t	*p_rtp_id);

cs_status_t cs_rtp_translate_delete(
CS_IN	cs_dev_id_t	device_id,
CS_IN	cs_rtp_cfg_t	*p_rtp_cfg);

cs_status_t cs_rtp_translate_delete_by_id(
CS_IN	cs_dev_id_t	device_id,
CS_IN	cs_rtp_id_t	rtp_id);

cs_status_t cs_rtp_statistics_get(
CS_IN	cs_rtp_id_t	rtp_id,
CS_OUT	cs_rtp_stats_t	*data);
#endif /* CONFIG_CS75XX_HW_ACCEL_RTP_PROXY */

#endif /* __CS_RTP_PROXY_H__ */
