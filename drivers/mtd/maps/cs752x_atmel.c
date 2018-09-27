/*
 * $Id: cs752x_atmel.c,v 1.3 2011/05/31 13:24:23 middle Exp $
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


extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static int cs752x_at_mapserial_erase(struct mtd_info *mtd, struct erase_info *instr);
static int cs752x_at_mapserial_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cs752x_at_mapserial_write (struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
static void cs752x_at_mapserial_nop (struct mtd_info *);
struct mtd_info *cs752x_at_map_serial_probe(struct map_info *map);

void cs752x_copy_from(struct map_info *map, void *buf, unsigned long ofs, ssize_t len);
__u32 cs752x_read32(struct map_info *map, unsigned long ofs);
void cs752x_write32(struct map_info *map, __u32 d, unsigned long ofs);
void cs752x_copy_to(struct map_info *map, unsigned long ofs, void *buf, ssize_t len);
void cs752x_main_memory_to_buffer(__u8 cmd, __u16 page);
void cs752x_atmel_buffer_write(__u8 cmd, __u16 offset, __u8 data);

static struct mtd_chip_driver cs752x_at_chipdrv = {
probe: cs752x_at_map_serial_probe,
       name: "cs752x_at_map_serial",
       module: THIS_MODULE
};

static struct mtd_info *cs752x_at_mtd;

static struct mtd_partition *parsed_parts;

static struct map_info cs752x_serial_map = {

	.name = "cs752x serial flash",
	.size = 4194304, 
	.bankwidth = 4,
	.phys =		 GOLDENGATE_FLASH_BASE,
#ifdef CONFIG_MTD_COMPLEX_MAPPINGS	
	.copy_from = cs752x_copy_from,
	.read = cs752x_read32,
	.write = cs752x_write32,
	.copy_to = cs752x_copy_to
#endif
};

static FLASH_TYPE_t sf_type;

unsigned char	g_page_addr=AT45DB321_PAGE_SHIFT;    /* 321 : shift 10  ; 642 : shift 11 */
unsigned char	g_block_addr=AT45DB321_BLOCK_SHIFT;    /*321 : shift 10  ; 642 : shift 11 */
unsigned int	g_chipen=SERIAL_FLASH_CHIP1_EN;   /* atmel */


#ifdef CONFIG_CORTINA_FPGA
static struct mtd_partition cs752x_partitions[] = {

	/* boot code */
	{ .name = "bootloader", .offset = 0x00000000, .size = 0x200000, },
	{ .name = "kerel image", .offset = 0x00200000, .size = 0x200000, },
}; 
#else
static struct mtd_partition cs752x_partitions[] = {

	/* boot code */
	{ .name = "bootloader", .offset = 0x00000000, .size = 0x20000, },
	/* kernel image */
	{ .name = "kerel image", .offset = 0x000020000, .size = 0xE0000 },
	/* All else is writable (e.g. JFFS) */
	{ .name = "user data", .offset = 0x00100000, .size = 0x00300000, },
};
#endif

void cs752x_address_to_page(__u32 address, __u16 *page, __u16 *offset)
{

	/*assume oob of serial flash not use, so skip it, then all page are the same 0x200 */
#if 1
	*offset = address % SPAGE_SIZE;
	*page = address / SPAGE_SIZE; 
#else       
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);

	if(sf_type.bf.flashPin == 0)
	{
		*offset = address % SPAGE_LEG_SIZE;
		*page = address / SPAGE_LEG_SIZE;        
	}        
	else
	{
		*offset = address % SPAGE_SIZE;
		*page = address / SPAGE_SIZE;        

	}    
#endif    

}



void cs752x_atmel_read_status(__u8 cmd, __u8 *data)
{
	__u32 opcode;
	__u32 value;

	opcode = SF_AC_OPCODE_1_DATA | cmd | g_chipen;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	opcode=read_flash_ctrl_reg(FLASH_SF_ACCESS);

	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	value=read_flash_ctrl_reg(FLASH_SF_DATA);
	*data = value & 0xff;	
}

void cs752x_main_memory_page_read(__u8 cmd, __u16 page, __u16 offset, __u8 *data)
{
	__u32 opcode;
	__u32 address;
	__u32 value;

	/*opcode = SF_AC_OPCODE_4_ADDR_1_DATA | cmd | g_chipen;//SF_AC_OPCODE_4_ADDR_4X_1_DATA*/
	opcode = SF_AC_OPCODE_4_ADDR_4X_1_DATA | cmd | g_chipen;/*SF_AC_OPCODE_4_ADDR_4X_1_DATA*/
	address = (page << g_page_addr);

	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;

	address += offset;

	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

	value=read_flash_ctrl_reg(FLASH_SF_DATA);
	*data = value & 0xff;
}

void cs752x_buffer_to_main_memory(__u8 cmd, __u16 page)
{
	__u32 opcode;
	__u32 address;
	__u8  status;

	opcode = SF_AC_OPCODE_4_ADDR | cmd | g_chipen;
	address = (page << g_page_addr);	
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	}

}


void cs752x_atmel_flash_read_page(__u32 address, __u8 *buffer, __u32 len)
{
	__u8  byte;
	__u16 page, offset;
	__u16 i;

	cs752x_address_to_page(address, &page, &offset);

	for(i=0; i<len; i++,offset++)
	{ 
		cs752x_main_memory_page_read(MAIN_MEMORY_PAGE_READ_SPI , page, offset, &byte);
		buffer [i]= byte;
	} 
}

void cs752x_atmel_flash_program_page(__u32 address, __u8 *buffer, __u32 len)
{
	__u8  pattern;
	__u16 page, offset;
	__u32 i, opcode;
	__u8 *dst;

	cs752x_address_to_page(address, &page, &offset);

	/*if(offset)*/
	cs752x_main_memory_to_buffer(MAIN_MEMORY_TO_BUFFER1,page);

#ifdef AT_INDIRECT_ACCESS   /*indirect access */

	for(i=0; i<len; i++,offset++)
	{
		pattern = buffer[i];
		cs752x_atmel_buffer_write(BUFFER1_WRITE,offset,pattern);
	}
#else
	opcode = SF_AC_OPCODE_4_ADDR_1_DATA | BUFFER1_WRITE  | g_chipen | SFLASH_FORCEBURST;
	/*write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);*/
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	/** Bug 27438 
	  In a direct write cycle, the driver programs SF_ACCESS[sflashOpCode] (through
		MPIF) and then starts the direct write cycle through AXI interface.  It happens
		occasionally that the direct write cycle (AXI) kicks off before the SF_ACCESS
		register write completed
	**/
	opcode = read_flash_ctrl_reg(FLASH_SF_ACCESS);
	wmb();
	dst = (__u8 *)(((__u32)cs752x_serial_map.virt) + address);
	memcpy(dst, buffer, len);

	opcode = g_chipen | SFLASH_FORCETERM;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);


#endif
	cs752x_buffer_to_main_memory(BUFFER1_TO_MAIN_MEMORY, page);

	opcode = g_chipen | SFLASH_FORCETERM;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
}


void cs752x_main_memory_to_buffer(__u8 cmd, __u16 page)
{
	__u32 opcode;
	__u32 address;
	__u8  status;

	opcode = SF_AC_OPCODE_4_ADDR | cmd | g_chipen;
	address = (page << g_page_addr);	
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	}

}

void cs752x_main_memory_page_program(__u8 cmd, __u16 page, __u16 offset, __u8 data)
{
	__u32 opcode;
	__u32 address;
	__u8  status;

	/*opcode = FLASH_ACCESS_ACTION_SHIFT_ADDRESS_DATA | cmd | g_chipen;*/
	opcode = SF_AC_OPCODE_4_ADDR_1_DATA | cmd | g_chipen;
	address = (page << g_page_addr);	
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;

	address += offset;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_DATA, data);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	}
}

void cs752x_atmel_buffer_write(__u8 cmd, __u16 offset, __u8 data)
{
	__u32 opcode;
	__u32 address;

	/*opcode = FLASH_ACCESS_ACTION_SHIFT_ADDRESS_DATA | cmd  | g_chipen;*/
	opcode = SF_AC_OPCODE_4_ADDR_1_DATA | cmd  | g_chipen;
	address = offset;	
	/*sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	//if(sf_type.bf.flashPin == 0)
	//		address *= 2;
	*/
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_DATA, data);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}

}

void cs752x_atmel_erase_page(__u8 cmd, __u16 page)
{
	__u32 opcode, test;
	__u32 address;
	__u8  status;

	opcode = SF_AC_OPCODE_4_ADDR | cmd | g_chipen;
	address = (page << g_page_addr);	
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	test = read_flash_ctrl_reg(FLASH_SF_ADDRESS);
	printk("erase : %x\n",test);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	}

}

void cs752x_atmel_erase_block(__u8 cmd, __u16 block)
{
	__u32 opcode;
	__u32 address;
	__u8  status;

	opcode = SF_AC_OPCODE_4_ADDR | cmd | g_chipen;
	address = (block << g_block_addr);	
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, address);
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		udelay(1);
		schedule();
	}
	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	}

}

__u32 cs752x_read32(struct map_info *map, unsigned long ofs)
{

#ifdef AT_INDIRECT_ACCESS   /*indirect access */
	__u16 page, offset;
	__u32 pattern;
	__u8  byte, i;

	pattern = 0;
	cs752x_address_to_page(ofs, &page, &offset);
	for(i=0; i<4; i++, offset++)
	{ 
		pattern = pattern << 8;
		cs752x_main_memory_page_read(MAIN_MEMORY_PAGE_READ_SPI , page, offset, &byte);
		pattern += byte;
	} 
	return pattern;
#else
	unsigned int opcode, *data;
	unsigned char buffer[4];

	opcode = g_chipen | SFLASH_FORCETERM;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	data = (unsigned int *)(((__u32)cs752x_serial_map.virt) + (__u32)ofs);

	opcode = *data | ((*(data+1))<<8) | ((*(data+2))<<16) | ((*(data+3))<<24);

	return opcode;
#endif    

}

__u8 cs752x_read8(struct map_info *map, unsigned long ofs)
{
	__u16 page, offset;
	__u8  byte;

	cs752x_address_to_page(ofs, &page, &offset);
	cs752x_main_memory_page_read(MAIN_MEMORY_PAGE_READ_SPI , page, offset, &byte);
	return byte;

}

void cs752x_write32(struct map_info *map, __u32 d, unsigned long ofs)
{
#ifdef AT_INDIRECT_ACCESS   /*indirect access */    
	__u16 page, offset;
	__u8  byte, i;

	cs752x_address_to_page(ofs, &page, &offset);
	for(i=0; i<4; i++, offset++)
	{ 
		byte = d & 0xff;	
		cs752x_main_memory_page_program(MAIN_MEMORY_PROGRAM_BUFFER1, page, offset, byte);	
		d = d >> 8;
	}
#else
	__u8  buf[4], i;

	for(i=0;i<4;i++)
		buf[i] = (d >> (8*i)) & 0xff;


	cs752x_copy_to(map, ofs, buf, 4);

#endif	
}

void cs752x_write8(struct map_info *map, __u8 d, unsigned long ofs)
{
	__u16 page, offset;

	cs752x_address_to_page(ofs, &page, &offset);
	cs752x_main_memory_page_program(MAIN_MEMORY_PROGRAM_BUFFER1, page, offset, d);	

}

void cs752x_copy_from(struct map_info *map, void *buf, unsigned long ofs, ssize_t len)
{
	__u32 size,page_size, opcode;
	__u8  *buffer;
	__u32 length;



#ifdef AT_INDIRECT_ACCESS   /*indirect access */ 
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);    
	if(sf_type.bf.flashPin == 0)
		page_size = SPAGE_LEG_SIZE;
	else
#endif    
		page_size = SPAGE_SIZE;

	length = len;
	buffer = (__u8 *)buf;

#ifdef AT_INDIRECT_ACCESS   /*indirect access */          
	while(len)
	{
		size = page_size - (ofs%page_size);
		if(size > len)
			size = len;
		cs752x_atmel_flash_read_page(ofs, buffer, size);
		buffer+=size;
		ofs+=size;
		len -= size;
	}	

#else
	opcode = g_chipen ;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	buffer = (__u8 *)(((__u32)cs752x_serial_map.virt) + (__u32)ofs);
	/*for(i=0; i<length; i++)
	//{
	//    buf[i] = buffer[i];  
	//}
	*/
	memcpy(buf, buffer, len);
#endif        

}


void cs752x_copy_to(struct map_info *map, unsigned long ofs, void *buf, ssize_t len)
{
	__u32 size,page_size;
	__u8  *buffer;

	buffer = (__u8 *)buf;

#ifdef AT_INDIRECT_ACCESS   /*indirect access */     
	if(sf_type.bf.flashPin == 0)
		page_size = SPAGE_LEG_SIZE;
	else
#endif    
		page_size = SPAGE_SIZE;

	while(len)
	{
		size = page_size - (ofs%page_size);
		if(size > len)
			size = len;
		cs752x_atmel_flash_program_page(ofs, buffer, size);
		buffer+=size;
		ofs+=size;
		len-=size;
	}

}


int init_cs752x_maps(void)
{
	int nr_parts = 0;
	struct mtd_partition *parts;
	__u8  status;

#ifdef CONFIG_CORTINA_FPGA 	
	/*if fpage board need to set timing*/
	write_flash_ctrl_reg(FLASH_SF_TIMING, 0x3000000);
#endif        

	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	sf_type.bf.flashType = 0;
	sf_type.bf.flashWidth = 1;	/*at*/
	sf_type.bf.flashSize = 1;	/*AT45DB321B*/
	sf_type.bf.flashPin = 0;	/*'power of 2' binary page size (i.e. 256B/512B/1024B);*/
	write_flash_ctrl_reg(FLASH_TYPE, sf_type.wrd);
	
	if ( sf_type.bf.flashWidth != 1 ) {
		return NULL;
	}

	if( sf_type.bf.flashSize == 1 ) {
		g_page_addr= AT45DB321_PAGE_SHIFT;     
		g_block_addr= AT45DB321_BLOCK_SHIFT;   
		g_chipen = SERIAL_FLASH_CHIP1_EN;   /*atmel 321 cs1*/
	} else if ( sf_type.bf.flashSize == 2 ) {
		g_page_addr= AT45DB642_PAGE_SHIFT;
		g_block_addr= AT45DB642_BLOCK_SHIFT;
		g_chipen = SERIAL_FLASH_CHIP0_EN;   /*atmel 642 cs0*/
	} else {
		return NULL;
	}

	nr_parts = 0;

	cs752x_at_mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!cs752x_at_mtd)
		return NULL;

	memset(cs752x_at_mtd, 0, sizeof(struct mtd_info));
	cs752x_serial_map.phys = FLASH_START;
	cs752x_serial_map.virt = (unsigned long)ioremap(GOLDENGATE_FLASH_BASE, SFLASH_SIZE);
	if (!cs752x_serial_map.virt) {
		printk(" failed to ioremap \n");
		kfree( cs752x_at_mtd);
		return -EIO;
	}
	printk("flash virt : %x\n",cs752x_serial_map.virt);
	simple_map_init(&cs752x_serial_map);
	cs752x_at_mtd = do_map_probe("cs752x_at_map_serial", &cs752x_serial_map);
	if (cs752x_at_mtd) {
		cs752x_at_mtd->owner = THIS_MODULE;
		mtd_device_register(cs752x_at_mtd, cs752x_partitions,
				ARRAY_SIZE(cs752x_partitions));
	}


	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	} 


	__u32 opcode;
	const char disable_protection_cmd[]= { 0x3D, 0x2A, 0x7F, 0x9A, 0 };
	const char *p= disable_protection_cmd;

	for(  ; *p ; ++p) {
		opcode = SF_AC_OPCODE | CONTINUOUS_MODE | g_chipen | *p;
		write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

		write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
		opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

		while(opcode&SF_START_BIT_ENABLE)
		{
			opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
			udelay(1);
			schedule();
		}
	}




	if( status & 0x02) { 
		/* Protection status.  Try agian after remove protection.*/

		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		while(!(status&SFLASH_STS_READY))
		{
			cs752x_atmel_read_status(READ_STATUS_SPI, &status);
			udelay(1);
			schedule();
		} 

		if( status & 0x02) { 
			printk( KERN_NOTICE "Flash is at protection status");	
		}
	}


	printk(KERN_NOTICE "status(bit0(0:528, 1:512): %x  mtd->writesize: %x \n",status,cs752x_at_mtd->writesize);

	/*set flash pin
	//direct access no need to modify address but "indirect access" need to 
	//2 * address because Legacy page with oob
	*/
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	sf_type.bf.flashPin = status&0x01;
	write_flash_ctrl_reg(FLASH_TYPE, sf_type.wrd);

	return 0;
}

void cleanup_cs752x_maps(void)
{
	if (cs752x_at_mtd) {
		mtd_device_unregister(cs752x_at_mtd);
		map_destroy(cs752x_at_mtd);
	}
}

struct mtd_info *cs752x_at_map_serial_probe(struct map_info *map)
{

	struct mtd_info *mtd;


	mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	if (!mtd)
		return NULL;


	down( &cs752x_flash_sem);
	write_flash_ctrl_reg( FLASH_TYPE, sf_type.wrd);

	memset(mtd, 0, sizeof(*mtd));

	map->fldrv = &cs752x_at_chipdrv;
	mtd->priv = map;
	mtd->name = map->name;
	mtd->type = MTD_NORFLASH;/*MTD_OTHER;*/
	mtd->erase = cs752x_at_mapserial_erase;
	mtd->size = map->size;
	mtd->read = cs752x_at_mapserial_read;
	mtd->write = cs752x_at_mapserial_write;
	mtd->sync = cs752x_at_mapserial_nop;
	mtd->flags = MTD_CAP_NORFLASH; /*(MTD_WRITEABLE|MTD_ERASEABLE);*/
	/*	mtd->erasesize = 512; // page size;*/


	mtd->erasesize = 0x1000; /* block size;*/
	mtd->writesize = SPAGE_SIZE; /* block size;*/

	__module_get(THIS_MODULE);
	
	up( &cs752x_flash_sem);

	
	/*MOD_INC_USE_COUNT;*/
	return mtd;
}

/*erase unit == page*/
static int cs752x_at_mapserial_erase_block(struct map_info *map,unsigned int block)
{


	__u32 address;
	__u32 opcode;
	__u32 count=0;
	__u8  status;

	opcode = SF_AC_OPCODE_4_ADDR | BLOCK_ERASE | g_chipen;;
	address = (block << g_block_addr);
	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
	if(sf_type.bf.flashPin == 0)
		address *= 2;

	write_flash_ctrl_reg(FLASH_SF_ADDRESS,address);
	write_flash_ctrl_reg(FLASH_SF_ACCESS,opcode);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);
	opcode=read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);

	while(opcode&SF_START_BIT_ENABLE)
	{
		opcode = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		count++;
		if (count > 10000)
		{
			return (MTD_ERASE_FAILED);
		}  
	}
	cs752x_atmel_read_status(READ_STATUS_SPI, &status);
	while(!(status&SFLASH_STS_READY))
	{
		cs752x_atmel_read_status(READ_STATUS_SPI, &status);
		udelay(1);
		schedule();
	}

	return (MTD_ERASE_DONE);
	

}

static int cs752x_at_mapserial_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = (struct map_info *)mtd->priv;
	unsigned int    addr;
	int             len;
	unsigned int    block;
	unsigned int    ret=0;


	addr = instr->addr;
	len = instr->len;
	instr->state = MTD_ERASING;

	down( &cs752x_flash_sem);
	write_flash_ctrl_reg( FLASH_TYPE, sf_type.wrd);

	while (len > 0)
	{
		block = addr / mtd->erasesize;
		ret = cs752x_at_mapserial_erase_block(map,block);
		/*ret = cs752x_atmel_erase_page(map,block);	*/
		if(ret == MTD_ERASE_FAILED)
		{
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = ((loff_t)addr);
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

	up( &cs752x_flash_sem);

	return (ret);
}    

static int cs752x_at_mapserial_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = (struct map_info *)mtd->priv;

	down( &cs752x_flash_sem);
	write_flash_ctrl_reg( FLASH_TYPE, sf_type.wrd);


#ifdef CONFIG_MTD_COMPLEX_MAPPINGS	
	map->copy_from(map, buf, from, len);
#endif
	*retlen = len;

	up( &cs752x_flash_sem);
	return 0;
}

static void cs752x_at_mapserial_nop(struct mtd_info *mtd)
{
	/* Nothing to see here */
}

static int cs752x_at_mapserial_write (struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	struct map_info *map = (struct map_info *)mtd->priv;

	down( &cs752x_flash_sem);
	write_flash_ctrl_reg( FLASH_TYPE, sf_type.wrd);

#ifdef CONFIG_MTD_COMPLEX_MAPPINGS	
	map->copy_to(map, to, buf, len);
#endif
	*retlen = len;

	up( &cs752x_flash_sem);
	return 0;
}

int __init cs752x_at_map_serial_init(void)
{
	printk( "atmel initialize\n");
	write_flash_ctrl_reg( FLASH_TYPE, sf_type.wrd);
/*	sf_type.wrd = read_flash_ctrl_reg(FLASH_TYPE);
//	if( sf_type.bf.flashType != 0 ) {
//		return -EACCES;
//	}
*/
	register_mtd_chip_driver(&cs752x_at_chipdrv);
	init_cs752x_maps();
	printk( "atmel leaved\n");
	return 0;
}

static void __exit cs752x_at_map_serial_exit(void)
{
	cleanup_cs752x_maps();
	unregister_mtd_chip_driver(&cs752x_at_chipdrv);
}

module_init(cs752x_at_map_serial_init);
module_exit(cs752x_at_map_serial_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Middle Huahg <middle.huang@cortina-systems.com>");
MODULE_DESCRIPTION("MTD map driver for Cortina CS752X boards");

