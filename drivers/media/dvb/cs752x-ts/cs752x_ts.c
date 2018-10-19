/*
* Copyright 2010 Cortina System, Inc.  All rights reserved.
*
* Name		: g2_ts.c
* Description	: Transport Stream device driver for Golden Gate
*
* History
*
*	Date		Writer		Description
*	-----------	-----------	-------------------------------------------------
*	05/01/2010	Amos Lee	Create and implement
*   	01/02/2010  	CH HSU      	Porting to Linux 2.6.28
*   	04/02/2010  	CH HSU		Modify data strue for performance
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/compiler.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <mach/registers.h>

#include "demux.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_frontend.h"
#include "dvb_net.h"
#include "dvbdev.h"
#include "dvb_ringbuffer.h"
#include "cs752x_ts.h"
#if defined(CONFIG_DVB_MXL241SF) || defined(CONFIG_DVB_MXL241SF_MODULE)
#include "cs75xx_mxl241sf.h"
#endif

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

#define DRIVER_NAME	"CORTINA TS"

#define	CPU		1
#define	DMA		0

#define NHWFILTERS	(G2_TS_MAX_QUEUE_NUM)


#define DMA_MALLOC(size,handle)		dma_alloc_coherent(NULL,size,handle,GFP_DMA)
#define DMA_MFREE(mem,size,handle)	dma_free_coherent(NULL,size,mem,handle)

static G2_TS_INFO_T	*ts_dvb_info;

static int 		tsdebug = 0;
static int		tuner_num = (DVB_MAX_ADAPTERS/2);
static int		packet_unit = 0;

#define dprintk	if (tsdebug) printk

static inline G2_TS_INFO_T *feed_to_ts(struct dvb_demux_feed *feed)
{
	struct dmxdev_filter 	*filter;
	unsigned int		qid = 0;

	filter = (struct dmxdev_filter *)feed->feed.ts.priv;
	qid = filter->dev->dvbdev->id;
	return container_of(feed->demux, G2_TS_INFO_T, demux[qid]);
}

static inline G2_TS_INFO_T *frontend_to_ts(struct dvb_frontend *fe)
{
	return container_of(fe->dvb, G2_TS_INFO_T, dvb_adapter);
}

/*
static int g2_ts_read_reg(unsigned int addr)
{
	unsigned int	reg_val=0;

	reg_val = readl(addr);
	return (reg_val);
}

static int g2_ts_write_reg(unsigned int addr, unsigned int val, unsigned int bitmask)
{
	unsigned int	reg_val;

	reg_val = (readl(addr) & (~bitmask) ) | (val & bitmask);
	writel(reg_val, addr);
	return CS_OK;
}
*/

/***********************************************/
/*                TS DMA                       */
/***********************************************/
#ifdef USED
/* This function gets configurations of DMA RX control function. */
static int g2_ts_get_rxdma_control(G2_TS_RXDMA_CONTROL 	*ctrl)
{
	TS_T_DMA_RXDMA_CONTROL_t	rxdma_control;

	rxdma_control.wrd = readl(TS_T_DMA_RXDMA_CONTROL);
	ctrl->rx_dma_enable = rxdma_control.bf.rx_dma_enable;
	ctrl->rx_check_own 	= rxdma_control.bf.rx_check_own;
	ctrl->rx_burst_len 	= rxdma_control.bf.rx_burst_len;

	return CS_OK;
}
#endif

/* This function sets configurations of DMA RX control function. */
static int g2_ts_set_rxdma_control(G2_TS_RXDMA_CONTROL 	ctrl)
{
	TS_T_DMA_RXDMA_CONTROL_t	rxdma_control;

	rxdma_control.wrd = readl(TS_T_DMA_RXDMA_CONTROL);
	rxdma_control.bf.rx_dma_enable 	= ctrl.rx_dma_enable;
	rxdma_control.bf.rx_check_own  	= ctrl.rx_check_own;
	rxdma_control.bf.rx_burst_len	= ctrl.rx_burst_len;
	writel(rxdma_control.wrd, TS_T_DMA_RXDMA_CONTROL);

	return CS_OK;
}

#ifdef USED
/* This function enables/disables DMA RX fucntion for receiving data. */
static int g2_ts_set_rx_dma_enable(cs_ctl_t mode)
{
	TS_T_DMA_RXDMA_CONTROL_t	rxdma_control;

	rxdma_control.wrd = readl(TS_T_DMA_RXDMA_CONTROL);
	rxdma_control.bf.rx_dma_enable 	= mode;
	writel(rxdma_control.wrd, TS_T_DMA_RXDMA_CONTROL);

	return CS_OK;
}

/* This function gets the "Read with MSB clear" or "Read with ALL clear"
   for the RX Queue received packet counter and full drop packet counter*/
static int g2_ts_get_rxq_pktcnt_read(cs_uint8 qid, cs_boolean *rx_msb_clr,
				cs_boolean *full_drop_msb_clr)
{
	TS_T_DMA_RXQ0_PKTCNT_READ_t	pktcnt_read;

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	pktcnt_read.wrd = readl(TS_T_DMA_RXQ0_PKTCNT_READ + qid*4);
	*rx_msb_clr 		= pktcnt_read.bf.rxq0_msb_clr;
	*full_drop_msb_clr 	= pktcnt_read.bf.rxq0_full_drop_msb_clr;

	return CS_OK;
}
#endif

/* This function sets the "Read with MSB clear" or "Read with ALL clear"
   for the RX Queue received packet counter and full drop packet counter*/
static int g2_ts_set_rxq_pktcnt_read(cs_uint8 qid, cs_boolean rx_msb_clr,
				cs_boolean full_drop_msb_clr)
{
	TS_T_DMA_RXQ0_PKTCNT_READ_t	pktcnt_read;

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	pktcnt_read.wrd = readl(TS_T_DMA_RXQ0_PKTCNT_READ + qid*4);
	pktcnt_read.bf.rxq0_msb_clr	 = rx_msb_clr;
	pktcnt_read.bf.rxq0_full_drop_msb_clr = full_drop_msb_clr;
	writel(pktcnt_read.wrd, TS_T_DMA_RXQ0_PKTCNT_READ + qid*4);

	return CS_OK;
}

#ifdef USED
/* This function gets the RX Queue descriptor base address and queue depth. */
static int g2_ts_get_rxq_base_depth(cs_uint8 qid, cs_uint32 *base, cs_uint8 *depth)
{
	TS_T_DMA_RXQ0_BASE_DEPTH_t	rxq_base_depth;

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	rxq_base_depth.wrd = readl(TS_T_DMA_RXQ0_BASE_DEPTH + qid*4);
	*base = rxq_base_depth.bf.base << 4;
	*depth= rxq_base_depth.bf.depth;
	writel(rxq_base_depth.wrd, TS_T_DMA_RXQ0_BASE_DEPTH + qid*4);

	return CS_OK;
}
#endif

/* This function sets the RX Queue descriptor base address and queue depth. */
static int g2_ts_set_rxq_base_depth(cs_uint8 qid, cs_uint32 base, cs_uint8 depth)
{
	TS_T_DMA_RXQ0_BASE_DEPTH_t	rxq_base_depth;

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	rxq_base_depth.wrd = readl(TS_T_DMA_RXQ0_BASE_DEPTH + qid*4);
	rxq_base_depth.bf.base = base >> 4;
	rxq_base_depth.bf.depth= depth;
	writel(rxq_base_depth.wrd, TS_T_DMA_RXQ0_BASE_DEPTH + qid*4);

	return CS_OK;
}

/* When a packet is received into the RXQ0,
   the write pointer will beupdated by the DMA RX engine */
static int g2_ts_get_rxq_wptr(cs_uint8 qid)
{
	cs_uint16	index;

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	index = readl(TS_T_DMA_RXQ0_WPTR + qid*8);
	return index;
}

/* When CPU has processed a packet of RX Queue, it will return the memeory buffer
   by update the read pointer */
static int g2_ts_get_rxq_rptr(cs_uint8 qid)
{
	cs_uint16	index;

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	index = readl(TS_T_DMA_RXQ0_RPTR + qid*8);
	return index;
}

/* When CPU has processed a packet of RX Queue, it will return the memeory buffer
   by update the read pointer */
static int g2_ts_set_rxq_rptr(cs_uint8 qid, cs_uint16 index)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	writel(index, TS_T_DMA_RXQ0_RPTR + qid*8);
	return CS_OK;
}

#ifdef USED
/* When the RXQ11 free queue depths are smaller than or equal to the full
   threshold,the interrupt event will be triggered */
static int g2_ts_get_rxq_full_threshold(cs_uint8 qid, cs_uint16 *depth)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	*depth = readl(TS_T_DMA_RXQ0_FULL_THRESHOLD + qid*4);
	return CS_OK;
}
#endif

/* When the RXQ11 free queue depths are smaller than or equal to the full
   threshold,the interrupt event will be triggered */
static int g2_ts_set_rxq_full_threshold(cs_uint8 qid, cs_uint16 depth)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	writel(depth, TS_T_DMA_RXQ0_FULL_THRESHOLD + qid*4);
	return CS_OK;
}

#ifdef USED
/* This function gets RX Queue received packet counter. */
static int g2_ts_get_rxq_pktcnt(cs_uint8 qid, cs_uint32 *counter)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	*counter = readl(TS_T_DMA_RXQ0_PKTCNT + qid*4);
	return CS_OK;
}

/* This function gets RX Queue full drop packet counter. */
static int g2_ts_get_rxq_full_drop_pktcnt(cs_uint8 qid, cs_uint32 *counter)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	*counter = readl(TS_T_DMA_RXQ0_FULL_DROP_PKTCNT + qid*4);
	return CS_OK;
}
#endif

static int g2_ts_get_dma_interrupt_0(cs_uint32 *interrupt)
{
	*interrupt = readl(TS_T_DMA_TS_INTERRUPT_0);
	return CS_OK;
}

#ifdef USED
static int g2_ts_get_dma_intenable_0(cs_uint32 *int_enable)
{
	*int_enable = readl(TS_T_DMA_TS_INTENABLE_0);
	return CS_OK;
}
#endif

static int g2_ts_set_dma_intenable_0(cs_uint32 int_enable)
{
	writel(int_enable, TS_T_DMA_TS_INTENABLE_0);
	return CS_OK;
}

#ifdef USED
static int g2_ts_get_dma_interrupt_1(cs_uint32 *interrupt)
{
	*interrupt = readl(TS_T_DMA_TS_INTERRUPT_1);
	return CS_OK;
}

static int g2_ts_get_dma_intenable_1(cs_uint32 *int_enable)
{
	*int_enable = readl(TS_T_DMA_TS_INTENABLE_1);
	return CS_OK;
}

static int g2_ts_set_dma_intenable_1(cs_uint32 int_enable)
{
	writel(int_enable, TS_T_DMA_TS_INTENABLE_1);
	return CS_OK;
}
#endif

static int g2_ts_get_dma_desc_interrupt(cs_uint32 *interrupt)
{
	*interrupt = readl(TS_T_DMA_DESC_INTERRUPT);
	return CS_OK;
}

static int g2_ts_set_dma_desc_interrupt(cs_uint32 interrupt)
{
	writel(interrupt, TS_T_DMA_DESC_INTERRUPT);
	return CS_OK;
}

#ifdef USED
static int g2_ts_get_dma_desc_intenable(cs_uint32 *int_enable)
{
	*int_enable = readl(TS_T_DMA_DESC_INTENABLE);
	return CS_OK;
}
#endif

static int g2_ts_set_dma_desc_intenable(cs_uint32 int_enable)
{
	writel(int_enable, TS_T_DMA_DESC_INTENABLE);
	return CS_OK;
}

static int g2_ts_get_dma_rxq_interrupt(cs_uint8 qid, cs_uint32 *interrupt)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	*interrupt = readl(TS_T_DMA_RXQ0_INTERRUPT + qid*8);
	return CS_OK;
}

static int g2_ts_set_dma_rxq_interrupt(cs_uint8 qid, cs_uint32 interrupt)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	writel(interrupt, TS_T_DMA_RXQ0_INTERRUPT + qid*8);
	return CS_OK;
}

#ifdef USED
static int g2_ts_get_dma_rxq_intenable(cs_uint8 qid, cs_uint32 *int_enable)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	*int_enable = readl(TS_T_DMA_RXQ0_INTENABLE + qid*8);
	return CS_OK;
}
#endif

static int g2_ts_set_dma_rxq_intenable(cs_uint8 qid, cs_uint32 int_enable)
{
	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	writel(int_enable, TS_T_DMA_RXQ0_INTENABLE + qid*8);
	return CS_OK;
}

/***********************************************/
/*     TS RX Control and PID Table 0 ~ 5       */
/***********************************************/
static int g2_ts_set_start_pulse(cs_uint8 rx_ch, cs_ctl_t mode)
{
	TS_RXPID_CONTROL_t	rxpid_control;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxpid_control.wrd = readl(TS_RXPID_CONTROL + rx_ch*0x200);
	rxpid_control.bf.start_pulse = mode;
	writel(rxpid_control.wrd, TS_RXPID_CONTROL + rx_ch*0x200);
	return CS_OK;
}

static int g2_ts_set_rx_enable(cs_uint8 rx_ch, cs_ctl_t mode)
{
	TS_RXPID_CONTROL_t	rxpid_control;

	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxpid_control.wrd = readl(TS_RXPID_CONTROL + rx_ch*0x200);
	rxpid_control.bf.continuous_mode = 0;
	rxpid_control.bf.data_mode = 1;
	rxpid_control.bf.endian_dir = 0;
	rxpid_control.bf.bit_dir = 1;
	/* rxpid_control.bf.start_pulse = 1; */
	rxpid_control.bf.rx_enable = mode;
	writel(rxpid_control.wrd, TS_RXPID_CONTROL + rx_ch*0x200);
	return CS_OK;
}

static int g2_ts_set_rxpid_enable(cs_uint8 rx_ch, cs_ctl_t mode)
{
	TS_RXPID_CONTROL_t	rxpid_control;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxpid_control.wrd = readl(TS_RXPID_CONTROL + rx_ch*0x200);
	rxpid_control.bf.pid_enable = mode;
	writel(rxpid_control.wrd, TS_RXPID_CONTROL + rx_ch*0x200);
	return CS_OK;
}

static int g2_ts_get_default_queue(cs_uint8 rx_ch, cs_uint8 *def_q)
{
	TS_RXPID_CONTROL_t	rxpid_control;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxpid_control.wrd = readl(TS_RXPID_CONTROL + rx_ch*0x200);
	*def_q = rxpid_control.bf.default_qid;

	return CS_OK;
}

static int g2_ts_set_default_queue(cs_uint8 rx_ch, cs_uint8 def_q)
{
	TS_RXPID_CONTROL_t	rxpid_control;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxpid_control.wrd = readl(TS_RXPID_CONTROL + rx_ch*0x200);
	rxpid_control.bf.default_qid = def_q;
	writel(rxpid_control.wrd, TS_RXPID_CONTROL + rx_ch*0x200);

	return CS_OK;
}

#ifdef USED
static int g2_ts_get_sync(cs_uint8 rx_ch, G2_TS_RXPID_SYNC_T *info)
{
	TS_RXPID_SYNC_t		rxpid_sync;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	/* To check if the sync byte index(0~2) is valid. */
	if (info->index >= G2_TS_MAX_SYNC_BYTE_COUNT)
		return CS_ERROR;

	rxpid_sync.wrd = readl(TS_RXPID_SYNC + rx_ch*0x200);

	switch (info->index) {
	case 0:
		info->sync_byte = rxpid_sync.bf.sync_byte0;
		info->sync_byte_en = rxpid_sync.bf.sync_byte0_en;
		break;

	case 1:
		info->sync_byte = rxpid_sync.bf.sync_byte1;
		info->sync_byte_en = rxpid_sync.bf.sync_byte1_en;
		break;

	case 2:
		info->sync_byte = rxpid_sync.bf.sync_byte2;
		info->sync_byte_en = rxpid_sync.bf.sync_byte2_en;
		break;
	}

	return CS_OK;
}

static int g2_ts_set_sync_byte(cs_uint8 rx_ch, G2_TS_RXPID_SYNC_T info)
{
	TS_RXPID_SYNC_t		rxpid_sync;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	/* To check if the sync byte index(0~2) is valid. */
	if (info.index >= G2_TS_MAX_SYNC_BYTE_COUNT)
		return CS_ERROR;

	rxpid_sync.wrd = readl(TS_RXPID_SYNC + rx_ch*0x200);

	switch (info.index) {
	case 0:
		rxpid_sync.bf.sync_byte0 	= info.sync_byte;
		rxpid_sync.bf.sync_byte0_en 	= info.sync_byte_en;
		break;

	case 1:
		rxpid_sync.bf.sync_byte1 	= info.sync_byte;
		rxpid_sync.bf.sync_byte1_en 	= info.sync_byte_en;
		break;

	case 2:
		rxpid_sync.bf.sync_byte2 	= info.sync_byte;
		rxpid_sync.bf.sync_byte2_en 	= info.sync_byte_en;
		break;
	}

	writel(rxpid_sync.wrd, TS_RXPID_SYNC + rx_ch*0x200);

	return CS_OK;
}

/* This function gets the RX buffer size for the RX channel. */
static int g2_ts_get_rx_buf_size(cs_uint8 rx_ch, cs_uint8 *buf_size)
{
	TS_RXPID_RXBUF_SIZE_t	rxbuf_size;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxbuf_size.wrd = readl(TS_RXPID_RXBUF_SIZE + rx_ch*0x200);
	*buf_size = rxbuf_size.bf.entry_num;

	return CS_OK;
}

/* This function sets the RX buffer size for the RX channel. */
static int g2_ts_set_rx_buf_size(cs_uint8 rx_ch, cs_uint8 buf_size)
{
	TS_RXPID_RXBUF_SIZE_t	rxbuf_size;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	rxbuf_size.wrd = readl(TS_RXPID_RXBUF_SIZE + rx_ch*0x200);
	rxbuf_size.bf.entry_num = buf_size;
	writel(rxbuf_size.wrd, TS_RXPID_RXBUF_SIZE + rx_ch*0x200);

	return CS_OK;
}

/* This function sets the "Read with MSB clear" or "Read with ALL clear" for the timer*/
static int g2_ts_set_cnt_read(cs_uint8 rx_ch, cs_boolean clear_mode)
{

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	if (clear_mode) {	/* read with MSB clear */
		writel(0x1f, TS_RXPID_CNT_READ + rx_ch*0x200);
	} else {	/* read with ALL clear */
		writel(0x00, TS_RXPID_CNT_READ + rx_ch*0x200);
	}
	return CS_OK;
}

/* This function gets the data timeout timer reload value for not receiving good packet */
static int g2_ts_get_data_timeout_reload(cs_uint8 rx_ch, cs_uint32 *value)
{
	TS_RXPID_DATA_TIMEOUT_RELOAD_t	timeout_reload;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	timeout_reload.wrd = readl(TS_RXPID_DATA_TIMEOUT_RELOAD + rx_ch*0x200);
	*value = timeout_reload.bf.value;

	return CS_OK;
}

/* This function sets the data timeout timer reload value for not receiving good packet */
static int g2_ts_set_data_timeout_reload(cs_uint8 rx_ch, cs_uint32 value)
{
	TS_RXPID_DATA_TIMEOUT_RELOAD_t	timeout_reload;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	timeout_reload.wrd = readl(TS_RXPID_DATA_TIMEOUT_RELOAD + rx_ch*0x200);
	timeout_reload.bf.value = value;
	writel(timeout_reload.wrd, TS_RXPID_DATA_TIMEOUT_RELOAD + rx_ch*0x200);

	return CS_OK;
}

static int g2_ts_get_data_timeout_timer(cs_uint8 rx_ch, cs_uint32 *value)
{
	TS_RXPID_DATA_TIMEOUT_TIMER_t	timeout_timer;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	timeout_timer.wrd = readl(TS_RXPID_DATA_TIMEOUT_TIMER + rx_ch*0x200);
	*value = timeout_timer.bf.value;

	return CS_OK;
}

/* This function gets good RX packet that received from the RX serial interface. */
static int g2_ts_get_good_pktcnt(cs_uint8 rx_ch, cs_uint32 *counter)
{
	TS_RXPID_GOOD_PKTCNT_t	good_pktcnt;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	good_pktcnt.wrd = readl(TS_RXPID_GOOD_PKTCNT + rx_ch*0x200);
	*counter = good_pktcnt.bf.couter;

	return CS_OK;
}

/* This function gets good RX packet that dropped by the PID does not match. */
static int g2_ts_get_pid_unmatch_drop_pktcnt(cs_uint8 rx_ch, cs_uint32 *counter)
{
	TS_RXPID_PID_UNMATCH_DROP_PKTCNT_t 	unmatch_drop;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	unmatch_drop.wrd = readl(TS_RXPID_PID_UNMATCH_DROP_PKTCNT + rx_ch*0x200);
	*counter = unmatch_drop.bf.counter;

	return CS_OK;
}

/* This function gets good RX packet that dropped by the RX buffer is full */
static int g2_ts_get_buffer_full_drop_pktcnt(cs_uint8 rx_ch, cs_uint32 *counter)
{
	TS_RXPID_BUFFER_FULL_DROP_PKTCNT_t	full_drop;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	full_drop.wrd = readl(TS_RXPID_BUFFER_FULL_DROP_PKTCNT + rx_ch*0x200);
	*counter = full_drop.bf.counter;

	return CS_OK;
}

/* This function gets frame counter no matter the packet is good or bad.*/
static int g2_ts_get_frame_cnt(cs_uint8 rx_ch, cs_uint32 *counter)
{
	TS_RXPID_FRAME_CNT_t	frame_cnt;

	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	frame_cnt.wrd = readl(TS_RXPID_FRAME_CNT + rx_ch*0x200);
	*counter = frame_cnt.bf.counter;

	return CS_OK;
}

/* This function gets the clock counter. */
static int g2_ts_get_clock_cnt(cs_uint8 rx_ch, cs_uint32 *counter)
{
	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	*counter = readl(TS_RXPID_CLOCK_CNT + rx_ch*0x200);

	return CS_OK;
}
#endif

/* This function sets the parameters of PID filter table */
int g2_ts_set_pid_table_entry(unsigned int rx_ch,unsigned int index,
                     G2_TS_RXPID_ENTRY_T rxpid_entry)
{
	TS_RXPID_ENTRY0_WORD0_t		word0;
	TS_RXPID_ENTRY0_WORD1_t		word1;
	unsigned int			addr0;
	unsigned int			addr1;

	/* To check if the channel number and entry number are valid. */
	if ((rx_ch >= G2_TS_MAX_CHANNEL) || (index >= G2_TS_MAX_PID_ENTRY))
		return CS_ERROR;

	addr0 = TS_RXPID_ENTRY0_WORD0 + rx_ch*0x200 + index*8;
	word0.wrd = readl(addr0);
	addr1 = TS_RXPID_ENTRY0_WORD0 + rx_ch*0x200 + index*8 + 4;
	word1.wrd = readl(addr1);

	word0.bf.pid 		= rxpid_entry.pid;
	word0.bf.qid0 		= rxpid_entry.qid0;
	word0.bf.new_pid0 	= rxpid_entry.new_pid0;
	word0.bf.action0 	= rxpid_entry.action0;
	word0.bf.valid0 	= rxpid_entry.valid0;
	word1.bf.qid1 		= rxpid_entry.qid1;
	word1.bf.new_pid1 	= rxpid_entry.new_pid1;
	word1.bf.action1 	= rxpid_entry.action1;
	word1.bf.valid1 	= rxpid_entry.valid1;

	writel(word0.wrd, addr0);
	writel(word1.wrd, addr1);

	return CS_OK;
}

/* This function gets the parameters of PID filter table */
int g2_ts_get_pid_table_entry(unsigned int rx_ch,unsigned int index,
				G2_TS_RXPID_ENTRY_T *rxpid_entry)
{
	TS_RXPID_ENTRY0_WORD0_t		word0;
	TS_RXPID_ENTRY0_WORD1_t		word1;
	unsigned int			addr0;
	unsigned int			addr1;

	/* To check if the channel number and entry number are valid. */
	if ((rx_ch >= G2_TS_MAX_CHANNEL) || (index >= G2_TS_MAX_PID_ENTRY))
		return CS_ERROR;

	addr0 = TS_RXPID_ENTRY0_WORD0 + rx_ch*0x200 + index*8;
	word0.wrd = readl(addr0);
	addr1 = TS_RXPID_ENTRY0_WORD0 + rx_ch*0x200 + index*8 + 4;
	word1.wrd = readl(addr1);

	rxpid_entry->pid 	= word0.bf.pid;
	rxpid_entry->qid0	= word0.bf.qid0;
	rxpid_entry->new_pid0 	= word0.bf.new_pid0;
	rxpid_entry->action0 	= word0.bf.action0;
	rxpid_entry->valid0 	= word0.bf.valid0;
	rxpid_entry->qid1 	= word1.bf.qid1;
	rxpid_entry->new_pid1 	= word1.bf.new_pid1;
	rxpid_entry->action1 	= word1.bf.action1;
	rxpid_entry->valid1 	= word1.bf.valid1;

	return CS_OK;
}

/* This function gets the INTERRUPT status bits. */
static int g2_ts_get_rxpid_interrupt(cs_uint8 rx_ch, cs_uint32 *int_status)
{
	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	*int_status = readl(TS_RXPID_INTERRUPT + rx_ch*0x200);

	return CS_OK;
}

/* This function clears the INTERRUPT status bits. */
static int g2_ts_set_rxpid_interrupt(cs_uint8 rx_ch, cs_uint32 int_status)
{
	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	writel(int_status, TS_RXPID_INTERRUPT + rx_ch*0x200);

	return CS_OK;
}

#ifdef USED
/* This function gets the enable bits for the INTERRUPT register. */
static int g2_ts_get_rxpid_intenable(cs_uint8 rx_ch, cs_uint32 *int_en)
{
	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	*int_en = readl(TS_RXPID_INTENABLE + rx_ch*0x200);

	return CS_OK;
}
#endif

/* This function sets the enable bits for the INTERRUPT register. */
static int g2_ts_set_rxpid_intenable(cs_uint8 rx_ch, cs_uint32 int_en)
{
	/* To check if the channel number is valid. */
	if (rx_ch >= G2_TS_MAX_CHANNEL)
		return CS_ERROR;

	writel(int_en, TS_RXPID_INTENABLE + rx_ch*0x200);

	return CS_OK;
}



#if 0

/* This function enables or disables PID filter table of channel N. */
int ts_set_pid_table_mode(unsigned int table_id, unsigned int mode)
{
	TS_GLOBAL_CONTROL_t	ts_ctrl,ts_ctrl_mask;

	if (table_id >= G2_TS_MAX_CHANNEL)
		return TS_ERR;

	ts_ctrl_mask.wrd = 1 << (TS_GLOBAL_CONTROL_PID_EN_START_BIT + table_id);
	if (mode == TS_ENABLE) {
		ts_ctrl.wrd = 1 << (TS_GLOBAL_CONTROL_PID_EN_START_BIT + table_id);
	} else {
		ts_ctrl.wrd = 0;
	}
	g2_ts_write_reg(TS_GLOBAL_CONTROL, ts_ctrl.wrd, ts_ctrl_mask.wrd);

	return (TS_OK);
}

/* This function gets the status from PID filter table of channel N. */
int ts_get_pid_table_mode(unsigned int table_id, unsigned int *mode)
{
	TS_GLOBAL_CONTROL_t	ts_ctrl;

	if (table_id >= G2_TS_MAX_CHANNEL)
		return TS_ERR;

	ts_ctrl.wrd = g2_ts_read_reg(TS_GLOBAL_CONTROL);
	*mode = ts_ctrl.wrd & (1 << (TS_GLOBAL_CONTROL_PID_EN_START_BIT + table_id));

	return (TS_OK);
}

/* This function enables or disables TS RX function of channel N. */
int ts_set_rx_mode(unsigned int table_id, unsigned int mode)
{
	TS_GLOBAL_CONTROL_t	ts_ctrl,ts_ctrl_mask;

	if (table_id >= G2_TS_MAX_CHANNEL)
		return TS_ERR;

	ts_ctrl_mask.wrd = 1 << (TS_GLOBAL_CONTROL_RX_EN_START_BIT + table_id);
	if (mode == TS_ENABLE) {
		ts_ctrl.wrd = 1 << (TS_GLOBAL_CONTROL_RX_EN_START_BIT + table_id);
	} else {
		ts_ctrl.wrd = 0;
	}
	g2_ts_write_reg(TS_GLOBAL_CONTROL, ts_ctrl.wrd, ts_ctrl_mask.wrd);

	return (TS_OK);
}

/* This function gets the TS RX status of channel N. */
int ts_get_rx_mode(unsigned int table_id, unsigned int *mode)
{
	TS_GLOBAL_CONTROL_t	ts_ctrl;

	if (table_id >= G2_TS_MAX_CHANNEL)
		return TS_ERR;

	ts_ctrl.wrd = g2_ts_read_reg(TS_GLOBAL_CONTROL);
	*mode = ts_ctrl.wrd & (1 << (TS_GLOBAL_CONTROL_RX_EN_START_BIT + table_id));

	return (TS_OK);
}

#endif


int g2_ts_rxq_map_init(G2_TS_INFO_T *tsdvb)
{
	unsigned int	i;

	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		tsdvb->rxq_map[i].qid 	= i;
		tsdvb->rxq_map[i].channel = i % G2_TS_MAX_CHANNEL;
		tsdvb->rxq_map[i].type	= (i<G2_TS_MAX_CHANNEL)
						?CS_TS_DATA:CS_TS_CONTROL;
		tsdvb->rxq_map[i].depth	= (i<G2_TS_MAX_CHANNEL)
						?(G2_TS_SW_RXQ_DEPTH)
						:(G2_TS_SW_RXQ_DEPTH+2);
		if (packet_unit == 0)
			tsdvb->rxq_map[i].size	= (i<G2_TS_MAX_CHANNEL)
						?(G2_TS_RX_DATA_SIZE)
						:(G2_TS_RX_CONTROL_SIZE);
		else
			tsdvb->rxq_map[i].size	= (i<G2_TS_MAX_CHANNEL)
						?(packet_unit*188)
						:(G2_TS_RX_CONTROL_SIZE);
	}
	return CS_OK;
}

int g2_ts_axi_bus_init(void)
{
	writel(0x0f0000c0, TS_T_AXI_CONFIG);

	return CS_OK;
}

int g2_ts_rxpid_init(void)
{
	return CS_OK;
}

int g2_ts_int_enable_init(void)
{
	unsigned int	i;

	/* Enable RXPID interrupt registers */
	for (i=0; i<G2_TS_MAX_CHANNEL; i++) {
		g2_ts_set_rxpid_intenable(i,G2_TS_DISABLE_ALL_INT);
	}

	/* Enable DMA interrupt registers */
	g2_ts_set_dma_desc_intenable(G2_TS_ENABLE_ALL_INT);
	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		/* enable rxq0_eof_en & rxq0_full_en bit */
		g2_ts_set_dma_rxq_intenable(i,G2_TS_RXQ_EOF_FULL);
	}

	/* Enable AXI interrupt registers */
	writel(G2_TS_ENABLE_ALL_INT,TS_T_AXI_READ_CHANNEL000_INTERRUPT_ENABLE);
	writel(G2_TS_ENABLE_ALL_INT,TS_T_AXI_WRITE_CHANNEL000_INTERRUPT_ENABLE);
	writel(G2_TS_ENABLE_ALL_INT,TS_T_AXI_WRITE_CHANNEL001_INTERRUPT_ENABLE);

	return CS_OK;
}

int g2_ts_iomux_init(void)
{
	unsigned int	reg_val;

	reg_val = readl(GLOBAL_GPIO_MUX_1);
	reg_val = reg_val & (~TS_GLOBAL_GPIO_MUX_1);
	writel(reg_val, GLOBAL_GPIO_MUX_1);

	reg_val = readl(GLOBAL_GPIO_MUX_2);
	reg_val = reg_val & (~TS_GLOBAL_GPIO_MUX_2);
	writel(reg_val, GLOBAL_GPIO_MUX_2);

	return CS_OK;
}

int g2_ts_dma_control_init(void)
{
	G2_TS_RXDMA_CONTROL ctrl;
	cs_uint32			i;

	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		/* set "Read with ALL clear" */
		g2_ts_set_rxq_pktcnt_read(i,0,0);
	}

	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		g2_ts_set_rxq_full_threshold(i,G2_TS_FULL_THRESHOLD_DEPTH);
	}

	for (i=0; i<G2_TS_MAX_CHANNEL; i++) {
		g2_ts_set_default_queue(i,i);
		/* 1: for Si2165 DEMUX  0: for MAXLINEAR DEMUX */
		g2_ts_set_start_pulse(i,0);
	}

	ctrl.rx_burst_len 	= 2;
	ctrl.rx_check_own 	= CS_ENABLE;
	ctrl.rx_dma_enable 	= CS_ENABLE;
	g2_ts_set_rxdma_control(ctrl);

	return (0);
}

int g2_ts_dma_buffer_init(G2_TS_INFO_T *tsdvb, cs_uint8 qid)
{
	G2_TS_RX_DESC_T	*rxq_desc;
	unsigned char   *buf_addr;
	unsigned int	desc_count;
	unsigned int	buf_size;
	unsigned int	i;

	if (tsdvb->rxq_map[qid].init_done == CS_ENABLE)
		return CS_ERROR;

	buf_size = tsdvb->rxq_map[qid].size;
	desc_count = (1 << tsdvb->rxq_map[qid].depth);
	rxq_desc = (G2_TS_RX_DESC_T *)tsdvb->dma_desc[qid].rxq_desc_base;
	printk("%s : RXQ Desc=%x desc_count=%d buf_size=%d \n",__func__,
			(cs_uint32)rxq_desc,desc_count,buf_size);

	for (i=0; i<desc_count; i++) {
		buf_addr = kzalloc(buf_size, GFP_KERNEL);
		if (!buf_addr) {
			printk("%s::Buffer allocation fail !\n",__func__);
			return CS_ERROR;
		} else {
			dprintk("%s : TS RXQ %d buffer %i address = %x \n",
				__func__,qid,i,(unsigned int)buf_addr);
		}

		/* fill initial value in rxq dma descriptor */
		rxq_desc->word0.bits.own = DMA;
		rxq_desc->word0.bits.buf_size = buf_size;
		rxq_desc->word1.buf_addr = (unsigned int)__pa(buf_addr);
		rxq_desc++;
	}
	tsdvb->rxq_map[qid].init_done = CS_ENABLE;

	return CS_OK;
}

int g2_ts_dma_buffer_free(G2_TS_INFO_T *tsdvb, cs_uint8 qid)
{
	G2_TS_RX_DESC_T	*rxq_desc;
	unsigned int	desc_count;
	unsigned int	i;

	desc_count = (1 << tsdvb->rxq_map[qid].depth);
	rxq_desc = (G2_TS_RX_DESC_T	*)tsdvb->dma_desc[qid].rxq_desc_base;
	for (i=0; i<desc_count; i++) {
		kfree(__va(rxq_desc->word1.buf_addr));
		rxq_desc++;
	}

	return CS_OK;
}

int g2_ts_rxq_buffer_init(unsigned char id)
{
	G2_TS_INFO_T *tsdvb = ts_dvb_info;

	return(g2_ts_dma_buffer_init(tsdvb,id));
}

int g2_ts_dma_desc_init(G2_TS_INFO_T *tsdvb)
{
	unsigned char	def_qid;
	unsigned int 	i;
	unsigned int	desc_count;

	/* init rx descriptions for TS RX queue */
	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		desc_count = (1 << tsdvb->rxq_map[i].depth);

		/* allocate DMA descriptor buffer for RX queues */
		tsdvb->dma_desc[i].rxq_desc_base = (unsigned int)DMA_MALLOC(
			(desc_count * sizeof(G2_TS_RX_DESC_T)),
			(dma_addr_t *)&tsdvb->dma_desc[i].rxq_desc_base_dma );

		if (!tsdvb->dma_desc[i].rxq_desc_base) {
			printk("%s::DMA_MALLOC fail !\n",__func__);
			return CS_ERROR;
		}

		printk("%s : RXQ %d desc base = %x dma base=%x desc count=%d\n",
			__func__,i,
        		tsdvb->dma_desc[i].rxq_desc_base,
        		tsdvb->dma_desc[i].rxq_desc_base_dma,
        		desc_count);

		memset((void *)tsdvb->dma_desc[i].rxq_desc_base, 0,
			G2_TS_SW_RXQ_DESC_NUM * sizeof(G2_TS_RX_DESC_T));

		/* write the descriptor base register and queue depth */
		g2_ts_set_rxq_base_depth(i,
					tsdvb->dma_desc[i].rxq_desc_base_dma,
					tsdvb->rxq_map[i].depth);
	}

	/* init DMA descriptor and buffers for default queue */
	g2_ts_get_default_queue(0, &def_qid);
	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		if (g2_ts_dma_buffer_init(tsdvb, i))
			return CS_ERROR;
	}

	return CS_OK;
}

int g2_ts_dma_desc_free(G2_TS_INFO_T *tsdvb)
{
	unsigned int	desc_count;
	unsigned int 	i;

	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		desc_count = (1 << tsdvb->rxq_map[i].depth);

		DMA_MFREE((void *)tsdvb->dma_desc[i].rxq_desc_base,
			(desc_count * sizeof(G2_TS_RX_DESC_T)),
			(dma_addr_t)&tsdvb->dma_desc[i].rxq_desc_base_dma);
	}
	return CS_OK;
}

int g2_ts_dsec_int_process(G2_TS_INFO_T *ts_dvb)
{
	unsigned int	desc_int;

	g2_ts_get_dma_desc_interrupt(&desc_int);/* get interrupt status */
	g2_ts_set_dma_desc_interrupt(desc_int);	/* clear interrupt status */
	printk("%s : RX DMA descriptor error! (0x%x)\n",__func__,desc_int);

	return CS_OK;
}

int g2_ts_rxpid_int_process(G2_TS_INFO_T *ts_dvb)
{
	unsigned int	rxpid_int;
	unsigned int	i;

	for (i=0; i<G2_TS_MAX_CHANNEL; i++) {
		/* get interrupt status */
		g2_ts_get_rxpid_interrupt(i, &rxpid_int);
		/* clear interrupt status */
		g2_ts_set_rxpid_interrupt(i, rxpid_int);
	}

	return CS_OK;
}

int g2_ts_axi_int_process(G2_TS_INFO_T *ts_dvb)
{
	unsigned int	axi_read0_int;
	unsigned int	axi_write0_int;
	unsigned int	axi_write1_int;

	axi_read0_int = readl(TS_T_AXI_READ_CHANNEL000_INTERRUPT);
	writel(axi_read0_int,TS_T_AXI_READ_CHANNEL000_INTERRUPT);

	axi_write0_int = readl(TS_T_AXI_WRITE_CHANNEL000_INTERRUPT);
	writel(axi_write0_int,TS_T_AXI_WRITE_CHANNEL000_INTERRUPT);

	axi_write1_int = readl(TS_T_AXI_WRITE_CHANNEL001_INTERRUPT);
	writel(axi_write1_int,TS_T_AXI_WRITE_CHANNEL001_INTERRUPT);

	printk("%s : AXI Interrupt! (0x%x 0x%x 0x%x)\n",__func__,
			axi_read0_int,axi_write0_int,axi_write1_int);

	return CS_OK;
}

int g2_ts_dma_rx_packet(G2_TS_INFO_T *tsdvb, cs_uint32 rxq_int)
{
	unsigned int	rxq_wptr;
	unsigned int	rxq_rptr;
	G2_TS_RX_DESC_T *curr_desc;
	unsigned int 	i;
	unsigned int	int_status;

	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		/* check which rxq arises the interrupt */
		if ( rxq_int & (1<<i) ) {
			g2_ts_get_dma_rxq_interrupt(i, &int_status);
			/* clear interrupt status */
			g2_ts_set_dma_rxq_interrupt(i, int_status);
			if ((int_status & G2_TS_RXQ_EOF_FULL)==0) {
				printk("%s: qid=%d interrupt status=%x \n",
					__func__,i,int_status);
				continue;
			}
		} else {
			continue;
		}

		/* get write pointer of queue i */
		rxq_wptr = g2_ts_get_rxq_wptr(i);
		/* get read pointer of queue i */
		rxq_rptr = g2_ts_get_rxq_rptr(i);

		/* to check the RX queue is not empty */
		while (rxq_wptr != rxq_rptr) {
			/* to get RX descriptor pointer of RX queue i */
			curr_desc = (G2_TS_RX_DESC_T *)tsdvb->dma_desc[i].rxq_desc_base + rxq_rptr;
			wmb();
			if (curr_desc->word0.bits.own == CPU) {
				/* read TS packet from RX queue */
				/* and sent to DVB core.        */
				tsdvb->demux[i].playing = i;/*indicate the RXQ ID*/
				dvb_dmx_swfilter_packets(&tsdvb->demux[i],
					__va((unsigned char *)curr_desc->word1.buf_addr),
					tsdvb->rxq_map[i].size);
				wmb();
				dma_map_single(NULL,__va((unsigned char *)curr_desc->word1.buf_addr),
                				tsdvb->rxq_map[i].size,
                				DMA_FROM_DEVICE);
				curr_desc->word0.bits.buf_size = tsdvb->rxq_map[i].size;
				curr_desc->word0.bits.own = DMA;
			}
			rxq_rptr++;
			if (rxq_rptr == (1<<tsdvb->rxq_map[i].depth))
				rxq_rptr = 0;
		}
		g2_ts_set_rxq_rptr(i, rxq_rptr);
	}

	return CS_OK;
}

irqreturn_t g2_ts_isr(int irq, void *dev_id)
{
	G2_TS_INFO_T	*ts_dvb = dev_id;
	cs_uint32	ts_interrupt;

	/* disable global TS interrupt */
	g2_ts_set_dma_intenable_0(G2_TS_DISABLE_ALL_INT);

	/* read interrupt status */
	g2_ts_get_dma_interrupt_0(&ts_interrupt);

	/* When these bits are set, it indicates that
	   the RXQn_INTERRUPT register has an active source */
	if (ts_interrupt & G2_TS_DMA_RXQ_INT)
		g2_ts_dma_rx_packet(ts_dvb, (ts_interrupt>>1));

	/* When this bit is set, it indicates that
	   the DESC_INTERRUPT register has an active source */
	if (ts_interrupt & G2_TS_DMA_DESC_INT)
		g2_ts_dsec_int_process(ts_dvb);

	/* When these bits are set, it indicates that
	   the AXI INTERRUPT register has an active source */
	if (ts_interrupt & G2_TS_DMA_AXI_INT)
		g2_ts_axi_int_process(ts_dvb);

	/* When these bits are set, it indicates that
	   the RXPIDn INTERRUPT register has an active source */
	if (ts_interrupt & G2_TS_DMA_RXPID_INT)
		g2_ts_rxpid_int_process(ts_dvb);

	/* enable global TS interrupt */
	g2_ts_set_dma_intenable_0(G2_TS_ENABLE_ALL_INT);

	return IRQ_HANDLED;
}

int g2_ts_flush_pid_table(unsigned int channel, unsigned short *pids)
{
	G2_TS_RXPID_ENTRY_T	rxpid;
	unsigned int		i;

	for (i=0; i<G2_TS_PID_TABLE_MAX_ENTRY; i++) {
		rxpid.pid 	= 0;
		rxpid.qid0 	= 0;
		rxpid.new_pid0 	= 0;
		rxpid.action0 	= 0;
		rxpid.valid0 	= CS_DISABLE;
		rxpid.qid1 	= 0;
		rxpid.new_pid1 	= 0;
		rxpid.action1 	= 0;
		rxpid.valid1 	= CS_DISABLE;
		g2_ts_set_pid_table_entry(channel, i, rxpid);
	}

	return CS_OK;
}

int g2_ts_dmx_add_pid(unsigned char qid, unsigned short pid)
{
	G2_TS_INFO_T 		*tsdvb = ts_dvb_info;
	G2_TS_RXPID_ENTRY_T	rxpid;
	unsigned char		channel;
	unsigned int		index = 0;
	cs_ts_type_t		q_type;
	unsigned int		i;
	unsigned char		found = false;

	dprintk("\n\n%s: qid=%d  PID=%d \n",__func__,qid,pid);

	if (qid >= G2_TS_MAX_QUEUE_NUM) {
		return CS_ERROR;
	}

	channel = tsdvb->rxq_map[qid].channel;	/* get TS port number */
	q_type  = tsdvb->rxq_map[qid].type;		/* get TS queue type */

	/* search rxpid table to check if it is exist */
	for (i=0; i<G2_TS_PID_TABLE_MAX_ENTRY; i++) {
		g2_ts_get_pid_table_entry(channel, i, &rxpid);
		if ((rxpid.valid0 == CS_ENABLE) && (rxpid.pid == pid)) {
			index = i;
			found = true;
			break;
		}
	}

	if (q_type == CS_TS_DATA) {
		if (found) {
			return CS_OK;
		} else {
			/* To find a free entry for adding a new PID */
			for (i=0; i<G2_TS_PID_TABLE_MAX_ENTRY; i++) {
				g2_ts_get_pid_table_entry(channel, i, &rxpid);
				if (rxpid.valid0 == CS_DISABLE) {
					index = i;
					break;
				}
			}
			if (i == G2_TS_PID_TABLE_MAX_ENTRY) {
				printk("%s : PID table %d is full. \n",__func__,channel);
				return CS_ERROR;
			}

			rxpid.pid 	= pid;
			rxpid.qid0 	= qid;
			rxpid.new_pid0 	= 0;
			rxpid.action0 	= 0;
			rxpid.valid0 	= CS_ENABLE;
			g2_ts_set_pid_table_entry(channel, index, rxpid);
		}
	} else {
		if (!found) {
			/* printk("%s : Can not find the PID in Data Queue! \n",__func__); */
			/* printk("Add PID %d to queue %d...\n",pid,channel); */
			/* To find a free entry for adding a new PID */
			for (i=0; i<G2_TS_PID_TABLE_MAX_ENTRY; i++) {
				g2_ts_get_pid_table_entry(channel, i, &rxpid);
				if (rxpid.valid0 == CS_DISABLE) {
					index = i;
					break;
				}
			}
			if (i == G2_TS_PID_TABLE_MAX_ENTRY) {
				printk("%s : PID table %d is full. \n",__func__,channel);
				return CS_ERROR;
			}
	
			rxpid.pid 	= pid;
			rxpid.qid0 	= qid - G2_TS_MAX_CHANNEL;
			rxpid.new_pid0 	= 0;
			rxpid.action0 	= 0;
			rxpid.valid0 	= CS_ENABLE;
			g2_ts_set_pid_table_entry(channel, index, rxpid);
		}
		
		g2_ts_get_pid_table_entry(channel, index, &rxpid);
		rxpid.qid1 	= qid;
		rxpid.new_pid1 	= 0;
		rxpid.action1 	= 0;
		rxpid.valid1 	= CS_ENABLE;
		g2_ts_set_pid_table_entry(channel, index, rxpid);
	}
	return CS_OK;
}

int g2_ts_dmx_remove_pid(unsigned char qid, unsigned short pid)
{
	G2_TS_INFO_T 		*tsdvb = ts_dvb_info;
	G2_TS_RXPID_ENTRY_T	rxpid;
	unsigned char		channel;
	cs_ts_type_t		q_type;
	unsigned int		i;

	dprintk("\n%s: qid=%d  PID=%d \n",__func__,qid,pid);

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	if (pid==0)
		g2_ts_flush_pid_table(qid, &pid);

	channel = tsdvb->rxq_map[qid].channel;	/* get TS port number */
	q_type  = tsdvb->rxq_map[qid].type;	/* get TS queue type */

	/* search rxpid table to check if it is exist */
	for (i=0; i<G2_TS_PID_TABLE_MAX_ENTRY; i++) {
		g2_ts_get_pid_table_entry(channel, i, &rxpid);
		if ((rxpid.valid0 == CS_ENABLE) && (rxpid.pid == pid)) {
			rxpid.pid 	= 0;
			rxpid.qid0 	= qid;
			rxpid.new_pid0 	= 0;
			rxpid.action0 	= 0;
			rxpid.valid0 	= CS_DISABLE;
			rxpid.qid1 	= qid;
			rxpid.new_pid1 	= 0;
			rxpid.action1 	= 0;
			rxpid.valid1 	= CS_DISABLE;
			g2_ts_set_pid_table_entry(channel, i, rxpid);
			return CS_OK;
		}
	}

	return CS_ERROR;
}


static int g2_ts_start_feed(struct dvb_demux_feed *f)
{
	G2_TS_INFO_T 		*tsdvb = feed_to_ts(f);
	struct dmxdev_filter 	*filter;
	unsigned int		qid = 0;
	unsigned int		channel;

	filter = (struct dmxdev_filter *)f->feed.ts.priv;
	qid = filter->dev->dvbdev->id;
	dprintk("\n\n %s : dvbdev->id = %d pid=%d index=%d\n",
			__func__,qid,f->pid,f->index); 

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	channel = tsdvb->rxq_map[qid].channel;
	
	if (f->pid < 0x2000)
		g2_ts_dmx_add_pid(qid, f->pid);

	if (f->pid == 0x2000)	/* disable H/W PID filter--> include all PIDs */
		g2_ts_set_rxpid_enable(channel, CS_DISABLE); 
	else
		g2_ts_set_rxpid_enable(channel, CS_ENABLE);		

	g2_ts_set_rx_enable(channel,CS_ENABLE);

	return CS_OK;
}

static int g2_ts_stop_feed(struct dvb_demux_feed *f)
{
	G2_TS_INFO_T *tsdvb = feed_to_ts(f);
	struct dmxdev_filter 	*filter;
	unsigned int		qid = 0;
	unsigned int		channel;

	filter = (struct dmxdev_filter *)f->feed.ts.priv;
	qid = filter->dev->dvbdev->id;
	dprintk("\n\n %s : dvbdev->id = %d pid=%d index=%d\n",
			__func__,qid,f->pid,f->index); 

	if (qid >= G2_TS_MAX_QUEUE_NUM)
		return CS_ERROR;

	channel = tsdvb->rxq_map[qid].channel;

	/* g2_ts_set_rx_enable(channel,CS_DISABLE); */

	/* printk("\n>>disable PID filtering...\n"); */
	/* g2_ts_set_rxpid_enable(channel, CS_DISABLE); */

	g2_ts_dmx_remove_pid(qid, f->pid);

	return CS_OK;
}

int ts_get_pid_table(unsigned int channel, unsigned short *pids)
{
	G2_TS_RXPID_ENTRY_T	rxpid;
	unsigned short		pid_table[G2_TS_PID_TABLE_MAX_ENTRY];
	unsigned int		i;

	for (i=0; i<G2_TS_PID_TABLE_MAX_ENTRY; i++) {
		g2_ts_get_pid_table_entry(channel, i, &rxpid);
		pid_table[i] = rxpid.pid;
	}

	memcpy(pids, pid_table, G2_TS_PID_TABLE_MAX_ENTRY*sizeof(short));
	return 0;
}

#if defined(CONFIG_DVB_MXL241SF) || defined(CONFIG_DVB_MXL241SF_MODULE)

static const struct mxl241sf_config mxl241sf_g2_config[G2_TS_MAX_CHANNEL] = {
	{
		.demod_address  = 97,		/* the I2C address */
		.xtal_freq	= 48000000,	/* xtal freq in Hz */
	},
	{
		.demod_address  = 98,		/* the I2C address */
		.xtal_freq	= 48000000,	/* xtal freq in Hz */
	},
	{
		.demod_address  = 101,		/* the I2C address */
		.xtal_freq	= 48000000,	/* xtal freq in Hz */
	},
	{
		.demod_address  = 102,		/* the I2C address */
		.xtal_freq	= 48000000,	/* xtal freq in Hz */
	},
	{
		.demod_address  = 99,		/* the I2C address */
		.xtal_freq	= 48000000,	/* xtal freq in Hz */
	},
	{
		.demod_address  = 100,		/* the I2C address */
		.xtal_freq	= 48000000,	/* xtal freq in Hz */
	},
 };

static int __devinit g2_ts_frontend_init(G2_TS_INFO_T *tsdvb)
{
	int ret, i;

	for (i = 0; i < tuner_num; i++) {
		tsdvb->frontend[i] = dvb_attach(mxl241sf_attach, &mxl241sf_g2_config[i],
					tsdvb->i2c_adap);

/*		if (tsdvb->frontend[i]) {
			tsdvb->frontend[i]->ops.set_voltage = g2_ts_set_voltage;
		}
 */
		if (!tsdvb->frontend[i]) {
			printk("Could not attach frontend %d.\n", i);
			ret = -ENODEV;
			goto failed;
		}

		ret = dvb_register_frontend(&tsdvb->dvb_adapter, tsdvb->frontend[i]);
		if (ret < 0) {
			printk("%s : Register frontend %d fail !\n", __func__, i);
			goto failed;
		}
	}
	
	return 0;

failed:
	for (i = 0; i < G2_TS_MAX_CHANNEL; i++) {
		if (tsdvb->frontend[i]) {
			if (tsdvb->frontend[i]->ops.release)
				tsdvb->frontend[i]->ops.release(tsdvb->frontend[i]);
			tsdvb->frontend[i] = NULL;
		}
	}
	return ret;
}

#endif

static int __devinit g2_ts_probe(struct platform_device *pdev)
{
	static G2_TS_INFO_T	*tsdvb;
	struct dvb_adapter 	*dvb_adapter;
	struct dvb_demux 	*dvbdemux;
	struct dmx_demux 	*dmx;
	struct resource 	*res;
	unsigned int		i;
	int ret = -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto out;
	}

	tsdvb = kzalloc(sizeof(G2_TS_INFO_T), GFP_KERNEL);
	if (!tsdvb)
		goto out;

	tsdvb->dev = &pdev->dev;

	tsdvb->irq = platform_get_irq(pdev, 0);
	if (tsdvb->irq < 0) {
		ret = -ENXIO;
		goto out;
	}

	tsdvb->base = ioremap(res->start, res->end - res->start + 1);
	if (!tsdvb->base) {
		ret = -ENOMEM;
		goto out;
	}

	printk("%s : TS base address=%x IRQ number=%d \n",__func__,
			(unsigned int)tsdvb->base,tsdvb->irq);

	platform_set_drvdata(pdev, tsdvb);
	ts_dvb_info = tsdvb;

	g2_ts_iomux_init();
	g2_ts_rxq_map_init(tsdvb);
	g2_ts_axi_bus_init();
	g2_ts_dma_desc_init(tsdvb);
	g2_ts_dma_control_init();
	g2_ts_int_enable_init();

	ret = dvb_register_adapter(&tsdvb->dvb_adapter, DRIVER_NAME,
				   THIS_MODULE, &pdev->dev, adapter_nr);
	if (ret < 0)
		return ret;

	dvb_adapter = &tsdvb->dvb_adapter;

	for (i=0;i<G2_TS_MAX_QUEUE_NUM;i++) {
		dvbdemux = &tsdvb->demux[i];
		dvbdemux->filternum = G2_TS_MAX_PID_ENTRY;
		dvbdemux->feednum = G2_TS_MAX_PID_ENTRY;
		dvbdemux->start_feed = g2_ts_start_feed;
		dvbdemux->stop_feed = g2_ts_stop_feed;
		dvbdemux->dmx.capabilities = (DMX_TS_FILTERING |
				DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING);
		ret = dvb_dmx_init(dvbdemux);
	
		if (ret < 0)
			goto err_dvb_unregister_adapter;

		dmx = &dvbdemux->dmx;

		/* To create 12 demux device. */
		tsdvb->dmxdev[i].filternum = NHWFILTERS;
		tsdvb->dmxdev[i].demux = dmx;
		tsdvb->dmxdev[i].capabilities = 0;

		ret = dvb_dmxdev_init(&tsdvb->dmxdev[i], dvb_adapter);
		if (ret > 0)
			goto err_dvb_dmx_release;
	}

	if (request_irq(tsdvb->irq, g2_ts_isr, IRQF_DISABLED, DRIVER_NAME, tsdvb)) {
		printk("Error: Register IRQ for TS control failed\n");
	}

	g2_ts_set_dma_intenable_0(G2_TS_ENABLE_ALL_INT);

	/* get i2c adapter */
	tsdvb->i2c_adap = i2c_get_adapter(0);
	if (!tsdvb->i2c_adap) {
		printk("can't get i2c adapter 0\n");
		return 1;
	}

#if defined(CONFIG_DVB_MXL241SF) || defined(CONFIG_DVB_MXL241SF_MODULE)
	g2_ts_frontend_init(tsdvb);
#endif	

out:
	return 0;

err_dvb_dmx_release:
	for (i=0;i<G2_TS_MAX_QUEUE_NUM;i++) {
		dvb_dmx_release(&tsdvb->demux[i]);
	}
err_dvb_unregister_adapter:
	dvb_unregister_adapter(dvb_adapter);
	kfree(tsdvb);
	goto out;
}

static int __devexit g2_ts_remove(struct platform_device *pdev)
{
	G2_TS_INFO_T 		*tsdvb = platform_get_drvdata(pdev);
	struct dvb_adapter 	*dvb_adapter = &tsdvb->dvb_adapter;
	struct dvb_demux 	*dvbdemux;
	struct dmx_demux 	*dmx;
	unsigned int		i;

	for (i=0;i<G2_TS_MAX_QUEUE_NUM;i++) {
		dvbdemux = &tsdvb->demux[i];
		dmx = &dvbdemux->dmx;
		dmx->close(dmx);
	}
	
	for (i=0;i<G2_TS_MAX_QUEUE_NUM;i++)
		dvb_dmxdev_release(&tsdvb->dmxdev[i]);


	for (i=0; i<G2_TS_MAX_QUEUE_NUM; i++) {
		if (g2_ts_dma_buffer_free(tsdvb, i))
			return CS_ERROR;
	}

	g2_ts_dma_desc_free(tsdvb);

	for (i=0;i<G2_TS_MAX_QUEUE_NUM;i++)
		dvb_dmx_release(&tsdvb->demux[i]);

	for (i=0; i<G2_TS_MAX_CHANNEL; i++) {
		if (tsdvb->frontend[i])
			dvb_unregister_frontend(tsdvb->frontend[i]);
	}
	dvb_unregister_adapter(dvb_adapter);
	free_irq(tsdvb->irq, tsdvb);
	platform_set_drvdata(pdev, NULL);
	kfree(tsdvb);
	return 0;
}


static struct platform_driver g2_ts_driver = {
	.driver		= {
		.name	= "g2-ts",
		.owner	= THIS_MODULE,
	},
	.probe		= g2_ts_probe,
	.remove		= __devexit_p(g2_ts_remove),
};

int __init g2_ts_init(void)
{
	return platform_driver_register(&g2_ts_driver);
}

void __exit g2_ts_exit(void)
{
	platform_driver_unregister(&g2_ts_driver);
}

module_init(g2_ts_init);
module_exit(g2_ts_exit);

module_param(tsdebug, int, 0644);
MODULE_PARM_DESC(tsdebug, "Turn on/off debugging (default:off).");

module_param(tuner_num, int, 0644);
MODULE_PARM_DESC(tuner_num, "Set the port number of MaxLinear tuner.");

module_param(packet_unit, int, 0644);
MODULE_PARM_DESC(packet_unit, "Set the TS data packet size : packet_unit*188B");

MODULE_AUTHOR("Amos Lee <amos.lee@cortina-system.com>");
MODULE_DESCRIPTION("G2 Transport Stream Driver");
MODULE_LICENSE("GPL");


