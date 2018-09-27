/*
 * Header file of pseudo NIC interace.
 */

#ifndef _CS75XX_PNI_H_
#define _CS75XX_PNI_H_

#include <linux/skbuff.h>

typedef enum wfo_pkt_type {
	WFO_CNTL_PKT,
	WFO_MGMT_PKT,
	WFO_QOS_DATA,
	WFO_MLME_PKT,
	WFO_NON_QOS_DATA,
	WFO_PKT_TYPE_MAX
} wfo_pkt_type_e;

struct wfo_tx_wrapper {
	u8	wifi_mac_da[6];
	u8	mac_bcast[6];
	u16	eth_type;
	u8	priority;
	u8	pkt_type;
	u16	sequence;
	u16	hdr_len;
	u16	pkt_len;
} __attribute__((packed));

struct pni_dma_pkt;
struct pni_dma_pkt {
	u16 len;	// this buffer length
	u16 ttl_len;	// total length of chained buffers, useless if not chain head.
	u16 tx_qid;
	u16 buf_as_skb;
	unsigned char *buf_addr;
	struct pni_dma_pkt *next;
	struct sk_buff *skb;
};

typedef struct {
	u8 init;
	void *adapter;
	u16 (*cb_fn)(u8 qid, void *adapter, struct sk_buff* skb);
	u16 (*cb_fn_802_3)(u8 qid, void *adapter, struct sk_buff* skb);
	u16 (*cb_fn_xmit_done)(void *adapter, struct sk_buff* skb);
	u8 chip_type;
} pni_rxq_s;

void cs75xx_pni_init(void);
void cs75xx_pni_rx(int instance, int voq, struct sk_buff *skb);
void cs75xx_pni_rx_8023(int instance, int voq, struct sk_buff *skb);

int cs_pni_register_callback(u8* pni_tx_base, void* adapter, u16(*callback_fn) , u16 (*cb_8023));
int cs_pni_register_chip_callback(u8 chip_type, int instance, void* adapter, u16(*callback_fn) , u16 (*cb_8023));

void cs_pni_xmit_ar988x(u8 pe_id, u8 voq, u32 buf0, int len0, u32 buf1, int len1, struct sk_buff *skb);
void cs_pni_start_xmit(u8 qid, struct pni_dma_pkt *pkt);

u8 cs75xx_pni_get_chip_type(int idx);

#endif
