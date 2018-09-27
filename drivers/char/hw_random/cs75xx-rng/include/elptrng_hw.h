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
//   TRNG
//
// Description:
//
// This file defines a system abstraction layer for some generic
// functions and data structures for accessing the registers of the TRNG
//
//-----------------------------------------------------------------------
//
// Language:         C
//
// Filename:         $Source: /auto/project/cvsroot/design/sw/platforms/g2-36/openwrt-2.4.2011-trunk/target/linux/g2/files/drivers/char/hw_random/cs75xx-rng/include/elptrng_hw.h,v $
// Current Revision: $Revision: 1.1 $
// Last Updated:     $Date: 2011/04/09 11:46:23 $
// Current Tag:      $Name:  $
//
//-----------------------------------------------------------------------*/

#ifndef _ELPTRNG_HW_H_
#define _ELPTRNG_HW_H_

#include "elphw_platform.h"

/* Register offset addresses */
#define TRNG_CTRL       0x0000

#ifdef TRNG_SASPA
#define TRNG_IRQ_STAT   0x0800
#define TRNG_IRQ_EN     0x0804
#else
#define TRNG_IRQ_STAT   0x0004
#define TRNG_IRQ_EN     0x0008
#endif

#define TRNG_DATA       0x0010  /* start offset of 4 32-bit words */

/* TRNG_CTRL register definitions */
#define TRNG_CTRL_RAND_RESEED          31
#define TRNG_CTRL_NONCE_RESEED         30
#define TRNG_CTRL_NONCE_RESEED_LD      29
#define TRNG_CTRL_NONCE_RESEED_SELECT  28
#define TRNG_CTRL_GEN_NEW_RANDOM        0

#define TRNG_RAND_RESEED            (1 << TRNG_CTRL_RAND_RESEED)
#define TRNG_NONCE_RESEED           (1 << TRNG_CTRL_NONCE_RESEED)
#define TRNG_NONCE_RESEED_LD        (1 << TRNG_CTRL_NONCE_RESEED_LD)
#define TRNG_NONCE_RESEED_SELECT    (1 << TRNG_CTRL_NONCE_RESEED_SELECT)
#define TRNG_GET_NEW                (1 << TRNG_CTRL_GEN_NEW_RANDOM)
#define TRNG_BUSY                   (1 << TRNG_CTRL_GEN_NEW_RANDOM)

#define TRNG_IRQ_STAT_BIT              27
#define TRNG_IRQ_DONE               (1 << TRNG_IRQ_STAT_BIT)

#define TRNG_IRQ_EN_BIT                27
#define TRNG_IRQ_ENABLE             (1 << TRNG_IRQ_EN_BIT)

#endif
