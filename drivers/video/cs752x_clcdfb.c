/*
 *  linux/drivers/video/cs752x_clcdfb.c
 *
 * Copyright (C) 2001 ARM Limited, by David A Rusling
 * Updated to 2.5, Deep Blue Solutions Ltd.
 *
 * Copyright (c) Cortina-Systems Limited 2012-2015.  All rights reserved.
 *                Ryan Chen <ryan.chen@cortina-systems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  Goldengate PrimeCell PL111 Color LCD Controller
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/hardirq.h>      
#include <linux/clk.h>
#include <linux/hardirq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/sizes.h>

#include <mach/platform.h>
#include <linux/dma-mapping.h>
#include <mach/cs752x_clcd_regs.h>
#include <mach/cs752x_clcdfb.h>

#undef CS75XX_LCD_INFO
#define DBGPRINT(level, format, ...)

static char *panel = "PANEL35";

module_param(panel, charp, 0644);
MODULE_PARM_DESC(panel, "Panel list : VGA,HD480,SVGA,XVGA,HD720");

#ifdef  CONFIG_DOUBLE_BUFFER
#define NUMBER_OF_BUFFERS 2
#else
#define NUMBER_OF_BUFFERS 1
#endif

static u32 master_error=0;
static u32 fifo_underflow=0;

struct lcd_regs {
	u32			tim0;
	u32			tim1;
	u32			tim2;
	u32			tim3;
	u32			cntl;
};

struct cs75xx_lcd_fb {
	struct platform_device *pdev;
	struct fb_info		*info;	
	struct cs75xx_lcd_panel *panel;
	struct resource *reg_res;
	struct resource *fb_res;
	void __iomem *base;
	void __iomem *addr;
	struct lcd_regs regs;
	u32	cmap[16];
	int addr_assign;
	int irq;
	int bpp;
	int hdmi_en;
	u32 target_clk;   //khz
	u32 clk_div;
	struct cs75xx_fb_plat_data *fb_plat_data;
};

static int cs75xx_lcdfb_check(struct cs75xx_lcd_fb *sfb, struct fb_var_screeninfo *var)
{
	var->xres_virtual = var->xres = (var->xres + 15) & ~15;
	var->yres = (var->yres + 1) & ~1;	
	var->yres_virtual=var->yres*NUMBER_OF_BUFFERS;

	/*
	 * You can't change the grayscale setting, and
	 * we can only do non-interlaced video.
	 */
	if (var->grayscale != sfb->info->var.grayscale ||
	    (var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED)
		return -EINVAL;

	var->nonstd = 0;
	var->accel_flags = 0;

	return 0;
}

/*
 * Unfortunately, the enable/disable functions may be called either from
 * process or IRQ context, and we _need_ to delay.  This is _not_ good.
 */
static inline void clcdfb_msleep(unsigned int ms)
{
	if (in_atomic())
	{
		mdelay(ms);
	}
	else
	{
		msleep(ms);
	}
}

#ifdef CS75XX_LCD_INFO 
static irqreturn_t cs75xx_lcd_irq(int irq, void *parm)
{
	struct cs75xx_lcd_fb *sfb=parm;

	unsigned int lcd_int_pending;

	lcd_int_pending = readl(sfb->base + CLCD_MIS);

	if (lcd_int_pending) {
		if (lcd_int_pending & LNBUMIS) 
		{	/* LCD next base address update*/
//			wake_up_interruptible(&g2_lcd_wait_q);
		}
		/* AHB master error, intr sts */
		if (lcd_int_pending & MBERRORRIS)	
		{
//			printk("AHB master error\n");
			master_error++;
		}
		/* FIFO underflow, intr sts */
		if (lcd_int_pending & FUFMIS)
		{
			fifo_underflow++;
//			printk("FIFO underflow\n");
		}
	}
	writel(lcd_int_pending , sfb->base + CLCD_IENB);


	return IRQ_HANDLED;
}
#endif
	
static inline void cs75xx_set_start(struct cs75xx_lcd_fb *sfb)
{
	unsigned int ustart = sfb->info->fix.smem_start;

	ustart += sfb->info->var.yoffset * sfb->info->fix.line_length;
	writel(ustart, sfb->base + CLCD_UBAS);
}

static void cs75xx_lcdfb_disable(struct cs75xx_lcd_fb *sfb)
{

		u32 val;
	
		val = readl(sfb->base + CLCD_CNTL);
	
		if (val & CNTL_LCDPWR) {
			val &= ~CNTL_LCDPWR;
			writel(val, sfb->base + CLCD_CNTL);
		}
	
		if (val & CNTL_LCDEN) {
			val &= ~CNTL_LCDEN;
			writel(val, sfb->base + CLCD_CNTL);
		}

}

static void cs75xx_lcdfb_enable(struct cs75xx_lcd_fb *sfb)
{
	writel(sfb->regs.cntl |CNTL_LCDPWR | CNTL_LCDEN , sfb->base + CLCD_CNTL);
	
#ifdef CS75XX_LCD_INFO
	writel(MBERRORRIS |LNBUMIS , sfb->base + CLCD_IENB);	
#endif
}

static int cs75xx_lcdfb_set_bitfields(struct cs75xx_lcd_fb *fb,
				       struct fb_var_screeninfo *var)
{
	int ret = 0;
	memset(&var->transp, 0, sizeof(var->transp));
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		var->red.length = var->bits_per_pixel;
		var->red.offset = 0;
		var->green.length = var->bits_per_pixel;
		var->green.offset = 0;
		var->blue.length = var->bits_per_pixel;
		var->blue.offset = 0;
		break;
	case 16:
		var->red.length = 5;
		var->blue.length = 5;

		/*
		 * Green length can be 5 or 6 depending whether
		 * we're operating in RGB555 or RGB565 mode.
		 */
		if (var->green.length != 5 && var->green.length != 6)
			var->green.length = 6;
		break;
	case 24:
	case 32:
#if 0
			var->red.length = 8;
			var->green.length = 8;
			var->blue.length = 8;
				break;
#else
			var->red.length 	= 8;
			var->green.length	= 8;
			var->blue.length	= 8;
			var->transp.length=8;
			var->transp.offset=24;
			break;
#endif

	default:
		ret = -EINVAL;
		break;
	}

	DBGPRINT(1, "var->bits_per_pixel=0x%08x, ret=0x%08x\n",
		 var->bits_per_pixel, ret);

	/*
	 * >= 16bpp displays have separate colour component bitfields
	 * encoded in the pixel data.  Calculate their position from
	 * the bitfield length defined above.
	 */
	if (ret == 0 && var->bits_per_pixel >= 16) {
#if 1
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
#else
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset =
			var->green.offset + var->green.length;
#endif
	}
	return ret;
}

static int cs75xx_lcdfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct cs75xx_lcd_fb *sfb = info->par;
	int ret = -EINVAL;
	ret = cs75xx_lcdfb_check(sfb, var);

	if (ret == 0 &&
	    var->xres_virtual * var->bits_per_pixel / 8 *
	    var->yres_virtual > sfb->info->fix.smem_len)
		ret = -EINVAL;

	if (ret == 0)
		ret = cs75xx_lcdfb_set_bitfields(sfb, var);

	return ret;
}

static int quick_addr;  /* Pan the display if device supports it. */
static int cs75xx_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct cs75xx_lcd_fb *sfb = info->par;
	quick_addr = info->var.yres* info->fix.line_length + info->fix.smem_start;
	
	if(var->yoffset) {
		writel(quick_addr, sfb->base + CLCD_UBAS);
	//	while(readl(sfb->base + CLCD_LCUR) < quick_addr);
	}
	else {
		writel(info->fix.smem_start, sfb->base + CLCD_UBAS);
	//	while(readl(sfb->base + CLCD_LCUR) > quick_addr);
	}
	return 0;
}

static int cs75xx_lcdfb_set_par(struct fb_info *info)
{
	struct cs75xx_lcd_fb *sfb = info->par;
	struct fb_var_screeninfo *var = &info->var;
	u32 val, cpl;

//	printk("pixel clock :%d",var->pixclock);

	info->fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;

	if (var->bits_per_pixel <= 8)
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else
		info->fix.visual = FB_VISUAL_TRUECOLOR;


	//decode
	{ 

	/*
	 * Program the CLCD controller registers and start the CLCD
	 */
#if 0
	/*	A0: PPL 6bits: tim0 bit[7:2]
		      HFP 8bits: tim0 bit[23:16]	*/
	val = ((var->xres / 16) - 1) << 2;	/* PPL: Pixels-per-line */
	val |= (var->right_margin - 1) << 16;	/* HFP: Horizontal front porch */
#else
	/*	A1: PPL extends to 7bits.  Bit6:tim0 bit[0] and bit5:0 is at tim0 bit[7:2]
		      HFP extends to 9bits.  Bit8:tim0 bit[1] and bit7:0 is at tim0 bit[23:16]	
		SJ added the following codes for G2 A1 ECO chip. 20120328	*/
	
	u32 ppl;
	u32 hfp;
	ppl = (var->xres / 16) - 1;
	hfp = var->right_margin - 1;
	val = (((ppl & 0x3f) << 2) | ((ppl & 0x40) >> 6));	/* PPL: Pixels-per-line */
	val |= (((hfp & 0xff) << 16) | (((hfp & 0x100) >> 8) << 1));	/* HFP: Horizontal front porch */
#endif
	val |= (var->hsync_len - 1) << 8;		/* HSW: Horizontal synchronization pulse width */
	val |= (var->left_margin - 1) << 24;	/* HBP: Horizontal back porch */
	sfb->regs.tim0 = val;

	val = var->yres;  /* LPP: Lines per panel */
	if (sfb->regs.cntl & CNTL_LCDDUAL)
		val /= 2;
	val -= 1; 				/* Program to the number of lines required, minus one */
	val |= (var->vsync_len - 1) << 10;/* VSW: Vertical synchronization pulse width */
	val |= var->lower_margin << 16;	/* VFP: Vertical front porch */
	val |= var->upper_margin << 24;	/* VBP: Vertical back porch */
	sfb->regs.tim1 = val;
	
	val = sfb->regs.tim2;
	val &= ~(TIM2_IHS | TIM2_IVS);

	val |= var->sync & FB_SYNC_HOR_HIGH_ACT  ? 0 : TIM2_IHS; /* Invert hor sync */
	val |= var->sync & FB_SYNC_VERT_HIGH_ACT ? 0 : TIM2_IVS; /* Invert ver sync */

	cpl = var->xres_virtual;  	/* cpl: Clock per line. This field specifies the number of actual CLCP clocks to the LCD panel on each line. */

	val &= ~0x3ff8000;	
	if (sfb->regs.cntl & CNTL_LCDTFT)	/* TFT */
		/* / 1 */;
	else if (!var->grayscale)		/* STN color */
		cpl = cpl * 8 / 3;
	else if (sfb->regs.cntl & CNTL_LCDMONO8) /* STN monochrome, 8bit */
		cpl /= 8;
	else					/* STN monochrome, 4bit */
		cpl /= 4;

#if 0
	printk("A0 2 \n");
	/*	A0: CPL 10bits at tim2 bit[25:16]	*/
	sfb->regs.tim2 = val | ((cpl - 1) << 16);	/* Clock and Singal Polarity Control Register */
#else		
	/*	A1: CPL extends to 11bits at tim2 bit[15] + bit[25:16]	*/
	sfb->regs.tim2 = val | (((cpl - 1) & 0x3ff) << 16) | ((((cpl - 1) & 0x400) >> 10) << 15);
#endif	
	sfb->target_clk=1000*1000*1000/(var->pixclock);
//	printk("var->pixclock = %d ,sfb->target_clk = %d \n",var->pixclock,sfb->target_clk);

	sfb->clk_div = sfb->fb_plat_data->get_clk_div(sfb->target_clk);
	if(sfb->clk_div == -1) {
		sfb->regs.tim2 |= TIM2_BCD;
	} else {
		sfb->regs.tim2 &= ~TIM2_BCD;
		sfb->regs.tim2 &= ~(0x1f | (0x1f << 27));
		sfb->regs.tim2 |= sfb->clk_div& 0x1f; //PCD_LO 5bit
		sfb->regs.tim2 |= ((sfb->clk_div>>5) & 0x1f ) << 27; //PCD_HI 5bit		
	}

//	printk("clk div = %x \n",sfb->clk_div);



	sfb->regs.tim3 = 1;
// 	orignal setting
	val = CNTL_LCDTFT | CNTL_LCDVCOMP(3) | CNTL_WATERMARK |CNTL_BGR;

	if (var->grayscale)
		val |= CNTL_LCDBW;

	switch (var->bits_per_pixel) {
	case 1:
		val |= CNTL_LCDBPP1;
		break;
	case 2:
		val |= CNTL_LCDBPP2;
		break;
	case 4:
		val |= CNTL_LCDBPP4;
		break;
	case 8:
		val |= CNTL_LCDBPP8;
		break;
	case 16:		//Must check HW layout . 
		if (var->green.length == 5) {
				val |= CNTL_LCDBPP16;
		} else {
				val |= CNTL_LCDBPP16_565;
		}
		break;
	case 32:
		val |= CNTL_LCDBPP24;
		break;
	}

	} //decode end
	
	sfb->regs.cntl=val;
	cs75xx_lcdfb_disable(sfb);

	writel(sfb->regs.tim0, sfb->base + CLCD_TIM0);
	writel(sfb->regs.tim1, sfb->base + CLCD_TIM1);
	writel(sfb->regs.tim2, sfb->base + CLCD_TIM2);
	writel(sfb->regs.tim3, sfb->base + CLCD_TIM3);

	writel(0, sfb->base + CLCD_IENB);

	cs75xx_set_start(sfb);

	cs75xx_lcdfb_enable(sfb);

#ifdef DEBUG
	printk(KERN_INFO "CLCD: Registers set to\n"
	       KERN_INFO "  %08x %08x %08x %08x\n"
	       KERN_INFO "  %08x %08x %08x %08x\n",
		readl(sfb->base + CLCD_TIM0), readl(sfb->base + CLCD_TIM1),
		readl(sfb->base + CLCD_TIM2), readl(sfb->base + CLCD_TIM3),
		readl(sfb->base + CLCD_UBAS), readl(sfb->base + CLCD_LBAS),
		readl(sfb->base + CLCD_IENB), readl(sfb->base + CLCD_CNTL));
#endif

	return 0;
}

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	unsigned int mask = (1 << bf->length) - 1;
	return (val >> (16 - bf->length) & mask) << bf->offset;
}

/*
 *  Set a single color register. The values supplied have a 16 bit
 *  magnitude.  Return != 0 for invalid regno.
 */
static int cs75xx_lcdfb_setcolreg(unsigned int regno, unsigned int red, unsigned int green,
		 unsigned int blue, unsigned int transp, struct fb_info *info)
{
	struct cs75xx_lcd_fb *sfb = info->par;

	if (regno < 16)
		sfb->cmap[regno] = convert_bitfield(transp, &info->var.transp) |
				  convert_bitfield(blue, &info->var.blue) |
				  convert_bitfield(green, &info->var.green) |
				  convert_bitfield(red, &info->var.red);

	if (info->fix.visual == FB_VISUAL_PSEUDOCOLOR && regno < 256) {
		int hw_reg = CLCD_PALETTE + ((regno * 2) & ~3);
		u32 val, mask, newval;

		newval  = (red >> 11)  & 0x001f;
		newval |= (green >> 6) & 0x03e0;
		newval |= (blue >> 1)  & 0x7c00;

		/*
		 * 3.2.11: if we're configured for big endian
		 * byte order, the palette entries are swapped.
		 */
		if (sfb->regs.cntl & CNTL_BEBO)
			regno ^= 1;

		if (regno & 1) {
			newval <<= 16;
			mask = 0x0000ffff;
		} else {
			mask = 0xffff0000;
		}

		val = readl(sfb->base + hw_reg) & mask;
		writel(val | newval, sfb->base + hw_reg);
	}

	return regno > 255;

}

/*
 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */

static int cs75xx_lcdfb_blank(int blank_mode, struct fb_info *info)
{
	struct cs75xx_lcd_fb *sfb = info->par;
	if (blank_mode != 0) {
		cs75xx_lcdfb_disable(sfb);
	} 
	else {
		cs75xx_lcdfb_enable(sfb);
	}
	return 0;
}


static struct fb_ops cs75xx_lcdfb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= cs75xx_lcdfb_check_var,
	.fb_set_par	= cs75xx_lcdfb_set_par,
	.fb_setcolreg	= cs75xx_lcdfb_setcolreg,
	.fb_blank		= cs75xx_lcdfb_blank,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_pan_display = cs75xx_fb_pan_display,
};

static int cs75xx_lcd_setup(struct cs75xx_lcd_fb *sfb)
{
	char *options = NULL;
	char *this_opt;
	char tmp[128];
	char *tmp_opt;
	char name[10];
	int count=sizeof(panels)/sizeof(struct cs75xx_lcd_panel);
	int i;
	//TODO
	strcpy(name,panel);
	sfb->bpp=16;
	sfb->info->fix.smem_start=0;
	sfb->addr_assign=0;
 	
	fb_get_options("cs75xxfb",&options);
	
	if(options) {
	  	strcpy(tmp, options);
  		tmp_opt=tmp;
	  	while ((this_opt = strsep(&tmp_opt, ",")) != NULL) {
  			if (!strncmp(this_opt, "type=", 5)) {
  				strcpy(name,this_opt+5);
  			}
	  		else if(!strncmp(this_opt, "bpp=", 4)) {
  				if(!strncmp(this_opt+4, "32", 2))
  						sfb->bpp = 32;
	  			else if(!strncmp(this_opt+4, "16", 2))
  						sfb->bpp = 16;
  			}
  			else if(!strncmp(this_opt, "addr=", 5)) {
	  			sfb->info->fix.smem_start = PHYS_OFFSET + memparse(this_opt + 5, NULL);
  				sfb->addr_assign=1;
  			}
  		}
  	}
	
	for(i=0;i<count;i++) {
  	if(!strcmp(name,panels[i].mode.name)) {
  		sfb->panel=(struct cs75xx_lcd_panel *)&panels[i];
  		break;
  	}
  }


	return 0;
}


#ifdef CONFIG_HDMI_ANX9805
static ssize_t cs75xx_get_hdmi_info(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	ssize_t len = 0;
	int rc;

	rc=get_hdmi_info(fb_info);
	if(rc==-1)
		len=sprintf(buf, "UNPLUG\n");
	else if(rc==0)
		len=sprintf(buf, "PLUG\n");
	else
		len=sprintf(buf, "UNKNOWN\n");
	return len;
}
#endif

static ssize_t show_lcd_enable(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct fb_info *info = dev_get_drvdata(device);
	struct cs75xx_lcd_fb *sfb = info->par;
	if(readl(sfb->base + CLCD_CNTL)& CNTL_LCDEN)
		return sprintf(buf, "%d\n",1);
	else
		return sprintf(buf, "%d\n",0);
}

static ssize_t store_lcd_enable(struct device *device,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct fb_info *info = dev_get_drvdata(device);
	struct cs75xx_lcd_fb *sfb = info->par;
	
	if(buf[0]=='1')
		cs75xx_lcdfb_enable(sfb);
	else 
		cs75xx_lcdfb_disable(sfb);
		
	return count;
}

static ssize_t show_pix_clk(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct fb_info *info = dev_get_drvdata(device);
	struct cs75xx_lcd_fb *sfb = info->par;

	return sprintf(buf, "target_clk=%d \n",sfb->target_clk);
}

#ifdef CS75XX_LCD_INFO
static ssize_t lcd_info(struct device *device,
                             struct device_attribute *attr, char *buf)
{
        struct fb_info *info = dev_get_drvdata(device);
        struct cs75xx_lcd_fb *sfb = info->par;

        return sprintf(buf, "master error : %d, fifo_underflow : %d \n",master_error,fifo_underflow);
}
#endif

static struct device_attribute device_attrs[] = {
	__ATTR(lcd_enable, S_IRUGO | S_IWUGO, show_lcd_enable, store_lcd_enable),
	__ATTR(pixel_clock, S_IRUGO, show_pix_clk, NULL),
#ifdef CS75XX_LCD_INFO
	__ATTR(lcd_info, S_IRUGO, lcd_info, NULL),
#endif
#ifdef CONFIG_HDMI_ANX9805
	__ATTR(hdmi_info, S_IRUGO, cs75xx_get_hdmi_info, NULL),
#endif
};

static void cs75xx_fbmem_free(struct cs75xx_lcd_fb *sfb)
{	
	if(sfb->addr_assign) 
		iounmap(sfb->info->screen_base);
	else
		dma_free_writecombine(&sfb->pdev->dev, sfb->info->fix.smem_len, sfb->info->screen_base, sfb->info->fix.smem_start);
}

static int cs75xx_lcdfb_probe(struct platform_device *pdev)
{
	struct cs75xx_lcd_fb *sfb;
	struct fb_info	 *info;
	struct fb_videomode fbmode;	
	struct device *dev = &pdev->dev;
	int ret,i;
	info = framebuffer_alloc(sizeof(struct cs75xx_lcd_fb), dev);

	if (!info) {
	  dev_err(dev, "cannot allocate memory\n");
	  return -ENOMEM;
	}

	sfb = info->par;
	sfb->info = info;
	sfb->pdev = pdev;
	sfb->fb_plat_data=(struct cs75xx_fb_plat_data *)dev->platform_data;
	strcpy(info->fix.id, sfb->pdev->name);

	sfb->reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!sfb->reg_res) {
	  dev_err(dev, "resources unusable\n");
	  ret = -ENXIO;
	  goto free_fb;
	}

	if(!sfb->fb_plat_data) {
	  dev_err(dev, "unable to get socle fb platform data\n");
	  ret = -ENXIO;
	  goto free_fb;
	}
	info->fix.mmio_start	  = sfb->reg_res->start;
	info->fix.mmio_len  = sfb->reg_res->end - sfb->reg_res->start + 1;

	if (!request_mem_region(info->fix.mmio_start, info->fix.mmio_len, pdev->name)) {
	  dev_err(dev, "cannot request LCDC registers\n");
	  ret = -EBUSY;
	  goto free_fb;
	}

	sfb->base = ioremap(info->fix.mmio_start, info->fix.mmio_len);
	if (!sfb->base) {
	  dev_err(dev, "cannot map LCDC registers\n");
	  ret = -EBUSY;
	  goto free_res;
	}

#ifdef CS75XX_LCD_INFO
	sfb->irq = platform_get_irq(pdev, 0);
	 if (sfb->irq == 0) {
			 dev_err(&pdev->dev, "failed to get interrupt resouce.\n");
			 ret = -EINVAL;
			 goto free_res;
	 }
	
	ret = request_irq(sfb->irq, cs75xx_lcd_irq, IRQF_DISABLED, "cs75xx_lcdfb", sfb);
	if (ret) {
	  goto free_res;
	}
#endif
	cs75xx_lcd_setup(sfb);
	
	if(sfb->addr_assign) {
		
		info->fix.smem_len = 6*1024*1024; //assign 6M for 1024*768*32it double-buffering
		if (!request_mem_region(info->fix.smem_start, info->fix.smem_len, pdev->name)) {
			dev_err(dev, "cannot request LCDC mem\n");
			ret = -EBUSY;
			goto free_io;
		}

		info->screen_base = ioremap(info->fix.smem_start, info->fix.smem_len);
		if (!info->screen_base) {
			dev_err(dev, "cannot map LCDC mem\n");
			ret = -EBUSY;
			goto free_addr;
		}
	}
	else {
		
		info->fix.smem_len = sfb->panel->mode.xres * sfb->panel->mode.yres * sfb->bpp / 8 * NUMBER_OF_BUFFERS;
		info->screen_base = dma_alloc_writecombine(dev, info->fix.smem_len,(dma_addr_t *)&info->fix.smem_start, GFP_KERNEL);
		if (!info->screen_base) {
			dev_err(dev, "cannot alloc LCDC mem\n");
			ret = -ENOMEM;
			goto free_io;
		}
	}		
	
	info->fbops		= &cs75xx_lcdfb_ops;
	info->flags		= FBINFO_FLAG_DEFAULT;
	info->pseudo_palette	= sfb->cmap;

	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux	= 0;
	info->fix.xpanstep	= 0;
#ifndef CONFIG_DOUBLE_BUFFER
	info->fix.ypanstep	= 0;
#else
	info->fix.ypanstep	= 1;
#endif
	info->fix.ywrapstep	= 0;
	info->fix.accel	= FB_ACCEL_NONE;

	info->var.xres		= sfb->panel->mode.xres;
	info->var.yres		= sfb->panel->mode.yres;
	info->var.xres_virtual	= sfb->panel->mode.xres;
#ifndef CONFIG_DOUBLE_BUFFER
	info->var.yres_virtual	= sfb->panel->mode.yres;
#else
	info->var.yres_virtual	= sfb->panel->mode.yres * NUMBER_OF_BUFFERS;
#endif
	info->var.bits_per_pixel = sfb->bpp;
	info->var.grayscale	= 0;
	info->var.pixclock	= sfb->panel->mode.pixclock;
	info->var.left_margin	= sfb->panel->mode.left_margin;
	info->var.right_margin	= sfb->panel->mode.right_margin;
	info->var.upper_margin	= sfb->panel->mode.upper_margin;
	info->var.lower_margin	= sfb->panel->mode.lower_margin;
	info->var.hsync_len	= sfb->panel->mode.hsync_len;
	info->var.vsync_len	= sfb->panel->mode.vsync_len;
	info->var.sync		= sfb->panel->mode.sync;
	info->var.vmode	= sfb->panel->mode.vmode;
	info->var.activate	= FB_ACTIVATE_NOW;
	info->var.nonstd	= 0;
	info->var.height	= sfb->panel->height;
	info->var.width	= sfb->panel->width;
	info->var.accel_flags	= 0;

	cs75xx_lcdfb_set_bitfields(sfb, &info->var);

	ret=fb_alloc_cmap(&(info->cmap), 256, 0);
	if(ret) {
		dev_err(dev, "Alloc color map failed\n");
		goto free_mem;
	}

 	cs75xx_lcdfb_set_par(info);
 	
	platform_set_drvdata(pdev, sfb);
	ret = register_framebuffer(info);
	if (!ret) {
		for(i=0;i<sizeof(device_attrs)/sizeof(struct device_attribute);i++) 
			device_create_file(info->dev, &device_attrs[i]);
		goto out;
	}
	/* add selected videomode to modelist */
	fb_var_to_videomode(&fbmode, &info->var);
	fb_add_videomode(&fbmode, &info->modelist);

	dev_err(dev, "CLCD: cannot register framebuffer (%d)\n", ret);
	
	cs75xx_lcdfb_disable(sfb);	
	platform_set_drvdata(pdev, NULL);
	

	fb_dealloc_cmap(&info->cmap);
free_mem:	
	cs75xx_fbmem_free(sfb);
free_addr:
	if(sfb->addr_assign)
		release_mem_region(info->fix.smem_start, info->fix.smem_len);
free_io:
	iounmap(sfb->base);
free_res:
 	release_mem_region(info->fix.mmio_start, info->fix.mmio_len);
free_fb:
	framebuffer_release(info);
out:
	return ret;		

}

static int cs75xx_lcdfb_remove(struct platform_device *pdev)
{
	struct cs75xx_lcd_fb *sfb = platform_get_drvdata(pdev);
	cs75xx_lcdfb_disable(sfb);
	fb_dealloc_cmap(&sfb->info->cmap);
	cs75xx_fbmem_free(sfb);
	if(sfb->addr_assign)
		release_mem_region(sfb->info->fix.smem_start, sfb->info->fix.smem_len);
	iounmap(sfb->base);
	release_mem_region(sfb->info->fix.mmio_start, sfb->info->fix.mmio_len);
	framebuffer_release(sfb->info);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
#define LCD_SUSPEND_REG_NUM	13
u32 cs75xx_lcd_save_addr[LCD_SUSPEND_REG_NUM];

static int
cs75xx_lcdfb_suspend(struct platform_device *pdev, pm_message_t msg)
{
 	int tmp;	
	u32 *base;
	struct cs75xx_lcd_fb *sfb;
	u32 val;
	
	pr_debug("socle_clcdfb_suspend\n");
	
	sfb = platform_get_drvdata(pdev);

	base = sfb->base;

	val = readl(base + CLCD_CNTL);

	if (val & CNTL_LCDPWR) {
		val &= ~CNTL_LCDPWR;
		writel(val, base + CLCD_CNTL);
	}
			
	for(tmp=0;tmp<LCD_SUSPEND_REG_NUM;tmp++){
		iowrite32(ioread32(base+tmp), (cs75xx_lcd_save_addr+tmp));
	}
	
	return 0;
}

static int
cs75xx_lcdfb_resume(struct platform_device *pdev)
{
        int tmp;
        u32 *base;
        struct cs75xx_lcd_fb *sfb;
        u32 val;
        pr_debug("cs75xx_clcdfb_resume\n");

        sfb = platform_get_drvdata(pdev);

        base = sfb->base;


        for(tmp=0;tmp<LCD_SUSPEND_REG_NUM;tmp++){
                iowrite32(ioread32(cs75xx_lcd_save_addr+tmp), (base+tmp));
        }

        val = readl(base + CLCD_CNTL);
        val |= CNTL_LCDPWR;
        writel(val, base + CLCD_CNTL);

  return 0;
}

#else
#define cs75xx_lcdfb_suspend NULL
#define cs75xx_lcdfb_resume NULL
#endif


static struct platform_driver cs75xx_lcdfb_driver = {
        .probe          = cs75xx_lcdfb_probe,
        .remove         = cs75xx_lcdfb_remove,
        .suspend        = cs75xx_lcdfb_suspend,
        .resume         = cs75xx_lcdfb_resume,
        .driver         = {
                .name   = "cs75xx_lcdfb",
                .owner  = THIS_MODULE,
        },
};

static int __init cs75xx_lcdfb_init(void)
{
	return platform_driver_register(&cs75xx_lcdfb_driver);
}
module_init(cs75xx_lcdfb_init);

static void __exit cs75xx_lcdfb_exit(void)
{
	platform_driver_unregister(&cs75xx_lcdfb_driver);
}
module_exit(cs75xx_lcdfb_exit);

MODULE_DESCRIPTION("Cortina CS75XX LCD Core driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ryan Chen");
