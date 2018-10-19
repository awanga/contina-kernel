#include <linux/spinlock.h>
#include <mach/cs75xx_pni.h>
#include <mach/cs75xx_ipc_wfo.h>

#include "cs752x_eth.h"
#include "cs_mut.h"

pni_rxq_s pni_rxq[2];

unsigned char pni_swtxq_lock_init = 0;
spinlock_t pni_swtxq_lock;

u8 cs75xx_pni_free_rt3593(void* adapter, void * xmit_pkt)
{
	struct pni_dma_pkt *pkt = (struct pni_dma_pkt *) xmit_pkt;
	if (pkt) {
#if 0
		printk("%s::idx %d, pkt %p\t", __func__, swtxq->finished_idx, pkt);
		printk("buf %p\t", pkt->buf_addr);
		printk("next %p\n", pkt->next);
#endif
		if (pkt->buf_addr)
			cs_free(pkt->buf_addr);

		if (pkt->next) {
			//printk("%s::skb %p\n\n", __func__, pkt->next->skb);
			dev_kfree_skb_any(pkt->next->skb);
			cs_free(pkt->next);
		}
		cs_free(pkt);
	}
	return 0;
}

void cs75xx_pni_init(void)
{
	int i;
	for (i=0; i<2; i++) {
		pni_rxq[i].init = 0;
		pni_rxq[i].adapter = NULL;
	}

	/* spin lock init */
	if (!pni_swtxq_lock_init) {
		spin_lock_init(&pni_swtxq_lock);
		pni_swtxq_lock_init = 1;
	}
}

int cs_pni_get_free_pid(void)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&pni_swtxq_lock, flags);
	for (i = 0; i < 2; i++) {
		if (pni_rxq[i].init == 0) {
			pni_rxq[i].init = 1;
			spin_unlock_irqrestore(&pni_swtxq_lock, flags);
			if (i == 0)
				return CS_WFO_IPC_PE0_CPU_ID;
			else
				return CS_WFO_IPC_PE1_CPU_ID;
		}
	}
	spin_unlock_irqrestore(&pni_swtxq_lock, flags);

	return -1;
}
EXPORT_SYMBOL(cs_pni_get_free_pid);

bool cs_pni_get_qm_internal_buffer(void) {
	QM_INT_BUF_CONFIG_0_t qm_int_buf_cfg0;

	qm_int_buf_cfg0.wrd =readl(QM_INT_BUF_CONFIG_0);
	return (qm_int_buf_cfg0.bf.use_internal == 1);
}

int cs_pni_register_chip_callback_xmit(u8 chip_type, int instance,
	void* adapter, u16 (*cb) , u16 (*cb_8023) , u16 (*cb_xmit_done))
{
	if (cs_pni_get_qm_internal_buffer()) {
		printk("\n ?????????????????????????????????????? \n");
		printk("\n ERROR: QM Internal Buffer setup !!!! \n");
		printk("for WFO , need to let qm use external buffer\n");
		printk("please use command \"fw_setenv QM_INT_BUFF 0\" to set it and reboot !!\n");
		printk("\n ?????????????????????????????????????? \n");
		return 0;
	}
//	if (pni_rxq[instance].init == 0) {
		pni_rxq[instance].init = 1;
		pni_rxq[instance].adapter = adapter;
		pni_rxq[instance].cb_fn = cb;
		pni_rxq[instance].cb_fn_802_3 = cb_8023;
		pni_rxq[instance].chip_type = chip_type;
		pni_rxq[instance].cb_fn_xmit_done = cb_xmit_done;
//	}
	printk("\t***%s:: instance %d, pAd %p\n",
		__func__, instance, adapter);
	return 1;
}
EXPORT_SYMBOL(cs_pni_register_chip_callback_xmit);

int cs_pni_register_callback(u8 *tx_base, void* adapter, u16 (*cb) , u16 (*cb_8023))
{
	int instance;

	if (cs_pni_get_qm_internal_buffer()) {
		printk("\n ?????????????????????????????????????? \n");
		printk("\n ERROR: QM Internal Buffer setup !!!! \n");
		printk("for WFO , need to let qm use external buffer\n");
		printk("please use command \"fw_setenv QM_INT_BUFF 0\" to set it and reboot !!\n");
		printk("\n ?????????????????????????????????????? \n");
		return 0;
	}

	if (*tx_base == ENCRYPTION_VOQ_BASE)
		instance = 0;
	else if (*tx_base == ENCAPSULATION_VOQ_BASE)
		instance = 1;
	else
		return 0;

//	if (pni_rxq[instance].init == 0) {
		pni_rxq[instance].init = 1;
		pni_rxq[instance].adapter = adapter;
		pni_rxq[instance].cb_fn = cb;
		pni_rxq[instance].cb_fn_802_3= cb_8023;
		pni_rxq[instance].chip_type = CS_WFO_CHIP_RT3593;
		pni_rxq[instance].cb_fn_xmit_done = &cs75xx_pni_free_rt3593;
//	}
	printk("\t***%s:: tx_qid %d, instance %d, pAd %p\n",
		__func__, *tx_base, instance, adapter);
	return 1;
}
EXPORT_SYMBOL(cs_pni_register_callback);

int cs_pni_unregister_callback(u8 *tx_base, void* adapter)
{
	int instance;

	if (*tx_base == ENCRYPTION_VOQ_BASE)
		instance = 0;
	else if (*tx_base == ENCAPSULATION_VOQ_BASE)
		instance = 1;
	else
		return 0;

	//if (pni_rxq[instance].init == 1) {
		pni_rxq[instance].init = 0;
		pni_rxq[instance].adapter = NULL;
		pni_rxq[instance].cb_fn = NULL;
		pni_rxq[instance].cb_fn_802_3= NULL;
		pni_rxq[instance].cb_fn_xmit_done = NULL;
	//}
	printk("***%s:: tx_qid %d, instance %d, pAd %p\n",
		__func__, *tx_base, instance, adapter);
	return 0;
}
EXPORT_SYMBOL(cs_pni_unregister_callback);


#define PNI_LSO_TX_QID	5
#define PNI_LSO_BYPASS	(1<<21)
#define DMA_ONE_DESC	(3<<27)
#define DMA_SOF_DESC	(2<<27)
#define DMA_EOF_DESC	(1<<27)

extern void cs_ni_tx_queue_complete_free(int tx_qid);

#define HDRA_CPU_PKT	0xc30001

void cs_pni_xmit_ar988x(u8 pe_id, u8 voq, u32 buf0, int len0, u32 buf1, int len1, struct sk_buff *skb)
{
	ni_header_a_0_t ni_hdra;
	dma_swtxq_t *swtxq;

	unsigned int wptr, word0, word1, word2, word3, word4, word5, rptr;
	unsigned int desc_count;
	dma_txdesc_t *curr_desc;
	dma_txdesc_0_t	desc_word0;
	volatile dma_wptr_t wptr_reg;
	u32 free_desc;
	int lso_tx_qid;

	if (smp_processor_id() == 0)
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX - 1;
	else
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX;

	swtxq = &ni_private_data.swtxq[lso_tx_qid];

#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
	cs_ni_tx_queue_complete_free(lso_tx_qid);
#endif
	spin_lock(&swtxq->lock);
	if (swtxq->wptr >= swtxq->finished_idx)
		free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
			swtxq->finished_idx;
	else
		free_desc = swtxq->finished_idx - swtxq->wptr - 1;

	if (free_desc <= 1) {
		printk("%s no more tx queue\n", __func__);
		int instance = pe_id - CS_WFO_IPC_PE0_CPU_ID;
		if	(((instance == 0) || (instance == 1)) && (pni_rxq[instance].cb_fn_xmit_done != NULL))
			pni_rxq[instance].cb_fn_xmit_done(pni_rxq[instance].adapter, skb);
		spin_unlock(&swtxq->lock);
		return;
	}

	ni_hdra.bits32 = HDRA_CPU_PKT;
	ni_hdra.bits.dvoq = voq;

	wptr_reg.bits32 = readl(swtxq->wptr_reg);
	wptr = wptr_reg.bits.wptr;

	curr_desc = swtxq->desc_base + wptr;
	if ((len0 + len1) < MIN_DMA_SIZE)
		len0 = 64;

	word0 = len0 | OWN_BIT | SOF_BIT;
	if (buf1==0)
		word0 |= EOF_BIT;

	word3 = 0; /*cannot set LSO_BYPASS enable, that is for HW debug*/

	if ((len0 + len1) < 64)
		word3 |= LSO_IP_LENFIX_EN;
	word1 = buf0;
	word2 = ((len0 + len1)<<16);
	word4 = ni_hdra.bits32;
	word5 = 0;
	wmb();

	curr_desc->word0.bits32 = word0;
	curr_desc->word1.bits32 = word1;
	curr_desc->word2.bits32 = word2;
	curr_desc->word3.bits32 = word3;
	curr_desc->word4.bits32 = word4;
	curr_desc->word5.bits32 = word5;
	swtxq->xmit_pkt[wptr] = (void *) skb;
	swtxq->wfo_pe_id[wptr] = pe_id;

	if (buf1) {
		wptr = (wptr+1) & (swtxq->total_desc_num-1);
		curr_desc = swtxq->desc_base + wptr;
		word0 = len1 | EOF_BIT;
		word1 = buf1;
		wmb();
		curr_desc->word0.bits32 = word0;
		curr_desc->word1.bits32 = word1;
		curr_desc->word2.bits32 = 0;
		curr_desc->word3.bits32 = word3;
		curr_desc->word4.bits32 = word4;
		curr_desc->word5.bits32 = word5;
		swtxq->xmit_pkt[wptr] = NULL;
		swtxq->wfo_pe_id[wptr] = pe_id;
	}

	wptr = (wptr+1) & (swtxq->total_desc_num-1);
	writel(wptr, swtxq->wptr_reg);
	swtxq->wptr = wptr;
	spin_unlock(&swtxq->lock);
	//printk("\t*** %s::voq %d, skb=%p wptr=%d buf0 %08x, len0 %d, buf1 %08x, len1 %d\n",
	//	__func__, voq, skb, wptr, buf0, len0, buf1, len1);
	return;
}
EXPORT_SYMBOL(cs_pni_xmit_ar988x);

int cs_pni_xmit_none_bypass_ne(u8 instance, u16 lspid, u16 recirc_idx, u32 buf0, int len0,
	u32 buf1, int len1, u32 buf2, int len2, void * data)
{
	ni_header_a_0_t ni_hdra;
	ni_header_a_1_t ni_hdra1;
	dma_swtxq_t *swtxq;

	unsigned int wptr, word0, word1, word2, word3, word4, word5, rptr;
	unsigned int desc_count;
	dma_txdesc_t *curr_desc;
	dma_txdesc_0_t  desc_word0;
	volatile dma_wptr_t wptr_reg;
	u32 free_desc;
	int lso_tx_qid;

	if (smp_processor_id() == 0)
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX - 1;
	else
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX;

	swtxq = &ni_private_data.swtxq[lso_tx_qid];

#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
	cs_ni_tx_queue_complete_free(lso_tx_qid);
#endif
	spin_lock(&swtxq->lock);
	if (swtxq->wptr >= swtxq->finished_idx)
		free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
			swtxq->finished_idx;
	else
		free_desc = swtxq->finished_idx - swtxq->wptr - 1;

	if (free_desc <= 1) {
		printk("%s no more tx queue\n", __func__);
		if	(((instance == 0) || (instance == 1)) && (pni_rxq[instance].cb_fn_xmit_done != NULL))
			pni_rxq[instance].cb_fn_xmit_done(pni_rxq[instance].adapter, data);
		spin_unlock(&swtxq->lock);
		return -1;
	}

	ni_hdra.bits32 = HDRA_CPU_PKT;
	//ni_hdra.bits.dvoq = voq;
	ni_hdra.bits.pspid = lspid;
	ni_hdra.bits.fwd_type = 0; /* Normal */
	ni_hdra1.bits32 = 0;
	ni_hdra1.bits.recirc_idx = recirc_idx;

	wptr_reg.bits32 = readl(swtxq->wptr_reg);
	wptr = wptr_reg.bits.wptr;

	curr_desc = swtxq->desc_base + wptr;
	if ((len0 + len1 + len2) < MIN_DMA_SIZE)
		len0 = 64;

	word0 = len0 | OWN_BIT | SOF_BIT;
	if (buf1 == 0)
		word0 |= EOF_BIT;

	word3 = 0; /*cannot set LSO_BYPASS enable, that is for HW debug*/

	if ((len0 + len1 + len2) < 64)
		word3 |= LSO_IP_LENFIX_EN;

	/* Enable UDP/TCP HW checksum offload */
	word3 |= LSO_UDP_CHECKSUM_EN | LSO_TCP_CHECKSUM_EN | LSO_IPV4_FRAGMENT_EN;

	word1 = buf0;
	word2 = ((len0 + len1 + len2)<<16);
	word4 = ni_hdra.bits32;
	word5 = ni_hdra1.bits32;

	curr_desc->word0.bits32 = word0;
	curr_desc->word1.bits32 = word1;
	curr_desc->word2.bits32 = word2;
	curr_desc->word3.bits32 = word3;
	curr_desc->word4.bits32 = word4;
	curr_desc->word5.bits32 = word5;
	swtxq->xmit_pkt[wptr] = data;
	swtxq->wfo_pe_id[wptr] = instance + CS_WFO_IPC_PE0_CPU_ID;

	if (buf1) {
		wptr = (wptr+1) & (swtxq->total_desc_num-1);
		curr_desc = swtxq->desc_base + wptr;
		word0 = len1;
		if (buf2 == 0)
			word0 = word0 | EOF_BIT;
		word1 = buf1;
		wmb();
		curr_desc->word0.bits32 = word0;
		curr_desc->word1.bits32 = word1;
		curr_desc->word2.bits32 = 0;
		curr_desc->word3.bits32 = word3;
		curr_desc->word4.bits32 = word4;
		curr_desc->word5.bits32 = word5;
		swtxq->xmit_pkt[wptr] = NULL;
		swtxq->wfo_pe_id[wptr] = instance + CS_WFO_IPC_PE0_CPU_ID;
	}

	if (buf2) {
		wptr = (wptr+1) & (swtxq->total_desc_num-1);
		curr_desc = swtxq->desc_base + wptr;
		word0 = len2 | EOF_BIT;
		word1 = buf2;
		wmb();
		curr_desc->word0.bits32 = word0;
		curr_desc->word1.bits32 = word1;
		curr_desc->word2.bits32 = 0;
		curr_desc->word3.bits32 = word3;
		curr_desc->word4.bits32 = word4;
		curr_desc->word5.bits32 = word5;
		swtxq->xmit_pkt[wptr] = NULL;
		swtxq->wfo_pe_id[wptr] = instance + CS_WFO_IPC_PE0_CPU_ID;
	}
	wmb();

	wptr = (wptr+1) & (swtxq->total_desc_num-1);
	writel(wptr, swtxq->wptr_reg);
	swtxq->wptr = wptr;
	spin_unlock(&swtxq->lock);
	//printk("\t*** %s::voq %d, skb=%p wptr=%d buf0 %08x, len0 %d, buf1 %08x, len1 %d\n",
	//	__func__, voq, skb, wptr, buf0, len0, buf1, len1);
	return 0;
}
EXPORT_SYMBOL(cs_pni_xmit_none_bypass_ne);


void cs_pni_start_xmit(u8 voq, struct pni_dma_pkt *tx_pkt)
{
	ni_header_a_0_t ni_hdra;
	dma_swtxq_t *swtxq;
	unsigned int wptr, word0, word1, word2, word3, word4, word5;
	dma_txdesc_t *curr_desc;
	volatile dma_wptr_t wptr_reg;
	u32 free_desc;	
	int lso_tx_qid;
	
	if (smp_processor_id() == 0)
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX - 1;
	else
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX;
			
	swtxq = &ni_private_data.swtxq[lso_tx_qid];
	
#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT	
	cs_ni_tx_queue_complete_free(lso_tx_qid);
#endif
	spin_lock(&swtxq->lock);
	if (swtxq->wptr >= swtxq->finished_idx)
		free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
			swtxq->finished_idx;
	else
		free_desc = swtxq->finished_idx - swtxq->wptr - 1;
	
	if (free_desc <= 1) {	
		printk("%s no more tx queue\n", __func__);		
		int instance = (voq == ENCRYPTION_VOQ_BASE)? CS_WFO_IPC_PE0_CPU_ID: CS_WFO_IPC_PE1_CPU_ID;
		if	(((instance == 0) || (instance == 1)) && (pni_rxq[instance].cb_fn_xmit_done != NULL))
			pni_rxq[instance].cb_fn_xmit_done(pni_rxq[instance].adapter, tx_pkt);	
		spin_unlock(&swtxq->lock);
		return;
	}	

	// Default 0xc30001, from CPU, fwd type bypass
	ni_hdra.bits32 = HDRA_CPU_PKT;
	//ni_hdra.bits.dvoq = GE_PORT2_VOQ_BASE + (7 - priority);
	ni_hdra.bits.dvoq = voq;

	// we don't check free_desc_count now, suppose enough...

	wptr_reg.bits32 = readl(swtxq->wptr_reg);
	wptr = wptr_reg.bits.wptr;

	curr_desc = swtxq->desc_base + wptr;
	//word0 = tx_pkt->len | DMA_ONE_DESC;	// first desc.
	word0 = tx_pkt->len | DMA_SOF_DESC;	// first desc.
	word1 = (u32)dma_map_single(NULL, (void*) tx_pkt->buf_addr, tx_pkt->len,
					DMA_TO_DEVICE);
	word2 = ((tx_pkt->ttl_len)<< 16);
	word3 = 0;	// disable mode	
	word4 = ni_hdra.bits32;
	word5 = 0;
	wmb();
#if 0
	printk("%s::%08x %08x %08x %08x %08x %08x\n",
		__func__, word0, word1, word2, word3, word4, word5);
#endif
	curr_desc->word0.bits32 = word0;
	curr_desc->word1.bits32 = (u32)word1;
	curr_desc->word2.bits32 = word2;
	curr_desc->word3.bits32 = word3;
	curr_desc->word4.bits32 = word4;
	curr_desc->word5.bits32 = word5;

	//swtxq->pkt[wptr] = tx_pkt;
	swtxq->xmit_pkt[wptr] = (void *) tx_pkt;
	swtxq->wfo_pe_id[wptr] = (voq == ENCRYPTION_VOQ_BASE)? CS_WFO_IPC_PE0_CPU_ID: CS_WFO_IPC_PE1_CPU_ID;
//	printk("%s::tx idx %d, pkt %p, buf %p, next %p ",
//		__func__, wptr, tx_pkt, tx_pkt->buf_addr, tx_pkt->next);
//	printk("buf %p\n", tx_pkt->next->buf_addr);
	wmb();

	wptr = (wptr+1) & (swtxq->total_desc_num-1);
	curr_desc = swtxq->desc_base + wptr;
	//word0 = tx_pkt->next->len | DMA_ONE_DESC;
	word0 = tx_pkt->next->len | DMA_EOF_DESC;
	word1 = (u32)dma_map_single(NULL, (void*)tx_pkt->next->buf_addr,
					tx_pkt->next->len, DMA_TO_DEVICE);
	word2 = (tx_pkt->ttl_len << 16);
	//word2 = (tx_pkt->next->len << 16);

	wmb();
	curr_desc->word0.bits32 = word0;
	curr_desc->word1.bits32 = (u32)word1;
	curr_desc->word2.bits32 = word2;
	curr_desc->word3.bits32 = word3;
	curr_desc->word4.bits32 = word4;
	curr_desc->word5.bits32 = word5;

	wmb();

	wptr = (wptr+1) & (swtxq->total_desc_num-1);
	writel(wptr, swtxq->wptr_reg);
	swtxq->wptr = wptr;
	spin_unlock(&swtxq->lock);
	
	return;

}
EXPORT_SYMBOL(cs_pni_start_xmit);


#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
void cs75xx_pni_rx(int instance, int voq, struct sk_buff *skb)
{
	u8 *data = skb->data;
	int i;

//	printk("%s:%d::instance %d, len %d, voqid %d, adapter %p\n",
//		__func__, __LINE__, instance, skb->len, voq, pni_rxq[instance].adapter);
	if (pni_rxq[instance].init) {
		pni_rxq[instance].cb_fn(voq, pni_rxq[instance].adapter, skb);
	} else {
		printk("%s::pni_rxq %d not initialized!\n", __func__, instance);
		dev_kfree_skb_any(skb);
	}
	return;
}

void cs75xx_pni_rx_8023(int instance, int voq, struct sk_buff *skb)
{
	u8 *data = skb->data;
	int i;

	if (pni_rxq[instance].init) {
		pni_rxq[instance].cb_fn_802_3(voq, pni_rxq[instance].adapter, skb);
	} else {
		//printk("%s::pni_rxq %d not initialized!\n", __func__, instance);
		dev_kfree_skb_any(skb);
	}
	return;
}

#else //CONFIG_CS75XX_OFFSET_BASED_QOS

void cs75xx_pni_rx(int instance, int voq, struct sk_buff *skb)
{
	u8 *data = skb->data;
	int i;

	//printk("\n%s::instance %d, len %d, voqid %d, adapter %p\n",
	//	__func__, instance, skb->len, voq, pni_rxq[instance-3].adapter);
#if 0
	for (i=0; i<4; i++) {
		printk("%02x %02x %02x %02x     %02x %02x %02x %02x\n",
			*(data+i*8), *(data+i*8+1), *(data+i*8+2), *(data+i*8+3),
			*(data+i*8+4), *(data+i*8+5), *(data+i*8+6), *(data+i*8+7));
	}
#endif
	if (pni_rxq[instance-3].init) {
		pni_rxq[instance-3].cb_fn(voq, pni_rxq[instance-3].adapter, skb);
	} else {
		printk("%s::pni_rxq %d not initialized!\n", __func__, instance-3);
		dev_kfree_skb_any(skb);
	}
	return;
}

void cs75xx_pni_rx_8023(int instance, int voq, struct sk_buff *skb)
{
	u8 *data = skb->data;
	int i;

	if (pni_rxq[instance-3].init) {
		pni_rxq[instance-3].cb_fn_802_3(voq, pni_rxq[instance-3].adapter, skb);
	} else {
		printk("%s::pni_rxq %d not initialized!\n", __func__, instance-3);
		dev_kfree_skb_any(skb);
	}
	return;
}
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

u8 cs75xx_pni_get_chip_type(int idx)
{
	if (pni_rxq[idx].init) {
		return pni_rxq[idx].chip_type;
	}
	return -1;
}

