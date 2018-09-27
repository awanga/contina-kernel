
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <mach/cs75xx_spi.h>
#include <mach/spi_agent.h>

#define SLAVE_NUM	CS75XX_SPI_NUM_CHIPSELECT

#define SLAVE_0	0
#define SLAVE_1	1
#define SLAVE_2	2
#define SLAVE_3	3
#define SLAVE_4	4

static int slave_probe(struct spi_device *spi);
static int slave_remove(struct spi_device *spi);
static int slave_tx(int cs, void *tx_param);
static int slave_rx(int cs, void *rx_param);

struct spi_slaves spi_slave_dbs[SLAVE_NUM] =
{
#ifdef CONFIG_CORTINA_ENGINEERING
	[SLAVE_0] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
	[SLAVE_1] = {
		.slave = {
			.driver = {
				.name = "ve880_slot0",
				.owner	= THIS_MODULE,
			},
			.probe		= slave_probe,
			.remove 	= slave_remove,
		},
		.spi_tx = slave_tx,
		.spi_rx = slave_rx,
		.bits_per_word = 8,
	},
	[SLAVE_2] = {
		.slave = {
			.driver = {
				.name = "ve880_slot1",
				.owner	= THIS_MODULE,
			},
			.probe		= slave_probe,
			.remove 	= slave_remove,
		},
		.spi_tx = slave_tx,
		.spi_rx = slave_rx,
		.bits_per_word = 8,
	},
	[SLAVE_3] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
	[SLAVE_4] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
#else
#ifdef CONFIG_CORTINA_ENGINEERING_S
	[SLAVE_0] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},

	[SLAVE_1] = {
		.slave = {
			.driver = {
				.name = "ve880_slot0",
				.owner	= THIS_MODULE,
			},
			.probe		= slave_probe,
			.remove 	= slave_remove,
		},
		.spi_tx = slave_tx,
		.spi_rx = slave_rx,
		.bits_per_word = 8,
	},
#else
	[SLAVE_0] = {
		.slave = {
			.driver = {
				.name = "ve880_slot0",
				.owner	= THIS_MODULE,
			},
			.probe		= slave_probe,
			.remove 	= slave_remove,
		},
		.spi_tx = slave_tx,
		.spi_rx = slave_rx,
		.bits_per_word = 8,
	},
	[SLAVE_1] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
#endif
	[SLAVE_2] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
	[SLAVE_3] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
	[SLAVE_4] = {
		.slave = {
			.driver = {
				.name = NULL,
			},
		},
	},
#endif
};
EXPORT_SYMBOL(spi_slave_dbs);

static int slave_tx(int cs, void *tx_param)
{
	struct spi_transfer t = {
		.tx_buf = tx_param,
		.len = 1
	};
	struct spi_message m = {
		.spi = spi_slave_dbs[cs].spi
	};
	int rc;

	if ((cs < 0) || (cs >= SLAVE_NUM) || (tx_param == NULL))
		return -EINVAL;
	if (spi_slave_dbs[cs].spi == NULL)
		return -ENXIO;

	INIT_LIST_HEAD(&m.transfers);
	spi_message_add_tail(&t, &m);
	//rc = spi_sync(spi_slave_dbs[cs].spi, &m);
	rc = cs75xx_spi_fast_transfer(spi_slave_dbs[cs].spi, &m);

	return (rc >= 0 ? 0 : rc);
}

static int slave_rx(int cs, void *rx_param)
{
	struct spi_transfer t = {
		.rx_buf = rx_param,
		.len = 1
	};
	struct spi_message m = {
		.spi = spi_slave_dbs[cs].spi
	};
	int rc;

	if ((cs < 0) || (cs >= SLAVE_NUM) || (rx_param == NULL))
		return -EINVAL;
	if (spi_slave_dbs[cs].spi == NULL)
		return -ENXIO;

	INIT_LIST_HEAD(&m.transfers);
	spi_message_add_tail(&t, &m);
	//rc = spi_sync(spi_slave_dbs[cs].spi, &m);
	rc = cs75xx_spi_fast_transfer(spi_slave_dbs[cs].spi, &m);

	return (rc >= 0 ? 0 : rc);
}

static int slave_probe(struct spi_device *spi)
{
	int ret;

	spi_slave_dbs[spi->chip_select].spi = spi;

	printk("modalias = %s, chip_select = %d\n", spi->modalias, spi->chip_select);

	spi->bits_per_word = spi_slave_dbs[spi->chip_select].bits_per_word;
	ret = spi_setup(spi);
	if (ret < 0) {
		printk("spi_setup() fail(%d)!\n", ret);
		return ret;
	}

	return 0;
}

static int slave_remove(struct spi_device *spi)
{
	spi_slave_dbs[spi->chip_select].spi = NULL;

	return 0;
}

static int __init spiagent_init(void)
{
	int i;

	printk("spiagent_init\n");

	for (i = 0; i < SLAVE_NUM; i++)
		if (spi_slave_dbs[i].slave.driver.name != NULL) {
			printk("spi_slave_dbs[%d].slave.driver.name = %s\n", i, spi_slave_dbs[i].slave.driver.name);
			spi_register_driver(&spi_slave_dbs[i].slave);
		}

	return 0;
}
module_init(spiagent_init);

static void __exit spiagent_exit(void)
{
	int i;

	for (i = 0; i < SLAVE_NUM; i++)
		if (spi_slave_dbs[i].slave.driver.name != NULL)
			spi_unregister_driver(&spi_slave_dbs[i].slave);

}
module_exit(spiagent_exit);

