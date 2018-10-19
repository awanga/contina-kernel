/*
 * File: sound/soc/cs75xx/cs75xx_sound_sport.h
 * Description: ASoC platform driver for CS75XX - DMA engine
 *
 * Copyright (c) Cortina-Systems Limited 2012. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CS755XX_SND_SPORT_H__
#define __CS755XX_SND_SPORT_H__

/* audio formats */
#define SPORT_DATA_I2S			0 /* I2S mode */
#define SPORT_DATA_RIGHT_J		1 /* Right Justified mode */
#define SPORT_DATA_LEFT_J		2 /* Left Justified mode */
#define SPORT_DATA_DSP_A		3 /* L data MSB after FRM LRC */
#define SPORT_DATA_DSP_B		4 /* L data MSB during FRM LRC */
#define SPORT_DATA_AC97			5 /* AC97 */
#define SPORT_DATA_PDM			6 /* Pulse density modulation */

/* clock gating */
#define SPORT_CLOCK_CONT		(0 << 4) /* continuous clock */
#define SPORT_CLOCK_GATED		(1 << 4) /* clock is gated */

/* signal inversions */
#define SPORT_SIGNAL_NB_NF		(0 << 8) /* normal bit clock + frame */
#define SPORT_SIGNAL_NB_IF		(1 << 8) /* normal BCLK + inv FRM */
#define SPORT_SIGNAL_IB_NF		(2 << 8) /* invert BCLK + nor FRM */
#define SPORT_SIGNAL_IB_IF		(3 << 8) /* invert BCLK + FRM */

/* clock masters */
#define SPORT_CLOCK_CBM_CFM		(0 << 12) /* codec clk & FRM master */
#define SPORT_CLOCK_CBS_CFM		(1 << 12) /* codec clk slave & FRM master */
#define SPORT_CLOCK_CBM_CFS		(2 << 12) /* codec clk master & frame slave */
#define SPORT_CLOCK_CBS_CFS		(3 << 12) /* codec clk & FRM slave */

#define SPORT_DATA_FORMAT_MASK		0x000f
#define SPORT_CLOCK_GATE_MASK		0x00f0
#define SPORT_SIGNAL_INV_MASK		0x0f00
#define SPORT_CLOCK_MASTER_MASK		0xf000

/* clock division id */
#define SPORT_SPDIF_CLK_DIV		0
#define SPORT_EXT_16384_CLK_DIV		1
#define SPORT_INT_SYS_CLK_DIV		2


#define DIR_TX	0
#define DIR_RX	1

#if defined(CONFIG_CORTINA_ENGINEERING_S) || defined(CONFIG_CORTINA_REFERENCE_S)
#define CS75XX_SSP_NUM		1
#else
#define CS75XX_SSP_NUM		2
#endif

#define CS75XX_SPORT_PCM	0
#define CS75XX_SPORT_I2S	1
#define CS75XX_SPORT_SPDIF	2
#define CS75XX_SPORT_UNDEF	3

int cs75xx_snd_sport_dma_callback(unsigned int idx, int dir, void (*callback)(void *), void *data);
int cs75xx_snd_sport_dma_register(unsigned int idx, int dir, dma_addr_t paddr, unsigned int buf_num, unsigned int buf_size);
int cs75xx_snd_sport_dma_unregister(unsigned int idx, int dir);
int cs75xx_snd_sport_dma_idx(unsigned int idx, int dir, u16 *wt_idx_p, u16 *rd_idx_p);
int cs75xx_snd_sport_dma_update(unsigned int idx, int dir, u16 new_idx);
int cs75xx_snd_sport_start(unsigned int idx, int dir);
int cs75xx_snd_sport_stop(unsigned int idx, int dir);
unsigned int cs75xx_snd_sport_dma_curr_offset(unsigned int idx, int dir);
int cs75xx_snd_sport_dma_request(unsigned int idx);
int cs75xx_snd_sport_clkdiv(unsigned int idx, int clk_src, int data_rate);
int cs75xx_snd_sport_signal_formt(unsigned int idx, unsigned int format);
int cs75xx_snd_sport_slots(unsigned int idx, unsigned int tx_mask, unsigned int rx_mask, int slots, int width);
int cs75xx_snd_sport_audio_formt(unsigned int idx, int dir, int wide, int endian);
int cs75xx_snd_sport_if_register(unsigned int idx, int type, int tx, int rx);
int cs75xx_snd_sport_if_unregister(unsigned int idx);
unsigned int cs75xx_pwr_dma_start(unsigned int idx);
unsigned int cs75xx_pwr_dma_stop(unsigned int idx);
struct device * cs75xx_snd_sport_device(void);

#define SPORT_TRACE	BIT(0)
#define SPORT_INFO	BIT(1)
#define DAI_TRACE	BIT(2)
#define DAI_INFO	BIT(3)
#define PLAT_TRACE	BIT(4)
#define PLAT_INFO	BIT(5)

extern unsigned int sport_debug;

#endif /* __CS755XX_SND_SPORT_H__ */

