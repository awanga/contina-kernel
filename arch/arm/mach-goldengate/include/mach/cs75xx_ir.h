/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _CS752X_IR_H
#define _CS752X_IR_H

#include <linux/ioctl.h>

#define CS75XX_IR_NAME		"cs75xx-ir"

#define VCR_KEY_POWER		0x613E609F
#define TV1_KEY_POWER		0x40040100
#define TV1_KEY_POWER_EXT	0xBCBD
#define DVB_KEY_POWER		0x38000000

#define VCR_H_ACT_PER		(16-1)
#define VCR_L_ACT_PER		(8-1)
#define VCR_DATA_LEN		(32-1)
#define TV1_H_ACT_PER		(8-1)
#define TV1_L_ACT_PER		(4-1)
#define TV1_DATA_LEN		(48-1)

#define VCR_BAUD		540 	/* us */
#define TV1_BAUD		430	/* us */
#define DVB_BAUD		830	/* us */

#ifdef CONFIG_CORTINA_FPGA
#define	EXT_CLK_SRC		104	/* MHz */
#define	EXT_CLK_DIV		12
#else
#define	EXT_CLK			24	/* MHz */
#endif

#define VCR_PROTOCOL		0x0	/* KOKA KUC-100 VCR-33 */
#define TV1_PROTOCOL		0x1	/* KOKA KUC-100 TV1-26 */
#define DVB_PROTOCOL		0x2	/* white brand DVB-T */
#define CUSTOM_RC		0x3	/* customize RC */

#endif /* _CS752X_IR_H */

