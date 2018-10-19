/*
 * arch/arm/mach-goldengate/include/mach/irqs-eb.h
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Jason Li <jason.li@cortina-systems.com>
 *
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

#ifndef __MACH_IRQS_EB_H
#define __MACH_IRQS_EB_H

#define IRQ_EB_GIC_START	32

/*
 * GoldenGate EB interrupt sources
 */
#define IRQ_GLOBAL_SYS		(IRQ_EB_GIC_START + 0)	/* PLL lock status */
#define IRQ_NET_ENG		(IRQ_EB_GIC_START + 1)	/* Network Engine aggregation */
#define IRQ_ARM_PARITY		(IRQ_EB_GIC_START + 2)	/* ARM memory parity errors */
#define IRQ_PERI_REGBUS		(IRQ_EB_GIC_START + 3)	/* Peripheral aggregation */
#define IRQ_L2_CTRL		(IRQ_EB_GIC_START + 4)	/* L2 cache status */
#define IRQ_SDRAM_CTRL		(IRQ_EB_GIC_START + 5)	/* SDRAM controller stats */
#define IRQ_UART0		(IRQ_EB_GIC_START + 6)	/* UART 0 IRQ */
#define IRQ_UART1		(IRQ_EB_GIC_START + 7)	/* UART 1 IRQ */
#define IRQ_UART2		(IRQ_EB_GIC_START + 8)	/* UART 2 IRQ */
#define IRQ_UART3		(IRQ_EB_GIC_START + 9)	/* UART 3 IRQ */
#define IRQ_RTC_ALM		(IRQ_EB_GIC_START + 10)	/* RTC Alarm interrupt */
#define IRQ_RTC_PRI		(IRQ_EB_GIC_START + 11)	/* RTC Periodic interrupt */
#define IRQ_WOL0		(IRQ_EB_GIC_START + 12)	/* Wake on Lan Port 0 */
#define IRQ_WOL1		(IRQ_EB_GIC_START + 13)	/* Wake on Lan Port 1 */
#define IRQ_WOL2		(IRQ_EB_GIC_START + 14)	/* Wake on Lan Port 2 */
#define IRQ_DMA			(IRQ_EB_GIC_START + 15)	/* DMA interrupt */
#define IRQ_SW0			(IRQ_EB_GIC_START + 16)	/* Software interruput 0 */
#define IRQ_SW1			(IRQ_EB_GIC_START + 17)	/* Software interruput 1 */
#define IRQ_FLASH_CTRL		(IRQ_EB_GIC_START + 18)	/* Flash Controlle Interrupt */
#define IRQ_TS_CTRL		(IRQ_EB_GIC_START + 19)	/* Transport Stream IRQ */
#define IRQ_CIR_CTRL		(IRQ_EB_GIC_START + 20)	/* CIR IRQ */
#define IRQ_PWR_CTRL		(IRQ_EB_GIC_START + 21)	/* Power Controller IRQ */
#define IRQ_MB0			(IRQ_EB_GIC_START + 22)	/* Recirc Mailbox 0 */
#define IRQ_MB1			(IRQ_EB_GIC_START + 23)	/* Recirc Mailbox 1 */
#define IRQ_CRYPT_ENG0		(IRQ_EB_GIC_START + 24)	/* Recirc Crypto core */
#define IRQ_CRYPT_ENG1		(IRQ_EB_GIC_START + 25)	/* Recirc Crypto core */
#define IRQ_SDC			(IRQ_EB_GIC_START + 26)	/* SD/MMC IRQ */
#define IRQ_ARM_CTIRQ0		(IRQ_EB_GIC_START + 27)	/* Coresight Crosstrigger 0 */
#define IRQ_ARM_CTIRQ1		(IRQ_EB_GIC_START + 28)	/* Coresight Crosstrigger 1 */
#define IRQ_USB_DEV		(IRQ_EB_GIC_START + 29)	/* USB Host 0 */
#define IRQ_USB_EHCI		(IRQ_EB_GIC_START + 30)	/* USB Host 1 */
#define IRQ_USB_OHCI		(IRQ_EB_GIC_START + 31)	/* USB Device */
#define IRQ_AHCI_CTRL		(IRQ_EB_GIC_START + 32)	/* AHCI SATA Controller */
#define IRQ_PCIE0		(IRQ_EB_GIC_START + 33)	/* PCIe 0 */
#define IRQ_PCIE1		(IRQ_EB_GIC_START + 34)	/* PCIe 1 */
#define IRQ_PCIE2		(IRQ_EB_GIC_START + 35)	/* PCIe 2 */
#define IRQ_LCD			(IRQ_EB_GIC_START + 36)	/* LCD */

#ifndef CONFIG_CORTINA_FPGA
#define IRQ_NI_RX_XRAM0		(IRQ_EB_GIC_START + 37)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM1		(IRQ_EB_GIC_START + 38)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM2		(IRQ_EB_GIC_START + 39)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM3		(IRQ_EB_GIC_START + 40)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM4		(IRQ_EB_GIC_START + 41)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM5		(IRQ_EB_GIC_START + 42)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM6		(IRQ_EB_GIC_START + 43)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM7		(IRQ_EB_GIC_START + 44)	/* NI RX XRAM 0 has packet */
#define IRQ_NI_RX_XRAM8		(IRQ_EB_GIC_START + 45)	/* NI RX XRAM 0 has packet */
#define IRQ_SPDIF		(IRQ_EB_GIC_START + 46)	/* SPDIF */
#endif

#ifdef CONFIG_CORTINA_FPGA
#undef IRQ_UART2
#undef IRQ_UART3
#undef IRQ_USB_OHCI
#undef IRQ_AHCI_CTRL
#undef IRQ_PCIE0
#undef IRQ_PCIE1
#undef IRQ_PCIE2
#undef IRQ_LCD

#define IRQ_LCD			(IRQ_EB_GIC_START + 2)
#define IRQ_PCIE0		(IRQ_EB_GIC_START + 8)
#define IRQ_PCIE1		(IRQ_EB_GIC_START + 9)
#define IRQ_AHCI_CTRL		(IRQ_EB_GIC_START + 31)
#endif

/* Regbus Interrupt Controller */
#define REGBUS_IRQ_BASE		80
#define REGBUS_AGGRE_NO		15	/* Number of Interrupt aggregated */
#define REGBUS_INT_MASK       	((1<<REGBUS_AGGRE_NO)-1)
#define IRQ_REGBUS_SOFT0	(REGBUS_IRQ_BASE + 0)	/* Software triggered Interrupt */
#define IRQ_REGBUS_SOFT1	(REGBUS_IRQ_BASE + 1)	/* Software triggered Interrupt */
#define IRQ_REGBUS_TIMER0	(REGBUS_IRQ_BASE + 2)	/* Interrupt bit for timer 0 */
#define IRQ_REGBUS_TIMER1	(REGBUS_IRQ_BASE + 3)	/* Interrupt bit for timer 1 */
#define IRQ_REGBUS_AXII		(REGBUS_IRQ_BASE + 4)	/* Interrupt bit for the AXI2REG bridge. */
#define IRQ_REGBUS_SPI		(REGBUS_IRQ_BASE + 5)	/* Interrupt bit for the SPI interface. */
#define IRQ_REGBUS_GPIO0	(REGBUS_IRQ_BASE + 6)	/* Interrupt bit for the GPIO0. */
#define IRQ_REGBUS_GPIO1	(REGBUS_IRQ_BASE + 7)	/* Interrupt bit for the GPIO1. */
#define IRQ_REGBUS_GPIO2	(REGBUS_IRQ_BASE + 8)	/* Interrupt bit for the GPIO2. */
#define IRQ_REGBUS_GPIO3	(REGBUS_IRQ_BASE + 9)	/* Interrupt bit for the GPIO3. */
#define IRQ_REGBUS_GPIO4	(REGBUS_IRQ_BASE + 10)	/* Interrupt bit for the GPIO4. */
#define IRQ_REGBUS_BIWI		(REGBUS_IRQ_BASE + 11)	/* Interrupt bit for the Bi-Wire interface. */
#define IRQ_REGBUS_MDIO		(REGBUS_IRQ_BASE + 12)	/* Interrupt bit for the MDIO interface. */
#define IRQ_REGBUS_SSP		(REGBUS_IRQ_BASE + 13)	/* Interrupt bit for SSP interface. */
#define IRQ_REGBUS_TRNG		(REGBUS_IRQ_BASE + 14)	/* Interrupt bit for TRNG interface. */

#define PCIE_IRQ_BASE		96
#define PCIE_IRQ_NO		(8 + 8 + 8)

/* DMA Engine IRQ */
#define IRQ_DMAENG_BASE		120
#define IRQ_DMAENG_NO		19
#define DMAENG_INT_MASK       	((1 << IRQ_DMAENG_NO) - 1)
#define IRQ_DMA_DESC		IRQ_DMAENG_BASE
#define IRQ_DMA_RX_0		(IRQ_DMA_DESC + 1)
#define IRQ_DMA_RX_1		(IRQ_DMA_RX_0 + 1)
#define IRQ_DMA_RX_2		(IRQ_DMA_RX_1 + 1)
#define IRQ_DMA_RX_3		(IRQ_DMA_RX_2 + 1)
#define IRQ_DMA_RX_4		(IRQ_DMA_RX_3 + 1)
#define IRQ_DMA_RX_5		(IRQ_DMA_RX_4 + 1)
#define IRQ_DMA_RX_6		(IRQ_DMA_RX_5 + 1)
#define IRQ_DMA_RX_7		(IRQ_DMA_RX_6 + 1)

#define IRQ_DMA_TX_0		(IRQ_DMA_RX_7 + 1)
#define IRQ_DMA_TX_1		(IRQ_DMA_TX_0 + 1)
#define IRQ_DMA_TX_2		(IRQ_DMA_TX_1 + 1)
#define IRQ_DMA_TX_3		(IRQ_DMA_TX_2 + 1)
#define IRQ_DMA_TX_4		(IRQ_DMA_TX_3 + 1)
#define IRQ_DMA_TX_5		(IRQ_DMA_TX_4 + 1)
#define IRQ_DMA_TX_6		(IRQ_DMA_TX_5 + 1)
#define IRQ_DMA_TX_7		(IRQ_DMA_TX_6 + 1)

#define IRQ_DMA_BMC0		(IRQ_DMA_TX_7 + 1)
#define IRQ_DMA_BMC1		(IRQ_DMA_BMC0 + 1)

/* DMA SSP Engine IRQ */
#define IRQ_DMASSP_BASE		140
#define IRQ_DMASSP_NO		15
#define DMASSP_INT_MASK       	((1 << IRQ_DMASSP_NO) - 1)
#define IRQ_DMASSP_DESC		IRQ_DMASSP_BASE
#define IRQ_DMASSP_RX_5		(IRQ_DMASSP_DESC + 1)
#define IRQ_DMASSP_RX_6		(IRQ_DMASSP_RX_5 + 1)
#define IRQ_DMASSP_RX_7		(IRQ_DMASSP_RX_6 + 1)
#define IRQ_DMASSP_TX_5		(IRQ_DMASSP_RX_7 + 1)
#define IRQ_DMASSP_TX_6		(IRQ_DMASSP_TX_5 + 1)
#define IRQ_DMASSP_TX_7		(IRQ_DMASSP_TX_6 + 1)
#define IRQ_DMASSP_AXI_RX_RD_DESC	(IRQ_DMASSP_TX_7 + 1)
#define IRQ_DMASSP_AXI_TX_RD_DESC	(IRQ_DMASSP_AXI_RX_RD_DESC + 1)
#define IRQ_DMASSP_AXI_TX_RD_DATA	(IRQ_DMASSP_AXI_TX_RD_DESC + 1)
#define IRQ_DMASSP_AXI_RX_WR_DESC	(IRQ_DMASSP_AXI_TX_RD_DATA + 1)
#define IRQ_DMASSP_AXI_RX_WR_DATA	(IRQ_DMASSP_AXI_RX_WR_DESC + 1)
#define IRQ_DMASSP_AXI_TX_WR_DESC	(IRQ_DMASSP_AXI_RX_WR_DATA + 1)
#define IRQ_DMASSP_SSP0			(IRQ_DMASSP_AXI_TX_WR_DESC + 1)
#define IRQ_DMASSP_SSP1			(IRQ_DMASSP_SSP0 + 1)


#define GPIO_IRQ_BASE		155
#define IRQ_GPIO(x)		((x & 0x7F) + GPIO_IRQ_BASE)

/*
 * Only define NR_IRQS if less than NR_IRQS_EB
 */
#define NR_IRQS_EB		(GPIO_IRQ_BASE + 160)

#if defined(CONFIG_MACH_CORTINA_G2) \
	&& (!defined(NR_IRQS) || (NR_IRQS < NR_IRQS_EB))
#undef NR_IRQS
#define NR_IRQS			NR_IRQS_EB
#endif

#if !defined(MAX_GIC_NR) || (MAX_GIC_NR < 1)
#undef MAX_GIC_NR
#define MAX_GIC_NR		1
#endif

/* HIERARCHICAL_INTERRUPT */
static inline unsigned int get_up_irq(unsigned int irq)
{
	if (irq >= GPIO_IRQ_BASE)
		return IRQ_REGBUS_GPIO0 + (irq - GPIO_IRQ_BASE)/32;	/* 32 = GPIO_BANK_SIZE */

	return 0;
}

static inline int up_irq_is_exist(unsigned int irq)
{
	if (irq >= GPIO_IRQ_BASE)
		return 1;
	else
		return 0;
}

#endif				/* __MACH_IRQS_EB_H */
