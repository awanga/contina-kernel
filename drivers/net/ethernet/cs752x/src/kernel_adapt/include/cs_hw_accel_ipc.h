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
#ifndef __CS_HW_ACCEL_IPC_H__
#define __CS_HW_ACCEL_IPC_H__

#define CS_IPC_ENABLED	1
#ifdef CS_IPC_ENABLED
#include <mach/g2cpu_ipc.h>
#include "cs_hw_accel_sa_id.h"

/* IPC related */
cs_status_t cs_pe_ipc_register(void);
cs_status_t cs_pe_ipc_deregister(void);

cs_status_t
cs_pe_ipc_send(
		cs_sa_id_direction_t direction,
		cs_uint16_t msg_type,
		const void *msg_data,
		cs_uint16_t msg_size);

cs_status_t
cs_pe_ipc_send_dump(
		cs_sa_id_direction_t direction,
		cs_uint8_t fun_id,
		cs_uint8_t entry_id);

cs_status_t
cs_pe_ipc_send_mib_en(
		cs_sa_id_direction_t direction,
		cs_uint8_t	enabled);

cs_status_t
cs_pe_ipc_send_pe_entry_en(
		cs_sa_id_direction_t direction);
#endif /* CS_IPC_ENABLED */
#endif /* __CS_HW_ACCEL_IPC_H__ */
