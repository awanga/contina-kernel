/*
  * sound/soc/cs75xx/cs75xx_snd_plat.h
 *
 * Copyright 2007 Analog Device Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _CS75XX_SOUND_PLAT_H
#define _CS75XX_SOUND_PLAT_H

#define MAX_TOTAL_DMA_SIZE	(16 * PAGE_SIZE)

/*
 * For Kernel3.3.8
 * Author:Rudolph
 */
extern struct snd_soc_platform_driver cs75xx_snd_platform;

//extern struct snd_soc_platform cs75xx_snd_platform;

#endif
