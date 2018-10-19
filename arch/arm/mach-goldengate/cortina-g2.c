/*
 *  linux/arch/arm/mach-goldengate/cortina-g2.c
 *
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Jason Li <jason.li@cortina-systems.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
//#include <linux/dma-mapping.h> /* for dma_alloc_writecombine */
#include <linux/amba/bus.h>
#include <linux/amba/pl061.h>
#include <linux/amba/mmci.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>

#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/gpio.h>
#include <mach/gpio_alloc.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/leds_pwm.h>

#if 0 /* Disable OpenWRT gpio_button patch */
#include <linux/gpio_buttons.h>
#endif  /* Disable OpenWRT gpio_button patch */

#include <mach/cs_cpu.h>
#include <mach/cs75xx_i2c.h>
#include <linux/spi/spi.h>
#include <mach/cs75xx_spi.h>
#include <mach/cs75xx_ir.h>
#include "../../../sound/soc/cs75xx/cs75xx_snd_sport.h"
#include <mach/global_timer.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/hardware/icst.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/exception.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/hw_irq.h>

#include <mach/irqs.h>
#include <mach/vmalloc.h>
#include <mach/debug_print.h>
#include <mach/cs75xx_adma.h>

#include <mach/cs752x_clcdfb.h>
#include <mach/g2-acp.h>
#include <asm/smp_twd.h>
#include <asm/system_info.h>

#include "core.h"
/* #include "clock.h" */

static u64 cs75xx_dmamask = 0xffffffffUL;
static DEFINE_SPINLOCK(regbus_irq_controller_lock);
static DEFINE_SPINLOCK(dmaeng_irq_controller_lock);
static DEFINE_SPINLOCK(dmassp_irq_controller_lock);

#ifdef CONFIG_ACP
unsigned int acp_enabled = 0;
#endif
extern u32 cs_acp_enable;
#ifndef CONFIG_CS752X_PROC
u32 cs_acp_enable;
EXPORT_SYMBOL(cs_acp_enable);
#endif

static struct map_desc goldengate_io_desc[] __initdata = {

	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_GLOBAL_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_GLOBAL_BASE),
	 .length = SZ_8M,
	 .type = MT_DEVICE,
	 },
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_SCU_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_SCU_BASE),
	 .length = SZ_8K,
	 .type = MT_DEVICE,
	 },
#if 0 /* Subregion of GOLDENGATE_SCU_BASE */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_TWD_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_TWD_BASE),
	 .length = SZ_4K,
	 .type = MT_DEVICE,
	 },
#endif
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RTC_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RTC_BASE),
	 .length = SZ_4K,
	 .type = MT_DEVICE,
	 },
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_L220_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_L220_BASE),
	 .length = SZ_8K,
	 .type = MT_DEVICE,
	 },
#if 0 /* Subregion of GOLDENGATE_GLOBAL_BASE */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_XRAM_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_XRAM_BASE),
	 .length = SZ_1M,
	 .type = MT_DEVICE,
	 },
#endif
	{
         .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_AHCI_BASE),
         .pfn = __phys_to_pfn(GOLDENGATE_AHCI_BASE),
         .length = SZ_4K,
         .type = MT_DEVICE,
         },
	/* RCPU I/DRAM  */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_DRAM0_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_DRAM0_BASE),
	 .length = SZ_256K,
	 .type = MT_DEVICE,
	 },
	/* RRAM0 */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_RRAM0_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_RRAM0_BASE),
	 .length = SZ_32K,
	 .type = MT_DEVICE,
	 },
	/* RRAM1 */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_RRAM1_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_RRAM1_BASE),
	 .length = SZ_32K,
	 .type = MT_DEVICE,
	 },
	/* Crypto Core0 */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_CRYPT0_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_CRYPT0_BASE),
	 .length = SZ_64K,
	 .type = MT_DEVICE,
	 },
	/* Crypto Core0 */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_CRYPT1_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_CRYPT1_BASE),
	 .length = SZ_64K,
	 .type = MT_DEVICE,
	 },
	/* RCPU_REG  */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_REG_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_REG_BASE),
	 .length = SZ_512K,
	 .type = MT_DEVICE,
	 },
	/* RCPU SADB */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_SADB_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_SADB_BASE),
	 .length = SZ_64K,
	 .type = MT_DEVICE,
	 },
	/* RCPU PKT Buffer */
	{
	 .virtual = (unsigned long)IO_ADDRESS(GOLDENGATE_RCPU_PKBF_BASE),
	 .pfn = __phys_to_pfn(GOLDENGATE_RCPU_PKBF_BASE),
	 .length = SZ_256K,
	 .type = MT_DEVICE,
	 },

#ifdef CONFIG_CS75xx_IPC2RCPU
	/* Share memory for Re-circulation CPU */
	{
	 .virtual = (unsigned long)GOLDENGATE_IPC_BASE_VADDR,
	 .pfn = __phys_to_pfn(GOLDENGATE_IPC_BASE),
	 .length = GOLDENGATE_IPC_MEM_SIZE,
	 .type = MT_DEVICE,
	 },
#endif
	{
	 .virtual = (unsigned long)IO_ADDRESS( GOLDENGATE_OTP_BASE ),
	 .pfn = __phys_to_pfn( GOLDENGATE_OTP_BASE ),
	 .length = SZ_1K,
	 .type = MT_DEVICE,
	},
};

static void __init goldengate_map_io(void)
{
	iotable_init(goldengate_io_desc, ARRAY_SIZE(goldengate_io_desc));
}

/* FPGA Primecells */
#if (defined(CONFIG_FB_CS752X_CLCD) | defined(CONFIG_FB_CS752X_CLCD_MODULE))
//    Added by SJ, 20120328
//	GLOBAL_SCRATCH: 0xf00000c0
//    bit 13: 0 - the LCD base clock is 250MHz
//               1 - the LCD base clock is 150MHz
#define CLCDCLK_150M       (1 << 13)

static int cs75xx_fb_get_clk_div(unsigned int target_clk)
{
//	printk("cs75xx_fb_get_clk_div :  target_clk %d------\n",target_clk);
	if ((target_clk >= 148500) && (target_clk <= 150000)) { /* pixel clock = 150 = 150MHz  */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) | CLCDCLK_150M,
			IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH));
//		bypass
		return -1;
	} else if ((target_clk >= 74000) && (target_clk <= 75000)) { /* pixel clock = 150/(0+2) = 75MHz  */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) | CLCDCLK_150M,
			IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH));
		return 0;
	} else if ((target_clk >= 60000) && (target_clk <= 65100)) {	/* pixel clock = 250/(2+2) = 62.5MHz  */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) & ~CLCDCLK_150M,
			IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH));
		return 2;
	} else if ((target_clk >= 50000) && (target_clk < 51000)) {	/* pixel clock = 250/(3+2) = 50MHz  */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) & ~CLCDCLK_150M,
			IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH));
		return 2;
	} else if ((target_clk >= 40000) && (target_clk < 41000) ) {	/* pixel clock = 250/(4+2) = 41.67 MHz */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) & ~CLCDCLK_150M,
			IO_ADDRESS(GLOBAL_SCRATCH));
		return 4;
	} else if ((target_clk >= 27000) && (target_clk < 30000)) {	/* pixel clock = 250/(7+2) = 27.7 MHz */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) & ~CLCDCLK_150M,
			IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH));
		return 7;
	} else if ((target_clk >= 25000) && (target_clk < 27000)) { /* pixel clock = 250/(8+2) = 25MHz */
		writel(readl(IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH)) & ~CLCDCLK_150M,
			IO_ADDRESS((volatile void __iomem *)GLOBAL_SCRATCH));
		return 8;
	} else {
		printk("target_clk :%d, Error not in table list",target_clk);
		return 0;
	}

}

static struct cs75xx_fb_plat_data fb_plat_data = {
        .get_clk_div=cs75xx_fb_get_clk_div,
};


static struct resource cs75xx_lcdc_resources[] = {
        [0] = {
                .start = GOLDENGATE_LCDC_BASE,
                .end = GOLDENGATE_LCDC_BASE + SZ_4K - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_LCD,
                .end = IRQ_LCD,
                .flags = IORESOURCE_IRQ,
        },
};

static u64 cs75xx_device_lcd_dmamask = 0xffffffffUL;
struct platform_device cs75xx_lcd_device = {
        .name             = "cs75xx_lcdfb",
        .id               = -1,
        .dev            = {
                .dma_mask               = &cs75xx_device_lcd_dmamask,
                .coherent_dma_mask      = 0xffffffffUL,
                .platform_data= &fb_plat_data,
        },
        .resource       = cs75xx_lcdc_resources,
        .num_resources  = ARRAY_SIZE(cs75xx_lcdc_resources),
};

void __init cs75xx_add_device_lcd(void)
{
	/* GPIO 4 Bit[28:0] */
	int status;
	status = readl(IO_ADDRESS(GLOBAL_GPIO_MUX_4));
	status &= ~0x1FFFFFFF;  /* GPIO 4 Bit[28:0] */
	printk("write gpio %x \n",status);
	writel(status, IO_ADDRESS(GLOBAL_GPIO_MUX_4));
	//MEMORY Priority
	writel(0xffff00, IO_ADDRESS(0xf050012c));
	platform_device_register(&cs75xx_lcd_device);
}
#else
void __init cs75xx_add_device_lcd(void) {}
#endif


/*
 * GoldenGate EB platform devices
 */
/*
static struct resource goldengate_flash_resource = {
	.start			= GOLDENGATE_FLASH_BASE,
	.end			= GOLDENGATE_FLASH_BASE + GOLDENGATE_FLASH_SIZE - 1,
	.flags			= IORESOURCE_MEM,
};

static struct resource goldengate_eth_resources[] = {
	[0] = {
		.start		= GOLDENGATE_NEMAC_BASE,
		.end		= GOLDENGATE_NEMAC_BASE + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_EB_ETH,
		.end		= IRQ_EB_ETH,
		.flags		= IORESOURCE_IRQ,
	},
};
*/
static struct resource goldengate_ahci_resources[] = {
	[0] = {
	       .start = GOLDENGATE_AHCI_BASE,
	       .end = GOLDENGATE_AHCI_BASE + SZ_64K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = GOLDENGATE_IRQ_AHCI,
	       .end = GOLDENGATE_IRQ_AHCI,
	       .flags = IORESOURCE_IRQ,
	       },
};

/* Platform device */
struct platform_device goldengate_ahci_device = {
	.name = "goldengate-ahci",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_ahci_resources),
	.resource = goldengate_ahci_resources,
};

#ifdef CONFIG_CORTINA_G2_ADMA
/* Async. DMA */
static struct resource goldengate_adma_resources[] = {
	[0] = {
	       .start = GOLDENGATE_DMA_BASE,
	       .end = GOLDENGATE_DMA_BASE + SZ_1K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = GOLDENGATE_IRQ_DMA_RX6,
	       .end = GOLDENGATE_IRQ_DMA_RX6,
	       .flags = IORESOURCE_IRQ,
	       },
	[2] = {
	       .start = GOLDENGATE_IRQ_DMA_TX6,
	       .end = GOLDENGATE_IRQ_DMA_TX6,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device goldengate_dma_device = {
	.name = "cs75xx_adma",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_adma_resources),
	.resource = goldengate_adma_resources,
};
#endif

static struct resource goldengate_ts_resources[] = {
	[0] = {
	       .start = GOLDENGATE_TS_BASE,
	       .end = GOLDENGATE_TS_BASE + SZ_64K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = GOLDENGATE_IRQ_TS,
	       .end = GOLDENGATE_IRQ_TS,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device goldengate_ts_device = {
	.name = "g2-ts",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_ts_resources),
	.resource = goldengate_ts_resources,
};

static struct resource goldengate_wdt_resources[] = {
	[0] = {
	       .start = GOLDENGATE_TWD_BASE,
	       .end = GOLDENGATE_TWD_BASE + SZ_1K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = GOLDENGATE_IRQ_WDT,
	       .end = GOLDENGATE_IRQ_WDT,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device goldengate_wdt_device = {
	.name = "g2-wdt",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_wdt_resources),
	.resource = goldengate_wdt_resources,
};

static struct resource goldengate_flash_resources[] = {
	[0] = {
	       .start = GOLDENGATE_FLASH_BASE,
	       .end = GOLDENGATE_FLASH_BASE + SZ_128M - 1,
	       .flags = IORESOURCE_IO,
	       },
	[1] = {
	       .start = GOLDENGATE_IRQ_FLSH,
	       .end = GOLDENGATE_IRQ_FLSH,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device goldengate_flash_device = {
	.name = "cs752x_nand",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_flash_resources),
	.resource = goldengate_flash_resources,
};

static struct resource goldengate_rtc_resources[] = {
	[0] = {
	       .start = GOLDENGATE_RTC_BASE,
	       .end = GOLDENGATE_RTC_BASE + SZ_1K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_RTC_ALM,
	       .end = IRQ_RTC_ALM,
	       .flags = IORESOURCE_IRQ,
	       },
	[2] = {
	       .start = IRQ_RTC_PRI,
	       .end = IRQ_RTC_PRI,
	       .flags = IORESOURCE_IRQ,
	       },
};

struct platform_device goldengate_rtc_device = {
	.name = "g2-rtc",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_rtc_resources),
	.resource = goldengate_rtc_resources,
};

#if defined(CONFIG_CS752X_SD) || defined(CONFIG_CS752X_SDIO) || defined(CONFIG_CS752X_SDIO_MODULE)
#define SD_DEVICE_NAME "cs752x_sd"
static struct resource goldengate_sd_resources[] = {
	[0] = {
	       .start = GOLDENGATE_SDC_BASE,
	       .end = GOLDENGATE_SDC_BASE + SZ_64K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = GOLDENGATE_IRQ_MMC,
	       .end = GOLDENGATE_IRQ_MMC,
	       .flags = IORESOURCE_IRQ,
	       },
};
struct platform_device goldengate_sd_device = {
	.name = SD_DEVICE_NAME,
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_sd_resources),
	.resource = goldengate_sd_resources,
};
#endif				/* CONFIG_CS752X_SD */

static struct resource cs75xx_gpio_resources[] = {
	[0] = {
	       .name = "global",
	       .start = GOLDENGATE_GLOBAL_BASE,
	       .end = GOLDENGATE_GLOBAL_BASE + 0xBB,
	       .flags = IORESOURCE_IO,
	       },
	[1] = {
	       .name = "gpio0",
	       .start = GOLDENGATE_GPIO0_BASE,
	       .end = GOLDENGATE_GPIO1_BASE - 1,
	       .flags = IORESOURCE_IO,
	       },
	[2] = {
	       .name = "gpio1",
	       .start = GOLDENGATE_GPIO1_BASE,
	       .end = GOLDENGATE_GPIO2_BASE - 1,
	       .flags = IORESOURCE_IO,
	       },
	[3] = {
	       .name = "gpio2",
	       .start = GOLDENGATE_GPIO2_BASE,
	       .end = GOLDENGATE_GPIO3_BASE - 1,
	       .flags = IORESOURCE_IO,
	       },
	[4] = {
	       .name = "gpio3",
	       .start = GOLDENGATE_GPIO3_BASE,
	       .end = GOLDENGATE_GPIO4_BASE - 1,
	       .flags = IORESOURCE_IO,
	       },
	[5] = {
	       .name = "gpio4",
	       .start = GOLDENGATE_GPIO4_BASE,
	       .end = GOLDENGATE_GPIO4_BASE + 0x1B,
	       .flags = IORESOURCE_IO,
	       },
	[6] = {
	       .name = "irq_gpio0",
	       .start = GOLDENGATE_IRQ_GPIO0,
	       .end = GOLDENGATE_IRQ_GPIO0,
	       .flags = IORESOURCE_IRQ,
	       },
	[7] = {
	       .name = "irq_gpio1",
	       .start = GOLDENGATE_IRQ_GPIO1,
	       .end = GOLDENGATE_IRQ_GPIO1,
	       .flags = IORESOURCE_IRQ,
	       },
	[8] = {
	       .name = "irq_gpio2",
	       .start = GOLDENGATE_IRQ_GPIO2,
	       .end = GOLDENGATE_IRQ_GPIO2,
	       .flags = IORESOURCE_IRQ,
	       },
	[9] = {
	       .name = "irq_gpio3",
	       .start = GOLDENGATE_IRQ_GPIO3,
	       .end = GOLDENGATE_IRQ_GPIO3,
	       .flags = IORESOURCE_IRQ,
	       },
	[10] = {
		.name = "irq_gpio4",
		.start = GOLDENGATE_IRQ_GPIO4,
		.end = GOLDENGATE_IRQ_GPIO4,
		.flags = IORESOURCE_IRQ,
		},
};

static struct platform_device cs75xx_gpio_device = {
	.name = CS75XX_GPIO_CTLR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_gpio_resources),
	.resource = cs75xx_gpio_resources,
};

static struct resource cs75xx_pwm_resources[] = {
	{
		.name = "pwm_base",
		.start = GOLDENGATE_PWM_BASE,
		.end = GOLDENGATE_PWM_BASE + 7,
		.flags = IORESOURCE_IO,
	},
};

static struct platform_device cs75xx_pwm_device = {
	.name = "cs75xx-pwm",
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(cs75xx_pwm_resources),
	.resource = cs75xx_pwm_resources,
};

static struct gpio_keys_button cs75xx_gpio_keys[] = {
#ifdef GPIO_SW_RST_N
	{
		/* Configuration parameters */
		.code = KEY_RESTART,
		.gpio = GPIO_SW_RST_N,
		.active_low = 1,
		.desc = "GPIO_SW_RST_N",
		.type = EV_KEY,		/* input event type (EV_KEY, EV_SW) */
		.wakeup = 1,		/* configure the button as a wake-up source */
		.debounce_interval = 0,	/* debounce ticks interval in msecs */
		.can_disable = 1	/* dedicated pin */
	},
#endif
#ifdef GPIO_WPS_SWITCH
	{
		/* Configuration parameters */
		.code = KEY_WPS_BUTTON,
		.gpio = GPIO_WPS_SWITCH,
		.active_low = 1,
		.desc = "GPIO_WPS_SWITCH",
		.type = EV_KEY,         /* input event type (EV_KEY, EV_SW) */
		.wakeup = 1,            /* configure the button as a wake-up source */
		.debounce_interval = 0, /* debounce ticks interval in msecs */
		.can_disable = 1        /* dedicated pin */
	},
#endif
// Adds support for the temperature sensor
// The kernel will emit a KEY_POWER input event if the temperature is too high
#ifdef GPIO_TEMP_SENSOR
	{
		/* Configuration parameters */
		.code = KEY_POWER,
		.gpio = GPIO_TEMP_SENSOR,
		.active_low = 0,
		.desc = "GPIO_TEMP_SENSOR",
		.type = EV_KEY,           /* input event type (EV_KEY, EV_SW) */
		.wakeup = 1,              /* configure the button as a wake-up source */
		.debounce_interval = 100, /* debounce ticks interval in msecs */
		.can_disable = 1          /* dedicated pin */
	},
#endif
};

static struct gpio_keys_platform_data cs75xx_gpio_keys_platform = {
	.buttons = cs75xx_gpio_keys,
	.nbuttons = ARRAY_SIZE(cs75xx_gpio_keys),
	.rep = 0,		/* enable input subsystem auto repeat */
	.enable = NULL,
	.disable = NULL
};

static struct platform_device cs75xx_gpio_keys_device = {
	.name = "gpio-keys",
	.id = -1,
	.dev = {
		.power.can_wakeup = 1,
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &cs75xx_gpio_keys_platform
	},
};

#if 0 /* Disable OpenWRT gpio_button patch */
static struct gpio_button cs75xx_gpio_btns[] = {
#ifdef GPIO_FACTORY_DEFAULT
	{
		.gpio = GPIO_FACTORY_DEFAULT,
		.active_low = 1,
		.desc = "GPIO_FACTORY_DEFAULT",
		.type = EV_KEY,
		.code = KEY_VENDOR,
		.threshold = FACTORY_DEFAULT_TIME,
	},
#endif
};

static struct gpio_buttons_platform_data cs75xx_gpio_btns_platform = {
	.buttons = cs75xx_gpio_btns,
	.nbuttons = ARRAY_SIZE(cs75xx_gpio_btns),
#ifdef GPIO_BTNS_POLLING_INTERVAL
	.poll_interval = GPIO_BTNS_POLLING_INTERVAL,
#endif
};

static struct platform_device cs75xx_gpio_btns_device = {
	.name = "gpio-buttons",
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &cs75xx_gpio_btns_platform
		},
};
#endif /* Disable OpenWRT gpio_button patch */

static struct gpio_led cs75xx_leds[] = {
#ifdef GPIO_WIFI_LED_R
	{
		.name = "wifi:red",
		.gpio = GPIO_WIFI_LED_R,
		.active_low = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
		.default_trigger = "none",
	},
#endif
#ifdef GPIO_WIFI_LED_W
	{
		.name = "wifi:white",
		.gpio = GPIO_WIFI_LED_W,
		.active_low = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
		.default_trigger = "none",
	},
#endif
#ifdef GPIO_FAULT_LED
	{
		.name = "fault_led",
		.gpio = GPIO_FAULT_LED,
		.active_low = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
		.default_trigger = "none",
	},
#endif
#ifdef GPIO_POWER_LED
	{
		.name = "power_led",
		.gpio = GPIO_POWER_LED,
		.active_low = 0,
		.default_state = LEDS_GPIO_DEFSTATE_ON,
		.default_trigger = "none",
	},
#endif
};

static struct led_pwm cs75xx_pwm_leds[] = {
	{
		.name		= "pwm_led",
		.pwm_id		= 0,
		.max_brightness	= 4,
		.pwm_period_ns	= 500,
	},
};

static struct led_pwm_platform_data cs75xx_pwm_data = {
	.num_leds	= ARRAY_SIZE(cs75xx_pwm_leds),
	.leds		= cs75xx_pwm_leds,
};

static struct platform_device cs75xx_leds_pwm = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data = &cs75xx_pwm_data,
	},
};

static struct gpio_led_platform_data cs75xx_led_platform_data = {
	.leds		= cs75xx_leds,
	.num_leds	= ARRAY_SIZE(cs75xx_leds),
};

static struct platform_device cs75xx_led_device = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &cs75xx_led_platform_data,
	},
};

static struct cs75xx_i2c_pdata cs75xx_i2c_cfg = {
	APB_CLOCK,
	100000,
	100,
	3
};

static struct resource cs75xx_i2c_resources[] = {
	{
	 .name = "i2c",
	 .start = GOLDENGATE_BIW_BASE,
	 .end = GOLDENGATE_BIW_BASE + 0x27,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_i2c",
	 .start = GOLDENGATE_IRQ_BIWI,
	 .end = GOLDENGATE_IRQ_BIWI,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device cs75xx_i2c_device = {
	.name = CS75XX_I2C_CTLR_NAME,
	.id = -1,
	.dev = {
		.platform_data = &cs75xx_i2c_cfg,
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_i2c_resources),
	.resource = cs75xx_i2c_resources,
};

#if defined(CONFIG_I2C_CS75XX) && defined(CONFIG_I2C_BOARDINFO)
static struct i2c_board_info i2c_board_infos[] __initdata = {
#if defined(CONFIG_CORTINA_FPGA)
	{
	 I2C_BOARD_INFO("cs4341", 0x10),
	 .flags = 0,
	 .irq = 0,
	 .platform_data = NULL,
	},
#endif
#if defined(CONFIG_CORTINA_ENGINEERING) || defined(CONFIG_CORTINA_ENGINEERING_S)
#if defined(CONFIG_SND_SOC_DAE4P)
	{
	 I2C_BOARD_INFO("dae4p", 0x59),
	 .flags = 0,
	 .irq = 0,
#if defined(CONFIG_CORTINA_ENGINEERING)
	 .platform_data = GPIO_SLIC1_RESET,
#elif defined(CONFIG_CORTINA_ENGINEERING_S)
	 .platform_data = GPIO_SLIC0_RESET,
#endif
	},
#endif
#endif
#if defined(CONFIG_CORTINA_ENGINEERING) || defined(CONFIG_CORTINA_ENGINEERING_S)\
	|| defined(CONFIG_CORTINA_REFERENCE_Q) || defined(CONFIG_CORTINA_REFERENCE_QD)
	{
	 I2C_BOARD_INFO("si5338_70", 0x70),
	 .flags = 0,
	 .irq = 0,
	 .platform_data = NULL,
	},
	{
	 I2C_BOARD_INFO("pcap7200", 0x0a),
	 .flags = 0,
	 .irq = GPIO_HDMI_INT,
	 .platform_data = NULL,
	},
#if !defined(CONFIG_CORTINA_ENGINEERING_S)
	{
	 I2C_BOARD_INFO("si5338_71", 0x71),
	 .flags = 0,
	 .irq = 0,
	 .platform_data = NULL,
	},
#endif
#endif
#if defined(CONFIG_CORTINA_ENGINEERING) || defined(CONFIG_CORTINA_PON) \
	|| defined(CONFIG_CORTINA_WAN)
	{
		I2C_BOARD_INFO("anx9805-sys", 0x39),
		.flags = 0,
		.irq = 0,
		.platform_data = NULL,
	},
	{
		I2C_BOARD_INFO("anx9805-dp", 0x38),
		.flags = 0,
		.irq = 0,
		.platform_data = NULL,
	},
	{
		I2C_BOARD_INFO("anx9805-hdmi", 0x3d),
		.flags = 0,
		.irq = 0,
		.platform_data = NULL,
	},
#endif
};
#endif /* defined(CONFIG_I2C_CS75XX) && defined(CONFIG_I2C_BOARDINFO) */

static struct cs75xx_spi_info cs75xx_spi_cfg = {
	.tclk = APB_CLOCK,
	.divider = CS75XX_SPI_CLOCK_DIV,
	.timeout = (2 * HZ)
};

static struct resource cs75xx_spi_resources[] = {
	{
	 .name = "spi",
	 .start = GOLDENGATE_SPI_BASE,
	 .end = GOLDENGATE_SPI_BASE + 0x3B,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_spi",
	 .start = GOLDENGATE_IRQ_SPI,
	 .end = GOLDENGATE_IRQ_SPI,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device cs75xx_spi_device = {
	.name = CS75XX_SPI_CTLR_NAME,
	.id = -1,
	.dev = {
		.platform_data = &cs75xx_spi_cfg,
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_spi_resources),
	.resource = cs75xx_spi_resources,
};

static struct spi_board_info spi_board_infos[] __initdata = {

#if defined(CONFIG_CORTINA_CUSTOM_BOARD)


/* custom board patch start HERE: */

/* Review contents of included template below. After review, apply patch to replace 
 * everything between the start HERE: comment above to the and end HERE: comment below
 * including the start HERE: and end HERE: lines themselves. 
 *
 * This patch should also remove the warning below and also change inclusion path to be a location 
 * within YOUR own custom_board/my_board_name tree which will not be overwritten by
 * future Cortina releases.   
 *
 * WARNING: Do NOT remove or change the CONFIG_CORTINA_CUSTOM_BOARD pre-processor definition name above.
 * Cortina will only support custom board builds which use the CONFIG_CORTINA_CUSTOM_BOARD definition.
 */

#warning CUSTOM_BOARD_REVIEW_ME
#include <mach/custom_board/template/spi/cfg_spi_board_info.h>

/* custom board patch end HERE: */

#else /* defined(CONFIG_CORTINA_CUSTOM_BOARD) else */


#if !defined(CONFIG_CORTINA_BHR) && defined(CONFIG_SLIC_VE880_SLOT0)
	{
	 .modalias = "ve880_slot0",
	 .platform_data = GPIO_SLIC0_RESET,
#if defined(CONFIG_CORTINA_ENGINEERING_S) || defined(CONFIG_CORTINA_REFERENCE_S)
	 .irq = -1,	/* not support SLIC interrupt */
#else
	 .irq = GPIO_SLIC_INT,
#endif /* defined(CONFIG_CORTINA_ENGINEERING_S) || defined(CONFIG_CORTINA_REFERENCE_S) */
	 .max_speed_hz = 8000000,
	 .bus_num = 0,
#if defined(CONFIG_CORTINA_FPGA) || defined(CONFIG_CORTINA_REFERENCE) \
	 || defined(CONFIG_CORTINA_REFERENCE_B) || defined(CONFIG_CORTINA_PON) \
	 || defined(CONFIG_CORTINA_WAN) || defined(CONFIG_CORTINA_REFERENCE_Q) 
	 .chip_select = 0,
#elif defined(CONFIG_CORTINA_ENGINEERING) || defined(CONFIG_CORTINA_ENGINEERING_S) || defined(CONFIG_CORTINA_REFERENCE_S)
	 .chip_select = 1,
#endif /* defined(CONFIG_CORTINA_ENGINEERING_S) || defined(CONFIG_CORTINA_REFERENCE_S) ...defined(CONFIG_CORTINA_REFERENCE_Q) endif */
	 .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
	 },
#endif /* !defined(CONFIG_CORTINA_BHR) && defined(CONFIG_SLIC_VE880_SLOT0) endif */

#if defined(CONFIG_CORTINA_ENGINEERING) && defined(CONFIG_SLIC_VE880_SLOT1)
	{
	 .modalias = "ve880_slot1",
	 .platform_data = GPIO_SLIC1_RESET,
	 .irq = GPIO_SLIC_INT,
	 .max_speed_hz = 8000000,
	 .bus_num = 0,
	 .chip_select = 2,
	 .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
	 },
#endif

#if defined(CONFIG_SLIC_SI3226X_SLOT0)	 
	// For silicon lab si3226x SLIC IC.
	{
	 .modalias = "si3226x_slot0",
	 .platform_data = GPIO_SLIC0_RESET,
	 .irq = GPIO_SLIC_INT,
	 .max_speed_hz = 8000000,
	 .bus_num = 0,
	 .chip_select = 0,
	 .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
	 },
#endif

#if defined(CONFIG_SLIC_SI3226X_SLOT1)	
	{
	 .modalias = "si3226x_slot1",
	 .platform_data = GPIO_SLIC1_RESET,
	 .irq = GPIO_SLIC_INT,
	 .max_speed_hz = 8000000,
	 .bus_num = 0,
	 .chip_select = 2,
	 .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
	 },
#endif
#if defined(CONFIG_PANEL_HX8238A)
        {       /* HX8238A TFT LCD Single Chip */
         .modalias = "hx8238a_panel",
         .max_speed_hz = 200000,
         .bus_num = 0,
         .chip_select = 3,
         .mode = SPI_MODE_3},   /* CPOL=1, CPHA=1 */
#endif  /* CONFIG_PANEL_HX8238A */
#endif /* defined(CONFIG_CORTINA_CUSTOM_BOARD) endif */
};

static struct resource cs75xx_ir_resources[] = {
	{
	 .name = "cir",
	 .start = GOLDENGATE_CIR_BASE,
	 .end = GOLDENGATE_CIR_BASE + 0x33,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_cir",
	 .start = GOLDENGATE_IRQ_CIR,
	 .end = GOLDENGATE_IRQ_CIR,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device cs75xx_ir_device = {
	.name = CS75XX_IR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_ir_resources),
	.resource = cs75xx_ir_resources,
};

static struct resource cs75xx_pwr_resources[] = {
	{
	 .name = "cir",
	 .start = GOLDENGATE_CIR_BASE,
	 .end = GOLDENGATE_CIR_BASE + 0x33,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_pwr",
	 .start = GOLDENGATE_IRQ_PWC,
	 .end = GOLDENGATE_IRQ_PWC,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device cs75xx_pwr_device = {
	.name = "cs75xx-pwr",
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_pwr_resources),
	.resource = cs75xx_pwr_resources,
};

static struct resource cs75xx_sport_resources[] = {
	/* SSP */
	{
	 .name = "ssp0",
	 .start = GOLDENGATE_SSP0_BASE,
	 .end = GOLDENGATE_SSP0_BASE + 0x57,
	 .flags = IORESOURCE_IO,
	 },
#if !defined(CONFIG_CORTINA_ENGINEERING_S)
	{
	 .name = "ssp1",
	 .start = GOLDENGATE_SSP1_BASE,
	 .end = GOLDENGATE_SSP1_BASE + 0x57,
	 .flags = IORESOURCE_IO,
	 },
#endif
	{
	 .name = "irq_ssp0",
	 .start = GOLDENGATE_IRQ_DMASSP_SSP0,
	 .end = GOLDENGATE_IRQ_DMASSP_SSP0,
	 .flags = IORESOURCE_IRQ,
	 },
#if !defined(CONFIG_CORTINA_ENGINEERING_S)
	{
	 .name = "irq_ssp1",
	 .start = GOLDENGATE_IRQ_DMASSP_SSP1,
	 .end = GOLDENGATE_IRQ_DMASSP_SSP1,
	 .flags = IORESOURCE_IRQ,
	 },
#endif
	/* SPDIF */
	{
	 .name = "spdif",
#ifdef CONFIG_CORTINA_FPGA
	 .start = 0xF0700000,
	 .end = 0xF0700000 + 0xf7,
#else
	 .start = GOLDENGATE_SPDIF_BASE,
	 .end = GOLDENGATE_SPDIF_BASE + 0xf7,
#endif
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_spdif",
#ifdef CONFIG_CORTINA_FPGA
	.start = GOLDENGATE_IRQ_SSP0,
	.end = GOLDENGATE_IRQ_SSP0,
#else
	 .start = GOLDENGATE_IRQ_SPDIF,
	 .end = GOLDENGATE_IRQ_SPDIF,
#endif
	 .flags = IORESOURCE_IRQ,
	 },
	/* DMA */
	{
	 .name = "dma_ssp",
	 .start = DMA_DMA_SSP_RXDMA_CONTROL,
	 .end = DMA_DMA_SSP_RXDMA_CONTROL + 0xE7,
	 .flags = IORESOURCE_DMA,
	 },
	{
	 .name = "dma_desc",
	 .start = GOLDENGATE_IRQ_DMASSP_DESC,
	 .end = GOLDENGATE_IRQ_DMASSP_DESC,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .name = "dma_rx_ssp0",
	 .start = GOLDENGATE_IRQ_DMASSP_RX6,
	 .end = GOLDENGATE_IRQ_DMASSP_RX6,
	 .flags = IORESOURCE_IRQ,
	 },
#if !defined(CONFIG_CORTINA_ENGINEERING_S)
	{
	 .name = "dma_rx_ssp1",
	 .start = GOLDENGATE_IRQ_DMASSP_RX7,
	 .end = GOLDENGATE_IRQ_DMASSP_RX7,
	 .flags = IORESOURCE_IRQ,
	 },
#endif
	{
	 .name = "dma_tx_ssp0",
	 .start = GOLDENGATE_IRQ_DMASSP_TX6,
	 .end = GOLDENGATE_IRQ_DMASSP_TX6,
	 .flags = IORESOURCE_IRQ,
	 },
#if !defined(CONFIG_CORTINA_ENGINEERING_S)
	{
	 .name = "dma_tx_ssp1",
	 .start = GOLDENGATE_IRQ_DMASSP_TX7,
	 .end = GOLDENGATE_IRQ_DMASSP_TX7,
	 .flags = IORESOURCE_IRQ,
	 },
#endif
};

static struct platform_device cs75xx_sport_device = {
	.name = "cs75xx-sport",
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_sport_resources),
	.resource = cs75xx_sport_resources,
};

#if defined(CONFIG_SND_SOC_SPDIF)
static struct platform_device spdif_dit_device = {
	.name = "spdif-dit",
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources =0,
	.resource = NULL,
};
#endif

#ifdef CONFIG_HW_RANDOM_CS75XX
static struct resource cs75xx_trng_resources[] = {
	{
	 .name = "trng",
	 .start = GOLDENGATE_TRNG_BASE,
	 .end = GOLDENGATE_TRNG_BASE + 0x1B,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_trng",
	 .start = GOLDENGATE_IRQ_TRNG,
	 .end = GOLDENGATE_IRQ_TRNG,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device cs75xx_trng_device = {
	.name = "cs75xx_trng",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_trng_resources),
	.resource = cs75xx_trng_resources,
};
#endif				/* CONFIG_HW_RANDOM_CS75XX */

static struct resource cs75xx_spacc_resources[] = {
	{
	 .name = "spacc0",
	 .start = GOLDENGATE_RCPU_CRYPT0_BASE,
	 .end = GOLDENGATE_RCPU_CRYPT0_BASE + 0x000C0000,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "spacc1",
	 .start = GOLDENGATE_RCPU_CRYPT1_BASE,
	 .end = GOLDENGATE_RCPU_CRYPT1_BASE + 0x000C0000,
	 .flags = IORESOURCE_IO,
	 },
	{
	 .name = "irq_spacc0",
	 .start = GOLDENGATE_IRQ_CRYPT0,
	 .end = GOLDENGATE_IRQ_CRYPT0,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .name = "irq_spacc1",
	 .start = GOLDENGATE_IRQ_CRYPT1,
	 .end = GOLDENGATE_IRQ_CRYPT1,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device cs75xx_spacc_device = {
	.name = "cs75xx_spacc",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_spacc_resources),
	.resource = cs75xx_spacc_resources,
};

/* G2 NE */
static struct resource goldengate_ne_resources[] = {
	{
	 .name = "g2_ne",
	 .start = GOLDENGATE_NI_TOP_BASE,
	 .end = GOLDENGATE_NI_TOP_BASE + (5 * SZ_64K) - 1,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .name = "irq_ne",
	 .start = IRQ_NET_ENG,
	 .end = IRQ_NET_ENG,
	 .flags = IORESOURCE_IRQ,
	 },
};

struct platform_device goldengate_ne_device = {
	.name = "g2-ne",
	.id = 0,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(goldengate_ne_resources),
	.resource = goldengate_ne_resources,
};
/* G2 NE */

/* G2 Read/Write register */
//static u64 g2_rw_dmamask = 0xffffffffUL;

static struct resource g2_rw_resources[] = {
	[0] = {
	       .start = GOLDENGATE_OHCI_BASE,
	       .end = GOLDENGATE_OHCI_BASE + 0x000000ff,
	       .flags = IORESOURCE_IO,
	       }
};

static struct platform_device g2_rw_device = {
	.name = "g2_rw",
	.id = -1,
//      .dev            = {*/
//              .dma_mask = &g2_rw_dmamask,*/
//              .coherent_dma_mask = 0xffffffff,*/
//      },*/
	.num_resources = ARRAY_SIZE(g2_rw_resources),
	.resource = g2_rw_resources,
};

/* #include "pcie.h" */
static u64 g2_pcie_dmamask = 0xffffffffUL;
//static u64 g2_pcie_1_dmamask = 0xffffffffUL;

static struct resource g2_pcie_resources[] = {
	[0] = {
	       .name = "PCIe 0 Base Space",
	       .start = GOLDENGATE_PCIE0_BASE,
	       .end = GOLDENGATE_PCIE0_BASE + 0x1000 - 1,
	       .flags = IORESOURCE_IO,
	       },
	[1] = {
	       .name = "PCIe 0 I/O Space",
	       .start = GOLDENGATE_PCIE0_BASE + 0x1000,
	       .end = GOLDENGATE_PCIE0_BASE + 0x1000 + 0x5000 - 1,
	       .flags = IORESOURCE_IO,
	       },
	[2] = {
	       .name = "PCIe 0 irq",
	       .start = IRQ_PCIE0,
	       .end = IRQ_PCIE0,
	       .flags = IORESOURCE_IRQ,
	       },
	[3] = {
	       .name = "PCIe 1 Base Space",
	       .start = GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE,
	       .end =
	       GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE + 0x1000 - 1,
	       .flags = IORESOURCE_IO,
	       },
	[4] = {
	       .name = "PCIe 1 I/O Space",
	       .start =
	       GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE + 0x1000,
	       .end =
	       GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE + 0x1000 +
	       0x5000 - 1,
	       .flags = IORESOURCE_IO,
	       },
	[5] = {
	       .name = "PCIe 1 irq",
	       .start = IRQ_PCIE1,
	       .end = IRQ_PCIE1,
	       .flags = IORESOURCE_IRQ,
	       },
/* debug_Aaron on 04/15/2011 add for PCIe RC 2 */
#ifndef CONFIG_CORTINA_FPGA
	[6] = {
	       .name = "PCIe 2 Base Space",
	       .start = GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE * 2,
	       .end =
	       GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE * 2 + 0x1000 - 1,
	       .flags = IORESOURCE_IO,
	       },
	[7] = {
	       .name = "PCIe 2 I/O Space",
	       .start =
	       GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE * 2 + 0x1000,
	       .end =
	       GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE * 2 + 0x1000 +
	       0x5000 - 1,
	       .flags = IORESOURCE_IO,
	       },
	[8] = {
	       .name = "PCIe 2 irq",
	       .start = IRQ_PCIE2,
	       .end = IRQ_PCIE2,
	       .flags = IORESOURCE_IRQ,
	       },
#endif
};

static struct platform_device goldengate_pcie_device = {
	.name = "g2_pcie",
	.id = -1,
	.dev = {
		.dma_mask = &g2_pcie_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(g2_pcie_resources),
	.resource = g2_pcie_resources,
};

/* cs752x USB host configuration */

static u64 cs752x_ehci_dmamask = 0xffffffffUL;
static u64 cs752x_ohci_dmamask = 0xffffffffUL;
static u64 cs752x_otg_dmamask = 0xffffffffUL;

static struct resource cs752x_ehci_resources[] = {
	[0] = {
	       .start = GOLDENGATE_EHCI_BASE,
	       .end = GOLDENGATE_EHCI_BASE + 0x0003ffff,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_USB_EHCI,
	       .end = IRQ_USB_EHCI,
	       .flags = IORESOURCE_IRQ,
	       },
};

#ifndef CONFIG_CORTINA_FPGA
static struct resource cs752x_ohci_resources[] = {
	[0] = {
	       .start = GOLDENGATE_OHCI_BASE,	/* 0xf4040000 */
	       .end = GOLDENGATE_OHCI_BASE + 0x00000fff,	/* 0xf407ffff */
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_USB_OHCI,
	       .end = IRQ_USB_OHCI,
	       .flags = IORESOURCE_IRQ,
	       },
};
#endif

static struct resource cs752x_otg_resources[] = {
	[0] = {
	       .start = GOLDENGATE_USB_DEVICE_BASE,
	       .end = GOLDENGATE_USB_DEVICE_BASE + SZ_1M - 1,
	       .flags = IORESOURCE_IO,
	       },
	[1] = {
	       .name = "otg_irq",
	       .start = IRQ_USB_DEV,
	       .end = IRQ_USB_DEV,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct platform_device cs752x_ehci_device = {
	.name = "cs752x_ehci",
	.id = -1,
	.dev = {
		.dma_mask = &cs752x_ehci_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs752x_ehci_resources),
	.resource = cs752x_ehci_resources,
};

#ifndef CONFIG_CORTINA_FPGA
static struct platform_device cs752x_ohci_device = {
	.name = "cs752x_ohci",
	.id = -1,
	.dev = {
		.dma_mask = &cs752x_ohci_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs752x_ohci_resources),
	.resource = cs752x_ohci_resources,
};
#endif

static struct platform_device cs752x_otg_device = {
	.name = "dwc_otg",
	.id = -1,
	.dev = {
		.dma_mask = &cs752x_otg_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs752x_otg_resources),
	.resource = cs752x_otg_resources,
};

#ifdef CONFIG_PHONE_CS75XX_WRAPPER
/*
 * phone wrapper resource for VoIP
 */
static struct resource cs75xx_phone_resources[] = {

#if defined(CONFIG_CORTINA_CUSTOM_BOARD)


/* custom board patch start HERE: */

/* Review contents of included template below. After review, apply patch to replace 
 * everything between the start HERE: comment above to the and end HERE: comment below
 * including the start HERE: and end HERE: lines themselves. 
 *
 * This patch should also remove the warning below and also change inclusion path to be a location 
 * within YOUR own custom_board/my_board_name tree which will not be overwritten by
 * future Cortina releases.   
 *
 * WARNING: Do NOT remove or change the CONFIG_CORTINA_CUSTOM_BOARD pre-processor definition name above.
 * Cortina will only support custom board builds which use the CONFIG_CORTINA_CUSTOM_BOARD definition.
 */

#warning CUSTOM_BOARD_REVIEW_ME
#include <mach/custom_board/template/sound/misc/cs75xx_phone/cfg_cs75xx_phone.h>

/* custom board patch end HERE: */

#else /* CUSTOM_BOARD else */
	{

#ifdef CONFIG_CORTINA_CUSTOM_BOARD

#warning CUSTOM_BOARD_REVIEW_ME

#endif /* CUSTOM_BOARD end */

	 .name = "ssp_index",
#ifdef CONFIG_CORTINA_ENGINEERING
	 .start = 1,
	 .end = 1,
#else
	 .start = 0,
	 .end = 0,
#endif
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .name = "sclk_sel",
#if defined(CONFIG_CORTINA_PON) || defined(CONFIG_CORTINA_WAN) || \
	 defined(CONFIG_CORTINA_REFERENCE_Q)
	 .start = SPORT_INT_SYS_CLK_DIV,
	 .end = SPORT_INT_SYS_CLK_DIV,
#else
	 .start = SPORT_EXT_16384_CLK_DIV,
	 .end = SPORT_EXT_16384_CLK_DIV,
#endif
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .name = "dev_num",
	 .start = 1,
	 .end = 1,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .name = "chan_num",
	 .start = 2,
	 .end = 2,
	 .flags = IORESOURCE_IRQ,
	 },
#if !defined(CONFIG_CORTINA_BHR)
	{
	 .name = "slic_reset",
	 .start = GPIO_SLIC0_RESET,
	 .end = GPIO_SLIC0_RESET,
	 .flags = IORESOURCE_IRQ,
	 },
#endif
#endif /* CUSTOM_BOARD endif */
};

static struct platform_device cs75xx_phone_device = {
	.name = "phone_wrapper",
	.id = -1,
	.dev = {
		.dma_mask = &cs75xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(cs75xx_phone_resources),
	.resource = cs75xx_phone_resources,
};
#endif

static struct resource cs75xx_uart0_resources[] = {
	[0] = {
		.start	= (unsigned long)IO_ADDRESS(UART0_BASE_ADDR),
		.end	= (unsigned long)IO_ADDRESS(UART0_BASE_ADDR) + 0x30 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= GOLDENGATE_IRQ_UART0,
		.end	= GOLDENGATE_IRQ_UART0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource cs75xx_uart1_resources[] = {
	[0] = {
		.start	= (unsigned long)IO_ADDRESS(UART1_BASE_ADDR),
		.end	= (unsigned long)IO_ADDRESS(UART1_BASE_ADDR) + 0x30 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= GOLDENGATE_IRQ_UART1,
		.end	= GOLDENGATE_IRQ_UART1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource cs75xx_uart2_resources[] = {
	[0] = {
		.start	= (unsigned long)IO_ADDRESS(UART2_BASE_ADDR),
		.end	= (unsigned long)IO_ADDRESS(UART2_BASE_ADDR) + 0x30 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= GOLDENGATE_IRQ_UART2,
		.end	= GOLDENGATE_IRQ_UART2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource cs75xx_uart3_resources[] = {
	[0] = {
		.start	= (unsigned long)IO_ADDRESS(UART3_BASE_ADDR),
		.end	= (unsigned long)IO_ADDRESS(UART3_BASE_ADDR) + 0x30 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= GOLDENGATE_IRQ_UART3,
		.end	= GOLDENGATE_IRQ_UART3,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device cs75xx_device_uart0 = {
	.name		= "cortina_serial",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(cs75xx_uart0_resources),
	.resource	= cs75xx_uart0_resources,
};

struct platform_device cs75xx_device_uart1 = {
	.name           = "cortina_serial",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(cs75xx_uart1_resources),
	.resource       = cs75xx_uart1_resources,
};

struct platform_device cs75xx_device_uart2 = {
	.name           = "cortina_serial",
	.id             = 2,
	.num_resources  = ARRAY_SIZE(cs75xx_uart2_resources),
	.resource       = cs75xx_uart2_resources,
};

struct platform_device cs75xx_device_uart3 = {
	.name           = "cortina_serial",
	.id             = 3,
	.num_resources  = ARRAY_SIZE(cs75xx_uart3_resources),
	.resource       = cs75xx_uart3_resources,
};

static struct platform_device *platform_devices[] __initdata = {
	&cs75xx_device_uart0,
	&cs75xx_device_uart1,
	&cs75xx_device_uart2,
	&cs75xx_device_uart3,
	&goldengate_ahci_device,
#if defined(CONFIG_CS752X_SD) || defined(CONFIG_CS752X_SDIO) || defined(CONFIG_CS752X_SDIO_MODULE)
	&goldengate_sd_device,
#endif				/* CS752X_SD */
	&cs75xx_gpio_device,	/* CS75XX GPIO */
	&cs75xx_pwm_device,
	&cs75xx_gpio_keys_device,
#if 0 /* Disable OpenWRT gpio button patch */
	&cs75xx_gpio_btns_device,
#endif /* Disable OpenWRT gpio button patch */
	&cs75xx_led_device,
	&cs75xx_leds_pwm,
	&cs75xx_i2c_device,	/* CS75XX I2C */
	&cs75xx_spi_device,	/* CS75XX SPI */
	&goldengate_flash_device,	/* G2 NAND */
	&cs75xx_ir_device,	/* CS75XX IR */
	&cs75xx_pwr_device,	/* CS75XX PWR */
	&cs75xx_sport_device,	/* CS75XX SSP */
#if defined(CONFIG_SND_SOC_SPDIF)
	&spdif_dit_device,
#endif
#ifdef CONFIG_HW_RANDOM_CS75XX
	&cs75xx_trng_device,	/* CS75XX TRNG */
#endif
	&cs75xx_spacc_device,	/* CS75XX SPAcc */
	&goldengate_ne_device,	/* G2 NE */
	&g2_rw_device,		/* G2 read/write register */
	&goldengate_wdt_device,	/* G2 Watchdog Timer */
	&goldengate_rtc_device,	/* G2 RTC */
	&goldengate_ts_device,	/* G2 TS */
	&goldengate_pcie_device,	/* G2 PCIe */
	&cs752x_ehci_device,	/* G2 EHCI */
#ifndef CONFIG_CORTINA_FPGA
	&cs752x_ohci_device,	/* G2 OHCI */
#endif
	&cs752x_otg_device,	/* G2 OTG */
#ifdef CONFIG_CORTINA_G2_ADMA
	&goldengate_dma_device,	/* Asynchro DMA */
#endif
#ifdef CONFIG_PHONE_CS75XX_WRAPPER
	&cs75xx_phone_device,
#endif
};

void goldengate_acp_update(void)
{
	static const struct {
		u32 acp_mask;
		struct platform_device *pdev;
	} acp_user[] = {
		{ CS75XX_ACP_ENABLE_AHCI, &goldengate_ahci_device },
		/*{ CS75XX_ACP_ENABLE_MMC, },*/
		{ CS75XX_ACP_ENABLE_USB, &cs752x_ehci_device },
		/*{ CS75XX_ACP_ENABLE_LCD, },*/
		{ CS75XX_ACP_ENABLE_SSP, &cs75xx_sport_device },
		/*{ CS75XX_ACP_ENABLE_CRYPT, },*/
		{ CS75XX_ACP_ENABLE_TS, &goldengate_ts_device },
#ifdef CONFIG_CORTINA_G2_ADMA
		{ CS75XX_ACP_ENABLE_DMA, &goldengate_dma_device },
#endif
		{ CS75XX_ACP_ENABLE_FLASH, &goldengate_flash_device },
		{ CS75XX_ACP_ENABLE_NI, &goldengate_ne_device },
		{ CS75XX_ACP_ENABLE_PCI_TX, &goldengate_pcie_device },
		{ CS75XX_ACP_ENABLE_PCI_RX, &goldengate_pcie_device },
		/*FIXME: If you want PCI ACP only for RX, 
			  please ask Jason Li*/
	};
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(acp_user); ++i) {
		if (cs_acp_enable & acp_user[i].acp_mask)
			set_dma_ops(&acp_user[i].pdev->dev, &acp_dma_ops);
		else
			set_dma_ops(&acp_user[i].pdev->dev, &arm_dma_ops);
	}
}
EXPORT_SYMBOL(goldengate_acp_update);

void regbus_ack_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* RegBus doesn't provide ack register, so just disable it */
	if ((irq > (REGBUS_IRQ_BASE + REGBUS_AGGRE_NO)) ||
		(irq < REGBUS_IRQ_BASE)) {
		printk("%s wrong irq no:%d\n", __func__, irq);
		return;
	}

	spin_lock(&regbus_irq_controller_lock);
	val = readl(IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	val &= ~(1 << (irq - REGBUS_IRQ_BASE));
	writel(val, IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	spin_unlock(&regbus_irq_controller_lock);
}

void regbus_mask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* Disable INT */
	if ((irq > (REGBUS_IRQ_BASE + REGBUS_AGGRE_NO)) ||
		(irq < REGBUS_IRQ_BASE)) {
		printk("%s wrong irq no:%d\n", __func__, irq);
		return;
	}

	spin_lock(&regbus_irq_controller_lock);
	val = readl(IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	val &= ~(1 << (irq - REGBUS_IRQ_BASE));
	writel(val, IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	spin_unlock(&regbus_irq_controller_lock);
}

void regbus_unmask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* Enable INT */
	if ((irq > (REGBUS_IRQ_BASE + REGBUS_AGGRE_NO)) ||
		(irq < REGBUS_IRQ_BASE)) {
		printk("%s wrong irq no:%d\n", __func__, irq);
		return;
	}

	spin_lock(&regbus_irq_controller_lock);
	val = readl(IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	val |= 1 << (irq - REGBUS_IRQ_BASE);
	writel(val, IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	spin_unlock(&regbus_irq_controller_lock);
}

static void regbus_irq_handler(struct irq_desc *desc)
{
	unsigned int irq_stat;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int i = 0;

	chained_irq_enter(chip, desc);

	/* Read Status to decide interrupt source */
	spin_lock(&regbus_irq_controller_lock);
	irq_stat = __raw_readl((volatile void __iomem *)GOLDENGATE_REGBUS_BASE + 0);
	spin_unlock(&regbus_irq_controller_lock);

	irq_stat &= REGBUS_INT_MASK;

	for (i = REGBUS_IRQ_BASE; irq_stat; i++, irq_stat >>= 1)
		if (irq_stat & 1)
			generic_handle_irq(i);

	chained_irq_exit(chip, desc);
}

#ifdef CONFIG_PM
extern u16 regbus_wakeups;
extern u16 regbus_backups;
static int regbus_set_wake(struct irq_data *d, unsigned int value)
{
	unsigned irq = d->hwirq;

	/* Enable INT */
	if ((irq > (REGBUS_IRQ_BASE + REGBUS_AGGRE_NO)) ||
		(irq < REGBUS_IRQ_BASE)) {
		printk("%s wrong irq no:%d\n", __func__, irq);
		return -ENXIO;
	}

	if (value)
		regbus_wakeups |= 1 << (irq - REGBUS_IRQ_BASE);
	else
		regbus_wakeups &= ~(1 << (irq - REGBUS_IRQ_BASE));

	return 0;
}
#endif

static struct irq_chip regbus_irq_chip = {
	.name = "REGBUS",
	.irq_ack = regbus_ack_irq,
	.irq_mask = regbus_mask_irq,
	.irq_unmask = regbus_unmask_irq,
#ifdef CONFIG_PM
	.irq_set_wake = regbus_set_wake,
#endif
};

/* DMA Engine IRQ dispatcher */
void dmaeng_ack_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* No ack register, so just disable it */
	if((irq > (IRQ_DMAENG_BASE + IRQ_DMAENG_NO)) ||
		(irq < IRQ_DMAENG_BASE)){
		printk("%s wrong irq no:%d\n", __func__, irq);
		return ;
	}

	spin_lock(&dmaeng_irq_controller_lock);
	val = readl(IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	val &= ~(1 << (irq - IRQ_DMAENG_BASE));
	writel(val,IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	spin_unlock(&dmaeng_irq_controller_lock);
}

void dmaeng_mask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* Disable INT */
	if((irq > (IRQ_DMAENG_BASE + IRQ_DMAENG_NO)) ||
		(irq < IRQ_DMAENG_BASE)){
		printk("%s wrong irq no:%d\n", __func__, irq);
		return ;
	}

	spin_lock(&dmaeng_irq_controller_lock);
	val = readl(IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	val &= ~(1 << (irq - IRQ_DMAENG_BASE));
	writel(val,IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	spin_unlock(&dmaeng_irq_controller_lock);
}

void dmaeng_unmask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* Enable INT */
	if((irq > (IRQ_DMAENG_BASE + IRQ_DMAENG_NO)) ||
		(irq < IRQ_DMAENG_BASE)){
		printk("%s wrong irq no:%d\n", __func__, irq);
		return ;
	}

	spin_lock(&dmaeng_irq_controller_lock);
	val = readl(IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	val |= 1 << (irq - IRQ_DMAENG_BASE);
	writel(val,IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	spin_unlock(&dmaeng_irq_controller_lock);
}

static void dmaeng_irq_handler(struct irq_desc *desc)
{
	unsigned int irq_stat;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int i=0;

	chained_irq_enter(chip, desc);

	/* Read Status to decide interrupt source */
	spin_lock(&dmaeng_irq_controller_lock);
	irq_stat = __raw_readl((volatile void __iomem *)DMA_DMA_LSO_DMA_LSO_INTERRUPT_0);
	spin_unlock(&dmaeng_irq_controller_lock);

	irq_stat &= DMAENG_INT_MASK;

	for (i = IRQ_DMAENG_BASE; irq_stat; i++, irq_stat >>= 1)
		if (irq_stat & 1)
			generic_handle_irq(i);

	chained_irq_exit(chip, desc);
}

static struct irq_chip dmaeng_irq_chip = {
	.name		= "DMA-ENGINE",
	.irq_ack	= dmaeng_ack_irq,
	.irq_mask	= dmaeng_mask_irq,
	.irq_unmask	= dmaeng_unmask_irq,
};

/* DMA SSP IRQ dispatcher */
void dmassp_ack_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* No ack register, so just disable it */
	if((irq > (IRQ_DMASSP_BASE + IRQ_DMASSP_NO)) ||
		(irq < IRQ_DMASSP_BASE)){
		printk("%s wrong irq no:%d\n", __func__, irq);
		return ;
	}

	spin_lock(&dmassp_irq_controller_lock);
	val = readl(IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	val &= ~(1 << (irq - IRQ_DMASSP_BASE));
	writel(val,IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	spin_unlock(&dmassp_irq_controller_lock);
}

void dmassp_mask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* Disable INT */
	if((irq > (IRQ_DMASSP_BASE + IRQ_DMASSP_NO)) ||
		(irq < IRQ_DMASSP_BASE)){
		printk("%s wrong irq no:%d\n", __func__, irq);
		return ;
	}

	spin_lock(&dmassp_irq_controller_lock);
	val = readl(IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	val &= ~(1 << (irq - IRQ_DMASSP_BASE));
	writel(val,IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	spin_unlock(&dmassp_irq_controller_lock);
}

void dmassp_unmask_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	unsigned int val;

	/* Enable INT */
	if((irq > (IRQ_DMASSP_BASE + IRQ_DMASSP_NO)) ||
		(irq < IRQ_DMASSP_BASE)){
		printk("%s wrong irq no:%d\n", __func__, irq);
		return ;
	}

	spin_lock(&dmassp_irq_controller_lock);
	val = readl(IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	val |= 1 << (irq - IRQ_DMASSP_BASE);
	writel(val,IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	spin_unlock(&dmassp_irq_controller_lock);
}

static void dmassp_irq_handler(struct irq_desc *desc)
{
	unsigned int irq_stat;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int i=0;

	chained_irq_enter(chip, desc);

	/* Read Status to decide interrupt source */
	spin_lock(&dmassp_irq_controller_lock);
	irq_stat = __raw_readl((volatile void __iomem *)DMA_DMA_SSP_DMA_SSP_INTERRUPT_0);
	spin_unlock(&dmassp_irq_controller_lock);

	irq_stat &= DMAENG_INT_MASK;

	for (i = IRQ_DMASSP_BASE; irq_stat; i++, irq_stat >>= 1)
		if (irq_stat & 1)
			generic_handle_irq(i);

	chained_irq_exit(chip, desc);
}

static struct irq_chip dmassp_irq_chip = {
	.name		= "DMA-SSP",
	.irq_ack	= dmassp_ack_irq,
	.irq_mask	= dmassp_mask_irq,
	.irq_unmask	= dmassp_unmask_irq,
};

void __init gic_init_irq(void)
{
	int j;

	/* core tile GIC, primary */
	gic_init(0, 29, __io_address(GOLDENGATE_GIC_DIST_BASE),
			__io_address(GOLDENGATE_GIC_CPU_BASE));

	/* REG Bus INT Controller, secondary */
	/* mask and all interrupts */
	writel(0, IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	for (j = REGBUS_IRQ_BASE; j < REGBUS_IRQ_BASE + REGBUS_AGGRE_NO; j++) {
		irq_set_chip(j, &regbus_irq_chip);
		irq_set_handler(j, handle_level_irq);
		irq_clear_status_flags(j, IRQ_NOREQUEST);
	}
	irq_set_chained_handler(IRQ_PERI_REGBUS, regbus_irq_handler);
	irq_set_handler_data(IRQ_PERI_REGBUS, NULL);
	irq_set_affinity(IRQ_PERI_REGBUS, cpu_all_mask);

	/* DMA SSP INT dispatcher */
	/* mask all interrupts */
	writel(0,IO_ADDRESS(DMA_DMA_SSP_DMA_SSP_INTENABLE_0));
	for (j = IRQ_DMASSP_BASE ; j < IRQ_DMASSP_BASE + IRQ_DMASSP_NO; j++) {
		irq_set_chip(j, &dmassp_irq_chip);
		irq_set_handler(j, handle_level_irq);
		irq_clear_status_flags(j, IRQ_NOREQUEST);
	}
	irq_set_chained_handler(IRQ_REGBUS_SSP, dmassp_irq_handler);
	irq_set_handler_data(IRQ_REGBUS_SSP, NULL);

	/* DMA Engine INT dispatcher */
	/* mask all interrupts */
	writel(0,IO_ADDRESS(DMA_DMA_LSO_DMA_LSO_INTENABLE_0));
	for (j = IRQ_DMAENG_BASE ; j < IRQ_DMAENG_BASE + IRQ_DMAENG_NO; j++) {
		irq_set_chip(j, &dmaeng_irq_chip);
		irq_set_handler(j, handle_level_irq);
		irq_clear_status_flags(j, IRQ_NOREQUEST);
	}
	irq_set_chained_handler(IRQ_DMA, dmaeng_irq_handler);
	irq_set_handler_data(IRQ_DMA, NULL);
	irq_set_affinity(IRQ_DMA, cpu_all_mask);

}

#ifdef CONFIG_HAVE_ARM_TWD

static DEFINE_TWD_LOCAL_TIMER(twd_local_timer, IO_ADDRESS(GOLDENGATE_TWD_BASE), IRQ_LOCALTIMER);

static void __init goldengate_twd_init(void)
{
        int err = twd_local_timer_register(&twd_local_timer);
        if (err)
                pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define goldengate_twd_init()        do {} while(0)
#endif

static void __init goldengate_timer_init(void)
{
	unsigned int timer_irq;

	timer0_va_base = __io_address(TIMER0_BASE);
	timer1_va_base = __io_address(TIMER1_BASE);

	timer_irq = GOLDENGATE_IRQ_TIMER0;


	goldengate_clock_init(timer_irq);

#ifdef CONFIG_HAVE_ARM_TWD
	goldengate_twd_init();
#endif

#ifdef CONFIG_CORTINA_G2_GLOBAL_TIMER
	global_timer_register();
#endif
}

int cs_rtl8211_phy_addr[] = {
	1,
	2,
	0
};

static void __init cs75xx_serial_init(void)
{
	int tmp;

	tmp = readl(IO_ADDRESS(GLOBAL_GPIO_MUX_2));
	tmp &= ~0x00003C00;	/* Bit[13:10] */
	writel(tmp, IO_ADDRESS(GLOBAL_GPIO_MUX_2));

#if defined(CONFIG_CORTINA_REFERENCE) || defined(CONFIG_CORTINA_REFERENCE_B)
	tmp = readl(IO_ADDRESS(GLOBAL_GPIO_MUX_2));
	tmp &= ~0x00003000;	/* Bit[13:12] */
	writel(tmp, IO_ADDRESS(GLOBAL_GPIO_MUX_2));
#endif
}

static void __init goldengate_init(void)
{
#if (defined(CONFIG_FB_CS752X_CLCD) | defined(CONFIG_FB_CS752X_CLCD_MODULE))
	int i;
#endif
	struct platform_clk clk;
	GLOBAL_SCRATCH_t global_scratch;

#ifdef CONFIG_CACHE_L2X0
#ifdef CONFIG_ACP
	GLOBAL_ARM_CONFIG_D_t gbl_arm_cfg_d;
	gbl_arm_cfg_d.wrd = readl((volatile void __iomem *)GLOBAL_ARM_CONFIG_D);
	/* Peripheral Cache: Write Back, No write allocate */
	gbl_arm_cfg_d.bf.periph_cache = 0x7;
	writel(gbl_arm_cfg_d.wrd, (volatile void __iomem *)GLOBAL_ARM_CONFIG_D);
#endif
	void __iomem *l2x0_base = __io_address(GOLDENGATE_L220_BASE);

	writel(0, l2x0_base + L310_TAG_LATENCY_CTRL);
	writel(0x00000001, l2x0_base + L310_DATA_LATENCY_CTRL);

	/* updated by hkou & ted 02/23/2012 */
	//l2x0_init(__io_address(GOLDENGATE_L220_BASE), 0x00740000, 0xfe000fff);
	l2x0_init(__io_address(GOLDENGATE_L220_BASE), 0x40740001, 0x8200c3fe);
#endif
	/* Check Chip version in JTAG ID */
	system_rev = cs_get_soc_type();
	if (system_rev == CS_SOC_UNKNOWN)
		printk("Wrong JTAG ID:%p (FPGA ?)\n", __io_address(GLOBAL_JTAG_ID));

	if (cs_soc_is_cs7522a1() || cs_soc_is_cs7542a1()) {
		cs_acp_enable = 0;
#if defined(CONFIG_SMB_TUNING) && defined(CONFIG_SMP)
		cs_acp_enable |= CS75XX_ACP_ENABLE_AHCI | CS75XX_ACP_ENABLE_USB;
#endif
		writel(0x210FF, (volatile void __iomem *)GLOBAL_ARM_CONFIG_D);

		global_scratch.wrd = readl((volatile void __iomem *)GLOBAL_SCRATCH);
		if (cs_acp_enable & 0x000007FF) {
			/*enable the recirc ACP transactions "secured"*/
			global_scratch.wrd |= 0x1000;
		} else {
			global_scratch.wrd &= ~0x1000;
		}
		writel(global_scratch.wrd, (volatile void __iomem *)GLOBAL_SCRATCH);

		writel(0xFF,__io_address(GOLDENGATE_L220_BASE) + 0x910);
		writel(0xFF,__io_address(GOLDENGATE_L220_BASE) + 0x914);
	}else{
		cs_acp_enable = 0;
	}
	goldengate_acp_update();

	get_platform_clk(&clk);

#ifdef CONFIG_I2C_CS75XX
	cs75xx_i2c_cfg.freq_rcl = clk.apb_clk;
#endif
#ifdef CONFIG_SPI_CS75XX
	cs75xx_spi_cfg.tclk = clk.apb_clk;
#endif

	platform_add_devices(platform_devices, ARRAY_SIZE(platform_devices));
	//Ryan Add for LCD
	cs75xx_add_device_lcd();

#if defined(CONFIG_I2C_CS75XX) && defined(CONFIG_I2C_BOARDINFO)
	i2c_register_board_info(0, i2c_board_infos, ARRAY_SIZE(i2c_board_infos));
#endif
#ifdef CONFIG_SPI_CS75XX
	spi_register_board_info(spi_board_infos, ARRAY_SIZE(spi_board_infos));
#endif

	/* Initialize GPIO pin muxing
	 * GPIO_0 is shared with Pflash/nflash/sflash */
	writel(0xff80ff00, IO_ADDRESS(GLOBAL_GPIO_MUX_0));

	/* GPIO_1 is shared with TS/I2C/SPI/SSP/UART0 */
	writel(0xfffffc88, IO_ADDRESS(GLOBAL_GPIO_MUX_1));

	/* GPIO_2 is shared with TS/UART1/UART2/UART3/SD */
	writel(0xffffffff, IO_ADDRESS(GLOBAL_GPIO_MUX_2));

	/* GPIO_3 is shared with GMAC1/GMAC2 */
	writel(0x00000000, IO_ADDRESS(GLOBAL_GPIO_MUX_3));

	/* GPIO_4 is shared with LCD */
	writel(0xffffffff, IO_ADDRESS(GLOBAL_GPIO_MUX_4));

	cs75xx_serial_init();
}

#define CONFIG_CS752X_NR_QMBANK 1
void goldengate_fixup(struct tag *tags, char **from)
{
	volatile u32 mem_sz=0;

	mem_sz = readl(IO_ADDRESS(GLOBAL_SOFTWARE2)) >> 20;
	printk("Mem_size=%d\n", mem_sz);

	memblock_add(GOLDENGATE_DRAM_BASE,
		(mem_sz - (mem_sz>>3)*CONFIG_CS752X_NR_QMBANK)*0x100000);
}

static inline void goldengate_reset(enum reboot_mode mode, const char *cmd)
{
#ifndef CONFIG_CORTINA_FPGA
	/*
	 * To reset, use watchdog to reset whole system
	 */
	unsigned int reg_v;
	reg_v = readl(IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));
	/* enable axi & L2 reset */
	reg_v &= ~0x00000300;

	/* wd*_enable are exclusive with wd0_reset_subsys_enable */
	reg_v &= ~0x0000000E;

	/* reset remap, all block & subsystem */
	reg_v |= 0x000000F0;
	writel(reg_v, IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));

	/* Stall RCPU0/1, stall and clocken */
	writel(0x129, IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));

	/* Reset external device */
	reg_v = readl(IO_ADDRESS(GLOBAL_SCRATCH));
	reg_v |= 0x400;
	writel(reg_v, IO_ADDRESS(GLOBAL_SCRATCH));
	//mdelay(10);
	reg_v &= ~0x400;
	writel(reg_v, IO_ADDRESS(GLOBAL_SCRATCH));

	/* Fire */
	writel(0, IO_ADDRESS(GOLDENGATE_TWD_BASE + 0x28)); /* Disable WD */
	writel(10, IO_ADDRESS(GOLDENGATE_TWD_BASE + 0x20)); /* LOAD */
	/* Enable watchdog - prescale=256, watchdog mode=1, enable=1 */
	writel(0x0000FF09, IO_ADDRESS(GOLDENGATE_TWD_BASE + 0x28)); /* Enable WD */
#endif
}

#ifdef CONFIG_CORTINA_FPGA
MACHINE_START(GOLDENGATE, "CORTINA-G2 FPGA")
#else
MACHINE_START(GOLDENGATE, "CORTINA-G2 EB")
#endif
    /* Maintainer: Cortina-Systems Digital-Home BU */
    .atag_offset    = 0x100,
    .nr = MACH_TYPE_GOLDENGATE,
    .smp = smp_ops(goldengate_smp_ops),
    .fixup = goldengate_fixup,
    .map_io = goldengate_map_io,
    .init_irq = gic_init_irq,
    .init_time = goldengate_timer_init,
    .init_machine = goldengate_init,
    .restart = goldengate_reset,
MACHINE_END
