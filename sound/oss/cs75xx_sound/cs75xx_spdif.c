/*
 * FILE NAME cs75xx_spdif.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX SPDIF interface.
 *
 *  Copyright 2010 Cortina , Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

//#define DEBUG
#define DMA_FLUSH
#define SUPPORT_AC3
#define SUPPORT_DTS

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sound.h>
#include <linux/interrupt.h>
#include <linux/soundcard.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include "cwda15.h"
#include "cs75xx_spdif.h"
#include "cs75xx_spdif_diagdata.h"
#ifdef SUPPORT_AC3
#include "diag_nonpcm_ac3.h"
#endif

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
	/* SPDIF level */
	char name[16];
	cs_reg ssp_irq_status;

	/* DMA level */
	int (*dma_hander)(int, int);
	u16 dam_tbuf_num;
	u16 dma_tbuf_size;
	dma_addr_t dma_tdesc_paddr;
	cs75xx_dma_desc_t *dma_tdesc_vaddr;
	cs_reg dma_irq_tx_status;
	cs_reg dma_irq_rx_status;

	/* Device level */
	u16 tx_buf_num;
	u16 tx_buf_size;
	dma_addr_t tx_buf_paddr;
	u8 *tx_buf_vaddr;
	u8 *tx_buf_curr;
	u8 *tx_buf_end;
	u32 tx_gap;

	u32 file_len;
	u32 wait_write_len;
	u32 out_len;

	spinlock_t mutex;
	unsigned long flags;
	u32 sample_rate;
	u8 dma_mode;	/* 0: data register, 1: dma */
	u8 inter_clock;	/* 0: external clock, 1: internal clock */
	u8 preamble;	/* 0: by SW, 1: by HW */
	u8 tx_en;
	u8 tx_data_size;
	u8 clk_gen;	/* generate clock to dac */

	/* PA and PB are constant */
	u16 nonpcm_pc;
	u16 nonpcm_pd;
	u16 frame_size;

	spdif_audio_type_t audio_type;
} cs75xx_spdif_dma_ctrl_t;

struct cs75xx_spdif {
	struct device	*dev;
	void __iomem	*global_base;
	void __iomem	*ssp_base;
	void __iomem	*dma_base;
	void __iomem	*spdif_base;
	/* DMA SSP INT dispatcher */
	int		irq_tx;
	int		irq_spdif;
	cs75xx_spdif_dma_ctrl_t ctrl;

	CWda15_Config_Type CWda15_Peripheral;
};

static struct platform_device *cs75xx_spdif_dev;
static int audio_dev_id = -1;

#define cs75xx_global_read_reg(offset)		(readl(spdif->global_base+offset))
#define cs75xx_global_write_reg(offset, val)	(writel(val, spdif->global_base+offset))
#define cs75xx_ssp_read_reg(offset)		(readl(spdif->ssp_base+offset))
#define cs75xx_ssp_write_reg(offset, val)	(writel(val, spdif->ssp_base+offset))
#define cs75xx_dma_read_reg(offset)		(readl(spdif->dma_base+offset))
#define cs75xx_dma_write_reg(offset, val)	(writel(val, spdif->dma_base+offset))



/* SSP Register Map */
#define CS752X_SSP_ID				0x00
#define CS752X_SSP_CTRL0			0x04
#define CS752X_SSP_FRAME_CTRL0			0x0C
#define CS752X_SSP_INTERRUPT			0x20
#define CS752X_SSP_INTENABLE			0x24


#define SSP_DEV_ID(index)       ((cs75xx_ssp_read_reg(CS752X_SSP_ID) & 0x00FFFF00) >> 8)
#define SSP_REV_ID(index)       (cs75xx_ssp_read_reg(CS752X_SSP_ID) & 0x000000FF)

/* DMA Register Map */
#define CS75XX_DMA_SSP_RX_CTRL			0x00
#define CS75XX_DMA_SSP_TX_CTRL			0x04
#define CS75XX_DMA_SSP_RXQ5_CTRL		0x08
#define CS75XX_DMA_SSP_RXQ6_CTRL		0x0C
#define CS75XX_DMA_SSP_RXQ7_CTRL		0x10
#define CS75XX_DMA_SSP_TXQ5_CTRL		0x14
#define CS75XX_DMA_SSP_TXQ6_CTRL		0x18
#define CS75XX_DMA_SSP_TXQ7_CTRL		0x1C
#define CS75XX_DMA_SSP_RXQ5_PKTCNT_READ		0x20
#define CS75XX_DMA_SSP_RXQ6_PKTCNT_READ		0x24
#define CS75XX_DMA_SSP_RXQ7_PKTCNT_READ		0x28
#define CS75XX_DMA_SSP_TXQ5_PKTCNT_READ		0x2C
#define CS75XX_DMA_SSP_TXQ6_PKTCNT_READ		0x30
#define CS75XX_DMA_SSP_TXQ7_PKTCNT_READ		0x34
#define CS75XX_DMA_SSP_RXQ5_BASE_DEPTH		0x38
#define CS75XX_DMA_SSP_RXQ6_BASE_DEPTH		0x3C
#define CS75XX_DMA_SSP_RXQ7_BASE_DEPTH		0x40
#define CS75XX_DMA_SSP_RXQ5_WPTR		0x44
#define CS75XX_DMA_SSP_RXQ5_RPTR		0x48
#define CS75XX_DMA_SSP_RXQ6_WPTR		0x4C
#define CS75XX_DMA_SSP_RXQ6_RPTR		0x50
#define CS75XX_DMA_SSP_RXQ7_WPTR		0x54
#define CS75XX_DMA_SSP_RXQ7_RPTR		0x58
#define CS75XX_DMA_SSP_TXQ5_BASE_DEPTH		0x5C
#define CS75XX_DMA_SSP_TXQ6_BASE_DEPTH		0x60
#define CS75XX_DMA_SSP_TXQ7_BASE_DEPTH		0x64
#define CS75XX_DMA_SSP_TXQ5_WPTR		0x68
#define CS75XX_DMA_SSP_TXQ5_RPTR		0x6C
#define CS75XX_DMA_SSP_TXQ6_WPTR		0x70
#define CS75XX_DMA_SSP_TXQ6_RPTR		0x74
#define CS75XX_DMA_SSP_TXQ7_WPTR		0x78
#define CS75XX_DMA_SSP_TXQ7_RPTR		0x7C
#define CS75XX_DMA_SSP_RXQ5_FULL_THRESHOLD	0x80
#define CS75XX_DMA_SSP_RXQ6_FULL_THRESHOLD	0x84
#define CS75XX_DMA_SSP_RXQ7_FULL_THRESHOLD	0x88
#define CS75XX_DMA_SSP_RXQ5_PKTCNT		0x8C
#define CS75XX_DMA_SSP_RXQ6_PKTCNT		0x90
#define CS75XX_DMA_SSP_RXQ7_PKTCNT		0x94
#define CS75XX_DMA_SSP_RXQ5_FULL_DROP_PKTCNT	0x98
#define CS75XX_DMA_SSP_RXQ6_FULL_DROP_PKTCNT	0x9C
#define CS75XX_DMA_SSP_RXQ7_FULL_DROP_PKTCNT	0xA0
#define CS75XX_DMA_SSP_TXQ5_PKTCNT		0xA4
#define CS75XX_DMA_SSP_TXQ6_PKTCNT		0xA8
#define CS75XX_DMA_SSP_TXQ7_PKTCNT		0xAC
#define CS75XX_DMA_SSP_INTERRUPT_0		0xB0
#define CS75XX_DMA_SSP_INTENABLE_0		0xB4
#define CS75XX_DMA_SSP_INTERRUPT_1		0xB8
#define CS75XX_DMA_SSP_INTENABLE_1		0xBC
#define CS75XX_DMA_SSP_DESC_INTERRUPT		0xC0
#define CS75XX_DMA_SSP_DESC_INTENABLE		0xC4
#define CS75XX_DMA_SSP_RXQ5_INTERRUPT		0xC8
#define CS75XX_DMA_SSP_RXQ5_INTENABLE		0xCC
#define CS75XX_DMA_SSP_RXQ6_INTERRUPT		0xD0
#define CS75XX_DMA_SSP_RXQ6_INTENABLE		0xD4
#define CS75XX_DMA_SSP_RXQ7_INTERRUPT		0xD8
#define CS75XX_DMA_SSP_RXQ7_INTENABLE		0xDC
#define CS75XX_DMA_SSP_TXQ5_INTERRUPT		0xE0
#define CS75XX_DMA_SSP_TXQ5_INTENABLE		0xE4
#define CS75XX_DMA_SSP_TXQ6_INTERRUPT		0xE8
#define CS75XX_DMA_SSP_TXQ6_INTENABLE		0xEC
#define CS75XX_DMA_SSP_TXQ7_INTERRUPT		0xF0
#define CS75XX_DMA_SSP_TXQ7_INTENABLE		0xF4


#define CS75XX_DMA_SSP_DEPTH_MIN		3
#define CS75XX_DMA_SSP_DEPTH_MAX		13
#define CS752X_DMA_WPTR_RPTR_MASK		0x00001FFF


#define BLOKING_VERSION

#define INIT_BUF_GAP	(DEF_BUF_NUM/8)
#define HIGH_BUF_GAP	(DEF_BUF_NUM*3/4)
#define LOW_BUF_GAP	(DEF_BUF_NUM/4)

void cs75xx_dump_owner(void);
void cs75xx_dump_size(void);
cs_status cs75xx_dma_spdif_bypass(int bypass);
cs_status cs75xx_dma_spdif_tx_update(u16 dma_wt_ptr);
cs_status cs75xx_dma_spdif_tx_ptr(u16 *wt_ptr_p, u16 *rd_ptr_p);
int cs75xx_spdif_enable(void);
int cs75xx_spdif_disable(void);
static void spdif_dma_write(u32 data);

static DECLARE_WAIT_QUEUE_HEAD(spdif_wait_q);
static int spdif_wait_en = 0;
static unsigned int debug = 0;
#ifdef CONFIG_DAC_REF_INTERNAL_CLK
static unsigned int clock = 1;
#else
static unsigned int clock = 0;
#endif
static unsigned int delay = 0;

/*
void CWda15_Write_Audio_Sample(unsigned int baseaddress, unsigned int sample){
    int tmp = 1;

    while (tmp){
        tmp = CWda15_READ_REG(baseaddress, CWda15_FIFO_LEVEL);
        tmp = tmp & CWda15_FIFO_FULL;
		mdelay(1);
    }
    CWda15_WRITE_REG(baseaddress, CWda15_WRITE_DATA_OFFSET, sample);
}
*/

/*******************************************************************************
 * DIAGNOSIS
 ******************************************************************************/
/* output: right - 1KHz sine wave, left - 2KHz sine wave */
int cs75xx_spdif_diag_pcm(spdif_diag_cmd_t diag_cmd)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	u16 dma_wt_ptr, tx_diff_ptr;
	int i, rc = 0;
#ifndef BLOKING_VERSION
	int j, tmp;
	int cwda15_fifo_lenght = 0;
#endif
	u16 tx_write_ptr, tx_read_ptr;
	unsigned long end_time;
	DECLARE_WAITQUEUE(wait, current);

	if (diag_cmd.data == 0) {
		CWda15_StopData(&spdif->CWda15_Peripheral);
		CWda15_StopSpdif(&spdif->CWda15_Peripheral);
		cs75xx_spdif_disable();
		spdif->ctrl.tx_en = 0;
		return 0;
	}

	printk("\r\n");
	printk("\r\n");
	printk("*******************************************************************************\r\n");
	printk("CWda15 IP Core Diagnostic PCM Example Program:\r\n");
	printk("*******************************************************************************\r\n");
	printk("\r\n");

	// Configuration Example of physical addresses
	spdif->CWda15_Peripheral.DataBaseAddress      = (unsigned int)spdif->spdif_base;//0x00C0A000;
	spdif->CWda15_Peripheral.RegistersBaseAddress = (unsigned int)spdif->spdif_base;//0x00C0A000;
	spdif->CWda15_Peripheral.PcmMode              = TRUE;
	spdif->CWda15_Peripheral.SysClockFrequency    = SYS_REF_CLOCK;
	if (spdif->ctrl.inter_clock)
		spdif->CWda15_Peripheral.SampleFrequency  = 48000;
	else
		spdif->CWda15_Peripheral.SampleFrequency  = 0;


	if (CWda15_Identify(&spdif->CWda15_Peripheral)){
		printk("CWda15 IP Core found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
	}
	else{
		printk("CWda15 IP Core not found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
		return -ENXIO;
	}
	CWda15_Configure(&spdif->CWda15_Peripheral);
	CWda15_Configure_Buffers(&spdif->CWda15_Peripheral);

#ifndef BLOKING_VERSION
	cwda15_fifo_lenght = CWda15_READ_REG(CWda15_Peripheral.RegistersBaseAddress, CWda15_FIFO_SIZE);
	printk("CWda15 IP Core fifo lengh	   : 0x%08x\r\n", cwda15_fifo_lenght);
#endif

	add_wait_queue(&spdif_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	// Write some data words before starting sending data
	if (spdif->ctrl.dma_mode == 0) {
		for (i = 0; i < 48; i++) {
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, ((audio_samples_1khz[i] + 0x007FFFFF)/2) | CHANNEL_RIGHT);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, ((audio_samples_2khz[i] + 0x007FFFFF)/2) | CHANNEL_LEFT);
		}
	}
	else {
		for (i = 0; i < (SINAUDIOSAMPLES*2); i++) {
			if (spdif->ctrl.preamble) {
				spdif_dma_write((test_samples_1khz[i] + 0x007FFFFF)/2);
				spdif_dma_write((test_samples_2khz[i] + 0x007FFFFF)/2);
			}
			else {
				spdif_dma_write(((test_samples_1khz[i] + 0x007FFFFF)/2) | CHANNEL_RIGHT);
				spdif_dma_write(((test_samples_2khz[i] + 0x007FFFFF)/2) | CHANNEL_LEFT);
			}
		}
	}

	if (spdif->ctrl.dma_mode == 0) {
		spdif->ctrl.tx_data_size = 24;
		cs75xx_spdif_enable();
		CWda15_StartSpdif(&spdif->CWda15_Peripheral);
		CWda15_StartData(&spdif->CWda15_Peripheral);
	}

	i = 0;
	end_time = jiffies + diag_cmd.data*HZ;
	while ((long)jiffies - (long)end_time < 0) { //while(1) {
#ifdef BLOKING_VERSION
		// If the fifo is not full, write the audio sample to fifo
		if (spdif->ctrl.dma_mode == 0) {
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, ((audio_samples_1khz[i] + 0x007FFFFF)/2) | CHANNEL_RIGHT);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, ((audio_samples_2khz[i] + 0x007FFFFF)/2) | CHANNEL_LEFT);
		}
		else {
			if (spdif->ctrl.preamble) {
				spdif_dma_write((test_samples_1khz[i] + 0x007FFFFF)/2);
				spdif_dma_write((test_samples_2khz[i] + 0x007FFFFF)/2);
			}
			else {
				spdif_dma_write(((test_samples_1khz[i] + 0x007FFFFF)/2) | CHANNEL_RIGHT);
				spdif_dma_write(((test_samples_2khz[i] + 0x007FFFFF)/2) | CHANNEL_LEFT);
			}
		}
		// Similar with (i % SINAUDIOSAMPLES), but this way there is no integer division
		if (spdif->ctrl.dma_mode == 0) {
			if (i >= SINAUDIOSAMPLES-1)
				i = 0;
			else
				i++;
		}
		else {
			if (i >= (SINAUDIOSAMPLES*2-1)) {
				i = 0;
				cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);
				dma_wt_ptr = (spdif->ctrl.tx_buf_curr - spdif->ctrl.tx_buf_vaddr)/spdif->ctrl.tx_buf_size;

				if (spdif->ctrl.tx_en == 0) {
					if (dma_wt_ptr >= INIT_BUF_GAP) {

						spdif->ctrl.tx_data_size = 24;
						cs75xx_spdif_enable();
						CWda15_StartSpdif(&spdif->CWda15_Peripheral);
						CWda15_StartData(&spdif->CWda15_Peripheral);
						spdif->ctrl.tx_en = 1;
					}
				}
				else {
					if (dma_wt_ptr >= tx_read_ptr)
						tx_diff_ptr = dma_wt_ptr - tx_read_ptr;
					else
						tx_diff_ptr = dma_wt_ptr + spdif->ctrl.tx_buf_num - tx_read_ptr;

					if (tx_diff_ptr > HIGH_BUF_GAP)
						spdif->ctrl.tx_gap = 0;
					else
						spdif->ctrl.tx_gap = HIGH_BUF_GAP - tx_diff_ptr;

					if (spdif_wait_en == 0 && (tx_diff_ptr > HIGH_BUF_GAP)) {
						spdif_wait_en = 1;
						if (0 == interruptible_sleep_on_timeout(&spdif_wait_q, (5*HZ))) {
							printk("SPDIF DMA Tx Timeout!\n");
							spdif_wait_en = 0;
							rc = -EIO;
							goto END;
						}
					}
				}

				cs75xx_dma_spdif_tx_update(dma_wt_ptr);
			} else
				i++;
		}
#else
		// If the current number of audio samples in the fifo is higher that 75% of the fifo size then do not write anything
		// and the user can run something else, otherwise write about 25% fifo size audio samples in the fifo
		tmp = (CWda15_READ_REG(CWda15_Peripheral.RegistersBaseAddress, CWda15_FIFO_LEVEL));
		if (CWda15_FIFO_LEVEL < cwda15_fifo_lenght*3/4) {
			tmp = cwda15_fifo_lenght - CWda15_FIFO_LEVEL - 1;
			for (j = 0; j < tmp; j += 2) {
				CWda15_Write_Audio_Sample(CWda15_Peripheral.DataBaseAddress, audio_samples_1khz[i] | CHANNEL_RIGHT);
				CWda15_Write_Audio_Sample(CWda15_Peripheral.DataBaseAddress, audio_samples_2khz[i] | CHANNEL_LEFT);

				// similar with (i % SINAUDIOSAMPLES), but this way there is no integer division
				if (i >= SINAUDIOSAMPLES-1) {
					i = 0;
				} else {
					i++;
				}
			}
		}

		/*
		 * Do something else here
		 */

#endif	/* BLOKING_VERSION*/
	}

END:
	CWda15_StopData(&spdif->CWda15_Peripheral);
	CWda15_StopSpdif(&spdif->CWda15_Peripheral);
	cs75xx_spdif_disable();
	spdif->ctrl.tx_en = 0;

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&spdif_wait_q, &wait);

	return rc;
}

int cs75xx_spdif_diag_nonpcm(spdif_diag_cmd_t diag_cmd)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	u16 dma_wt_ptr, tx_diff_ptr;
	int i, j, rc = 0;
	int PA, PB, PC, PD;
	unsigned long burtlength, end_time;
	DECLARE_WAITQUEUE(wait, current);
	u16 tx_write_ptr, tx_read_ptr;

	if (diag_cmd.data == 0) {
		CWda15_StopData(&spdif->CWda15_Peripheral);
		CWda15_StopSpdif(&spdif->CWda15_Peripheral);
		cs75xx_spdif_disable();
		spdif->ctrl.tx_en = 0;
		return 0;
	}

	printk("\r\n");
	printk("\r\n");
	printk("*******************************************************************************\r\n");
	printk("CWda15 IP Core Diagnostic non-PCM Example Program:\r\n");
	printk("*******************************************************************************\r\n");
	printk("\r\n");

	// Configuration Example of physical addresses
	spdif->CWda15_Peripheral.DataBaseAddress	= (unsigned int)spdif->spdif_base;//0x00C0A000;
	spdif->CWda15_Peripheral.RegistersBaseAddress	= (unsigned int)spdif->spdif_base;//0x00C0A000;
	spdif->CWda15_Peripheral.burstLength		= 64;
	spdif->CWda15_Peripheral.framePeriod		= 1024;
	spdif->CWda15_Peripheral.pausePeriod		= 32;
	spdif->CWda15_Peripheral.PcmMode		= FALSE;
	spdif->CWda15_Peripheral.SysClockFrequency      = SYS_REF_CLOCK;
	if (spdif->ctrl.inter_clock)
		spdif->CWda15_Peripheral.SampleFrequency = 48000;
	else
		spdif->CWda15_Peripheral.SampleFrequency = 0;


	if (CWda15_Identify(&spdif->CWda15_Peripheral)) {
		printk("CWda15 IP Core found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
	}
	else{
		printk("CWda15 IP Core not found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
		return -ENXIO;
	}
	CWda15_Configure(&spdif->CWda15_Peripheral);
	CWda15_Configure_Buffers(&spdif->CWda15_Peripheral);

#ifndef BLOKING_VERSION
	cwda15_fifo_lenght = CWda15_READ_REG(CWda15_Peripheral.RegistersBaseAddress, CWda15_FIFO_SIZE);
	printk("CWda15 IP Core fifo lengh		 : 0x%08x\r\n", cwda15_fifo_lenght);
#endif

	add_wait_queue(&spdif_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	// Write some data words before starting sending data (in this case simply null samples)
	for (i = 0; i< 48 ; i++) {
		if (spdif->ctrl.dma_mode == 0) {
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, CHANNEL_RIGHT);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, CHANNEL_LEFT);
		}
		else {
			spdif_dma_write(CHANNEL_RIGHT);
			spdif_dma_write(CHANNEL_LEFT);
		}
	}
	if (spdif->ctrl.dma_mode == 0) {
		spdif->ctrl.tx_data_size = 24;
		cs75xx_spdif_enable();
		CWda15_StartSpdif(&spdif->CWda15_Peripheral);
		CWda15_StartData(&spdif->CWda15_Peripheral);
	}

	i = 0;
	j = 0;
	burtlength = spdif->CWda15_Peripheral.burstLength;

	end_time = jiffies + diag_cmd.data*HZ;
	while ((long)jiffies - (long)end_time < 0) { //while(1) {

		// Each loop writes a burst of burtlength words to the CWda15 ip core that depending on
		// which type on non pcm data the user is sending, will correspond (or not) to one
		// compressed audio frame; in this case:
		//	 burtlength 			 = 64	nonpcm words or subframes
		//	 repetition period		 = 1024 frames
		//	 pause repetition period = 32 frames;

		PA = 0x00F87200 | CHANNEL_RIGHT;  // sync word A
		PB = 0x004E1F00 | CHANNEL_LEFT;   // sync word B
		PC = 0x00000100 | CHANNEL_RIGHT;  // burst info dependent
		PD = 0x00040000 | CHANNEL_LEFT;   // burtlength in bits ( 64*16 << 8)

		if (spdif->ctrl.dma_mode == 0) {
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, PA);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, PB);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, PC);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, PD);
		}
		else {
			spdif_dma_write(PA);
			spdif_dma_write(PB);
			spdif_dma_write(PC);
			spdif_dma_write(PD);
		}

		for(i = 0; i < burtlength; i += 2) {
			if (spdif->ctrl.dma_mode == 0) {
				CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, nonpcmdata[j]	 | CHANNEL_RIGHT);
				CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, nonpcmdata[j+1] | CHANNEL_LEFT);
			}
			else {
				spdif_dma_write(nonpcmdata[j]	| CHANNEL_RIGHT);
				spdif_dma_write(nonpcmdata[j+1] | CHANNEL_LEFT);
			}

			if (spdif->ctrl.dma_mode) {
				if (j >= NONPCMBUFFERSIZE-1)
					j = 0;
				else
					j += 2;
			}
			else {
				if (i >= (SINAUDIOSAMPLES*2-1)) {
					i = 0;
					cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);
					dma_wt_ptr = (spdif->ctrl.tx_buf_curr-spdif->ctrl.tx_buf_vaddr)/spdif->ctrl.dma_tbuf_size;

					if (spdif->ctrl.tx_en == 0) {
						if (dma_wt_ptr >= INIT_BUF_GAP) {
							spdif->ctrl.tx_data_size = 24;
							cs75xx_spdif_enable();
							CWda15_StartSpdif(&spdif->CWda15_Peripheral);
							CWda15_StartData(&spdif->CWda15_Peripheral);
							spdif->ctrl.tx_en = 1;
						}
					}
					else {
						if (dma_wt_ptr >= tx_read_ptr)
							tx_diff_ptr = dma_wt_ptr - tx_read_ptr;
						else
							tx_diff_ptr = dma_wt_ptr + spdif->ctrl.dam_tbuf_num - tx_read_ptr;

						if (spdif_wait_en == 0 && (tx_diff_ptr > HIGH_BUF_GAP)) {
							spdif_wait_en = 1;
							if (0 == interruptible_sleep_on_timeout(&spdif_wait_q, (5*HZ))) {
								printk("SPDIF DMA Tx Timeout!\n");
								spdif_wait_en = 0;
								rc = -EIO;
								goto END;
							}
						}
					}

					cs75xx_dma_spdif_tx_update(dma_wt_ptr);
				} else
					j += 2;
			}
		}
	}

END:
	CWda15_StopData(&spdif->CWda15_Peripheral);
	CWda15_StopSpdif(&spdif->CWda15_Peripheral);
	cs75xx_spdif_disable();
	spdif->ctrl.tx_en = 0;

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&spdif_wait_q, &wait);

	return rc;
}

#ifdef SUPPORT_AC3
char fscod_str[][16] = {
	"48 kHz",
	"44.1 kHz",
	"32 kHz",
	"reserved"
};

int fscod_value[] = {
	48000,
	44100,
	32000,
	44100	/* reserved */
};

char bsmod_str[][64] = {
	"main audio service: complete main (CM)",
	"main audio service: music and effects (ME)",
	"associated service: visually impaired (VI)",
	"associated service: hearing impaired (HI)",
	"associated service: dialogue (D)",
	"associated service: commentary (C)",
	"associated service: emergency (E)",
	"others"
};

char acmod_str[][32] = {
	"1+1 (Ch1, Ch2)",
	"1/0 (C)",
	"2/0 (L, R)",
	"3/0 (L, C, R)",
	"2/1 (L, R, S)",
	"3/1 (L, C, R, S)",
	"2/2 (L, R, SL, SR)",
	"3/2 (L, C, R, SL, SR)"
};

char cmixlev_str[][16] = {
	"0.707 (-3.0 dB)",
	"0.595 (-4.5 dB)",
	"0.500 (-6.0 dB)",
	"reserved"
};

char surmixlev_str[][16] = {
	"0.707 (-3 dB)",
	"0.500 (-6 dB)",
	"0",
	"reserved"
};

char dsurmod_str[][32] = {
	"not indicated",
	"Not Dolby Surround encoded",
	"Dolby Surround encoded",
	"reserved"
};

const unsigned int ac3_frame_size_code_table[][3] = {
                       /* 48 kHz,    44.1 kHz,  32 kHz */
/* 0x00 - 32 kbps */   {  64,        69,        96   },
/* 0x01 - 32 kbps */   {  64,        70,        96   },
/* 0x02 - 40 kbps */   {  80,        87,        120  },
/* 0x03 - 40 kbps */   {  80,        88,        120  },
/* 0x04 - 48 kbps */   {  96,        104,       144  },
/* 0x05 - 48 kbps */   {  96,        105,       144  },
/* 0x06 - 56 kbps */   {  112,       121,       168  },
/* 0x07 - 56 kbps */   {  112,       122,       168  },
/* 0x08 - 64 kbps */   {  128,       139,       192  },
/* 0x09 - 64 kbps */   {  128,       140,       192  },
/* 0x0A - 80 kbps */   {  160,       174,       240  },
/* 0x0B - 80 kbps */   {  160,       175,       240  },
/* 0x0C - 96 kbps */   {  192,       208,       288  },
/* 0x0D - 96 kbps */   {  192,       209,       288  },
/* 0x0E - 112 kbps */  {  224,       243,       336  },
/* 0x0F - 112 kbps */  {  224,       244,       336  },
/* 0x10 - 128 kbps */  {  256,       278,       384  },
/* 0x11 - 128 kbps */  {  256,       279,       384  },
/* 0x12 - 160 kbps */  {  320,       348,       480  },
/* 0x13 - 160 kbps */  {  320,       349,       480  },
/* 0x14 - 192 kbps */  {  384,       417,       576  },
/* 0x15 - 192 kbps */  {  384,       418,       576  },
/* 0x16 - 224 kbps */  {  448,       487,       672  },
/* 0x17 - 224 kbps */  {  448,       488,       672  },
/* 0x18 - 256 kbps */  {  512,       557,       768  },
/* 0x19 - 256 kbps */  {  512,       558,       768  },
/* 0x1A - 320 kbps */  {  640,       696,       960  },
/* 0x1B - 320 kbps */  {  640,       697,       960  },
/* 0x1C - 384 kbps */  {  768,       835,       1152 },
/* 0x1D - 384 kbps */  {  768,       836,       1152 },
/* 0x1E - 448 kbps */  {  896,       975,       1344 },
/* 0x1F - 448 kbps */  {  896,       976,       1344 },
/* 0x20 - 512 kbps */  {  1024,      1114,      1536 },
/* 0x21 - 512 kbps */  {  1024,      1115,      1536 },
/* 0x22 - 576 kbps */  {  1152,      1253,      1728 },
/* 0x23 - 576 kbps */  {  1152,      1254,      1728 },
/* 0x24 - 640 kbps */  {  1280,      1393,      1920 },
/* 0x25 - 640 kbps */  {  1280,      1394,      1920 },
};

int cs75xx_spdif_diag_ac3(spdif_diag_cmd_t diag_cmd)	/* non-PCM */
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	u16 tx_write_ptr, tx_read_ptr, tx_diff_ptr;
	int i, j, framesize, rc = 0;
	u16 PA, PB, PC, PD;
	unsigned long end_time;
	DECLARE_WAITQUEUE(wait, current);
	ac3_file_info_t ac3_header = {
		.syncword   = 0x770B,
		.crc1       = 0xB518,
		.frmsizecod = 0x1E,
		.fscod      = 0,
		.bsmod      = 0,
		.bsid       = 0x8,
		.dsurmod    = 0x1,
		.surmixlev  = 0,
		.cmixlev    = 0x2,
		.acmod      = 0x7
	};

	printk("\r\n");
	printk("\r\n");
	printk("*******************************************************************************\r\n");
	printk("CWda15 IP Core Diagnostic AC3 Program:\r\n");
	printk("*******************************************************************************\r\n");
	printk("\r\n");

	// Configuration Example of physical addresses
	spdif->CWda15_Peripheral.DataBaseAddress	= (unsigned int)spdif->spdif_base;//0x00C0A000;
	spdif->CWda15_Peripheral.RegistersBaseAddress	= (unsigned int)spdif->spdif_base;//0x00C0A000;
	spdif->CWda15_Peripheral.framePeriod		= 1535;
	spdif->CWda15_Peripheral.pausePeriod		= 3;
	spdif->CWda15_Peripheral.PcmMode		= FALSE;
	spdif->CWda15_Peripheral.SysClockFrequency      = SYS_REF_CLOCK;
	if (spdif->ctrl.inter_clock)
		spdif->CWda15_Peripheral.SampleFrequency = 48000;
	else
		spdif->CWda15_Peripheral.SampleFrequency = 0;

	if (CWda15_Identify(&spdif->CWda15_Peripheral)) {
		printk("CWda15 IP Core found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
	}
	else{
		printk("CWda15 IP Core not found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
		return -ENXIO;
	}
	CWda15_Configure(&spdif->CWda15_Peripheral);
	CWda15_Configure_Buffers(&spdif->CWda15_Peripheral);

	add_wait_queue(&spdif_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

#if defined(AC3_RAW_HEX_BYTE)
	printk("CWda15 IP Core fifo lengh : 0x%08x\r\n", CWda15_READ_REG(spdif->CWda15_Peripheral.RegistersBaseAddress, CWda15_FIFO_SIZE));

	PA = 0xF872;	// sync word A
	PB = 0x4E1F;	// sync word B
	PC = ac3_header.bsmod << 8 | 1;
	PD = ac3_frame_size_code_table[ac3_header.frmsizecod][ac3_header.fscod] << 4;
	printk("PC = 0x%04X, PD = 0x%04X\n", PC, PD);

	spdif->ctrl.sample_rate = fscod_value[ac3_header.fscod];
	spdif->ctrl.tx_data_size = 16;	/* 1 word */
	framesize = ac3_frame_size_code_table[ac3_header.frmsizecod][ac3_header.fscod]*2;
	spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
	printk("sample_rate = %d, framesize = %d\n", spdif->ctrl.sample_rate, framesize);

	for (i = 0; i < sizeof(diag_nonpcm_ac3)/framesize; i++) {
		/* PA ~ PD */
		*(spdif->ctrl.tx_buf_curr++) = PA & 0x00FF;
		*(spdif->ctrl.tx_buf_curr++) = (PA & 0xFF00) >> 8;
		*(spdif->ctrl.tx_buf_curr++) = PB & 0x00FF;
		*(spdif->ctrl.tx_buf_curr++) = (PB & 0xFF00) >> 8;
		*(spdif->ctrl.tx_buf_curr++) = PC & 0x00FF;
		*(spdif->ctrl.tx_buf_curr++) = (PC & 0xFF00) >> 8;
		*(spdif->ctrl.tx_buf_curr++) = PD & 0x00FF;
		*(spdif->ctrl.tx_buf_curr++) = (PD & 0xFF00) >> 8;
		if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
			spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;

		/* AC3 Frame */
		for (j = i*framesize; j < (i+1)*framesize; j+=8) {
#if 0
			memcpy(spdif->ctrl.tx_buf_curr, &diag_nonpcm_ac3[j], 8);
			spdif->ctrl.tx_buf_curr += 8;
#else
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+1];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+3];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+2];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+5];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+4];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+7];
			*(spdif->ctrl.tx_buf_curr++) = diag_nonpcm_ac3[j+6];
#endif
			if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
				spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
		}

		/* Update DMA */
		cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);
		tx_write_ptr = (spdif->ctrl.tx_buf_curr - spdif->ctrl.tx_buf_vaddr)/spdif->ctrl.dma_tbuf_size;
		printk("w_p=%d\n", tx_write_ptr);

		if (spdif->ctrl.tx_en == 0) {
			if (tx_write_ptr >= INIT_BUF_GAP) {
				cs75xx_spdif_enable();
				CWda15_StartSpdif(&spdif->CWda15_Peripheral);
				CWda15_StartData(&spdif->CWda15_Peripheral);
				spdif->ctrl.tx_en = 1;
			}
		}
		else {
			if (tx_write_ptr >= tx_read_ptr)
				tx_diff_ptr = tx_write_ptr - tx_read_ptr;
			else
				tx_diff_ptr = tx_write_ptr + spdif->ctrl.dam_tbuf_num - tx_read_ptr;

			if (spdif_wait_en == 0 && (tx_diff_ptr > HIGH_BUF_GAP)) {
				spdif_wait_en = 1;
				if (0 == interruptible_sleep_on_timeout(&spdif_wait_q, (25*HZ))) {
					printk("SPDIF DMA Tx Timeout!\n");
					spdif_wait_en = 0;
					rc = -EIO;
					goto END;
				}
			}
		}

		cs75xx_dma_spdif_tx_update(tx_write_ptr);
	}
#elif defined(AC3_RAW_HEX_WORD)
	for (i = 0; i < 48; i++){
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, CHANNEL_RIGHT);
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, CHANNEL_LEFT);
	}

	spdif->ctrl.dma_mode = 0;
	spdif->ctrl.preamble = 0;
	cs75xx_spdif_enable();

	CWda15_StartClock(&spdif->CWda15_Peripheral, 48000, 0);
	CWda15_StartSpdif(&spdif->CWda15_Peripheral);
	CWda15_StartData(&spdif->CWda15_Peripheral);

	for (i = 0; i < sizeof(diag_nonpcm_ac3)/framesize; i++) {
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, 0x00F87200 | CHANNEL_RIGHT | 0x1000000);
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, 0x004E1F00 | CHANNEL_LEFT | 0x1000000);
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, 0x00000100 | CHANNEL_RIGHT | 0x1000000);
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, 0x00380000 | CHANNEL_LEFT | 0x1000000);

		/* AC3 Frame */
		for (j = i*framesize/2; j < (i+1)*framesize/2; j+=2) {
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, (diag_nonpcm_ac3[j] << 8) | CHANNEL_RIGHT | 0x1000000);
			CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, (diag_nonpcm_ac3[j+1] << 8) | CHANNEL_LEFT | 0x1000000);
		}
	}

#elif defined(AC3_HEX_WORD_WITH_PREAMBLE)
	for (i = 0; i < 4; i++) {
		//CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, CHANNEL_RIGHT);
		//CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, CHANNEL_LEFT);
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, diag_nonpcm_ac3[i]);
	}

	spdif->ctrl.dma_mode = 0;
	spdif->ctrl.preamble = 0;
	cs75xx_spdif_enable();

	CWda15_StartClock(&spdif->CWda15_Peripheral, 48000, 0);
	CWda15_StartSpdif(&spdif->CWda15_Peripheral);
	CWda15_StartData(&spdif->CWda15_Peripheral);

	for (i = 4; i < sizeof(diag_nonpcm_ac3)/4; i++)
		CWda15_Write_Audio_Sample(spdif->CWda15_Peripheral.DataBaseAddress, diag_nonpcm_ac3[i]);
#endif

END:
	{
		unsigned int dump_buf[24];

		CWda15_Dump_Registers(&spdif->CWda15_Peripheral, dump_buf);
		printk("REG - CWda15_DATA_REGISTER: 0x%08X\n", dump_buf[0]);
		printk("REG - CWda15_CONFIGURATION_REGISTER: 0x%08X\n", dump_buf[1]);
		printk("REG - CWda15_INTERRUPT_STATE_REGISTER: 0x%08X\n", dump_buf[2]);
		printk("REG - CWda15_IP_CORE_VERSION: 0x%08X\n", dump_buf[3]);
		printk("REG - CWda15_FIFO_LEVEL: 0x%08X\n", dump_buf[4]);
		printk("REG - CWda15_FIFO_SIZE: 0x%08X\n", dump_buf[5]);
		printk("REG - CWda15_FIFO_LOWER_LIMIT: 0x%08X\n", dump_buf[6]);
		printk("REG - CWda15_CLOCK_GENERATOR: 0x%08X\n", dump_buf[7]);

		CWda15_Dump_Buffers(&spdif->CWda15_Peripheral, dump_buf);
		printk("REG - CWda15_CHANNEL1_STATUS_BITS_BUFFER(0): 0x%08X\n", dump_buf[0]);
		printk("REG - CWda15_CHANNEL1_STATUS_BITS_BUFFER(1): 0x%08X\n", dump_buf[1]);
		printk("REG - CWda15_CHANNEL1_STATUS_BITS_BUFFER(2): 0x%08X\n", dump_buf[2]);
		printk("REG - CWda15_CHANNEL1_STATUS_BITS_BUFFER(3): 0x%08X\n", dump_buf[3]);
		printk("REG - CWda15_CHANNEL1_STATUS_BITS_BUFFER(4): 0x%08X\n", dump_buf[4]);
		printk("REG - CWda15_CHANNEL1_STATUS_BITS_BUFFER(5): 0x%08X\n", dump_buf[5]);


		printk("REG - CWda15_CHANNEL2_STATUS_BITS_BUFFER(0): 0x%08X\n", dump_buf[6]);
		printk("REG - CWda15_CHANNEL2_STATUS_BITS_BUFFER(1): 0x%08X\n", dump_buf[7]);
		printk("REG - CWda15_CHANNEL2_STATUS_BITS_BUFFER(2): 0x%08X\n", dump_buf[8]);
		printk("REG - CWda15_CHANNEL2_STATUS_BITS_BUFFER(3): 0x%08X\n", dump_buf[9]);
		printk("REG - CWda15_CHANNEL2_STATUS_BITS_BUFFER(4): 0x%08X\n", dump_buf[10]);
		printk("REG - CWda15_CHANNEL2_STATUS_BITS_BUFFER(5): 0x%08X\n", dump_buf[11]);

		printk("REG - CWda15_CHANNEL1_USER_BITS_BUFFER(0): 0x%08X\n", dump_buf[12]);
		printk("REG - CWda15_CHANNEL1_USER_BITS_BUFFER(1): 0x%08X\n", dump_buf[13]);
		printk("REG - CWda15_CHANNEL1_USER_BITS_BUFFER(2): 0x%08X\n", dump_buf[14]);
		printk("REG - CWda15_CHANNEL1_USER_BITS_BUFFER(3): 0x%08X\n", dump_buf[15]);
		printk("REG - CWda15_CHANNEL1_USER_BITS_BUFFER(4): 0x%08X\n", dump_buf[16]);
		printk("REG - CWda15_CHANNEL1_USER_BITS_BUFFER(5): 0x%08X\n", dump_buf[17]);

		printk("REG - CWda15_CHANNEL2_USER_BITS_BUFFER(0): 0x%08X\n", dump_buf[18]);
		printk("REG - CWda15_CHANNEL2_USER_BITS_BUFFER(1): 0x%08X\n", dump_buf[19]);
		printk("REG - CWda15_CHANNEL2_USER_BITS_BUFFER(2): 0x%08X\n", dump_buf[20]);
		printk("REG - CWda15_CHANNEL2_USER_BITS_BUFFER(3): 0x%08X\n", dump_buf[21]);
		printk("REG - CWda15_CHANNEL2_USER_BITS_BUFFER(4): 0x%08X\n", dump_buf[22]);
		printk("REG - CWda15_CHANNEL2_USER_BITS_BUFFER(5): 0x%08X\n", dump_buf[23]);
	}

	CWda15_StopData(&spdif->CWda15_Peripheral);
	CWda15_StopSpdif(&spdif->CWda15_Peripheral);
	cs75xx_spdif_disable();
	spdif->ctrl.tx_en = 0;

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&spdif_wait_q, &wait);

	return rc;
}
#endif


/*******************************************************************************
 * OSS
 ******************************************************************************/
static void spdif_dump_buf(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	int i;

	for (i = 0; i < 128; i++) {
		printk("%02X ", *(spdif->ctrl.tx_buf_vaddr+i));
		if ((i+1)%16 == 0)
			printk("\n");
	}
}

static void spdif_dump(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);

	printk("/*** base address and irq ***/\n");
	printk("ssp_base = 0x%p\n", spdif->ssp_base);
	printk("dma_base = 0x%p\n", spdif->dma_base);
	printk("spdif_base = 0x%p\n", spdif->spdif_base);
	printk("irq_spdif = %d\n", spdif->irq_spdif);

	printk("/*** ctrl dma ***/\n");
	printk("dam_tbuf_num = %d\n", spdif->ctrl.dam_tbuf_num);
	printk("dma_tbuf_size = %d\n", spdif->ctrl.dma_tbuf_size);
	printk("dma_tdesc_paddr = 0x%x\n", spdif->ctrl.dma_tdesc_paddr);
	printk("dma_irq_tx_status = 0x%x\n", spdif->ctrl.dma_irq_tx_status);
	printk("dma_irq_rx_status = 0x%x\n", spdif->ctrl.dma_irq_rx_status);

	printk("/*** ctrl device ***/\n");
	printk("tx_buf_num = %d\n", spdif->ctrl.tx_buf_num);
	printk("tx_buf_size = %d\n", spdif->ctrl.tx_buf_size);
	printk("tx_buf_paddr = 0x%x\n", spdif->ctrl.tx_buf_paddr);
	printk("tx_buf_vaddr = 0x%p\n", spdif->ctrl.tx_buf_vaddr);
	printk("tx_buf_curr = %p\n", spdif->ctrl.tx_buf_curr);
	printk("tx_buf_end = %p\n", spdif->ctrl.tx_buf_end);
	printk("tx dam owner\n");
	cs75xx_dump_owner();
	cs75xx_dump_size();

	printk("/*** ctrl user ***/\n");
	printk("file_len = 0x%x\n", spdif->ctrl.file_len);
	printk("wait_write_len = 0x%x\n", spdif->ctrl.wait_write_len);
	printk("out_len = %d\n", spdif->ctrl.out_len);

	printk("sample_rate = %d\n", spdif->ctrl.sample_rate);
	printk("dma_mode = %d\n", spdif->ctrl.dma_mode);
	printk("inter_clock = %d\n", spdif->ctrl.inter_clock);
	printk("preamble = %d\n", spdif->ctrl.preamble);
	printk("tx_en = %d\n", spdif->ctrl.tx_en);
	printk("tx_data_size = %d\n", spdif->ctrl.tx_data_size);
	printk("clk_gen = %d\n", spdif->ctrl.clk_gen);
}


static void spdif_dma_write(u32 data)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);

	memcpy(spdif->ctrl.tx_buf_curr, &data, 4);
	spdif->ctrl.tx_buf_curr += 4;
	if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
		spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
}


static void spdif_isr_handler(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	u16 tx_write_ptr, tx_read_ptr, tx_diff_ptr;

	cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);

	if (tx_write_ptr >= tx_read_ptr) {
		tx_diff_ptr = tx_write_ptr - tx_read_ptr;
	}
	else {
		tx_diff_ptr = tx_write_ptr + spdif->ctrl.dam_tbuf_num - tx_read_ptr;
	}
	if (spdif->ctrl.tx_gap)
		spdif->ctrl.tx_gap--;

	if (spdif_wait_en == 1 && (tx_diff_ptr <= LOW_BUF_GAP)) {
		spdif_wait_en = 0;
		wake_up_interruptible(&spdif_wait_q);
	}
}


static int spdif_dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	int retval, value;
	spdif_diag_cmd_t diag_cmd;
	DECLARE_WAITQUEUE(wait, current);

	int ival, new_format;
	int frag_size, frag_buf;
	struct audio_buf_info info;
	count_info inf;
	u16 tx_write_ptr, tx_read_ptr;

	switch (cmd) {
	/* proprietory test - start */
	case SPDIF_INIT_BUF:
		printk("SPDIF_INIT_BUF\n");
		//spdif->CWda15_Peripheral.PcmMode = TRUE;
		CWda15_Configure(&spdif->CWda15_Peripheral);
		CWda15_Configure_Buffers(&spdif->CWda15_Peripheral);
		spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
		spdif_wait_en = 0;
		break;

	case SPDIF_FILE_LEN:
		if (arg) {
			if (copy_from_user(&value, (int *) arg, sizeof(value)))
				retval = -EFAULT;
		}
		spdif->ctrl.file_len = value;
		spdif->ctrl.wait_write_len = value;
		spdif->ctrl.out_len = 0;
		printk("spdif.file_len : %d\n", spdif->ctrl.file_len);
		break;

	case SPDIF_STOP_PLAY:
		printk("SPDIF_STOP_PLAY\n");
		cs75xx_spdif_disable();
		CWda15_StopData(&spdif->CWda15_Peripheral);
		CWda15_StopSpdif(&spdif->CWda15_Peripheral);
		spdif->ctrl.tx_en = 0;
		spdif->ctrl.audio_type = AUDIO_UNDEF;
		break;

	case SPDIF_WAVE_FILEINFO:
		{
		wave_file_info_t fileInfo;

		if (arg) {
			if (copy_from_user(&fileInfo, (int *) arg, sizeof(fileInfo)))
				retval = -EFAULT;
		}

		if (debug) {
			printk("format = %d\n", fileInfo.format);
			printk("nChannels = %d\n", fileInfo.nChannels);
			printk("nSamplesPerSec = %d\n", fileInfo.nSamplesPerSec);
			printk("nSamplesPerSec = %d\n", fileInfo.wBitsPerSample);
			printk("nDataLen = %d\n", fileInfo.nDataLen);
		}

		if ((fileInfo.wBitsPerSample == 16) ||
			(fileInfo.wBitsPerSample == 24))
			spdif->ctrl.tx_data_size = fileInfo.wBitsPerSample;

		spdif->ctrl.sample_rate = fileInfo.nSamplesPerSec;
		spdif->CWda15_Peripheral.SysClockFrequency = SYS_REF_CLOCK;
		if (spdif->ctrl.inter_clock)
			spdif->CWda15_Peripheral.SampleFrequency = spdif->ctrl.sample_rate;
		else
			spdif->CWda15_Peripheral.SampleFrequency = 0;
		spdif->CWda15_Peripheral.PcmMode = TRUE;

		spdif->ctrl.audio_type = AUDIO_WAV;
		}
		break;

#ifdef SUPPORT_AC3
	case SPDIF_AC3_FILEINFO:
		{
		ac3_file_info_t fileInfo;

		if (arg) {
			if (copy_from_user(&fileInfo, (int *) arg, sizeof(fileInfo)))
				retval = -EFAULT;
		}

		if (debug) {
			printk("First AC-3 Header:\n");
			printk("\tsyncword = %02X%02X\n", (fileInfo.syncword & 0x00FF), (fileInfo.syncword >> 8));
			printk("\tCRC = %02X%02X\n", (fileInfo.crc1 & 0x00FF), (fileInfo.crc1 >> 8));
			printk("\tSampling frequency = 0x%X - %s\n", fileInfo.fscod, fscod_str[fileInfo.fscod]);
			printk("\tFrame Size Code = 0x%X\n", fileInfo.frmsizecod);
			printk("\tBit Stream Mode = 0x%X - %s\n", fileInfo.bsmod, bsmod_str[fileInfo.bsmod]);
			printk("\tBit Stream Identification = 0x%X\n", fileInfo.bsid);
			printk("\tDolby Surround Mode = 0x%X - %s\n", fileInfo.dsurmod, dsurmod_str[fileInfo.dsurmod]);
			printk("\tSurround Mix Level = 0x%X - %s\n", fileInfo.surmixlev, surmixlev_str[fileInfo.surmixlev]);
			printk("\tCenter Mix level = 0x%X - %s\n", fileInfo.cmixlev, cmixlev_str[fileInfo.cmixlev]);
			printk("\tAudio Coding Mode = 0x%X - %s\n", fileInfo.acmod, acmod_str[fileInfo.acmod]);
		}

		spdif->ctrl.tx_data_size = 16;
		spdif->ctrl.sample_rate = fscod_value[fileInfo.fscod];
		spdif->ctrl.frame_size = ac3_frame_size_code_table[fileInfo.frmsizecod][fileInfo.fscod] * 2;
		spdif->ctrl.nonpcm_pc = fileInfo.bsmod << 8 | 1;
		spdif->ctrl.nonpcm_pd = ac3_frame_size_code_table[fileInfo.frmsizecod][fileInfo.fscod] << 4;
		spdif->CWda15_Peripheral.framePeriod = 1535;
		spdif->CWda15_Peripheral.pausePeriod = 3;
		spdif->CWda15_Peripheral.PcmMode = FALSE;
		spdif->CWda15_Peripheral.SysClockFrequency = SYS_REF_CLOCK;
		if (spdif->ctrl.inter_clock)
			spdif->CWda15_Peripheral.SampleFrequency = spdif->ctrl.sample_rate;
		else
			spdif->CWda15_Peripheral.SampleFrequency = 0;
		printk("sample_rate = %d, frame_szie(word) = %d, pc = %04x, pd = %04x\n",
			spdif->ctrl.sample_rate, spdif->ctrl.frame_size/2, spdif->ctrl.nonpcm_pc, spdif->ctrl.nonpcm_pd);

		spdif->ctrl.audio_type = AUDIO_AC3;
		}
		break;
#endif

#ifdef SUPPORT_DTS
	case SPDIF_DTS_FILEINFO:
		{
		dts_file_info_t fileInfo;

		if (arg) {
			if (copy_from_user(&fileInfo, (int *) arg, sizeof(fileInfo)))
				retval = -EFAULT;
		}
		printk("samplerate = %d, framesize = %d, type = %d\n", fileInfo.samplerate, fileInfo.framesize, fileInfo.type);

		spdif->ctrl.tx_data_size = 16;
		spdif->ctrl.sample_rate = fileInfo.samplerate;
		spdif->ctrl.frame_size = fileInfo.framesize + 1;	// unit of byte
		spdif->ctrl.nonpcm_pc = fileInfo.type;
		spdif->ctrl.nonpcm_pd = (fileInfo.framesize + 1) << 3;
		if (fileInfo.type == 11)
			spdif->CWda15_Peripheral.framePeriod = 512 - 1;
		else if (fileInfo.type == 12)
			spdif->CWda15_Peripheral.framePeriod = 1024 - 1;
		else if (fileInfo.type == 13)
			spdif->CWda15_Peripheral.framePeriod = 2048 - 1;
		spdif->CWda15_Peripheral.pausePeriod = 3 - 1;
		spdif->CWda15_Peripheral.PcmMode = FALSE;
		spdif->CWda15_Peripheral.SysClockFrequency = SYS_REF_CLOCK;
		if (spdif->ctrl.inter_clock)
			spdif->CWda15_Peripheral.SampleFrequency = spdif->ctrl.sample_rate;
		else
			spdif->CWda15_Peripheral.SampleFrequency = 0;

		spdif->ctrl.audio_type = AUDIO_DTS;
		}
#endif

	case SPDIF_DIAG_CMD:
		if (arg) {
			if (copy_from_user(&diag_cmd, (int *)arg, sizeof(spdif_diag_cmd_t))) {
				retval = -EFAULT;
				break;
			}
			if (diag_cmd.cmd == 0 || diag_cmd.cmd == 1) {/* example */
				if (diag_cmd.data == 0) {
					cs75xx_spdif_disable();
					CWda15_StopData(&spdif->CWda15_Peripheral);
					CWda15_StopSpdif(&spdif->CWda15_Peripheral);
					spdif->ctrl.tx_en = 0;
					break;
				}
			}

			switch (diag_cmd.cmd) {
			case DIAG_PCM_DATA:		/* example_pcm of coreworks */
				cs75xx_spdif_diag_pcm(diag_cmd);
				break;

			case DIAG_NONPCM_DATA:	/* example_nonpcm of coreworks */
				cs75xx_spdif_diag_nonpcm(diag_cmd);
				break;

#ifdef SUPPORT_AC3
			case DIAG_NONPCM_AC3:	/* AC3 */
				cs75xx_spdif_diag_ac3(diag_cmd);
				break;
#endif

			case DIAG_DMA_MODE:
				if (diag_cmd.data) {
					spdif->ctrl.dma_mode = 1;
					cs75xx_dma_spdif_bypass(0);
				}
				else {
					spdif->ctrl.dma_mode = 0;
					cs75xx_dma_spdif_bypass(1);
				}
				break;

			case DIAG_INTER_CLOCK:
				if (diag_cmd.data)
					spdif->ctrl.inter_clock = 1;
				else {
					spdif->ctrl.inter_clock = 0;
					spdif->CWda15_Peripheral.SampleFrequency = 0;
					CWda15_Configure(&spdif->CWda15_Peripheral);
				}
				break;

			case DIAG_PREAMBLE_MODE: /* preamble clock */
				if (diag_cmd.data)
					spdif->ctrl.preamble = 1;
				else
					spdif->ctrl.preamble = 0;
				break;

			case DIAG_DUMP_INFO: /* preamble clock */
				spdif_dump();
				break;

			case DIAG_CLOCK_GEN:
				if (diag_cmd.data)
					spdif->ctrl.clk_gen = 1;
				else
					spdif->ctrl.clk_gen = 0;
				break;
			default:
				;
			}
		}
		break;
	/* proprietory test - end */
	case OSS_GETVERSION:
		printk("OSS_GETVERSION\n");
		return put_user(SOUND_VERSION, (int *) arg);

	case SNDCTL_DSP_GETCAPS:
		ival = DSP_CAP_BATCH;
		printk("SNDCTL_DSP_GETCAPS\n");
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETFMTS:
		printk("SNDCTL_DSP_GETFMTS\n");
		ival = (AFMT_S16_BE | AFMT_S16_LE);	//| AFMT_MU_LAW | AFMT_A_LAW ); //AFMT_S16_BE
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SETFMT:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("SNDCTL_DSP_SETFMT(%x)\n", ival);
		if (ival != AFMT_QUERY) {
			switch (ival) {
			case AFMT_S16_BE:
			case AFMT_S16_LE:
			case AFMT_U16_BE:
			case AFMT_U16_LE:
				spdif->ctrl.tx_data_size = 16;
				break;
			default:
				printk("unsupported sound format 0x%04x requested.\n", ival);
				ival = AFMT_S16_BE;
				return put_user(ival, (int *) arg);
			}
			//CWda15_Configure(&spdif->CWda15_Peripheral);
			//CWda15_Configure_Buffers(&spdif->CWda15_Peripheral);
			spdif->ctrl.tx_gap = INIT_BUF_GAP;
			spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
			spdif->CWda15_Peripheral.PcmMode = TRUE;
			spdif->ctrl.audio_type = AUDIO_WAV;

			memset(spdif->ctrl.tx_buf_vaddr, 0, spdif->ctrl.tx_buf_num * spdif->ctrl.tx_buf_size);
			//spin_lock_irqsave(&spdif->ctrl.mutex, spdif->ctrl.flags);
			spdif_wait_en =0;
			//spin_unlock_irqrestore(&spdif->ctrl.mutex, spdif->ctrl.flags);
			return 0;
		} else {
#if 0
			switch (ssp_i2s.data_format) {
			//case SSP_DF_8BIT_ULAW:	ival = AFMT_MU_LAW; break;
			//case SSP_DF_8BIT_ALAW:	ival = AFMT_A_LAW;  break;
			//case SSP_DF_16BIT_LINEAR:	ival = AFMT_U16_BE; break;//AFMT_S16_BE
			case SSP_DF_8BIT_LINEAR:
				ival = AFMT_U8;
			case SSP_DF_16BITB_LINEAR:
				ival = AFMT_S16_BE;
				break;	//AFMT_S16_BE
			case SSP_DF_16BITL_LINEAR:
				ival = AFMT_S16_LE;
				break;	//AFMT_S16_BE
			case SSP_DF_16BITUB_LINEAR:
				ival = AFMT_U16_BE;
				break;	//AFMT_S16_BE
			case SSP_DF_16BITUL_LINEAR:
				ival = AFMT_U16_LE;
				break;	//AFMT_S16_BE
			default:
				ival = 0;
			}
#endif
			return put_user(ival, (int *) arg);
		}

	case SOUND_PCM_READ_RATE:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("SOUND_PCM_READ_RATE(%d)\n", ival);
		//ssp_i2s.dac_rate = 44100;
		//ival = ssp_i2s.dac_rate;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SPEED:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("SNDCTL_DSP_SPEED(%d)\n", ival);
		if (ival < 32000) ival = 32000;
		if (ival > 96000) ival = 96000;
		spdif->ctrl.sample_rate = ival;
		spdif->CWda15_Peripheral.SysClockFrequency = SYS_REF_CLOCK;
		if (spdif->ctrl.inter_clock)
			spdif->CWda15_Peripheral.SampleFrequency = spdif->ctrl.sample_rate;
		else
			spdif->CWda15_Peripheral.SampleFrequency = 0;
		CWda15_Configure(&spdif->CWda15_Peripheral);
		CWda15_Configure_Buffers(&spdif->CWda15_Peripheral);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_STEREO:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("SNDCTL_DSP_STEREO(%d)\n", ival);
		if (ival != 0 && ival != 1)
			return -EINVAL;

		//ssp_i2s.stereo_select = ival;
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("SNDCTL_DSP_CHANNELS(%d)\n", ival);
		//if (ival != 1 && ival != 2) {
		//	ival = ssp_i2s.stereo_select == SSP_SS_MONO ? 1 : 2;
		//	return put_user(ival, (int *) arg);
		//}
		//ssp_i2s.stereo_select = ival - 1;
		return 0;

	case SOUND_PCM_READ_CHANNELS:
		printk("SOUND_PCM_READ_CHANNELS\n");
		//ival = ssp_i2s.stereo_select + 1;
		//return put_user(ival, (int *) arg);
		return 0;

	case SNDCTL_DSP_GETBLKSIZE:
		ival = spdif->ctrl.tx_buf_num;
		printk("SNDCTL_DSP_GETBLKSIZE(0x%x)\n", ival);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_NONBLOCK:
		printk("SNDCTL_DSP_NONBLOCK\n");
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_RESET:
		printk("SNDCTL_DSP_RESET\n");
		if (file->f_mode & FMODE_READ) {
			//audio_reset_buf(is);
		}
		cs75xx_spdif_disable();
		CWda15_StopData(&spdif->CWda15_Peripheral);
		CWda15_StopSpdif(&spdif->CWda15_Peripheral);
		spdif->ctrl.tx_en = 0;
		spdif->ctrl.audio_type = AUDIO_UNDEF;
		return 0;

	case SNDCTL_DSP_SETFRAGMENT:
		printk("SNDCTL_DSP_SETFRAGMENT\n");
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		frag_size = ival & 0xffff;
		frag_buf = (ival >> 16) & 0xffff;
		/* TODO: We use hardcoded fragment sizes and numbers for now */
		frag_size = 11; /* 4096 == 2^12 *///ssp_ctrl.buf_num
		frag_buf = spdif->ctrl.tx_buf_num;
		ival = (frag_buf << 16) + frag_size;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETODELAY:
		if (!(file->f_mode & FMODE_WRITE))
			return -EPERM;
		if (delay == 0) {
			u16 tx_write_ptr, tx_read_ptr;
			u32 data_rate = spdif->ctrl.sample_rate * (spdif->ctrl.tx_data_size / 8) * 2; /* Bytes/sec, 2 means 2 channels */
			u32 buf_data;

			cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);
			if (tx_write_ptr >= tx_read_ptr)
				buf_data = (tx_write_ptr - tx_read_ptr);
			else
				buf_data = (tx_write_ptr + spdif->ctrl.tx_buf_num - tx_read_ptr);

			/* FIXME */
			if (buf_data > 4)
				buf_data = 4;
			buf_data *= spdif->ctrl.tx_buf_size;

			/* ival with us unit? */
			ival = (buf_data * 1000) / (data_rate / 1000);
		}
		else
			ival = delay;

		//printk("SNDCTL_DSP_GETODELAY(%d)\n", ival);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETOSPACE:
		if (!(file->f_mode & FMODE_WRITE))
			return -EPERM;
		//spin_lock_irqsave(&ssp_lock, flags);
		info.fragstotal = spdif->ctrl.tx_buf_num;
		if (spdif->ctrl.tx_en == 0)
			info.fragments = spdif->ctrl.tx_gap;
		else
			info.fragments = HIGH_BUF_GAP - (spdif->ctrl.tx_gap + LOW_BUF_GAP);
		info.fragsize = spdif->ctrl.tx_buf_size;
		info.bytes = info.fragments * info.fragsize;
		//spin_unlock_irqrestore(&ssp_lock, flags);
		//printk("SNDCTL_DSP_GETOSPACE(%d)\n", info.fragments);
		return copy_to_user((void *) arg, &info, sizeof(info)) ? -EPERM: 0;

	case SNDCTL_DSP_GETISPACE:
		printk("SNDCTL_DSP_GETISPACE\n");
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		info.fragstotal = spdif->ctrl.tx_buf_num;
		info.fragments = 20;	// ssp_i2s.nb_filled_record; /*ssp_ctrl.buf_num-*/
		info.fragsize = spdif->ctrl.tx_buf_num;
		info.bytes = info.fragments * info.fragsize;
		return 0;	//copy_to_user((void *)arg, &info, sizeof(info)) ? -EFAULT : 0;

	case SNDCTL_DSP_SYNC:
		printk("SNDCTL_DSP_SYNC\n");
		spdif->ctrl.tx_en = cs75xx_spdif_disable();
		return 0;

	case SNDCTL_DSP_SETDUPLEX:
		printk("SNDCTL_DSP_SETDUPLEX\n");
		return 0;

	case SNDCTL_DSP_POST:
		printk("SNDCTL_DSP_POST\n");
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		printk("SNDCTL_DSP_GETTRIGGER\n");
		//PCM_ENABLE_INPUT
		//PCM_ENABLE_OUTPUT
		return 0;

	case SNDCTL_DSP_GETOPTR:
		printk("SNDCTL_DSP_GETOPTR\n");

		//int bytecount, offset, flags;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;

		inf.blocks = spdif->ctrl.tx_buf_num;
		inf.bytes = spdif->ctrl.tx_buf_size;

		return copy_to_user((void *) arg, &inf, sizeof(inf));

	case SNDCTL_DSP_GETIPTR:
		printk("SNDCTL_DSP_GETIPTR \n");
		//count_info inf = { 0, };

		//int bytecount, offset, flags;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;

		inf.blocks = spdif->ctrl.tx_buf_num;
		inf.bytes = spdif->ctrl.tx_buf_size;
		return copy_to_user((void *) arg, &inf, sizeof(inf));

	case SOUND_PCM_READ_BITS:
		printk("SOUND_PCM_READ_BITS\n");
		return 0;

	case SOUND_PCM_READ_FILTER:
		printk("SOUND_PCM_READ_FILTER\n");
		return 0;

	case SNDCTL_DSP_SUBDIVIDE:
		printk("SNDCTL_DSP_SUBDIVIDE\n");
		return 0;

	case SNDCTL_DSP_SETTRIGGER:
		printk("SNDCTL_DSP_SETTRIGGER\n");
		return 0;

	case SNDCTL_DSP_SETSYNCRO:
		printk("SNDCTL_DSP_SETSYNCRO\n");
		return 0;

	case SNDCTL_DSP_MAPINBUF:
		printk("SNDCTL_DSP_MAPINBUF\n");
		return 0;

	case SNDCTL_DSP_MAPOUTBUF:
		printk("SNDCTL_DSP_MAPOUTBUF\n");
		return 0;
	default:
		printk("Not support ioctl cmd %d!\n", cmd);
	}

	return -1;
}

static int spdif_dsp_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int spdif_dsp_release(struct inode *inode, struct file *file)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);


	if (spdif->ctrl.tx_en) {
		CWda15_StopData(&spdif->CWda15_Peripheral);
		CWda15_StopSpdif(&spdif->CWda15_Peripheral);
		cs75xx_spdif_disable();
		spdif->ctrl.tx_en = 0;
	}
	return 0;
}

static ssize_t spdif_dsp_write(struct file *file_p, const char __user * buf,
                                 size_t count, loff_t * ppos)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);

	u16 symbol_num, symbol_buf_size, dma_wt_ptr, tx_diff_ptr;
	int i, j, k = 0, round, offset, len = 0;
	static cs_uint8 tmp_buf[1920*2];
	int symbol_byte = spdif->ctrl.tx_data_size/8;
	DECLARE_WAITQUEUE(wait, current);
	u16 tx_write_ptr, tx_read_ptr;

	add_wait_queue(&spdif_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	if (spdif->ctrl.audio_type == AUDIO_WAV) {
		symbol_num = count/symbol_byte;
		if (symbol_byte == 2) {
			symbol_buf_size = spdif->ctrl.tx_buf_size/2;
		}
		else {
			symbol_buf_size = spdif->ctrl.tx_buf_size/4;

			if (copy_from_user(tmp_buf, buf, count)) {
				printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
			}
		}

		if ((spdif->ctrl.tx_data_size == 16) && (spdif->ctrl.preamble == 1)) {
			round = count / spdif->ctrl.tx_buf_size;
			offset = count % spdif->ctrl.tx_buf_size;

		} else {
			round = symbol_num / symbol_buf_size;
			offset = symbol_num % symbol_buf_size;
		}

		for (i = 0; i < round; i++) {
			if ((spdif->ctrl.tx_data_size == 16) && (spdif->ctrl.preamble == 1)) {
				if (copy_from_user(spdif->ctrl.tx_buf_curr, buf + spdif->ctrl.tx_buf_size * i, spdif->ctrl.tx_buf_size)) {
					printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
				}
				spdif->ctrl.tx_buf_curr += spdif->ctrl.tx_buf_size;
				if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
					spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
			}
			else {
				if (spdif->ctrl.preamble == 1) {	/* 24-bits + preamble */
					for (j = 0; j < symbol_buf_size; j += 2) {
						memcpy(spdif->ctrl.tx_buf_curr, &tmp_buf[k*symbol_byte], symbol_byte);
						k++;
						spdif->ctrl.tx_buf_curr += 4;	/* right channel */
						memcpy(spdif->ctrl.tx_buf_curr, &tmp_buf[k*symbol_byte], symbol_byte);
						k++;
						spdif->ctrl.tx_buf_curr += 4;	/* left channel */
						if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
							spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
					}
				}
				else {	/* 16-bits/24-bits non-preamble */
					u32 tmp_data;
					for (j = 0; j < symbol_buf_size; j += 2) {
						tmp_data = 0;
						if (copy_from_user(&tmp_data, buf+symbol_byte, symbol_byte)) {
							printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
						}
						if (spdif->ctrl.tx_data_size == 16)
							tmp_data <<= 8;
						spdif_dma_write(tmp_data | CHANNEL_RIGHT);	// right channel
						tmp_data = 0;
						if (copy_from_user(&tmp_data, buf+2*symbol_byte, symbol_byte)) {
							printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
						}
						if (spdif->ctrl.tx_data_size == 16)
							tmp_data <<= 8;
						spdif_dma_write(tmp_data | CHANNEL_LEFT);	// left channel
						buf += 2*symbol_byte;
					}
				}
			}

			dma_wt_ptr = (spdif->ctrl.tx_buf_curr - spdif->ctrl.tx_buf_vaddr)/spdif->ctrl.tx_buf_size;
			cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);

			if (spdif->ctrl.tx_en == 0) {
				if (dma_wt_ptr >= INIT_BUF_GAP) {
					if (debug) spdif_dump_buf();
					cs75xx_spdif_enable();
					CWda15_StartSpdif(&spdif->CWda15_Peripheral);
					CWda15_StartData(&spdif->CWda15_Peripheral);
					spdif->ctrl.tx_en = 1;
					spdif->ctrl.tx_gap = 0;
				}
			}
			else {
				if (dma_wt_ptr >= tx_read_ptr) {
					tx_diff_ptr = dma_wt_ptr - tx_read_ptr;
				}
				else {
					tx_diff_ptr = dma_wt_ptr + spdif->ctrl.tx_buf_num - tx_read_ptr;
				}
				if (tx_diff_ptr < LOW_BUF_GAP)
					spdif->ctrl.tx_gap = 0;
				else
					spdif->ctrl.tx_gap = tx_diff_ptr - LOW_BUF_GAP;

				if (spdif_wait_en == 0 && (tx_diff_ptr > HIGH_BUF_GAP)) {
					if (file_p->f_flags & O_NONBLOCK)
						return -EAGAIN;

					spdif_wait_en = 1;
					if (0 == interruptible_sleep_on_timeout(&spdif_wait_q, (5*HZ))) {
						printk("DMA Tx Timeout!\n");
						spdif_wait_en = 0;
						goto END;
					}
				}
			}

			cs75xx_dma_spdif_tx_update(dma_wt_ptr);
		}
	}	//	audio_type == AUDIO_WAV
#if defined(SUPPORT_AC3) || defined(SUPPORT_DTS)
	else if ((spdif->ctrl.audio_type == AUDIO_AC3) ||
			 (spdif->ctrl.audio_type == AUDIO_DTS)) {
		u16 tx_write_ptr, tx_read_ptr, tx_diff_ptr;
		u16 pa = 0xF872, pb = 0x4E1F;

		if (copy_from_user(tmp_buf, buf, count)) {
			printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
		}

		for (i = 0; i < spdif->ctrl.frame_size; i += 2) {
			if ((i % spdif->ctrl.frame_size) == 0) {
				*(u16 *)spdif->ctrl.tx_buf_curr = pa;
				spdif->ctrl.tx_buf_curr += 2;
				if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
					spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
				*(u16 *)spdif->ctrl.tx_buf_curr = pb;
				spdif->ctrl.tx_buf_curr += 2;
				if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
					spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
				*(u16 *)spdif->ctrl.tx_buf_curr = spdif->ctrl.nonpcm_pc;
				spdif->ctrl.tx_buf_curr += 2;
				if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
					spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
				*(u16 *)spdif->ctrl.tx_buf_curr = spdif->ctrl.nonpcm_pd;
				spdif->ctrl.tx_buf_curr += 2;
				if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
					spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
			}

			*(spdif->ctrl.tx_buf_curr++) = tmp_buf[i+1];
			*(spdif->ctrl.tx_buf_curr++) = tmp_buf[i];

			if (spdif->ctrl.tx_buf_curr >= spdif->ctrl.tx_buf_end)
				spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;

			if (((i + 2) % spdif->ctrl.frame_size) == 0) {
				cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);
				tx_write_ptr = (spdif->ctrl.tx_buf_curr - spdif->ctrl.tx_buf_vaddr)/spdif->ctrl.dma_tbuf_size;

				if (spdif->ctrl.tx_en == 0) {
					if (tx_write_ptr >= INIT_BUF_GAP) {
						cs75xx_spdif_enable();
						CWda15_StartSpdif(&spdif->CWda15_Peripheral);
						CWda15_StartData(&spdif->CWda15_Peripheral);
						spdif->ctrl.tx_en = 1;
					}
				}
				else {
					if (tx_write_ptr >= tx_read_ptr) {
						tx_diff_ptr = tx_write_ptr - tx_read_ptr;
					}
					else {
						tx_diff_ptr = tx_write_ptr + spdif->ctrl.dam_tbuf_num - tx_read_ptr;
					}

					if (spdif_wait_en == 0 && (tx_diff_ptr > HIGH_BUF_GAP)) {
						spdif_wait_en = 1;
						if (0 == interruptible_sleep_on_timeout(&spdif_wait_q, (5*HZ))) {
							printk("SPDIF DMA Tx Timeout!\n");
							spdif_wait_en = 0;
							goto END;
						}
					}
				}

				cs75xx_dma_spdif_tx_update(tx_write_ptr);
			}
		}
	}
#endif

	spdif->ctrl.wait_write_len -= count;
	len = count;

END:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&spdif_wait_q, &wait);

	return len;
}


static struct file_operations ssp_spdif_dsp_fops = {
	.owner          = THIS_MODULE,
	.open           = spdif_dsp_open,
	.release        = spdif_dsp_release,
	.write          = spdif_dsp_write,
	.unlocked_ioctl = spdif_dsp_ioctl,
};

int oss_driver_init(void)
{
	/* register devices */
	if ((audio_dev_id = register_sound_dsp(&ssp_spdif_dsp_fops, -1)) < 0) {
		printk("Func: %s - register_sound_dsp fail!\n", __func__);
		goto fail;
	}

	return 0;

fail:
	if (audio_dev_id != -1) {
		unregister_sound_dsp(audio_dev_id);
		audio_dev_id = -1;
	}

	return -EPERM;
}

void oss_driver_exit(void)
{
	unregister_sound_dsp(audio_dev_id);
}

/*******************************************************************************
 * DMA
 ******************************************************************************/
static int two_power(int value)
{
	int i;

	for (i = CS75XX_DMA_SSP_DEPTH_MIN; i <= CS75XX_DMA_SSP_DEPTH_MAX; i++)
		if (value == (0x0001 << i))
			return i;
	return 0;

}

void cs75xx_dump_owner(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	int i;

	for (i = 0; i < spdif->ctrl.dam_tbuf_num; i++) {
		printk("%d ", spdif->ctrl.dma_tdesc_vaddr[i].own);
		if ((i+1) % 16 == 0)
			printk("\n");
	}
}

void cs75xx_dump_size(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	int i;

	for (i = 0; i < spdif->ctrl.dam_tbuf_num; i++) {
		printk("%04d ", spdif->ctrl.dma_tdesc_vaddr[i].buf_size);
		if ((i+1) % 16 == 0)
			printk("\n");
	}
}

cs_status cs75xx_dma_spdif_bypass(int bypass)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;

	reg_txq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_CTRL);
	if (bypass)
		reg_txq6_ctrl.bf.txq6_en = 0;
	else
		reg_txq6_ctrl.bf.txq6_en = 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(CS752X_SSP_CTRL0);
	if (bypass)
		reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 64;
	else
		reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 32;
	cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	return CS_OK;
}

cs_status cs75xx_dma_spdif_tx_ptr(u16 *wt_ptr_p, u16 *rd_ptr_p)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	DMA_DMA_SSP_TXQ6_WPTR_t reg_txq6_wptr;
	DMA_DMA_SSP_TXQ6_RPTR_t reg_txq6_rptr;

	reg_txq6_wptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_WPTR);
	*wt_ptr_p = reg_txq6_wptr.bf.index;

	reg_txq6_rptr.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_RPTR);
	*rd_ptr_p = reg_txq6_rptr.bf.index;

	return CS_OK;
}

cs_status cs75xx_dma_spdif_tx_update(u16 tx_wt_ptr)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	u16 wt_ptr, rd_ptr;
	int change = 0;

	/* update buffer descriptor own bit */
	//spin_lock_irqsave(&spdif->ctrl.mutex, spdif->ctrl.flags);
	cs75xx_dma_spdif_tx_ptr(&wt_ptr, &rd_ptr);
	while (wt_ptr != tx_wt_ptr) {
		change = 1;

		dma_map_single(NULL, __va(spdif->ctrl.dma_tdesc_vaddr[wt_ptr].buf_addr), spdif->ctrl.dma_tbuf_size, DMA_TO_DEVICE);

		if ((spdif->ctrl.dma_tdesc_vaddr[wt_ptr].own == 0) && spdif->ctrl.tx_en) {
			printk("Can't Update Tx Write Pointer(dma_wt_ptr = %d, wptr = %d, own = 0, rptr = %d, len = 0x%x)\n",
			tx_wt_ptr, wt_ptr, rd_ptr, spdif->ctrl.wait_write_len);

			cs75xx_dump_owner();
		}

		spdif->ctrl.dma_tdesc_vaddr[wt_ptr].own = 0; // DMA
		spdif->ctrl.dma_tdesc_vaddr[wt_ptr].buf_size = spdif->ctrl.dma_tbuf_size;
		wt_ptr = (wt_ptr + 1) % spdif->ctrl.dam_tbuf_num;
	}
	//spin_unlock_irqrestore(&spdif->ctrl.mutex, spdif->ctrl.flags);

	if (change == 1)
		cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_WPTR, tx_wt_ptr);

	return CS_OK;
}

static cs_status cs75xx_dma_spdif_tx_enable(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
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

	return 0;
}

static cs_status cs75xx_dma_spdif_tx_disable(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;
	DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;
#ifdef DMA_FLUSH
	cs75xx_dma_desc_t *tmp_desc;
	int i;
#endif
	u16 tx_write_ptr, tx_read_ptr;

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

#ifdef DMA_FLUSH
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_WPTR, 0);

	/* reset DMA descriptor */
	for (i = 0; i < spdif->ctrl.dam_tbuf_num; i++) {
		tmp_desc = (cs75xx_dma_desc_t *)(spdif->ctrl.dma_tdesc_vaddr + i);
		tmp_desc->own = 1;	// CPU
		tmp_desc->misc = 0;
		tmp_desc->buf_size = spdif->ctrl.dma_tbuf_size;
	}

	/* update Tx write ptr and read ptr */
	cs75xx_dma_spdif_tx_ptr(&tx_write_ptr, &tx_read_ptr);
#endif

	return 0;
}

/* DMA SSP INT dispatcher */
static irqreturn_t cs75xx_dma_spdif_irq_handler(int irq, void *dev_instance)
{
	struct cs75xx_spdif *spdif = (struct cs75xx_spdif *)dev_instance;
	//DMA_DMA_SSP_DMA_SSP_INTENABLE_0_t reg_dma_ssp_inten;
	DMA_DMA_SSP_TXQ6_INTERRUPT_t reg_txq6_int;
	int tx = 0, index;
	enum irqreturn rc = IRQ_NONE;

	/* DMA Operation */
	reg_txq6_int.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTERRUPT);
	tx = reg_txq6_int.bf.txq6_eof;	// BIT(0)
	if (reg_txq6_int.bf.txq6_empty) {
		printk("TXQ6_EMPTY\n");
		spdif->ctrl.dma_irq_rx_status |= BIT(1);
	}
	if (reg_txq6_int.bf.txq6_overrun) {
		printk("TXQ6_OVERRUN\n");
		spdif->ctrl.dma_irq_rx_status |= BIT(2);
	}
	if (reg_txq6_int.bf.txq6_cntmsb) {
		printk("TXQ6_CNTMSB\n");
		spdif->ctrl.dma_irq_rx_status |= BIT(3);
	}
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTERRUPT, reg_txq6_int.wrd);

	spdif_isr_handler();

	rc = IRQ_HANDLED;

	return rc;
}


/*******************************************************************************
 * DMA/SPDIF
 ******************************************************************************/
int cs75xx_spdif_enable(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;

	dev_info(spdif->dev, "Func:%s\n", __func__);

	if (spdif->ctrl.dma_mode)
		cs75xx_dma_spdif_tx_enable();

	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(CS752X_SSP_CTRL0);
	if (spdif->ctrl.inter_clock)
		reg_ssp_ctrl0.wrd |= BIT(23);
	else
		reg_ssp_ctrl0.wrd &= ~BIT(23);

	if (spdif->ctrl.preamble) {
		reg_ssp_ctrl0.bf.spdif_preambleIns = 1;

		if (spdif->ctrl.tx_data_size == 16) {
		  reg_ssp_ctrl0.bf.spdif_byteShift = 1;
		  reg_ssp_ctrl0.bf.spdif_mode4WordEn = 1;
		  reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 127;
		}
	  else {
		  reg_ssp_ctrl0.bf.spdif_byteShift = 0;
		  reg_ssp_ctrl0.bf.spdif_mode4WordEn = 0;
		  reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 32;
	  }
	}
	else {
		reg_ssp_ctrl0.bf.spdif_preambleIns = 0;
		reg_ssp_ctrl0.bf.spdif_byteShift = 0;
		reg_ssp_ctrl0.bf.spdif_mode4WordEn = 0;
	}

	cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	reg_ssp_ctrl0.bf.spdif_enable = 1;
	cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	if (spdif->ctrl.clk_gen == 0) {
		reg_ssp_ctrl0.bf.s_enable = 1;
		cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);
	}

	return 0;
}

int cs75xx_spdif_disable(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;

	dev_info(spdif->dev, "Func:%s\n", __func__);

	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(CS752X_SSP_CTRL0);
	reg_ssp_ctrl0.bf.s_enable = 0;
	cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	reg_ssp_ctrl0.bf.spdif_enable = 0;
	cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);

	mdelay(10);

	cs75xx_dma_spdif_tx_disable();

	return 0;
}

int cs75xx_spdif_dam_cofnig(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	DMA_DMA_SSP_TXQ6_BASE_DEPTH_t reg_txq6_base;
	cs75xx_dma_desc_t *tmp_desc;
	int i, power;

	dev_info(spdif->dev, "Func:%s - SPDIF\n", __func__);

	/* Tx Buf */
	spdif->ctrl.tx_buf_vaddr = kzalloc(spdif->ctrl.tx_buf_num*spdif->ctrl.tx_buf_size, GFP_KERNEL|GFP_DMA);
	spdif->ctrl.tx_buf_paddr = __pa(spdif->ctrl.tx_buf_vaddr);
	if (spdif->ctrl.tx_buf_vaddr == NULL)
		goto fail;
	printk("tbuf vaddr = 0x%p, paddr = 0x%x\n", spdif->ctrl.tx_buf_vaddr, spdif->ctrl.tx_buf_paddr);
	spdif->ctrl.tx_buf_curr = spdif->ctrl.tx_buf_vaddr;
	spdif->ctrl.tx_buf_end = spdif->ctrl.tx_buf_vaddr + spdif->ctrl.tx_buf_num*spdif->ctrl.tx_buf_size;

	/* Tx Descriptor */
	spdif->ctrl.dma_tdesc_vaddr = dma_alloc_coherent(NULL, spdif->ctrl.dam_tbuf_num*sizeof(cs75xx_dma_desc_t),
	                                                &spdif->ctrl.dma_tdesc_paddr, GFP_KERNEL|GFP_DMA);
	if (spdif->ctrl.dma_tdesc_vaddr == NULL)
		goto fail;
	printk("tdesc vaddr = 0x%p, paddr = 0x%08X\n", spdif->ctrl.dma_tdesc_vaddr, spdif->ctrl.dma_tdesc_paddr);

	for (i = 0; i < spdif->ctrl.dam_tbuf_num; i++) {
		tmp_desc = (cs75xx_dma_desc_t *)(spdif->ctrl.dma_tdesc_vaddr + i);

		tmp_desc->own = 1;	// CPU
		tmp_desc->misc = 0;
		tmp_desc->buf_size = spdif->ctrl.dma_tbuf_size;
		tmp_desc->buf_addr = spdif->ctrl.tx_buf_paddr + i*spdif->ctrl.dma_tbuf_size;
	}

	power = two_power(spdif->ctrl.dam_tbuf_num);

	reg_txq6_base.wrd = 0;
	reg_txq6_base.bf.base =  spdif->ctrl.dma_tdesc_paddr >> 4;
	reg_txq6_base.bf.depth = power;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_BASE_DEPTH, reg_txq6_base.wrd);

	return 0;

fail:
	if (spdif->ctrl.tx_buf_vaddr)
		kfree(spdif->ctrl.tx_buf_vaddr);
	if (spdif->ctrl.dma_tdesc_vaddr)
		kfree(spdif->ctrl.dma_tdesc_vaddr);

	return -ENOMEM;
}


static cs_status cs75xx_spdif_dma_init(struct cs75xx_spdif *spdif)
{
	DMA_SSP_CTRL0_t reg_ssp_ctrl0;
	DMA_SSP_INT_ENABLE_t reg_ssp_inten;
	DMA_DMA_SSP_DESC_INTENABLE_t reg_desc_inten;
	DMA_DMA_SSP_TXDMA_CONTROL_t reg_dma_tx_ctrl;
	DMA_DMA_SSP_TXQ6_INTENABLE_t reg_txq6_inten;
	DMA_DMA_SSP_DMA_SSP_INTENABLE_0_t reg_dma_ssp_inten;

	dev_info(spdif->dev, "Func: %s\n", __func__);

	/* SPDIF level */
	spdif->ctrl.ssp_irq_status = 0;

	/* DMA level */
	spdif->ctrl.dma_hander = NULL;
	spdif->ctrl.dam_tbuf_num = DEF_BUF_NUM;
	spdif->ctrl.dma_tbuf_size = DEF_BUF_SIZE;
	spdif->ctrl.dma_tdesc_paddr = 0;
	spdif->ctrl.dma_tdesc_vaddr = NULL;
	spdif->ctrl.dma_irq_tx_status = 0;
	spdif->ctrl.dma_irq_rx_status = 0;

	/* Device level */
	spdif->ctrl.tx_buf_num = DEF_BUF_NUM;
	spdif->ctrl.tx_buf_size = DEF_BUF_SIZE;
	spdif->ctrl.tx_buf_paddr = 0;
	spdif->ctrl.tx_buf_vaddr = NULL;
	spdif->ctrl.tx_buf_curr = NULL;
	spdif->ctrl.tx_buf_end = NULL;

	spdif->ctrl.file_len = 0;
	spdif->ctrl.wait_write_len = 0;
	spdif->ctrl.out_len = 0;

	spdif->ctrl.mutex = SPIN_LOCK_UNLOCKED;
	spdif->ctrl.flags = 0;
	spdif->ctrl.sample_rate = 44100;	// 44.1K sample rate
	spdif->ctrl.dma_mode = 1;
	spdif->ctrl.inter_clock = 1;
	spdif->ctrl.preamble = 1;
	spdif->ctrl.tx_en = 0;
	spdif->ctrl.tx_data_size = 16;	// bit
	spdif->ctrl.clk_gen = clock ? 1 : 0;
	spdif->ctrl.audio_type = AUDIO_UNDEF;

	/* init SSP to SPDIF */
	reg_ssp_ctrl0.wrd = cs75xx_ssp_read_reg(CS752X_SSP_CTRL0);
	reg_ssp_ctrl0.bf.spdif_dma_wtMk  = 63;
	cs75xx_ssp_write_reg(CS752X_SSP_CTRL0, reg_ssp_ctrl0.wrd);

#ifndef CONFIG_DAC_REF_INTERNAL_CLK
	reg_ssp_inten.wrd = 0;
	reg_ssp_inten.wrd |= BIT(17);
	cs75xx_ssp_write_reg(CS752X_SSP_INTENABLE, reg_ssp_ctrl0.wrd);
#endif

	/* init DMA */
	reg_dma_tx_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TX_CTRL);
	reg_dma_tx_ctrl.bf.tx_dma_enable = 1;
	reg_dma_tx_ctrl.bf.tx_check_own = 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TX_CTRL, reg_dma_tx_ctrl.wrd);

	reg_desc_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_DESC_INTENABLE);
	reg_desc_inten.bf.tx_desc_err_en = 1;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_DESC_INTENABLE, reg_dma_tx_ctrl.wrd);

	reg_txq6_inten.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE);
	reg_txq6_inten.bf.txq6_eof_en = 0;
	reg_txq6_inten.bf.txq6_empty_en = 0;
	reg_txq6_inten.bf.txq6_overrun_en = 0;
	reg_txq6_inten.bf.txq6_cntmsb_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_INTENABLE, reg_txq6_inten.wrd);

	cs75xx_spdif_dam_cofnig();

	if (clock == 0) {
		/* DMA SSP INT dispatcher */
		if (request_irq(spdif->irq_tx, cs75xx_dma_spdif_irq_handler, IRQF_DISABLED, "cs75xx-dma-spdif", spdif)) {
			dev_err(spdif->dev, "ERROR: can't register IRQ for DMA of SPDIF\n");
			goto fail;
		}
	}

	return CS_OK;

fail:
	if (clock == 0)
		free_irq(spdif->irq_tx, spdif);

	return CS_ERROR;
}

static int cs75xx_spdif_dma_exit(struct cs75xx_spdif *spdif)
{
	struct platform_device *pdev = cs75xx_spdif_dev;
	DMA_DMA_SSP_TXQ6_CONTROL_t reg_txq6_ctrl;

	dev_info(&pdev->dev, "Func: %s\n", __func__);

	reg_txq6_ctrl.wrd = cs75xx_dma_read_reg(CS75XX_DMA_SSP_TXQ6_CTRL);
	reg_txq6_ctrl.bf.txq6_en = 0;
	cs75xx_dma_write_reg(CS75XX_DMA_SSP_TXQ6_CTRL, reg_txq6_ctrl.wrd);

	if (clock == 0) {
		free_irq(spdif->irq_tx, spdif);
	}

	return 0;
}

int cs75xx_spdif_clock_start(unsigned int sample_rate, int ext_out, unsigned int clk_target)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	GLOBAL_PIN_MUX_t reg_pin_mux;
	DMA_SSP_FRAME_CTRL0_t reg_frame_ctrl0;

	printk("%s-%s:%s\n", __DATE__, __TIME__, __func__);

	if (ext_out) {	/* output clock via gpio */
		reg_pin_mux.wrd = cs75xx_global_read_reg(0x18);
		reg_pin_mux.bf.pmux_frac_clk_en_gpio1_16 = 1;
		cs75xx_global_write_reg(0x18, reg_pin_mux.wrd);
	}

	reg_frame_ctrl0.wrd = cs75xx_ssp_read_reg(CS752X_SSP_FRAME_CTRL0);
	reg_frame_ctrl0.bf.mclkSel = 1;
	cs75xx_ssp_write_reg(CS752X_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

//	cs75xx_spdif_enable();
	if (CWda15_StartClock(&spdif->CWda15_Peripheral, sample_rate, clk_target))
		return -1;
//	CWda15_StartSpdif(&spdif->CWda15_Peripheral);
//	CWda15_StartData(&spdif->CWda15_Peripheral);

	return 0;
}
EXPORT_SYMBOL(cs75xx_spdif_clock_start);

void cs75xx_spdif_clock_stop(void)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(cs75xx_spdif_dev);
	GLOBAL_PIN_MUX_t reg_pin_mux;
	DMA_SSP_FRAME_CTRL0_t reg_frame_ctrl0;

	reg_pin_mux.wrd = cs75xx_global_read_reg(0x18);
	reg_pin_mux.bf.pmux_frac_clk_en_gpio1_16 = 0;
	cs75xx_global_write_reg(0x18, reg_pin_mux.wrd);

	reg_frame_ctrl0.wrd = cs75xx_ssp_read_reg(CS752X_SSP_FRAME_CTRL0);
	reg_frame_ctrl0.bf.mclkSel = 0;
	cs75xx_ssp_write_reg(CS752X_SSP_FRAME_CTRL0, reg_frame_ctrl0.wrd);

//	cs75xx_spdif_disable();
//	CWda15_StopData(&spdif->CWda15_Peripheral);
//	CWda15_StopSpdif(&spdif->CWda15_Peripheral);
}
EXPORT_SYMBOL(cs75xx_spdif_clock_stop);

static int __devinit cs75xx_spdif_probe(struct platform_device *pdev)
{
	struct cs75xx_spdif *spdif;
	struct resource *res_mem;

	dev_info(&pdev->dev, "Func: %s, pdev->name = %s\n", __func__, pdev->name);

	spdif = kzalloc(sizeof(struct cs75xx_spdif), GFP_KERNEL);
	if (!spdif) {
		dev_err(&pdev->dev, "\nFunc: %s - can't allocate memory for %s device\n", __func__, "spdif");
		return -ENOMEM;
	}
	memset(spdif, 0, sizeof(struct cs75xx_spdif));
	spdif->dev = &pdev->dev;

	if (clock) {
		/* GLOBAL */
		res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, "global");
		if (!res_mem) {
			dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "global");
			goto fail;
		}
		spdif->global_base = ioremap(res_mem->start, resource_size(res_mem));
		if (!spdif->global_base) {
			dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory\n",
						__func__, "global", resource_size(res_mem));
			goto fail;
		}
		dev_info(&pdev->dev, "\tspdif->global_base = 0x%08x, range = 0x%x\n",
			(u32)spdif->global_base, resource_size(res_mem));
	}

	/* SSP0 */
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, "ssp0");
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "ssp0");
		goto fail;
	}
	spdif->ssp_base = ioremap(res_mem->start, resource_size(res_mem));
	if (!spdif->ssp_base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory\n",
		            __func__, "ssp0", resource_size(res_mem));
		goto fail;
	}
	dev_info(&pdev->dev, "\tspdif->ssp_base = 0x%08x, range = 0x%x\n",
		(u32)spdif->ssp_base, resource_size(res_mem));

	/* SPDIF(CWda15) */
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, "spdif");
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "spdif");
		goto fail;
	}
	spdif->spdif_base = ioremap(res_mem->start, resource_size(res_mem));
	if (!spdif->spdif_base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
		            __func__, "spdif", resource_size(res_mem));
		goto fail;
	}
	dev_info(&pdev->dev, "\tspdif->spdif_base = 0x%08x, range = 0x%x\n",
		(u32)spdif->spdif_base, resource_size(res_mem));
	spdif->CWda15_Peripheral.DataBaseAddress = (unsigned int)spdif->spdif_base;
	spdif->CWda15_Peripheral.RegistersBaseAddress = (unsigned int)spdif->spdif_base;
	if (CWda15_Identify(&spdif->CWda15_Peripheral)) {
		dev_info(&pdev->dev, "CWda15 IP Core found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
	}
	else {
		dev_err(&pdev->dev, "CWda15 IP Core not found at Address: 0x%08x\r\n", spdif->CWda15_Peripheral.RegistersBaseAddress);
		goto fail;
	}

	/* IRQ */
	/* DMA SSP INT dispatcher */
	spdif->irq_tx = platform_get_irq_byname(pdev, "dma_tx_spdif");
	if (spdif->irq_tx == -ENXIO) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "irq_tx");
		goto fail;
	}
	dev_info(&pdev->dev, "\tirq_tx = %d\n", spdif->irq_tx);

	spdif->irq_spdif = platform_get_irq_byname(pdev, "irq_spdif");
	if (spdif->irq_spdif == -ENXIO) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "irq_spdif");
		goto fail;
	}
	dev_info(&pdev->dev, "\tirq_spdif = %d\n", spdif->irq_spdif);

	/* DMA-SPDIF */
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_DMA, "dma_spdif");
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "dma_spdif");
		goto fail;
	}
	spdif->dma_base = ioremap(res_mem->start, resource_size(res_mem));
	if (!spdif->dma_base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
		            __func__, "dma-ssp", resource_size(res_mem));
		goto fail;
	}
	dev_info(&pdev->dev, "\tdma_base = 0x%08x, range = 0x%x\n", (u32)spdif->dma_base,
		resource_size(res_mem));

	platform_set_drvdata(pdev, spdif);
	cs75xx_spdif_dev = pdev;

	/* init SSP/DMA */
	if (cs75xx_spdif_dma_init(spdif))
		goto fail;

	if (clock == 0) {
		if (oss_driver_init())
			goto fail;
	}

	return 0;

fail:
	if (spdif) {
		if (spdif->ssp_base)
			iounmap(spdif->ssp_base);

		if (spdif->ssp_base)
			iounmap(spdif->spdif_base);
		if (spdif->dma_base)
			iounmap(spdif->dma_base);

		kfree(spdif);
	}

	return -EPERM;
}

static int __devexit cs75xx_spdif_remove(struct platform_device *pdev)
{
	struct cs75xx_spdif *spdif = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Func: %s, pdev->name = %s\n", __func__, pdev->name);

	if (clock == 0) {
		printk("oss_driver_exit\n");
		oss_driver_exit();
	}

	cs75xx_spdif_dma_exit(spdif);

	platform_set_drvdata(pdev, NULL);

	cs75xx_spdif_dev = NULL;

	iounmap(spdif->ssp_base);

	iounmap(spdif->spdif_base);
	iounmap(spdif->dma_base);

	kfree(spdif);

	return 0;
}

static struct platform_driver cs75xx_spdif_platform_driver = {
	.probe	= cs75xx_spdif_probe,
	.remove	= __devexit_p(cs75xx_spdif_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name  = "cs75xx_spdif",
	},
};

static int __init cs75xx_spdif_init(void)
{
	printk("\n%s\n", __func__);

	return platform_driver_register(&cs75xx_spdif_platform_driver);
}

static void __exit cs75xx_spdif_exit(void)
{
	printk("\n%s\n", __func__);

	platform_driver_unregister(&cs75xx_spdif_platform_driver);
}

module_init(cs75xx_spdif_init);
module_exit(cs75xx_spdif_exit);

module_param(debug, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug, "spdif debug flag");

module_param(clock, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clock, "spdif clock generation");

module_param(delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(delay, "spdif sync delay");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX SPDIF driver");

