/*
 *  Copyright c 2001 Flaga hf. Medical Devices, Kari Daviesson <kd@flaga.is>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation,
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <mach/platform.h>
#include <mach/hardware.h>
#include "../mtdcore.h"

#define FLASH_PHYS_ADDR GOLDENGATE_FLASH_BASE
//#define FLASH_SIZE 0x08000000 //64MB
#define FLASH_SIZE CONFIG_MTD_CORTINA_CS752X_PFLASH_SIZE

/* the base address of FLASH control register */
#define FLASH_CONTROL_BASE_ADDR	    (IO_ADDRESS(FLASH_ID))
//#define cs752x_GLOBAL_BASE_ADDR     (IO_ADDRESS(GOLDENGATE_GLOBAL_BASE))
//#define       cs752x_FLASH_ACCESS_REG (0x00000020)
//#define       cs752x_PFLASH_DIRECT_ACCESS     (0x00004000)

FLASH_PF_ACCESS_t pf_access;

static struct map_info cs752x_map = {
	.name = "cs752x_nor_flash",
	//.size =               FLASH_SIZE,
	.bankwidth = 2,
};

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
#include <linux/semaphore.h>
extern struct semaphore cs752x_flash_sem;
#endif

#if 1
static struct mtd_partition cs752x_parts[] = {
 {name: "Boot", offset: 0x00000000, size:0x00500000,},
 {name: "Kernel", offset: 0x00500000, size:0x2000000,},

};
#else
static struct mtd_partition cs752x_parts[] = {
 {name: "Boot", offset: 0x00000000, size:0x00020000,},
 {name: "Kernel", offset: 0x00020000, size:0x00300000,},
 {name: "Ramdisk", offset: 0x00320000, size:0x00800000,},
 {name: "Application", offset: 0x00B20000, size:0x00400000,},
 {name: "VCTL", offset: 0x00F20000, size:0x00020000,},
 {name: "CurConf", offset: 0x00F40000, size:0x000A0000,},
 {name: "FIS directory", offset: 0x00FE0000, size:0x00020000,}
};
#endif
#define PARTITION_COUNT ARRAY_SIZE(cs752x_parts)

static struct mtd_info *mymtd;

static int nr_mtd_parts;
static struct mtd_partition *mtd_parts;
static const char *probes[] = { "cmdlinepart", NULL };

#ifdef CONFIG_CORTINA_cs752x_SHARE_PIN
void cs752x_flash_enable_parallel_flash(void)
{

	pf_access.wrd = 0;
	pf_access.bf.pflashCeAlt = 0;	//chip 0 enable
	pf_access.bf.pflashDirWr = 1;	//enable direct access
	write_flash_ctrl_reg(FLASH_PF_ACCESS_REG, pf_access.wrd);

	return;
}

EXPORT_SYMBOL(cs752x_flash_enable_parallel_flash);

void cs752x_flash_disable_parallel_flash(void)
{
	unsigned int reg_val;

	pf_access.wrd = 0;
	pf_access.bf.pflashCeAlt = 0;	//chip 0 enable
	pf_access.bf.pflashDirWr = 0;	//disable direct access
	write_flash_ctrl_reg(FLASH_PF_ACCESS_REG, pf_access.wrd);
	return;
}

EXPORT_SYMBOL(cs752x_flash_disable_parallel_flash);
#endif

static int __init cs752x_init_cfi(void)
{
	unsigned int reg_val;
	char *part_type = NULL;

	printk("CS-cs752x MTD Driver Init.......\n");

#ifdef CONFIG_CORTINA_cs752x_SHARE_PIN
	cs752x_flash_enable_parallel_flash();	/* enable Parallel FLASH */
#endif

	/* parallel flash direct access mode */
	// write_flash_ctrl_reg(cs752x_FLASH_ACCESS_REG,cs752x_PFLASH_DIRECT_ACCESS);

	cs752x_map.phys = FLASH_PHYS_ADDR;
	cs752x_map.size = FLASH_SIZE;
  /** printk("Flash size : 0x%x\n",cs752x_map.size);**/
	cs752x_map.virt = ioremap(FLASH_PHYS_ADDR, FLASH_SIZE);

	if (!cs752x_map.virt) {
		printk("Failed to ioremap\n");
#ifdef CONFIG_CORTINA_cs752x_SHARE_PIN
		cs752x_flash_disable_parallel_flash();	/* disable Parallel FLASH */
#endif
		return -EIO;
	}
	// simple_map_init(&cs752x_map);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	writel( 0x1E00, 0xf005000c);
#endif

#ifdef CONFIG_MTD_JEDECPROBE
	mymtd = do_map_probe("jedec_probe", &cs752x_map);
#else
	mymtd = do_map_probe("cfi_probe", &cs752x_map);
#endif

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

	if (!mymtd) {
		printk("Failed to probe\n");
#ifdef CONFIG_CORTINA_cs752x_SHARE_PIN
		cs752x_flash_disable_parallel_flash();	/* disable Parallel FLASH */
#endif
	} else			//if (mymtd) {
	{
		mymtd->owner = THIS_MODULE;

		nr_mtd_parts = 0;
		printk(KERN_INFO "Checking for command line partitions ...\n");
		nr_mtd_parts =
		    parse_mtd_partitions(mymtd, probes, &mtd_parts, 0);
		if (nr_mtd_parts > 0)
			part_type = "command line";
		if (nr_mtd_parts <= 0) {
			mtd_parts = cs752x_parts;
			nr_mtd_parts = PARTITION_COUNT;
			part_type = "builtin";
		}
		printk(KERN_INFO "Using %s partition table\n", part_type);
		add_mtd_partitions(mymtd, mtd_parts, nr_mtd_parts);

		printk(KERN_NOTICE "CS752x flash device initialized\n");
#ifdef CONFIG_CORTINA_cs752x_SHARE_PIN
		cs752x_flash_disable_parallel_flash();	/* disable Parallel FLASH */
#endif
		return 0;
	}

	//    iounmap((void *)cs752x_map.virt);

	return -ENXIO;
}

static void __exit cs752x_cleanup_cfi(void)
{
	if (mymtd) {
		mtd_device_unregister(mymtd);
		map_destroy(mymtd);
	}
	if (cs752x_map.virt) {
		iounmap((void *)cs752x_map.virt);
		cs752x_map.virt = 0;
	}
}

module_init(cs752x_init_cfi);
module_exit(cs752x_cleanup_cfi);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Middle Huang <middle.huang@cortina-systems.com>");
MODULE_DESCRIPTION("MTD map driver for Cortina CS752x flash module");
