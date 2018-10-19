/*
 *  include/asm-arm/arch-lynxe/uncompress.h
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
#ifndef __ASM_ARCH_LYNXE_UNCOMPRESS_H__
#define __ASM_ARCH_LYNXE_UNCOMPRESS_H__

#include <mach/hardware.h>
#include <mach/lynxe.h>

/*
 * The following code assumes the serial port has already been
 * initialized by the bootloader.  
*/

static volatile unsigned long *UART = (unsigned long *)UART0_BASE_ADDR;

#define UART_INFO                   (0x18>>2)
#define UART_INFO_TX_FIFO_EMPTY     (1<<3)
#define UART_TXDAT                  (0x10>>2)
#define UART_CFG                    (0x0>>2)
#define UART_CFG_UART_EN            0x80 

static void putc(int ch)
{
    if(UART[UART_CFG] & UART_CFG_UART_EN) 
    {
    	while (!(UART[UART_INFO] & UART_INFO_TX_FIFO_EMPTY))
            
    	barrier();

    	UART[UART_TXDAT] = ch;
    }
}

static inline void flush(void)
{
}


/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()

#endif				/* __ASM_ARCH_LYNXE_UNCOMPRESS_H__ */

