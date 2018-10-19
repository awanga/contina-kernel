/*
 * FILE NAME spi_cs75xx.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX SPI controller
 *
 *  Copyright 2013 Cortina Systems, Corp.
 *
 *  Based on spi_stmp.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
//#define SPI_INT_MODE

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#ifdef SPI_INT_MODE
#include <linux/sched.h>
#endif
#include <mach/cs75xx_spi.h>
#include <mach/platform.h>

/* Register Map */
#define CS75XX_SPI_CLK		0x00
#define CS75XX_SPI_CFG		0x04
#define CS75XX_SPI_CTRL		0x08
#define CS75XX_SPI_CA0		0x0C
#define CS75XX_SPI_CA1		0x10
#define CS75XX_SPI_CA2		0x14
#define CS75XX_SPI_WDAT1	0x18
#define CS75XX_SPI_WDAT0	0x1C
#define CS75XX_SPI_RDAT1	0x20
#define CS75XX_SPI_RDAT0	0x24
#define CS75XX_SPI_IE0		0x28
#define CS75XX_SPI_INT0		0x2C
#define CS75XX_SPI_IE1		0x30
#define CS75XX_SPI_INT1		0x34
#define CS75XX_SPI_STAT		0x38
#define CS75XX_SPI_MODE		0x20C

#define SPI_CTRL_SSPDONE	BIT(0)
#define SPI_INTR_INT		BIT(0)

#define	CFG_CMD_NORMAL		0
#define CFG_CMD_ONLY		1
#define CFG_CMD_WRITE		0
#define CFG_CMD_READ		1
#define CFG_SPI			0
#define CFG_MWR			1
#define CFG_CS1			2

struct cs75xx_spi {
	int		id;

	void *  __iomem regs;	/* vaddr of the control registers */

	u32		speed_khz;
	u32		divider;
	u32		timeout;

	struct device	*master_dev;

#ifdef SPI_INT_MODE
	int		irq;
	u32		block;
	wait_queue_head_t waitq;
#endif
	struct work_struct work;
	struct workqueue_struct *workqueue;

	/* lock protects queue access */
	spinlock_t lock;
	struct list_head queue;

	struct completion done;
};

#define busy_wait(cond, timeout)					\
	({								\
	unsigned long end_jiffies = jiffies + timeout;			\
	bool succeeded = false;						\
	do {								\
		if (cond) {						\
			succeeded = true;				\
			break;						\
		}							\
		cpu_relax();						\
	} while ((long)jiffies - (long)end_jiffies < 0);		\
	succeeded;							\
	})


#ifdef SPI_INT_MODE
static irqreturn_t
cs75xx_spi_intr(int irq, void *param)
{
	struct cs75xx_spi *controller = param;
	irqreturn_t rc = IRQ_NONE;

	/* disable spi interrupt */
	writel(0, (controller->regs + CS75XX_SPI_IE0));
	writel(SPI_INTR_INT, (controller->regs + CS75XX_SPI_INT0));

	controller->block = readl(controller->regs + CS75XX_SPI_CTRL) & SPI_CTRL_SSPDONE ? 0 : 1;
	wake_up_interruptible(&controller->waitq);

	rc = IRQ_HANDLED;

	/* enable spi interrupt */
	writel(SPI_INTR_INT, (controller->regs + CS75XX_SPI_IE0));

	return rc;
}
#endif

static int cs75xx_spi_setup_transfer(struct spi_device *slave,
		struct spi_transfer *t)
{
	u32 hz;
	struct cs75xx_spi *controller = spi_master_get_devdata(slave->master);
	PER_SPI_CLK_t reg_clk;
	PER_SPI_CFG_t reg_cfg;

	/*
	 * Calculate speed:
	 *	- by default, use maximum speed from ssp clk
	 *	- if device overrides it, use it
	 *	- if transfer specifies other speed, use transfer's one
	 */
	hz = 1000 * controller->speed_khz / controller->divider;
	if (slave->max_speed_hz)
		hz = min(hz, slave->max_speed_hz);
	if (t && t->speed_hz)
		hz = min(hz, t->speed_hz);

	if (hz == 0) {
		dev_err(&slave->dev, "Cannot continue with zero clock\n");
		return -EINVAL;
	}

	/*
	dev_dbg(&slave->dev, "Requested clk rate = %uHz, max = %uHz/%d = %uHz\n",
		hz, controller->speed_khz * 1000, controller->divider,
		controller->speed_khz * 1000 / controller->divider);
	*/

	if (controller->speed_khz * 1000 / controller->divider < hz) {
		dev_err(&slave->dev, "%s, unsupported clock rate %uHz\n",
			__func__, hz);
		return -EINVAL;
	}

	reg_clk.wrd = readl(controller->regs + CS75XX_SPI_CLK);
	reg_clk.bf.ssp_igap = 0;
	reg_clk.bf.counter_load = ((controller->speed_khz * 1000) / (2 * hz) - 1);
	writel(reg_clk.wrd, controller->regs + CS75XX_SPI_CLK);

	reg_cfg.wrd = readl(controller->regs + CS75XX_SPI_CFG);
	reg_cfg.bf.micro_wire_cs_sel = 0;
	reg_cfg.bf.sel_ssp_cs = 0x01 << slave->chip_select;
	writel(reg_cfg.wrd, controller->regs + CS75XX_SPI_CFG);


	/* Bad Code, Should be momdified ... */
	writel(slave->mode, PER_SPI_MODE);


	return 0;
}

static int cs75xx_spi_setup(struct spi_device *slave)
{
	/*
	 * spi_setup() does basic checks,
	 * cs75xx_spi_setup_transfer() does more later,
	 * hardware support TX wordsize 8 ~ 160 bits and RX wordsize to 8 ~ 64 bits,
	 * to simplify driver, only implement TX and RX wordsize 1 ~ 8 bytes
	 */
	if ((slave->bits_per_word < 8) ||
		(slave->bits_per_word > 64) ||
		((slave->bits_per_word % 8) != 0)) {
		dev_err(&slave->dev, "%s, unsupported bits_per_word=%d\n",
			__func__, slave->bits_per_word);
		return -EINVAL;
	}
	return 0;
}

static int cs75xx_spi_txrx_msg(struct cs75xx_spi *controller, struct spi_device *slave,
		void *dout, int bitout, void *din, int bitin)
{
	PER_SPI_CFG_t 	spi_cfg;
	PER_SPI_CTRL_t 	spi_ctrl;
#ifdef SPI_INT_MODE
	long time_left;
#endif
	unsigned char	*buf8;
	unsigned int 	bytein;
	unsigned int 	byteout;
	unsigned int	value;
	unsigned int 	i;

	byteout = bitout / 8;
	bytein = bitin / 8;

	spi_cfg.wrd = readl(controller->regs + CS75XX_SPI_CFG);
	if (bitin) {	/* read command */
		spi_cfg.bf.command_cyc = CFG_CMD_NORMAL;
		spi_cfg.bf.read_write = CFG_CMD_READ;

		if(!bitout)
			spi_cfg.bf.ssp_cmd_cnt = 0;
		else
			spi_cfg.bf.ssp_cmd_cnt = bitout - 1;

		spi_cfg.bf.pre_ssp_dat_cnt = bitin - 1;
	} else {	/* write command */
		spi_cfg.bf.command_cyc = CFG_CMD_ONLY;
		spi_cfg.bf.read_write = CFG_CMD_WRITE;
		if(!bitout)
			spi_cfg.bf.ssp_cmd_cnt = 0;
		else
			spi_cfg.bf.ssp_cmd_cnt = bitout - 1;
		spi_cfg.bf.pre_ssp_dat_cnt = 0;
	}

	writel(spi_cfg.wrd, controller->regs + CS75XX_SPI_CFG);

	if (dout) {
		buf8 = (u8 *)dout;
		for (i = 0; i < byteout; i++) {
			if ((i % 4) == 0)
				value = 0;

			value |= ((u32)(*buf8)) << (24 - 8 * (i % 4));
			buf8++;

			if ((i & 3) == 3)
				writel(value, controller->regs + CS75XX_SPI_CA0 + 4 * (i / 4));
		}
		if (i % 4)
			writel(value, controller->regs + CS75XX_SPI_CA0 + 4 * (i / 4));
	}

	/* Run and Transfer! */
	spi_ctrl.bf.sspstart = 1;
	writel(spi_ctrl.wrd, controller->regs + CS75XX_SPI_CTRL);

#ifdef SPI_INT_MODE
	controller->block = 1;

	time_left = wait_event_interruptible_timeout(controller->waitq,
						!controller->block, 10);
	if (time_left <= 0 && controller->block) {
		break;
	}
#else
	if (!busy_wait(readl(controller->regs + CS75XX_SPI_CTRL) &
			SPI_CTRL_SSPDONE, controller->timeout)) {
		dev_err(&slave->dev, "spi %s len timeout(%d)\n", din?
			 "rx" : "tx", controller->timeout);
	}
#endif
	writel(SPI_CTRL_SSPDONE, controller->regs + CS75XX_SPI_CTRL);

	if (din) {
		buf8 = (u8 *)din;
		for (i = 0; i < bytein; i++) {
			if ((i % 4) == 0)
				value = readl(controller->regs + CS75XX_SPI_RDAT0 - 4 * (i / 4));
			*buf8 = (value >> (i % 4) * 8) & 0xFF;
			buf8++;
		}
	}

	return (0);
}

static int cs75xx_spi_txrx(struct cs75xx_spi *controller, struct spi_device *slave,
		void *buf, int len, bool first, bool last, bool write, u8 bits_per_word)
{
	PER_SPI_CFG_t reg_cfg;
	PER_SPI_CTRL_t reg_ctrl;
	u8 *buf8 = (u8 *)buf;
	u16 *buf16 = (u16 *)buf;
	u32 *buf32 = (u32 *)buf;
	u64 *buf64 = (u64 *)buf;
	u32 value;
	int i, wsize;
#ifdef SPI_INT_MODE
	long time_left;
#endif



	wsize = bits_per_word / 8;


	if (len % wsize) {
		dev_err(&slave->dev, "%s - len %d byte not match bits_per_word %d!\n",
						__func__, len, slave->bits_per_word);
		return -EINVAL;
	}

	reg_cfg.wrd = readl(controller->regs + CS75XX_SPI_CFG);
	if (write) {
		reg_cfg.bf.command_cyc = 1;
		reg_cfg.bf.read_write = 0;
		reg_cfg.bf.ssp_cmd_cnt = bits_per_word - 1;
		reg_cfg.bf.pre_ssp_dat_cnt = 0;
	} else {
		reg_cfg.bf.command_cyc = 0;
		reg_cfg.bf.read_write = 1;
		reg_cfg.bf.ssp_cmd_cnt = 0;
		reg_cfg.bf.pre_ssp_dat_cnt = bits_per_word - 1;
	}
	writel(reg_cfg.wrd, controller->regs + CS75XX_SPI_CFG);

	while (len) {
		if (write) {
			for (i = 0; i < wsize; i++) {
				if ((i % 4) == 0)
					value = 0;

				value |= ((u32)(*buf8)) << (24 - 8 * (i % 3));
				buf8++;

				if ((i & 3) == 3)
					writel(value, controller->regs + CS75XX_SPI_CA0 + 4 * (i / 4));
			}
			if (i % 4)
				writel(value, controller->regs + CS75XX_SPI_CA0 + 4 * (i / 4));

		}

		/* Run and Transfer! */
		reg_ctrl.bf.sspstart = 1;
		writel(reg_ctrl.wrd, controller->regs + CS75XX_SPI_CTRL);

#ifdef SPI_INT_MODE
		controller->block = 1;

		time_left = wait_event_interruptible_timeout(controller->waitq,
							!controller->block, 10);
		if (time_left <= 0 && controller->block) {
			break;
		}
#else
		if (!busy_wait(readl(controller->regs + CS75XX_SPI_CTRL) &
				SPI_CTRL_SSPDONE, controller->timeout)) {
			dev_err(&slave->dev, "spi %s len timeout(%d)\n", write?
				 "tx" : "rx", controller->timeout);
			break;
		}
#endif
		writel(SPI_CTRL_SSPDONE, controller->regs + CS75XX_SPI_CTRL);

		if (!write) {
			for (i = 0; i < wsize; i++) {
				if ((i % 4) == 0)
					value = readl(controller->regs + CS75XX_SPI_RDAT0 - 4 * (i / 4));

				*buf8 = (value >> (i % 4) * 8) & 0xFF;
				buf8++;
			}
		}

		len -= wsize;
	}

	return len == 0 ? 0 : -ETIMEDOUT;
}

static int cs75xx_spi_handle_message(struct cs75xx_spi *controller, struct spi_message *m)
{
	bool first, last;
	struct spi_transfer *t, *tmp_t;
	int status = 0;
	static u32 speed_hz = 0;
	static u8 bits_per_word = 0;
	u32 t_speed_hz;
	u8 t_bits_per_word;
	void		*tx_buf = NULL, *rx_buf = NULL;
	unsigned int	tx_bits = 0, rx_bits = 0;

	list_for_each_entry_safe(t, tmp_t, &m->transfers, transfer_list) {
		first = (&t->transfer_list == m->transfers.next);
		last = (&t->transfer_list == m->transfers.prev);

		t_speed_hz = t->speed_hz ? : m->spi->max_speed_hz;
		t_bits_per_word = t->bits_per_word ? : m->spi->bits_per_word;

		cs75xx_spi_setup_transfer(m->spi, t);
		speed_hz = t_speed_hz;

		/* reject "not last" transfers which request to change cs */
		/* if (t->cs_change && !last) {
			dev_err(&m->spi->dev,
				"Message with t->cs_change has been skipped\n");
			continue;
		} */

		if (t->tx_buf) {
			tx_buf = (void *)t->tx_buf;
			tx_bits = t->len * 8;
		}
		if (t->rx_buf) {
			rx_buf = (void *)t->rx_buf;
			rx_bits = t->len * 8;
		}

		if (t->cs_change && !last) {
			status = cs75xx_spi_txrx_msg(controller,
				m->spi, tx_buf,	tx_bits, rx_buf, rx_bits);
		}
	}
	status = cs75xx_spi_txrx_msg(controller, m->spi, tx_buf,
			tx_bits, rx_buf, rx_bits);
	return status;
}

/**
 * cs75xx_spi_handle - handle messages from the queue
 */
static void cs75xx_spi_handle(struct work_struct *w)
{
	struct cs75xx_spi *controller = container_of(w, struct cs75xx_spi, work);
	struct spi_message *m;
	unsigned long flags;

	spin_lock_irqsave(&controller->lock, flags);
	while (!list_empty(&controller->queue)) {
		m = list_entry(controller->queue.next, struct spi_message, queue);
		list_del_init(&m->queue);

		m->status = cs75xx_spi_handle_message(controller, m);
		m->complete(m->context);
	}
	spin_unlock_irqrestore(&controller->lock, flags);

	return;
}

/**
 * cs75xx_spi_fast_transfer - perform SHORT message transfer.
 * Called directly from slave_rx/slave_rx and send message to
 * spi_handle_message. This doesn't flow Linux standard SPI
 * flow. This SHOULD NOT be called to send long message.
 * @spi: spi device
 * @m: message to be queued
 */
int cs75xx_spi_fast_transfer(struct spi_device *slave, struct spi_message *m)
{
	struct cs75xx_spi *controller = spi_master_get_devdata(slave->master);
	unsigned long flags;

	m->status = -EINPROGRESS;
	spin_lock_irqsave(&controller->lock, flags);
	m->status = cs75xx_spi_handle_message(controller, m);
	spin_unlock_irqrestore(&controller->lock, flags);

	return 0;
}
EXPORT_SYMBOL(cs75xx_spi_fast_transfer);

/**
 * cs75xx_spi_transfer - perform message transfer.
 * Called indirectly from spi_async, queues all the messages to
 * spi_handle_message.
 * @spi: spi device
 * @m: message to be queued
 */
static int cs75xx_spi_transfer(struct spi_device *slave, struct spi_message *m)
{
	struct cs75xx_spi *controller = spi_master_get_devdata(slave->master);
	unsigned long flags;

	m->status = -EINPROGRESS;
	spin_lock_irqsave(&controller->lock, flags);
	list_add_tail(&m->queue, &controller->queue);
	queue_work(controller->workqueue, &controller->work);
	spin_unlock_irqrestore(&controller->lock, flags);

	return 0;
}

static int __devinit cs75xx_spi_probe(struct platform_device *dev)
{
	int err = 0;
	struct spi_master *master;
	struct cs75xx_spi *controller;
	struct cs75xx_spi_info *pdata = dev->dev.platform_data;
	struct resource *r;

	master = spi_alloc_master(&dev->dev, sizeof(struct cs75xx_spi));
	if (master == NULL) {
		err = -ENOMEM;
		goto out0;
	}
	master->flags = 0; //SPI_MASTER_HALF_DUPLEX;

	controller = spi_master_get_devdata(master);
	platform_set_drvdata(dev, master);

	/* Get resources(memory, IRQ) associated with the device */
	r = platform_get_resource(dev, IORESOURCE_IO, 0);
	if (r == NULL) {
		err = -ENODEV;
		goto out_put_master;
	}
	controller->regs = ioremap(r->start, resource_size(r));
	if (!controller->regs) {
		err = -EINVAL;
		goto out_put_master;
	}

	if (dev->id < 0) /* only one SPI controller */
		controller->id = 0;
	else
		controller->id = dev->id;
	controller->master_dev = &dev->dev;
	controller->speed_khz = pdata->tclk / 1000;
	controller->divider = pdata->divider;
	controller->timeout = pdata->timeout;

	INIT_WORK(&controller->work, cs75xx_spi_handle);
	INIT_LIST_HEAD(&controller->queue);
	spin_lock_init(&controller->lock);

	controller->workqueue = create_singlethread_workqueue(dev_name(&dev->dev));
	if (!controller->workqueue) {
		err = -ENXIO;
		goto out_put_master;
	}
#ifdef SPI_INT_MODE
	controller->irq = platform_get_irq(dev, 0);
	if (controller->irq == -ENXIO) {
		err = -ENXIO;
		goto out_put_master;
	}
	init_waitqueue_head(&controller->waitq);
	if (request_irq(controller->irq, cs75xx_spi_intr, 0,
			CS75XX_SPI_CTLR_NAME, controller)) {
		dev_err(&dev->dev, "CS75XX: Can't register intr handler irq: %d\n",
			controller->irq);
		err = -EINVAL;
		goto out_put_master;
	}
#endif

	master->transfer = cs75xx_spi_fast_transfer;
	master->setup = cs75xx_spi_setup;

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = 0x0F; //SPI_CPOL | SPI_CPHA;

	master->bus_num = controller->id;
	master->num_chipselect = CS75XX_SPI_NUM_CHIPSELECT;

	err = spi_register_master(master);
	if (err) {
		dev_dbg(&dev->dev, "cannot register spi master, %d\n", err);
		goto out_put_master;
	}
	dev_info(&dev->dev, "at (mapped) 0x%08X, bus %d\n",
			(u32)controller->regs, master->bus_num);

#ifdef SPI_INT_MODE
	writel(SPI_INTR_INT, controller->regs + CS75XX_SPI_IE0);
#endif

	return 0;

out_put_master:
#ifdef SPI_INT_MODE
	if ((controller->irq != 0) && (controller->irq == -ENXIO))
		free_irq(controller->irq);
#endif
	if (controller->workqueue)
		destroy_workqueue(controller->workqueue);
	if (controller->regs)
		iounmap(controller->regs);
	platform_set_drvdata(dev, NULL);
	spi_master_put(master);
out0:

	return err;
}

static int __devexit cs75xx_spi_remove(struct platform_device *dev)
{
	struct cs75xx_spi *controller;
	struct spi_master *master;

	master = platform_get_drvdata(dev);
	if (master == NULL)
		goto out0;
	controller = spi_master_get_devdata(master);

	spi_unregister_master(master);

#ifdef SPI_INT_MODE
	writel(0, controller->regs + CS75XX_SPI_IE0);
	free_irq(controller->irq);
#endif
	destroy_workqueue(controller->workqueue);
	iounmap(controller->regs);
	spi_master_put(master);
	platform_set_drvdata(dev, NULL);
out0:
	return 0;
}

static struct platform_driver cs75xx_spi_driver = {
	.probe	= cs75xx_spi_probe,
	.remove	= __devexit_p(cs75xx_spi_remove),
	.driver = {
		.name = CS75XX_SPI_CTLR_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init cs75xx_spi_init(void)
{
	return platform_driver_register(&cs75xx_spi_driver);
}

static void __exit cs75xx_spi_exit(void)
{
	platform_driver_unregister(&cs75xx_spi_driver);
}

module_init(cs75xx_spi_init);
module_exit(cs75xx_spi_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX SPI driver");

