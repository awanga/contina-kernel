/*
 *  linux/drivers/video/hx8238a_panel.h
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  HX8238A TFT LCD Single Chip (spi protocol)
 */
#ifndef __HX8238A_PANEL_H__
#define __HX8238A_PANEL_H__

#include <linux/spi/spi.h>

/* HX8238A Registers Map */
#define	HX_STATUS_REG	0x00	/* Status Read */
#define	HX_DRVOCTL_REG	0x01	/* xx00: Driver Output Control */
#define	HX_DRVWFCTL_REG	0x02	/* 0200: LCD-Driving-Waveform Control */
#define	HX_PWRCTL1_REG	0x03	/* 6364: Power control 1 */
#define	HX_IDCFCTL_REG	0x04	/* 04xx: Input Data and Color Filter Control */
#define	HX_FUNCTL_REG	0x05	/* Function Control */
#define	HX_CTBRCTL_REG	0x0a	/* 4008: Contrast/Brightness Control */
#define	HX_FCYCTL_REG	0x0b	/* d400: Frame Cycle Control */
#define	HX_PWRCTL2_REG	0x0d	/* 3229: Power Control 2 */
#define	HX_PWRCTL3_REG	0x0e	/* 3200: Power Control 3 */
#define	HX_GSPOS_REG	0x0f	/* 0000: Gate Scan Position */
#define	HX_HPOR_REG	0x16	/* 9f80: Horizontal Porch */
#define	HX_VPOR_REG	0x17	/* Vertical Porch */
#define	HX_PWRCTL4_REG	0x1e	/* 0052: Power Control 4 */
#define	HX_GACTL1_REG	0x30	/* Gamma Control 1 (R30h to R37h) */
				/* 0000, 0407, 0202, 0000, 0505, 0003, 0707, 0000 */
#define	HX_GACTL2_REG	0x3a	/* Gamma Control 2 (R3ah to R3bh) */
				/* 0904, 0904 */
#define	HX_OTP_REG	0x60	/* OTP register, write only */
#define	HX_OTP_STS_REG	0x61	/* OTP status register (VCM=0x52 by default, IND=0: OPT is programmable) */

typedef struct hx8238a
{
	struct fb_videomode *videomode;
	struct spi_device *spi;
	struct mutex lock;     /* reentrant protection for struct */
	int (*read)(struct spi_device *, u16, u16 *);
	int (*write)(struct spi_device *, u16, u16);
} HX8238A;

#define	MAX_HX8238A_SPI_FREQ_HZ		200000
#define	HX8238A_DEVID		0x70
#define	HX8238A_REG_SEL		(1 << 1)
#define	HX8238A_REG_READ	(1 << 0)
#define	HX8238A_WRITEREG(reg)	(reg & 0x3F)
#define	HX8238A_READREG(reg)	(HX8238A_REG_READ | (reg & 0x3F))

#define HX_SPI_READ(hx, reg, rbuf)  ((hx)->read((hx)->spi, reg, rbuf))
#define HX_SPI_WRITE(hx, reg, val)  ((hx)->write((hx)->spi, reg, val))

#endif		/* __HX8238A_PANEL_H__ */
