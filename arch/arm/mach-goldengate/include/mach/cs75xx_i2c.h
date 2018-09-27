/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _CS75XX_I2C_H_
#define _CS75XX_I2C_H_

#include <linux/types.h>

#define CS75XX_I2C_CTLR_NAME	"cs75xx-i2c"

/* i2c Platform Device, Driver Data */
struct cs75xx_i2c_pdata {
	u32	freq_rcl;	/* Reference Clock */
	u32	freq_scl;	/* Serial Clock */
	u16	timeout;	/* In milliseconds */
	u16	retries;	/* retry count */
};

#endif /*_CS75XX_I2C_H_*/
