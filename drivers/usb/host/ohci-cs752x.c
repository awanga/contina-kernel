/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * Copyright (C) 2008 Renesas Solutions Corp.
 *
 * Author : Yoshihiro Shimoda <shimoda.yoshihiro@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/platform_device.h>

static int ohci_cs752x_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);

	ohci_hcd_init(ohci);
	ohci_init(ohci);
	ohci_run(ohci);
	hcd->state = HC_STATE_RUNNING;
	return 0;
}

static const struct hc_driver ohci_cs752x_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"cs752x OHCI",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_cs752x_start,
	.stop =			ohci_stop,
	.shutdown =		ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

/*-------------------------------------------------------------------------*/

#define resource_len(r) (((r)->end - (r)->start) + 1)
static int cs752x_ohci_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct usb_hcd *hcd = NULL;
	const struct hc_driver *driver = &ohci_cs752x_hc_driver;
	int irq = -1;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		err("ohci : fail to platform_get_irq");
		return -ENODEV;
	}

	/* initialize hcd */
	hcd = usb_create_hcd(driver, &pdev->dev, (char *)hcd_name);
	if (!hcd) {
		err("ohci : fail to usb_create_hcd");
		ret = -ENOMEM;
		goto fail_create_hcd;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err("ohci : fail to platform_get_resource (memory)");
		ret = -ENODEV;
		goto fail_request_resource;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_len(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
				driver->description)) {
		dev_dbg(&pdev->dev, "ohci : controller already in use\n");
		ret = -EBUSY;
		goto fail_request_resource;
	}

	hcd->regs = ioremap_nocache(hcd->rsrc_start, hcd->rsrc_len);
	printk("==>%s ohci hcd->regs 0x%p\n", __func__, hcd->regs);
	if (!hcd->regs) {
		dev_dbg(&pdev->dev, "ohci : fail to ioremap_nocache\n");
		ret = -EFAULT;
		goto fail_ioremap;
	}

	device_init_wakeup(&pdev->dev, 1);

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto fail_add_hcd;

	return ret;

fail_add_hcd :
	device_init_wakeup(&pdev->dev, 0);
	iounmap(hcd->regs);

fail_ioremap :
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

fail_request_resource :
	usb_put_hcd(hcd);

fail_create_hcd :
	dev_err(&pdev->dev, "ohci : init %s fail, %d\n", dev_name(&pdev->dev), ret);

	return ret;
}

static int cs752x_ohci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef	CONFIG_PM
static int cs752x_ohci_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	printk("%s:in\n", __FUNCTION__);
	if (time_before(jiffies, ohci->next_statechange))
		msleep(5);
	ohci->next_statechange = jiffies;

#if 0
	ohci_writel(ohci, OHCI_INTR_MIE, &ohci->regs->intrdisable);
	(void) ohci_readl(ohci, &ohci->regs->intrdisable);
#endif
#if 0
	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
#endif
#if 0
	/*
	 * The integrated transceivers seem unable to notice disconnect,
	 * reconnect, or wakeup without the 48 MHz clock active.  so for
	 * correctness, always discard connection state (using reset).
	 *
	 * REVISIT: some boards will be able to turn VBUS off...
	 */
	ohci_usb_reset(ohci);
	/* flush the writes */
	(void) ohci_readl(ohci, &ohci->regs->control);
#endif
#if 0
	hcd->state = HC_STATE_SUSPENDED;
#endif

	if (device_may_wakeup(&pdev->dev))
		enable_irq_wake(hcd->irq);
	printk("%s:out\n", __FUNCTION__);
	return 0;
}

static int cs752x_ohci_resume(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	printk("%s:in\n", __FUNCTION__);
	if (time_before(jiffies, ohci->next_statechange))
		msleep(5);
	ohci->next_statechange = jiffies;

	if (device_may_wakeup(&pdev->dev))
		disable_irq_wake(hcd->irq);

	/*printk("%08x,%08x\n", ohci->regs->control, ohci->regs->cmdstatus);*/
#if 0
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
#endif
#if 1
	ohci_finish_controller_resume(hcd);
#endif
	printk("%s:out\n", __FUNCTION__);
	return 0;
}
#endif

static struct platform_driver ohci_hcd_cs752x_driver = {
	.probe		= cs752x_ohci_probe,
	.remove		= cs752x_ohci_remove,
	.shutdown	= usb_hcd_platform_shutdown,
#ifdef	CONFIG_PM
	.suspend	= cs752x_ohci_suspend,
	.resume		= cs752x_ohci_resume,
#endif
	.driver		= {
		.name	= "cs752x_ohci",
		.owner	= THIS_MODULE,
	},
};

MODULE_ALIAS("platform:cs752x_ohci");
