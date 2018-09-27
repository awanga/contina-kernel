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
// Filename:         $Source: /auto/project/cvsroot/design/sw/platforms/g2-36/openwrt-2.4.2011-trunk/target/linux/g2/files/drivers/char/hw_random/cs75xx-rng/include/elptrng.h,v $
// Current Revision: $Revision: 1.1 $
// Last Updated:     $Date: 2011/04/09 11:46:23 $
// Current Tag:      $Name:  $
//
//-----------------------------------------------------------------------*/

#ifndef _ELPTRNG_H_
#define _ELPTRNG_H_

#include "elptrng_hw.h"


/* Error codes */
enum
{
   ELPTRNG_OK = 0,
   ELPTRNG_NOT_INITIALIZED,
   ELPTRNG_TIMEOUT,
   ELPTRNG_INVALID_ARGUMENT,
};

/* wait flag */
enum
{
   ELPTRNG_NO_WAIT = 0,
   ELPTRNG_WAIT,
};

/* reseed flag */
enum
{
   ELPTRNG_NO_RESEED = 0,
   ELPTRNG_RESEED,
};

/* IRQ flag */
enum
{
   ELPTRNG_IRQ_PIN_DISABLE = 0,
   ELPTRNG_IRQ_PIN_ENABLE,
};

/* init status */
enum
{
   ELPTRNG_NOT_INIT = 0,
   ELPTRNG_INIT,
};

typedef struct trng_hw {
   uint32_t * trng_ctrl;
   uint32_t * trng_irq_stat;
   uint32_t * trng_irq_en;
   uint32_t * trng_data;
   uint32_t initialized;
} trng_hw;

/* useful constants */
#define TRNG_DATA_SIZE_WORDS 4
#define TRNG_DATA_SIZE_BYTES 16
#define TRNG_NONCE_SIZE_WORDS 8
#define TRNG_NONCE_SIZE_BYTES 32

#define LOOP_WAIT      1000000   /* Configurable delay */

/* Function definitions for external use */
void trng_dump_registers (trng_hw * hw);
int32_t trng_wait_for_done (trng_hw * hw, uint32_t cycles);
int32_t trng_reseed_random (trng_hw * hw, uint32_t wait);
int32_t trng_reseed_nonce (trng_hw * hw, uint32_t seed[TRNG_NONCE_SIZE_WORDS]);
int32_t trng_init (trng_hw * hw, uint32_t base_addr, uint32_t enable_irq, uint32_t reseed);
void trng_close (trng_hw * hw);
int32_t trng_rand (trng_hw * hw, uint8_t * randbuf, uint32_t size);

#endif
