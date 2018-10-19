/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _CS75XX_SPI_H_
#define _CS75XX_SPI_H_

#include <linux/spi/spi.h>

#define CS75XX_SPI_NUM_CHIPSELECT	5
#define CS75XX_SPI_CLOCK_DIV		10

// G2 SPI mode selection.
#define CS75XX_SPI_MODE_ISAM		(0x1<<3)
#define CS75XX_SPI_MODE_CMDS		(0x1<<2)

//G2 chip select mask
#define CS75XX_SPI_CS_MASK		(0x1f<<8)


#define CS75XX_SPI_CTLR_NAME	"cs75xx-spi"

struct cs75xx_spi_info {
	u32	tclk;		/* no <linux/clk.h> support yet */
	u32	divider;
	u32	timeout;
};

int cs75xx_spi_fast_transfer(struct spi_device *slave, struct spi_message *m);

#endif /*_CS75XX_SPI_H_*/
