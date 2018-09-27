/*
 *  arch/arm/mach-goldengate/include/mach/g2_flash.h
 *
 *  This file contains the hardware definitions of the GoldenGate platform.
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Jason Li <jason.li@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_G2_FLASH_H
#define __ASM_ARCH_G2_FLASH_H

#include <asm/sizes.h>
#include <mach/platform.h>

#define FLASH_START                                     GOLDENGATE_FLASH_BASE
#define SFLASH_SIZE                      		0x00400000
#define SPAGE_SIZE                       		0x200
#define BLOCK_ERASE                     		0x50
#define BUFFER1_READ                    		0x54
#define BUFFER2_READ                    		0x56
#define PAGE_ERASE                      		0x81
#define MAIN_MEMORY_PAGE_READ           		0x52
#define MAIN_MEMORY_PROGRAM_BUFFER1     		0x82
#define MAIN_MEMORY_PROGRAM_BUFFER2     		0x85
#define BUFFER1_TO_MAIN_MEMORY          		0x83
#define BUFFER2_TO_MAIN_MEMORY          		0x86
#define MAIN_MEMORY_TO_BUFFER1          		0x53
#define MAIN_MEMORY_TO_BUFFER2          		0x55
#define BUFFER1_WRITE                   		0x84
#define BUFFER2_WRITE                   		0x87
#define AUTO_PAGE_REWRITE_BUFFER1       		0x58
#define AUTO_PAGE_REWRITE_BUFFER2       		0x59
#define READ_STATUS                     		0x57

#define MAIN_MEMORY_PAGE_READ_SPI       		0xD2
#define BUFFER1_READ_SPI                		0xD4
#define BUFFER2_READ_SPI                		0xD6
#define READ_STATUS_SPI                 		0xD7

#define SERIAL_FLASH_CHIP1_EN           0x00010000  // 16th bit = 1
#define SERIAL_FLASH_CHIP0_EN           0x00000000  // 16th bit = 0 
#define AT45DB321_PAGE_SHIFT		        0x9  
#define AT45DB321_BLOCK_SHIFT				0xc
#define AT45DB642_PAGE_SHIFT		        0xa  
#define AT45DB642_BLOCK_SHIFT				0xd
#define CONTINUOUS_MODE		         			0x00008000  

//flash start bits
#define FIFO_START_BIT_ENABLE		         	0x00000004  
#define SF_START_BIT_ENABLE		         	0x00000002 
#define NF_START_BIT_ENABLE		         	0x00000001  

#define FLASH_ACCESS_ACTION_OPCODE                      0x0000
#define FLASH_ACCESS_ACTION_OPCODE_DATA                 0x0100
#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS               0x0200
#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_DATA          0x0300
#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_X_DATA          0x0400
#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_2X_DATA         0x0500
#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_3X_DATA         0x0600
#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_4X_DATA         0x0700
//#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_X_DATA        0x0600
//#define FLASH_ACCESS_ACTION_SHIFT_ADDRESS_4X_DATA       0x0700


#define SF_AC_OPCODE                	0x0000
#define SF_AC_OPCODE_1_DATA           	0x0100
#define SF_AC_OPCODE_2_DATA           	0x0200
#define SF_AC_OPCODE_3_DATA           	0x0300
#define SF_AC_OPCODE_4_DATA           	0x0400
#define SF_AC_OPCODE_4_ADDR         	0x0500
#define SF_AC_OPCODE_4_ADDR_1_DATA    	0x0600
#define SF_AC_OPCODE_4_ADDR_2_DATA    	0x0700
#define SF_AC_OPCODE_4_ADDR_3_DATA    	0x0800
#define SF_AC_OPCODE_4_ADDR_4_DATA    	0x0900
#define SF_AC_OPCODE_4_ADDR_X_1_DATA   	0x0A00
#define SF_AC_OPCODE_4_ADDR_X_2_DATA   	0x0B00
#define SF_AC_OPCODE_4_ADDR_X_3_DATA   	0x0C00
#define SF_AC_OPCODE_4_ADDR_X_4_DATA   	0x0D00
#define SF_AC_OPCODE_4_ADDR_4X_1_DATA  	0x0E00

//status register 
//for atmel
#define	SFLASH_STS_READY             	0x80 //write protect
#define	SFLASH_STS_COMP             	0x10	//block protect bit2
#define SFLASH_FORCEBURST		0x2000
#define SFLASH_FORCETERM		0x1000


//for ST
#define	SFLASH_STS_SRWD             	0x80 //write protect
#define	SFLASH_STS_BP2             	0x10	//block protect bit2
#define	SFLASH_STS_BP1             	0x08	//block protect bit1
#define	SFLASH_STS_BP0             	0x04	//block protect bit0
#define	SFLASH_STS_WEL             	0x02	//write enable latch bit
#define	SFLASH_STS_WIP             	0x01	//write in progress bit

#define SPAGE_SIZE                       		0x200


#define M25P80_PAGE_SIZE  0x100
#define M25P80_SECTOR_SIZE  0x10000


//#define M25P80_BULK_ERASE                                      1
//#define M25P80_SECTOR_ERASE                                    2
//#define M25P80_SECTOR_SIZE                                     0x10000

#define M25P80_WRITE_ENABLE                  		0x06
#define M25P80_WRITE_DISABLE                 		0x04
#define M25P80_READ_STATUS                   		0x05
#define M25P80_WRITE_STATUS              		0x01
#define M25P80_READ                      		0x03
#define M25P80_FAST_READ                 		0x0B
#define M25P80_PAGE_PROGRAM              		0x02 
#define M25P80_SECTOR_ERASE              		0xD8 
#define M25P80_BULK_ERASE                		0xC7 
#define FLASH_ERR_OK							0x0


#define	NCNT_EMPTY_OOB    			0x3FF
#define	NCNT_512P_OOB    				0x0F
#define	NCNT_2kP_OOB    				0x3F
#define	NCNT_4kP_OOB    				0x7F
#define	NCNT_M4kP_OOB    				0xdF
#define	NCNT_EMPTY_DATA    				0x3FFF
#define	NCNT_512P_DATA    				0x1FF
#define	NCNT_2kP_DATA    				0x7FF
#define	NCNT_4kP_DATA    				0xFFF
#define	NCNT_DATA_1    					0x0
#define	NCNT_DATA_2    					0x1
#define	NCNT_DATA_3    					0x2
#define	NCNT_DATA_4    					0x3
#define	NCNT_DATA_5    					0x4
#define	NCNT_DATA_6    					0x5
#define	NCNT_DATA_7    					0x6
#define	NCNT_DATA_8    					0x7

#define	NCNT_EMPTY_ADDR    			0x7
#define	NCNT_ADDR_5	    				0x4
#define	NCNT_ADDR_4	    				0x3
#define	NCNT_ADDR_3	    				0x2
#define	NCNT_ADDR_2	    				0x1
#define	NCNT_ADDR_1	    				0x0
#define	NCNT_EMPTY_CMD    			0x3
#define	NCNT_CMD_3    					0x2
#define	NCNT_CMD_2    					0x1
#define	NCNT_CMD_1    					0x0



#define	NFLASH_WiDTH8              0x0
#define	NFLASH_WiDTH16             0x1
#define	NFLASH_WiDTH32             0x2
#define NFLASH_CHIP0_EN            0x0
#define NFLASH_CHIP1_EN            0x1
//#define	NFLASH_DIRECT              0x00004000
//#define	NFLASH_INDIRECT            0x00000000

#define	ECC_DONE             		0x1	//ECC generation complete
#define	NF_RESET             		0x1	//ECC generation complete
#define	FIFO_CLR             		0x1	//ECC generation complete
#define	ECC_CLR             		0x1	//ECC generation complete
#define	FLASH_GO             		0x1	//ECC generation complete

// ECC comparison result,
#define	ECC_UNCORRECTABLE       0x3
#define	ECC_1BIT_DATA_ERR       0x1
#define	ECC_1BIT_ECC_ERR        0x2
#define	ECC_NO_ERR             	0x0

#define	ECC_ENABLE           		0x1	//ECC generation complete
#define	ECC_GEN_256          		0x0	//NAND Flash ECC Generation Mode 256
#define	ECC_GEN_512          		0x1	//NAND Flash ECC Generation Mode 512
#define	ECC_PAUSE_EN            0x1

#define	FLASH_RD             	0x2
#define	FLASH_WT             	0x3

#define	FLASH_SERIAL  	           	0x0	//Serial Flash
#define	FLASH_PARALLEL             	0x1	//Parallel Flash
#define	FLASH_NAND_512P            	0x4	//NAND Flash with 512B page size
#define	FLASH_NAND_2KP             	0x5	//NAND Flash with 2KB page size
#define	FLASH_NAND_4KP             	0x6	//Samsung NAND Flash with 4KB page size
#define	FLASH_NAND_M4KP            	0x7	//Micron NAND Flash with 4KB page size

#define	FLASH_WIDTH8_STM          	0x0	//8 bits width of flash for NAND and Parallel flash, but STMicroelectronic-compatible for Serial flash
#define	FLASH_WIDTH16_AT          	0x1	//16 bits width of flash for NAND and Parallel flash, but Atmel-Compatible for Serial flash

#define	FLASH_SIZE_STM16MB          	0x0
#define	FLASH_SIZE_STM32MB          	0x1

#define	FLASH_SIZE_AT1MB 		         	0x0
#define	FLASH_SIZE_AT2MB	          	0x1
#define	FLASH_SIZE_AT8MB	          	0x2
#define	FLASH_SIZE_AT16MB	          	0x3

#define	FLASH_SIZE_NP512_32MB        	0x0
#define	FLASH_SIZE_NP512_64MB        	0x1
#define	FLASH_SIZE_NP512_128MB       	0x2

#define	FLASH_SIZE_NP2K_128MB        	0x0
#define	FLASH_SIZE_NP2K_256MB        	0x1
#define	FLASH_SIZE_NP2K_512MB       	0x2
#define	FLASH_SIZE_NP2K_1GMB	       	0x3

#define	FLASH_SIZE_NP4K_1GMB        	0x0
#define	FLASH_SIZE_NP4K_2GMB        	0x1
#define	FLASH_SIZE_NP4K_4GMB	       	0x2

// jenfeng8K
#define FLASH_SIZE_NP8K_CONF		0x4


//bch
#define	BCH_ENABLE	       	0x1
#define	BCH_DISABLE	       	0x0
#define	BCH_DECODE	       	0x1
#define	BCH_ENCODE	       	0x0
// BCH ECC comparison result,
#define	BCH_UNCORRECTABLE       	0x3
#define	BCH_CORRECTABLE_ERR         0x2
#define	BCH_NO_ERR             		0x1
#define	BCH_ING		             	0x0

#define BCH_ERR_CAP_8_512	0x0
#define BCH_ERR_CAP_12_512	0x1


//#define	DWIDTH             NFLASH_WiDTH8
//#define	PAGE512_SIZE              	0x200
//#define	PAGE512_OOB_SIZE              	0x10
//#define	PAGE512_RAW_SIZE              	0x210
//#define	PAGE2K_SIZE              		0x800
//#define	PAGE2K_OOB_SIZE              	0x40
//#define	PAGE2K_RAW_SIZE              	0x840
//#define	FLASH_TYPE_MASK             	0x1800
//#define	FLASH_WIDTH_MASK             	0x0400
//#define	FLASH_SIZE_MASK             	0x0300
//#define	FLASH_SIZE_32 	            	0x0000
//#define	FLASH_SIZE_64 	            	0x0100
//#define	FLASH_SIZE_128 	            	0x0200
//#define	FLASH_SIZE_256 	            	0x0300
//#define	FLASH_TYPE_NAND             	0x1000
//#define	FLASH_TYPE_NOR             	0x0800
//#define	FLASH_TYPE_SERIAL             	0x0000
//#define	FLASH_START_BIT             	0x80000000
//#define	FLASH_RD             	0x00002000
//#define	FLASH_WT             	0x00003000
//#define	ECC_CHK_MASK             	0x00000003
//#define	ECC_UNCORRECTABLE             	0x00000003
//#define	ECC_1BIT_DATA_ERR             	0x00000001
//#define	ECC_1BIT_ECC_ERR             	0x00000002
//#define	ECC_NO_ERR             	0x00000000
//#define	ECC_ERR_BYTE             	0x0000ff80
//#define	ECC_ERR_BIT             	0x00000078
//


#define	FLASH_CLR_FIFO             	0x8000

#define	STS_WP             	0x80
#define	STS_READY             	0x40
#define	STS_TRUE_READY             	0x40
#define	NFLASH_ENABLE             	0x00000004
#define	GLOBAL_MISC_CTRL			0x30

//extern void g2_address_to_page(__u32, __u16 *, __u16 *);
//extern void g2_main_memory_page_read(__u8, __u16, __u16, __u8 *);
//extern void g2_buffer_to_main_memory(__u8, __u16);
//extern void g2_main_memory_to_buffer(__u8, __u16);
//extern void g2_main_memory_page_program(__u8, __u16, __u16, __u8);
//extern void g2_atmel_flash_read_page(__u32, __u8 *, __u32);
//extern void g2_atmel_erase_page(__u8, __u16);
//extern void g2_atmel_read_status(__u8, __u8 *);
//extern void g2_atmel_flash_program_page(__u32, __u8 *, __u32);
//extern void g2_atmel_buffer_write(__u8, __u16, __u8);
//extern void flash_delay(void);

//define DMA parameter
#define FDMA_DEPTH	3
#define FDMA_DESC_NUM	(1<<FDMA_DEPTH) 


typedef struct tx_descriptor_t {
	
	union tx_word0_t
	{
  		struct {
#   	ifdef CS_BIG_ENDIAN
    	cs_uint32 own        				:  1 ;  /* bits 31:31 */
    	cs_uint32 share_rsrvd          		:  1 ;  /* bits 30 */
    	cs_uint32 cache_rsrvd 				:  1 ;  /* bits 29 */
    	cs_uint32 sof_eof_rsrvd   			:  2 ;  /* bits 28:27 */
    	cs_uint32 sgm_rsrvd   				:  5 ;  /* bits 26:22 */
    	cs_uint32 desccnt     				:  6 ;  /* bits 21:16 */
    	cs_uint32 buf_size           		:  16 ; /* bits 15:0 */
#   	else /* CS_LITTLE_ENDIAN */
    	cs_uint32 buf_size           		:  16 ; /* bits 15:0 */
    	cs_uint32 desccnt     				:  6 ;  /* bits 21:16 */
    	cs_uint32 sgm_rsrvd   				:  5 ;  /* bits 26:22 */
    	cs_uint32 sof_eof_rsrvd   			:  2 ;  /* bits 28:27 */
    	cs_uint32 cache_rsrvd 				:  1 ;  /* bits 29 */
    	cs_uint32 share_rsrvd          		:  1 ;  /* bits 30 */
    	cs_uint32 own        				:  1 ;  /* bits 31:31 */
#   	endif
  		} bf ;
  		cs_uint32     wrd ;
	}word0;
	
	cs_uint32 buf_adr;	/* data buffer address */
	cs_uint32 word2;	/* data buffer address */
	cs_uint32 word3;	/* data buffer address */
	
	
} DMA_SSP_TX_DESC_T;


typedef struct rx_descriptor_t {
	
	union rx_word0_t
	{
  		struct {
#   	ifdef CS_BIG_ENDIAN
    	cs_uint32 own        				:  1 ;  /* bits 31 */
    	cs_uint32 share_rsrvd          		:  1 ;  /* bits 30 */
    	cs_uint32 cache_rsrvd 				:  1 ;  /* bits 29 */
    	cs_uint32 rqsts_rsrvd   			:  7 ;  /* bits 28:22 */
    	cs_uint32 desccnt     				:  6 ;  /* bits 21:16 */
    	cs_uint32 buf_size           		:  16 ; /* bits 15:0 */
#   	else /* CS_LITTLE_ENDIAN */
    	cs_uint32 buf_size           		:  16 ; /* bits 15:0 */
    	cs_uint32 desccnt     				:  6 ;  /* bits 21:16 */
    	cs_uint32 rqsts_rsrvd   			:  7 ;  /* bits 28:22 */
    	cs_uint32 cache_rsrvd 				:  1 ;  /* bits 29 */
    	cs_uint32 share_rsrvd          		:  1 ;  /* bits 30 */
    	cs_uint32 own        				:  1 ;  /* bits 31 */
#   	endif
  		} bf ;
  		cs_uint32     wrd ;
	}word0;
	
	cs_uint32 buf_adr;	/* data buffer address */
	cs_uint32 word2;	/* data buffer address */
	cs_uint32 word3;	/* data buffer address */
	
	
} DMA_SSP_RX_DESC_T;

static __u32 read_flash_ctrl_reg(__u32 ofs)
{
    __u32 *base;	
    
    base = (__u32 *)IO_ADDRESS( ofs);
    return __raw_readl(base);
}

static void write_flash_ctrl_reg(__u32 ofs,__u32 data)
{
    __u32 *base;
    
    base = (__u32 *)IO_ADDRESS( ofs);
    __raw_writel(data, base);
}

static __u32 read_dma_ctrl_reg(__u32 ofs)
{
    __u32 *base;	
    
    base = (__u32 *)IO_ADDRESS(ofs);
    return __raw_readl(base);
}

static void write_dma_ctrl_reg(__u32 ofs,__u32 data)
{
    __u32 *base;
    
    base = (__u32 *)IO_ADDRESS(ofs);
    __raw_writel(data, base);
}

void cs752x_ecc_check_enable( int isEnable);
  
extern struct semaphore cs752x_flash_sem;


#endif

