/* arch/arm/mach-realview/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/hardware.h>


#define PER_UART0_CFG                            (0xf0070110)
#define PER_UART0_TXDAT                          (0xf0070120)
#define PER_UART0_INFO                           (0xf0070128)
#define UART_PADDR	PER_UART0_CFG
#define UART_VADDR	PER_UART0_CFG  /**(IO_ADDRESS(PER_UART0_CFG))**/
#define	UTXDAT		(PER_UART0_TXDAT - PER_UART0_CFG)
#define	UINFO		(PER_UART0_INFO - PER_UART0_CFG)
#define		INFO_TX_EMPTY   (1<<3)
#define	 	INFO_TX_FULL	(1<<2)

	.macro	addruart, rp, rv, tmp
	ldr	\rp, =UART_PADDR				@ System peripherals (phys address)
	ldr	\rv, =UART_VADDR		@ System peripherals (virt address)
	.endm

	.macro	senduart,rd,rx
	strb	\rd, [\rx, #UTXDAT]		@ Write to Transmitter Holding Register
	.endm

	.macro	waituart,rd,rx
1001:	ldr	\rd, [\rx, #UINFO]		@ Read Status Register
	tst	\rd, #INFO_TX_EMPTY			@ DBGU_TXRDY = 1 when ready to transmit
	beq	1001b
	.endm

	.macro	busyuart,rd,rx
1001:	ldr	\rd, [\rx, #UINFO]		@ Read Status Register
	tst	\rd, #INFO_TX_EMPTY			@ DBGU_TXEMPTY = 1 when transmission complete
	beq	1001b
	.endm


