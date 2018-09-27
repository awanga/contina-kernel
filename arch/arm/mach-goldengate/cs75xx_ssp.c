/*
 * FILE NAME cs75xx-ssp.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX SSP interface.
 *
 *  Copyright 2010 Cortina , Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#define DMA_FLUSH

#include <linux/string.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/memory.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <mach/cs75xx_ssp.h>
#include <mach/hardware.h>

//#define SSP_DIS_TIMEOUT     (HZ/10)	/* ForceTc timeout */

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
	dma_addr_t buf_addr;
	u32 reserved[2];
} cs75xx_dma_desc_t;

typedef struct {
	char name[16];
	cs_reg ssp_irq_status;
	cs75xx_ssp_profile_t profile;
	u8 tx_en;
	u8 tx_swap;
	u8 rx_en;
	u8 rx_swap;

	void (*dma_tx_hander)(void);
	void (*dma_rx_hander)(void);
	u16 tx_buf_num, rx_buf_num;
	u16 tx_buf_size, rx_buf_size;
	//cs_addr tx_buf_vaddr, rx_buf_vaddr;	/* virtual addr */
	dma_addr_t tx_desc_paddr, rx_desc_paddr;	/* physical addr */
	cs75xx_dma_desc_t *tx_dma_desc, *rx_dma_desc;
	cs_reg dma_irq_tx_status;
	cs_reg dma_irq_rx_status;

	u16 rxq_full;
	u16 rxq_overrun;
	u16 rxq_cntmsb;
	u16 rxq_drop_overrun;
	u16 rxq_drop_cntmsb;
	u16 txq_empty;
	u16 txq_overrun;
	u16 txq_cntmsb;
} cs75xx_ssp_dma_ctrl_t;

struct cs75xx_ssp {
	struct device	*dev;
	void __iomem	*ssp_base[CS75XX_SSP_NUM];
	/* DMA SSP INT dispatcher */
	int		irq_desc;
	char		irq_desc_name[16];
	int		irq_rx[CS75XX_SSP_NUM];
	char		irq_rx_name[CS75XX_SSP_NUM][16];
	int		irq_tx[CS75XX_SSP_NUM];
	char		irq_tx_name[CS75XX_SSP_NUM][16];
	int		irq_ssp[CS75XX_SSP_NUM];
	char		irq_ssp_name[CS75XX_SSP_NUM][16];
	void __iomem	*dma_base;
	spinlock_t	lock[CS75XX_SSP_NUM];
	cs75xx_ssp_dma_ctrl_t ctrl[CS75XX_SSP_NUM];
};

static struct platform_device *cs75xx_ssp_dev = NULL;

#define cs75xx_ssp_read_reg(index, offset)		(readl(ssp->ssp_base[index]+offset))
#define cs75xx_ssp_write_reg(index, offset, val)	(writel(val, ssp->ssp_base[index]+offset))
#define cs75xx_dma_read_reg(offset)			(readl(ssp->dma_base+offset))
#define cs75xx_dma_write_reg(offset, val)		(writel(val, ssp->dma_base+offset))



/* SSP Register Map */
#define CS75XX_SSP_ID					0x00
#define CS75XX_SSP_CTRL0				0x04
#define CS75XX_SSP_CTRL1				0x08
#define CS75XX_SSP_FRAME_CTRL0				0x0C
#define CS75XX_SSP_FRAME_CTRL1				0x10
#define CS75XX_SSP_BAUD_RATE				0x14
#define CS75XX_SSP_FIFO_CTRL				0x18
#define CS75XX_SSP_FIFO_PTR				0x1C
#define CS75XX_SSP_INTERRUPT				0x20
#define CS75XX_SSP_INTENABLE				0x24
#define CS75XX_SSP_TXSLOT_VLD0				0x28
#define CS75XX_SSP_TXSLOT_VLD1				0x2C
#define CS75XX_SSP_TXSLOT_VLD2				0x30
#define CS75XX_SSP_TXSLOT_VLD3				0x34
#define CS75XX_SSP_RXSLOT_VLD0				0x38
#define CS75XX_SSP_RXSLOT_VLD1				0x3C
#define CS75XX_SSP_RXSLOT_VLD2				0x40
#define CS75XX_SSP_RXSLOT_VLD3				0x44
#define CS75XX_SSP_SLOT_SIZE0				0x48
#define CS75XX_SSP_SLOT_SIZE1				0x4C
#define CS75XX_SSP_SLOT_SIZE2				0x50
#define CS75XX_SSP_SLOT_SIZE3				0x54

#define SSP_DEV_ID(index)       ((cs75xx_ssp_read_reg(index, CS75XX_SSP_ID) & 0x00FFFF00) >> 8)
#define SSP_REV_ID(index)       (cs75xx_ssp_read_reg(index, CS75XX_SSP_ID) & 0x000000FF)

/* DMA Register Map */
#define CS75XX_DMA_SSP_RX_CTRL				0x00
#define CS75XX_DMA_SSP_TX_CTRL				0x04
#define CS75XX_DMA_SSP_RXQ5_CTRL			0x08
#define CS75XX_DMA_SSP_RXQ6_CTRL			0x0C
#define CS75XX_DMA_SSP_RXQ7_CTRL			0x10
#define CS75XX_DMA_SSP_TXQ5_CTRL			0x14
#define CS75XX_DMA_SSP_TXQ6_CTRL			0x18
#define CS75XX_DMA_SSP_TXQ7_CTRL			0x1C
#define CS75XX_DMA_SSP_RXQ5_PKTCNT_READ			0x20
#define CS75XX_DMA_SSP_RXQ6_PKTCNT_READ			0x24
#define CS75XX_DMA_SSP_RXQ7_PKTCNT_READ			0x28
#define CS75XX_DMA_SSP_TXQ5_PKTCNT_READ			0x2C
#define CS75XX_DMA_SSP_TXQ6_PKTCNT_READ			0x30
#define CS75XX_DMA_SSP_TXQ7_PKTCNT_READ			0x34
#define CS75XX_DMA_SSP_RXQ5_BASE_DEPTH			0x38
#define CS75XX_DMA_SSP_RXQ6_BASE_DEPTH			0x3C
#define CS75XX_DMA_SSP_RXQ7_BASE_DEPTH			0x40
#define CS75XX_DMA_SSP_RXQ5_WPTR			0x44
#define CS75XX_DMA_SSP_RXQ5_RPTR			0x48
#define CS75XX_DMA_SSP_RXQ6_WPTR			0x4C
#define CS75XX_DMA_SSP_RXQ6_RPTR			0x50
#define CS75XX_DMA_SSP_RXQ7_WPTR			0x54
#define CS75XX_DMA_SSP_RXQ7_RPTR			0x58
#define CS75XX_DMA_SSP_TXQ5_BASE_DEPTH			0x5C
#define CS75XX_DMA_SSP_TXQ6_BASE_DEPTH			0x60
#define CS75XX_DMA_SSP_TXQ7_BASE_DEPTH			0x64
#define CS75XX_DMA_SSP_TXQ5_WPTR			0x68
#define CS75XX_DMA_SSP_TXQ5_RPTR			0x6C
#define CS75XX_DMA_SSP_TXQ6_WPTR			0x70
#define CS75XX_DMA_SSP_TXQ6_RPTR			0x74
#define CS75XX_DMA_SSP_TXQ7_WPTR			0x78
#define CS75XX_DMA_SSP_TXQ7_RPTR			0x7C
#define CS75XX_DMA_SSP_RXQ5_FULL_THRESHOLD		0x80
#define CS75XX_DMA_SSP_RXQ6_FULL_THRESHOLD		0x84
#define CS75XX_DMA_SSP_RXQ7_FULL_THRESHOLD		0x88
#define CS75XX_DMA_SSP_RXQ5_PKTCNT			0x8C
#define CS75XX_DMA_SSP_RXQ6_PKTCNT			0x90
#define CS75XX_DMA_SSP_RXQ7_PKTCNT			0x94
#define CS75XX_DMA_SSP_RXQ5_FULL_DROP_PKTCNT		0x98
#define CS75XX_DMA_SSP_RXQ6_FULL_DROP_PKTCNT		0x9C
#define CS75XX_DMA_SSP_RXQ7_FULL_DROP_PKTCNT		0xA0
#define CS75XX_DMA_SSP_TXQ5_PKTCNT			0xA4
#define CS75XX_DMA_SSP_TXQ6_PKTCNT			0xA8
#define CS75XX_DMA_SSP_TXQ7_PKTCNT			0xAC
#define CS75XX_DMA_SSP_INTERRUPT_0			0xB0
#define CS75XX_DMA_SSP_INTENABLE_0			0xB4
#define CS75XX_DMA_SSP_INTERRUPT_1			0xB8
#define CS75XX_DMA_SSP_INTENABLE_1			0xBC
#define CS75XX_DMA_SSP_DESC_INTERRUPT			0xC0
#define CS75XX_DMA_SSP_DESC_INTENABLE			0xC4
#define CS75XX_DMA_SSP_RXQ5_INTERRUPT			0xC8
#define CS75XX_DMA_SSP_RXQ5_INTENABLE			0xCC
#define CS75XX_DMA_SSP_RXQ6_INTERRUPT			0xD0
#define CS75XX_DMA_SSP_RXQ6_INTENABLE			0xD4
#define CS75XX_DMA_SSP_RXQ7_INTERRUPT			0xD8
#define CS75XX_DMA_SSP_RXQ7_INTENABLE			0xDC
#define CS75XX_DMA_SSP_TXQ5_INTERRUPT			0xE0
#define CS75XX_DMA_SSP_TXQ5_INTENABLE			0xE4
#define CS75XX_DMA_SSP_TXQ6_INTERRUPT			0xE8
#define CS75XX_DMA_SSP_TXQ6_INTENABLE			0xEC
#define CS75XX_DMA_SSP_TXQ7_INTERRUPT			0xF0
#define CS75XX_DMA_SSP_TXQ7_INTENABLE			0xF4


#define CS75XX_DMA_SSP_DEPTH_MIN			3
#define CS75XX_DMA_SSP_DEPTH_MAX			13
#define CS75XX_DMA_WPTR_RPTR_MASK			0x00001FFF

static bool hw_ssp_eco_r0 = TRUE; /* BUG#32098 */

static u8 sclk_gen_params[5][5] = {
	/*	m,	n,	div,	a1,	a2 */
	[0] = {63,	27,	60,	1,	2}, /* 1024KHz - 0.0000(ppm) */
	[1] = {13,	2,	40,	2,	1}, /* 1536KHz - 9.1380(ppm) */
	[2] = {27,	1,	30,	2,	1}, /* 2048KHz - 9.1380(ppm) */
	[3] = {27,	3,	14,	1,	1}, /* 4096KHz - 9.1711(ppm) */
	[4] = {8,	2,	7,	2,	2}, /* 8096KHz - 30.8633(ppm) */
};


/*******************************************************************************
 * misc
 ******************************************************************************/
static int two_power(int value)
{
	int i;

	for (i = CS75XX_DMA_SSP_DEPTH_MIN; i <= CS75XX_DMA_SSP_DEPTH_MAX; i++)
		if (value == (0x0001 << i))
			return i;
	return 0;
}

static int sclk_param_select(u32 sclk)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	switch (sclk) {
	case 1024:
		return 0;
	case 1536:
		return 1;
	case 2048:
		return 2;
	case 4096:
		return 3;
	case 8096:
		return 4;
	}

	dev_err(ssp->dev, "Func: %s - invalid SCLK select %d\n", __func__, sclk);
	return -1;
}


/*******************************************************************************
 * DMA
 ******************************************************************************/
void cs75xx_dma_tx_disable(int disable)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	DMA_DMA_SSP_TXDMA_CONTROL_t reg_dma_tx_ctrl;

	reg_dma_tx_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TX_CTRL);
	reg_dma_tx_ctrl.bf.tx_dma_enable = disable ? 0 : 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TX_CTRL, reg_dma_tx_ctrl.wrd);
}
EXPORT_SYMBOL(cs75xx_dma_tx_disable);

void cs75xx_dma_rx_disable(int disable)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	DMA_DMA_SSP_RXDMA_CONTROL_t reg_dma_rx_ctrl;

	reg_dma_rx_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RX_CTRL);
	reg_dma_rx_ctrl.bf.rx_dma_enable = disable ? 0 : 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_RX_CTRL, reg_dma_rx_ctrl.wrd);
}
EXPORT_SYMBOL(cs75xx_dma_rx_disable);

int cs75xx_dma_ssp_register(
	u32  index,
	dma_addr_t tbuf_paddr,
	u32  tx_buf_num,
	u32  tx_buf_size,
	dma_addr_t rbuf_paddr,
	u32  rx_buf_num,
	u32  rx_buf_size)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	cs75xx_dma_desc_t *tmp_desc;
	int i, power;

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	if (tbuf_paddr != 0) {
		if (tx_buf_size == 0)
			goto init_fail;
		power = two_power(tx_buf_num);
		if (power < CS75XX_DMA_SSP_DEPTH_MIN || power > CS75XX_DMA_SSP_DEPTH_MAX)
			goto init_fail;

		/* allocate and config descriptor */
		ssp->ctrl[index].tx_buf_num = tx_buf_num;
		ssp->ctrl[index].tx_buf_size = tx_buf_size;
		//ssp->ctrl[index].tx_buf_vaddr = (cs_addr)tbuf;
		ssp->ctrl[index].tx_dma_desc = dma_alloc_coherent(ssp->dev, tx_buf_num*sizeof(cs75xx_dma_desc_t), &ssp->ctrl[index].tx_desc_paddr, GFP_KERNEL|GFP_DMA);
		//ssp->ctrl[index].tx_dma_desc = kmalloc(tx_buf_num*sizeof(cs75xx_dma_desc_t), GFP_ATOMIC);
		if (ssp->ctrl[index].tx_dma_desc == NULL)
			goto init_fail;
		else
			dev_dbg(ssp->dev, "tx_dma_desc = 0x%p, tx_desc_paddr = 0x%08X\n", ssp->ctrl[index].tx_dma_desc, ssp->ctrl[index].tx_desc_paddr);

		for (i = 0; i < tx_buf_num; i++) {
			tmp_desc = (cs75xx_dma_desc_t *)(ssp->ctrl[index].tx_dma_desc + i);

			tmp_desc->own = 1;	// CPU
			tmp_desc->misc = 0;
			//tmp_desc->desccnt = x;
			tmp_desc->buf_size = tx_buf_size;
			tmp_desc->buf_addr = tbuf_paddr + i*tx_buf_size;
		}

		/* set DMA register */
		if (index == 0) {
			DMA_DMA_SSP_TXQ6_BASE_DEPTH_t reg_txq6_base;

			reg_txq6_base.wrd = 0;
			reg_txq6_base.bf.base =  ssp->ctrl[index].tx_desc_paddr >> 4;
			reg_txq6_base.bf.depth = power;
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_BASE_DEPTH, reg_txq6_base.wrd);
		}
		else if (index == 1) {
			DMA_DMA_SSP_TXQ7_BASE_DEPTH_t reg_txq7_base;

			reg_txq7_base.wrd = 0;
			reg_txq7_base.bf.base =  ssp->ctrl[index].tx_desc_paddr >> 4;
			reg_txq7_base.bf.depth = power;
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_BASE_DEPTH, reg_txq7_base.wrd);
		}
	}

	if (rbuf_paddr != 0) {
		if (rx_buf_size == 0)
			goto init_fail;
		power = two_power(rx_buf_num);
		if (power < CS75XX_DMA_SSP_DEPTH_MIN || power > CS75XX_DMA_SSP_DEPTH_MAX)
			goto init_fail;

		/* allocate and config descriptor */
		ssp->ctrl[index].rx_buf_num = rx_buf_num;
		ssp->ctrl[index].rx_buf_size = rx_buf_size;
		//ssp->ctrl[index].rx_buf_vaddr = (cs_addr)rbuf;
		ssp->ctrl[index].rx_dma_desc = dma_alloc_coherent(ssp->dev, rx_buf_num*sizeof(cs75xx_dma_desc_t), &ssp->ctrl[index].rx_desc_paddr, GFP_KERNEL|GFP_DMA);
		//ssp->ctrl[index].rx_dma_desc = kmalloc(rx_buf_num*sizeof(cs75xx_dma_desc_t), GFP_ATOMIC);
		if (ssp->ctrl[index].rx_dma_desc == NULL)
			goto init_fail;
		else
			dev_dbg(ssp->dev, "rx_dma_desc = 0x%p, rx_desc_paddr = 0x%08X\n", ssp->ctrl[index].rx_dma_desc, ssp->ctrl[index].rx_desc_paddr);

		for (i = 0; i < rx_buf_num; i++) {
			tmp_desc = (cs75xx_dma_desc_t *)(ssp->ctrl[index].rx_dma_desc + i);

			tmp_desc->own = 0;	// DMA
			tmp_desc->misc = 0;
			//tmp_desc->desccnt = x;
			tmp_desc->buf_size = rx_buf_size;
			tmp_desc->buf_addr = rbuf_paddr + i*rx_buf_size;
		}

		/* set DMA register */
		if (index == 0) {
			DMA_DMA_SSP_RXQ6_BASE_DEPTH_t reg_rxq6_base;

			reg_rxq6_base.wrd = 0;
			reg_rxq6_base.bf.base =  ssp->ctrl[index].rx_desc_paddr >> 4;
			reg_rxq6_base.bf.depth = power;
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_BASE_DEPTH, reg_rxq6_base.wrd);
		}
		else if (index == 1) {
			DMA_DMA_SSP_RXQ7_BASE_DEPTH_t reg_rxq7_base;

			reg_rxq7_base.wrd = 0;
			reg_rxq7_base.bf.base =  ssp->ctrl[index].rx_desc_paddr >> 4;
			reg_rxq7_base.bf.depth = power;
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_BASE_DEPTH, reg_rxq7_base.wrd);
		}
	}

	if (0) {
		int i;
		cs75xx_dma_desc_t *tmp_desc;

		for (i = 0; i < ssp->ctrl[index].tx_buf_num; i++) {
			tmp_desc = (cs75xx_dma_desc_t *)(ssp->ctrl[index].tx_dma_desc + i);
			//memcpy(&tmp_desc, (ssp->ctrl.tx_dma_desc + i*sizeof(cs75xx_dma_desc_t)), sizeof(cs75xx_dma_desc_t));
			printk("tx des[%d].own = %d\n", i, tmp_desc->own);
			printk("tx des[%d].misc = %d\n", i, tmp_desc->misc);
			printk("tx des[%d].desccnt = %d\n", i, tmp_desc->desccnt);
			printk("tx des[%d].buf_size = %d\n", i, tmp_desc->buf_size);
			printk("tx des[%d].buf_addr = 0x%08X\n", i, tmp_desc->buf_addr);
		}

		for (i = 0; i < ssp->ctrl[index].rx_buf_num; i++) {
			tmp_desc = (cs75xx_dma_desc_t *)(ssp->ctrl[index].rx_dma_desc + i);
			//memcpy(&tmp_desc, (ssp->ctrl.rx_dma_desc + i*sizeof(cs75xx_dma_desc_t)), sizeof(cs75xx_dma_desc_t));
			printk("rx des[%d].own = %d\n", i, tmp_desc->own);
			printk("rx des[%d].misc = %d\n", i, tmp_desc->misc);
			printk("rx des[%d].desccnt = %d\n", i, tmp_desc->desccnt);
			printk("rx des[%d].buf_size = %d\n", i, tmp_desc->buf_size);
			printk("rx des[%d].buf_addr = 0x%08X\n", i, tmp_desc->buf_addr);
		}
	}

	return 0;

init_fail:
	if (ssp->ctrl[index].tx_dma_desc)
		//kfree(ssp->ctrl[index].tx_dma_desc);
		dma_free_coherent(ssp->dev, ssp->ctrl[index].tx_buf_num*sizeof(cs75xx_dma_desc_t),
			ssp->ctrl[index].tx_dma_desc, ssp->ctrl[index].tx_desc_paddr);
	if (ssp->ctrl[index].rx_dma_desc)
		//kfree(ssp->ctrl[index].rx_dma_desc);
		dma_free_coherent(ssp->dev, ssp->ctrl[index].rx_buf_num*sizeof(cs75xx_dma_desc_t),
			ssp->ctrl[index].rx_dma_desc, ssp->ctrl[index].rx_desc_paddr);

	return -EINVAL;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_register);

int cs75xx_dma_ssp_unregister(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	if (ssp->ctrl[index].tx_dma_desc)
		//kfree(ssp->ctrl[index].tx_dma_desc);
		dma_free_coherent(ssp->dev, ssp->ctrl[index].tx_buf_num*sizeof(cs75xx_dma_desc_t),
			ssp->ctrl[index].tx_dma_desc, ssp->ctrl[index].tx_desc_paddr);
	if (ssp->ctrl[index].rx_dma_desc)
		//kfree(ssp->ctrl[index].rx_dma_desc);
		dma_free_coherent(ssp->dev, ssp->ctrl[index].rx_buf_num*sizeof(cs75xx_dma_desc_t),
			ssp->ctrl[index].rx_dma_desc, ssp->ctrl[index].rx_desc_paddr);

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_unregister);

int request_cs75xx_dma_ssp_irq(u32 index, void (*tx_handler)(void), void (*rx_handler)(void))
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	ssp->ctrl[index].dma_tx_hander = tx_handler;
	ssp->ctrl[index].dma_rx_hander = rx_handler;

	return 0;
}
EXPORT_SYMBOL(request_cs75xx_dma_ssp_irq);

int free_cs75xx_dma_ssp_irq(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	ssp->ctrl[index].dma_tx_hander = NULL;
	ssp->ctrl[index].dma_rx_hander = NULL;

	return 0;
}
EXPORT_SYMBOL(free_cs75xx_dma_ssp_irq);

int cs75xx_dma_ssp_tx_ptr(u32 index, u16 *dma_wt_ptr_p, u16 *dma_rd_ptr_p)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	//dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	if (index == 0) {
		DMA_DMA_SSP_TXQ6_WPTR_t reg_txq6_wptr;
		DMA_DMA_SSP_TXQ6_RPTR_t reg_txq6_rptr;

		reg_txq6_wptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_WPTR);
		*dma_wt_ptr_p = reg_txq6_wptr.bf.index;

		reg_txq6_rptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_RPTR);
		*dma_rd_ptr_p = reg_txq6_rptr.bf.index;

	}
	else if (index == 1) {
		DMA_DMA_SSP_TXQ7_WPTR_t reg_txq7_wptr;
		DMA_DMA_SSP_TXQ7_RPTR_t reg_txq7_rptr;

		reg_txq7_wptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_WPTR);
		*dma_wt_ptr_p = reg_txq7_wptr.bf.index;

		reg_txq7_rptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_RPTR);
		*dma_rd_ptr_p = reg_txq7_rptr.bf.index;
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_tx_ptr);

int cs75xx_dma_ssp_rx_ptr(u32 index, u16 *dma_wt_ptr_p, u16 *dma_rd_ptr_p)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	//dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	if (index == 0) {
		DMA_DMA_SSP_TXQ6_WPTR_t reg_rxq6_wptr;
		DMA_DMA_SSP_TXQ6_RPTR_t reg_rxq6_rptr;

		reg_rxq6_wptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_WPTR);
		*dma_wt_ptr_p = reg_rxq6_wptr.bf.index;

		reg_rxq6_rptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_RPTR);
		*dma_rd_ptr_p = reg_rxq6_rptr.bf.index;

	}
	else if (index == 1) {
		DMA_DMA_SSP_TXQ7_WPTR_t reg_rxq7_wptr;
		DMA_DMA_SSP_TXQ7_RPTR_t reg_rxq7_rptr;

		reg_rxq7_wptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_WPTR);
		*dma_wt_ptr_p = reg_rxq7_wptr.bf.index;

		reg_rxq7_rptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_RPTR);
		*dma_rd_ptr_p = reg_rxq7_rptr.bf.index;
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_rx_ptr);

int cs75xx_dma_ssp_tx_update(u32 index, u16 dma_wt_ptr)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	u16 txq_wptr, txq_rptr;
	int change = 0;

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	cs75xx_dma_ssp_tx_ptr(index, &txq_wptr, &txq_rptr);

	/* update buffer descriptor own bit */
	while (txq_wptr != dma_wt_ptr) {
		dma_map_single(NULL, __va(ssp->ctrl[index].tx_dma_desc[txq_wptr].buf_addr), ssp->ctrl[index].tx_buf_size, DMA_TO_DEVICE);

		if (ssp->ctrl[index].tx_dma_desc[txq_wptr].own == 0) {
			printk("Can't Update SSP%d Tx-(w:%d,r:%d) to %d\n",
			        index, txq_wptr, txq_rptr, dma_wt_ptr);
			break;
		}
		change = 1;

		ssp->ctrl[index].tx_dma_desc[txq_wptr].own = 0; // DMA
		ssp->ctrl[index].tx_dma_desc[txq_wptr].buf_size = ssp->ctrl[index].tx_buf_size;
		txq_wptr = (txq_wptr + 1) % ssp->ctrl[index].tx_buf_num;
	}

	if (change == 1) {
		if (index == 0)
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_WPTR, txq_wptr);
		else if (index == 1)
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_WPTR, txq_wptr);
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_tx_update);

int cs75xx_dma_ssp_rx_update(u32 index, u16 dma_rd_ptr)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	u16 rxq_wptr, rxq_rptr;
	int change = 0;

	if (unlikely(index >= CS75XX_SSP_NUM)) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	cs75xx_dma_ssp_rx_ptr(index, &rxq_wptr, &rxq_rptr);

	/* update buffer descriptor own bit */
	while (rxq_rptr != dma_rd_ptr) {
		if (ssp->ctrl[index].rx_dma_desc[rxq_rptr].own == 0) {
			printk("Can't Update SSP%d Rx-(w:%d,r:%d) to %d\n",
				index, rxq_wptr, rxq_rptr, dma_rd_ptr);
			break;
		}
		change = 1;

		ssp->ctrl[index].rx_dma_desc[rxq_rptr].own = 0; // DMA
		ssp->ctrl[index].rx_dma_desc[rxq_rptr].buf_size = ssp->ctrl[index].rx_buf_size;
		rxq_rptr = (rxq_rptr + 1) % ssp->ctrl[index].rx_buf_num;
	}

	if (change == 1) {
		if (index == 0)
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_RPTR, rxq_rptr);
		else if (index == 1)
			cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_RPTR, rxq_rptr);
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_rx_update);

int cs75xx_dma_ssp_tx_enable(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (index == 0) {
		DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
		DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;

		reg_txq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_CTRL);
#ifdef DMA_FLUSH
		reg_txq6_ctrl.bf.txq6_flush_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);
#endif
		reg_txq6_ctrl.bf.txq6_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

		reg_txq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE);
		reg_txq6_inten.bf.txq6_eof_en = 1;
		reg_txq6_inten.bf.txq6_empty_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE, reg_txq6_inten.wrd);
	}
	else if (index == 1) {
		DMA_DMA_SSP_TXQ7_CONTROL_t reg_txq7_ctrl;
		DMA_DMA_SSP_TXQ7_INTENABLE_t reg_txq7_inten;

		reg_txq7_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_CTRL);
#ifdef DMA_FLUSH
		reg_txq7_ctrl.bf.txq7_flush_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);
#endif
		reg_txq7_ctrl.bf.txq7_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);

		reg_txq7_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_INTENABLE);
		reg_txq7_inten.bf.txq7_eof_en = 1;
		reg_txq7_inten.bf.txq7_empty_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_INTENABLE, reg_txq7_inten.wrd);
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_tx_enable);

int cs75xx_dma_ssp_tx_disable(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
#ifdef DMA_FLUSH
	cs75xx_dma_desc_t *tmp_desc;
	u16 txq_wptr, txq_rptr;
	int i;
#endif

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (index == 0) {
		DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
		DMA_DMA_SSP_TXQ6_INTERRUPT_t reg_txq6_int;
		DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;

		reg_txq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_CTRL);
		reg_txq6_ctrl.bf.txq6_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);
#ifdef DMA_FLUSH
		reg_txq6_ctrl.bf.txq6_flush_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);
#endif

		reg_txq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE);
		reg_txq6_inten.bf.txq6_eof_en = 0;
		reg_txq6_inten.bf.txq6_empty_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE, reg_txq6_inten.wrd);

		reg_txq6_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTERRUPT);
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTERRUPT, reg_txq6_int.wrd);

#ifdef DMA_FLUSH
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_WPTR, 0);
#endif
	}
	else if (index == 1) {
		DMA_DMA_SSP_TXQ7_CONTROL_t reg_txq7_ctrl;
		DMA_DMA_SSP_TXQ7_INTERRUPT_t reg_txq7_int;
		DMA_DMA_SSP_TXQ7_INTENABLE_t reg_txq7_inten;

		reg_txq7_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_CTRL);
		reg_txq7_ctrl.bf.txq7_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);
#ifdef DMA_FLUSH
		reg_txq7_ctrl.bf.txq7_flush_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);
#endif

		reg_txq7_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_INTENABLE);
		reg_txq7_inten.bf.txq7_eof_en = 0;
		reg_txq7_inten.bf.txq7_empty_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_INTENABLE, reg_txq7_inten.wrd);

		reg_txq7_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_INTERRUPT);
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_INTERRUPT, reg_txq7_int.wrd);

#ifdef DMA_FLUSH
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_WPTR, 0);
#endif
	}

#ifdef DMA_FLUSH
	/* reset DMA descriptor */
	for (i = 0; i < ssp->ctrl[index].tx_buf_num; i++) {
		tmp_desc = (cs75xx_dma_desc_t *)(ssp->ctrl[index].tx_dma_desc + i);
		tmp_desc->own = 1;	// CPU
		tmp_desc->misc = 0;
		//tmp_desc->desccnt = x;
		tmp_desc->buf_size = ssp->ctrl[index].tx_buf_size;
		//tmp_desc->buf_addr = rbuf_paddr + i*rx_buf_size;
	}

	cs75xx_dma_ssp_tx_ptr(index, &txq_wptr, &txq_rptr);
#endif

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_tx_disable);

int cs75xx_dma_ssp_rx_enable(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	/* disable Rx Interrupt */
	if (index == 0) {
		DMA_DMA_SSP_RXQ6_INTENABLE_t reg_rxq6_inten;
#ifdef DMA_FLUSH
		DMA_DMA_SSP_RXQ6_CONTROL_t reg_rxq6_ctrl;

		reg_rxq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_CTRL);
		reg_rxq6_ctrl.bf.rxq6_flush_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_CTRL, reg_rxq6_ctrl.wrd);
#endif

		reg_rxq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_INTENABLE);
		reg_rxq6_inten.bf.rxq6_eof_en = 1;
		reg_rxq6_inten.bf.rxq6_full_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_INTENABLE, reg_rxq6_inten.wrd);
	}
	else if (index == 1) {
		DMA_DMA_SSP_RXQ7_INTENABLE_t reg_rxq7_inten;
#ifdef DMA_FLUSH
		DMA_DMA_SSP_RXQ7_CONTROL_t reg_rxq7_ctrl;

		reg_rxq7_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_CTRL);
		reg_rxq7_ctrl.bf.rxq7_flush_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_CTRL, reg_rxq7_ctrl.wrd);
#endif

		reg_rxq7_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_INTENABLE);
		reg_rxq7_inten.bf.rxq7_eof_en = 1;
		reg_rxq7_inten.bf.rxq7_full_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_INTENABLE, reg_rxq7_inten.wrd);
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_rx_enable);

int cs75xx_dma_ssp_rx_disable(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	u16 wt_ptr, rd_ptr;
#ifdef DMA_FLUSH
	cs75xx_dma_desc_t *tmp_desc;
	int i;
#endif

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	/* flush left data */
	cs75xx_dma_ssp_rx_ptr(index, &wt_ptr, &rd_ptr);
	cs75xx_dma_ssp_rx_update(index, wt_ptr);

	/* disable Rx Interrupt */
	if (index == 0) {
		DMA_DMA_SSP_RXQ6_INTERRUPT_t reg_rxq6_int;
		DMA_DMA_SSP_RXQ6_INTENABLE_t reg_rxq6_inten;
#ifdef DMA_FLUSH
		DMA_DMA_SSP_RXQ6_CONTROL_t reg_rxq6_ctrl;
#endif

		reg_rxq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_INTENABLE);
		reg_rxq6_inten.bf.rxq6_eof_en = 0;
		reg_rxq6_inten.bf.rxq6_full_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_INTENABLE, reg_rxq6_inten.wrd);

		reg_rxq6_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_INTERRUPT);
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_INTERRUPT, reg_rxq6_int.wrd);

#ifdef DMA_FLUSH
		reg_rxq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_CTRL);
		reg_rxq6_ctrl.bf.rxq6_flush_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_CTRL, reg_rxq6_ctrl.wrd);

		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_RPTR, 0);
#endif
	}
	else if (index == 1) {
		DMA_DMA_SSP_RXQ7_INTERRUPT_t reg_rxq7_int;
		DMA_DMA_SSP_RXQ7_INTENABLE_t reg_rxq7_inten;
#ifdef DMA_FLUSH
		DMA_DMA_SSP_RXQ7_CONTROL_t reg_rxq7_ctrl;
#endif

		reg_rxq7_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_INTENABLE);
		reg_rxq7_inten.bf.rxq7_eof_en = 0;
		reg_rxq7_inten.bf.rxq7_full_en = 0;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_INTENABLE, reg_rxq7_inten.wrd);

		reg_rxq7_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_INTERRUPT);
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_INTERRUPT, reg_rxq7_int.wrd);

#ifdef DMA_FLUSH
		reg_rxq7_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_CTRL);
		reg_rxq7_ctrl.bf.rxq7_flush_en = 1;
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_CTRL, reg_rxq7_ctrl.wrd);

		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_RPTR, 0);
#endif
	}

#ifdef DMA_FLUSH
	/* reset DMA descriptor */
	for (i = 0; i < ssp->ctrl[index].rx_buf_num; i++) {
		tmp_desc = (cs75xx_dma_desc_t *)(ssp->ctrl[index].rx_dma_desc + i);
		tmp_desc->own = 0;	// DMA
		tmp_desc->misc = 0;
		//tmp_desc->desccnt = x;
		tmp_desc->buf_size = ssp->ctrl[index].rx_buf_size;
		//tmp_desc->buf_addr = rbuf_paddr + i*rx_buf_size;
	}
#endif

	return 0;
}
EXPORT_SYMBOL(cs75xx_dma_ssp_rx_disable);

/* DMA SSP INT dispatcher */
static irqreturn_t cs75xx_dma_ssp_desc_handler(int irq, void *dev_instance)
{
	struct cs75xx_ssp *ssp = (struct cs75xx_ssp *)dev_instance;
	DMA_DMA_SSP_DESC_INTERRUPT_t reg_desc_int;

	reg_desc_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_DESC_INTERRUPT);
	if (reg_desc_int.bf.rx_des_err)
		dev_err(ssp->dev, "ERROR!! DMA_SSP_RX_DESC_ERR\n");
	if (reg_desc_int.bf.tx_des_err)
		dev_err(ssp->dev, "ERROR!! DMA_SSP_TX_DESC_ERR\n");
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_DESC_INTERRUPT, reg_desc_int.wrd);

	return IRQ_HANDLED;
}

static irqreturn_t cs75xx_dma_ssp_rx_handler(int irq, void *dev_instance)
{
	struct cs75xx_ssp *ssp = (struct cs75xx_ssp *)dev_instance;
	DMA_DMA_SSP_RXQ6_INTERRUPT_t reg_rxq6_int;
	DMA_DMA_SSP_RXQ7_INTERRUPT_t reg_rxq7_int;
	int index = (irq == ssp->irq_rx[0]) ? 0 : 1;
	int eof = 0;

	if (index == 0) {
		reg_rxq6_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_INTERRUPT);

		eof = reg_rxq6_int.bf.rxq6_eof;
		if (reg_rxq6_int.bf.rxq6_full)
			ssp->ctrl[index].rxq_full++;
		if (reg_rxq6_int.bf.rxq6_overrun)
			ssp->ctrl[index].rxq_overrun++;
		if (reg_rxq6_int.bf.rxq6_cntmsb)
			ssp->ctrl[index].rxq_cntmsb++;
		if (reg_rxq6_int.bf.rxq6_full_drop_overrun)
			ssp->ctrl[index].rxq_drop_overrun++;
		if (reg_rxq6_int.bf.rxq6_full_drop_cntmsb)
			ssp->ctrl[index].rxq_drop_overrun++;

		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_INTERRUPT, reg_rxq6_int.wrd);
	} else {
		reg_rxq7_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_INTERRUPT);

		eof = reg_rxq7_int.bf.rxq7_eof;
		if (reg_rxq7_int.bf.rxq7_full)
			ssp->ctrl[index].rxq_full++;
		if (reg_rxq7_int.bf.rxq7_overrun)
			ssp->ctrl[index].rxq_overrun++;
		if (reg_rxq7_int.bf.rxq7_cntmsb)
			ssp->ctrl[index].rxq_cntmsb++;
		if (reg_rxq7_int.bf.rxq7_full_drop_overrun)
			ssp->ctrl[index].rxq_drop_overrun++;
		if (reg_rxq7_int.bf.rxq7_full_drop_cntmsb)
			ssp->ctrl[index].rxq_drop_overrun++;

		cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_INTERRUPT, reg_rxq7_int.wrd);
	}

	if (eof)
		ssp->ctrl[index].dma_rx_hander();

	return IRQ_HANDLED;
}

static irqreturn_t cs75xx_dma_ssp_tx_handler(int irq, void *dev_instance)
{
	struct cs75xx_ssp *ssp = (struct cs75xx_ssp *)dev_instance;
	DMA_DMA_SSP_TXQ6_INTERRUPT_t reg_txq6_int;
	DMA_DMA_SSP_TXQ7_INTERRUPT_t reg_txq7_int;
	int index = (irq == ssp->irq_tx[0]) ? 0 : 1;
	int eof = 0;

	if (index == 0) {
		reg_txq6_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTERRUPT);

		eof = reg_txq6_int.bf.txq6_eof;
		if (reg_txq6_int.bf.txq6_empty)
			ssp->ctrl[index].txq_empty++;
		if (reg_txq6_int.bf.txq6_overrun)
			ssp->ctrl[index].txq_overrun++;
		if (reg_txq6_int.bf.txq6_cntmsb)
			ssp->ctrl[index].txq_cntmsb++;

		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTERRUPT, reg_txq6_int.wrd);
	} else {
		reg_txq7_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_INTERRUPT);

		eof = reg_txq7_int.bf.txq7_eof;

		if (reg_txq7_int.bf.txq7_empty)
			ssp->ctrl[index].txq_empty++;
		if (reg_txq7_int.bf.txq7_overrun)
			ssp->ctrl[index].txq_overrun++;
		if (reg_txq7_int.bf.txq7_cntmsb)
			ssp->ctrl[index].txq_cntmsb++;

		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_INTERRUPT, reg_txq7_int.wrd);
	}

	if (eof)
		ssp->ctrl[index].dma_tx_hander();

	return IRQ_HANDLED;
}

static irqreturn_t cs75xx_dma_ssp_ssp_handler(int irq, void *dev_instance)
{
	struct cs75xx_ssp *ssp = (struct cs75xx_ssp *)dev_instance;
	DMA_SSP_INTERRUPT_t reg_ssp_int;
	int index = (irq == ssp->irq_ssp[0]) ? 0 : 1;

	reg_ssp_int.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_INTERRUPT);
	if (reg_ssp_int.wrd & (~(BIT(0) | BIT(1) | BIT(2) | BIT(5) | BIT(6) | BIT(7))))
		dev_warn(ssp->dev, "SINT%d %03x\n", index, reg_ssp_int.wrd);
#if 0
	if (reg_ssp_int.wrd & BIT(0))
		printk("Transmit FIFO Full\n");
	if (reg_ssp_int.wrd & BIT(1))
		printk("Transmit FIFO Empty\n");
	if (reg_ssp_int.wrd & BIT(2))
		printk("Transmit FIFO Write Watermark reach\n");
	if (reg_ssp_int.wrd & BIT(3))
		printk("Transmit FIFO Under Run\n");
	if (reg_ssp_int.wrd & BIT(4))
		printk("Transmit FIFO Over Run\n");
	if (reg_ssp_int.wrd & BIT(5))
		printk("Receive FIFO Full\n");
	if (reg_ssp_int.wrd & BIT(6))
		printk("Receive FIFO Empty\n");
	if (reg_ssp_int.wrd & BIT(7))
		printk("Receive FIFO Read Watermark reach\n");
	if (reg_ssp_int.wrd & BIT(8))
		printk("Receive FIFO Under Run\n");
	if (reg_ssp_int.wrd & BIT(9))
		printk("Receive FIFO Over Run\n");
	if (reg_ssp_int.wrd & BIT(10))
		printk("16 bit sample data shift one byte on RX\n");
	if (reg_ssp_int.wrd & BIT(11))
		printk("16 bit sample data shift one byte on TX\n");
#endif
	cs75xx_ssp_write_reg(index, CS75XX_SSP_INTERRUPT, reg_ssp_int.wrd);

	return IRQ_HANDLED;
}

/*******************************************************************************
 * SSP
 ******************************************************************************/
#ifdef CONFIG_DAC_REF_INTERNAL_CLK
extern int cs75xx_spdif_clock_start(unsigned int sample_rate, int ext_out, unsigned int clk_target);
#endif
static int cs75xx_ssp_config(u32 index, cs75xx_ssp_cfg_t ssp_cfg)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	//DMA_SSP_CTRL0_t reg_ctrl0;
	//DMA_SSP_CTRL1_t reg_ctrl1;
	DMA_SSP_FRAME_CTRL0_t reg_frame_ctrl0;
	DMA_SSP_FRAME_CTRL1_t reg_frame_ctrl1;
	DMA_SSP_BAUDRATE_CTRL_t reg_baud_ctrl;
	DMA_SSP_FIFO_CTRL_t reg_fifo_ctrl;
	//DMA_SSP_FIFO_PTR_t reg_fifo_ptr;
	//DMA_SSP_INTERRUPT_t reg_ssp_int;
	//DMA_SSP_INT_ENABLE_t reg_ssp_inten;
	DMA_SSP_TXSLOT_VLD0_t reg_txslot_vld0;
	DMA_SSP_TXSLOT_VLD1_t reg_txslot_vld1;
	DMA_SSP_TXSLOT_VLD2_t reg_txslot_vld2;
	DMA_SSP_TXSLOT_VLD3_t reg_txslot_vld3;
	DMA_SSP_RXSLOT_VLD0_t reg_rxslot_vld0;
	DMA_SSP_RXSLOT_VLD1_t reg_rxslot_vld1;
	DMA_SSP_RXSLOT_VLD2_t reg_rxslot_vld2;
	DMA_SSP_RXSLOT_VLD3_t reg_rxslot_vld3;
	//DMA_SSP_SLOT_SIZE0_t reg_slot_size0;
	//DMA_SSP_SLOT_SIZE0_t reg_slot_size1;
	//DMA_SSP_SLOT_SIZE0_t reg_slot_size2;
	//DMA_SSP_SLOT_SIZE0_t reg_slot_size3;
	u32 ssp_valid_slots[4];
	int i, valid_slot, total_slots, sclk_select = 0;

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	reg_fifo_ctrl.wrd = 0;
	reg_fifo_ctrl.bf.rfrWtMkLvl = 127;
	reg_fifo_ctrl.bf.rfwWtMkLvl = 0;
	reg_fifo_ctrl.bf.tfrWtMkLvl = 4;
	reg_fifo_ctrl.bf.tfwWtMkLvl = 127;

	cs75xx_ssp_write_reg(index, CS75XX_SSP_FIFO_CTRL, reg_fifo_ctrl.wrd);

	reg_txslot_vld0.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD0, reg_txslot_vld0.wrd);
	reg_txslot_vld1.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD1, reg_txslot_vld1.wrd);
	reg_txslot_vld2.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD2, reg_txslot_vld2.wrd);
	reg_txslot_vld3.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD3, reg_txslot_vld3.wrd);

	reg_rxslot_vld0.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD0, reg_rxslot_vld0.wrd);
	reg_rxslot_vld1.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD1, reg_rxslot_vld1.wrd);
	reg_rxslot_vld2.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD2, reg_rxslot_vld2.wrd);
	reg_rxslot_vld3.wrd = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD3, reg_rxslot_vld3.wrd);

	cs75xx_ssp_write_reg(index, CS75XX_SSP_SLOT_SIZE0, 0xffffffff);
	cs75xx_ssp_write_reg(index, CS75XX_SSP_SLOT_SIZE1, 0xffffffff);
	cs75xx_ssp_write_reg(index, CS75XX_SSP_SLOT_SIZE2, 0xffffffff);
	cs75xx_ssp_write_reg(index, CS75XX_SSP_SLOT_SIZE3, 0xffffffff);

	switch (ssp_cfg.profile) {
	case SSP_I2S_DAC:	/* CS-4341 */
		reg_baud_ctrl.wrd = 0;
		reg_baud_ctrl.bf.param_a1 = 0;
		reg_baud_ctrl.bf.param_n = 1;
		reg_baud_ctrl.bf.param_a2 = 0;
		reg_baud_ctrl.bf.param_m = 1;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_BAUD_RATE, reg_baud_ctrl.wrd);

		reg_frame_ctrl1.wrd = 0;
		reg_frame_ctrl1.bf.FSLen = 31;
		reg_frame_ctrl1.bf.slot2FS = 8;
		reg_frame_ctrl1.bf.FS2Slot = 0;
		reg_frame_ctrl1.bf.numOfSlots = 7;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);

		reg_frame_ctrl0.wrd = 0;
		reg_frame_ctrl0.bf.clkDiv = 1;
		reg_frame_ctrl0.bf.extClkSel = 1;
		reg_frame_ctrl0.bf.txEdge = 1;
		reg_frame_ctrl0.bf.FSEdge = 1;

		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_txslot_vld0.wrd = 0;
		reg_txslot_vld0.bf.SlotVld = 0x00000033;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD0, reg_txslot_vld0.wrd);

		cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD0, 0);
		break;

	case SSP_I2S_D2:	/* D2-45057 */
		reg_baud_ctrl.wrd = 0;
		reg_baud_ctrl.bf.param_a1 = 0;
		reg_baud_ctrl.bf.param_n = 1;
		reg_baud_ctrl.bf.param_a2 = 0;
		reg_baud_ctrl.bf.param_m = 1;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_BAUD_RATE, reg_baud_ctrl.wrd);

		/* When PCM/I2S slot was shifted one bit, the TX underrun situation
		   can not be recovered. And it make SSP TX will be stuck. */
		reg_frame_ctrl1.wrd = 0;
		reg_frame_ctrl1.bf.FSLen = 31;
		reg_frame_ctrl1.bf.slot2FS = 7;
		reg_frame_ctrl1.bf.FS2Slot = 1;
		reg_frame_ctrl1.bf.numOfSlots = 7;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);

		reg_frame_ctrl0.wrd = 0;
		if (ssp_cfg.ext_clk == SCLK_EXT_OSC) {
			reg_frame_ctrl0.bf.clkDiv = 1;
			reg_frame_ctrl0.bf.extClkSel = 1;
		}
		reg_frame_ctrl0.bf.txEdge = 1;
		reg_frame_ctrl0.bf.FSEdge = 1;

		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_txslot_vld0.wrd = 0;
		reg_txslot_vld0.bf.SlotVld = 0x00000033;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD0, reg_txslot_vld0.wrd);

		cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD0, 0);
		break;

	case SSP_PCM_SSLIC:	/* Silicon Labs Si3210 */
		reg_frame_ctrl0.wrd = 0;
		reg_frame_ctrl0.bf.clkDiv = (ssp_cfg.chan_num == 1) ? 243 : 121;
		reg_frame_ctrl0.bf.FSCFreeRun = 1;
		reg_frame_ctrl0.bf.rxEdge = 1;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_frame_ctrl1.wrd = 0;
		reg_frame_ctrl1.bf.FSLen = 7;
		reg_frame_ctrl1.bf.slot2FS = 8;
		reg_frame_ctrl1.bf.numOfSlots = (ssp_cfg.chan_num == 1) ? 3 : 7;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);

		reg_baud_ctrl.wrd = 0;
		reg_baud_ctrl.bf.param_a1 = 2; // -1
		reg_baud_ctrl.bf.param_n = (ssp_cfg.chan_num == 1) ? 6 : 13;
		reg_baud_ctrl.bf.param_a2 = 1; // 1
		reg_baud_ctrl.bf.param_m = 63;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_BAUD_RATE, reg_baud_ctrl.wrd);


		reg_txslot_vld0.wrd = 0;
		reg_txslot_vld0.bf.SlotVld = (ssp_cfg.chan_num == 1) ? 0x00000003 : 0x0000000f;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD0, reg_txslot_vld0.wrd);

		reg_rxslot_vld0.wrd = 0;
		reg_rxslot_vld0.bf.SlotVld = (ssp_cfg.chan_num == 1) ? 0x00000003 : 0x0000000f;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD0, reg_rxslot_vld0.wrd);
		break;

	case SSP_PCM_ZSLIC:
		/* Zarlink VE880
		   There is a 16.384 MHz external on ENG/REF board */
		if (ssp_cfg.ext_clk == SCLK_EXT_OSC) {
			sclk_select = sclk_param_select(ssp_cfg.sclk);
			if (sclk_select < 0)
				return -EINVAL;
		}
#ifdef CONFIG_DAC_REF_INTERNAL_CLK
		else if (ssp_cfg.ext_clk == SCLK_INT_SPDIF) {
			cs75xx_spdif_clock_start(8000, 0, 2);
		}
#endif

		reg_frame_ctrl0.wrd = 0;
		if (ssp_cfg.ext_clk == SCLK_EXT_OSC)
			reg_frame_ctrl0.bf.clkDiv = (ssp_cfg.sclk == 1024? 7 : 0);
		else
			reg_frame_ctrl0.bf.clkDiv = sclk_gen_params[sclk_select][2];

		if (hw_ssp_eco_r0 == TRUE)
			reg_frame_ctrl0.bf.FSCFreeRun = 0;
		else
			reg_frame_ctrl0.bf.FSCFreeRun = 1;

		if (ssp_cfg.ext_clk == SCLK_EXT_OSC)
			reg_frame_ctrl0.bf.extClkSel = 1;
#ifdef CONFIG_DAC_REF_INTERNAL_CLK
		else if (ssp_cfg.ext_clk == SCLK_INT_SPDIF)
			reg_frame_ctrl0.bf.mclkSel = 1;
#endif

		reg_frame_ctrl0.bf.txEdge = 0;
		reg_frame_ctrl0.bf.rxEdge = 1;
		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

		reg_frame_ctrl1.wrd = 0;
		reg_frame_ctrl1.bf.FSLen = 7;
		reg_frame_ctrl1.bf.FS2Slot = 1;
		reg_frame_ctrl1.bf.slot2FS = 7;
		if (ssp_cfg.sclk == 1024)
			reg_frame_ctrl1.bf.numOfSlots = 15;
		else if (ssp_cfg.sclk == 8192)
			reg_frame_ctrl1.bf.numOfSlots = 127;

		cs75xx_ssp_write_reg(index, CS75XX_SSP_FRAME_CTRL1, reg_frame_ctrl1.wrd);

		reg_baud_ctrl.wrd = 0;
		reg_baud_ctrl.bf.param_a1 = (ssp_cfg.ext_clk == SCLK_EXT_OSC)? 0 : sclk_gen_params[sclk_select][3];
		reg_baud_ctrl.bf.param_n = (ssp_cfg.ext_clk == SCLK_EXT_OSC)? 0 : sclk_gen_params[sclk_select][1];
		reg_baud_ctrl.bf.param_a2 = (ssp_cfg.ext_clk == SCLK_EXT_OSC)? 0 : sclk_gen_params[sclk_select][4];
		reg_baud_ctrl.bf.param_m = (ssp_cfg.ext_clk == SCLK_EXT_OSC)? 0 : sclk_gen_params[sclk_select][0];
		cs75xx_ssp_write_reg(index, CS75XX_SSP_BAUD_RATE, reg_baud_ctrl.wrd);

		for (i = 0; i < 4; i++)
			ssp_valid_slots[i] = 0;

		total_slots = ssp_cfg.sclk / 64;

		for (i = 0; i < ssp_cfg.chan_num; i++) {
			if (hw_ssp_eco_r0 == FALSE) {
				if (ssp_cfg.codec_size == 4) {
					valid_slot = i * 4;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					valid_slot += 1;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					valid_slot = total_slots/2 + i * 4;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					valid_slot += 1;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
				} else {
					valid_slot = i * 4;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					if (ssp_cfg.codec_size == 2) {
						valid_slot += 1;
						ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					}
				}
			} else {
				/* After SSP recovered from RX overrun situation, if the final slot
				   of frame is not valid, the RX data of final slot of pervious
				   frame will be pushed into RX FIFO with current write pointer.
				   But the final slot is invalid so that this slot should be discarded.
				   And this will make RX data one byte shifted. If the final slot
				   of frame is valid, the RX data of final slot of previous frame
				   will be pushed into RX FIFO with current write pointer minus
				   one. The first slot of current frame will be pushed into RX FIFO
				   with current write pointer. The final slot of frame should be
				   set valid prevent from RX overrun causing one byte shifted.
				 */
				if (ssp_cfg.codec_size == 4) {
					valid_slot = total_slots/2 - (ssp_cfg.chan_num-i)*4 + (4-ssp_cfg.codec_size/2);
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					valid_slot += 1;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					valid_slot = total_slots - (ssp_cfg.chan_num-i)*4 + (4-ssp_cfg.codec_size/2);
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					valid_slot += 1;
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
				} else {
					valid_slot = total_slots - (ssp_cfg.chan_num-i)*4 + (4-ssp_cfg.codec_size);
					ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					if (ssp_cfg.codec_size == 2) {
						valid_slot += 1;
						ssp_valid_slots[valid_slot / 32] |= BIT(valid_slot % 32);
					}
				}
			}
		}

		for (i = 0; i < 4; i++)
			dev_dbg(ssp->dev, "SLOT_VLD%d = %08x\n", i, ssp_valid_slots[i]);

		for (i = 0; i < 4; i++) {
			cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD0 + i*4, ssp_valid_slots[i]);
			cs75xx_ssp_write_reg(index, CS75XX_SSP_RXSLOT_VLD0 + i*4, ssp_valid_slots[i]);
		}
		break;

	default:
		dev_info(ssp->dev, "Func: %s - Not support ssp_cfg profile %d!\n", __func__, ssp_cfg.profile);
	}

	return 0;
}

int cs75xx_ssp_slot(u32 index, cs75xx_ssp_profile_t profile, u8 param)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	DMA_SSP_TXSLOT_VLD0_t reg_txslot_vld0;
	DMA_SSP_TXSLOT_VLD2_t reg_txslot_vld2;
	DMA_SSP_RXSLOT_VLD0_t reg_rxslot_vld0;
	DMA_SSP_RXSLOT_VLD2_t reg_rxslot_vld2;
	u8 slot_num;
	u16 codec_size;

	if ((profile == SSP_I2S_DAC) || (profile == SSP_I2S_D2)) {
		slot_num = param;
		reg_txslot_vld0.wrd = 0;

		if (slot_num == 1)
			reg_txslot_vld0.bf.SlotVld = 0x00000011;
		else if (slot_num == 2)
			reg_txslot_vld0.bf.SlotVld = 0x00000033;
		else if (slot_num == 3) {
			/* After I2S transmits 24 bit sample data and TX underrun
			   happens, the music can not play well after DMA come
			   back and server SSP/TX again.
			   The TX queue of DMA stores sample data in contiguous
			   memory locations. For example, the first sample data
			   is located byte 0, 1 and 2. The next sample data is
			   located at byte 3, 4 and 5. Each DMA transaction is
			   128 bytes, it will make last sample data is transferred
			   by separated two DMA transactions. If SSP TX underrun
			   happens after first DMA transaction has finished, the
			   DMA engine will start transferring second DMA transaction
			   after DMA engine serves again. The first byte of TXD
			   will be the byte 2 of last sample data, and this first
			   byte of TXD will be transmitted through first slot of
			   I2S frame. The first three bytes of TXD will not belong
			   to same sample data. It will make music can not play well.
			   So the sample data was stored in memory should be 4
			   bytes alignment. That is byte 0,1,2 for sample data
			   and byte 3 for ¡§x¡¨ and the I2S frame should be set
			   as left/right channel has 4 slots; 3 slots are valid
			   and 1 slot is invalid.
			 */
			reg_txslot_vld0.bf.SlotVld = 0x000000FF;
		}

		cs75xx_ssp_write_reg(index, CS75XX_SSP_TXSLOT_VLD0, reg_txslot_vld0.wrd);
	}
	else if (profile == SSP_PCM_ZSLIC) {
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(cs75xx_ssp_slot);


int cs75xx_ssp_register(u32 index, cs75xx_ssp_cfg_t ssp_cfg)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (index >= CS75XX_SSP_NUM) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	if (ssp_cfg.chan_num == 0)
		return -EINVAL;

	cs75xx_ssp_config(index, ssp_cfg);

	/* DMA SSP INT dispatcher */
	if (request_irq(ssp->irq_rx[index], cs75xx_dma_ssp_rx_handler, 0, ssp->irq_rx_name[index], ssp)) {
		dev_err(ssp->dev, "ERROR: can't register IRQ %s\n", ssp->irq_rx_name[index]);
		goto fail;
	}

	if (request_irq(ssp->irq_tx[index], cs75xx_dma_ssp_tx_handler, 0, ssp->irq_tx_name[index], ssp)) {
		dev_err(ssp->dev, "ERROR: can't register IRQ %s\n", ssp->irq_tx_name[index]);
		goto fail;
	}

	if (request_irq(ssp->irq_ssp[index], cs75xx_dma_ssp_ssp_handler, 0, ssp->irq_ssp_name[index], ssp)) {
		dev_err(ssp->dev, "ERROR: can't register IRQ %s\n", ssp->irq_ssp_name[index]);
		goto fail;
	}

	return 0;
fail:
	free_irq(ssp->irq_rx[index], ssp);
	free_irq(ssp->irq_tx[index], ssp);
	free_irq(ssp->irq_ssp[index], ssp);

	return 0;
}
EXPORT_SYMBOL(cs75xx_ssp_register);


int cs75xx_ssp_unregister(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (index >= CS75XX_SSP_NUM) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	/* DMA SSP INT dispatcher */
	free_irq(ssp->irq_rx[index], ssp);
	free_irq(ssp->irq_tx[index], ssp);
	free_irq(ssp->irq_ssp[index], ssp);

	return 0;
}
EXPORT_SYMBOL(cs75xx_ssp_unregister);


int cs75xx_ssp_reg_read(u32 index, u32 offset, cs_reg *val_p)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	/* check offset */
	if (index > CS75XX_SSP_SLOT_SIZE3)
		return CS_ERROR;
	if (index % 4)
		return CS_ERROR;

	*val_p = cs75xx_ssp_read_reg(index, offset);

	return 0;
}

int cs75xx_ssp_reg_write(u32 index, u32 offset, cs_reg val)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	/* check offset */
	if (index > CS75XX_SSP_SLOT_SIZE3)
		return CS_ERROR;
	if (index % 4)
		return CS_ERROR;

	cs75xx_ssp_write_reg(index, offset, val);

	return 0;
}

int cs75xx_dma_ssp_reg_read(u32 offset, cs_reg *val_p)
{
        struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);

        /* check offset */
        if (offset > CS75XX_DMA_SSP_TXQ7_INTENABLE)
                return CS_ERROR;
        if (offset % 4)
                return CS_ERROR;

        *val_p = cs75xx_dma_read_reg(offset);

        return 0;

}
EXPORT_SYMBOL(cs75xx_dma_ssp_reg_read);

/*******************************************************************************
 * SSP/DMA
 ******************************************************************************/
int cs75xx_ssp_enable(u32 index, u32 tx_en, u32 tx_swap, u32 rx_en, u32 rx_swap)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;
	DMA_SSP_CTRL1_t reg_ssp_ctrl1;
	unsigned long flags;

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (index >= CS75XX_SSP_NUM) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	ssp->ctrl[index].rxq_full = 0;
	ssp->ctrl[index].rxq_overrun = 0;
	ssp->ctrl[index].rxq_cntmsb = 0;
	ssp->ctrl[index].rxq_drop_overrun = 0;
	ssp->ctrl[index].rxq_drop_cntmsb = 0;

	ssp->ctrl[index].txq_empty = 0;
	ssp->ctrl[index].txq_overrun = 0;
	ssp->ctrl[index].txq_cntmsb = 0;

	spin_lock_irqsave(&ssp->lock[index], flags);
	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_CTRL0);
	reg_ssp_ctrl0.bf.forceTC = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	if (tx_en == 1) {
		cs75xx_dma_ssp_tx_enable(index);

		ssp->ctrl[index].tx_swap = tx_swap ? 1 : 0;
			reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_CTRL0);
		reg_ssp_ctrl0.bf.txChByteSwap = ssp->ctrl[index].tx_swap;
			cs75xx_ssp_write_reg(index, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

		ssp->ctrl[index].tx_en = 1;
	}

	if (rx_en == 1) {
		cs75xx_dma_ssp_rx_enable(index);

		ssp->ctrl[index].rx_swap = rx_swap ? 1 : 0;
			reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_CTRL0);
		reg_ssp_ctrl0.bf.rxChByteSwap = ssp->ctrl[index].rx_swap;
			cs75xx_ssp_write_reg(index, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

		ssp->ctrl[index].rx_en = 1;
	}

	reg_ssp_ctrl1.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_CTRL1);
	reg_ssp_ctrl1.bf.startProc = 1;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_CTRL1, reg_ssp_ctrl1.wrd);
	spin_unlock_irqrestore(&ssp->lock[index], flags);

	return 0;
}
EXPORT_SYMBOL(cs75xx_ssp_enable);

int cs75xx_ssp_disable(u32 index)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(cs75xx_ssp_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;
	DMA_SSP_FIFO_PTR_t reg_ssp_fifo_ptr;
	unsigned long flags;
	int cnt;

	dev_dbg(ssp->dev, "Func: %s(SSP%d)\n", __func__, index);

	if (index >= CS75XX_SSP_NUM) {
		dev_err(ssp->dev, "Func: %s - invalid SSP index %d\n", __func__, index);
		return -EINVAL;
	}

	spin_lock_irqsave(&ssp->lock[index], flags);
	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_CTRL0);
	reg_ssp_ctrl0.bf.forceTC = 1;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	udelay(50);
	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_CTRL0);
	reg_ssp_ctrl0.bf.forceTC = 0;
	cs75xx_ssp_write_reg(index, CS75XX_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	if (ssp->ctrl[index].tx_en) {
		//timeout_time = jiffies + SSP_DIS_TIMEOUT;
		//printk("tx timeout_time = %lx, jiffies = %lx\n", timeout_time, jiffies);
		cnt = 0;
		do {
			reg_ssp_fifo_ptr.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_FIFO_PTR);

			if ((reg_ssp_fifo_ptr.bf.tfWrPtr == 0) && (reg_ssp_fifo_ptr.bf.tfRdPtr == 0))
				break;
			else {
				cnt++;
				mdelay(1);
			}
		} while (cnt <= 50);

		if (cnt > 50)
			dev_warn(ssp->dev, "Func: %s - forceTc to Tx idle timeout\n", __func__);

		cs75xx_dma_ssp_tx_disable(index);
		ssp->ctrl[index].tx_en = 0;
	}

	if (ssp->ctrl[index].rx_en) {
		//timeout_time = jiffies + SSP_DIS_TIMEOUT;
		//printk("rx timeout_time = %lx, jiffies = %lx\n", timeout_time, jiffies);
		cnt = 0;
		do {
			reg_ssp_fifo_ptr.wrd = cs75xx_ssp_read_reg(index, CS75XX_SSP_FIFO_PTR);

			if ((reg_ssp_fifo_ptr.bf.rfWrPtr == 0) && (reg_ssp_fifo_ptr.bf.rfRdPtr == 0))
				break;
			else {
				cnt++;
				mdelay(1);
			}
		} while (cnt <= 50);

		if (cnt > 50)
			dev_warn(ssp->dev, "Func: %s - forceTc to Rx idle timeout\n", __func__);

		cs75xx_dma_ssp_rx_disable(index);
		ssp->ctrl[index].rx_en = 0;
	}
	spin_unlock_irqrestore(&ssp->lock[index], flags);

	dev_dbg(ssp->dev, "rxq%d_full = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].rxq_full);
	dev_dbg(ssp->dev, "rxq%d_overrun = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].rxq_overrun);
	dev_dbg(ssp->dev, "rxq%d_cntmsb = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].rxq_cntmsb);
	dev_dbg(ssp->dev, "rxq%d_drop_overrun = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].rxq_drop_overrun);
	dev_dbg(ssp->dev, "rxq%d_drop_cntmsb = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].rxq_drop_cntmsb);

	dev_dbg(ssp->dev, "txq%d_empty = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].txq_empty);
	dev_dbg(ssp->dev, "txq%d_overrun = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].txq_overrun);
	dev_dbg(ssp->dev, "txq%d_cntmsb = %d\n", (index == 0 ? 6 : 7), ssp->ctrl[index].txq_cntmsb);

	return 0;
}
EXPORT_SYMBOL(cs75xx_ssp_disable);

static int cs75xx_ssp_dma_init(struct cs75xx_ssp *ssp)
{
	struct platform_device *pdev = cs75xx_ssp_dev;
	DMA_SSP_INT_ENABLE_t reg_ssp_inten;
	DMA_DMA_SSP_RXDMA_CONTROL_t reg_dma_rx_ctrl;
 	DMA_DMA_SSP_RXQ6_FULL_THRESHOLD_t reg_rx6_full_th;
 	DMA_DMA_SSP_RXQ7_FULL_THRESHOLD_t reg_rx7_full_th;
	DMA_DMA_SSP_RXQ6_INTENABLE_t reg_rxq6_inten;
	DMA_DMA_SSP_RXQ7_INTENABLE_t reg_rxq7_inten;
	DMA_DMA_SSP_TXDMA_CONTROL_t reg_dma_tx_ctrl;
	DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;
	DMA_DMA_SSP_TXQ7_INTENABLE_t reg_txq7_inten;
	DMA_DMA_SSP_DMA_SSP_INTENABLE_0_t reg_dma_ssp_inten;
	char irq_name[32];
	int i;

	dev_dbg(ssp->dev, "Func: %s\n", __func__);

	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		ssp->ctrl[i].ssp_irq_status = 0;

		ssp->ctrl[i].profile = SSP_I2S_DAC;
		ssp->ctrl[i].tx_dma_desc = NULL;
		ssp->ctrl[i].rx_dma_desc = NULL;
		ssp->ctrl[i].tx_en = 0;
		ssp->ctrl[i].tx_swap = 0;
		ssp->ctrl[i].rx_en = 0;
		ssp->ctrl[i].rx_swap = 0;

		ssp->ctrl[i].dma_tx_hander = NULL;
		ssp->ctrl[i].dma_rx_hander = NULL;
		ssp->ctrl[i].tx_buf_num = 0;
		ssp->ctrl[i].rx_buf_num = 0;
//		ssp->ctrl[i].tx_buf_vaddr = 0;
//		ssp->ctrl[i].rx_buf_vaddr = 0;
		ssp->ctrl[i].tx_desc_paddr = 0;
		ssp->ctrl[i].rx_desc_paddr = 0;
		ssp->ctrl[i].tx_dma_desc = NULL;
		ssp->ctrl[i].rx_dma_desc = NULL;
		ssp->ctrl[i].dma_irq_tx_status = 0;
		ssp->ctrl[i].dma_irq_rx_status = 0;
	}

	/* init SSP */
	reg_ssp_inten.wrd = BIT(1) | BIT(3) | BIT(4) |  BIT(6) | BIT(8) | BIT(9) | BIT(10) | BIT(11);//0x00000FFF;
	for (i = 0; i < CS75XX_SSP_NUM; i++)
		cs75xx_ssp_write_reg(i, CS75XX_SSP_INTENABLE, reg_ssp_inten.wrd);

	/* init DMA */
	/* Rx */
	reg_dma_rx_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RX_CTRL);
	reg_dma_rx_ctrl.bf.rx_dma_enable = 1;
	reg_dma_rx_ctrl.bf.rx_check_own = 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_RX_CTRL, reg_dma_rx_ctrl.wrd);

	/* SSP0 */
	reg_rx6_full_th.wrd = 0x00000002;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_FULL_THRESHOLD, reg_rx6_full_th.wrd);

	reg_rxq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ6_INTENABLE);
	reg_rxq6_inten.bf.rxq6_eof_en = 0;
	reg_rxq6_inten.bf.rxq6_full_en = 0;
	reg_rxq6_inten.bf.rxq6_overrun_en = 0;
	reg_rxq6_inten.bf.rxq6_cntmsb_en = 0;
	reg_rxq6_inten.bf.rxq6_full_drop_overrun_en = 0;
	reg_rxq6_inten.bf.rxq6_full_drop_cntmsb_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ6_INTENABLE, reg_rxq6_inten.wrd);

	/* SSP1 */
	reg_rx7_full_th.wrd = 0x00000002;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_FULL_THRESHOLD, reg_rx7_full_th.wrd);

	reg_rxq7_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_RXQ7_INTENABLE);
	reg_rxq7_inten.bf.rxq7_eof_en = 0;
	reg_rxq7_inten.bf.rxq7_full_en = 0;
	reg_rxq7_inten.bf.rxq7_overrun_en = 0;
	reg_rxq7_inten.bf.rxq7_cntmsb_en = 0;
	reg_rxq7_inten.bf.rxq7_full_drop_overrun_en = 0;
	reg_rxq7_inten.bf.rxq7_full_drop_cntmsb_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_RXQ7_INTENABLE, reg_rxq7_inten.wrd);


	/* Tx */
	reg_dma_tx_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TX_CTRL);
	reg_dma_tx_ctrl.bf.tx_dma_enable = 1;
	reg_dma_tx_ctrl.bf.tx_check_own = 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TX_CTRL, reg_dma_tx_ctrl.wrd);

	/* SSP0 */
	reg_txq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE);
	reg_txq6_inten.bf.txq6_eof_en = 0;
	reg_txq6_inten.bf.txq6_empty_en = 0;
	reg_txq6_inten.bf.txq6_overrun_en = 0;
	reg_txq6_inten.bf.txq6_cntmsb_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE, reg_txq6_inten.wrd);

	/* SSP1 */
	reg_txq7_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_INTENABLE);
	reg_txq7_inten.bf.txq7_eof_en = 0;
	reg_txq7_inten.bf.txq7_empty_en = 0;
	reg_txq7_inten.bf.txq7_overrun_en = 0;
	reg_txq7_inten.bf.txq7_cntmsb_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_INTENABLE, reg_txq7_inten.wrd);

	/* IRQ shared with FLASH */
	sprintf(irq_name, "DMA_SSP_DESC");
	if (request_irq(ssp->irq_desc, cs75xx_dma_ssp_desc_handler, IRQF_SHARED, ssp->irq_desc_name, ssp)) {
		dev_err(&pdev->dev, "ERROR: can't register IRQ %s\n", irq_name);
		return -ENODEV;
	}

	return 0;
}

static int cs75xx_ssp_dma_exit(struct cs75xx_ssp *ssp)
{
	struct platform_device *pdev = cs75xx_ssp_dev;
	DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
	DMA_DMA_SSP_TXQ7_CONTROL_t reg_txq7_ctrl;
	int i;

	dev_dbg(ssp->dev, "Func: %s\n", __func__);

	reg_txq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_CTRL);
	reg_txq6_ctrl.bf.txq6_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

	reg_txq7_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ7_CTRL);
	reg_txq7_ctrl.bf.txq7_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ7_CTRL, reg_txq7_ctrl.wrd);

	free_irq(ssp->irq_desc, ssp);

	return 0;
}

static int __devinit cs75xx_ssp_probe(struct platform_device *pdev)
{
	struct cs75xx_ssp *ssp;
	struct resource *res_mem;
	char res_name[32];
	int i;

	dev_info(&pdev->dev, "Func: %s(%s) %s-%s\n", __func__, pdev->name, __DATE__, __TIME__);

	ssp = kzalloc(sizeof(struct cs75xx_ssp), GFP_KERNEL);
	if (!ssp) {
		dev_err(&pdev->dev, "Func: %s - can't allocate memory for %s device\n", __func__, "ssp");
		return -ENOMEM;
	}
	ssp->dev = &pdev->dev;

	/* DMA SSP INT dispatcher */
	for (i = 0; i < CS75XX_SSP_NUM; i++) {
		sprintf(ssp->ctrl[i].name, "ssp%d", i);
		res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, ssp->ctrl[i].name);
		if (!res_mem) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
						__func__, ssp->ctrl[i].name);
			goto fail;
		}
		ssp->ssp_base[i] = ioremap(res_mem->start, resource_size(res_mem));
		if (!ssp->ssp_base[i]) {
			dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
				    __func__, ssp->ctrl[i].name, resource_size(res_mem));
			goto fail;
		}
		dev_dbg(&pdev->dev, "\tssp_base[%d] = 0x%08x, range = 0x%x\n",
				i, (u32)ssp->ssp_base[i], resource_size(res_mem));

		sprintf(res_name, "dma_rx_ssp%d", i);
		ssp->irq_rx[i] = platform_get_irq_byname(pdev, res_name);
		if (ssp->irq_rx[i] == -ENXIO) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
							__func__, res_name);
			goto fail;
		}
		sprintf(ssp->irq_rx_name[i], res_name);
		dev_dbg(&pdev->dev, "\tirq_rx[%d] = %d\n", i, ssp->irq_rx[i]);

		sprintf(res_name, "dma_tx_ssp%d", i);
		ssp->irq_tx[i] = platform_get_irq_byname(pdev, res_name);
		if (ssp->irq_tx[i] == -ENXIO) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
							__func__, res_name);
			goto fail;
		}
		sprintf(ssp->irq_tx_name[i], res_name);
		dev_dbg(&pdev->dev, "\tirq_tx[%d] = %d\n", i, ssp->irq_tx[i]);

		sprintf(res_name, "irq_ssp%d", i);
		ssp->irq_ssp[i] = platform_get_irq_byname(pdev, res_name);
		if (ssp->irq_ssp[i] == -ENXIO) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
							__func__, res_name);
			goto fail;
		}
		sprintf(ssp->irq_ssp_name[i], res_name);
		dev_dbg(&pdev->dev, "\tirq_ssp[%d] = %d\n", i, ssp->irq_ssp[i]);
	}
	/* DMA-SSP */
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_DMA, "dma_ssp");
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "dma_ssp");
		goto fail;
	}
	ssp->dma_base = ioremap(res_mem->start, resource_size(res_mem));
	if (!ssp->dma_base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
			    __func__, "dma-ssp", resource_size(res_mem));
		goto fail;
	}
	dev_dbg(&pdev->dev, "\tdma_base = 0x%08x, range = 0x%x\n", (u32)ssp->dma_base,
		resource_size(res_mem));

	sprintf(res_name, "dma_desc");
	ssp->irq_desc = platform_get_irq_byname(pdev, res_name);
	if (ssp->irq_desc == -ENXIO) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n",
						__func__, res_name);
		goto fail;
	}
	sprintf(ssp->irq_desc_name, res_name);
	dev_dbg(&pdev->dev, "\t%s = %d\n", res_name, ssp->irq_desc);

	platform_set_drvdata(pdev, ssp);
	cs75xx_ssp_dev = pdev;

	for (i = 0; i < CS75XX_SSP_NUM; i++)
		spin_lock_init(&ssp->lock[i]);

	/* init SSP/DMA */
	if (cs75xx_ssp_dma_init(ssp))
		goto fail;

	return 0;

fail:
	if (ssp) {
		for (i = 0; i < CS75XX_SSP_NUM; i++)
			if (ssp->ssp_base[i])
				iounmap(ssp->ssp_base[i]);
		if (ssp->dma_base)
			iounmap(ssp->dma_base);

		kfree(ssp);
	}
	cs75xx_ssp_dev = NULL;

	return -EPERM;
}

static int __devexit cs75xx_ssp_remove(struct platform_device *pdev)
{
	struct cs75xx_ssp *ssp = platform_get_drvdata(pdev);
	int i;

	dev_info(&pdev->dev, "Func: %s(%s)\n", __func__, pdev->name);

	for (i = 0; i < CS75XX_SSP_NUM; i++)
		cs75xx_ssp_disable(i);
	cs75xx_ssp_dma_exit(ssp);

	platform_set_drvdata(pdev, NULL);

	cs75xx_ssp_dev = NULL;

	for (i = 0; i < CS75XX_SSP_NUM; i++)
		iounmap(ssp->ssp_base[i]);
	iounmap(ssp->dma_base);

	kfree(ssp);

	return 0;
}

static struct platform_driver cs75xx_ssp_platform_driver = {
	.probe	= cs75xx_ssp_probe,
	.remove	= __devexit_p(cs75xx_ssp_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name  = CS75XX_SSP_CTLR_NAME,
	},
};

static int __init cs75xx_ssp_init(void)
{
	return platform_driver_register(&cs75xx_ssp_platform_driver);
}

static void __exit cs75xx_ssp_exit(void)
{
	platform_driver_unregister(&cs75xx_ssp_platform_driver);
}

module_init(cs75xx_ssp_init);
module_exit(cs75xx_ssp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX SSP driver");

