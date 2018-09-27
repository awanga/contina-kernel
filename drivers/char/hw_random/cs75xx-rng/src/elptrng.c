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
//   This is the base level interaction with the hardware registers.
//
//-----------------------------------------------------------------------
//
// Language:         C
//
// Filename:         $Source: /auto/project/cvsroot/design/sw/platforms/g2-36/openwrt-2.4.2011-trunk/target/linux/g2/files/drivers/char/hw_random/cs75xx-rng/src/elptrng.c,v $
// Current Revision: $Revision: 1.1 $
// Last Updated:     $Date: 2011/04/09 11:47:21 $
// Current Tag:      $Name:  $
//
//-----------------------------------------------------------------------*/

#include <elptrng.h>

void trng_dump_registers (trng_hw * hw)
{
   if (ELPTRNG_INIT == hw->initialized) {
      ELPHW_PRINT ("TRNG CTRL     @%p = %08x\n", hw->trng_ctrl, ELPHW_READ_REG (hw->trng_ctrl));
      ELPHW_PRINT ("TRNG IRQ_STAT @%p = %08x\n", hw->trng_irq_stat, ELPHW_READ_REG (hw->trng_irq_stat));
      ELPHW_PRINT ("TRNG IRQ_EN   @%p = %08x\n", hw->trng_irq_en, ELPHW_READ_REG (hw->trng_irq_en));
      ELPHW_PRINT ("TRNG DATA     @%p : ", hw->trng_data);
      ELPHW_PRINT ("%08x-%08x-%08x-%08x\n", ELPHW_READ_REG (hw->trng_data), ELPHW_READ_REG (hw->trng_data +1),
                                           ELPHW_READ_REG (hw->trng_data +2), ELPHW_READ_REG (hw->trng_data +3));
   }
}

/* wait for operation complete */
/* input: number of cylces to wait */
/* output: engine status */
int32_t trng_wait_for_done (trng_hw * hw, uint32_t cycles)
{
   if (ELPTRNG_NOT_INIT == hw->initialized) {
      return ELPTRNG_NOT_INITIALIZED;
   } else {

      while ((cycles > 0) && (0 == (TRNG_IRQ_DONE & ELPHW_READ_REG (hw->trng_irq_stat)))) {
         CPU_YIELD;
         --cycles;
      }
      if (!cycles) {
         trng_dump_registers (hw);
         return ELPTRNG_TIMEOUT;
      }
      /* clear the status */
      ELPHW_WRITE_REG (hw->trng_irq_stat, TRNG_IRQ_DONE);
      return ELPTRNG_OK;
   }
}

/* start the rand reseed */
/* input: wait at end for irq status */
/* output: engine status */
int32_t trng_reseed_random (trng_hw * hw, uint32_t wait)
{
   int32_t ret;

   ret = ELPTRNG_OK;

   if (ELPTRNG_NOT_INIT == hw->initialized) {
      ret = ELPTRNG_NOT_INITIALIZED;
   } else {
      ELPHW_WRITE_REG (hw->trng_ctrl, TRNG_RAND_RESEED);
   }

   if (ELPTRNG_WAIT == wait) {
      ret = trng_wait_for_done (hw, LOOP_WAIT);
   }

   return ret;
}

/* set nonce seed */
/* input: *seed, the size has to be 32 bytes which is 8 32-bit words)
          The hw uses 128+127=255 bit) */
/* output: engine status */
int32_t trng_reseed_nonce (trng_hw * hw, uint32_t seed[TRNG_NONCE_SIZE_WORDS])
{
   int32_t ret;
   uint32_t n;

   ret = ELPTRNG_OK;

   if (ELPTRNG_NOT_INIT == hw->initialized) {
      ret = ELPTRNG_NOT_INITIALIZED;
   } else if (seed == NULL) {
      ret = ELPTRNG_INVALID_ARGUMENT;
   } else {
      /* write the lower words */
      ELPHW_WRITE_REG (hw->trng_ctrl, TRNG_NONCE_RESEED);
      for (n = 0; n < TRNG_DATA_SIZE_WORDS; n++) {
         ELPHW_WRITE_REG ((hw->trng_data + n), seed[n]);
      }
      ELPHW_WRITE_REG (hw->trng_ctrl, (TRNG_NONCE_RESEED | TRNG_NONCE_RESEED_LD));

      /* write the upper words */
      for (n = 0; n < TRNG_DATA_SIZE_WORDS; n++) {
         ELPHW_WRITE_REG ((hw->trng_data + n), seed[n + TRNG_DATA_SIZE_WORDS]);
      }
      ELPHW_WRITE_REG (hw->trng_ctrl, (TRNG_NONCE_RESEED | TRNG_NONCE_RESEED_LD | TRNG_NONCE_RESEED_SELECT));

      /* finish the operation */
      ELPHW_WRITE_REG (hw->trng_ctrl, 0);

      ret = trng_wait_for_done (hw, LOOP_WAIT);
   }
   return ret;
}

/* Initialize the trng. Optionally enable the IRQ and reseed if desired */
int32_t trng_init (trng_hw * hw, uint32_t reg_base, uint32_t enable_irq, uint32_t reseed)
{
   int32_t ret;

   ret = ELPTRNG_OK;

   /* set the register addresses */
   hw->trng_ctrl     = (uint32_t *) (reg_base + TRNG_CTRL);
   hw->trng_irq_stat = (uint32_t *) (reg_base + TRNG_IRQ_STAT);
   hw->trng_irq_en   = (uint32_t *) (reg_base + TRNG_IRQ_EN);
   hw->trng_data     = (uint32_t *) (reg_base + TRNG_DATA);
   hw->initialized = ELPTRNG_INIT;

   if (ELPTRNG_IRQ_PIN_ENABLE == enable_irq) {
      /* enable the interrupt pin and clear the status */
      ELPHW_WRITE_REG (hw->trng_irq_en, TRNG_IRQ_ENABLE);
      ELPHW_WRITE_REG (hw->trng_irq_stat, TRNG_IRQ_DONE);
   }

   if (ELPTRNG_RESEED == reseed) {
      /* reseed with the rings */
      ret = trng_reseed_random (hw, ELPTRNG_WAIT);
      if (ret != ELPTRNG_OK) {
         hw->initialized = ELPTRNG_NOT_INIT;
      }
   }
   return ret;
}

/* Close the trng */
void trng_close (trng_hw * hw)
{
   /* clear the initialization flag and any settings */
   hw->initialized = ELPTRNG_NOT_INIT;
   ELPHW_WRITE_REG (hw->trng_ctrl, 0);
   ELPHW_WRITE_REG (hw->trng_irq_en, 0);
}

/* get a stream of random bytes */
/* input: *randbuf and it size, the size should be > 0,
         s/w supports byte access even if h/w word (4 bytes) access */
/* output: data into randbuf and returns engine status */
int32_t trng_rand (trng_hw * hw, uint8_t * randbuf, uint32_t size)
{
   int32_t ret;
   uint32_t buf[TRNG_DATA_SIZE_WORDS];
   uint32_t i;
   uint32_t n;

   ret = ELPTRNG_OK;

   if (ELPTRNG_NOT_INIT == hw->initialized) {
      ret = ELPTRNG_NOT_INITIALIZED;
   } else if ((!randbuf) || (size == 0)) {
      ret = ELPTRNG_INVALID_ARGUMENT;
   } else {
      for (; size;) {
         /* This tests for a reseeding operation or a new value generation */
         if (ELPHW_READ_REG (hw->trng_ctrl) > 0) {
            if ((ret = trng_wait_for_done (hw, LOOP_WAIT)) != ELPTRNG_OK) {
               break;
            }
         }

         /* read out in maximum TRNG_DATA_SIZE_BYTES byte chunks */
         i = size > TRNG_DATA_SIZE_BYTES ? TRNG_DATA_SIZE_BYTES : size;

         for (n = 0; n < TRNG_DATA_SIZE_WORDS; n++) {
            buf[n] = ELPHW_READ_REG (hw->trng_data + n);
         }

         ELPHW_MEMCPY (randbuf, (uint8_t *) buf, i);
         randbuf += i;
         size -= i;

         /* request next data */
         ELPHW_WRITE_REG (hw->trng_ctrl, TRNG_GET_NEW);
         if ((ret = trng_wait_for_done (hw, LOOP_WAIT)) != ELPTRNG_OK) {
            break;
         }
      }
   }

   return ret;
}

