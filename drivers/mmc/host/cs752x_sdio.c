/*
 * Synopsys DesignWare Multimedia Card Interface driver
 *
 * Copyright (C) 2009 NXP Semiconductors
 * Copyright (C) 2009, 2010 Imagination Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/dw_mmc.h>
#include <mach/hardware.h>
#include "dw_mmc.h"


static int g2_do_nothing_init(u32 slot_id, irq_handler_t irq, void *data)
{
	return 0;
}

static struct dw_mci_board g2_board={
	.num_slots		= 1,
	.caps			=	MMC_CAP_MMC_HIGHSPEED |\
					MMC_CAP_SDIO_IRQ |\
					MMC_CAP_SD_HIGHSPEED ,
					MMC_CAP_4_BIT_DATA |\
					MMC_CAP_8_BIT_DATA,
	.bus_hz 		= 50 * 1000 * 1000,
	.detect_delay_ms	= 200,
	.fifo_depth		= 16,
	.init 			= g2_do_nothing_init,
};


static int dw_mci_pltfm_probe(struct platform_device *pdev)
{
	struct dw_mci *host;
	struct resource	*regs;
	int ret;

	host = kzalloc(sizeof(struct dw_mci), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		ret = -ENXIO;
		goto err_free;
	}


	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		ret = host->irq;
		goto err_free;
	}

	host->dev = pdev->dev;
	host->irq_flags = 0;
	host->pdata = pdev->dev.platform_data;
	host->pdata =  &g2_board;
	ret = -ENOMEM;
	host->regs = ioremap(regs->start, resource_size(regs));
	if (!host->regs)
		goto err_free;
	printk( "sd mapping 0x%08x \n", host->regs);
	platform_set_drvdata(pdev, host);
	ret = dw_mci_probe(host);
	if (ret)
		goto err_out;
	return ret;
err_out:
	iounmap(host->regs);
err_free:
	kfree(host);
	return ret;
}

static int __exit dw_mci_pltfm_remove(struct platform_device *pdev)
{
	struct dw_mci *host = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	dw_mci_remove(host);
	iounmap(host->regs);
	kfree(host);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
/*
 * TODO: we should probably disable the clock to the card in the suspend path.
 */
static int dw_mci_pltfm_suspend(struct device *dev)
{
	int ret;
	struct dw_mci *host = dev_get_drvdata(dev);

	ret = dw_mci_suspend(host);
	if (ret)
		return ret;

	return 0;
}

static int dw_mci_pltfm_resume(struct device *dev)
{
	int ret;
	struct dw_mci *host = dev_get_drvdata(dev);

	ret = dw_mci_resume(host);
	if (ret)
		return ret;

	return 0;
}
#else
#define dw_mci_pltfm_suspend	NULL
#define dw_mci_pltfm_resume	NULL
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(dw_mci_pltfm_pmops, dw_mci_pltfm_suspend, dw_mci_pltfm_resume);

#define SD_DRIVER_NAME "cs752x_sd"
static struct platform_driver cs752x_sd_driver = {
	.probe    = dw_mci_pltfm_probe,
	.remove		= __exit_p(dw_mci_pltfm_remove),
	.driver		= {
		.name		= SD_DRIVER_NAME,
		.pm		= &dw_mci_pltfm_pmops,
	},
};

static int __init cs752x_sdio_init(void)
{
	int status;
#ifndef CONFIG_CORTINA_FPGA
	status = readl(IO_ADDRESS(GLOBAL_GPIO_MUX_2));
	status &= ~0x000000FF;	/* GPIO 2 Bit[7:0] */
	writel(status, IO_ADDRESS(GLOBAL_GPIO_MUX_2));
#endif

	return  platform_driver_register(&cs752x_sd_driver);
}

static void __exit cs752x_sdio_exit(void)
{
	platform_driver_unregister(&cs752x_sd_driver);
}

module_init(cs752x_sdio_init);
module_exit(cs752x_sdio_exit);

MODULE_LICENSE("GPL v2");
