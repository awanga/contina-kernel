/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _CS75XX_SSP_H
#define _CS75XX_SSP_H

#if defined(CONFIG_CORTINA_ENGINEERING_S)
#define CS75XX_SSP_NUM		1
#else
#define CS75XX_SSP_NUM		2
#endif
#define CS75XX_SSP_CTLR_NAME	"cs75xx-ssp"

typedef enum {
	SSP_I2S_DAC,	/* DAC - CS4341 */
	SSP_I2S_D2,	/* D2-45057 */
	SSP_PCM_SSLIC,	/* Silicon Labs SLIC */
	SSP_PCM_ZSLIC	/* Zarlink SLIC */
} cs75xx_ssp_profile_t;

typedef enum {
	SCLK_INT_CORE,	/* internal CORE */
	SCLK_EXT_OSC,	/* external 16.384MHz oscillator */
	SCLK_INT_SPDIF	/* internal SPDIF module */
} cs75xx_ssp_sclk_t;

typedef struct {
	cs75xx_ssp_profile_t profile;
	u16 codec_size;
	u8 chan_num;
	u8 ext_clk;
	u16 sclk;
} cs75xx_ssp_cfg_t;

typedef struct {
	u16 buf_num;
	u16 buf_size;
	u16 tx_read_ptr, tx_write_ptr;
	u16 rx_read_ptr, rx_write_ptr;
	u8 *tbuf, *rbuf;
	dma_addr_t tbuf_paddr, rbuf_paddr;	/* physical addr */
	u8 ssp_index;
	u8 byte_swap;
	u8 ssp_en;
	u8 sample_size;
} cs75xx_ssp_ctrl_t;

int cs75xx_ssp_register(u32 index, cs75xx_ssp_cfg_t ssp_cfg);
int cs75xx_ssp_unregister(u32 index);
int cs75xx_ssp_slot(u32 index, cs75xx_ssp_profile_t profile, u8 slot_num);
int cs75xx_ssp_reg_read(u32 index, u32 offset, u32 *val_p);
int cs75xx_ssp_reg_write(u32 index, u32 offset, u32 val);

int cs75xx_ssp_enable(u32 index, u32 tx_en, u32 tx_swap, u32 rx_en, u32 rx_swap);
int cs75xx_ssp_disable(u32 index);

int cs75xx_dma_ssp_register(u32 index, dma_addr_t tbuf_paddr, u32 tx_buf_num, u32 tx_buf_size, dma_addr_t rbuf_paddr, u32 rx_buf_num, u32 rx_buf_size);
int cs75xx_dma_ssp_unregister(u32 index);
int request_cs75xx_dma_ssp_irq(u32 index, void (*tx_handler)(void), void (*rx_handler)(void));
int free_cs75xx_dma_ssp_irq(u32 index);
int cs75xx_dma_ssp_tx_ptr(u32 index, u16 *dma_wt_ptr_p, u16 *dma_rd_ptr_p);
int cs75xx_dma_ssp_rx_ptr(u32 index, u16 *dma_wt_ptr_p, u16 *dma_rd_ptr_p);
int cs75xx_dma_ssp_tx_update(u32 index, u16 dma_wt_ptr);
int cs75xx_dma_ssp_rx_update(u32 index, u16 dma_rd_ptr);
int cs75xx_dma_ssp_tx_enable(u32 index);
int cs75xx_dma_ssp_rx_enable(u32 index);
int cs75xx_dma_ssp_tx_disable(u32 index);
int cs75xx_dma_ssp_rx_disable(u32 index);

void cs75xx_dma_tx_disable(int enable);
void cs75xx_dma_rx_disable(int enable);
#endif	/* _CS75XX_SSP_H */

