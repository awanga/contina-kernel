/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _SPI_AGENT_H_
#define _SPI_AGENT_H_

struct spi_slaves {
	struct spi_driver slave;
	struct spi_device *spi;
	int (*spi_tx)(int cs, void *tx_param);
	int (*spi_rx)(int cs, void *rx_param);
	u8 bits_per_word;
};

extern struct spi_slaves spi_slave_dbs[];

#endif /* _SPI_AGENT_H_ */
