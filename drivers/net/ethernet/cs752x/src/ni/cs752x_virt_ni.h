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

#ifndef __CS752X_VIRT_NI_H__
#define __CS752X_VIRT_NI_H__

#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
/* this is the Realtek CPU tag format */
struct rtl_cpu_tag_s {
	__u16	rtl_ether_type;		/* always equals to 0x8899 */
#define ETH_P_RTL_CPU	0x8899
	__u8	protocol;
	__u8	reason;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	priority:3,
		priority_sel:1,
		enhanced_fid:3,
		efid:1;
	__u8	vidx:5,
		disable_learning:1,
		vsel:1,
		keep:1;
	__u8	port_mask_rsrv1;
	__u8	port_mask:4,
		port_mask_rsrv2:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	efid:1,
		enhanced_fid:3,
		priority_sel:1,
		priority:3;
	__u8	keep:1,
		vsel:1,
		disable_learning:1,
		vidx:5;
	__u8	port_mask_rsrv1;
	__u8	port_mask_rsrv2:4,
		port_mask:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
};

#define RTL_CPUTAG_LEN		sizeof(struct rtl_cpu_tag_s)
#else /* CONFIG_CS752X_VIRTUAL_NI_DBLTAG */
#define ETH_P_8021AD		0x88A8
#endif /* CONFIG_CS752X_VIRTUAL_NI_CPUTAG */

typedef struct cs_virt_ni_s {
	u16 port_mask;
	u16 vid;
	struct net_device *dev;
	struct net_device *real_dev;
	u8 mac_addr[6];
	u8 flag;
#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
	struct rtl_cpu_tag_s cpu_tag;
#endif
#define NI_ACTIVE	1
#define NI_INACTIVE	0
} cs_virt_ni_t;

/* use flag CONFIG_CS752X_VIRTUAL_ETH0/1/2 to determine whether or not
 * to create virtual interface on eth0/1/2 */
/* use flag CONFIG_CS752X_NR_VIRTUAL_ETH[0/1/2] to determine how many
 * interfaces are needed to create under that physical interface */

#ifdef CONFIG_CS752X_VIRTUAL_ETH0

#if !defined(CONFIG_CS752X_NR_VIRTUAL_ETH0)
	#define NR_VIRT_NI_ETH0	1
#elif (CONFIG_CS752X_NR_VIRTUAL_ETH0 == 0)
	#define NR_VIRT_NI_ETH0	1
#else
	#define NR_VIRT_NI_ETH0 CONFIG_CS752X_NR_VIRTUAL_ETH0
#endif

#if !defined(CONFIG_CS752X_VID_START_ETH0)
	#define VID_START_ETH0	1
#elif (CONFIG_CS752X_VID_START_ETH0 == 0)
	#define VID_START_ETH0	1
#else
	#define VID_START_ETH0	CONFIG_CS752X_VID_START_ETH0
#endif
#endif /* CONFIG_CS752X_VIRTUAL_ETH0 */

#ifdef CONFIG_CS752X_VIRTUAL_ETH1

#ifndef CONFIG_CS752X_NR_VIRTUAL_ETH1
	#define NR_VIRT_NI_ETH1	1
#elif (CONFIG_CS752X_NR_VIRTUAL_ETH1 == 0)
	#define NR_VIRT_NI_ETH1	1
#else
	#define NR_VIRT_NI_ETH1 CONFIG_CS752X_NR_VIRTUAL_ETH1
#endif

#if !defined(CONFIG_CS752X_VID_START_ETH1)
	#define VID_START_ETH1	1
#elif (CONFIG_CS752X_VID_START_ETH1 == 0)
	#define VID_START_ETH1	1
#else
	#define VID_START_ETH1	CONFIG_CS752X_VID_START_ETH1
#endif
#endif /* CONFIG_CS752X_VIRTUAL_ETH1 */

#ifdef CONFIG_CS752X_VIRTUAL_ETH2

#if !defined(CONFIG_CS752X_NR_VIRTUAL_ETH2)
	#define NR_VIRT_NI_ETH2	1
#elif (CONFIG_CS752X_NR_VIRTUAL_ETH2 == 0)
	#define NR_VIRT_NI_ETH2	1
#else
	#define NR_VIRT_NI_ETH2 CONFIG_CS752X_NR_VIRTUAL_ETH2
#endif

#if !defined(CONFIG_CS752X_VID_START_ETH2)
	#define VID_START_ETH2	1
#elif (CONFIG_CS752X_VID_START_ETH2 == 0)
	#define VID_START_ETH2	1
#else
	#define VID_START_ETH2	CONFIG_CS752X_VID_START_ETH2
#endif
#endif /* CONFIG_CS752X_VIRTUAL_ETH2 */

cs_virt_ni_t *cs_ni_get_virt_ni(u8 vpid, struct net_device *virt_dev);
int cs_ni_virt_ni_set_phy_port_active(u8 port_id, bool active);
int cs_ni_virt_ni_create_if(int port_id, struct net_device *phy_dev,
		mac_info_t *phy_tp);
int cs_ni_virt_ni_open(u8 port_id, struct net_device *dev);
int cs_ni_virt_ni_close(struct net_device *dev);
int cs_ni_virt_ni_remove_if(u8 port_id);
int cs_ni_virt_ni_process_rx_skb(struct sk_buff *skb);
int cs_ni_virt_ni_process_tx_skb(struct sk_buff *skb, struct net_device *dev,
		struct netdev_queue *txq);

#endif	/* __CS752X_VIRT_NI_H__ */
