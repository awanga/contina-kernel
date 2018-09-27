/*
 *  linux/drivers/video/hx8238a_panel.c
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  Reference:
 *    drivers/video/omaps/displays/panel-acx565akm.c
 *    drivers/input/misc/adxl34x-spi.c
 *    drivers/gpio/max730x.c
 *    drivers/leds/leds-dac124s085.c
 *    drivers/telephony/arch/linux/vp_hal.c
 *    drivers/video/backlight/tdo24m.c
 *    drivers/video/backlight/s6e63m0.c
 *
 *  HX8238A 320x240 TFT LCD Single Chip
 */
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/amba/bus.h>
#include <linux/fb.h>
#include <linux/amba/clcd.h>
#include <linux/delay.h>
#include "hx8238a_panel.h"

static struct hx8238a *pHX8238A;

void hx8238a_panel(struct fb_videomode *videomode)
{
#if 0
	if (videomode) {
		pHX8238A->videomode = videomode;
	}
#endif

	/* mode */
	videomode->name = "hx8238a-panel";
	videomode->refresh = 60;		/* 60 Hz */
	videomode->xres = 320;		/* H_DISP */
	videomode->yres = 240;		/* V_DISP */
	videomode->pixclock = 154000;	/* 154000? */
	videomode->left_margin = 68;	/* t_HBP */
	videomode->right_margin = 20;	/* t_HFP */
	videomode->upper_margin = 18;	/* t_VBP */
	videomode->lower_margin = 4;	/* t_VFP */
	videomode->hsync_len = 2;		/* 2 */
	videomode->vsync_len = 2;		/* 2 */
	videomode->sync = 0;
	videomode->vmode = FB_VMODE_NONINTERLACED;
}
EXPORT_SYMBOL(hx8238a_panel);

#if defined(CONFIG_HX8238A_SPI_READ)
static int hx8238a_spi_read(struct spi_device *spi,
			    u16 reg, u16 *p_data)
{
	unsigned char tbuf[4], rbuf[2];
	int ret = 0;

	tbuf[0] = HX8238A_DEVID;
	tbuf[1] = (reg & 0xff00) >> 8;
	tbuf[2] = (reg & 0x00ff);
	tbuf[3] = HX8238A_DEVID | HX8238A_REG_SEL | HX8238A_REG_READ;

	ret = spi_write_then_read(spi, tbuf, sizeof(tbuf), rbuf, sizeof(rbuf));
	*p_data = (rbuf[1] << 8) | rbuf[0];

	return ret;
}
#endif

static int hx8238a_spi_write(struct spi_device *spi,
			     u16 reg, u16 p_data)
{
	unsigned char 	buf[3];
	int		ret_val;

	buf[0] = HX8238A_DEVID;
	buf[1] = (reg & 0xff00) >> 8;
	buf[2] = (reg & 0x00ff);
	ret_val = spi_write(spi, buf, sizeof(buf));

	buf[0] = HX8238A_DEVID | HX8238A_REG_SEL;
	buf[1] = ((p_data) & 0xff00) >> 8;
	buf[2] = ((p_data) & 0x00ff);
	ret_val |= spi_write(spi, buf, sizeof(buf));

	return ret_val;
}

#if defined(CONFIG_HX8238A_SPI_READ)
static inline void hx_spi_print(struct hx8238a *hx, u16 reg)
{
	u16 rbuf;

	HX_SPI_READ(hx, reg, &rbuf);
	printk("[%04x]=0x%04x\n", reg, rbuf);
}

static int hx8238a_rstval(struct hx8238a *hx)
{
	printk("HX8238A controller reset value:\n");

	hx_spi_print(hx, HX_STATUS_REG);
	hx_spi_print(hx, HX_DRVOCTL_REG);	/* xx00: SM=0 */
	hx_spi_print(hx, HX_DRVWFCTL_REG);	/* 0200: B/C=1 */
	hx_spi_print(hx, HX_PWRCTL1_REG);	/* 6364: DCT=0110 BT=011 BTF=0 DC=0110 AP=010 */
	hx_spi_print(hx, HX_IDCFCTL_REG);	/* 04xx: PALM=1 BLT=00 */
	hx_spi_print(hx, HX_FUNCTL_REG);
	hx_spi_print(hx, HX_CTBRCTL_REG);	/* 4008: BR=1000000 CON=01000 */
	hx_spi_print(hx, HX_FCYCTL_REG);	/* d400: NO=11 SDT=01 EQ=100 */
	hx_spi_print(hx, HX_PWRCTL2_REG);	/* 3229: VRC=011 VDS=10 VRH=101001 */
	hx_spi_print(hx, HX_PWRCTL3_REG);	/* 3200: VDV=1001000 */
	hx_spi_print(hx, HX_GSPOS_REG);		/* 0000: SCN=00000000 */
	hx_spi_print(hx, HX_HPOR_REG);		/* 9f80: XLIM=100111111 */
	hx_spi_print(hx, HX_VPOR_REG);		/*       STH=00 */
	hx_spi_print(hx, HX_PWRCTL4_REG);	/* 0052: nOTP=0 VCM=1010010 */
	hx_spi_print(hx, HX_GACTL1_REG+0);	/* 0000: PKP1=000 PKP0=000 */
	hx_spi_print(hx, HX_GACTL1_REG+1);	/* 0407: PKP3=100 PKP2=111 */
	hx_spi_print(hx, HX_GACTL1_REG+2);	/* 0202: PKP5=101 PKP4=010 */
	hx_spi_print(hx, HX_GACTL1_REG+3);	/* 0000: PRP1=000 PRP0=000 */
	hx_spi_print(hx, HX_GACTL1_REG+4);	/* 0505: PKN1=101 PKN0=101 */
	hx_spi_print(hx, HX_GACTL1_REG+5);	/* 0003: PKN3=000 PKN2=011 */
	hx_spi_print(hx, HX_GACTL1_REG+6);	/* 0707: PKN5=111 PKN4=111 */
	hx_spi_print(hx, HX_GACTL1_REG+7);	/* 0000: PRN1=000 PRN0=000 */
	hx_spi_print(hx, HX_GACTL2_REG+0);	/* 0904: VRP1=01001 VRP0=0100 */
	hx_spi_print(hx, HX_GACTL2_REG+1);	/* 0904: VRN1=01001 VRN0=0100 */
	hx_spi_print(hx, HX_OTP_STS_REG);

	printk("\n");
	return 0;
}
#endif

static int init_hx8238a(struct hx8238a *hx)
{
#if defined(CONFIG_HX8238A_SPI_READ)
	hx8238a_rstval(hx);
#endif

	/* Ref: http://meld.mvista.com/group_discussion.aspx?DiscussionID=79196c407bd04ce3aa8203d6b5fe103f */
	HX_SPI_WRITE(hx, HX_DRVOCTL_REG, 0x7300);	/* Driver output control */
		/* RL=1: S0 shifts to S959 and RGB is assigned from S0 */
		/* REV=1: displays all character and graphics display sections with reversal */
		/* PINV=1: POL output phase is reversed with VCOM signal */
		/* BGR=0: RGB color is assigned from S0 */
		/* SM=0: odd/even division is selected */
		/* TB=1: G0 shifts to G239 */
		/* CPE=1: internal charge pump Vcim, VGH, VGL, and Vcix2 are enabled */
	HX_SPI_WRITE(hx, HX_DRVWFCTL_REG, 0x0200);	/* LCD-Driving-Waveform Control */
		/* B/C=1: line inversion waveform is generated */
	HX_SPI_WRITE(hx, HX_PWRCTL4_REG, 0x00ca);	/* Power control 4, 0x00db? */
		/* nOPT=1: setting of VCM becomes valid and voltage of VCOMH can be adjusted */
		/* VCM=0x4a: VLCD63 x (0.360 + 0.005 * 0x4a) */
	HX_SPI_WRITE(hx, HX_PWRCTL1_REG, 0x7164);	/* Power control 1, 0x7184? */
		/* DCT=0x7: Fline x 4. Set the step-up cycle of the step-up circuit for 8-color mode (CM=VDDIO). */
		/* BTF=0 and BT=0x1: VGH=Vcix2j X 3, VGL=-(Vcix2j X 2) */
		/* DC=0x6: Fline x 5. Set the step-up circuit for 262k-color mode (CM=VSS) */
		/* AP=0x2: Small to medium. Adjust the amount of current from the stable-current source in the */
		/*	internal operational amplifier circuit. */
	HX_SPI_WRITE(hx, HX_IDCFCTL_REG, 0x0447);	/* Input Data and Color Filter Control */
		/* PALM=1: 288 lines. set input data line number in PAL mode */
		/* BLT=0x0: 10 fields. Set the initial power on black image insertion time. */
		/* OEA=0x1: Display Start@VBP delay for Odd field and @VBP for Even field */
		/* SEL=0x0: parallel-RGB@6.5MHz. define the input interface mode */
		/* SWD=0x7: control and switch the relationship between the RGB data and color filter type */
	HX_SPI_WRITE(hx, HX_FUNCTL_REG,  0xc800);	/* Function control, 0xB4D4? */
		/* GHN=1: gate driver is normal operation */
		/* XDK=1: VCIX2 is 2 phase pumping from VCI. (VCIX2=2 x VCI) */
		/* GDIS=0: VGL has no discharge path to VSS in sleep mode */
		/* LPF=0: the low pass filter function in YUV mode is disabled */
		/* DEP=1: DEN is positive polarity active */
		/* CKP=0: data is latched by CLK rising edge */
		/* VSP=0: VSYNC is negative polarity */
		/* HSP=0: HSYNC is negative polarity */
		/* DEO=0: VSYNC/HSYNC are also needed in DE mode. */
		/* DIT=0: turn off dithering function */
		/* PWM=0: disable PWM function */
		/* FB=0x0: 0.4V. Set PWM feedback level adjustment */
	HX_SPI_WRITE(hx, HX_CTBRCTL_REG, 0x4008);	/* Contrast/Brightness Control, default=0x4008 */
		/* BR=0x40: (-128 + 2 * 0x40). Brightness level adjustment */
		/* CON=0x08: (0.125 * 0x08). Display contract level adjustment */
	HX_SPI_WRITE(hx, HX_FCYCTL_REG, 0xd400);	/* Frame Cycle Control, default=0xd400 */
		/* NO=0x3: 6us. Set amount of non-overlap of the gate output */
		/* SDT=0x1: 3us. Set delay amount from the gate output signal falling edge to the source outputs */
		/* EQ=0x4: 6us. set the equalizing period */
	HX_SPI_WRITE(hx, HX_PWRCTL2_REG, 0x123a);	/* Power control 2, 0x1235? */
		/* VRC=0x1: 5.3V. set the VCIX2 charge pump voltage clamp */
		/* VDS=0x2: 2.2V. set the VDD regulator voltage if pin REGVDD is set to VDDIO */
		/* VRH=0x3a: Vref x 4.312. set amplitude magnification of VLCD63. */
	HX_SPI_WRITE(hx, HX_PWRCTL3_REG, 0x2c40);	/* Power control 3, 0x2b00? */
		/* VDV=0x31: VLCD63 x (0.600 + 0.0075 * 0x31). set the alternating amplitudes of VCOM */
		/*	at the VCOM alternating drive. */
	HX_SPI_WRITE(hx, HX_GSPOS_REG, 0x0000);	/* Gate Scan Position, default=0x0000 */
		/* SCN=0x00: first line of data (top line) */
	HX_SPI_WRITE(hx, HX_HPOR_REG, 0x9f80);	/* Horizontal Porch, default=0x9f80 */
		/* XLIM=0x13f: 320 pixels per line */
	HX_SPI_WRITE(hx, HX_VPOR_REG, 0x2212);	/* Vertical Porch */
		/* STH=0x0: +0 dot clock. adjust the first valid data by dot clock. invalid setting in parallel RGB */
		/* HBP=0x44: 68 dotclks. set the delay period from falling edge of HSYNC signal to first valid data. I's only effective in SYNC mode timing. */
		/* VBP=0x12: 18 lines. set the delay period from falling edge of VSYNC to first valid line. I's only effective in SYNC mode timing. */
	msleep(50);

	HX_SPI_WRITE(hx, HX_GACTL1_REG+0, 0x0507);	/* Gamma control 1 */
	HX_SPI_WRITE(hx, HX_GACTL1_REG+1, 0x0004);
	HX_SPI_WRITE(hx, HX_GACTL1_REG+2, 0x0707);
	HX_SPI_WRITE(hx, HX_GACTL1_REG+3, 0x0000);
	HX_SPI_WRITE(hx, HX_GACTL1_REG+4, 0x0000);
	HX_SPI_WRITE(hx, HX_GACTL1_REG+5, 0x0307);
	HX_SPI_WRITE(hx, HX_GACTL1_REG+6, 0x0700);
	HX_SPI_WRITE(hx, HX_GACTL1_REG+7, 0x0000);

	HX_SPI_WRITE(hx, HX_GACTL2_REG+0, 0x140b);	/* Gamma control 2 */
		/* VRP1=0x14 */
		/* VRP0=0xb */
	HX_SPI_WRITE(hx, HX_GACTL2_REG+1, 0x140b);
		/* VRN1=0x14 */
		/* VRN0=0xb */
	msleep(100);

	return 0;
}

static int __devinit hx8238a_probe(struct spi_device *spi)
{
	struct hx8238a *hx;
	int err=0, ret;
	u16 buf;

	spi->bits_per_word = 24;
	ret = spi_setup(spi);
	if (ret < 0) {
		printk("spi_setup() fail(%d)!\n", ret);
		return ret;
	}

	if (spi->max_speed_hz > MAX_HX8238A_SPI_FREQ_HZ) {
		dev_err(&spi->dev, "SPI CLK %d Hz too fast\n", spi->max_speed_hz);
		return -EINVAL;
	}
	hx = kzalloc(sizeof(*hx), GFP_KERNEL);
	if (!hx) {
		err = -ENOMEM;
		goto err_out;
	}

	pHX8238A = hx;
#if defined(CONFIG_HX8238A_SPI_READ)
	hx->read = hx8238a_spi_read;
#endif
	hx->write = hx8238a_spi_write;
	hx->spi = spi;

	mutex_init(&hx->lock);
	spi_set_drvdata(spi, hx);

#if defined(CONFIG_HX8238A_SPI_READ)
	ret = hx->read(spi, HX_STATUS_REG, &buf);
#endif

	init_hx8238a(hx);

	return err;

err_out:
	return err;
}

static int __devexit hx8238a_spi_remove(struct spi_device *spi)
{
	struct hx8238a *hx = spi_get_drvdata(spi);

	spi_set_drvdata(spi, NULL);
	kfree(hx);

	return 0;
}

#ifdef CONFIG_PM
static int hx8238a_spi_suspend(struct spi_device *spi, pm_message_t message)
{
	return 0;
}

static int hx8238a_spi_resume(struct spi_device *spi)
{
	return 0;
}

#else
# define	hx8238a_spi_suspend	NULL
# define	hx8238a_spi_resume	NULL
#endif

static struct spi_driver hx8238a_spi_driver = {
	.driver = {
		.name	= "hx8238a_panel",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= hx8238a_probe,
	.remove		= __devexit_p(hx8238a_spi_remove),
	.suspend	= hx8238a_spi_suspend,
	.resume		= hx8238a_spi_resume,
};

int __init hx8238a_spi_init(void)
{
	int ret;
	
	ret = spi_register_driver(&hx8238a_spi_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register hx8238a spi panel driver: %d\n",
				ret);
	}

	return ret;
}

void __exit hx8238a_spi_exit(void)
{
	spi_unregister_driver(&hx8238a_spi_driver);
}

module_init(hx8238a_spi_init);
module_exit(hx8238a_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joe Hsu <joe.hsu@cortina-systems.com>");
MODULE_DESCRIPTION("HX8238A TFT LCD panel driver for CS75XX");
