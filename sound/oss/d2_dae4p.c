/*
* 	sound/oss/d2_dae4p.c

* Storm audio driver
*
* Copyright (c) 2000 Middle Huang
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License.
*
* History:
*

*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/io.h>	/** for i2c **/
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <mach/cs75xx_ssp.h>

#include "d2_dae4p.h"
#include "dac.h"

#

#define PFX "Cortina: "
#

#define DEBUG
#ifdef DEBUG
# define DPRINTK printk
#else
# define DPRINTK(x,...)
#endif

#define SSP_DF_16BITB_LINEAR	0
#define SSP_DF_8BIT_ULAW	1
#define SSP_DF_8BIT_ALAW	2
#define SSP_DF_8BIT_LINEAR	3
#define SSP_DF_16BITL_LINEAR	4
#define SSP_DF_16BITUB_LINEAR	5
#define SSP_DF_16BITUL_LINEAR	6
#define SSP_DF_24BITUB_LINEAR	7
#define SSP_DF_24BITUL_LINEAR	8

#define SSP_SS_MONO		0
#define SSP_SS_STEREO		1

#define DMA_PRE_TX_BUF		8
#define DMA_TIME_OUT		(5*HZ)

/*
 * Some magics numbers used to auto-detect file formats
 */

#define SSP_MAGIC_8B_ULAW	1
#define SSP_MAGIC_8B_ALAW	27
#define SSP_MAGIC_16B_LINEAR 	3
#define SSP_MAGIC_MONO		1
#define SSP_MAGIC_STEREO	2


static int audio_dev_id = -1, mixer_dev_id = -1;

#define I2S_CPU	0
#define I2S_DMA 1


typedef struct {
	unsigned int tol_size, wt_size, rm_size;
	unsigned int wt_curr, tx_curr, tx_start;
	unsigned int in_curr, in_cont, in_bufok;
	char *tbuf, *tbuf_curr, *tbuf_end;
	unsigned char *in_ptr;
	unsigned int wait_write_len;
	unsigned int file_len;
	unsigned int out_len;
	u32 current_gain;
	u32 dac_rate;		/* 8000 ... 48000 (Hz) */
	u8 data_format;		/* HARMONY_DF_xx_BIT_xxx */
	u8 stereo_select;	/* HARMONY_SS_MONO or HARMONY_SS_STEREO */
	u16 sample_rate;	/* HARMONY_SR_xx_KHZ */
	u32 level;
} SSP_I2S;

static cs75xx_ssp_ctrl_t ssp_ctrl;
static SSP_I2S ssp_i2s;

static DECLARE_WAIT_QUEUE_HEAD(ssp_wait_q);
static int buf_gap;
unsigned long flags;
static spinlock_t ssp_lock = SPIN_LOCK_UNLOCKED;
static int dac_wait_en = 0;
static unsigned int halt = 0;
static unsigned int debug = 0;
#ifdef CONFIG_SOUND_D2_45057_SPDIF
static unsigned int source = 1;
#else
static unsigned int source = 0;
#endif

#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
extern int cs75xx_spdif_clock_start(unsigned int sample_rate, int ext_out, unsigned int clk_target);
#endif

struct i2c_client *d2_i2c_client;
static int d2_dae4p_reg_i2c_write(struct i2c_client *client, u32 reg, u32 value)
{
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

static int d2_dae4p_reg_i2c_read(struct i2c_client *client, u32 reg)
{
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

static int d2_dae4p_hw_init(struct i2c_client *client)
{
	int i, rc;

	/* reset */
	if (gpio_request(GPIO2_BIT23, "d2-dae4p_reset")) {
		printk("Can't reserver GPIO(%d) for d2-dae4p_reset!\n", GPIO2_BIT23);
		return -EPERM;
	}
	gpio_direction_output(GPIO2_BIT23, 1);
	mdelay(10);
	gpio_set_value(GPIO2_BIT23, 0);
	mdelay(20);
	gpio_set_value(GPIO2_BIT23, 1);
	mdelay(2000);

	/* set default value */
	for (i = 0; i < NUM_REGS_MAX; i++) {
		//printk("d2_reg[%d].Reg_Addr(%06x) d2_reg[%d].Reg_Val(%06x)\n",
		//		i, d2_reg[i].Reg_Addr, i, d2_reg[i].Reg_Val);
		rc = d2_dae4p_reg_i2c_write(client, d2_reg[i].Reg_Addr, d2_reg[i].Reg_Val);
		if (rc < 0) {
			printk("write reg(%d) rc = %d\n", i, rc);
			return -EIO;
		}
		udelay(30);
	}

	mdelay(3000);
	if (source == 0) /* I2S */
		d2_dae4p_reg_i2c_write(client, 0x020001, 0xC0000E);
	else	/* SPDIF */
		d2_dae4p_reg_i2c_write(client, 0x020001, 0xC0000F);
	d2_dae4p_reg_i2c_write(client, 0, 0xCCCCC);

	d2_i2c_client = client;
	return 0;
}

static void d2_dae4p_hw_exit(void)
{
	gpio_free(GPIO2_BIT23);
}

static long dae4p_mixer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int val, ret = 0;

	if (cmd == SOUND_MIXER_INFO) {
		mixer_info info;
		memset(&info, 0, sizeof(info));
		strlcpy(info.id, "D2-45057", sizeof(info.id));
		strlcpy(info.name, "Intersil D2-45057", sizeof(info.name));
		//info.modify_counter = s->mix.modcnt;
		if (copy_to_user((void *) arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	if (cmd == OSS_GETVERSION)
		return put_user(SOUND_VERSION, (int *) arg);

	if (_IOC_TYPE(cmd) != 'M' || _SIOC_SIZE(cmd) != sizeof(int))
		return -EINVAL;

	/* read */
	val = 0;
	if (_SIOC_DIR(cmd) & _SIOC_WRITE)
		if (get_user(val, (int *) arg))
			return -EFAULT;

	switch (cmd) {
	case MIXER_READ(SOUND_MIXER_CAPS):
		printk("SOUND_MIXER_CAPS\n");
		ret = SOUND_CAP_EXCL_INPUT;
		break;

	case MIXER_READ(SOUND_MIXER_STEREODEVS):
		printk("SOUND_MIXER_STEREODEVS\n");
		ret = SOUND_MASK_VOLUME | SOUND_MASK_PCM;
		break;

	case MIXER_READ(SOUND_MIXER_RECMASK):
		printk("SOUND_MIXER_RECMASK\n");
		ret = 0; //SOUND_MASK_MIC | SOUND_MASK_LINE;
		break;

	case MIXER_READ(SOUND_MIXER_DEVMASK):
		printk("SOUND_MIXER_DEVMASK\n");
		ret = SOUND_MASK_VOLUME | SOUND_MASK_PCM; // | SOUND_MASK_IGAIN | SOUND_MASK_MONITOR;
		break;

	case MIXER_READ(SOUND_MIXER_OUTMASK):
		printk("SOUND_MIXER_OUTMASK\n");
		ret = 0; //MASK_INTERNAL | MASK_LINEOUT | MASK_HEADPHONES;
		break;

	case MIXER_WRITE(SOUND_MIXER_RECSRC):
		printk("SOUND_MIXER_RECSRC\n");
		ret = 0; //harmony_mixer_set_recmask(val);
		break;

	case MIXER_READ(SOUND_MIXER_RECSRC):
		printk("SOUND_MIXER_RECSRC\n");
		ret = 0; //harmony_mixer_get_recmask();
		break;

	case MIXER_WRITE(SOUND_MIXER_OUTSRC):
		printk("SOUND_MIXER_OUTSRC\n");
		ret = 0; //val & (MASK_INTERNAL | MASK_LINEOUT | MASK_HEADPHONES);
		break;

	case MIXER_READ(SOUND_MIXER_OUTSRC):
		printk("SOUND_MIXER_OUTSRC\n");
		ret = 0; //(MASK_INTERNAL | MASK_LINEOUT | MASK_HEADPHONES);
		break;

	case MIXER_WRITE(SOUND_MIXER_VOLUME):
		//case MIXER_WRITE(SOUND_MIXER_IGAIN):
		//case MIXER_WRITE(SOUND_MIXER_MONITOR):
		printk("SOUND_MIXER_VOLUME\n");
	case MIXER_WRITE(SOUND_MIXER_PCM):
		printk("SOUND_MIXER_VOLUME\n");
		d2_dae4p_reg_i2c_write(d2_i2c_client, 0, (val & 0x00FF) * (0x7FFFFF - 0x54) / 100);
		ssp_i2s.level = val;
		break;

	case MIXER_READ(SOUND_MIXER_VOLUME):
		printk("SOUND_MIXER_VOLUME\n");
		//case MIXER_READ(SOUND_MIXER_IGAIN):
		//case MIXER_READ(SOUND_MIXER_MONITOR):
	case MIXER_READ(SOUND_MIXER_PCM):
		printk("SOUND_MIXER_PCM\n");
		ret = ssp_i2s.level;
		break;

	default:
		printk("%s: unknown cmd %x\n", __func__, cmd);
		return -EINVAL;
	}

	if (put_user(ret, (int *) arg))
		return -EFAULT;
	return 0;
}

static int dae4p_mixer_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int dae4p_mixer_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int dae4p_dsp_open(struct inode *inode, struct file *file)
{
	ssp_ctrl.ssp_en = 0;

	return 0;
}

static int dae4p_dsp_release(struct inode *inode, struct file *file)
{
	if (ssp_ctrl.ssp_en)
		ssp_ctrl.ssp_en = cs75xx_ssp_disable(ssp_ctrl.ssp_index);

	memset(ssp_ctrl.tbuf, 0x00, ssp_ctrl.buf_num*ssp_ctrl.buf_size);

	return 0;
}

#define INIT_BUF_GAP	(MAX_BUFS/2)
#define HIGH_BUF_GAP	(MAX_BUFS*3/4)
#define LOW_BUF_GAP	(MAX_BUFS/4)
void i2s_tx_notify(void)
{
	unsigned short tx_write_ptr, tx_read_ptr;

	cs75xx_dma_ssp_tx_ptr(ssp_ctrl.ssp_index, &tx_write_ptr, &tx_read_ptr);

	if (tx_write_ptr >= tx_read_ptr)
		buf_gap = tx_write_ptr - tx_read_ptr;
	else
		buf_gap = ssp_ctrl.buf_num - tx_read_ptr + tx_write_ptr;
	ssp_i2s.out_len += buf_gap * ssp_ctrl.buf_size;

	if (dac_wait_en == 1 && (buf_gap <= LOW_BUF_GAP)) {
		dac_wait_en = 0;
		wake_up_interruptible(&ssp_wait_q);
	}
}

static ssize_t dae4p_dsp_write(struct file *file_p, const char __user * buf,
                                 size_t count, loff_t * ppos)
{
	unsigned short round, tx_write_ptr, tx_read_ptr;
	static char tmp_tbuf[3*SSP_BUF_SIZE];
	int i, j;

	if (halt) {
		local_irq_disable();
		mdelay(halt*1000);
		halt = 0;
		local_irq_enable();
	}

	if ((ssp_i2s.data_format == SSP_DF_8BIT_ULAW) ||
		(ssp_i2s.data_format == SSP_DF_8BIT_ALAW) ||
		(ssp_i2s.data_format == SSP_DF_8BIT_LINEAR)) {
		round = count/ssp_ctrl.buf_size;

		for (i = 0; i < round; i++) {
			if (copy_from_user(tmp_tbuf, buf + i*ssp_ctrl.buf_size, ssp_ctrl.buf_size)) {
				printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
			}
			for (j = 0; j < ssp_ctrl.buf_size; j++) {
				*(ssp_i2s.tbuf_curr) = (int)tmp_tbuf[j] + 128;
				ssp_i2s.tbuf_curr++;
			}
			if (ssp_i2s.tbuf_curr >= ssp_i2s.tbuf_end)
				ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
		}
	}
	else if ((ssp_i2s.data_format == SSP_DF_16BITB_LINEAR) ||
		(ssp_i2s.data_format == SSP_DF_16BITL_LINEAR) ||
		(ssp_i2s.data_format == SSP_DF_16BITUB_LINEAR) ||
		(ssp_i2s.data_format == SSP_DF_16BITUL_LINEAR) ||
		(ssp_i2s.data_format == SSP_DF_24BITUB_LINEAR)) {
		/* count = n*SSP_BUF_SIZE */
		round = count/ssp_ctrl.buf_size;

		for (i = 0; i < round; i++) {
			if (copy_from_user(ssp_i2s.tbuf_curr, buf + i*ssp_ctrl.buf_size, ssp_ctrl.buf_size)) {
				printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
			}

			ssp_i2s.tbuf_curr += ssp_ctrl.buf_size;
			if (ssp_i2s.tbuf_curr >= ssp_i2s.tbuf_end)
				ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
		}
	}
	else if (ssp_i2s.data_format == SSP_DF_24BITUL_LINEAR) {
		/* count = 3*SSP_BUF_SIZE */

		if (copy_from_user(tmp_tbuf, buf, 3*SSP_BUF_SIZE)) {
			printk("%s:%d - copy_from_user fail!\n", __func__, __LINE__);
		}

		for (i = 0; i < 3*SSP_BUF_SIZE; i=i+3) {
			*(ssp_i2s.tbuf_curr++) = tmp_tbuf[i+2];
			*(ssp_i2s.tbuf_curr++) = tmp_tbuf[i+1];
			*(ssp_i2s.tbuf_curr++) = tmp_tbuf[i+0];
			*(ssp_i2s.tbuf_curr++) = 0;
			if (ssp_i2s.tbuf_curr >= ssp_i2s.tbuf_end)
				ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
		}
	}

	cs75xx_dma_ssp_tx_ptr(ssp_ctrl.ssp_index, &tx_write_ptr, &tx_read_ptr);
	tx_write_ptr = ((int)ssp_i2s.tbuf_curr - (int)ssp_ctrl.tbuf)/ssp_ctrl.buf_size;
	cs75xx_dma_ssp_tx_update(ssp_ctrl.ssp_index, tx_write_ptr);

	if (ssp_ctrl.ssp_en == 0) {
		if (tx_write_ptr > INIT_BUF_GAP) {
			cs75xx_ssp_enable(ssp_ctrl.ssp_index, 1, ssp_ctrl.byte_swap, 0, 0);
			ssp_ctrl.ssp_en = 1;
		}
	}
	else {
		if (tx_write_ptr >= tx_read_ptr)
			buf_gap = tx_write_ptr - tx_read_ptr;
		else
			buf_gap = ssp_ctrl.buf_num - tx_read_ptr + tx_write_ptr;

		if (dac_wait_en == 0 && buf_gap >= HIGH_BUF_GAP) {
			dac_wait_en = 1;
			if (0 == wait_event_interruptible_timeout(ssp_wait_q, (buf_gap <= LOW_BUF_GAP), DMA_TIME_OUT)) {
				cs75xx_dma_ssp_tx_ptr(ssp_ctrl.ssp_index, &tx_write_ptr, &tx_read_ptr);
				printk("DMA Tx Timeout(%d, %d)! %d\n", tx_write_ptr, tx_read_ptr, dac_wait_en);
				printk("Play out total %d bytes\n", ssp_i2s.out_len);
				count = 0;
				//cs75xx_ssp_disable(ssp_ctrl.ssp_index);
				goto END;
			}
		}
	}
END:
	return count;
}

static long dae4p_dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int value, retval;
	count_info inf;

	switch (cmd) {
		int ival, new_format;
		int frag_size, frag_buf;
		struct audio_buf_info info;

	/* proprietory test - start */
	case SSP_I2S_INIT_BUF:
		memset(ssp_ctrl.tbuf, 0x00, (ssp_ctrl.buf_num*ssp_ctrl.buf_size));
		//memset(ssp_i2s.tbuf, 0x00, (ssp_ctrl.buf_num*ssp_ctrl.buf_num));
		spin_lock_irqsave(&ssp_lock, flags);
		ssp_i2s.tol_size = 0;
		ssp_i2s.wt_size = 0;
		ssp_i2s.rm_size = 0;
		ssp_i2s.wt_curr = 0;
		ssp_i2s.in_curr = 0;
		ssp_i2s.in_cont = 0;
		ssp_i2s.tx_curr = 0;
		ssp_i2s.file_len = 0;
		ssp_i2s.wait_write_len = 0;
		ssp_i2s.out_len = 0;
		ssp_i2s.in_bufok = 0;
		ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;

		dac_wait_en = 0;
		spin_unlock_irqrestore(&ssp_lock, flags);

		printk("SSP_I2S_DMA SSP_I2S_INIT_BUF\n");
		break;

	case SSP_I2S_STOP_DMA:
		ssp_ctrl.ssp_en = cs75xx_ssp_disable(ssp_ctrl.ssp_index);
		printk("Play out total %d bytes, en = %d\n", ssp_i2s.out_len, ssp_ctrl.ssp_en);
		break;

	case SSP_I2S_FILE_INFO:
		{
			wave_file_info_t fileInfo;

			if (arg) {
				if (copy_from_user(&fileInfo, (int *) arg, sizeof(fileInfo)))
					retval = -EFAULT;
			}
			if (fileInfo.wBitsPerSample == 8) {
				//printk("8 bits\n");
				ssp_i2s.data_format = SSP_DF_8BIT_ULAW;
				ssp_ctrl.byte_swap = 0;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 1);
			}
			else if (fileInfo.wBitsPerSample == 16) {
				//printk("16 bits\n");
				ssp_i2s.data_format = SSP_DF_16BITUL_LINEAR;
				ssp_ctrl.byte_swap = 1;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 2);
			}
			else if (fileInfo.wBitsPerSample == 24) {
				//printk("24 bits - %s\n", fileInfo.endian ? "big" : "little");
				if (fileInfo.endian == 0)
					ssp_i2s.data_format = SSP_DF_24BITUL_LINEAR;
				else
					ssp_i2s.data_format = SSP_DF_24BITUB_LINEAR;
				ssp_ctrl.byte_swap = 0;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 3);
			}
		#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
			cs75xx_spdif_clock_start(fileInfo.nSamplesPerSec, 0, 1);
		#endif
		}
		break;

	case SSP_I2S_FILE_LEN:
		if (arg) {
			if (copy_from_user(&value, (int *) arg, sizeof(value)))
				retval = -EFAULT;
		}
		ssp_i2s.file_len = value;
		ssp_i2s.wait_write_len = ssp_i2s.file_len;
		ssp_i2s.out_len = 0;
		printk("ssp_i2s.file_len : %d\n", ssp_i2s.file_len);
		break;

	case SSP_I2S_INIT_MIXER:
		break;

	case SSP_I2S_INC_LEVEL:
		break;

	case SSP_I2S_DEC_LEVEL:
		break;

	case SSP_I2S_SETFMT:
		if (arg) {
			if (copy_from_user(&value, (int *) arg, sizeof(value)))
				retval = -EFAULT;
		}

		ssp_i2s.data_format = value;
		printk("data_format : %d\n", ssp_i2s.data_format);
		break;

	case SSP_I2S_STEREO:
		if (arg) {
			if (copy_from_user(&value, (int *) arg, sizeof(value)))
				retval = -EFAULT;
		}

		ssp_i2s.stereo_select = value;
		printk("stereo_select : %d\n", ssp_i2s.stereo_select);
		break;

	case SSP_I2S_SETSPEED:
		if (arg) {
			if (copy_from_user(&value, (int *) arg, sizeof(value)))
				retval = -EFAULT;
		}

		switch (value) {
		case 0:
			ssp_i2s.dac_rate = 44100;
			break;
		case 1:
			ssp_i2s.dac_rate = 22050;
			break;
		case 2:
			ssp_i2s.dac_rate = 48000;
			break;
		case 3:
			ssp_i2s.dac_rate = 24000;
			break;
		default:
			ssp_i2s.dac_rate = 44100;
			break;
		}
		ssp_i2s.sample_rate = ssp_i2s.dac_rate;
		printk("speed : %d\n", ssp_i2s.dac_rate);
		break;

	case SSP_I2S_SETCHANNELS:
		if (arg) {
			if (copy_from_user(&value, (int *) arg, sizeof(value)))
				retval = -EFAULT;
		}

		ssp_i2s.stereo_select = value - 1;
		printk("channel : %d\n", ssp_i2s.stereo_select);
		break;
	/* proprietory test - end */
	case OSS_GETVERSION:
		if (debug) printk("OSS_GETVERSION\n");
		return put_user(SOUND_VERSION, (int *) arg);

	case SNDCTL_DSP_GETCAPS:
		ival = DSP_CAP_BATCH;
		if (debug) printk("SNDCTL_DSP_GETCAPS\n");
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETFMTS:
		if (debug) printk("SNDCTL_DSP_GETFMTS\n");
		ival = (AFMT_S16_BE | AFMT_S16_LE);	//| AFMT_MU_LAW | AFMT_A_LAW ); //AFMT_S16_BE
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SETFMT:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		if (debug) printk("SNDCTL_DSP_SETFMT(%x)\n", ival);
		if (ival != AFMT_QUERY) {
			switch (ival) {
				//case AFMT_MU_LAW:     new_format = SSP_DF_8BIT_ULAW; break;
				//case AFMT_A_LAW:      new_format = SSP_DF_8BIT_ALAW; break;
			case AFMT_U8:
				new_format = SSP_DF_8BIT_LINEAR;
				ssp_ctrl.byte_swap = 0;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 1);
				break;
			case AFMT_S16_BE:
				new_format = SSP_DF_16BITB_LINEAR;
				ssp_ctrl.byte_swap = 0;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 2);
				break;
			case AFMT_S16_LE:
				new_format = SSP_DF_16BITL_LINEAR;
				ssp_ctrl.byte_swap = 1;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 2);
				break;
			case AFMT_U16_BE:
				new_format = SSP_DF_16BITUB_LINEAR;
				ssp_ctrl.byte_swap = 0;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 2);
				break;
			case AFMT_U16_LE:
				new_format = SSP_DF_16BITUL_LINEAR;
				ssp_ctrl.byte_swap = 1;
				cs75xx_ssp_slot(ssp_ctrl.ssp_index, SSP_I2S_DAC, 2);
				break;
			default:
				{
					DPRINTK(KERN_WARNING PFX
						"unsupported sound format 0x%04x requested.\n",
						ival);
					ival = AFMT_S16_BE;
					return put_user(ival, (int *) arg);
				}
			}

			ssp_i2s.data_format = new_format;
			//cs75xx_ssp_reset(ssp_ctrl.ssp_index);
			//memset(ssp_i2s.tbuf, 0x00, (ssp_ctrl.buf_num*ssp_ctrl.buf_num));
			memset(ssp_ctrl.tbuf, 0x00, (ssp_ctrl.buf_num*ssp_ctrl.buf_size));
			spin_lock_irqsave(&ssp_lock, flags);
			ssp_i2s.tol_size = 0;
			ssp_i2s.wt_size = 0;
			ssp_i2s.rm_size = 0;
			ssp_i2s.wt_curr = 0;
			ssp_i2s.in_curr = 0;
			ssp_i2s.in_cont = 0;
			ssp_i2s.tx_curr = 0;
			ssp_i2s.file_len = 0;
			ssp_i2s.wait_write_len = 0;
			ssp_i2s.out_len = 0;
			ssp_i2s.in_bufok = 0;
			ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
			dac_wait_en = 0;

			if (new_format == SSP_DF_16BITL_LINEAR || new_format == SSP_DF_16BITUL_LINEAR)
				ssp_ctrl.byte_swap = 1;
			spin_unlock_irqrestore(&ssp_lock, flags);

			return 0;
		} else {
			switch (ssp_i2s.data_format) {
				//case SSP_DF_8BIT_ULAW:        ival = AFMT_MU_LAW; break;
				//case SSP_DF_8BIT_ALAW:        ival = AFMT_A_LAW;  break;
				//case SSP_DF_16BIT_LINEAR:     ival = AFMT_U16_BE; break;//AFMT_S16_BE
			case SSP_DF_8BIT_LINEAR:
				ival = AFMT_U8;
			case SSP_DF_16BITB_LINEAR:
				ival = AFMT_S16_BE;
				break;	//AFMT_S16_BE
			case SSP_DF_16BITL_LINEAR:
				ival = AFMT_S16_LE;
				break;	//AFMT_S16_BE
			case SSP_DF_16BITUB_LINEAR:
				ival = AFMT_U16_BE;
				break;	//AFMT_S16_BE
			case SSP_DF_16BITUL_LINEAR:
				ival = AFMT_U16_LE;
				break;	//AFMT_S16_BE
			default:
				ival = 0;
			}
			return put_user(ival, (int *) arg);
		}

	case SOUND_PCM_READ_RATE:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		if (debug) printk("SOUND_PCM_READ_RATE(%d)\n", ival);
		ssp_i2s.dac_rate = 44100;
		ival = ssp_i2s.dac_rate;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SPEED:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		if (debug) printk("SNDCTL_DSP_SPEED(%d)\n", ival);
		if (ival < 32000) ival = 32000;
		if (ival > 96000) ival = 96000;
#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
		if (cs75xx_spdif_clock_start(ival, 0, 1))
			return -EINVAL;
		ssp_i2s.dac_rate = ival;
#else
		ival = 44100;
		ssp_i2s.dac_rate = 44100;
#endif
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_STEREO:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		if (debug) printk("SNDCTL_DSP_STEREO(%d)\n", ival);
		if (ival != 0 && ival != 1)
			return -EINVAL;

		ssp_i2s.stereo_select = ival;
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		if (debug) printk("SNDCTL_DSP_CHANNELS(%d)\n", ival);
		if (ival != 1 && ival != 2) {
			ival = ssp_i2s.stereo_select == SSP_SS_MONO ? 1 : 2;
			return put_user(ival, (int *) arg);
		}
		ssp_i2s.stereo_select = ival - 1;
		return 0;

	case SOUND_PCM_READ_CHANNELS:
		if (debug) printk("SOUND_PCM_READ_CHANNELS\n");
		ival = ssp_i2s.stereo_select + 1;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETBLKSIZE:
		ival = ssp_ctrl.buf_num;
		if (debug) printk("SNDCTL_DSP_GETBLKSIZE(0x%x)\n", ival);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_NONBLOCK:
		if (debug) printk("SNDCTL_DSP_NONBLOCK\n");
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_RESET:
		if (debug) printk("SNDCTL_DSP_RESET\n");
		if (file->f_mode & FMODE_READ) {
			//audio_reset_buf(is);
		}
		ssp_ctrl.ssp_en = cs75xx_ssp_disable(ssp_ctrl.ssp_index);

		if (file->f_mode & FMODE_WRITE) {
			//cs75xx_ssp_reset(ssp_ctrl.ssp_index);
			memset(ssp_ctrl.tbuf, 0x00, (ssp_ctrl.buf_num*ssp_ctrl.buf_size));
			//memset(ssp_i2s.tbuf, 0x00, (ssp_ctrl.buf_num*ssp_ctrl.buf_num));
			spin_lock_irqsave(&ssp_lock, flags);
			ssp_i2s.tol_size = 0;
			ssp_i2s.wt_size = 0;
			ssp_i2s.rm_size = 0;
			ssp_i2s.wt_curr = 0;
			ssp_i2s.in_curr = 0;
			ssp_i2s.in_cont = 0;
			ssp_i2s.tx_curr = 0;
			ssp_i2s.file_len = 0;
			ssp_i2s.wait_write_len = 0;
			ssp_i2s.out_len = 0;
			ssp_i2s.in_bufok = 0;
			ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
			spin_unlock_irqrestore(&ssp_lock, flags);
		}
		return 0;

	case SNDCTL_DSP_SETFRAGMENT:
		if (debug) printk("SNDCTL_DSP_SETFRAGMENT\n");
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		frag_size = ival & 0xffff;
		frag_buf = (ival >> 16) & 0xffff;
		/* TODO: We use hardcoded fragment sizes and numbers for now */
		frag_size = 11;	/* 4096 == 2^12 *///ssp_ctrl.buf_num
		frag_buf = ssp_ctrl.buf_num;
		ival = (frag_buf << 16) + frag_size;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETODELAY:
		if (!(file->f_mode & FMODE_WRITE))
			return -EPERM;
		if (delay == 0) {
			u16 tx_write_ptr, tx_read_ptr;
			u32 data_rate = ssp_ctrl.sample_rate * (ssp_ctrl.tx_data_size / 8) * 2; /* Bytes/sec, 2 means 2 channels */
			u32 buf_data;

			cs75xx_dma_ssp_tx_ptr(&tx_write_ptr, &tx_read_ptr);
			if (tx_write_ptr >= tx_read_ptr)
				buf_data = (tx_write_ptr - tx_read_ptr);
			else
				buf_data = (tx_write_ptr + ssp_ctrl.tx_buf_num - tx_read_ptr);

			/* FIXME */
			if (buf_data > 4)
				buf_data = 4;
			buf_data *= ssp_ctrl.tx_buf_size;

			/* ival with us unit? */
			ival = (buf_data * 1000) / (data_rate / 1000);
		}
		else
			ival = delay;

		if (debug) printk("SNDCTL_DSP_GETODELAY(%d)\n", ival);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETOSPACE:
		//if (debug) printk("SNDCTL_DSP_GETOSPACE \n");
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		info.fragstotal = ssp_ctrl.buf_num;
		spin_lock_irqsave(&ssp_lock, flags);;
		info.fragments = (ssp_ctrl.buf_num - ssp_i2s.in_cont);
		spin_unlock_irqrestore(&ssp_lock, flags);
		info.fragsize = ssp_ctrl.buf_num;
		info.bytes = info.fragments * info.fragsize;
		// printk("[%x-%x-%x]\n",info.bytes,info.fragments,ssp_i2s.in_cont);
		return copy_to_user((void *) arg, &info, sizeof(info)) ? -EFAULT : 0;

	case SNDCTL_DSP_GETISPACE:
		if (debug) printk("SNDCTL_DSP_GETISPACE\n");
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		info.fragstotal = ssp_ctrl.buf_num;
		info.fragments = 20;	// ssp_i2s.nb_filled_record; /*ssp_ctrl.buf_num-*/
		info.fragsize = ssp_ctrl.buf_num;
		info.bytes = info.fragments * info.fragsize;
		return 0;	//copy_to_user((void *)arg, &info, sizeof(info)) ? -EFAULT : 0;

	case SNDCTL_DSP_SYNC:
		if (debug) printk("SNDCTL_DSP_SYNC\n");
		ssp_ctrl.ssp_en = cs75xx_ssp_disable(ssp_ctrl.ssp_index);
		return 0;

	case SNDCTL_DSP_SETDUPLEX:
		if (debug) printk("SNDCTL_DSP_SETDUPLEX\n");
		return 0;

	case SNDCTL_DSP_POST:
		if (debug) printk("SNDCTL_DSP_POST\n");
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		if (debug) printk("SNDCTL_DSP_GETTRIGGER\n");
		//PCM_ENABLE_INPUT
		//PCM_ENABLE_OUTPUT
		return 0;

	case SNDCTL_DSP_GETOPTR:
		if (debug) printk("SNDCTL_DSP_GETOPTR\n");

		//int bytecount, offset, flags;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;

		inf.blocks = ssp_ctrl.buf_num;
		inf.bytes = ssp_i2s.tol_size;

		return copy_to_user((void *) arg, &inf, sizeof(inf));

	case SNDCTL_DSP_GETIPTR:
		if (debug) printk("SNDCTL_DSP_GETIPTR \n");
		//count_info inf = { 0, };

		//int bytecount, offset, flags;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;

		inf.blocks = ssp_ctrl.buf_num;
		inf.bytes = ssp_i2s.tol_size;
		return copy_to_user((void *) arg, &inf, sizeof(inf));

	case SOUND_PCM_READ_BITS:
		if (debug) printk("SOUND_PCM_READ_BITS\n");
		return 0;

	case SOUND_PCM_READ_FILTER:
		if (debug) printk("SOUND_PCM_READ_FILTER\n");
		return 0;

	case SNDCTL_DSP_SUBDIVIDE:
		if (debug) printk("SNDCTL_DSP_SUBDIVIDE\n");
		return 0;

	case SNDCTL_DSP_SETTRIGGER:
		if (debug) printk("SNDCTL_DSP_SETTRIGGER\n");
		return 0;

	case SNDCTL_DSP_SETSYNCRO:
		if (debug) printk("SNDCTL_DSP_SETSYNCRO\n");
		return 0;

	case SNDCTL_DSP_MAPINBUF:
		if (debug) printk("SNDCTL_DSP_MAPINBUF\n");
		return 0;

	case SNDCTL_DSP_MAPOUTBUF:
		if (debug) printk("SNDCTL_DSP_MAPOUTBUF\n");
		return 0;
	default:
		printk("%s: unknown cmd %x\n", __func__, cmd);
	}

	return -EINVAL;
}

static int dac_ssp_init(void)
{
	cs75xx_ssp_cfg_t ssp_cfg;

	/* init dac and ssp control block */
	spin_lock_irqsave(&ssp_lock, flags);

	ssp_i2s.tol_size = 0;
	ssp_i2s.wt_size = 0;
	ssp_i2s.rm_size = 0;
	ssp_i2s.wt_curr = 0;
	ssp_i2s.in_curr = 0;
	ssp_i2s.in_cont = 0;
	ssp_i2s.tx_curr = 0;
	ssp_i2s.in_bufok = 0;
	ssp_i2s.file_len = 0;
	ssp_i2s.wait_write_len = 0;
	ssp_i2s.out_len = 0;
	ssp_i2s.level = 36;
	ssp_i2s.data_format = SSP_DF_16BITUL_LINEAR;
	ssp_i2s.sample_rate = 44100;

	ssp_ctrl.buf_num = LLP_SIZE;
	ssp_ctrl.buf_size = DBUF_SIZE;
	ssp_ctrl.tbuf = NULL;
	ssp_ctrl.rbuf = NULL;
	ssp_ctrl.ssp_index = 0;

	if ((ssp_i2s.data_format == SSP_DF_16BITL_LINEAR) ||
	    (ssp_i2s.data_format == SSP_DF_16BITUL_LINEAR))
		ssp_ctrl.byte_swap = 1;
	else
		ssp_ctrl.byte_swap = 0;
	ssp_ctrl.ssp_en = 0;

	spin_unlock_irqrestore(&ssp_lock, flags);

	/* allocate buffer for tx */
	ssp_ctrl.tbuf = kzalloc(ssp_ctrl.buf_num*ssp_ctrl.buf_size, GFP_KERNEL|GFP_DMA);
	ssp_ctrl.tbuf_paddr = __pa(ssp_ctrl.tbuf);
	if (debug) printk("tbuf = %p, tbuf_paddr = %x\n", ssp_ctrl.tbuf, ssp_ctrl.tbuf_paddr);

	if (ssp_ctrl.tbuf == NULL)
		printk("Can't alloc Tx buffer 0x%x for DMA SSP%d\n", ssp_ctrl.buf_num*ssp_ctrl.buf_size, ssp_ctrl.ssp_index);
	if (!ssp_ctrl.tbuf) {
		return -ENOMEM;
	}
	ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
	ssp_i2s.tbuf_end = ssp_ctrl.tbuf + ssp_ctrl.buf_num*ssp_ctrl.buf_size;

	d2_dae4p_reg_i2c_write(d2_i2c_client, 0, ssp_i2s.level * (0x7FFFFF - 0x54) / 100);
#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
	cs75xx_spdif_clock_start(44100, 0, 1);
#endif

	/* init ssp and dma */
	ssp_cfg.profile = SSP_I2S_D2;
	ssp_cfg.codec_size = 0;
	ssp_cfg.chan_num = 2;
#ifdef CONFIG_DAC_REF_INTERNAL_CLK
	ssp_cfg.ext_clk = SCLK_INT_SPDIF;
#else
	ssp_cfg.ext_clk = SCLK_EXT_OSC;
#endif
	cs75xx_ssp_register(ssp_ctrl.ssp_index, ssp_cfg);
	cs75xx_dma_ssp_register(ssp_ctrl.ssp_index, ssp_ctrl.tbuf_paddr, ssp_ctrl.buf_num,
	                    ssp_ctrl.buf_size, 0, 0, 0);
	request_cs75xx_dma_ssp_irq(ssp_ctrl.ssp_index, i2s_tx_notify, NULL);

	return 0;
}

static void dac_ssp_exit(void)
{
	cs75xx_ssp_unregister(ssp_ctrl.ssp_index);
	cs75xx_dma_ssp_unregister(ssp_ctrl.ssp_index);
	free_cs75xx_dma_ssp_irq(ssp_ctrl.ssp_index);
	kfree(ssp_ctrl.tbuf);
}

static struct file_operations dae4p_dsp_fops = {
	.owner          = THIS_MODULE,
	.open           = dae4p_dsp_open,
	.release        = dae4p_dsp_release,
	.write          = dae4p_dsp_write,
	.unlocked_ioctl = dae4p_dsp_ioctl,
};

static struct file_operations dae4p_mixer_fops = {
	.owner          = THIS_MODULE,
	.open           = dae4p_mixer_open,
	.release        = dae4p_mixer_release,
	//.write        = dae4p_mixer_write,
	.unlocked_ioctl = dae4p_mixer_ioctl,
};

static int d2_dae4p_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int j;

	printk("Intersil D2-45057 with CS75XX SSP - I2C Initialization(dsp).");
	if (d2_dae4p_hw_init(client))
		goto fail;
	printk(".");

	if (source == 0) {/* I2S */
		if (dac_ssp_init())
			goto fail;
		printk(".");

		/* register devices */
		if ((audio_dev_id = register_sound_dsp(&dae4p_dsp_fops, -1)) < 0)
			goto fail;
		printk(".");
	}

	if ((mixer_dev_id = register_sound_mixer(&dae4p_mixer_fops, -1)) < 0)
		goto fail;
	printk(".OK!\n");

	return 0;
fail:
	return -1;
}

static int d2_dae4p_i2c_remove(struct i2c_client *client)
{
	unregister_sound_mixer(mixer_dev_id);
	if (source == 0) {
		unregister_sound_dsp(audio_dev_id);
		dac_ssp_exit();
	}

	d2_dae4p_hw_exit();

	return 0;
}

static const struct i2c_device_id d2_dae4p_id[] = {
	{ "d2_dae4p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2_dae4p_id);

static struct i2c_driver d2_dae4p_i2c_driver = {
	.driver = {
		.name	= "d2_dae4p",
		.owner	= THIS_MODULE,
	},
	.probe		= d2_dae4p_i2c_probe,
	.remove		= __devexit_p(d2_dae4p_i2c_remove),
	.id_table	= d2_dae4p_id,
};

static int __init d2_dae4p_init(void)
{
	int ret;

	ret = i2c_add_driver(&d2_dae4p_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register d2_dae4p-70: %d\n", ret);
	}
	return ret;

}

static void __exit d2_dae4p_exit(void)
{
	i2c_del_driver(&d2_dae4p_i2c_driver);
}

module_init(d2_dae4p_init);
module_exit(d2_dae4p_exit);

module_param(halt, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(halt, "dae4p halt time");

module_param(debug, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug, "dae4p debug flag");

module_param(source, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(source, "data source 0:i2s, 1:spdif");

module_param(delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(delay, "dac sync delay");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Middle Huang <middle.huang@cortina-systems.com>");
MODULE_DESCRIPTION("d2_dae4p with Cortina Golden Gate SSP driver");

