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

static const struct platform_device_id pwm_id_table[] = {
	{ "cs75xx-pwm", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, pwm_id_table);

/* PWM registers and bits definitions */
#define PWM_PERIOD	(0x00)
#define PWM_DUTY	(0x04)

struct pwm_device {
	struct platform_device	*pdev;

	const char	*label;
	void __iomem	*io_base;

	unsigned int	use_count;
	unsigned int	pwm_id;
};

int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	if (pwm == NULL || period_ns == 0 || duty_ns > period_ns)
		return -EINVAL;

	__raw_writel((period_ns*150000), pwm->io_base + PWM_PERIOD);
	__raw_writel((duty_ns*150000), pwm->io_base + PWM_DUTY);

	return 0;
}
EXPORT_SYMBOL(pwm_config);

int pwm_enable(struct pwm_device *pwm)
{
	return 0;
}
EXPORT_SYMBOL(pwm_enable);

void pwm_disable(struct pwm_device *pwm)
{
}
EXPORT_SYMBOL(pwm_disable);

static DEFINE_MUTEX(pwm_lock);
static struct pwm_device* cs75xx_pwm;

struct pwm_device *pwm_request(int pwm_id, const char *label)
{
	int found = 0;

	mutex_lock(&pwm_lock);

	if (cs75xx_pwm->pwm_id == pwm_id) {
		found = 1;
	}

	if (found) {
		if (cs75xx_pwm->use_count == 0) {
			cs75xx_pwm->use_count++;
			cs75xx_pwm->label = label;
		} else
			cs75xx_pwm = ERR_PTR(-EBUSY);
	} else
		cs75xx_pwm = ERR_PTR(-ENOENT);

	mutex_unlock(&pwm_lock);
	return cs75xx_pwm;
}
EXPORT_SYMBOL(pwm_request);

void pwm_free(struct pwm_device *pwm)
{
	mutex_lock(&pwm_lock);

	if (pwm->use_count) {
		pwm->use_count--;
		pwm->label = NULL;
	} else
		pr_warning("PWM device already freed\n");

	mutex_unlock(&pwm_lock);
}
EXPORT_SYMBOL(pwm_free);

static int __devinit pwm_probe(struct platform_device *pdev)
{
	struct resource *r;
	int ret = 0;

	cs75xx_pwm = kzalloc(sizeof(struct pwm_device), GFP_KERNEL);
	if (cs75xx_pwm == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	cs75xx_pwm->use_count = 0;
	cs75xx_pwm->pwm_id = 0;
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

	platform_set_drvdata(pdev, cs75xx_pwm);
	return 0;

err_free:
	kfree(cs75xx_pwm);
	return ret;
}

static int __devexit pwm_remove(struct platform_device *pdev)
{
	struct pwm_device *pwm;
	struct resource *r;

	pwm = platform_get_drvdata(pdev);
	if (pwm == NULL)
		return -ENODEV;

	iounmap(pwm->io_base);

	kfree(pwm);
	return 0;
}

static struct platform_driver pwm_driver = {
	.driver		= {
		.name	= "cs75xx-pwm",
		.owner	= THIS_MODULE,
	},
	.probe		= pwm_probe,
	.remove		= __devexit_p(pwm_remove),
	.id_table	= pwm_id_table,
};

static int __init pwm_init(void)
{
	return platform_driver_register(&pwm_driver);
}
arch_initcall(pwm_init);

static void __exit pwm_exit(void)
{
	platform_driver_unregister(&pwm_driver);
}
module_exit(pwm_exit);

MODULE_LICENSE("GPL v2");
