/*
 * linux/arch/arm/mach-goldengate/include/mach/cs752x_clcd_regs.h
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Reference:
 */
#ifndef __CS752X_CLCD_REGS_H
#define __CS752X_CLCD_REGS_H
#if 0
typedef enum cs752x_clcd_regs {
    LCDTiming0_REG	=	0x00,    /* Horizontal Axis Panel Control Register */
    LCDTiming1_REG	=	0x04,    /* Vertical Axis Panel Control Register */
    LCDTiming2_REG	=	0x08,    /* Clock and Signal Polarity Control Register */
    LCDTiming3_REG	=	0x0c,    /* Line End Control Register */
    LCDUPBASE_REG	=	0x10,    /* Upper Panel Frame Base Address Registers */
    LCDLPBASE_REG	=	0x14,    /* Lower Panel Frame Base Address Registers */
    LCDControl_REG		=	0x18,    /* LCD Control Register */
    LCDIMSC_REG		=	0x1c,    /* Interrupt Mask Set/Clear Register */
    LCDRIS_REG		=	0x20,    /* Raw Interrupt Status Register */
    LCDMIS_REG		=	0x24,    /* Masked Interrupt Status Register */
    LCDICR_REG		=	0x28,    /* LCD Interrupt Clear Register */
    LCDUPCURR_REG	=	0x2c,    /* LCD Upper Panel Current Address Value Registers */
    LCDLPCURR_REG	=	0x30,    /* LCD Lower Panel Current Address Value Registers */

    LCDPalette_REG		=	0x200,    /* 256x16-bit Color Palette Registers */
    CursorImage_REG	=	0x800,    /* Cursor Control Register */

    ClcdCrsrCtrl_REG	=	0xc00,    /* Cursor Control Register */
    ClcdCrsrConfig_REG	=	0xc04,    /* Cursor Configuration Register */
    ClcdCrsrPalette0_REG=	0xc08,    /* Cursor Palette Registers */
    ClcdCrsrPalette1_REG=	0xc0c,    /* Cursor Palette Registers */
    ClcdCrsrXY_REG		=	0xc10,    /* Cursor XY Position Register */
    ClcdCrsrClip_REG	=	0xc14,    /* Cursor Clip Position Register */

    ClcdCrsrIMSC_REG	=	0xc20,    /* Cursor Interrupt Mask Set/Clear Register */
    ClcdCrsrICR_REG	=	0xc24,    /* Cursor Interrupt Clear Register */
    ClcdCrsrRIS_REG	=	0xc28,    /* Cursor Raw Interrupt Status Register */
    ClcdCrsrMIS_REG	=	0xc2c,    /* Cursor Masked Interrupt Status Register */

    ClcdTCR_REG		= 0xf00,
    
    // Added for A1 ECO Chip
    ClcdITOP1_REG		= 0xf04,
    ClcdITOP2_REG		= 0xf08,

    CLCDPeriphID0_REG    = 0xfe0,    /* (reset=0x11) Peripheral Identification Register0 */
    CLCDPeriphID1_REG    = 0xfe4,    /* (reset=0x11) Peripheral Identification Register1 */
    CLCDPeriphID2_REG    = 0xfe8,    /* Peripheral Identification Register2 */
    CLCDPeriphID3_REG    = 0xfec,    /* Peripheral Identification Register3 */
    CLCDPCellID0_REG     = 0xff0,    /* (reset=0x0d) PrimeCell Identification Register0 */
    CLCDPCellID1_REG     = 0xff4,    /* (reset=0xf0) PrimeCell Identification Register1 */
    CLCDPCellID2_REG     = 0xff8,    /* (reset=0x05) PrimeCell Identification Register2 */
    CLCDPCellID3_REG     = 0xffc,    /* (reset=0xb1) PrimeCell Identification Register3 */
} CLCD_REGS;

/*******************************************/
/*     Registers detailed descriptions     */
/*******************************************/
//    LCDTiming0_REG       =  0x00,    /* Horizontal Axis Panel Control Register */
//    LCDTiming1_REG       =  0x04,    /* Vertical Axis Panel Control Register */
//    LCDTiming2_REG       =  0x08,    /* Clock and Signal Polarity Control Register */
    // The names of bit fileds are already defined in amba/clcd.h
    //#define TIM2_CLKSEL	(1 << 5) 
    //#define TIM2_IVS		(1 << 11)
    //#define TIM2_IHS		(1 << 12)
    //#define TIM2_IPC		(1 << 13)
    //#define TIM2_IOE		(1 << 14)
    //#define TIM2_BCD		(1 << 26)
//    LCDTiming3_REG       =  0x0c,    /* Line End Control Register */
//    LCDUPBASE_REG        =  0x10,    /* Upper and Lower Panel Frame Base Address Registers */
//    LCDLPBASE_REG        =  0x14,    /* Upper and Lower Panel Frame Base Address Registers */
//    LCDControl_REG       =  0x18,    /* LCD Control Register */
    // The names of bit fileds are already defined in amba/clcd.h
    //#define CNTL_LCDEN	(1 << 0)	// CLCDC enable, 1=enable CLLP, CLCP, CLEP, CLAC, and CLLE
    //#define CNTL_LCDBPP1	(0 << 1)	// b000=1bpp
    //#define CNTL_LCDBPP2	(1 << 1)	// b001=2bpp
    //#define CNTL_LCDBPP4	(2 << 1)	// b010=4bpp
    //#define CNTL_LCDBPP8	(3 << 1)	// b011=8bpp
    //#define CNTL_LCDBPP16	(4 << 1)	// b100=16bpp
    //#define CNTL_LCDBPP16_565	(6 << 1)	// b110=16bpp 5:6:5 mode
    //#define CNTL_LCDBPP24	(5 << 1)	// b101=24bpp
    //#define CNTL_LCDBW	(1 << 4)	// STN LCD is monochrome, 1=mono, 0=color
    //#define CNTL_LCDTFT	(1 << 5)	// LCD is TFT, 1=TFT, 0=STN
    //#define CNTL_LCDMONO8	(1 << 6)	// Mono LCD. This has an 8-bit interface.
    //#define CNTL_LCDDUAL	(1 << 7)	// LCD interface 1=dual-panel, 0=single-panel
    //#define CNTL_BGR		(1 << 8)	// 1=BGR red and blue swapped. 0=RGB normal output
    //#define CNTL_BEBO		(1 << 9)	// 1=Big-/0=little-endian
    //#define CNTL_BEPO		(1 << 10)	// 1=big-/0=little-endian pixel ordering within a byte
    //#define CNTL_LCDPWR	(1 << 11)	// 1=power gated through to LCD panel and CLD(23:0) signals enabled
						// 0=power not gated through to LCD panel and CLD[23:0] signals disabled
    //#define CNTL_LCDVCOMP(x)	((x) << 12)	// b00=start of vertical synchronization
						// b01=start of back porch
						// b10=start of active video
						// b11=start of front porch
    //#define CNTL_WATERMARK	(1 << 16)	// LCD DMA FIFO watermark level
						// 1=asserts HBUSREQM when either of the DMA FIFOs have eight or more empty locations
						// 0=asserts HBUSREQM when either of the DMA FIFOs have four or more empty locations
//    LCDIMSC_REG          =  0x1c,    /* Interrupt Mask Set/Clear Register */

//    LCDRIS_REG           =  0x20,    /* Raw Interrupt Status Register */
    #define MBERRORRIS  (1 << 4)	// AHB master error interrupt status
    #define VcompRIS    (1 << 3)	// Vertical compare interrupt status
    #define LNBURIS     (1 << 2)	// LCD next base address update, mode dependent, set when the current base address registers have been successfully updated by the next address registers. 
    #define FUFRIS      (1 << 1)	// FIFO underflow, set when either the upper or lower DMA FIFOs have been read accessed when empty, causing an underflow condition to occur.
//    LCDMIS_REG           =  0x24,    /* Masked Interrupt Status Register */

//    LCDICR_REG           =  0x28,    /* LCD Interrupt Clear Register */
    #define MBERRORIC   (1 << 4)	// Clear AHB master error interrupt
    #define VcompIC     (1 << 3)	// Clear vertical compare interrupt
    #define LNBUIC      (1 << 2)	// Clear LCD next base address update interrupt
    #define FUFIC       (1 << 1)	// Clear FIFO underflow interrupt
//    LCDUPCURR_REG        =  0x2c,    /* LCD Upper and Lower Panel Current Address Value Registers */
//    LCDLPCURR_REG        =  0x30,    /* LCD Upper and Lower Panel Current Address Value Registers */
//
//    LCDPalette_REG       = 0x200,    /* 256x16-bit Color Palette Registers */
//    CursorImage_REG      = 0x800,    /* Cursor Control Register */
//
//    ClcdCrsrCtrl_REG     = 0xc00,    /* Cursor Control Register */
//    ClcdCrsrConfig_REG   = 0xc04,    /* Cursor Configuration Register */
//    ClcdCrsrPalette0_REG = 0xc08,    /* Cursor Palette Registers */
//    ClcdCrsrPalette1_REG = 0xc0c,    /* Cursor Palette Registers */
//    ClcdCrsrXY_REG       = 0xc10,    /* Cursor XY Position Register */
//    ClcdCrsrClip_REG     = 0xc14,    /* Cursor Clip Position Register */
//
//    ClcdCrsrIMSC_REG     = 0xc20,    /* Cursor Interrupt Mask Set/Clear Register */
//    ClcdCrsrICR_REG      = 0xc24,    /* Cursor Interrupt Clear Register */
//    ClcdCrsrRIS_REG      = 0xc28,    /* Cursor Raw Interrupt Status Register */
//    ClcdCrsrMIS_REG      = 0xc2c,    /* Cursor Masked Interrupt Status Register */
//
//	ClCDTCR_REG	   = 0xf00,
	#define ENABLE_TESTMODE	(1 << 0)	/*  on-chip frame buffer feature is enabled. SJ 20120402  */
	
//	ClcdITOP1_REG	   = 0xf04,
	#define ENABLE_INCRALL	(1 << 3)	/*  on-chip frame buffer feature is enabled. SJ 20120402  */
	#define ENABLE_OCFB		(1 << 4)	/*  on-chip frame buffer feature is enabled. SJ 20120402  */

	
#else
/*
 * linux/include/asm-arm/hardware/regs_clcd.h -- Integrator LCD panel.
 *
 * David A Rusling
 *
 * Copyright (C) 2001 ARM Limited
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * CLCD Controller Internal Register addresses
 */
#define CLCD_TIM0		0x00000000
#define CLCD_TIM1 		0x00000004
#define CLCD_TIM2 		0x00000008
#define CLCD_TIM3 		0x0000000c
#define CLCD_UBAS 		0x00000010
#define CLCD_LBAS 		0x00000014
#define CLCD_CNTL 		0x00000018
#define CLCD_IENB 		0x0000001c
#define CLCD_RIS		0x00000020
#define CLCD_MIS		0x00000024
#define MBERRORRIS	(1 << 4)	// AHB master error interrupt status

#define MBERRORMIS	(1 << 4)	// AHB master error interrupt status
#define VcompMIS	(1 << 3)	// Vertical compare interrupt status
#define LNBUMIS 	(1 << 2)	// LCD next base address update interrupt status bit
#define FUFMIS		(1 << 1)	// FIFO underflow, interrupt status bit

#define CLCD_ICR		0x00000028
#define CLCD_UCUR		0x0000002c
#define CLCD_LCUR		0x00000030

#define CLCD_PALL 		0x00000200
#define CLCD_PALETTE		0x00000200

#define TIM2_CLKSEL		(1 << 5)
#define TIM2_IVS		(1 << 11)
#define TIM2_IHS		(1 << 12)
#define TIM2_IPC		(1 << 13)
#define TIM2_IOE		(1 << 14)
#define TIM2_BCD		(1 << 26)

#define CNTL_LCDEN		(1 << 0)
#define CNTL_LCDBPP1		(0 << 1)
#define CNTL_LCDBPP2		(1 << 1)
#define CNTL_LCDBPP4		(2 << 1)
#define CNTL_LCDBPP8		(3 << 1)
#define CNTL_LCDBPP16		(4 << 1)
#define CNTL_LCDBPP16_565	(6 << 1)
#define CNTL_LCDBPP24		(5 << 1)
#define CNTL_LCDBW		(1 << 4)
#define CNTL_LCDTFT		(1 << 5)
#define CNTL_LCDMONO8		(1 << 6)
#define CNTL_LCDDUAL		(1 << 7)
#define CNTL_BGR		(1 << 8)
#define CNTL_BEBO		(1 << 9)
#define CNTL_BEPO		(1 << 10)
#define CNTL_LCDPWR		(1 << 11)
#define CNTL_LCDVCOMP(x)	((x) << 12)
#define CNTL_LDMAFIFOTIME	(1 << 15)
#define CNTL_WATERMARK		(1 << 16)



#endif
#endif	// __CS752X_CLCD_REGS_H
