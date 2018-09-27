
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
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/math64.h>
#include <linux/mod_devicetable.h>

#include <linux/init.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mtd/cfi.h>

#define  g_chipen			SERIAL_FLASH_CHIP0_EN
#define SFLASH_4BYTE_ADDR		0x400



#define SPANSION_WRITE_STATUS_OPCODE	( 0x01 | SF_AC_OPCODE_1_DATA  )
#define SPANSION_PROGRAM_OPCODE         ( 0x02 | SF_AC_OPCODE_4_ADDR_1_DATA )
#define SPANSION_READ_OPCODE            ( 0x03 | SF_AC_OPCODE_4_ADDR_1_DATA )
#define SPANSION_READ_STATUS_OPCODE	( 0x05 | SF_AC_OPCODE_1_DATA  )
#define SPANSION_WRITE_ENABLE_OPCODE	( 0x06 | SF_AC_OPCODE )
#define SPANSION_ERASE_OPCODE           ( 0xD8 | SF_AC_OPCODE_4_ADDR )


#define SFLASH_FASTREAD_OPCODE          ( 0x0B | SF_AC_OPCODE_4_ADDR_1_DATA )
#define SFLASH_BE_4K_OPCODE           	( 0x20 | SF_AC_OPCODE_4_ADDR )
#define SFLASH_BE_32K_OPCODE           	( 0x52 | SF_AC_OPCODE_4_ADDR )
#define SFLASH_CHIP_ERASE_OPCODE      	( 0xc7 | SF_AC_OPCODE_4_ADDR )
#define SFLASH_READID_OPCODE           	( 0x9F | SF_AC_OPCODE_4_DATA )

/* Used for SST flashes only. */
//#define OPCODE_BP               0x02    /* Byte program */
//#define OPCODE_WRDI             0x04    /* Write disable */
//#define OPCODE_AAI_WP           0xad    /* Auto address increment word program */

/* Used for Macronix flashes only. */
#define SFLASH_OPCODE_EN4B             ( 0xb7 | SF_AC_OPCODE  )    /* Enter 4-byte mode */
#define SFLASH_OPCODE_EX4B             ( 0xe9 | SF_AC_OPCODE  )    /* Exit 4-byte mode */

/* Used for Spansion flashes only. */
#define SFLASH_OPCODE_BRWR             ( 0x17 | SF_AC_OPCODE_1_DATA  )   /* Bank register write */

#define SPANSION_STATUS_WEL             0x02
#define SPANSION_STATUS_WIP             0x01
#define SPANSION_STATUS_BP		0x1C
#define SPANSION_STATUS_SRWD            0x80

#define SPANSION_WEL_TIMEOUT             400
#define SPANSION_PROGRAM_TIMEOUT         300
#define SPANSION_ERASE_TIMEOUT          4000
#define SPANSION_CMD_TIMEOUT		2000

/* Define to check block protection */
//#define SPANSION_BLOCK_PROTECTION		1

//#define S25FL128P_SECTOR_SIZE         SZ_64K
//#define S25FL128P_PAGE_SIZE           SZ_256
//#define S25FL128P_SIZE		      SZ_16M

static u32 sflash_sector_size = 0x00040000;	//0x00010000
static u32 sflash_page_size = 0x00000100;
static u32 sflash_size = 0x01000000;
static u32 sflash_type = 0x0;
struct mutex	sflash_lock;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
#include <linux/semaphore.h>
extern struct semaphore cs752x_flash_sem;
#endif

struct mtd_info *cs752x_map_serial_probe(struct map_info *map);
static struct mtd_info *cs752x_spansion_mtd;

static int                   nr_mtd_parts;
static struct mtd_partition *mtd_parts;
static const char *probes[] = { "cmdlinepart", NULL };

/* Default flash setting */
static struct map_info s25fl128p_map = {
	.name = "cs752x_serial_flash",
	.size = SZ_16M,
	.bankwidth = 4, 	/* buswidth: 4, */
	.phys =	GOLDENGATE_FLASH_BASE,
};


static struct mtd_partition cs752x_s25fl128p_partitions[] = {
	{ .name = "loader",	.offset = 0x0,		.size = SZ_4M,},
	{ .name = "kernel",	.offset = SZ_4M,	.size = SZ_4M,},
	{ .name = "user",	.offset = SZ_8M,	.size = SZ_8M,},
};

/*
 * SPI device driver setup and teardown
 */
 /*
struct spi_device_id {
        char name[32];
        unsigned long driver_data
                        __attribute__((aligned(sizeof(unsigned long))));
};
*/

#define JEDEC_MFR(_jedec_id)    ((_jedec_id) >> 16)

struct flash_info {
        /* JEDEC id zero means "no ID" (most older chips); otherwise it has
         * a high byte of zero plus three data bytes: the manufacturer id,
         * then a two byte device id.
         */
        u32             jedec_id;
        u16             ext_id;

        /* The size listed here is what works with OPCODE_SE, which isn't
         * necessarily called a "sector" by the vendor.
         */
        unsigned        sector_size;
        u16             n_sectors;

        u16             page_size;
        u16             addr_width;

        u16             flags;
#define SECT_4K         0x01            /* OPCODE_BE_4K works uniformly */
#define M25P_NO_ERASE   0x02            /* No erase command needed */
};

#define INFO(_jedec_id, _ext_id, _sector_size, _n_sectors, _flags)      \
        ((unsigned long)&(struct flash_info) {                         \
                .jedec_id = (_jedec_id),                                \
                .ext_id = (_ext_id),                                    \
                .sector_size = (_sector_size),                          \
                .n_sectors = (_n_sectors),                              \
                .page_size = 256,                                       \
                .flags = (_flags),                                      \
        })

/* NOTE: double check command sets and memory organization when you add
 * more flash chips.  This current list focusses on newer chips, which
 * have been converging on command sets which including JEDEC ID.
 */
static const struct spi_device_id m25p_ids[] = {
        /* Atmel -- some are (confusingly) marketed as "DataFlash" */
        { "at25fs010",  INFO(0x1f6601, 0, 32 * 1024,   4, SECT_4K) },
        { "at25fs040",  INFO(0x1f6604, 0, 64 * 1024,   8, SECT_4K) },

        { "at25df041a", INFO(0x1f4401, 0, 64 * 1024,   8, SECT_4K) },
        { "at25df321a", INFO(0x1f4701, 0, 64 * 1024,  64, SECT_4K) },
        { "at25df641",  INFO(0x1f4800, 0, 64 * 1024, 128, SECT_4K) },

        { "at26f004",   INFO(0x1f0400, 0, 64 * 1024,  8, SECT_4K) },
        { "at26df081a", INFO(0x1f4501, 0, 64 * 1024, 16, SECT_4K) },
        { "at26df161a", INFO(0x1f4601, 0, 64 * 1024, 32, SECT_4K) },
        { "at26df321",  INFO(0x1f4700, 0, 64 * 1024, 64, SECT_4K) },

        /* EON -- en25xxx */
        { "en25f32", INFO(0x1c3116, 0, 64 * 1024,  64, SECT_4K) },
        { "en25p32", INFO(0x1c2016, 0, 64 * 1024,  64, 0) },
        { "en25q32b", INFO(0x1c3016, 0, 64 * 1024,  64, 0) },
        { "en25p64", INFO(0x1c2017, 0, 64 * 1024, 128, 0) },

        /* Intel/Numonyx -- xxxs33b */
        { "160s33b",  INFO(0x898911, 0, 64 * 1024,  32, 0) },
        { "320s33b",  INFO(0x898912, 0, 64 * 1024,  64, 0) },
        { "640s33b",  INFO(0x898913, 0, 64 * 1024, 128, 0) },

        /* Macronix */
        { "mx25l4005a",  INFO(0xc22013, 0, 64 * 1024,   8, SECT_4K) },
        { "mx25l8005",   INFO(0xc22014, 0, 64 * 1024,  16, 0) },
        { "mx25l1606e",  INFO(0xc22015, 0, 64 * 1024,  32, SECT_4K) },
        { "mx25l3205d",  INFO(0xc22016, 0, 64 * 1024,  64, 0) },
        { "mx25l6405d",  INFO(0xc22017, 0, 64 * 1024, 128, 0) },
        { "mx25l12845e", INFO(0xc2c218, 0, 64 * 1024, 512, 0) },
        /* { "mx25l12805d", INFO(0xc22018, 0, 64 * 1024, 256, 0) }, */
        { "mx25l12845e", INFO(0xc22018, 0, 64 * 1024, 256, 0) },
        { "mx25l12855e", INFO(0xc22618, 0, 64 * 1024, 256, 0) },
         /* 257 4 bytes address */
        { "mx25l25735e", INFO(0xc22019, 0, 64 * 1024, 512, 0) },
        /* 256 3 bytes address */
        { "mx25l25635e", INFO(0xc22019, 0, 64 * 1024, 512, 0) },
        { "mx25l25655e", INFO(0xc22619, 0, 64 * 1024, 512, 0) },
        { "mx66l51235f", INFO(0xc2201a, 0, 64 * 1024, 1024, 0) },

        /* Spansion -- single (large) sector size only, at least
         * for the chips listed here (without boot sectors).
         */
        { "s25sl004a",  INFO(0x010212,      0,  64 * 1024,   8, 0) },
        { "s25sl008a",  INFO(0x010213,      0,  64 * 1024,  16, 0) },
        { "s25sl016a",  INFO(0x010214,      0,  64 * 1024,  32, 0) },
        { "s25sl032a",  INFO(0x010215,      0,  64 * 1024,  64, 0) },
        { "s25sl032p",  INFO(0x010215, 0x4d00,  64 * 1024,  64, SECT_4K) },
        { "s25sl064a",  INFO(0x010216,      0,  64 * 1024, 128, 0) },
        { "s25fl256s0", INFO(0x010219, 0x4d00, 256 * 1024, 128, 0) },
        { "s25fl256s1", INFO(0x010219, 0x4d01,  64 * 1024, 512, 0) },
        { "s25fl512s",  INFO(0x010220, 0x4d00, 256 * 1024, 256, 0) },
        { "s70fl01gs",  INFO(0x010221, 0x4d00, 256 * 1024, 256, 0) },
        { "s25sl12800", INFO(0x012018, 0x0300, 256 * 1024,  64, 0) },
        /* { "s25sl12801", INFO(0x012018, 0x0301,  64 * 1024, 256, 0) }, */
        { "s25fl129p0", INFO(0x012018, 0x4d00, 256 * 1024,  64, 0) },
        { "s25fl129p1", INFO(0x012018, 0x4d01,  64 * 1024, 256, 0) },
        { "s25fl016k",  INFO(0xef4015,      0,  64 * 1024,  32, SECT_4K) },
        { "s25fl064k",  INFO(0xef4017,      0,  64 * 1024, 128, SECT_4K) },

        /* SST -- large erase sizes are "overlays", "sectors" are 4K */
        { "sst25vf040b", INFO(0xbf258d, 0, 64 * 1024,  8, SECT_4K) },
        { "sst25vf080b", INFO(0xbf258e, 0, 64 * 1024, 16, SECT_4K) },
        { "sst25vf016b", INFO(0xbf2541, 0, 64 * 1024, 32, SECT_4K) },
        { "sst25vf032b", INFO(0xbf254a, 0, 64 * 1024, 64, SECT_4K) },
        { "sst25wf512",  INFO(0xbf2501, 0, 64 * 1024,  1, SECT_4K) },
        { "sst25wf010",  INFO(0xbf2502, 0, 64 * 1024,  2, SECT_4K) },
        { "sst25wf020",  INFO(0xbf2503, 0, 64 * 1024,  4, SECT_4K) },
        { "sst25wf040",  INFO(0xbf2504, 0, 64 * 1024,  8, SECT_4K) },

        /* ST Microelectronics -- newer production may have feature updates */
        { "m25p05",  INFO(0x202010,  0,  32 * 1024,   2, 0) },
        { "m25p10",  INFO(0x202011,  0,  32 * 1024,   4, 0) },
        { "m25p20",  INFO(0x202012,  0,  64 * 1024,   4, 0) },
        { "m25p40",  INFO(0x202013,  0,  64 * 1024,   8, 0) },
        { "m25p80",  INFO(0x202014,  0,  64 * 1024,  16, 0) },
        { "m25p16",  INFO(0x202015,  0,  64 * 1024,  32, 0) },
        { "m25p32",  INFO(0x202016,  0,  64 * 1024,  64, 0) },
        { "m25p64",  INFO(0x202017,  0,  64 * 1024, 128, 0) },
        { "m25p128", INFO(0x202018,  0, 256 * 1024,  64, 0) },

        { "m25p05-nonjedec",  INFO(0, 0,  32 * 1024,   2, 0) },
        { "m25p10-nonjedec",  INFO(0, 0,  32 * 1024,   4, 0) },
        { "m25p20-nonjedec",  INFO(0, 0,  64 * 1024,   4, 0) },
        { "m25p40-nonjedec",  INFO(0, 0,  64 * 1024,   8, 0) },
        { "m25p80-nonjedec",  INFO(0, 0,  64 * 1024,  16, 0) },
        { "m25p16-nonjedec",  INFO(0, 0,  64 * 1024,  32, 0) },
        { "m25p32-nonjedec",  INFO(0, 0,  64 * 1024,  64, 0) },
        { "m25p64-nonjedec",  INFO(0, 0,  64 * 1024, 128, 0) },
        { "m25p128-nonjedec", INFO(0, 0, 256 * 1024,  64, 0) },

        { "m45pe10", INFO(0x204011,  0, 64 * 1024,    2, 0) },
        { "m45pe80", INFO(0x204014,  0, 64 * 1024,   16, 0) },
        { "m45pe16", INFO(0x204015,  0, 64 * 1024,   32, 0) },

        { "m25pe80", INFO(0x208014,  0, 64 * 1024, 16,       0) },
        { "m25pe16", INFO(0x208015,  0, 64 * 1024, 32, SECT_4K) },

        { "m25px32",    INFO(0x207116,  0, 64 * 1024, 64, SECT_4K) },
        { "m25px32-s0", INFO(0x207316,  0, 64 * 1024, 64, SECT_4K) },
        { "m25px32-s1", INFO(0x206316,  0, 64 * 1024, 64, SECT_4K) },
        { "m25px64",    INFO(0x207117,  0, 64 * 1024, 128, 0) },

        /* Winbond -- w25x "blocks" are 64K, "sectors" are 4KiB */
        { "w25x10", INFO(0xef3011, 0, 64 * 1024,  2,  SECT_4K) },
        { "w25x20", INFO(0xef3012, 0, 64 * 1024,  4,  SECT_4K) },
        { "w25x40", INFO(0xef3013, 0, 64 * 1024,  8,  SECT_4K) },
        { "w25x80", INFO(0xef3014, 0, 64 * 1024,  16, SECT_4K) },
        { "w25x16", INFO(0xef3015, 0, 64 * 1024,  32, SECT_4K) },
        { "w25x32", INFO(0xef3016, 0, 64 * 1024,  64, SECT_4K) },
        { "w25q32", INFO(0xef4016, 0, 64 * 1024,  64, SECT_4K) },
        { "w25x64", INFO(0xef3017, 0, 64 * 1024, 128, SECT_4K) },
        { "w25q64", INFO(0xef4017, 0, 64 * 1024, 128, SECT_4K) },
        { "w25q128", INFO(0xef4018, 0, 64 * 1024, 256, SECT_4K) },

        /* Catalyst / On Semiconductor -- non-JEDEC */
        { },
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
struct spi_device_id *cs752x_jedec_probe(void)
{
        u32                     jedec, tmp;
        u16                     ext_jedec;
        struct flash_info       *info;
	unsigned long status;

	if (cs752x_spansion_cmd(SFLASH_READID_OPCODE, 0, 0)) {
		return NULL;
	}

	status = read_flash_ctrl_reg(FLASH_SF_DATA);

	jedec = ((status&0xff)<<16) | (status&0xff00) | ((status&0xff0000)>>16);

	//For G2 just can read 4 bytesdata only, So without ext_jedec
        //ext_jedec = id[3] << 8 | id[4];

        for (tmp = 0; tmp < ARRAY_SIZE(m25p_ids) - 1; tmp++) {

                info = (void *)m25p_ids[tmp].driver_data;

                if (info->jedec_id == jedec) {
                        //if (info->ext_id != 0 && info->ext_id != ext_jedec)
                        //        continue;
                        return &m25p_ids[tmp];
                }
        }

        printk("%s : unrecognized JEDEC id %06x\n",__func__ , jedec);

        return NULL;
}


static int wait_flash_status(u32 flag, u32 value, u32 wait_msec)
{
	u32 status;
	unsigned long timeout_jiffies;

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

/*
 * Enable/disable 4-byte addressing mode.
 */
static inline int cs752x_set_4byte(u32 jedec_id, int enable)
{

	u32 tmp;

        switch (JEDEC_MFR(jedec_id)) {
        case CFI_MFR_MACRONIX:
               // flash->command[0] = enable ? OPCODE_EN4B : OPCODE_EX4B;
               // return spi_write(flash->spi, flash->command, 1);

               	if (cs752x_spansion_cmd(enable ? SFLASH_OPCODE_EN4B : SFLASH_OPCODE_EX4B, 0, 0)) {
			printk("%s : MACRONIX set 4 bytes error.\n",__func__);
		}
		break;
	case CFI_MFR_ST:
		/* Before issue EN4B or EX4B the WRITE ENABLE command must issued first. */
		if (cs752x_spansion_cmd(SPANSION_WRITE_ENABLE_OPCODE, 0, 0)) {
			return -ETIME;
		}

		if (wait_flash_status
		    (SPANSION_STATUS_WEL, SPANSION_STATUS_WEL, SPANSION_WEL_TIMEOUT)) {
			return -ETIME;
		}

		if (cs752x_spansion_cmd(enable ? SFLASH_OPCODE_EN4B : SFLASH_OPCODE_EX4B, 0, 0)) {
			printk("%s : Micron serial flash set 4 bytes error.\n",__func__);
		}

		break;
        default:
                /* Spansion style */
                //flash->command[0] = OPCODE_BRWR;
                //flash->command[1] = enable << 7;
                //return spi_write(flash->spi, flash->command, 2);
                if (cs752x_spansion_cmd(SFLASH_OPCODE_BRWR, 0, enable << 7)) {
			printk("%s : SFlash set 4 bytes error.\n",__func__);
		}
        }

        tmp = read_flash_ctrl_reg(FLASH_TYPE);
        if(enable)
        	sflash_type = (tmp | SFLASH_4BYTE_ADDR);
	else
		sflash_type = (tmp & ~SFLASH_4BYTE_ADDR);

		write_flash_ctrl_reg(FLASH_TYPE, sflash_type);


	return 0;
}

#ifdef	SPANSION_BLOCK_PROTECTION

static int protect_lower_bound(u32 status)
{
	u32 protect_lower;

	//for spansion uni-256KB sector size if uni-64kB or MX need to check 4bits(BP0 - BP3)
	//And base on TBPROT bit to decide BP starts at bottom or top
	protect_lower = (status & SPANSION_STATUS_BP) >> 2;
	if (protect_lower == 0) {
		return sflash_size;
	}
	else if (protect_lower == 0) {
		return 0;
	}
	else
		return (sflash_size / (1<<(7-protect_lower)) );

/**
	if (protect_lower == 0) {
		return 0xFFFFFF;
	}

	protect_lower = 0xFFFFFF * (0x01 << (protect_lower - 1)) / 64;
	protect_lower = 0xFFFFFF - protect_lower;
**/
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
#else
/* return 1 when it is writeable */
static int cs752x_s25fl128p_writeable(u32 addr, u32 len)
{
		return 1;
}
#endif

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


#ifdef	SPANSION_BLOCK_PROTECTION
int cs752x_spansion_islocked(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	return !cs752x_s25fl128p_writeable(ofs, len);
}

/* We implement lock/unlcok entire flash function only */
int cs752x_spansion_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	u32 flash_status;

	if (cs752x_spansion_islocked(mtd, 0, sflash_page_size) == 0) {
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
#endif

static int cs752x_spansion_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	uint64_t addr;
	uint64_t len;
	unsigned int block;
	unsigned int ret = 0;
	uint32_t rem;

	mutex_lock(&sflash_lock);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, sflash_type);
#endif

	/* Sanity checks */
	if (instr->addr + instr->len > mtd->size) {

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
		mutex_unlock(&sflash_lock);
		return -EINVAL;
	}

	div_u64_rem(instr->addr, mtd->erasesize, &rem);
	if (rem) {
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
		mutex_unlock(&sflash_lock);
		return -EINVAL;
	}

	div_u64_rem(instr->len, mtd->erasesize, &rem);
	if (rem) {
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
		mutex_unlock(&sflash_lock);
		return -EINVAL;
	}



	if (cs752x_s25fl128p_writeable(instr->addr, mtd->erasesize) == 0) {
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
		mutex_unlock(&sflash_lock);
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

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
	mutex_unlock(&sflash_lock);
	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;

	if (!ret)
		mtd_erase_callback(instr);

	return (ret);
}


#ifdef	SPANSION_BLOCK_PROTECTION
/* We implement lock/unlcok entire flash function only */
int cs752x_spansion_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	u32 flash_status;
	if (cs752x_spansion_islocked(mtd, 0, sflash_page_size) == 1) {
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
#endif

static void cs752x_spansion_nop(struct mtd_info *mtd)
{
	/* Nothing to see here */
}

static int cs752x_spansion_write(struct mtd_info *mtd, loff_t to, size_t len,
				 size_t * retlen, const u_char * buf)
{
	u32 size, i, remain_length;
	struct map_info *map = (struct map_info *)mtd->priv;

	mutex_lock(&sflash_lock);

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, sflash_type);
#endif
	/* sanity checks */
	if (len == 0 || (to + len > mtd->size)) {
		*retlen = 0;
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
		mutex_unlock(&sflash_lock);
		return 0;
	}

	if (cs752x_s25fl128p_writeable(to, len) == 0) {
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif
		mutex_unlock(&sflash_lock);
		return -EACCES;
	}


	for (remain_length = len; remain_length > 0;) {
		size = sflash_page_size - ((u32)to % sflash_page_size);

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
#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

	*retlen = len;
	mutex_unlock(&sflash_lock);
	return 0;
}

static int cs752x_spansion_read(struct mtd_info *mtd, loff_t from, size_t len,
				size_t * retlen, u_char * buf)
{
	mutex_lock(&sflash_lock);

#if defined( _SPANSION_INDIRECT_ACCESS_ )

	for (*retlen = 0; len > *retlen; ++buf, ++from, *retlen += 1) {
		if (cs752x_spansion_cmd(SPANSION_READ_OPCODE, from,
					SPANSION_ERASE_TIMEOUT)) {
			mutex_unlock(&sflash_lock);
			return 0;
		}

		*buf = read_flash_ctrl_reg(FLASH_SF_DATA) & 0xFF;
	};

#else
	u32 opcode;
	u8 *src;
	struct map_info *map = (struct map_info *)mtd->priv;

	opcode = g_chipen;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	down(&cs752x_flash_sem);
	write_flash_ctrl_reg(FLASH_TYPE, sflash_type);
#endif


	write_flash_ctrl_reg(FLASH_SF_ACCESS, opcode);

	src = (u8 *) (((u32) map->virt) + (u32) from);

	memcpy(buf, src, len);

	*retlen = len;

#ifdef CONFIG_MTD_CS752X_MULTIFLASH
	up(&cs752x_flash_sem);
#endif

#endif
	mutex_unlock(&sflash_lock);
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
		printk("Serial flash allocation failed\n");
		return NULL;
	}

	mutex_init(&sflash_lock);

	memset(mtd, 0, sizeof(*mtd));

	map->fldrv = &cs752x_spansion_chipdrv;
	mtd->priv = map;
	mtd->name = map->name;
	mtd->type = MTD_NORFLASH;
	mtd->_erase = cs752x_spansion_erase;
	mtd->size = map->size;
	mtd->_read = cs752x_spansion_read;
	mtd->_write = cs752x_spansion_write;
	mtd->_sync = cs752x_spansion_nop;
	mtd->flags = MTD_CAP_NORFLASH;

#ifdef	SPANSION_BLOCK_PROTECTION
	mtd->_lock = cs752x_spansion_lock;
	mtd->_unlock = cs752x_spansion_unlock;
	mtd->_is_locked = cs752x_spansion_islocked;
#endif
	mtd->erasesize = sflash_sector_size;
	mtd->writesize = sflash_page_size;

	__module_get(THIS_MODULE);

	return mtd;
}

int init_cs752x_s25fl128p(void)
{
	const struct spi_device_id *jid;
	struct flash_info               *info;
	u32 tmp;
	char *ptr;



#ifdef CONFIG_CORTINA_FPGA
	write_flash_ctrl_reg(FLASH_SF_TIMING, 0x3000000);
#endif

	/* parsing serial flash type */
	ptr = strstr(saved_command_line, "sf_type");
	if (ptr) {
		ptr += strlen("sf_type") + 1;
		sflash_type = simple_strtoul(ptr, NULL, 0);
	}
	else
	{
		sflash_type = read_flash_ctrl_reg(FLASH_TYPE)& 0xffff;
		printk("Serial flash type : 0x%04x \n",sflash_type);
	}


	/* set flash type to serial flash */
	write_flash_ctrl_reg(FLASH_TYPE, sflash_type);

	jid = cs752x_jedec_probe();

	info = (void *)jid->driver_data;

	//flash = kmalloc(sizeof(struct spi_flash), GFP_KERNEL);
        //
	//if (!flash) {
	//	printk("%s : Failed to allocate memory\n",__func__);
	//	return -1;
	//}

	/*
         * Atmel, SST and Intel/Numonyx serial flash tend to power
         * up with the software protection bits set
         */
         printk("found %s.\n",jid->name);

        if (JEDEC_MFR(info->jedec_id) == CFI_MFR_ATMEL ||
            JEDEC_MFR(info->jedec_id) == CFI_MFR_INTEL ||
            JEDEC_MFR(info->jedec_id) == CFI_MFR_SST) {
              //  write_enable(flash);
              //  write_sr(flash, 0);
              printk("Atmel, Intel or SST SFlash.\n");
        }

	/* For S25-FL512SAIFG1 page size is 512B, 64MB with RESET# pin*/
	if(info->jedec_id == 0x010220)
		info->page_size = 512;

	sflash_sector_size = info->sector_size;
	sflash_page_size = info->page_size;
	sflash_size = info->sector_size * info->n_sectors;
	/* prefer "small sector" erase if possible */
        if (info->flags & SECT_4K) {
        	printk("Check OPCODE_BE_4K.\n");
                //flash->erase_opcode = OPCODE_BE_4K;
                //flash->mtd.erasesize = 4096;
        }

	/* enable 4-byte addressing if the device exceeds 16MiB */
        if (sflash_size > 0x1000000) {
                printk("4 Bytes address flash.\n");
		tmp = read_flash_ctrl_reg(FLASH_TYPE);
		sflash_type = (tmp | SFLASH_4BYTE_ADDR);
		write_flash_ctrl_reg(FLASH_TYPE, sflash_type);
                cs752x_set_4byte(info->jedec_id, 1);
        }
        //else
        //        printk("%s : 3 Bytes mode flash.\n",__func__);


	//s25fl128p_map.name = jid->name;
	s25fl128p_map.size = sflash_size;
	//flash->mtd.name = jid->name;
	//flash->mtd.size = info->sector_size * info->n_sectors;


	s25fl128p_map.virt = ioremap(GOLDENGATE_FLASH_BASE, sflash_size);
	if (!s25fl128p_map.virt) {
		printk(" failed to ioremap \n");
		return -EIO;
	}

	simple_map_init(&s25fl128p_map);
	cs752x_spansion_mtd =
	    do_map_probe("cs752x_spansion_map_serial", &s25fl128p_map);
	if (cs752x_spansion_mtd == NULL) {
		iounmap(s25fl128p_map.virt);
		printk("Serial flash probe failed\n");
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
	printk("Serial flash: indirect access mode\n");
#else
	printk("Serial flash: direct access mode\n");
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
