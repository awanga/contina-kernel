/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *				Wen Hsu <whsu@cortina-systems.com>
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
 * cs_hw_accel_core.c
 *
 * $Id$
 *
 * This file contains the implementation for CS Forwarding Engine Offload
 * Kernel Module.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/kernel.h>
//#include <linux/slab.h>

#include "cs_hw_accel_core.h"
#include "cs_hw_accel_forward.h"
#include "cs_hw_accel_mc.h"
#include "cs_hw_accel_bridge.h"
#include "cs_hw_accel_pppoe.h"
#include "cs_hw_accel_vlan.h"
#include "cs_hw_accel_arp.h"
#include "cs_hw_nf_drop.h"
#include "cs_core_vtable.h"

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
#include "cs_hw_accel_ipsec.h"
#endif
#ifdef CONFIG_CS75XX_WFO
#include "cs_hw_accel_wfo.h"
#endif

#ifdef CS75XX_HW_ACCEL_TUNNEL
#include "cs_hw_accel_tunnel.h"
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_RTP_PROXY
#include "cs_hw_accel_rtp_proxy.h"
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
#include "cs_hw_accel_wireless.h"
#endif
#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#endif /* CONFIG_CS752X_PROC */

static int cs_hw_accel_major = CS_HW_ACCEL_MAJOR_N;

#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
#include "cs_hw_accel_pptp.h"
#endif

#ifdef CONFIG_CS752X_PROC
#define DBG(x)  if(cs_adapt_debug & CS752X_ADAPT_CORE) x
#else
#define DBG(x)	{}
#endif

const struct file_operations cs_hw_accel_fops = {
	.read = cs_hw_accel_read,
	.open = cs_hw_accel_open,
	.release = cs_hw_accel_release,
};

ssize_t cs_hw_accel_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	DBG(printk("%s: \n",__func__));
	return  0;
}

int cs_hw_accel_open(struct inode *inode, struct file *filp)
{
	DBG(printk("%s: \n",__func__));
	return 0;
}

int cs_hw_accel_release(struct inode *inode, struct file *filp)
{
	DBG(printk("%s: \n",__func__));
	return 0;
}

int __init cs_hw_accel_init(void)
{
	int ret;

	ret = register_chrdev(cs_hw_accel_major, "CS752X_HW_ACCELERATION",
			&cs_hw_accel_fops);
	if (ret < 0) {
		printk(KERN_ERR "cs_hw_accel: can't get major %d.\n",
			cs_hw_accel_major);
		return ret;
	}

	cs_hw_accel_major = ret;
//	cs_kernel_core_hw_fe_init();
//	cs_kernel_core_init_cfg();
	cs_mc_init();
	cs_forward_init();
	cs_hw_nf_drop_init();
	cs_bridge_init();
	cs_vlan_init();
#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
	cs_hw_accel_ipsec_init();
#endif
	cs_pppoe_init();
#ifdef CONFIG_CS75XX_WFO
	cs_hw_accel_wfo_init();
#endif

#ifdef CONFIG_IPV6
	//cs_v6_route_init();
#endif
	//cs_mc_init();
	cs_hw_arp_init();

#ifdef CS75XX_HW_ACCEL_TUNNEL
	cs_hw_accel_tunnel_init();
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_RTP_PROXY
	cs_hw_accel_rtp_proxy_init();
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
	cs_hw_accel_pptp_init();
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	cs_hw_accel_wireless_init();
#endif
	return 0;
}

void __exit cs_hw_accel_cleanup(void)
{
	unregister_chrdev(cs_hw_accel_major, "CS752X_HW_ACCELERATION");
	cs_mc_exit();
	cs_forward_exit();
	cs_hw_nf_drop_exit();
	cs_bridge_exit();
	cs_vlan_exit();
	cs_hw_arp_exit();
#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
	cs_hw_accel_ipsec_exit();
#endif
#ifdef CONFIG_CS75XX_WFO
	cs_hw_accel_wfo_exit();
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_RTP_PROXY
	cs_hw_accel_rtp_proxy_exit();
#endif
#ifdef CS75XX_HW_ACCEL_TUNNEL
	cs_hw_accel_tunnel_exit();
#endif
}

module_init(cs_hw_accel_init);
module_exit(cs_hw_accel_cleanup);

