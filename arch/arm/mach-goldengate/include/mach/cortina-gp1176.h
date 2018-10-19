/*
 * arch/arm/mach-realview/include/mach/board-pb1176.h
 *
 * Copyright (C) 2008 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __ASM_ARCH_CORTINA_GP1176_H
#define __ASM_ARCH_CORTINA_GP1176_H

#include <mach/platform.h>

/*
 * Peripheral addresses
 */
#define GEMINIP_DRAM_BASE			0x00000000
#define GEMINIP_FLASH_BASE			0x80000000
#define GEMINIP_SDRAM_BASE			0x90000000
#define GEMINIP_SRAM0_BASE			0x9FFE0000
#define GEMINIP_SRAM1_BASE			0x9FFF0000
#define GEMINIP_PCI_MEM_BASE		0xA0000000

/* AHB1 */
#define GEMINIP_DDRC_BASE			0xF0000000
#define GEMINIP_AHCI_BASE			0xF0100000
#define GEMINIP_PCIE0_BASE			0xF0200000
#define GEMINIP_PCIE1_BASE			0xF0300000
#define GEMINIP_DMA_BASE			0xF0400000
#define GEMINIP_SECURE_BASE			0xF0500000
#define GEMINIP_APB0_BASE			0xF0600000
#define GEMINIP_APB1_BASE			0xF0700000

/* AHB2 */
#define GEMINIP_FLASH_CTRL_BASE		0xF0800000
#define GEMINIP_NEMAC_BASE			0xF0900000
#define GEMINIP_OHCI_BASE			0xF0A00000
#define GEMINIP_EHCI_BASE			0xF0B00000
#define GEMINIP_USB_DEVICE_BASE		0xF0C00000
#define GEMINIP_LCDC_BASE			0xF0D00000
#define GEMINIP_SDC_BASE			0xF0E00000

/* APB0 */
#define GEMINIP_TIME0_BASE			0xF0600000
#define GEMINIP_WDT0_BASE			0xF0610000
#define GEMINIP_GPIO0_BASE			0xF0620000
#define GEMINIP_GPIO1_BASE			0xF0630000
#define GEMINIP_UART0_BASE			0xF0640000
#define GEMINIP_UART1_BASE			0xF0650000
#define GEMINIP_SSP0_BASE			0xF0660000
#define GEMINIP_SSP1_BASE			0xF0670000
#define GEMINIP_CIR_BASE			0xF0680000
#define GEMINIP_POWERC_BASE			0xF0690000
#define GEMINIP_RTC_BASE			0xF06A0000
#define GEMINIP_SMI_BASE			0xF06B0000
#define GEMINIP_INTERRUPT_BASE		0xF06C0000

/* APB1 */
#define GEMINIP_TIME1_BASE			0xF0700000
#define GEMINIP_WDT1_BASE			0xF0710000
#define GEMINIP_GPIO2_BASE			0xF0720000
#define GEMINIP_UART2_BASE			0xF0730000
#define GEMINIP_UART3_BASE			0xF0740000
#define GEMINIP_SSP2_BASE			0xF0750000
#define GEMINIP_SSP3_BASE			0xF0760000
#define GEMINIP_TLCD_BASE			0xF0770000
#define GEMINIP_GLOBAL_BASE			0xF0780000


/*
 * GEMINI PLUS 1176 Interrupt sources
 */
#define GEMINIP_IRQ_TIMER0		8
#define GEMINIP_IRQ_TIMER1		9
#define GEMINIP_IRQ_TIMER2		10
#define GEMINIP_IRQ_WDT			3
#define GEMINIP_IRQ_GPIO0		4
#define GEMINIP_IRQ_GPIO1		5
#define	GEMINIP_IRQ_UART0		12
#define GEMINIP_IRQ_SSP0		7
#define GEMINIP_IRQ_SSP1		8
#define GEMINIP_IRQ_CIR			9
#define GEMINIP_IRQ_PWC			10

/*
 * Synopsys ICTL registers
 */
#define ICTL_ENABLE_L				0x00
#define ICTL_ENABLE_H				0x04
#define ICTL_MASK_L					0x08
#define ICTL_MASK_H					0x0C
#define ICTL_FORCE_L				0x10
#define ICTL_FORCE_H				0x14
#define ICTL_RAWSTAT_L				0x18
#define ICTL_RAWSTAT_H				0x1C
#define ICTL_STAT_L					0x20
#define ICTL_STAT_H					0x24
#define ICTL_MASKTAT_L				0x28
#define ICTL_MASKSTAT_H				0x2C
#define ICTL_FINALSTAT_L			0x30
#define ICTL_FINALSTAT_H			0x34
#define ICTL_VECTOR					0x38
#define ICTL_VECTOR0				0x40
#define ICTL_VECTOR1				0x48
#define ICTL_VECTOR2				0x50
#define ICTL_VECTOR3				0x58
#define ICTL_VECTOR4				0x60
#define ICTL_VECTOR5				0x68
#define ICTL_VECTOR6				0x70
#define ICTL_VECTOR7				0x78
#define ICTL_VECTOR8				0x80
#define ICTL_VECTOR9				0x88
#define ICTL_VECTOR10				0x90
#define ICTL_VECTOR11				0x98
#define ICTL_VECTOR12				0xA0
#define ICTL_VECTOR13				0xA8
#define ICTL_VECTOR14				0xB0
#define ICTL_VECTOR15				0xB8
#define ICTL_FIQ_ENABLE				0xC0
#define ICTL_FIQ_MASK				0xC4
#define ICTL_FIQ_FORCE				0xC8
#define ICTL_FIQ_RAWSTAT			0xCC
#define ICTL_FIQ_STAT				0xD0
#define ICTL_FIQ_FINALSTAT			0xD4
#define ICTL_FIQ_PLEVEL				0xD8
#define ICTL_FIQ_IPLEVEL			0xDC
#define NR_IRQS						32

/*
 * Synopsys timer registers
 */
#define GEMINIP_TIMERN_LOAD			0x00
#define GEMINIP_TIMERN_CURRENT		0x04
#define GEMINIP_TIMERN_CTL			0x08
#define GEMINIP_TIMERN_EOI			0x0C
#define GEMINIP_TIMERN_INTSTAT		0x10
#define GEMINIP_TIMER_INTSTATUS		0xA0
#define GEMINIP_TIMER_EOI			0xA4
#define GEMINIP_TIMER_RAWINTSTAT	0xA8
#define GEMINIP_TIMER_VERSION		0xAC


/*
 * CLOCK definetion
 */
#ifdef CONFIG_CORTINA_GP1176_FPGA
#define SYSTEM_CLOCK		25000000
#else
#define SYSTEM_CLOCK		700000000
#endif

#ifdef CONFIG_CORTINA_GP1176_FPGA
#define APB_CLOCK			SYSTEM_CLOCK
#else
#define APB_CLOCK			(SYSTEM_CLOCK/6)
#endif

#define UART_CLOCK			APB_CLOCK

#define GEMINIP_BAUD_230400		(UART_CLOCK / (16*230400))
#define GEMINIP_BAUD_115200		(UART_CLOCK / (16*115200))
#define GEMINIP_BAUD_57600		(UART_CLOCK / (16*57600))
#define GEMINIP_BAUD_38400		(UART_CLOCK / (16*38400))
#define GEMINIP_BAUD_19200		(UART_CLOCK / (16*19200))
#define GEMINIP_BAUD_14400		(UART_CLOCK / (16*14400))
#define GEMINIP_BAUD_9600		(UART_CLOCK / (16*9600))

#endif	/* __ASM_ARCH_CORTINA_GP1176_H */
