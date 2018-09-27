
#include <asm/io.h>
#include <linux/etherhook.h>
#include <linux/etherdevice.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <net/ip.h>
#include <cs752x_eth.h>
#include <mach/cs75xx_pni.h>

extern int cs_pni_xmit_none_bypass_ne(u8 instance, u16 lspid, u16 recirc_idx, u32 buf0, int len0,
									   u32 buf1, int len1, u32 buf2, int len2, void * data);

extern int cs_pni_register_chip_callback_xmit(u8 chip_type, int instance,
											  void* adapter, u16 (*cb) ,
											  u16 (*cb_8023) , u16 (*cb_xmit_done));

/*
 * The currently registed handler/hook
 */
static struct etherhook_handler *hook = NULL;

/*
 * Queue of skbs deferred for the bottom half
 * processing in a separate tasklet
 */
static struct sk_buff_head rx_queue;

/*
 * Tasklet for deferred processing of packets
 */
void deferred_handle_rx(unsigned long);
DECLARE_TASKLET(rx_tasklet, deferred_handle_rx, 0);

/**
 * dispatch_skb - prepare and dispatch a single skb to the hook
 *
 * If the hook returns 0, it means is has taken ownership of the skb,
 * and is responsible for free'ing it (kfree_skb).
 */
void dispatch_skb(struct sk_buff* skb)
{
	int ret = 0;
	__be16 protocol = 0;

	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	skb_reset_mac_len(skb);

	protocol = skb->protocol;
	if (protocol == cpu_to_be16(ETH_P_8021Q)) {
		protocol = vlan_eth_hdr(skb)->h_vlan_encapsulated_proto;
	}

	if (protocol == cpu_to_be16(ETH_P_IP)) {
		/*
		 * We only expect IP packets, hence pull to the beginning
		 * of the IP datagram (similar to the IP stack itself)
		 */
		skb_pull(skb, ip_hdrlen(skb));
		skb_reset_transport_header(skb);

		/*
		 * If handle_rx returns zero (success), we assume it has taken
		 * ownership of the socket buffer.
		 */
		ret = hook->handle_rx(skb);
		if (ret == 0) {
			return;
		}
	}
	dev_kfree_skb(skb);
}

/**
 * deferred_handle_rx - dispatch all queued skbs to the hook
 */
void deferred_handle_rx(unsigned long data)
{
	unsigned long flags;
	struct sk_buff_head q;
	struct sk_buff *skb;

	__skb_queue_head_init(&q);
	spin_lock_irqsave(&rx_queue.lock, flags);
	skb_queue_splice_init(&rx_queue, &q);
	spin_unlock_irqrestore(&rx_queue.lock, flags);

	while ((skb = __skb_dequeue(&q))) {
		dispatch_skb(skb);
	}
}

/**
 * etherhook_rx_skb - receive a single socket buffer from the driver
 *
 * The skb is received directly from the ethernet driver, and expects
 * the data pointer to be pointing at the begining of ethernet header.
 * The preprocessed skb is queue for deferred processing in a tasklet.
 *
 * Prior to queueing the skb, the correct ethernet protocol is set
 * hould always be IP), and the the data pointer is pulled to point
 * at the begining of the IP datagram (e.g. UDP header).
 */
int etherhook_rx_skb(struct sk_buff* skb)
{
	int ret = 0;

	if (unlikely(!hook)) {
		pr_debug("etherhook_rx_skb: no hook\n");
		ret = -EIO;
		goto error;
	}

	if (!pskb_may_pull(skb, ETH_HLEN)) {
		pr_debug("etherhook_rx_skb: invalid eth header\n");
		ret = -EIO;
		goto error;
	}

	skb->protocol = eth_type_trans(skb, skb->dev);
	if (skb->protocol == cpu_to_be16(ETH_P_8021Q)) {
	   	skb_pull(skb, VLAN_HLEN);
	}

	skb_queue_tail(&rx_queue, skb);
	tasklet_hi_schedule(&rx_tasklet);
	goto out;

error:
	dev_kfree_skb(skb);
out:
	return ret;
}
EXPORT_SYMBOL(etherhook_rx_skb);

/**
 * etherhook_tx - transmit buffers aggregated into a single packet
 *
 * All pointers to data must be given as physical addresses, hence must
 * data must to virt_to_phys before passed to this function. Further more,
 * data must be DMA mapped.
 */
int etherhook_tx(u32 buf0, int len0, u32 buf1, int len1, u32 buf2, int len2, void *arg)
{
	return cs_pni_xmit_none_bypass_ne(1, 1, 0, buf0, len0, buf1, len1, buf2, len2, arg);
}
EXPORT_SYMBOL(etherhook_tx);

/**
 * etherhook_register - register a hook/handler
 *
 * Only one hook/handler may registered simultaniously.
 * TODO: Do we need to register more hooks for different voqs?
 */
int etherhook_register(struct etherhook_handler* handler)
{
	if (hook) {
		pr_debug("etherhook_register: busy\n");
		return -EBUSY;
	}
	if (!handler) {
		pr_debug("etherhook_register: null handler\n");
		return -EINVAL;
	}
	pr_info("etherhook_register: sw_action: %d\n", handler->sw_action);
	hook = handler;
	return 0;
}
EXPORT_SYMBOL(etherhook_register);

int etherhook_unregister(void)
{
	pr_info("etherhook_unregister\n");
	hook = NULL;
	return 0;
}
EXPORT_SYMBOL(etherhook_unregister);

int etherhook_has_hook(u32 sw_action)
{
	return hook != NULL && hook->sw_action == sw_action;
}
EXPORT_SYMBOL(etherhook_has_hook);

static u16 etherhook_after_xmit_cb(void *adapter, struct sk_buff *arg)
{
	if (likely(hook->after_xmit_cb)) {
		return hook->after_xmit_cb((void*) arg);
   	}
	return 0;
}

/**
 * etherhook_exit
 */
static void __exit etherhook_exit(void)
{
	tasklet_kill(&rx_tasklet);
	pr_info("etherhook: exit\n");
}

/**
 * etherhook_init
 */
static int __init etherhook_init(void)
{
	skb_queue_head_init(&rx_queue);
	cs_pni_register_chip_callback_xmit(0, 1, NULL, NULL, NULL, etherhook_after_xmit_cb);
	pr_info("etherhook: init\n");
	return 0;
}

module_init(etherhook_init);
module_exit(etherhook_exit);
MODULE_LICENSE("GPL");
