/*
 *  arch/arm/mach-goldengate/include/mach/hardware.h
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
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>
//#include <mach/platform.h>
#include "io.h"

#include "../../pcie.h"

//For PCIe driver compiler used
//===============================================

#define pcibios_assign_all_busses()     1
#define PCIBIOS_MIN_IO                  0x00001000
#define PCIBIOS_MIN_MEM                 0x01000000
#define PCIMEM_BASE                     Upper_Base_Address_CFG_Region1 /* mem base for VGA */

//====================================================

/* macro to get at IO space when running virtually */
#ifdef CONFIG_MMU
/* IO address scatter over several area, it's not good for SW mapping
 * Statically mapped addresses:
 *
 * F0xx xxxx -> f0xx xxxx
 *		 *					*
 * FDxx xxxx -> fdxx xxxx
 *
 *
 * 1xxx xxxx -> fe1x xxxx		// Can't over 64KB map
 * 2xxx xxxx -> fe2x xxxx		// Can't over 64KB map
 * 3xxx xxxx -> fe3x xxxx		// Can't over 64KB map
 *		 *					*
 *		 *					*
 */
//#define IO_ADDRESS(x)		(((x) & 0x0FFFFFFF) + 0xF0000000)
#define IO_ADDRESS(x)		( (((x) & 0xF0000000) == 0xF0000000) ? \
								(x) : (((x)&0xFF000000)>>8 |0xFE000000| ((x)&0x0000FFFF)) )

#else
#define IO_ADDRESS(x)		(x)
#endif
#define __io_address(n)		__io(IO_ADDRESS(n))

#endif
