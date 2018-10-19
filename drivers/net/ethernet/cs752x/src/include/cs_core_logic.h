/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <whsu@cortina-systems.com>
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
#ifndef __CS_CORE_LOGIC_H__
#define __CS_CORE_LOGIC_H__

#include "cs_core_logic_data.h"
#include <linux/export.h>
#include <linux/skbuff.h>
#include "cs_accel_cb.h"

/* initialization and exit APIs */
int cs_core_logic_init(void);
void cs_core_logic_exit(void);

/* control block APIs */
/* initialize CB at NI RX */
int cs_core_logic_input_set_cb(struct sk_buff *skb);
int cs_core_logic_output_set_cb(struct sk_buff *skb);

/* set up life time for the hash that might get created */
int cs_core_logic_set_lifetime(cs_kernel_accel_cb_t *cb,
		unsigned int lifetime_sec);

/* insert a swid64 to control block */
int cs_core_logic_add_swid64(cs_kernel_accel_cb_t *cb, u64 swid64);

/* to add hash for flow-based hardware acceleration */
int cs_core_logic_add_connections(struct sk_buff *skb);

/* manually add a forwarding hash */
int cs_core_add_fwd_hash(cs_fwd_hash_t *fwd_hash_entry, u16 *fwd_hash_idx);

/* manually add a QoS hash */
int cs_core_add_qos_hash(cs_qos_hash_t *qos_hash_entry, u16 *qos_hash_idx);

/* manually add both FWD and QOS hashes and link both of them together */
int cs_core_add_hash(cs_fwd_hash_t *fwd_hash_entry, u16 *fwd_hash_idx,
		cs_qos_hash_t *qos_hash_entry, u16 *qos_hash_idx);

/* setting up hash key info from flag and cs_cb */
int cs_core_set_hash_key(u64 flag, cs_kernel_accel_cb_t *cb, fe_sw_hash_t *key,
		bool is_from_re);

#endif	/* __CS_CORE_LOGIC_H__ */
