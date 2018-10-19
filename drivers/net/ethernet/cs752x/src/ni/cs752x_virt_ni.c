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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include "cs752x_eth.h"
#include "cs752x_virt_ni.h"
#ifdef CONFIG_CS752X_ACCEL_KERNEL
#include "cs_core_logic_data.h"
#endif


#ifdef CONFIG_CS752X_VIRTUAL_ETH0
static u8 ni0_active_flag = 0;
static cs_virt_ni_t virt_ni0[NR_VIRT_NI_ETH0];
#endif /* CONFIG_CS752X_VIRTUAL_ETH0 */

#ifdef CONFIG_CS752X_VIRTUAL_ETH1
static u8 ni1_active_flag = 0;
static cs_virt_ni_t virt_ni1[NR_VIRT_NI_ETH1];
#endif /* CONFIG_CS752X_VIRTUAL_ETH1 */

#ifdef CONFIG_CS752X_VIRTUAL_ETH2
static u8 ni2_active_flag = 0;
static cs_virt_ni_t virt_ni2[NR_VIRT_NI_ETH2];
#endif /* CONFIG_CS752X_VIRTUAL_ETH2 */

/*
 * This function will return virtual device info structure.
 * vpid is the virtual device port ID, and virt_dev is the virtual device
 * struct tp->port_id of virtual device means virtual port ID now, not
 * physical device port ID.
 */
cs_virt_ni_t *cs_ni_get_virt_ni(u8 vpid, struct net_device *virt_dev)
{
//	int i;
	mac_info_t *virt_tp = (mac_info_t *)netdev_priv(virt_dev);
	mac_info_t *phy_tp  = (mac_info_t *)netdev_priv(virt_tp->dev);
	if (virt_tp->dev == virt_dev) {
		// physical device
		return NULL;
	}

	if (phy_tp->port_id == GE_PORT0) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH0
		return (&virt_ni0[vpid]);
#if 0
		for (i = 0; i < NR_VIRT_NI_ETH0; i++) {
			if (virt_ni0[i].dev == virt_dev)
				return &virt_ni0[i];
		}
#endif
#endif
		return NULL;
	}

	if (phy_tp->port_id == GE_PORT1) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
		return (&virt_ni1[vpid]);
#if 0
		for (i = 0; i < NR_VIRT_NI_ETH1; i++) {
			if (virt_ni1[i].dev == virt_dev)
				return &virt_ni1[i];
		}
#endif
#endif
		return NULL;
	}

	if (phy_tp->port_id == GE_PORT2) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
		return (&virt_ni2[vpid]);
#if 0
		for (i = 0; i < NR_VIRT_NI_ETH2; i++) {
			if (virt_ni2[i].dev == virt_dev)
				return &virt_ni2[i];
		}
#endif
#endif
		return NULL;
	}
	return NULL;
} /* cs_ni_get_virt_ni */

#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
static cs_virt_ni_t *cs_ni_virt_ni_get_ni_by_port_mask(u8 port_id,
		u16 port_mask)
{
	int i;

	if (port_id == GE_PORT0) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH0
		for (i = 0; i < NR_VIRT_NI_ETH0; i++) {
			if (virt_ni0[i].port_mask == port_mask)
				return &virt_ni0[i];
		}
#endif
		return NULL;
	}

	if (port_id == GE_PORT1) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
		for (i = 0; i < NR_VIRT_NI_ETH1; i++) {
			if (virt_ni1[i].port_mask == port_mask)
				return &virt_ni1[i];
		}
#endif
		return NULL;
	}

	if (port_id == GE_PORT2) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
		for (i = 0; i < NR_VIRT_NI_ETH2; i++) {
			if (virt_ni2[i].port_mask == port_mask)
				return &virt_ni2[i];
		}
#endif
		return NULL;
	}
	return NULL;
} /* cs_ni_virt_ni_get_ni_by_port_mask */

#else /* CONFIG_CS752X_VIRTUAL_NI_DBLTAG */
static cs_virt_ni_t *cs_ni_virt_ni_get_ni_by_vid(u8 port_id, u16 vid)
{
	int i;

	if (port_id == GE_PORT0) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH0
		for (i = 0; i < NR_VIRT_NI_ETH0; i++) {
			if (virt_ni0[i].vid == vid)
				return &virt_ni0[i];
		}
#endif
		return NULL;
	}

	if (port_id == GE_PORT1) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
		for (i = 0; i < NR_VIRT_NI_ETH1; i++) {
			if (virt_ni1[i].vid == vid)
				return &virt_ni1[i];
		}
#endif
		return NULL;
	}

	if (port_id == GE_PORT2) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
		for (i = 0; i < NR_VIRT_NI_ETH2; i++) {
			if (virt_ni2[i].vid == vid)
				return &virt_ni2[i];
		}
#endif
		return NULL;
	}
	return NULL;
} /* cs_ni_virt_ni_get_ni_by_vid */
#endif

#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
static int process_rx_skb_with_cputag(u16 port_id, struct sk_buff *skb)
{
	mac_info_t *virt_tp;
	cs_virt_ni_t *virt_ni_ptr;
	struct rtl_cpu_tag_s *cputag_ptr = NULL;
	struct rtl_cpu_tag_s cputag_buffer;
	struct sk_buff *prev_skb = NULL, *last_skb = NULL;
#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_kernel_accel_cb_t *cs_cb;
#endif

	if (unlikely(skb_has_frags(skb))) {
		/* case of fragmented skb. hope it's not likely. */
		prev_skb = skb;
		last_skb = skb_shinfo(skb)->frag_list;
		while (last_skb->next != NULL) {
			prev_skb = last_skb;
			last_skb = last_skb->next;
		}
		if (last_skb->len >= RTL_CPUTAG_LEN) {
			cputag_ptr = (struct rtl_cpu_tag_s *)(
					skb_tail_pointer(skb) -
					RTL_CPUTAG_LEN);
		} else {
			/* this is an extreme case that cpu tag at the
			 * end of packet actually goes into 2 different
			 * fragments.  need special care. */
			cputag_ptr = &cputag_buffer;
			memcpy(cputag_ptr, skb_tail_pointer(prev_skb) -
					RTL_CPUTAG_LEN + last_skb->len,
					RTL_CPUTAG_LEN - last_skb->len);
			memcpy(cputag_ptr - last_skb->len + RTL_CPUTAG_LEN,
					last_skb->data, last_skb->len);
		}
	} else {
		cputag_ptr = (struct rtl_cpu_tag_s *)(skb_tail_pointer(skb) -
				RTL_CPUTAG_LEN);
	}

	if ((cputag_ptr != NULL) &&
			(cputag_ptr->rtl_ether_type == htons(ETH_P_RTL_CPU)) &&
			(cputag_ptr->protocol == 0x4)) {
#ifdef CONFIG_CS752X_ACCEL_KERNEL
		/*
		 * for RX packet with cpu tag, we don't support hw acceleration
		 */
		cs_cb = CS_KERNEL_SKB_CB(skb);
		if ((cs_cb != NULL) && (cs_cb->common.tag == CS_CB_TAG)) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
		}
#endif
		virt_ni_ptr = cs_ni_virt_ni_get_ni_by_port_mask(
				port_id, cputag_ptr->port_mask);
		if (virt_ni_ptr == NULL) {
			/* How could this happen? We will let kernel handle
			 * this packet whether it makes sense to Kernel
			 * network stack or not */
			return 0;
		} else {
			if (virt_ni_ptr->flag == NI_INACTIVE)
				return -1;
			skb->dev = virt_ni_ptr->dev;
		}
		/* removing the cpu tag from the end of the packet. We
		 * can simply use skb_trim to get it work. */
		if (unlikely(skb_has_frags(skb))) {
			/* supposedly we should've already got prev_skb and
			 * last_skb point to the right place */
			if (last_skb->len > RTL_CPUTAG_LEN) {
				skb_trim(last_skb, last_skb->len -
						RTL_CPUTAG_LEN);
			} else {
				if (last_skb->len < RTL_CPUTAG_LEN)
					skb_trim(prev_skb, prev_skb->len -
							RTL_CPUTAG_LEN +
							last_skb->len);

				dev_kfree_skb(last_skb);
				if (prev_skb == skb)
					skb_shinfo(skb)->frag_list = NULL;
				else
					prev_skb->next = NULL;
			}
		} else {
			skb_trim(skb, skb->len - RTL_CPUTAG_LEN);
		}
		virt_tp = netdev_priv(skb->dev);
		virt_tp->ifStatics.rx_packets++;
		virt_tp->ifStatics.rx_bytes += skb->len;
		return 1;
	}
	return 0;
} /* process_rx_skb_with_cputag */

#else /* CONFIG_CS752X_VIRTUAL_NI_DBLTAG */
static int process_rx_skb_with_vlantag(u16 port_id, struct sk_buff *skb)
{
	mac_info_t *virt_tp;
	cs_virt_ni_t *virt_ni_ptr;
//	struct vlan_ethhdr *veth = vlan_eth_hdr(skb);
	struct vlan_ethhdr *veth = veth = (struct vlan_ethhdr*)skb->data;

	/* in current Realtek switch setting, we are applying SVLAN for
	 * this virtual interface.  Therefore, if we enter here, the eth_type
	 * should always be ETH_P_8021AD (0x88A8) */
	if ((veth != NULL) && (veth->h_vlan_proto == htons(ETH_P_8021AD))) {
		unsigned char *src, *dst;
		unsigned short i;

		virt_ni_ptr = cs_ni_virt_ni_get_ni_by_vid(
				port_id, ntohs(veth->h_vlan_TCI) & VLAN_VID_MASK);
		if (virt_ni_ptr == NULL) {
			/* How could this happen? We will let kernel handle
			 * this packet whether it makes sense to Kernel
			 * network stack or not */
			return 0;
		} else {
			if (virt_ni_ptr->flag == NI_INACTIVE)
				return -1;
			skb->dev = virt_ni_ptr->dev;
		}

		/* remove the VLAN tag from the packet!! */
		if (unlikely(!pskb_may_pull(skb, VLAN_HLEN)))
			return -1;

		/* We have to do 3 4-byte copies, because the source and
		 * the target overlapped. */
		for (i = 1; i <= 3; i++) {
			src = skb->data + (ETH_ALEN * 2) - (VLAN_HLEN * i);
			dst = skb->data + VLAN_ETH_HLEN - (VLAN_HLEN * i) - 2;
			memcpy(dst, src, VLAN_HLEN);
		}
		skb_pull(skb, VLAN_HLEN);
		virt_tp = netdev_priv(skb->dev);
		virt_tp->ifStatics.rx_packets++;
		virt_tp->ifStatics.rx_bytes += skb->len;
		return 1;
	}

	return 0;
} /* process_rx_skb_with_vlantag */

#endif

int cs_ni_virt_ni_process_rx_skb(struct sk_buff *skb)
{
	mac_info_t *tp = netdev_priv(skb->dev);
	bool do_tag_check = false;

#ifdef CONFIG_CS752X_VIRTUAL_ETH0
	if (tp->port_id == GE_PORT0)
		do_tag_check = true;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
	if (tp->port_id == GE_PORT1)
		do_tag_check = true;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
	if (tp->port_id == GE_PORT2)
		do_tag_check = true;
#endif

	if (do_tag_check == true) {
#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
		return process_rx_skb_with_cputag(tp->port_id, skb);
#else	// defined(CONFIG_CS752X_VIRTUAL_NI_DBLTAG)
		return process_rx_skb_with_vlantag(tp->port_id, skb);
#endif
	}

	return 0;
} /* cs_ni_virt_ni_process_rx_skb */

int cs_ni_virt_ni_process_tx_skb(struct sk_buff *skb, struct net_device *dev,
		struct netdev_queue *txq)
{
	mac_info_t *tp = netdev_priv(dev);
	cs_virt_ni_t *virt_ni_ptr;
	struct sk_buff *nskb;
#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
	struct rtl_cpu_tag_s *cputag_ptr;
	int padding_len;
#else
	struct vlan_ethhdr *veth;
#endif

#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_kernel_accel_cb_t *cs_cb;
#endif
#endif

	if (tp->dev == dev)
		return 0;

	virt_ni_ptr = cs_ni_get_virt_ni(tp->port_id, dev);
	skb->dev = dev;

	if (virt_ni_ptr != NULL) {
		if (!(virt_ni_ptr->flag & NI_ACTIVE))
			return -1;

#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
		/* We need to insert the CPU tag at the very end of the packet.
		 * However, we need to watch out for the case that the packet
		 * size is smaller than 60 byte.  If that's the case, we need to
		 * create software padding to 60 bytes, and then insert the CPU
		 * tag. */
		if (skb->len < 60)
			padding_len = 60 - skb->len;
		else
			padding_len = 0;

		/* in case we don't have enough room to insert the CPU tag and
		 * maintaining the at least 60 bytes of packet size, we need
		 * to expand the packet size */
		if ((padding_len + RTL_CPUTAG_LEN) > skb_tailroom(skb)) {
			struct sk_buff *nskb, temp_skb_data;

			nskb = skb_copy_expand(skb, 0, 64, GFP_ATOMIC);
			memcpy(&temp_skb_data, skb, sizeof(struct sk_buff));
			memcpy(skb, nskb, sizeof(struct sk_buff));
			memcpy(nskb, &temp_skb_data, sizeof(struct sk_buff));
			kfree_skb(nskb);
		}

		if (padding_len > 0) {
			skb_put(skb, padding_len);
			memset(skb_tail_pointer(skb) - padding_len, 0x0,
					padding_len);
		}

		skb_put(skb, RTL_CPUTAG_LEN);

		cputag_ptr = (struct rtl_cpu_tag_s *)(skb_tail_pointer(skb) -
				RTL_CPUTAG_LEN);

		memcpy(cputag_ptr, &virt_ni_ptr->cpu_tag, RTL_CPUTAG_LEN);

		/* port_mask is apparently to be good enough for packet
		 * transmission.. don't know if there is any need to use
		 * Realtek switch's other features */
#ifdef CONFIG_CS752X_ACCEL_KERNEL
		/*
		 * for TX packet with cpu tag, we don't support hw acceleration
		 */
		cs_cb = CS_KERNEL_SKB_CB(skb);
		if ((cs_cb != NULL) && (cs_cb->common.tag == CS_CB_TAG)) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
		}
#endif

#else	// defined(CONFIG_CS752X_VIRTUAL_NI_DBLTAG)
		nskb = __vlan_put_tag(skb, virt_ni_ptr->vid);
		if (nskb == NULL)
			return -ENOMEM;
		/* need to update the h_vlan_proto to the customized type */
		veth = (struct vlan_ethhdr *)skb->data;
		veth->h_vlan_proto = htons(ETH_P_8021AD);
		skb->protocol = htons(ETH_P_8021AD);
#endif
		skb->dev = virt_ni_ptr->real_dev;
		tp->ifStatics.tx_packets++;
		tp->ifStatics.tx_bytes += skb->len;
		txq_trans_update(txq);
		return 1;
	}
	return 0;
} /* cs_ni_virt_ni_process_tx_skb */

int cs_ni_virt_ni_create_if(int port_id, struct net_device *phy_dev,
		mac_info_t *phy_tp)
{
	cs_virt_ni_t *p_virt_ni = NULL;
	int j, nr_virt, vid_start, err;
	mac_info_t *virt_tp;

#ifdef CONFIG_CS752X_VIRTUAL_ETH0
	if (port_id == 0) {
		p_virt_ni = &virt_ni0[0];
		nr_virt = NR_VIRT_NI_ETH0;
		vid_start = VID_START_ETH0;
	}
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
	if (port_id == 1) {
		p_virt_ni = &virt_ni1[0];
		nr_virt = NR_VIRT_NI_ETH1;
		vid_start = VID_START_ETH1;
	}
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
	if (port_id == 2) {
		p_virt_ni = &virt_ni2[0];
		nr_virt = NR_VIRT_NI_ETH2;
		vid_start = VID_START_ETH2;
	}
#endif

	if (p_virt_ni != NULL) {
		for (j = 0; j < nr_virt; j++) {
#ifdef CONFIG_NET_SCH_MULTIQ
			p_virt_ni[j].dev = alloc_etherdev_mq(sizeof(*phy_tp),
						QUEUE_PER_INSTANCE);
#else
			p_virt_ni[j].dev = alloc_etherdev(sizeof(*phy_tp));
#endif
			if (p_virt_ni[j].dev == NULL) {
				printk(KERN_ERR "Unable to alloc new "
					"ethernet device#%d_%d\n", port_id, j);
				return -ENOMEM;
			}
			snprintf(p_virt_ni[j].dev->name, IFNAMSIZ, "%s_%d",
					phy_dev->name, j);

			virt_tp = netdev_priv(p_virt_ni[j].dev);
			/* basically virtual interface's tp is the same as
			 * its physical interface, but some of them. */
			memcpy(virt_tp, phy_tp, sizeof(*phy_tp));
			virt_tp->irq = 0;

			p_virt_ni[j].real_dev = phy_dev;
			virt_tp->mac_addr = (u32 *)p_virt_ni[j].mac_addr;
			p_virt_ni[j].dev->netdev_ops = phy_dev->netdev_ops;
			p_virt_ni[j].dev->irq = phy_tp->irq;
			p_virt_ni[j].dev->watchdog_timeo = NI_TX_TIMEOUT;
#if 0
#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
			p_virt_ni[j].dev->mtu = phy_dev->mtu - RTL_CPUTAG_LEN;
#else	// defined(CONFIG_CS752X_VIRTUAL_NI_DBLTAG)
			p_virt_ni[j].dev->mtu = phy_dev->mtu - VLAN_HLEN;
#endif
#endif
			p_virt_ni[j].dev->features = NETIF_F_SG | NETIF_F_HW_CSUM;
			/* NETIF_F_TSO nor NETIF_F_TSO6 works, because TSO is
			 * not able to generate the tag automatically.
			 * For VLAN double tag case, if it is just a single tag,
			 * then TSO is supported; however, if user decides to
			 * create another tag, then TSO is not supported. */
			memcpy(&p_virt_ni[j].dev->dev_addr[0],
					&phy_dev->dev_addr[0],
					p_virt_ni[j].dev->addr_len);
			memcpy(p_virt_ni[j].dev->perm_addr,
					p_virt_ni[j].dev->dev_addr,
					p_virt_ni[j].dev->addr_len);
			p_virt_ni[j].port_mask = j;
			p_virt_ni[j].vid = vid_start + j;
			p_virt_ni[j].flag = NI_INACTIVE;
#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
			memset(&p_virt_ni[j].cpu_tag, 0x0, RTL_CPUTAG_LEN);
			p_virt_ni[j].cpu_tag.rtl_ether_type =
				htons(ETH_P_RTL_CPU);
			p_virt_ni[j].cpu_tag.disable_learning = 0;
			p_virt_ni[j].cpu_tag.port_mask = 0x1 << j;
			p_virt_ni[j].cpu_tag.protocol = 0x4;
#endif
			// FIXME! do we need the following 2 lines?
			spin_lock_init(&virt_tp->stats_lock);
			spin_lock_init(&virt_tp->lock);
			cs_ni_set_ethtool_ops(p_virt_ni[j].dev);

			// port id of virt_tp
			virt_tp->dev = phy_dev;
			virt_tp->port_id = j;

			err = register_netdev(p_virt_ni[j].dev);
			if (err) {
				printk(KERN_ERR "%s:Cannot register net"
						" device, aborting.\n",
						p_virt_ni[j].dev->name);
				free_netdev(p_virt_ni[j].dev);
				return err;
			}
		}
	}
	return 0;
} /* cs_ni_virt_ni_create_if */

int cs_ni_virt_ni_open(u8 port_id, struct net_device *dev)
{
	mac_info_t *tp, *virt_tp = netdev_priv(dev);
	cs_virt_ni_t *virt_ni = cs_ni_get_virt_ni(port_id, dev);

	if (virt_ni->flag == NI_ACTIVE)
		return 0;
	tp = netdev_priv(virt_tp->dev); // tp of physical device.
	if (tp->status == 0) {
		/* enable phydev now. */
		dev_open(virt_tp->dev);
	}

	//printk("\t%s::open dev %p %s, phy_dev %p %s, virt ID %d\n",
	//			__func__, dev, dev->name, virt_tp->dev,
	//			virt_tp->dev->name, virt_tp->port_id);
	netif_tx_start_all_queues(dev);
	netif_carrier_on(dev);
	virt_ni->flag = NI_ACTIVE;
	return 0;
} /* cs_ni_virt_ni_open */

/* return 0 when the caller function doesn't have to turn off the physical
 * interface.  return 1 otherwise */
int cs_ni_virt_ni_close(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	cs_virt_ni_t *virt_ni = cs_ni_get_virt_ni(tp->port_id, dev);
	int j;

	if (virt_ni == NULL) {
#ifdef CONFIG_CS752X_VIRTUAL_ETH0
		if (tp->port_id == GE_PORT0) {
			for (j = 0; j < NR_VIRT_NI_ETH0; j++) {
				if (virt_ni0[j].flag == NI_ACTIVE)
					dev_close(virt_ni0[j].dev);
			}
		}
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
		if (tp->port_id == GE_PORT1) {
			for (j = 0; j < NR_VIRT_NI_ETH1; j++) {
				if (virt_ni1[j].flag == NI_ACTIVE)
					dev_close(virt_ni1[j].dev);
			}
		}
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
		if (tp->port_id == GE_PORT2) {
			for (j = 0; j < NR_VIRT_NI_ETH2; j++) {
				if (virt_ni2[j].flag == NI_ACTIVE)
					dev_close(virt_ni2[j].dev);
			}
		}
#endif
	} else {
		netif_tx_disable(dev);
		virt_ni->flag = NI_INACTIVE;
		tp->status = 0;
		return 0;
	}
	return 1;
} /* cs_ni_virt_ni_close */

int cs_ni_virt_ni_remove_if(u8 port_id)
{
	int j;

#ifdef CONFIG_CS752X_VIRTUAL_ETH0
	if (port_id == GE_PORT0) {
		for (j = 0; j < NR_VIRT_NI_ETH0; j++) {
			if (virt_ni0[j].dev)
				unregister_netdev(virt_ni0[j].dev);
		}
	}
#endif /* CONFIG_CS752X_VIRTUAL_ETH0 */
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
	if (port_id == GE_PORT1) {
		for (j = 0; j < NR_VIRT_NI_ETH1; j++) {
			if (virt_ni1[j].dev)
				unregister_netdev(virt_ni1[j].dev);
		}
	}
#endif /* CONFIG_CS752X_VIRTUAL_ETH1 */
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
	if (port_id == GE_PORT2) {
		for (j = 0; j < NR_VIRT_NI_ETH2; j++) {
			if (virt_ni2[j].dev)
				unregister_netdev(virt_ni2[j].dev);
		}
	}
#endif /* CONFIG_CS752X_VIRTUAL_ETH2 */
	return 0;
} /* cs_ni_virt_ni_remove_if */

int cs_ni_virt_ni_set_phy_port_active(u8 port_id, bool active)
{
#ifdef CONFIG_CS752X_VIRTUAL_ETH0
	if (port_id == GE_PORT0)
		ni0_active_flag = active? NI_ACTIVE: NI_INACTIVE;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
	if (port_id == GE_PORT1)
		ni1_active_flag = active? NI_ACTIVE: NI_INACTIVE;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
	if (port_id == GE_PORT2)
		ni2_active_flag = active? NI_ACTIVE: NI_INACTIVE;
#endif
	return 0;
} /* cs_ni_virt_ni_set_phy_port_active */

