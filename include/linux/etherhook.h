
#ifndef _LINUX_ETHERHOOK_H_
#define _LINUX_ETHERHOOK_H_

#include <linux/types.h>
#include <linux/skbuff.h>

struct etherhook_handler {
	u32 sw_action;
	int (*handle_rx)(struct sk_buff*);
	int (*after_xmit_cb)(void*);
};

int etherhook_register(struct etherhook_handler*);
int etherhook_unregister(void);
int etherhook_has_hook(u32 sw_action);
int etherhook_rx_skb(struct sk_buff*);
int etherhook_tx(u32 buf0, int len0, u32 buf1, int len1, u32 buf2, int len2, void*);

#endif
