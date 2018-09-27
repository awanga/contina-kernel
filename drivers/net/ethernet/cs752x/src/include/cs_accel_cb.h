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
#ifndef __CS_ACCEL_CB_H__
#define __CS_ACCEL_CB_H__

#include <linux/skbuff.h>

/* initialization and exit APIs */
int cs_accel_cb_init(void);
void cs_accel_cb_exit(void);

int cs_accel_cb_add(struct sk_buff *skb);
int cs_accel_cb_del(struct sk_buff *skb);
int cs_accel_cb_copy(struct sk_buff *dst_skb, const struct sk_buff *src_skb);
int cs_accel_cb_clone(struct sk_buff *new_skb, const struct sk_buff *old_skb);
int cs_accel_cb_reset(struct sk_buff *skb);
int cs_accel_cb_reset_state(struct sk_buff *skb);
void cs_accel_cb_print(void);

#endif	/* __CS_ACCEL_CB_H__ */
