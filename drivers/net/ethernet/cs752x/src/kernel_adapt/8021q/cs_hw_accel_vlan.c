/******************************************************************************
     Copyright (c) 2010, Cortina Systems, Inc.  All rights reserved.

 ******************************************************************************
   Module      : cs_hw_accel_vlan.c
   Date        : 2012-02-23
   Description : Process Cortina GoldenGate VLAN Offload.
   Author      : Bird Hsieh <bird.hsieh@cortina-systems.com>
   Remarks     :

 *****************************************************************************/
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <../net/8021q/vlan.h>

#include "cs_hw_accel_manager.h"
#include "cs_core_logic_data.h"
#include "cs_core_logic.h"
#include "cs_core_hmu.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;

#define DBG(x) {if (cs_adapt_debug & CS752X_ADAPT_8021Q) x;}

#else
#define DBG(x) {}
#endif

void cs_vlan_del_hash(void)
{
	cs_hw_accel_mgr_delete_flow_based_hash_entry();
}

void cs_vlan_callback_ham_notify(unsigned long notify_event,
					unsigned long value)
{
	DBG(printk("%s() cs hw accel vlan event %ld\n", __func__,
			notify_event));
	/* notify_event == CS_HAM_ACTION_CLEAN_HASH_ENTRY -
	 * vlan hash entry is based on the other hw accel modules*/
	if ((notify_event == CS_HAM_ACTION_MODULE_DISABLE) ||
			(notify_event == CS_HAM_ACTION_MODULE_ENABLE))
		cs_vlan_del_hash();
}

int cs_vlan_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_8021Q);
}

int cs_vlan_set_input_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth;
	struct vlan_ethhdr *vlan_eth_hdr;
	int l2_hdr_len = ETH_HLEN;

	if (!cs_cb)
		return -1;

	eth = (struct ethhdr *)skb->data;

	if (eth == NULL)
		return -1;

	/*
	 * get input vlan tci
	 */
	cs_cb->input.raw.eth_protocol = eth->h_proto;
	if ((cs_cb->input.raw.eth_protocol == htons(ETH_P_8021Q)) ||
			(cs_cb->input.raw.eth_protocol == htons(0x88a8)) ||
			(cs_cb->input.raw.eth_protocol == htons(0x9100)) ||
			(cs_cb->input.raw.eth_protocol == htons(0x9200))) {
		if ((cs_cb->input.raw.eth_protocol != htons(ETH_P_8021Q)) &&
			(cs_cb->input.raw.eth_protocol != htons(0x88a8)))
			cs_cb->common.sw_only = CS_SWONLY_STATE;

		vlan_eth_hdr = (struct vlan_ethhdr *)eth;
		l2_hdr_len += VLAN_HLEN;

		cs_cb->input.raw.vlan_tpid = ntohs(cs_cb->input.raw.eth_protocol);
		cs_cb->input.raw.vlan_tci = ntohs(vlan_eth_hdr->h_vlan_TCI);
		cs_cb->input.raw.eth_protocol = vlan_eth_hdr->h_vlan_encapsulated_proto;

		/*DBL Tag*/
		if (cs_cb->input.raw.eth_protocol == htons(ETH_P_8021Q)) {
			vlan_eth_hdr = (struct vlan_ethhdr *) ((u8 *)(eth) + VLAN_HLEN);
			cs_cb->input.raw.vlan_tpid_2 = ntohs(vlan_eth_hdr->h_vlan_proto);
			cs_cb->input.raw.eth_protocol = vlan_eth_hdr->h_vlan_encapsulated_proto;
			cs_cb->input.raw.vlan_tci_2 = ntohs(vlan_eth_hdr->h_vlan_TCI);
			l2_hdr_len += VLAN_HLEN;
			/* Double tagged p-bits remarking */
			uint16_t new_tci;
			new_tci = (cs_cb->input.raw.vlan_tci & VLAN_PRIO_MASK) | (cs_cb->input.raw.vlan_tci_2 & ~VLAN_PRIO_MASK);
			vlan_eth_hdr->h_vlan_TCI = htons(new_tci);
		}

		/*3rd Tag*/
		if (cs_cb->input.raw.eth_protocol == htons(ETH_P_8021Q)) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}

		if (cs_vlan_enable() == 0)
			cs_cb->common.sw_only = CS_SWONLY_STATE;
	}
	struct iphdr *iph = skb->data + l2_hdr_len;
	cs_cb->input.l3_nh.iph.tos = iph->tos;
	return 0;
}

int cs_vlan_set_output_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth;
	struct vlan_ethhdr *vlan_eth_hdr;
	int l2_hdr_len = ETH_HLEN;

	if (!cs_cb)
		return -1;

	/* cs_cb->common.tag Tag check already in
	 * cs_core_logic_add_connections() */

	if (memcmp(cs_cb->input.raw.da, skb->data, ETH_ALEN) != 0) {
		cs_cb->output_mask |= CS_HM_MAC_DA_MASK;
	}
	memcpy(cs_cb->output.raw.da, skb->data, ETH_ALEN);

	if (memcmp(cs_cb->input.raw.sa,skb->data + ETH_ALEN, ETH_ALEN) != 0) {
		cs_cb->output_mask |= CS_HM_MAC_SA_MASK;
	}
	memcpy(cs_cb->output.raw.sa, skb->data + ETH_ALEN, ETH_ALEN);

	eth = (struct ethhdr *)skb->data;
	if (eth == NULL)
		return 0;

	/* get output vlan tci */
	cs_cb->output.raw.eth_protocol = eth->h_proto;
	if ((cs_cb->output.raw.eth_protocol == htons(ETH_P_8021Q)) ||
			(cs_cb->output.raw.eth_protocol == htons(0x88a8)) ||
			(cs_cb->output.raw.eth_protocol == htons(0x9100)) ||
			(cs_cb->output.raw.eth_protocol == htons(0x9200))) {

		vlan_eth_hdr = (struct vlan_ethhdr *)eth;
		cs_cb->output.raw.vlan_tpid = ntohs(vlan_eth_hdr->h_vlan_proto);
		cs_cb->output.raw.eth_protocol = vlan_eth_hdr->h_vlan_encapsulated_proto;
		cs_cb->output.raw.vlan_tci = ntohs(vlan_eth_hdr->h_vlan_TCI);
		l2_hdr_len += VLAN_HLEN;

		/*DBL Tag*/
		if (cs_cb->output.raw.eth_protocol == htons(ETH_P_8021Q)) {
			vlan_eth_hdr = (struct vlan_ethhdr *) ((u8 *)(eth) + VLAN_HLEN);
			cs_cb->output.raw.vlan_tpid_2 = ntohs(vlan_eth_hdr->h_vlan_proto);
			cs_cb->output.raw.eth_protocol = vlan_eth_hdr->h_vlan_encapsulated_proto;
			cs_cb->output.raw.vlan_tci_2 = ntohs(vlan_eth_hdr->h_vlan_TCI);
			/* HW not support the following vlan change, so set SW only
			 * input 1st vid<>0 , 2nd vid=0 ==> vid<>0, vid<>0
			 */
			l2_hdr_len += VLAN_HLEN;
			if  ((cs_cb->input.raw.vlan_tpid_2 != 0) &&
				 ((cs_cb->input.raw.vlan_tci_2 & VLAN_VID_MASK) == 0)) {
				 if ((cs_cb->output.raw.vlan_tci_2 & VLAN_VID_MASK) != 0) {
				 	cs_cb->common.sw_only = CS_SWONLY_STATE;
					return -1;
				 }
			}
		}

		/*3rd Tag*/
		if (cs_cb->output.raw.eth_protocol == htons(ETH_P_8021Q)) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}

		if (cs_vlan_enable() == 0) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}
	}

	struct iphdr *iph = skb->data + l2_hdr_len;
	cs_cb->output.l3_nh.iph.tos = iph->tos;
	if (cs_cb->input.l3_nh.iph.tos != cs_cb->output.l3_nh.iph.tos) {
	cs_cb->output_mask |= CS_HM_DSCP_MASK;
	cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
	}

	if ((cs_cb->input.raw.vlan_tpid != cs_cb->output.raw.vlan_tpid) ||
		((cs_cb->input.raw.vlan_tci & VLAN_VID_MASK) !=
			(cs_cb->output.raw.vlan_tci & VLAN_VID_MASK))) { /*VLAN ID changed*/
		cs_cb->common.module_mask |= CS_MOD_MASK_VLAN;
		cs_cb->output_mask |= CS_HM_VID_1_MASK;
	}

	if ((cs_cb->input.raw.vlan_tpid_2 != cs_cb->output.raw.vlan_tpid_2) ||
			((cs_cb->input.raw.vlan_tci_2 & VLAN_VID_MASK) !=
				(cs_cb->output.raw.vlan_tci_2 & VLAN_VID_MASK))) { /*VLAN ID changed*/
			cs_cb->common.module_mask |= CS_MOD_MASK_VLAN;
			cs_cb->output_mask |= CS_HM_VID_2_MASK;
	}
	DBG(printk("%s input %x %x output %x %x mask %x tos %x %x\n", __func__, cs_cb->input.raw.vlan_tpid, cs_cb->input.raw.vlan_tpid_2,
		cs_cb->output.raw.vlan_tpid, cs_cb->output.raw.vlan_tpid_2,
		cs_cb->output_mask, cs_cb->input.l3_nh.iph.tos, cs_cb->output.l3_nh.iph.tos));

	if (cs_cb->output.raw.vlan_tpid != 0) {
		if ((cs_cb->input.raw.vlan_tci & VLAN_PRIO_MASK)
			!= (cs_cb->output.raw.vlan_tci & VLAN_PRIO_MASK)) {
			cs_cb->output_mask |= CS_HM_8021P_1_MASK;
			cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
		}

		if ((cs_cb->input.raw.vlan_tci & VLAN_CFI_MASK)
			!= (cs_cb->output.raw.vlan_tci & VLAN_CFI_MASK)) {
			cs_cb->output_mask |= CS_HM_DEI_1_MASK;
			cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
		}
	}

	if (cs_cb->output.raw.vlan_tpid_2 != 0) {
		if ((cs_cb->input.raw.vlan_tci_2 & VLAN_PRIO_MASK)
			!= (cs_cb->output.raw.vlan_tci_2 & VLAN_PRIO_MASK)) {
			cs_cb->output_mask |= CS_HM_8021P_2_MASK;
			cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
		}

		if ((cs_cb->input.raw.vlan_tci_2 & VLAN_CFI_MASK)
			!= (cs_cb->output.raw.vlan_tci_2 & VLAN_CFI_MASK)) {
			cs_cb->output_mask |= CS_HM_DEI_2_MASK;
			cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
		}
	}

	return 0;
} /*cs_eth_set_output_cb*/

int cs_vlan_init(void)
{
	/*for hw_accel_manager */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_8021Q,
					       cs_vlan_callback_ham_notify);

	return 0;
}

int cs_vlan_exit(void)
{
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_8021Q,
					       NULL);

	return 0;
}

void cs_vlan_print(const char *fun_name, struct net_device *dev)
{
	if (cs_adapt_debug & CS752X_ADAPT_8021Q) {
		if (dev == NULL) {
			printk("%s dev is NULL \n", __func__);
			return;
		}

		printk("%s dev %s \n", __func__, dev->name);
	}
}

void k_jt_cs_vlan_ioctl_handler_add_vlan(struct net_device *real_dev, u16 vlan_id)
{
	cs_vlan_print(__func__, real_dev);
	cs_vlan_del_hash();
}

void k_jt_cs_vlan_ioctl_handler_del_vlan(struct net_device *dev)
{
	cs_vlan_print(__func__, dev);
	cs_vlan_del_hash();
}

EXPORT_SYMBOL(k_jt_cs_vlan_ioctl_handler_add_vlan);
EXPORT_SYMBOL(k_jt_cs_vlan_ioctl_handler_del_vlan);
