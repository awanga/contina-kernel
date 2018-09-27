/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
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
 * cs_hw_accel_etherip.h
 *
 * $Id: cs_hw_accel_etherip.h,v 1.1 2012/03/14 15:28:15 pritam Exp $
 *
 * This header file defines the data structures and APIs for CS EtherIP Offload.
 */

#ifndef __CS_HW_ACCEL_ETHERIP_H__
#define __CS_HW_ACCEL_ETHERIP_H__

#include <linux/skbuff.h>
#include <net/xfrm.h>
#include <linux/rbtree.h>
#include <mach/cs_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_core_logic.h>
#include <net/ipv6.h>
#include <linux/in6.h>
#include <linux/if_tunnel.h>

#define ETHERIP_MAX_TUNNELS 32
void cs_hw_accel_etherip_init(void) ;
void cs_hw_accel_etherip_exit(void) ;

#endif				/* __CS_HW_ACCEL_ETHERIP_H__ */
