/*
 * offload engine driver for the cs75xx series processors
 * Copyright c 2011, Cortina-Systems Corporation.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/sched.h>

#include <linux/random.h>
#include <mach/platform.h>
#include <mach/hardware.h>

/* Number of descriptors to allocate for each channel. This should be
 * made configurable somehow; preferably, the clients (at least the
 * ones using slave transfers) should be able to give us a hint.
 */
#define POWER_OF_DESC		6	/* Maximum is 13 */
#define CS75XX_DMA_DESCRIPTORS	(1 << POWER_OF_DESC)
/* (TX + RX) * No_of_desc * size_of_desc + alignment */
#define DESC_POOL_SIZE		(2 * CS75XX_DMA_DESCRIPTORS * \
				 sizeof(struct cs75xx_dma_desc) + 16)

#define DMA_MAX_LEN		0xFFFF

/* Definition for descript */
#define DESC_RXQ6_ID			6
#define DESC_OWNER_CPU			1
#define DESC_OWNER_HW			0
#define DESC_SOF_LINK			0
#define DESC_SOF_LAST			1
#define DESC_SOF_FIRST			2
#define DESC_SOF_ONE			3

/* Interrupt Status */
#define DMA_RX_INT_RX_EOF		0x01
#define DMA_RX_INT_RX_OVRUN		0x04

#define DMA_RX_INT_TX_EOF		0x01
#define DMA_RX_INT_TX_EMPTY		0x02
#define DMA_RX_INT_TX_OVRUN		0x04
#define DMA_INT_TXQ_EMPTY		0x02

#define DMA_TXQ_ENABLE			1

/* Macro definitions */
#define CS75XX_DMA_CHANNELS	1

/* CS75xx DMA engine registers */
#define DMA_REGLEN		0x400

struct __attribute__ ((__packed__)) cs75xx_dma_desc {
	/* 0x00 */
	u32 count	:16;		/* bits 15:0 */
	u32 desccnt	:6;		/* bits 21:16 */
	u32 sgm		:5;		/* bits 26:22 */
	u32 sofeof	:2;		/* bits 28:27 */
	u32 cache	:1;		/* bits 29:29 */
	u32 share	:1;		/* bits 30:30  */
	u32 own		:1;		/* bits 31:31  */

	/* 0x04 */
	u32 buff_addr;

	u32 wrd2;
	u32 wrd3;
	u32 wrd4;
	u32 wrd5;
	u32 wrd6;
	u32 wrd7;
};

struct cs_adma_desc {
	struct dma_async_tx_descriptor desc;
	struct cs75xx_dma_desc *txd;
	dma_addr_t txd_paddr;
	struct cs75xx_dma_desc *rxd;
	dma_addr_t rxd_paddr;
	int error;
	int desc_id;
	struct list_head node;
};

struct cs75xx_dma_chan {
	struct dma_chan chan;
	struct list_head free;
	struct list_head prepared;
	struct list_head queued;
	struct list_head active;
	struct list_head completed;
	struct cs75xx_dma_desc *alloc_txd;
	dma_addr_t alloc_txd_paddr;
	struct cs75xx_dma_desc *txd;
	dma_addr_t txd_paddr;
	struct cs75xx_dma_desc *rxd;
	dma_addr_t rxd_paddr;
	dma_cookie_t completed_cookie;
	unsigned short tx_wptr;
	unsigned short rx_rptr;
	unsigned long prepared_cookie;
	unsigned long submit_cookie;
	/* Lock for this structure */
	spinlock_t lock;
};

struct cs75xx_dma {
	struct dma_device dma;
	struct tasklet_struct tasklet;
	struct cs75xx_dma_chan channels[CS75XX_DMA_CHANNELS];
	int irq;
	uint error_status;

	/* Lock for error_status field in this structure */
	spinlock_t error_status_lock;
};

#define DRV_NAME	"cs75xx_adma"

/* Convert struct dma_chan to struct cs75xx_dma_chan */
static inline struct cs75xx_dma_chan *dma_chan_to_cs75xx_dma_chan(struct
								  dma_chan *c)
{
	return container_of(c, struct cs75xx_dma_chan, chan);
}

/* Convert struct dma_chan to struct cs75xx_dma */
static inline struct cs75xx_dma *dma_chan_to_cs75xx_dma(struct dma_chan *c)
{
	struct cs75xx_dma_chan *dma_chan = dma_chan_to_cs75xx_dma_chan(c);
	return container_of(dma_chan, struct cs75xx_dma, channels[c->chan_id]);
}

/* */
static void cs75xx_adma_dma_off(struct cs75xx_dma *cs_adma)
{
	int i = 0;

	while (i < cs_adma->dma.chancnt) {
		/* Stop Tx */
		writel(0, DMA_DMA_LSO_TXQ6_CONTROL + i * 4);

		/* Disable RX interrupt */
		writel(0, DMA_DMA_LSO_RXQ6_INTENABLE + i * 8);

		/* Disable TX interrupt */
		writel(0, DMA_DMA_LSO_TXQ6_INTENABLE + i * 8);

		/* Clear pending INT */
		writel(readl(DMA_DMA_LSO_RXQ6_INTERRUPT + i * 8),
		       DMA_DMA_LSO_RXQ6_INTERRUPT + i * 8);
		writel(readl(DMA_DMA_LSO_TXQ6_INTERRUPT + i * 8),
		       DMA_DMA_LSO_TXQ6_INTERRUPT + i * 8);

		i++;
	}
}

/*
 * Execute all queued DMA descriptors.
 *
 * Following requirements must be met while calling cs75xx_dma_execute():
 * 	a) dma_chan->lock is acquired,
 * 	b) dma_chan->active list is empty,
 * 	c) dma_chan->queued list contains at least one entry.
 */
void cs75xx_dma_execute(struct cs75xx_dma_chan *dma_chan)
{
	/* Move all queued descriptors to active list */
	list_splice_tail_init(&dma_chan->queued, &dma_chan->active);

	/* Update TX write pointer */
	writel(dma_chan->tx_wptr, IO_ADDRESS(DMA_DMA_LSO_TXQ6_WPTR));
}

/* Interrupt handler */
static irqreturn_t cs75xx_dma_irq(int irq, void *data)
{
	struct cs75xx_dma *cs75xx_dma = data;
	unsigned int reg_v;
	struct cs75xx_dma_chan *dma_chan = &cs75xx_dma->channels[0];

	writel(0, IO_ADDRESS(DMA_DMA_LSO_TXQ6_INTENABLE));

	reg_v = readl(IO_ADDRESS(DMA_DMA_LSO_RXQ6_INTERRUPT));
	writel(reg_v, IO_ADDRESS(DMA_DMA_LSO_RXQ6_INTERRUPT));
	if (reg_v & DMA_RX_INT_RX_OVRUN)
		printk("ADMA RXQ Overrun\n");

	reg_v = readl(IO_ADDRESS(DMA_DMA_LSO_TXQ6_INTERRUPT));
	writel(reg_v, IO_ADDRESS(DMA_DMA_LSO_TXQ6_INTERRUPT));
	if (reg_v & DMA_RX_INT_TX_OVRUN)
		printk("ADMA TXQ Overrun\n");

	spin_lock(&cs75xx_dma->error_status_lock);
	if ((reg_v & DMA_RX_INT_TX_OVRUN) && cs75xx_dma->error_status == 0)
		cs75xx_dma->error_status = reg_v;
	spin_unlock(&cs75xx_dma->error_status_lock);

	/* Execute queued descriptors */
	list_splice_tail_init(&dma_chan->active, &dma_chan->completed);
	if (!list_empty(&dma_chan->queued))
		cs75xx_dma_execute(dma_chan);

	/* Schedule tasklet */
	tasklet_schedule(&cs75xx_dma->tasklet);

	return IRQ_HANDLED;
}

/* DMA Tasklet */
static void cs75xx_dma_tasklet(unsigned long data)
{
	struct cs75xx_dma *cs75xx_dma = (void *)data;
	dma_cookie_t last_cookie = 0;
	struct cs75xx_dma_chan *dma_chan;
	struct cs_adma_desc *cs75xx_desc;
	struct dma_async_tx_descriptor *desc;
	unsigned long flags;
	struct cs75xx_dma_desc *rxd;
	LIST_HEAD(list);
	uint es;
	int i;

	spin_lock_irqsave(&cs75xx_dma->error_status_lock, flags);
	es = cs75xx_dma->error_status;
	spin_unlock_irqrestore(&cs75xx_dma->error_status_lock, flags);

	for (i = 0; i < cs75xx_dma->dma.chancnt; i++) {
		dma_chan = &cs75xx_dma->channels[i];

		/* Get all completed descriptors */
		spin_lock_irqsave(&dma_chan->lock, flags);
		if (!list_empty(&dma_chan->completed))
			list_splice_tail_init(&dma_chan->completed, &list);

		if (list_empty(&list))
			continue;

		/* Execute callbacks and run dependencies */
		list_for_each_entry(cs75xx_desc, &list, node) {
			desc = &cs75xx_desc->desc;
			rxd = cs75xx_desc->rxd;
			while (rxd->own == DESC_OWNER_HW)
				printk("Error!! RXQ DESC Owner bit != CPU\n");
			writel((cs75xx_desc->desc_id +
				1) % CS75XX_DMA_DESCRIPTORS,
			       IO_ADDRESS(DMA_DMA_LSO_RXQ6_RPTR));

			if (desc->callback)
				desc->callback(desc->callback_param);

			last_cookie = desc->cookie;

			dma_run_dependencies(desc);
		}

		/* Free descriptors */
		list_splice_tail_init(&list, &dma_chan->free);
		dma_chan->completed_cookie = last_cookie;
		spin_unlock_irqrestore(&dma_chan->lock, flags);
	}
	writel(DMA_INT_TXQ_EMPTY, IO_ADDRESS(DMA_DMA_LSO_TXQ6_INTENABLE));
}

/* Submit descriptor to hardware */
static dma_cookie_t cs75xx_dma_tx_submit(struct dma_async_tx_descriptor *txd)
{

	struct cs75xx_dma_chan *dma_chan =
	    dma_chan_to_cs75xx_dma_chan(txd->chan);
	struct cs_adma_desc *cs75xx_desc;
	unsigned long flags;
	dma_cookie_t cookie;

	cs75xx_desc = container_of(txd, struct cs_adma_desc, desc);

wait:
	spin_lock_irqsave(&dma_chan->lock, flags);
	if (cs75xx_desc->txd->wrd6 > dma_chan->submit_cookie){
		spin_unlock_irqrestore(&dma_chan->lock, flags);
		schedule();
		goto wait;
	}

	/* Move descriptor to queue */
	list_move_tail(&cs75xx_desc->node, &dma_chan->queued);


	dma_chan->submit_cookie++;

	/* If channel is idle, execute all queued descriptors */
	if (list_empty(&dma_chan->active))
		cs75xx_dma_execute(dma_chan);

	/* Update cookie */
	cookie = dma_chan->chan.cookie + 1;
	if (cookie <= 0)
		cookie = 1;

	dma_chan->chan.cookie = cookie;
	cs75xx_desc->desc.cookie = cookie;

	spin_unlock_irqrestore(&dma_chan->lock, flags);

	return cookie;
}

/* Alloc channel resources */
static int cs75xx_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct cs75xx_dma *cs75xx_dma = dma_chan_to_cs75xx_dma(chan);
	struct cs75xx_dma_chan *dma_chan = dma_chan_to_cs75xx_dma_chan(chan);
	struct cs_adma_desc *cs75xx_desc, *tmp_desc;
	struct cs75xx_dma_desc *txd;
	struct cs75xx_dma_desc *rxd;
	dma_addr_t txd_paddr, rxd_paddr;
	unsigned short tx_rptr;
	unsigned short rx_wptr;
	unsigned long flags;
	LIST_HEAD(descs);
	int i, tmp;

	/* Alloc DMA memory for Transfer Control Descriptors */
	/* FIXME, allocate more buffer for alignment */
	dma_chan->alloc_txd = dma_alloc_coherent(cs75xx_dma->dma.dev,
				 DESC_POOL_SIZE, &dma_chan->alloc_txd_paddr,
				 GFP_KERNEL);

	if (dma_chan->alloc_txd == NULL)
		return -ENOMEM;


	txd = (unsigned int)dma_chan->alloc_txd & 0xFFFFFFF0;
	txd_paddr = dma_chan->alloc_txd_paddr & 0xFFFFFFF0;

	rxd = &txd[CS75XX_DMA_DESCRIPTORS];
	rxd_paddr = txd_paddr +
	    CS75XX_DMA_DESCRIPTORS * sizeof(struct cs75xx_dma_desc);

	rx_wptr = readl(IO_ADDRESS(DMA_DMA_LSO_RXQ6_WPTR));
	tx_rptr = readl(IO_ADDRESS(DMA_DMA_LSO_TXQ6_RPTR));
	printk("%s", tx_rptr == 0 ? "" : "ADMA TX rptr != 0\n");

	/* Alloc descriptors for this channel */
	for (i = 0; i < CS75XX_DMA_DESCRIPTORS; i++) {
		cs75xx_desc = kzalloc(sizeof(struct cs_adma_desc), GFP_KERNEL);
		if (!cs75xx_desc) {
			dev_notice(cs75xx_dma->dma.dev,
				   "Memory allocation error. "
				   "Allocated only %u descriptors\n", i);

			break;
		}

		dma_async_tx_descriptor_init(&cs75xx_desc->desc, chan);
		cs75xx_desc->desc.flags = DMA_CTRL_ACK;
		cs75xx_desc->desc.tx_submit = cs75xx_dma_tx_submit;

		cs75xx_desc->txd = &txd[(i + tx_rptr) % CS75XX_DMA_DESCRIPTORS];
		cs75xx_desc->txd_paddr = txd_paddr +
		    (((i + tx_rptr) % CS75XX_DMA_DESCRIPTORS) *
		     sizeof(struct cs75xx_dma_desc));
		cs75xx_desc->rxd = &rxd[(i + tx_rptr) % CS75XX_DMA_DESCRIPTORS];
		cs75xx_desc->rxd_paddr = rxd_paddr +
		    (((i + tx_rptr) % CS75XX_DMA_DESCRIPTORS) *
		     sizeof(struct cs75xx_dma_desc));
		cs75xx_desc->desc_id = (i + tx_rptr) % CS75XX_DMA_DESCRIPTORS;
		list_add_tail(&cs75xx_desc->node, &descs);
	}

	/* Return error only if no descriptors were allocated */
	/* FIXME, free allocated descript if no space */
	if (i < CS75XX_DMA_DESCRIPTORS ) {
		dma_free_coherent(cs75xx_dma->dma.dev,
				  DESC_POOL_SIZE, dma_chan->alloc_txd,
				  dma_chan->alloc_txd_paddr);

		/* Free list */
		list_for_each_entry_safe(cs75xx_desc, tmp_desc, &descs, node)
			kfree(cs75xx_desc);

		return -ENOMEM;
	}

	/* configure queue depth and descript base address */
	writel(txd_paddr | POWER_OF_DESC,
	       IO_ADDRESS(DMA_DMA_LSO_TXQ6_BASE_DEPTH));
	writel(rxd_paddr | POWER_OF_DESC,
	       IO_ADDRESS(DMA_DMA_LSO_RXQ6_BASE_DEPTH));

	/* Enable DMA channel */
	tmp = readl(IO_ADDRESS(DMA_DMA_LSO_RXDMA_CONTROL));
	tmp |= 0x0D;
	writel(tmp, IO_ADDRESS(DMA_DMA_LSO_RXDMA_CONTROL));

	/* FIXME, force read then modify write */
	tmp = readl(IO_ADDRESS(DMA_DMA_LSO_RXDMA_CONTROL));
	tmp |= 0x0D;
	writel(tmp, IO_ADDRESS(DMA_DMA_LSO_TXDMA_CONTROL));

	dma_chan->tx_wptr = tx_rptr;
	dma_chan->rx_rptr = rx_wptr;
	/* set rptr == wptr so HW will be held */
	writel(tx_rptr, IO_ADDRESS(DMA_DMA_LSO_TXQ6_WPTR));
	writel(rx_wptr, IO_ADDRESS(DMA_DMA_LSO_RXQ6_RPTR));

	spin_lock_irqsave(&dma_chan->lock, flags);
	dma_chan->txd = txd;
	dma_chan->txd_paddr = txd_paddr;
	dma_chan->rxd = rxd;
	dma_chan->rxd_paddr = rxd_paddr;
	list_splice_tail_init(&descs, &dma_chan->free);
	spin_unlock_irqrestore(&dma_chan->lock, flags);

	/* Enable TXQ6 channel */
	writel(DMA_TXQ_ENABLE, IO_ADDRESS(DMA_DMA_LSO_TXQ6_CONTROL));

	/* Enable Error Interrupt */
	writel(DMA_RX_INT_RX_EOF | DMA_RX_INT_RX_OVRUN,
	       IO_ADDRESS(DMA_DMA_LSO_RXQ6_INTENABLE));
	writel(DMA_INT_TXQ_EMPTY, IO_ADDRESS(DMA_DMA_LSO_TXQ6_INTENABLE));

	return 0;
}

/* Free channel resources */
static void cs75xx_dma_free_chan_resources(struct dma_chan *chan)
{

	struct cs75xx_dma *cs75xx_dma = dma_chan_to_cs75xx_dma(chan);
	struct cs75xx_dma_chan *dma_chan = dma_chan_to_cs75xx_dma_chan(chan);
	struct cs_adma_desc *cs75xx_desc, *tmp;
	unsigned long flags;
	LIST_HEAD(descs);

	spin_lock_irqsave(&dma_chan->lock, flags);

	/* Channel must be idle */
	BUG_ON(!list_empty(&dma_chan->prepared));
	BUG_ON(!list_empty(&dma_chan->queued));
	BUG_ON(!list_empty(&dma_chan->active));
	BUG_ON(!list_empty(&dma_chan->completed));

	/* Move data */
	list_splice_tail_init(&dma_chan->free, &descs);

	spin_unlock_irqrestore(&dma_chan->lock, flags);

	/* Free DMA memory used by descriptors */
	dma_free_coherent(cs75xx_dma->dma.dev,
			  DESC_POOL_SIZE, dma_chan->alloc_txd,
			  dma_chan->alloc_txd_paddr);

	/* Free descriptors */
	list_for_each_entry_safe(cs75xx_desc, tmp, &descs, node)
	    kfree(cs75xx_desc);

	/* Disable Error Interrupt */
	cs75xx_adma_dma_off(cs75xx_dma);
}

/* Send all pending descriptor to hardware */
static void cs75xx_dma_issue_pending(struct dma_chan *chan)
{
	/*
	 * We are posting descriptors to the hardware as soon as
	 * they are ready, so this function does nothing.
	 */
}

/* Check request completion status */
static enum dma_status
cs75xx_dma_tx_status(struct dma_chan *chan, dma_cookie_t cookie,
		     struct dma_tx_state *txstate)
{
	struct cs75xx_dma_chan *dma_chan = dma_chan_to_cs75xx_dma_chan(chan);
	unsigned long flags;
	dma_cookie_t last_used;
	dma_cookie_t last_complete;

	spin_lock_irqsave(&dma_chan->lock, flags);
	last_used = dma_chan->chan.cookie;
	last_complete = dma_chan->completed_cookie;
	spin_unlock_irqrestore(&dma_chan->lock, flags);

	dma_set_tx_state(txstate, last_complete, last_used, 0);
	return dma_async_is_complete(cookie, last_complete, last_used);
}

/* Prepare descriptor for memory to memory copy */
static struct dma_async_tx_descriptor *cs75xx_dma_prep_memcpy(struct dma_chan
							      *chan,
							      dma_addr_t dst,
							      dma_addr_t src,
							      size_t len,
							      unsigned long
							      flags)
{
	struct cs75xx_dma_chan *dma_chan = dma_chan_to_cs75xx_dma_chan(chan);
	struct cs_adma_desc *cs75xx_desc = NULL;
	struct cs75xx_dma_desc *txd, *rxd;
	unsigned long iflags;

	/* Reject if length over 64KB */
	if (len > DMA_MAX_LEN) {
		printk("ADMA REQUEST SIZE OVER[ %d > %d]\n", len, DMA_MAX_LEN);
		return NULL;
	}

	/* Get free descriptor */
	spin_lock_irqsave(&dma_chan->lock, iflags);
	if (!list_empty(&dma_chan->free)) {
		cs75xx_desc =
		    list_first_entry(&dma_chan->free, struct cs_adma_desc,
				     node);
		list_del(&cs75xx_desc->node);
	}
	spin_unlock_irqrestore(&dma_chan->lock, iflags);

	if (!cs75xx_desc){
		printk(" Allocate desc fail for memcpy\n");
		return NULL;
	}


	cs75xx_desc->error = 0;
	txd = cs75xx_desc->txd;
	rxd = cs75xx_desc->rxd;

	/* Prepare Transfer Control Descriptor for this transaction */
	//memset(txd, 0, sizeof(struct cs75xx_dma_desc));

	rxd->count = len;
	rxd->own = DESC_OWNER_HW;
	rxd->buff_addr = dst;

	txd->count = len;
	txd->sgm = 0x10 | DESC_RXQ6_ID;
	txd->sofeof = DESC_SOF_ONE;
	txd->own = DESC_OWNER_HW;
	txd->buff_addr = src;

	/* Increase write pointer */
	dma_chan->tx_wptr = (cs75xx_desc->desc_id + 1) % CS75XX_DMA_DESCRIPTORS;

	txd->wrd6 = dma_chan->prepared_cookie++;

	/* Place descriptor in prepared list */
	spin_lock_irqsave(&dma_chan->lock, iflags);
	list_add_tail(&cs75xx_desc->node, &dma_chan->prepared);
	spin_unlock_irqrestore(&dma_chan->lock, iflags);

	return &cs75xx_desc->desc;
}

static int __devinit cs75xx_dma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dma_device *dma;
	struct cs75xx_dma *cs75xx_dma;
	struct cs75xx_dma_chan *dma_chan;
	int retval, i;
	struct resource *io;
	int err;

	cs75xx_dma = devm_kzalloc(dev, sizeof(struct cs75xx_dma), GFP_KERNEL);
	if (!cs75xx_dma) {
		dev_err(dev, "Memory exhausted!\n");
		return -ENOMEM;
	}

	cs75xx_dma->irq = platform_get_irq(pdev, 1);
	if (cs75xx_dma->irq == NO_IRQ) {
		dev_err(dev, "Error mapping IRQ!\n");
		return -EINVAL;
	}

	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!request_mem_region(io->start, DMA_REGLEN, pdev->dev.driver->name)) {
		dev_err(dev, "Error requesting memory region!\n");
		err = -EBUSY;
		goto err_kfree;
	}

	retval =
	    devm_request_irq(dev, cs75xx_dma->irq, &cs75xx_dma_irq, 0, DRV_NAME,
			     cs75xx_dma);
	if (retval) {
		dev_err(dev, "Error requesting IRQ!\n");
		return -EINVAL;
	}

	spin_lock_init(&cs75xx_dma->error_status_lock);

	dma = &cs75xx_dma->dma;
	dma->dev = dev;
	dma->chancnt = CS75XX_DMA_CHANNELS;
	dma->device_alloc_chan_resources = cs75xx_dma_alloc_chan_resources;
	dma->device_free_chan_resources = cs75xx_dma_free_chan_resources;
	dma->device_issue_pending = cs75xx_dma_issue_pending;
	dma->device_tx_status = cs75xx_dma_tx_status;
	dma->device_prep_dma_memcpy = cs75xx_dma_prep_memcpy;

	INIT_LIST_HEAD(&dma->channels);
	dma_cap_set(DMA_MEMCPY, dma->cap_mask);

	for (i = 0; i < dma->chancnt; i++) {
		dma_chan = &cs75xx_dma->channels[i];

		dma_chan->chan.device = dma;
		dma_chan->chan.chan_id = i;
		dma_chan->chan.cookie = 1;
		dma_chan->completed_cookie = dma_chan->chan.cookie;

		INIT_LIST_HEAD(&dma_chan->free);
		INIT_LIST_HEAD(&dma_chan->prepared);
		INIT_LIST_HEAD(&dma_chan->queued);
		INIT_LIST_HEAD(&dma_chan->active);
		INIT_LIST_HEAD(&dma_chan->completed);

		spin_lock_init(&dma_chan->lock);
		list_add_tail(&dma_chan->chan.device_node, &dma->channels);
	}

	tasklet_init(&cs75xx_dma->tasklet, cs75xx_dma_tasklet,
		     (unsigned long)cs75xx_dma);

	/*
	 * Configure DMA Engine:
	 * - Burst length 64 * 64 Bits
	 */
	cs75xx_adma_dma_off(cs75xx_dma);

	/* Register DMA engine */
	dev_set_drvdata(dev, cs75xx_dma);
	retval = dma_async_device_register(dma);
	if (retval)
		devm_free_irq(dev, cs75xx_dma->irq, cs75xx_dma);

	return retval;

      err_kfree:
	kfree(cs75xx_dma);
	return err;
}

static int __devexit cs75xx_dma_remove(struct platform_device *op)
{

	struct device *dev = &op->dev;
	struct cs75xx_dma *cs75xx_dma = dev_get_drvdata(dev);

	dma_async_device_unregister(&cs75xx_dma->dma);
	devm_free_irq(dev, cs75xx_dma->irq, cs75xx_dma);

	return 0;
}

static struct platform_driver cs75xx_dma_driver = {
	.remove = __exit_p(cs75xx_dma_remove),
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init cs75xx_dma_init(void)
{
	return platform_driver_probe(&cs75xx_dma_driver, cs75xx_dma_probe);
}

module_init(cs75xx_dma_init);

static void __exit cs75xx_dma_exit(void)
{
	platform_driver_unregister(&cs75xx_dma_driver);
}

module_exit(cs75xx_dma_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GoldenGate DMA Controller driver");
MODULE_AUTHOR("Jason Li <jason.li@cortina-systems.com>");
