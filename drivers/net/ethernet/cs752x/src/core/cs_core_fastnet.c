/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_core_fastnet.c
 *
 * $Id: cs_core_fastnet.c,v 1.10 2012/03/28 23:22:27 whsu Exp $
 *
 * It contains the implementation of applicationed based classification
 * by utilizing vtable framework.
 */

#include <linux/spinlock.h>
#include <linux/ip.h>
#include <net/ipv6.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/if_vlan.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include <mach/registers.h>
#include <mach/cs75xx_fe_core_table.h>
#include "cs_fe.h"
#include "cs_core_vtable.h"
#include "cs_core_logic.h"
#include "cs_core_logic_data.h"
#include "cs_core_hmu.h"
#include "cs_core_fastnet.h"
#include "cs_hmu.h"
#include "cs752x_eth.h"
#include "cs_mut.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ne_core_logic_debug;
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_FASTNET) x;}
#define DBG_ERR(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_FASTNET) x;}
#else
#define DBG(x) { }
#define DBG_ERR(x) { }

#endif

static struct list_head cs_fastnet_hash_base[CS_FASTNET_HASH_MAX];
static spinlock_t cs_fastnet_hash_base_lock;

typedef struct cs_fastnet_hash_s {
	u64 output_mask;
	u32 module_mask;
	u8 dec_ttl;

	struct cb_network_field output;
	struct cb_network_field input;
	unsigned char skb_data[CS_FASTNET_SKB_DBCHECK_MAX];
	struct net_device *dst_nic[CS_FASTNET_DST_DEV_MAX];
	struct list_head list;
	/*for copy output skb header */
	unsigned char output_skb_data[128];
	int output_l2_len;
	int output_l3_len;
	int output_l4_len;
} cs_fastnet_hash_table_t;

#if 0
static int cs_fastnet_hash_flush(void)
{
	int i;
	cs_fastnet_hash_table_t *entry;
	struct list_head *pos, *q, *table;

	DBG(printk("%s start - \n", __func__));
	spin_lock(&cs_fastnet_hash_base_lock);
	for (i = 0; i < CS_FASTNET_HASH_MAX; i++) {
		table = &cs_fastnet_hash_base[i];

		if (!list_empty(table)) {
			list_for_each_safe(pos, q, table) {
				entry =
				    list_entry(pos, cs_fastnet_hash_table_t,
					       list);
				list_del_init(&entry->list);
				cs_free(entry);

			}
		}
	}
	spin_unlock(&cs_fastnet_hash_base_lock);
	DBG(printk("%s end - \n", __func__));

	return 0;
}
#endif

#if 0
static void cs_fastnet_print_skb(char *func_str, char *data)
{
	int i;
//#if 0
//#ifdef CONFIG_CS752X_PROC
//	if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_FASTNET) {
		printk("%s skb->data\n \t", func_str);
		for (i = 0; i < 40; i++)
			printk("%02X ", data[i]);
		printk("\n");
//	}
//#endif
//#endif
}
#endif

#if 0
static int cs_fastnet_save_skb_content(cs_fastnet_hash_table_t *action_entry,
				       struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	int eth_len = ETH_HLEN;
	int vlan_len = 0;
	int pppoe_len = 0;
	int ip_len = 0;
	int l4_len = 0;
	int proto_type;		/*L4 protocol: TCP or UDP */

	if (action_entry->output.raw.vlan_tpid != 0)
		vlan_len = VLAN_HLEN;

	if (action_entry->output.l3_nh.iph.ver == 4) {
		iph = ip_hdr(skb);
		ip_len = iph->ihl * 4;
		proto_type = iph->protocol;
	} else {
		ip6h = ipv6_hdr(skb);
		ip_len = sizeof(struct ipv6hdr);
		proto_type = ip6h->nexthdr;
	}

	if (proto_type == IPPROTO_TCP) 		/* TCP */
		l4_len = sizeof(struct tcphdr);
	else if (proto_type == IPPROTO_UDP) 	/* UDP */
		l4_len = sizeof(struct udphdr);

	action_entry->output_l2_len = eth_len + vlan_len + pppoe_len;
	action_entry->output_l3_len = ip_len;
	action_entry->output_l4_len = l4_len;
	memcpy(action_entry->output_skb_data, skb->data,
	       eth_len + vlan_len + pppoe_len + ip_len + l4_len);

	return 0;
}
#endif

#if 0
static int cs_fastnet_cpy_skb_content(cs_fastnet_hash_table_t *action_entry,
				      struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	u8 ttl;

	skb_set_mac_header(skb, 0);
	skb_set_network_header(skb, action_entry->output_l2_len);
	if (action_entry->output.l3_nh.iph.ver == 4) {
		iph = ip_hdr(skb);
		ttl = iph->ttl;
	} else {
		ip6h = ipv6_hdr(skb);
		ttl = ip6h->hop_limit;
	}

	if (ttl <= 1)
		return -1;
	skb_set_transport_header(skb, action_entry->output_l2_len +
				 action_entry->output_l3_len);
	memcpy(skb->data, action_entry->output_skb_data,
	       action_entry->output_l2_len + action_entry->output_l3_len +
	       action_entry->output_l4_len);

	if (action_entry->dec_ttl == CS_DEC_TTL_ENABLE) {
		iph->ttl = ttl - 1;
	}

	skb->ip_summed = CHECKSUM_PARTIAL;

	return 0;
}
#endif

#if 0
static int cs_fastnet_setup_skb_content(cs_fastnet_hash_table_t *action_entry,
					struct sk_buff *skb)
{
	u64 temp_output_mask = action_entry->output_mask;
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct vlan_ethhdr *vlan_eth_hdr = NULL;
	//struct pppoe_hdr *ph = NULL;
	void *l4hdr = NULL;
	int eth_len = ETH_HLEN;
	int vlan_len = 0;
	int pppoe_len = 0;
	int ip_len = 0;
	u8 proto_type;		/*L4 protocol: TCP or UDP */
	u16 packet_type;

	packet_type = eth->h_proto;

	if (eth->h_proto == htons(ETH_P_8021Q)) {
		vlan_eth_hdr = (struct vlan_ethhdr *)eth;
		packet_type = vlan_eth_hdr->h_vlan_encapsulated_proto;
		vlan_len = VLAN_HLEN;
	}

	//if (action_entry->output.l3_nh.iph.ver == 4) {
	if (packet_type == htons(ETH_P_IP) /*0x0800 */ ) {
		//iph = ip_hdr(skb);
		iph = (struct iphdr *)((u8 *)(eth) + eth_len + vlan_len + pppoe_len);
		ip_len = iph->ihl * 4;
		proto_type = iph->protocol;
		if (iph->ttl <= 1)
			return -1;

		if (/*(action_entry->input.l3_nh.iph.sip != iph->saddr)*/
				(action_entry->input.l3_nh.iph.dip != iph->daddr)||
				(proto_type != action_entry->input.l3_nh.iph.protocol)) {
			DBG_ERR(printk("%s hash collsion in compare content\n",
						__func__));
			DBG_ERR(printk("\t org \t protocol 0x%hhx from %pI4 to "
					"%pI4 \n",
					action_entry->input.l3_nh.iph.protocol,
					&action_entry->input.l3_nh.iph.sip,
					&action_entry->input.l3_nh.iph.dip));
			DBG_ERR(printk("\t this \t protocol 0x%hhx from %pI4 "
						"to %pI4 \n", proto_type,
						&iph->saddr, &iph->daddr));
			return -1;
		}
	} else {
		//ip6h = ipv6_hdr(skb);
		ip6h = (struct ipv6hdr *)((u8 *)(eth) + eth_len + vlan_len +
				pppoe_len);
		ip_len = sizeof(struct ipv6hdr);
		proto_type = ip6h->nexthdr;
		if (ip6h->hop_limit <= 1)
			return -1;
	}

	skb_set_mac_header(skb, 0);
	skb_set_network_header(skb, eth_len + vlan_len + pppoe_len);
	skb_set_transport_header(skb, eth_len + vlan_len + pppoe_len + ip_len);
	l4hdr = (void *)(skb_transport_header(skb));

#if 0
	if (proto_type == IPPROTO_TCP) {
		if ((tcp_flag_word((struct tcphdr *)l4hdr)
			& (TCP_FLAG_SYN | TCP_FLAG_FIN | TCP_FLAG_RST))) {
			DBG_ERR(printk("%s error in tcp flag \n", __func__));
			return -1;
		}
#if 0
		if (((struct tcphdr *)l4hdr)->source !=
					action_entry->input.l4_h.th.sport) {
			printk("%s error in tcp soruce port %d : %d\n",
					__func__,
					((struct tcphdr *)l4hdr)->source,
					action_entry->input.l4_h.th.sport);
			return -1;
		}

		if (((struct tcphdr *)l4hdr)->dest !=
					action_entry->input.l4_h.th.dport) {
			printk("%s error in tcp dest port %d : %d\n", __func__,
				((struct tcphdr *)l4hdr)->dest,
				action_entry->input.l4_h.th.dport);
			return -1;
		}
#endif
	}

	if (proto_type == IPPROTO_UDP) {
		if ((struct udphdr *)l4hdr)->source !=
			action_entry->input.l4_h.uh.sport)
			printk("%s error in udp dest port %d : %d\n", __func__,
				((struct udphdr *)l4hdr)->source,
				action_entry->input.l4_h.uh.sport);

		if ((struct udphdr *)l4hdr)->dest !=
			action_entry->input.l4_h.uh.dest)
			printk("%s error in udp dest port %d : %d\n", __func__,
				((struct udphdr *)l4hdr)->dest,
				action_entry->input.l4_h.uh.dport);
	}
#endif
	DBG(printk
	    ("\t %s %d skb->len=%d eth=%d vlan_len=%d pppoe_len=%d ip_lan=%d \n",
	     __func__, __LINE__, skb->len, eth_len, vlan_len, pppoe_len,
	     ip_len));

	DBG(printk("\t %s %d skb->data ptr=%p eth=%p vlan=%p iph=%p l4hdr=%p\n",
		   __func__, __LINE__, skb->data, eth, vlan_eth_hdr, iph,
		   l4hdr));

	if (skb->len < (vlan_len + pppoe_len + ip_len))
		return -1;

	if (temp_output_mask & CS_HM_MAC_DA_MASK) {
		memcpy(eth->h_dest, action_entry->output.raw.da, ETH_ALEN);
	}

	if (temp_output_mask & CS_HM_MAC_SA_MASK) {
		memcpy(eth->h_source, action_entry->output.raw.sa, ETH_ALEN);
	}

	if (temp_output_mask & CS_HM_VID_1_MASK) {
		if ((action_entry->input.raw.vlan_tpid == 0) &&
		    (action_entry->output.raw.vlan_tpid != 0)) {
			/*INSERT VLAN */
			__vlan_hwaccel_put_tag(skb, action_entry->output.raw.vlan_tci,
					action_entry->output.raw.vlan_tpid);
		} else if ((action_entry->input.raw.vlan_tpid != 0) &&
			   (action_entry->output.raw.vlan_tpid == 0)) {
			/*REMOVE VLAN */
			if (skb_cow(skb, skb_headroom(skb)) < 0)
				return -1;

			/* Lifted from Gleb's VLAN code... */
			memmove(skb->data - ETH_HLEN,
				skb->data - VLAN_ETH_HLEN, 12);
			skb->mac_header += VLAN_HLEN;
		} else {
			/*REPLACE VLAN */
			vlan_eth_hdr->h_vlan_TCI =
			    htons(action_entry->output.raw.vlan_tci);
		}
	}

	if (temp_output_mask & CS_HM_IP_SA_MASK) {
		if (action_entry->output.l3_nh.iph.ver == 6) {
			memcpy(&ip6h->saddr,
			       action_entry->output.l3_nh.ipv6h.sip, 16);
		} else {
			iph->saddr = action_entry->output.l3_nh.iph.sip;
		}
	}

	if (action_entry->dec_ttl == CS_DEC_TTL_ENABLE) {
		if (iph)
			iph->ttl--;
		else if (ip6h)
			ip6h->hop_limit--;
	}

	if (temp_output_mask & CS_HM_IP_DA_MASK) {
		if (action_entry->output.l3_nh.iph.ver == 6) {
			memcpy(&ip6h->daddr,
			       action_entry->output.l3_nh.ipv6h.dip, 16);
		} else {
			iph->daddr = action_entry->output.l3_nh.iph.dip;
		}
	}

	if (temp_output_mask & CS_HM_L4_SP_MASK) {
		if (proto_type == IPPROTO_TCP) {	/* TCP */
			((struct tcphdr *)l4hdr)->source =
			    action_entry->output.l4_h.th.sport;
		} else if (proto_type == IPPROTO_UDP) {	/* UDP */
			((struct udphdr *)l4hdr)->source =
			    action_entry->output.l4_h.uh.sport;
		}
	}

	if (temp_output_mask & CS_HM_L4_DP_MASK) {
		if (proto_type == IPPROTO_TCP) {	/* TCP */
			((struct tcphdr *)l4hdr)->dest =
			    action_entry->output.l4_h.th.dport;
		} else if (proto_type == IPPROTO_UDP) {	/* UDP */
			((struct udphdr *)l4hdr)->dest =
			    action_entry->output.l4_h.uh.dport;
		}
	}

	skb->ip_summed = CHECKSUM_PARTIAL;
	return 0;
}
#endif

static int cs_fastnet_setup_skb_content_word3(
		cs_fastnet_hash_table_t *action_entry,
		struct sk_buff *skb, u32 *word3)
{
	u64 temp_output_mask = action_entry->output_mask;
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct vlan_ethhdr *vlan_eth_hdr = NULL;
	void *l4hdr = NULL;
	int eth_len = ETH_HLEN;
	int vlan_len = 0;
	int pppoe_len = 0;
	int ip_len = 0;
	u8 proto_type;		/*L4 protocol: TCP or UDP */
	u16 packet_type;

	packet_type = eth->h_proto;

	if (eth->h_proto == htons(ETH_P_8021Q)) {
		vlan_eth_hdr = (struct vlan_ethhdr *)eth;
		packet_type = vlan_eth_hdr->h_vlan_encapsulated_proto;
		vlan_len = VLAN_HLEN;
	}
	*word3 = LSO_SEGMENT_EN | (eth_len + vlan_len + pppoe_len);

	//if (action_entry->output.l3_nh.iph.ver == 4) {
	if (packet_type == htons(ETH_P_IP) /*0x0800 */ ) {
		//iph = ip_hdr(skb);
		iph = (struct iphdr *)((u8 *)(eth) + eth_len + vlan_len + pppoe_len);
		ip_len = iph->ihl * 4;
		proto_type = iph->protocol;
		if (iph->ttl <= 1)
			return -1;
		*word3 |= LSO_IPV4_FRAGMENT_EN;

		if  ((action_entry->module_mask != CS_MOD_MASK_BRIDGE) &&
			 ((action_entry->input.l3_nh.iph.dip != iph->daddr)||
			  (proto_type != action_entry->input.l3_nh.iph.protocol))) {
				DBG_ERR(printk("%s hash collsion in compare content \n", __func__));
				DBG_ERR(printk("\t org \t protocol 0x%hhx from %pI4 to %pI4 \n",
					action_entry->input.l3_nh.iph.protocol,
					&action_entry->input.l3_nh.iph.sip,
					&action_entry->input.l3_nh.iph.dip));
				DBG_ERR(printk("\t this \t protocol 0x%hhx from %pI4 to %pI4 \n",
					proto_type,
					&iph->saddr,
					&iph->daddr));
			return -1;
		}
	} else {
		//ip6h = ipv6_hdr(skb);
		ip6h = (struct ipv6hdr *)((u8 *)(eth) + eth_len + vlan_len + pppoe_len);
		ip_len = sizeof(struct ipv6hdr);
		proto_type = ip6h->nexthdr;
		*word3 |= LSO_IPV6_FREGMENT_EN;
		if (ip6h->hop_limit <= 1)
			return -1;
		if  ((action_entry->module_mask != CS_MOD_MASK_BRIDGE) &&
			 ((memcmp(action_entry->input.l3_nh.ipv6h.dip, &ip6h->daddr, 16) != 0)||
			  (proto_type != action_entry->input.l3_nh.iph.protocol))) {
				DBG_ERR(printk("%s hash collsion in compare content \n", __func__));
				DBG_ERR(printk("\t org \t protocol 0x%hhx from %pI64 to %pI64 \n",
					action_entry->input.l3_nh.iph.protocol,
					action_entry->input.l3_nh.ipv6h.sip,
					action_entry->input.l3_nh.ipv6h.dip));
				DBG_ERR(printk("\t this \t protocol 0x%hhx from %pI64 to %pI64 \n",
					proto_type,
					&ip6h->saddr,
					&ip6h->daddr));
			return -1;
		}
	}

	skb_set_mac_header(skb, 0);
	skb_set_network_header(skb, eth_len + vlan_len + pppoe_len);


	skb_set_transport_header(skb, eth_len + vlan_len + pppoe_len + ip_len);
	l4hdr = (void *)(skb_transport_header(skb));
	if (proto_type == IPPROTO_TCP)
		*word3 |= LSO_TCP_CHECKSUM_EN;
	else if (proto_type == IPPROTO_UDP)
		*word3 |= LSO_UDP_CHECKSUM_EN;
	else
		return -1;

#if 0
	if (proto_type == IPPROTO_TCP) {
		if ((tcp_flag_word((struct tcphdr *)l4hdr)
			& (TCP_FLAG_SYN | TCP_FLAG_FIN | TCP_FLAG_RST))) {
			DBG_ERR(printk("%s error in tcp flag \n", __func__));
			return -1;
		}
#if 0
		if (((struct tcphdr *)l4hdr)->source !=
					action_entry->input.l4_h.th.sport) {
			printk("%s error in tcp soruce port %d : %d\n", __func__,
				((struct tcphdr *)l4hdr)->source,
				action_entry->input.l4_h.th.sport);
			return -1;
		}

		if (((struct tcphdr *)l4hdr)->dest !=
					action_entry->input.l4_h.th.dport) {
			printk("%s error in tcp dest port %d : %d\n", __func__,
				((struct tcphdr *)l4hdr)->dest,
				action_entry->input.l4_h.th.dport);
			return -1;
		}
#endif
	}

	if (proto_type == IPPROTO_UDP) {
		if ((struct udphdr *)l4hdr)->source !=
			action_entry->input.l4_h.uh.sport)
			printk("%s error in udp dest port %d : %d\n", __func__,
				((struct udphdr *)l4hdr)->source,
				action_entry->input.l4_h.uh.sport);

		if ((struct udphdr *)l4hdr)->dest !=
			action_entry->input.l4_h.uh.dest)
			printk("%s error in udp dest port %d : %d\n", __func__,
				((struct udphdr *)l4hdr)->dest,
				action_entry->input.l4_h.uh.dport);
	}
#endif
	DBG(printk
	    ("\t %s %d skb->len=%d eth=%d vlan_len=%d pppoe_len=%d ip_lan=%d \n",
	     __func__, __LINE__, skb->len, eth_len, vlan_len, pppoe_len,
	     ip_len));

	DBG(printk("\t %s %d skb->data ptr=%p eth=%p vlan=%p iph=%p l4hdr=%p\n",
		   __func__, __LINE__, skb->data, eth, vlan_eth_hdr, iph,
		   l4hdr));

	if (skb->len < (vlan_len + pppoe_len + ip_len))
		return -1;

	if (temp_output_mask & CS_HM_MAC_DA_MASK) {
		memcpy(eth->h_dest, action_entry->output.raw.da, ETH_ALEN);
	}

	if (temp_output_mask & CS_HM_MAC_SA_MASK) {
		memcpy(eth->h_source, action_entry->output.raw.sa, ETH_ALEN);
	}

	if (temp_output_mask & CS_HM_VID_1_MASK) {
		if ((action_entry->input.raw.vlan_tpid == 0) &&
		    (action_entry->output.raw.vlan_tpid != 0)) {
			/*INSERT VLAN */
			__vlan_hwaccel_put_tag(skb, action_entry->output.raw.vlan_tci,
					action_entry->output.raw.vlan_tpid);
		} else if ((action_entry->input.raw.vlan_tpid != 0) &&
			   (action_entry->output.raw.vlan_tpid == 0)) {
			/*REMOVE VLAN */
			if (skb_cow(skb, skb_headroom(skb)) < 0)
				return -1;

			/* Lifted from Gleb's VLAN code... */
			memmove(skb->data - ETH_HLEN,
				skb->data - VLAN_ETH_HLEN, 12);
			skb->mac_header += VLAN_HLEN;
		} else {
			/*REPLACE VLAN */
			vlan_eth_hdr->h_vlan_TCI =
			    htons(action_entry->output.raw.vlan_tci);
		}
	}

	if (temp_output_mask & CS_HM_IP_SA_MASK) {
		if (action_entry->output.l3_nh.iph.ver == 6) {
			memcpy(&ip6h->saddr,
			       action_entry->output.l3_nh.ipv6h.sip, 16);
		} else {
			iph->saddr = action_entry->output.l3_nh.iph.sip;
		}
	}

	if (action_entry->dec_ttl == CS_DEC_TTL_ENABLE) {
		if (iph)
			iph->ttl--;
		else if (ip6h)
			ip6h->hop_limit--;
	}

	if (temp_output_mask & CS_HM_IP_DA_MASK) {
		if (action_entry->output.l3_nh.iph.ver == 6) {
			memcpy(&ip6h->daddr,
			       action_entry->output.l3_nh.ipv6h.dip, 16);
		} else {
			iph->daddr = action_entry->output.l3_nh.iph.dip;
		}
	}

	if (temp_output_mask & CS_HM_L4_SP_MASK) {
		if (proto_type == IPPROTO_TCP) {	/* TCP */
			((struct tcphdr *)l4hdr)->source =
			    action_entry->output.l4_h.th.sport;
		} else if (proto_type == IPPROTO_UDP) {	/* UDP */
			((struct udphdr *)l4hdr)->source =
			    action_entry->output.l4_h.uh.sport;
		}
	}

	if (temp_output_mask & CS_HM_L4_DP_MASK) {
		if (proto_type == IPPROTO_TCP) {	/* TCP */
			((struct tcphdr *)l4hdr)->dest =
			    action_entry->output.l4_h.th.dport;
		} else if (proto_type == IPPROTO_UDP) {	/* UDP */
			((struct udphdr *)l4hdr)->dest =
			    action_entry->output.l4_h.uh.dport;
		}
	}

	skb->ip_summed = CHECKSUM_PARTIAL;
	return 0;
}

static int cs_fastnet_start_xmit(cs_fastnet_hash_table_t * action_entry,
				 struct sk_buff *cs_skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(cs_skb);
	struct net_device *dst_nic;
	int ret = -1;
	//struct sk_buff * cs_skb = skb_clone(skb, GFP_ATOMIC);

	if (!cs_cb)
		return -1;

	cs_cb->fastnet.word3_valid = false;

	cs_skb->data = cs_skb->data - ETH_HLEN;
	cs_skb->len += ETH_HLEN;

	//ret = cs_fastnet_setup_skb_content(action_entry, cs_skb);
	//ret = cs_fastnet_cpy_skb_content(action_entry, cs_skb);
	ret = cs_fastnet_setup_skb_content_word3(action_entry, cs_skb,
				&cs_cb->fastnet.word3);

	if (ret != 0) {
		DBG(printk
		    ("\t %s %d setup_skb_content fail \n", __func__, __LINE__));
		cs_skb->data = cs_skb->data + ETH_HLEN;
		cs_skb->len -= ETH_HLEN;
		return -1;
	}

	dst_nic = action_entry->dst_nic[0];
	//skb_set_dev(cs_skb, dst_nic);

#if 1
	cs_skb->dev = dst_nic;
	cs_cb->fastnet.word3 += dst_nic->mtu;
	cs_cb->fastnet.word3_valid = true;
	if (cs_cb->fastnet.word3 & LSO_IPV6_FREGMENT_EN)
		cs_cb->fastnet.word3 -= dst_nic->mtu & 0x7;

	ret = dst_nic->netdev_ops->ndo_start_xmit(cs_skb, dst_nic);
	//ret = dev_queue_xmit(skb);
#else
	ret = cs_ni_start_fastnet_xmit(cs_skb, dst_nic, word3);
#endif

	DBG(printk
	    ("\t %s %d xmit %s \n", __func__, __LINE__,
	     (ret == 0) ? "success" : "fail"));

	//dev_kfree_skb(cs_skb);
	return ret;

}

int cs_core_fastnet_init(void)
{
	fe_hash_mask_entry_t hm_entry;
	unsigned int hash_mask_idx;
	int ret, i;
	uint32_t *addr;

	convert_hashmask_flag_to_data(CS_HASHMASK_SUPER_HASH, &hm_entry);
	ret = cs_fe_table_find_entry(FE_TABLE_HASH_MASK, &hm_entry,
				     &hash_mask_idx, 0);

	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		ret = cs_fe_table_add_entry(FE_TABLE_HASH_MASK, &hm_entry,
					    &hash_mask_idx);
		if (ret != 0)
			return ret;
	}
	DBG(printk("%s super hash hash mask = %d \n", __func__, hash_mask_idx));
	addr = (uint32_t *)FETOP_HASH_SUPER_HASH;
	*addr = hash_mask_idx | 0x40;

	DBG(printk("%s super hash hash mask = %d (enable 0x%x) \n", __func__,
				hash_mask_idx, *addr));

	for (i = 0; i < CS_FASTNET_HASH_MAX; i++) {
		INIT_LIST_HEAD(&cs_fastnet_hash_base[i]);
	}

	spin_lock_init(&cs_fastnet_hash_base_lock);

	return ret;
}

int cs_core_fastnet_exit(void)
{
	return 0;
}

int cs_core_fastnet_add_fwd_hash(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u16 fastnet_hash_idx;
	u32 hash_idx;
	cs_fastnet_hash_table_t *entry;
	struct list_head *table;
	int ret = CS_ACCEL_HASH_DONT_CARE;

	if (!cs_cb)
		return CS_ACCEL_HASH_DONT_CARE;

	if (cs_cb->key_misc.super_hash_vld == 0) {
		DBG(printk("%s invalid super hash - \n", __func__));
		return CS_ACCEL_HASH_DONT_CARE;
	}

	if ((cs_cb->common.module_mask & CS_MOD_MASK_PPPOE) ||
	    (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) ||
	    (cs_cb->common.module_mask & CS_MOD_MASK_L2TP) ||
	    (cs_cb->common.module_mask & CS_MOD_MASK_IPV6_ROUTING) ||
	    (cs_cb->common.module_mask & CS_MOD_MASK_IPV4_MULTICAST) ||
	    (cs_cb->common.module_mask & CS_MOD_MASK_IPV6_MULTICAST)
	    ) {
		return CS_ACCEL_HASH_DONT_CARE;
	}

	fastnet_hash_idx = cs_cb->key_misc.super_hash;
	hash_idx = HASH_INDEX_FASTNET2SW(fastnet_hash_idx);
	DBG(printk("%s create fastnet_hash_idx =0x%x hash_idx=0x%x \n",
		   __func__, fastnet_hash_idx, hash_idx));

	if (skb->dev == NULL) {
		return CS_ACCEL_HASH_DONT_CARE;
	}

	spin_lock(&cs_fastnet_hash_base_lock);
	table = &cs_fastnet_hash_base[fastnet_hash_idx];

	if (list_empty(table)) {
		entry =
		    (cs_fastnet_hash_table_t *)
		    cs_zalloc(sizeof(cs_fastnet_hash_table_t), GFP_ATOMIC);
		if (entry == NULL) {
			DBG(printk
			    ("%s no more memory - alloc memory fail \n",
			     __func__));
			ret = CS_ACCEL_HASH_FAIL;
			goto exit_add;
		}

		entry->input = cs_cb->input;
		entry->output = cs_cb->output;
		entry->module_mask = cs_cb->common.module_mask;
		entry->output_mask = cs_cb->output_mask;
		entry->dec_ttl = cs_cb->common.dec_ttl;
		entry->dst_nic[0] = skb->dev;
		//cs_fastnet_save_skb_content(entry, skb);

		DBG(printk
		    ("\t output_mask=0x%llx module_mask=0x%x, dec_ttl=%hhx\n",
		     entry->output_mask, entry->module_mask, entry->dec_ttl));

		cs_core_hmu_add_hash(hash_idx, cs_cb->lifetime, NULL);
		cs_core_hmu_link_src_and_hash(cs_cb, hash_idx, NULL);

		ret = CS_ACCEL_HASH_SUCCESS;

		list_add_tail(&entry->list, table);

	}
	/*FIXME: need to handle multicast and hash collision */
exit_add:
	spin_unlock(&cs_fastnet_hash_base_lock);
	return ret;
}

int cs_core_fastnet_fast_xmit(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u16 fastnet_hash_idx;
	u32 hash_idx;
	cs_fastnet_hash_table_t *entry;
	struct list_head *table;
	int ret = -1;

	if (!cs_cb)
		return -1;
	if (cs_cb->key_misc.super_hash_vld == 0) {
		DBG(printk("%s invalid super hash - \n", __func__));
		return -1;
	}

	fastnet_hash_idx = cs_cb->key_misc.super_hash;
	hash_idx = HASH_INDEX_FASTNET2SW(fastnet_hash_idx);
	DBG(printk
	    ("%s xmit fastnet_hash_idx=0x%x - \n", __func__, fastnet_hash_idx));

	spin_lock(&cs_fastnet_hash_base_lock);
	table = &cs_fastnet_hash_base[fastnet_hash_idx];
	if (!list_empty(table)) {

		entry = list_first_entry(table, cs_fastnet_hash_table_t, list);

		DBG(printk("\t %s found fastnet entry@%p\n", __func__, entry));
		/*FIXME: if hash collsion, need to check skb content */
		DBG(printk("\t out dev: %s entry=@%p output_mask=0x%llx"
					"module_mask=0x%x\n",
					entry->dst_nic[0]->name, entry,
					entry->output_mask, entry->module_mask));
		ret = cs_fastnet_start_xmit(entry, skb);
	}
	spin_unlock(&cs_fastnet_hash_base_lock);

	if (ret == 0)
		cs_hmu_hash_update_last_use(hash_idx, true);

	return ret;
}

int cs_core_fastnet_fast_xmit_hash(struct sk_buff *skb, u16 fastnet_hash_idx)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u32 hash_idx;
	cs_fastnet_hash_table_t *entry;
	struct list_head *table;
	int ret = -1;

	if (!cs_cb)
		return -1;
#if 0
	if (cs_cb->key_misc.super_hash_vld == 0) {
		DBG(printk("%s invalid super hash - \n", __func__));
		return -1;
	}
#endif

	//fastnet_hash_idx = cs_cb->key_misc.super_hash;
	hash_idx = HASH_INDEX_FASTNET2SW(fastnet_hash_idx);
	DBG(printk
	    ("%s xmit fastnet_hash_idx=0x%x - \n", __func__, fastnet_hash_idx));

	spin_lock(&cs_fastnet_hash_base_lock);
	table = &cs_fastnet_hash_base[fastnet_hash_idx];
	if (!list_empty(table)) {

		entry = list_first_entry(table, cs_fastnet_hash_table_t, list);

		DBG(printk("\t %s found fastnet entry@%p \n", __func__, entry));
		/*FIXME: if hash collsion, need to check skb content */
		DBG(printk("\t out dev: %s entry=@%p output_mask=0x%llx "
					"module_mask=0x%x\n",
					entry->dst_nic[0]->name, entry,
					entry->output_mask,
					entry->module_mask));
		ret = cs_fastnet_start_xmit(entry, skb);
	}
	spin_unlock(&cs_fastnet_hash_base_lock);

	if (ret == 0)
		cs_hmu_hash_update_last_use(hash_idx, true);

	return ret;
}


int cs_core_fastnet_del_fwd_hash(u32 hash_idx)
{
	u16 fastnet_hash_idx;
	cs_fastnet_hash_table_t *entry;
	struct list_head *table;
	struct list_head *pos, *q;

	DBG(printk("%s delete hash_idx=0x%x - \n", __func__, hash_idx));

	fastnet_hash_idx = HASH_INDEX_SW2FASTNET(hash_idx);

	spin_lock(&cs_fastnet_hash_base_lock);
	table = &cs_fastnet_hash_base[fastnet_hash_idx];

	if (!list_empty(table)) {
		list_for_each_safe(pos, q, table) {
			/*FIXME: if hash collsion, need to check skb content */
			entry = list_entry(pos, cs_fastnet_hash_table_t, list);
			DBG(printk("\t %s delete fastnet entry@%x\n", __func__,
						(u32)entry));
			list_del_init(&entry->list);
			cs_free(entry);
		}
	}
	spin_unlock(&cs_fastnet_hash_base_lock);

	return 0;
}

void cs_core_fastnet_print_table_used_count()
{
	struct list_head *table;
	int count = 0;
	int i;

	spin_lock(&cs_fastnet_hash_base_lock);
	for (i = 0; i < CS_FASTNET_HASH_MAX; i++) {
		table = &cs_fastnet_hash_base[i];
		if (!list_empty(table)) {
			count++;
		}
	}
	spin_unlock(&cs_fastnet_hash_base_lock);

	printk("\t avail_entry=%d ", CS_FASTNET_HASH_MAX - count);
	printk("\t\t used_entry=%d \t\t max_entry=%d \n",
		count, CS_FASTNET_HASH_MAX);

}

