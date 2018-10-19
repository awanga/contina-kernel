/*
 *  linux/drivers/video/cs752x_clcdfb.h
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  Goldengate PrimeCell PL111 Color LCD Controller
 */
#ifndef __CS75XX_CLCDFB_H__
#define __CS75XX_CLCDFB_H__

#include <linux/fb.h>

struct cs75xx_lcd_panel {
        struct fb_videomode     mode;
        signed short            width;  /* width in mm */
        signed short            height; /* height in mm */
};

struct cs75xx_monitor_info {
        int status; //0: no data   1:get data
        int type;   //0:dvi  1:hdmi
        struct fb_monspecs specs;
        char edid[256];
};

int get_hdmi_info(struct fb_info *fb_info);

struct cs75xx_fb_plat_data {
        int (*get_clk_div)(unsigned int target_clk);
};

static const struct cs75xx_lcd_panel panels[]={
	{
		.mode = {  
			.name = "PANEL56",	/* Select InnoLux AT056TN53 */
			.refresh = 60,	/* 1000000/(31.76 us *525 line) */
			.xres = 640,
			.yres = 480,
			.pixclock = 39700,
			/* PXLCLK clock time: 39.7 ns */
			.left_margin = 134, /* unit: pixel clk */
			.right_margin = 16, /* unit: pixel clk */
			.upper_margin = 11, /* unit: pixel line */
			.lower_margin = 32, /* unit: pixel line */
			.hsync_len = 10,	/* unit: pixel clk */
			.vsync_len = 2, /* unit: pixel line */
			.sync = 0,
			.vmode = FB_VMODE_NONINTERLACED,
		},
		.width = 70,			/* in mm for hx8238a panel */
		.height = 52,			/* in mm for hx8238a panel */
	},
	{
		.mode = {
			.name			= "PANEL35",
			.refresh		= 60,
			.xres			= 320,
			.yres			= 240,
			.pixclock		= 37000,
			.left_margin	= 68,	// unit: pixel clk
			.right_margin	= 20,	// unit: pixel clk
			.upper_margin	= 18,	// unit: pixel line
			.lower_margin	= 4,	// unit: pixel line
			.hsync_len		= 2,	// unit: pixel clk
			.vsync_len		= 2,	// unit: pixel line
			.sync			= 0,
			.vmode			= FB_VMODE_NONINTERLACED,
		},
		.width	= 70,
		.height = 52,
	},				
	{
		.mode	= {
			.name					= "VGA",
			.refresh					= 60,
			.xres					= 640,
			.yres					= 480,
//			.pixclock					= 25000000,
			.pixclock						= 39682,
			.left_margin				= 48,
			.right_margin				= 16,
			.upper_margin				= 33,
			.lower_margin				= 10,
			.hsync_len				= 96,
			.vsync_len				= 2,
			.sync					= 0,
			.vmode					= FB_VMODE_NONINTERLACED,
			},
		.width	= -1,
		.height = -1,
	},
	{
		.mode	= {
				.name					= "HD480",
				.refresh					= 60,
				.xres					= 720,
				.yres					= 480,
//				.pixclock					= 27000000,
				.pixclock					= 37000,
				.left_margin				= 60,
				.right_margin				= 16,
				.upper_margin				= 30,
				.lower_margin				= 9,
				.hsync_len				= 62,
				.vsync_len				= 6,
				.sync					= 0,
				.vmode					= FB_VMODE_NONINTERLACED,
		},
		.width	= -1,
		.height = -1,
	},
	{
		.mode	= {
			.name				= "SVGA",
			.refresh				= 60,
			.xres				= 800,
			.yres				= 600,
//			.pixclock				= 25000000,
			.pixclock						= 25000,
			.left_margin			= 88,
			.right_margin			= 40,
			.upper_margin			= 23,
			.lower_margin			= 1,
			.hsync_len			= 128,
			.vsync_len			= 4,
			.sync						= 0,
			.vmode				= FB_VMODE_NONINTERLACED,
		},
		.width	= -1,
		.height = -1,
	},
	{
		.mode	= {
				.name				= "XVGA",
				.refresh				= 70,
				.xres				= 1024,
				.yres				= 768,
				.pixclock						= 13333,
//				.pixclock				= 65000000,
				.left_margin			= 144,
				.right_margin			= 24,
				.upper_margin			= 29,
				.lower_margin			= 3,
				.hsync_len			= 136,
				.vsync_len			= 6,
				.sync				= 0,
				.vmode				= FB_VMODE_NONINTERLACED,
		},
		.width	= -1,
		.height = -1,
	},
/*
	{
		// 
		.mode   = {
			.name                                   = "HD720@50",
			.refresh                                 = 50,
			.xres                                     = 1280,
			.yres                                     = 720,
			.pixclock                                = 75000000,
			.left_margin                            = 440,
			.right_margin                          = 220,
			.upper_margin                         = 20,
			.lower_margin                         = 5,
			.hsync_len                             = 20,
			.vsync_len                             = 5,
			.sync                                    = 0,
			.vmode                                  = FB_VMODE_NONINTERLACED,
		},
		.width  = -1,
		.height = -1,
	},

*/
	{
	// 
		.mode	= {
				.name					= "HD720",
				.refresh						= 60,
				.xres						= 1280,
				.yres						= 720,
//				.pixclock						= 75000000,
				.pixclock						= 13468,
				.left_margin					= 220,
				.right_margin					= 110,
				.upper_margin					= 20,
				.lower_margin					= 5,
				.hsync_len					= 40,
				.vsync_len					= 5,
				.sync						= 0,
//				.sync						= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
				.vmode						= FB_VMODE_NONINTERLACED,
		},
		.width	= -1,
		.height = -1,
	},
	{
		.mode	= {
				.name					= "HD1080",
				.refresh					= 60,
				.xres					= 1920,
				.yres					= 1080,
//				.pixclock					= 75000000,
				.pixclock						= 6734,
				.left_margin				= 44,
				.right_margin				= 148,
				.upper_margin				= 2,
				.lower_margin				= 15,
				.hsync_len				= 44,
				.vsync_len				= 5,
				.sync						= 0,
//				.sync					= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
				.vmode					= FB_VMODE_NONINTERLACED,
		},
		.width	= -1,
		.height = -1,
	},

};

#endif		/* __CS75XX_CLCDFB_H__ */
