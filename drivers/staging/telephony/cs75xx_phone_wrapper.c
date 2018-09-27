#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/spi/spi.h>
#include <linux/cs75xx_phone_wrapper.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/gpio_alloc.h>
#include <mach/cs75xx_spi.h>
#include "Le71HR8821_IB_LITE_profiles.c"
#include "../../../sound/soc/cs75xx/cs75xx_snd_sport.h"

/*
   For the 3rd party VoIP solution, it needs a 10ms interrupt to trigger its
   task including SLIC, DSP and SIP. The voice's sampling rate is 8KHz. That
   means 80 samples/10ms. The DMA buffer size must be the multiple of 128bytes.
   The least commom multiple number of 80 and 128 is 640. The 3rd party uses
   linear PCM(2byte-symbol) for soft DSP and SLIC codec. One channel takes 160
   bytes/10ms. Based on the above limitation and request, we always assumes 4
   channels running. If there are only two real channels, the other two channels
   will be dummy and useless. If the codec is wideband linear, we only need two
   real channels without dummy channels.
   */
#define CS75XX_10MS_CHAN_NUM		8
#define CS75XX_10MS_4CHAN_BUF_SIZE	(CS75XX_10MS_CHAN_NUM*160)
#define CS75XX_BUF_NUM			8
#define DIRECT_MAP			1

typedef struct {
	struct device *dev;
	char spi_name[5][SPI_NAME_SIZE];
	struct spi_device *spi[5];
	spinlock_t lock;
	void (*tx_handler)(void);
	void (*rx_handler)(void);
	dma_addr_t tbuf_paddr;
	s8 *tbuf;
	s8 *cur_tbuf;
	dma_addr_t rbuf_paddr;
	s8 *rbuf;
	s8 *cur_rbuf;
	u16 tx_write_ptr;
	u16 rx_read_ptr;
	unsigned int valid_chan_mask;
	u8 ssp_en;
	u8 ssp_index;
	u8 sclk_sel;
	u8 dev_num;
	u8 chan_num;
	u8 slic_reset;
	u8 freq_change;
} phone_wrapper_t;


/*******************************************************************************
 * Internal resource
 ******************************************************************************/
static phone_wrapper_t phone;

#if (CS75XX_10MS_CHAN_NUM == 4)
static unsigned int channel_2byte_maps[][4] = {
	{1, 2, 3, 0},	/* 1 real channel */
	{2, 3, 0, 1},	/* 2 real channels */
	{3, 0, 1, 2},	/* 3 real channels */
	{0, 1, 2, 3}	/* 4 real channels */
};
#endif

//static int clock_sel = -1;
//static unsigned int wrapper_debug = 0;

/*******************************************************************************
 * Misc Wrapper API
 ******************************************************************************/


/*******************************************************************************
 * SLIC Wrapper API
 ******************************************************************************/
int cs_slic_hw_reset(void)
{
	gpio_set_value(phone.slic_reset, 0);
	mdelay(250);
	gpio_set_value(phone.slic_reset, 1);
	mdelay(250);

	return 0;
}
EXPORT_SYMBOL(cs_slic_hw_reset);

unsigned char* cs_slic_profile(unsigned int index)
{
	switch (index) {
	case 0: /* DEV Profile */
		return DEV_PROFILE;
	case 1: /* AC Profile */
		return AC_600R_FXS;
	case 2: /* DC Profile */
		return DC_25MA_CC;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(cs_slic_profile);

unsigned int cs_slic_timeslot(unsigned int chan)
{
#if (DIRECT_MAP == 1)
	return chan * 2;
#else
#if (CS75XX_10MS_CHAN_NUM == 4)
	int i, total_chans;

	total_chans = phone.dev_num * phone.chan_num;

	for (i = 0; i < CS75XX_10MS_CHAN_NUM; i++) {
		if (channel_2byte_maps[total_chans - 1][i] == chan)
			return i * 4 + 2;
	}

	return -1;
#else
	printk("%s: no channel_2byte_maps\n", __func__);
#endif
#endif
}
EXPORT_SYMBOL(cs_slic_timeslot);

extern int cs75xx_spi_fast_transfer(struct spi_device *slave, struct spi_message *m);
int cs_slic_read_byte(unsigned int slic_id, void *rx_param)
{
	struct spi_transfer t = {
		.rx_buf = rx_param,
		.len = 1,
	};
	struct spi_message m = {
		.spi = NULL
	};
	int rc;

	if (rx_param == NULL)
		return -EINVAL;
	if (phone.spi[slic_id] == NULL)
		return -ENXIO;

	m.spi = phone.spi[slic_id];

	INIT_LIST_HEAD(&m.transfers);
	spi_message_add_tail(&t, &m);
	//rc = spi_sync(phone.spi[slic_id], &m);
	rc = cs75xx_spi_fast_transfer(phone.spi[slic_id], &m);

	return (rc >= 0 ? 0 : rc);
}
EXPORT_SYMBOL(cs_slic_read_byte);

int cs_slic_write_byte(unsigned int slic_id, void *tx_param)
{
	struct spi_transfer t = {
		.tx_buf = tx_param,
		.len = 1
	};
	struct spi_message m = {
		.spi = NULL
	};
	int rc;

	m.spi = phone.spi[slic_id];

	if (tx_param == NULL)
		return -EINVAL;
	if (phone.spi[slic_id] == NULL)
		return -ENXIO;

	INIT_LIST_HEAD(&m.transfers);
	spi_message_add_tail(&t, &m);
	//rc = spi_sync(phone.spi[slic_id], &m);
	rc = cs75xx_spi_fast_transfer(phone.spi[slic_id], &m);

	return (rc >= 0 ? 0 : rc);
}
EXPORT_SYMBOL(cs_slic_write_byte);


int cs_slic_rw_word(uint slic_id, void *tx_param,void *rx_param,int len)
{
	
	struct spi_transfer t = {
		.tx_buf = tx_param,
		.rx_buf = rx_param,
		.len = len
	};
	struct spi_message m = {
		.spi = NULL
	};
	int rc;

	m.spi = phone.spi[slic_id];

	if (phone.spi[slic_id] == NULL)
		return -ENXIO;
		
	INIT_LIST_HEAD(&m.transfers);
	spi_message_add_tail(&t, &m);
	rc = spi_sync(phone.spi[slic_id], &m);	

	return (rc >= 0 ? 0 : rc);
}
EXPORT_SYMBOL(cs_slic_rw_word);



/*******************************************************************************
 * PCM Wrapper API
 ******************************************************************************/
int cs_pcm_total_chan(void)
{
	return CS75XX_10MS_CHAN_NUM;
}
EXPORT_SYMBOL(cs_pcm_total_chan);

unsigned int cs_pcm_valid_chan(void)
{
	return phone.valid_chan_mask;
}
EXPORT_SYMBOL(cs_pcm_valid_chan);

char* cs_pcm_tx_buf_req(void)
{
	unsigned int diff, total_size;

	total_size = CS75XX_BUF_NUM * CS75XX_10MS_4CHAN_BUF_SIZE;
	diff = cs75xx_snd_sport_dma_curr_offset(phone.ssp_index, DIR_TX);
	phone.cur_tbuf = phone.tbuf + (diff + CS75XX_10MS_4CHAN_BUF_SIZE) % total_size;

//	if (cs_ssp_debug & PHONE_BUF_ADDR) printk("tbuf=%p\n", phone.cur_tbuf);

	return phone.cur_tbuf;
}
EXPORT_SYMBOL(cs_pcm_tx_buf_req);

void cs_pcm_tx_buf_done(void)
{

}
EXPORT_SYMBOL(cs_pcm_tx_buf_done);

char* cs_pcm_rx_buf_req(void)
{
	unsigned int diff, total_size;

	total_size = CS75XX_BUF_NUM * CS75XX_10MS_4CHAN_BUF_SIZE;
	diff = cs75xx_snd_sport_dma_curr_offset(phone.ssp_index, DIR_RX);
	phone.cur_rbuf = phone.rbuf + (diff + total_size - CS75XX_10MS_4CHAN_BUF_SIZE) % total_size;

//	if (cs_ssp_debug & PHONE_BUF_ADDR) printk("rbuf=%p\n", phone.cur_rbuf);

	return phone.cur_rbuf;
}
EXPORT_SYMBOL(cs_pcm_rx_buf_req);

void cs_pcm_rx_buf_done(void)
{

}
EXPORT_SYMBOL(cs_pcm_rx_buf_done);

int cs_pcm_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&phone.lock, flags);
	phone.ssp_en = 1;
	spin_unlock_irqrestore(&phone.lock, flags);

	return 0;
}
EXPORT_SYMBOL(cs_pcm_enable);

int cs_pcm_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&phone.lock, flags);
	phone.ssp_en = 0;
	spin_unlock_irqrestore(&phone.lock, flags);

	return 0;
}
EXPORT_SYMBOL(cs_pcm_disable);

static void phone_wrapper_tx_handler(void *data)
{
	unsigned long flags;
	u16 freq_change;

	spin_lock_irqsave(&phone.lock, flags);
	freq_change = phone.freq_change;
	spin_unlock_irqrestore(&phone.lock, flags);
	
	phone.tx_write_ptr = (phone.tx_write_ptr + 1) % CS75XX_BUF_NUM;
        
	if(freq_change == 0)
		cs75xx_snd_sport_dma_update(phone.ssp_index, DIR_TX, phone.tx_write_ptr);
	
        
	if (phone.ssp_en)
		phone.tx_handler();
	
}

static void phone_wrapper_rx_handler(void *data)
{
	unsigned long flags;
	u16 freq_change;

	spin_lock_irqsave(&phone.lock, flags);
	freq_change = phone.freq_change;
	spin_unlock_irqrestore(&phone.lock, flags);
	
	phone.rx_read_ptr = (phone.rx_read_ptr + 1) % CS75XX_BUF_NUM;
        
	if(freq_change == 0)
		cs75xx_snd_sport_dma_update(phone.ssp_index, DIR_RX, phone.rx_read_ptr);
        
	if (phone.ssp_en)
		phone.rx_handler();
	
}

int cs_pcm_init(void (*tx_handler)(void), void (*rx_handler)(void))
{
	unsigned int index, signal_fmt, tx_mask, rx_mask;
	int total_chans, size, endian, slots, width, chans;
	int i;

	dev_dbg(phone.dev, "%s\n", __func__);
	
	if ((tx_handler == NULL) || (rx_handler == NULL)) {
		dev_err(phone.dev, "tx_handler() and rx_handler() are necessary!\n");
		return -1;
	}
	phone.tx_handler = tx_handler;
	phone.rx_handler = rx_handler;

	phone.valid_chan_mask = 0;
	total_chans = phone.dev_num * phone.chan_num;
#if (DIRECT_MAP == 1)
	for (i = 0; i < total_chans; i++)
		phone.valid_chan_mask |= BIT(i);
#else
#if (CS75XX_10MS_CHAN_NUM == 4)
	for (i = 0; i < CS75XX_10MS_CHAN_NUM; i++) {
		if (channel_2byte_maps[total_chans - 1][i] < total_chans)
			phone.valid_chan_mask |= BIT(i);
	}
#else
	printk("%s: no channel_2byte_maps\n", __func__);
#endif
#endif
	index = phone.ssp_index;
	/* For Zarlink SLIC, PCLK >= 1024K(=16 slots), linear PCM codec */
	slots = 16;
	width = 8;	/* slot width - bits */
	size = 2;	/* codec size - bytes */
	endian = 0;
	chans = CS75XX_10MS_CHAN_NUM;

	if (cs75xx_snd_sport_if_register(phone.ssp_index, CS75XX_SPORT_PCM, 1, 1))
		return -EPERM;

	signal_fmt = SPORT_DATA_DSP_A | SPORT_CLOCK_GATED | \
		    SPORT_SIGNAL_IB_IF | SPORT_CLOCK_CBS_CFS;
	if (cs75xx_snd_sport_signal_formt(index, signal_fmt))
		return -EINVAL;

	if (cs75xx_snd_sport_audio_formt(index, DIR_TX, size, endian))
		return -EINVAL;
	if (cs75xx_snd_sport_audio_formt(index, DIR_RX, size, endian))
		return -EINVAL;

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
	tx_mask = rx_mask = 0;
	for (i = 0; i < chans; i++) {
#if (DIRECT_MAP == 1)
		tx_mask |= BIT(i*2);
		tx_mask |= BIT(i*2 + 1);
#else
#if (CS75XX_10MS_CHAN_NUM == 4)
		tx_mask |= BIT(slots - (chans-i)*4 + (4-size));
		if (size == 2)
			tx_mask |= BIT(slots - (chans-i)*4 + (4-size) + 1);
#else
		printk("%s: no mask rule\n", __func__);
#endif
#endif
	}
	rx_mask = tx_mask;
	if (cs75xx_snd_sport_slots(index, tx_mask, rx_mask, slots, width))
		return -EINVAL;

	if (cs75xx_snd_sport_clkdiv(index, phone.sclk_sel, 8000*slots*width))
		return -EINVAL;

	cs75xx_snd_sport_dma_register(index, DIR_TX, phone.tbuf_paddr, \
	                              CS75XX_BUF_NUM, CS75XX_10MS_4CHAN_BUF_SIZE);
	cs75xx_snd_sport_dma_callback(index, DIR_TX, phone_wrapper_tx_handler, NULL);

	cs75xx_snd_sport_dma_register(index, DIR_RX, phone.rbuf_paddr, \
	                              CS75XX_BUF_NUM, CS75XX_10MS_4CHAN_BUF_SIZE);
	cs75xx_snd_sport_dma_callback(index, DIR_RX, phone_wrapper_rx_handler, NULL);

	phone.rx_read_ptr = 0;
	phone.tx_write_ptr = 2;
	cs75xx_snd_sport_dma_update(index, DIR_TX, phone.tx_write_ptr);

	cs75xx_snd_sport_start(index, DIR_RX);
	cs75xx_snd_sport_start(index, DIR_TX);

	return 0;
}
EXPORT_SYMBOL(cs_pcm_init);

int cs_pcm_exit(void)
{
	unsigned int index = phone.ssp_index;

	cs75xx_snd_sport_stop(index, DIR_TX);
	cs75xx_snd_sport_stop(index, DIR_RX);

	cs75xx_snd_sport_dma_callback(index, DIR_TX, NULL, NULL);
	cs75xx_snd_sport_dma_unregister(index, DIR_TX);

	cs75xx_snd_sport_dma_callback(index, DIR_RX, NULL, NULL);
	cs75xx_snd_sport_dma_unregister(index, DIR_RX);

	cs75xx_snd_sport_clkdiv(index, SPORT_SPDIF_CLK_DIV, 0);

	cs75xx_snd_sport_if_unregister(index);
 
	return 0;
}
EXPORT_SYMBOL(cs_pcm_exit);

int cs_pcm_dma_start(void)
{
	unsigned int index = phone.ssp_index;
	unsigned long flags;

	spin_lock_irqsave(&phone.lock, flags);
	phone.freq_change = 0;
	spin_unlock_irqrestore(&phone.lock, flags);
	
	cs75xx_pwr_dma_start(index);

	
	return 0;
}
EXPORT_SYMBOL(cs_pcm_dma_start);

int cs_pcm_dma_stop(void)
{

	unsigned int index = phone.ssp_index;
	unsigned long flags;

	spin_lock_irqsave(&phone.lock, flags);
	phone.freq_change = 1;
	spin_unlock_irqrestore(&phone.lock, flags);

	cs75xx_pwr_dma_stop(index);


	
	return 0;
}
EXPORT_SYMBOL(cs_pcm_dma_stop);

/*******************************************************************************
 *
 ******************************************************************************/
static int phone_wrapper_spi_probe(struct spi_device *spi);
static int phone_wrapper_control_init(void);
static int phone_wrapper_control_exit(void);
static int phone_wrapper_data_init(void);
static int phone_wrapper_data_exit(void);
static struct spi_driver phone_wrapper_spi_driver[5];

static int phone_wrapper_spi_probe(struct spi_device *spi)
{
	int i, ret;

	dev_dbg(phone.dev, "%s\n", __func__);

#ifdef CONFIG_PHONE_CS75XX_SPI_16BITS_PERWORD
	spi->bits_per_word = 16;
#else
	spi->bits_per_word = 8;	
#endif	
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(phone.dev, "spi_setup() fail(%d)!\n", ret);
		return ret;
	}

	for (i = 0; i < phone.dev_num; i++) {
		if (!strncmp(spi->modalias, phone_wrapper_spi_driver[i].driver.name,
						strlen(spi->modalias))) {
			phone.spi[i] = spi;
			return 0;
		}
	}

	return -ENODEV;
}

typedef struct slic_info{
	char spi_name[SPI_NAME_SIZE];
}slic_info_t;

const slic_info_t slic_name[] = {
#ifdef CONFIG_SLIC_SI3226X_SLOT0
	{.spi_name = "si3226x_slot0"},
#endif
#ifdef CONFIG_SLIC_SI3226X_SLOT1
	{.spi_name = "si3226x_slot1"},
#endif
#ifdef CONFIG_SLIC_VE880_SLOT0
	{.spi_name = "ve880_slot0"},
#endif
#ifdef CONFIG_SLIC_VE880_SLOT1
	{.spi_name = "ve880_slot1"},
#endif
};

static int phone_wrapper_control_init(void)
{
	int i,rlt;

	dev_dbg(phone.dev, "%s\n", __func__);

	if (gpio_request(phone.slic_reset, "SLIC_RESET")) {
		dev_err(phone.dev, "can't reserve GPIO %d\n", phone.slic_reset);
		return -1;
	}
	gpio_direction_output(phone.slic_reset, 1);

	memset(phone_wrapper_spi_driver, 0, ARRAY_SIZE(phone_wrapper_spi_driver));
	
	for (i = 0; i < phone.dev_num; i++) {
		sprintf(phone.spi_name[i], "%s",slic_name[i].spi_name);
		phone_wrapper_spi_driver[i].driver.name = phone.spi_name[i];
		phone_wrapper_spi_driver[i].driver.owner = THIS_MODULE;
		phone_wrapper_spi_driver[i].probe = phone_wrapper_spi_probe;
		printk("phone_wrapper_control_init:%s \r\n",slic_name[i].spi_name);
	}

	for (i = 0; i < phone.dev_num; i++){
		rlt = spi_register_driver(&phone_wrapper_spi_driver[i]);
		if(rlt){
			printk("spi_register_driver fail \r\n");			
		}
	}

	for (i = 0; i < phone.dev_num; i++) {
		if (phone.spi[i] == NULL)
			phone_wrapper_control_exit();
	}

	return 0;
}

static int phone_wrapper_control_exit(void)
{
	int i;

	dev_dbg(phone.dev, "%s\n", __func__);

	gpio_free(phone.slic_reset);

	for (i = 0; i < phone.dev_num; i++)
		spi_unregister_driver(&phone_wrapper_spi_driver[i]);

	return 0;
}

static int phone_wrapper_data_init(void)
{
	int total_bufsize = CS75XX_BUF_NUM * CS75XX_10MS_4CHAN_BUF_SIZE;

	dev_dbg(phone.dev, "%s\n", __func__);

	phone.tbuf = (u8 *)dma_alloc_coherent(NULL, total_bufsize,
				&phone.tbuf_paddr, GFP_KERNEL|GFP_DMA);
	if (phone.tbuf == NULL) {
		dev_err(phone.dev, "can't alloc DMA tx buffer 0x%x\n", total_bufsize);
		return -ENOMEM;
	} else {
		memset(phone.tbuf, 0, total_bufsize);
	}

	phone.rbuf = (u8 *)dma_alloc_coherent(NULL, total_bufsize,
				&phone.rbuf_paddr, GFP_KERNEL|GFP_DMA);
	if (phone.rbuf == NULL) {
		dev_err(phone.dev, "can't alloc DMA rx buffer 0x%x\n", total_bufsize);
		dma_free_coherent(NULL, total_bufsize, phone.tbuf, phone.tbuf_paddr);
		return -ENOMEM;
	} else {
		memset(phone.rbuf, 0, total_bufsize);
	}
	dev_dbg(phone.dev, "tbuf = 0x%p, rbuf = 0x%p\n", phone.tbuf, phone.rbuf);

	return 0;
}

static int phone_wrapper_data_exit(void)
{
	int total_bufsize = CS75XX_BUF_NUM * CS75XX_10MS_4CHAN_BUF_SIZE;

	dev_dbg(phone.dev, "%s\n", __func__);

	dma_free_coherent(NULL, total_bufsize, phone.tbuf, phone.tbuf_paddr);
	dma_free_coherent(NULL, total_bufsize, phone.rbuf, phone.rbuf_paddr);

	return 0;
}

static int phone_wrapper_misc_init(void)
{
	dev_dbg(phone.dev, "%s\n", __func__);

	spin_lock_init(&phone.lock);

	return 0;
}

static int phone_wrapper_misc_exit(void)
{
	dev_dbg(phone.dev, "%s\n", __func__);

	/* oneshot timer */
	free_irq(GOLDENGATE_IRQ_TIMER0, &phone);

	return 0;
}

static int phone_wrapper_resource_init(void)
{
	dev_dbg(phone.dev, "%s\n", __func__);

	if (phone_wrapper_control_init())
		return -ENXIO;

	if (phone_wrapper_data_init()) {
		phone_wrapper_control_exit();
		return -ENXIO;
	}

	if (phone_wrapper_misc_init()) {
		phone_wrapper_control_exit();
		phone_wrapper_data_exit();
		return -ENXIO;
	}

	return 0;
}

static int phone_wrapper_resource_exit(void)
{
	dev_dbg(phone.dev, "%s\n", __func__);

	phone_wrapper_control_exit();
	phone_wrapper_data_exit();
	phone_wrapper_misc_exit();

	return 0;
}

static int __devinit phone_wrapper_probe(struct platform_device *pfdev)
{
	char res_name[16];
	int res_param;

	phone.dev = &pfdev->dev;

	dev_info(phone.dev, "%s: %s-%s\n", __func__, __DATE__, __TIME__);

	memset(&phone, 0, sizeof(phone));

	sprintf(res_name, "ssp_index");
	res_param = platform_get_irq_byname(pfdev, res_name);
	if (res_param == -ENXIO) {
		dev_err(phone.dev, "can't get resource %s\n", res_name);
		goto fail;
	}
	phone.ssp_index = res_param;

	sprintf(res_name, "sclk_sel");
	res_param = platform_get_irq_byname(pfdev, res_name);
	if (res_param == -ENXIO) {
		dev_err(phone.dev, "can't get resource %s\n", res_name);
		goto fail;
	}
	phone.sclk_sel = res_param;

	sprintf(res_name, "dev_num");
	res_param = platform_get_irq_byname(pfdev, res_name);
	if (res_param == -ENXIO) {
		dev_err(phone.dev, "can't get resource %s\n", res_name);
		goto fail;
	}
	phone.dev_num = res_param;

	sprintf(res_name, "chan_num");
	res_param = platform_get_irq_byname(pfdev, res_name);
	if (res_param == -ENXIO) {
		dev_err(phone.dev, "can't get resource %s\n", res_name);
		goto fail;
	}
	phone.chan_num = res_param;

	sprintf(res_name, "slic_reset");
	res_param = platform_get_irq_byname(pfdev, res_name);
	if (res_param == -ENXIO) {
		dev_err(phone.dev, "can't get resource %s\n", res_name);
		goto fail;
	}
	phone.slic_reset = res_param;

	if ((phone.dev_num * phone.chan_num) > CS75XX_10MS_CHAN_NUM) {
		dev_err(phone.dev, "only supports up to %d channels!\n", CS75XX_10MS_CHAN_NUM);
		goto fail;
	}

	dev_dbg(phone.dev, "ssp_index = %d\n", phone.ssp_index);
	dev_dbg(phone.dev, "sclk_sel = %d\n", phone.sclk_sel);
	dev_dbg(phone.dev, "dev_num = %d\n", phone.dev_num);
	dev_dbg(phone.dev, "chan_num = %d\n", phone.chan_num);
	dev_dbg(phone.dev, "slic_reset = %d\n", phone.slic_reset);

	if (phone_wrapper_resource_init())
		goto fail;

	return 0;

fail:
	return -ENXIO;
}

static int __devexit phone_wrapper_remove(struct platform_device *dev)
{
	phone_wrapper_resource_exit();

	return 0;
}

static struct platform_driver phone_wrapper_driver = {
	.probe	= phone_wrapper_probe,
	.remove	= __devexit_p(phone_wrapper_remove),
	.driver = {
		.name = "phone_wrapper",
		.owner = THIS_MODULE,
	},
};

static int __init phone_wrapper_init(void)
{
	return platform_driver_register(&phone_wrapper_driver);
}
module_init(phone_wrapper_init);

static void __exit phone_wrapper_exit(void)
{
	platform_driver_unregister(&phone_wrapper_driver);
}
module_exit(phone_wrapper_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX Phone Wrapper");

