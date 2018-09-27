/*
 *  linux/drivers/video/anx9805_hdmi.h
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  ANX9805 HDMI Transmitter
 */
#ifndef __ANX9805_HDMI_H__
#define __ANX9805_HDMI_H__

#define ANX9805_HDMI_DRIVER_NAME	"anx9805_hdmi"

#define CS75XX_HDMI_IOCTL_MAGIC   0xFF

#define ANX9805_SYS_GET_REG	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,64, unsigned int)
#define ANX9805_SYS_SET_REG	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,65, unsigned int)
#define ANX9805_SYS_SET_BITS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,66, unsigned int)
#define ANX9805_SYS_CLR_BITS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,67, unsigned int)
#define ANX9805_DP_GET_REG	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,68, unsigned int)
#define ANX9805_DP_SET_REG	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,69, unsigned int)
#define ANX9805_DP_SET_BITS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,70, unsigned int)
#define ANX9805_DP_CLR_BITS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,71, unsigned int)
#define ANX9805_HDMI_GET_REG	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,72, unsigned int)
#define ANX9805_HDMI_SET_REG	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,73, unsigned int)
#define ANX9805_HDMI_SET_BITS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,74, unsigned int)
#define ANX9805_HDMI_CLR_BITS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,75, unsigned int)
#define ANX9805_EDID_READ	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,76, unsigned int)
#define ANX9805_EEDID_READ	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,78, unsigned int)
#define ANX9805_REG_DELAY_MS	_IOWR(CS75XX_HDMI_IOCTL_MAGIC,80, unsigned int)

#ifndef __IODATA__
#define __IODATA__
union iodata {
	unsigned long dat;
	struct {
		unsigned int off;
		unsigned int val;
	} regs;
};
#endif

#define ANX9805_MCLK	0x00
#define ANX9805_MODE	0x01
#define ANX9805_TRANS	0x02
#define ANX9805_CHA_VOL	0x03
#define ANX9805_CHB_VOL	0x04

#define	ANX9805_GPIO_INT	91	/* INT - GPIO group 2, bit 27 */

#ifndef HDMI_MINOR
#define HDMI_MINOR	255
#endif

/******************************************************************************
 *	ANX9805 HDMI Debug
 ******************************************************************************/
#if 1
#if 0//def CONFIG_HDMI_ANX9805_DEBUG
#define CS752X_HDMIDBG_LEVEL 10
#include <linux/spinlock.h>

#define MAX_DBG_INDENT_LEVEL	5
#define DBG_INDENT_SIZE		2
#define MAX_DBG_MESSAGES	0

#define dbg_print(level, format, arg...)                           \
       if (level <= CS752X_HDMIDBG_LEVEL) {                        \
           if (!MAX_DBG_MESSAGES || dbg_cnt < MAX_DBG_MESSAGES) {  \
               int ind = dbg_indent;                               \
               unsigned long flags;                                \
               spin_lock_irqsave(&dbg_spinlock, flags);            \
               dbg_cnt++;                                          \
               if (ind > MAX_DBG_INDENT_LEVEL)                     \
                  ind = MAX_DBG_INDENT_LEVEL;                      \
	       printk("%*s", ind * DBG_INDENT_SIZE, "");           \
               printk(format, ## arg);                             \
               spin_unlock_irqrestore(&dbg_spinlock, flags);       \
           }                                                       \
       }

#define DBGPRINT	dbg_print
#else /* HDMI_ANX9805_DEBUG */
#define DBGPRINT(level, format, ...)
#endif
#endif

struct anx9805_info {
	struct miscdevice *miscdev;	/* miscdevice */

	const struct i2c_device_id *id;
	unsigned int delay;

	struct i2c_client *cli_sys;	/* system: device address=0x72 */
	struct i2c_client *cli_hdmi;	/* hdmi:   device address=0x7a */
	struct i2c_client *cli_dp;	/* display port:   device address=0x70 */

	struct work_struct work;	/* delay work */
	u32 int_status[8];	/* Interrupt status: F0-F7 */
	u32 init_done;
};

static const unsigned char edid_v1_header[] = { 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x00
};

/******************************************************************************
 *	ANX9805 HDMI Device Addresses
 ******************************************************************************/
#define	ANX9805_SYS_D0_ADDR	0x39	/* 0x72: HDMI/DisplayPort Dev Addr 0 (default) */
#define	ANX9805_SYS_D1_ADDR	0x3b	/* 0x76: HDMI/DisplayPort Dev Addr 1 */
#define	ANX9805_DP_D0_ADDR	0x38	/* 0x70: DisplayPort Dev Addr 0 */
#define	ANX9805_DP_D1_ADDR	0x3c	/* 0x78: DisplayPort Dev Addr 1 */
#define	ANX9805_HDMI_D0_ADDR	0x3d	/* 0x7a: HDMI System Device Address 0 */
#define	ANX9805_HDMI_D1_ADDR	0x3f	/* 0x7e: HDMI System Device Address 1 */

/******************************************************************************
 *	ANX9805 System Registers
 ******************************************************************************/
#define	VENDOR_ID_L_REG	0x00	/* 0xAA, Vendor ID low byte */
#define	VENDOR_ID_H_REG	0x01	/* 0xAA, Vendor ID high byte */
#define	DEVICE_ID_L_REG	0x02	/* 0x05, Device ID low byte */
#define	DEVICE_ID_L	0x05	/* 0x05, Device ID low */
#define	DEVICE_ID_H_REG	0x03	/* 0x98, Device ID high byte */
#define	DEVICE_ID_H	0x98	/* 0x98, Device ID high */
#define	DEVICE_VER_REG	0x04	/* 0x01, Device Version */
#define	SYS_PD_REG	0x05	/* 0x82, Power Down Register */
#define	PD_REG		0x80
#define	PD_MISC		0x40
#define	PD_IO		0x20
#define	PD_AUDIO	0x10
#define	PD_VIDEO	0x08
#define	PD_LINK		0x04
#define	PD_TOTAL	0x02
#define	MODE_SEL	0x01
#define	HDMI_MODE	0x01	/* 0=DP mode; 1=HDMI mode */
#define	SYS_RST1_REG	0x06	/* 0x04, Reset Control 1 Register */
#define	MISC_RST	0x80
#define	VID_CAP_RST	0x40
#define	VID_FIFO_RST	0x20
#define	AUD_FIFO_RST	0x10	/* 1=reset audio capture block */
#define	AUD_RST		0x08	/* 1=reset audio FIFO */
#define	HDCP_RST	0x04	/* 1=reset hdcp logic */
#define	SW_RST		0x02	/* 1=reset all logic except registers */
#define	HW_RST		0x01	/* 1=hardware reset, wait at least 200ms */
#define	SYS_RST2_REG	0x07	/* 0x00, Reset Control 2 Register */
#define	SSC_RST		0x80
#define	AC_MODE		0x40
#define	DDC_RST		0x10
#define	TMDS_BIST_RST	0x08
#define	AUX_RST		0x04
#define	SERDES_FIFO_RST	0x02
#define	I2C_REG_RST	0x01
#define	VIDEO_CTRL1_REG	0x08	/* 0x07, Video Control 1 Register */
#define	VIDEO_EN	0x80
#define	VIDEO_MUTE	0x40
#define	DE_GEN		0x20
#define	DEMUX		0x10
#define	BSEL_18B	0x04	/* 1=18-bot ddr. one pixel data per clock edge */
#define	DDR_CTRL	0x02
#define	NEG_EDGE	0x01	/* 1=negative edge latch */
#define	VIDEO_CTRL2_REG	0x09	/* 0x10, Video Control 2 Register */
#define	IN_BPC_8bit	0x10	/* Bit per color/component */
#define	IN_BPC_10bit	0x20	/* Bit per color/component */
#define	IN_BPC_12bit	0x30	/* Bit per color/component */
#define	IN_BPC_CLEAR	0x70	/* clear all bits setting */
#define	CLR_COLOR2RGB	0x03	/* Clear colorimetric format to RGB mode */
#define	VIDEO_CTRL3_REG	0x0a	/* 0x00, Video Control 3 Register */
#define	C_SWAP_RGB	0x00
#define	VIDEO_IN_YC_COEFFI	0x80	/* 1=ITU709, 0=ITU601 */
#define	VIDEO_DE_DELAY		0x20	/* 1=Enable delay one pixel of DE */
#define	VIDEO_VID_CHK_UPDATE_EN	0x10	/* 1=Enable update enable control of video format parameter check */
#define	VIDEO_C_SWAP		0x0e	/* 000=rgb */
#define	VIDEO_B_SWAP		0x01	/* 1=Swap */
#define	VIDEO_CTRL4_REG	0x0b	/* 0x00, Video Control 4 Register */
#define	VIDEO_E_SYNC		0x80	/* 1=Enable embedded sync */
#define	VIDEO_EX_E_SYNC		0x40	/* 1=check 10 bit; 0= 8 bit */
#define	VIDEO_E_SYNC_RPT	0x30	/* 00=don't repeat, 01=2x repeat, 11=4x repeat */
#define	VIDEO_BIST_EN		0x08	/* 1=Enable video BIST */
#define	VIDEO_BIST_WIDTH	0x04	/* 1=Enable bar is 64 pixels width */
#define	VIDEO_BIST_TYPE		0x03	/* 00=color bar; 01=... */
#define	VIDEO_CTRL5_REG	0x0c	/* 0x00, Video Control 5 Register */
#define	VIDEO_CTRL6_REG	0x0d	/* 0x00, Video Control 6 Register */
#define	VIDEO_CLR_PIXEL_RET	0x30	/* Clear input pixel repetition mode indicator */
#define	VIDEO_SET_PIXEL_1X_REP	0x00	/* 00=1x repetition */
#define	VIDEO_SET_PIXEL_2X_REP	0x10	/* 01=2x repetition */
#define	VIDEO_SET_PIXEL_4X_REP	0x30	/* 11=4x repetition */
#define	VIDEO_CTRL7_REG	0x0e	/* 0x00, Video Control 7 Register */
#define	VIDEO_CTRL8_REG	0x0f	/* 0x20, Video Control 8 Register */
#define	VIDEO_CTRL9_REG	0x10	/* 0x00, Video Control 9 Register */
#define	BRU_WIDTH8		0x10	/* Bus width per pixel component. 0x2=8bits */
#define	VIDEO_CTRL10_REG	0x11	/* 0x00, Video Control 10 Register */
#define	VIDEO_F_SEL		0x10	/* 1=select video format from register */
#define	VIDEO_INV_F		0x08	/* 1=invert the field polarity */
#define	VSYNC_P_CFG		0x02	/* 1=active low */
#define	HSYNC_P_CFG		0x01	/* 1=active low */
#define	TOTAL_LINE_CFG_L_REG	0x12	/* 0x00, Total Line Config Reg Low */
#define	TOTAL_LINE_CFG_H_REG	0x13	/* 0x00, Total Line Config Reg High */
#define	ACTIVE_LINE_CFG_L_REG	0x14	/* 0x00, Active Line Config Reg Low */
#define	ACTIVE_LINE_CFG_H_REG	0x15	/* 0x00, Active Line Config Reg High */
#define	V_F_PORCH_CFG_REG	0x16	/* 0x00, Vertical Front Porch Config Reg */
#define	V_SYNC_CFG_REG	0x17	/* 0x00, Vertical Sync Width Config Reg */
#define	V_B_PORCH_CFG	0x18	/* 0x00, Vertical Back Porch Config Reg */
#define	TOTAL_PIXEL_CFG_L_REG	0x19	/* 0x00, Total Pixel Config Reg Low */
#define	TOTAL_PIXEL_CFG_H_REG	0x1a	/* 0x00, Total Pixel Config Reg High */
#define	ACTIVE_PIXEL_CFG_L_REG	0x1b	/* 0x00, Active Pixel Config Reg Low */
#define	ACTIVE_PIXEL_CFG_H_REG	0x1c	/* 0x00, Active Pixel Config Reg High */
#define	H_F_PORCH_CFG_L_REG	0x1d	/* 0x00, Horizontal Front Porch Reg Low */
#define	H_F_PORCH_CFG_H_REG	0x1e	/* 0x00, Horizontal Front Porch Reg High */
#define	H_SYNC_CFG_L_REG	0x1f	/* 0x00, Horizontal Sync Width Reg Low */
#define	H_SYNC_CFG_H_REG	0x20	/* 0x00, Horizontal Sync Width Reg High */
#define	H_B_PORCH_CFG_L_REG	0x21	/* 0x00, Horizontal Back Porch Reg Low */
#define	H_B_PORCH_CFG_H_REG	0x22	/* 0x00, Horizontal Back Porch Reg High */
#define	VIDEO_STS_REG		0x23	/* 0x03, Video Status Register */
#define	I_SCAN_S		0x04	/* 1=Interlace scan; 0=Progressive scan */
#define	VSYNC_P_S		0x02	/* 1=Active low */
#define	HSYNC_P_S		0x01	/* 1=Active low */
#define	TOTAL_LINE_STA_L_REG	0x24	/* 0x01, Total Line Status Reg Low */
#define	TOTAL_LINE_STA_H_REG	0x25	/* 0x00, Total Line Status Reg High */
#define	ACTIVE_LINE_STA_L_REG	0x26	/* 0x00, Active Line Status Reg Low */
#define	ACTIVE_LINE_STA_H_REG	0x27	/* 0x00, Active Line Status Reg High */
#define	V_F_PORCH_STA_REG	0x28	/* 0x01, Vertical front porch status */
#define	V_SYNC_STA_REG		0x29	/* 0x00, Vertical sync width status */
#define	V_B_PORCH_STA_REG	0x2a	/* 0x00, Vertical Back Porch Status */
#define	TOTAL_PIXEL_STA_L_REG	0x2b	/* 0x00, Total pixel status low */
#define	TOTAL_PIXEL_STA_H_REG	0x2c	/* 0x00, Total pixel status high */
#define	ACTIVE_PIXEL_STA_L_REG	0x2d	/* 0x00, Active pixel status low */
#define	ACTIVE_PIXEL_STA_H_REG	0x2e	/* 0x00, Active pixel status high */
#define	H_F_PORCH_STA_L_REG	0x2f	/* 0x00, Horizontal front porch status low */
#define	H_F_PORCH_STA_H_REG	0x30	/* 0x00, Horizontal front porch status high */
#define	H_SYNC_STA_L_REG	0x31	/* 0x00, Horizontal sync status low */
#define	H_SYNC_STA_H_REG	0x32	/* 0x00, Horizontal sync status high */
#define	H_B_PORCH_STA_L_REG	0x33	/* 0x00, Horizontal back porch status low */
#define	H_B_PORCH_STA_H_REG	0x34	/* 0x00, Horizontal back porch status high */
#define	VIDEO_BIST_REG		0x35	/* 0x00, Video Interface BIST Register */
#define	SPDIF_AUDIO_CTL0_REG	0x36	/* 0x00, S/PDIF Audio Control Register 0 */
#define	AUD_SPDIF_IN		0x80	/* 1=Enable SPDIF audio stream input */
#define	SPDIF1_SEL		0x08	/* 0=Select SPDIF1 pin0; 1=pin1 */
#define	SPDIF_AUDIO_CTL1_REG	0x37	/* 0x00, S/PDIF Audio Control Register 1 */
#define	SPDIF_AUDIO_STA0_REG	0x38	/* 0x00, S/PDIF Audio Status Register 0 */
#define	SPDIF_CLK_DET		0x80	/* 1=clock detected. Input SPDIF audio clock detected indicator */
#define	SPDIF_DET		0x01	/* 1=input detected. Detect indicator of SPDIF audio input */
#define	SPDIF_AUDIO_STA1_REG	0x39	/* 0x00, S/PDIF Audio Status Register 1 */
#define	SPDIF_FS_FREQ_44	0x00	/* 44.1kHz */
#define	SPDIF_FS_FREQ_48	0x20	/* 48kHz */
#define	SPDIF_FS_FREQ_32	0x30	/* 32kHz */
#define	SPDIF_FS_FREQ_96	0xa0	/* 96kHz */
#define	VIDEO_BIT_CTRL0_REG	0x40	/* 0x00, Video Bit Control Reg 0 */
#define	VIDEO_BIT_CTRL1_REG	0x41	/* 0x01, Video Bit Control Reg 1 */
#define	VIDEO_BIT_CTRL2_REG	0x42	/* 0x02, Video Bit Control Reg 2 */
#define	VIDEO_BIT_CTRL3_REG	0x43	/* 0x03, Video Bit Control Reg 3 */
#define	VIDEO_BIT_CTRL4_REG	0x44	/* 0x04, Video Bit Control Reg 4 */
#define	VIDEO_BIT_CTRL5_REG	0x45	/* 0x05, Video Bit Control Reg 5 */
#define	VIDEO_BIT_CTRL6_REG	0x46	/* 0x06, Video Bit Control Reg 6 */
#define	VIDEO_BIT_CTRL7_REG	0x47	/* 0x07, Video Bit Control Reg 7 */
#define	AVI_TYPE_REG		0x70	/* 0x00, AVI InfoFrame type code */
#define	AVI_VER_REG		0x71	/* 0x00, AVI InfoFrame version code */
#define	AVI_LEN_REG		0x72	/* 0x00, AVI InfoFrame length */
#define	AVI_DATA0_REG		0x73	/* 0x00, AVI Checksum (used in HDMI mode only) */
#define	AVI_DATA1_REG		0x74	/* 0x00, AVI InfoFrame data bytes */
#define	AUDIO_TYPE_REG		0x83	/* 0x00, Audio InfoFrame type code */
#define	AUDIO_INFOFRAME		0x84	/* Audio InfoFrame */
#define	AUDIO_VER_REG		0x84	/* 0x00, Audio InfoFrame version code */
#define	AUDIO_IF_VER1		0x01
#define	AUDIO_LEN_REG		0x85	/* 0x00, Audio InfoFrame length */
#define	AUDIO_IF_LEN		0x0A
#define	AUDIO_DATA0_REG		0x86	/* 0x00, Audio Checksum (used in HDMI mode only) */
#define	AUDIO_DATA1_REG		0x87	/* 0x00, Audio InfoFrame data bytes */
#define	SPD_TYPE_REG		0x91	/* 0x00, SPD InfoFrame type code */
#define	SPD_VER_REG		0x92	/* 0x00, SPD InfoFrame version code */
#define	SPD_LEN_REG		0x93	/* 0x00, SPD InfoFrame length */
#define	SPD_DATA0_REG		0x94	/* 0x00, SPD Checksum (used in HDMI mode only) */
#define	SPD_DATA1_REG		0x95	/* 0x00, SPD InfoFrame data bytes */
#define	MPEG_TYPE_REG		0xb0	/* 0x00, MPEG InfoFrame type code */
#define	MPEG_VER_REG		0xb1	/* 0x00, MPEG InfoFrame version code */
#define	MPEG_LEN_REG		0xb2	/* 0x00, MPEG InfoFrame length */
#define	MPEG_DATA0_REG		0xb3	/* 0x00, MPEG Checksum (used in HDMI mode only) */
#define	MPEG_DATA1_REG		0xb4	/* 0x00, MPEG InfoFrame data bytes */
#define	AUDIO_BIST1_REG		0xd0	/* 0x00, Audio BIST Chan Status Reg 1 */
#define	AUDIO_BIST2_REG		0xd1	/* 0x00, Audio BIST Chan Status Reg 2 */
#define	AUDIO_BIST3_REG		0xd2	/* 0x00, Audio BIST Chan Status Reg 3 */
#define	AUDIO_BIST4_REG		0xd3	/* 0x00, Audio BIST Chan Status Reg 4 */
#define	AUDIO_BIST5_REG		0xd4	/* 0x0b, Audio BIST Chan Status Reg 5 */
#define	GPIO_CTL_REG		0xd6	/* 0x7?, GPIO Control Register */
#define	LANE_MAP_REG		0xd7	/* 0xe4, Lane Map Register */
#define	E_EDID_PTR_DA_REG	0xd9	/* 0x30, E_EDID Pointer Device Address Register */
#define	EDID_DA_REG		0xda	/* 0x50, EDID Device Address Register */
#define	MCCS_DA_REG		0xdb	/* 0x37, MCCS Device Address Register */
#define	ANALOG_DEBUG_REG1	0xdc
#define	ANALOG_DEBUG_REG3	0xde
#define	DP_TX_PLL_FILTER_CTRL1	0xdf
#define	PLL_FIL_CTL3_REG	0xe1	/* 0x18, PLL Filter Control Register 3 */
#define	PLL_FIL_CTL4_REG	0xe2	/* 0x06, PLL Filter Control Register 4 */
#define	PLLF_PWDN		0x04	/* 1=power down PLL filter */
#define	PLLF_17V		0x03	/* 3=1.7V */
#define	PLLF_16V		0x02	/* 2=1.6V */
#define	PLLF_15V		0x01	/* 1=1.5V */
#define	PLLF_14V		0x00	/* 0=1.4V */
#define	TX_PLL_CTL3_REG		0xe6	/* 0x60, TxPLL Control Register 3 */
#define	TXPLL_CPREG_BLEED	0x40	/* 1=leakage on */
#define	TXPLL_HIGH_1375V_TH	0x20	/* high threshold 1.375V */
#define	TXPLL_HIGH_1300V_TH	0x18	/* high threshold 1.3V */
#define	TXPLL_LOW_TH_CTL	0x00	/* low threshold=Vth V */
#define	TXPLL_LOW_075V_TH	0x01	/* low threshold=Vth V + 0.075V */
#define	INT_STATE_REG		0xf0	/* 0x00, Interrupt Status Register */
#define	INT_STATE	0x01	/* 1=Interrupt service requested */
#define	INT_COM_STS1_REG	0xf1	/* 0x00, Common Interrupt Status Reg 1 */
#define	INT_COM_STS2_REG	0xf2	/* 0x00, Common Interrupt Status Reg 2 */
#define	INT_COM_STS3_REG	0xf3	/* 0x00, Common Interrupt Status Reg 3 */
#define	AFIFO_UNDER		0x80
#define	R0_CHK_FLAG		0x20
#define	RI_NO_UPDATE		0x08
#define	SYNC_POST_CHK_FAIL	0x02

#define	INT_COM_STS4_REG	0xf4	/* 0x00, Common Interrupt Status Reg 4 */
#define	ANX_HOTPLUG_CHG		0x04
#define	ANX_HPD_LOST		0x02
#define	ANX_PLUG		0x01

#define	INT_HDMI_STS1_REG	0xf5	/* 0x00, HDMI Interrupt Status Reg 1 */
#define	INT_HDMI_STS2_REG	0xf6	/* 0x00, HDMI Interrupt Status Reg 2 */
#define	INT_DP_STS1_REG		0xf7	/* 0x00, DP Interrupt Status Register 1 */

#define	INT_COM_MSK1_REG	0xf8	/* 0x00, Interrupt Common Mask Reg 1 */
#define	AUD_CLK_CHG	0x04	/* 1=audio input clock change detected */
#define	VID_CLK_CHG	0x02	/* 1=video input clock change detected */

#define	INT_COM_MSK2_REG	0xf9	/* 0x00, Interrupt Common Mask Reg 2 */
#define	AUTH_STATE_CHG	0x02	/* 1=hardware hdcp auth state is changed */
#define	AUTH_DONE	0x01	/* 1=hdcp auth has ended */

#define	INT_COM_MSK3_REG	0xfa	/* 0x00, Interrupt Common Mask Reg 3 */
#define	INT_COM_MSK4_REG	0xfb	/* 0x00, Interrupt Common Mask Reg 4 */
#define	HOTPLUG_CHG	0x04	/* 1=int asserted when hotplug detected */
#define	HPD_LOST	0x02	/* 1=int asserted when hot plug detect signal lost for more than 2ms */
#define	PLUG_IN		0x01	/* 1=int asserted when HPD goes high for greater than 2ms */

#define	INT_HDMI_MSK1_REG	0xfc	/* 0x00, Interrupt Hdmi Mask Reg 1 */
#define	INT_HDMI_MSK2_REG	0xfd	/* 0x00, Interrupt Hdmi Mask Reg 2 */
#define	INT_DP_MSK1_REG		0xfe	/* 0x00, Interrupt DP Mask Reg 1 */
#define	INT_CTL_REG		0xff	/* 0x02, Interrupt Control Reg */
#define	OPEN_DRAIN	0x02	/* 1=open drain output */
#define	INT_POL_HIGH	0x01	/* 1=active high */

/******************************************************************************
 *	ANX9805 DP Mode Registers
 ******************************************************************************/
#define	HDCP_STATUS_REG	0x00	/* 0x00, HDCP Status Register */
#define	HDCP_CTL0_REG	0x01	/* 0x00, HDCP Control Register 0 */
#define	RE_AUTHEN	0x20	/* set 1 then clear: restart hdcp authen */
#define	HDCP_ENC_EN	0x04	/* 1=enable hdcp encryption mode */
#define	BKSV_SRM_PASS	0x02	/* BKSV SRM Check indicator status in HDCP */
#define	KSVLIST_VLD	0x01	/* HDCP Repeater KSV list check indicator in HDCP authentication */
#define	HDCP_CTL1_REG	0x02	/* 0x89, HDCP Control Register 1 */
#define	HDCP_LINK_CHK_FRM_NUM_REG	0x03	/* 0x00: HDCP Link Check Frame Number Register */
#define	HDCP_CTL2_REG	0x04	/* 0xc0: HDCP Control Register 2 */
#define	LINK_DBL_CHK_EN	0x40	/* Enable link auto checking of HDCP 3rd step auth */

#define	HDCP_WAIT_R0_TIM_REG	0x40	/* HDCP Wait R0 Timing Register */
#define	HDCP_LINK_CHK_TIM_REG	0x41	/* HDCP Link Integrity Check Timer Register */
#define	HDCP_REP_RDY_WTIM_REG	0x42	/* HDCP Repeater Ready Wait Timer Register */

#define	DP_SCTL1_REG	0x80	/* 0x00, DP System Control 1 Register */
#define	DP_DET_STA	0x04	/* Stream clock detect status */
#define	DP_FORCE_STA	0x02	/* Force stream clock detect */
#define	DP_DET_CTRL	0x01	/* Stream clock detect status control */
#define	DP_SCTL2_REG	0x81	/* 0x40, DP System Control 2 Register */
#define	DP_CHA_STA	0x04	/* Clock change status */
#define	DP_FORCE_CHA	0x02	/* Force stream clock change status */
#define	DP_CHA_CTRL	0x01	/* Stream clock change status control */
#define	DP_SCTL3_REG	0x82	/* 0x00, DP System Control 3 Register */
#define	HPD_STATUS	0x40	/* Hot plug detect status */
#define	F_HPD		0x20	/* Force Hot plug detect */
#define	HPD_CTRL	0x10	/* Hot plug detect manual control */
#define	DP_SCTL4_REG	0x83	/* 0x00, DP System Control 4 Register */
#define	DP_VIDEO_CTL_REG	0x84	/* 0x00, DP Video Control Register */
#define	DP_DATA_ADELAY_REG	0x85	/* 0x0A, Data Assert Delay Register */
#define	DP_AUDIO_CTL_REG	0x86	/* 0x00: DP Audio Control Register */
#define	DP_PACKET_CTL_REG	0x90	/* 0x00: DP Packet Control Register */
#define	DP_HDCP_CTL_REG		0x92	/* 0x00: DP HDCP Control Register */

#define	DP_SPDIF_P1_CTL0_REG	0x94	/* 0x00: DP SPDIF Phase 1 Control Register 0 (debug only) */
#define	DP_SPDIF_P1_CTL1_REG	0x95	/* 0x00: DP SPDIF Phase 1 Control Register 1 (debug only) */
#define	DP_SPDIF_P2_CTL0_REG	0x96	/* 0x00: DP SPDIF Phase 2 Control Register 0 (debug only) */
#define	DP_SPDIF_P2_CTL1_REG	0x97	/* 0x00: DP SPDIF Phase 2 Control Register 1 (debug only) */
#define	DP_SPDIF_P3_CTL0_REG	0x98	/* 0x00: DP SPDIF Phase 3 Control Register 0 (debug only) */
#define	DP_SPDIF_P3_CTL1_REG	0x99	/* 0x00: DP SPDIF Phase 3 Control Register 1 (debug only) */

#define	DP_MLINK_BW_REG		0xa0	/* 0x0a: DP Main Link Bandwidth Setting Register */
#define	DP_LINK_BW_2_70G	0x0a	/* Main link bandwidth setting: 2.70 Gbps/line */
#define	DP_LINK_BW_1_62G	0x06	/* Main link bandwidth setting: 1.62 Gbps/line */
#define	DP_MLINK_LANE_CNT_REG	0xa1	/* 0x04: DP Main Link Lane Count Register */
#define	DP_TRAIN_PATSET_REG	0xa2	/* 0x00: DP Training Pattern Set Register */
#define	DP_LANE0_LINK_CTL_REG	0xa3	/* 0x00: DP Lane 0 Link Training Control Register */
#define	DP_LANE1_LINK_CTL_REG	0xa4	/* 0x00: DP Lane 1 Link Training Control Register */
#define	DP_LANE2_LINK_CTL_REG	0xa5	/* 0x00: DP Lane 2 Link Training Control Register */
#define	DP_LANE3_LINK_CTL_REG	0xa6	/* 0x00: DP Lane 3 Link Training Control Register */
#define	DP_DOWN_SPREAD_CTL_REG	0xa7	/* 0x01: DP Down Spreading Control Register */
#define	DP_HW_LINK_TRAIN_CTL_REG	0xa8	/* 0x00: DP Hardware Link Training Control Register */

#define	DP_DEBUG1_REG		0xb0	/* 0x00: DP Debug1 Register */
#define	DP_DEBUG1_PLL_LOCK	0x10	/* PLL Lock */
#define	DP_POLL_PERIOD_REG	0xb3	/* 0x0e: DP Polling Period Register */
#define	DP_POLL_CTL_REG		0xb4	/* 0x00: DP Polling Control Register */
#define	DP_LINK_DBG_CTL_REG	0xb8	/* 0x10: DP Link Debug Control Register */
#define	DP_SINK_CNT_RESULT_REG	0xb9	/* 0x00: DP Sink Count Result Register */
#define	DP_IRQ_VECT_RESULT_REG	0xba	/* 0x00: DP IRQ Vector Result Register */
#define	DP_LINK_STS_RESULT1_REG	0xbb	/* 0x00: DP Link Status Result Register 1 */
#define	DP_LINK_STS_RESULT2_REG	0xbc	/* 0x00: DP Link Status Result Register 2 */
#define	DP_ALIGN_STS_REG	0xbd	/* 0x00: DP Align Status Register */
#define	DP_SINK_STS_RESULT_REG	0xbe	/* 0x00: DP Sink Status Result Register */

#define	DP_M_VID0_REG	0xc0	/* 0x00: DP M_VID Configuration Register 1 */
#define	DP_M_VID1_REG	0xc1	/* 0x00: DP M_VID Configuration Register 2 */
#define	DP_M_VID2_REG	0xc2	/* 0x00: DP M_VID Configuration Register 3 */
#define	DP_N_VID0_REG	0xc3	/* 0x00: DP N_VID Configuration Register 1 */
#define	DP_N_VID1_REG	0xc4	/* 0x80: DP N_VID Configuration Register 2 */
#define	DP_N_VID2_REG	0xc5	/* 0x00: DP N_VID Configuration Register 3 */

#define	DP_AUX_CH_DBG_REG	0xc6	/* 0x00: DP AUX CH Debug Register */
#define	DP_0xC7			0xc7	/* undocument, TX_PLL_CTRL */
#define	DP_TX_PLL_CTRL_PLL_RESET	0x40
#define	DP_TX_PLL_PWR_19V		0x05	/* PLL power 1.9V */
#define	DP_TX_PLL_PWR_17V		0x02	/* PLL power 1.7V */
#define	DP_ANALOG_PWRDOWN_REG	0xc8	/* 0x00: DP Analog Power down Register */
#define	ANALOG_PWRDOWN_MACRO_PD	0x20	/*  */
#define	ANALOG_PWRDOWN_AUX_PD	0x10	/*  */
#define	ANALOG_PWRDOWN_CH3_PD	0x08	/*  */
#define	ANALOG_PWRDOWN_CH2_PD	0x04	/*  */
#define	ANALOG_PWRDOWN_CH1_PD	0x02	/*  */
#define	ANALOG_PWRDOWN_CH0_PD	0x01	/*  */
#define	DP_ANALOG_TEST_REG	0xc9	/* DP Analog Power down Register (no datasheet) */
#define	ANALOG_TEST_MACRO	0x20	/*  */
#define	ANALOG_TEST_AUX		0x10	/*  */
#define	ANALOG_TEST_CH3		0x08	/*  */
#define	ANALOG_TEST_CH2		0x04	/*  */
#define	ANALOG_TEST_CH1		0x02	/*  */
#define	ANALOG_TEST_CH0		0x01	/*  */
#define	DP_MISC_CTL_REG		0xcd	/* 0x18: DP Miscellaneous Control Register */
#define	DP_EXTRA_I2C_REG	0xce	/* 0xd0: DP Extra I2C Register */
#define	EXTRA_I2C_ADDR		0x50	/* Set extra I2C device address for EDID reading via AUX channel */
#define	DP_DOWN_SPREAD_CTL1_REG	0xd0	/* 0x00: DP Down Spreading Control Register 1 */
#define	DP_DOWN_SPREAD_CTL2_REG	0xd1	/* 0x00: DP Down Spreading Control Register 2 */
#define	DP_I2C_STRETECH_TO_CTL_REG	0xda	/* 0x02: DP I2C Stretch Timeout Control Register */

#define	DP_M_AUD0_REG	0xd2	/* 0x00: DP M_AUD Contiguration Register 0 */
#define	DP_M_AUD1_REG	0xd3	/* 0x80: DP M_AUD Contiguration Register 1 */
#define	DP_M_AUD2_REG	0xd4	/* 0x00: DP M_AUD Contiguration Register 2 */
#define	DP_N_AUD0_REG	0xd5	/* 0x00: DP N_AUD Contiguration Register 0 */
#define	DP_N_AUD1_REG	0xd6	/* 0x80: DP N_AUD Contiguration Register 1 */
#define	DP_N_AUD2_REG	0xd7	/* 0x00: DP N_AUD Contiguration Register 2 */

#define	DP_M_CALCU_CTRL_REG	0xd8	/* 0x00 M Value Calculation Control Register */
#define	M_GEN_CLK_SEL		0x01	/*  */
#define	DP_M_VID_CAL_CTL_REG	0xd9	/* 0x04: M_VID Value Calculation Control Register */
#define	DP_M_AUD_GEN_FIL_TER_REG	0xde	/* 0x02: M_AUD Value Calculation Control Register   */

#define	DP_AUX_CH_ACC_STS_REG	0xe0	/* 0x00: AUX CH Access Status Register   */
#define	DP_AUX_CH_DEFER_CTL_REG	0xe1	/* 0x00: AUX CH DEFER Control Register   */
#define	DP_AUX_CH_DEFER_CTL1_REG	0xe2	/* 0x7f: AUX CH DEFER Control Register   */
#define	DP_AUX_CH_REC_CMD_REG	0xe3	/* 0x00: AUX Receiver Command Register   */
#define	DP_BUF_DATA_CNT_REG	0xe4	/* 0x00: Buffer Data Count Register   */
#define	DP_AUX_CH_CTL_REG	0xe5	/* 0x00: AUX CH Control Register   */
#define	DP_AUX_CH_ADDR_7_0_REG	0xe6	/* 0x00: AUX CH Address[7:0] Register   */
#define	DP_AUX_CH_ADDR_15_8_REG	0xe7	/* 0x00: AUX CH Address[15:8] Register   */
#define	DP_AUX_CH_ADDR_19_16_REG	0xe8	/* 0x00: AUX CH Address[19:16] Register   */
#define	DP_AUX_CH_CTL2_REG	0xe9	/* 0x00: AUX CH Control Register 2 */
#define	DP_AUX_BUF_REG		0xf0	/* 0x00: AUX CH Buffer Data Registers 0xf0-0xff */

/******************************************************************************
 *	ANX9805 HDMI Mode Registers
 ******************************************************************************/
#define	HDMI_RST_REG	0x00	/* 0x00, HDMI Reset Register */
#define	TMDS_CHNL_ALIGN		0x01	/* TMDS analog four channels clock alignment reset 1=reset */
#define	HDMI_SSTS_REG	0x01	/* 0x00, HDMI System Status Register */
#define	TXPLL_MISC_LOCK	0x40	/* Lock detected indicator of TX PLL misc control 1=lock detected */
#define	HOT_PLUG	0x08	/* 1=HDMI receiver present 5V */
#define	CLK_DET		0x02	/* 1=Input video clock detected */
#define	RSV_DET		0x01	/* receiver is active */
#define	HDMI_SCTRL1_REG	0x02	/* 0x04, HDMI System Control Register 1 */
#define	HDCP_MODE_EN	0x04	/* 1=HDCP mode enabled */
#define	HDMI_MODE_EN	0x02	/* 1=HDMI mode enabled */
#define	VID_STABLE	0x01	/* 1=input video data is stable */
#define	HDMI_VCTRL1_REG	0x03	/* 0x00, HDMI Video Control Register 1 */
#define	VH_SYNC_ALIGN_EN	0x02	/* Enable control ind VSYNC */
#define	HDMI_AUD_MUTE		0x01	/* Enable control ind for sending zeros in audio sample packet */
#define	HDMI_VCTRL2_REG	0x04	/* 0x00, HDMI Video Control Register 2 */
#define	HDMI_LINK_VID_EN	0x04	/* 1=enable to control Link FSM to progress and not consider video stream valid */
#define	HDMI_CAP_CTL_REG	0x05	/* 0x00, HDMI Video Capture Control Register */
#define	HDMI_LNKFMT1_OFF_REG	0x06	/* 0x00, HDMI Link Format Lines Offset Register 1 */
#define	HDMI_LNKFMT2_OFF_REG	0x07	/* 0x00, HDMI Link Format Lines Offset Register 2 */
#define	HDMI_AUDIO_CTL1_REG	0x08	/* 0x00, HDMI Audio Control Register 1 */
#define	HDMI_AUDIO_CTL_REG	0x09	/* 0x40, HDMI Audio Control Register */
#define	HDMI_AUD_EN		0x80	/* 1=enable HDMI audio stream on the HDMI link */
#define	PD_RING_OSC		0x40	/* 1=Powerdown ring oscillator, 0=powerup */
#define	HDMI_LNK1_CTL_REG	0x30	/* 0x00, HDMI Link Control Register 1 */
#define	HDMI_LNK2_CTL_REG	0x31	/* 0x02, HDMI Link Control Register 2 */
#define	HDMI_LNK_MUTE_EX_EN_REG	0x32	/* 0x00, HDMI Link Mute Exception Enable Register */
#define	SERDES_TEST_PTRN0_REG	0x33	/* 0x00, TMDS SerDes Functional Test Pattern Register 1 [7:0] */
#define	SERDES_TEST_PTRN1_REG	0x34	/* 0x00, TMDS SerDes Functional Test Pattern Register 2 [15:8] */
#define	SERDES_TEST_PTRN2_REG	0x35	/* 0x00, TMDS SerDes Functional Test Pattern Register 3 [19:16] */
#define	PLL_MISC_CTL1_REG	0x38	/* 0xb0, Chip PLL Miscellaneous Control Register 1 */
#define	MISC_TIMER_SEL		0x10
#define	MISC_RNGCHK_EN		0x01
#define	PLL_MISC_CTL2_REG	0x39	/* 0x80, Chip PLL Miscellaneous Control Register 2 */
#define	MISC_MODE_SEL		0x20
#define	FORCE_PLLF_LOCK		0x02
#define	MISC_TXPLL_MAN_RNG	0x01
#define	VID_FREQ_CNT_REG	0x3a	/* 0x00, Video Input Clock Frequency Counter Register */
#define	DDC_DEV_ADDR_REG	0x40	/* 0x00, DDC Slave Device Address Register */
#define	DDC_SEG_ADDR_REG	0x41	/* 0x00, DDC Slave Device Segment Address Register */
#define	DDC_OFFSET_ADDR_REG	0x42	/* 0x00, DDC Slave Device Offset Address Register */
#define	DDC_ACCESS_CMD_REG	0x43	/* 0x00, DDC Access Command Register */
#define	DDC_I2C_RESET		0x06	/* 110=I2C reset command */
#define	CLR_DDC_DFIFO		0x05	/* 101=Clear DDC Data FIFO */
#define	ESEQ_BYTE_READ		0x04	/* 100=Enhanced DDC Sequential Read */
#define	DDC_HDCP_IOAR		0x03	/* 011=Implicit Offset Address Read (HDCP) */
#define	SEQ_BYTE_WRITE		0x02	/* 010=Sequential Byte Write */
#define	SEQ_BYTE_READ		0x01	/* 001=Sequential Byte Read */
#define	SINGLE_BYTE_READ	0x81	/* 001=Single Byte Read */
#define	DDC_ABORT_OP		0x00	/* 000=Abort current operation */
#define	DDC_ACCESS_NUM0_REG	0x44	/* 0x00, DDC Access Number Register 0 B[7:0] */
#define	DDC_ACCESS_NUM1_REG	0x45	/* 0x00, DDC Access Number Register 1 B[9:8] */
#define	DDC_CH_STATUS_REG	0x46	/* 0x10, DDC Channel Status Register */
#define	DDC_ERROR		0x80	/* An error has occurred when accessing the DDC channel */
#define	DDC_OCCUPY		0x40	/* DDC channel is accessed by an external device */
#define	DDC_FIFO_FULL		0x20	/* Indicator of FIFO full status */
#define	DDC_FIFO_EMPTY		0x10	/* Indicator of FIFO empty status */
#define	DDC_NO_ACK		0x08	/* No acknowledge detection has occurred when accessing the DDC cahnnel */
#define	DDC_READ		0x04	/* Indicator of FIFO being read */
#define	DDC_WRITE		0x02	/* Indicator of FIFO being written */
#define	DDC_PROGRESS		0x01	/* Indicator of DDC operation in process */
#define	DDC_FIFO_DATA_REG	0x47	/* 0x00, DDC FIFO Access Register */
#define	DDC_FIFO_CNT_REG	0x48	/* 0x00, DDC FIFO ACount Register */
#define	DDC_FIFO_MAX_CNT	0x1f	/* The number of bytes in the DDC FIFO */
#define	DBG_LINK_FSM1_REG	0x49	/* 0x00, Debug Shared/Chip Link FSM Debug Status Register 1 */
#define	DBG_LINK_FSM2_REG	0x4a	/* 0x00, Debug Shared/Chip Link FSM Debug Status Register 2 */

#define	HDMI_DBG_CTRL1_REG	0x4c	/* 0x00, HDMI Debug Control Register 1 */
#define	TMDS_PD_REG		0x60	/* 0x00, System Power Down Register */
#define	TMDS_CH_CFG1_REG	0x61	/* 0x0c, HDMI TMDS Channel Configuration Register 1 */
#define	TMDS_CH_CFG2_REG	0x62	/* 0x0c, HDMI TMDS Channel Configuration Register 2 */
#define	TMDS_CH_CFG3_REG	0x63	/* 0x0c, HDMI TMDS Channel Configuration Register 3 */
#define	TMDS_CH_CFG4_REG	0x64	/* 0x0c, HDMI TMDS Channel Configuration Register 4 */
#define	TMDS_CLK_NO_MUTE_CTRL	0x40	/* 1=don't mute TMDS clock */
#define	HDMI_CHIP_CTRL_REG	0x65	/* 0x02, HDMI Chip Control Register */
#define	HDMI_CHIP_STS_REG	0x66	/* 0x0?, 000011?0b HDMI Chip Status Register */
#define	HDMI_CHIP_DCTRL1_REG	0x67	/* 0x30, HDMI Chip Debug Control Register 1 */
#define	HDMI_FORCE_HOTPLUG	0x01	/* Force Hot plug detect. for debug use only */

#define	IF_PKT_CTRL1_REG	0x70	/* 0x00, InfoFrame Packet Control Register 1 */
#define	AVI_PKT_RPT		0x20	/* 1=enable control of AVI packet transmission in every VBLANK period */
#define	AVI_PKT_EN		0x10	/* 1=enable control of AVI packet transmission */
#define	GCP_PKT_RPT		0x08	/* 1=Enable control of Control Packet transmission in every VBLANK period */
#define	GCP_PKT_EN		0x04	/* 1=Enable control of Control Packet transmission */
#define	ACR_PKT_NEW		0x02	/* 1=only new CTS value packet is sent */
#define	ACR_PKT_EN		0x01	/* 1=enable control of ACR packet transmission */
#define	IF_PKT_CTRL2_REG	0x71	/* 0x00, InfoFrame Packet Control Register 2 */
#define	AIF_PKT_RPT		0x02	/* 1=enable control of audio InfoFrame packet transmission in every VBLANK period */
#define	AIF_PKT_EN		0x01	/* 1=enable control of audio InfoFrame packet transmission */
#define	ACR_SVAL1_REG		0x72	/* 0x00, ACR N Software Value Register 1 */
#define	ACR_SVAL2_REG		0x73	/* 0x00, ACR N Software Value Register 2 */
#define	ACR_SVAL3_REG		0x74	/* 0x00, ACR N Software Value Register 3 */
#define	ACR_CTS_SVAL1_REG	0x75	/* 0x00, ACR CTS Software Value Register 1 */
#define	ACR_CTS_SVAL2_REG	0x76	/* 0x00, ACR CTS Software Value Register 2 */
#define	ACR_CTS_SVAL3_REG	0x77	/* 0x00, ACR CTS Software Value Register 3 */
#define	ACR_CTS_HVAL1_REG	0x78	/* 0x00, ACR CTS Hardware Value Register 1 */
#define	ACR_CTS_HVAL2_REG	0x79	/* 0x00, ACR CTS Hardware Value Register 2 */
#define	ACR_CTS_HVAL3_REG	0x7a	/* 0x00, ACR CTS Hardware Value Register 3 */
#define	ACR_CTS_CTL_REG		0x7b	/* 0x10, ACR CTS Control Register */
#define	GEN_CTL_PKT_REG		0x7c	/* 0x80, General Control Packet Register */
#define	GCP_DCP_CTRL		0x80	/* enable control indicator of deep color packet */
#define	DC_PKT_EN		0x40	/* enable indicator of deep color packet trans */
#define	DC_PKT_MODE_CLR		0x3c	/* clear color depth values sent to sink */
#define	DC_PKT_MODE_24B		0x10	/* 24bit/pixel */
#define	DC_PKT_MODE_30B		0x14	/* 30bit/pixel */
#define	DC_PKT_MODE_36B		0x18	/* 36bit/pixel */
#define	CLR_AVMUTE		0x02	/* clear the AVMUTE flag */
#define	SET_AVMUTE		0x01	/* set the AVMUTE flag */
#define	AUDIO_PKT_FL_CTL_REG	0x7d	/* 0x30, Audio Packet Flatline Control Register */
#define	GEN_CTL_PKT_HID_REG	0x7e	/* 0x03, General Control Packet Header ID Register */
#define	AUDIO_PKT_HID_REG	0x7f	/* 0x02, Audio Packet Header ID Register */

#define	CEC_CTL_REG		0x80	/* 0x00, CEC Control Register */
#define	CEC_REC_STS_REG		0x81	/* 0x10, CEC Receiver Status Register */
#define	CEC_TXR0_STS_REG	0x82	/* 0x10, CEC Transmitter Status Register 0 */
#define	CEC_TXR1_STS_REG	0x83	/* 0x00, CEC Transmitter Status Register 1 */
#define	CEC_SPD_CTL_REG		0x84	/* 0x00, CEC Speed Control Register */
		/* 7:4: 0x0=28MHz crystal */

int g2_anx9805_init(void);
int anx9805_i2c_write(struct i2c_client *i2c_cli, u8 reg, u8 value);
int anx9805_i2c_read(struct i2c_client *i2c_cli, u8 reg, u8 *value_p);

#endif /* __ANX9805_HDMI_H__ */
