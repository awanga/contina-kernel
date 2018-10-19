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
#ifndef __CS_HW_ACCEL_SA_ID_H__
#define __CS_HW_ACCEL_SA_ID_H__

#include <mach/cs_tunnel.h>

#define G2_MAX_SA_IDS		255 /* for UP_STREAM=255 for DOWN_STREAM=255  */
#define G2_MAX_ENCRYPTED_TUNNEL	24 /* Per direction*/
#define G2_INVALID_SA_ID					0xffff
/*
	Encryption tunnel:	0~0x17=24
	Other kinds of plaintext offload:	0x20~0xff
*/
#define PLAINTEXT_SA_ID_BASE 0x20

typedef enum {
	UP_STREAM = 0,
	DOWN_STREAM = 1
} cs_sa_id_direction_t;

/* exported APIs */
cs_status_t
cs_sa_id_alloc(
	cs_sa_id_direction_t direction,		/* UP_STREAM(0) or DOWN_STREAM(1)*/
	cs_boolean_t		is_encrypted_tunnel,	/* CS_TRUE:1	CS_FALSE:0(plaintext tunnel) */
	cs_boolean_t		is_used_for_rekey,	/* CS_TRUE:1	CS_FALSE:0 */
	cs_tunnel_type_t	tunnel_type,
	cs_uint16_t		*sa_id			/* CS_OUT */
	);

cs_status_t cs_sa_id_free(cs_sa_id_direction_t direction, cs_uint16_t sa_id);

cs_status_t cs_sa_id_dump(void);

void cs_hw_accel_sa_id_init(void);

/* Internal data structure */
typedef struct cs_sa_id_table_s {
	cs_sa_id_direction_t direction;
	cs_boolean_t              is_encrypted_tunnel;
	cs_boolean_t              is_used_for_rekey;
	cs_tunnel_type_t        tunnel_type;
} __attribute__ ((__packed__)) cs_sa_id_t;

#endif /* __CS_HW_ACCEL_SA_ID_H__ */
