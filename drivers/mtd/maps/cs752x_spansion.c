
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

#define  g_chipen			SERIAL_FLASH_CHIP0_EN

#define SPANSION_WRITE_STATUS_OPCODE	( 0x01 | SF_AC_OPCODE_1_DATA  )
#define SPANSION_PROGRAM_OPCODE         ( 0x02 | SF_AC_OPCODE_4_ADDR_1_DATA )
#define SPANSION_READ_OPCODE            ( 0x03 | SF_AC_OPCODE_4_ADDR_1_DATA )
#define SPANSION_READ_STATUS_OPCODE	( 0x05 | SF_AC_OPCODE_2_DATA  )
#define SPANSION_WRITE_ENABLE_OPCODE	( 0x06 | SF_AC_OPCODE )
#define SPANSION_ERASE_OPCODE           ( 0xD8 | SF_AC_OPCODE_4_ADDR )

#define SPANSION_STATUS_WEL             0x02
#define SPANSION_STATUS_WIP             0x01
#define SPANSION_STATUS_BP		0x1C
#define SPANSION_STATUS_SRWD            0x80

#define SPANSION_WEL_TIMEOUT             400
#define SPANSION_PROGRAM_TIMEOUT         300
#define SPANSION_ERASE_TIMEOUT          4000
#define SPANSION_CMD_TIMEOUT		2000

#define S25FL128P_SECTOR_SIZE         SZ_64K
#define S25FL128P_PAGE_SIZE           SZ_256
#define S25FL128P_SIZE		      SZ_16M

#ifdef _MULTI_FLASH_
#include <linux/semaphore.h>
extern struct semaphore cs752x_flash_sem;
#endif


struct mtd_info *cs752x_map_serial_probe(struct map_info *map);
static struct mtd_info *cs752x_spansion_mtd;


static struct map_info s25fl128p_map = {
	.name = "cs752x_s25fl128p", 
	.size = S25FL128P_SIZE,
	.bankwidth = 4, 	/* buswidth: 4, */
	.phys =	GOLDENGATE_FLASH_BASE,
};


static struct mtd_partition cs752x_s25fl128p_partitions[] = {
	{ .name = "loader",	.offset = 0x0,		.size = SZ_4M,},
	{ .name = "kernel",	.offset = SZ_4M,	.size = SZ_4M,},
	{ .name = "user",	.offset = SZ_8M,	.size = SZ_8M,},
};

static int cs752x_spansion_cmd(u32 opcode, u32 addr, u32 data)
{
	u32 tmp;
	u32 timeout_jiffies;

	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode | g_chipen);
	write_flash_ctrl_reg(FLASH_SF_ADDRESS, addr);
	write_flash_ctrl_reg(FLASH_SF_DATA, data);
	write_flash_ctrl_reg(FLASH_FLASH_ACCESS_START, SF_START_BIT_ENABLE);

	timeout_jiffies = jiffies + msecs_to_jiffies(SPANSION_CMD_TIMEOUT);

	for (;;) {
		tmp = read_flash_ctrl_reg(FLASH_FLASH_ACCESS_START);
		if ((tmp & SF_START_BIT_ENABLE) == 0) {
			return 0;
		}

		if (time_after(jiffies, timeout_jiffies)) {
			return -ETIME;
		}

		udelay(1);
		schedule();
	}
}

static int wait_flash_status(u32 flag, u32 value, u32 wait_msec)
{
	u32 status;
	u32 timeout_jiffies;

	timeout_jiffies = jiffies + msecs_to_jiffies(wait_msec);

	for (;;) {
		if (cs752x_spansion_cmd(SPANSION_READ_STATUS_OPCODE, 0, 0)) {
			return -ETIME;
		}

		status = read_flash_ctrl_reg(FLASH_SF_DATA);
		if ((status & flag) == value) {
			return 0;
		}

		if (time_after(jiffies, timeout_jiffies)) {
			return -ETIME;
		}

		udelay(1);
		schedule();
	}
}

static int protect_lower_bound(u32 status)
{
	u32 protect_lower;

	protect_lower = (status & SPANSION_STATUS_BP) >> 2;
	if (protect_lower == 0) {
		return 0xFFFFFF;
	}

	protect_lower = 0xFFFFFF * (0x01 << (protect_lower - 1)) / 64;
	protect_lower = 0xFFFFFF - protect_lower;

	return protect_lower;
}

/* return 1 when it is writeable */
static int cs752x_s25fl128p_writeable(u32 addr, u32 len)
{
	u32 lower_bound;
	u32 status;

	cs752x_spansion_cmd(SPANSION_READ_STATUS_OPCODE, 0, 0);
	status = read_flash_ctrl_reg(FLASH_SF_DATA);
	if (status & SPANSION_STATUS_SRWD) {
		return 0;
	}

	lower_bound = protect_lower_bound(status);

	if (lower_bound <= addr) {
		return 0;
	}

	addr += len;

	if (lower_bound < addr) {
		return 0;
	}

	return 1;
}

#if defined( _SPANSION_INDIRECT_ACCESS_ )

static int cs752x_s25fl128p_page_program(u32 addr, u8 * data)
{
	if (cs752x_s25fl128p_writeable(addr, 1) == 0) {
		return -EACCES;
	}

	if (cs752x_spansion_cmd(SPANSION_WRITE_ENABLE_OPCODE, 0, 0)) {
		return -ETIME;
	}

	if (wait_flash_status
	    (SPANSION_STATUS_WEL, SPANSION_STATUS_WEL, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	if (cs752x_spansion_cmd(SPANSION_PROGRAM_OPCODE, addr, *data)) {
		return -ETIME;
	}

	if (wait_flash_status(SPANSION_STATUS_WIP, 0, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	return 0;;
}

#else

static int cs752x_s25fl128p_page_dprogram(struct map_info *map,
					  unsigned long addr, const void *buf,
					  ssize_t len)
{
	u32 opcode;
	u8 *dst;

	if (cs752x_s25fl128p_writeable(addr, len) == 0) {
		return -EACCES;
	}

	if (cs752x_spansion_cmd(SPANSION_WRITE_ENABLE_OPCODE, 0, 0)) {
		return -ETIME;
	}

	if (wait_flash_status
	    (SPANSION_STATUS_WEL, SPANSION_STATUS_WEL, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	opcode = SPANSION_PROGRAM_OPCODE | g_chipen | SFLASH_FORCEBURST;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);
	/* Bug 27438
	   In a direct write cycle, the driver programs SF_ACCESS[sflashOpCode] 
	   (through MPIF) and then starts the direct write cycle through AXI
	   interface.  
	   It happens occasionally that the direct write cycle (AXI) kicks off 
	   before the SF_ACCESS register write completed
	 */
	read_flash_ctrl_reg(FLASH_SF_ACCESS);	/* dummy read */
	wmb();
	dst = (u8 *) (((u32) map->virt) + addr);
	memcpy(dst, buf, len);

	opcode = g_chipen | SFLASH_FORCETERM;
	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	wait_flash_status(SPANSION_STATUS_WIP, 0, SPANSION_PROGRAM_OPCODE);

	return 0;
}

#endif

static int cs752x_s25fl128p_sector_erase(u32 address)
{
	if (cs752x_spansion_cmd(SPANSION_WRITE_ENABLE_OPCODE, 0, 0)) {
		return MTD_ERASE_FAILED;
	}

	if (wait_flash_status
	    (SPANSION_STATUS_WEL, SPANSION_STATUS_WEL, SPANSION_WEL_TIMEOUT)) {
		return MTD_ERASE_FAILED;
	}

	if (cs752x_spansion_cmd(SPANSION_ERASE_OPCODE, address, 0)) {
		return MTD_ERASE_FAILED;
	}

	if (wait_flash_status(SPANSION_STATUS_WIP, 0, SPANSION_ERASE_TIMEOUT)) {
		return MTD_ERASE_FAILED;
	}

	return MTD_ERASE_DONE;
}

int cs752x_spansion_islocked(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	return !cs752x_s25fl128p_writeable(ofs, len);
}

/* We implement lock/unlcok entire flash function only */
int cs752x_spansion_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	u32 flash_status;

	if (cs752x_spansion_islocked(mtd, 0, S25FL128P_PAGE_SIZE) == 0) {
		return 0;
	}

	if (cs752x_spansion_cmd(SPANSION_READ_STATUS_OPCODE, 0, 0)) {
		return -ETIME;
	}
	flash_status = read_flash_ctrl_reg(FLASH_SF_DATA);

	if (flash_status & SPANSION_STATUS_SRWD) {
		return -EIO;
	}

	flash_status &= ~SPANSION_STATUS_BP;

	if (cs752x_spansion_cmd(SPANSION_WRITE_ENABLE_OPCODE, 0, 0)) {
		return -ETIME;
	}

	if (wait_flash_status
	    (SPANSION_STATUS_WEL, SPANSION_STATUS_WEL, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	if (cs752x_spansion_cmd(SPANSION_WRITE_STATUS_OPCODE, 0, flash_status)) {
		return -ETIME;
	}

	if (wait_flash_status(SPANSION_STATUS_BP, 0, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	return 0;

}

static int cs752x_spansion_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	uint64_t addr;
	uint64_t len;
	unsigned int block;
	unsigned int ret = 0;
	uint32_t rem;

#ifdef _MULTI_FLASH_
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, 0);
#endif


	/* Sanity checks */
	if (instr->addr + instr->len > mtd->size) {

#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif
		return -EINVAL;
	}

	div_u64_rem(instr->addr, mtd->erasesize, &rem);
	if (rem) {
#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif
		return -EINVAL;
	}

	div_u64_rem(instr->len, mtd->erasesize, &rem);
	if (rem) {
#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif
		return -EINVAL;
	}

	if (cs752x_s25fl128p_writeable(instr->addr, mtd->erasesize) == 0) {
#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif
		return -EACCES;
	}

	addr = instr->addr;
	len = instr->len;


	while (len > 0) {
		block = addr / mtd->erasesize;
		ret = cs752x_s25fl128p_sector_erase(addr);
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

#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;

	if (!ret)
		mtd_erase_callback(instr);

	return (ret);
}

/* We implement lock/unlcok entire flash function only */
int cs752x_spansion_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	u32 flash_status;
	if (cs752x_spansion_islocked(mtd, 0, S25FL128P_PAGE_SIZE) == 1) {
		return 0;
	}

	if (cs752x_spansion_cmd(SPANSION_READ_STATUS_OPCODE, 0, 0)) {
		return -ETIME;
	}

	flash_status = read_flash_ctrl_reg(FLASH_SF_DATA);
	flash_status |= SPANSION_STATUS_BP;

	if (cs752x_spansion_cmd(SPANSION_WRITE_ENABLE_OPCODE, 0, 0)) {
		return -ETIME;
	}

	if (wait_flash_status
	    (SPANSION_STATUS_WEL, SPANSION_STATUS_WEL, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	if (cs752x_spansion_cmd(SPANSION_WRITE_STATUS_OPCODE, 0, flash_status)) {
		return -ETIME;
	}

	if (wait_flash_status
	    (SPANSION_STATUS_BP, SPANSION_STATUS_BP, SPANSION_WEL_TIMEOUT)) {
		return -ETIME;
	}

	return 0;
}

static void cs752x_spansion_nop(struct mtd_info *mtd)
{
	/* Nothing to see here */
}

static int cs752x_spansion_write(struct mtd_info *mtd, loff_t to, size_t len,
				 size_t * retlen, const u_char * buf)
{
	u32 size, i, remain_length;
	struct map_info *map = (struct map_info *)mtd->priv;

#ifdef _MULTI_FLASH_
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, 0);
#endif
	/* sanity checks */
	if (len == 0 || (to + len > mtd->size)) {
		*retlen = 0;
#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif
		return 0;
	}

	if (cs752x_s25fl128p_writeable(to, len) == 0) {
#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif
		return -EACCES;
	}


	for (remain_length = len; remain_length > 0;) {
		size = S25FL128P_PAGE_SIZE - (to % S25FL128P_PAGE_SIZE);

		if (size > remain_length)
			size = remain_length;

#if defined( _SPANSION_INDIRECT_ACCESS_ )
		for (i = 0; i < size; i++)
			cs752x_s25fl128p_page_program(to + i, buf + i);
#else
		cs752x_s25fl128p_page_dprogram(map, to, buf, size);
#endif
		remain_length -= size;
		buf += size;
		to += size;
	};
#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif

	*retlen = len;

	return 0;
}

static int cs752x_spansion_read(struct mtd_info *mtd, loff_t from, size_t len,
				size_t * retlen, u_char * buf)
{

#if defined( _SPANSION_INDIRECT_ACCESS_ )

	for (*retlen = 0; len > *retlen; ++buf, ++from, *retlen += 1) {
		if (cs752x_spansion_cmd(SPANSION_READ_OPCODE, from,
					SPANSION_ERASE_TIMEOUT)) {
			return 0;
		}

		*buf = read_flash_ctrl_reg(FLASH_SF_DATA) & 0xFF;
	};

#else
	u32 opcode;
	u8 *src;
	struct map_info *map = (struct map_info *)mtd->priv;

	opcode = g_chipen;

#ifdef _MULTI_FLASH_
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, 0);
#endif


	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	src = (u8 *) (((u32) map->virt) + (u32) from);

	memcpy(buf, src, len);

	*retlen = len;

#ifdef _MULTI_FLASH_
	up(&cs752x_flash_sem);
#endif

#endif
	return 0;
}

static struct mtd_chip_driver cs752x_spansion_chipdrv = {
 probe:cs752x_map_serial_probe,
 name:	"cs752x_spansion_map_serial",
 module:THIS_MODULE
};

struct mtd_info *cs752x_map_serial_probe(struct map_info *map)
{
	struct mtd_info *mtd;

	mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	if (!mtd) {
		printk("spansion flash allocation failed\n");
		return NULL;
	}

	memset(mtd, 0, sizeof(*mtd));

	map->fldrv = &cs752x_spansion_chipdrv;
	mtd->priv = map;
	mtd->name = map->name;
	mtd->type = MTD_NORFLASH;
	mtd->erase = cs752x_spansion_erase;
	mtd->size = map->size;
	mtd->read = cs752x_spansion_read;
	mtd->write = cs752x_spansion_write;
	mtd->sync = cs752x_spansion_nop;
	mtd->flags = MTD_CAP_NORFLASH;

	mtd->lock = cs752x_spansion_lock;
	mtd->unlock = cs752x_spansion_unlock;
	mtd->is_locked = cs752x_spansion_islocked;

	mtd->erasesize = S25FL128P_SECTOR_SIZE;
	mtd->writesize = S25FL128P_PAGE_SIZE;

	__module_get(THIS_MODULE);

	return mtd;
}

int init_cs752x_s25fl128p(void)
{
#ifdef CONFIG_CORTINA_FPGA
	write_flash_ctrl_reg(FLASH_SF_TIMING, 0x3000000);
#endif

	/* set flash type to serial flash */
	write_flash_ctrl_reg(FLASH_TYPE, 0);

	s25fl128p_map.virt = ioremap(GOLDENGATE_FLASH_BASE, S25FL128P_SIZE);
	if (!s25fl128p_map.virt) {
		printk(" failed to ioremap \n");
		return -EIO;
	}

	simple_map_init(&s25fl128p_map);

	cs752x_spansion_mtd =
	    do_map_probe("cs752x_spansion_map_serial", &s25fl128p_map);
	if (cs752x_spansion_mtd == NULL) {
		iounmap(s25fl128p_map.virt);
		printk("spansion flash probe failed\n");
		return -EIO;
	}

	cs752x_spansion_mtd->owner = THIS_MODULE;

	mtd_device_register(cs752x_spansion_mtd, cs752x_s25fl128p_partitions,
			ARRAY_SIZE(cs752x_s25fl128p_partitions));

	return 0;
}

void cleanup_cs752x_s25fl128p(void)
{
	if (cs752x_spansion_mtd) {
		mtd_device_unregister(cs752x_spansion_mtd);
		map_destroy(cs752x_spansion_mtd);
	}
}

int __init cs752x_spansion_map_serial_init(void)
{
#if defined( _SPANSION_INDIRECT_ACCESS_ )
	printk("spansion flash: indirect access mode\n");
#else
	printk("spansion flash: direct access mode\n");
#endif
	register_mtd_chip_driver(&cs752x_spansion_chipdrv);
	return init_cs752x_s25fl128p();
}

static void __exit cs752x_spansion_map_serial_exit(void)
{
	cleanup_cs752x_s25fl128p();
	unregister_mtd_chip_driver(&cs752x_spansion_chipdrv);
}

module_init(cs752x_spansion_map_serial_init);
module_exit(cs752x_spansion_map_serial_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Middle Huang <middle.huang@cortina-systems.com>");
MODULE_DESCRIPTION("MTD map driver for Cortina cs752x boards");
