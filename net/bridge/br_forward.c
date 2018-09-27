/*
 *	Forwarding decision
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/netpoll.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/netfilter_bridge.h>
#ifdef CONFIG_BRIDGE_PKT_FWD_FILTER
#include <linux/export.h>
#include <net/br_pkt_fwd_filter.h>
#endif
#include "br_private.h"

static int deliver_clone(const struct net_bridge_port *prev,
			 struct sk_buff *skb,
			 void (*__packet_hook)(const struct net_bridge_port *p,
					       struct sk_buff *skb));

/* Don't forward packets to originating port or forwarding diasabled */
static inline int should_deliver(const struct net_bridge_port *p,
				 const struct sk_buff *skb)
{
	return (((p->flags & BR_HAIRPIN_MODE) || skb->dev != p->dev) &&
		p->state == BR_STATE_FORWARDING);
}

static inline unsigned packet_length(const struct sk_buff *skb)
{
	return skb->len - (skb->protocol == htons(ETH_P_8021Q) ? VLAN_HLEN : 0);
}

int br_dev_queue_push_xmit(struct sk_buff *skb)
{
	/* ip_fragment doesn't copy the MAC header */
	if (nf_bridge_maybe_copy_header(skb) ||
	    (packet_length(skb) > skb->dev->mtu && !skb_is_gso(skb))) {
		kfree_skb(skb);
	} else {
		skb_push(skb, ETH_HLEN);
		br_drop_fake_rtable(skb);
		dev_queue_xmit(skb);
	}

	return 0;
}

int br_forward_finish(struct sk_buff *skb)
{
	return NF_HOOK(NFPROTO_BRIDGE, NF_BR_POST_ROUTING, skb, NULL, skb->dev,
		       br_dev_queue_push_xmit);

}

static void __br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;

	if (unlikely(netpoll_tx_running(to->dev))) {
		if (packet_length(skb) > skb->dev->mtu && !skb_is_gso(skb))
			kfree_skb(skb);
		else {
			skb_push(skb, ETH_HLEN);
			br_netpoll_send_skb(to, skb);
		}
		return;
	}

	NF_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
		br_forward_finish);
}

#ifdef CONFIG_BRIDGE_PKT_FWD_FILTER
extern unsigned short vlan_dev_get_vid(struct net_device *dev);
#endif

static void __br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	struct net_device *indev;
#ifdef CONFIG_BRIDGE_PKT_FWD_FILTER
	int vid;
	struct packet_fwd_filter *from_filter, *to_filter;
	unsigned char from_grp, to_grp;
#endif

	if (skb_warn_if_lro(skb)) {
		kfree_skb(skb);
		return;
	}

#ifdef CONFIG_BRIDGE_PKT_FWD_FILTER
	/* forwarding between the same group is denied */
	from_grp = skb->dev->fwd_grp;
	to_grp = to->dev->fwd_grp;
	if (from_grp & to_grp)
		goto out;

	/*
	 * Packets from LAN/WLAN to WAN is controlled as below.
	 * Check if VID of egress port is belong to pkt_fwd_filter of
	 * the ingress port. (1: forward, 0: drop)
	 */
	from_filter = skb->dev->pkt_fwd_filter;
	if((to_grp & NETDEV_GRP_WAN) &&
		(from_grp != NETDEV_GRP_IGNORE) &&
		(from_filter)) {
		if (to->dev->priv_flags & IFF_802_1Q_VLAN) {
			vid = vlan_dev_get_vid(to->dev);
			if (!(from_filter->vid_map[vid >> 3] & (1 << (vid & 0x07))))
				goto out;
		} else {
			if (!(from_filter->vid_map[0] & 0x01))
				goto out;
		}
	}
	
	/*
	 * Packets from WAN to LAN/WLAN is controlled as below.
	 * Check if VID of ingress port is belong to pkt_fwd_filter of
	 * the egress port. (1: forward, 0: drop)
	 */
	to_filter = to->dev->pkt_fwd_filter;
	if((from_grp & NETDEV_GRP_WAN) &&
		(to_grp != NETDEV_GRP_IGNORE) &&
		(to_filter)) {
		if (skb->dev->priv_flags & IFF_802_1Q_VLAN) {
			vid = vlan_dev_get_vid(skb->dev);
			if (!(to_filter->vid_map[vid >> 3] & (1 << (vid & 0x07))))
				goto out;
		} else {
			if (!(to_filter->vid_map[0] & 0x01))
				goto out;
		}
	}
#endif	
	indev = skb->dev;
	skb->dev = to->dev;
	skb_forward_csum(skb);

	NF_HOOK(NFPROTO_BRIDGE, NF_BR_FORWARD, skb, indev, skb->dev,
		br_forward_finish);

#ifdef CONFIG_BRIDGE_PKT_FWD_FILTER
	return;
out:
	kfree_skb(skb);
#endif
}

/* called with rcu_read_lock */
void br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (to && should_deliver(to, skb)) {
		__br_deliver(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_forward(const struct net_bridge_port *to, struct sk_buff *skb, struct sk_buff *skb0)
{
	if (should_deliver(to, skb)) {
		if (skb0)
			deliver_clone(to, skb, __br_forward);
		else
			__br_forward(to, skb);
		return;
	}

	if (!skb0)
		kfree_skb(skb);
}

static int deliver_clone(const struct net_bridge_port *prev,
			 struct sk_buff *skb,
			 void (*__packet_hook)(const struct net_bridge_port *p,
					       struct sk_buff *skb))
{
	struct net_device *dev = BR_INPUT_SKB_CB(skb)->brdev;

	skb = skb_clone(skb, GFP_ATOMIC);
	if (!skb) {
		dev->stats.tx_dropped++;
		return -ENOMEM;
	}

	__packet_hook(prev, skb);
	return 0;
}

static struct net_bridge_port *maybe_deliver(
	struct net_bridge_port *prev, struct net_bridge_port *p,
	struct sk_buff *skb,
	void (*__packet_hook)(const struct net_bridge_port *p,
			      struct sk_buff *skb))
{
	int err;

	if (!should_deliver(p, skb))
		return prev;

	if (!prev)
		goto out;

	err = deliver_clone(prev, skb, __packet_hook);
	if (err)
		return ERR_PTR(err);

out:
	return p;
}

/* called under bridge lock */
static void br_flood(struct net_bridge *br, struct sk_buff *skb,
		     struct sk_buff *skb0,
		     void (*__packet_hook)(const struct net_bridge_port *p,
					   struct sk_buff *skb))
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;

	prev = NULL;

	list_for_each_entry_rcu(p, &br->port_list, list) {
		prev = maybe_deliver(prev, p, skb, __packet_hook);
		if (IS_ERR(prev))
			goto out;
	}

	if (!prev)
		goto out;

	if (skb0)
		deliver_clone(prev, skb, __packet_hook);
	else
		__packet_hook(prev, skb);
	return;

out:
	if (!skb0)
		kfree_skb(skb);
}


/* called with rcu_read_lock */
void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb)
{
	br_flood(br, skb, NULL, __br_deliver);
}

/* called under bridge lock */
void br_flood_forward(struct net_bridge *br, struct sk_buff *skb,
		      struct sk_buff *skb2)
{
	br_flood(br, skb, skb2, __br_forward);
}

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
/* called with rcu_read_lock */
static void br_multicast_flood(struct net_bridge_mdb_entry *mdst,
			       struct sk_buff *skb, struct sk_buff *skb0,
			       void (*__packet_hook)(
					const struct net_bridge_port *p,
					struct sk_buff *skb))
{
	struct net_device *dev = BR_INPUT_SKB_CB(skb)->brdev;
	struct net_bridge *br = netdev_priv(dev);
	struct net_bridge_port *prev = NULL;
	struct net_bridge_port_group *p;
	struct hlist_node *rp;

	rp = rcu_dereference(hlist_first_rcu(&br->router_list));
	p = mdst ? rcu_dereference(mdst->ports) : NULL;
	while (p || rp) {
		struct net_bridge_port *port, *lport, *rport;

		lport = p ? p->port : NULL;
		rport = rp ? hlist_entry(rp, struct net_bridge_port, rlist) :
			     NULL;

		port = (unsigned long)lport > (unsigned long)rport ?
		       lport : rport;

		prev = maybe_deliver(prev, port, skb, __packet_hook);
		if (IS_ERR(prev))
			goto out;

		if ((unsigned long)lport >= (unsigned long)port)
			p = rcu_dereference(p->next);
		if ((unsigned long)rport >= (unsigned long)port)
			rp = rcu_dereference(hlist_next_rcu(rp));
	}

	if (!prev)
		goto out;

	if (skb0)
		deliver_clone(prev, skb, __packet_hook);
	else
		__packet_hook(prev, skb);
	return;

out:
	if (!skb0)
		kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_multicast_deliver(struct net_bridge_mdb_entry *mdst,
			  struct sk_buff *skb)
{
	br_multicast_flood(mdst, skb, NULL, __br_deliver);
}

/* called with rcu_read_lock */
void br_multicast_forward(struct net_bridge_mdb_entry *mdst,
			  struct sk_buff *skb, struct sk_buff *skb2)
{
	br_multicast_flood(mdst, skb, skb2, __br_forward);
}
#endif

#ifdef CONFIG_BRIDGE_PKT_FWD_FILTER
static long br_pkt_fwd_filter_ioctl(struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	BR_FWD_PKT_FT_CMD_T br_cmd;
	struct net_device *dev;
	int i, vid, mode;

	if (cmd != SIOCDEVPRIVATE) {
	    printk("It is not private command (0x%X). cmd = (0x%X)\n",
		    SIOCDEVPRIVATE, cmd);
	    return -EOPNOTSUPP;
	}

	if (copy_from_user((void *)&br_cmd, argp, sizeof(br_cmd))) {
	    printk("Copy from user space fail\n");
	    return -EFAULT;
	}

	br_cmd.netdev_name[IFNAMSIZ-1] = '\0';
	dev = dev_get_by_name(&init_net, br_cmd.netdev_name);
	if(!dev) {
		printk("Unknown net device: %s\n", br_cmd.netdev_name);
		return -EPERM;
	}

	switch (br_cmd.cmd) {
	case PKT_FWD_FT_RESET:
		dev->fwd_grp = NETDEV_GRP_IGNORE;
		if(dev->pkt_fwd_filter) {
			kfree(dev->pkt_fwd_filter);
			dev->pkt_fwd_filter = NULL;
		}
		break;
		
	case PKT_FWD_FT_RAW_SET:
		dev->fwd_grp = br_cmd.fwd_grp;
		if(!dev->pkt_fwd_filter) {
			dev->pkt_fwd_filter = 
				kmalloc(sizeof(struct packet_fwd_filter),
					GFP_KERNEL);
			if(!dev->pkt_fwd_filter) {
				dev_put(dev);
				return -ENOMEM;
			}
		}
		memcpy(dev->pkt_fwd_filter->vid_map, br_cmd.vid_map,
			BR_PKT_FWD_FT_VLAN_ARRAY_LEN);
		break;
		
	case PKT_FWD_FT_RAW_GET:
		br_cmd.fwd_grp = dev->fwd_grp;
		if(dev->pkt_fwd_filter) {
			memcpy(br_cmd.vid_map, dev->pkt_fwd_filter->vid_map,
				BR_PKT_FWD_FT_VLAN_ARRAY_LEN);
		} else {
			for (i = 0; i < BR_PKT_FWD_FT_VLAN_ARRAY_LEN; i++)
				br_cmd.vid_map[i] = 0xFF;
		}

		if (copy_to_user(argp, (void *)&br_cmd, sizeof(br_cmd))) {
		    printk("Copy to user space fail\n");
		    dev_put(dev);
		    return -EFAULT;
		}
		
		break;
		
	case PKT_FWD_FT_ETH_GRP_SET:
		dev->fwd_grp = br_cmd.fwd_grp;
		break;
		
	case PKT_FWD_FT_ETH_GRP_GET:
		br_cmd.fwd_grp = dev->fwd_grp;

		if (copy_to_user(argp, (void *)&br_cmd, sizeof(br_cmd))) {
		    printk("Copy to user space fail\n");
		    dev_put(dev);
		    return -EFAULT;
		}
		
		break;
		
	case PKT_FWD_FT_ETH_VLAN_SET:
		if(!dev->pkt_fwd_filter) {
			dev->pkt_fwd_filter = 
				kzalloc(sizeof(struct packet_fwd_filter),
					GFP_KERNEL);
			if(!dev->pkt_fwd_filter) {
				dev_put(dev);
				return -ENOMEM;
			}
		}
		/* translate VID to bit offset */
		vid = br_cmd.vlan_mode.vid;
		if (br_cmd.vlan_mode.mode)	/* 1: forward */
			dev->pkt_fwd_filter->vid_map[vid >> 3] |=
				(1 << (vid & 0x07));
		else				/* 0: drop */
			dev->pkt_fwd_filter->vid_map[vid >> 3] &=
				~(1 << (vid & 0x07));
		break;
	case PKT_FWD_FT_ETH_VLAN_GET:
		if(dev->pkt_fwd_filter) {
			/* translate VID to bit offset */
			vid = br_cmd.vlan_mode.vid;
			if (dev->pkt_fwd_filter->vid_map[vid >> 3] &
				(1 << (vid & 0x07)))	/* 1: forward */
				br_cmd.vlan_mode.mode = 1;
			else				/* 0: drop */
				br_cmd.vlan_mode.mode = 0;
			}

		if (copy_to_user(argp, (void *)&br_cmd, sizeof(br_cmd))) {
		    printk("Copy to user space fail\n");
			dev_put(dev);
			return -EFAULT;
		}
		
		break;
	default:
		dev_put(dev);
		return -EPERM;
	}

	dev_put(dev);
	return 0;

}

static int br_pkt_fwd_filter_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int br_pkt_fwd_filter_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations br_pkt_fwd_filter_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = br_pkt_fwd_filter_ioctl,
	.open = br_pkt_fwd_filter_open,
	.release = br_pkt_fwd_filter_release,
};

static struct miscdevice br_pkt_fwd_filter_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = BR_PKT_FWD_FT_DRIVER_NAME,
	.fops = &br_pkt_fwd_filter_fops,
};

int br_pkt_fwd_filter_register(void)
{
	return misc_register(&br_pkt_fwd_filter_miscdev);
}

int br_pkt_fwd_filter_deregister(void)
{
	return misc_deregister(&br_pkt_fwd_filter_miscdev);
}
#endif /* CONFIG_BRIDGE_PKT_FWD_FILTER */
