/*
 *  Copyright (C) 2003 Rick Bronson
 *
 *  Derived from drivers/mtd/nand/autcpu12.c
 *	 Copyright (c) 2001 Thomas Gleixner (gleixner@autronix.de)
 *
  *  Derived from drivers/mtd/spia.c
 *	 Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *
 *
 *  Add Hardware ECC support for AT91SAM9260 / AT91SAM9263
 *     Richard Genoud (richard.genoud@gmail.com), Adeneo Copyright (C) 2007
 *
 *     Derived from Das U-Boot source code
 *     		(u-boot-1.1.5/board/atmel/at91sam9263ek/nand.c)
 *     (C) Copyright 2006 ATMEL Rousset, Lacressonniere Nicolas
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>



#include <linux/gpio.h>
#include <linux/io.h>

#include <linux/interrupt.h>

#include <mach/platform.h>
#include <mach/hardware.h>
#include <mach/registers.h>
#include <mach/cs752x_flash.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <mach/cs_clk_change.h>

#include <linux/mutex.h>

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
#include <linux/semaphore.h>
extern struct semaphore cs752x_flash_sem;
#endif


#if defined(CONFIG_CS752X_NAND_ECC_HW_BCH_8_512) || defined(CONFIG_CS752X_NAND_ECC_HW_BCH_12_512)
  #define CONFIG_CS752X_NAND_ECC_HW_BCH
#endif

#if defined(CONFIG_CS752X_NAND_ECC_HW_HAMMING_256) || defined(CONFIG_CS752X_NAND_ECC_HW_HAMMING_512)
  #define CONFIG_CS752X_NAND_ECC_HW_HAMMING
#endif


/** #define NAND_DIRECT_ACCESS  1 **/
#define NAND_ECC_TEST  1

static int cs752x_ecc_check = 0;

static int cs752x_oob_config_size= 640;

struct cs752x_nand_host {
	struct nand_chip	*nand_chip;
	struct mtd_info		*     mtd;
	void __iomem		*io_base;
	struct device		*dev;
};

struct cs752x_nand_host *cs752x_host;
static int nand_page=0,nand_col=0;
static struct mtd_partition *mtd_parts;
static const char *probes[] = { "cmdlinepart", NULL };
static volatile int dummy;
static u32 nflash_type = 0x5000;

#if 0
static struct mtd_partition cs752x_partition_info[] = {
	{ name: "RedBoot",     	 	offset: 0x00000000, size: 0x00100000, },
	{ name: "Kernel",      	 	offset: 0x00100000, size: 0x00A00000, },
	{ name: "Ramdisk",     	 	offset: 0x00B00000, size: 0x01000000, },
	{ name: "Application", 	 	offset: 0x01B00000, size: 0x01000000, },
	{ name: "VCTL", 		offset: 0x02B00000, size: 0x00100000, },
	{ name: "CurConf", 	 	offset: 0x02C00000, size: 0x00200000, },
	{ name: "FIS directory", 	offset: 0x02E00000, size: 0x00200000, },
	{ name: "Reserved",      	offset: 0x03000000, size: 0x00100000, },
};
#else
static struct mtd_partition cs752x_partition_info[] = {
	{ name: "RedBoot",     	 	offset: 0x00000000, size: 0x00A00000, },
	{ name: "Kernel",      	 	offset: 0x00A00000, size: 0x00A00000, },
	{ name: "Application",      	offset: 0x01400000, size: 0x00A00000, },
	{ name: "Test",		      	offset: 0x01e00000, size: 0x02200000, },
	{ name: "Test1",	      	offset: 0x04000000, size: 0x04000000, },
	{ name: "Test2",	      	offset: 0x08000000, size: 0x08000000, },
};
#endif


static struct nand_ecclayout cs752x_nand_ecclayout;

/* Define default oob placement schemes for large and small page devices */
#ifdef	CONFIG_CS752X_NAND_ECC_HW_BCH

static struct nand_ecclayout cs752x_nand_bch_oob_16 = {
	.eccbytes = 13,
	.eccpos = {0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14},
	.oobfree = {
		{.offset = 15,
		 /*  . length = 1}} // resever 1 for erase tags: 1 - 1 = 0 */
		 . length = 0}} // resever 1 for erase tags: 1 - 1 = 0
};

#else

static struct nand_ecclayout cs752x_nand_oob_8 = {
	.eccbytes = 3,
	.eccpos = {0, 1, 2},
	.oobfree = {
		{.offset = 3,
		 .length = 2},
		{.offset = 6,
		 .length = 2}}
};

static struct nand_ecclayout cs752x_nand_oob_16 = {
	.eccbytes = 6,
	.eccpos = {0, 1, 2, 3, 6, 7},
	.oobfree = {
		{.offset = 8,
		 . length = 8}}
};

#endif


/* Generic flash bbt decriptors
*/
static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr cs752x_bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = 4,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr cs752x_bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = 4,
	.pattern = mirror_pattern
};

static unsigned int CHIP_EN;
static unsigned int *pread, *pwrite;
static FLASH_TYPE_t			flash_type;
static FLASH_STATUS_t			flash_status;
static FLASH_NF_ACCESS_t		nf_access;
static FLASH_NF_COUNT_t		nf_cnt;
static FLASH_NF_COMMAND_t		nf_cmd;
static FLASH_NF_ADDRESS_1_t		nf_addr1;
static FLASH_NF_ADDRESS_2_t		nf_addr2;
static FLASH_NF_ECC_STATUS_t		ecc_sts;
static FLASH_NF_ECC_CONTROL_t 		ecc_ctl;
static FLASH_NF_ECC_OOB_t		ecc_oob;
static FLASH_NF_ECC_GEN0_t		ecc_gen0;
static FLASH_NF_FIFO_CONTROL_t		fifo_ctl;
static FLASH_FLASH_ACCESS_START_t 	flash_start;
static FLASH_NF_ECC_RESET_t		ecc_reset;
static FLASH_FLASH_INTERRUPT_t		flash_int_sts;
static FLASH_NF_BCH_STATUS_t		bch_sts;
static FLASH_NF_BCH_ERROR_LOC01_t	bch_err_loc01;
static FLASH_NF_BCH_CONTROL_t		bch_ctrl;
static FLASH_NF_BCH_OOB0_t		bch_oob0;
static FLASH_NF_BCH_GEN0_0_t		bch_gen00;

/* DMA regs */
//#ifndef NAND_DIRECT_ACCESS
static DMA_DMA_SSP_TXQ5_CONTROL_t 			dma_txq5_ctrl;
static DMA_DMA_SSP_RXQ5_WPTR_t				dma_rxq5_wptr;
static DMA_DMA_SSP_RXQ5_RPTR_t				dma_rxq5_rptr;
static DMA_DMA_SSP_TXQ5_WPTR_t				dma_txq5_wptr;
static DMA_DMA_SSP_TXQ5_RPTR_t				dma_txq5_rptr;
static DMA_DMA_SSP_RXQ5_INTERRUPT_t		dma_ssp_rxq5_intsts;
static DMA_DMA_SSP_TXQ5_INTERRUPT_t		dma_ssp_txq5_intsts;
static DMA_SSP_TX_DESC_T *tx_desc;
static DMA_SSP_RX_DESC_T *rx_desc;
//#endif

#define OWN_DMA	0
#define OWN_SW	1

static int cs752x_nand_block_checkbad(struct mtd_info *mtd, loff_t ofs, int getchip, int allowbbt);
static int cs752x_nand_get_device(struct nand_chip *chip, struct mtd_info *mtd, int new_state);
static void cs752x_nand_release_device (struct mtd_info *mtd);
static int cs752x_nand_do_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);
static int cs752x_nand_do_write_ops(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);
static void cs752x_nand_wait_ready(struct mtd_info *mtd);
static int cs752x_nand_do_read_ops(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);
static int cs752x_nand_erase_nand(struct mtd_info *mtd, struct erase_info *instr, int allowbbt);
static int cs752x_nand_check_wp (struct mtd_info *mtd);
static int cs752x_nand_dev_ready(struct mtd_info *mtd);

#ifdef	NAND_ECC_TEST
unsigned char tw[NAND_MAX_PAGESIZE]__attribute__((aligned(16))) ,  tr[NAND_MAX_PAGESIZE]__attribute__((aligned(16))); ;
unsigned int eccdata;
unsigned char eccode[32]__attribute__((aligned(16)));


void cs752x_ecc_check_enable( int isEnable)
{
	cs752x_ecc_check = isEnable;
}
EXPORT_SYMBOL( cs752x_ecc_check_enable);

int nand_calculate_256_ecc(unsigned char* data_buf, unsigned char* ecc_buf)
{

	unsigned int i;
	unsigned int tmp;
	unsigned int uiparity = 0;
	unsigned int parityCol, ecc = 0;
	unsigned int parityCol4321 = 0, parityCol4343 = 0, parityCol4242 =
	    0, parityColTot = 0;
	unsigned int *Data = (unsigned int *)(data_buf);
	unsigned int Xorbit = 0;

	for (i = 0; i < 8; i++) {
		parityCol = *Data++;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4242 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4343 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4343 ^= tmp;
		parityCol4242 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4321 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4242 ^= tmp;
		parityCol4321 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4343 ^= tmp;
		parityCol4321 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4242 ^= tmp;
		parityCol4343 ^= tmp;
		parityCol4321 ^= tmp;

		parityColTot ^= parityCol;

		tmp = (parityCol >> 16) ^ parityCol;
		tmp = (tmp >> 8) ^ tmp;
		tmp = (tmp >> 4) ^ tmp;
		tmp = ((tmp >> 2) ^ tmp) & 0x03;
		if ((tmp == 0x01) || (tmp == 0x02)) {
			uiparity ^= i;
			Xorbit ^= 0x01;
		}
	}

	tmp = (parityCol4321 >> 16) ^ parityCol4321;
	tmp = (tmp << 8) ^ tmp;
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp >> 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x200;	/*  p128 */

	tmp = (parityCol4343 >> 16) ^ parityCol4343;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 4) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x80;	/*  p64 */

	tmp = (parityCol4242 >> 16) ^ parityCol4242;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 4) ^ tmp;
	tmp = (tmp >> 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x20;	/*  p32 */

	tmp = parityColTot & 0xFFFF0000;
	tmp = tmp >> 16;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x08;	/*  p16 */

	tmp = parityColTot & 0xFF00FF00;
	tmp = (tmp >> 16) ^ tmp;
	tmp = (tmp >> 8);
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp >> 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x02;	/*  p8 */

	tmp = parityColTot & 0xF0F0F0F0;
	tmp = (tmp << 16) ^ tmp;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x800000;	/*  p4 */

	tmp = parityColTot & 0xCCCCCCCC;
	tmp = (tmp << 16) ^ tmp;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 4) ^ tmp;
	tmp = (tmp >> 2);
	ecc |= ((tmp << 1) ^ tmp) & 0x200000;	/*  p2 */

	tmp = parityColTot & 0xAAAAAAAA;
	tmp = (tmp << 16) ^ tmp;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= (tmp & 0x80000);	/*  p1 */

	ecc |= (uiparity & 0x01) << 11;	/*  parit256_1 */
	ecc |= (uiparity & 0x02) << 12;	/*  parit512_1 */
	ecc |= (uiparity & 0x04) << 13;	/*  parit1024_1 */

	if (Xorbit) {
		ecc |= (ecc ^ 0x00A8AAAA) >> 1;
	} else {
		ecc |= (ecc >> 1);
	}

	ecc = ~ecc;
	*(ecc_buf + 2) = (unsigned char)(ecc >> 16);
	*(ecc_buf + 1) = (unsigned char)(ecc >> 8);
	*(ecc_buf + 0) = (unsigned char)(ecc);

	return 0;
}
EXPORT_SYMBOL(nand_calculate_256_ecc);
#endif	/* NAND_ECC_TEST */
/**
 * nand_calculate_512_ecc - [NAND Interface] Calculate 3-byte ECC for 256/512-byte
 *			 block
 * @mtd:	MTD block structure
 * @buf:	input buffer with raw data
 * @code:	output buffer with ECC
 */
int nand_calculate_512_ecc(struct mtd_info *mtd, const unsigned char *data_buf,
		       unsigned char *ecc_buf)
{
#if 1
	unsigned long i, ALIGN_FACTOR;
	unsigned long tmp;
	unsigned long uiparity = 0;
	unsigned long parityCol, ecc = 0;
	unsigned long parityCol4321 = 0, parityCol4343 = 0, parityCol4242 =
	    0, parityColTot = 0;
	unsigned long *Data;
	unsigned long Xorbit = 0;

	ALIGN_FACTOR = (unsigned long)data_buf % 4;
	Data = (unsigned long *)(data_buf + ALIGN_FACTOR);

	for (i = 0; i < 16; i++) {
		parityCol = *Data++;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4242 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4343 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4343 ^= tmp;
		parityCol4242 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4321 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4242 ^= tmp;
		parityCol4321 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4343 ^= tmp;
		parityCol4321 ^= tmp;
		tmp = *Data++;
		parityCol ^= tmp;
		parityCol4242 ^= tmp;
		parityCol4343 ^= tmp;
		parityCol4321 ^= tmp;

		parityColTot ^= parityCol;

		tmp = (parityCol >> 16) ^ parityCol;
		tmp = (tmp >> 8) ^ tmp;
		tmp = (tmp >> 4) ^ tmp;
		tmp = ((tmp >> 2) ^ tmp) & 0x03;
		if ((tmp == 0x01) || (tmp == 0x02)) {
			uiparity ^= i;
			Xorbit ^= 0x01;
		}
	}

	tmp = (parityCol4321 >> 16) ^ parityCol4321;
	tmp = (tmp << 8) ^ tmp;
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp >> 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x200;	/*  p128 */

	tmp = (parityCol4343 >> 16) ^ parityCol4343;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 4) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x80;	/*  p64 */

	tmp = (parityCol4242 >> 16) ^ parityCol4242;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 4) ^ tmp;
	tmp = (tmp >> 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x20;	/*  p32 */

	tmp = parityColTot & 0xFFFF0000;
	tmp = tmp >> 16;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x08;	/*  p16 */

	tmp = parityColTot & 0xFF00FF00;
	tmp = (tmp >> 16) ^ tmp;
	tmp = (tmp >> 8);
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp >> 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x02;	/*  p8 */

	tmp = parityColTot & 0xF0F0F0F0;
	tmp = (tmp << 16) ^ tmp;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= ((tmp << 1) ^ tmp) & 0x800000;	/*  p4 */

	tmp = parityColTot & 0xCCCCCCCC;
	tmp = (tmp << 16) ^ tmp;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp << 4) ^ tmp;
	tmp = (tmp >> 2);
	ecc |= ((tmp << 1) ^ tmp) & 0x200000;	/*  p2 */

	tmp = parityColTot & 0xAAAAAAAA;
	tmp = (tmp << 16) ^ tmp;
	tmp = (tmp >> 8) ^ tmp;
	tmp = (tmp >> 4) ^ tmp;
	tmp = (tmp << 2) ^ tmp;
	ecc |= (tmp & 0x80000);	/*  p1 */

	ecc |= (uiparity & 0x01) << 11;
	ecc |= (uiparity & 0x02) << 12;
	ecc |= (uiparity & 0x04) << 13;
	ecc |= (uiparity & 0x08) << 14;

	if (Xorbit) {
		ecc |= (ecc ^ 0x00AAAAAA) >> 1;
	} else {
		ecc |= (ecc >> 1);
	}

	ecc = ~ecc;
	*(ecc_buf + 2) = (uint8_t) (ecc >> 16);
	*(ecc_buf + 1) = (uint8_t) (ecc >> 8);
	*(ecc_buf + 0) = (uint8_t) (ecc);
#endif
	return 0;
}
EXPORT_SYMBOL(nand_calculate_512_ecc);


/**
 * nand_correct_512_data - [NAND Interface] Detect and correct bit error(s)
 * @mtd:	MTD block structure
 * @buf:	raw data read from the chip
 * @read_ecc:	ECC from the chip
 * @calc_ecc:	the ECC calculated from raw data
 *
 * Detect and correct a 1 bit error for 256/512 byte block
 */
int nand_correct_512_data(struct mtd_info *mtd, unsigned char *pPagedata,
		      unsigned char *iEccdata1, unsigned char *iEccdata2)
{

	uint32_t iCompecc = 0, iEccsum = 0;
	uint32_t iFindbyte = 0;
	uint32_t iIndex;
	uint32_t nT1 = 0, nT2 = 0;

	uint8_t iNewvalue;
	uint8_t iFindbit = 0;

	uint8_t *pEcc1 = (uint8_t *) iEccdata1;
	uint8_t *pEcc2 = (uint8_t *) iEccdata2;

	for (iIndex = 0; iIndex < 2; iIndex++) {
		nT1 ^= (((*pEcc1) >> iIndex) & 0x01);
		nT2 ^= (((*pEcc2) >> iIndex) & 0x01);
	}

	for (iIndex = 0; iIndex < 3; iIndex++)
		iCompecc |= ((~(*pEcc1++) ^ ~(*pEcc2++)) << iIndex * 8);

	for (iIndex = 0; iIndex < 24; iIndex++) {
		iEccsum += ((iCompecc >> iIndex) & 0x01);
	}

	switch (iEccsum) {
	case 0:
		/* printf("RESULT : no error\n"); */
		return 1;	/* ECC_NO_ERROR; */

	case 1:
		/* printf("RESULT : ECC code 1 bit fail\n"); */
		return 1;	/* ECC_ECC_ERROR; */

	case 12:
		if (nT1 != nT2) {
			iFindbyte =
			    ((iCompecc >> 17 & 1) << 8) +
			    ((iCompecc >> 15 & 1) << 7) +
			    ((iCompecc >> 13 & 1) << 6)
			    + ((iCompecc >> 11 & 1) << 5) +
			    ((iCompecc >> 9 & 1) << 4) +
			    ((iCompecc >> 7 & 1) << 3)
			    + ((iCompecc >> 5 & 1) << 2) +
			    ((iCompecc >> 3 & 1) << 1) + (iCompecc >> 1 & 1);
			iFindbit =
			    (uint8_t) (((iCompecc >> 23 & 1) << 2) +
				       ((iCompecc >> 21 & 1) << 1) +
				       (iCompecc >> 19 & 1));
			iNewvalue =
			    (uint8_t) (pPagedata[iFindbyte] ^ (1 << iFindbit));

			/* printf("iCompecc = %d\n",iCompecc); */
			/* printf("RESULT : one bit error\r\n"); */
			/* printf("byte = %d, bit = %d\r\n", iFindbyte, iFindbit); */
			/* printf("corrupted = %x, corrected = %x\r\n", pPagedata[iFindbyte], iNewvalue); */

			return 1;	/* ECC_CORRECTABLE_ERROR; */
		} else
			return -1;	/* ECC_UNCORRECTABLE_ERROR; */

	default:
		/* printf("RESULT : unrecoverable error\n"); */
		return -1;	/* ECC_UNCORRECTABLE_ERROR; */
	}
	/* middle not yet */
	return 0;
}

#ifdef CONFIG_PM
static cs_status_t cs752x_nand_pwr_notifier(cs_pm_freq_notifier_data_t *data)
{
	struct nand_chip *chip = cs752x_host->nand_chip;
	struct mtd_info *mtd = cs752x_host->mtd;
	
	if (data->event == CS_PM_FREQ_PRECHANGE) {

		cs752x_nand_get_device(chip, mtd, FL_FREQ_CHANGE);
			
		//printk(KERN_ERR ">>>start %x! per %d -> %d, axi %d -> %d \n", 
		//(int)data->data,
		//data->old_peripheral_clk, data->new_peripheral_clk,
		//data->old_axi_clk, data->new_axi_clk);

		
	} else if (data->event == CS_PM_FREQ_POSTCHANGE) {

		cs752x_nand_release_device(mtd);
		//printk(KERN_ERR "<<<stop %x! per %d -> %d, axi %d -> %d \n", 
		//       (int)data->data,
		//       data->old_peripheral_clk, data->new_peripheral_clk,
		//       data->old_axi_clk, data->new_axi_clk);
	}

	return CS_E_OK;
}

static cs_pm_freq_notifier_t n = {
	.notifier = cs752x_nand_pwr_notifier,
	.data = (void *)0x1,
};
#endif

static void check_flash_ctrl_status()
{
	int rty = 0;
	unsigned long	timeo ;
	
	timeo = jiffies + HZ;
	do {
		flash_status.wrd = read_flash_ctrl_reg(FLASH_STATUS);
		if(!flash_status.bf.nState)
			return;
	} while (time_before(jiffies, timeo));

	printk("FLASH_STATUS ERROR: %x\n", flash_status.wrd);
	
}

/**
 * cs752x_nand_fill_oob - [Internal] Transfer client buffer to oob
 * @chip:	nand chip structure
 * @oob:	oob data buffer
 * @ops:	oob ops structure
 */
static uint8_t *cs752x_nand_fill_oob(struct mtd_info *mtd, uint8_t *oob, size_t len,
			      struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;

	memset(chip->oob_poi, 0xff, mtd->oobsize);

	switch (ops->mode) {

	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
		memcpy(chip->oob_poi + ops->ooboffs, oob, len);
		return oob + len;

	case MTD_OPS_AUTO_OOB:{
			struct nand_oobfree *free = chip->ecc.layout->oobfree;
			uint32_t boffs = 0, woffs = ops->ooboffs;
			size_t bytes = 0;

			for (; free->length && len; free++, len -= bytes) {
				/* Write request not from offset 0 ? */
				if (unlikely(woffs)) {
					if (woffs >= free->length) {
						woffs -= free->length;
						continue;
					}
					boffs = free->offset + woffs;
					bytes = min_t(size_t, len,
						      (free->length - woffs));
					woffs = 0;
				} else {
					bytes =
					    min_t(size_t, len, free->length);
					boffs = free->offset;
				}
				memcpy(chip->oob_poi + boffs, oob, bytes);
				oob += bytes;
			}
			return oob;
		}
	default:
		BUG();
	}
	return NULL;
}

/**
 * nand_transfer_oob - [Internal] Transfer oob to client buffer
 * @chip:	nand chip structure
 * @oob:	oob destination address
 * @ops:	oob ops structure
 * @len:	size of oob to transfer
 */
static uint8_t *cs752x_nand_transfer_oob(struct nand_chip *chip, uint8_t *oob,
				  struct mtd_oob_ops *ops, size_t len)
{
	switch (ops->mode) {

	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
		memcpy(oob, chip->oob_poi + ops->ooboffs, len);
		return oob + len;

	case MTD_OPS_AUTO_OOB:{
			struct nand_oobfree *free = chip->ecc.layout->oobfree;
			uint32_t boffs = 0, roffs = ops->ooboffs;
			size_t bytes = 0;

			for (; free->length && len; free++, len -= bytes) {
				/* Read request not from offset 0 ? */
				if (unlikely(roffs)) {
					if (roffs >= free->length) {
						roffs -= free->length;
						continue;
					}
					boffs = free->offset + roffs;
					bytes = min_t(size_t, len,
						      (free->length - roffs));
					roffs = 0;
				} else {
					bytes =
					    min_t(size_t, len, free->length);
					boffs = free->offset;
				}
				memcpy(oob, chip->oob_poi + boffs, bytes);
				oob += bytes;
			}
			return oob;
		}
	default:
		BUG();
	}
	return NULL;
}

/**
 * cs752x_nand_block_isbad - [MTD Interface] Check if block at offset is bad
 * @mtd:	MTD device structure
 * @offs:	offset relative to mtd start
 */
static int cs752x_nand_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	int rc;
	/* Check for invalid offset */
	if (offs > mtd->size)
		return -EINVAL;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif

	rc = cs752x_nand_block_checkbad(mtd, offs, 1, 0);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

	return rc;
}

/**
 * cs752x_nand_block_markbad - [MTD Interface] Mark block at the given offset as bad
 * @mtd:	MTD device structure
 * @ofs:	offset relative to mtd start
 */
static int cs752x_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif
	if ((ret = cs752x_nand_block_isbad(mtd, ofs))) {
		/* If it was bad already, return success and do nothing. */
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

		if (ret > 0)
			return 0;
		return ret;
	}

	ret = chip->block_markbad(mtd, ofs);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
	return ret;
}

#if 0 /* CONFIG_PM */
static int cs752x_nand_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mfd_cell *cell = (struct mfd_cell *)dev->dev.platform_data;

	struct nand_chip *chip = cs752x_host->nand_chip;/* mtd->priv; */

	return cs752x_nand_get_device(chip, cs752x_host->mtd, FL_PM_SUSPENDED);
}

static int cs752x_nand_resume(struct platform_device *dev)
{
	struct nand_chip *chip = cs752x_host->nand_chip;/* mtd->priv; */

	if (chip->state == FL_PM_SUSPENDED)
		cs752x_nand_release_device(cs752x_host->mtd);
	else
		printk(KERN_ERR "%s called for a chip which is not "
		       "in suspended state\n", __func__);
}
#else
#define cs752x_nand_suspend NULL
#define cs752x_nand_resume NULL
#endif

/**
 * cs752x_nand_sync - [MTD Interface] sync
 * @mtd:	MTD device structure
 *
 * Sync is actually a wait for chip ready function
 */
static void cs752x_nand_sync(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	pr_debug("%s: called\n", __func__);

	/* Grab the lock and see if the device is available */
	cs752x_nand_get_device(chip, mtd, FL_SYNCING);
	/* Release it and go back */
	cs752x_nand_release_device(mtd);
}

/**
 * cs752x_nand_write_oob - [MTD Interface] NAND write data and/or out-of-band
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operation description structure
 */
static int cs752x_nand_write_oob(struct mtd_info *mtd, loff_t to,
			  struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	/* Do not allow writes past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size) {
		pr_debug("%s: Attempt write beyond "
		      "end of device\n", __func__);
		return -EINVAL;
	}
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif

	cs752x_nand_get_device(chip, mtd, FL_WRITING);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = cs752x_nand_do_write_oob(mtd, to, ops);
	else
		ret = cs752x_nand_do_write_ops(mtd, to, ops);

 out:
	cs752x_nand_release_device(mtd);
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

	return ret;
}

/**
 * cs752x_nand_do_read_oob - [Intern] NAND read out-of-band
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob operations description structure
 *
 * NAND read out-of-band data from the spare area
 */
static int cs752x_nand_do_read_oob(struct mtd_info *mtd, loff_t from,
			    struct mtd_oob_ops *ops)
{
	int page, realpage, chipnr, sndcmd = 1;
	struct nand_chip *chip = mtd->priv;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int readlen = ops->ooblen;
	int len;
	uint8_t *buf = ops->oobbuf;

	pr_debug("%s: from = 0x%08Lx, len = %i\n",
	      __func__, (unsigned long long)from, readlen);

	if (ops->mode == MTD_OPS_AUTO_OOB)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	if (unlikely(ops->ooboffs >= len)) {
		pr_debug("%s: Attempt to start read "
		      "outside oob\n", __func__);
		return -EINVAL;
	}
#if 0
	/* Do not allow read past end of page */
	if ((ops->ooboffs + ops->ooblen) > len) {
		pr_debug("%s: Attempt to read "
		      "past end of page\n", __func__);
		return -EINVAL;
	}
#endif

	/* Do not allow reads past end of device */
	if (unlikely(from >= mtd->size ||
		     ops->ooboffs + readlen > ((mtd->size >> chip->page_shift) -
					       (from >> chip->page_shift)) *
		     len)) {
		pr_debug(
		      "%s: Attempt read beyond end " "of device\n", __func__);
		return -EINVAL;
	}

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Shift to get page */
	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	while (1) {
		sndcmd = chip->ecc.read_oob(mtd, chip, page, sndcmd);

		len = min(len, readlen);
		buf = cs752x_nand_transfer_oob(chip, buf, ops, len);

		if (!(chip->options & NAND_NO_READRDY)) {
			/*
			 * Apply delay or wait for ready/busy pin. Do this
			 * before the AUTOINCR check, so no problems arise if a
			 * chip which does auto increment is marked as
			 * NOAUTOINCR by the board driver.
			 */
			if (!chip->dev_ready)
				udelay(chip->chip_delay);
			else
				cs752x_nand_wait_ready(mtd);
		}

		readlen -= len;
		if (!readlen)
			break;

		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		/* Check, if the chip supports auto page increment
		 * or if we have hit a block boundary.
		 */
		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->oobretlen = ops->ooblen;
	return 0;
}

/**
 * cs752x_nand_read_oob - [MTD Interface] NAND read data and/or out-of-band
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob operation description structure
 *
 * NAND read data and/or out-of-band data
 */
static int cs752x_nand_read_oob(struct mtd_info *mtd, loff_t from,
			 struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	/* Do not allow reads past end of device */
	if (ops->datbuf && (from + ops->len) > mtd->size) {
		pr_debug("%s: Attempt read "
		      "beyond end of device\n", __func__);
		return -EINVAL;
	}
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif

	cs752x_nand_get_device(chip, mtd, FL_READING);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = cs752x_nand_do_read_oob(mtd, from, ops);
	else
		ret = cs752x_nand_do_read_ops(mtd, from, ops);

 out:
	cs752x_nand_release_device(mtd);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
	return ret;
}



/**
 * cs752x_nand_erase_block - [GENERIC] erase a block
 * @mtd:	MTD device structure
 * @page:	page address
 *
 * Erase a block.
 */

static int cs752x_nand_erase_block(struct mtd_info *mtd, int page)
{
/* 	int opcode,tst=0,tst1=0,tst2=0; */
	struct nand_chip *this = mtd->priv;
	u64 test;
	unsigned long	timeo ;

	check_flash_ctrl_status();

	/* Send commands to erase a page */
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0);	/*  */

	nf_cnt.wrd = 0;
	nf_cmd.wrd = 0;
	nf_addr1.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = NCNT_EMPTY_OOB;
	nf_cnt.bf.nflashRegDataCount = NCNT_EMPTY_DATA;
	nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;

	/*
	   test = this->chipsize;
	   test = test / mtd->writesize;
	   if((this->chipsize/mtd->writesize) > 0x10000)
	 */

	test = 0x10000 * mtd->writesize;
	if (this->chipsize > test) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_3;
	} else {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_2;
	}

	nf_cmd.bf.nflashRegCmd0 = NAND_CMD_ERASE1;
	nf_cmd.bf.nflashRegCmd1 = NAND_CMD_ERASE2;
	nf_addr1.wrd = page;
	nf_addr2.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);

	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = CHIP_EN;
	/* nf_access.bf.nflashDirWr = ; */
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;

	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);
	flash_start.wrd = 0;
	flash_start.bf.nflashRegReq = FLASH_GO;
	flash_start.bf.nflashRegCmd = FLASH_RD;	/* no data access use read.. */
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);
	
	timeo = jiffies + HZ;
	do {
		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		if(!flash_start.bf.nflashRegReq)
			return 0;
	} while (time_before(jiffies, timeo));
	
	printk("%s: %x\n", __func__,flash_start.wrd);
	
	return 0;
}

/**
 * cs752x_nand_block_checkbad - [GENERIC] Check if a block is marked bad
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 * @getchip:	0, if the chip is already selected
 * @allowbbt:	1, if its allowed to access the bbt area
 *
 * Check, if the block is bad. Either by reading the bad block table or
 * calling of the scan function.
 */
static int cs752x_nand_block_checkbad(struct mtd_info *mtd, loff_t ofs, int getchip,
			       int allowbbt)
{
	struct nand_chip *chip = mtd->priv;

	if (!chip->bbt)
		return chip->block_bad(mtd, ofs, getchip);

	/* Return info from the table */
	return nand_isbad_bbt(mtd, ofs, allowbbt);
}

/**
 * cs752x_nand_erase - [MTD Interface] erase block(s)
 * @mtd:	MTD device structure
 * @instr:	erase instruction
 *
 * Erase one ore more blocks
 */
static int cs752x_nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int rc;
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif

	rc = cs752x_nand_erase_nand(mtd, instr, 0);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
	return rc;

}

#define BBT_PAGE_MASK	0xffffff3f
/**
 * cs752x_nand_erase_nand - [Internal] erase block(s)
 * @mtd:	MTD device structure
 * @instr:	erase instruction
 * @allowbbt:	allow erasing the bbt area
 *
 * Erase one ore more blocks
 */
int cs752x_nand_erase_nand(struct mtd_info *mtd, struct erase_info *instr,
		    int allowbbt)
{
	int page, status, pages_per_block, ret, chipnr;
	struct nand_chip *chip = mtd->priv;
	loff_t rewrite_bbt[NAND_MAX_CHIPS] = { 0 };
	unsigned int bbt_masked_page = 0xffffffff;
	loff_t len;
	unsigned long flags;

	pr_debug("%s: start = 0x%012llx, len = %llu\n",
	      __func__, (unsigned long long)instr->addr,
	      (unsigned long long)instr->len);

	/* Start address must align on block boundary */
	if (instr->addr & ((1 << chip->phys_erase_shift) - 1)) {
		pr_debug("%s: Unaligned address\n", __func__);
		printk(KERN_ALERT "unaligned_chipptr!!!");
		return -EINVAL;
	}

	/* Length must align on block boundary */
	if (instr->len & ((1 << chip->phys_erase_shift) - 1)) {
		pr_debug("%s: Length not block aligned\n",
		      __func__);
		return -EINVAL;
	}

	/* Do not allow erase past end of device */
	if ((instr->len + instr->addr) > mtd->size) {
		pr_debug("%s: Erase past end of device\n",
		      __func__);
		return -EINVAL;
	}

	instr->fail_addr = MTD_FAIL_ADDR_UNKNOWN;

	/* Grab the lock and see if the device is available */
	cs752x_nand_get_device(chip, mtd, FL_ERASING);

	/* Shift to get first page */
	page = (int)(instr->addr >> chip->page_shift);
	chipnr = (int)(instr->addr >> chip->chip_shift);

	/* Calculate pages in each block */
	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);

	/* Select the NAND device */
	chip->select_chip(mtd, chipnr);

	/* Check, if it is write protected */
	if (cs752x_nand_check_wp(mtd)) {
		pr_debug("%s: Device is write protected!!!\n",
		      __func__);
		instr->state = MTD_ERASE_FAILED;
		goto erase_exit;
	}

	/*
	 * If BBT requires refresh, set the BBT page mask to see if the BBT
	 * should be rewritten. Otherwise the mask is set to 0xffffffff which
	 * can not be matched. This is also done when the bbt is actually
	 * erased to avoid recusrsive updates
	 */
	if (chip->options & BBT_AUTO_REFRESH && !allowbbt)
		bbt_masked_page = chip->bbt_td->pages[chipnr] & BBT_PAGE_MASK;

	/* Loop through the pages */
	len = instr->len;

	instr->state = MTD_ERASING;

	while (len) {
		/*
		 * heck if we have a bad block, we do not erase bad blocks !
		 */
		if (cs752x_nand_block_checkbad(mtd, ((loff_t) page) <<
					       chip->page_shift, 0, allowbbt)) {
			printk(KERN_WARNING "%s: attempt to erase a bad block "
			       "at page 0x%08x\n", __func__, page);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}

		/*
		 * Invalidate the page cache, if we erase the block which
		 * contains the current cached page
		 */
		if (page <= chip->pagebuf && chip->pagebuf <
		    (page + pages_per_block))
			chip->pagebuf = -1;

		/* chip->erase_cmd(mtd, page & chip->pagemask); */
		cs752x_nand_erase_block(mtd, page);

		status = chip->waitfunc(mtd, chip);

		/*
		 * See if operation failed and additional status checks are
		 * available
		 */
		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd, chip, FL_ERASING,
					       status, page);

		/* See if block erase succeeded */
		if (status & NAND_STATUS_FAIL) {
			pr_debug("%s: Failed erase, "
			      "page 0x%08x\n", __func__, page);
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = ((loff_t) page << chip->page_shift);
			goto erase_exit;
		}

		/*
		 * If BBT requires refresh, set the BBT rewrite flag to the
		 * page being erased
		 */
		if (bbt_masked_page != 0xffffffff &&
		    (page & BBT_PAGE_MASK) == bbt_masked_page)
			rewrite_bbt[chipnr] =
			    ((loff_t) page << chip->page_shift);

		/* Increment page address and decrement length */
		len -= (1 << chip->phys_erase_shift);
		page += pages_per_block;

		/* Check, if we cross a chip boundary */
		if (len && !(page & chip->pagemask)) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);

			/*
			 * If BBT requires refresh and BBT-PERCHIP, set the BBT
			 * page mask to see if this BBT should be rewritten
			 */
			if (bbt_masked_page != 0xffffffff &&
			    (chip->bbt_td->options & NAND_BBT_PERCHIP))
				bbt_masked_page = chip->bbt_td->pages[chipnr] &
				    BBT_PAGE_MASK;
		}
	}
	instr->state = MTD_ERASE_DONE;

 erase_exit:

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;

	/* Deselect and wake up anyone waiting on the device */
	cs752x_nand_release_device(mtd);

	/* Do call back function */
	if (!ret)
		mtd_erase_callback(instr);

	/*
	 * If BBT requires refresh and erase was successful, rewrite any
	 * selected bad block tables
	 */
	if (bbt_masked_page == 0xffffffff || ret)
		return ret;

	for (chipnr = 0; chipnr < chip->numchips; chipnr++) {
		if (!rewrite_bbt[chipnr])
			continue;
		/* update the BBT for chip */
		pr_debug("%s: nand_update_bbt "
		      "(%d:0x%0llx 0x%0x)\n", __func__, chipnr,
		      rewrite_bbt[chipnr], chip->bbt_td->pages[chipnr]);
		nand_update_bbt(mtd, rewrite_bbt[chipnr]);
	}

	/* Return more or less happy */
	return ret;
}

#define NOTALIGNED(x)	(x & (chip->subpagesize - 1)) != 0
/**
 * cs752x_nand_do_write_ops - [Internal] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operations description structure
 *
 * NAND write with ECC
 */
static int cs752x_nand_do_write_ops(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	int chipnr, realpage, page, blockmask, column;
	struct nand_chip *chip = mtd->priv;
	uint32_t writelen = ops->len;

	uint32_t oobwritelen = ops->ooblen;
	uint32_t oobmaxlen = ops->mode == MTD_OPS_AUTO_OOB ?
				mtd->oobavail : mtd->oobsize;

	uint8_t *oob = ops->oobbuf;
	uint8_t *buf = ops->datbuf;
	int ret, subpage;

	ops->retlen = 0;
	if (!writelen)
		return 0;

	/* reject writes, which are not page aligned */
	if (NOTALIGNED(to) || NOTALIGNED(ops->len)) {
		printk(KERN_NOTICE "%s: Attempt to write not "
		       "page aligned data\n", __func__);
		return -EINVAL;
	}

	column = to & (mtd->writesize - 1);
	subpage = column || (writelen & (mtd->writesize - 1));

	if (subpage && oob) {
		return -EINVAL;
	}

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Check, if it is write protected */
	if (cs752x_nand_check_wp(mtd))
		return -EIO;

	realpage = (int)(to >> chip->page_shift);
	page = realpage & chip->pagemask;
	blockmask = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;

	/* Invalidate the page cache, when we write to the cached page */
	if (to <= (chip->pagebuf << chip->page_shift) &&
	    (chip->pagebuf << chip->page_shift) < (to + ops->len))
		chip->pagebuf = -1;

	/* Don't allow multipage oob writes with offset */
	if (oob && ops->ooboffs && (ops->ooboffs + ops->ooblen > oobmaxlen))
		return -EINVAL;

	while (1) {
		int bytes = mtd->writesize;
		int cached = writelen > bytes && page != blockmask;
		uint8_t *wbuf = buf;

		/* Partial page write ? */
		if (unlikely(column || writelen < (mtd->writesize - 1))) {
			cached = 0;
			bytes = min_t(int, bytes - column, (int)writelen);
			chip->pagebuf = -1;
			/* chip->ecc.read_page(mtd, chip, chip->buffers->databuf, (page<<chip->page_shift)); */
			chip->ecc.read_page(mtd, chip, chip->buffers->databuf,
					    page);
			memcpy(&chip->buffers->databuf[column], buf, bytes);
			wbuf = chip->buffers->databuf;
		}

		if (unlikely(oob)) {

			size_t len = min(oobwritelen, oobmaxlen);
			oob = cs752x_nand_fill_oob(mtd, oob, len, ops);
			oobwritelen -= len;
		} else {
			/* We still need to erase leftover OOB data */
			memset(chip->oob_poi, 0xff, mtd->oobsize);
		}


		ret = chip->write_page(mtd, chip, wbuf, page, cached,
				       (ops->mode == MTD_OPS_RAW));
		if (ret)
			break;

		writelen -= bytes;
		if (!writelen)
			break;

		column = 0;
		buf += bytes;
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}
	}

	ops->retlen = ops->len - writelen;
	if (unlikely(oob))
		ops->oobretlen = ops->ooblen;
	return ret;
}

/**
 * cs752x_nand_write - [MTD Interface] NAND write with ECC
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @len:	number of bytes to write
 * @retlen:	pointer to variable to store the number of written bytes
 * @buf:	the data to write
 *
 * NAND write with ECC
 */
static int cs752x_nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			  size_t *retlen, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	int ret;

	/* Do not allow reads past end of device */
	if ((to + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif

	memset(&ops, 0, sizeof(ops));
	cs752x_nand_get_device(chip, mtd, FL_WRITING);

	ops.len = len;
	ops.datbuf = (uint8_t *) buf;
	ops.oobbuf = NULL;
	//ops.mode = 0;

	ret = cs752x_nand_do_write_ops(mtd, to, &ops);

	*retlen = ops.retlen;

	cs752x_nand_release_device(mtd);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

	return ret;
}

/**
 * cs752x_nand_read_subpage - [REPLACABLE] software ecc based sub-page read function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @data_offs:	offset of requested data within the page
 * @readlen:	data length
 * @bufpoi:	buffer to store read data
 */
static int cs752x_nand_read_subpage(struct mtd_info *mtd,
				    struct nand_chip *chip, uint32_t data_offs,
				    uint32_t readlen, uint8_t * bufpoi)
{
	int start_step, end_step, num_steps;
	/* uint32_t *eccpos = chip->ecc.layout->eccpos, page; */
	/* uint8_t *p; */
	/* struct nand_chip *this = mtd->priv; */
	/* int data_col_addr, i, gaps = 0; */
	int datafrag_len;
	/* int busw = (chip->options & NAND_BUSWIDTH_16) ? 2 : 1; */
	uint32_t page;

	/* Column address wihin the page aligned to ECC size (256bytes). */
	start_step = data_offs / chip->ecc.size;
	end_step = (data_offs + readlen - 1) / chip->ecc.size;
	num_steps = end_step - start_step + 1;

	/* Data size aligned to ECC ecc.size */
	datafrag_len = num_steps * chip->ecc.size;

	page = nand_page;
	chip->pagebuf = -1;
	/* chip->ecc.read_page(mtd, chip, chip->buffers->databuf, (page<<this->page_shift)); */
	chip->ecc.read_page(mtd, chip, chip->buffers->databuf, page);

	memcpy(bufpoi, chip->buffers->databuf + data_offs, datafrag_len);

	return 0;
}


/**
 * cs752x_nand_do_read_ops - [Internal] Read data with ECC
 *
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @ops:	oob ops structure
 *
 * Internal function. Called with chip held.
 */
static int cs752x_nand_do_read_ops(struct mtd_info *mtd, loff_t from,
			    struct mtd_oob_ops *ops)
{
	int chipnr, page, realpage, col, bytes, aligned;
	struct nand_chip *chip = mtd->priv;
	struct mtd_ecc_stats stats;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int sndcmd = 1;
	int ret = 0;
	uint32_t readlen = ops->len;
	uint32_t oobreadlen = ops->ooblen;
	uint8_t *bufpoi, *oob, *buf;

	stats = mtd->ecc_stats;

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	col = (int)(from & (mtd->writesize - 1));

	buf = ops->datbuf;
	oob = ops->oobbuf;

	while (1) {
		bytes = min(mtd->writesize - col, readlen);
		aligned = (bytes == mtd->writesize);

		/* Is the current page in the buffer ? */
		if (realpage != chip->pagebuf || oob) {
			bufpoi = aligned ? buf : chip->buffers->databuf;

			if (likely(sndcmd)) {
				chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
				sndcmd = 0;
			}

			/* Now read the page into the buffer */
			if (unlikely(ops->mode == MTD_OPS_RAW))
				ret = chip->ecc.read_page_raw(mtd, chip,
							      bufpoi, page);
			else if (!aligned && NAND_SUBPAGE_READ(chip) && !oob)
				ret =
				    chip->ecc.read_subpage(mtd, chip, col,
							   bytes, bufpoi);
			else {
				ret =
				    chip->ecc.read_page(mtd, chip, bufpoi,
							page);
			}
			if (ret < 0)
				break;

			/* Transfer not aligned data */
			if (!aligned) {
				if (!NAND_SUBPAGE_READ(chip) && !oob)
					chip->pagebuf = realpage;
				memcpy(buf, chip->buffers->databuf + col,
				       bytes);
			}

			buf += bytes;

			if (unlikely(oob)) {
				/* Raw mode does data:oob:data:oob */
				if (ops->mode != MTD_OPS_RAW) {
					int toread = min(oobreadlen,
							 chip->ecc.layout->
							 oobavail);
					if (toread) {
						oob =
						    cs752x_nand_transfer_oob
						    (chip, oob, ops, toread);
						oobreadlen -= toread;
					}
				} else
					buf =
					    cs752x_nand_transfer_oob(chip, buf,
								     ops,
								     mtd->
								     oobsize);
			}

			if (!(chip->options & NAND_NO_READRDY)) {
				/*
				 * Apply delay or wait for ready/busy pin. Do
				 * this before the AUTOINCR check, so no
				 * problems arise if a chip which does auto
				 * increment is marked as NOAUTOINCR by the
				 * board driver.
				 */
				if (!chip->dev_ready)
					udelay(chip->chip_delay);
				else
					cs752x_nand_wait_ready(mtd);
			}
		} else {
			memcpy(buf, chip->buffers->databuf + col, bytes);
			buf += bytes;
		}

		readlen -= bytes;

		if (!readlen)
			break;

		/* For subsequent reads align to page boundary. */
		col = 0;
		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		/* Check, if the chip supports auto page increment
		 * or if we have hit a block boundary.
		 */
		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->retlen = ops->len - (size_t) readlen;
	if (oob)
		ops->oobretlen = ops->ooblen - oobreadlen;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}


/**
 * cs752x_nand_read - [MTD Interface] MTD compability function for nand_do_read_ecc
 * @mtd:	MTD device structure
 * @from:	offset to read from
 * @len:	number of bytes to read
 * @retlen:	pointer to variable to store the number of read bytes
 * @buf:	the databuffer to put data
 *
 * Get hold of the chip and call nand_do_read
 */
static int cs752x_nand_read(struct mtd_info *mtd, loff_t from, size_t len,
		     size_t *retlen, uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	int ret;

	/* Do not allow reads past end of device */
	if ((from + len) > mtd->size)
		return -EINVAL;
	if (!len)
		return 0;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);

#endif

	cs752x_nand_get_device(chip, mtd, FL_READING);

	memset(&ops, 0, sizeof(ops));
	ops.len = len;
	ops.datbuf = buf;
	ops.oobbuf = NULL;
	//ops.mode = 0;

	ret = cs752x_nand_do_read_ops(mtd, from, &ops);

	*retlen = ops.retlen;

	cs752x_nand_release_device(mtd);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);

#endif

	return ret;
}


/**
 * cs752x_hw_nand_correct_data - [NAND Interface] Detect and correct bit error(s)
 * @mtd:	MTD block structure
 * @buf:	raw data read from the chip
 * @read_ecc:	ECC from the chip
 * @calc_ecc:	the ECC calculated from raw data
 *
 * Detect and correct a 1 bit error for 256/512 byte block
 */
int cs752x_hw_nand_correct_data(struct mtd_info *mtd, unsigned char *buf,
		      unsigned char *read_ecc, unsigned char *calc_ecc)
{
	/* unsigned char b0, b1, b2, bit_addr; */
	/* unsigned int byte_addr; */
	/* struct nand_chip *chip = mtd->priv; */
	/* 256 or 512 bytes/ecc  */
	/* const uint32_t eccsize_mult = (chip->ecc.size) >> 8;//eccsize >> 8; */
	/* int eccsize = chip->ecc.size */
	/*
	 * b0 to b2 indicate which bit is faulty (if any)
	 * we might need the xor result  more than once,
	 * so keep them in a local var
	 */
	/* middle not yet */
	return 1;

}


/**
 * cs752x_hw_nand_calculate_ecc - [NAND Interface] Calculate 3-byte ECC for 256/512-byte
 *			 block
 * @mtd:	MTD block structure
 * @buf:	input buffer with raw data
 * @code:	output buffer with ECC
 */
int cs752x_hw_nand_calculate_ecc(struct mtd_info *mtd, const unsigned char *buf,
		       unsigned char *code)
{
	/* int i; */
	/* const uint32_t *bp = (uint32_t *)buf; */
	/* 256 or 512 bytes/ecc  */
	/* const uint32_t eccsize_mult = */
	/* 		(((struct nand_chip *)mtd->priv)->ecc.size) >> 8; */
	/* uint32_t cur;		 current value in buffer  */
	/* rp0..rp15..rp17 are the various accumulated parities (per byte) */
	/* middle not yet */

	return 0;
}

/**
 * cs752x_nand_write_oob_std - [REPLACABLE] the most common OOB data write function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @page:	page number to write
 */
static int cs752x_nand_write_oob_std(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{
	int status = 0, i;
	/* const uint8_t *buf = chip->oob_poi; */
	/* int length = mtd->oobsize; */

	check_flash_ctrl_status();

#if 0
	int old_sts;
	old_sts = read_flash_ctrl_reg(FLASH_STATUS);
	if ((old_sts & 0xffff) != 0)
		printk("%s: --> old_sts : %x     \n", __func__, old_sts);
#endif
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);
	/* chip->write_buf(mtd, buf, length); */
	/* Send command to program the OOB data */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);	/* disable ecc gen */

	nf_cmd.wrd = 0;
	nf_addr1.wrd = 0;
	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = mtd->oobsize - 1;
	nf_cnt.bf.nflashRegDataCount = NCNT_EMPTY_DATA;

	nf_addr2.wrd = 0;

	if (chip->chipsize < SZ_32M) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_3;

		if (mtd->writesize > NCNT_512P_DATA) {
			nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
			nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
			nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		} else {
			nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_3;
			nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READOOB;	/* ?? */
			nf_cmd.bf.nflashRegCmd1 = NAND_CMD_SEQIN;
			nf_cmd.bf.nflashRegCmd2 = NAND_CMD_PAGEPROG;
		}
		/* read oob need to add page data size to match correct oob ddress */
		nf_addr1.wrd = (((page & 0x00ffffff) << 8));
		nf_addr2.wrd = ((page & 0xff000000) >> 24);
	} else if (chip->chipsize <= SZ_128M) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_4;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd =
		    (((page & 0xffff) << 16) + (mtd->writesize & 0xffff));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);

	} else {		/* if((chip->chipsize > (128 << 20)) )) */

		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_5;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd =
		    (((page & 0xffff) << 16) + (mtd->writesize & 0xffff));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);

	}

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x0 */
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);

	pwrite = (unsigned int *)chip->oob_poi;

	for (i = 0; i < ((mtd->oobsize / 4)); i++) {
		nf_access.wrd = 0;
		nf_access.bf.nflashCeAlt = CHIP_EN;
		/* nf_access.bf.nflashDirWr = ; */
		nf_access.bf.nflashRegWidth = NFLASH_WiDTH32;
		write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

		write_flash_ctrl_reg(FLASH_NF_DATA, pwrite[i]);

		flash_start.wrd = 0;
		flash_start.bf.nflashRegReq = FLASH_GO;
		flash_start.bf.nflashRegCmd = FLASH_WT;
		/* flash_start.bf.nflash_random_access = RND_ENABLE; */
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		while (flash_start.bf.nflashRegReq) {
			udelay(1);
			schedule();
			flash_start.wrd =
			    read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		}
		/* pwrite[i] = read_flash_ctrl_reg(FLASH_NF_DATA); */

	}
#if 0
	unsigned int rtmp[80];
	nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;
	nf_cmd.bf.nflashRegCmd1 = NAND_CMD_READSTART;
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */

	for (i = 0; i < ((mtd->oobsize / 4)); i++) {
		nf_access.wrd = 0;
		nf_access.bf.nflashCeAlt = CHIP_EN;
		/* nf_access.bf.nflashDirWr = ; */
		nf_access.bf.nflashRegWidth = NFLASH_WiDTH32;
		write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

		flash_start.wrd = 0;
		flash_start.bf.nflashRegReq = FLASH_GO;
		flash_start.bf.nflashRegCmd = FLASH_RD;
		/* flash_start.bf.nflash_random_access = RND_ENABLE; */
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		while (flash_start.bf.nflashRegReq) {
			udelay(1);
			schedule();
			flash_start.wrd =
			    read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		}
		rtmp[i] = read_flash_ctrl_reg(FLASH_NF_DATA);

	}
	if (memcmp
	    ((unsigned char *)pwrite, (unsigned char *)rtmp, mtd->oobsize))
		printk("W->R oob error\n");
#endif
#if 0
	int tnp_sts, tnp_sts1;
	check_flash_ctrl_status();
	old_sts = read_flash_ctrl_reg(FLASH_STATUS);
	if ((old_sts & 0xffff) != 0) {
		tnp_sts = read_flash_ctrl_reg(FLASH_STATUS);
		printk("%s:old_sts : %x     ", __func__, old_sts);
		tnp_sts1 = read_flash_ctrl_reg(FLASH_STATUS);
		printk("-->  tnp_sts : %x   %x  \n", tnp_sts, tnp_sts1);
	}
#endif

	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;

}


/**
 * cs752x_nand_read_oob_std - [REPLACABLE] the most common OOB data read function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @page:	page number to read
 * @sndcmd:	flag whether to issue read command or not
 */
static int cs752x_nand_read_oob_std(struct mtd_info *mtd, struct nand_chip *chip,
			     int page, int sndcmd)
{
	int i;

	check_flash_ctrl_status();
	/* if (sndcmd) { */
	/*      chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page); */
	/*      sndcmd = 0; */
	/* } */

	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);	/* disable ecc gen */

	nf_cmd.wrd = 0;
	nf_addr1.wrd = 0;
	nf_addr2.wrd = 0;
	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = mtd->oobsize - 1;
	nf_cnt.bf.nflashRegDataCount = NCNT_EMPTY_DATA;

	if (chip->chipsize < (32 << 20)) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_3;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
		if (mtd->writesize > NCNT_512P_DATA)
			nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;
		else
			nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READOOB;

		nf_addr1.wrd = ((page & 0x00ffffff) << 8);
		nf_addr2.wrd = ((page & 0xff000000) >> 24);
	} else if ((chip->chipsize >= (32 << 20))
		   && (chip->chipsize <= (128 << 20))) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_4;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;

		/*  Jeneng */
		if (mtd->writesize > NCNT_512P_DATA) {
			nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
			nf_cmd.bf.nflashRegCmd1 = NAND_CMD_READSTART;
		}
		nf_addr1.wrd =
		    (((page & 0xffff) << 16) + (mtd->writesize & 0xffff));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);

	} else {		/* if((chip->chipsize > (128 << 20)) )) */

		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_5;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_READSTART;
		nf_addr1.wrd =
		    (((page & 0xffff) << 16) + (mtd->writesize & 0xffff));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);
	}

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x0 */
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);

	pread = (unsigned int *)chip->oob_poi;

	for (i = 0; i < mtd->oobsize / 4; i++) {
		nf_access.wrd = 0;
		nf_access.bf.nflashCeAlt = CHIP_EN;
		/* nf_access.bf.nflashDirWr = ; */
		nf_access.bf.nflashRegWidth = NFLASH_WiDTH32;
		write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

		flash_start.wrd = 0;
		flash_start.bf.nflashRegReq = FLASH_GO;
		flash_start.bf.nflashRegCmd = FLASH_RD;
		/* flash_start.bf.nflash_random_access = RND_ENABLE; */
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		while (flash_start.bf.nflashRegReq) {
			udelay(1);
			schedule();
			flash_start.wrd =
			    read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		}
		pread[i] = read_flash_ctrl_reg(FLASH_NF_DATA);

	}

	return sndcmd;
}

static void reset_ecc_bch_registers( void )
{
	ecc_reset.wrd = 3;
	ecc_reset.bf.eccClear = ECC_CLR;
	ecc_reset.bf.fifoClear = FIFO_CLR;
	write_flash_ctrl_reg(FLASH_NF_ECC_RESET, ecc_reset.wrd);

	flash_int_sts.bf.regIrq = 1;
	write_flash_ctrl_reg(FLASH_FLASH_INTERRUPT, flash_int_sts.wrd);

	ecc_reset.wrd = 0;
	ecc_reset.bf.eccClear = 1;
	write_flash_ctrl_reg(FLASH_NF_ECC_RESET, ecc_reset.wrd);

	/*  Disable ECC function */
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, 0);
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0);

}


/**
 * cs752x_nand_write_page_raw - [Intern] raw page write function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	data buffer
 *
 * Not for syndrome calculating ecc controllers, which use a special oob layout
 */
static void cs752x_nand_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf)
{
	int page;
	uint32_t addr;


	check_flash_ctrl_status();

	page = nand_page;

	reset_ecc_bch_registers();

#ifndef NAND_DIRECT_ACCESS

	/* disable txq5 */
	dma_txq5_ctrl.bf.txq5_en = 0;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_CONTROL, dma_txq5_ctrl.wrd);
	/* clr tx/rx eof */

	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);


	nf_cnt.wrd = 0;
	nf_cmd.wrd = 0;
	nf_addr1.wrd = 0;
	nf_addr2.wrd = 0;
	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = mtd->oobsize - 1;
	nf_cnt.bf.nflashRegDataCount = mtd->writesize - 1;

	if (chip->chipsize < (32 << 20)) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_3;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd = (((page & 0x00ffffff) << 8));
		nf_addr2.wrd = ((page & 0xff000000) >> 24);

	} else if ((chip->chipsize >= (32 << 20))
		   && (chip->chipsize <= (128 << 20))) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_4;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd = (((page & 0xffff) << 16));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);

	} else {		/* if((chip->chipsize > (128 << 20)) )) */

		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_5;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd = (((page & 0xffff) << 16));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);
	}

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);

	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = CHIP_EN;
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;
	/* write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd); */

	/* write */
	/* prepare dma descriptor */
	/* chip->buffers->databuf */
	/* nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START); */
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = ((page << chip->page_shift) % SZ_128M) + GOLDENGATE_FLASH_BASE;

	/* The fifo depth is 64 bytes. We have a sync at each frame and frame
	 * length is 64 bytes. --> for vmalloc not kmalloc
	 */
	uint8_t *vaddr;
	vaddr = 0;
	if (buf >= high_memory) {
		struct page *p1;

		if (((size_t)buf & PAGE_MASK) !=
			((size_t)(buf + mtd->writesize - 1) & PAGE_MASK))
			goto out_copy;
		p1 = vmalloc_to_page(buf);
		if (!p1)
			goto out_copy;
		buf = page_address(p1) + ((size_t)buf & ~PAGE_MASK);

	}
	goto out_copy_done;
out_copy:
	vaddr = buf;
	chip->pagebuf = -1;
	buf = chip->buffers->databuf;
	memcpy(buf, vaddr, mtd->writesize);
out_copy_done:

	/* page data tx desc */
	dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR);
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.own = OWN_DMA;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.buf_size = mtd->writesize;
	tx_desc[dma_txq5_wptr.bf.index].buf_adr =
	    dma_map_single(NULL, (void *)buf, mtd->writesize, DMA_TO_DEVICE);

#ifdef	NAND_ECC_TEST
	memset(tw, 0, NAND_MAX_PAGESIZE);
	memcpy(tw, buf, mtd->writesize);
#endif

	/* page data rx desc */
	/* printk("cs752x_nand_write_page_hwecc : addr : %p  buf: %p",addr, buf); */

	dma_rxq5_rptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR);
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.own = OWN_DMA;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.buf_size = mtd->writesize;
	rx_desc[dma_rxq5_rptr.bf.index].buf_adr = addr;

	/* oob rx desc */
	addr = (unsigned int *)((unsigned int)addr + mtd->writesize);
	/* printk("  oob : addr(%p)  chip->oob_poi(%p) \n",addr, chip->oob_poi); */

	dma_rxq5_rptr.bf.index = (dma_rxq5_rptr.bf.index + 1) % FDMA_DESC_NUM;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.own = OWN_DMA;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.buf_size = mtd->oobsize;
	rx_desc[dma_rxq5_rptr.bf.index].buf_adr = addr;

	wmb();
	dummy = rx_desc[dma_rxq5_rptr.bf.index].word0.wrd;

	/* update page tx write ptr */
	dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR, dma_txq5_wptr.wrd);
	/* set axi_bus_len = 8 */
	/* set fifo control */
	fifo_ctl.wrd = 0;
	fifo_ctl.bf.fifoCmd = FLASH_WT;
	write_flash_ctrl_reg(FLASH_NF_FIFO_CONTROL, fifo_ctl.wrd);

	flash_start.wrd = 0;
	flash_start.bf.fifoReq = FLASH_GO;
	/* flash_start.bf.nflashRegCmd = FLASH_WT; */
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

	/* enable txq5 */
	dma_txq5_ctrl.bf.txq5_en = 1;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_CONTROL, dma_txq5_ctrl.wrd);

#else

#ifdef	NAND_ECC_TEST
	memset(tw, 0, NAND_MAX_PAGESIZE);
	memcpy(tw, buf, mtd->writesize);
#endif
	/* direct access nand */
	unsigned int *pwrt;
	unsigned int i;
	pwrt = (unsigned int *)buf;

	/* addr = (unsigned int *)((page << chip->page_shift) + cs752x_host->io_base ); */
	nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (unsigned int *)((page << chip->page_shift) % SZ_128M);
	addr =
	    (unsigned int *)((unsigned int)addr +
			     (unsigned int)(cs752x_host->io_base));
	/* printk("cs752x_nand_write_page_hwecc : addr : %p  pwrt: %p",addr, pwrt); */
	for (i = 0; i < ((mtd->writesize) / 4); i++) {
		*((unsigned int *)addr + i) = *(pwrt + i);
	}
#endif

#ifndef NAND_DIRECT_ACCESS

	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	while (!dma_ssp_rxq5_intsts.bf.rxq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_rxq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	}
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	while (!dma_ssp_txq5_intsts.bf.txq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_txq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	}
	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

#ifdef	CONFIG_CS752X_NAND_ECC_HW_BCH
	/* jenfeng clear erase tag */
	*(chip->oob_poi + chip->ecc.layout->oobfree[0].offset +
	  chip->ecc.layout->oobfree[0].length) = 0;
#endif

	/* dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR); */
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.own = OWN_DMA;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.buf_size = mtd->oobsize;
	tx_desc[dma_txq5_wptr.bf.index].buf_adr =
	    dma_map_single(NULL, (void *)chip->oob_poi, mtd->oobsize,
			   DMA_TO_DEVICE);

	wmb();
	dummy = tx_desc[dma_txq5_wptr.bf.index].word0.wrd;

	/* update tx write ptr */
	dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR, dma_txq5_wptr.wrd);

	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	while (!dma_ssp_rxq5_intsts.bf.rxq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_rxq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	}
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	while (!dma_ssp_txq5_intsts.bf.txq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_txq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	}

	flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	while (flash_start.bf.fifoReq) {
		udelay(1);
		schedule();
		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	}

	/* update rx read ptr */
	/* dma_rxq5_rptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR); */
	dma_rxq5_rptr.bf.index = (dma_rxq5_rptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR, dma_rxq5_rptr.wrd);

	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

	/* The fifo depth is 64 bytes. We have a sync at each frame and frame
	 * length is 64 bytes. --> for vmalloc not kmalloc
	 */
	if(vaddr!=0)
	{
		buf = vaddr;
		vaddr = 0;
	}

	/* /////////////// */
	/* chip->write_buf(mtd, chip->oob_poi, mtd->oobsize); */
#else
	/* direct access nand */
	pwrt = (unsigned int *)chip->oob_poi;
	/* addr = (unsigned int *)(((page << chip->page_shift) + mtd->writesize) + cs752x_host->io_base ); */
	nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (unsigned int *)((page << chip->page_shift) % SZ_128M);
	addr =
	    (unsigned int *)((unsigned int)addr + mtd->writesize +
			     (unsigned int)(cs752x_host->io_base));
	/* printk("  oob : addr(%p)  pwrt(%p) \n",addr, pwrt); */
	unsigned int tmp_addr;
	tmp_addr = (unsigned int)addr;
	if (tmp_addr == 0x91000000)
		printk("%s : out of range  page(%x)\n ", __func__, page);

	if (((unsigned int)addr) ==
	    ((unsigned int)(cs752x_host->io_base) + SZ_128M)) {
		cs752x_nand_write_oob_std(mtd, chip, page);
	} else {
		for (i = 0; i < (mtd->oobsize / 4); i++) {
			*((unsigned int *)addr + i) = *(pwrt + i);
		}
	}

#endif

}


static int cs752x_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int page)
{
	unsigned int addr;
	

#ifndef NAND_DIRECT_ACCESS

	/* disable txq5 */

	dma_txq5_ctrl.bf.txq5_en = 0;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_CONTROL, dma_txq5_ctrl.wrd);

	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

	/* for indirect access with DMA, because DMA not ready  */
	nf_cnt.wrd = 0;
	nf_cmd.wrd = 0;
	nf_addr1.wrd = 0;
	nf_addr2.wrd = 0;
	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = mtd->oobsize - 1;
	nf_cnt.bf.nflashRegDataCount = mtd->writesize - 1;

	if (chip->chipsize < SZ_32M) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_3;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;
		nf_addr1.wrd = (((page & 0x00ffffff) << 8));
		nf_addr2.wrd = ((page & 0xff000000) >> 24);
	} else if (chip->chipsize <= SZ_128M) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_4;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;

		if (mtd->writesize > NCNT_512P_DATA) {
			nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
			nf_cmd.bf.nflashRegCmd1 = NAND_CMD_READSTART;
		}
		nf_addr1.wrd = (((page & 0xffff) << 16));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);
	} else {		/* if((chip->chipsize > SZ_128M ) )) */

		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_5;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READ0;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_READSTART;
		nf_addr1.wrd = (((page & 0xffff) << 16));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);
	}

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x0 */
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);

	nf_cnt.wrd = read_flash_ctrl_reg(FLASH_NF_COUNT);
	nf_cmd.wrd = read_flash_ctrl_reg(FLASH_NF_COMMAND);
	nf_addr1.wrd = read_flash_ctrl_reg(FLASH_NF_ADDRESS_1);
	nf_addr2.wrd = read_flash_ctrl_reg(FLASH_NF_ADDRESS_2);

	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = CHIP_EN;
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	nf_access.wrd = read_flash_ctrl_reg(FLASH_NF_ACCESS);

	addr = (page << chip->page_shift) % SZ_128M + GOLDENGATE_FLASH_BASE;

	/* The fifo depth is 64 bytes. We have a sync at each frame and frame
	 * length is 64 bytes. --> for vmalloc not kmalloc
	 */
	uint8_t *vaddr;
	vaddr = 0;
	if (buf >= high_memory) {
		struct page *p1;

		if (((size_t)buf & PAGE_MASK) !=
			((size_t)(buf + mtd->writesize - 1) & PAGE_MASK))
			goto out_copy;
		p1 = vmalloc_to_page(buf);
		if (!p1)
			goto out_copy;
		buf = page_address(p1) + ((size_t)buf & ~PAGE_MASK);
	}
	goto out_copy_done;

out_copy:
	vaddr = buf;
	chip->pagebuf = -1;
	buf = chip->buffers->databuf;
out_copy_done:

	dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR);

	tx_desc[dma_txq5_wptr.bf.index].word0.bf.own = OWN_DMA;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.buf_size = mtd->writesize;
	tx_desc[dma_txq5_wptr.bf.index].buf_adr = addr;

	/* page data rx desc */
	dma_rxq5_rptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR);
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.own = OWN_DMA;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.buf_size = mtd->writesize;

	rx_desc[dma_rxq5_rptr.bf.index].buf_adr =
	    dma_map_single(NULL, (void *)buf, mtd->writesize, DMA_FROM_DEVICE);

	wmb();
	dummy = rx_desc[dma_rxq5_rptr.bf.index].word0.wrd;
	/* set axi_bus_len = 8 */

	/* set fifo control */
	fifo_ctl.wrd = 0;
	fifo_ctl.bf.fifoCmd = FLASH_RD;
	write_flash_ctrl_reg(FLASH_NF_FIFO_CONTROL, fifo_ctl.wrd);

	flash_start.wrd = 0;
	flash_start.bf.fifoReq = FLASH_GO;
	/* flash_start.bf.nflashRegCmd = FLASH_RD; */
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

	/* update tx write ptr */
	dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR, dma_txq5_wptr.wrd);
	/* dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR); */

	/* enable txq5 */
	dma_txq5_ctrl.bf.txq5_en = 1;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_CONTROL, dma_txq5_ctrl.wrd);

	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);

	//#define DMA_DMA_SSP_TXQ5_RPTR                    0xf009046c

	while (!dma_ssp_rxq5_intsts.bf.rxq5_eof ) { //444 + 2
		udelay(1);
		schedule();
		dma_ssp_rxq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	}
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	while (!dma_ssp_txq5_intsts.bf.txq5_eof  ) { //46c +2
		udelay(1);
		schedule();
		dma_ssp_txq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	}

	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

	dma_map_single(NULL, (void *)buf, mtd->writesize, DMA_FROM_DEVICE);
	wmb();

	/* The fifo depth is 64 bytes. We have a sync at each frame and frame
	 * length is 64 bytes. --> for vmalloc not kmalloc
	 */
	if(vaddr!=0)
	{
		memcpy(vaddr, buf, mtd->writesize);
		buf = vaddr;
		vaddr = 0;

	}


	/* oob tx desc */
	//dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;

	//addr +=  mtd->writesize;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.own = OWN_DMA;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.buf_size = mtd->oobsize;
	tx_desc[dma_txq5_wptr.bf.index].buf_adr = (addr + mtd->writesize);

	/* oob rx desc */
	dma_rxq5_rptr.bf.index = (dma_rxq5_rptr.bf.index + 1) % FDMA_DESC_NUM;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.own = OWN_DMA;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.buf_size = mtd->oobsize;
	rx_desc[dma_rxq5_rptr.bf.index].buf_adr =
	    dma_map_single(NULL, (void *)chip->oob_poi, mtd->oobsize,
			   DMA_FROM_DEVICE);

	wmb();
	dummy = rx_desc[dma_rxq5_rptr.bf.index].word0.wrd;
	/* set axi_bus_len = 8 */

	/* update tx write ptr */
	dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR, dma_txq5_wptr.wrd);
	/* dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR); */


	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	//#define DMA_DMA_SSP_TXQ5_RPTR                    0xf009046c

	while (!dma_ssp_rxq5_intsts.bf.rxq5_eof ) { //444 + 2
		udelay(1);
		schedule();
		dma_ssp_rxq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	}
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	while (!dma_ssp_txq5_intsts.bf.txq5_eof  ) { //46c +2
		udelay(1);
		schedule();
		dma_ssp_txq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	}

	flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	while (flash_start.bf.fifoReq) {
		udelay(1);
		schedule();
		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	}


	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

	dma_rxq5_rptr.bf.index = (dma_rxq5_rptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR, dma_rxq5_rptr.wrd);

	dma_map_single(NULL, (void *)chip->oob_poi, mtd->oobsize,
			   DMA_FROM_DEVICE);
	wmb();

#else
	/* direct access nand */
	static unsigned int *pread;
	int i;
	pread = (unsigned int *)buf;

	nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (unsigned int *)((page << chip->page_shift) % SZ_128M);
	addr =
	    (unsigned int *)((unsigned int)addr +
			     (unsigned int)(cs752x_host->io_base));
	/* printk("%s : addr : %p  pread: %p",__func__, addr, pread); */
	for (i = 0; i < (mtd->writesize / 4); i++) {
		*(pread + i) = *((unsigned int *)addr + i);
	}

	/* ecc_sts.wrd=read_flash_ctrl_reg(FLASH_NF_ECC_STATUS); */
	/* while(!ecc_sts.bf.eccDone) */
	/* { */
	/*     ecc_sts.wrd=read_flash_ctrl_reg(FLASH_NF_ECC_STATUS); */
	/*     udelay(1); */
	/*     schedule(); */
	/* } */

	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0);	/* disable ecc gen */

	pread = (unsigned int *)(chip->oob_poi);

	nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (unsigned int *)((page << chip->page_shift) % SZ_128M);
	addr =
	    (unsigned int *)((unsigned int)addr +
			     (unsigned int)(cs752x_host->io_base) +
			     mtd->writesize);
	/* printk("   oob : addr (%p)  pread (%p) \n", addr, pread); */
	unsigned int tmp_addr;
	tmp_addr = (unsigned int)addr;
	if (tmp_addr == 0x91000000)
		printk("%s : out of range  page(%x)\n ", __func__, page);
	if (((unsigned int)addr) ==
	    ((unsigned int)(cs752x_host->io_base) + SZ_128M)) {
		cs752x_nand_read_oob_std(mtd, chip, page, 0);
	} else {
		for (i = 0; i < (mtd->oobsize / 4); i++) {
			*(pread + i) = *((unsigned int *)addr + i);
		}
	}

#endif

	return 0;
}

/**
 * cs752x_nand_read_page_raw - [Intern] read raw page data without ecc
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	buffer to store read data
 * @page:	page number to read
 *
 * Not for syndrome calculating ecc controllers, which use a special oob layout
 */
static int cs752x_nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int page)
{
	check_flash_ctrl_status();

	ecc_reset.wrd = 0;
	ecc_reset.bf.eccClear = ECC_CLR;
	ecc_reset.bf.fifoClear = FIFO_CLR;
	write_flash_ctrl_reg(FLASH_NF_ECC_RESET, ecc_reset.wrd);

	flash_int_sts.bf.regIrq = 1;
	write_flash_ctrl_reg(FLASH_FLASH_INTERRUPT, flash_int_sts.wrd);

	ecc_reset.wrd = 0;
	ecc_reset.bf.eccClear = 1;
	write_flash_ctrl_reg(FLASH_NF_ECC_RESET, ecc_reset.wrd);

	bch_ctrl.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);

	ecc_ctl.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);

	return cs752x_nand_read_page(mtd, chip, buf, page);
}



#ifdef CONFIG_CS752X_NAND_ECC_HW_BCH

static void fill_bch_oob_data( struct nand_chip *chip )
{
	u32 i, j;
	u32 *eccpos = chip->ecc.layout->eccpos;
	int eccsteps = chip->ecc.steps;

	FLASH_NF_BCH_GEN0_0_t bch_gen00;
	const u32 reg_offset = FLASH_NF_BCH_GEN0_1 - FLASH_NF_BCH_GEN0_0;
	const u32 group_offset = FLASH_NF_BCH_GEN1_0 - FLASH_NF_BCH_GEN0_0;

	u32 addr = FLASH_NF_BCH_GEN0_0;
	u32 *ecc_end_pos;

	for (; eccsteps; --eccsteps, addr += group_offset) {
		ecc_end_pos = eccpos + chip->ecc.bytes;

		for (i = 0; eccpos != ecc_end_pos; ++i) {
			bch_gen00.wrd =
			    read_flash_ctrl_reg(addr + reg_offset * i);

			for (j = 0; j < 4 && eccpos != ecc_end_pos;
			     ++j, ++eccpos) {
				chip->oob_poi[*eccpos] =
				    ((bch_gen00.wrd >> (j * 8)) & 0xff);
			}
		}
	}

	/*  erase tag */
	*(chip->oob_poi + chip->ecc.layout->oobfree[0].offset +
	  chip->ecc.layout->oobfree[0].length) = 0;
}


static void bch_correct( struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf)
{
	int i, j, k;
	int eccsteps, eccbytes, eccsize;
	uint32_t *eccpos, *ecc_end_pos;
	uint8_t *p = buf;

	eccsteps = chip->ecc.steps;
	eccbytes = chip->ecc.bytes;
	eccsize = chip->ecc.size;
	eccpos = chip->ecc.layout->eccpos;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		ecc_end_pos = eccpos + chip->ecc.bytes;
		for (j = 0; j < chip->ecc.bytes; j += 4) {
			bch_oob0.wrd = 0;
			for (k = 0; k < 4 && eccpos != ecc_end_pos;
			     ++k, ++eccpos) {
				bch_oob0.wrd |=
				    chip->oob_poi[*eccpos] << (8 * k);
			}

			write_flash_ctrl_reg(FLASH_NF_BCH_OOB0 + j,
					     bch_oob0.wrd);
		}

		/* enable ecc compare */
		bch_ctrl.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_CONTROL);
		bch_ctrl.bf.bchCodeSel = (i / eccbytes);
		bch_ctrl.bf.bchCompare = 1;
		write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);

		bch_sts.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_STATUS);
		while (!bch_sts.bf.bchDecDone) {
			udelay(1);
			schedule();
			bch_sts.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_STATUS);
		}

		switch (bch_sts.bf.bchDecStatus) {
		case BCH_CORRECTABLE_ERR:
			//printk("correctable error(%x)!!\n",bch_sts.bf.bchErrNum);
			for (j = 0; j < ((bch_sts.bf.bchErrNum + 1) / 2); j++) {
				/** printk("nand_page(%x)\n", nand_page); **/
				bch_err_loc01.wrd =
				    read_flash_ctrl_reg(FLASH_NF_BCH_ERROR_LOC01
							+ j * 4);

				if ((j + 1) * 2 <= bch_sts.bf.bchErrNum) {
					if (((bch_err_loc01.bf.
					      bchErrLoc1 & 0x1fff) >> 3) <
					    0x200) {
						/**
						 printk("pdata[%x] %x , ",
						       ((i / eccbytes) *
							chip->ecc.size +
							((bch_err_loc01.bf.
							  bchErrLoc1 & 0x1fff)
							 >> 3)),
						       p[(bch_err_loc01.bf.
							  bchErrLoc1 & 0x1fff)
							 >> 3]);
						**/
						mtd->ecc_stats.corrected++;
						p[(bch_err_loc01.bf.
						   bchErrLoc1 & 0x1fff) >> 3] ^=
				   (1 << (bch_err_loc01.bf.bchErrLoc1 & 0x07));
						/**
						printk("pdata_new %x  \n",
						       p[(bch_err_loc01.bf.
							  bchErrLoc1 & 0x1fff)
							 >> 3]);
						**/
					} else {
						/**
						printk
						    ("BCH bit error [%x]:[%x]\n",
						     ((bch_err_loc01.bf.
						       bchErrLoc1 & 0x1fff) >>
						      3) - 0x200,
						     bch_err_loc01.bf.
						     bchErrLoc1 & 0x07);
						**/

					}
				}

				if (((bch_err_loc01.bf.
				      bchErrLoc0 & 0x1fff) >> 3) < 0x200) {
					/**
					printk("j: %x  ,pdata[%x] %x , ", j,
					       ((i / eccbytes) *
						chip->ecc.size +
						((bch_err_loc01.bf.
						  bchErrLoc0 & 0x1fff) >> 3)),
					       p[(bch_err_loc01.bf.
						  bchErrLoc0 & 0x1fff) >> 3]);
					**/
					mtd->ecc_stats.corrected++;
					p[(bch_err_loc01.bf.
					   bchErrLoc0 & 0x1fff) >> 3] ^=
			   (1 << (bch_err_loc01.bf.bchErrLoc0 & 0x07));
					/**
					printk("pdata_new %x  \n",
					       p[(bch_err_loc01.bf.
						  bchErrLoc0 & 0x1fff) >> 3]);
					**/
				} else {
					/**
					printk("BCH bit error [%x]:[%x]\n",
					       ((bch_err_loc01.bf.
						 bchErrLoc0 & 0x1fff) >> 3) -
					       0x200,
					       bch_err_loc01.bf.
					       bchErrLoc0 & 0x07);
					**/
				}
			}
			break;
		case BCH_UNCORRECTABLE:
			printk("nand_page :%x, uncorrectable error!!\n",
			       nand_page);
			mtd->ecc_stats.failed++;
			break;
		}

		if (cs752x_ecc_check && i == 0) {
			unsigned char *free =
			    buf + mtd->writesize + mtd->oobsize;
			*free = bch_sts.bf.bchDecStatus;
			*(free + 1) = bch_sts.bf.bchErrNum;
		}

		/* disable ecc compare */
		bch_ctrl.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_CONTROL);
		bch_ctrl.bf.bchCompare = 0;
		write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);

	}
}

#endif

#ifdef CONFIG_CS752X_NAND_ECC_HW_HAMMING

static void fill_hamming_oob_data( struct nand_chip *chip, struct mtd_info *mtd )
{
	u32 i, j;
	u32 *eccpos = chip->ecc.layout->eccpos;
	int eccsteps = chip->ecc.steps;
	int eccbytes = chip->ecc.bytes;

	for (i = 0, j = 0; eccsteps; eccsteps--, i++, j += eccbytes) {
#ifdef NAND_ECC_TEST
#ifndef CONFIG_PREEMPT_NONE
		get_cpu();
#endif//CONFIG_PREEMPT_NONE
		if (chip->ecc.size == 256) {
			memset(eccode, 0, 32);
			/* printk("%s : ecc 256 addr %p ",__func__,&tw[i*256]); */
			nand_calculate_256_ecc(&tw[i * 256], &eccode[0]);
			nand_calculate_ecc(mtd, &tw[i * 256], &eccode[3]);
			eccdata = 0;
			eccdata = eccode[0] | eccode[1] << 8 | eccode[2] << 16;
			ecc_gen0.wrd =
			    read_flash_ctrl_reg(FLASH_NF_ECC_GEN0 + 4 * i);
			if (ecc_gen0.wrd != eccdata)
				printk
				    (" i (%x)HWecc error ecc_gen0.wrd(%x)  eccdata (%x)!\n",
				     i, ecc_gen0.wrd, eccdata);
		} else {
			memset(eccode, 0, 32);
			/* printk("%s : ecc 512 addr %p ",__func__,&tw[i*512]); */
			nand_calculate_512_ecc(mtd, &tw[i * 512], &eccode[0]);

			eccdata = 0;
			eccdata = eccode[0] | eccode[1] << 8 | eccode[2] << 16;
			ecc_gen0.wrd =
			    read_flash_ctrl_reg(FLASH_NF_ECC_GEN0 + 4 * i);
			if (ecc_gen0.wrd != eccdata)
				printk
				    (" i (%x)HWecc error ecc_gen0.wrd(%x)  eccdata (%x)!\n",
				     i, ecc_gen0.wrd, eccdata);
		}
#ifndef CONFIG_PREEMPT_NONE
		put_cpu();
#endif//CONFIG_PREEMPT_NONE
#endif /* NAND_ECC_TEST */

		ecc_gen0.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_GEN0 + 4 * i);
		chip->oob_poi[eccpos[j]] = ecc_gen0.wrd & 0xff;	/*  */
		chip->oob_poi[eccpos[j + 1]] = (ecc_gen0.wrd >> 8) & 0xff;
		chip->oob_poi[eccpos[j + 2]] = (ecc_gen0.wrd >> 16) & 0xff;
		/*  printk("%x ", ecc_gen0.wrd);                 */
	}
}

#endif

/**
 * cs752x_nand_write_page_hwecc - [REPLACABLE] hardware ecc based page write function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	data buffer
 */
static void cs752x_nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int page, col;
	uint32_t addr;
	

	page = nand_page;
	col = nand_col;

	check_flash_ctrl_status();

	reset_ecc_bch_registers();

	dma_txq5_ctrl.bf.txq5_en = 0;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_CONTROL, dma_txq5_ctrl.wrd);

	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);


#if defined( CONFIG_CS752X_NAND_ECC_HW_BCH_8_512 )
	bch_ctrl.wrd = 0;
	bch_ctrl.bf.bchEn = BCH_ENABLE;
	bch_ctrl.bf.bchErrCap = BCH_ERR_CAP_8_512;
	bch_ctrl.bf.bchOpcode = BCH_ENCODE;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);
#elif  defined( CONFIG_CS752X_NAND_ECC_HW_BCH_12_512 )
	bch_ctrl.wrd = 0;
	bch_ctrl.bf.bchEn = BCH_ENABLE;
	bch_ctrl.bf.bchErrCap = BCH_ERR_CAP_12_512;
	bch_ctrl.bf.bchOpcode = BCH_ENCODE;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);
#elif defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_512 )
	ecc_ctl.wrd = 0;
	ecc_ctl.bf.eccGenMode = ECC_GEN_512;
	ecc_ctl.bf.eccEn = ECC_ENABLE;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);
#elif defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_256 )
	ecc_ctl.wrd = 0;
	ecc_ctl.bf.eccGenMode = ECC_GEN_256;
	ecc_ctl.bf.eccEn = ECC_ENABLE;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);
#endif

#ifndef NAND_DIRECT_ACCESS

	nf_cnt.wrd = 0;
	nf_cmd.wrd = 0;
	nf_addr1.wrd = 0;
	nf_addr2.wrd = 0;
	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = mtd->oobsize - 1;
	nf_cnt.bf.nflashRegDataCount = mtd->writesize - 1;

	if (chip->chipsize < SZ_32M) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_3;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd = (((page & 0x00ffffff) << 8));
		nf_addr2.wrd = ((page & 0xff000000) >> 24);

	} else if (chip->chipsize <= SZ_128M) {
		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_4;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd = (((page & 0xffff) << 16));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);

	} else {		/* if((chip->chipsize > SZ_128M ) )) */

		nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_5;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_2;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_SEQIN;
		nf_cmd.bf.nflashRegCmd1 = NAND_CMD_PAGEPROG;
		nf_addr1.wrd = (((page & 0xffff) << 16));
		nf_addr2.wrd = ((page & 0xffff0000) >> 16);
	}

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);

	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = CHIP_EN;
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (page << chip->page_shift) % SZ_128M + GOLDENGATE_FLASH_BASE;

	/* The fifo depth is 64 bytes. We have a sync at each frame and frame
	 * length is 64 bytes. --> for vmalloc not kmalloc
	 */
	uint8_t *vaddr;
	vaddr = 0;
	if (buf >= high_memory) {
		struct page *p1;

		if (((size_t)buf & PAGE_MASK) !=
			((size_t)(buf + mtd->writesize - 1) & PAGE_MASK))
			goto out_copy;
		p1 = vmalloc_to_page(buf);
		if (!p1)
			goto out_copy;
		buf = page_address(p1) + ((size_t)buf & ~PAGE_MASK);

	}
	goto out_copy_done;
out_copy:
	vaddr = buf;
	chip->pagebuf = -1;
	buf = chip->buffers->databuf;
	memcpy(buf, vaddr, mtd->writesize);
out_copy_done:

	/* page data tx desc */
	dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR);
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.own = OWN_DMA;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.buf_size = mtd->writesize;
	tx_desc[dma_txq5_wptr.bf.index].buf_adr =
	    dma_map_single(NULL, (void *)buf, mtd->writesize, DMA_TO_DEVICE);

#ifdef	NAND_ECC_TEST
	memset(tw, 0, NAND_MAX_PAGESIZE);
	memcpy(tw, buf, mtd->writesize);
#endif

	dma_rxq5_rptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR);
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.own = OWN_DMA;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.buf_size = mtd->writesize;
	rx_desc[dma_rxq5_rptr.bf.index].buf_adr = addr;

	/* update page tx write ptr */
	dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR, dma_txq5_wptr.wrd);
	/* set axi_bus_len = 8 */
	/* set fifo control */
	fifo_ctl.wrd = 0;
	fifo_ctl.bf.fifoCmd = FLASH_WT;
	write_flash_ctrl_reg(FLASH_NF_FIFO_CONTROL, fifo_ctl.wrd);

	wmb();
	dummy = rx_desc[dma_rxq5_rptr.bf.index].word0.wrd;

	flash_start.wrd = 0;
	flash_start.bf.fifoReq = FLASH_GO;
	/* flash_start.bf.nflashRegCmd = FLASH_WT; */
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

	/* enable txq5 */
	dma_txq5_ctrl.bf.txq5_en = 1;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_CONTROL, dma_txq5_ctrl.wrd);

	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	/**
	if(dma_ssp_rxq5_intsts.bf.rxq5_full)
	{
		printk("rxq5_full \n");
		while(1);
	}
	**/
	while (!dma_ssp_rxq5_intsts.bf.rxq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_rxq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	}

	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	while (!dma_ssp_txq5_intsts.bf.txq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_txq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	}

	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

#else

#ifdef	NAND_ECC_TEST
	memset(tw, 0, NAND_MAX_PAGESIZE);
	memcpy(tw, buf, mtd->writesize);
#endif
	/* direct access nand */
	unsigned int *pwrt, i;
	pwrt = (unsigned int *)buf;

	/* addr = (unsigned int *)((page << chip->page_shift) + cs752x_host->io_base ); */
	nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (unsigned int *)((page << chip->page_shift) % SZ_128M);
	addr =
	    (unsigned int *)((unsigned int)addr +
			     (unsigned int)(cs752x_host->io_base));
	/* printk("cs752x_nand_write_page_hwecc : addr : %p  pwrt: %p",addr, pwrt); */
	for (i = 0; i < ((mtd->writesize) / 4); i++) {
		*((unsigned int *)addr + i) = *(pwrt + i);
	}
#endif

#ifdef	CONFIG_CS752X_NAND_ECC_HW_BCH
	bch_sts.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_STATUS);
	while (!bch_sts.bf.bchGenDone) {
		udelay(1);
		schedule();
		bch_sts.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_STATUS);
	}
	bch_ctrl.wrd = read_flash_ctrl_reg(FLASH_NF_BCH_CONTROL);	/* disable ecc gen */
	bch_ctrl.bf.bchEn = BCH_DISABLE;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);

#else

	ecc_sts.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_STATUS);
	while (!ecc_sts.bf.eccDone) {
		udelay(1);
		schedule();
		ecc_sts.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_STATUS);
	}

	ecc_ctl.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_CONTROL);
	ecc_ctl.bf.eccEn = 0;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);	/* disable ecc gen */

#endif

	/* printk("write page ecc(page %x) : ", page); */

#if defined( CONFIG_CS752X_NAND_ECC_HW_BCH )
	fill_bch_oob_data(chip);

#else
	fill_hamming_oob_data(chip, mtd);
#endif
	/* printk("\n"); */

#ifndef NAND_DIRECT_ACCESS

	/* oob rx desc */
	//addr +=  mtd->writesize;

	dma_rxq5_rptr.bf.index = (dma_rxq5_rptr.bf.index + 1) % FDMA_DESC_NUM;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.own = OWN_DMA;
	rx_desc[dma_rxq5_rptr.bf.index].word0.bf.buf_size = mtd->oobsize;
	rx_desc[dma_rxq5_rptr.bf.index].buf_adr = (addr + mtd->writesize);

	/* dma_txq5_wptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR); */
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.own = OWN_DMA;
	tx_desc[dma_txq5_wptr.bf.index].word0.bf.buf_size = mtd->oobsize;
	tx_desc[dma_txq5_wptr.bf.index].buf_adr =
	    dma_map_single(NULL, (void *)chip->oob_poi, mtd->oobsize,
			   DMA_TO_DEVICE);

	wmb();
	dummy = tx_desc[dma_txq5_wptr.bf.index].word0.wrd;

	/* dma_cache_sync(NULL, chip->oob_poi, mtd->oobsize, DMA_BIDIRECTIONAL); */
	/* update tx write ptr */
	dma_txq5_wptr.bf.index = (dma_txq5_wptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_WPTR, dma_txq5_wptr.wrd);

	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	/**
	if(dma_ssp_rxq5_intsts.bf.rxq5_full)
	{
		printk("rxq5_full \n");
		while(1);
	}
	**/
	while (!dma_ssp_rxq5_intsts.bf.rxq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_rxq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	}
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	while (!dma_ssp_txq5_intsts.bf.txq5_eof) {
		udelay(1);
		schedule();
		dma_ssp_txq5_intsts.wrd =
		    read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	}

	flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	while (flash_start.bf.fifoReq) {
		udelay(1);
		schedule();
		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	}

	/* update rx read ptr */
	/* dma_rxq5_rptr.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR); */
	dma_rxq5_rptr.bf.index = (dma_rxq5_rptr.bf.index + 1) % FDMA_DESC_NUM;
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_RPTR, dma_rxq5_rptr.wrd);


	/* clr tx/rx eof */
	dma_ssp_txq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_INTERRUPT, dma_ssp_txq5_intsts.wrd);
	dma_ssp_rxq5_intsts.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT);
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_INTERRUPT, dma_ssp_rxq5_intsts.wrd);

	/* The fifo depth is 64 bytes. We have a sync at each frame and frame
	 * length is 64 bytes. --> for vmalloc not kmalloc
	 */
	if(vaddr!=0)
	{
		buf = vaddr;
		vaddr = 0;
	}

	/* /////////////// */
	/* chip->write_buf(mtd, chip->oob_poi, mtd->oobsize); */
#else
	/* direct access nand */
	pwrt = (unsigned int *)chip->oob_poi;
	/* addr = (unsigned int *)(((page << chip->page_shift) + mtd->writesize) + cs752x_host->io_base ); */
	nf_access.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	nf_access.bf.nflashExtAddr = ((page << chip->page_shift) / SZ_128M);
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	addr = (unsigned int *)((page << chip->page_shift) % SZ_128M);
	addr =
	    (unsigned int *)((unsigned int)addr + mtd->writesize +
			     (unsigned int)(cs752x_host->io_base));
	/* printk("  oob : addr(%p)  pwrt(%p) \n",addr, pwrt); */
	unsigned int tmp_addr;
	tmp_addr = (unsigned int)addr;
	if (tmp_addr == 0x91000000)
		printk("%s : out of range  page(%x)\n ", __func__, page);

	if (((unsigned int)addr) ==
	    ((unsigned int)(cs752x_host->io_base) + SZ_128M)) {
		cs752x_nand_write_oob_std(mtd, chip, page);
	} else {
		for (i = 0; i < (mtd->oobsize / 4); i++) {
			*((unsigned int *)addr + i) = *(pwrt + i);
		}
	}
#endif

}


/**
 * cs752x_nand_read_page_hwecc - [REPLACABLE] hardware ecc based page read function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	buffer to store read data
 * @page:	page number to read
 *
 * Not for syndrome calculating ecc controllers which need a special oob layout
 */
static int cs752x_nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int page)
{
	int  col;
	uint8_t *p = buf;
	uint32_t  addr;

	uint8_t *ecc_code = chip->buffers->ecccode;

	col  = nand_col;
	p = buf;
	check_flash_ctrl_status();
	reset_ecc_bch_registers();

#if defined( CONFIG_CS752X_NAND_ECC_HW_BCH_8_512 )

	bch_ctrl.wrd = 0;
	bch_ctrl.bf.bchEn = BCH_ENABLE;
	bch_ctrl.bf.bchOpcode = BCH_DECODE;
	bch_ctrl.bf.bchErrCap = BCH_ERR_CAP_8_512;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);

#elif defined( CONFIG_CS752X_NAND_ECC_HW_BCH_12_512 )

	bch_ctrl.wrd = 0;
	bch_ctrl.bf.bchEn = BCH_ENABLE;
	bch_ctrl.bf.bchOpcode = BCH_DECODE;
	bch_ctrl.bf.bchErrCap = BCH_ERR_CAP_12_512;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);

#elif defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_512)

	ecc_ctl.wrd = 0;
	ecc_ctl.bf.eccGenMode = ECC_GEN_512;
	ecc_ctl.bf.eccEn = ECC_ENABLE;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);

#elif defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_256)

	ecc_ctl.wrd = 0;
	ecc_ctl.bf.eccGenMode = ECC_GEN_256;
	ecc_ctl.bf.eccEn = ECC_ENABLE;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);

#endif


	cs752x_nand_read_page( mtd, chip, buf, page);

#if	defined( CONFIG_CS752X_NAND_ECC_HW_BCH )
	bch_sts.wrd=read_flash_ctrl_reg(FLASH_NF_BCH_STATUS);
	while(!bch_sts.bf.bchGenDone)
	{
		udelay(1);
		schedule();
		bch_sts.wrd=read_flash_ctrl_reg(FLASH_NF_BCH_STATUS);
	}

#else
	ecc_sts.wrd=read_flash_ctrl_reg(FLASH_NF_ECC_STATUS);
	while(!ecc_sts.bf.eccDone)
	{
		udelay(1);
		schedule();
		ecc_sts.wrd=read_flash_ctrl_reg(FLASH_NF_ECC_STATUS);
	}

	ecc_ctl.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_CONTROL );
	ecc_ctl.bf.eccEn = 0;
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);
#endif

#ifdef	NAND_ECC_TEST
	memset(tr , 0, NAND_MAX_PAGESIZE);
	memcpy(tr, buf, mtd->writesize);
	addr =  ((page << chip->page_shift) % SZ_128M) + cs752x_host->io_base;
#endif

#ifdef	CONFIG_CS752X_NAND_ECC_HW_BCH
	if( 0xff ==  *(chip->oob_poi+ chip->ecc.layout->oobfree[0].offset +  chip->ecc.layout->oobfree[0].length)){
		/*  Erase tga is on , No needs to check. */
		memset(buf, 0xFF, mtd->writesize);
		goto BCH_EXIT;
	}

#endif

#ifdef	CONFIG_CS752X_NAND_ECC_HW_BCH
	bch_correct( mtd, chip, buf);
#else

	int i ;

	int eccsteps, eccbytes, eccsize;
	uint32_t *eccpos;

	eccsteps = chip->ecc.steps;
	eccbytes = chip->ecc.bytes;
	eccsize = chip->ecc.size;
	eccpos = chip->ecc.layout->eccpos;

	for (i = 0; i < chip->ecc.total; i++) {
		ecc_code[i] = chip->oob_poi[eccpos[i]];
	}

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
#ifdef NAND_ECC_TEST
#ifndef CONFIG_PREEMPT_NONE
		get_cpu();
#endif//CONFIG_PREEMPT_NONE
		if(chip->ecc.size == 256)
		{
			memset(eccode, 0 ,32);
			if(memcmp(tr, buf, mtd->writesize))
				printk("copy error ");
			/* printk("%s : ecc 256 addr %p ",__func__,&tr[(i/eccbytes)*256]); */
			nand_calculate_256_ecc(&tr[(i/eccbytes)*256], &eccode[0] );
			nand_calculate_256_ecc(&buf[(i/eccbytes)*256], &eccode[3] );
			/* nand_calculate_ecc(mtd, &tr[(i/eccbytes)*256], &eccode[3]); */
			eccdata = 0;
			eccdata = eccode[0] | eccode[1]<<8 | eccode[2]<<16;
			ecc_oob.wrd =	ecc_code[i] | ecc_code[i+1]<<8 | ecc_code[i+2]<<16;
			write_flash_ctrl_reg(FLASH_NF_ECC_OOB, ecc_oob.wrd);
			if(ecc_oob.wrd != eccdata)
				printk("256: (i/eccbytes) : %x HWecc error ecc_oob.wrd(%x)  eccdata (%x)!\n",(i/eccbytes) ,ecc_oob.wrd,eccdata);
		}
		else
		{
			memset(eccode, 0 ,32);
			/* printk("%s : ecc 512 addr %p ",__func__,&tr[(i/eccbytes)*512]); */
			nand_calculate_512_ecc(mtd, &tr[(i/eccbytes)*512], &eccode[0] );

			/* nand_calculate_ecc(mtd, &tr[(i/eccbytes)*512], &eccode[3]); */
			eccdata = 0;
			eccdata = eccode[0] | eccode[1]<<8 | eccode[2]<<16;
			ecc_oob.wrd =	ecc_code[i] | ecc_code[i+1]<<8 | ecc_code[i+2]<<16;
			write_flash_ctrl_reg(FLASH_NF_ECC_OOB, ecc_oob.wrd);
			if(ecc_oob.wrd != eccdata)
				printk("512: (i/eccbytes) : %x HWecc error ecc_oob.wrd(%x)  eccdata (%x)!\n",(i/eccbytes) ,ecc_oob.wrd,eccdata);
		}
#ifndef CONFIG_PREEMPT_NONE
		put_cpu();
#endif//CONFIG_PREEMPT_NONE
#endif
		/* for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) { */
		ecc_oob.wrd =	ecc_code[i] | ecc_code[i+1]<<8 | ecc_code[i+2]<<16;
		write_flash_ctrl_reg(FLASH_NF_ECC_OOB, ecc_oob.wrd);

		ecc_ctl.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_CONTROL);
		ecc_ctl.bf.eccCodeSel = (i/eccbytes);
		write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, ecc_ctl.wrd);

		ecc_sts.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_STATUS);

		switch(ecc_sts.bf.eccStatus)
		{
			case ECC_NO_ERR:
				/* printk("ECC no error!!\n"); */
				break;
			case ECC_1BIT_DATA_ERR:
				/* flip the bit */
				mtd->ecc_stats.corrected++;
				p[ecc_sts.bf.eccErrByte] ^= (1 << ecc_sts.bf.eccErrBit);
				/* printk("ecc_code- %x (%x) :\n",i ,chip->ecc.total); */
				ecc_gen0.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_GEN0 + (4*(i/eccbytes)));
				/* for (i = 0; i < chip->ecc.total; i++) */
				/*   printk(" %x", ecc_code[i]); */

				printk("\nECC one bit data error(%x)!!(org: %x) HW(%xs) page(%x)\n", (i/eccbytes),ecc_oob.wrd, ecc_gen0.wrd, page);
				break;
			case ECC_1BIT_ECC_ERR:
				mtd->ecc_stats.corrected++;
				/* printk("ecc_code- %x (%x) :\n",i ,chip->ecc.total); */
				ecc_gen0.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_GEN0 + (4*(i/eccbytes)));
				/* for (i = 0; i < chip->ecc.total; i++) */
				/*   printk(" %x", ecc_code[i]); */
				printk("\nECC one bit ECC error(%x)!!(org: %x) HW(%xs) page(%x)\n", (i/eccbytes), ecc_oob.wrd, ecc_gen0.wrd, page);
				break;
			case ECC_UNCORRECTABLE:
				mtd->ecc_stats.failed++;
				ecc_gen0.wrd = read_flash_ctrl_reg(FLASH_NF_ECC_GEN0 + (4*(i/eccbytes)));
				printk("\nECC uncorrectable error(%x)!!(org: %x) HW(%xs) page(%x)\n", (i/eccbytes), ecc_oob.wrd, ecc_gen0.wrd, page);
				break;
		}

		if ( cs752x_ecc_check && i == 0 ) {
			unsigned char *free= buf+ mtd->writesize+ mtd->oobsize;
			*free=  ecc_sts.bf.eccStatus;
		}
	}
#endif


	#ifdef	CONFIG_CS752X_NAND_ECC_HW_BCH
BCH_EXIT:
	bch_ctrl.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_BCH_CONTROL, bch_ctrl.wrd);
	#endif

	return 0;
}

/**
 * cs752x_nand_write_page - [REPLACEABLE] write one page
 * @mtd:	MTD device structure
 * @chip:	NAND chip descriptor
 * @buf:	the data to write
 * @page:	page number to write
 * @cached:	cached programming
 * @raw:	use _raw version of write_page
 */
static int cs752x_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			   const uint8_t *buf, int page, int cached, int raw)
{
	int status;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0x00, page);

	if (unlikely(raw))
		chip->ecc.write_page_raw(mtd, chip, buf);
	else
		chip->ecc.write_page(mtd, chip, buf);

	/*
	 * Cached progamming disabled for now, Not sure if its worth the
	 * trouble. The speed gain is not very impressive. (2.3->2.6Mib/s)
	 */
	cached = 0;

	if (!cached || !(chip->options & NAND_CACHEPRG)) {
		chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);
		status = chip->waitfunc(mtd, chip);
		/*
		 * See if operation failed and additional status checks are
		 * available
		 */
		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd, chip, FL_WRITING, status,
					       page);

		if (status & NAND_STATUS_FAIL) {
			return -EIO;
		}
	} else {
		chip->cmdfunc(mtd, NAND_CMD_CACHEDPROG, -1, -1);
		status = chip->waitfunc(mtd, chip);
		printk
		    ("%s: ------------------------------->NAND_CMD_CACHEDPROG\n",
		     __func__);
	}

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	/* Send command to read back the data */
	//printk("%s: verify------------------------------->page(%x)--%x\n",__func__,page,buf[0]);
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page);

	if (chip->verify_buf(mtd, buf, mtd->writesize)) {
		return -EIO;
	}
#endif
	return 0;
}

/**
 * cs752x_nand_scan_tail - [NAND Interface] Scan for the NAND device
 * @mtd:	    MTD device structure
 *
 * This is the second phase of the normal nand_scan() function. It
 * fills out all the uninitialized function pointers with the defaults
 * and scans for a bad block table if appropriate.
 */
int cs752x_nand_scan_tail(struct mtd_info *mtd)
{
	int i, eccStartOffset;
	struct nand_chip *chip = mtd->priv;

	if (!(chip->options & NAND_OWN_BUFFERS))
		chip->buffers = kmalloc(sizeof(*chip->buffers), GFP_KERNEL);

	if (!chip->buffers)
		return -ENOMEM;

	/* Set the internal oob buffer location, just after the page data */
	chip->oob_poi = chip->buffers->databuf + mtd->writesize;

	/*
	 * If no default placement scheme is given, select an appropriate one
	 */
	if (!chip->ecc.layout) {
#ifdef CONFIG_CS752X_NAND_ECC_HW_BCH_8_512
		if (mtd->oobsize == 16) {
			chip->ecc.layout = &cs752x_nand_bch_oob_16;
		} else
#elif defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_512 ) ||  defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_256 )
		if (mtd->oobsize == 8) {
			chip->ecc.layout = &cs752x_nand_oob_8;
		} else if (mtd->oobsize == 16) {
			chip->ecc.layout = &cs752x_nand_oob_16;
		} else
#endif
		{
			memset(&cs752x_nand_ecclayout, 0,
			       sizeof(cs752x_nand_ecclayout));
			cs752x_nand_ecclayout.eccbytes =
			    mtd->writesize / chip->ecc.size * chip->ecc.bytes;
			if (sizeof(cs752x_nand_ecclayout.eccpos) <
			    4 * cs752x_nand_ecclayout.eccbytes) {
				printk(KERN_WARNING
				       "eccpos memory is less than needed eccbytes");
				return 1;
			}

			if (cs752x_nand_ecclayout.eccbytes > mtd->oobsize) {
				printk(KERN_WARNING
				       "eccbytes is less than oob size");
				return 1;
			}

			memset(cs752x_nand_ecclayout.eccpos, 0,
			       sizeof(cs752x_nand_ecclayout.eccpos));
			eccStartOffset =
			    mtd->oobsize - cs752x_nand_ecclayout.eccbytes;
			for (i = 0; i < cs752x_nand_ecclayout.eccbytes; ++i) {
				if ((i + eccStartOffset) == chip->badblockpos) {
					continue;
				}
				cs752x_nand_ecclayout.eccpos[i] =
				    i + eccStartOffset;
			}

			cs752x_nand_ecclayout.oobfree[0].offset = 2;
			cs752x_nand_ecclayout.oobfree[0].length =
			    mtd->oobsize - cs752x_nand_ecclayout.eccbytes -
			    cs752x_nand_ecclayout.oobfree[0].offset;

#ifdef CONFIG_CS752X_NAND_ECC_HW_BCH
			/*  BCH algorithm needs one extra byte to tag erase status */
			if (cs752x_nand_ecclayout.oobfree[0].length == 0) {
				printk(KERN_WARNING
				       "eccbytes is less than required");
				return 1;
			};
			cs752x_nand_ecclayout.oobfree[0].length -= 1;
#endif
			chip->ecc.layout = &cs752x_nand_ecclayout;
		}
	}

	chip->write_page = cs752x_nand_write_page;

	/*
	 * check ECC mode, default to software if 3byte/512byte hardware ECC is
	 * selected and we have 256 byte pagesize fallback to software ECC
	 */

	switch (chip->ecc.mode) {
	case NAND_ECC_HW:
		/* Use standard hwecc read page function ? */
		chip->ecc.read_page = cs752x_nand_read_page_hwecc;
		chip->ecc.write_page = cs752x_nand_write_page_hwecc;
		chip->ecc.read_page_raw = cs752x_nand_read_page_raw;
		chip->ecc.write_page_raw = cs752x_nand_write_page_raw;
		chip->ecc.read_oob = cs752x_nand_read_oob_std;
		chip->ecc.write_oob = cs752x_nand_write_oob_std;

		/* HW ecc need read/write data to calculate */
		/* so calculate/correct use SW */
		if (chip->ecc.size == 256) {
			chip->ecc.calculate = nand_calculate_ecc;
			chip->ecc.correct = nand_correct_data;
		} else {
			chip->ecc.calculate = nand_calculate_512_ecc;
			chip->ecc.correct = nand_correct_512_data;
		}
		/* chip->ecc.calculate = cs752x_hw_nand_calculate_ecc; */
		/* chip->ecc.correct = cs752x_hw_nand_correct_data; */
		chip->ecc.read_subpage = cs752x_nand_read_subpage;

		if (mtd->writesize >= chip->ecc.size)
			break;
		printk(KERN_WARNING "%d byte HW ECC not possible on "
		       "%d byte page size, fallback to SW ECC\n",
		       chip->ecc.size, mtd->writesize);
#if 0
		chip->ecc.mode = NAND_ECC_SOFT;

	case NAND_ECC_SOFT:
		chip->ecc.calculate = nand_calculate_ecc;
		chip->ecc.correct = nand_correct_data;
		chip->ecc.read_page = nand_read_page_swecc;
		chip->ecc.read_subpage = nand_read_subpage;
		chip->ecc.write_page = nand_write_page_swecc;
		chip->ecc.read_page_raw = nand_read_page_raw;
		chip->ecc.write_page_raw = nand_write_page_raw;
		chip->ecc.read_oob = cs752x_nand_read_oob_std;
		chip->ecc.write_oob = cs752x_nand_write_oob_std;
		if (!chip->ecc.size)
			chip->ecc.size = 256;
		chip->ecc.bytes = 3;
		break;

	case NAND_ECC_NONE:
		printk(KERN_WARNING "NAND_ECC_NONE selected by board driver. "
		       "This is not recommended !!\n");
		chip->ecc.read_page = nand_read_page_raw;
		chip->ecc.write_page = nand_write_page_raw;
		chip->ecc.read_oob = nand_read_oob_std;
		chip->ecc.read_page_raw = nand_read_page_raw;
		chip->ecc.write_page_raw = nand_write_page_raw;
		chip->ecc.write_oob = cs752x_nand_write_oob_std;
		chip->ecc.size = mtd->writesize;
		chip->ecc.bytes = 0;
		break;
#endif
	default:
		printk(KERN_WARNING "Invalid NAND_ECC_MODE %d\n",
		       chip->ecc.mode);
		BUG();
	}

	/*
	 * The number of bytes available for a client to place data into
	 * the out of band area
	 */
	chip->ecc.layout->oobavail = 0;
	for (i = 0; chip->ecc.layout->oobfree[i].length
	     && i < ARRAY_SIZE(chip->ecc.layout->oobfree); i++)
		chip->ecc.layout->oobavail +=
		    chip->ecc.layout->oobfree[i].length;
	mtd->oobavail = chip->ecc.layout->oobavail;

	/*
	 * Set the number of read / write steps for one page depending on ECC
	 * mode
	 */
	chip->ecc.steps = mtd->writesize / chip->ecc.size;
	if (chip->ecc.steps * chip->ecc.size != mtd->writesize) {
		printk(KERN_WARNING "Invalid ecc parameters\n");
		BUG();
	}
	chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;

	/*
	 * Allow subpage writes up to ecc.steps. Not possible for MLC
	 * FLASH.
	 */
	if (!(chip->options & NAND_NO_SUBPAGE_WRITE) &&
	    !(chip->cellinfo & NAND_CI_CELLTYPE_MSK)) {
		switch (chip->ecc.steps) {
		case 2:
			mtd->subpage_sft = 1;
			break;
		case 4:
		case 8:
		case 16:
			mtd->subpage_sft = 2;
			break;
		}
	}
	chip->subpagesize = mtd->writesize >> mtd->subpage_sft;

	/* Initialize state */
	chip->state = FL_READY;

	/* De-select the device */
	chip->select_chip(mtd, -1);

	/* Invalidate the pagebuffer reference */
	chip->pagebuf = -1;

	/* Fill in remaining MTD driver data */
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->_erase = cs752x_nand_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_read = cs752x_nand_read;
	mtd->_write = cs752x_nand_write;
	mtd->_read_oob = cs752x_nand_read_oob;
	mtd->_write_oob = cs752x_nand_write_oob;
	mtd->_sync = cs752x_nand_sync;
	mtd->_lock = NULL;
	mtd->_unlock = NULL;
	mtd->_suspend = cs752x_nand_suspend;
	mtd->_resume = cs752x_nand_resume;
	mtd->_block_isbad = cs752x_nand_block_isbad;
	mtd->_block_markbad = cs752x_nand_block_markbad;
	mtd->writebufsize = mtd->writesize;

	/* propagate ecc.layout to mtd_info */
	mtd->ecclayout = chip->ecc.layout;

	/* Check, if we should skip the bad block table scan */
	if (chip->options & NAND_SKIP_BBTSCAN)
		return 0;

	/* erase all blocks */
#if 0
	int ready;
	printk("start nand erase...\n");
	for (i = 0; i < chip->chipsize; i += mtd->erasesize) {
		cs752x_nand_erase_block(mtd, i >> chip->page_shift);

		ready = cs752x_nand_dev_ready(mtd);
		while (ready != NAND_STATUS_READY)
			ready = cs752x_nand_dev_ready(mtd);

	}
	printk("end nand erase...\n");
#endif
	/* Build bad block table */
	return chip->scan_bbt(mtd);
}

/**
 * cs752x_nand_verify_buf - [DEFAULT] Verify chip data against buffer
 * @mtd:	MTD device structure
 * @buf:	buffer containing the data to compare
 * @len:	number of bytes to compare
 *
 * Default verify function for 8bit buswith
 */
static int cs752x_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i, page = 0;
	struct nand_chip *chip = mtd->priv;
	size_t retlen;
	retlen = 0;

	page = nand_page;

	chip->pagebuf = -1;
	memset(chip->buffers->databuf, 0, mtd->writesize);
	chip->ecc.read_page(mtd, chip, chip->buffers->databuf, page);

	if (len == mtd->writesize) {
		for (i = 0; i < len; i++) {
			if (buf[i] != chip->buffers->databuf[i]) {
				printk
				    ("Data verify error -> page: %x, byte: %x, buf[i]:%x  chip->buffers->databuf[i]:%x \n",
				     nand_page, i, buf[i],
				     chip->buffers->databuf[i]);
				return i;
			}
		}
	} else if (len == mtd->oobsize) {
		for (i = 0; i < len; i++) {
			if (buf[i] != chip->oob_poi[i]) {
				printk
				    ("OOB verify error -> page: %x, byte: %x, buf[i]:%x  chip->oob_poi[i]:%x  \n",
				     nand_page, i, buf[i], chip->oob_poi[i]);
				return i;
			}
		}
	} else {
		printk(KERN_WARNING "verify length not match 0x%08x\n", len);

		return -1;
	}

	return 0;
}

/**
 * cs752x_nand_read_buf - [DEFAULT] read chip data into buffer
 * @mtd:	MTD device structure
 * @buf:	buffer to store date
 * @len:	number of bytes to read
 *
 * Default read function for 8bit buswith
 */
static void cs752x_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int page, col;
	struct nand_chip *this = mtd->priv;

	if (len <= (mtd->writesize + mtd->oobsize)) {
		page = nand_page;
		col = nand_col;

		this->ecc.read_page(mtd, this, this->buffers->databuf, page);
		memcpy(buf, &(this->buffers->databuf[col]), len);

	}
}

/**
 * cs752x_nand_write_buf - [DEFAULT] write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to write
 *
 * Default write function for 8bit buswith
 */
static void cs752x_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i, page = 0, col = 0;
	struct nand_chip *chip = mtd->priv;
	size_t retlen;
	retlen = 0;

	if (len <= (mtd->writesize + mtd->oobsize)) {

		page = nand_page;
		col = nand_col;
		chip->pagebuf = -1;
		chip->ecc.read_page(mtd, chip, chip->buffers->databuf, page);

		for (i = 0; i < len; i++)
			chip->buffers->databuf[col + i] = buf[i];

		chip->ecc.write_page(mtd, chip, chip->buffers->databuf);
	}

}

/**
 * cs752x_nand_release_device - [GENERIC] release chip
 * @mtd:	MTD device structure
 *
 * Deselect, release chip lock and wake up anyone waiting on the device
 */
static void cs752x_nand_release_device (struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	/* De-select the NAND device */
	chip->select_chip(mtd, -1);

	/* Release the controller and the chip */
	spin_lock(&chip->controller->lock);
	chip->controller->active = NULL;
	chip->state = FL_READY;
	wake_up(&chip->controller->wq);
	spin_unlock(&chip->controller->lock);
}

/**
 * cs752x_nand_check_wp - [GENERIC] check if the chip is write protected
 * @mtd:	MTD device structure
 * Check, if the device is write protected
 *
 * The function expects, that the device is already selected
 */
static int cs752x_nand_check_wp (struct mtd_info *mtd)
{
	/* Check the WP bit */
	int ready;
	unsigned long	timeo ;

	check_flash_ctrl_status();

	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);	/* disable ecc gen */

	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = NCNT_EMPTY_OOB;
	nf_cnt.bf.nflashRegDataCount = NCNT_DATA_1;
	nf_cnt.bf.nflashRegAddrCount = NCNT_EMPTY_ADDR;
	nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);

	nf_cmd.wrd = 0;
	nf_cmd.bf.nflashRegCmd0 = NAND_CMD_STATUS;
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);

	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = CHIP_EN;
	/* nf_access.bf.nflashDirWr = ; */
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	flash_start.wrd = 0;
	flash_start.bf.nflashRegReq = FLASH_GO;
	flash_start.bf.nflashRegCmd = FLASH_RD;
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

	timeo = jiffies + HZ;
	do {
		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		if(!flash_start.bf.nflashRegReq)
			break;
	} while (time_before(jiffies, timeo));
	
	ready = read_flash_ctrl_reg(FLASH_NF_DATA) & 0xff;

	return (ready & NAND_STATUS_WP) ? 0 : 1;
}

/**
 * cs752x_nand_do_write_oob - [MTD Interface] NAND write out-of-band
 * @mtd:	MTD device structure
 * @to:		offset to write to
 * @ops:	oob operation description structure
 *
 * NAND write out-of-band
 */
static int cs752x_nand_do_write_oob(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	int chipnr, page, status, len;
	struct nand_chip *chip = mtd->priv;

	pr_debug("%s: to = 0x%08x, len = %i\n",
			 __func__, (unsigned int)to, (int)ops->ooblen);

	if (ops->mode == MTD_OPS_AUTO_OOB)
		len = chip->ecc.layout->oobavail;
	else
		len= mtd->oobsize;


	/* Do not allow write past end of page */
	if ((ops->ooboffs + ops->ooblen) > len) {
		pr_debug("%s: Attempt to write "
				"past end of page\n", __func__);
		return -EINVAL;
	}

	if (unlikely(ops->ooboffs >= len)) {
		pr_debug("%s: Attempt to start "
				"write outside oob\n", __func__);
		return -EINVAL;
	}

	/* Do not allow reads past end of device */
	if (unlikely(to >= mtd->size ||
		     ops->ooboffs + ops->ooblen >
			((mtd->size >> chip->page_shift) -
			 (to >> chip->page_shift)) * len)) {
		pr_debug("%s: Attempt write beyond "
				"end of device\n", __func__);
		return -EINVAL;
	}

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Shift to get page */
	page = (int)(to >> chip->page_shift);

	/*
	 * Reset the chip. Some chips (like the Toshiba TC5832DC found in one
	 * of my DiskOnChip 2000 test units) will clear the whole data page too
	 * if we don't do this. I have no clue why, but I seem to have 'fixed'
	 * it in the doc2000 driver in August 1999.  dwmw2.
	 */
	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

	/* Check, if it is write protected */
	if (cs752x_nand_check_wp(mtd))
		return -EROFS;

	/* Invalidate the page cache, if we write to the cached page */
	if (page == chip->pagebuf)
		chip->pagebuf = -1;

	cs752x_nand_fill_oob(mtd, ops->oobbuf, ops->ooblen, ops);
	status = chip->ecc.write_oob(mtd, chip, page & chip->pagemask);


	if (status)
		return status;

	ops->oobretlen = ops->ooblen;

	return 0;
}

/**
 * cs752x_nand_get_device - [GENERIC] Get chip for selected access
 * @chip:	the nand chip descriptor
 * @mtd:	MTD device structure
 * @new_state:	the state which is requested
 *
 * Get the device and lock it for exclusive access
 */
static int
cs752x_nand_get_device(struct nand_chip *chip, struct mtd_info *mtd, int new_state)
{
	spinlock_t *lock = &chip->controller->lock;
	wait_queue_head_t *wq = &chip->controller->wq;
	DECLARE_WAITQUEUE(wait, current);
 retry:
	spin_lock(lock);

	/* Hardware controller shared among independent devices */
	if (!chip->controller->active)
		chip->controller->active = chip;

	if (chip->controller->active == chip && chip->state == FL_READY) {
		chip->state = new_state;
		spin_unlock(lock);
		return 0;
	}
	if (new_state == FL_PM_SUSPENDED) {
		spin_unlock(lock);
		return (chip->state == FL_PM_SUSPENDED) ? 0 : -EAGAIN;
	}
	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(wq, &wait);
	spin_unlock(lock);
	schedule();
	remove_wait_queue(wq, &wait);
	goto retry;
}

/**
 * cs752x_nand_wait - [DEFAULT]  wait until the command is done
 * @mtd:	MTD device structure
 * @chip:	NAND chip structure
 *
 * Wait for command done. This applies to erase and program only
 * Erase can take up to 400ms and program up to 20ms according to
 * general NAND and SmartMedia specs
 */
static int cs752x_nand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{

	unsigned long timeo = jiffies;
	struct nand_chip *this = mtd->priv;
	/* unsigned long        timeo = jiffies + 2; */
	int status, state = chip->state;

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	/* wait until command is processed or timeout occures */
	do {
		if (this->dev_ready(mtd))
			break;
		touch_softlockup_watchdog();
	} while (time_before(jiffies, timeo));

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	udelay(100);

	if ((state == FL_ERASING) && (chip->options & NAND_IS_AND))
		chip->cmdfunc(mtd, NAND_CMD_STATUS_MULTI, -1, -1);
	else
		chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);

	while (time_before(jiffies, timeo)) {
		if (chip->dev_ready) {
			if (chip->dev_ready(mtd))
				break;
		} else {
			/* if (chip->read_byte(mtd) & NAND_STATUS_READY) */
			break;
		}
		cond_resched();
	}

	/* status = (int)chip->read_byte(mtd); */
	/* return status; */
	status = read_flash_ctrl_reg(FLASH_NF_DATA) & 0xff;
	return status;
}

/**
 * cs752x_nand_default_block_markbad - [DEFAULT] mark a block bad
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 *
 * This is the default implementation, which can be overridden by
 * a hardware specific driver.
*/
static int cs752x_nand_default_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	uint8_t buf[2] = { 0, 0 };
	int block, ret;

	/* Get block number */
	block = (int)(ofs >> chip->bbt_erase_shift);
	if (chip->bbt)
		chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);

	/* Do we have a flash based bad block table ? */
	if (chip->options & NAND_BBT_USE_FLASH)
		ret = nand_update_bbt(mtd, ofs);
	else {
		/* We write two bytes, so we dont have to mess with 16 bit
		 * access
		 */
		cs752x_nand_get_device(chip, mtd, FL_WRITING);
		ofs += mtd->oobsize;
		ops.len = ops.ooblen = 2;
		ops.datbuf = NULL;
		ops.oobbuf = buf;
		ops.ooboffs = chip->badblockpos & ~0x01;

		ret = cs752x_nand_do_write_oob(mtd, ofs, &ops);
		cs752x_nand_release_device(mtd);
	}
	if (!ret)
		mtd->ecc_stats.badblocks++;

	return ret;
}
/**
 * cs752x_nand_block_bad - [DEFAULT] Read bad block marker from the chip
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 * @getchip:	0, if the chip is already selected
 *
 * Check, if the block is bad.
 */
static int cs752x_nand_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	int page, chipnr, res = 0;
	struct nand_chip *chip = mtd->priv;

	page = (int)(ofs >> chip->page_shift) & chip->pagemask;

	if (getchip) {
		chipnr = (int)(ofs >> chip->chip_shift);

		cs752x_nand_get_device(chip, mtd, FL_READING);

		/* Select the NAND device */
		chip->select_chip(mtd, chipnr);
	}

	cs752x_nand_read_oob_std(mtd, chip, page, 0);
#if 0				//middle check bad block
	cs752x_nand_read_page(mtd, chip, chip->buffers->databuf, page);
	if (chip->oob_poi[0] != 0xff || chip->buffers->databuf[0] != 0xff) {
		printk
		    ("page(%x): chip->buffers->databuf[0]:%x, chip->oob_poi[0]:%x\n",
		     page, chip->buffers->databuf[0], chip->oob_poi[0]);
		return 1;
	}
	memset(chip->buffers->databuf, 0, mtd->oobsize + mtd->writesize);
	cs752x_nand_read_page(mtd, chip, chip->buffers->databuf, page + 0x79);
	if (chip->oob_poi[0] != 0xff || chip->buffers->databuf[0] != 0xff) {
		printk
		    ("page(%x): chip->buffers->databuf[0]:%x, chip->oob_poi[0]:%x\n",
		     (page + 0x79), chip->buffers->databuf[0],
		     chip->oob_poi[0]);
		return 1;
	}
#else
	if (chip->oob_poi[chip->badblockpos] != 0xff)
		return 1;
#endif
	if (getchip)
		cs752x_nand_release_device(mtd);

	return res;
}

/**
 * cs752x_nand_read_byte - [DEFAULT] read one byte from the chip
 * @mtd:	MTD device structure
 *
 * Default read function for 8bit buswith
 */
static uint8_t cs752x_nand_read_byte(struct mtd_info *mtd)
{
	unsigned int data = 0, page = 0, col = 0;
	struct nand_chip *chip = mtd->priv;

	page = nand_page;
	col = nand_col;
	chip->pagebuf = -1;
	/* cs752x_nand_read_page_raw(mtd, chip,chip->buffers->databuf,chip->oob_poi); */
	cs752x_nand_read_page_raw(mtd, chip, chip->buffers->databuf, page);
	/* cs752x_nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,  uint8_t *buf, int page) */
	data = *(chip->buffers->databuf + col);

	return data & 0xff;
}


/*
 * Wait for the ready pin, after a command
 * The timeout is catched later.
 */
static void cs752x_nand_wait_ready(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	unsigned long	timeo = jiffies + 2;

	/* wait until command is processed or timeout occures */
	do {
		if (this->dev_ready(mtd))
			return;
		touch_softlockup_watchdog();
	} while (time_before(jiffies, timeo));
}

/**
 * cs752x_nand_command_lp - [DEFAULT] Send command to NAND large page device
 * @mtd:	MTD device structure
 * @command:	the command to be sent
 * @column:	the column address for this command, -1 if none
 * @page_addr:	the page address for this command, -1 if none
 *
 * Send command to NAND device. This is the version for the new large page
 * devices We dont have the separate regions as we have in the small page
 * devices.  We must emulate NAND_CMD_READOOB to keep the code compatible.
 */
static void cs752x_nand_command_lp(struct mtd_info *mtd, unsigned int command,
			    int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;

	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}

	/* Command latch cycle */
	chip->cmd_ctrl(mtd, command & 0xff,
		       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

	if (column != -1 || page_addr != -1) {
		int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

		/* Serially input address */
		if (column != -1) {
			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;
			chip->cmd_ctrl(mtd, column, ctrl);
			ctrl &= ~NAND_CTRL_CHANGE;
			chip->cmd_ctrl(mtd, column >> 8, ctrl);
			nand_col = column;
		}
		if (page_addr != -1) {
			chip->cmd_ctrl(mtd, page_addr, ctrl);
			chip->cmd_ctrl(mtd, page_addr >> 8,
				       NAND_NCE | NAND_ALE);
			/* One more address cycle for devices > 128MiB */
			if (chip->chipsize > (128 << 20))
				chip->cmd_ctrl(mtd, page_addr >> 16,
					       NAND_NCE | NAND_ALE);

			nand_page = page_addr;
		}
	}
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	/*
	 * program and erase have their own busy handlers
	 * status, sequential in, and deplete1 need no delay
	 */
	switch (command) {

	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_RNDIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_DEPLETE1:
		/*
		 * Write out the command to the device.
		 */
		if (column != -1 || page_addr != -1) {

			/* Serially input address */
			if (column != -1)
				/* FLASH_WRITE_REG(NFLASH_ADDRESS,column); */
				nand_col = column;

			if (page_addr != -1)
				/* FLASH_WRITE_REG(NFLASH_ADDRESS,opcode|(page_addr<<8)); */
				nand_page = page_addr;

		}
		return;

		/*
		 * read error status commands require only a short delay
		 */
	case NAND_CMD_STATUS_ERROR:
	case NAND_CMD_STATUS_ERROR0:
	case NAND_CMD_STATUS_ERROR1:
	case NAND_CMD_STATUS_ERROR2:
	case NAND_CMD_STATUS_ERROR3:
		udelay(chip->chip_delay);
		return;

	case NAND_CMD_RESET:
		check_flash_ctrl_status();
		udelay(chip->chip_delay);
		write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);	/* disable ecc gen */
		nf_cnt.wrd = 0;
		nf_cnt.bf.nflashRegOobCount = NCNT_EMPTY_OOB;
		nf_cnt.bf.nflashRegDataCount = NCNT_EMPTY_DATA;
		nf_cnt.bf.nflashRegAddrCount = NCNT_EMPTY_ADDR;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
		write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);

		nf_cmd.wrd = 0;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_RESET;
		write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
		nf_addr1.wrd = 0;
		write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x00 */
		nf_addr2.wrd = 0;
		write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);	/* write address 0x00 */
		nf_access.wrd = 0;
		nf_access.bf.nflashCeAlt = CHIP_EN;
		/* nf_access.bf.nflashDirWr = ; */
		nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;

		write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);
		flash_start.wrd = 0;
		flash_start.bf.nflashRegReq = FLASH_GO;
		flash_start.bf.nflashRegCmd = FLASH_RD;
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		while (flash_start.bf.nflashRegReq) {
			udelay(1);
			schedule();
			flash_start.wrd =
			    read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		}

		udelay(2);

		while (!
		       ((read_flash_ctrl_reg(FLASH_NF_DATA) & 0xff) &
			NAND_STATUS_READY)) ;
		{

			udelay(1);
			schedule();
			return;
		}

	case NAND_CMD_RNDOUT:
		/* No ready / busy check necessary */
		chip->cmd_ctrl(mtd, NAND_CMD_RNDOUTSTART,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
		return;

	case NAND_CMD_READ0:
		chip->cmd_ctrl(mtd, NAND_CMD_READSTART,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

		/* This applies to read commands */
	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		 */
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	udelay(100);

	cs752x_nand_wait_ready(mtd);
}

/**
 * cs752x_nand_command - [DEFAULT] Send command to NAND device
 * @mtd:	MTD device structure
 * @command:	the command to be sent
 * @column:	the column address for this command, -1 if none
 * @page_addr:	the page address for this command, -1 if none
 *
 * Send command to NAND device. This function is used for small page
 * devices (256/512 Bytes per page)
 */
static void cs752x_nand_command(struct mtd_info *mtd, unsigned int command,
			 int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	int ctrl = NAND_CTRL_CLE | NAND_CTRL_CHANGE;

	/*
	 * Write out the command to the device.
	 */
	if (command == NAND_CMD_SEQIN) {
		int readcmd;

		if (column >= mtd->writesize) {
			/* OOB area */
			column -= mtd->writesize;
			readcmd = NAND_CMD_READOOB;
		} else if (column < 256) {
			/* First 256 bytes --> READ0 */
			readcmd = NAND_CMD_READ0;
		} else {
			column -= 256;
			readcmd = NAND_CMD_READ1;
		}
		chip->cmd_ctrl(mtd, readcmd, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	chip->cmd_ctrl(mtd, command, ctrl);

	/*
	 * Address cycle, when necessary
	 */
	ctrl = NAND_CTRL_ALE | NAND_CTRL_CHANGE;
	/* Serially input address */
	if (column != -1) {
		/* Adjust columns for 16 bit buswidth */
		if (chip->options & NAND_BUSWIDTH_16)
			column >>= 1;
		chip->cmd_ctrl(mtd, column, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
		nand_col = column;
	}
	if (page_addr != -1) {
		chip->cmd_ctrl(mtd, page_addr, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
		chip->cmd_ctrl(mtd, page_addr >> 8, ctrl);
		/* One more address cycle for devices > 32MiB */
		if (chip->chipsize > (32 << 20))
			chip->cmd_ctrl(mtd, page_addr >> 16, ctrl);

		nand_page = page_addr;
	}
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	/*
	 * program and erase have their own busy handlers
	 * status and sequential in needs no delay
	 */
	switch (command) {

	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
		/*
		 * Write out the command to the device.
		 */
		if (column != -1 || page_addr != -1) {

			/* Serially input address */
			if (column != -1)
				/* FLASH_WRITE_REG(NFLASH_ADDRESS,column); */
				nand_col = column;

			if (page_addr != -1)
				/* FLASH_WRITE_REG(NFLASH_ADDRESS,opcode|(page_addr<<8)); */
				nand_page = page_addr;

		}
		return;

	case NAND_CMD_RESET:
		check_flash_ctrl_status();
		udelay(chip->chip_delay);
		write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);
		nf_cnt.wrd = 0;
		nf_cnt.bf.nflashRegOobCount = NCNT_EMPTY_OOB;
		nf_cnt.bf.nflashRegDataCount = NCNT_EMPTY_DATA;
		nf_cnt.bf.nflashRegAddrCount = NCNT_EMPTY_ADDR;
		nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;
		write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);

		nf_cmd.wrd = 0;
		nf_cmd.bf.nflashRegCmd0 = NAND_CMD_RESET;
		write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
		nf_addr1.wrd = 0;
		write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x00 */
		nf_addr2.wrd = 0;
		write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);	/* `write address 0x00 */

		nf_access.wrd = 0;
		nf_access.bf.nflashCeAlt = CHIP_EN;
		/* nf_access.bf.nflashDirWr = ; */
		nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;

		write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);
		flash_start.wrd = 0;
		flash_start.bf.nflashRegReq = FLASH_GO;
		flash_start.bf.nflashRegCmd = FLASH_WT;
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		while (flash_start.bf.nflashRegReq) {
			udelay(1);
			schedule();
			flash_start.wrd =
			    read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		}

		udelay(100);
		break;

		/* This applies to read commands */
	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		 */
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}
	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */

	udelay(100);
	cs752x_nand_wait_ready(mtd);
}

static void cs752x_nand_select_chip(struct mtd_info *mtd, int chip)
{
	switch (chip) {
	case -1:
		CHIP_EN = NFLASH_CHIP0_EN;
		break;
	case 0:
		CHIP_EN = NFLASH_CHIP0_EN;
		break;
	case 1:
		CHIP_EN = NFLASH_CHIP1_EN;
		break;

	default:
		/* BUG(); */
		CHIP_EN = NFLASH_CHIP0_EN;
	}
}

/*
*	read device ready pin
*/
static int cs752x_nand_dev_ready(struct mtd_info *mtd)
{
	int ready, old_sts, new_sts;
	unsigned long	timeo ;

	check_flash_ctrl_status();

	write_flash_ctrl_reg(FLASH_NF_DATA, 0xffffffff);
	old_sts = read_flash_ctrl_reg(FLASH_STATUS);
	if ((old_sts & 0xffff) != 0)
		printk("old_sts : %x      ", old_sts);

 RD_STATUS:
	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);	/* disable ecc gen */
	nf_cnt.wrd = 0;
	nf_cnt.bf.nflashRegOobCount = NCNT_EMPTY_OOB;
	nf_cnt.bf.nflashRegDataCount = NCNT_DATA_1;
	nf_cnt.bf.nflashRegAddrCount = NCNT_EMPTY_ADDR;
	nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);

	nf_cmd.wrd = 0;
	nf_cmd.bf.nflashRegCmd0 = NAND_CMD_STATUS;
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
	nf_addr1.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x00 */
	nf_addr2.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);	/* write address 0x00 */

	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = CHIP_EN;
	/* nf_access.bf.nflashDirWr = ; */
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;

	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);
	flash_start.wrd = 0;
	flash_start.bf.nflashRegReq = FLASH_GO;
	flash_start.bf.nflashRegCmd = FLASH_RD;
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);
	
	timeo = jiffies + HZ;
	do {
		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		if(!flash_start.bf.nflashRegReq)
			break;
	} while (time_before(jiffies, timeo));

	ready = read_flash_ctrl_reg(FLASH_NF_DATA) & 0xff;
	if (ready == 0xff) {
		new_sts = read_flash_ctrl_reg(FLASH_STATUS);
		printk
		    ("--------->   %s : FLASH_STATUS : old_sts:%x, new_sts:%x \n",
		     __func__, old_sts, new_sts);
		goto RD_STATUS;
	}

	return (ready & NAND_STATUS_READY);
}


/*
 *	hardware specific access to control-lines
 *	ctrl:
 */
static void cs752x_nand_hwcontrol(struct mtd_info *mtd, int cmd,
				   unsigned int ctrl)
{
	/*
	   //struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	   //struct nand_chip *chip = mtd->priv;
	   //middle not yet
	   //nothing to do now.
	 */
}


/*Add function*/
static void cs752x_nand_read_id(int chip_no, unsigned char *id)
{
	unsigned int opcode, i;
	const unsigned int extid = 8;

	check_flash_ctrl_status();

	write_flash_ctrl_reg(FLASH_NF_ECC_CONTROL, 0x0);	/* disable ecc gen */

	flash_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);

	/* need to check extid byte counts */
	nf_cnt.wrd = 0;

	nf_cnt.bf.nflashRegOobCount = NCNT_EMPTY_OOB;
	nf_cnt.bf.nflashRegDataCount = NCNT_DATA_8;
	nf_cnt.bf.nflashRegAddrCount = NCNT_ADDR_1;
	nf_cnt.bf.nflashRegCmdCount = NCNT_CMD_1;

	write_flash_ctrl_reg(FLASH_NF_COUNT, nf_cnt.wrd);

	nf_cmd.wrd = 0;
	nf_cmd.bf.nflashRegCmd0 = NAND_CMD_READID;
	write_flash_ctrl_reg(FLASH_NF_COMMAND, nf_cmd.wrd);	/* write read id command */
	nf_addr1.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_1, nf_addr1.wrd);	/* write address 0x00 */
	nf_addr2.wrd = 0;
	write_flash_ctrl_reg(FLASH_NF_ADDRESS_2, nf_addr2.wrd);	/* write address 0x00 */

	/* read maker code */
	nf_access.wrd = 0;
	nf_access.bf.nflashCeAlt = chip_no;
	/* nf_access.bf.nflashDirWr = ; */
	nf_access.bf.nflashRegWidth = NFLASH_WiDTH8;
	write_flash_ctrl_reg(FLASH_NF_ACCESS, nf_access.wrd);

	for (i = 0; i < extid; i++) {

		flash_start.wrd = 0;
		flash_start.bf.nflashRegReq = FLASH_GO;
		flash_start.bf.nflashRegCmd = FLASH_RD;
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, flash_start.wrd);

		flash_start.wrd = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		while (flash_start.bf.nflashRegReq) {
			udelay(1);
			schedule();
			flash_start.wrd =
			    read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		}

		opcode = read_flash_ctrl_reg(FLASH_NF_DATA);
		id[i] = (opcode >> ((i << 3) % 32)) & 0xff;
	}

	ecc_reset.wrd = 0;
	ecc_reset.bf.eccClear = ECC_CLR;
	ecc_reset.bf.fifoClear = FIFO_CLR;
	ecc_reset.bf.nflash_reset = NF_RESET;
	write_flash_ctrl_reg(FLASH_NF_ECC_RESET, ecc_reset.wrd);
}

/*
 * Get the flash and manufacturer id and lookup if the type is supported
 */
static struct nand_flash_dev *cs752x_nand_get_flash_type(struct mtd_info *mtd,
						  struct nand_chip *chip,
						  int busw, int *maf_id)
{
	struct nand_flash_dev *type = NULL;
	int i, dev_id, maf_idx;
	unsigned char id[8];
	u16 oobsize_8kp[] = { 0, 128, 218, 400, 436, 512, 640, 0 };

	/* Select the device */
	chip->select_chip(mtd, 0);

	/*
	 * Reset the chip, required by some chips (e.g. Micron MT29FxGxxxxx)
	 * after power-up
	 */
	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

	/* Send the command for reading device ID */
	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);

	/* Read manufacturer and device IDs */
	memset(id, 0, sizeof(id));
	cs752x_nand_read_id(0, &id[0]);
	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

	*maf_id = id[0];
	dev_id = id[1];

	/* Lookup the flash id */
	for (i = 0; nand_flash_ids[i].name != NULL; i++) {
		if (dev_id == nand_flash_ids[i].id) {
			type = &nand_flash_ids[i];
			break;
		}
	}

	if (!type)
		return ERR_PTR(-ENODEV);

	if (!mtd->name)
		mtd->name = type->name;

	chip->chipsize = (uint64_t) type->chipsize << 20;

	/* Newer devices have all the information in additional id bytes */
	if (!type->pagesize) {
		int extid;
		/*  The 3rd id byte holds MLC / multichip data  */
		//chip->cellinfo = chip->read_byte(mtd);
		chip->cellinfo = id[2];
		/*  The 4th id byte is the important one  */
		extid = id[3];
		if (id[0] == id[6] && id[1] == id[7] &&
		    id[0] == NAND_MFR_SAMSUNG &&
		    (chip->cellinfo & NAND_CI_CELLTYPE_MSK) && id[5] != 0x00) {
			mtd->writesize = 2048 * (1 << (extid & 0x3));

			/* Calc oobsize */
			mtd->oobsize =
			    oobsize_8kp[((extid & 0x40) >> 4) +
					((extid >> 2) & 0x03)];

			/* Calc blocksize. Blocksize is multiples of 128KB */
			mtd->erasesize =
			    (1 <<
			     (((extid & 0x80) >> 5) +
			      ((extid >> 4) & 0x03))) * (128 * 1024);
			busw = 0;
		} else {
			/* Calc pagesize */
			mtd->writesize = 1024 << (extid & 0x3);
			extid >>= 2;
			/* Calc oobsize */
			mtd->oobsize =
			    (8 << (extid & 0x01)) * (mtd->writesize >> 9);
			extid >>= 2;
			/* Calc blocksize. Blocksize is multiples of 64KiB */
			mtd->erasesize = (64 * 1024) << (extid & 0x03);
			extid >>= 2;
			/* Get buswidth information */
			busw = (extid & 0x01) ? NAND_BUSWIDTH_16 : 0;
		}
	} else {
		/*
		 * Old devices have chip data hardcoded in the device id table
		 */
		mtd->erasesize = type->erasesize;
		mtd->writesize = type->pagesize;
		mtd->oobsize = mtd->writesize / 32;
		busw = type->options & NAND_BUSWIDTH_16;
	}

	/* Try to identify manufacturer */
	for (maf_idx = 0; nand_manuf_ids[maf_idx].id != 0x0; maf_idx++) {
		if (nand_manuf_ids[maf_idx].id == *maf_id)
			break;
	}

	/*  if Toshiba TH58NVG4S0FTA20 : 2GB, 4k page size, 256kB block size, 232B oob size
	 *  M_ID=0x98, D_ID=0xD3, ID[3]:[1:0] page size 1,2,4,8k;
	 *  ID[3]:[5:4] block size 64kB,128kB,256kB,512kB
	 *  And Strap pin need to set to 4k page with 224B oob size : flash type : 0x7
	 * ECC level : 4bit ECC for each 512Byte is required. So need to define BCH ECC algorithm.
	 * TH58NVG4S0FTA20 - 2G * 8bit
	 * 0x 98 D3 90 26 76 15 02 08
	 * 
	 * TC58NVG3S0FTA00 - 1G * 8bit
	 * 0x 98 D3 91 26 76 16 08 00
	 */
	  if(id[0] == NAND_MFR_TOSHIBA && id[1]== 0xD3)
	 {
	 	int extid;
	 	extid = id[3];
	 	mtd->writesize = 1024 << (extid & 0x3);
	 	flash_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	 	if(flash_type.bf.flashType==0x7)
	 		mtd->oobsize =  224;
	 	else if(flash_type.bf.flashType==0x6)
	 		mtd->oobsize =  128;
	 	else
	 		printk(KERN_INFO "Check flash type of HW strap pin for 4k page. \n");
	 		
	 	extid >>= 4;
	 	mtd->erasesize = (64 * 1024) << (extid & 0x03);
	 	if(id[2]==0x90)
	 	{
	 		type->name = "NAND 2GiB 3,3V 8-bit";
	 		mtd->name = "NAND 2GiB 3,3V 8-bit";
	 		type->chipsize  = 2048;
		}       

	 	chip->chipsize = type->chipsize << 20;
	 	#if !defined ( CONFIG_CS752X_NAND_ECC_HW_BCH )
	 		printk(KERN_INFO "NAND ECC Level 4bit ECC for each 512Byte is required. \n");
	 	#endif
	 	//printk(KERN_INFO "***********************\n pagesize : %x, oob:%x, nflash_type:%x \n",mtd->writesize, mtd->oobsize, nflash_type);
	 }

/////// middle debug : oob size not 4 bytes alignment
	if (mtd->oobsize % 8)
		mtd->oobsize = mtd->oobsize - (mtd->oobsize % 8);
///////

	/*
	 * Check, if buswidth is correct. Hardware drivers should set
	 * chip correct !
	 */
	if (busw != (chip->options & NAND_BUSWIDTH_16)) {
		printk(KERN_INFO "NAND device: Manufacturer ID:"
		       " 0x%02x, Chip ID: 0x%02x (%s %s)\n", *maf_id,
		       dev_id, nand_manuf_ids[maf_idx].name, mtd->name);
		printk(KERN_WARNING "NAND bus width %d instead %d bit\n",
		       (chip->options & NAND_BUSWIDTH_16) ? 16 : 8,
		       busw ? 16 : 8);
		return ERR_PTR(-EINVAL);
	}

	/* Calculate the address shift from the page size */
	chip->page_shift = ffs(mtd->writesize) - 1;
	/* Convert chipsize to number of pages per chip -1. */
	chip->pagemask = (chip->chipsize >> chip->page_shift) - 1;

	chip->bbt_erase_shift = chip->phys_erase_shift =
	    ffs(mtd->erasesize) - 1;
	if (chip->chipsize & 0xffffffff)
		chip->chip_shift = ffs((unsigned)chip->chipsize) - 1;
	else
		chip->chip_shift =
		    ffs((unsigned)(chip->chipsize >> 32)) + 32 - 1;

	/* Set the bad block position */
	chip->badblockpos = mtd->writesize > 512 ?
	    NAND_LARGE_BADBLOCK_POS : NAND_SMALL_BADBLOCK_POS;

	/* Get chip options */
	chip->options |= type->options;

	/*
	 * Set chip as a default. Board drivers can override it, if necessary
	 */
	chip->options |= NAND_NO_AUTOINCR;

	/* Check if chip is a not a samsung device. Do not clear the
	 * options for chips which are not having an extended id.
	 */
	if (*maf_id != NAND_MFR_SAMSUNG && !type->pagesize)
		chip->options &= ~NAND_SAMSUNG_LP_OPTIONS;

#if 0
	/* Check for AND chips with 4 page planes */
	if (chip->options & NAND_4PAGE_ARRAY)
		chip->erase_cmd = multi_erase_cmd;
	else
		chip->erase_cmd = single_erase_cmd;

	/* Do not replace user supplied command function ! */
	if (mtd->writesize > 512 && chip->cmdfunc == cs752x_nand_command)
		chip->cmdfunc = cs752x_nand_command_lp;
#endif

	printk(KERN_INFO "NAND device: Manufacturer ID:"
	       " 0x%02x, Chip ID: 0x%02x (%s %s)\n", *maf_id, dev_id,
	       nand_manuf_ids[maf_idx].name, type->name);

	return type;
}


/*
 * Set default functions
 */
static void cs752x_nand_set_defaults(struct nand_chip *chip, int busw)
{
	/* check for proper chip_delay setup, set 20us if not */
	chip->chip_delay = 20;
	/* check, if a user supplied command function given */
	chip->cmdfunc = cs752x_nand_command;
	chip->waitfunc = cs752x_nand_wait;
	chip->select_chip = cs752x_nand_select_chip;
	chip->read_byte = cs752x_nand_read_byte;
	/* chip->read_word = cs752x_nand_read_word; */
	chip->block_bad = cs752x_nand_block_bad;
	chip->block_markbad = cs752x_nand_default_block_markbad;
	chip->write_buf = cs752x_nand_write_buf;
	chip->read_buf = cs752x_nand_read_buf;
	chip->verify_buf = cs752x_nand_verify_buf;
	chip->scan_bbt = nand_default_bbt;

	/* set the bad block tables to support debugging */
	chip->bbt_td = &cs752x_bbt_main_descr;
	chip->bbt_md = &cs752x_bbt_mirror_descr;

	if (!chip->controller) {
		chip->controller = &chip->hwcontrol;
		spin_lock_init(&chip->controller->lock);
		init_waitqueue_head(&chip->controller->wq);
	}
}

/**
 * cs752x_nand_scan_ident - [NAND Interface] Scan for the NAND device
 * @mtd:	     MTD device structure
 * @maxchips:	     Number of chips to scan for
 *
 * This is the first phase of the normal nand_scan() function. It
 * reads the flash ID and sets up MTD fields accordingly.
 *
 * The mtd->owner field must be set to the module of the caller.
 */
int cs752x_nand_scan_ident(struct mtd_info *mtd, int maxchips)
{
	int i, busw, nand_maf_id, nand_dev_id;
	struct nand_chip *chip = mtd->priv;
	struct nand_flash_dev *type;
	unsigned char id[5];

	/* Get buswidth to select the correct functions */
	busw = chip->options & NAND_BUSWIDTH_16;
	/* Set the default functions */
	cs752x_nand_set_defaults(chip, busw);

	/* Read the flash type */
	type = cs752x_nand_get_flash_type(mtd, chip, busw, &nand_maf_id);

	if (IS_ERR(type)) {
		printk(KERN_WARNING "No NAND device found!!!\n");
		chip->select_chip(mtd, -1);
		return PTR_ERR(type);
	}

	nand_dev_id = type->id;
	/* Check for a chip array */
	for (i = 1; i < maxchips; i++) {
		chip->select_chip(mtd, i);
		/* Send the command for reading device ID */
		cs752x_nand_read_id(1, id);

		/* Read manufacturer and device IDs */
		if (nand_maf_id != id[0] || nand_dev_id != id[1])
			break;
	}
	if (i > 1)
		printk(KERN_INFO "%d NAND chips detected\n", i);

	/* Store the number of chips and calc total size for mtd */
	chip->numchips = i;
	mtd->size = i * chip->chipsize;

	return 0;
}

/**
 * cs752x_nand_scan - [NAND Interface] Scan for the NAND device
 * @mtd:	MTD device structure
 * @maxchips:	Number of chips to scan for
 *
 * This fills out all the uninitialized function pointers
 * with the defaults.
 * The flash ID is read and the mtd/chip structures are
 * filled with the appropriate values.
 * The mtd->owner field must be set to the module of the caller
 *
 */
int cs752x_nand_scan(struct mtd_info *mtd, int maxchips)
{
	int ret;

	ret = cs752x_nand_scan_ident(mtd, maxchips);
	if (!ret) {
		ret = cs752x_nand_scan_tail(mtd);
	}
	return ret;
}

static int init_DMA_SSP( void )
{
	int i;

	dma_addr_t dma_tx_handle;
	dma_addr_t dma_rx_handle;

	DMA_DMA_SSP_RXDMA_CONTROL_t dma_rxdma_ctrl;
	DMA_DMA_SSP_TXDMA_CONTROL_t dma_txdma_ctrl;

	DMA_DMA_SSP_RXQ5_BASE_DEPTH_t dma_rxq5_base_depth;
	DMA_DMA_SSP_TXQ5_BASE_DEPTH_t dma_txq5_base_depth;

	dma_rxdma_ctrl.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_RXDMA_CONTROL);
	dma_txdma_ctrl.wrd = read_dma_ctrl_reg(DMA_DMA_SSP_TXDMA_CONTROL);

	if ((dma_rxdma_ctrl.bf.rx_check_own != 1)
	    && (dma_rxdma_ctrl.bf.rx_dma_enable != 1)) {
		dma_rxdma_ctrl.bf.rx_check_own = 1;
		dma_rxdma_ctrl.bf.rx_dma_enable = 1;
		write_dma_ctrl_reg(DMA_DMA_SSP_RXDMA_CONTROL,
				   dma_rxdma_ctrl.wrd);
	}
	if ((dma_txdma_ctrl.bf.tx_check_own != 1)
	    && (dma_txdma_ctrl.bf.tx_dma_enable != 1)) {
		dma_txdma_ctrl.bf.tx_check_own = 1;
		dma_txdma_ctrl.bf.tx_dma_enable = 1;
		write_dma_ctrl_reg(DMA_DMA_SSP_TXDMA_CONTROL,
				   dma_txdma_ctrl.wrd);
	}

	tx_desc =
	    (DMA_SSP_TX_DESC_T *) dma_alloc_coherent(NULL,
						     (sizeof(DMA_SSP_TX_DESC_T)
						      * FDMA_DESC_NUM),
						     &dma_tx_handle,
						     GFP_KERNEL | GFP_DMA);
	rx_desc =
	    (DMA_SSP_RX_DESC_T *) dma_alloc_coherent(NULL,
						     (sizeof(DMA_SSP_RX_DESC_T)
						      * FDMA_DESC_NUM),
						     &dma_rx_handle,
						     GFP_KERNEL | GFP_DMA);

	if (!rx_desc || !tx_desc) {
		printk("Buffer allocation for failed!\n");
		if (rx_desc) {
			kfree(rx_desc);
		}

		if (tx_desc) {
			kfree(tx_desc);
		}

		return 0;
	}
	//printk("tx_desc_v: %p (p: %x), rx_desc_v: %p (p: %x)\n", tx_desc,
	//       dma_tx_handle, rx_desc, dma_rx_handle);
	/* set base address and depth */

	dma_rxq5_base_depth.bf.base = dma_rx_handle >> 4;
	dma_rxq5_base_depth.bf.depth = FDMA_DEPTH;
	write_dma_ctrl_reg(DMA_DMA_SSP_RXQ5_BASE_DEPTH,
			   dma_rxq5_base_depth.wrd);

	dma_txq5_base_depth.bf.base = dma_tx_handle >> 4;
	dma_txq5_base_depth.bf.depth = FDMA_DEPTH;
	write_dma_ctrl_reg(DMA_DMA_SSP_TXQ5_BASE_DEPTH,
			   dma_txq5_base_depth.wrd);

	memset((unsigned char *)tx_desc, 0,
	       (sizeof(DMA_SSP_TX_DESC_T) * FDMA_DESC_NUM));
	memset((unsigned char *)rx_desc, 0,
	       (sizeof(DMA_SSP_RX_DESC_T) * FDMA_DESC_NUM));

	for (i = 0; i < FDMA_DESC_NUM; i++) {
		/* set own by sw */
		tx_desc[i].word0.bf.own = OWN_SW;
		/* enable q5 Scatter-Gather memory copy */
		tx_desc[i].word0.bf.sgm_rsrvd = 0x15;
	}

	return 1;
}


/*
 * Probe for the NAND device.
 */
static int __devinit cs752x_nand_probe(struct platform_device *pdev)
{
	struct nand_chip *this;
	char *part_type = NULL;
#ifdef CONFIG_MTD_PARTITIONS
	int nr_partitions;
#endif
	struct resource *r;
	int err = 0;

	printk("CS752X NAND init ...\n");

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
#endif

#ifdef CONFIG_CORTINA_FPGA
	/* if fpage board need to set timing */
	write_flash_ctrl_reg(FLASH_SF_TIMING, 0x3000000);
#endif

	/* Allocate memory for MTD device structure and private data */
	cs752x_host = kzalloc(sizeof(struct cs752x_nand_host), GFP_KERNEL);
	if (!cs752x_host) {
		printk
		    ("Unable to allocate cs752x_host NAND MTD device structure.\n");
		return -ENOMEM;
	}

	r = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!r) {
		dev_err(&pdev->dev, "no io memory resource defined!\n");
		err = -ENODEV;
		goto err_get_res;
	}

	/* map physical address */
	cs752x_host->io_base = ioremap(r->start, resource_size(r) + 1024);
	if (!cs752x_host->io_base) {
		printk("ioremap to access Cortina CS752X NAND chip failed\n");
		err = -EIO;
		goto err_ioremap;
	}
//	printk("Flash base at: 0x%p\n", cs752x_host->io_base);
	/* Get pointer to private data */
	/* Allocate memory for MTD device structure and private data */
	cs752x_host->nand_chip =
	    (struct nand_chip *)kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
	if (!cs752x_host->nand_chip) {
		printk
		    ("Unable to allocate CS752X NAND MTD device structure.\n");
		err = -ENOMEM;
		goto err_mtd;
	}
	cs752x_host->mtd =
	    (struct mtd_info *)kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!cs752x_host->mtd) {
		printk("Unable to allocate CS752X NAND MTD INFO structure.\n");
		err = -ENOMEM;
		goto err_mtd;
	}
#ifndef NAND_DIRECT_ACCESS
	if (init_DMA_SSP() == 0) {
		goto err_add;
	}
#endif

	this = (struct nand_chip *)(cs752x_host->nand_chip);

	/* Link the private data with the MTD structure */
	cs752x_host->mtd->priv = this;
	cs752x_host->mtd->owner = THIS_MODULE;

	platform_set_drvdata(pdev, cs752x_host);

	/* Set address of NAND IO lines */
	this->IO_ADDR_R = (__u32 *) IO_ADDRESS(GOLDENGATE_FLASH_CTRL_BASE);
	this->IO_ADDR_W = (__u32 *) IO_ADDRESS(GOLDENGATE_FLASH_CTRL_BASE);
	/* Set address of hardware control function */
	this->cmd_ctrl = cs752x_nand_hwcontrol;
	this->dev_ready = cs752x_nand_dev_ready;
	/* set eccmode using hardware ECC */
	this->ecc.mode = NAND_ECC_HW;
#if defined( CONFIG_CS752X_NAND_ECC_HW_BCH_8_512 )
	printk("Error Correction Method: bch 8\n");
	this->ecc.size = 512;
	this->ecc.bytes = 13;
#elif defined( CONFIG_CS752X_NAND_ECC_HW_BCH_12_512 )
	printk("Error Correction Method: bch 12\n");
	this->ecc.size = 512;
	this->ecc.bytes = 20;
#elif defined( CONFIG_CS752X_NAND_ECC_HW_HAMMING_512 )
	printk("Error Correction Method: ecc 512\n");
	this->ecc.size = 512;
	this->ecc.bytes = 3;
#else
	printk("Error Correction Method: ecc 256\n");
	this->ecc.size = 256;
	this->ecc.bytes = 3;
#endif

	/* Scan to find existence of the device */
	err = cs752x_nand_scan(cs752x_host->mtd, 1);
	if (err)
		goto err_scan;

	/* Register the partitions */

	cs752x_host->mtd->name = "cs752x_nand_flash";

	mtd_device_register(cs752x_host->mtd, cs752x_partition_info,
			ARRAY_SIZE(cs752x_partition_info));

	if (err)
		goto err_add;

#ifdef CONFIG_PM
	cs_pm_freq_register_notifier(&n, 1);
#endif
	/* Return happy */
	printk("cx752x NAND init ok...\n");

#if 0

	/** use to test direct access **/
	//direct access test for 128MB access size on 256MB
	//cs752x_host->io_base
	//mtd->writesize
	//mtd->oobsize
	int i;
	//printk("Flash base at: 0x%p, access last: %x, mtd->oobsize: %x\n", cs752x_host->io_base,(cs752x_host->io_base+cs752x_host->mtd->writesize-1),cs752x_host->mtd->oobsize);
	unsigned char test_last_page[4096];

	memset(test_last_page, 0, 4096);
	memcpy(test_last_page,
	       (cs752x_host->io_base + cs752x_host->mtd->writesize - 4),
	       (cs752x_host->mtd->oobsize + 4));

	for (i = 0; i < cs752x_host->mtd->oobsize; i++) {

		printk(" %02x", test_last_page[i]);
		if ((i % 16) == 15)
			printk("\n");
	}
	printk("\n");

	memset(test_last_page, 0, 4096);
	memcpy(test_last_page,
	       (cs752x_host->io_base + cs752x_host->mtd->writesize - 3),
	       (cs752x_host->mtd->oobsize + 3));

	for (i = 0; i < cs752x_host->mtd->oobsize; i++) {

		printk(" %02x", test_last_page[i]);
		if ((i % 16) == 15)
			printk("\n");
	}
	printk("\n");
	write_flash_ctrl_reg(0xf00500a8, 7);
	memset(test_last_page, 0, 4096);
	memcpy(test_last_page,
	       (cs752x_host->io_base + cs752x_host->mtd->writesize - 2),
	       (cs752x_host->mtd->oobsize + 2));

	for (i = 0; i < cs752x_host->mtd->oobsize; i++) {

		printk(" %02x", test_last_page[i]);
		if ((i % 16) == 15)
			printk("\n");
	}
	printk("\n");
	write_flash_ctrl_reg(0xf00500a8, 7);
	memset(test_last_page, 0, 4096);
	memcpy(test_last_page,
	       (cs752x_host->io_base + cs752x_host->mtd->writesize - 1),
	       (cs752x_host->mtd->oobsize + 1));
	for (i = 0; i < cs752x_host->mtd->oobsize; i++) {

		printk(" %02x", test_last_page[i]);
		if ((i % 16) == 15)
			printk("\n");
	}
	printk("\n");

	write_flash_ctrl_reg(0xf00500a8, 7);
	memset(test_last_page, 0, 4096);
	memcpy(test_last_page, (cs752x_host->io_base + 0x8000000 - 4),
	       (cs752x_host->mtd->oobsize + 4));
	for (i = 0; i < cs752x_host->mtd->oobsize; i++) {

		printk(" %02x", test_last_page[i]);
		if ((i % 16) == 15)
			printk("\n");
	}
	printk("\n");

	write_flash_ctrl_reg(0xf00500a8, 7);
#endif
	return 0;

 err_add:
	nand_release(cs752x_host->mtd);

 err_scan:
	platform_set_drvdata(pdev, NULL);
	iounmap(cs752x_host->io_base);
 err_mtd:
	if (cs752x_host->mtd)
		kfree(cs752x_host->mtd);
	if (cs752x_host->nand_chip)
		kfree(cs752x_host->nand_chip);
 err_ioremap:
 err_get_res:
	kfree(cs752x_host);
	return err;
}

/*
 * Remove a NAND device.
 */
static int __devexit cs752x_nand_remove(struct platform_device *pdev)
{
	struct cs752x_nand_host *cs752x_host = platform_get_drvdata(pdev);

	mtd_device_unregister(cs752x_host->mtd);
	
#ifdef CONFIG_PM
	cs_pm_freq_unregister_notifier(&n);
#endif
	/* Release resources, unregister device */
	nand_release(cs752x_host->mtd);

	platform_set_drvdata(pdev, NULL);

	iounmap(cs752x_host->io_base);

	/* Free the MTD device structure */
	kfree(cs752x_host);

	return 0;
}


static struct platform_driver cs752x_nand_driver = {
	.driver.name	= "cs752x_nand",
	.driver.owner	= THIS_MODULE,
	.probe		= cs752x_nand_probe,
	.remove		= cs752x_nand_remove,
	/* .suspend	= cs752x_nand_suspend, */
	/* .resume	= cs752x_nand_resume, */
};


static int __init cs752x_nand_init(void)
{
	FLASH_TYPE_t sf_type;
	char *ptr;

	/* parsing serial flash type */
	ptr = strstr(saved_command_line, "nf_type");
	if (ptr) {
		ptr += strlen("nf_type") + 1;
		nflash_type = simple_strtoul(ptr, NULL, 0);
		write_flash_ctrl_reg(FLASH_TYPE, nflash_type);
	}
	else
	{
		nflash_type = read_flash_ctrl_reg(FLASH_TYPE)& 0xffff;
		printk("%s : Flash type : 0x%04x \n",__func__,nflash_type);
	}



	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);

	if (sf_type.bf.flashType == 0 || sf_type.bf.flashType == 1) {
		return -EACCES;
	}

	printk("--> cs752x_nand_init\n");
	return platform_driver_register(&cs752x_nand_driver);
}

static void __exit cs752x_nand_exit(void)
{
	platform_driver_unregister(&cs752x_nand_driver);
}

module_init(cs752x_nand_init);
module_exit(cs752x_nand_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Middle Huang <middle.huang@cortina-systems.com>");
MODULE_DESCRIPTION("NAND flash driver for Cortina CS752X flash module");
MODULE_ALIAS("platform:cs752x_nand");
