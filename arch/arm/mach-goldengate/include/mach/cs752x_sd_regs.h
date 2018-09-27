/*
 * linux/arch/arm/mach-goldengate/include/mach/cs752x_sd_regs.h
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Reference: gemini_sd.h
 */
#ifndef __CS752X_MMC_REGS_H
#define __CS752X_MMC_REGS_H
#define    SAMPLE_REG     (0x00)
typedef enum cs752x_sd_regs {                                              
    CTRL_REG     = 0x00,    /* Control */
    PWREN_REG    = 0x04,    /* Power-enable */
    CLKDIV_REG   = 0x08,    /* Clock divider */
    CLKSRC_REG   = 0x0C,    /* Clock source */
    CLKENA_REG   = 0x10,    /* Clock enable */
    TMOUT_REG    = 0x14,    /* Timeout */
    CTYPE_REG    = 0x18,    /* Card type */
    BLKSIZ_REG   = 0x1C,    /* Block Size */
    BYTCNT_REG   = 0x20,    /* Byte count */
    INTMASK_REG  = 0x24,    /* Interrupt Mask */
    CMDARG_REG   = 0x28,    /* Command Argument */
    CMD_REG      = 0x2C,    /* Command */
    RESP0_SHORT_REG    = 0x30,    /* Response 0 and short response */
    RESP1_REG    = 0x34,    /* Response 1 */
    RESP2_REG    = 0x38,    /* Response 2 */
    RESP3_REG    = 0x3C,    /* Response 3 */
    MINTSTS_REG  = 0x40,    /* Masked interrupt status */
    RINTSTS_REG  = 0x44,    /* Raw interrupt status */
    STATUS_REG   = 0x48,    /* Status */
    FIFOTH_REG   = 0x4C,    /* FIFO threshold */
    CDETECT_REG  = 0x50,    /* Card detect */
    WRTPRT_REG   = 0x54,    /* Write protect */
    GPIO_REG     = 0x58,    /* General Purpose IO */
    TCBCNT_REG   = 0x5C,    /* Transferred CIU byte count */
    TBBCNT_REG   = 0x60,    /* Transferred host/DMA to/from byte count */
    DEBNCE_REG   = 0x64,    /* Card detect debounce */
    USRID_REG    = 0x68,    /* User ID */
    VERID_REG    = 0x6C,    /* Version ID */
    HCON_REG     = 0x70,    /* Hardware Configuration */
    BMOD_REG     = 0x80,    /* Bus Mode */
    PLDMND_REG   = 0x84,    /* Poll Demand */
    DBADDR_REG   = 0x88,    /* Descriptor List Base Address */
    IDSTS_REG    = 0x8C,    /* Internal DMAC Status */
    IDINTEN_REG  = 0x90,    /* Internal DMAC Interrupt Enable */
    DSCADDR_REG  = 0x94,    /* Current Host Descriptor Address */
    BUFADDR_REG  = 0x98,    /* Current Buffer Descriptor Address */
    FIFODATA_REG = 0x100,   /* FIFO data read write */
} MMC_REGS;

// CTRL_REG     = 0x00,    /* Control */
    #define CONTROLLER_RESET        0x00000001
    #define FIFO_RESET              0x00000002
    #define DMA_RESET               0x00000004
    //
    #define INT_ENABLE              0x00000010
    #define DMA_ENABLE              0x00000020
    #define READ_WAIT               0x00000040
    #define SEND_IRQ_RESPONSE       0x00000080
    #define ABORT_READ_DATA         0x00000100
    #define SEND_CCSD               0x00000200
    #define SEND_AUTO_STOP_CCSD     0x00000400
    //
    #define ENABLE_OD_PULLUP        0x01000000
    #define USE_INTERNAL_DMAC       0x02000000

// PWREN_REG    = 0x04,    /* Power-enable */
    #define G2_SDCARD0          0x00000001
// CLKDIV_REG   = 0x08,    /* Clock divider */
    #define MMCCLK_DIV_1        0x00
    #define MMCCLK_DIV_2        0x01
    #define MMCCLK_DIV_510      0xff
// CLKSRC_REG   = 0x0C,    /* Clock source */
// CLKENA_REG   = 0x10,    /* Clock enable */
// TMOUT_REG    = 0x14,    /* Timeout */
    #define G2_DATTMOUT         0xffffff00
    #define G2_CMDTMOUT         0x00000064
// CTYPE_REG    = 0x18,    /* Card type */
    #define G2_CARD0_4BIT       0x00000001
    #define G2_CARD0_8BIT       0x00010000
// BLKSIZ_REG   = 0x1C,    /* Block Size */
// BYTCNT_REG   = 0x20,    /* Byte count */
// INTMASK_REG  = 0x24,    /* Interrupt Mask */
// CMDARG_REG   = 0x28,    /* Command Argument */
// CMD_REG      = 0x2C,    /* Command */
    #define CMD_OPCODE_INDEX   (0x3f)	// sd opcode index
    #define CMD_RSP_EXP        (1<<6)	// 1=Response expected from card
    #define CMD_RSP_LONG_LEN   (1<<7)	// 1=Long response expected from card
    #define CMD_CHK_RSP_CRC    (1<<8)	// 1=Check response CRC
    #define CMD_DATA_EXP       (1<<9)	// 1=Data transfer expected (read/write)
    #define CMD_WRITE_CARD     (1<<10)	// 1=write to card; 0=read from card
    #define CMD_STREAM_XFER    (1<<11)	// 1=Stream data transfer command
    #define CMD_SND_AUTOSTOP   (1<<12)	// 1=Send stop command at end of data transfer
    #define CMD_WAIT_PDATA     (1<<13)	// 1=Wait for previous data transfer completion before sending command
                                        //     It is recommended that you not set the wait_prvdata_complete bit in the CMD register for data transfers with the IDMAC.
    #define CMD_STOP_ABTCMD    (1<<14)	// 1=Stop or abort command intended to stop current data transfer in progress.
    #define CMD_SEND_INIT      (1<<15)	// 1=Send initialization sequence before sending this command
    #define CMD_CARD00         0x00010000	// card select number 0
    #define CMD_CARD01         0x00020000	// card select number 1
    #define CMD_UP_CLKREG      (1<<21)	// 1=Do not send commands, just update clock register value into card clock domain
    #define CMD_READ_CEATA_DEV (1<<22)	// 1=Host is performing read access (RW_REG or RW_BLK) towards CE-ATA device
    #define CMD_CCS_EXP        (1<<23)	// 1=Interrupts are enabled in CE-ATA device (nIEN=0), and RW_BLK command expects command completion signal from CE-ATA device
    #define CMD_START          (1<<31)	// 1=Start command. When bit is set, host should not attempt to write to any command registers.
// RESP0_REG    = 0x30,    /* Response 0 */
// RESP1_REG    = 0x34,    /* Response 1 */
// RESP2_REG    = 0x38,    /* Response 2 */
// RESP3_REG    = 0x3C,    /* Response 3 */

// MINTSTS_REG  = 0x40,    /* Masked interrupt status */
// RINTSTS_REG  = 0x44,    /* Raw interrupt status */
    #define G2_INTMSK_ALL          0xffffffff	// mask all interrupts
    #define G2_INTMSK_CDT          0x00000001	// b0:  Card detect
    #define G2_INTMSK_RE           0x00000002	// b1:  Response error
    #define G2_INTMSK_CD           0x00000004	// b2:  Command done
    #define G2_INTMSK_DTO          0x00000008	// b3:  Data transfer over
    #define G2_INTMSK_TXDR         0x00000010	// b4:  Transmit FIFO data request
    #define G2_INTMSK_RXDR         0x00000020	// b5:  Receive FIFO data request
    #define G2_INTMSK_RCRC         0x00000040	// b6:  Response CRC error
    #define G2_INTMSK_DCRC         0x00000080	// b7:  Data CRC error
    #define G2_INTMSK_RTO          0x00000100	// b8:  Response timeout
    #define G2_INTMSK_DRTO         0x00000200	// b9:  Data read timeout
    #define G2_INTMSK_HTO          0x00000400	// b10: Data starvation-by-host timeout
    #define G2_INTMSK_FRUN         0x00000800	// b11: FIFO underrun/overrun error
    #define G2_INTMSK_HLE          0x00001000	// b12: Hardware locked write error
    #define G2_INTMSK_SBE          0x00002000	// b13: Start-bit error
    #define G2_INTMSK_ACD          0x00004000	// b14: Auto command done
    #define G2_INTMSK_EBE          0x00008000	// b15: End-bit error (read)/write no CRC
    #define G2_INTMSK_SDIO_INTR    0xffff0000	// Mask SDIO interrupts, in MMC-v3.3 mode, these bits are always 0
    #define G2_INTMSK_RESP_ERR    (G2_INTMSK_RE   | G2_INTMSK_RCRC | G2_INTMSK_RTO)
//#ifdef CONFIG_G2_SD_WORKQUEUE
//    #define G2_INTMSK_DATA_TX_ERR (G2_INTMSK_HTO | G2_INTMSK_EBE | G2_INTMSK_DCRC)
//    #define G2_INTMSK_DATA_RX_ERR (G2_INTMSK_HTO | G2_INTMSK_EBE | G2_INTMSK_DCRC | \
//                                   G2_INTMSK_DRTO | G2_INTMSK_SBE)
//    #define G2_INTMSK_FATAL_ERR   (G2_INTMSK_HLE | G2_INTMSK_FRUN)
//#else
    #define G2_INTMSK_DATA_XFER_ERR (G2_INTMSK_DCRC | G2_INTMSK_DRTO | G2_INTMSK_EBE)
    #define G2_INTMSK_DATA_TX_ERR G2_INTMSK_DATA_XFER_ERR
    #define G2_INTMSK_DATA_RX_ERR G2_INTMSK_DATA_XFER_ERR
    #define G2_INTMSK_FATAL_ERR   (G2_INTMSK_HLE | G2_INTMSK_HTO | G2_INTMSK_FRUN | G2_INTMSK_SBE)
//#endif
    #define G2_INTMSK_DATA_ERR    (G2_INTMSK_DATA_TX_ERR | G2_INTMSK_DATA_RX_ERR)
    #define G2_INTMSK_OTHERS      (G2_INTMSK_TXDR | G2_INTMSK_RXDR)

    #define G2_INTMSK_ERRORS      (G2_INTMSK_RESP_ERR | G2_INTMSK_DATA_ERR | G2_INTMSK_FATAL_ERR)

// STATUS_REG   = 0x48,    /* Status */
    #define G2_CMD_STATUS_MASK   0x000000f0	// 
    #define G2_CMD_IDLE          0x00000000	// idle
    #define G2_CMD_SEND_INIT     0x00000010	// 
    #define G2_CMD_T_START       0x00000020	// 
    #define G2_CMD_T_TX          0x00000030	// 
    #define G2_CMD_T_IDXARG      0x00000040	// 
    #define G2_CMD_T_CRC7        0x00000050	// 
    #define G2_CMD_T_END         0x00000060	// 
    #define G2_CMD_R_START       0x00000070	// 
    #define G2_CMD_R_IRQ_RESP    0x00000080	// 
    #define G2_CMD_R_TX          0x00000090	// 
    #define G2_CMD_R_CMDIDX      0x000000a0	// 
    #define G2_CMD_R_DATA        0x000000b0	// 
    #define G2_CMD_R_CRC7        0x000000c0	// 
    #define G2_CMD_R_END         0x000000d0	// 
    #define G2_CMD_NCC           0x000000e0	// 
    #define G2_CMD_WAIT          0x000000f0	// 
    #define G2_SD_DAT3_CD        (1<<8) // 1=Dat3 card exits
    #define G2_SD_DATA_BUSY      (1<<9) // 1=Data busy
    #define G2_SD_DATA_STATE_BUSY      (1<<10) // 1=Data state busy
// FIFOTH_REG   = 0x4C,    /* FIFO threshold */
// CDETECT_REG  = 0x50,    /* Card detect */
// WRTPRT_REG   = 0x54,    /* Write protect */
// GPIO_REG     = 0x58,    /* General Purpose IO */
// TCBCNT_REG   = 0x5C,    /* Transferred CIU byte count */
// TBBCNT_REG   = 0x60,    /* Transferred host/DMA to/from byte count */
// DEBNCE_REG   = 0x64,    /* Card detect debounce */
// USRID_REG    = 0x68,    /* User ID */
// VERID_REG    = 0x6C,    /* Version ID */
// HCON_REG     = 0x70,    /* Hardware Configuration */
// BMOD_REG     = 0x80,    /* Bus Mode */
    #define G2_BMOD_PBL_1       0x00000000	// 1 transfer
    #define G2_BMOD_PBL_4       0x00000100	// 4 transfers
    #define G2_BMOD_PBL_8       0x00000200	// 8 transfers
    #define G2_BMOD_PBL_16      0x00000300
    #define G2_BMOD_PBL_32      0x00000400
    #define G2_BMOD_PBL_64      0x00000500
    #define G2_BMOD_PBL_128     0x00000600
    #define G2_BMOD_PBL_256     0x00000700
    #define G2_BMOD_DE          0x00000080	// IDMAC Enable
    #define G2_BMOD_FB          0x00000002	// Fixed Burst
    #define G2_BMOD_SWR         0x00000001	// Software Reset
// PLDMND_REG   = 0x84,    /* Poll Demand */
    #define G2_ANY_VALUE_OK     0xffffffff
// DBADDR_REG   = 0x88,    /* Descriptor List Base Address */
// IDSTS_REG    = 0x8C,    /* Internal DMAC Status */
    #define G2_INTDMA_ALL       0x00000337      // mask all dma interrupts
// IDINTEN_REG  = 0x90,    /* Internal DMAC Interrupt Enable */
    #define G2_INTDMA_TI        0x00000001	// Transmit Interrupt
    #define G2_INTDMA_RI        0x00000002	// Receive Interrupt
    #define G2_INTDMA_FBE       0x00000004	// Fatal Bus Error Interrupt
    #define G2_INTDMA_DU        0x00000010	// Descriptor Unavailable Interrupt
    #define G2_INTDMA_CES       0x00000020	// Card Error Summary
    #define G2_INTDMA_NIS       0x00000100	// Normal Interrupt Summary
    #define G2_INTDMA_AIS       0x00000200	// Abnormal Interrupt Summary
    #define G2_INTDMA_EB        0x00001C00	// Error Bits (read-only for IDSTS)
    #define G2_INTDMA_FSM       0x0001E000	// DMAC FSM state machine (read-only for IDSTS)
// DSCADDR_REG  = 0x94,    /* Current Host Descriptor Address */
// BUFADDR_REG  = 0x98,    /* Current Buffer Descriptor Address */
// FIFODATA_REG = 0x100,   /* FIFO data read write */

#endif
