/******************************************************************************
     Copyright (c) 2010, Cortina Systems, Inc.  All rights reserved.

 ******************************************************************************
   Module      : cs_hw_accel_dscp.c
   Date        : 2010-09-24
   Description : Process Cortina GoldenGate DSCP Offload.
   Author      : Axl Lee <axl.lee@cortina-systems.com>
   Remarks     :

 *****************************************************************************/

#include <linux/spinlock.h>
#include <linux/if_pppox.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/export.h>
#include <linux/ppp_defs.h>
#include "cs_hw_accel_manager.h"
#include "cs_core_hmu.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;

#define DBG(x) {if (cs_adapt_debug & CS752X_ADAPT_DSCP) x;}
#else
#define DBG(x) {}
#endif /* CONFIG_CS752X_PROC */


int cs_dscp_set_input_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth = NULL;
	struct iphdr *iph = NULL;
	struct iphdr6 *iph6 = NULL;
	struct pppoe_hdr *ph = NULL;
	int iph_off = 0;

	if (!cs_cb)
		return -1;

	if (cs_cb->common.sw_only == CS_SWONLY_STATE)
		return -1;

	if ((cs_cb->input.raw.eth_protocol != htons(ETH_P_IP)) &&
		(cs_cb->input.raw.eth_protocol != htons(ETH_P_IPV6)) &&
		(cs_cb->input.raw.eth_protocol != htons(ETH_P_PPP_SES))) {
		return 0;
	}

 	eth = (struct ethhdr *)skb->data;

	/*
	 * get input DSCP
	 */
	iph_off = ETH_HLEN;

    if (cs_cb->input.raw.vlan_tpid != 0) {
        iph_off += VLAN_HLEN;
    }
    if (cs_cb->input.raw.vlan_tpid_2 != 0) {
        iph_off += VLAN_HLEN;
    }

	if (cs_cb->input.raw.eth_protocol == htons(ETH_P_PPP_SES)) {
		ph = (struct pppoe_hdr *)((u8 *)(eth) + iph_off);
		if ((ph->tag[0].tag_type != htons(PPP_IP)) && (ph->tag[0].tag_type != htons(PPP_IPV6)))
			return 0;
	}
	if (cs_cb->input.raw.pppoe_frame_vld == 1) {
		iph_off += PPPOE_SES_HLEN;
	}

    iph = (struct iphdr *)((u8 *)(eth) + iph_off);
    if (iph == NULL) {
        cs_cb->common.sw_only = CS_SWONLY_STATE;
		return -1;
    }


    // IPv4/IPv6
    cs_cb->input.l3_nh.iph.ver = iph->version;
    cs_cb->output.l3_nh.iph.ver = iph->version;
    if (iph->version == 4) {
        DBG(printk("%s:%d:: iph->version 0x%x, iph->tos 0x%x\n", __func__, __LINE__, iph->version, iph->tos));
        cs_cb->input.l3_nh.iph.tos = iph->tos;
        cs_cb->common.module_mask |= CS_MOD_MASK_DSCP;
    } else if (iph->version == 6) {
        iph6 = (struct iphdr6 *)((u8 *)(eth) + iph_off);
        // Doesn't need to support TC
    }

	return 0;
} /* cs_dscp_set_input_cb() */
EXPORT_SYMBOL(cs_dscp_set_input_cb);



int cs_dscp_set_output_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (!cs_cb)
		return -1;

	/*
	 * Set output mask
	 */
//	if (cs_cb->common.module_mask & CS_MOD_MASK_DSCP){
//		cs_cb->output_mask |= CS_HM_DSCP_MASK;
//	}

	return 0;
} /* cs_dscp_set_output_cb() */
EXPORT_SYMBOL(cs_dscp_set_output_cb);
