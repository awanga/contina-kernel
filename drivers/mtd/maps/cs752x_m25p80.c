/*
 * $Id: cs752x_m25p80.c,v 1.4 2011/05/31 13:25:11 middle Exp $
 *
 * Flash and EPROM on Hitachi Solution Engine and similar boards.
 *
 * (C) 2001 Red Hat, Inc.
 *
 * GPL'd
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/init.h> 
#include <mach/platform.h>
#include <mach/hardware.h>
#include <mach/cs752x_flash.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

#define  g_chipen     SERIAL_FLASH_CHIP0_EN

static void cs752x_m25p80_write_cmd(__u8 cmd, __u32 schip_en);
int cs752x_m25p80_sector_erase(__u32 address, __u32 schip_en);

#ifdef CONFIG_MTD_REDBOOT_PARTS
extern	static int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts, unsigned long fis_origin);
#endif

static int cs752x_st_erase(struct mtd_info *mtd, struct erase_info *instr);
static int cs752x_st_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cs752x_st_write (struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
static void cs752x_st_nop (struct mtd_info *);
struct mtd_info *cs752x_map_serial_probe(struct map_info *map);

void cs752x_m25p80_copy_from(struct map_info *map, void *buf, unsigned long ofs, ssize_t len);
__u32 cs752x_m25p80_read32(struct map_info *map, unsigned long ofs);
void cs752x_m25p80_write32(struct map_info *map, __u32 d, unsigned long ofs);
void cs752x_m25p80_copy_to(struct map_info *map, unsigned long ofs, void *buf, ssize_t len);

static struct mtd_info *cs752x_st_mtd;

static struct mtd_partition *parsed_parts;
static FLASH_TYPE_t sf_type;

static struct map_info m25p80_map = {

	.name = "cs752x serial flash m25p80",
	.size = 1048576, /* 0x100000, */
		/* buswidth: 4, */
	.bankwidth = 4,
	.phys =		 GOLDENGATE_FLASH_BASE,
#ifdef CONFIG_MTD_COMPLEX_MAPPINGS	
	.copy_from = cs752x_m25p80_copy_from,
	.read = cs752x_m25p80_read32,
	.write = cs752x_m25p80_write32,
	.copy_to = cs752x_m25p80_copy_to
#endif
};


#ifdef CONFIG_CORTINA_FPGA 

static struct mtd_partition cs752x_m25p80_partitions[] = {
	/* boot code */
	{ .name = "bootloader", .offset = 0x00000000, .size = 0x100000, },
};

#else

static struct mtd_partition cs752x_m25p80_partitions[] = {
	
	/* boot code */
	{ .name = "bootloader", .offset = 0x00000000, .size = 0x20000, },
	/* kernel image */
	{ .name = "kerel image", .offset = 0x000020000, .size = 0xC0000 },
	/* All else is writable (e.g. JFFS) */
	{ .name = "user data", .offset = 0x000E0000, .size = 0x00010000, },
	

};

#endif

static void cs752x_m25p80_read(__u32 address, __u8 *data, __u32 schip_en)
{
	__u32 opcode, status;
	__u32 value;

	opcode = SF_AC_OPCODE_4_ADDR_1_DATA | M25P80_READ;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	status = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
	while (status & SF_START_BIT_ENABLE) {
		status = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	value = read_flash_ctrl_reg(FLASH_SF_DATA);
	*data = value & 0xff;
}


static int cs752x_m25p80_page_dprogram(struct map_info *map, unsigned long ofs, void *buf, ssize_t len)
{
	__u32 opcode;
	__u32 status;
	__u32 tmp;
	int res = FLASH_ERR_OK;
	__u8 *dst;

	opcode = SF_AC_OPCODE_1_DATA | M25P80_READ_STATUS;
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	status = read_flash_ctrl_reg(FLASH_SF_DATA);
	if ((status & SFLASH_STS_WEL) == SFLASH_STS_WEL) {
		
		cs752x_m25p80_write_cmd(M25P80_WRITE_DISABLE, g_chipen);
	}

	cs752x_m25p80_write_cmd(M25P80_WRITE_ENABLE, g_chipen);

	opcode =
	    SF_AC_OPCODE_4_ADDR_1_DATA | M25P80_PAGE_PROGRAM | g_chipen |
	    SFLASH_FORCEBURST;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	/** Bug 27438 
	  In a direct write cycle, the driver programs SF_ACCESS[sflashOpCode] (through
		MPIF) and then starts the direct write cycle through AXI interface.  It happens
		occasionally that the direct write cycle (AXI) kicks off before the SF_ACCESS
		register write completed
	**/
	tmp = read_flash_ctrl_reg(FLASH_SF_ACCESS);
	wmb();
	dst = (__u8 *) (((__u32) m25p80_map.virt) + ofs);
	memcpy(dst, buf, len);

	opcode = g_chipen | SFLASH_FORCETERM;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	opcode = SF_AC_OPCODE_1_DATA | M25P80_READ_STATUS;
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	
	status = read_flash_ctrl_reg(FLASH_SF_DATA);

		
	while(status&SFLASH_STS_WIP)
  	{
  		write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
    		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
 		tmp=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

  		while(tmp&SF_START_BIT_ENABLE)
    		{
    			tmp=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
    		  udelay(1);
    		  schedule();
    		}
    		status = read_flash_ctrl_reg(FLASH_SF_DATA);
    		udelay(1);
    		schedule();
	}
	
	cs752x_m25p80_write_cmd(M25P80_WRITE_DISABLE, g_chipen);

	return res;
}

static int cs752x_m25p80_page_program(__u32 address, __u8 *data, __u32 schip_en)
{
	__u32 opcode;
	__u32 status;
	__u32 tmp;
	int res = FLASH_ERR_OK;

	opcode = SF_AC_OPCODE_1_DATA | M25P80_READ_STATUS;
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	status = read_flash_ctrl_reg(FLASH_SF_DATA);
	if ((status & SFLASH_STS_WEL) == SFLASH_STS_WEL) {
		
		cs752x_m25p80_write_cmd(M25P80_WRITE_DISABLE, schip_en);
	}

	cs752x_m25p80_write_cmd(M25P80_WRITE_ENABLE, schip_en);
	opcode = SF_AC_OPCODE_4_ADDR_1_DATA | M25P80_PAGE_PROGRAM;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_DATA, *data);
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	opcode = SF_AC_OPCODE_1_DATA | M25P80_READ_STATUS;
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	status = read_flash_ctrl_reg(FLASH_SF_DATA);
	while (status & SFLASH_STS_WIP) {
		write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START,
				     SF_START_BIT_ENABLE);
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

		while (tmp & SF_START_BIT_ENABLE) {
			tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
			udelay(1);
			schedule();
		}
		status = read_flash_ctrl_reg(FLASH_SF_DATA);
		udelay(1);
		schedule();
	}

	if ((status & SFLASH_STS_WEL) == SFLASH_STS_WEL)
		cs752x_m25p80_write_cmd(M25P80_WRITE_DISABLE, schip_en);

	return res;
}

void cs752x_m25p80_copy_from(struct map_info *map, void *buf, unsigned long ofs, ssize_t len)
{

	__u8 *buffer;
	__u32 length, opcode;
	length = len;

	buffer = (__u8 *) buf;
	opcode = g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	buffer = (__u8 *) (((__u32) m25p80_map.virt) + (__u32) ofs);

	memcpy(buf, buffer, len);

}

__u32 cs752x_m25p80_read32(struct map_info *map, unsigned long ofs)
{
	__u8 buf[4];

	cs752x_m25p80_copy_from(map, buf, ofs, 4);

	return ((__u32)
		(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24)));
}

void cs752x_m25p80_write32(struct map_info *map, __u32 d, unsigned long ofs)
{
	__u8  buf[4], i;
	
	for(i=0;i<4;i++)
		buf[i] = (d >> (8*i)) & 0xff;
	
		
      cs752x_m25p80_copy_to(map, ofs, buf, 4);
}

void cs752x_m25p80_copy_to(struct map_info *map, unsigned long ofs, void *buf, ssize_t len)
{
	__u32 size, i, ret;

	while (len > 0) {
		size = M25P80_PAGE_SIZE - (ofs % M25P80_PAGE_SIZE);

		if (size > len)
			size = len;

		cs752x_m25p80_page_dprogram(map, ofs, buf, size);

		buf += size;
		ofs += size;
		len -= size;

	};
}

int cs752x_m25p80_sector_erase(__u32 address, __u32 schip_en)
{
	__u32 opcode;
	__u32 status;
	__u32 tmp;
	int res = FLASH_ERR_OK;

	if (address >= FLASH_START)
		address -= FLASH_START;

	cs752x_m25p80_write_cmd(M25P80_WRITE_ENABLE, schip_en);

	opcode = SF_AC_OPCODE_4_ADDR | M25P80_SECTOR_ERASE;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	opcode = SF_AC_OPCODE_1_DATA | M25P80_READ_STATUS;

	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	status = read_flash_ctrl_reg(FLASH_SF_DATA);
	while (status & SFLASH_STS_WIP) {
		write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START,
				     SF_START_BIT_ENABLE);
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

		while (tmp & SF_START_BIT_ENABLE) {
			tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
			udelay(1);
			schedule();
		}
		status = read_flash_ctrl_reg(FLASH_SF_DATA);
		udelay(1);
		schedule();
	}
	if ((status & SFLASH_STS_WEL) == SFLASH_STS_WEL)
		cs752x_m25p80_write_cmd(M25P80_WRITE_DISABLE, schip_en);

	return res;
}

static void cs752x_m25p80_write_cmd(__u8 cmd, __u32 schip_en)
{
	__u32 opcode, tmp;
	__u32 status;

	opcode = SF_AC_OPCODE | cmd;
	opcode |= g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	opcode = SF_AC_OPCODE_1_DATA | M25P80_READ_STATUS;

	opcode |= g_chipen;

	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while (tmp & SF_START_BIT_ENABLE) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	status = read_flash_ctrl_reg(FLASH_SF_DATA);
	if (cmd == M25P80_WRITE_ENABLE) {
		while ((status & (SFLASH_STS_WIP | SFLASH_STS_WEL)) !=
		       SFLASH_STS_WEL) {
			write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
			write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START,
					     SF_START_BIT_ENABLE);
			tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

			while (tmp & SF_START_BIT_ENABLE) {
				tmp =
				    read_flash_ctrl_reg
				    (FLASH_FLASH_ACCESS_START);
				udelay(1);
				schedule();
			}
			status = read_flash_ctrl_reg(FLASH_SF_DATA);
			udelay(1);
			schedule();
		}
	} else if (cmd == M25P80_WRITE_DISABLE) {
		while ((status & (SFLASH_STS_WEL | SFLASH_STS_WIP)) != 0) {
			write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
			write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START,
					     SF_START_BIT_ENABLE);
			tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

			while (tmp & SF_START_BIT_ENABLE) {
				tmp =
				    read_flash_ctrl_reg
				    (FLASH_FLASH_ACCESS_START);
				udelay(1);
				schedule();
			}
			status = read_flash_ctrl_reg(FLASH_SF_DATA);
			udelay(1);
			schedule();
		}
	} else {
		while ((status & SFLASH_STS_WIP) != 0) {
			write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
			write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START,
					     SF_START_BIT_ENABLE);
			tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

			while (tmp & SF_START_BIT_ENABLE) {
				tmp =
				    read_flash_ctrl_reg
				    (FLASH_FLASH_ACCESS_START);
				udelay(1);
				schedule();
			}
			status = read_flash_ctrl_reg(FLASH_SF_DATA);
			udelay(1);
			schedule();
		}
	}
}

int init_cs752x_m25p80(void)
{
#ifdef CONFIG_CORTINA_FPGA
	write_flash_ctrl_reg(FLASH_SF_TIMING, 0x3000000);
#endif

	/* set flash type to setial flash atmel */
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	sf_type.bf.flashType = 0;
	sf_type.bf.flashWidth = 0;	/* st */
	sf_type.bf.flashSize = 0;	/* m25p80 */
	sf_type.bf.flashPin = 1;	/* m25p80 */
	write_flash_ctrl_reg(FLASH_TYPE, sf_type.wrd);

	cs752x_st_mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!cs752x_st_mtd)
		return -ENOMEM;

	memset(cs752x_st_mtd, 0, sizeof(struct mtd_info));
	m25p80_map.virt = (unsigned long)ioremap(GOLDENGATE_FLASH_BASE, SFLASH_SIZE);
	if (!m25p80_map.virt) {
		printk(" failed to ioremap \n");
		return -EIO;
	}

	simple_map_init(&m25p80_map);

	cs752x_st_mtd = do_map_probe("cs752x_st_map_serial", &m25p80_map);
	if (!cs752x_st_mtd) {
		iounmap(m25p80_map.virt);
		return -ENXIO;
	}

	cs752x_st_mtd->owner = THIS_MODULE;

	mtd_device_register(cs752x_st_mtd, cs752x_m25p80_partitions,
			ARRAY_SIZE(cs752x_m25p80_partitions));

	return 0;
}

void cleanup_cs752x_m25p80(void)
{
	if (cs752x_st_mtd) {
		mtd_device_unregister(cs752x_st_mtd);
		map_destroy(cs752x_st_mtd);
	}
}


static struct mtd_chip_driver cs752x_st_chipdrv = {
	probe: cs752x_map_serial_probe,
	name: "cs752x_st_map_serial",
	module: THIS_MODULE
};

struct mtd_info *cs752x_map_serial_probe(struct map_info *map)
{
	struct mtd_info *mtd;

	mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	if (!mtd)
		return NULL;

	memset(mtd, 0, sizeof(*mtd));

	map->fldrv = &cs752x_st_chipdrv;
	mtd->priv = map;
	mtd->name = map->name;
	mtd->type = MTD_NORFLASH;
	mtd->erase = cs752x_st_erase;
	mtd->size = map->size;
	mtd->read = cs752x_st_read;
	mtd->write = cs752x_st_write;
	mtd->sync = cs752x_st_nop;
	mtd->flags = MTD_CAP_NORFLASH;

	mtd->erasesize = M25P80_SECTOR_SIZE; 
	mtd->writesize = M25P80_PAGE_SIZE; /* block size*/;


	__module_get(THIS_MODULE);
	
	return mtd;
}


#ifndef	CONFIG_MTD_CORTINA_cs752x_SERIAL_ST
static __u32 readflash_ctrl_reg(__u32 ofs)
{
    __u32 *base;	
    
    base = (__u32 *)IO_ADDRESS((GOLDENGATE_FLASH_CTRL_BASE + ofs));
    return __raw_readl(base);
}

static void writeflash_ctrl_reg(__u32 data, __u32 ofs)
{
    __u32 *base;
    
    base = (__u32 *)IO_ADDRESS((GOLDENGATE_FLASH_CTRL_BASE + ofs));
    __raw_writel(data, base);
}
#endif

static int cs752x_st_erase_block(struct map_info *map,unsigned int block)
{

	if (!cs752x_m25p80_sector_erase(block, 0))
		return (MTD_ERASE_DONE);
	else
		return (MTD_ERASE_FAILED);

}
    
static int cs752x_st_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = (struct map_info *)mtd->priv;
	unsigned int addr;
	int len;
	unsigned int block;
	unsigned int ret = 0;

	addr = instr->addr;
	len = instr->len;

	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, sf_type.wrd);

	while (len > 0) {
		block = addr / mtd->erasesize;
		ret = cs752x_st_erase_block(map, addr);
		if (ret == MTD_ERASE_FAILED) {
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = ((loff_t) addr);
			goto erase_exit;
		}

		addr = addr + mtd->erasesize;
		len = len - mtd->erasesize;
		schedule();
	}
	instr->state = MTD_ERASE_DONE;

erase_exit:

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;

	if (!ret)
		mtd_erase_callback(instr);

	up(&cs752x_flash_sem);
	return (ret);
}

static int cs752x_st_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = (struct map_info *)mtd->priv;

	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, sf_type.wrd);

	map->copy_from(map, buf, from, len);
	*retlen = len;
	up(&cs752x_flash_sem);
	return 0;
}

static void cs752x_st_nop(struct mtd_info *mtd)
{
	/* Nothing to see here */
}

static int cs752x_st_write (struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	struct map_info *map = (struct map_info *)mtd->priv;

	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, sf_type.wrd);
	map->copy_to(map, to, buf, len);
	*retlen = len;
	up(&cs752x_flash_sem);
	return 0;
}

int __init cs752x_st_map_serial_init(void)
{
	register_mtd_chip_driver(&cs752x_st_chipdrv);
	init_cs752x_m25p80();
	return 0;
}

static void __exit cs752x_st_map_serial_exit(void)
{
	cleanup_cs752x_m25p80();
	unregister_mtd_chip_driver(&cs752x_st_chipdrv);
}

module_init(cs752x_st_map_serial_init);
module_exit(cs752x_st_map_serial_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Middle Huang <middle.huang@cortina-systems.com>");
MODULE_DESCRIPTION("MTD map driver for Cortina cs752x boards");

