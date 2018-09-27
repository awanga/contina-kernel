/* arch/arm/mach-goldengate/include/mach/rtc-g2.h
 *
 * Copyright (c) 2010 Cortina-Systems <linux@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * GoldenGate Internal RTC register definition
*/

#ifndef __ASM_ARCH_REGS_RTC_H
#define __ASM_ARCH_REGS_RTC_H __FILE__

#define G2_RTC_RTCREG(x) (x)

#define G2_RTC_RTCCON	      G2_RTC_RTCREG(0x00)
#define G2_RTC_RTCCON_STARTB (1<<0)
#define G2_RTC_RTCCON_RTCEN  (1<<1)
#define G2_RTC_RTCCON_CLKRST (1<<2)
#define G2_RTC_RTCCON_OSCEN  (1<<3)

//#define G2_RTC_TICNT	      G2_RTC_RTCREG(0x44)
//#define G2_RTC_TICNT_ENABLE  (1<<7)

#define G2_RTC_RTCALM	      G2_RTC_RTCREG(0x04)
#define G2_RTC_RTCALM_ALMEN  (1<<7)
#define G2_RTC_RTCALM_YEAREN (1<<6)
#define G2_RTC_RTCALM_MONEN  (1<<5)
#define G2_RTC_RTCALM_DAYEN  (1<<4)
#define G2_RTC_RTCALM_DATEEN (1<<3)
#define G2_RTC_RTCALM_HOUREN (1<<2)
#define G2_RTC_RTCALM_MINEN  (1<<1)
#define G2_RTC_RTCALM_SECEN  (1<<0)

#define G2_RTC_RTCALM_ALL \
  G2_RTC_RTCALM_ALMEN | G2_RTC_RTCALM_YEAREN | G2_RTC_RTCALM_MONEN |\
  G2_RTC_RTCALM_DAYEN | G2_RTC_RTCALM_HOUREN | G2_RTC_RTCALM_MINEN |\
  G2_RTC_RTCALM_SECEN


#define G2_RTC_ALMSEC	      G2_RTC_RTCREG(0x08)
#define G2_RTC_ALMMIN	      G2_RTC_RTCREG(0x0c)
#define G2_RTC_ALMHOUR	      G2_RTC_RTCREG(0x10)

#define G2_RTC_ALMDATE	      G2_RTC_RTCREG(0x14)
#define G2_RTC_ALMDAY	      G2_RTC_RTCREG(0x18)
#define G2_RTC_ALMMON	      G2_RTC_RTCREG(0x1c)
#define G2_RTC_ALMYEAR	      G2_RTC_RTCREG(0x20)

//#define G2_RTC_RTCRST	      G2_RTC_RTCREG(0x6c)

#define G2_RTC_RTCSEC	      G2_RTC_RTCREG(0x24)
#define G2_RTC_RTCMIN	      G2_RTC_RTCREG(0x28)
#define G2_RTC_RTCHOUR	      G2_RTC_RTCREG(0x2c)
#define G2_RTC_RTCDATE	      G2_RTC_RTCREG(0x30)
#define G2_RTC_RTCDAY	      G2_RTC_RTCREG(0x34)
#define G2_RTC_RTCMON	      G2_RTC_RTCREG(0x38)
#define G2_RTC_RTCYEAR	      G2_RTC_RTCREG(0x3c)
#define G2_RTC_RTCIM	      G2_RTC_RTCREG(0x40)
#define G2_RTC_PIE_ENABLE     (1<<2)

#define G2_RTC_RTCPEND	      G2_RTC_RTCREG(0x44)
#define G2_RTC_PRIPEND	      G2_RTC_RTCREG(0x48)
#define G2_RTC_WKUPPEND	      G2_RTC_RTCREG(0x4c)


#endif /* __ASM_ARCH_REGS_RTC_H */
