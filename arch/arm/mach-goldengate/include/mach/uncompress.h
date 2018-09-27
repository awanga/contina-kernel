/*
 *  arch/arm/mach-goldengate/include/mach/uncompress.h
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Jason Li <jason.li@cortina-systems.com>
 *
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
#include <mach/hardware.h>
#include <asm/mach-types.h>

#include <mach/platform.h>

static volatile unsigned long *UART = (unsigned long *)GOLDENGATE_UART0_BASE;

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	volatile unsigned int status;

	// Wait for Tx FIFO not full
	do
	{
		status = UART[UINFO>>2];
	}
	while (status & UINFO_TX_FIFO_FULL);
	barrier();

	//UART TX data register
	UART[UTX_DATA>>2] = c;
}

//#define flush() do { } while (0)
/* For openwrt */
static inline void flush(void)
{
}

/*
 * nothing to do
 */
//#define arch_decomp_setup()
static inline void arch_decomp_setup(void)
{
	unsigned int sample;
#ifndef CONFIG_CORTINA_FPGA
	unsigned int baudrate;
	unsigned int reg_v;
	volatile unsigned int *tmp;

#endif
	/* Set the baud rate to be our internal peripheral clock rate / our
	* desired baudrate
	*
	* Also, enable the RX/TX state machines and set the data width to
	* eight bits (also implicitly set no-parity and 1 stop by
	*/
#ifdef CONFIG_CORTINA_FPGA
	UART[UCFG >> 2] = GOLDENGATE_BAUD_DEFAULT << UCFG_BAUD_COUNT | UCFG_EN | UCFG_TX_EN | UCFG_CHAR_8;
	sample = (GOLDENGATE_BAUD_DEFAULT / 2);
#else
	tmp = (unsigned int *)GLOBAL_STRAP;
	reg_v = (*tmp >> 1) & 0x07;
	switch (reg_v) {
	case 5:
		baudrate = 150 * 1000000 / 115200;
		break;
	case 6:
		baudrate = 170 * 1000000 / 115200;
		break;
	default:
		baudrate = 100 * 1000000 / 115200;
		break;
	}
	UART[UCFG >> 2] = baudrate << UCFG_BAUD_COUNT | UCFG_EN | UCFG_TX_EN | UCFG_CHAR_8;
	sample = (baudrate / 2);
#endif
	sample = (sample < 7) ? 7 : sample;
	UART[URX_SAMPLE >> 2] = sample;
}
#define arch_decomp_wdog()
