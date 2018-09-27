/*
 * FILE NAME i2c-cs75xx.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX I2C(BIW) controller
 *
 *  Copyright 2011 Cortina , Corp.
 *
 *  Based on i2c-mv64xxx.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <mach/cs75xx_i2c.h>
#include <asm/io.h>

/* Register defines */
#define CS75XX_BIW_CFG		0x00
#define CS75XX_BIW_CTRL		0x04
#define CS75XX_BIW_TXR		0x08
#define CS75XX_BIW_RXR		0x0C
#define CS75XX_BIW_ACK		0x10
#define CS75XX_BIW_IE0		0x14
#define CS75XX_BIW_INT0		0x18
#define CS75XX_BIW_IE1		0x1C
#define CS75XX_BIW_INT1		0x20
#define CS75XX_BIW_STAT		0x24

/* CS75XX_BIW_CFG BIT */
#define BIW_CFG_CORE_EN		BIT(0)
#define BIW_CFG_PRER_OFF	16
#define BIW_CFG_PRER_MASK	0xFFFF0000

/* CS75XX_BIW_CTRL BIT */
#define BIW_CTRL_DONE		BIT(0)
#define BIW_CTRL_ACK_IN		BIT(3)
#define BIW_CTRL_WRITE		BIT(4)
#define BIW_CTRL_READ		BIT(5)
#define BIW_CTRL_STOP		BIT(6)
#define BIW_CTRL_START		BIT(7)

/* CS75XX_BIW_TXR BIT */
#define BIW_TXR_OFF		0
#define BIW_TXR_MASK		0x000000FF

/* CS75XX_BIW_RXR BIT */
#define BIW_RXR_OFF		0
#define BIW_RXR_MASK		0x000000FF

/* CS75XX_BIW_ACK */
#define BIW_ACK_AL		BIT(0)
#define BIW_ACK_BUSY		BIT(1)
#define BIW_ACK_ACK_OUT		BIT(2)

/* CS75XX_BIW_IE0 BIT */
#define BIW_IE_BIT		BIT(0)

/* CS75XX_BIW_INT0 */
#define BIW_INT_BIT		BIT(0)

/* CS75XX_BIW_STAT */
#define BIW_STAT_BIT		BIT(0)

/* Driver states */
enum {
	CS75XX_I2C_STATE_INVALID,
	CS75XX_I2C_STATE_IDLE,
	CS75XX_I2C_STATE_START,
	CS75XX_I2C_STATE_WAITING_FOR_ADDR_1_ACK,
	CS75XX_I2C_STATE_WAITING_FOR_ADDR_2_ACK,
	CS75XX_I2C_STATE_WAITING_FOR_WRITE_ACK,
	CS75XX_I2C_STATE_WAITING_FOR_READ_ACK,
};

struct cs75xx_i2c_data {
	int			irq;
	u32			state;
	u32			aborting;
	void __iomem		*reg_base;
	u32			reg_base_p;
	u32			reg_size;
	u32			addr1;
	u32			addr2;
	u32			bytes_left;
	u32			byte_posn;
	u32			block;
	int			rc;
	u32			freq_rcl;
	u32			freq_scl;
	u32			ack;
	wait_queue_head_t	waitq;
	spinlock_t		lock;
	struct i2c_msg		*msg;
	struct i2c_adapter	adapter;
};

#define I2C_M_NOSTOP	0x8000

/*
 *****************************************************************************
 *
 *	Finite State Machine & Interrupt Routines
 *
 *****************************************************************************
 */

static void
cs75xx_i2c_hw_fini(struct cs75xx_i2c_data *drv_data)
{
	writel(0, drv_data->reg_base + CS75XX_BIW_CFG);

	drv_data->state = CS75XX_I2C_STATE_INVALID;
}

/* Reset hardware and initialize FSM */
static void
cs75xx_i2c_hw_init(struct cs75xx_i2c_data *drv_data)
{
	PER_BIW_CFG_t reg_biw_cfg;

	reg_biw_cfg.wrd = readl(drv_data->reg_base + CS75XX_BIW_CFG);

	if (drv_data->freq_scl == 0) {
		reg_biw_cfg.bf.core_en = 0;
	}
	else {
		/* reset */
		if (reg_biw_cfg.bf.core_en) {
			cs75xx_i2c_hw_fini(drv_data);
			mdelay(50);
		}

		if (drv_data->freq_scl != 100000 && drv_data->freq_scl != 400000) {
			printk("WRNG: I2C SCL set to %dKHz!\n", drv_data->freq_scl) ;
		}

		reg_biw_cfg.bf.prer = drv_data->freq_rcl/ (5 * (drv_data->freq_scl + 1));
		reg_biw_cfg.bf.core_en = 1;
	}
	writel(reg_biw_cfg.wrd, drv_data->reg_base + CS75XX_BIW_CFG);
	mdelay(50);

	drv_data->state = CS75XX_I2C_STATE_IDLE;
}

static void
cs75xx_i2c_fsm(struct cs75xx_i2c_data *drv_data)
{
	/*
	 * If error result occurs, stop the following actions and reset hardware
	 */
	if (((drv_data->ack & BIW_ACK_ACK_OUT) ||
	    (drv_data->ack & BIW_ACK_AL)) &&
	    (drv_data->bytes_left != 0))
		drv_data->state = CS75XX_I2C_STATE_INVALID;

	/* The status from the ctlr [mostly] tells us what to do next */
	switch (drv_data->state) {
	case CS75XX_I2C_STATE_START:
#ifdef CONFIG_PANEL_HX8238A_TOUCH
		drv_data->state = CS75XX_I2C_STATE_WAITING_FOR_READ_ACK;	/* next state */
#else
		drv_data->state = CS75XX_I2C_STATE_WAITING_FOR_ADDR_1_ACK;	/* next state */
#endif
		writel(drv_data->addr1, drv_data->reg_base + CS75XX_BIW_TXR);
		writel(BIW_CTRL_START | BIW_CTRL_WRITE, drv_data->reg_base + CS75XX_BIW_CTRL);
		break;

	/* Performing a read/write */
	case CS75XX_I2C_STATE_WAITING_FOR_ADDR_1_ACK:
		if ((drv_data->msg->flags & I2C_M_TEN) &&
		    !(drv_data->msg->flags & I2C_M_NOSTART)) {
			drv_data->state = CS75XX_I2C_STATE_WAITING_FOR_ADDR_2_ACK;	/* next state */
			writel(drv_data->addr2, drv_data->reg_base + CS75XX_BIW_TXR);
			writel(BIW_CTRL_WRITE, drv_data->reg_base + CS75XX_BIW_CTRL);
			break;
		}
		/* FALLTHRU */
	case CS75XX_I2C_STATE_WAITING_FOR_ADDR_2_ACK:
		if (drv_data->msg->flags & I2C_M_RD) {	/* read */
			drv_data->state = CS75XX_I2C_STATE_WAITING_FOR_READ_ACK;
			drv_data->bytes_left--;

			if (drv_data->bytes_left || (drv_data->msg->flags & I2C_M_NOSTOP))
				writel(BIW_CTRL_READ, drv_data->reg_base + CS75XX_BIW_CTRL);
			else
				writel(BIW_CTRL_READ | BIW_CTRL_ACK_IN | BIW_CTRL_STOP,
						drv_data->reg_base + CS75XX_BIW_CTRL);
		}
		else {	/* write */
			drv_data->state = CS75XX_I2C_STATE_WAITING_FOR_WRITE_ACK;
			writel(drv_data->msg->buf[drv_data->byte_posn++],
						drv_data->reg_base + CS75XX_BIW_TXR);
			drv_data->bytes_left--;

			if (drv_data->bytes_left || (drv_data->msg->flags & I2C_M_NOSTOP))
				writel(BIW_CTRL_WRITE, drv_data->reg_base + CS75XX_BIW_CTRL);
			else
				writel(BIW_CTRL_WRITE | BIW_CTRL_STOP,
						drv_data->reg_base + CS75XX_BIW_CTRL);
		}
		break;

	case CS75XX_I2C_STATE_WAITING_FOR_WRITE_ACK:
		if (drv_data->bytes_left) {
			writel(drv_data->msg->buf[drv_data->byte_posn++],
						drv_data->reg_base + CS75XX_BIW_TXR);
			drv_data->bytes_left--;

			if (drv_data->bytes_left || (drv_data->msg->flags & I2C_M_NOSTOP))
				writel(BIW_CTRL_WRITE, drv_data->reg_base + CS75XX_BIW_CTRL);
			else
				writel(BIW_CTRL_WRITE | BIW_CTRL_STOP,
						drv_data->reg_base + CS75XX_BIW_CTRL);
		}
		else {
			drv_data->block = 0;
			wake_up_interruptible(&drv_data->waitq);
		}
		break;

	case CS75XX_I2C_STATE_WAITING_FOR_READ_ACK:
		drv_data->msg->buf[drv_data->byte_posn++] =
					readl(drv_data->reg_base + CS75XX_BIW_RXR);

		if (drv_data->bytes_left) {
			drv_data->bytes_left--;

			if (drv_data->bytes_left)
				writel(BIW_CTRL_READ, drv_data->reg_base + CS75XX_BIW_CTRL);
			else
				writel(BIW_CTRL_READ | BIW_CTRL_ACK_IN | BIW_CTRL_STOP,
						drv_data->reg_base + CS75XX_BIW_CTRL);
		}
		else {
			drv_data->block = 0;
			wake_up_interruptible(&drv_data->waitq);
		}
		break;

	case CS75XX_I2C_STATE_INVALID:
	default:
		dev_err(&drv_data->adapter.dev,
			"cs75xx_i2c_fsm: Ctlr Error -- state: 0x%x, "
			"ack: 0x%x, addr: 0x%x, flags: 0x%x, left: 0x%x\n",
			 drv_data->state, drv_data->ack, drv_data->msg->addr,
			 drv_data->msg->flags, drv_data->bytes_left);

		drv_data->rc = -EIO;

		cs75xx_i2c_hw_init(drv_data);

		drv_data->block = 0;
		wake_up_interruptible(&drv_data->waitq);
	}
}

static irqreturn_t
cs75xx_i2c_intr(int irq, void *param)
{
	struct cs75xx_i2c_data *drv_data = param;
	PER_BIW_CTRL_t reg_biw_ctrl;
	PER_BIW_INT_0_t reg_biw_int0;
	irqreturn_t rc = IRQ_NONE;

	/* disable biw interrupt */
	writel(0, drv_data->reg_base + CS75XX_BIW_IE0);

	reg_biw_int0.wrd = readl(drv_data->reg_base + CS75XX_BIW_INT0);
	reg_biw_int0.bf.biwi = 1;
	writel(reg_biw_int0.wrd, drv_data->reg_base + CS75XX_BIW_INT0);

	reg_biw_ctrl.wrd = readl(drv_data->reg_base + CS75XX_BIW_CTRL);
	if (reg_biw_ctrl.bf.biwdone != 1)
		printk("no done!\n");
	writel(reg_biw_ctrl.wrd, drv_data->reg_base + CS75XX_BIW_CTRL);

	drv_data->ack = readl(drv_data->reg_base + CS75XX_BIW_ACK);

	cs75xx_i2c_fsm(drv_data);
	rc = IRQ_HANDLED;

	/* enable biw interrupt */
	writel(1, drv_data->reg_base + CS75XX_BIW_IE0);

	return rc;
}

/*
 *****************************************************************************
 *
 *	I2C Msg Execution Routines
 *
 *****************************************************************************
 */
static void
cs75xx_i2c_prepare_for_io(struct cs75xx_i2c_data *drv_data,
	struct i2c_msg *msg)
{
	u32	dir = 0;

	drv_data->msg = msg;
	drv_data->byte_posn = 0;
	drv_data->bytes_left = msg->len;
	drv_data->aborting = 0;
	drv_data->rc = 0;
	drv_data->ack = 0;

	if (msg->flags & I2C_M_RD)
		dir = 1;

	if (msg->flags & I2C_M_TEN) {
		drv_data->addr1 = 0xf0 | (((u32)msg->addr & 0x300) >> 7) | dir;
		drv_data->addr2 = (u32)msg->addr & 0xff;
	} else {
		drv_data->addr1 = ((u32)msg->addr & 0x7f) << 1 | dir;
		drv_data->addr2 = 0;
	}
}

static void
cs75xx_i2c_wait_for_completion(struct cs75xx_i2c_data *drv_data)
{
	long		time_left;
	char		abort = 0;

	time_left = wait_event_interruptible_timeout(drv_data->waitq,
		!drv_data->block, drv_data->adapter.timeout);

	spin_lock(&drv_data->lock);
	if (!time_left) { /* Timed out */
		drv_data->rc = -ETIMEDOUT;
		abort = 1;
	} else if (time_left < 0) { /* Interrupted/Error */
		drv_data->rc = time_left; /* errno value */
		abort = 1;
	}
	spin_unlock(&drv_data->lock);

	if (abort && drv_data->block) {
		spin_lock(&drv_data->lock);
		drv_data->aborting = 1;
		spin_unlock(&drv_data->lock);

		time_left = wait_event_timeout(drv_data->waitq,
			!drv_data->block, drv_data->adapter.timeout);

		if ((time_left <= 0) && drv_data->block) {
			drv_data->state = CS75XX_I2C_STATE_IDLE;
			dev_err(&drv_data->adapter.dev,
				"CS75XX: I2C bus locked, block: %d, "
				"time_left: %d\n", drv_data->block,
				(int)time_left);
			cs75xx_i2c_hw_init(drv_data);
			drv_data->block = 0;
		}
	}
}

static int
cs75xx_i2c_execute_msg(struct cs75xx_i2c_data *drv_data, struct i2c_msg *msg)
{
	spin_lock(&drv_data->lock);

	cs75xx_i2c_prepare_for_io(drv_data, msg);

	if (unlikely(msg->flags & I2C_M_NOSTART))
		drv_data->state = CS75XX_I2C_STATE_WAITING_FOR_ADDR_2_ACK;
	else
		drv_data->state = CS75XX_I2C_STATE_START;

	drv_data->block = 1;
	cs75xx_i2c_fsm(drv_data);

	spin_unlock(&drv_data->lock);

	cs75xx_i2c_wait_for_completion(drv_data);

	return drv_data->rc;
}

/*
 *****************************************************************************
 *
 *	I2C Core Support Routines (Interface to higher level I2C code)
 *
 *****************************************************************************
 */
static u32
cs75xx_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR | I2C_FUNC_SMBUS_EMUL;
}

static int
cs75xx_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	struct cs75xx_i2c_data *drv_data = i2c_get_adapdata(adap);
	int	i, rc;
	int	retry = 0;

	for (i = 0; i < num; i++) {
		/* check if next msg is continus msg */
		if (i < (num -1)) {
			if (msgs[i + 1].flags & I2C_M_NOSTART) {
				if ((msgs[i].flags & I2C_M_RD) == (msgs[i + 1].flags & I2C_M_RD))
					msgs[i].flags |= I2C_M_NOSTOP;
				else
					msgs[i + 1].flags &= ~I2C_M_NOSTART;
			}
		}

		if ((rc = cs75xx_i2c_execute_msg(drv_data, &msgs[i])) < 0) {

			for (retry = 0; retry < drv_data->adapter.retries; retry++)
				if ((rc = cs75xx_i2c_execute_msg(drv_data, &msgs[i])) == 0)
					break;
			if (retry == drv_data->adapter.retries)
				printk("I2C master_xfer retry %d fail!\n", drv_data->adapter.retries);
		}
	}

	return num;
}

static const struct i2c_algorithm cs75xx_i2c_algo = {
	.master_xfer = cs75xx_i2c_xfer,
	.functionality = cs75xx_i2c_functionality,
};

/*
 *****************************************************************************
 *
 *	Driver Interface & Early Init Routines
 *
 *****************************************************************************
 */
static int __devinit
cs75xx_i2c_map_regs(struct platform_device *pd,
	struct cs75xx_i2c_data *drv_data)
{
	int size;
	struct resource	*r = platform_get_resource(pd, IORESOURCE_IO, 0);

	if (!r)
		return -ENODEV;

	size = resource_size(r);

	if (!request_mem_region(r->start, size, drv_data->adapter.name))
		return -EBUSY;

	drv_data->reg_base = ioremap(r->start, size);
	drv_data->reg_base_p = r->start;
	drv_data->reg_size = size;

	return 0;
}

static void
cs75xx_i2c_unmap_regs(struct cs75xx_i2c_data *drv_data)
{
	if (drv_data->reg_base) {
		iounmap(drv_data->reg_base);
		release_mem_region(drv_data->reg_base_p, drv_data->reg_size);
	}

	drv_data->reg_base = NULL;
	drv_data->reg_base_p = 0;
}

static int __devinit
cs75xx_i2c_probe(struct platform_device *pd)
{
	struct cs75xx_i2c_data *drv_data;
	struct cs75xx_i2c_pdata	*pdata = pd->dev.platform_data;
	int	rc;

	if (!pdata)
		return -ENODEV;

	drv_data = kzalloc(sizeof(struct cs75xx_i2c_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	if (cs75xx_i2c_map_regs(pd, drv_data)) {
		rc = -ENODEV;
		goto exit_kfree;
	}

	strlcpy(drv_data->adapter.name, CS75XX_I2C_CTLR_NAME " adapter",
		sizeof(drv_data->adapter.name));

	init_waitqueue_head(&drv_data->waitq);
	spin_lock_init(&drv_data->lock);

	drv_data->state = CS75XX_I2C_STATE_INVALID;

	drv_data->freq_rcl = pdata->freq_rcl;
	drv_data->freq_scl = pdata->freq_scl;
	drv_data->irq = platform_get_irq(pd, 0);
	if (drv_data->irq < 0) {
		rc = -ENXIO;
		goto exit_unmap_regs;
	}

	drv_data->adapter.dev.parent = &pd->dev;
	drv_data->adapter.algo = &cs75xx_i2c_algo;
	drv_data->adapter.owner = THIS_MODULE;
	drv_data->adapter.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	drv_data->adapter.timeout = msecs_to_jiffies(pdata->timeout);
	drv_data->adapter.retries = pdata->retries;
	if (pd->id < 0)	/* only one I2C controller */
		drv_data->adapter.nr = 0;
	else
		drv_data->adapter.nr = pd->id;
	platform_set_drvdata(pd, drv_data);
	i2c_set_adapdata(&drv_data->adapter, drv_data);

	cs75xx_i2c_hw_init(drv_data);

	if (request_irq(drv_data->irq, cs75xx_i2c_intr, 0,
			CS75XX_I2C_CTLR_NAME, drv_data)) {
		dev_err(&drv_data->adapter.dev,
			"CS75XX: Can't register intr handler irq: %d\n",
			drv_data->irq);
		rc = -EINVAL;
		goto exit_unmap_regs;
	} else if ((rc = i2c_add_numbered_adapter(&drv_data->adapter)) != 0) {
		dev_err(&drv_data->adapter.dev,
			"CS75XX: Can't add i2c adapter, rc: %d\n", -rc);
		goto exit_free_irq;
	}

	writel(1, drv_data->reg_base + CS75XX_BIW_IE0);

	return 0;

	exit_free_irq:
		free_irq(drv_data->irq, drv_data);
	exit_unmap_regs:
		cs75xx_i2c_unmap_regs(drv_data);
	exit_kfree:
		kfree(drv_data);
	return rc;
}

static int __devexit
cs75xx_i2c_remove(struct platform_device *dev)
{
	struct cs75xx_i2c_data *drv_data = platform_get_drvdata(dev);
	int	rc;

	rc = i2c_del_adapter(&drv_data->adapter);
	cs75xx_i2c_hw_fini(drv_data);
	free_irq(drv_data->irq, drv_data);
	cs75xx_i2c_unmap_regs(drv_data);
	kfree(drv_data);

	return rc;
}

static struct platform_driver cs75xx_i2c_driver = {
	.probe	= cs75xx_i2c_probe,
	.remove	= __devexit_p(cs75xx_i2c_remove),
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= CS75XX_I2C_CTLR_NAME,
	},
};

static int __init
cs75xx_i2c_init(void)
{
	return platform_driver_register(&cs75xx_i2c_driver);
}

static void __exit
cs75xx_i2c_exit(void)
{
	platform_driver_unregister(&cs75xx_i2c_driver);
}

module_init(cs75xx_i2c_init);
module_exit(cs75xx_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX host bridge i2c ctlr driver");

