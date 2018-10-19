/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies
// Copyright (C) 2002-2010, all rights reserved
// Elliptic Technologies Inc.
//
// As part of our confidentiality  agreement, Elliptic  Technologies and
// the Company, as  a  Receiving Party, of  this  information  agrees to
// keep strictly  confidential  all Proprietary Information  so received
// from Elliptic  Technologies. Such Proprietary Information can be used
// solely for  the  purpose  of evaluating  and/or conducting a proposed
// business  relationship  or  transaction  between  the  parties.  Each
// Party  agrees  that  any  and  all  Proprietary  Information  is  and
// shall remain confidential and the property of Elliptic  Technologies.
// The  Company  may  not  use  any of  the  Proprietary  Information of
// Elliptic  Technologies for any purpose other  than  the  above-stated
// purpose  without the prior written consent of Elliptic  Technologies.
//
//-----------------------------------------------------------------------
//
// Project:
//
//   HW Platform Abstraction for Elliptic SDKs
//
// Description:
//
// This file defines the I/O interface for reading and writing registers.
// Adjust for the target platform for proper execution.
//
//-----------------------------------------------------------------------
//
// Language:         C
//
// Filename:         $Source: /auto/project/cvsroot/design/sw/platforms/g2-36/openwrt-2.4.2011-trunk/target/linux/g2/files/drivers/char/hw_random/cs75xx-rng/include/elphw_platform.h,v $
// Current Revision: $Revision: 1.2 $
// Last Updated:     $Date: 2012/04/12 06:02:53 $
// Current Tag:      $Name:  $
//
//-----------------------------------------------------------------------*/

#ifndef _ELPHW_PLATFORM_H_
#define _ELPHW_PLATFORM_H_


#ifdef ELP_PCI_FPGA_BOARD

#ifdef __KERNEL__
#include "elp_labpci.h"
/* CPU specific yield
 * This is used in while loops to insert code as desired */
#define CPU_YIELD    cpu_relax()

#else
/* user space interface */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CPU_YIELD
#define ELPHW_MEMCPY memcpy
#define ELPHW_PRINT printf
//#define ELPHW_PRINT(...)

#include <asm/byteorder.h>
#define cpu_to_le32 __cpu_to_le32
#define le32_to_cpu __le32_to_cpu

/* the user space functions to read the PCI bus */

static inline uint32_t pci_user_read_reg(uint32_t * addr)
{
   return le32_to_cpu(*addr);
};

static inline void pci_user_write_reg(uint32_t *addr, uint32_t data)
{
   *addr = cpu_to_le32(data);
};

#define ELPHW_READ_REG(addr)  pci_user_read_reg(addr)
#define ELPHW_WRITE_REG(addr,data) pci_user_write_reg(addr, data)

#endif

#else

//#warning Define your platform configuration here!

/* This library requires a definition of the following:
      stdint types, NULL, printf, memcpy and CPU_YIELD */
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/processor.h>
#include <asm/io.h>

#define CPU_YIELD 
#define ELPHW_MEMCPY memcpy
#define ELPHW_PRINT printk

/* It also requires a function/macro for reading the registers */

#define ELPHW_READ_REG(addr)  readl(addr)
#define ELPHW_WRITE_REG(addr,data) writel(data, addr)

#endif

#endif /* _ELPHW_PLATFORM_H_ */
