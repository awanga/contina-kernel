/*
 * File: sound/soc/cs75xx/cs75xx_snd_evm.c
 * Description: ASoC machine driver for CS75XX using DAE4P codec
 *
 * Copyright (c) Cortina Systems Limited 2012. All rights reserved.
 *
 * Mostly copied from bf5xx-ssm2602.c and some from davinci-evm.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>

#include <linux/gpio.h>
#include "../codecs/dae4p.h"
// For Kernel3.3.6 Rudolph
//#include "../codecs/spdif_transciever.h"
#include "cs75xx_snd_plat.h"
#include "cs75xx_snd_dai.h"
#include "cs75xx_snd_sport.h"

#define EVM_TRACE	BIT(0)
#define EVM_INFO	BIT(1)

static struct snd_soc_card cs75xx_evm;
static unsigned int evm_debug = 0;//EVM_TRACE | EVM_INFO;
#ifdef CONFIG_SND_SOC_DAE4P
static int select = DAI_TYPE_I2S;
#else
static int select = DAI_TYPE_SPDIF;
#endif


static int cs75xx_evm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	
	//For Kernel3.3.8 Author:Rudolph
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	//struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	//struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int tx_mask, rx_mask;
	unsigned int audio_fmt = 0;
	int ret = 0;
	int i, size, slots, width, chans, rate;

	if (evm_debug & (EVM_TRACE | EVM_INFO))
		printk("%s: rate %d format 0x%x\n", __func__, params_rate(params), \
		        params_format(params));

	if (cpu_dai == (&cs75xx_snd_dai[DAI_TYPE_PCM])) {
		audio_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_GATED | \
		            SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS;
	} else if (cpu_dai == (&cs75xx_snd_dai[DAI_TYPE_I2S])) {
		audio_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_GATED | \
		            SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS;
	} else if (cpu_dai == (&cs75xx_snd_dai[DAI_TYPE_SPDIF])) {
		goto evm_spdif;
	}

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, audio_fmt);
	if (ret < 0) {
		pr_err("%s not support audio format 0x%x\n", codec_dai->name, audio_fmt);
		return ret;
	}

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, audio_fmt);
	if (ret < 0) {
		pr_err("%s not support audio format 0x%x\n", codec_dai->name, audio_fmt);
		return ret;
	}

	size = snd_pcm_format_size(params_format(params), 1);
	tx_mask = rx_mask = 0;
	if (cpu_dai == (&cs75xx_snd_dai[DAI_TYPE_PCM])) {
		slots = 16;	/* For Zarlink SLIC limitation, PCLK >= 1024K */
		width = 8;

		/* After SSP recovered from RX overrun situation, if the final slot
		   of frame is not valid, the RX data of final slot of pervious
		   frame will be pushed into RX FIFO with current write pointer.
		   But the final slot is invalid so that this slot should be discarded.
		   And this will make RX data one byte shifted. If the final slot
		   of frame is valid, the RX data of final slot of previous frame
		   will be pushed into RX FIFO with current write pointer minus
		   one. The first slot of current frame will be pushed into RX FIFO
		   with current write pointer. The final slot of frame should be
		   set valid prevent from RX overrun causing one byte shifted.
		 */
		
		//For Kernel3.3.8 Author:Rudolph
		chans = cpu_dai->driver->playback.channels_max;
		//chans = cpu_dai->playback.channels_max;

		for (i = 0; i < chans; i++) {
			tx_mask |= BIT(slots - (chans-i)*4 + (4-size));
			if (size == 2)
				tx_mask |= BIT(slots - (chans-i)*4 + (4-size) + 1);
		}
		rx_mask = tx_mask;
	} else if (cpu_dai == (&cs75xx_snd_dai[DAI_TYPE_I2S])) {
		slots = 8;	/* 2 channels, up to 4 bytes sampling */
		width = 8;
		switch (size) {
		case 1:
			tx_mask = 0x00000011;
			break;
		case 2:
			tx_mask = 0x00000033;
			break;
		case 4:
			tx_mask = 0x000000FF;
			break;
		}
	} else {
		return -ENODEV;
	}
	ret = snd_soc_dai_set_tdm_slot(cpu_dai, tx_mask, rx_mask, slots, width);
	if (ret < 0) {
		pr_err("%s not support tdm slots %d, width %d\n", cpu_dai->name, slots, width);
		return ret;
	}

evm_spdif:
	/* set the codec system clock */
	rate = params_rate(params);
	if (cpu_dai == (&cs75xx_snd_dai[DAI_TYPE_SPDIF]))
		ret = snd_soc_dai_set_clkdiv(cpu_dai, SPORT_SPDIF_CLK_DIV, rate*256);
	else
		ret = snd_soc_dai_set_clkdiv(cpu_dai, SPORT_SPDIF_CLK_DIV, rate*slots*width);
	if (ret < 0) {
		pr_err("%s not support clkdiv rate %d, slots %d, width %d\n", \
			cpu_dai->name, rate, slots, width);
		return ret;
	}

	return 0;
}

static struct snd_soc_ops cs75xx_evm_ops = {
	//.startup = cs75xx_evm_startup,
	.hw_params = cs75xx_evm_hw_params,
};

#ifdef CONFIG_SND_SOC_DAE4P
static struct snd_soc_dai_link cs75xx_evm_dae4p_dai_link = {
	.name = "DAE4P",
	.stream_name = "DAE4P I2S",
	.cpu_dai = &cs75xx_snd_dai[DAI_TYPE_I2S],
	.codec_dai = &dae4p_dai,
	.ops = &cs75xx_evm_ops,
};

static struct snd_soc_card cs75xx_evm_dae4p = {
	.name = "CS75XX_EVM_DAE4P",
	.platform = &cs75xx_snd_platform,
	.dai_link = &cs75xx_evm_dae4p_dai_link,
	.num_links = 1,
};

static struct snd_soc_device cs75xx_evm_dae4p_snd_devdata = {
	.card = &cs75xx_evm_dae4p,
	.codec_dev = &soc_codec_dev_dae4p,
};
#endif

static int cs75xx_spdif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int rate;

	if (evm_debug & (EVM_TRACE | EVM_INFO))
		printk("%s: rate %d format %x\n", __func__, params_rate(params), \
		        params_format(params));

	// set audio source clock rates 
	rate = params_rate(params);
	// ckeck rate 

	// set the codec system clock 
	ret = snd_soc_dai_set_clkdiv(cpu_dai, SPORT_SPDIF_CLK_DIV, rate * 256);
	if (ret < 0) {
		pr_err("%s: snd_soc_dai_set_clkdiv() fail\n", __func__);
		return ret;
	}

	return 0;
}

/*
static int cs75xx_spdif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
//	unsigned int clk = 0;
	int ret = 0;
	int rate;

	if (evm_debug & (EVM_TRACE | EVM_INFO))
		printk("%s: rate %d format %x\n", __func__, params_rate(params), \
		        params_format(params));

	// set audio source clock rates 
	rate = params_rate(params);
	// ckeck rate 

	// set the codec system clock 
	ret = snd_soc_dai_set_clkdiv(cpu_dai, SPORT_SPDIF_CLK_DIV, rate * 256);
	if (ret < 0) {
		pr_err("%s: snd_soc_dai_set_clkdiv() fail\n", __func__);
		return ret;
	}

	return 0;
}
*/

#ifdef CONFIG_SND_SOC_SPDIF
static struct snd_soc_dai_link cs75xx_evm_spdif_dai_link = {
	.name = "CS75XX_SPDIF",
	.stream_name = "SPDIF",
	.ops = &cs75xx_evm_ops,
};

static struct snd_soc_card cs75xx_evm_spdif = {
	.name = "CS75XX_EVM_SPDIF",
	.dai_link = &cs75xx_evm_spdif_dai_link,
	.num_links = 1,
};

/*
static struct snd_soc_device cs75xx_evm_spdif_snd_devdata = {
	.card = &cs75xx_evm_spdif,
	.codec_dev = &soc_codec_dev_spdif_dit,
};

static struct snd_soc_dai_link cs75xx_evm_spdif_dai_link = {
	.name = "CS75XX_SPDIF",
	.stream_name = "SPDIF",
	.cpu_dai = &cs75xx_snd_dai[DAI_TYPE_SPDIF],
	.codec_dai = &dit_stub_dai,
	.ops = &cs75xx_evm_ops,
};

static struct snd_soc_card cs75xx_evm_spdif = {
	.name = "CS75XX_EVM_SPDIF",
	.platform = &cs75xx_snd_platform,
	.dai_link = &cs75xx_evm_spdif_dai_link,
	.num_links = 1,
};

static struct snd_soc_device cs75xx_evm_spdif_snd_devdata = {
	.card = &cs75xx_evm_spdif,
	.codec_dev = &soc_codec_dev_spdif_dit,
};
*/
#endif

static struct platform_device *cs75xx_evm_snd_device;

int __init cs75xx_snd_evm_init(void)
{
	int ret;

	if (evm_debug & (EVM_TRACE | EVM_INFO))
		printk("%s: select %d\n", __func__, select);

	cs75xx_evm_snd_device = platform_device_alloc("soc-audio", -1);
	if (!cs75xx_evm_snd_device)
		return -ENOMEM;

	switch (select) {
	case DAI_TYPE_PCM:
		printk("%s: DAI_TYPE_PCM not support noew!\n", __func__);
		return -ENODEV;
#ifdef CONFIG_SND_CS75XX_SOC_EVM_DAE4P
	case DAI_TYPE_I2S:	/* I2S - DAE4P */
		platform_set_drvdata(cs75xx_evm_snd_device,
					&cs75xx_evm_dae4p_snd_devdata);
		cs75xx_evm_dae4p_snd_devdata.dev = &cs75xx_evm_snd_device->dev;

		if (cs75xx_snd_dai_set_port(cs75xx_evm_dae4p_dai_link.cpu_dai, 0, DAI_TYPE_I2S)) {
			pr_err("%s: dai_set_port(port %d, type %d) fail\n", __func__, 0, DAI_TYPE_I2S);
			return -ENODEV;
		}
		break;
#endif
#ifdef CONFIG_SND_CS75XX_SOC_EVM_SPDIF
	case DAI_TYPE_SPDIF:	/* SPDIF */
		/*
 		 * For Kernel3.3.8
		platform_set_drvdata(cs75xx_evm_snd_device,
					&cs75xx_evm_spdif_snd_devdata);
		cs75xx_evm_spdif_snd_devdata.dev = &cs75xx_evm_snd_device->dev;
		
		if (cs75xx_snd_dai_set_port(cs75xx_evm_spdif_dai_link.cpu_dai, 0, DAI_TYPE_SPDIF)) {
			pr_err("%s: dai_set_port(port %d, type %d) fail\n", __func__, 0, DAI_TYPE_SPDIF);
			return -ENODEV;
		}
		*/
		break;
#endif
	default:
		return -ENODEV;
	}

	ret = platform_device_add(cs75xx_evm_snd_device);
	if (ret) {
		pr_err("%s: platform_device_add() fail\n", __func__);
		platform_device_put(cs75xx_evm_snd_device);
	}

	return ret;
}

void __exit cs75xx_snd_evm_exit(void)
{
	if (evm_debug & EVM_TRACE)
		printk("%s\n", __func__);

	platform_device_unregister(cs75xx_evm_snd_device);
}
module_init(cs75xx_snd_evm_init);
module_exit(cs75xx_snd_evm_exit);

module_param(select, int, 0644);
MODULE_PARM_DESC(select, "evm select\n" \
	"\t1 - I2S DAE4P\n" \
	"\t2 - S/PDIF\n");

module_param_named(debug, evm_debug, uint, 0644);
MODULE_PARM_DESC(debug, "evm module debug flag\n");

MODULE_DESCRIPTION("ALSA SoC CS75XX EVM driver");
MODULE_LICENSE("GPL");

