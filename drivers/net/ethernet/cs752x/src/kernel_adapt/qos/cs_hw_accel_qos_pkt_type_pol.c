/*
 * Copyright (c) Cortina-Systems Limited 2012.  All rights reserved.
 *                Wen Hsu <wen.hsu@cortina-systems.com>
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
 * cs_hw_accel_qos_pkt_type_pol.c
 *
 * $Id: cs_hw_accel_qos_pkt_type_pol.c,v 1.2 2012/09/05 08:16:31 ewang Exp $
 *
 * This file contains the implementation for CS QoS implementation
 * for pkt_type policer.
 */

#include "cs_hw_accel_qos.h"
#include <linux/cs_ne_ioctl.h>
#include "cs75xx_tm.h"

typedef struct {
	u32 cir, cbs, pir, pbs;
} cs_qos_pkt_type_pol_t;

#define CS_QOS_PKT_TYPE_POL_PORT_MAX	3
#define CS_QOS_PKT_TYPE_POL_TYPE_MAX	5

cs_qos_pkt_type_pol_t pkt_type_pol_db[CS_QOS_PKT_TYPE_POL_PORT_MAX]
		[CS_QOS_PKT_TYPE_POL_TYPE_MAX];

int cs_qos_set_pkt_type_pol(u8 port_id, u8 pkt_type, u32 cir, u32 cbs,
		u32 pir, u32 pbs)
{
	cs_tm_pol_profile_mem_t pol_profile;
	unsigned char pol_id;
	int status;

	/* only support port_id = 0, 1, 2. pkt_type = 0 to 4. */
	if ((port_id > 2) || (pkt_type > 4))
		return -1;

	/* we only accept cir, cbs, pir, pbs under the following case:
	 * 1) cir == cbs == pir == pbs == 0; <- reset
	 * 2) cir != 0 && cbs != 0 && pbs != 0 (pir can be equalled to 0) */
	if (((cir == 0) || (cbs == 0) || (pbs == 0)) && (cir + cbs + pbs + pir))
		return -1;

	pol_id = port_id * 5 + pkt_type;

	memset(&pol_profile, 0, sizeof(pol_profile));

	pol_profile.bypass_yellow = 0;
	pol_profile.bypass_red = 0;
	pol_profile.pir_max_credit = pbs >> 7;
	/* it's in a unit of 128 bytes */
	pol_profile.cir_max_credit = cbs >> 7;
	/* it's in a unit of 128 bytes */

	/* if pir != 0, set up dual-rate.. if not, set up single-rate */
	if (pir != 0) {
		pol_profile.policer_type = CS_TM_POL_RFC_2698;
		if (cs_tm_pol_convert_rate_to_hw_value(pir, TRUE,
					&pol_profile.range,
					&pol_profile.pir_credit) != 0)
			return -1;
		if (cs_tm_pol_rate_divisor_to_credit(cir, pol_profile.range,
					&pol_profile.cir_credit) != 0)
			return -1;
	} else {
		pol_profile.policer_type = CS_TM_POL_RFC_2697;
		if (cs_tm_pol_convert_rate_to_hw_value(cir, TRUE,
					&pol_profile.range,
					&pol_profile.cir_credit) != 0)
			return -1;
		if (pol_profile.pir_max_credit == 0)
			pol_profile.pir_max_credit =
				pol_profile.cir_max_credit + 1;
	}

	status = cs_tm_pol_set_profile_mem(CS_TM_POL_PKT_TYPE_PROFILE_MEM,
			pol_id ,&pol_profile);
	if (status != 0)
		return status;

	pkt_type_pol_db[port_id][pkt_type].cir = cir;
	pkt_type_pol_db[port_id][pkt_type].cbs = cbs;
	pkt_type_pol_db[port_id][pkt_type].pir = pir;
	pkt_type_pol_db[port_id][pkt_type].pbs = pbs;

	return 0;
} /* cs_qos_set_pkt_type_pol */

int cs_qos_reset_pkt_type_pol(u8 port_id, u8 pkt_type)
{
	return cs_qos_set_pkt_type_pol(port_id, pkt_type, 0, 0, 0, 0);
} /* cs_qos_reset_pkt_type_pol */

int cs_qos_get_pkt_type_pol(u8 port_id, u8 pkt_type, u32 *cir, u32 *cbs,
		u32 *pir, u32 *pbs)
{
	/* only support port_id = 0, 1, 2. pkt_type = 0 to 4. */
	if ((port_id > 2) || (pkt_type > 4))
		return -1;
	if ((cir == NULL) || (cbs == NULL) || (pir == NULL) || (pbs == NULL))
		return -1;
	*cir = pkt_type_pol_db[port_id][pkt_type].cir;
	*cbs = pkt_type_pol_db[port_id][pkt_type].cbs;
	*pir = pkt_type_pol_db[port_id][pkt_type].pir;
	*pbs = pkt_type_pol_db[port_id][pkt_type].pbs;

	return 0;
} /* cs_qos_get_pkt_type_pol */

void cs_qos_print_pkt_type_pol(u8 port_id, u8 pkt_type)
{
	/* only support port_id = 0, 1, 2. pkt_type = 0 to 4. */
	if ((port_id > 2) || (pkt_type > 4)) {
		printk("unacceptable value: port_id = %d, pkt_type = %d\n",
				port_id, pkt_type);
		return;
	}

	printk("port_id = %d, pkt_type = %d:\n\tcir = %d, cbs = %d, pir = %d, "
			"pbs = %d\n", port_id, pkt_type,
			pkt_type_pol_db[port_id][pkt_type].cir,
			pkt_type_pol_db[port_id][pkt_type].cbs,
			pkt_type_pol_db[port_id][pkt_type].pir,
			pkt_type_pol_db[port_id][pkt_type].pbs);
} /* cs_qos_print_pkt_type_pol */

void cs_qos_print_pkt_type_pol_per_port(u8 port_id)
{
	int iii;

	/* only support port_id = 0, 1, 2. pkt_type = 0 to 4. */
	if (port_id > 2) {
		printk("unacceptable value: port_id = %d\n", port_id);
		return;
	}

	for (iii = 0; iii < CS_QOS_PKT_TYPE_POL_TYPE_MAX; iii++)
		cs_qos_print_pkt_type_pol(port_id, iii);
} /* cs_qos_print_pkt_type_pol_per_port */

void cs_qos_print_all_pkt_type_pol(void)
{
	int iii;

	for (iii = 0; iii < CS_QOS_PKT_TYPE_POL_PORT_MAX; iii++)
		cs_qos_print_pkt_type_pol_per_port(iii);
} /* cs_qos_print_all_pkt_type_pol */

int cs_qos_pkt_type_pol_init(void)
{
	memset(pkt_type_pol_db, 0x0, sizeof(cs_qos_print_pkt_type_pol_per_port)
			* CS_QOS_PKT_TYPE_POL_PORT_MAX
			* CS_QOS_PKT_TYPE_POL_PORT_MAX);
	return 0;
} /* cs_qos_pkt_type_pol_init */

void cs_qos_pkt_type_pol_exit(void)
{
} /* cs_qos_pkt_type_pol_exit */

