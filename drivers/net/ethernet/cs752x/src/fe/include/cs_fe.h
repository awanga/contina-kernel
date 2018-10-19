/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                CH Hsu <ch.hsu@cortina-systems.com>
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

#ifndef __CS_FE_H__
#define __CS_FE_H__

#include <linux/types.h>
#include <linux/etherdevice.h>
#include <mach/cs75xx_fe_core_table.h>
#include "cs_fe_head_table.h"
#include "cs_fe_table_api.h"
#include "cs_fe_hash.h"
#include "cs_fe_util_api.h"

typedef enum {
	FE_STATUS_OK,
	FE_ERR_ENTRY_NOT_FOUND,
	FE_ERR_ENTRY_DUPLICATE,
	FE_ERR_ENTRY_USED,	// specified entry occupied by someone else
	FE_ERR_ENTRY_INVALID,
	FE_ERR_TABLE_FULL,
	FE_ERR_WRITE_HW_TABLE,

	FE_ERR_ALLOC_FAIL,
	FE_ERR_RELEASE_FAIL,
	/* we may add more table specific error types here */
	FE_ERR_TOTAL,
} g2_fe_table_error_e;

int cs_fe_init(void);
void cs_fe_exit(void);

/* ioctl declaration */
int cs_fe_ioctl_acl(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_an_bng_mac(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_class(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_etype(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_fvlan(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_fwdrslt(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_hashcheck(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_hashhash(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_hashmask(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_hashoverflow(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_l2mac(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_l3ip(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_l4portrngs(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_llchdr(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_lpb(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_lpm(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_pktlen(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_qosrslt(struct net_device *dev, void *pdata,
		void *cmd);
int cs_fe_ioctl_sdb(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_vlan(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_voqdrp(struct net_device *dev, void *pdata, void *cmd);
int cs_fe_ioctl_voqpol(struct net_device *dev, void *pdata, void *cmd);
#endif /* __CS_FE_H__ */
