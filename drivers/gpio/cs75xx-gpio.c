/*
 * FILE NAME cs75xx_gpio.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XY GPIO.
 *
 *  Copyright 2010 Cortina , Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/gpio.h>

/* GLOBAL Register Map */
#define CS75XX_GPIO_MUX_0		0x1C
#define CS75XX_GPIO_MUX_1		0x20
#define CS75XX_GPIO_MUX_2		0x24
#define CS75XX_GPIO_MUX_3		0x28
#define CS75XX_GPIO_MUX_4		0x2C

/* GPIO Register Map */
#define CS75XX_GPIO_CFG			0x00
#define CS75XX_GPIO_OUT			0x04
#define CS75XX_GPIO_IN			0x08
#define CS75XX_GPIO_LVL			0x0C
#define CS75XX_GPIO_EDGE		0x10
#define CS75XX_GPIO_IE			0x14
#define CS75XX_GPIO_INT			0x18
#define CS75XX_GPIO_STAT		0x1C

/* CS75XX_GPIO_CFG BIT */
#define GPIO_CFG_OUT			0
#define GPIO_CFG_IN			1

static int cs75xx_gpio_debug = 1;
#define gpio_dbgmsg(fmt...) if (cs75xx_gpio_debug >= 1) printk(fmt)

static void __iomem *cs75xx_gpio_base[GPIO_BANK_NUM];
static u32 cs75xx_irq_gpio[GPIO_BANK_NUM];
static u32 cs75xx_gpio_pin[GPIO_BANK_NUM];
static void __iomem *cs75xx_global_base;

static void _set_gpio_irqenable(void __iomem *base, unsigned int index,
				int enable)
{
	unsigned int reg;

	reg = __raw_readl(base + CS75XX_GPIO_IE);
	reg = (reg & (~(1 << index))) | (!!enable << index);
	__raw_writel(reg, base + CS75XX_GPIO_IE);
}

static void cs75xx_gpio_ack_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int gpio = irq_to_gpio(irq);
	void __iomem *base = cs75xx_gpio_base[gpio / GPIO_BANK_SIZE];

	__raw_writel(1 << (gpio % GPIO_BANK_SIZE), base + CS75XX_GPIO_INT);
}

void cs75xx_gpio_mask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int gpio = irq_to_gpio(irq);
	void __iomem *base = cs75xx_gpio_base[gpio / GPIO_BANK_SIZE];

	_set_gpio_irqenable(base, gpio % GPIO_BANK_SIZE, 0);
}
EXPORT_SYMBOL_GPL(cs75xx_gpio_mask_irq);

void cs75xx_gpio_unmask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int gpio = irq_to_gpio(irq);
	void __iomem *base = cs75xx_gpio_base[gpio / GPIO_BANK_SIZE];

	cs75xx_gpio_ack_irq(irqd);
	_set_gpio_irqenable(base, gpio % GPIO_BANK_SIZE, 1);
}
EXPORT_SYMBOL_GPL(cs75xx_gpio_unmask_irq);

int cs75xx_gpio_set_irq_type(struct irq_data *irqd, unsigned int type)
{
	unsigned irq = irqd->irq;
	unsigned int gpio = irq_to_gpio(irq);
	unsigned int gpio_mask = 1 << (gpio % GPIO_BANK_SIZE);
	void __iomem *base = cs75xx_gpio_base[gpio / GPIO_BANK_SIZE];
	unsigned int reg_level, reg_edge;

	reg_level = __raw_readl(base + CS75XX_GPIO_LVL);
	reg_edge = __raw_readl(base + CS75XX_GPIO_EDGE);

	switch (type) {
	case IRQ_TYPE_LEVEL_LOW:
		reg_level &= ~gpio_mask;
		reg_edge &= ~gpio_mask;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		reg_level |= gpio_mask;
		reg_edge &= ~gpio_mask;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		reg_level &= ~gpio_mask;
		reg_edge |= gpio_mask;
		break;
	case IRQ_TYPE_EDGE_RISING:
		reg_level |= gpio_mask;
		reg_edge |= gpio_mask;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		default:
	return -EINVAL;
	}

	__raw_writel(reg_level, base + CS75XX_GPIO_LVL);
	__raw_writel(reg_edge, base + CS75XX_GPIO_EDGE);

	return 0;
}
EXPORT_SYMBOL_GPL(cs75xx_gpio_set_irq_type);

#ifdef CONFIG_PM
static int cs75xx_gpio_set_irq_wake(struct irq_data *irqd, unsigned int on)
{
	unsigned irq = irqd->irq;
	unsigned int gpio = irq_to_gpio(irq);
	void __iomem *base;

	if (gpio >= (GPIO_BANK_NUM * GPIO_BANK_SIZE))
		return -EINVAL;

	base = cs75xx_gpio_base[gpio / GPIO_BANK_SIZE];

	if (on)	/* enter PM, GPIO irq will be disabled */
		_set_gpio_irqenable(base, gpio % GPIO_BANK_SIZE, 1);

	return 0;
}
#endif

static void cs75xx_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned int gpio_irq_no, irq_stat;
	unsigned int port = (unsigned int)irq_get_handler_data(irq);

	irq_stat = __raw_readl(cs75xx_gpio_base[port] + CS75XX_GPIO_INT);
	irq_stat &= __raw_readl(cs75xx_gpio_base[port] + CS75XX_GPIO_IE);

	gpio_irq_no = GPIO_IRQ_BASE + port * GPIO_BANK_SIZE;
	for (; irq_stat != 0; irq_stat >>= 1, gpio_irq_no++) {

		if ((irq_stat & 1) == 0)
			continue;

		BUG_ON(!(irq_desc[gpio_irq_no].handle_irq));
		irq_desc[gpio_irq_no].handle_irq(gpio_irq_no,
				&irq_desc[gpio_irq_no]);
	}
}

static struct irq_chip cs75xx_gpio_irq_chip = {
	.name = CS75XX_GPIO_CTLR_NAME,
	.irq_ack = cs75xx_gpio_ack_irq,
	.irq_mask = cs75xx_gpio_mask_irq,
	.irq_unmask = cs75xx_gpio_unmask_irq,
	.irq_set_type = cs75xx_gpio_set_irq_type,
#ifdef CONFIG_PM
	.irq_set_wake = cs75xx_gpio_set_irq_wake,
#endif
};

static int cs75xx_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	unsigned int gpio_mux;

	gpio_mux = __raw_readl(cs75xx_global_base + CS75XX_GPIO_MUX_0 +
	                                  (offset / GPIO_BANK_SIZE) * 4);
	if ((gpio_mux & BIT(offset % GPIO_BANK_SIZE)) == 0)
		return -EINVAL;

	return 0;
}

void cs75xx_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	return ;
}

static void _set_gpio_direction(struct gpio_chip *chip, unsigned offset,
				int dir)
{
	void __iomem *base = cs75xx_gpio_base[offset / GPIO_BANK_SIZE];
	unsigned int reg;

	reg = __raw_readl(base + CS75XX_GPIO_CFG);
	if (dir)
		reg |= 1 << (offset % GPIO_BANK_SIZE);
	else
		reg &= ~(1 << (offset % GPIO_BANK_SIZE));
	__raw_writel(reg, base + CS75XX_GPIO_CFG);
}

static void cs75xx_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	void __iomem *base = cs75xx_gpio_base[offset / GPIO_BANK_SIZE];
	unsigned int reg;

	reg = __raw_readl(base + CS75XX_GPIO_OUT);
	if (value)
		reg |= 1 << (offset % GPIO_BANK_SIZE);
	else
		reg &= ~(1 << (offset % GPIO_BANK_SIZE));
	__raw_writel(reg, base + CS75XX_GPIO_OUT);
}

static int cs75xx_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	void __iomem *base = cs75xx_gpio_base[offset / GPIO_BANK_SIZE];

	return (__raw_readl(base + CS75XX_GPIO_IN) >> (offset % GPIO_BANK_SIZE)) & 1;
}

static int cs75xx_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	_set_gpio_direction(chip, offset, GPIO_CFG_IN);
	return 0;
}

static int cs75xx_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
					int value)
{
	_set_gpio_direction(chip, offset, GPIO_CFG_OUT);
	cs75xx_gpio_set(chip, offset, value);
	return 0;
}

static struct gpio_chip cs75xx_gpio_chip = {
	.label			= CS75XX_GPIO_CTLR_NAME,
	.request		= cs75xx_gpio_request,
	.free			= cs75xx_gpio_free,
	.direction_input	= cs75xx_gpio_direction_input,
	.get			= cs75xx_gpio_get,
	.direction_output	= cs75xx_gpio_direction_output,
	.set			= cs75xx_gpio_set,
	.base			= 0,
	.ngpio			= GPIO_BANK_NUM * GPIO_BANK_SIZE,
};

static int __devinit cs75xx_gpio_probe(struct platform_device *pdev)
{
	int i, j;
	char tmp_str[16];
	struct resource *res_mem;

	gpio_dbgmsg("Function: %s, pdev->name = %s\n", __func__, pdev->name);

	memset(cs75xx_gpio_base, 0, sizeof(cs75xx_gpio_base));

	/* get the module base address and irq number */
	sprintf(tmp_str, "global");
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, tmp_str);
	if (!res_mem) {
		gpio_dbgmsg("Func: %s - can't get resource %s\n", __func__, tmp_str);
		goto fail;
	}
	cs75xx_global_base = ioremap(res_mem->start, res_mem->end - res_mem->start + 1);
	if (!cs75xx_global_base) {
		gpio_dbgmsg("Func: %s - unable to remap %s %d memory \n",
		            __func__, tmp_str, res_mem->end - res_mem->start + 1);
		goto fail;
	}
	gpio_dbgmsg("\tcs75xx_global_base = %p\n", cs75xx_global_base);

	for (i = 0; i < GPIO_BANK_NUM; i++) {
		sprintf(tmp_str, "gpio%d", i);
		res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, tmp_str);
		if (!res_mem) {
			gpio_dbgmsg("Func: %s - can't get resource %s\n", __func__, tmp_str);
			goto fail;
		}
		cs75xx_gpio_base[i] = ioremap(res_mem->start, res_mem->end - res_mem->start + 1);
		if (!cs75xx_gpio_base[i]) {
			gpio_dbgmsg("Func: %s - unable to remap %s %d memory \n",
			            __func__, tmp_str, res_mem->end - res_mem->start + 1);
			goto fail;
		}
		gpio_dbgmsg("\tcs75xx_gpio_base[%d] = %p\n", i, cs75xx_gpio_base[i]);
	}

	for (i = 0; i < GPIO_BANK_NUM; i++) {
		sprintf(tmp_str, "irq_gpio%d", i);
		cs75xx_irq_gpio[i] = platform_get_irq_byname(pdev, tmp_str);
		if (cs75xx_irq_gpio[i] == -ENXIO) {
			gpio_dbgmsg("Func: %s - can't get resource %s\n", __func__, tmp_str);
			goto fail;
		}
		gpio_dbgmsg("\tcs75xx_irq_gpio[%d] = %08x\n", i, cs75xx_irq_gpio[i]);
	}

	/* disable irq and register to gpiolib */
	for (i = 0; i < GPIO_BANK_NUM; i++) {
		/* disable, unmask and clear all interrupts */
		__raw_writel(0x0, cs75xx_gpio_base[i] + CS75XX_GPIO_IE);

		for (j = GPIO_IRQ_BASE + i * GPIO_BANK_SIZE;
		     j < GPIO_IRQ_BASE + (i + 1) * GPIO_BANK_SIZE; j++) {
			irq_set_chip(j, &cs75xx_gpio_irq_chip);
			irq_set_handler(j, handle_edge_irq);
			set_irq_flags(j, IRQF_VALID);
		}

		irq_set_chained_handler(cs75xx_irq_gpio[i], cs75xx_gpio_irq_handler);
		irq_set_handler_data(cs75xx_irq_gpio[i], (void *)i);
	}

	BUG_ON(gpiochip_add(&cs75xx_gpio_chip));

	return 0;

fail:
	for (i = 0; i < GPIO_BANK_NUM; i++)
		if (cs75xx_gpio_base[i])
			iounmap(cs75xx_gpio_base[i]);

	return -ENODEV;
}

static int __devexit cs75xx_gpio_remove(struct platform_device *pdev)
{
	int i, j;

	gpio_dbgmsg("Function: %s\n", __func__);

	/* disable irq and deregister to gpiolib */
	for (i = 0; i < GPIO_BANK_NUM; i++) {
		/* disable, unmask and clear all interrupts */
		__raw_writel(0x0, cs75xx_gpio_base[i] + CS75XX_GPIO_IE);
		__raw_writel(~0x0, cs75xx_gpio_base[i] + CS75XX_GPIO_INT);

		for (j = GPIO_IRQ_BASE + i * GPIO_BANK_SIZE;
		     j < GPIO_IRQ_BASE + (i + 1) * GPIO_BANK_SIZE; j++) {
			irq_set_chip(j, NULL);
			irq_set_handler(j, NULL);
			set_irq_flags(j, 0);
		}

		irq_set_chained_handler(cs75xx_irq_gpio[i], NULL);
		irq_set_handler_data(cs75xx_irq_gpio[i], NULL);
	}

	BUG_ON(gpiochip_remove(&cs75xx_gpio_chip));

	return 0;
}

static struct platform_driver cs75xx_gpio_platform_driver = {
	.probe	= cs75xx_gpio_probe,
	.remove	= __devexit_p(cs75xx_gpio_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name  = CS75XX_GPIO_CTLR_NAME,
	},
};

static int __init cs75xx_gpio_init(void)
{
	int rc;

	gpio_dbgmsg("\n%s\n", __func__);

	rc = platform_driver_register(&cs75xx_gpio_platform_driver);
	gpio_dbgmsg(", rc = %d\n", rc);

	return rc;
}

static void __exit cs75xx_gpio_exit(void)
{
	gpio_dbgmsg("\n%s\n", __func__);

	platform_driver_unregister(&cs75xx_gpio_platform_driver);
}

module_init(cs75xx_gpio_init);
module_exit(cs75xx_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX GPIO driver");

