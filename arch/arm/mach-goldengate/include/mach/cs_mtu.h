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

#ifndef __CS_MTU_H__
#define __CS_MTU_H__

#include "cs_types.h"

typedef enum {
	/* Max IP MTU (IP payload) in ether frame = 1500 */
	CS_ENCAP_IPOE = 1,
	/* Max IP MTU (IP payload) in PPPoE frame = 1492 */
	CS_ENCAP_PPPOE = 2,
	/* Max IP MTU (IP payload) in L2TP tunnel = 1452 */
	CS_ENCAP_L2TP = 3,
	/* Max IP MTU (IP payload) in IPSEC frame = 1389 */	
	CS_ENCAP_IPSEC = 4,
	CS_ENCAP_MAX
} cs_encap_type_t;

/* Set the IP MTU (Max IP payload including IP header) for given encaps and port */
cs_status_t cs_port_encap_ip_mtu_add( CS_IN cs_dev_id_t device_id, 
                                    CS_IN cs_port_id_t port_id, 
                                    CS_IN cs_encap_type_t encap, 
                                    CS_IN cs_uint32_t ip_mtu );
/* Delete the MTU check for given encaps and port */
cs_status_t cs_port_encap_ip_mtu_delete( CS_IN cs_dev_id_t device_id, 
                                       CS_IN cs_port_id_t port_id, 
                                       CS_IN cs_encap_type_t encap );
/* Get the IP MTU (Max IP payload including IP header) for given encaps and port */
cs_status_t cs_port_encap_ip_mtu_get( CS_IN  cs_dev_id_t device_id, 
                                    CS_IN  cs_port_id_t port_id, 
                                    CS_IN  cs_encap_type_t encap, 
                                    CS_OUT cs_uint32_t *ip_mtu );
#endif /* __CS_MTU_H__ */

