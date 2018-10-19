/******************************************************************************
     Copyright (c) 2010, Cortina Systems, Inc.  All rights reserved.

 ******************************************************************************
   Module      : cs_hw_accel_pppoe.c
   Date        : 2010-09-24
   Description : Process Cortina GoldenGate PPPoE Offload.
   Author      : Axl Lee <axl.lee@cortina-systems.com>
   Remarks     :

 *****************************************************************************/

#include <linux/spinlock.h>
#include <linux/if_pppox.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include "cs_hw_accel_manager.h"
#include "cs_core_hmu.h"
#include "cs_hw_accel_pppoe.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;

#define DBG(x) {if (cs_adapt_debug & CS752X_ADAPT_PPPOE) x;}
#else
#define DBG(x) {}
#endif /* CONFIG_CS752X_PROC */

// PPPoE queue definitions
typedef struct {
	unsigned long	sync;		/* 0xFFFFFFFE */
	unsigned long	seq_number;	/* For Debug */
	unsigned long	queue_addr;
} PPPOE_HACK_INFO;

typedef struct {
	unsigned char	used;	/* */
	unsigned char	hack_info[sizeof(PPPOE_HACK_INFO)];
	unsigned char	cs_cb[sizeof(cs_kernel_accel_cb_t)];
} PPPOE_HACK_QUEUE;

#define PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH	4096
#define PPPOE_HACK_INFO_SYNC		0xFEFFFFFF
#define PPPOE_HACK_OFFSET		16

unsigned long current_queue_index;
PPPOE_HACK_QUEUE pppoe_hack_queue[PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH];
unsigned long current_seq_number = 0;

unsigned char pppoe_queue_lock_init = 0;
spinlock_t pppoe_queue_lock;


// pppoe definitions for hmu watch
cs_core_hmu_t pppoe_hmu_entry;
cs_core_hmu_value_t pppoe_hmu_value;


/*
 * Local Function
 */
unsigned long get_empty_queue(void)
{
	unsigned long unused_queue_index = current_queue_index;
	int i;

	for (i = 0; i < PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH; i++) {
		if (pppoe_hack_queue[unused_queue_index].used == 0) {
			current_queue_index = unused_queue_index;
			return unused_queue_index;
		}
		unused_queue_index++;
		if (unused_queue_index == PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH)
			unused_queue_index = 0;
	}

	// FIXME:
	// Can't find empty queue
	memset(&(pppoe_hack_queue[0]), 0, (sizeof(PPPOE_HACK_QUEUE) *
			PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH));
	unused_queue_index = current_queue_index = 0;

	return PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH;
}/* get_empty_queue */

#define ISDIGIT(c)	((c) >= '0' && (c) <= '9')
unsigned char cs_strtouc(char* pStr)
{
	unsigned char uc = 0;
	unsigned char str_len = strlen(pStr);
	int i;

	if ((str_len == 0) || (str_len > 3))
		return 255;

	for (i = 0; i < str_len; i++) {
		uc *= 10;
		uc = uc + (pStr[i] - '0');
	}

	return uc;
}/* cs_strtouc */


int cs_pppoe_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_PPPOE_SERVER
					| CS752X_ADAPT_ENABLE_PPPOE_CLIENT);
}

int cs_pppoe_kernel_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_PPPOE_KERNEL);
}

void cs_pppoe_del_all_hwhash(void)
{
	DBG(printk("(%s, %d)\n", __func__, __LINE__));

	// delete hardware hash and nodes
	cs_core_hmu_clean_watch(&pppoe_hmu_entry);

	return;
}// cs_pppoe_del_all_hwhash()


/*
 * Callback Function
 */
void cs_pppoe_callback_enable(unsigned long notify_event, unsigned long value)
{
	DBG(printk("(%s) cs hw accel pppoe event %d\n", __func__, (int)notify_event));

	if ((notify_event == CS_HAM_ACTION_MODULE_DISABLE) ||
			(notify_event == CS_HAM_ACTION_CLEAN_HASH_ENTRY)) {
		// clear all pppoe hardware hash
		cs_pppoe_del_all_hwhash();
	}

	return;
} /* cs_pppoe_callback_enable */


int cs_pppoe_hmu_watch_callback(u32 watch_bitmask, cs_core_hmu_value_t *value,
		u32 status)
{
	DBG(printk("(%s, %d): watch_bitmask 0x%8.8x, status 0x%x\n",
			__func__, __LINE__ , watch_bitmask, status));

	return 0;
} /* cs_pppoe_hmu_watch_callback */


/*
 * Global Function
 */

void cs_pppoe_skb_recv_hook(struct sk_buff *skb, u16 pppoe_session_id,
		u8 direction)
{
	unsigned char *pPayload;
	u8 *pwr;
	u16 protocol, ptype;
	u16 hack_position;
	PPPOE_HACK_INFO *pPPPoE_hack_info;
	unsigned long unused_queue_index = PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct pppoe_hdr *ph;
	struct iphdr *iph;
	unsigned long flag;

	if (!cs_cb)
		return;

	if (cs_cb->common.tag != CS_CB_TAG)
		return;

	if (cs_cb->common.sw_only & CS_SWONLY_STATE)
		return;

	if (!cs_pppoe_enable()) {
		DBG(printk("### Warning: PPPoE adaption Disabled\n"));
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}

	if (skb->len < 32) {
		DBG(printk("(%s, %d), %s, skb->len %d\n", __func__, __LINE__,
				((direction == 0)? "DIR_LAN2WAN":"DIR_WAN2LAN"),
				skb->len));
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}

	switch (direction) {
	case DIR_LAN2WAN:
		// FIXME: Temp. solution. For pppoe-client only
		// Maybe this packet send by pppoe-client?
		if (skb->len > 14) {
			protocol = *((u16*)(skb->mac_header + 12));
			if (protocol == htons(ETH_P_PPP_SES))
				return;
		}

		cs_cb->common.module_mask |= CS_MOD_MASK_PPPOE;
//		printk("~~~ (%s, %d): DIR_LAN2WAN, dev->name %s\n",
//				__func__, __LINE__, skb->dev->name);
		//if (strncmp(skb->dev->name, "ppp", 3) == 0)
		//	cs_cb->output.raw.ppp_interface =
		//		cs_strtouc(skb->dev->name + 3);

		spin_lock_irqsave(&pppoe_queue_lock, flag);
		unused_queue_index = get_empty_queue();
		DBG(printk("--> (%s, %d): DIR_LAN2WAN, unused_queue_index %d\n",
				__func__, __LINE__, (int)unused_queue_index));

		if (unused_queue_index < PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH) {
			pPayload = skb->tail - PPPOE_HACK_OFFSET;

			/* Fill Queue Info */
			/* keep packet info in queue */
			memcpy(pppoe_hack_queue[unused_queue_index].hack_info,
					pPayload, sizeof(PPPOE_HACK_INFO));
			/* copy skb->cs_cb to queue */
			memcpy(pppoe_hack_queue[unused_queue_index].cs_cb,
					(char *)cs_cb,
					sizeof(cs_kernel_accel_cb_t));

			/* Fill queue info to packet */
			pPPPoE_hack_info = (PPPOE_HACK_INFO*)pPayload;
			pPPPoE_hack_info->sync = PPPOE_HACK_INFO_SYNC;
			pPPPoE_hack_info->seq_number = current_seq_number++;
			pPPPoE_hack_info->queue_addr = (unsigned long)
				(&(pppoe_hack_queue[unused_queue_index]));
			pppoe_hack_queue[unused_queue_index].used = 1;
		}
		spin_unlock_irqrestore(&pppoe_queue_lock, flag);
		break;

	case DIR_WAN2LAN:
		if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_PPPOE_SERVER)) {
			//FIXME: For demo only, pppoe-server
			if ((skb->len > 14) && (skb->len < 70)) {
				protocol = *((u16*)(skb->mac_header + 12));
				if (protocol == htons(ETH_P_PPP_SES))
					return;
			}
		} //if (cs_adapt_debug & CS752X_ADAPT_PPPOE)

		cs_cb->common.module_mask |= CS_MOD_MASK_PPPOE;
		cs_cb->input.raw.pppoe_frame = pppoe_session_id;
		cs_cb->input.raw.pppoe_frame_vld = 1;

		// TODO: It needs to add ppp interface infomation into
		// cs_cb->input.raw.ppp_interface. This is for gc.

		spin_lock_irqsave(&pppoe_queue_lock, flag);
		unused_queue_index = get_empty_queue();
		DBG(printk("--> (%s, %d): DIR_WAN2LAN, unused_queue_index %d, "
				"0x%4.4x\n", __func__, __LINE__,
				(int)unused_queue_index,
				(int)&pppoe_hack_queue[unused_queue_index]));

		if (unused_queue_index < PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH) {
			pPayload = skb->tail - PPPOE_HACK_OFFSET;

			/* Workaround for small size IPv4/UDP packet */
			if (skb->len == ETH_ZLEN) {
				if (skb->network_header) {
					// point to next header
					pwr = (u8*)(skb->network_header + 6);
					ptype = *((u16*)pwr);
					// skip 802.1Q header
					if (ptype == htons(ETH_P_8021Q)) {
						pwr += 4;
						ptype = *((u16*)pwr);
					}
					if (ptype == 0x2100) {
						pwr += 2;
						iph = (struct iphdr*)pwr;
						if (iph->protocol == IPPROTO_UDP) {
							ph = pppoe_hdr(skb);
							hack_position = ETH_ZLEN - (ETH_ALEN * 2)
									- PPPOE_SES_HLEN
									- htons(ph->length)
									+ PPPOE_HACK_OFFSET;
							pPayload = skb->tail - hack_position;
						}
					}
				} else {
					DBG(printk("### Warning (%s, %d): Should "
							"NOT be occured\n",
							__func__, __LINE__));
				}
			}

			/* Fill Queue Info */
			/* keep packet info in queue */
			memcpy(pppoe_hack_queue[unused_queue_index].hack_info,
					pPayload, sizeof(PPPOE_HACK_INFO));
			/* copy skb->cs_cb to queue */
			memcpy(pppoe_hack_queue[unused_queue_index].cs_cb,
					(char *)cs_cb,
					sizeof(cs_kernel_accel_cb_t));

			/* Fill queue info to packet */
			pPPPoE_hack_info = (PPPOE_HACK_INFO*)pPayload;
			pPPPoE_hack_info->sync = PPPOE_HACK_INFO_SYNC;
			pPPPoE_hack_info->seq_number = current_seq_number++;
			pPPPoE_hack_info->queue_addr = (unsigned long)
				(&(pppoe_hack_queue[unused_queue_index]));

			pppoe_hack_queue[unused_queue_index].used = 1;
		}
		spin_unlock_irqrestore(&pppoe_queue_lock, flag);
		break;
	} // switch (direction)

	return;
} /* cs_pppoe_skb_recv_hook */
EXPORT_SYMBOL(cs_pppoe_skb_recv_hook);

void cs_pppoe_skb_xmit_hook(struct sk_buff *skb, u16 pppoe_session_id,
		u8 direction)
{
	unsigned char *pPayload;
	PPPOE_HACK_INFO *pPPPoE_hack_info;
	PPPOE_HACK_QUEUE *ppppoe_hack_queue;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	unsigned long flag;

	/*for skb->len < 32, there is no hack information*/
	if (skb->len < 32) {
		DBG(printk("(%s, %d), %s, skb->len %d\n", __func__, __LINE__,
				((direction == 0)? "DIR_LAN2WAN": "DIR_WAN2LAN"),
				skb->len));
		//cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}

	/* get the PPPOE_HACK_INFO from packet */
	pPayload = skb->tail - PPPOE_HACK_OFFSET;

	pPPPoE_hack_info = (PPPOE_HACK_INFO*)pPayload;
	/* check SYNC */
	if (pPPPoE_hack_info->sync == PPPOE_HACK_INFO_SYNC) {
		spin_lock_irqsave(&pppoe_queue_lock, flag);
		/* get the cs_cb queue pointer */
		ppppoe_hack_queue = (PPPOE_HACK_QUEUE*)
			(pPPPoE_hack_info->queue_addr);
		/* recover original data to packet */
		memcpy(pPayload, ppppoe_hack_queue->hack_info,
				sizeof(PPPOE_HACK_INFO));
		/* recover cs_cb */
		if (cs_cb == NULL) {
			if (cs_accel_cb_add(skb) != 0) {
				ppppoe_hack_queue->used = 0;
				spin_unlock_irqrestore(&pppoe_queue_lock, flag);
				return ;
			}
			cs_cb = CS_KERNEL_SKB_CB(skb);
		}

		memcpy((char *)cs_cb, ppppoe_hack_queue->cs_cb,
					sizeof(cs_kernel_accel_cb_t));

		spin_unlock_irqrestore(&pppoe_queue_lock, flag);
	} else {
		DBG(printk("### Warning (%s, %d): pPPPoE_hack_info->sync "
			"0x%8.8x\n", __func__, __LINE__,
			(int)pPPPoE_hack_info->sync));
		return;
	}

	if (!cs_pppoe_enable()) {
		DBG(printk("### Warning: PPPoE adaption Disabled\n"));
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}

	DBG(printk("<-- (%s, %d): direction %d, pppoe_session_id 0x%4.4x\n",
			__func__, __LINE__, (int)direction,
			pppoe_session_id));

	if (cs_cb->common.tag != CS_CB_TAG) {
		return;
	}
	if (cs_cb->common.sw_only & CS_SWONLY_STATE) {
		return;
	}

	switch (direction) {
	case DIR_LAN2WAN:
		/* set pppoe output parameter to cs_cb */
		cs_cb->output_mask |= CS_HM_PPPOE_SESSION_ID_MASK;
		cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_INSERT;
		cs_cb->output.raw.pppoe_frame = pppoe_session_id;

		cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;

		spin_lock_irqsave(&pppoe_queue_lock, flag);
		ppppoe_hack_queue->used = 0;
		spin_unlock_irqrestore(&pppoe_queue_lock, flag);

		break;

	case DIR_WAN2LAN:
		/* set pppoe output parameter to cs_cb */
		cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_REMOVE;
		cs_cb->output.raw.pppoe_frame = 0;

		cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;

		spin_lock_irqsave(&pppoe_queue_lock, flag);
		ppppoe_hack_queue->used = 0;
		spin_unlock_irqrestore(&pppoe_queue_lock, flag);

		break;
	} // switch (direction)

	return;
} /* cs_pppoe_skb_xmit_hook */
EXPORT_SYMBOL(cs_pppoe_skb_xmit_hook);


void cs_pppoe_del_hash_hook(char *pppoe_dev_name)
{
	DBG(printk("(%s, %d): %s\n", __func__, __LINE__ , pppoe_dev_name));

	//FIXME: Should not be deleted all
	cs_pppoe_del_all_hwhash();
	return;
} /* cs_pppoe_del_hash_hook */
EXPORT_SYMBOL(cs_pppoe_del_hash_hook);

int cs_pppoe_kernel_set_input_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth;
	struct pppoe_hdr *ph = NULL;

	if (!cs_cb)
		return -1;

	if (cs_cb->common.sw_only == CS_SWONLY_STATE)
		return -1;

	eth = (struct ethhdr *)skb->data;

	if (eth == NULL)
		return -1;

	/*
	 * get input pppoe header
	 */

	if (cs_cb->input.raw.eth_protocol == htons(ETH_P_PPP_SES)) {
		if (cs_pppoe_kernel_enable() == 0) {
			if (cs_pppoe_enable() == 0) {
				cs_cb->common.sw_only = CS_SWONLY_STATE;
			}
			return -1;
		}
		if (cs_cb->input.raw.vlan_tpid != 0) {
			if (cs_cb->input.raw.vlan_tpid_2 != 0)
				ph = (struct pppoe_hdr *)((u8 *)(eth) + ETH_HLEN + VLAN_HLEN * 2);
			else
				ph = (struct pppoe_hdr *)((u8 *)(eth) + ETH_HLEN + VLAN_HLEN);
		} else
			ph = (struct pppoe_hdr *)((u8 *)(eth) + ETH_HLEN);

		DBG(printk("%s() pppoe session id 0x%hx\n",
			__func__, ph->sid));

		cs_cb->common.module_mask |= CS_MOD_MASK_PPPOE;
		cs_cb->input.raw.pppoe_frame = ph->sid;
		cs_cb->input.raw.pppoe_frame_vld = 1;
		cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_REMOVE;
	}

	return 0;
}


int  cs_pppoe_kernel_set_output_cb(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth;
	struct pppoe_hdr *ph = NULL;

	if (!cs_cb)
		return -1;

	eth = (struct ethhdr *)skb->data;
	if (eth == NULL)
		return -1;

	/*
	 * get input pppoe header
	 */

	if (cs_cb->output.raw.eth_protocol == htons(ETH_P_PPP_SES)){

		if (cs_pppoe_kernel_enable() == 0) {
			if (cs_pppoe_enable() == 0) {
				cs_cb->common.sw_only = CS_SWONLY_STATE;
			}
			return -1;
		}
		if (cs_cb->output.raw.vlan_tpid != 0) {
			if (cs_cb->output.raw.vlan_tpid_2 != 0)
				ph = (struct pppoe_hdr *)((u8 *)(eth) + ETH_HLEN + VLAN_HLEN * 2);
			else
				ph = (struct pppoe_hdr *)((u8 *)(eth) + ETH_HLEN + VLAN_HLEN);
		} else
			ph = (struct pppoe_hdr *)((u8 *)(eth) + ETH_HLEN);

		DBG(printk("%s() pppoe session id 0x%hx cs_cb eth protocol=0x%hx eth protocol=0x%hx\n",
					__func__, ph->sid, cs_cb->output.raw.eth_protocol,
					eth->h_proto));

		cs_cb->common.module_mask |= CS_MOD_MASK_PPPOE;
		cs_cb->output.raw.pppoe_frame = ph->sid;
		if (cs_cb->input.raw.pppoe_frame_vld) {
			if (cs_cb->input.raw.pppoe_frame != cs_cb->output.raw.pppoe_frame) {
				DBG(printk("%s() HW NE doesn't support pppoe id switch from id 0x%hx to 0x%hx\n",
					__func__, cs_cb->input.raw.pppoe_frame, cs_cb->output.raw.pppoe_frame));
				cs_cb->common.sw_only = CS_SWONLY_STATE;
				return -1;
			} else {
				cs_cb->output_mask &= ~CS_HM_PPPOE_SESSION_ID_MASK;
				cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_NO_ENABLE;
			}
		} else {
			cs_cb->output_mask |= CS_HM_PPPOE_SESSION_ID_MASK;
			cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_INSERT;
		}
	}

	return 0;
}


void cs_pppoe_init(void)
{
	/* spin lock init */
	if (!pppoe_queue_lock_init) {
		spin_lock_init(&pppoe_queue_lock);
		pppoe_queue_lock_init = 1;
	}

	/* Queue init */
	current_queue_index = 0;
	memset(&(pppoe_hack_queue[0]), 0, (sizeof(PPPOE_HACK_QUEUE) *
				PPPOE_KERNEL_ADAPTION_QUEUE_DEPTH));

	/* Register PPPoE kernel adapter callback function */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_PPPOE_CLIENT,
			cs_pppoe_callback_enable);

	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_PPPOE_KERNEL,
				cs_pppoe_callback_enable);

	/* Register PPPoE HMU watch callback function */
	memset(&pppoe_hmu_entry, 0, sizeof(cs_core_hmu_t));
	memset(&pppoe_hmu_value, 0, sizeof(cs_core_hmu_value_t));

	pppoe_hmu_value.type = CS_CORE_HMU_WATCH_PPPOE_ID;
	pppoe_hmu_value.value.pppoe_session_id = 0;
	pppoe_hmu_value.mask = 0;
	pppoe_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_PPPOE_ID;
	pppoe_hmu_entry.value_mask = &pppoe_hmu_value;
	pppoe_hmu_entry.callback = cs_pppoe_hmu_watch_callback;
	cs_core_hmu_register_watch(&pppoe_hmu_entry);

	return;
} /* cs_pppoe_init */


void cs_pppoe_exit(void)
{
	/* Unregister PPPoE kernel adapter callback function */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_PPPOE_CLIENT,
			NULL);

	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_PPPOE_KERNEL,
			NULL);

	/* Unregister PPPoE HMU watch callback function */
	cs_core_hmu_unregister_watch(&pppoe_hmu_entry);

	return;
} /* cs_pppoe_exit */

