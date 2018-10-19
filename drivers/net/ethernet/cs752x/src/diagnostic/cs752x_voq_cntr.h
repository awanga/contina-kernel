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

#ifndef __CS752X_VOQ_CNTR_H__
#define __CS752X_VOQ_CNTR_H__
#include <linux/netdevice.h>
#include "cs75xx_tm.h"

int cs_diag_add_voq_cntr(u8 voq_id);
int cs_diag_del_voq_cntr(u8 voq_id);
int cs_diag_get_voq_cntr(u8 voq_id, cs_tm_pm_cntr_t *p_tm_pm_cntr);
void cs_diag_print_voq_cntr(u8 voq_id);
int cs_diag_voq_cntr_ioctl(struct net_device *dev, void *pdata, void *cmd);
int cs_diag_voq_cntr_init(void);
void cs_diag_voq_cntr_exit(void);

#endif  /* __CS752X_VOQ_CNTR_H__ */

