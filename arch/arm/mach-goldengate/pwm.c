/*
 * arch/arm/mach-goldengate/pwm.c
 *
 * simple driver for PWM (Pulse Width Modulator) controller
 *
 * (C) by Hoang Tran <hoang.tran@greenwavereality.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pwm.h>

static const struct platform_device_id g2_pwm_id_table[] = {
	{ "cs75xx-pwm", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, g2_pwm_id_table);

/* PWM registers and bits definitions */
#define PWM_PERIOD	(0x00)
#define PWM_DUTY	(0x04)

struct g2_pwm_device {
	struct platform_device	*pdev;
	void __iomem	*io_base;
        struct pwm_chip chip;
};

static int g2_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
                          int duty_ns, int period_ns)
{
	struct g2_pwm_device *cs75xx_pwm = container_of(chip, struct g2_pwm_device, chip);

	if (pwm == NULL || period_ns == 0 || duty_ns > period_ns)
		return -EINVAL;

	__raw_writel((period_ns*150000), cs75xx_pwm->io_base + PWM_PERIOD);
	__raw_writel((duty_ns*150000), cs75xx_pwm->io_base + PWM_DUTY);

	return 0;
}

static int g2_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	return 0;
}

static void g2_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
}

static const struct pwm_ops g2_pwm_ops = {
        .config = g2_pwm_config,
        .enable = g2_pwm_enable,
        .disable = g2_pwm_disable,
        .owner = THIS_MODULE,
};

static int g2_pwm_probe(struct platform_device *pdev)
{
	struct g2_pwm_device *cs75xx_pwm;
	struct resource *r;
	int ret = 0;

	cs75xx_pwm = kzalloc(sizeof(struct g2_pwm_device), GFP_KERNEL);
	if (cs75xx_pwm == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	cs75xx_pwm->pdev = pdev;

	r = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no IO resource defined\n");
		ret = -ENODEV;
		goto err_free;
	}

	cs75xx_pwm->io_base = ioremap(r->start, resource_size(r));
	if (cs75xx_pwm->io_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap() registers\n");
		ret = -ENODEV;
		goto err_free;
	}

	cs75xx_pwm->chip.dev = &pdev->dev;
	cs75xx_pwm->chip.ops = &g2_pwm_ops;
	cs75xx_pwm->chip.base = -1;
	cs75xx_pwm->chip.npwm = 1;
	cs75xx_pwm->chip.can_sleep = false;

        ret = pwmchip_add(&cs75xx_pwm->chip);
        if (ret < 0) {
                dev_err(&pdev->dev, "failed to add pwm chip %d\n", ret);
                return ret;
	}

	platform_set_drvdata(pdev, cs75xx_pwm);
	return 0;

err_free:
	kfree(cs75xx_pwm);
	return ret;
}

static int g2_pwm_remove(struct platform_device *pdev)
{
	struct g2_pwm_device *cs75xx_pwm;

	cs75xx_pwm = platform_get_drvdata(pdev);
	if (cs75xx_pwm == NULL)
		return -ENODEV;

	iounmap(cs75xx_pwm->io_base);

	kfree(cs75xx_pwm);
	return 0;
}

static struct platform_driver g2_pwm_driver = {
	.driver		= {
		.name	= "cs75xx-pwm",
		.owner	= THIS_MODULE,
	},
	.probe		= g2_pwm_probe,
	.remove		= g2_pwm_remove,
	.id_table	= g2_pwm_id_table,
};

static int __init g2_pwm_init(void)
{
	return platform_driver_register(&g2_pwm_driver);
}
arch_initcall(g2_pwm_init);

static void __exit g2_pwm_exit(void)
{
	platform_driver_unregister(&g2_pwm_driver);
}
module_exit(g2_pwm_exit);

MODULE_ALIAS("platform:g2-pwm");
MODULE_LICENSE("GPL v2");
