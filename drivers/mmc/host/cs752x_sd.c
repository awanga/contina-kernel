/*
 *  linux/drivers/mmc/host/cs752x_sd.c
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/seq_file.h>

#include <mach/platform.h>
#include <mach/hardware.h>
#include <mach/cs752x_sd_regs.h>
#include "cs752x_sd.h"

#include <linux/time.h>

#include <asm/delay.h>

#define REG32(addr)	(*(volatile unsigned long  * const)(addr))
#define SD_DRIVER_NAME "cs752x_sd"

#define cs752x_sd_dbg(cs752x_host, channels, args...)   \
	do {						\
	if (cs752x_dbgmap_err & channels)		\
		dev_err(&cs752x_host->pdev->dev, args);	\
	else if (cs752x_dbgmap_info & channels)		\
		dev_info(&cs752x_host->pdev->dev, args);\
	else if (cs752x_dbgmap_debug & channels)	\
		dev_dbg(&cs752x_host->pdev->dev, args);	\
	} while (0)

#ifdef CONFIG_CS752X_SD_DEBUG
#define FBDBG_LEVEL	1
#else
#undef  FBDBG_LEVEL
#endif
#ifdef  FBDBG_LEVEL

#define MAX_DBG_INDENT_LEVEL    10
#define DBG_INDENT_SIZE         2
#define MAX_DBG_MESSAGES        0

static int dbg_indent;
static int dbg_cnt;
static spinlock_t dbg_spinlock = SPIN_LOCK_UNLOCKED;

static dma_addr_t  dma_phyaddr;

#define dbg_print(level, format, arg...)				\
	if (level <= FBDBG_LEVEL) {                                 	\
		if (!MAX_DBG_MESSAGES || dbg_cnt < MAX_DBG_MESSAGES) {  \
			int ind = dbg_indent;                           \
			unsigned long flags;                            \
			spin_lock_irqsave(&dbg_spinlock, flags);        \
			dbg_cnt++;                                      \
			if (ind > MAX_DBG_INDENT_LEVEL)                 \
				ind = MAX_DBG_INDENT_LEVEL;             \
			printk("%*s", ind * DBG_INDENT_SIZE, "");       \
			printk(format, ## arg);                         \
			spin_unlock_irqrestore(&dbg_spinlock, flags);   \
		}                                                       \
	}

#define DBGPRINT        dbg_print

#define DBGENTER(level) do {					\
	dbg_print(level, "%s: Entering\n", __FUNCTION__);	\
	dbg_indent++;						\
} while (0)

#define DBGLEAVE(level) do {					\
	dbg_indent--;						\
	dbg_print(level, "%s: Leaving\n", __FUNCTION__);	\
} while (0)

#define DBGDUMPBUF(level, bufptr, sectors)			\
	if (level <= FBDBG_LEVEL) {                             \
		const u32 depth=512*sectors;			\
		u32 i,j;					\
		for(j=0; j<depth; j+=16) {			\
			if (0 == j%512) {			\
				printk("\n");			\
			}					\
			for(i=0; i<16; i+=4) {			\
				printk("0x%02x%02x%02x%02x\t", bufptr[3+i+j], bufptr[2+i+j], bufptr[1+i+j], bufptr[0+i+j]);			\
			}					\
			printk("\n");				\
		}						\
	}

#else	/* FBDBG_LEVEL */

#define DBGPRINT(level, format, ...)
#define DBGENTER(level)
#define DBGLEAVE(level)
#define DBGDUMPBUF(level, bufptr, sectors)

#endif	/* FBDBG_LEVEL */

#ifdef CONFIG_CS752X_SD_DEBUG
static void cs752x_dbg_prepare_msg(struct cs752x_sd_host *cs752x_host,
				struct mmc_command *cmd, int stop)
{
	/* copy cmd message to debug_buffer_cmd */
	snprintf(cs752x_host->dbgmsg_cmd, (DBG_BUF_LEN-1),
	"%s op:%i arg:0x%08x flags:0x08%x retries:%u",
	(stop ? " (STOP)" : ""),
	cmd->opcode, cmd->arg, cmd->flags, cmd->retries);

	/* copy dat message to debug_buffer_dat */
	if (cmd->data) {
		snprintf(cs752x_host->dbgmsg_dat, (DBG_BUF_LEN-1),
			"bsize:%u blocks:%u bytes:%u",
			cmd->data->blksz,
			cmd->data->blocks,
			cmd->data->blocks * cmd->data->blksz);
	} else {
		cs752x_host->dbgmsg_dat[0] = '\0';
	}
}

static void cs752x_dbg_dmpdesc(struct cs752x_sd_host *cs752x_host, char *prefix)
{
	SD_DESCRIPT_T  *txrx_desc = cs752x_host->desc_cpu;
	u32 cnt, descnt;

	if (cs752x_host->desc_cpu) {
		descnt = cs752x_host->descnt ? cs752x_host->descnt : G2_NR_SG;
		for (cnt=0; cnt<descnt; cnt++) {
			cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
				"%08x %08x %08x %08x\n",
				txrx_desc->des0.bits32, txrx_desc->des1.bits32,
				txrx_desc->buffer_addr, txrx_desc->next_desc);
			txrx_desc = (SD_DESCRIPT_T *)(cs752x_host->desc_cpu +
				(cnt+1));
		}
	}
}

static void cs752x_dbg_dmpregs(struct cs752x_sd_host *cs752x_host, char *prefix)
{
	u32 ctrl, pwren, clkdiv, clksrc, clkena, tmout, ctype, blksiz,
		bytcnt, intmask, cmdarg, cmd, resp0, resp1, resp2, resp3,
		mintsts, rintsts, status, fifoth, cdetect, wrtprt, gpio,
		tcbcnt, tbbcnt, debnce, usrid, verid, hcon, bmod, pldmnd,
		dbaddr, idsts, idinten, dscaddr, bufaddr;

	ctrl    = readl(cs752x_host->base + CTRL_REG);
	pwren   = readl(cs752x_host->base + PWREN_REG);
	clkdiv  = readl(cs752x_host->base + CLKDIV_REG);
	clksrc  = readl(cs752x_host->base + CLKSRC_REG);
	clkena  = readl(cs752x_host->base + CLKENA_REG);
	tmout   = readl(cs752x_host->base + TMOUT_REG);
	ctype   = readl(cs752x_host->base + CTYPE_REG);
	blksiz  = readl(cs752x_host->base + BLKSIZ_REG);
	bytcnt  = readl(cs752x_host->base + BYTCNT_REG);
	intmask = readl(cs752x_host->base + INTMASK_REG);
	cmdarg  = readl(cs752x_host->base + CMDARG_REG);
	cmd     = readl(cs752x_host->base + CMD_REG);
	resp0   = readl(cs752x_host->base + RESP0_SHORT_REG);
	resp1   = readl(cs752x_host->base + RESP1_REG);
	resp2   = readl(cs752x_host->base + RESP2_REG);
	resp3   = readl(cs752x_host->base + RESP3_REG);
	mintsts = readl(cs752x_host->base + MINTSTS_REG);
	rintsts = readl(cs752x_host->base + RINTSTS_REG);
	status  = readl(cs752x_host->base + STATUS_REG);
	fifoth  = readl(cs752x_host->base + FIFOTH_REG);
	cdetect = readl(cs752x_host->base + CDETECT_REG);
	wrtprt  = readl(cs752x_host->base + WRTPRT_REG);
	gpio    = readl(cs752x_host->base + GPIO_REG);
	tcbcnt  = readl(cs752x_host->base + TCBCNT_REG);
	tbbcnt  = readl(cs752x_host->base + TBBCNT_REG);
	debnce  = readl(cs752x_host->base + DEBNCE_REG);
	usrid   = readl(cs752x_host->base + USRID_REG);
	verid   = readl(cs752x_host->base + VERID_REG);
	hcon    = readl(cs752x_host->base + HCON_REG);
	bmod    = readl(cs752x_host->base + BMOD_REG);
	pldmnd  = readl(cs752x_host->base + PLDMND_REG);
	dbaddr  = readl(cs752x_host->base + DBADDR_REG);
	idsts   = readl(cs752x_host->base + IDSTS_REG);
	idinten = readl(cs752x_host->base + IDINTEN_REG);
	dscaddr = readl(cs752x_host->base + DSCADDR_REG);
	bufaddr = readl(cs752x_host->base + BUFADDR_REG);

	printk("\n");
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s CTRL[%08x]  PWREN[%08x]  STATUS[%08x]\n",
		prefix, ctrl, pwren, status);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s CLKDIV[%08x]  CLKSRC[%08x]  CLKENA[%08x]  TMOUT[%08x]\n",
		prefix, clkdiv, clksrc, clkena, tmout);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s CTYPE[%08x]  BLKSIZ[%08x]  BYTCNT[%08x]\n",
		prefix, ctype, blksiz, bytcnt);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s CMDARG[%08x]  CMD[%08x] CMD%d RESP0[%08x]\n",
		prefix, cmdarg, cmd, cmd & 0x3f, resp0);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s INTMASK[%08x]  MINTSTS[%08x]  RINTSTS[%08x] FIFOTH[%08x]\n",
		prefix, intmask, mintsts, rintsts, fifoth);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s CDETECT[%08x]  WRTPRT[%08x]  GPIO[%08x]  DEBNCE[%08x]\n",
		prefix, cdetect, wrtprt, gpio, debnce);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s TCBCNT[%08x]  TBBCNT[%08x]  USRID[%08x]  VERID[%08x]\n",
		prefix, tcbcnt, tbbcnt, usrid, verid);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s HCON[%08x]  BMOD[%08x]  PLDMND[%08x]\n",
		prefix, hcon, bmod, pldmnd);
	cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug,
		"%s DBADDR[%08x]  IDSTS[%08x]  IDINTEN[%08x]  DSCADDR[%08x]\n",
		prefix, dbaddr, idsts, idinten, dscaddr);
	if (cs752x_host) {
		DBGPRINT(1, "cs752x_host=%p\n", cs752x_host);
		if (cs752x_host->mrq && cs752x_host->cardin) {
			DBGPRINT(1, "cs752x_host->mrq=%p\n", cs752x_host->mrq);
			if ((unsigned int)(cs752x_host->mrq->cmd) & 0x80000000) {
				DBGPRINT(1, "cs752x_host->mrq->cmd=%p\n",
					cs752x_host->mrq->cmd);
				if (cs752x_host->mrq->cmd->error) {
	  				DBGPRINT(1, "cs752x_host->mrq->cmd->error=%p\n", cs752x_host->mrq->cmd->error);
	  				cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug, "%s BUFADDR[%08x] int_status=%08x, g2_base=%p cmderror=0x%08x\n",
	   				prefix, bufaddr, cs752x_host->int_status, cs752x_host->base, cs752x_host->mrq->cmd->error);
	  			} else {
					cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug, "%s BUFADDR[%08x] int_status=%08x, g2_base=%p\n",
	   				prefix, bufaddr, cs752x_host->int_status, cs752x_host->base);
	  			}
			}else {
				cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug, "%s BUFADDR[%08x] int_status=%08x, g2_base=%p\n",
	   				prefix, bufaddr, cs752x_host->int_status, cs752x_host->base);
			}
		} else {
			cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug, "%s BUFADDR[%08x] int_status=%08x, g2_base=%p\n",
	   			prefix, bufaddr, cs752x_host->int_status, cs752x_host->base);
		}
	} else {
		cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug, "%s BUFADDR[%08x] int_status=%08x, g2_base=%p\n",
	   		prefix, bufaddr, cs752x_host->int_status, cs752x_host->base);
	}

	if (cs752x_host->mrq && cs752x_host->cardin) {
		if (cs752x_host->mrq->data) {
			if (cs752x_host->desc_cpu) {
				SD_DESCRIPT_T  *txrx_desc = cs752x_host->desc_cpu;
				unsigned int   cnt;
				for (cnt=0; cnt<(cs752x_host->descnt); cnt++) {
					cs752x_sd_dbg(cs752x_host, cs752x_dbg_debug, "%s (%p) txrx_[%08x:%08x:%08x:%08x]\n",
						prefix, txrx_desc,
						txrx_desc->des0.bits32,
						txrx_desc->des1.bits32,
						txrx_desc->buffer_addr,
						txrx_desc->next_desc);
					txrx_desc = (SD_DESCRIPT_T *)(cs752x_host->desc_cpu + (cnt+1));
				}
			}
		}
	}
}

static void cs752x_dbg_dumpcmd(struct cs752x_sd_host *cs752x_host,
				struct mmc_command *cmd, int fail)
{
	unsigned int dbglevel = fail ? cs752x_dbg_fail : cs752x_dbg_debug;

	if (!cmd)
		return;

	if (cmd->error)
		cs752x_sd_dbg(cs752x_host, dbglevel, "CMD[ERR %i] %s\n", cmd->error, cs752x_host->dbgmsg_cmd);

	if (!cmd->data)
		return;

	if (cmd->data->error)
		cs752x_sd_dbg(cs752x_host, dbglevel, "DAT[ERR %i] %s\n", cmd->data->error, cs752x_host->dbgmsg_dat);
}
#else
static void cs752x_dbg_prepare_msg(struct cs752x_sd_host *cs752x_host,
				struct mmc_command *cmd, int stop){ }
static void cs752x_dbg_dmpdesc(struct cs752x_sd_host *cs752x_host,
				char *prefix){ }
static void cs752x_dbg_dmpregs(struct cs752x_sd_host *cs752x_host,
				char *prefix){ }
static void cs752x_dbg_dumpcmd(struct cs752x_sd_host *cs752x_host,
				struct mmc_command *cmd, int fail){ }
#endif /* CONFIG_CS752X_SD_DEBUG */

#ifdef  CONFIG_DEBUG_FS
static int cs752x_sd_state_show(struct seq_file *seq, void *v)
{
	struct cs752x_sd_host *cs752x_host = seq->private;

	seq_printf(seq, "Register base = 0x%08x\n", (u32)cs752x_host->base);
	seq_printf(seq, "IRQ = %d\n", cs752x_host->irq);
	seq_printf(seq, "Do DMA = %d\n", cs752x_sd_host_usedma(cs752x_host));

	return 0;
}

static int cs752x_sd_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, cs752x_sd_state_show, inode->i_private);
}

static const struct file_operations cs752x_sd_fops_state = {
	.owner          = THIS_MODULE,
	.open           = cs752x_sd_state_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#define DBG_REG(_reg) { .name = #_reg, .addr = _reg ## _REG }

struct cs752x_sd_reg debug_regs[] = {
	DBG_REG(CTRL),
	DBG_REG(PWREN),
	DBG_REG(CLKDIV),
	DBG_REG(CLKSRC),
	DBG_REG(CLKENA),
	DBG_REG(TMOUT),
	DBG_REG(CTYPE),
	DBG_REG(BLKSIZ),
	DBG_REG(BYTCNT),
	DBG_REG(INTMASK),
	DBG_REG(CMDARG),
	DBG_REG(CMD),
	DBG_REG(RESP0_SHORT),
	DBG_REG(RESP1),
	DBG_REG(RESP2),
	DBG_REG(RESP3),
	DBG_REG(MINTSTS),
	DBG_REG(RINTSTS),
	DBG_REG(STATUS),
	DBG_REG(FIFOTH),
	DBG_REG(CDETECT),
	DBG_REG(WRTPRT),
	DBG_REG(GPIO),
	DBG_REG(TCBCNT),
	DBG_REG(TBBCNT),
	DBG_REG(DEBNCE),
	DBG_REG(USRID),
	DBG_REG(VERID),
	DBG_REG(HCON),
	DBG_REG(BMOD),
	DBG_REG(PLDMND),
	DBG_REG(DBADDR),
	DBG_REG(IDSTS),
	DBG_REG(IDINTEN),
	DBG_REG(DSCADDR),
	DBG_REG(BUFADDR),
	{}
};

static int cs752x_sd_regs_show(struct seq_file *seq, void *v)
{
	struct cs752x_sd_host *cs752x_host = seq->private;
	struct cs752x_sd_reg *rptr = debug_regs;

	for (; rptr->name; rptr++) {
		seq_printf(seq, "[%04x] G2_%s\t=0x%08x\n",
			rptr->addr, rptr->name,
			readl(cs752x_host->base + rptr->addr));
	}
	return 0;
}

static int cs752x_sd_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, cs752x_sd_regs_show, inode->i_private);
}

static const struct file_operations cs752x_sd_fops_regs = {
	.owner          = THIS_MODULE,
	.open           = cs752x_sd_regs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

/* mount debugfs instruction */
/* $ mount -t debugfs none /sys/kernel/debug */
static void cs752x_sd_debugfs_attach(struct cs752x_sd_host *cs752x_host)
{
	struct device *dev = &cs752x_host->pdev->dev;

	cs752x_host->debug_root = debugfs_create_dir(dev_name(dev), NULL);
	if (IS_ERR(cs752x_host->debug_root)) {
		dev_err(dev, "failed to create debugfs root\n");
		return;
	}

	cs752x_host->debug_state = debugfs_create_file("state", 0444,
					cs752x_host->debug_root, cs752x_host,
					&cs752x_sd_fops_state);
	if (IS_ERR(cs752x_host->debug_state))
		dev_err(dev, "failed to create debug state file\n");

	cs752x_host->debug_regs = debugfs_create_file("regs", 0444,
					cs752x_host->debug_root, cs752x_host,
					&cs752x_sd_fops_regs);
	if (IS_ERR(cs752x_host->debug_regs))
		dev_err(dev, "failed to create debug regs file\n");
}

static void cs752x_sd_debugfs_remove(struct cs752x_sd_host *cs752x_host)
{
	debugfs_remove(cs752x_host->debug_regs);
	debugfs_remove(cs752x_host->debug_state);
	debugfs_remove(cs752x_host->debug_root);
}
#else
static inline void cs752x_sd_debugfs_attach(struct cs752x_sd_host *cs752x_host) { }
static inline void cs752x_sd_debugfs_remove(struct cs752x_sd_host *cs752x_host) { }
#endif  /* end of CONFIG_DEBUG_FS */

static int cs752x_sd_finished_data(struct cs752x_sd_host *cs752x_host)
{
	struct mmc_data *data = cs752x_host->mrq->data;
	int data_error=0;

	if (cs752x_sd_host_usedma(cs752x_host)) {
		dma_unmap_sg(mmc_dev(cs752x_host->mmc), data->sg, data->sg_len, cs752x_host->dmadir);
		if (cs752x_host->mrq->cmd->data->error)
			data->bytes_xfered = readl(cs752x_host->base + TCBCNT_REG);
		else
			data->bytes_xfered = (data->blocks * data->blksz);
	}

	return data_error;
}

/*
 * Handle a command that has been done
 */
static void cs752x_sd_cmd_done(struct cs752x_sd_host *cs752x_host,
				u32 manu_cmd12)
{
	struct mmc_command *cmd = cs752x_host->mrq->cmd;
	u32  sd_int_status      = cs752x_host->int_status;

	DBGENTER(3);
	pr_debug("Command Done\n");

	if (MMC_STOP_TRANSMISSION == manu_cmd12) {
		cmd  = cs752x_host->mrq->stop;
		sd_int_status = cs752x_host->int_status_for_stopcmd;
	}
	if (!cmd) {
		printk("<%s:%d> cmd=NULL (halt)\n", __func__, __LINE__);
		BUG_ON(1);
	}
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = readl(cs752x_host->base + RESP0_SHORT_REG);	/* B[31:00] */
			cmd->resp[2] = readl(cs752x_host->base + RESP1_REG);	/* B[63:32] */
			cmd->resp[1] = readl(cs752x_host->base + RESP2_REG);	/* B[95:64] */
			cmd->resp[0] = readl(cs752x_host->base + RESP3_REG);	/* B[127:96]*/
			DBGPRINT(3, "<%s:%d> resp0=0x%08x\n",
				__func__, __LINE__, cmd->resp[0]);
			DBGPRINT(3, "<%s:%d> resp1=0x%08x\n",
				__func__, __LINE__, cmd->resp[1]);
			DBGPRINT(3, "<%s:%d> resp2=0x%08x\n",
				__func__, __LINE__, cmd->resp[2]);
			DBGPRINT(3, "<%s:%d> resp3=0x%08x\n",
				__func__, __LINE__, cmd->resp[3]);
			DBGPRINT(1, "Command response [%08x %08x %08x %08x] current state=%d\n",
				cmd->resp[0], cmd->resp[1], cmd->resp[2],
				cmd->resp[3], R1_CURRENT_STATE(cmd->resp[0]));
		} else {
			if (sd_int_status & G2_INTMSK_ACD)
				cmd->resp[0] = readl(cs752x_host->base + RESP1_REG);
			else
				cmd->resp[0] = readl(cs752x_host->base + RESP0_SHORT_REG);
#ifdef CONFIG_CS752X_SD_DEBUG
			if (cs752x_host->dbg_cmdinerr) {
				printk("<%s:%d> cmd%d, resp0=0x%08x current state=%d\n",
					__func__, __LINE__, cmd->opcode,
					cmd->resp[0],
					R1_CURRENT_STATE(cmd->resp[0]));
			}
			DBGPRINT(3, "<%s:%d> resp0=0x%08x\n",
				__func__, __LINE__, cmd->resp[0]);
#endif
		}
	}
	pr_debug("Command response [%08x %08x %08x %08x]\n",
		cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3]);
	DBGLEAVE(3);
}

/*
 * Handle a data transaction that has been done
 */
static void cs752x_sd_data_done(struct cs752x_sd_host *cs752x_host)
{
	int data_error;

	DBGENTER(3);
	data_error = cs752x_sd_finished_data(cs752x_host);
	DBGLEAVE(3);
}

static u32 cs752x_sd_interrupt_status_check(struct cs752x_sd_host *cs752x_host)
{
	const u32 sd_int_status = cs752x_host->int_status;
	struct mmc_request *mrq  = cs752x_host->mrq;
	u32 int_handle = REQUEST_NORMAL_DONE;

	DBGENTER(3);

	/* Check error event flags */
	if (sd_int_status & (G2_INTMSK_RESP_ERR | G2_INTMSK_FATAL_ERR))
		mrq->cmd->error = -ECOMM;

	/* Check fatal errors flags */
	if (sd_int_status & G2_INTMSK_HTO) {	/* b10: Data starv-by-host */
		mrq->cmd->error = -ETIMEDOUT;
		cs752x_dbg_dmpregs(cs752x_host, "starvation wr err");
	}
	if (sd_int_status & G2_INTMSK_FRUN) {	/* b11: FIFO under/overrun */
		mrq->cmd->data->error = -EILSEQ;
		cs752x_dbg_dmpregs(cs752x_host, "fifo_u/o_run_A");
		cs752x_dbg_dmpdesc(cs752x_host, "fifo_u/o_run_A");
	}
	if (sd_int_status & G2_INTMSK_HLE) {	/* b12: Hardware locked write
						error. If write is attempted
						during previous command
						processing (busy now), this bit
						will be set. */
		cs752x_dbg_dmpregs(cs752x_host, "HW locked wr err");
		//DBGPRINT(1, "<%s:%d> Hardware locked error\n", __func__, __LINE__);
		printk("<%s:%d> Hardware locked error\n", __func__, __LINE__);
	}
	if (sd_int_status & G2_INTMSK_SBE) {	/* b13: Start-bit error */
		mrq->cmd->data->error = -EILSEQ;
		cs752x_dbg_dmpregs(cs752x_host, "StartBit err");
		printk("<%s:%d> Start Bit error (cmd%d)\n", __func__, __LINE__, cs752x_host->mrq->cmd->opcode);
	}
	if (sd_int_status & G2_INTMSK_CDT){  	/* b0: Card Detect */
		cs752x_dbg_dmpregs(cs752x_host, "Card Abnormal Change");
		mmc_detect_change(cs752x_host->mmc, 0);
	}
	if (sd_int_status & (G2_INTMSK_FATAL_ERR | G2_INTMSK_CDT)) {
		cs752x_host->flag.bit.rstctrlreq = 1;
		int_handle = ERR_FATAL;
		DBGLEAVE(3);
		return int_handle;
	}

/* Check command flags */
	if (sd_int_status & G2_INTMSK_RESP_ERR){/* b8,6,1: */
		if (mrq->cmd->data)
			cs752x_host->flag.bit.rstctrlreq = 1;
	}
	if (sd_int_status & G2_INTMSK_RE)	/* b1: Response error */
		cs752x_dbg_dmpregs(cs752x_host, "Resp err");

	if (sd_int_status & G2_INTMSK_RCRC) 	/* b6: Response CRC error */
		cs752x_dbg_dmpregs(cs752x_host, "Resp CRC");

	if (sd_int_status & G2_INTMSK_RTO) {	/* b8: Response timeout */
		mrq->cmd->error = -ETIMEDOUT;
		cs752x_dbg_dmpregs(cs752x_host, "Resp to");
	}

/* Check data flags */
	if (sd_int_status & G2_INTMSK_DATA_ERR) {	/* b15,9,7 */
#ifdef CONFIG_CS752X_SD_DEBUG
		SD_DESCRIPT_T  *txrx_desc = cs752x_host->desc_cpu;
		unsigned int   cnt, sgcnt;
		u8 *phybufadr, *virtbufadr;
#endif
		u32 tcbcnt, tbbcnt;

		mrq->cmd->data->error = -EILSEQ;
		tcbcnt  = readl(cs752x_host->base + TCBCNT_REG);
		tbbcnt  = readl(cs752x_host->base + TBBCNT_REG);
		if (tcbcnt != tbbcnt) {
			DBGPRINT(1, "tcbcnt=0x%08x, tbbcnt=0x%08x, need to reset the controller\n", tcbcnt, tbbcnt);
			cs752x_host->flag.bit.rstctrlreq = 1;
		}

#ifdef CONFIG_CS752X_SD_DEBUG
		/* Display the buffer contents */
		sgcnt = tcbcnt >> PAGE_SHIFT;
		if (sgcnt) {
			for (cnt=0; cnt<sgcnt; cnt++) {
		 		phybufadr = txrx_desc->buffer_addr;
				virtbufadr = __va(phybufadr);
				DBGPRINT(1, "<%s:%d> buf addr [phy:%p vir:%p]\n",
					__func__, __LINE__,
					phybufadr, virtbufadr);
				txrx_desc = (SD_DESCRIPT_T *)(cs752x_host->desc_cpu + (cnt+1));
			}
			DBGDUMPBUF(1, virtbufadr, 8);
		}
#endif
	}
	if (sd_int_status & G2_INTMSK_DCRC) {	/* b7: Data Write CRC error */
		(DMA_TO_DEVICE == cs752x_host->dmadir) ?
		cs752x_dbg_dmpregs(cs752x_host, "Wr CRC err"):
		cs752x_dbg_dmpregs(cs752x_host, "Rd CRC err");
	}
	if (sd_int_status & G2_INTMSK_DRTO) {	/* b9: Data read timeout, may be
						generated by sending cmd12 */
		mrq->cmd->data->error = -ETIMEDOUT;
		cs752x_dbg_dmpregs(cs752x_host, "Dat rd to");
	}
	if (sd_int_status & G2_INTMSK_EBE) 	/* b15: End-bit error (read)/write no CRC */
		cs752x_dbg_dmpregs(cs752x_host, "Endbit wr no CRC");

/* Check AUTO_CMD_DONE, CMD_DONE and DTO flags */
	if (sd_int_status & (G2_INTMSK_CD | G2_INTMSK_DTO | G2_INTMSK_ACD)) {
		u32 runcmd=0;
		u32 sd_int_status_tmp = sd_int_status & (G2_INTMSK_CD |
							 G2_INTMSK_DTO |
							 G2_INTMSK_ACD);
		switch (sd_int_status_tmp) {
		case G2_INTMSK_CD:
			if (mrq->data)
				int_handle = cs752x_host->flag.bit.xferdone ?
					DTO_TRAP : mrq->cmd->error ?
					ERR_CMD_HANDLE : WAIT_DTO;
			else
				int_handle = mrq->cmd->error ?
					ERR_CMD_HANDLE : REQUEST_NORMAL_DONE;
			break;
		case G2_INTMSK_DTO:
			cs752x_host->flag.bit.xferdone = 1;
			runcmd = readl(cs752x_host->base + CMD_REG);
			int_handle = mrq->cmd->data->error ?
				ERR_DAT_HANDLE : (runcmd & CMD_SND_AUTOSTOP) ?
				WAIT_ACD : REQUEST_NORMAL_DONE;
			break;
		case G2_INTMSK_CD | G2_INTMSK_DTO:
			cs752x_host->flag.bit.xferdone = 1;
			runcmd = readl(cs752x_host->base + CMD_REG);
			int_handle = (mrq->cmd->error || mrq->cmd->data->error) ?
				ERR_CMD_DAT_HANDLE : (runcmd & CMD_SND_AUTOSTOP) ?
				WAIT_ACD : REQUEST_NORMAL_DONE;
			break;
		case G2_INTMSK_ACD:
			if (mrq->data) {
				int_handle = (mrq->cmd->error) ?
					ERR_CMD_HANDLE : cs752x_host->flag.bit.xferdone ?
					REQUEST_NORMAL_DONE : ACD_TRAP;
			} else {
				printk("<%s:%d> Auto Cmd Done Trap\n",
					__func__, __LINE__);
				int_handle = ACD_TRAP;
			}
			break;
		case G2_INTMSK_DTO | G2_INTMSK_ACD:
			cs752x_host->flag.bit.xferdone = 1;
			int_handle = mrq->cmd->data->error ?
				ERR_DAT_HANDLE : REQUEST_NORMAL_DONE;
			break;
		case G2_INTMSK_CD | G2_INTMSK_DTO | G2_INTMSK_ACD:/* BUG#25729,
									#25755 */
			cs752x_host->flag.bit.xferdone = 1;
			int_handle = (mrq->cmd->error || mrq->cmd->data->error) ?
				ERR_CMD_DAT_HANDLE : REQUEST_NORMAL_DONE;
			break;
		default:
			printk("<%s:%d> I don't know who you are. (int=%08x)\n",
				__func__, __LINE__, sd_int_status);
			BUG_ON(1);
			break;
		}
	} else {
		printk("<%s:%d> I don't know who you are. (int=%08x)\n",
			__func__, __LINE__, sd_int_status);
		BUG_ON(1);
	}

	DBGPRINT(3, "int_handle=0x%08x\n", int_handle);
	DBGLEAVE(3);
	return int_handle;
}

static inline void cs752x_sd_report_irq(struct cs752x_sd_host *cs752x_host)
{
	const u32 sd_int_status = cs752x_host->int_status;
	static const char *cs752x_sd_status_bits[] = {
		"CDT", "RE", "CD", "DTO",
		"TXDR", "RXDR", "RCRC", "DCRC",
		"RTO", "DRTO", "HTO", "FRUN",
		"HLE", "SBE", "ACD", "EBE"
	};
	int i, cnt=0;
	for (i=0; i<ARRAY_SIZE(cs752x_sd_status_bits); i++) {
		if (sd_int_status & (1 << i)) {
			if (cnt)
				printk(" ");

			printk("%s", cs752x_sd_status_bits[i]);
			cnt++;
		}
	}
	if (!(sd_int_status & G2_INTMSK_CDT))
		printk(" [CMD%d] ", cs752x_host->mrq->cmd->opcode);
}

static void cs752x_sd_rst_ctrl(struct cs752x_sd_host *cs752x_host)
{
	unsigned long flags;
#ifdef CONFIG_CS752X_SD_DEBUG
	printk("reset the sd controller\n");
#endif
	spin_lock_irqsave(&cs752x_host->lock, flags);		/* BUG#27443 */
	cs752x_host->sdinrst = 1;
	cs752x_sd_set_bits(cs752x_host, CTRL_REG, DMA_RESET |
					FIFO_RESET |
					CONTROLLER_RESET);
	writel(0xffffffff, cs752x_host->base + RINTSTS_REG);
	writel(0xffffffff, cs752x_host->base + IDSTS_REG);
	cs752x_sd_set_bits(cs752x_host, CTRL_REG, FIFO_RESET);
	cs752x_host->sdinrst = 0;
	spin_unlock_irqrestore(&cs752x_host->lock, flags);	/* BUG#27443 */
}

/* Initial FIFO-related registers */
static void cs752x_sd_init_fifo(struct cs752x_sd_host *cs752x_host)
{
	unsigned int    regval = 0;
	unsigned int    fifo_depth;
	unsigned int    fifo_threshold;

	regval = readl(cs752x_host->base + FIFOTH_REG);
	fifo_depth = (regval >> 16) + 1;
	fifo_threshold = fifo_depth >> 1;

	DBGPRINT(3, "fifo_depth=%d, fifo_threshold=%d, regval=0x%08x\n",
		fifo_depth, fifo_threshold, regval);

	cs752x_sd_set_bits(cs752x_host, FIFOTH_REG, 0x20000000);
	/* TX Watermark */
	cs752x_sd_clr_bits(cs752x_host, FIFOTH_REG, 0x00000fff);
	cs752x_sd_set_bits(cs752x_host, FIFOTH_REG, fifo_threshold);
	/* RX Watermark */
	cs752x_sd_clr_bits(cs752x_host, FIFOTH_REG, 0x0fff0000);
	cs752x_sd_set_bits(cs752x_host, FIFOTH_REG, (fifo_threshold-1) << 16);
}

void cs752x_sd_init_idma_desc(struct cs752x_sd_host *cs752x_host)
{
	SD_DESCRIPT_T  *idma_txrx_desc;
	unsigned int   cnt;

	DBGENTER(3);

	idma_txrx_desc = cs752x_host->desc_cpu;

	for (cnt=0; cnt<(DESC_BUF_NUM); cnt++) {
		/* des0 */
		idma_txrx_desc->des0.bits.dic = 0;
		idma_txrx_desc->des0.bits.last_desc = 0;
		idma_txrx_desc->des0.bits.first_desc = 0;
		idma_txrx_desc->des0.bits.chained = 1;
		idma_txrx_desc->des0.bits.own = CPU;
		/* des1 */
		idma_txrx_desc->des1.bits.buffer_size1 = 0;
		/* des2 */
		idma_txrx_desc->buffer_addr = 0;
		/* des3 and next descriptor */
		if (cnt < (DESC_BUF_NUM-1)) {
			idma_txrx_desc->next_desc = cs752x_host->desc_dma +
				sizeof(SD_DESCRIPT_T)*(cnt+1);
			DBGPRINT(3, "idma_txrx_desc->next_desc=0x%08x\n",
				idma_txrx_desc->next_desc);
			idma_txrx_desc++;
		} else {	/* last descriptor entry */
			idma_txrx_desc->next_desc   = (u32)NULL;
			DBGPRINT(3, "idma_txrx_desc->next_desc=0x%08x\n",
				idma_txrx_desc->next_desc);
		}
	}
	DBGLEAVE(3);
}

static int cs752x_sd_init_idma(struct cs752x_sd_host *cs752x_host,
				u32 alloc_desc)
{
	/* IDMAC resets all internal registers. It is automatically cleared
	   after 1 clock cycle */
	cs752x_sd_set_bits(cs752x_host, BMOD_REG, G2_BMOD_SWR);
	cs752x_sd_set_bits(cs752x_host, BMOD_REG, G2_BMOD_FB | G2_BMOD_PBL_8);

	if (alloc_desc) {
		/* Allocate sg descriptor */
		cs752x_host->desc_cpu = dma_alloc_coherent(NULL,
					sizeof(SD_DESCRIPT_T)*DESC_BUF_NUM,
					&cs752x_host->desc_dma,
					GFP_KERNEL | GFP_DMA);
		if (!cs752x_host->desc_cpu)
			return -ENOMEM;
	}
	writel(cs752x_host->desc_dma, cs752x_host->base + DBADDR_REG);
	DBGPRINT(3, "cs752x_host->desc_cpu=0x%p (virt addr), cs752x_host->desc_dma=0x%08x (phy addr)\n", cs752x_host->desc_cpu, cs752x_host->desc_dma);

	cs752x_sd_init_idma_desc(cs752x_host);
	cs752x_sd_set_bits(cs752x_host, BMOD_REG, G2_BMOD_DE);	/* Enable IDMA */

	return 0;
}

static int cs752x_sd_init_ctrl(struct cs752x_sd_host *cs752x_host)
{
	u32 runcmd, sd_int_status, cnt;
	u32 handle_fail=0;

	cs752x_sd_rst_ctrl(cs752x_host);
	cs752x_sd_init_idma(cs752x_host, 0);

	/* check for 0 in bit 9 of the STATUS reg */
	cnt=100;
	while(readl(cs752x_host->base + STATUS_REG) & G2_SD_DATA_BUSY){
		if (!(cnt--)) {
			printk("<%s:%d> waiting sd card busy to ready timeout\n", __func__, __LINE__);
			handle_fail=-1;
			break;
		}
	};

/* Update card clock */
	runcmd = CMD_UP_CLKREG | CMD_START | CMD_STOP_ABTCMD | MMC_STOP_TRANSMISSION;

	cs752x_sd_set_bits(cs752x_host, CLKENA_REG, G2_SDCARD0);  /* enable card 0 clock */
	if (readl(cs752x_host->base + CMD_REG) & CMD_START) {
		cs752x_dbg_dmpregs(cs752x_host, "reset CMD hang");
		printk("cmd start_bit is set, I can't send pseudo cmd (cmd_reg=0x%08x).\n",
			readl(cs752x_host->base + CMD_REG));
	}
	DBGPRINT(1, "A cmd_reg=0x%08x, runcmd=0x%08x\n",
		readl(cs752x_host->base + CMD_REG), runcmd);
	writel(runcmd, cs752x_host->base + CMD_REG);
	DBGPRINT(1, "B cmd_reg=0x%08x\n",
		readl(cs752x_host->base + CMD_REG));
	cnt=100;
	do {
		if (!(readl(cs752x_host->base + CMD_REG) & CMD_START)) {
			printk("<%s:%d> start_bit timeout=%d (cmdreg=0x%08x)\n",
				__func__, __LINE__, cnt,
				readl(cs752x_host->base + CMD_REG));
			handle_fail=-2;
			break;
		}
	} while(cnt--);
	if (!cnt) {
		cs752x_dbg_dmpregs(cs752x_host, "reset CMD fail");
		printk("The reset pseudo cmd failed (cmd_reg=0x%08x).\n",
			readl(cs752x_host->base + CMD_REG));
	}

	/* Check Hardware locked error */
	sd_int_status = readl(cs752x_host->base + RINTSTS_REG);
	writel(sd_int_status,   cs752x_host->base + RINTSTS_REG);
	if (G2_INTMSK_HLE & sd_int_status) {
		printk("<%s:%d> Hardware locked error, reload card 0 clock\n", __func__, __LINE__);
		if (readl(cs752x_host->base + CMD_REG) & CMD_START) {
			cs752x_dbg_dmpregs(cs752x_host, "CMD lock hang");
			printk("cmd start_bit is set, I can't resend pseudo cmd (cmd_reg=0x%08x).\n",
				readl(cs752x_host->base + CMD_REG));
		}
		writel(runcmd, cs752x_host->base + CMD_REG);
		cnt=0;
		do {
			cnt++;
			if (!(readl(cs752x_host->base + CMD_REG) & CMD_START)) {
				DBGPRINT(1, "<%s:%d> cmd start_bit timeout count=%d\n", __func__, __LINE__, cnt);
				handle_fail=-3;
				break;
			}
		} while(cnt<1000);
		if (cnt>=1000) {
			cs752x_dbg_dmpregs(cs752x_host, "resend locked CMD fail");
			printk("The resent locked pseudo cmd failed.\n");
		}
	}
	return handle_fail;
}

void err_card_handle(struct cs752x_sd_host *cs752x_host, u32 runcmd)
{
	u32 sd_int_status, cnt, present;

	if (runcmd & CMD_SND_AUTOSTOP) {
		present = readl(cs752x_host->base + CDETECT_REG) & G2_SDCARD0;
#ifdef SD_CD_PRESENT_LOW
		present = (~present) & G2_SDCARD0;
#endif
		if ((!(cs752x_host->int_status & G2_INTMSK_ACD)) && present) {
			cs752x_sd_send_cmd(cs752x_host, MMC_STOP_TRANSMISSION);
			cnt=0;
			do {
				cnt++;
				sd_int_status  = readl(cs752x_host->base + RINTSTS_REG);
				writel(sd_int_status, cs752x_host->base + RINTSTS_REG);
				if (G2_INTMSK_CD & sd_int_status) {
	   				cs752x_host->int_status_for_stopcmd = sd_int_status;
	   				DBGPRINT(1, "stop_CMD12 response timeout count=%d\n", cnt);
	   				break;
				}
			} while(cnt<100);
			if (cnt>=100)
				printk("I don't receive the response of stop_CMD12\n");
			cs752x_sd_cmd_done(cs752x_host, MMC_STOP_TRANSMISSION);
		}
	}
}

/*
 * ISR for MMC/SD Interface IRQ
 *
 * Communication between driver and ISR works
 */
static irqreturn_t cs752x_sd_irq(int irq, void *dev_id)
{
	struct cs752x_sd_host *cs752x_host = (struct cs752x_sd_host *)dev_id;
	struct mmc_request *mrq  = cs752x_host->mrq;
	u32 sd_int_status, sd_int_idma_sts;
	struct mmc_command *cmd;
	u32 int_handle, present;

	DBGENTER(3);
	cs752x_sd_disable_int(cs752x_host);

	/* read and clear current interrupt status registers */
	sd_int_status  = readl(cs752x_host->base + RINTSTS_REG);
	writel(sd_int_status,   cs752x_host->base + RINTSTS_REG);
	sd_int_idma_sts = readl(cs752x_host->base + IDSTS_REG);
	writel(sd_int_idma_sts, cs752x_host->base + IDSTS_REG);

	sd_int_status &= 0xffff;	/* BUG#25928, (suspensive) interrupts */
	cs752x_host->int_status   = sd_int_status;
	cs752x_host->int_idma_sts = sd_int_idma_sts;

#ifdef CONFIG_CS752X_SD_DEBUG
	printk("<%s:%d> int_status=0x%08x\n", __func__, __LINE__, sd_int_status);
#endif
	if (cs752x_host->sdinrst) {
#ifdef CONFIG_CS752X_SD_DEBUG
		printk("ambiguous interrupt because of reset sd controller\n");
#endif
		goto irqhandled;
	}

	/* Firstly, check card detection, normal operation */
	if (G2_INTMSK_CDT == sd_int_status){
		mmc_detect_change(cs752x_host->mmc, 0);
		DBGPRINT(3, "<%s:%d> card change interrupt detected\n", __func__, __LINE__);
		goto irqhandled;
	}

	present = readl(cs752x_host->base + CDETECT_REG) & G2_SDCARD0;
#ifdef SD_CD_PRESENT_LOW
	present = (~present) & G2_SDCARD0;
#endif
	if (!present) {
		printk("sd card has been removed. (int_status=0x%08x)\n", sd_int_status);
		if (G2_INTMSK_FATAL_ERR & sd_int_status)
			cs752x_sd_init_ctrl(cs752x_host);

		if (cs752x_host->flag.bit.cmding) {
			cs752x_host->flag.bit.cmding = 0;
			mmc_request_done(cs752x_host->mmc, cs752x_host->mrq);
		}
		goto irqhandled;
	}

	cmd = mrq->cmd;

#ifdef CONFIG_CS752X_SD_DEBUG
	if (sd_int_status & G2_INTMSK_CD) {
		if (cs752x_host->dbg_cmdinerr) {
			printk("precmd[%d,%d,%d] resp0_reg=0x%08x\n",
				cs752x_host->precmd[0], cs752x_host->precmd[1],
				cs752x_host->precmd[2],
				readl(cs752x_host->base + RESP0_SHORT_REG));
		}
		cs752x_host->precmd[0] = cs752x_host->precmd[1];
		cs752x_host->precmd[1] = cs752x_host->precmd[2];
		cs752x_host->precmd[2] = cmd->opcode;
	}

	dev_dbg(mmc_dev(cs752x_host->mmc), "G2 SD IRQ %04x (CMD %d): ",
		sd_int_status, cmd->opcode);
	cs752x_sd_report_irq(cs752x_host);
	printk("\n");
	if (sd_int_status & (G2_INTMSK_ERRORS))
		if ((5 != cmd->opcode) && (8 != cmd->opcode))
			cs752x_host->dbg_cmdinerr = CS752X_DBG_ERRORHAPPEN;
#endif

	int_handle = cs752x_sd_interrupt_status_check(cs752x_host);
	if (ERR_FATAL & int_handle) {
		if (cs752x_host->flag.bit.cmding) {
			cs752x_sd_cmd_done(cs752x_host, 0);
			if (cmd->data) {
				cs752x_sd_data_done(cs752x_host);
				if(cs752x_host->flag.bit.rstctrlreq) {
					u32 runcmd=readl(cs752x_host->base + CMD_REG);
					cs752x_sd_init_ctrl(cs752x_host);
					err_card_handle(cs752x_host, runcmd);
				}
			} else {
				if (!(ERR_CMD_HANDLE & int_handle)) {
					DBGPRINT(1, "<%s:%d> I don't know who you are. (int=%08x, cmd%d)\n", __func__, __LINE__, sd_int_status, cs752x_host->mrq->cmd->opcode);
					cs752x_sd_init_ctrl(cs752x_host);
				}
			}
			cs752x_host->flag.bit.cmding = 0;
			mmc_request_done(cs752x_host->mmc, cs752x_host->mrq);
		} else {
			if(cs752x_host->flag.bit.rstctrlreq)
				cs752x_sd_init_ctrl(cs752x_host);
		}
	} else if (ERR_HANDLE & int_handle){
		if (cmd->data) {
			cs752x_sd_cmd_done(cs752x_host, 0);
			if ((ERR_DAT_HANDLE & int_handle) || (cs752x_host->flag.bit.rstctrlreq)) {
				cs752x_sd_data_done(cs752x_host);
				if(cs752x_host->flag.bit.rstctrlreq) {
					u32 runcmd=readl(cs752x_host->base + CMD_REG);
					cs752x_sd_init_ctrl(cs752x_host);
					err_card_handle(cs752x_host, runcmd);
				}
			}
		} else {
			if (ERR_CMD_HANDLE & int_handle) {
				cs752x_sd_cmd_done(cs752x_host, 0);
			} else {
				printk("<%s:%d> I don't know who you are. (int=%08x)\n", __func__, __LINE__, sd_int_status);
				BUG_ON(1);
			}
		}
		if (cs752x_host->flag.bit.cmding) {
			cs752x_host->flag.bit.cmding = 0;
			mmc_request_done(cs752x_host->mmc, cs752x_host->mrq);
		}
	} else if (WAIT_ANY & int_handle){
		DBGPRINT(3, "WAIT_%s (do nothing)\n", (WAIT_DTO & int_handle) ? "DTO":"ACD");
	} else if (ANY_TRAP & int_handle){
		printk("<%s:%d> I don't know who you are: %s_TRAP. (int=%08x)\n",
	  __func__, __LINE__, (DTO_TRAP & int_handle)? "DTO":"ACD", sd_int_status);
		BUG_ON(1);
	} else if (REQUEST_NORMAL_DONE == int_handle) {
		if (cmd->data) {
			DBGPRINT(3, "command with data transaction\n");
			cs752x_sd_data_done(cs752x_host);
			if (cs752x_host->flag.bit.xferdone) {
				cs752x_sd_cmd_done(cs752x_host, 0);
			} else {
				printk("<%s:%d> I don't know who you are. (int=%08x)\n", __func__, __LINE__, sd_int_status);
				BUG_ON(1);
			}
		} else {
			cs752x_sd_cmd_done(cs752x_host, 0);
		}
		cs752x_host->flag.bit.cmding = 0;	/* BUG#27443: clear cmding when cmd done */
		mmc_request_done(cs752x_host->mmc, cs752x_host->mrq);
	} else {
		printk("<%s:%d> I don't know who you are. (int=%08x)\n",
			__func__, __LINE__, sd_int_status);
		cs752x_host->flag.bit.cmding = 0;	/* BUG#27443: clear cmding when cmd done */
		mmc_request_done(cs752x_host->mmc, cs752x_host->mrq);
		BUG_ON(1);
	}

irqhandled:
	cs752x_sd_enable_int(cs752x_host);
	DBGLEAVE(3);
	return IRQ_HANDLED;
}

/*
 * cs752x_sd_host_usedma - return whether the host is using dma or pio
 * @host: The host state
 */
static inline bool cs752x_sd_host_usedma(struct cs752x_sd_host *cs752x_host)
{
	return cs752x_host->usedma;
}

static void cs752x_sd_send_cmd(struct cs752x_sd_host *cs752x_host,
				u32 manu_cmd12)
{
	struct mmc_request *mrq  = cs752x_host->mrq;
	unsigned long flags;
	struct mmc_command *cmd  = mrq->cmd;
	u32 runcmd;

	DBGENTER(3);

	spin_lock_irqsave(&cs752x_host->lock, flags);		/* BUG#29474 */
	if (readl(cs752x_host->base + CMD_REG) & CMD_START) {
		printk("sd cmd start_bit is set, I can't send new cmd (cmd_reg=0x%08x).\n", readl(cs752x_host->base + CMD_REG));
		/* BUG#27847: sd card hang. It should be error handling. */
		if (MMC_STOP_TRANSMISSION != manu_cmd12) {	/* Only in normal command path */
			spin_unlock_irqrestore(&cs752x_host->lock, flags);	/* BUG#29474 */
			cmd->error = -EINVAL;
			return;
		}
	}

	if (MMC_STOP_TRANSMISSION == manu_cmd12) {
		cmd  = mrq->stop;
		DBGPRINT(1, "Error dataxfer happened. Send stop_CMD12 manually. cmd->data=%p\n", cmd->data);
	}

	/* write mmc/sd command argument to register */
	DBGPRINT(3, "SD command arg = 0x%x\n", cmd->arg);
	writel(cmd->arg, cs752x_host->base + CMDARG_REG);
	spin_unlock_irqrestore(&cs752x_host->lock, flags);	/* BUG#29474 */

	/* Trigger mmc/sd command */
	runcmd = cmd->opcode & CMD_OPCODE_INDEX;
	if ((MMC_GO_IDLE_STATE == cmd->opcode) || (MMC_GO_INACTIVE_STATE == cmd->opcode)) 	/* Replace MMC_STOP_TRANSMISSION, p82 */
		runcmd |= CMD_STOP_ABTCMD;

	runcmd |= (MMC_STOP_TRANSMISSION == manu_cmd12) ? CMD_START : (CMD_START | CMD_WAIT_PDATA);
	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_R1: /* short CRC, OPCODE */
		DBGPRINT(3, "SD response type: short CRC, OPCODE\n");
	case MMC_RSP_R1B:/* short CRC, OPCODE, BUSY */
		DBGPRINT(3, "SD response type: short CRC, OPCODE, BUSY\n");
		runcmd |= CMD_RSP_EXP | CMD_CHK_RSP_CRC;
		break;
	case MMC_RSP_R2: /* long 136 bit + CRC */
		DBGPRINT(3, "SD response type: long 136 bit + CRC\n");
		runcmd |= CMD_RSP_EXP | CMD_RSP_LONG_LEN;
		break;
	case MMC_RSP_R3: /* short */
		DBGPRINT(3, "SD response type: short\n");
		runcmd |= CMD_RSP_EXP;
		break;
	case MMC_RSP_NONE:
		DBGPRINT(3, "SD response type: none\n");
		break;
	default:
		dev_err(mmc_dev(cs752x_host->mmc), "unhandled response type 0x%x\n", mmc_resp_type(cmd));
		cmd->error = -EINVAL;
		return;
	}
	if (cmd->data) {
		runcmd |= CMD_DATA_EXP;
		if (cmd->data->flags & MMC_DATA_WRITE) {
			runcmd |= CMD_WRITE_CARD;
			DBGPRINT(3, "data write transfer\n");
		} else {
			DBGPRINT(3, "data read  transfer\n");
		}
		if (cmd->data->flags & MMC_DATA_STREAM) {
			runcmd |= CMD_STREAM_XFER;
			DBGPRINT(3, "stream data transfer\n");
		} else if (cs752x_host->mrq->stop) {
			runcmd |= CMD_SND_AUTOSTOP;
			DBGPRINT(3, "block data transfer with autostop cmd\n");
		}
	}

#ifdef CONFIG_CS752X_SD_DEBUG
	if (cs752x_host->dbg_cmdinerr) {	/* Debug only */
		printk("\nCmdreg before issue: 0x%x (CMD%d, cmd->arg=0x%08x, cmd->data=%p, requestcnt=%d, dispcnt=%d)\n",
			runcmd, (runcmd & 0x3f), cmd->arg, cmd->data,
			cs752x_host->dbg_reqcnt, cs752x_host->dbg_cmdinerr);
		cs752x_host->dbg_cmdinerr--;
	}
#endif

	/*spin_lock(&cs752x_host->lock);*/	/* BUG#29474 */
	if (MMC_STOP_TRANSMISSION != manu_cmd12)
		cs752x_host->flag.word = 0x0001;

	writel(runcmd, cs752x_host->base + CMD_REG);
	/*spin_unlock(&cs752x_host->lock);*/	/* BUG#29474 */

	DBGLEAVE(3);
}

static inline void cs752x_sd_set_bits(struct cs752x_sd_host *cs752x_host,
				enum cs752x_sd_regs cs752x_reg, u32 setbits)
{
	u32 regval;

	regval = readl(cs752x_host->base + cs752x_reg);
	regval |= setbits;
	writel(regval, cs752x_host->base + cs752x_reg);
}

static inline void cs752x_sd_clr_bits(struct cs752x_sd_host *cs752x_host,
				enum cs752x_sd_regs cs752x_reg, u32 clrbits)
{
	u32 regval;

	regval = readl(cs752x_host->base + cs752x_reg);
	regval &= ~clrbits;
	writel(regval, cs752x_host->base + cs752x_reg);
}

static void cs752x_sd_enable_int(struct cs752x_sd_host *cs752x_host)
{
	u32 regval;

	regval = readl(cs752x_host->base + CTRL_REG);
	regval |= INT_ENABLE;
	writel(regval, cs752x_host->base + CTRL_REG);
}

static void cs752x_sd_disable_int(struct cs752x_sd_host *cs752x_host)
{
	u32 regval;

	regval = readl(cs752x_host->base + CTRL_REG);
	regval &= ~INT_ENABLE;
	writel(regval, cs752x_host->base + CTRL_REG);
}

/*
 * setup dma descriptor from scatter/gather sg
 */
static int cs752x_sd_setup_txrx_desc(struct cs752x_sd_host *cs752x_host)
{
	SD_DESCRIPT_T  *txrx_desc;
	struct scatterlist *sg = cs752x_host->mrq->data->sg;
	unsigned int   cnt;
	void *sg_virtaddr;

	txrx_desc = cs752x_host->desc_cpu;
	txrx_desc->des0.bits.first_desc = 1;
	for (cnt=0; cnt<(cs752x_host->descnt); cnt++) {
		/* desc0 */
		txrx_desc->des0.bits.dic = 1;
		txrx_desc->des0.bits.last_desc = 0;
		if (cnt == cs752x_host->descnt-1) {
			txrx_desc->des0.bits.dic = 0;
			txrx_desc->des0.bits.last_desc = 1;
			DBGPRINT(3, "this is the last descriptor\n");
		}
		txrx_desc->des0.bits.own = DMA;

		/* desc1 */
		txrx_desc->des1.bits.buffer_size1 = sg_dma_len(&sg[cnt]);
		DBGPRINT(3, "desc1: data buffer size to be transferred=%d\n", txrx_desc->des1.bits.buffer_size1);

		/* desc2 */
		txrx_desc->buffer_addr = sg[cnt].dma_address;
		DBGPRINT(3, "&sg[%d]=0x%p\n", cnt, &sg[cnt]);
		DBGPRINT(3, "&sg[%d].dma_address=0x%p, content=0x%08x\n", cnt, &sg[cnt].dma_address, sg[cnt].dma_address);
		DBGPRINT(3, "The physical address of the current data buffer=0x%08x\n", txrx_desc->buffer_addr);
		sg_virtaddr =  sg_virt(sg);
		DBGPRINT(3, "The virtual address of sg=0x%p\n", sg_virtaddr);

		/* desc3 */
		txrx_desc = (SD_DESCRIPT_T *)(cs752x_host->desc_cpu + (cnt+1));
		DBGPRINT(3, "The next descriptor address=0x%p\n", txrx_desc);
	}
	return 0;
}

static void cs752x_set_timeout(struct cs752x_sd_host *cs752x_host)
{
	struct mmc_host *mmc    = cs752x_host->mmc;
	struct mmc_request *mrq = cs752x_host->mrq;
	unsigned int timeout_clk;

	if (mrq) {
		DBGPRINT(3, "mrq=%p\n", mrq);
		if ((unsigned int)(mrq->data) & 0x80000000) {
			struct mmc_data *data   = mrq->data;

			timeout_clk = (data->timeout_ns/1000) * (mmc->f_max/1000000) + data->timeout_clks;

			writel((timeout_clk << 8) | G2_CMDTMOUT, cs752x_host->base + TMOUT_REG);
		}
	}
}

static int cs752x_sd_prepare_data(struct cs752x_sd_host *cs752x_host,
				struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;
	int datafail = 0;
	unsigned int descnt;

	cs752x_set_timeout(cs752x_host);

	cs752x_host->datasize = data->blocks * data->blksz;

	writel(data->blksz,  cs752x_host->base + BLKSIZ_REG);
	writel(cs752x_host->datasize, cs752x_host->base + BYTCNT_REG);
	DBGPRINT(3, "data->blksz=%d, data->blocks=%d\n", data->blksz, data->blocks);
	if (cs752x_sd_host_usedma(cs752x_host)) {
		if (data->flags & MMC_DATA_WRITE)
			cs752x_host->dmadir = DMA_TO_DEVICE;
		else if (data->flags & MMC_DATA_READ)
			cs752x_host->dmadir = DMA_FROM_DEVICE;
		else
			return -EINVAL;  /* undefined direction */

		descnt = dma_map_sg(mmc_dev(cs752x_host->mmc), data->sg,
			data->sg_len, cs752x_host->dmadir);
		if (0 == descnt) {
			return -ENOMEM;
		}
		if (descnt > G2_NR_SG) {
			pr_err("%s: Unable to map %d sg elements [max: %d]\n",
				mmc_hostname(cs752x_host->mmc),
				descnt, G2_NR_SG);
			datafail = -ENOMEM;
			goto unmap_entries;
		}
		if (descnt != data->sg_len) {
			pr_err("%s: Unable to map in all sg [in: %d, out: %d]\n",
				mmc_hostname(cs752x_host->mmc), data->sg_len,
				descnt);
			datafail = -EINVAL;
			goto unmap_entries;
		}
		cs752x_host->descnt = descnt;

		datafail = cs752x_sd_setup_txrx_desc(cs752x_host);
		if (datafail)
			goto unmap_entries;

		cs752x_sd_set_bits(cs752x_host, CTRL_REG, FIFO_RESET);
		writel(G2_ANY_VALUE_OK, cs752x_host->base + PLDMND_REG);
	}

	return datafail;  /* 0: data is ok */

unmap_entries:
	dma_unmap_sg(mmc_dev(cs752x_host->mmc), data->sg, data->sg_len, cs752x_host->dmadir);
	return datafail;
}

static void cs752x_sd_send_request(struct mmc_host *mmc)
{
	struct cs752x_sd_host *cs752x_host = mmc_priv(mmc);
	struct mmc_request *mrq  = cs752x_host->mrq;
	struct mmc_command *cmd  = mrq->cmd;
	int retry_cnt = CMD_MAX_RETRIES;
	u32 chkdataflag = G2_SD_DATA_STATE_BUSY;

#ifdef CONFIG_CS752X_SD_DEBUG
	DBGPRINT(3, "begin to process g2 request...\n");
#endif

	if (!mrq->data) {
		cs752x_dbg_prepare_msg(cs752x_host, cmd, 0);
		     cs752x_sd_send_cmd(cs752x_host, 0);
         	if (-EINVAL == mrq->cmd->error) {       /* BUG#27847 */
         	/* In the case, we must assure that the errorcode is NOT in the interrupt
            	status list. */
                 	cs752x_host->flag.bit.cmding = 0;
                	 mmc_request_done(mmc, mrq);
         	}
		return ;
	}
	if (mrq->data)
		chkdataflag = G2_SD_DATA_STATE_BUSY | G2_SD_DATA_BUSY;

	retry_cnt = CMD_MAX_RETRIES << 3;
	do {
		if (!retry_cnt--) {
			if (MMC_GO_IDLE_STATE == cmd->opcode) {
				printk("idle_CMD0 pass\n");
				break;
			}
			printk("<%s:%d> error: wait data ready timeout! (STATUS_REG=0x%08x)\n",
				__func__, __LINE__,
				readl(cs752x_host->base + STATUS_REG));
			cmd->error       = -EAGAIN;
#ifdef CONFIG_CS752X_SD_DEBUG
			cs752x_dbg_dmpregs(cs752x_host, "data ready to");
#endif
			mmc_request_done(mmc, mrq);
			return;
		}
	} while(readl(cs752x_host->base + STATUS_REG) & chkdataflag);

	cs752x_dbg_prepare_msg(cs752x_host, cmd, 0);
	if (cmd->data) {
		int datafail;
		datafail = cs752x_sd_prepare_data(cs752x_host, mrq);
		if (datafail) {
			cs752x_sd_dbg(cs752x_host, cs752x_dbg_err,
				"%s: setup data error %d\n", __func__, datafail);
			cmd->error       = datafail;
			cmd->data->error = datafail;
			mmc_request_done(mmc, mrq);
			return;
		}
	}
	cs752x_sd_send_cmd(cs752x_host, 0);
	if (-EINVAL == mrq->cmd->error) {	/* BUG#27847 */
	/* In the case, we must assure that the errorcode is NOT in the interrupt
	   status list. */
		cs752x_host->flag.bit.cmding = 0;
		mmc_request_done(mmc, mrq);
	}
}

/* Request function. */
static void cs752x_sd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct cs752x_sd_host *cs752x_host = mmc_priv(mmc);
	u32 cardpresent = readl(cs752x_host->base + CDETECT_REG) & G2_SDCARD0;

	DBGPRINT(3, "\n");
	DBGENTER(3);

#ifdef CONFIG_CS752X_SD_DEBUG
	cs752x_host->dbg_reqcnt++;
#endif

#ifdef CONFIG_CS752X_SD_DEBUG
	printk("<%s:%d>\n", __func__, __LINE__);
#endif

#ifdef SD_CD_PRESENT_LOW
	cardpresent = (~cardpresent) & G2_SDCARD0;
#endif
	cs752x_host->mrq = mrq;
	if (cs752x_host->cardin & cardpresent) {
		cs752x_sd_send_request(mmc);
	} else {
		cs752x_sd_dbg(cs752x_host, cs752x_dbg_err,
			"%s: no medium present (cmd%d)\n",
			__func__, mrq->cmd->opcode);
		cs752x_host->mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
	}

	DBGLEAVE(3);
	DBGPRINT(3, "\n");
}

#ifndef CONFIG_CORTINA_FPGA
unsigned int cs752x_sd_clkdiv(struct cs752x_sd_host *cs752x_host)
{
	unsigned int clkdiv=0;
	struct mmc_ios *ios = &(cs752x_host->mmc->ios);
	struct platform_clk *clk = &(cs752x_host->clk);
	unsigned int APB_MMC_HZ = clk->apb_clk;

	clkdiv = (unsigned int)((APB_MMC_HZ/2) / ios->clock);
	return	clkdiv;
}
#endif

static void cs752x_sd_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	u32 runcmd, error = 0;
	unsigned long flags;
	struct cs752x_sd_host *cs752x_host = mmc_priv(mmc);
	unsigned int clkdiv;

	spin_lock_irqsave(&cs752x_host->lock, flags);		/* BUG#27847 */
	/* Stage 1: check/set power up */
	switch (ios->power_mode) {
	case MMC_POWER_UP:
	case MMC_POWER_ON:
		cs752x_sd_set_bits(cs752x_host, PWREN_REG,  G2_SDCARD0);
		break;
	default:
		break;
	}

	/* bus mode control */
	if (ios->bus_mode == MMC_BUSMODE_OPENDRAIN) {
		DBGPRINT(3, "MMC_BUSMODE_OPENDRAIN: enable card open drain pullup bus\n");
		cs752x_sd_set_bits(cs752x_host, CTRL_REG, ENABLE_OD_PULLUP);
	} else {
		DBGPRINT(3, "disable card open drain pullup bus.\n");
		cs752x_sd_clr_bits(cs752x_host, CTRL_REG, ENABLE_OD_PULLUP);
	}

	/* bus width control */
	switch (mmc->ios.bus_width) {
	case MMC_BUS_WIDTH_4:
		cs752x_sd_set_bits(cs752x_host, CTYPE_REG, G2_CARD0_4BIT);
		break;
	case MMC_BUS_WIDTH_1:
		cs752x_sd_clr_bits(cs752x_host, CTYPE_REG, G2_CARD0_4BIT |
							   G2_CARD0_8BIT);
		break;
	case MMC_BUS_WIDTH_8:
		cs752x_sd_set_bits(cs752x_host, CTYPE_REG, G2_CARD0_8BIT);
		break;
	default:
		break;
	}

	/* Stage 2: set clock frequency */
	if (ios->timing) {
		switch(ios->timing) {
		case MMC_TIMING_SD_HS:
#ifdef CONFIG_CS752X_SD_25MHZ
			mmc->f_max = F_25MHZ;
#else
			mmc->f_max = F_50MHZ;
#endif	/* CONFIG_CS752X_SD_25MHZ */
			break;
		default:
			break;
		}
	}

	/* set clock source */
	if (MMC_POWER_ON == ios->power_mode) {
		u32 cnt=100;
		/* check for 0 in bit 9 of the STATUS reg */
		while(readl(cs752x_host->base + STATUS_REG) & G2_SD_DATA_BUSY){
			if (!(cnt--)) {
				printk("<%s:%d> wait sd card busy to ready timeout (STATUS=0x%08x)\n",
					__func__, __LINE__, readl(cs752x_host->base + STATUS_REG));
				/* BUG#27847, sd card hang. It should be error handling. */
				/* If hardware lock error, we can't write CMD_REG anymore. */
				error = 1;
				break;
			}
		};

		if (0 == error) {
			clkdiv = 0;
			if (0 == ios->clock) {  /* disable clock */
				writel(clkdiv, cs752x_host->base + CLKDIV_REG);
				cs752x_sd_clr_bits(cs752x_host, CLKENA_REG, G2_SDCARD0);
			} else {                /* enable clock */
#ifdef CONFIG_CORTINA_FPGA
				clkdiv = (unsigned int)((APB_MMC_HZ/2) / ios->clock);
#else
				clkdiv = cs752x_sd_clkdiv(cs752x_host);
#endif

				writel(clkdiv, cs752x_host->base + CLKDIV_REG);
				cs752x_sd_set_bits(cs752x_host, CLKENA_REG, G2_SDCARD0);
				udelay(100);	/* BUG#28906, 50 is the minimum stable value in the field test */
			}

			/* send clock to clock domain, don't send command */
			runcmd = CMD_UP_CLKREG | CMD_WAIT_PDATA | CMD_START;
			/* BUG#27847 hardware lock error because of G2_SD_DATA_BUSY */
			writel(runcmd, cs752x_host->base + CMD_REG);
		}
	}

	/* Stage 3: check/set power off */
	switch (ios->power_mode) {
	case MMC_POWER_OFF:	/* power off device card */
		cs752x_sd_clr_bits(cs752x_host, CLKENA_REG, G2_SDCARD0);
		cs752x_sd_clr_bits(cs752x_host, PWREN_REG,  G2_SDCARD0);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&cs752x_host->lock, flags);
}

static int cs752x_sd_get_cd(struct mmc_host *mmc)
{
	struct cs752x_sd_host  *cs752x_host  = mmc_priv(mmc);
	unsigned int present=0;

	DBGENTER(3);

	if (0 == present) {
		present = readl(cs752x_host->base + CDETECT_REG) & G2_SDCARD0;

#ifdef SD_CD_PRESENT_LOW
		present = (~present) & G2_SDCARD0;
#endif
		dev_dbg(&mmc->class_dev, "card is %spresent by detecting CD pin\n", present ? "" : "not ");
		DBGPRINT(1, "card is %spresent by detecting CD pin\n", present ? "" : "not ");
	} else {
		DBGPRINT(3, "status=0x%08x, present=0x%08x\n", readl(cs752x_host->base + STATUS_REG), present);
		dev_dbg(&mmc->class_dev, "card is %spresent by detecting DAT3 pin\n", present ? "" : "not ");
		DBGPRINT(3, "card is %spresent by detecting DAT3 pin\n", present ? "" : "not ");
	}

	cs752x_host->cardin = present;

	if (!present) {
		u32 rstCtrl;

#ifdef CONFIG_CS752X_SD_DEBUG
		cs752x_host->dbg_reqcnt = 0;
#endif
		cs752x_dbg_dmpregs(cs752x_host, "card_rem_A");
		DBGPRINT(1, "cs752x_host=%p, cs752x_host->base=%p\n", cs752x_host, cs752x_host->base);
		rstCtrl = readl(cs752x_host->base + CTRL_REG);
		DBGPRINT(1, "rstCtrl=0x%08x\n", rstCtrl);

		cs752x_sd_rst_ctrl(cs752x_host);

#ifdef CONFIG_CS752X_SD_DEBUG
		cs752x_dbg_dmpregs(cs752x_host, "card_rem_B");
		cs752x_host->dbg_cmdinerr = 0;	/* If error happened, enable some debugging messages */
#endif
	} else {
		if (cs752x_host->desc_cpu) {
			cs752x_dbg_dmpdesc(cs752x_host, "card_ins_A");
			cs752x_sd_init_idma_desc(cs752x_host);

		}
		cs752x_dbg_dmpregs(cs752x_host, "card_inserted");
	}

	DBGLEAVE(3);
	return present;
}

static struct mmc_host_ops cs752x_sd_ops = {
	.request         = cs752x_sd_request,
	.set_ios         = cs752x_sd_set_ios,
	.get_cd          = cs752x_sd_get_cd,
};

void cs752x_sd_init_interrupt(struct cs752x_sd_host *cs752x_host)
{
	u32 intmask = G2_INTMSK_CDT | G2_INTMSK_CD | G2_INTMSK_DTO | G2_INTMSK_ACD |
	  G2_INTMSK_HLE | G2_INTMSK_HTO | G2_INTMSK_FRUN | G2_INTMSK_SBE;
	u32 dmamask = G2_INTDMA_NIS | G2_INTDMA_AIS;

	if (!cs752x_sd_host_usedma(cs752x_host))
		dmamask = 0;


	/* Select MMC/SD mask interrupt events */
	cs752x_sd_set_bits(cs752x_host,RINTSTS_REG, G2_INTMSK_ALL);
	cs752x_sd_set_bits(cs752x_host,INTMASK_REG,intmask);

	/* Select DMA mask interrupt events */
	cs752x_sd_set_bits(cs752x_host, IDSTS_REG, G2_INTDMA_ALL);
	cs752x_sd_set_bits(cs752x_host, IDINTEN_REG, dmamask);

	/* enable dma function */
	if (cs752x_sd_host_usedma(cs752x_host))
		cs752x_sd_set_bits(cs752x_host,CTRL_REG,DMA_ENABLE |
							USE_INTERNAL_DMAC);
}

static int __devinit cs752x_sd_probe(struct platform_device *pdev)
{
	struct cs752x_sd_host *cs752x_host;
	struct mmc_host *mmc;
	int ret = 0;

	DBGENTER(3);

	/* Allocate a mmc_host ds and cs752x_sd_host ds */
	mmc = mmc_alloc_host(sizeof(struct cs752x_sd_host), &pdev->dev);
	if (!mmc) {
		dev_err(&pdev->dev, "couldn't allocate mmc host\n");
		ret = -ENOMEM;
		goto probe_out;
	}

	cs752x_host = mmc_priv(mmc);

	/* initial host */
#ifdef CONFIG_CS752X_SD_DEBUG
	cs752x_host->precmd[2] = cs752x_host->precmd[1] = cs752x_host->precmd[0] = 0;
#endif
	cs752x_host->sdinrst       = 0;	/* 1: sd is in reset status */
	cs752x_host->mmc           = mmc;
	cs752x_host->pdev          = pdev;
	cs752x_host->usedma        = true;

	spin_lock_init(&cs752x_host->lock);

	cs752x_host->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!cs752x_host->mem) {
		dev_err(&pdev->dev, "failed to get io memory region resouce.\n");
		ret = -ENOENT;
		goto probe_free_host;
	}

	cs752x_host->mem = request_mem_region(cs752x_host->mem->start, resource_size(cs752x_host->mem), pdev->name);
	if (!cs752x_host->mem) {
		dev_err(&pdev->dev, "failed to request io memory region.\n");
		ret = -ENOENT;
		goto probe_free_resource;
	}

	cs752x_host->base = ioremap(cs752x_host->mem->start, resource_size(cs752x_host->mem));
	if (!cs752x_host->base) {
		dev_err(&pdev->dev, "failed to ioremap() io memory region.\n");
		ret = -EINVAL;
		goto probe_free_mem_region;
	}

	DBGPRINT(3, "cs752x_host->base = %p\n", cs752x_host->base);
	cs752x_host->irq = platform_get_irq(pdev, 0);
	if (cs752x_host->irq == 0) {
		dev_err(&pdev->dev, "failed to get interrupt resouce.\n");
		ret = -EINVAL;
		goto probe_iounmap;
	}

	if (request_irq(cs752x_host->irq, cs752x_sd_irq, IRQF_SHARED | IRQF_SAMPLE_RANDOM, SD_DRIVER_NAME, cs752x_host)) {
		dev_err(&pdev->dev, "failed to request mmc/sd interrupt.\n");
		ret = -ENOENT;
		goto probe_iounmap;
	}

	DBGPRINT(3, "cs752x_host irq requested: 0x%08x\n", cs752x_host->irq);

#ifdef CONFIG_CORTINA_ENGINEERING
	get_platform_clk(&(cs752x_host->clk));
#endif
	/* reset the controller */
	cs752x_sd_rst_ctrl(cs752x_host);

	cs752x_host->cardin = cs752x_sd_get_cd(mmc);	/* check if card exists. 1: card exists */

	/* initial mmc */
	mmc->ops	= &cs752x_sd_ops;
	mmc->f_min	= F_400KHZ;
	mmc->f_max	= F_25MHZ;
	mmc->caps	|=	MMC_CAP_MMC_HIGHSPEED |
				MMC_CAP_SD_HIGHSPEED |
				MMC_CAP_4_BIT_DATA |
				MMC_CAP_8_BIT_DATA;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	mmc->max_seg_size  = PAGE_SIZE;
	mmc->max_segs   = G2_NR_SG;  /* number of scatter-gathers */

	mmc->max_blk_size  = MMC_MAX_BLOCK_SIZE;
#ifdef CS752X_SD_REQUEST_ONEPAGE
	mmc->max_blk_count = DESC_BUF_NUM >> 3;
#else
	mmc->max_blk_count = DESC_BUF_NUM << 3;
#endif
	mmc->max_req_size  = mmc->max_blk_size * mmc->max_blk_count;

#ifdef CONFIG_CS752X_SD_DEBUG
	cs752x_host->dbg_reqcnt = 0;
	cs752x_host->dbg_cmdinerr = 0;	/* If error happens, enable debug msg */
#endif

	cs752x_sd_init_fifo(cs752x_host);
	ret = cs752x_sd_init_idma(cs752x_host, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to kmalloc idma buffer\n");
		goto probe_free_irq;
	}

	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(&pdev->dev, "failed to add mmc host.\n");
		goto probe_free_idma;
	}

	cs752x_sd_debugfs_attach(cs752x_host);

	platform_set_drvdata(pdev, mmc);
	dev_info(&pdev->dev, "%s - using %s, %s MMC IRQ\n", mmc_hostname(mmc),
	 cs752x_sd_host_usedma(cs752x_host) ? "dma" : "pio",
	 mmc->caps & MMC_CAP_SDIO_IRQ ? "hw" : "sw");

	/* Only enable Card Detection interrupt signal */
	cs752x_sd_set_bits(cs752x_host, CTRL_REG, INT_ENABLE);

	cs752x_sd_init_interrupt(cs752x_host);
	cs752x_sd_enable_int(cs752x_host);

	DBGLEAVE(3);
	return 0;

probe_free_idma:
	if (cs752x_host->desc_cpu)
		dma_free_coherent(NULL, sizeof(SD_DESCRIPT_T)*DESC_BUF_NUM,
			cs752x_host->desc_cpu, cs752x_host->desc_dma);


probe_free_irq:
	free_irq(cs752x_host->irq, cs752x_host);

probe_iounmap:
	iounmap(cs752x_host->base);

probe_free_mem_region:
	release_mem_region(cs752x_host->mem->start,
			resource_size(cs752x_host->mem));

probe_free_resource:
probe_free_host:
	mmc_free_host(mmc);

probe_out:
	return ret;
}

static void cs752x_sd_shutdown(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct cs752x_sd_host *cs752x_host = mmc_priv(mmc);

	cs752x_sd_debugfs_remove(cs752x_host);
	mmc_remove_host(mmc);
}

static int __devexit cs752x_sd_remove(struct platform_device *pdev)
{
	struct mmc_host     *mmc  = platform_get_drvdata(pdev);
	struct cs752x_sd_host  *cs752x_host = mmc_priv(mmc);

	cs752x_sd_shutdown(pdev);

	cs752x_sd_clr_bits(cs752x_host, CLKENA_REG, G2_SDCARD0);
	writel(CMD_UP_CLKREG | CMD_WAIT_PDATA | CMD_START,
		cs752x_host->base + CMD_REG);
	cs752x_sd_clr_bits(cs752x_host, PWREN_REG,  G2_SDCARD0);

	cs752x_sd_disable_int(cs752x_host);
	dma_free_coherent(NULL, sizeof(SD_DESCRIPT_T)*DESC_BUF_NUM,
			cs752x_host->desc_cpu, cs752x_host->desc_dma);
	free_irq(cs752x_host->irq, cs752x_host);
	iounmap(cs752x_host->base);
	release_mem_region(cs752x_host->mem->start,
			resource_size(cs752x_host->mem));
	mmc_free_host(mmc);

	return 0;
}

static struct platform_driver cs752x_sd_driver = {
	.driver= {
		.name  = SD_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe    = cs752x_sd_probe,
	.remove   = __devexit_p(cs752x_sd_remove),
	.shutdown = cs752x_sd_shutdown,
};
MODULE_DEVICE_TABLE(platform, cs752x_sd_driver_ids);

static int __init cs752x_sd_init(void)
{
	int status;

#ifndef CONFIG_CORTINA_FPGA
	status = readl(IO_ADDRESS(GLOBAL_GPIO_MUX_2));
	status &= ~0x000000FF;	/* GPIO 2 Bit[7:0] */
	writel(status, IO_ADDRESS(GLOBAL_GPIO_MUX_2));
#endif
	status = platform_driver_register(&cs752x_sd_driver);

	return status;
}

static void __exit cs752x_sd_exit(void)
{
	platform_driver_unregister(&cs752x_sd_driver);
}

module_init(cs752x_sd_init);
module_exit(cs752x_sd_exit);

MODULE_DESCRIPTION("Cortina CS75XX SD Card Interface driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Joe Hsu <Joe.Hsu@cortina-systems.com>");
