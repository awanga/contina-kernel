/*
 *  linux/drivers/mmc/host/cs752x_sd.h
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 *  Reference: drivers/mmc/host/s3cmci.h
 *             arch/arm/mach-at91/include/mach/board.h
 */
#ifndef __CS752X_SD_H
#define __CS752X_SD_H

#include <linux/dma-mapping.h>

typedef struct sd_descriptor_s
{
	union des0_t
	{
		unsigned int bits32;
		struct bit_des0
		{
			unsigned int reserved0	: 1;
			unsigned int dic	: 1;/* b1: Dis Int on Comp */
			unsigned int last_desc	: 1;/* b2: Last Descriptor */
			unsigned int first_desc	: 1;/* b3: First Descriptor */
			unsigned int chained	: 1;/* b4: Second Add Chained */
			unsigned int end_ring	: 1;/* b5: End of Ring */
			unsigned int reserved1	:24;
			unsigned int card_error	: 1;/* b30: Card Error Summary */
			unsigned int own	: 1;/* b31: OWN */
		} bits;
	} des0;

	union des1_t
	{
		unsigned int bits32;
		struct bit_des1
		{
			unsigned int buffer_size1: 13;	/* b12:0:  Buf1 Size */
			unsigned int buffer_size2: 13;	/* b25:13: Buf2 Size */
			unsigned int reserved    :  6;
		} bits;
	} des1;

	unsigned int buffer_addr;	/* Buffer Address Pointer 1 (BAP1) */
	unsigned int next_desc;		/* Next Descriptor Address */
} SD_DESCRIPT_T;

struct cs752x_sd_reg {
	char *name;
	u32 addr;
};

struct cs752x_sd_host {
	struct platform_device *pdev;
	struct mmc_host *mmc;
	struct mmc_request *mrq;
	struct resource *mem;
	void __iomem *base;
	struct platform_clk clk;

	spinlock_t lock;

	/* Interrupt */
	int irq;
	u32 int_status;
	u32 int_status_for_stopcmd;
	u32 int_idma_sts;
	int cardin;	/* 1: card exists */

	/* DMA Data Transfer */
	int usedma;	/* 1: use dma, 0: use pio */
	enum dma_data_direction	dmadir;		/* dma direction */
	int descnt;	/* dma descriptor list count */
	int datasize;	/* transfer data size */
	dma_addr_t desc_dma;	/* physical address of idma descriptor */
	SD_DESCRIPT_T *desc_cpu;	/* virtual address of idma descriptor */

#ifdef CONFIG_DEBUG_FS
	struct dentry *debug_root;
	struct dentry *debug_state;
	struct dentry *debug_regs;
#endif

	union flag
	{
		u32 word;
		struct bit {
			u32 cmding     : 1;/* b0: (1) sd cmd is in processing */
			u32 xferdone   : 1;/* b1: (0) data transfer done */
			u32 err_cmd    : 1;/* b2: command with error */
			u32 err_xfer   : 1;/* b3: data transfer with error */
			u32 err_fatal  : 1;/* b4: fatal error */
			u32 rstctrlreq : 1;/* b5: (0) reset the ctrlr request */
			u32 reserved   :26;/* b7-31: */
		} bit;
	} flag;

	int sdinrst;	/* sd controller in reset status */

#ifdef CONFIG_MMC_DEBUG
	u32 precmd[3];	/* previous command */
#define DBG_BUF_LEN  256
	char dbgmsg_cmd[DBG_BUF_LEN];
	char dbgmsg_dat[DBG_BUF_LEN];

#define CS752X_DBG_ERRORHAPPEN	1000		/* max error count */
	u32 dbg_cmdinerr;/* If error happened, enable some debugging messages */
	u32 dbg_reqcnt;	 /* request count */
	u32 dbg_cmdcnt;	 /* sd issued command count excluding auto CMD12 */
#endif
};

#define F_400KHZ        400000
#define F_25MHZ         25000000
#define F_50MHZ         50000000
#ifdef CONFIG_CORTINA_FPGA
#define APB_MMC_HZ      50000000	/*  50 MHz for FPGA APB MMC bus */
#endif
#define CMD_MAX_RETRIES 0x100
#define DMA_MAX_BUFFER  0x10000		/* 64 KB */
#define MMC_MAX_BLOCK_SIZE      512    	/* max number of bytes */

#define G2_FIFOSIZE    (16*4)   	/* the size of the fifo in bytes */
#define G2_NR_SG        64		/* Max No. of scatter/gather */

#define	CPU             0
#define	DMA             1
#define	DESC_BUF_NUM   G2_NR_SG

enum cs752x_dbg_channels {
	cs752x_dbg_err   = (1 << 0),
	cs752x_dbg_debug = (1 << 1),
	cs752x_dbg_info  = (1 << 2),
	cs752x_dbg_irq   = (1 << 3),
	cs752x_dbg_sg    = (1 << 4),
	cs752x_dbg_dma   = (1 << 5),
	cs752x_dbg_pio   = (1 << 6),
	cs752x_dbg_fail  = (1 << 7),
	cs752x_dbg_conf  = (1 << 8),
};

static inline bool cs752x_sd_host_usedma(struct cs752x_sd_host *cs752x_host);
static const int cs752x_dbgmap_err   = cs752x_dbg_fail | cs752x_dbg_debug;
static const int cs752x_dbgmap_info  = cs752x_dbg_info | cs752x_dbg_conf;
static const int cs752x_dbgmap_debug = cs752x_dbg_err | cs752x_dbg_debug;

static inline void cs752x_sd_set_bits(struct cs752x_sd_host *cs752x_host, enum cs752x_sd_regs g2_reg, u32 setbits);
static inline void cs752x_sd_clr_bits(struct cs752x_sd_host *cs752x_host, enum cs752x_sd_regs g2_reg, u32 setbits);
static void cs752x_sd_enable_int(struct cs752x_sd_host *cs752x_host);
static void cs752x_sd_disable_int(struct cs752x_sd_host *cs752x_host);
static int cs752x_sd_get_cd(struct mmc_host *mmc);

static void cs752x_sd_send_cmd(struct cs752x_sd_host *cs752x_host, u32 manu_cmd12);

/* Error Handling Flags */
#define	REQUEST_NORMAL_DONE	0	/* this request is normally finished without error */
#define	ERR_CMD			(1<<0)	/* error command handle */
#define	ERR_DAT			(1<<1)	/* error data handle */
#define	ERR_FATAL		(1<<2)	/* fatal error happen */
#define	ERR_CDT			(1<<3)	/* card detection error */
#define	ERR_CMD_HANDLE		(ERR_CMD)
#define	ERR_DAT_HANDLE		(ERR_DAT)
#define	ERR_CMD_DAT_HANDLE	(ERR_CMD | ERR_DAT)
#define	FATAL_CMD_ERROR		(ERR_CMD | ERR_FATAL)
#define	FATAL_DAT_ERROR		(ERR_DAT | ERR_FATAL)
#define	ERR_HANDLE		(ERR_CMD | ERR_DAT)
#define	FATAL_HANDLE		(ERR_CMD | ERR_DAT | ERR_FATAL)

#define	WAIT_DTO		(1<<8)		/* CMD_DONE but DATA_NOT_DONE */
#define	WAIT_ACD		(1<<9)		/* DATA_DONE but AUTO_CMD_NOT_DONE */
#define	WAIT_ANY		(WAIT_DTO | WAIT_ACD)	/* Wait DTO or ACD flags */

#define	DTO_TRAP		(1<<16)		/* Data transfer over trap */
#define	ACD_TRAP		(1<<17)		/* Auto command done trap */
#define	ANY_TRAP		(DTO_TRAP | ACD_TRAP)	/* Any trap happens? */

/* Configure SD Card Detection High */
#define SD_CD_PRESENT_LOW   TRUE
#endif
