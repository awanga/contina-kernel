/*
 * File: sound/soc/cs75xx/cs75xx_snd_sport.c
 * Description: ASoC platform driver for CS75XX - DMA engine
 *
 * Copyright (c) Cortina-Systems Limited 2012. All rights reserved.
 *
 * Mostly copied from the bf5xx-sport.c driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include "cs75xx_snd_sport.h"
#include "cs75xx_snd_sport_reg.h"
#include "cwda15.h"

/*
 * HW ARCH:
 *
 * DMA_Q7  DMA_Q6
 *   |       |
 *  SSP1    SSP0
 *   |       |
 *   |    +--+--+ IOMUX
 *   |    |     |
 *   |  SSP0  SPDIF
 *
 * SSP0 -> PCM/I2S/SPDIF
 * SSP1 -> PCM/I2S
*/

/* sport - SSP/DMA ************************************************************/
#define CS75XX_SPORT_SPDIF_IDX	0

//#define CS75XX_SPORT_MIN_DMA_BLOCK	256
#define CS75XX_SPORT_MIN_DMA_FREE_DEPTH	0x00000002
#define NAME_SIZE			16

#define cs75xx_ssp_reg_read(index, offset)		(readl(sport->ssp_base[index]+offset))
#define cs75xx_ssp_reg_write(index, offset, val)	(writel(val, sport->ssp_base[index]+offset))
#define cs75xx_dma_reg_read(offset)			(readl(sport->dma_base+offset))
#define cs75xx_dma_reg_write(offset, val)		(writel(val, sport->dma_base+offset))

typedef struct {
	u16 txByteShiftErr;
	u16 rxByteShiftErr;
	u16 rfsOvrRun;
	u16 rfsUndRun;
	u16 rfrWtMk;
	u16 rfsEmpty;
	u16 rfsFull;
	u16 tfsOvrRun;
	u16 tfsUndRun;
	u16 tfwWtMk;
	u16 tfsEmpty;
	u16 tfsFull;
} cs75xx_sport_ssp_cnt_t;

typedef struct {
	cs75xx_sport_ssp_cnt_t cnt;
	u8 type;	/* PCM, I2S */
	u8 chan;
	u8 size;	/* sampling size, codec */
} cs75xx_sport_ssp_ctrl_t;

typedef struct {
	u16 spdifFifoFull_int;
	u16 spdif_int;
} cs75xx_sport_spdif_cnt_t;

typedef struct {
	cs75xx_sport_spdif_cnt_t cnt;

	u8 type;	/* PCM, AC3, DTS */
} cs75xx_sport_spdif_ctrl_t;

typedef struct {
#ifdef CS_BIG_ENDIAN
	u32 own:1;
	u32 misc:9;
	u32 desccnt:6;
	u32 buf_size:16;
#else
	u32 buf_size:16;
	u32 desccnt:6;
	u32 misc:9;
	u32 own:1;
#endif
	dma_addr_t buf_paddr;
	u32 reserved[2];
} cs75xx_sport_dma_desc_t;

typedef struct {
	u16 rxq_full;
	u16 rxq_overrun;
	u16 rxq_cntmsb;
	u16 rxq_drop_overrun;
	u16 rxq_drop_cntmsb;
	u16 txq_empty;
	u16 txq_overrun;
	u16 txq_cntmsb;
} cs75xx_sport_dma_cnt_t;

typedef struct {
	dma_addr_t tx_desc_paddr;
	dma_addr_t rx_desc_paddr;

	cs75xx_sport_dma_desc_t *tx_dma_desc;
	cs75xx_sport_dma_desc_t *rx_dma_desc;

	u16 tx_buf_num;
	u16 rx_buf_num;
	u16 tx_buf_size;
	u16 rx_buf_size;

	void (*dma_tx_notifier)(void *data);
	void *tx_data;
	void (*dma_rx_notifier)(void *data);
	void *rx_data;

	u32 dma_irq_tx_status;
	u32 dma_irq_rx_status;

	cs75xx_sport_dma_cnt_t cnt;

	u8 tx_swap;
	u8 rx_swap;
} cs75xx_sport_dma_ctrl_t;

struct cs75xx_sport {
	struct device	*dev;

	void __iomem	*ssp_base[CS75XX_SSP_NUM];
	int		irq_ssp[CS75XX_SSP_NUM];
	char		irq_ssp_name[CS75XX_SSP_NUM][NAME_SIZE];
	cs75xx_sport_ssp_ctrl_t	ssp_ctrl[CS75XX_SSP_NUM];

	void __iomem	*spdif_base;
	int		irq_spdif;
	char		irq_spdif_name[NAME_SIZE];
	spinlock_t	spdif_lock;
	CWda15_Config_Type spdif_core;
	cs75xx_sport_spdif_ctrl_t spdif_ctrl;

	/* SSP0 & SPDIF share the same DMA queue */
	void __iomem	*dma_base;
	int		irq_desc;
	char		irq_desc_name[NAME_SIZE];
	int		irq_rx[CS75XX_SSP_NUM];
	char		irq_rx_name[CS75XX_SSP_NUM][NAME_SIZE];
	int		irq_tx[CS75XX_SSP_NUM];
	char		irq_tx_name[CS75XX_SSP_NUM][NAME_SIZE];
	cs75xx_sport_dma_ctrl_t dma_ctrl[CS75XX_SSP_NUM];

	spinlock_t	lock[CS75XX_SSP_NUM];
	u8		type[CS75XX_SSP_NUM];	/* type for usage */
};

unsigned int sport_debug = 0;
//unsigned int sport_debug = SPORT_TRACE | SPORT_INFO | DAI_TRACE | DAI_INFO | PLAT_TRACE | PLAT_INFO;
EXPORT_SYMBOL(sport_debug);

/* internal APIs definition */
static int cs75xx_sport_dma_tx_idx(int idx, u16 *wt_idx_p, u16 *rd_idx_p);
static int cs75xx_sport_dma_rx_idx(int idx, u16 *wt_idx_p, u16 *rd_idx_p);
static int cs75xx_sport_dma_tx_update(int idx, u16 new_wt_idx);
static int cs75xx_sport_dma_rx_update(int idx, u16 new_rd_idx);
static int cs75xx_sport_dma_tx_enable(int idx);
static int cs75xx_sport_dma_rx_enable(int idx);
static int cs75xx_sport_dma_tx_disable(int idx);
static int cs75xx_sport_dma_rx_disable(int idx);
static void cs75xx_sport_dma_cnt_init(int idx, int dir);
static void cs75xx_sport_dma_cnt_disp(int idx, int dir);
static int cs75xx_sport_dma_buf_check(dma_addr_t paddr, u32 num, u32 size);


/* internal data */
static struct platform_device *cs75xx_sport_dev = NULL;

static u8 sclk_gen_params[5][5] = { /* generate from internal sys clock */
	/*	m,	n,	div,	a1,	a2 */
	[0] = {63,	27,	60,	1,	2}, /* 1024KHz - 0.0000(ppm) */
	[1] = {13,	2,	40,	2,	1}, /* 1536KHz - 9.1380(ppm) */
	[2] = {27,	1,	30,	2,	1}, /* 2048KHz - 9.1380(ppm) */
	[3] = {27,	3,	14,	1,	1}, /* 4096KHz - 9.1711(ppm) */
	[4] = {8,	2,	7,	2,	2}, /* 8096KHz - 30.8633(ppm) */
};


/* Export ASoC Platform(DMA) APIs *********************************************/
int cs75xx_snd_sport_dma_idx(unsigned int idx, int dir, u16 *wt_idx_p, u16 *rd_idx_p)
{
	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if (dir == DIR_TX)
		return cs75xx_sport_dma_tx_idx(idx, wt_idx_p, rd_idx_p);
	else
		return cs75xx_sport_dma_rx_idx(idx, wt_idx_p, rd_idx_p);
}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_idx);

int cs75xx_snd_sport_dma_update(unsigned int idx, int dir, u16 new_idx)
{
	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if (dir == DIR_TX)
		return cs75xx_sport_dma_tx_update(idx, new_idx);
	else
		return cs75xx_sport_dma_rx_update(idx, new_idx);

}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_update);

int cs75xx_snd_sport_dma_callback(unsigned int idx, int dir, void (*callback)(void *),
	void *data)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if (dir == DIR_TX) {
		sport->dma_ctrl[idx].dma_tx_notifier = callback;
		sport->dma_ctrl[idx].tx_data = data;
	}
	else { /* DIR_RX */
		sport->dma_ctrl[idx].dma_rx_notifier = callback;
		sport->dma_ctrl[idx].rx_data = data;
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_callback);

int cs75xx_snd_sport_dma_register(unsigned int idx, int dir, dma_addr_t paddr, unsigned int buf_num,
	unsigned int buf_size)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	cs75xx_sport_dma_ctrl_t *dma_ctrl;
	cs75xx_sport_dma_desc_t *dma_desc;
	int i, power;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d, dir %d, paddr 0x%x, buf_num %d, buf_size %d\n",
			__func__, idx, dir, paddr, buf_num, buf_size);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	power = cs75xx_sport_dma_buf_check(paddr, buf_num, buf_size);
	if (power < 0)
		return -EINVAL;

	dma_ctrl = &(sport->dma_ctrl[idx]);

	if (dir == DIR_TX) {
		/* allocate and config descriptor */
		dma_ctrl->tx_buf_num = buf_num;
		dma_ctrl->tx_buf_size = buf_size;
		dma_ctrl->tx_dma_desc = dma_alloc_coherent(sport->dev,
					    buf_num * sizeof(cs75xx_sport_dma_desc_t),
					    &(dma_ctrl->tx_desc_paddr), GFP_KERNEL|GFP_DMA);

		if (dma_ctrl->tx_dma_desc == NULL)
			return -ENOMEM;
		else {
			if (sport_debug & SPORT_INFO)
				printk("tx_dma_desc = 0x%p, tx_desc_paddr = 0x%08X\n",
				        dma_ctrl->tx_dma_desc, dma_ctrl->tx_desc_paddr);
		}

		for (i = 0; i < buf_num; i++) {
			dma_desc = (cs75xx_sport_dma_desc_t *)(dma_ctrl->tx_dma_desc + i);
			dma_desc->own = 1;	// CPU
			dma_desc->misc = 0;
			//tmp_desc->desccnt = x;
			dma_desc->buf_size = buf_size;
			dma_desc->buf_paddr = paddr + i * buf_size;
		}

		/* set DMA register */
		if (idx == 0) {
			DMA_DMA_SSP_TXQ6_BASE_DEPTH_t reg_txq6_base;

			reg_txq6_base.wrd = 0;
			reg_txq6_base.bf.base =  dma_ctrl->tx_desc_paddr >> 4;
			reg_txq6_base.bf.depth = power;
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_BASE_DEPTH, reg_txq6_base.wrd);
		}
		else if (idx == 1) {
			DMA_DMA_SSP_TXQ7_BASE_DEPTH_t reg_txq7_base;

			reg_txq7_base.wrd = 0;
			reg_txq7_base.bf.base =  dma_ctrl->tx_desc_paddr >> 4;
			reg_txq7_base.bf.depth = power;
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_BASE_DEPTH, reg_txq7_base.wrd);
		}
	}
	else if (dir == DIR_RX) {
		/* allocate and config descriptor */
		dma_ctrl->rx_buf_num = buf_num;
		dma_ctrl->rx_buf_size = buf_size;
		dma_ctrl->rx_dma_desc = dma_alloc_coherent(sport->dev,
					    buf_num * sizeof(cs75xx_sport_dma_desc_t),
					    &(dma_ctrl->rx_desc_paddr), GFP_KERNEL|GFP_DMA);

		if (dma_ctrl->rx_dma_desc == NULL)
			return -ENOMEM;
		else {
			if (sport_debug & SPORT_INFO)
				printk("rx_dma_desc = 0x%p, rx_desc_paddr = 0x%08X\n",
				        dma_ctrl->rx_dma_desc, dma_ctrl->rx_desc_paddr);
		}

		for (i = 0; i < buf_num; i++) {
			dma_desc = (cs75xx_sport_dma_desc_t *)(dma_ctrl->rx_dma_desc + i);
			dma_desc->own = 0;	// DMA
			dma_desc->misc = 0;
			//tmp_desc->desccnt = x;
			dma_desc->buf_size = buf_size;
			dma_desc->buf_paddr = paddr + i * buf_size;
		}

		/* set DMA register */
		if (idx == 0) {
			DMA_DMA_SSP_RXQ6_BASE_DEPTH_t reg_rxq6_base;

			reg_rxq6_base.wrd = 0;
			reg_rxq6_base.bf.base =  dma_ctrl->rx_desc_paddr >> 4;
			reg_rxq6_base.bf.depth = power;
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_BASE_DEPTH, reg_rxq6_base.wrd);
		}
		else if (idx == 1) {
			DMA_DMA_SSP_RXQ7_BASE_DEPTH_t reg_rxq7_base;

			reg_rxq7_base.wrd = 0;
			reg_rxq7_base.bf.base =  dma_ctrl->rx_desc_paddr >> 4;
			reg_rxq7_base.bf.depth = power;
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_BASE_DEPTH, reg_rxq7_base.wrd);
		}
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_register);

int cs75xx_snd_sport_dma_unregister(unsigned int idx, int dir)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d, dir %d\n", __func__, idx, dir);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if ((dir == DIR_TX) && (sport->dma_ctrl[idx].tx_dma_desc)) {
		dma_free_coherent(sport->dev, sport->dma_ctrl[idx].tx_buf_num*sizeof(cs75xx_sport_dma_desc_t),
			sport->dma_ctrl[idx].tx_dma_desc, sport->dma_ctrl[idx].tx_desc_paddr);
		sport->dma_ctrl[idx].tx_dma_desc = NULL;
	}
	if ((dir == DIR_RX) && (sport->dma_ctrl[idx].rx_dma_desc)) {
		dma_free_coherent(sport->dev, sport->dma_ctrl[idx].rx_buf_num*sizeof(cs75xx_sport_dma_desc_t),
			sport->dma_ctrl[idx].rx_dma_desc, sport->dma_ctrl[idx].rx_desc_paddr);
		sport->dma_ctrl[idx].rx_dma_desc = NULL;
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_unregister);

int cs75xx_snd_sport_start(unsigned int idx, int dir)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;
	DMA_SSP_CTRL1_t reg_ssp_ctrl1;
	unsigned long flags;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d, dir %d\n", __func__, idx, dir);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	switch (sport->type[idx]) {
	case CS75XX_SPORT_PCM:
		goto sport_pcm;
	case CS75XX_SPORT_I2S:
		goto sport_i2s;
	case CS75XX_SPORT_SPDIF:
		goto sport_spdif;
	default:
		return -EPERM;
	}

sport_pcm:
sport_i2s:
	/* PCM: TX/RX, I2S: TX */
	if (dir == DIR_TX) {
		cs75xx_sport_dma_tx_enable(idx);

		spin_lock_irqsave(&sport->lock[idx], flags);
		reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL0);
		reg_ssp_ctrl0.bf.forceTC = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

		reg_ssp_ctrl1.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL1);
		reg_ssp_ctrl1.bf.startProc = 1;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_CTRL1, reg_ssp_ctrl1.wrd);
		spin_unlock_irqrestore(&sport->lock[idx], flags);
	}
	else {
		cs75xx_sport_dma_rx_enable(idx);
	}

	return 0;

sport_spdif:
	/* SPDIF: only TX */
	cs75xx_sport_dma_tx_enable(idx);

	spin_lock_irqsave(&sport->lock[idx], flags);
	reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL0);
	reg_ssp_ctrl0.bf.spdif_enable = 1;
	reg_ssp_ctrl0.bf.s_enable = 1;
	cs75xx_ssp_reg_write(CS75XX_SPORT_SPDIF_IDX, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	CWda15_StartSpdif(&sport->spdif_core);
	CWda15_StartData(&sport->spdif_core);
	spin_unlock_irqrestore(&sport->lock[idx], flags);

	return 0;
}

EXPORT_SYMBOL(cs75xx_snd_sport_start);

int cs75xx_snd_sport_stop(unsigned int idx, int dir)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;
	DMA_SSP_FIFO_PTR_t reg_ssp_fifo_ptr;
	unsigned long flags;
	int cnt;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d, dir %d\n", __func__, idx, dir);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	switch (sport->type[idx]) {
	case CS75XX_SPORT_PCM:
		goto sport_pcm;
	case CS75XX_SPORT_I2S:
		goto sport_i2s;
	case CS75XX_SPORT_SPDIF:
		goto sport_spdif;
	default:
		return -EPERM;
	}

sport_pcm:
sport_i2s:
	if (dir == DIR_TX) {
		/* disable SSP */
		spin_lock_irqsave(&sport->lock[idx], flags);
		reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL0);
		reg_ssp_ctrl0.bf.forceTC = 1;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

		udelay(10);
		reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL0);
		reg_ssp_ctrl0.bf.forceTC = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);
		spin_unlock_irqrestore(&sport->lock[idx], flags);

		/* disable DMA */
		cnt = 0;
		do {
			reg_ssp_fifo_ptr.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FIFO_PTR);

			if ((reg_ssp_fifo_ptr.bf.tfWrPtr == 0) && (reg_ssp_fifo_ptr.bf.tfRdPtr == 0))
				break;
			else {
				cnt++;
				mdelay(1);
			}
		} while (cnt <= 50);

		if (cnt > 50)
			dev_warn(sport->dev, "Func: %s - forceTc to Tx idle timeout\n", __func__);

		cs75xx_sport_dma_tx_disable(idx);
	}
	else {
		cnt = 0;
		do {
			reg_ssp_fifo_ptr.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FIFO_PTR);

			if ((reg_ssp_fifo_ptr.bf.rfWrPtr == 0) && (reg_ssp_fifo_ptr.bf.rfRdPtr == 0))
				break;
			else {
				cnt++;
				mdelay(1);
			}
		} while (cnt <= 50);

		if (cnt > 50)
			dev_warn(sport->dev, "Func: %s - forceTc to Rx idle timeout\n", __func__);

		cs75xx_sport_dma_rx_disable(idx);
	}

	return 0;

sport_spdif:
	spin_lock_irqsave(&sport->lock[idx], flags);
	CWda15_StopData(&sport->spdif_core);
	CWda15_StopSpdif(&sport->spdif_core);

	reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(CS75XX_SPORT_SPDIF_IDX, CS75XX_SSP_CTRL0);
	reg_ssp_ctrl0.bf.s_enable = 0;
	cs75xx_ssp_reg_write(CS75XX_SPORT_SPDIF_IDX, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	reg_ssp_ctrl0.bf.spdif_enable = 0;
	cs75xx_ssp_reg_write(CS75XX_SPORT_SPDIF_IDX, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);
	spin_unlock_irqrestore(&sport->lock[idx], flags);

	cs75xx_sport_dma_tx_disable(CS75XX_SPORT_SPDIF_IDX);

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_stop);

unsigned int cs75xx_snd_sport_dma_curr_offset(unsigned int idx, int dir)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	u16 wt_idx, rd_idx;
	unsigned int offset;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if (dir == DIR_TX) {
		cs75xx_sport_dma_tx_idx(idx, &wt_idx, &rd_idx);
		offset = rd_idx * sport->dma_ctrl[idx].tx_buf_size;
	} else {
		cs75xx_sport_dma_rx_idx(idx, &wt_idx, &rd_idx);
		offset = wt_idx * sport->dma_ctrl[idx].rx_buf_size;
	}

	return offset;
}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_curr_offset);

unsigned int cs75xx_pwr_dma_start(unsigned int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	u16 tx_wt, tx_rd, rx_wt, rx_rd , i=0, buf_num=0;
	cs75xx_sport_dma_ctrl_t *dma_ctrl_p;
	
	dma_ctrl_p = &sport->dma_ctrl[idx];
	buf_num = dma_ctrl_p->rx_buf_num;
	
	cs75xx_sport_dma_tx_idx(idx, &tx_wt, &tx_rd);
	cs75xx_sport_dma_rx_idx(idx, &rx_wt, &rx_rd);
	
	cs75xx_snd_sport_dma_update(idx, DIR_TX, (tx_wt + 2) % buf_num);
	cs75xx_snd_sport_dma_update(idx, DIR_RX, (rx_wt + buf_num - 2) % buf_num);
	
	return 0;
}
EXPORT_SYMBOL(cs75xx_pwr_dma_start);

unsigned int cs75xx_pwr_dma_stop(unsigned int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	u16 tx_wt, tx_rd, rx_wt, rx_rd , i, buf_num=0;
	cs75xx_sport_dma_ctrl_t *dma_ctrl_p;
	unsigned long	timeo ;//= jiffies + HZ;
	
	dma_ctrl_p = &sport->dma_ctrl[idx];
	buf_num = dma_ctrl_p->rx_buf_num;		
	
	timeo = jiffies + HZ;
	do {
		cs75xx_sport_dma_tx_idx(idx, &tx_wt, &tx_rd);
		cs75xx_sport_dma_rx_idx(idx, &rx_wt, &rx_rd);
		if(tx_wt == tx_rd && ((rx_wt + 1)%buf_num) == rx_rd)
			break;
	} while (time_before(jiffies, timeo));
	
	//printk("%s - %04x idx(%x)  ok: tx_wt(%x), tx_rd(%x), rx_wt(%x), rx_rd(%x) buf_num(%x)\n",__func__,i,idx,tx_wt, tx_rd, rx_wt, rx_rd,dma_ctrl_p->rx_buf_num);
	
	return 0;
}
EXPORT_SYMBOL(cs75xx_pwr_dma_stop);

int cs75xx_snd_sport_dma_request(unsigned int idx)
{
	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_dma_request);


/* Export ASoC DAI APIs ***********************************************************/
int cs75xx_snd_sport_clkdiv(unsigned int idx, int clk_src, int data_rate)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;
	DMA_SSP_FRAME_CTRL0_t reg_frame_ctrl0;
	DMA_SSP_BAUDRATE_CTRL_t reg_baud_ctrl;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: clk_src %d, data_rate %d\n", __func__, clk_src, data_rate);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if (clk_src == SPORT_SPDIF_CLK_DIV) {
		sport->spdif_core.DataRate = data_rate;

		if (data_rate != 0) {
			if ((sport->type[idx] == CS75XX_SPORT_PCM) ||
			    (sport->type[idx] == CS75XX_SPORT_I2S)) {
#if 0
				/* output clock via gpio */
				if (ext_out) {
					GLOBAL_PIN_MUX_t reg_pin_mux;

					reg_pin_mux.wrd = cs75xx_global_read_reg(0x18);
					reg_pin_mux.bf.pmux_frac_clk_en_gpio1_16 = 1;
					cs75xx_global_write_reg(0x18, reg_pin_mux.wrd);
				}
#endif
				reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(0, CS75XX_SSP_FRAME_CTRL0);
				reg_frame_ctrl0.bf.mclkSel = 1;
				cs75xx_ssp_reg_write(0, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

				/* always devide 2? */
				reg_baud_ctrl.wrd = 0;
				reg_baud_ctrl.bf.param_n = 1;
				reg_baud_ctrl.bf.param_m = 1;
				cs75xx_ssp_reg_write(0, CS75XX_SSP_BAUD_RATE, reg_baud_ctrl.wrd);
			} else if (sport->type[idx] == CS75XX_SPORT_SPDIF) {
				reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(0, CS75XX_SSP_CTRL0);
				reg_ssp_ctrl0.bf.spdif_dll_en = 1;
				cs75xx_ssp_reg_write(0, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);
			}

			if (CWda15_StartClock(&sport->spdif_core))
				return -EINVAL;
		} else {
#if 0
			/* output clock via gpio */
			if (ext_out) {
				GLOBAL_PIN_MUX_t reg_pin_mux;

				reg_pin_mux.wrd = cs75xx_global_read_reg(0x18);
				reg_pin_mux.bf.pmux_frac_clk_en_gpio1_16 = 0;
				cs75xx_global_write_reg(0x18, reg_pin_mux.wrd);
			}
#endif
			if ((sport->type[idx] == CS75XX_SPORT_PCM) ||
			    (sport->type[idx] == CS75XX_SPORT_I2S)) {
				reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(0, CS75XX_SSP_FRAME_CTRL0);
				reg_frame_ctrl0.bf.mclkSel = 0;
				cs75xx_ssp_reg_write(0, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
			} else if (sport->type[idx] == CS75XX_SPORT_SPDIF) {
				reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(0, CS75XX_SSP_CTRL0);
				reg_ssp_ctrl0.bf.spdif_dll_en = 0;
				cs75xx_ssp_reg_write(0, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);
			}

			CWda15_StopClock(&sport->spdif_core);
		}
	}
	else if (clk_src == SPORT_EXT_16384_CLK_DIV) {
		if (data_rate != 0) {
			/* only used for PCM */
			if ((data_rate != 1024000) && (data_rate != 2048000) &&
			    (data_rate != 4096000) && (data_rate != 8192000))
			    return -EINVAL;

		        reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		        reg_frame_ctrl0.bf.extClkSel = 1;
		        reg_frame_ctrl0.bf.clkDiv = (16384000/2)/data_rate - 1;
		        cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		} else {
		        reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		        reg_frame_ctrl0.bf.extClkSel = 0;
			reg_frame_ctrl0.bf.clkDiv = 0;
		        cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		}
	}
	else if (clk_src == SPORT_INT_SYS_CLK_DIV) {
		if (data_rate != 0) {
			int sel;

			/* only used for PCM */
			switch (data_rate) {
			case 1024000:
				sel = 0;
				break;
			case 1536000:
				sel = 1;
				break;
			case 2048000:
				sel = 2;
				break;
			case 4096000:
				sel = 3;
				break;
			case 8096000:
				sel = 4;
				break;
			default:
				return -EINVAL;
			}

		        reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
			reg_frame_ctrl0.bf.clkDiv = sclk_gen_params[sel][2];
		        cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

			reg_baud_ctrl.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_BAUD_RATE);
			reg_baud_ctrl.bf.param_a1 = sclk_gen_params[sel][3];
			reg_baud_ctrl.bf.param_n = sclk_gen_params[sel][1];
			reg_baud_ctrl.bf.param_a2 = sclk_gen_params[sel][4];
			reg_baud_ctrl.bf.param_m = sclk_gen_params[sel][0];
			cs75xx_ssp_reg_write(idx, CS75XX_SSP_BAUD_RATE, reg_baud_ctrl.wrd);
		} else {
		        reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
			reg_frame_ctrl0.bf.clkDiv = 0;
		        cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

			reg_baud_ctrl.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_BAUD_RATE);
			reg_baud_ctrl.bf.param_a1 = 0;
			reg_baud_ctrl.bf.param_n = 0;
			reg_baud_ctrl.bf.param_a2 = 0;
			reg_baud_ctrl.bf.param_m = 0;
			cs75xx_ssp_reg_write(idx, CS75XX_SSP_BAUD_RATE, reg_frame_ctrl0.wrd);
		}
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_clkdiv);

int cs75xx_snd_sport_signal_formt(unsigned int idx, unsigned int format)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_FRAME_CTRL0_t reg_frame_ctrl0;
	DMA_SSP_FRAME_CTRL1_t reg_frame_ctrl1;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	switch (sport->type[idx]) {
	case CS75XX_SPORT_PCM:
		goto sport_pcm;
	case CS75XX_SPORT_I2S:
		goto sport_i2s;
	case CS75XX_SPORT_SPDIF:
		goto sport_spdif;
	default:
		return -EPERM;
	}

sport_pcm:
sport_i2s:
	switch (format & SPORT_DATA_FORMAT_MASK) {
	case SPORT_DATA_I2S:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.FSPol = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_frame_ctrl1.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL1);
		reg_frame_ctrl1.bf.FSLen = 31;
		reg_frame_ctrl1.bf.FS2Slot = 1;
		reg_frame_ctrl1.bf.slot2FS = 7;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);
		break;
	case SPORT_DATA_RIGHT_J:
	case SPORT_DATA_LEFT_J:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.FSPol = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_frame_ctrl1.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL1);
		reg_frame_ctrl1.bf.FSLen = 31;
		reg_frame_ctrl1.bf.FS2Slot = 0;
		reg_frame_ctrl1.bf.slot2FS = 8;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);
		break;
	case SPORT_DATA_DSP_A:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.FSPol = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_frame_ctrl1.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL1);
		reg_frame_ctrl1.bf.FSLen = 7;
		reg_frame_ctrl1.bf.FS2Slot = 1;
		reg_frame_ctrl1.bf.slot2FS = 7;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);
		break;
	case SPORT_DATA_DSP_B:
	case SPORT_DATA_AC97:
	case SPORT_DATA_PDM:
	default:
		return -1;
	}

	switch (format & SPORT_CLOCK_GATE_MASK) {
	case SPORT_CLOCK_CONT:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.FSCFreeRun = 1;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		break;
	case SPORT_CLOCK_GATED:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.FSCFreeRun = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		break;
	default:
		return -1;
	}

	switch (format & SPORT_SIGNAL_INV_MASK) {
	case SPORT_SIGNAL_NB_NF:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.txEdge = 1;
		reg_frame_ctrl0.bf.rxEdge = 0;
		reg_frame_ctrl0.bf.FSEdge = 1;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		break;
	case SPORT_SIGNAL_NB_IF:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.txEdge = 1;
		reg_frame_ctrl0.bf.rxEdge = 0;
		reg_frame_ctrl0.bf.FSEdge = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		break;
	case SPORT_SIGNAL_IB_NF:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.txEdge = 0;
		reg_frame_ctrl0.bf.rxEdge = 1;
		reg_frame_ctrl0.bf.FSEdge = 1;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		break;
	case SPORT_SIGNAL_IB_IF:
		reg_frame_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL0);
		reg_frame_ctrl0.bf.txEdge = 0;
		reg_frame_ctrl0.bf.rxEdge = 1;
		reg_frame_ctrl0.bf.FSEdge = 0;
		cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);
		break;
	default:
		return -1;
	}

	switch (format & SPORT_CLOCK_MASTER_MASK) {
	case SPORT_CLOCK_CBS_CFS:
		break;
	case SPORT_CLOCK_CBM_CFM:
	case SPORT_CLOCK_CBM_CFS:
	case SPORT_CLOCK_CBS_CFM:
	default:
		return -1;
	}

sport_spdif:

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_signal_formt);

int cs75xx_snd_sport_slots(unsigned int idx, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int width)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_FRAME_CTRL1_t reg_frame_ctrl1;
	u32 reg;
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_ssp_reg_write(idx, CS75XX_SSP_TXSLOT_VLD0, tx_mask);
	cs75xx_ssp_reg_write(idx, CS75XX_SSP_RXSLOT_VLD0, rx_mask);

	reg_frame_ctrl1.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FRAME_CTRL1);
	reg_frame_ctrl1.bf.numOfSlots = slots - 1;
	cs75xx_ssp_reg_write(idx, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);

	if (slots > 32)
		slots = 32;

	for (i = 0; i < slots; i++) {
		if ((i % 8) == 0)
			reg = 0;

		reg |= (width - 1) << ((i % 8) * 4);

		if (((i + 1) % 8) == 0)
			cs75xx_ssp_reg_write(idx, CS75XX_SSP_SLOT_SIZE0 + (i / 8) * 4, reg);
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_slots);

int cs75xx_snd_sport_audio_formt(unsigned int idx, int dir, int wide, int endian)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	switch (sport->type[idx]) {
	case CS75XX_SPORT_PCM:
		goto sport_pcm;
	case CS75XX_SPORT_I2S:
		goto sport_i2s;
	case CS75XX_SPORT_SPDIF:
		goto sport_spdif;
	default:
		return -EPERM;
	}

sport_pcm:
sport_i2s:
	reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL0);
	if (dir == DIR_TX) {
		if ((wide == 2) && (endian == 0))
			reg_ssp_ctrl0.bf.txChByteSwap = 1;
		else
			reg_ssp_ctrl0.bf.txChByteSwap = 0;
	} else {
		if ((wide == 2) && (endian == 0))
			reg_ssp_ctrl0.bf.rxChByteSwap = 1;
		else
			reg_ssp_ctrl0.bf.rxChByteSwap = 0;
	}
	cs75xx_ssp_reg_write(idx, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	return 0;

sport_spdif:
	reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_CTRL0);
	if (wide == 2) {
		reg_ssp_ctrl0.bf.spdif_byteShift = 1;
		reg_ssp_ctrl0.bf.spdif_mode4WordEn = 1;
		reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 127;
	} else {
		reg_ssp_ctrl0.bf.spdif_byteShift = 0;
		reg_ssp_ctrl0.bf.spdif_mode4WordEn = 0;
		reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 32;
	}
	cs75xx_ssp_reg_write(idx, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	/* PCM, AC3, DTS? */
	sport->spdif_core.PcmMode = TRUE;
	CWda15_Configure(&sport->spdif_core);
	CWda15_Configure_Buffers(&sport->spdif_core);

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_audio_formt);

/*
int cs75xx_snd_sport_slot(int format)
{

}
EXPORT_SYMBOL(cs75xx_snd_sport_formt);

int cs75xx_snd_sport_channel(int format)
{

}
EXPORT_SYMBOL(cs75xx_snd_sport_formt);
*/

int cs75xx_snd_sport_if_register(unsigned int idx, int type, int tx, int rx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_FIFO_CTRL_t reg_fifo_ctrl;
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d, tx %d, rx %d\n", __func__, idx, tx, rx);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	if ((tx == 0) && (rx == 0))
		return -EINVAL;

	if (sport->type[idx] != CS75XX_SPORT_UNDEF)
		return -EPERM;
	else
		sport->type[idx] = type;

	switch (sport->type[idx]) {
	case CS75XX_SPORT_PCM:
		goto sport_pcm;
	case CS75XX_SPORT_I2S:
		goto sport_i2s;
	case CS75XX_SPORT_SPDIF:
		if (idx != CS75XX_SPORT_SPDIF_IDX)
			return -EINVAL;
		goto sport_spdif;
	default:
		return -EINVAL;
	}

sport_pcm:
sport_i2s:
	reg_fifo_ctrl.wrd = cs75xx_ssp_reg_read(idx, CS75XX_SSP_FIFO_CTRL);
	if (tx) {
		reg_fifo_ctrl.bf.tfrWtMkLvl = 4;
		reg_fifo_ctrl.bf.tfwWtMkLvl = 127;
	}
	if (rx) {
		reg_fifo_ctrl.bf.rfrWtMkLvl = 127;
		reg_fifo_ctrl.bf.rfwWtMkLvl = 0;
	}
	cs75xx_ssp_reg_write(idx, CS75XX_SSP_FIFO_CTRL, reg_fifo_ctrl.wrd);

	return 0;

sport_spdif:
	reg_ssp_ctrl0.wrd = cs75xx_ssp_reg_read(CS75XX_SPORT_SPDIF_IDX, CS75XX_SSP_CTRL0);
	reg_ssp_ctrl0.bf.spdif_preambleIns = 1;
	cs75xx_ssp_reg_write(CS75XX_SPORT_SPDIF_IDX, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_if_register);

int cs75xx_snd_sport_if_unregister(unsigned int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d\n", __func__, idx);

	if (unlikely(WARN_ON(idx >= CS75XX_SSP_NUM)))
		return -EINVAL;

	sport->type[idx] = CS75XX_SPORT_UNDEF;

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_sport_if_unregister);

struct device * cs75xx_snd_sport_device(void)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (sport != NULL)
		return sport->dev;
	else
		return NULL;
}
EXPORT_SYMBOL(cs75xx_snd_sport_device);


/* Sport Interval Data and Functions ******************************************/
static int cs75xx_sport_dma_tx_idx(int idx, u16 *wt_idx_p, u16 *rd_idx_p)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (idx == 0) {
		DMA_DMA_SSP_TXQ6_WPTR_t reg_txq6_wptr;
		DMA_DMA_SSP_TXQ6_RPTR_t reg_txq6_rptr;

		reg_txq6_wptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_WPTR);
		*wt_idx_p = reg_txq6_wptr.bf.index;

		reg_txq6_rptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_RPTR);
		*rd_idx_p = reg_txq6_rptr.bf.index;

	}
	else if (idx == 1) {
		DMA_DMA_SSP_TXQ7_WPTR_t reg_txq7_wptr;
		DMA_DMA_SSP_TXQ7_RPTR_t reg_txq7_rptr;

		reg_txq7_wptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_WPTR);
		*wt_idx_p = reg_txq7_wptr.bf.index;

		reg_txq7_rptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_RPTR);
		*rd_idx_p = reg_txq7_rptr.bf.index;
	}

	return 0;
}

static int cs75xx_sport_dma_rx_idx(int idx, u16 *wt_idx_p, u16 *rd_idx_p)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (idx == 0) {
		DMA_DMA_SSP_TXQ6_WPTR_t reg_rxq6_wptr;
		DMA_DMA_SSP_TXQ6_RPTR_t reg_rxq6_rptr;

		reg_rxq6_wptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_WPTR);
		*wt_idx_p = reg_rxq6_wptr.bf.index;

		reg_rxq6_rptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_RPTR);
		*rd_idx_p = reg_rxq6_rptr.bf.index;

	}
	else if (idx == 1) {
		DMA_DMA_SSP_TXQ7_WPTR_t reg_rxq7_wptr;
		DMA_DMA_SSP_TXQ7_RPTR_t reg_rxq7_rptr;

		reg_rxq7_wptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_WPTR);
		*wt_idx_p = reg_rxq7_wptr.bf.index;

		reg_rxq7_rptr.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_RPTR);
		*rd_idx_p = reg_rxq7_rptr.bf.index;
	}

	return 0;
}

static int cs75xx_sport_dma_tx_update(int idx, u16 new_wt_idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	cs75xx_sport_dma_ctrl_t *dma_ctrl_p;
	u16 wt_idx, rd_idx;
	int change = 0;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	dma_ctrl_p = &sport->dma_ctrl[idx];

	cs75xx_sport_dma_tx_idx(idx, &wt_idx, &rd_idx);

	/* update buffer descriptor own bit */
	while (wt_idx != new_wt_idx) {
		if (dma_ctrl_p->tx_dma_desc[wt_idx].own == 0) {
			dev_err(sport->dev,"Can't Update SSP%d Tx-(w:%d,r:%d) to %d\n",
			        idx, wt_idx, rd_idx, new_wt_idx);
			break;
		}
		change = 1;

		dma_ctrl_p->tx_dma_desc[wt_idx].own = 0; // DMA
		dma_ctrl_p->tx_dma_desc[wt_idx].buf_size = dma_ctrl_p->tx_buf_size;
		wt_idx = (wt_idx + 1) % dma_ctrl_p->tx_buf_num;
	}

	if (change == 1) {
		if (idx == 0)
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_WPTR, wt_idx);
		else if (idx == 1)
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_WPTR, wt_idx);
	}

	return 0;
}

static int cs75xx_sport_dma_rx_update(int idx, u16 new_rd_idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	cs75xx_sport_dma_ctrl_t *dma_ctrl_p;
	u16 wt_idx, rd_idx;
	int change = 0;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	dma_ctrl_p = &sport->dma_ctrl[idx];

	cs75xx_sport_dma_rx_idx(idx, &wt_idx, &rd_idx);

	/* update buffer descriptor own bit */
	while (rd_idx != new_rd_idx) {
		if (dma_ctrl_p->rx_dma_desc[rd_idx].own == 0) {
			dev_err(sport->dev, "Can't Update SSP%d Rx-(w:%d,r:%d) to %d\n",
			        idx, wt_idx, rd_idx, new_rd_idx);
			break;
		}
		change = 1;

		dma_ctrl_p->rx_dma_desc[rd_idx].own = 0; // DMA
		dma_ctrl_p->rx_dma_desc[rd_idx].buf_size = dma_ctrl_p->rx_buf_size;
		rd_idx = (rd_idx + 1) % dma_ctrl_p->rx_buf_num;
	}

	if (change == 1) {
		if (idx == 0)
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_RPTR, rd_idx);
		else if (idx == 1)
			cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_RPTR, rd_idx);
	}

	return 0;
}

static int cs75xx_sport_dma_tx_enable(int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	unsigned long flags;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d\n", __func__, idx);

	cs75xx_sport_dma_cnt_init(idx, DIR_TX);

	spin_lock_irqsave(&sport->lock[idx], flags);
	if (idx == 0) {
		DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
		DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;

		reg_txq6_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_CTRL);
		reg_txq6_ctrl.bf.txq6_flush_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

		reg_txq6_ctrl.bf.txq6_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

		reg_txq6_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_INTENABLE);
		reg_txq6_inten.bf.txq6_eof_en = 1;
		reg_txq6_inten.bf.txq6_empty_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_INTENABLE, reg_txq6_inten.wrd);
	}
	else if (idx == 1) {
		DMA_DMA_SSP_TXQ7_CONTROL_t reg_txq7_ctrl;
		DMA_DMA_SSP_TXQ7_INTENABLE_t reg_txq7_inten;

		reg_txq7_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_CTRL);
		reg_txq7_ctrl.bf.txq7_flush_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);

		reg_txq7_ctrl.bf.txq7_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);

		reg_txq7_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_INTENABLE);
		reg_txq7_inten.bf.txq7_eof_en = 1;
		reg_txq7_inten.bf.txq7_empty_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_INTENABLE, reg_txq7_inten.wrd);
	}
	spin_unlock_irqrestore(&sport->lock[idx], flags);

	return 0;
}

static int cs75xx_sport_dma_rx_enable(int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	unsigned long flags;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d\n", __func__, idx);

	cs75xx_sport_dma_cnt_init(idx, DIR_RX);

	spin_lock_irqsave(&sport->lock[idx], flags);
	if (idx == 0) {
		DMA_DMA_SSP_RXQ6_INTENABLE_t reg_rxq6_inten;
		DMA_DMA_SSP_RXQ6_CONTROL_t reg_rxq6_ctrl;

		reg_rxq6_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_CTRL);
		reg_rxq6_ctrl.bf.rxq6_flush_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_CTRL, reg_rxq6_ctrl.wrd);

		reg_rxq6_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_INTENABLE);
		reg_rxq6_inten.bf.rxq6_eof_en = 1;
		reg_rxq6_inten.bf.rxq6_full_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_INTENABLE, reg_rxq6_inten.wrd);
	}
	else if (idx == 1) {
		DMA_DMA_SSP_RXQ7_INTENABLE_t reg_rxq7_inten;
		DMA_DMA_SSP_RXQ7_CONTROL_t reg_rxq7_ctrl;

		reg_rxq7_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_CTRL);
		reg_rxq7_ctrl.bf.rxq7_flush_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_CTRL, reg_rxq7_ctrl.wrd);

		reg_rxq7_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_INTENABLE);
		reg_rxq7_inten.bf.rxq7_eof_en = 1;
		reg_rxq7_inten.bf.rxq7_full_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_INTENABLE, reg_rxq7_inten.wrd);
	}
	spin_unlock_irqrestore(&sport->lock[idx], flags);

	return 0;
}

static int cs75xx_sport_dma_tx_disable(int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	cs75xx_sport_dma_desc_t *dma_desc_p;
	unsigned long flags;
	int i;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d\n", __func__, idx);

	spin_lock_irqsave(&sport->lock[idx], flags);
	if (idx == 0) {
		DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
		DMA_DMA_SSP_TXQ6_INTERRUPT_t reg_txq6_int;
		DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;

		reg_txq6_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_CTRL);
		reg_txq6_ctrl.bf.txq6_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

		reg_txq6_ctrl.bf.txq6_flush_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

		reg_txq6_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_INTENABLE);
		reg_txq6_inten.bf.txq6_eof_en = 0;
		reg_txq6_inten.bf.txq6_empty_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_INTENABLE, reg_txq6_inten.wrd);

		reg_txq6_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_INTERRUPT);
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_INTERRUPT, reg_txq6_int.wrd);

		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_WPTR, 0);
	}
	else if (idx == 1) {
		DMA_DMA_SSP_TXQ7_CONTROL_t reg_txq7_ctrl;
		DMA_DMA_SSP_TXQ7_INTERRUPT_t reg_txq7_int;
		DMA_DMA_SSP_TXQ7_INTENABLE_t reg_txq7_inten;

		reg_txq7_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_CTRL);
		reg_txq7_ctrl.bf.txq7_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);

		reg_txq7_ctrl.bf.txq7_flush_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);

		reg_txq7_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_INTENABLE);
		reg_txq7_inten.bf.txq7_eof_en = 0;
		reg_txq7_inten.bf.txq7_empty_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_INTENABLE, reg_txq7_inten.wrd);

		reg_txq7_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_INTERRUPT);
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_INTERRUPT, reg_txq7_int.wrd);

		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_WPTR, 0);
	}

	/* reset DMA descriptor */
	for (i = 0; i < sport->dma_ctrl[idx].tx_buf_num; i++) {
		dma_desc_p = (cs75xx_sport_dma_desc_t *)(sport->dma_ctrl[idx].tx_dma_desc + i);
		dma_desc_p->own = 1;	// CPU
		dma_desc_p->misc = 0;
		//dma_desc_p->desccnt = x;
		dma_desc_p->buf_size = sport->dma_ctrl[idx].tx_buf_size;
		//dma_desc_p->buf_paddr = rbuf_paddr + i*rx_buf_size;
	}
	spin_unlock_irqrestore(&sport->lock[idx], flags);

	cs75xx_sport_dma_cnt_disp(idx, DIR_TX);

	return 0;
}

static int cs75xx_sport_dma_rx_disable(int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	cs75xx_sport_dma_desc_t *dma_desc_p;
	u16 wt_idx, rd_idx;
	unsigned long flags;
	int i;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: idx %d\n", __func__, idx);

	spin_lock_irqsave(&sport->lock[idx], flags);
	/* flush left data */
	cs75xx_sport_dma_rx_idx(idx, &wt_idx, &rd_idx);
	cs75xx_sport_dma_rx_update(idx, wt_idx);

	/* disable Rx Interrupt */
	if (idx == 0) {
		DMA_DMA_SSP_RXQ6_INTENABLE_t reg_rxq6_inten;
		DMA_DMA_SSP_RXQ6_CONTROL_t reg_rxq6_ctrl;

		reg_rxq6_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_CTRL);
		reg_rxq6_ctrl.bf.rxq6_flush_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_CTRL, reg_rxq6_ctrl.wrd);

		reg_rxq6_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_INTENABLE);
		reg_rxq6_inten.bf.rxq6_eof_en = 0;
		reg_rxq6_inten.bf.rxq6_full_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_INTENABLE, reg_rxq6_inten.wrd);

		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_RPTR, 0);
	}
	else if (idx == 1) {
		DMA_DMA_SSP_RXQ7_INTENABLE_t reg_rxq7_inten;
		DMA_DMA_SSP_RXQ7_CONTROL_t reg_rxq7_ctrl;

		reg_rxq7_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_CTRL);
		reg_rxq7_ctrl.bf.rxq7_flush_en = 1;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_CTRL, reg_rxq7_ctrl.wrd);

		reg_rxq7_inten.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_INTENABLE);
		reg_rxq7_inten.bf.rxq7_eof_en = 0;
		reg_rxq7_inten.bf.rxq7_full_en = 0;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_INTENABLE, reg_rxq7_inten.wrd);

		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_RPTR, 0);
	}

	/* reset DMA descriptor */
	for (i = 0; i < sport->dma_ctrl[idx].rx_buf_num; i++) {
		dma_desc_p = (cs75xx_sport_dma_desc_t *)(sport->dma_ctrl[idx].rx_dma_desc + i);
		dma_desc_p->own = 0;	// DMA
		dma_desc_p->misc = 0;
		//dma_desc_p->desccnt = x;
		dma_desc_p->buf_size = sport->dma_ctrl[idx].rx_buf_size;
		//dma_desc_p->buf_addr = rbuf_paddr + i*rx_buf_size;
	}
	spin_unlock_irqrestore(&sport->lock[idx], flags);

	cs75xx_sport_dma_cnt_disp(idx, DIR_RX);

	return 0;
}

static int cs75xx_sport_dma_buf_check(dma_addr_t paddr, u32 num, u32 size)
{
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if ((paddr == 0) || (num == 0) || (size == 0))
		return -EINVAL;
	if ((size % 128) != 0)
		return -EINVAL;

	for (i = CS75XX_DMA_SSP_DEPTH_MIN; i <= CS75XX_DMA_SSP_DEPTH_MAX; i++)
		if (num == (0x0001 << i))
			return i;

	return -EINVAL;
}

static void cs75xx_sport_dma_cnt_init(int idx, int dir)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (dir == DIR_TX) {
		sport->dma_ctrl[idx].cnt.txq_empty = 0;
		sport->dma_ctrl[idx].cnt.txq_overrun = 0;
		sport->dma_ctrl[idx].cnt.txq_cntmsb = 0;

	} else {
		sport->dma_ctrl[idx].cnt.rxq_full = 0;
		sport->dma_ctrl[idx].cnt.rxq_overrun = 0;
		sport->dma_ctrl[idx].cnt.rxq_cntmsb = 0;
		sport->dma_ctrl[idx].cnt.rxq_drop_overrun = 0;
		sport->dma_ctrl[idx].cnt.rxq_drop_cntmsb = 0;
	}
}

static void cs75xx_sport_dma_cnt_disp(int idx, int dir)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	int q_idx = (idx == 0 ? 6 : 7);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (sport_debug & SPORT_INFO) {
		if (dir == DIR_TX) {
			printk("txq%d_empty = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.txq_empty);
			printk("txq%d_overrun = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.txq_overrun);
			printk("txq%d_cntmsb = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.txq_cntmsb);
		} else {
			printk("rxq%d_full = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.rxq_full);
			printk("rxq%d_overrun = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.rxq_overrun);
			printk("rxq%d_cntmsb = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.rxq_cntmsb);
			printk("rxq%d_drop_overrun = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.rxq_drop_overrun);
			printk("rxq%d_drop_cntmsb = %d\n", q_idx,
				sport->dma_ctrl[idx].cnt.rxq_drop_cntmsb);
		}
	}
}

static void cs75xx_sport_dma_cnt_update(int idx, int dir, u32 reg)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_DMA_SSP_TXQ6_INTERRUPT_t reg_txq6_int;
	DMA_DMA_SSP_TXQ7_INTERRUPT_t reg_txq7_int;
	DMA_DMA_SSP_RXQ6_INTERRUPT_t reg_rxq6_int;
	DMA_DMA_SSP_RXQ7_INTERRUPT_t reg_rxq7_int;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (dir == DIR_TX) {
		if (idx == 0) {
			reg_txq6_int.wrd = reg;

			if (reg_txq6_int.bf.txq6_empty)
				sport->dma_ctrl[idx].cnt.txq_empty++;
			if (reg_txq6_int.bf.txq6_overrun)
				sport->dma_ctrl[idx].cnt.txq_overrun++;
			if (reg_txq6_int.bf.txq6_cntmsb)
				sport->dma_ctrl[idx].cnt.txq_cntmsb++;
		} else {
			reg_txq7_int.wrd = reg;

			if (reg_txq7_int.bf.txq7_empty)
				sport->dma_ctrl[idx].cnt.txq_empty++;
			if (reg_txq7_int.bf.txq7_overrun)
				sport->dma_ctrl[idx].cnt.txq_overrun++;
			if (reg_txq7_int.bf.txq7_cntmsb)
				sport->dma_ctrl[idx].cnt.txq_cntmsb++;
		}
	} else {
		if (idx == 0) {
			reg_rxq6_int.wrd = reg;

			if (reg_rxq6_int.bf.rxq6_full)
				sport->dma_ctrl[idx].cnt.rxq_full++;
			if (reg_rxq6_int.bf.rxq6_overrun)
				sport->dma_ctrl[idx].cnt.rxq_overrun++;
			if (reg_rxq6_int.bf.rxq6_cntmsb)
				sport->dma_ctrl[idx].cnt.rxq_cntmsb++;
			if (reg_rxq6_int.bf.rxq6_full_drop_overrun)
				sport->dma_ctrl[idx].cnt.rxq_drop_overrun++;
			if (reg_rxq6_int.bf.rxq6_full_drop_cntmsb)
				sport->dma_ctrl[idx].cnt.rxq_drop_cntmsb++;
		} else {
			reg_rxq7_int.wrd = reg;

			if (reg_rxq7_int.bf.rxq7_full)
				sport->dma_ctrl[idx].cnt.rxq_full++;
			if (reg_rxq7_int.bf.rxq7_overrun)
				sport->dma_ctrl[idx].cnt.rxq_overrun++;
			if (reg_rxq7_int.bf.rxq7_cntmsb)
				sport->dma_ctrl[idx].cnt.rxq_cntmsb++;
			if (reg_rxq7_int.bf.rxq7_full_drop_overrun)
				sport->dma_ctrl[idx].cnt.rxq_drop_overrun++;
			if (reg_rxq7_int.bf.rxq7_full_drop_cntmsb)
				sport->dma_ctrl[idx].cnt.rxq_drop_cntmsb++;
		}
	}
}

static irqreturn_t cs75xx_sport_dma_desc_handler(int irq, void *dev_instance)
{
	struct cs75xx_sport *sport = (struct cs75xx_sport *)dev_instance;
	DMA_DMA_SSP_DESC_INTERRUPT_t reg_desc_int;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	reg_desc_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_DESC_INTERRUPT);
	if (reg_desc_int.bf.rx_des_err)
		dev_err(sport->dev, "ERROR!! DMA_SSP_RX_DESC_ERR\n");
	if (reg_desc_int.bf.tx_des_err)
		dev_err(sport->dev, "ERROR!! DMA_SSP_TX_DESC_ERR\n");
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_DESC_INTERRUPT, reg_desc_int.wrd);

	return IRQ_HANDLED;
}

static irqreturn_t cs75xx_sport_dma_tx_handler(int irq, void *dev_instance)
{
	struct cs75xx_sport *sport = (struct cs75xx_sport *)dev_instance;
	int idx = (irq == sport->irq_tx[0]) ? 0 : 1;
	DMA_DMA_SSP_TXQ6_INTERRUPT_t reg_txq6_int;
	DMA_DMA_SSP_TXQ7_INTERRUPT_t reg_txq7_int;
	u32 reg, eof = 0;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (idx == 0) {
		reg = reg_txq6_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ6_INTERRUPT);
		eof = reg_txq6_int.bf.txq6_eof;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_INTERRUPT, reg_txq6_int.wrd);
	} else {
		reg = reg_txq7_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TXQ7_INTERRUPT);
		eof = reg_txq7_int.bf.txq7_eof;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_INTERRUPT, reg_txq7_int.wrd);
	}
	cs75xx_sport_dma_cnt_update(idx, DIR_TX, reg);

	if (eof) {
		if (sport->dma_ctrl[idx].dma_tx_notifier)
			sport->dma_ctrl[idx].dma_tx_notifier(sport->dma_ctrl[idx].tx_data);
	}

	return IRQ_HANDLED;
}

static irqreturn_t cs75xx_sport_dma_rx_handler(int irq, void *dev_instance)
{
	struct cs75xx_sport *sport = (struct cs75xx_sport *)dev_instance;
	int idx = (irq == sport->irq_rx[0]) ? 0 : 1;
	DMA_DMA_SSP_RXQ6_INTERRUPT_t reg_rxq6_int;
	DMA_DMA_SSP_RXQ7_INTERRUPT_t reg_rxq7_int;
	u32 reg, eof = 0;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (idx == 0) {
		reg = reg_rxq6_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ6_INTERRUPT);
		eof = reg_rxq6_int.bf.rxq6_eof;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_INTERRUPT, reg_rxq6_int.wrd);
	} else {
		reg = reg_rxq7_int.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RXQ7_INTERRUPT);
		eof = reg_rxq7_int.bf.rxq7_eof;
		cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_INTERRUPT, reg_rxq7_int.wrd);
	}
	cs75xx_sport_dma_cnt_update(idx, DIR_RX, reg);

	if (eof) {
		if (sport->dma_ctrl[idx].dma_rx_notifier)
			sport->dma_ctrl[idx].dma_rx_notifier(sport->dma_ctrl[idx].rx_data);

	}

	return IRQ_HANDLED;
}

static int cs75xx_sport_dma_init(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	DMA_DMA_SSP_TXDMA_CONTROL_t reg_dma_tx_ctrl;
	DMA_DMA_SSP_RXDMA_CONTROL_t reg_dma_rx_ctrl;
	DMA_DMA_SSP_RXQ6_FULL_THRESHOLD_t reg_rx6_full_th;
	DMA_DMA_SSP_RXQ7_FULL_THRESHOLD_t reg_rx7_full_th;
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	/* turn on DMA top switch */
	reg_dma_rx_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_RX_CTRL);
	reg_dma_rx_ctrl.bf.rx_dma_enable = 1;
	reg_dma_rx_ctrl.bf.rx_check_own = 1;
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RX_CTRL, reg_dma_rx_ctrl.wrd);

	reg_dma_tx_ctrl.wrd = cs75xx_dma_reg_read(CS75XX_DMA_SSP_TX_CTRL);
	reg_dma_tx_ctrl.bf.tx_dma_enable = 1;
	reg_dma_tx_ctrl.bf.tx_check_own = 1;
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_TX_CTRL, reg_dma_tx_ctrl.wrd);

	/* disable interrupts */
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_INTENABLE, 0);
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_INTENABLE, 0);
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_INTENABLE, 0);
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_INTENABLE, 0);

	/* register handler */
	if (request_irq(sport->irq_desc, cs75xx_sport_dma_desc_handler, IRQF_SHARED, sport->irq_desc_name, sport)) {
		dev_err(&pdev->dev, "ERROR: can't register IRQ %s\n", sport->irq_desc_name);
		return -ENODEV;
	}
	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		if (request_irq(sport->irq_tx[i], cs75xx_sport_dma_tx_handler, 0, sport->irq_tx_name[i], sport)) {
			dev_err(&pdev->dev, "ERROR: can't register IRQ %s\n", sport->irq_tx_name[i]);
			return -ENODEV;
		}
		if (request_irq(sport->irq_rx[i], cs75xx_sport_dma_rx_handler, 0, sport->irq_rx_name[i], sport)) {
			dev_err(&pdev->dev, "ERROR: can't register IRQ %s\n", sport->irq_rx_name[i]);
			return -ENODEV;
		}
	}

	/* misc */
	reg_rx6_full_th.wrd = CS75XX_SPORT_MIN_DMA_FREE_DEPTH;
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_FULL_THRESHOLD, reg_rx6_full_th.wrd);
	reg_rx7_full_th.wrd = CS75XX_SPORT_MIN_DMA_FREE_DEPTH;
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_FULL_THRESHOLD, reg_rx7_full_th.wrd);

	return 0;
}


static int cs75xx_sport_dma_exit(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ6_INTENABLE, 0);
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_RXQ7_INTENABLE, 0);
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ6_INTENABLE, 0);
	cs75xx_dma_reg_write(CS75XX_DMA_SSP_TXQ7_INTENABLE, 0);

	free_irq(sport->irq_desc, sport);
	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		free_irq(sport->irq_tx[i], sport);
		free_irq(sport->irq_rx[i], sport);
	}

	return 0;
}

static int cs75xx_sport_dma_probe(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	struct resource *res_mem;
	char res_name[NAME_SIZE];
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	sprintf(res_name, "dma_ssp");
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_DMA, res_name);
	if (!res_mem) {
		dev_err(&pdev->dev, "%s: can't get resource %s\n", __func__, res_name);
		return -ENXIO;
	}
	sport->dma_base = ioremap(res_mem->start, resource_size(res_mem));
	if (!sport->dma_base) {
		dev_err(&pdev->dev, "%s: unable to remap %s %d memory \n",
			    __func__, res_name, resource_size(res_mem));
		return -ENXIO;
	}
	if (sport_debug & SPORT_INFO)
		printk("\tdma_base = 0x%p, range = 0x%x\n", sport->dma_base,
			resource_size(res_mem));

	sprintf(res_name, "dma_desc");
	sport->irq_desc = platform_get_irq_byname(pdev, res_name);
	if (sport->irq_desc == -ENXIO) {
		dev_err(&pdev->dev, "%s: can't get resource %s\n", __func__, res_name);
		return -ENXIO;
	}
	sprintf(sport->irq_desc_name, res_name);
	if (sport_debug & SPORT_INFO)
		printk("\t%s = %d\n", res_name, sport->irq_desc);

	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		sprintf(sport->irq_rx_name[i], "dma_rx_ssp%d", i);
		sport->irq_rx[i] = platform_get_irq_byname(pdev, sport->irq_rx_name[i]);
		if (sport->irq_rx[i] == -ENXIO) {
			dev_err(&pdev->dev, "%s: can't get resource %s\n",
						__func__, sport->irq_rx_name[i]);
			return -ENXIO;
		}
		if (sport_debug & SPORT_INFO)
			printk("\tirq_rx[%d] = %d\n", i, sport->irq_rx[i]);

		sprintf(sport->irq_tx_name[i], "dma_tx_ssp%d", i);
		sport->irq_tx[i] = platform_get_irq_byname(pdev, sport->irq_tx_name[i]);
		if (sport->irq_tx[i] == -ENXIO) {
			dev_err(&pdev->dev, "%s: can't get resource %s\n",
						__func__, sport->irq_tx_name[i]);
			return -ENXIO;
		}
		if (sport_debug & SPORT_INFO)
			printk("\tirq_tx[%d] = %d\n", i, sport->irq_tx[i]);
	}

	if (cs75xx_sport_dma_init(pdev))
		return -ENXIO;

	return 0;
}

static int cs75xx_sport_dma_remove(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_sport_dma_exit(pdev);

	iounmap(sport->dma_base);

	return 0;
}

static void cs75xx_sport_spdif_cnt_init(void)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	memset(&sport->spdif_ctrl.cnt, 0, sizeof(cs75xx_sport_spdif_cnt_t));
}

static void cs75xx_sport_spdif_cnt_disp(void)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (sport_debug & SPORT_INFO) {
		printk("SPDIF_FifoFull_int = %d\n", sport->spdif_ctrl.cnt.spdifFifoFull_int);
		printk("SPDIF_int = %d\n", sport->spdif_ctrl.cnt.spdif_int);
	}
}

static void cs75xx_sport_spdif_cnt_update(u32 reg)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_INTERRUPT_t reg_ssp_int;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	reg_ssp_int.wrd = reg;

	if (reg_ssp_int.bf.spdifFifoFull_int)
		sport->spdif_ctrl.cnt.spdifFifoFull_int++;
	if (reg_ssp_int.bf.spdif_int)
		sport->spdif_ctrl.cnt.spdif_int++;
}

static irqreturn_t cs75xx_sport_spdif_irq_handler(int irq, void *dev_instance)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	u32 reg = cs75xx_ssp_reg_read(0, CS75XX_SSP_INTERRUPT);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_sport_spdif_cnt_update(reg);

	return 0;
}


static int cs75xx_sport_spdif_init(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	sport->spdif_core.DataBaseAddress = (unsigned int)sport->spdif_base;
	sport->spdif_core.RegistersBaseAddress = (unsigned int)sport->spdif_base;
	if (CWda15_Identify(&sport->spdif_core)) {
		dev_err(&pdev->dev, "CWda15 IP Core not found at Address: 0x%08x\r\n",
					sport->spdif_core.RegistersBaseAddress);
		return -ENXIO;
	}

	cs75xx_sport_spdif_cnt_init();

	return 0;
}

static int cs75xx_sport_spdif_exit(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_sport_spdif_cnt_disp();

	sport->spdif_core.DataBaseAddress = 0;
	sport->spdif_core.RegistersBaseAddress = 0;

	return 0;
}

static int cs75xx_sport_spdif_probe(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	struct resource *res_mem;
	char res_name[NAME_SIZE];

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	sprintf(res_name, "spdif");
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, res_name);
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
					__func__, res_name);
		return -ENXIO;
	}
	sport->spdif_base = ioremap(res_mem->start, resource_size(res_mem));
	if (!sport->spdif_base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
			    __func__, res_name, resource_size(res_mem));
		return -ENXIO;
	}
	if (sport_debug & SPORT_INFO)
		printk("\tspdif_base = 0x%p, range = 0x%x\n",
			sport->spdif_base, resource_size(res_mem));

	sprintf(res_name, "irq_spdif");
	sport->irq_spdif = platform_get_irq_byname(pdev, res_name);
	if (sport->irq_spdif == -ENXIO) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
						__func__, res_name);
		return -ENXIO;
	}
	sprintf(sport->irq_spdif_name, res_name);
	if (sport_debug & SPORT_INFO)
		printk("\tirq_spdif = %d\n", sport->irq_spdif);

	spin_lock_init(&sport->spdif_lock);

	if (cs75xx_sport_spdif_init(pdev))
		return -ENXIO;

	return 0;
}

static int cs75xx_sport_spdif_remove(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_sport_spdif_exit(pdev);

	iounmap(sport->spdif_base);

	return 0;
}

static void cs75xx_sport_ssp_cnt_init(int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	memset(&sport->ssp_ctrl[idx].cnt, 0, sizeof(cs75xx_sport_ssp_cnt_t));
}

static void cs75xx_sport_ssp_cnt_disp(int idx)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	if (sport_debug & SPORT_INFO) {
		printk("SSP%d_txByteShiftErr = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.txByteShiftErr);
		printk("SSP%d_rxByteShiftErr = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.rxByteShiftErr);
		printk("SSP%d_rfsOvrRun = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.rfsOvrRun);
		printk("SSP%d_rfsUndRun = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.rfsUndRun);
		printk("SSP%d_rfrWtMk = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.rfrWtMk);
		printk("SSP%d_rfsEmpty = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.rfsEmpty);
		printk("SSP%d_rfsFull = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.rfsFull);
		printk("SSP%d_tfsOvrRun = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.tfsOvrRun);
		printk("SSP%d_tfsUndRun = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.tfsUndRun);
		printk("SSP%d_tfwWtMk = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.tfwWtMk);
		printk("SSP%d_tfsEmpty = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.tfsEmpty);
		printk("SSP%d_tfsFull = %d\n", idx,
			sport->ssp_ctrl[idx].cnt.tfsFull);
	}
}

static void cs75xx_sport_ssp_cnt_update(int idx, u32 reg)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	DMA_SSP_INTERRUPT_t reg_ssp_int;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	reg_ssp_int.wrd = reg;

	if (reg_ssp_int.bf.txByteShiftErr)
		sport->ssp_ctrl[idx].cnt.txByteShiftErr++;
	if (reg_ssp_int.bf.rxByteShiftErr)
		sport->ssp_ctrl[idx].cnt.rxByteShiftErr++;
	if (reg_ssp_int.bf.rfsOvrRun)
		sport->ssp_ctrl[idx].cnt.rfsOvrRun++;
	if (reg_ssp_int.bf.rfsUndRun)
		sport->ssp_ctrl[idx].cnt.rfsUndRun++;
	if (reg_ssp_int.bf.rfrWtMk)
		sport->ssp_ctrl[idx].cnt.rfrWtMk++;
	if (reg_ssp_int.bf.rfsEmpty)
		sport->ssp_ctrl[idx].cnt.rfsEmpty++;
	if (reg_ssp_int.bf.rfsFull)
		sport->ssp_ctrl[idx].cnt.rfsFull++;
	if (reg_ssp_int.bf.tfsOvrRun)
		sport->ssp_ctrl[idx].cnt.tfsOvrRun++;
	if (reg_ssp_int.bf.tfsUndRun)
		sport->ssp_ctrl[idx].cnt.tfsOvrRun++;
	if (reg_ssp_int.bf.tfwWtMk)
		sport->ssp_ctrl[idx].cnt.tfwWtMk++;
	if (reg_ssp_int.bf.tfsEmpty)
		sport->ssp_ctrl[idx].cnt.tfsEmpty++;
	if (reg_ssp_int.bf.tfsFull)
		sport->ssp_ctrl[idx].cnt.tfsFull++;
}

static irqreturn_t cs75xx_sport_ssp_irq_handler(int irq, void *dev_instance)
{
	struct cs75xx_sport *sport = platform_get_drvdata(cs75xx_sport_dev);
	int idx = (irq == sport->irq_tx[0]) ? 0 : 1;
	u32 reg = cs75xx_ssp_reg_read(idx, CS75XX_SSP_INTERRUPT);

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_sport_ssp_cnt_update(idx, reg);

	return 0;
}

static int cs75xx_sport_ssp_init(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		/* disable irq */
		cs75xx_ssp_reg_write(i, CS75XX_SSP_INTENABLE, 0);

		if (request_irq(sport->irq_ssp[i], cs75xx_sport_ssp_irq_handler, 0, sport->irq_ssp_name[i], sport)) {
			dev_err(&pdev->dev, "ERROR: can't register IRQ %s\n", sport->irq_ssp_name[i]);
			return -ENODEV;
		}
	}

	return 0;
}

static int cs75xx_sport_ssp_exit(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		/* disable irq */
		cs75xx_ssp_reg_write(i, CS75XX_SSP_INTENABLE, 0);
		free_irq(sport->irq_ssp[i], sport);
	}

	return 0;
}

static int cs75xx_sport_ssp_probe(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	struct resource *res_mem;
	char res_name[NAME_SIZE];
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		sprintf(res_name, "ssp%d", i);
		res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, res_name);
		if (!res_mem) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
						__func__, res_name);
			return -ENXIO;
		}
		sport->ssp_base[i] = ioremap(res_mem->start, resource_size(res_mem));
		if (!sport->ssp_base[i]) {
			dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
				    __func__, res_name, resource_size(res_mem));
			return -ENXIO;
		}
		if (sport_debug & SPORT_INFO)
			printk("\tssp_base[%d] = 0x%p, range = 0x%x\n",
				i, sport->ssp_base[i], resource_size(res_mem));

		sprintf(res_name, "irq_ssp%d", i);
		sport->irq_ssp[i] = platform_get_irq_byname(pdev, res_name);
		if (sport->irq_ssp[i] == -ENXIO) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
							__func__, res_name);
			return -ENXIO;
		}
		sprintf(sport->irq_ssp_name[i], res_name);
		if (sport_debug & SPORT_INFO)
			printk("\tirq_ssp[%d] = %d\n", i, sport->irq_ssp[i]);
	}

	if (cs75xx_sport_ssp_init(pdev))
		return -ENXIO;

	return 0;
}

static int cs75xx_sport_ssp_remove(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	cs75xx_sport_ssp_exit(pdev);

	for (i = 0; i < CS75XX_SSP_NUM; i++)
		iounmap(sport->ssp_base[i]);

	return 0;
}

#ifdef CONFIG_SND_SOC
extern int cs75xx_snd_dai_init(void);
extern void cs75xx_snd_dai_exit(void);
extern int __devinit cs75xx_soc_platform_probe(struct platform_device *pdev);
extern int __devexit cs75xx_soc_platform_remove(struct platform_device *pdev);

/*
extern int cs75xx_snd_plat_init(void);
extern void cs75xx_snd_plat_exit(void);
*/
#endif
static int cs75xx_sport_misc_init(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);
	int i;

	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		spin_lock_init(&sport->lock[i]);
		sport->type[i] = CS75XX_SPORT_UNDEF;
	}

#ifdef CONFIG_SND_SOC
	cs75xx_soc_platform_probe(pdev);
/* For Kernel3.3.8 Author:Rudolph
	cs75xx_snd_plat_init();
*/
	cs75xx_snd_dai_init();

#endif

	return 0;
}

static int cs75xx_sport_misc_exit(struct platform_device *pdev)
{
	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

#ifdef CONFIG_SND_SOC
	cs75xx_snd_dai_exit();
	cs75xx_soc_platform_remove(pdev);
/* For Kernel3.3.8 Author:Rudolph
	cs75xx_snd_plat_exit();
*/
#endif

	return 0;
}

static int __devinit cs75xx_sport_probe(struct platform_device *pdev)
{
	struct cs75xx_sport *sport;

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: %s %s-%s\n", __func__, pdev->name, __DATE__, __TIME__);

	sport = kzalloc(sizeof(struct cs75xx_sport), GFP_KERNEL);
	if (!sport) {
		dev_err(&pdev->dev, "Func: %s - can't allocate memory for %s device\n", __func__, "ssp");
		return -ENOMEM;
	}
	sport->dev = &pdev->dev;
	platform_set_drvdata(pdev, sport);
	cs75xx_sport_dev = pdev;

	if (cs75xx_sport_ssp_probe(pdev))
		goto fail;
	if (cs75xx_sport_spdif_probe(pdev))
		goto fail;
	if (cs75xx_sport_dma_probe(pdev))
		goto fail;
	if (cs75xx_sport_misc_init(pdev))
		goto fail;

	return 0;
fail:
	if (sport) {
		cs75xx_sport_misc_exit(pdev);
		cs75xx_sport_ssp_remove(pdev);
		cs75xx_sport_spdif_remove(pdev);
		cs75xx_sport_dma_remove(pdev);

		kfree(sport);
	}
	cs75xx_sport_dev = NULL;

	return -ENXIO;
}

static int __devexit cs75xx_sport_remove(struct platform_device *pdev)
{
	struct cs75xx_sport *sport = platform_get_drvdata(pdev);

	if (sport_debug & (SPORT_TRACE | SPORT_INFO))
		printk("%s: %s\n", __func__, pdev->name);

	cs75xx_sport_ssp_remove(pdev);
	cs75xx_sport_spdif_remove(pdev);
	cs75xx_sport_dma_remove(pdev);

	kfree(sport);

	platform_set_drvdata(pdev, NULL);

	cs75xx_sport_dev = NULL;

	return 0;
}

static struct platform_driver cs75xx_sport_platform_driver = {
	.probe	= cs75xx_sport_probe,
	.remove	= __devexit_p(cs75xx_sport_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name  = "cs75xx-sport",
	},
};

int __init cs75xx_sport_init(void)
{
	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	return platform_driver_register(&cs75xx_sport_platform_driver);
}

void __exit cs75xx_sport_exit(void)
{
	if (sport_debug & SPORT_TRACE)
		printk("%s\n", __func__);

	platform_driver_unregister(&cs75xx_sport_platform_driver);
}

module_init(cs75xx_sport_init);
module_exit(cs75xx_sport_exit);

module_param_named(debug, sport_debug, uint, 0644);
MODULE_PARM_DESC(debug, "sport module debug flag\n");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX SPORT driver");

