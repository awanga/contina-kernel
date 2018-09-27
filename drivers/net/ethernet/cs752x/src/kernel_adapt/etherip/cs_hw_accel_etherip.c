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
 * cs_hw_accel_etherip.c
 *
 * $Id: cs_hw_accel_etherip.c,v 1.3 2012/06/28 02:48:12 whsu Exp $
 *
 * This file contains the implementation for CS EtherIP Offload Kernel Module.
 */

#include <linux/if_ether.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/esp.h>
#include <net/ah.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/crypto.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_core_hmu.h"
#include <cs752x_eth.h>
#include <linux/etherdevice.h>
#include <linux/ip6_tunnel.h>
#include "cs_hw_accel_manager.h"
#include "cs_hw_accel_etherip.h"



#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#endif				/* CONFIG_CS752X_PROC */

#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_ETHERIP) (x)
//#define DBG(x)	{x;}

extern void ni_dm_byte(unsigned int location, int length);

/* ip_ver 
		1 :- IPv6 
		0 :- IPv4
 * dir 
		1 :- Tx
		0 :- Rx
*/
void k_jt_cs_etherip_handler(struct sk_buff *skb, struct ip6_tnl_parm *p,
		cs_uint8 ip_ver, cs_uint8 dir)
{
	int status = 0;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ipv6hdr *ip6h = (struct ipv6hdr *)((cs_uint8*)skb->data + 14);
	cs_uint16 *proto = (cs_uint16*)((cs_uint8*)skb->data + 12);

	DBG(printk("%s:%d\n",__func__, __LINE__));
	if (cs_cb == NULL)
		return;
	DBG(printk("%s:%d cs_cb->common.tag 0x%x\n",__func__, __LINE__,cs_cb->common.tag));
	if (cs_cb->common.tag != CS_CB_TAG)
		return;

	cs_cb->common.module_mask |= CS_MOD_MASK_ETHERIP;
	if (ip_ver) { /* V6 */
		DBG(printk("%s:%d:, sip %x:%x:%x:%x, dip %x:%x:%x:%x, proto %d "
				"skb 0x%x\n", __func__, __LINE__,
				ip6h->saddr.s6_addr32[0], ip6h->saddr.s6_addr32[1],
				ip6h->saddr.s6_addr32[2], ip6h->saddr.s6_addr32[3],
				ip6h->daddr.s6_addr32[0], ip6h->daddr.s6_addr32[1], 
				ip6h->daddr.s6_addr32[2], ip6h->daddr.s6_addr32[3], 
				ip6h->nexthdr, skb));
		DBG(printk("da %x:%x:%x:%x:%x:%x eth proto 0x%x : 0x%x\n",
				cs_cb->input.raw.da[0], cs_cb->input.raw.da[1],
				cs_cb->input.raw.da[2], cs_cb->input.raw.da[3],
				cs_cb->input.raw.da[4], cs_cb->input.raw.da[5],
				*proto,cs_cb->input.raw.eth_protocol));

		if (*proto != 0xdd86)
			return;
		if (dir) { /* Tx */
			memcpy(cs_cb->input.l3_nh.ipv6h.dip, &ip6h->daddr, 16);
			memcpy(cs_cb->input.l3_nh.ipv6h.sip, &ip6h->saddr, 16);
			cs_cb->input.l3_nh.ipv6h.ver = ip6h->version;
			cs_cb->input.l3_nh.ipv6h.protocol = ip6h->nexthdr;
			if (ip6h->nexthdr == NEXTHDR_TCP) {
				struct tcphdr *tcph;
				tcph = (struct tcphdr *)((void *)ip6h + sizeof(struct ipv6hdr));
				cs_cb->input.l4_h.th.sport = tcph->source;
				cs_cb->input.l4_h.th.dport = tcph->dest;
				DBG(printk("%s:%d:tcp sp %d, dp %d\n", __func__, __LINE__,
						tcph->source, tcph->dest));
			} else if (ip6h->nexthdr == NEXTHDR_UDP) {
				struct udphdr *udph;
				udph = (struct udphdr *)((void *)ip6h + sizeof(struct ipv6hdr));
				cs_cb->input.l4_h.uh.sport = udph->source;
				cs_cb->input.l4_h.uh.dport = udph->dest;
				DBG(printk("%s:%d:udp sp %d, dp %d\n", __func__, __LINE__,
						udph->source, udph->dest));
			}
		} else {
			/* Rx */
			/* Overwrite output CB so that hash from RE gets created properly */
			memcpy(cs_cb->output.l3_nh.ipv6h.dip, &ip6h->daddr, 16);
			memcpy(cs_cb->output.l3_nh.ipv6h.sip, &ip6h->saddr, 16);
			cs_cb->output.l3_nh.ipv6h.ver = ip6h->version;
			cs_cb->output.l3_nh.ipv6h.protocol = ip6h->nexthdr;
			if (ip6h->nexthdr == NEXTHDR_TCP) {
				struct tcphdr *tcph;
				tcph = (struct tcphdr *)((void *)ip6h + sizeof(struct ipv6hdr));
				cs_cb->output.l4_h.th.sport = tcph->source;
				cs_cb->output.l4_h.th.dport = tcph->dest;
				DBG(printk("%s:%d:tcp sp %d, dp %d\n", __func__, __LINE__,
						tcph->source, tcph->dest));
			} else if (ip6h->nexthdr == NEXTHDR_UDP) {
				struct udphdr *udph;
				udph = (struct udphdr *)((void *)ip6h + sizeof(struct ipv6hdr));
				cs_cb->output.l4_h.uh.sport = udph->source;
				cs_cb->output.l4_h.uh.dport = udph->dest;
				DBG(printk("%s:%d:udp sp %d, dp %d\n", __func__, __LINE__,
						udph->source, udph->dest));
			}
			memcpy(&cs_cb->output.raw.da[0], skb->data, 6);
			memcpy(&cs_cb->output.raw.sa[0], (cs_uint8*)skb->data + 6, 6);
			cs_cb->output.raw.eth_protocol = *proto;
			DBG(printk("output sa %x:%x:%x:%x:%x:%x eth proto 0x%x\n",
					cs_cb->output.raw.sa[0],
					cs_cb->output.raw.sa[1],
					cs_cb->output.raw.sa[2],
					cs_cb->output.raw.sa[3],
					cs_cb->output.raw.sa[4],
					cs_cb->output.raw.sa[5],
					cs_cb->output.raw.eth_protocol));
			DBG(printk("output da %x:%x:%x:%x:%x:%x eth proto 0x%x\n",
					cs_cb->output.raw.da[0],
					cs_cb->output.raw.da[1],
					cs_cb->output.raw.da[2],
					cs_cb->output.raw.da[3],
					cs_cb->output.raw.da[4],
					cs_cb->output.raw.da[5],
					cs_cb->output.raw.eth_protocol));


		}
	} else { /* v4 not yet supported */
	}

}

EXPORT_SYMBOL(k_jt_cs_etherip_handler);

void cs_hw_accel_etherip_init(void) 
{

}

void cs_hw_accel_etherip_exit(void) 
{
}

