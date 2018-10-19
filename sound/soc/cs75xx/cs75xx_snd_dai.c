/*
 * File: sound/soc/cs75xx/cs75xx_sound_dai.c
 * Description: ASoC platform driver for CS75XX - I2S interface
 *
 * Copyright (c) Cortina-Systems Limited 2012. All rights reserved.
 *
 * Mostly copied from bf5xx-i2s.c and some from davinci-mcasp.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "cs75xx_snd_sport.h"
#include "cs75xx_snd_dai.h"


cs75xx_dai_port_t dai_port[CS75XX_SSP_NUM];


/*
 * DAIs are able to configure to PCM, I2S, SPDIF(only DAI 0). ASoC core doesn't
 * provide the API to do this. So we provide a propreitary API to support this.
 */
int cs75xx_snd_dai_set_port(struct snd_soc_dai *cpu_dai, int port, int type)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);

	if (port >= CS75XX_SSP_NUM)
		return -EINVAL;

	switch (type) {
	case DAI_TYPE_SPDIF:
		if (port != 0)
			return -EINVAL;
	case DAI_TYPE_PCM:
	case DAI_TYPE_I2S:
		break;
	default:
		pr_err("%s: not support type %d\n", __func__, type);
		return -EINVAL;
	}

	dai_port[port].port = port;
	dai_port[port].type = type;

	cpu_dai->private_data = &dai_port[port];

	return 0;
}
EXPORT_SYMBOL(cs75xx_snd_dai_set_port);

static int cs75xx_snd_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_set_pll(struct snd_soc_dai *cpu_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_set_clkdiv(struct snd_soc_dai *cpu_dai,
		int div_id, int div)
{
	int ret = 0;

	if (sport_debug & (DAI_TRACE | DAI_INFO))
		printk("%s: id = %d, div_id = %d, div = %d\n", __func__, cpu_dai->id, div_id, div);

	ret = cs75xx_snd_sport_clkdiv(cpu_dai->id, div_id, div);
	if (ret)
		pr_err("%s unsupported dai clkdiv - %d\n", cpu_dai->name, div_id);

	return 0;
}

static int cs75xx_snd_dai_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	unsigned int signal_fmt = 0;
	int ret = 0;

	if (sport_debug & (DAI_TRACE | DAI_INFO))
		printk("%s: fmt 0x%x\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		signal_fmt |= SPORT_DATA_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		signal_fmt |= SPORT_DATA_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		signal_fmt |= SPORT_DATA_LEFT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		signal_fmt |= SPORT_DATA_DSP_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		signal_fmt |= SPORT_DATA_DSP_B;
		break;
	case SND_SOC_DAIFMT_AC97:
		signal_fmt |= SPORT_DATA_AC97;
		break;
	case SND_SOC_DAIFMT_PDM:
		signal_fmt |= SPORT_DATA_PDM;
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_CLOCK_MASK) {
	case SND_SOC_DAIFMT_CONT:
		signal_fmt |= SPORT_CLOCK_CONT;
		break;
	case SND_SOC_DAIFMT_GATED:
		signal_fmt |= SPORT_CLOCK_GATED;
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		signal_fmt |= SPORT_SIGNAL_NB_NF;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		signal_fmt |= SPORT_SIGNAL_NB_IF;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		signal_fmt |= SPORT_SIGNAL_IB_NF;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		signal_fmt |= SPORT_SIGNAL_IB_IF;
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		signal_fmt |= SPORT_CLOCK_CBM_CFM;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		signal_fmt |= SPORT_CLOCK_CBS_CFS;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		signal_fmt |= SPORT_CLOCK_CBM_CFS;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		signal_fmt |= SPORT_CLOCK_CBS_CFM;
		break;
	}

	ret = cs75xx_snd_sport_signal_formt(cpu_dai->id, signal_fmt);
	if (ret)
		pr_err("%s: unsupported dai format 0x%x\n", cpu_dai->name, fmt);

	return ret;
}

static int cs75xx_snd_dai_set_tdm_slot(struct snd_soc_dai *cpu_dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width)
{
	int ret = 0;

	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);

	ret = cs75xx_snd_sport_slots(cpu_dai->id, tx_mask, rx_mask, slots, slot_width);
	if (ret)
		pr_err("%s not support tdm slots %d, width %d\n", cpu_dai->name, slots, slot_width);

	return ret;
}

static int cs75xx_snd_dai_set_channel_map(struct snd_soc_dai *cpu_dai,
		unsigned int tx_num, unsigned int *tx_slot,
		unsigned int rx_num, unsigned int *rx_slot)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_set_dai_tristate(struct snd_soc_dai *cpu_dai,
	int tristate)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_digital_mute(struct snd_soc_dai *cpu_dai, int mute)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	int dir, size, endian = 0;
	int ret = 0;

	if (sport_debug & (DAI_TRACE | DAI_INFO))
		printk("%s: params %d\n", __func__, params_format(params));

	size = snd_pcm_format_size(params_format(params), 1);
	endian = snd_pcm_format_big_endian(params_format(params));
	dir = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? DIR_TX : DIR_RX;

	if (sport_debug & DAI_INFO)
		printk("%s: id %d, dir %d, size %d, endian %d\n", __func__, \
		        cpu_dai->id, dir, size, endian);

	ret = cs75xx_snd_sport_audio_formt(cpu_dai->id, dir, size, endian);
	if (ret)
		pr_err("%s unsupported dai format 0x%x\n", cpu_dai->name,
							params_format(params));

	return ret;
}

static int cs75xx_snd_dai_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static snd_pcm_sframes_t cs75xx_snd_dai_delay(
			struct snd_pcm_substream *substream,
			struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_sframes_t total, frames;
	unsigned int diff;

	if (sport_debug & DAI_TRACE)
		pr_debug("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		diff = cs75xx_snd_sport_dma_curr_offset(cpu_dai->id, DIR_TX);
		frames = bytes_to_frames(substream->runtime, diff);
		total = runtime->periods * runtime->period_size;
		return (runtime->control->appl_ptr + total - frames) % total;
	} else {
		;/* TBD */
	}

	return 0;
}

static int cs75xx_snd_dai_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		pr_debug("%s\n", __func__);
	return 0;
}

static void cs75xx_snd_dai_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		pr_debug("%s\n", __func__);
}



/* 
 * For Kernel 3.3.6
 * Author:Rudolph
 */
static int cs75xx_snd_dai_probe(struct snd_soc_dai *cpu_dai)
{
	cs75xx_dai_port_t *dai_port = (cs75xx_dai_port_t *)cpu_dai->private_data;
	int type, tx, rx;

	if (dai_port == NULL) {
		pr_err("%s: dai_port is NULL\n", __func__);
		return -ENODEV;
	}

	switch (dai_port->type) {
	case DAI_TYPE_PCM:
		type = CS75XX_SPORT_PCM;
		break;
	case DAI_TYPE_I2S:
		type = CS75XX_SPORT_I2S;
		break;
	case DAI_TYPE_SPDIF:
		type = CS75XX_SPORT_SPDIF;
		break;
	default:
		return -EPERM;
	}

	tx = cpu_dai->driver->playback.channels_max ? 1 : 0;
	rx = cpu_dai->driver->capture.channels_max ? 1 : 0;

	if (cs75xx_snd_sport_if_register(cpu_dai->driver->id, type, tx, rx))
		return -EPERM;

	return 0;
}


/*
static int cs75xx_snd_dai_probe(struct platform_device *pdev,
			   struct snd_soc_dai *cpu_dai)
{
	cs75xx_dai_port_t *dai_port = (cs75xx_dai_port_t *)cpu_dai->private_data;
	int type, tx, rx;

	if (dai_port == NULL) {
		pr_err("%s: dai_port is NULL\n", __func__);
		return -ENODEV;
	}

	switch (dai_port->type) {
	case DAI_TYPE_PCM:
		type = CS75XX_SPORT_PCM;
		break;
	case DAI_TYPE_I2S:
		type = CS75XX_SPORT_I2S;
		break;
	case DAI_TYPE_SPDIF:
		type = CS75XX_SPORT_SPDIF;
		break;
	default:
		return -EPERM;
	}

	tx = cpu_dai->playback.channels_max ? 1 : 0;
	rx = cpu_dai->capture.channels_max ? 1 : 0;

	if (cs75xx_snd_sport_if_register(cpu_dai->id, type, tx, rx))
		return -EPERM;

	return 0;
}
*/

/*
 * For Kernel3.3.8
 * Author:Rudolph
 */
static void cs75xx_snd_dai_remove(struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);

	cs75xx_snd_sport_if_unregister(cpu_dai->driver->id);
}

/*
static void cs75xx_snd_dai_remove(struct platform_device *pdev,
			struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);

	cs75xx_snd_sport_if_unregister(cpu_dai->id);
}
*/

#ifdef CONFIG_PM
static int cs75xx_snd_dai_suspend(struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

static int cs75xx_snd_dai_resume(struct snd_soc_dai *cpu_dai)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	return 0;
}

#else
#define cs75xx_snd_dai_suspend	NULL
#define cs75xx_snd_dai_resume	NULL
#endif


static struct snd_soc_dai_ops cs75xx_snd_dai_ops = {
	/* clocking configuration - all optional */
	//.set_sysclk	= cs75xx_snd_dai_set_sysclk,
	//.set_pll	= cs75xx_snd_dai_set_pll,
	.set_clkdiv	= cs75xx_snd_dai_set_clkdiv,

	/* format configuration */
	.set_fmt	= cs75xx_snd_dai_set_dai_fmt,
	.set_tdm_slot	= cs75xx_snd_dai_set_tdm_slot,
	.set_channel_map = cs75xx_snd_dai_set_channel_map,
	.set_tristate	= cs75xx_snd_dai_set_dai_tristate,

	/* digital mute - optional */
	.digital_mute = cs75xx_snd_dai_digital_mute,

	/* audio operations - all optional */
	.startup	= cs75xx_snd_dai_startup,
	.shutdown	= cs75xx_snd_dai_shutdown,
	.hw_params	= cs75xx_snd_dai_hw_params,
	.hw_free	= cs75xx_snd_dai_hw_free,
	.prepare	= cs75xx_snd_dai_prepare,
	.trigger	= cs75xx_snd_dai_trigger,

	/* delay reporting - optional */
	.delay		= cs75xx_snd_dai_delay,
};


/*
 * For 3.3.6 sound prototype.
 * Author: Rudolph
 */
struct snd_soc_dai_driver cs75xx_snd_dai[] = {
	[DAI_TYPE_PCM] = {
		.name = "cs75xx-pcm",
		.id = 0,
		.probe = cs75xx_snd_dai_probe,
		.remove = cs75xx_snd_dai_remove,
		.suspend = cs75xx_snd_dai_suspend,
		.resume = cs75xx_snd_dai_resume,
		.playback = {
			.channels_min = CS75XX_PCM_CHANNEL_MIN,
			.channels_max = CS75XX_PCM_CHANNEL_MIN,
			.rates = CS75XX_PCM_RATES,
			.formats = CS75XX_PCM_FORMATS,},
		.capture = {
			.channels_min = CS75XX_PCM_CHANNEL_MIN,
			.channels_max = CS75XX_PCM_CHANNEL_MIN,
			.rates = CS75XX_PCM_RATES,
			.formats = CS75XX_PCM_FORMATS},
	},
	[DAI_TYPE_I2S] = {
		.name = "cs75xx-i2s",
		.id = 0,
		.probe = cs75xx_snd_dai_probe,
		.remove = cs75xx_snd_dai_remove,
		.suspend = cs75xx_snd_dai_suspend,
		.resume = cs75xx_snd_dai_resume,
		.playback = {
			.channels_min = CS75XX_I2S_CHANNEL_MIN,
			.channels_max = CS75XX_I2S_CHANNEL_MAX,
			.rates = CS75XX_I2S_RATES,
			.formats = CS75XX_I2S_FORMATS,},
		.ops = &cs75xx_snd_dai_ops,
	},
	[DAI_TYPE_SPDIF] = {
		.name = "cs75xx-spdif",
		.id = 0,
		.probe = cs75xx_snd_dai_probe,
		.remove = cs75xx_snd_dai_remove,
		.suspend = cs75xx_snd_dai_suspend,
		.resume = cs75xx_snd_dai_resume,
		.playback = {
			.channels_min = CS75XX_SPDIF_CHANNEL_MIN,
			.channels_max = CS75XX_SPDIF_CHANNEL_MAX,
			.rates = CS75XX_SPDIF_RATES,
			.formats = CS75XX_SPDIF_FORMATS,},
		.ops = &cs75xx_snd_dai_ops,		
	},
};
EXPORT_SYMBOL_GPL(cs75xx_snd_dai);

/*
struct snd_soc_dai cs75xx_snd_dai[] = {
	[DAI_TYPE_PCM] = {
		.name = "cs75xx-pcm",
		.id = 0,
		.probe = cs75xx_snd_dai_probe,
		.remove = cs75xx_snd_dai_remove,
		.suspend = cs75xx_snd_dai_suspend,
		.resume = cs75xx_snd_dai_resume,
		.playback = {
			.channels_min = CS75XX_PCM_CHANNEL_MIN,
			.channels_max = CS75XX_PCM_CHANNEL_MIN,
			.rates = CS75XX_PCM_RATES,
			.formats = CS75XX_PCM_FORMATS
		},
		.capture = {
			.channels_min = CS75XX_PCM_CHANNEL_MIN,
			.channels_max = CS75XX_PCM_CHANNEL_MIN,
			.rates = CS75XX_PCM_RATES,
			.formats = CS75XX_PCM_FORMATS
		},
	},
	[DAI_TYPE_I2S] = {
		.name = "cs75xx-i2s",
		.id = 0,
		.probe = cs75xx_snd_dai_probe,
		.remove = cs75xx_snd_dai_remove,
		.suspend = cs75xx_snd_dai_suspend,
		.resume = cs75xx_snd_dai_resume,
		.playback = {
			.channels_min = CS75XX_I2S_CHANNEL_MIN,
			.channels_max = CS75XX_I2S_CHANNEL_MAX,
			.rates = CS75XX_I2S_RATES,
			.formats = CS75XX_I2S_FORMATS,
		},
		.ops = &cs75xx_snd_dai_ops,
	},
	[DAI_TYPE_SPDIF] = {
		.name = "cs75xx-spdif",
		.id = 0,
		.probe = cs75xx_snd_dai_probe,
		.remove = cs75xx_snd_dai_remove,
		.suspend = cs75xx_snd_dai_suspend,
		.resume = cs75xx_snd_dai_resume,
		.playback = {
			.channels_min = CS75XX_SPDIF_CHANNEL_MIN,
			.channels_max = CS75XX_SPDIF_CHANNEL_MAX,
			.rates = CS75XX_SPDIF_RATES,
			.formats = CS75XX_SPDIF_FORMATS,
		},
		.ops = &cs75xx_snd_dai_ops,
	},
};
EXPORT_SYMBOL_GPL(cs75xx_snd_dai);
*/

int cs75xx_snd_dai_init(void)
{
	int i, ret;
	struct snd_soc_dai *dev;

	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);
	
	dev = cs75xx_snd_sport_device();
	
	/*
         * For Kernel 3.3.8
	 * Author: Rudolph  
         */	
	return snd_soc_register_dais(dev, cs75xx_snd_dai, ARRAY_SIZE(cs75xx_snd_dai));
/*
	for (i = 0; i < ARRAY_SIZE(cs75xx_snd_dai); i++)
		cs75xx_snd_dai[i].dev = cs75xx_snd_sport_device();

	return snd_soc_register_dais(cs75xx_snd_dai, ARRAY_SIZE(cs75xx_snd_dai));
*/
	
}
EXPORT_SYMBOL(cs75xx_snd_dai_init);

void cs75xx_snd_dai_exit(void)
{
	if (sport_debug & DAI_TRACE)
		printk("%s\n", __func__);

	snd_soc_unregister_dais(cs75xx_snd_dai, ARRAY_SIZE(cs75xx_snd_dai));
}
EXPORT_SYMBOL(cs75xx_snd_dai_exit);

