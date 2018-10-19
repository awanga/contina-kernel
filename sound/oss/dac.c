/*
* 	sound/oss/dac.c

* Storm audio driver
*
* Copyright (c) 2000 Middle Huang
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License.
*
* History:
* cortina-g2.c
* J79(ssp1) & J78(ssp0)
*
* =  : i2s
* +  : spdif
*
* 1 =
* 2 =  +
* 3    +
*
*   make sure DMA_SSP_FRAME_CTRL1 =  0x1007701f : for standard i2s
*
*
*
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/io.h>	// for i2c
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <mach/cs75xx_ssp.h>
#include "dac.h"

#ifdef CONFIG_SOUND_DAC_SSP
#define HAVE_CS4341	1
#endif

#define PFX "Cortina: "
#define SSP_VERSION "V0.1"

#define DEBUG
#ifdef DEBUG
# define DPRINTK printk
#else
# define DPRINTK(x,...)
#endif

typedef u8 BOOL;
typedef u8 UINT8;
typedef u16 UINT16;
typedef u32 UINT32;

//mknod /dev/dsp c 14 3 mknod /dev/mixer c 14 0

//#define MAX_BUFS 64		/* maximum number of rotating buffers */
//#define SSP_BUF_SIZE 2048	/* needs to be a multiple of PAGE_SIZE (4096)! */

#define CNTL_C		0x80000000
#define	CNTL_ST		0x00000020
#define CNTL_44100	0x00000015	/* SSP_SR_44KHZ */
#define CNTL_8000	0x00000008	/* SSP_SR_8KHZ */

#define GAINCTL_HE	0x08000000
#define GAINCTL_LE	0x04000000
#define GAINCTL_SE	0x02000000

#define DSTATUS_PN	0x00000200
#define DSTATUS_RN	0x00000002

#define DSTATUS_IE	0x80000000

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

#define SSP_SR_8KHZ		0x08
#define SSP_SR_16KHZ		0x09
#define SSP_SR_27KHZ		0x0A
#define SSP_SR_32KHZ		0x0B
#define SSP_SR_48KHZ		0x0E
#define SSP_SR_9KHZ		0x0F
#define SSP_SR_5KHZ		0x10
#define SSP_SR_11KHZ		0x11
#define SSP_SR_18KHZ		0x12
#define SSP_SR_22KHZ		0x13
#define SSP_SR_37KHZ		0x14
#define SSP_SR_44KHZ		0x15
#define SSP_SR_33KHZ		0x16
#define SSP_SR_6KHZ		0x17
#define SSP_SR_24KHZ		0x18

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
	UINT32 current_gain;
	UINT32 dac_rate;	/* 8000 ... 48000 (Hz) */
	UINT8 data_format;	/* HARMONY_DF_xx_BIT_xxx */
	UINT8 stereo_select;	/* HARMONY_SS_MONO or HARMONY_SS_STEREO */
	UINT16 sample_rate;	/* HARMONY_SR_xx_KHZ */
	UINT32 level;
} SSP_I2S;

static cs75xx_ssp_ctrl_t ssp_ctrl;
static SSP_I2S ssp_i2s;

//static int ttt=0;
static DECLARE_WAIT_QUEUE_HEAD(ssp_wait_q);
static int buf_gap;
//wait_queue_head_t ssp_wait_q;
unsigned long flags;
static spinlock_t ssp_lock = SPIN_LOCK_UNLOCKED;

#ifdef HAVE_CS4341
#define CS4341_MCLK		0x00
#define CS4341_MODE		0x01
#define CS4341_TRANS		0x02
#define CS4341_CHA_VOL		0x03
#define CS4341_CHB_VOL		0x04

/* CS4341_MCLK_CTRL */
#define MCLKDIV			BIT(1)

/* CS4341_MODE_CTRL */
#define AMUTE			BIT(7)
#define DIF_OFF			4
#define DIF_MASK		0x70
#define DEM_OFF			2
#define DEM_MAKS		0x0C
#define POR			BIT(1)

/* CS4341_TRANS_MIX_CTRL */
#define AEQB
#define SZCX_OFF		5
#define SZCX_MASK		0x60
#define ATAPI_OFF		0
#define ATAPI_MASK		0x1F

/* CS4341_CH_A_VOL_CTRL, CS4341_CH_B_VOL_CTRL */
#define MUTE			BIT(7)
#define VOLX_OFF		0
#define VOLX_MASK		0x7F

#define CS4341_I2C_SLAVE_ADDR	0x10	/* prefix 001000 + AD0 */
#endif

#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
extern int cs75xx_spdif_clock_start(unsigned int);
#endif

#ifdef HAVE_CS4341
struct i2c_client *dac_i2c_client;
static int cs4341_reg_i2c_write(u8 slave_addr, u8 reg, u8 value)
{
	struct i2c_msg msg;
	unsigned char buf[2];

	//printk("cs4341_reg_i2c_write\n");

	buf[0] = reg;
	buf[1] = value;

	msg.addr = dac_i2c_client->addr;
	msg.buf = buf;
	msg.len = 2;

	return i2c_transfer(dac_i2c_client->adapter, &msg, 1);
}

static int cs4341_reg_i2c_read(u8 slave_addr, u8 reg, u8 *value_p)
{
	struct i2c_msg msg[2];

	//printk("cs4341_reg_i2c_read\n");

	msg[0].addr = dac_i2c_client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = 1;

	msg[1].addr = dac_i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = value_p;
	msg[1].len = 1;

	return i2c_transfer(dac_i2c_client->adapter, msg, 2);
}

static void dac_i2c_client_init(void)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = 0x10;//CS4341_I2C_SLAVE_ADDR;
	strlcpy(info.type, "dac", I2C_NAME_SIZE);

	adapter = i2c_get_adapter(0);
	if (!adapter) {
		printk("can't get i2c adapter 0\n");
		return;
	}

	dac_i2c_client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!dac_i2c_client) {
		printk("can't add i2c device at 0x%x\n", (unsigned int)info.addr);
		return;
	}
}

static void dac_i2c_client_remove(void)
{
	i2c_unregister_device(dac_i2c_client);
}

static UINT16 cs4341_setlevel(UINT8 left, UINT8 right)
{
	//printk("%s : val %x %x",__func__,left,right);
	cs4341_reg_i2c_write(CS4341_I2C_SLAVE_ADDR, CS4341_CHA_VOL, left);
	cs4341_reg_i2c_write(CS4341_I2C_SLAVE_ADDR, CS4341_CHB_VOL, right);

	return 0;
}

void cs4341_hw_init(void)
{
	u8 mclk, mode, trans, cha_vol, chb_vol;
	int count = 0;

#ifndef	CONFIG_I2C_BOARDINFO
	dac_i2c_client_init();
#endif

DO_AGAIN:
	cs4341_reg_i2c_write(0x10, CS4341_MODE, 0x11);
	cs4341_reg_i2c_write(0x10, CS4341_TRANS, 0x49);
	ssp_i2s.level = 0x0;
	cs4341_reg_i2c_write(0x10, CS4341_CHA_VOL, 0x0);
	cs4341_reg_i2c_write(0x10, CS4341_CHB_VOL, 0x0);
	cs4341_reg_i2c_write(0x10, CS4341_MODE, 0x20);

	cs4341_reg_i2c_read(0x10, CS4341_MCLK, &mclk);
	printk("REG(%d): - (%x)\n", CS4341_MCLK, mclk);
	cs4341_reg_i2c_read(0x10, CS4341_MODE, &mode);
	printk("REG(%d): - (%x)\n", CS4341_MODE, mode);
	cs4341_reg_i2c_read(0x10, CS4341_TRANS, &trans);
	printk("REG(%d): - (%x)\n", CS4341_TRANS, trans);
	cs4341_reg_i2c_read(0x10, CS4341_CHA_VOL, &cha_vol);
	printk("REG(%d): - (%x)\n", CS4341_CHA_VOL, cha_vol);
	cs4341_reg_i2c_read(0x10, CS4341_CHB_VOL, &chb_vol);
	printk("REG(%d): - (%x)\n", CS4341_CHB_VOL, chb_vol);
	cs4341_reg_i2c_read(0x10, CS4341_TRANS, &trans);

	if ((mclk == 0) && (mode == 0x20) && (trans == 0x49) && (cha_vol == 0) &&
	    (chb_vol == 0))
		printk("OK\n");
	else {
		printk("FAIL(%d)\n", ++count);
		if (count <= 5)
			goto DO_AGAIN;
	}
}

void cs4341_remove(void)
{
	dac_i2c_client_remove();
}
#endif

static int ssp_i2s_mixer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int val;
	int ret = 0;

	printk("ssp_i2s_mixer_ioctl\n");
	if (cmd == SOUND_MIXER_INFO) {
		//mixer_info info;
		return 0;
	}

	if (cmd == OSS_GETVERSION)
		return put_user(SOUND_VERSION, (int *) arg);

	/* read */
	val = 0;
	if (_SIOC_DIR(cmd) & _SIOC_WRITE)
		if (get_user(val, (int *) arg))
			return -EFAULT;

	printk("%s : **************** ssp_i2s_mixer_ioctl\n", __func__);
	switch (cmd) {
	case MIXER_READ(SOUND_MIXER_CAPS):
		printk("%s : SOUND_MIXER_CAPS\n", __func__);
		ret = SOUND_CAP_EXCL_INPUT;
		break;
	case MIXER_READ(SOUND_MIXER_STEREODEVS):
		printk("%s : SOUND_MIXER_STEREODEVS\n", __func__);
		ret = SOUND_MASK_VOLUME | SOUND_MASK_PCM;
		break;

	case MIXER_READ(SOUND_MIXER_RECMASK):
		ret = 0;	//SOUND_MASK_MIC | SOUND_MASK_LINE;
		printk("%s : SOUND_MIXER_RECMASK\n", __func__);
		break;

	case MIXER_READ(SOUND_MIXER_DEVMASK):
		printk("%s : SOUND_MIXER_DEVMASK\n", __func__);
		ret = SOUND_MASK_VOLUME | SOUND_MASK_PCM;	// | SOUND_MASK_IGAIN | SOUND_MASK_MONITOR;
		break;

	case MIXER_READ(SOUND_MIXER_OUTMASK):
		printk("%s : SOUND_MIXER_OUTMASK\n", __func__);
		ret = 0;	//MASK_INTERNAL | MASK_LINEOUT | MASK_HEADPHONES;
		break;

	case MIXER_WRITE(SOUND_MIXER_RECSRC):
		printk("%s : SOUND_MIXER_RECSRC\n", __func__);
		ret = 0;	//harmony_mixer_set_recmask(val);
		break;

	case MIXER_READ(SOUND_MIXER_RECSRC):
		printk("%s : SOUND_MIXER_RECSRC\n", __func__);
		ret = 0;	//harmony_mixer_get_recmask();
		break;

	case MIXER_WRITE(SOUND_MIXER_OUTSRC):
		printk("%s : SOUND_MIXER_OUTSRC\n", __func__);
		ret = 0;	//val & (MASK_INTERNAL | MASK_LINEOUT | MASK_HEADPHONES);
		break;

	case MIXER_READ(SOUND_MIXER_OUTSRC):
		printk("%s : SOUND_MIXER_OUTSRC\n", __func__);
		ret = 0;	//(MASK_INTERNAL | MASK_LINEOUT | MASK_HEADPHONES);
		break;

	case MIXER_WRITE(SOUND_MIXER_VOLUME):
	//case MIXER_WRITE(SOUND_MIXER_IGAIN):
	//case MIXER_WRITE(SOUND_MIXER_MONITOR):
	case MIXER_WRITE(SOUND_MIXER_PCM):
#ifdef HAVE_CS4341
		ret = cs4341_setlevel(val >> 0x08, val & 0xff);
		ssp_i2s.level = val;
#endif
		break;

	case MIXER_READ(SOUND_MIXER_VOLUME):
	//case MIXER_READ(SOUND_MIXER_IGAIN):
	//case MIXER_READ(SOUND_MIXER_MONITOR):
	case MIXER_READ(SOUND_MIXER_PCM):
		ret = ssp_i2s.level;
		break;

	default:
		return -EINVAL;
	}

	if (put_user(ret, (int *) arg))
		return -EFAULT;
	return 0;
}


static int ssp_i2s_mixer_open(struct inode *inode, struct file *file)
{
	printk("ssp_i2s_mixer_open\n");
	//cs4341_hw_init();

	return 0;
}


static int ssp_i2s_mixer_release(struct inode *inode, struct file *file)
{
	//gpio_i2c_writereg(0x22,0x01,0x11);
	printk("ssp_i2s_mixer_release\n");
	return 0;
}


//static ssize_t ssp_i2s_mixer_write(struct file *file_p, const char __user *buf, size_t count, loff_t * ppos)
//{
//	//printk("ssp_i2s_mixer_write\n");
//	return 0;
//}


static int ssp_i2s_dsp_open(struct inode *inode, struct file *file)
{
	ssp_ctrl.ssp_en = 0;
	return 0;
}


static int ssp_i2s_dsp_release(struct inode *inode, struct file *file)
{
	if (ssp_ctrl.ssp_en)
	ssp_ctrl.ssp_en = cs75xx_ssp_disable(ssp_ctrl.ssp_index);

	memset(ssp_ctrl.tbuf, 0x00, ssp_ctrl.buf_num*ssp_ctrl.buf_size);
	printk("Stoem I2S of SSP release(dsp)\n");
	return 0;
}


#define INIT_BUF_GAP	24
#define HIGH_BUF_GAP	32
#define LOW_BUF_GAP	16
static int dac_wait_en = 0;
void i2s_tx_notify(void)
{
	unsigned short round, tx_write_ptr, tx_read_ptr;

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

static ssize_t ssp_i2s_dsp_write(struct file *file_p, const char __user * buf,
                                 size_t count, loff_t * ppos)
{
	unsigned short round, tx_write_ptr, tx_read_ptr;
	static char tmp_tbuf[3*SSP_BUF_SIZE];
	int i, j;

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
		cs75xx_dma_ssp_tx_ptr(ssp_ctrl.ssp_index, &tx_write_ptr, &tx_read_ptr);
		if (tx_write_ptr >= tx_read_ptr)
			buf_gap = tx_write_ptr - tx_read_ptr;
		else
			buf_gap = ssp_ctrl.buf_num - tx_read_ptr + tx_write_ptr;

		if (dac_wait_en == 0 && buf_gap >= HIGH_BUF_GAP) {
			dac_wait_en = 1;

			if (0 == wait_event_interruptible_timeout(ssp_wait_q, (buf_gap <= LOW_BUF_GAP), DMA_TIME_OUT)) {
				cs75xx_dma_ssp_tx_ptr(ssp_ctrl.ssp_index, &tx_write_ptr, &tx_read_ptr);
				printk("DMA Tx Timeout(%d, %d)!\n", tx_write_ptr, tx_read_ptr);
				printk("Play out total %d bytes\n", ssp_i2s.out_len);
				count = 0;
				goto END;
			}
		}
	}
END:
	return count;
}

static int ssp_i2s_dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int value, retval;
	count_info inf;

	switch (cmd) {
		int ival, new_format;
		int frag_size, frag_buf;
		struct audio_buf_info info;

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
		//ssp_ctrl.tx_write_ptr = 0;
		//ssp_ctrl.tx_read_ptr = 0;
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
			cs75xx_spdif_clock_start(fileInfo.nSamplesPerSec);
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
#ifdef HAVE_CS4341
		cs4341_hw_init();
#endif
		break;

	case SSP_I2S_INC_LEVEL:
#ifdef HAVE_CS4341
		ssp_i2s.level += 0x0101;
		if (ssp_i2s.level > 0x5e5e)
			ssp_i2s.level = 0x5e5e;

		cs4341_setlevel(ssp_i2s.level >> 0x08, ssp_i2s.level & 0xff);
		value = ssp_i2s.level;
		//printk("SSP_I2S_INC_LEVEL ssp_i2s.level : %x\n",ssp_i2s.level);
#endif
		break;

	case SSP_I2S_DEC_LEVEL:
#ifdef HAVE_CS4341
		if (ssp_i2s.level < 0x0101)
			ssp_i2s.level -= 0;
		else
			ssp_i2s.level -= 0x0101;

		value = ssp_i2s.level;
		cs4341_setlevel(ssp_i2s.level >> 0x08, ssp_i2s.level & 0xff);
		//printk("SSP_I2S_DEC_LEVEL ssp_i2s.level : %x\n",ssp_i2s.level);
#endif
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
		/////////////
	case OSS_GETVERSION:
		printk("%s : OSS_GETVERSION\n", __func__);
		return put_user(SOUND_VERSION, (int *) arg);

	case SNDCTL_DSP_GETCAPS:
		ival = DSP_CAP_BATCH;
		printk("%s : SNDCTL_DSP_GETCAPS\n", __func__);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETFMTS:
		ival = (AFMT_S16_BE | AFMT_S16_LE);	//| AFMT_MU_LAW | AFMT_A_LAW ); //AFMT_S16_BE
		printk("%s : SNDCTL_DSP_GETFMTS\n", __func__);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SETFMT:
		printk("%s : SNDCTL_DSP_SETFMT\n", __func__);

		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("SSP_I2S_DMA SSP_I2S_INIT_BUF(%x)\n", ival);
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
		printk("%s : SOUND_PCM_READ_RATE(%d)\n", __func__, ival);
		ssp_i2s.dac_rate = 44100;
		ival = ssp_i2s.dac_rate;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SPEED:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("%s : SNDCTL_DSP_SPEED(%d)\n", __func__, ival);
		//if (ival < 8000) ival = 8000;
		//if (ival > 44100) ival = 44100;
#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
		if (cs75xx_spdif_clock_start(ival))
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
		printk("%s : SNDCTL_DSP_STEREO(%d)\n", __func__, ival);
		if (ival != 0 && ival != 1)
			return -EINVAL;

		ssp_i2s.stereo_select = ival;
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(ival, (int *) arg))
			return -EFAULT;

		printk("%s : SNDCTL_DSP_CHANNELS(%d)\n", __func__, ival);
		if (ival != 1 && ival != 2) {
			ival = ssp_i2s.stereo_select == SSP_SS_MONO ? 1 : 2;
			return put_user(ival, (int *) arg);
		}
		ssp_i2s.stereo_select = ival - 1;
		return 0;

	case SOUND_PCM_READ_CHANNELS:
		printk("%s : SOUND_PCM_READ_CHANNELS\n", __func__);
		ival = ssp_i2s.stereo_select + 1;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETBLKSIZE:
		ival = ssp_ctrl.buf_num;
		printk("%s : SNDCTL_DSP_GETBLKSIZE(0x%x)\n", __func__, ival);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_NONBLOCK:
		printk("%s : SNDCTL_DSP_NONBLOCK\n", __func__);
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_RESET:
		printk("%s : SNDCTL_DSP_RESET\n", __func__);
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
		printk("%s : SNDCTL_DSP_SETFRAGMENT\n", __func__);
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		frag_size = ival & 0xffff;
		frag_buf = (ival >> 16) & 0xffff;
		/* TODO: We use hardcoded fragment sizes and numbers for now */
		frag_size = 11;	/* 4096 == 2^12 *///ssp_ctrl.buf_num
		frag_buf = ssp_ctrl.buf_num;
		ival = (frag_buf << 16) + frag_size;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETOSPACE:
		//printk("%s : SNDCTL_DSP_GETOSPACE \n",__func__);
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		info.fragstotal = ssp_ctrl.buf_num;
		spin_lock_irqsave(&ssp_lock, flags);;
		info.fragments = (ssp_ctrl.buf_num - ssp_i2s.in_cont);
		spin_unlock_irqrestore(&ssp_lock, flags);
		info.fragsize = ssp_ctrl.buf_num;
		info.bytes = info.fragments * info.fragsize;
		//            printk("[%x-%x-%x]\n",info.bytes,info.fragments,ssp_i2s.in_cont);
		return copy_to_user((void *) arg, &info, sizeof(info)) ? -EFAULT : 0;
		//return 0;

	case SNDCTL_DSP_GETISPACE:
		printk("%s : SNDCTL_DSP_GETISPACE\n", __func__);
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		info.fragstotal = ssp_ctrl.buf_num;
		info.fragments = 20;	// ssp_i2s.nb_filled_record; /*ssp_ctrl.buf_num-*/
		info.fragsize = ssp_ctrl.buf_num;
		info.bytes = info.fragments * info.fragsize;
		return 0;	//copy_to_user((void *)arg, &info, sizeof(info)) ? -EFAULT : 0;

	case SNDCTL_DSP_SYNC:
		printk("%s : SNDCTL_DSP_SYNC\n", __func__);
		ssp_ctrl.ssp_en = cs75xx_ssp_disable(ssp_ctrl.ssp_index);
		return 0;

	case SNDCTL_DSP_SETDUPLEX:
		printk("%s : SNDCTL_DSP_SETDUPLEX\n", __func__);
		return 0;

	case SNDCTL_DSP_POST:
		printk("%s : SNDCTL_DSP_POST\n", __func__);
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		printk("%s : SNDCTL_DSP_GETTRIGGER\n", __func__);
		//PCM_ENABLE_INPUT
		//PCM_ENABLE_OUTPUT
		return 0;

	case SNDCTL_DSP_GETOPTR:
		printk("%s : SNDCTL_DSP_GETOPTR \n", __func__);

		//int bytecount, offset, flags;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;

		inf.blocks = ssp_ctrl.buf_num;
		inf.bytes = ssp_i2s.tol_size;

		return copy_to_user((void *) arg, &inf, sizeof(inf));

	case SNDCTL_DSP_GETIPTR:
		printk("%s : SNDCTL_DSP_GETIPTR \n", __func__);
		//count_info inf = { 0, };

		//int bytecount, offset, flags;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;

		inf.blocks = ssp_ctrl.buf_num;
		inf.bytes = ssp_i2s.tol_size;
		return copy_to_user((void *) arg, &inf, sizeof(inf));

	case SOUND_PCM_READ_BITS:
		printk("%s : SOUND_PCM_READ_BITS\n", __func__);
		return 0;

	case SOUND_PCM_READ_FILTER:
		printk("%s : SOUND_PCM_READ_FILTER\n", __func__);
		return 0;

	case SNDCTL_DSP_SUBDIVIDE:
		printk("%s : SNDCTL_DSP_SUBDIVIDE\n", __func__);
		return 0;

	case SNDCTL_DSP_SETTRIGGER:
		printk("%s : SNDCTL_DSP_SETTRIGGER\n", __func__);
		return 0;

	case SNDCTL_DSP_SETSYNCRO:
		printk("%s : SNDCTL_DSP_SETSYNCRO\n", __func__);
		return 0;

	case SNDCTL_DSP_MAPINBUF:
		printk("%s : SNDCTL_DSP_MAPINBUF\n", __func__);
		return 0;

	case SNDCTL_DSP_MAPOUTBUF:
		printk("%s : SNDCTL_DSP_MAPOUTBUF\n", __func__);
		return 0;
	default:
		printk("%s : unknown cmd %x\n", __func__, cmd);
	}

	return -EINVAL;
}

#if 0
static int gemini_i2s_dma(int byte_swap)
{
	cs75xx_dma_ssp_enable(ssp_ctrl.ssp_index, byte_swap);

	printk("%s :<--\n", __func__);

	//return written;
	return 0;
}
#endif

static struct file_operations ssp_i2s_dsp_fops = {
	.owner          = THIS_MODULE,
	.open           = ssp_i2s_dsp_open,
	.release        = ssp_i2s_dsp_release,
	.write          = ssp_i2s_dsp_write,
	.unlocked_ioctl = ssp_i2s_dsp_ioctl,
};


static struct file_operations ssp_i2s_mixer_fops = {
	.owner          = THIS_MODULE,
	.open           = ssp_i2s_mixer_open,
	.release        = ssp_i2s_mixer_release,
	//.write        = ssp_i2s_mixer_write,
	.unlocked_ioctl = ssp_i2s_mixer_ioctl,
};

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
	ssp_i2s.level = 0;
	ssp_i2s.data_format = SSP_DF_16BITUL_LINEAR;
	ssp_i2s.sample_rate = 44100;

	ssp_ctrl.buf_num = LLP_SIZE;
	ssp_ctrl.buf_size = DBUF_SIZE;
	ssp_ctrl.tbuf = NULL;
	ssp_ctrl.rbuf = NULL;
#ifdef CONFIG_SOUND_DAC_SSP
	ssp_ctrl.ssp_index = 1;
#elif defined(CONFIG_SOUND_D2_45057_SSP)
	ssp_ctrl.ssp_index = 0;
#endif
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
	printk("tbuf = %p, tbuf_paddr = %x\n", ssp_ctrl.tbuf, ssp_ctrl.tbuf_paddr);

	if (ssp_ctrl.tbuf == NULL)
		printk("Can't alloc Tx buffer 0x%x for DMA SSP%d\n", ssp_ctrl.buf_num*ssp_ctrl.buf_size, ssp_ctrl.ssp_index);
	if (!ssp_ctrl.tbuf) {
		return -ENOMEM;
	}
	ssp_i2s.tbuf_curr = ssp_ctrl.tbuf;
	ssp_i2s.tbuf_end = ssp_ctrl.tbuf + ssp_ctrl.buf_num*ssp_ctrl.buf_size;

#if defined(CONFIG_DAC_REF_INTERNAL_CLK) && defined(CONFIG_SOUND_CS75XX_SPDIF)
	cs75xx_spdif_clock_start(44100);
#endif

	/* init ssp and dma */
#ifdef CONFIG_SOUND_DAC_SSP
	ssp_cfg.profile = SSP_I2S_DAC;
#elif defined(CONFIG_SOUND_D2_45057_SSP)
	ssp_cfg.profile = SSP_I2S_D2;
#endif
	ssp_cfg.codec_size = 0;
	ssp_cfg.chan_num = 2;
#ifdef CONFIG_DAC_REF_INTERNAL_CLK
	ssp_cfg.ext_clk = 0;
#else
	ssp_cfg.ext_clk = 1;
#endif
	cs75xx_ssp_register(ssp_ctrl.ssp_index,ssp_cfg);
	cs75xx_dma_ssp_register(ssp_ctrl.ssp_index, ssp_ctrl.tbuf_paddr, ssp_ctrl.buf_num,
	                    ssp_ctrl.buf_size, 0, 0, 0);
	request_cs75xx_dma_ssp_irq(ssp_ctrl.ssp_index, i2s_tx_notify, NULL);

	return 0;
}

int cs4341_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#ifdef HAVE_CS4341
	dac_i2c_client = client;

	cs4341_hw_init();
#endif
	printk("DAC with CS75XX SSP - I2C Initialization(dsp) ");
	printk(".");

	/* register devices */
	if ((audio_dev_id = register_sound_dsp(&ssp_i2s_dsp_fops, -1)) < 0)
		goto fail;
	printk(".");

	if ((mixer_dev_id = register_sound_mixer(&ssp_i2s_mixer_fops, -1)) < 0)
		goto fail;
	printk(".");


	if (dac_ssp_init())
		goto fail;
	printk(".OK!\n");

	return 0;

fail:
	printk(" FAIL!\n");
	if (audio_dev_id >= 0)
		unregister_sound_dsp(audio_dev_id);
	if (mixer_dev_id >= 0)
		unregister_sound_mixer(mixer_dev_id);

	return -EPERM;
}

void cs4341_i2c_remove(struct i2c_client *client)
{
	printk("DAC with CS75XX SSP - I2C cleanup(dsp)\n");

	if (ssp_ctrl.ssp_en)
		cs75xx_ssp_disable(ssp_ctrl.ssp_index);

	free_cs75xx_dma_ssp_irq(ssp_ctrl.ssp_index);
	cs75xx_dma_ssp_unregister(ssp_ctrl.ssp_index);
	cs75xx_ssp_unregister(ssp_ctrl.ssp_index);

	if (ssp_ctrl.tbuf)
		kfree(ssp_ctrl.tbuf);

	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
}

static const struct i2c_device_id cs4341_id[] = {
	{ "cs4341", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs4341_id);

static struct i2c_driver cs4341_i2c_driver = {
	.driver = {
		.name	= "cs4341",
		.owner	= THIS_MODULE,
	},
	.probe		= cs4341_i2c_probe,
	.remove		= __devexit_p(cs4341_i2c_remove),
	.id_table	= cs4341_id,
};

static int __init cs4341_init(void)
{
	int ret;

	ret = i2c_add_driver(&cs4341_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register CS4341 I2C driver: %d\n",
				ret);
	}

	return ret;
}
module_init(cs4341_init);

static void __exit cs4341_exit(void)
{
	i2c_del_driver(&cs4341_i2c_driver);
}
module_exit(cs4341_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DAC with CS75XX SSP driver");

