/*
 * File: sound/soc/codec/dae4p.c
 * Description: ASoC codec driver for Intersil DAE4P(D2-45057/D2-45157) codec
 *
 * Copyright (c) Cortina-Systems Limited 2012. All rights reserved.
 *
 * Mostly copied from wm8750.c and some from wm8994.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "dae4p.h"

#define DAE4P_TRACE	BIT(0)
#define DAE4P_INFO	BIT(1)

static unsigned int dae4p_debug = 0;//DAE4P_TRACE | DAE4P_INFO;

#define OPTIONAL_IMPLEMENT	0	/* Need Intersil's Support */

#define RESERVED_REG 0xFFFFFF

/*
 * dae4p register cache & default register settings
 */
static const u32 dae4p_reg[] = {
	0x1F0000,	0xE00000,	0xE00000,	0xE00000, /* 0x000 */
	0xE00000,	0xE00000,	0x800000,	0x000000, /* 0x004 */
	0x000000,	0x800000,	0x834ED4,	0x000000, /* 0x008 */
	0x9DC83B,	0x000000,	0x834ED4,	0x000000, /* 0x00C */
	0x9DC83B,	0x000000,	0x0000E4,	0x24011D, /* 0x010 */
	0x7C0000,	0x7EB852,	0x04322A,	0x006D0B, /* 0x014 */
	0x020000,	0x24011D,	0x7C0000,	0x7EB852, /* 0x018 */
	0x04322A,	0x006D0B,	0x020000,	0x022222, /* 0x01C */
	0x0ECE1F,	0x000000,	0x022222,	0x0ECE1F, /* 0x020 */
	0x000000,	0x022222,	0x0ECE1F,	0x000000, /* 0x024 */
	0x022222,	0x0ECE1F,	0x000000,	0x022222, /* 0x028 */
	0x0ECE1F,	0x000000,	0x022222,	0x0ECE1F, /* 0x02C */
	0x000000,	0x022222,	0x0ECE1F,	0x000000, /* 0x030 */
	0x022222,	0x0ECE1F,	0x000000,	0x022222, /* 0x034 */
	0x0ECE1F,	0x000000,	0x022222,	0x0ECE1F, /* 0x038 */
	0x000000,	0x055555,	0x0ECE1F,	0x000000, /* 0x03C */
	0x055555,	0x0ECE1F,	0x000000,	0x055555, /* 0x040 */
	0x0ECE1F,	0x000000,	0x055555,	0x0ECE1F, /* 0x044 */
	0x000000,	0x055555,	0x0ECE1F,	0x000000, /* 0x048 */
	0x055555,	0x0ECE1F,	0x000000,	0x055555, /* 0x04C */
	0x0ECE1F,	0x000000,	0x055555,	0x0ECE1F, /* 0x050 */
	0x000000,	0x055555,	0x0ECE1F,	0x000000, /* 0x054 */
	0x055555,	0x0ECE1F,	0x000000, 	0x055555, /* 0x058 */
	0x0ECE1F,	0x000000,	0x055555,	0x0ECE1F, /* 0x05C */
	0x000000,	0x022222,	0x0ECE1F,	0x000000, /* 0x060 */
	0x022222,	0x0ECE1F,	0x000000,	0x022222, /* 0x064 */
	0x0ECE1F,	0x000000,	0x022222,	0x0ECE1F, /* 0x068 */
	0x000000,	0x022222,	0x0ECE1F,	0x000000, /* 0x06C */
	0x022222,	0x0ECE1F,	0x000000,	0x022222, /* 0x070 */
	0x0ECE1F,	0x000000,	0x022222,	0x0ECE1F, /* 0x074 */
	0x000000,	0x022222,	0x0ECE1F,	0x000000, /* 0x078 */
	0x022222,	0x0ECE1F,	0x000000,	0x800000, /* 0x07C */
	0x7FFFFF,	0x7641AF,	0x800000,	0x7FFFFF, /* 0x080 */
	0x30FBC5,	0x800000,	0x7FFFFF,	0x7641AF, /* 0x084 */
	0x800000,	0x7FFFFF,	0x30FBC5,	0x800000, /* 0x088 */
	0x7FFFFF,	0x7641AF,	0x800000,	0x7FFFFF, /* 0x08C */
	0x30FBC5,	0x800000,	0x7FFFFF,	0x7641AF, /* 0x090 */
	0x800000,	0x7FFFFF,	0x30FBC5,	0x800000, /* 0x094 */
	0x7FFFFF,	0x7641AF,	0x800000,	0x7FFFFF, /* 0x098 */
	0x30FBC5,	0x800000,	0x7FFFFF,	0x7641AF, /* 0x09C */
	0x800000,	0x7FFFFF,	0x30FBC5,	0x800000, /* 0x0A0 */
	0x7FFFFF,	0x7641AF,	0x800000,	0x7FFFFF, /* 0x0A4 */
	0x30FBC5,	0x800000,	0x7FFFFF,	0x7641AF, /* 0x0A8 */
	0x800000,	0x7FFFFF,	0x30FBC5,	0x400000, /* 0x0AC */
	0x400000,	0x800000,	0x7FFFFF,	0x7641AF, /* 0x0B0 */
	0x800000,	0x7FFFFF,	0x30FBC5,	0x800000, /* 0x0B4 */
	0x7FFFFF,	0x7641AF,	0x800000,	0x7FFFFF, /* 0x0B8 */
	0x30FBC5,	0x24011D,	0x7C0000,	0x7EB852, /* 0x0BC */
	0x04322A,	0x006D0B,	0x020000,	0x24011D, /* 0x0C0 */
	0x7C0000,	0x7EB852,	0x04322A,	0x006D0B, /* 0x0C4 */
	0x020000,	0x24011D,	0x7C0000,	0x7EB852, /* 0x0C8 */
	0x04322A,	0x006D0B,	0x020000,	0x24011D, /* 0x0CC */
	0x7C0000,	0x7EB852,	0x04322A,	0x006D0B, /* 0x0D0 */
	0x020000,	0x24011D,	0x7C0000,	0x7EB852, /* 0x0D4 */
	0x04322A,	0x006D0B,	0x020000,	0x000000, /* 0x0D8 */
	0x81AA26,	0xEF2603,	0x000000,	0x81AA26, /* 0x0DC */
	0xEF2603,	0x000000,	0x81AA26,	0xEF2603, /* 0x0E0 */
	0x000000,	0x81AA26,	0xEF2603,	0x000000, /* 0x0E4 */
	0x81AA26,	0xEF2603,	RESERVED_REG,	RESERVED_REG,/* 0x0E8 */
	0xC0E09C,	0x03E793,	0x80AB20,	0xF1FEB3, /* 0x0EC */
	0xE00000,	0x800000,	0x7FFFFF,	0x000002, /* 0x0F0 */
	0x11999A,	0x5A85F9,	0x000002,	0x11999A, /* 0x0F4 */
	0x5A85F9,	0x000006,	0x000006,	0x13BBBC, /* 0x0F8 */
	0x13BBBC,	0x008889,	0x008889,	0x00369D, /* 0x0FC */
	0x00369D,	0x00CCCD,	0x00CCCD,	0x006D3A, /* 0x100 */
	0x006D3A,	0x800000,	0x800000,	0x800000, /* 0x104 */
	0x800000,	0x00CCCD,	0x001B4F,	0x00001F, /* 0x108 */
	0x40348E,	0x08408D,	0x08408D,	0x140000, /* 0x10C */
	0x000000,	0x11999A,	0x000000,	0xB33333, /* 0x110 */
	0x000000,	0x400000,	0x616CB1,	0x08408D, /* 0x114 */
	0x08408D,	0x3298B0,	0x100000,	0x100000, /* 0x118 */
	0x011111,	0x011111,	0x000000,	0x0B3333, /* 0x11C */
	0xD9999A,	0xE66666,	0x333333,	0x19999A, /* 0x120 */
	0x355555,	0x355555,	0x0AAAAB,	0x0AAAAB, /* 0x124 */
	0xA56208,	0x800000,	0x100000,	0x100000, /* 0x128 */
	0x011111,	0x011111,	0x000000,	0x0B3333, /* 0x12C */
	0xD9999A,	0xE66666,	0x333333,	0x19999A, /* 0x130 */
	0x000000,	0x355555,	0x7641B1,	0x355555, /* 0x134 */
	0x0AAAAB,	0x0AAAAB,	0xA56208,	0x800000, /* 0x138 */
	0x000000,	RESERVED_REG,	0x000000,	0x400000, /* 0x13C */
	0x1C73D5,	0x23D1CD,	0x3FDF7B,	0x3FF060, /* 0x140 */
	0x7FEF00,	0x7FEF00,	0x4010E5,	0x000000, /* 0x144 */
	0x000000,	0x000000,	0x000001,	0x000003, /* 0x148 */
	0x000000,	0x000000,	0x000000,	0x000000, /* 0x14C */
	0x000000,	0x000000,	0x4CCCCD,	0x400000, /* 0x150 */
	0x7FFFFF,	0x5AE148,	0x266666,	0x266666, /* 0x154 */
	0x400000,	0x266666,	0x266666,	0x000003, /* 0x158 */
	0x000000,	0x000000,	0x000000,	0x000000, /* 0x15C */
	0x000000,	0x000002,	0x666666,	0x400000, /* 0x160 */
	0x333333,	0x600000,	0x0000FF,	0x00027F, /* 0x164 */
};

static unsigned int dae4p_read(struct snd_soc_codec *codec,
				      unsigned int reg)
{
	struct i2c_client *client = codec->control_data;
	static struct i2c_msg msg[2];
	static unsigned char reg_buf[4], data_buf[4];

	reg_buf[0] = ((reg >> 16) & 0xff);
	reg_buf[1] = ((reg >> 8) & 0xff);
	reg_buf[2] = ((reg) & 0xff);

	memset(data_buf, 0, 4);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = reg_buf;
	msg[0].len = 3;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data_buf;
	msg[1].len = 3;

	i2c_transfer(client->adapter, msg, 2);

	return (((unsigned int)data_buf[0] << 16) |((unsigned int)data_buf[1] << 8)| (data_buf[2]));
}

static int dae4p_write(struct snd_soc_codec *codec, unsigned int reg,
			      unsigned int value)
{
	struct i2c_client *client = codec->control_data;
	static struct i2c_msg msg;
	static unsigned char buf[6];

	buf[0] = ((reg >> 16) & 0xff);
	buf[1] = ((reg >> 8) & 0xff);
	buf[2] = ((reg) & 0xff);

	buf[3] = ((value >> 16) & 0xff);
	buf[4] = ((value >> 8) & 0xff);
	buf[5] = ((value) & 0xff);

	msg.addr = client->addr;
	msg.buf = buf;
	msg.len = 6;

	return i2c_transfer(client->adapter, &msg, 1);
}

static const struct snd_soc_dapm_widget dae4p_dapm_widgets[] = {
#if 0
	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
		&wm8750_left_mixer_controls[0],
		ARRAY_SIZE(wm8750_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
		&wm8750_right_mixer_controls[0],
		ARRAY_SIZE(wm8750_right_mixer_controls)),
	SND_SOC_DAPM_MIXER("Mono Mixer", WM8750_PWR2, 2, 0,
		&wm8750_mono_mixer_controls[0],
		ARRAY_SIZE(wm8750_mono_mixer_controls)),

	SND_SOC_DAPM_PGA("Right Out 2", WM8750_PWR2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", WM8750_PWR2, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 1", WM8750_PWR2, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 1", WM8750_PWR2, 6, 0, NULL, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", WM8750_PWR2, 7, 0),
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", WM8750_PWR2, 8, 0),

	SND_SOC_DAPM_MICBIAS("Mic Bias", WM8750_PWR1, 1, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", WM8750_PWR1, 2, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", WM8750_PWR1, 3, 0),

	SND_SOC_DAPM_MUX("Left PGA Mux", WM8750_PWR1, 5, 0,
		&wm8750_left_pga_controls),
	SND_SOC_DAPM_MUX("Right PGA Mux", WM8750_PWR1, 4, 0,
		&wm8750_right_pga_controls),
	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0,
		&wm8750_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0,
		&wm8750_right_line_controls),

	SND_SOC_DAPM_MUX("Out3 Mux", SND_SOC_NOPM, 0, 0, &wm8750_out3_controls),
	SND_SOC_DAPM_PGA("Out 3", WM8750_PWR2, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono Out 1", WM8750_PWR2, 2, 0, NULL, 0),

	SND_SOC_DAPM_MUX("Differential Mux", SND_SOC_NOPM, 0, 0,
		&wm8750_diffmux_controls),
	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0,
		&wm8750_monomux_controls),
	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0,
		&wm8750_monomux_controls),

	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_OUTPUT("MONO1"),
	SND_SOC_DAPM_OUTPUT("OUT3"),
	SND_SOC_DAPM_OUTPUT("VREF"),

	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("LINPUT2"),
	SND_SOC_DAPM_INPUT("LINPUT3"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT2"),
	SND_SOC_DAPM_INPUT("RINPUT3"),
#endif
};

static const struct snd_soc_dapm_route audio_map[] = {
#if 0
	/* left mixer */
	{"Left Mixer", "Playback Switch", "Left DAC"},
	{"Left Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Left Mixer", "Right Playback Switch", "Right DAC"},
	{"Left Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* right mixer */
	{"Right Mixer", "Left Playback Switch", "Left DAC"},
	{"Right Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Right Mixer", "Playback Switch", "Right DAC"},
	{"Right Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* left out 1 */
	{"Left Out 1", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},

	/* left out 2 */
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT2", NULL, "Left Out 2"},

	/* right out 1 */
	{"Right Out 1", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},

	/* right out 2 */
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT2", NULL, "Right Out 2"},

	/* mono mixer */
	{"Mono Mixer", "Left Playback Switch", "Left DAC"},
	{"Mono Mixer", "Left Bypass Switch", "Left Line Mux"},
	{"Mono Mixer", "Right Playback Switch", "Right DAC"},
	{"Mono Mixer", "Right Bypass Switch", "Right Line Mux"},

	/* mono out */
	{"Mono Out 1", NULL, "Mono Mixer"},
	{"MONO1", NULL, "Mono Out 1"},

	/* out 3 */
	{"Out3 Mux", "VREF", "VREF"},
	{"Out3 Mux", "ROUT1 + Vol", "ROUT1"},
	{"Out3 Mux", "ROUT1", "Right Mixer"},
	{"Out3 Mux", "MonoOut", "MONO1"},
	{"Out 3", NULL, "Out3 Mux"},
	{"OUT3", NULL, "Out 3"},

	/* Left Line Mux */
	{"Left Line Mux", "Line 1", "LINPUT1"},
	{"Left Line Mux", "Line 2", "LINPUT2"},
	{"Left Line Mux", "Line 3", "LINPUT3"},
	{"Left Line Mux", "PGA", "Left PGA Mux"},
	{"Left Line Mux", "Differential", "Differential Mux"},

	/* Right Line Mux */
	{"Right Line Mux", "Line 1", "RINPUT1"},
	{"Right Line Mux", "Line 2", "RINPUT2"},
	{"Right Line Mux", "Line 3", "RINPUT3"},
	{"Right Line Mux", "PGA", "Right PGA Mux"},
	{"Right Line Mux", "Differential", "Differential Mux"},

	/* Left PGA Mux */
	{"Left PGA Mux", "Line 1", "LINPUT1"},
	{"Left PGA Mux", "Line 2", "LINPUT2"},
	{"Left PGA Mux", "Line 3", "LINPUT3"},
	{"Left PGA Mux", "Differential", "Differential Mux"},

	/* Right PGA Mux */
	{"Right PGA Mux", "Line 1", "RINPUT1"},
	{"Right PGA Mux", "Line 2", "RINPUT2"},
	{"Right PGA Mux", "Line 3", "RINPUT3"},
	{"Right PGA Mux", "Differential", "Differential Mux"},

	/* Differential Mux */
	{"Differential Mux", "Line 1", "LINPUT1"},
	{"Differential Mux", "Line 1", "RINPUT1"},
	{"Differential Mux", "Line 2", "LINPUT2"},
	{"Differential Mux", "Line 2", "RINPUT2"},

	/* Left ADC Mux */
	{"Left ADC Mux", "Stereo", "Left PGA Mux"},
	{"Left ADC Mux", "Mono (Left)", "Left PGA Mux"},
	{"Left ADC Mux", "Digital Mono", "Left PGA Mux"},

	/* Right ADC Mux */
	{"Right ADC Mux", "Stereo", "Right PGA Mux"},
	{"Right ADC Mux", "Mono (Right)", "Right PGA Mux"},
	{"Right ADC Mux", "Digital Mono", "Right PGA Mux"},

	/* ADC */
	{"Left ADC", NULL, "Left ADC Mux"},
	{"Right ADC", NULL, "Right ADC Mux"},
#endif
};

static int dae4p_add_widgets(struct snd_soc_codec *codec)
{
	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	snd_soc_dapm_new_controls(codec, dae4p_dapm_widgets,
				  ARRAY_SIZE(dae4p_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	return 0;
}

/* codec private data */
struct dae4p_priv {
	struct snd_soc_codec codec;
	u32 reg_cache[ARRAY_SIZE(dae4p_reg)];
	u32 gpio_reset;
};

static const struct snd_kcontrol_new dae4p_snd_controls[] = {

SOC_SINGLE("Master Volume", DAE4P_MASTER_VOLUME, 0, 0x7FFFFF, 0),

/* others to be implemented */
};

#if 0
struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:5;
	u8 usb:1;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0x6, 0x0},
	{11289600, 8000, 1408, 0x16, 0x0},
	{18432000, 8000, 2304, 0x7, 0x0},
	{16934400, 8000, 2112, 0x17, 0x0},
	{12000000, 8000, 1500, 0x6, 0x1},

	/* 11.025k */
	{11289600, 11025, 1024, 0x18, 0x0},
	{16934400, 11025, 1536, 0x19, 0x0},
	{12000000, 11025, 1088, 0x19, 0x1},

	/* 16k */
	{12288000, 16000, 768, 0xa, 0x0},
	{18432000, 16000, 1152, 0xb, 0x0},
	{12000000, 16000, 750, 0xa, 0x1},

	/* 22.05k */
	{11289600, 22050, 512, 0x1a, 0x0},
	{16934400, 22050, 768, 0x1b, 0x0},
	{12000000, 22050, 544, 0x1b, 0x1},

	/* 32k */
	{12288000, 32000, 384, 0xc, 0x0},
	{18432000, 32000, 576, 0xd, 0x0},
	{12000000, 32000, 375, 0xa, 0x1},

	/* 44.1k */
	{11289600, 44100, 256, 0x10, 0x0},
	{16934400, 44100, 384, 0x11, 0x0},
	{12000000, 44100, 272, 0x11, 0x1},

	/* 48k */
	{12288000, 48000, 256, 0x0, 0x0},
	{18432000, 48000, 384, 0x1, 0x0},
	{12000000, 48000, 250, 0x0, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0x1e, 0x0},
	{16934400, 88200, 192, 0x1f, 0x0},
	{12000000, 88200, 136, 0x1f, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0xe, 0x0},
	{18432000, 96000, 192, 0xf, 0x0},
	{12000000, 96000, 125, 0xe, 0x1},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	printk(KERN_ERR "dae4p: could not get coeff for mclk %d @ rate %d\n",
		mclk, rate);
	return -EINVAL;
}
#endif

static int dae4p_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

#if 0
	struct snd_soc_codec *codec = codec_dai->codec;
	struct dae4p_priv *dae4p = snd_soc_codec_get_drvdata(codec);

	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		dae4p->sysclk = freq;
		return 0;
	}
#endif
	return -EINVAL;
}

static int dae4p_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

#if 0
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, WM8750_IFACE, iface);
#endif
	return 0;
}

static int dae4p_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

#if 0
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct dae4p_priv *dae4p = snd_soc_codec_get_drvdata(codec);
	u16 iface = snd_soc_read(codec, WM8750_IFACE) & 0x1f3;
	u16 srate = snd_soc_read(codec, WM8750_SRATE) & 0x1c0;
	int coeff = get_coeff(dae4p->sysclk, params_rate(params));

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x000c;
		break;
	}

	/* set iface & srate */
	snd_soc_write(codec, WM8750_IFACE, iface);
	if (coeff >= 0)
		snd_soc_write(codec, WM8750_SRATE, srate |
			(coeff_div[coeff].sr << 1) | coeff_div[coeff].usb);
#endif

	return 0;
}

#if OPTIONAL_IMPLEMENT
static int dae4p_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = snd_soc_read(codec, WM8750_ADCDAC) & 0xfff7;

	if (mute)
		snd_soc_write(codec, WM8750_ADCDAC, mute_reg | 0x8);
	else
		snd_soc_write(codec, WM8750_ADCDAC, mute_reg);

	return 0;
}

static int dae4p_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 pwr_reg = snd_soc_read(codec, WM8750_PWR1) & 0xfe3e;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* set vmid to 50k and unmute dac */
		snd_soc_write(codec, WM8750_PWR1, pwr_reg | 0x00c0);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->bias_level == SND_SOC_BIAS_OFF) {
			/* Set VMID to 5k */
			snd_soc_write(codec, WM8750_PWR1, pwr_reg | 0x01c1);

			/* ...and ramp */
			msleep(1000);
		}

		/* mute dac and set vmid to 500k, enable VREF */
		snd_soc_write(codec, WM8750_PWR1, pwr_reg | 0x0141);
		break;
	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, WM8750_PWR1, 0x0001);
		break;
	}
	codec->bias_level = level;

	return 0;
}
#endif

#define DAE4P_RATES (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
                     SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_64000 | \
                     SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
                     SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define DAE4P_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
                       SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE | \
                       SNDRV_PCM_FMTBIT_S24_BE)

static struct snd_soc_dai_ops dae4p_dai_ops = {
	.hw_params	= dae4p_pcm_hw_params,
#if OPTIONAL_IMPLEMENT
	.digital_mute	= dae4p_mute,
#endif
	.set_fmt	= dae4p_set_dai_fmt,
	.set_sysclk	= dae4p_set_dai_sysclk,
};

struct snd_soc_dai dae4p_dai = {
	.name = "DAE4P",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = DAE4P_RATES,
		.formats = DAE4P_FORMATS,},
	.ops = &dae4p_dai_ops,
};
EXPORT_SYMBOL_GPL(dae4p_dai);

#if OPTIONAL_IMPLEMENT
static int dae4p_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	dae4p_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int dae4p_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(dae4p_reg); i++) {
		if (i == WM8750_RESET)
			continue;
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->hw_write(codec->control_data, data, 2);
	}

	dae4p_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#endif

static struct snd_soc_codec *dae4p_codec;

static int dae4p_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	if (!dae4p_codec) {
		pr_err("%s: DAE4P codec not yet registered\n", __func__);
		return -EINVAL;
	}

	socdev->card->codec = dae4p_codec;
	codec = dae4p_codec;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		pr_err("%s: dae4p failed to create pcms\n", __func__);
		goto err;
	}

	snd_soc_add_controls(codec, dae4p_snd_controls,
				ARRAY_SIZE(dae4p_snd_controls));
#if OPTIONAL_IMPLEMENT
	dae4p_add_widgets(codec);
#endif

	return 0;

err:
	return ret;
}

/* power down chip */
static int dae4p_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

#if OPTIONAL_IMPLEMENT
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#endif

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_dae4p = {
	.probe		= dae4p_probe,
	.remove		= dae4p_remove,
#if OPTIONAL_IMPLEMENT
	.suspend	= dae4p_suspend,
	.resume		= dae4p_resume,
#endif
};
EXPORT_SYMBOL_GPL(soc_codec_dev_dae4p);

static int dae4p_reset(struct dae4p_priv *dae4p)
{
	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	gpio_direction_output(dae4p->gpio_reset, 1);
	mdelay(10);
	gpio_set_value(dae4p->gpio_reset, 0);
	mdelay(20);
	gpio_set_value(dae4p->gpio_reset, 1);
	mdelay(2000);

	return 0;
}

/*
 * initialise the DAE4P driver
 * register the mixer and dsp interfaces with the kernel
 */
static int dae4p_register(struct dae4p_priv *dae4p,
			enum snd_soc_control_type control)
{
	struct snd_soc_codec *codec = &dae4p->codec;
	int i, reg, ret = 0;

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	if (dae4p_codec) {
		pr_err("%s: multiple DAE4P devices not supported\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	codec->name = "DAE4P";
	codec->owner = THIS_MODULE;
	codec->read = dae4p_read;
	codec->write = dae4p_write;
#if OPTIONAL_IMPLEMENT
	codec->bias_level = SND_SOC_BIAS_STANDBY;
	codec->set_bias_level = dae4p_set_bias_level;
#endif
	codec->dai = &dae4p_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ARRAY_SIZE(dae4p->reg_cache) + 1;
	codec->reg_cache = &dae4p->reg_cache;
	snd_soc_codec_set_drvdata(codec, dae4p);

	memcpy(codec->reg_cache, dae4p_reg, sizeof(dae4p->reg_cache));

	/* reset */
	ret = dae4p_reset(dae4p);
	if (ret < 0) {
		pr_err("%s: dae4p: failed to reset: %d\n", __func__, ret);
		goto err;
	}

	/* load default value */
	for (i = 0; i < DAE4P_CACHE_REGNUM; i++) {
		if (dae4p->reg_cache[i] == RESERVED_REG)
			continue;

		ret = snd_soc_write(codec, i, dae4p->reg_cache[i]);
		if (ret < 0) {
			pr_err("%s: dae4p failed to set cache I/O: %d\n", __func__, ret);
			goto err;
		}
		udelay(30); /* must ??? */
	}
	snd_soc_write(codec, 0x800000, 0);
	mdelay(2000);
	snd_soc_write(codec, 0x020001, 0xC0000E); /* I2S, 0xC0000E -> S/PDIF */
	snd_soc_write(codec, 0, 0x2CCCC);

	/* charge output caps */

	/* set the update bits */

	dae4p_codec = codec;
	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		pr_err("%s: failed to register codec: %d\n", __func__, ret);
		goto err;
	}

	dae4p_dai.dev = codec->dev;
	ret = snd_soc_register_dais(&dae4p_dai, 1);
	if (ret != 0) {
		pr_err("%s: failed to register DAIs: %d\n", __func__, ret);
		goto err_codec;
	}

	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(dae4p);
	return ret;
}

static void dae4p_unregister(struct dae4p_priv *dae4p)
{
#if OPTIONAL_IMPLEMENT
	dae4p_set_bias_level(&dae4p->codec, SND_SOC_BIAS_OFF);
#endif

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	snd_soc_unregister_dais(&dae4p_dai, 1);
	snd_soc_unregister_codec(&dae4p->codec);
	kfree(dae4p);
	dae4p_codec = NULL;
}

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int dae4p_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_codec *codec;
	struct dae4p_priv *dae4p;

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	dae4p = kzalloc(sizeof(struct dae4p_priv), GFP_KERNEL);
	if (dae4p == NULL) {
		pr_err("%s: can't allocate memory %d for DAE4P!\n", __func__);
		return -ENOMEM;
	}

	dae4p->gpio_reset = dev_get_platdata(&i2c->dev);
	if (gpio_request(dae4p->gpio_reset, "DAE4P Reset")) {
		pr_err("%s: can't reserver GPIO(%d) for reset!\n", __func__, \
		        dae4p->gpio_reset);
		kfree(dae4p);
		return -ENXIO;
	}

	codec = &dae4p->codec;
	codec->control_data = i2c;
	i2c_set_clientdata(i2c, dae4p);
	codec->dev = &i2c->dev;

	return dae4p_register(dae4p, SND_SOC_I2C);
}

static int dae4p_i2c_remove(struct i2c_client *client)
{
	struct dae4p_priv *dae4p = i2c_get_clientdata(client);

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

	dae4p_unregister(dae4p);

	return 0;
}

static const struct i2c_device_id dae4p_i2c_id[] = {
	{ "dae4p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dae4p_i2c_id);

static struct i2c_driver dae4p_i2c_driver = {
	.driver = {
		.name = "dae4p",
		.owner = THIS_MODULE,
	},
	.probe =    dae4p_i2c_probe,
	.remove =   dae4p_i2c_remove,
	.id_table = dae4p_i2c_id,
};
#endif

static int __init dae4p_modinit(void)
{
	int ret;

	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&dae4p_i2c_driver);
	if (ret != 0)
		pr_err("%s: failed to register DAE4P I2C driver: %d\n", __func__, ret);
#endif
	return ret;
}
module_init(dae4p_modinit);

static void __exit dae4p_exit(void)
{
	if (dae4p_debug & DAE4P_TRACE)
		printk("%s\n", __func__);

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&dae4p_i2c_driver);
#endif
}
module_exit(dae4p_exit);

module_param_named(debug, dae4p_debug, uint, 0644);
MODULE_PARM_DESC(debug, "dae4p module debug flag\n");

MODULE_DESCRIPTION("ASoC DAE4P driver");
MODULE_LICENSE("GPL");

