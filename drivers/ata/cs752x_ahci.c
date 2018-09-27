/*
 *  ahci.c - AHCI SATA support
 *
 * * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Jason Li <jason.li@cortina-systems.com>
 *                Modified from ahci.c
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * libata documentation is available via 'make {ps|pdf}docs',
 * as Documentation/DocBook/libata.*
 *
 * AHCI hardware documentation:
 * http://www.intel.com/technology/serialata/pdf/rev1_0.pdf
 * http://www.intel.com/technology/serialata/pdf/rev1_1.pdf
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/libata.h>
#include <mach/platform.h>
#include <linux/platform_device.h>
#include <mach/registers.h>

#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
unsigned char *ahci_base;
#endif

#define DRV_NAME	"goldengate-ahci"
#define DRV_VERSION	"3.0"

/* Enclosure Management Control */
#define EM_CTRL_MSG_TYPE              0x000f0000

/* Enclosure Management LED Message Type */
#define EM_MSG_LED_HBA_PORT           0x0000000f
#define EM_MSG_LED_PMP_SLOT           0x0000ff00
#define EM_MSG_LED_VALUE              0xffff0000
#define EM_MSG_LED_VALUE_ACTIVITY     0x00070000
#define EM_MSG_LED_VALUE_OFF          0xfff80000
#define EM_MSG_LED_VALUE_ON           0x00010000


#define SNOW_PHY_BASE_ADDR PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0

#ifdef CONFIG_CS752X_PROC
extern u32 cs_acp_enable;
#endif

static int ahci_skip_host_reset;
static int ahci_ignore_sss;

module_param_named(skip_host_reset, ahci_skip_host_reset, int, 0444);
MODULE_PARM_DESC(skip_host_reset, "skip global host reset (0=don't skip, 1=skip)");

module_param_named(ignore_sss, ahci_ignore_sss, int, 0444);
MODULE_PARM_DESC(ignore_sss, "Ignore staggered spinup flag (0=don't ignore, 1=ignore)");

extern char *saved_command_line;

static int ahci_set_lpm(struct ata_link *link, enum ata_lpm_policy policy,
			unsigned int hints);
static ssize_t ahci_led_show(struct ata_port *ap, char *buf);
static ssize_t ahci_led_store(struct ata_port *ap, const char *buf,
			      size_t size);
static ssize_t ahci_transmit_led_message(struct ata_port *ap, u32 state,
					ssize_t size);

enum {
	AHCI_PCI_BAR		= 5,
	AHCI_MAX_PORTS		= 32,
	AHCI_MAX_SG		= 168, /* hardware max is 64K */
	AHCI_DMA_BOUNDARY	= 0xffffffff,
	AHCI_MAX_CMDS		= 32,
	AHCI_CMD_SZ		= 32,
	AHCI_CMD_SLOT_SZ	= AHCI_MAX_CMDS * AHCI_CMD_SZ,
	AHCI_RX_FIS_SZ		= 256,
	AHCI_CMD_TBL_CDB	= 0x40,
	AHCI_CMD_TBL_HDR_SZ	= 0x80,
	AHCI_CMD_TBL_SZ		= AHCI_CMD_TBL_HDR_SZ + (AHCI_MAX_SG * 16),
	AHCI_CMD_TBL_AR_SZ	= AHCI_CMD_TBL_SZ * AHCI_MAX_CMDS,
	AHCI_PORT_PRIV_DMA_SZ	= AHCI_CMD_SLOT_SZ + AHCI_CMD_TBL_AR_SZ +
				  AHCI_RX_FIS_SZ,
	AHCI_IRQ_ON_SG		= (1 << 31),
	AHCI_CMD_ATAPI		= (1 << 5),
	AHCI_CMD_WRITE		= (1 << 6),
	AHCI_CMD_PREFETCH	= (1 << 7),
	AHCI_CMD_RESET		= (1 << 8),
	AHCI_CMD_CLR_BUSY	= (1 << 10),

	RX_FIS_D2H_REG		= 0x40,	/* offset of D2H Register FIS data */
	RX_FIS_SDB		= 0x58, /* offset of SDB FIS data */
	RX_FIS_UNK		= 0x60, /* offset of Unknown FIS data */

	board_ahci		= 0,
	board_ahci_vt8251	= 1,
	board_ahci_ign_iferr	= 2,
	board_ahci_sb600	= 3,
	board_ahci_mv		= 4,
	board_ahci_sb700	= 5, /* for SB700 and SB800 */
	board_ahci_mcp65	= 6,
	board_ahci_nopmp	= 7,
	board_ahci_yesncq	= 8,

	/* global controller registers */
	HOST_CAP		= 0x00, /* host capabilities */
	HOST_CTL		= 0x04, /* global host control */
	HOST_IRQ_STAT		= 0x08, /* interrupt status */
	HOST_PORTS_IMPL		= 0x0c, /* bitmap of implemented ports */
	HOST_VERSION		= 0x10, /* AHCI spec. version compliancy */
	HOST_EM_LOC		= 0x1c, /* Enclosure Management location */
	HOST_EM_CTL		= 0x20, /* Enclosure Management Control */
	HOST_CAP2		= 0x24, /* host capabilities, extended */

	/* HOST_CTL bits */
	HOST_RESET		= (1 << 0),  /* reset controller; self-clear */
	HOST_IRQ_EN		= (1 << 1),  /* global IRQ enable */
	HOST_AHCI_EN		= (1 << 31), /* AHCI enabled */

	/* HOST_CAP bits */
	HOST_CAP_SXS		= (1 << 5),  /* Supports External SATA */
	HOST_CAP_EMS		= (1 << 6),  /* Enclosure Management support */
	HOST_CAP_CCC		= (1 << 7),  /* Command Completion Coalescing */
	HOST_CAP_PART		= (1 << 13), /* Partial state capable */
	HOST_CAP_SSC		= (1 << 14), /* Slumber state capable */
	HOST_CAP_PIO_MULTI	= (1 << 15), /* PIO multiple DRQ support */
	HOST_CAP_FBS		= (1 << 16), /* FIS-based switching support */
	HOST_CAP_PMP		= (1 << 17), /* Port Multiplier support */
	HOST_CAP_ONLY		= (1 << 18), /* Supports AHCI mode only */
	HOST_CAP_CLO		= (1 << 24), /* Command List Override support */
	HOST_CAP_LED		= (1 << 25), /* Supports activity LED */
	HOST_CAP_ALPM		= (1 << 26), /* Aggressive Link PM support */
	HOST_CAP_SSS		= (1 << 27), /* Staggered Spin-up */
	HOST_CAP_MPS		= (1 << 28), /* Mechanical presence switch */
	HOST_CAP_SNTF		= (1 << 29), /* SNotification register */
	HOST_CAP_NCQ		= (1 << 30), /* Native Command Queueing */
	HOST_CAP_64		= (1 << 31), /* PCI DAC (64-bit DMA) support */

	/* HOST_CAP2 bits */
	HOST_CAP2_BOH		= (1 << 0),  /* BIOS/OS handoff supported */
	HOST_CAP2_NVMHCI	= (1 << 1),  /* NVMHCI supported */
	HOST_CAP2_APST		= (1 << 2),  /* Automatic partial to slumber */

	/* registers for each SATA port */
	PORT_LST_ADDR		= 0x00, /* command list DMA addr */
	PORT_LST_ADDR_HI	= 0x04, /* command list DMA addr hi */
	PORT_FIS_ADDR		= 0x08, /* FIS rx buf addr */
	PORT_FIS_ADDR_HI	= 0x0c, /* FIS rx buf addr hi */
	PORT_IRQ_STAT		= 0x10, /* interrupt status */
	PORT_IRQ_MASK		= 0x14, /* interrupt enable/disable mask */
	PORT_CMD		= 0x18, /* port command */
	PORT_TFDATA		= 0x20,	/* taskfile data */
	PORT_SIG		= 0x24,	/* device TF signature */
	PORT_CMD_ISSUE		= 0x38, /* command issue */
	PORT_SCR_STAT		= 0x28, /* SATA phy register: SStatus */
	PORT_SCR_CTL		= 0x2c, /* SATA phy register: SControl */
	PORT_SCR_ERR		= 0x30, /* SATA phy register: SError */
	PORT_SCR_ACT		= 0x34, /* SATA phy register: SActive */
	PORT_SCR_NTF		= 0x3c, /* SATA phy register: SNotification */

	/* PORT_IRQ_{STAT,MASK} bits */
	PORT_IRQ_COLD_PRES	= (1 << 31), /* cold presence detect */
	PORT_IRQ_TF_ERR		= (1 << 30), /* task file error */
	PORT_IRQ_HBUS_ERR	= (1 << 29), /* host bus fatal error */
	PORT_IRQ_HBUS_DATA_ERR	= (1 << 28), /* host bus data error */
	PORT_IRQ_IF_ERR		= (1 << 27), /* interface fatal error */
	PORT_IRQ_IF_NONFATAL	= (1 << 26), /* interface non-fatal error */
	PORT_IRQ_OVERFLOW	= (1 << 24), /* xfer exhausted available S/G */
	PORT_IRQ_BAD_PMP	= (1 << 23), /* incorrect port multiplier */

	PORT_IRQ_PHYRDY		= (1 << 22), /* PhyRdy changed */
	PORT_IRQ_DEV_ILCK	= (1 << 7), /* device interlock */
	PORT_IRQ_CONNECT	= (1 << 6), /* port connect change status */
	PORT_IRQ_SG_DONE	= (1 << 5), /* descriptor processed */
	PORT_IRQ_UNK_FIS	= (1 << 4), /* unknown FIS rx'd */
	PORT_IRQ_SDB_FIS	= (1 << 3), /* Set Device Bits FIS rx'd */
	PORT_IRQ_DMAS_FIS	= (1 << 2), /* DMA Setup FIS rx'd */
	PORT_IRQ_PIOS_FIS	= (1 << 1), /* PIO Setup FIS rx'd */
	PORT_IRQ_D2H_REG_FIS	= (1 << 0), /* D2H Register FIS rx'd */

	PORT_IRQ_FREEZE		= PORT_IRQ_HBUS_ERR |
				  PORT_IRQ_IF_ERR |
				  PORT_IRQ_CONNECT |
				  PORT_IRQ_PHYRDY |
				  PORT_IRQ_UNK_FIS |
				  PORT_IRQ_OVERFLOW |
				  PORT_IRQ_BAD_PMP,
	PORT_IRQ_ERROR		= PORT_IRQ_FREEZE |
				  PORT_IRQ_TF_ERR |
				  PORT_IRQ_HBUS_DATA_ERR,
	DEF_PORT_IRQ		= PORT_IRQ_ERROR | PORT_IRQ_SG_DONE |
				  PORT_IRQ_SDB_FIS | PORT_IRQ_DMAS_FIS |
				  PORT_IRQ_PIOS_FIS | PORT_IRQ_D2H_REG_FIS,

	/* PORT_CMD bits */
	PORT_CMD_ASP		= (1 << 27), /* Aggressive Slumber/Partial */
	PORT_CMD_ALPE		= (1 << 26), /* Aggressive Link PM enable */
	PORT_CMD_ATAPI		= (1 << 24), /* Device is ATAPI */
	PORT_CMD_PMP		= (1 << 17), /* PMP attached */
	PORT_CMD_LIST_ON	= (1 << 15), /* cmd list DMA engine running */
	PORT_CMD_FIS_ON		= (1 << 14), /* FIS DMA engine running */
	PORT_CMD_FIS_RX		= (1 << 4), /* Enable FIS receive DMA engine */
	PORT_CMD_CLO		= (1 << 3), /* Command list override */
	PORT_CMD_POWER_ON	= (1 << 2), /* Power up device */
	PORT_CMD_SPIN_UP	= (1 << 1), /* Spin up device */
	PORT_CMD_START		= (1 << 0), /* Enable port DMA engine */

	PORT_CMD_ICC_MASK	= (0xf << 28), /* i/f ICC state mask */
	PORT_CMD_ICC_ACTIVE	= (0x1 << 28), /* Put i/f in active state */
	PORT_CMD_ICC_PARTIAL	= (0x2 << 28), /* Put i/f in partial state */
	PORT_CMD_ICC_SLUMBER	= (0x6 << 28), /* Put i/f in slumber state */

	/* hpriv->flags bits */
	AHCI_HFLAG_NO_NCQ		= (1 << 0),
	AHCI_HFLAG_IGN_IRQ_IF_ERR	= (1 << 1), /* ignore IRQ_IF_ERR */
	AHCI_HFLAG_IGN_SERR_INTERNAL	= (1 << 2), /* ignore SERR_INTERNAL */
	AHCI_HFLAG_32BIT_ONLY		= (1 << 3), /* force 32bit */
	AHCI_HFLAG_MV_PATA		= (1 << 4), /* PATA port */
	AHCI_HFLAG_NO_MSI		= (1 << 5), /* no PCI MSI */
	AHCI_HFLAG_NO_PMP		= (1 << 6), /* no PMP */
	AHCI_HFLAG_NO_HOTPLUG		= (1 << 7), /* ignore PxSERR.DIAG.N */
	AHCI_HFLAG_SECT255		= (1 << 8), /* max 255 sectors */
	AHCI_HFLAG_YES_NCQ		= (1 << 9), /* force NCQ cap on */
	AHCI_HFLAG_NO_SUSPEND		= (1 << 10), /* don't suspend */
	AHCI_HFLAG_SRST_TOUT_IS_OFFLINE	= (1 << 11), /* treat SRST timeout as
							link offline */

	/* ap->flags bits */

	AHCI_FLAG_COMMON		= ATA_FLAG_SATA |
					  ATA_FLAG_PIO_DMA |
					  ATA_FLAG_ACPI_SATA | ATA_FLAG_AN,

	ICH_MAP				= 0x90, /* ICH MAP register */

	/* em constants */
	EM_MAX_SLOTS			= 8,
	EM_MAX_RETRY			= 5,

	/* em_ctl bits */
	EM_CTL_RST			= (1 << 9), /* Reset */
	EM_CTL_TM			= (1 << 8), /* Transmit Message */
	EM_CTL_ALHD			= (1 << 26), /* Activity LED */
};

struct ahci_cmd_hdr {
	__le32			opts;
	__le32			status;
	__le32			tbl_addr;
	__le32			tbl_addr_hi;
	__le32			reserved[4];
};

struct ahci_sg {
	__le32			addr;
	__le32			addr_hi;
	__le32			reserved;
	__le32			flags_size;
};

struct ahci_em_priv {
	enum sw_activity blink_policy;
	struct timer_list timer;
	unsigned long saved_activity;
	unsigned long activity;
	unsigned long led_state;
};

struct ahci_host_priv {
	unsigned int		flags;		/* AHCI_HFLAG_* */
	u32			cap;		/* cap to use */
	u32			cap2;		/* cap2 to use */
	u32			port_map;	/* port map to use */
	u32			saved_cap;	/* saved initial cap */
	u32			saved_cap2;	/* saved initial cap2 */
	u32			saved_port_map;	/* saved initial port_map */
	u32 			em_loc; /* enclosure management location */
         u32                        irq;
	void __iomem               *base;
};

struct ahci_port_priv {
	struct ata_link		*active_link;
	struct ahci_cmd_hdr	*cmd_slot;
	dma_addr_t		cmd_slot_dma;
	void			*cmd_tbl;
	dma_addr_t		cmd_tbl_dma;
	void			*rx_fis;
	dma_addr_t		rx_fis_dma;
	/* for NCQ spurious interrupt analysis */
	unsigned int		ncq_saw_d2h:1;
	unsigned int		ncq_saw_dmas:1;
	unsigned int		ncq_saw_sdb:1;
	u32 			intr_mask;	/* interrupts to enable */
	/* enclosure management info per PM slot */
	struct ahci_em_priv	em_priv[EM_MAX_SLOTS];
};

static int ahci_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val);
static int ahci_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val);
static unsigned int ahci_qc_issue(struct ata_queued_cmd *qc);
static bool ahci_qc_fill_rtf(struct ata_queued_cmd *qc);
static int ahci_port_start(struct ata_port *ap);
static void ahci_port_stop(struct ata_port *ap);
static void ahci_qc_prep(struct ata_queued_cmd *qc);
static void ahci_freeze(struct ata_port *ap);
static void ahci_thaw(struct ata_port *ap);
static void ahci_pmp_attach(struct ata_port *ap);
static void ahci_pmp_detach(struct ata_port *ap);
static int ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static int ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static void ahci_postreset(struct ata_link *link, unsigned int *class);
static void ahci_error_handler(struct ata_port *ap);
static void ahci_post_internal_cmd(struct ata_queued_cmd *qc);
static int ahci_port_resume(struct ata_port *ap);
static void ahci_dev_config(struct ata_device *dev);
static void ahci_fill_cmd_slot(struct ahci_port_priv *pp, unsigned int tag,
			       u32 opts);
#ifdef CONFIG_PM
static int ahci_port_suspend(struct ata_port *ap, pm_message_t mesg);
static int ahci_platform_device_suspend(struct platform_device *pdev, pm_message_t mesg);
static int ahci_platform_device_resume(struct platform_device *pdev);

	#ifdef CONFIG_PM_RUNTIME
static int ahci_platform_device_runtime_idle(struct device *dev);
static int ahci_platform_device_runtime_suspend(struct device *dev);
static int ahci_platform_device_runtime_resume(struct device *dev);
	#endif
	
#endif
static ssize_t ahci_activity_show(struct ata_device *dev, char *buf);
static ssize_t ahci_activity_store(struct ata_device *dev,
				   enum sw_activity val);
static void ahci_init_sw_activity(struct ata_link *link);

static ssize_t ahci_show_host_caps(struct device *dev,
				   struct device_attribute *attr, char *buf);
static ssize_t ahci_show_host_cap2(struct device *dev,
				   struct device_attribute *attr, char *buf);
static ssize_t ahci_show_host_version(struct device *dev,
				      struct device_attribute *attr, char *buf);
static ssize_t ahci_show_port_cmd(struct device *dev,
				  struct device_attribute *attr, char *buf);

DEVICE_ATTR(ahci_host_caps, S_IRUGO, ahci_show_host_caps, NULL);
DEVICE_ATTR(ahci_host_cap2, S_IRUGO, ahci_show_host_cap2, NULL);
DEVICE_ATTR(ahci_host_version, S_IRUGO, ahci_show_host_version, NULL);
DEVICE_ATTR(ahci_port_cmd, S_IRUGO, ahci_show_port_cmd, NULL);

static struct device_attribute *ahci_shost_attrs[] = {
	&dev_attr_link_power_management_policy,
	&dev_attr_em_message_type,
	&dev_attr_em_message,
	&dev_attr_ahci_host_caps,
	&dev_attr_ahci_host_cap2,
	&dev_attr_ahci_host_version,
	&dev_attr_ahci_port_cmd,
	NULL
};

static struct device_attribute *ahci_sdev_attrs[] = {
	&dev_attr_sw_activity,
	&dev_attr_unload_heads,
	NULL
};

static struct scsi_host_template ahci_sht = {
	ATA_NCQ_SHT(DRV_NAME),
	.can_queue		= AHCI_MAX_CMDS - 1,
	.sg_tablesize		= AHCI_MAX_SG,
	.dma_boundary		= AHCI_DMA_BOUNDARY,
	.shost_attrs		= ahci_shost_attrs,
	.sdev_attrs		= ahci_sdev_attrs,
};

static struct ata_port_operations ahci_ops = {
	.inherits		= &sata_pmp_port_ops,

	.qc_defer		= sata_pmp_qc_defer_cmd_switch,
	.qc_prep		= ahci_qc_prep,
	.qc_issue		= ahci_qc_issue,
	.qc_fill_rtf		= ahci_qc_fill_rtf,

	.freeze			= ahci_freeze,
	.thaw			= ahci_thaw,
	.softreset		= ahci_softreset,
	.hardreset		= ahci_hardreset,
	.postreset		= ahci_postreset,
	.pmp_softreset		= ahci_softreset,
	.error_handler		= ahci_error_handler,
	.post_internal_cmd	= ahci_post_internal_cmd,
	.dev_config		= ahci_dev_config,

	.scr_read		= ahci_scr_read,
	.scr_write		= ahci_scr_write,
	.pmp_attach		= ahci_pmp_attach,
	.pmp_detach		= ahci_pmp_detach,

	.set_lpm		= ahci_set_lpm,
	.em_show		= ahci_led_show,
	.em_store		= ahci_led_store,
	.sw_activity_show	= ahci_activity_show,
	.sw_activity_store	= ahci_activity_store,
#ifdef CONFIG_PM
	.port_suspend		= ahci_port_suspend,
	.port_resume		= ahci_port_resume,
#endif
	.port_start		= ahci_port_start,
	.port_stop		= ahci_port_stop,
};

#define AHCI_HFLAGS(flags)	.private_data	= (void *)(flags)

static const struct ata_port_info ahci_port_info[] = {
	[board_ahci] =
	{
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
};

static int ahci_em_messages = 1;
module_param(ahci_em_messages, int, 0444);
/* add other LED protocol types when they become supported */
MODULE_PARM_DESC(ahci_em_messages,
	"Set AHCI Enclosure Management Message type (0 = disabled, 1 = LED");

static inline int ahci_nr_ports(u32 cap)
{
	return (cap & 0x1f) + 1;
}

static inline void __iomem *__ahci_port_base(struct ata_host *host,
					     unsigned int port_no)
{
	void __iomem *mmio = (void __iomem *)host->iomap;

	return mmio + 0x100 + (port_no * 0x80);
}

static inline void __iomem *ahci_port_base(struct ata_port *ap)
{
	return __ahci_port_base(ap->host, ap->port_no);
}

static void ahci_enable_ahci(void __iomem *mmio)
{
	int i;
	u32 tmp;

	/* turn on AHCI_EN */
	tmp = readl(mmio + HOST_CTL);
	if (tmp & HOST_AHCI_EN)
		return;

	/* Some controllers need AHCI_EN to be written multiple times.
	 * Try a few times before giving up.
	 */
	for (i = 0; i < 5; i++) {
		tmp |= HOST_AHCI_EN;
		writel(tmp, mmio + HOST_CTL);
		tmp = readl(mmio + HOST_CTL);	/* flush && sanity check */
		if (tmp & HOST_AHCI_EN)
			return;
		msleep(10);
	}

	WARN_ON(1);
}

static ssize_t ahci_show_host_caps(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;

	return sprintf(buf, "%x\n", hpriv->cap);
}

static ssize_t ahci_show_host_cap2(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;

	return sprintf(buf, "%x\n", hpriv->cap2);
}

static ssize_t ahci_show_host_version(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	void __iomem *mmio = (void __iomem*)ap->host->iomap;

	return sprintf(buf, "%x\n", readl(mmio + HOST_VERSION));
}

static ssize_t ahci_show_port_cmd(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	void __iomem *port_mmio = ahci_port_base(ap);

	return sprintf(buf, "%x\n", readl(port_mmio + PORT_CMD));
}

/**
 *	ahci_save_initial_config - Save and fixup initial config values
 *	@pdev: target PCI device
 *	@hpriv: host private area to store config values
 *
 *	Some registers containing configuration info might be setup by
 *	BIOS and might be cleared on reset.  This function saves the
 *	initial values of those registers into @hpriv such that they
 *	can be restored after controller reset.
 *
 *	If inconsistent, config values are fixed up by this function.
 *
 *	LOCKING:
 *	None.
 */
static void ahci_save_initial_config(struct platform_device *pdev,
				     struct ahci_host_priv *hpriv)
{
	void __iomem *mmio = hpriv->base;
	u32 cap, cap2, vers, port_map;
	int i;

	/* make sure AHCI mode is enabled before accessing CAP */
	ahci_enable_ahci(mmio);

	/* Values prefixed with saved_ are written back to host after
	 * reset.  Values without are used for driver operation.
	 */
	hpriv->saved_cap = cap = readl(mmio + HOST_CAP);
	hpriv->saved_port_map = port_map = readl(mmio + HOST_PORTS_IMPL);

	/* CAP2 register is only defined for AHCI 1.2 and later */
	vers = readl(mmio + HOST_VERSION);
	if ((vers >> 16) > 1 ||
	   ((vers >> 16) == 1 && (vers & 0xFFFF) >= 0x200))
		hpriv->saved_cap2 = cap2 = readl(mmio + HOST_CAP2);
	else
		hpriv->saved_cap2 = cap2 = 0;

	/* some chips have errata preventing 64bit use */
	if ((cap & HOST_CAP_64) && (hpriv->flags & AHCI_HFLAG_32BIT_ONLY)) {
		dev_printk(KERN_INFO, &pdev->dev,
			   "controller can't do 64bit DMA, forcing 32bit\n");
		cap &= ~HOST_CAP_64;
	}

	if ((cap & HOST_CAP_NCQ) && (hpriv->flags & AHCI_HFLAG_NO_NCQ)) {
		dev_printk(KERN_INFO, &pdev->dev,
			   "controller can't do NCQ, turning off CAP_NCQ\n");
		cap &= ~HOST_CAP_NCQ;
	}

	if (!(cap & HOST_CAP_NCQ) && (hpriv->flags & AHCI_HFLAG_YES_NCQ)) {
		dev_printk(KERN_INFO, &pdev->dev,
			   "controller can do NCQ, turning on CAP_NCQ\n");
		cap |= HOST_CAP_NCQ;
	}

	if ((cap & HOST_CAP_PMP) && (hpriv->flags & AHCI_HFLAG_NO_PMP)) {
		dev_printk(KERN_INFO, &pdev->dev,
			   "controller can't do PMP, turning off CAP_PMP\n");
		cap &= ~HOST_CAP_PMP;
	}


	/* cross check port_map and cap.n_ports */
	if (port_map) {
		int map_ports = 0;

		for (i = 0; i < AHCI_MAX_PORTS; i++)
			if (port_map & (1 << i))
				map_ports++;

		/* If PI has more ports than n_ports, whine, clear
		 * port_map and let it be generated from n_ports.
		 */
		if (map_ports > ahci_nr_ports(cap)) {
			dev_printk(KERN_WARNING, &pdev->dev,
				   "implemented port map (0x%x) contains more "
				   "ports than nr_ports (%u), using nr_ports\n",
				   port_map, ahci_nr_ports(cap));
			port_map = 0;
		}
	}

	/* fabricate port_map from cap.nr_ports */
	if (!port_map) {
		port_map = (1 << ahci_nr_ports(cap)) - 1;
		dev_printk(KERN_WARNING, &pdev->dev,
			   "forcing PORTS_IMPL to 0x%x\n", port_map);

		/* write the fixed up value to the PI register */
		hpriv->saved_port_map = port_map;
	}

	/* record values to use during operation */
	hpriv->cap = cap;
	hpriv->cap2 = cap2;
	hpriv->port_map = port_map;
}

/**
 *	ahci_restore_initial_config - Restore initial config
 *	@host: target ATA host
 *
 *	Restore initial config stored by ahci_save_initial_config().
 *
 *	LOCKING:
 *	None.
 */
static void ahci_restore_initial_config(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = (void __iomem*)host->iomap;

	writel(hpriv->saved_cap, mmio + HOST_CAP);
	if (hpriv->saved_cap2)
		writel(hpriv->saved_cap2, mmio + HOST_CAP2);
	writel(hpriv->saved_port_map, mmio + HOST_PORTS_IMPL);
	(void) readl(mmio + HOST_PORTS_IMPL);	/* flush */
}

static unsigned ahci_scr_offset(struct ata_port *ap, unsigned int sc_reg)
{
	static const int offset[] = {
		[SCR_STATUS]		= PORT_SCR_STAT,
		[SCR_CONTROL]		= PORT_SCR_CTL,
		[SCR_ERROR]		= PORT_SCR_ERR,
		[SCR_ACTIVE]		= PORT_SCR_ACT,
		[SCR_NOTIFICATION]	= PORT_SCR_NTF,
	};
	struct ahci_host_priv *hpriv = ap->host->private_data;

	if (sc_reg < ARRAY_SIZE(offset) &&
	    (sc_reg != SCR_NOTIFICATION || (hpriv->cap & HOST_CAP_SNTF)))
		return offset[sc_reg];
	return 0;
}

static int ahci_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	int offset = ahci_scr_offset(link->ap, sc_reg);

	if (offset) {
		*val = readl(port_mmio + offset);
		return 0;
	}
	return -EINVAL;
}

static int ahci_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	int offset = ahci_scr_offset(link->ap, sc_reg);

	if (offset) {
		writel(val, port_mmio + offset);
		return 0;
	}
	return -EINVAL;
}

static void ahci_start_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	/* start DMA */
	tmp = readl(port_mmio + PORT_CMD);

	tmp |= PORT_CMD_START;
	writel(tmp, port_mmio + PORT_CMD);
	readl(port_mmio + PORT_CMD); /* flush */
}

static int ahci_stop_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	tmp = readl(port_mmio + PORT_CMD);

	/* check if the HBA is idle */
	if ((tmp & (PORT_CMD_START | PORT_CMD_LIST_ON)) == 0)
		return 0;

	/* setting HBA to idle */
	tmp &= ~PORT_CMD_START;
	writel(tmp, port_mmio + PORT_CMD);

	/* wait for engine to stop. This could be as long as 500 msec */
	tmp = ata_wait_register(ap, port_mmio + PORT_CMD,
				PORT_CMD_LIST_ON, PORT_CMD_LIST_ON, 1, 500);
	if (tmp & PORT_CMD_LIST_ON)
		return -EIO;

	return 0;
}

static void ahci_start_fis_rx(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	u32 tmp;

	/* set FIS registers */
	if (hpriv->cap & HOST_CAP_64)
		writel((pp->cmd_slot_dma >> 16) >> 16,
		       port_mmio + PORT_LST_ADDR_HI);
	writel(pp->cmd_slot_dma & 0xffffffff, port_mmio + PORT_LST_ADDR);

	if (hpriv->cap & HOST_CAP_64)
		writel((pp->rx_fis_dma >> 16) >> 16,
		       port_mmio + PORT_FIS_ADDR_HI);
	writel(pp->rx_fis_dma & 0xffffffff, port_mmio + PORT_FIS_ADDR);

	/* enable FIS reception */
	tmp = readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_FIS_RX;
	writel(tmp, port_mmio + PORT_CMD);

	/* flush */
	readl(port_mmio + PORT_CMD);
}

static int ahci_stop_fis_rx(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	/* disable FIS reception */
	tmp = readl(port_mmio + PORT_CMD);
	tmp &= ~PORT_CMD_FIS_RX;
	writel(tmp, port_mmio + PORT_CMD);

	/* wait for completion, spec says 500ms, give it 1000 */
	tmp = ata_wait_register(ap, port_mmio + PORT_CMD, PORT_CMD_FIS_ON,
				PORT_CMD_FIS_ON, 10, 1000);
	if (tmp & PORT_CMD_FIS_ON)
		return -EBUSY;

	return 0;
}

static void ahci_power_up(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd;

	cmd = readl(port_mmio + PORT_CMD) & ~PORT_CMD_ICC_MASK;

	/* spin up device */
	if (hpriv->cap & HOST_CAP_SSS) {
		cmd |= PORT_CMD_SPIN_UP;
		writel(cmd, port_mmio + PORT_CMD);
	}

	/* wake up link */
	writel(cmd | PORT_CMD_ICC_ACTIVE, port_mmio + PORT_CMD);
}

static int ahci_set_lpm(struct ata_link *link, enum ata_lpm_policy policy,
			unsigned int hints)
{
	struct ata_port *ap = link->ap;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);

	if (policy != ATA_LPM_MAX_POWER) {
		/*
		 * Disable interrupts on Phy Ready. This keeps us from
		 * getting woken up due to spurious phy ready
		 * interrupts.
		 */
		pp->intr_mask &= ~PORT_IRQ_PHYRDY;
		writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);

		sata_link_scr_lpm(link, policy, false);
	}

	if (hpriv->cap & HOST_CAP_ALPM) {
		u32 cmd = readl(port_mmio + PORT_CMD);

		if (policy == ATA_LPM_MAX_POWER || !(hints & ATA_LPM_HIPM)) {
			cmd &= ~(PORT_CMD_ASP | PORT_CMD_ALPE);
			cmd |= PORT_CMD_ICC_ACTIVE;

			writel(cmd, port_mmio + PORT_CMD);
			readl(port_mmio + PORT_CMD);

			/* wait 10ms to be sure we've come out of LPM state */
			ata_msleep(ap, 10);
		} else {
			cmd |= PORT_CMD_ALPE;
			if (policy == ATA_LPM_MIN_POWER)
				cmd |= PORT_CMD_ASP;

			/* write out new cmd value */
			writel(cmd, port_mmio + PORT_CMD);
		}
	}

	if (policy == ATA_LPM_MAX_POWER) {
		sata_link_scr_lpm(link, policy, false);

		/* turn PHYRDY IRQ back on */
		pp->intr_mask |= PORT_IRQ_PHYRDY;
		writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
	}

	return 0;
}

#ifdef CONFIG_PM
static void ahci_power_down(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd, scontrol;

	if (!(hpriv->cap & HOST_CAP_SSS))
		return;

	/* put device into listen mode, first set PxSCTL.DET to 0 */
	scontrol = readl(port_mmio + PORT_SCR_CTL);
	scontrol &= ~0xf;
	writel(scontrol, port_mmio + PORT_SCR_CTL);

	/* then set PxCMD.SUD to 0 */
	cmd = readl(port_mmio + PORT_CMD) & ~PORT_CMD_ICC_MASK;
	cmd &= ~PORT_CMD_SPIN_UP;
	writel(cmd, port_mmio + PORT_CMD);
}
#endif

static void ahci_start_port(struct ata_port *ap)
{
	struct ahci_port_priv *pp = ap->private_data;
	struct ata_link *link;
	struct ahci_em_priv *emp;
	ssize_t rc;
	int i;

	/* enable FIS reception */
	ahci_start_fis_rx(ap);

	/* enable DMA */
	ahci_start_engine(ap);

	/* turn on LEDs */
	if (ap->flags & ATA_FLAG_EM) {
		ata_for_each_link(link, ap, EDGE) {
			emp = &pp->em_priv[link->pmp];

			/* EM Transmit bit maybe busy during init */
			for (i = 0; i < EM_MAX_RETRY; i++) {
				rc = ahci_transmit_led_message(ap,
							       emp->led_state,
							       4);
				if (rc == -EBUSY)
					msleep(1);
				else
					break;
			}
		}
	}

	if (ap->flags & ATA_FLAG_SW_ACTIVITY)
		ata_for_each_link(link, ap, EDGE)
			ahci_init_sw_activity(link);

}

static int ahci_deinit_port(struct ata_port *ap, const char **emsg)
{
	int rc;

	/* disable DMA */
	rc = ahci_stop_engine(ap);
	if (rc) {
		*emsg = "failed to stop engine";
		return rc;
	}

	/* disable FIS reception */
	rc = ahci_stop_fis_rx(ap);
	if (rc) {
		*emsg = "failed stop FIS RX";
		return rc;
	}

	return 0;
}

static int ahci_reset_controller(struct ata_host *host)
{
	void __iomem *mmio = (void __iomem*)host->iomap;
	u32 tmp;

	/* we must be in AHCI mode, before using anything
	 * AHCI-specific, such as HOST_RESET.
	 */
	ahci_enable_ahci(mmio);

	/* global controller reset */
	if (!ahci_skip_host_reset) {
		tmp = readl(mmio + HOST_CTL);
		if ((tmp & HOST_RESET) == 0) {
			writel(tmp | HOST_RESET, mmio + HOST_CTL);
			readl(mmio + HOST_CTL); /* flush */
		}

		/*
		 * to perform host reset, OS should set HOST_RESET
		 * and poll until this bit is read to be "0".
		 * reset must complete within 1 second, or
		 * the hardware should be considered fried.
		 */
		tmp = ata_wait_register(NULL, mmio + HOST_CTL, HOST_RESET,
					HOST_RESET, 10, 1000);

		if (tmp & HOST_RESET) {
			dev_printk(KERN_ERR, host->dev,
				   "controller reset failed (0x%x)\n", tmp);
			return -EIO;
		}

		/* turn on AHCI mode */
		ahci_enable_ahci(mmio);

		/* Some registers might be cleared on reset.  Restore
		 * initial values.
		 */
		ahci_restore_initial_config(host);
	} else
		dev_printk(KERN_INFO, host->dev,
			   "skipping global host reset\n");

	return 0;
}

static void ahci_sw_activity(struct ata_link *link)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];

	if (!(link->flags & ATA_LFLAG_SW_ACTIVITY))
		return;

	emp->activity++;
	if (!timer_pending(&emp->timer))
		mod_timer(&emp->timer, jiffies + msecs_to_jiffies(10));
}

static void ahci_sw_activity_blink(unsigned long arg)
{
	struct ata_link *link = (struct ata_link *)arg;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];
	unsigned long led_message = emp->led_state;
	u32 activity_led_state;
	unsigned long flags;

	led_message &= EM_MSG_LED_VALUE;
	led_message |= ap->port_no | (link->pmp << 8);

	/* check to see if we've had activity.  If so,
	 * toggle state of LED and reset timer.  If not,
	 * turn LED to desired idle state.
	 */
	spin_lock_irqsave(ap->lock, flags);
	if (emp->saved_activity != emp->activity) {
		emp->saved_activity = emp->activity;
		/* get the current LED state */
		activity_led_state = led_message & EM_MSG_LED_VALUE_ON;

		if (activity_led_state)
			activity_led_state = 0;
		else
			activity_led_state = 1;

		/* clear old state */
		led_message &= ~EM_MSG_LED_VALUE_ACTIVITY;

		/* toggle state */
		led_message |= (activity_led_state << 16);
		mod_timer(&emp->timer, jiffies + msecs_to_jiffies(100));
	} else {
		/* switch to idle */
		led_message &= ~EM_MSG_LED_VALUE_ACTIVITY;
		if (emp->blink_policy == BLINK_OFF)
			led_message |= (1 << 16);
	}
	spin_unlock_irqrestore(ap->lock, flags);
	ahci_transmit_led_message(ap, led_message, 4);
}

static void ahci_init_sw_activity(struct ata_link *link)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];

	/* init activity stats, setup timer */
	emp->saved_activity = emp->activity = 0;
	setup_timer(&emp->timer, ahci_sw_activity_blink, (unsigned long)link);

	/* check our blink policy and set flag for link if it's enabled */
	if (emp->blink_policy)
		link->flags |= ATA_LFLAG_SW_ACTIVITY;
}

static int ahci_reset_em(struct ata_host *host)
{
	void __iomem *mmio = (void __iomem*)host->iomap;
	u32 em_ctl;

	em_ctl = readl(mmio + HOST_EM_CTL);
	if ((em_ctl & EM_CTL_TM) || (em_ctl & EM_CTL_RST))
		return -EINVAL;

	writel(em_ctl | EM_CTL_RST, mmio + HOST_EM_CTL);
	return 0;
}

static ssize_t ahci_transmit_led_message(struct ata_port *ap, u32 state,
					ssize_t size)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *mmio = (void __iomem*)ap->host->iomap;
	u32 em_ctl;
	u32 message[] = {0, 0};
	unsigned long flags;
	int pmp;
	struct ahci_em_priv *emp;

	/* get the slot number from the message */
	pmp = (state & EM_MSG_LED_PMP_SLOT) >> 8;
	if (pmp < EM_MAX_SLOTS)
		emp = &pp->em_priv[pmp];
	else
		return -EINVAL;

	spin_lock_irqsave(ap->lock, flags);

	/*
	 * if we are still busy transmitting a previous message,
	 * do not allow
	 */
	em_ctl = readl(mmio + HOST_EM_CTL);
	if (em_ctl & EM_CTL_TM) {
		spin_unlock_irqrestore(ap->lock, flags);
		return -EBUSY;
	}

	/*
	 * create message header - this is all zero except for
	 * the message size, which is 4 bytes.
	 */
	message[0] |= (4 << 8);

	/* ignore 0:4 of byte zero, fill in port info yourself */
	message[1] = ((state & ~EM_MSG_LED_HBA_PORT) | ap->port_no);

	/* write message to EM_LOC */
	writel(message[0], mmio + hpriv->em_loc);
	writel(message[1], mmio + hpriv->em_loc+4);

	/* save off new led state for port/slot */
	emp->led_state = state;

	/*
	 * tell hardware to transmit the message
	 */
	writel(em_ctl | EM_CTL_TM, mmio + HOST_EM_CTL);

	spin_unlock_irqrestore(ap->lock, flags);
	return size;
}

static ssize_t ahci_led_show(struct ata_port *ap, char *buf)
{
	struct ahci_port_priv *pp = ap->private_data;
	struct ata_link *link;
	struct ahci_em_priv *emp;
	int rc = 0;

	ata_for_each_link(link, ap, EDGE) {
		emp = &pp->em_priv[link->pmp];
		rc += sprintf(buf, "%lx\n", emp->led_state);
	}
	return rc;
}

static ssize_t ahci_led_store(struct ata_port *ap, const char *buf,
				size_t size)
{
	int state;
	int pmp;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp;

	state = simple_strtoul(buf, NULL, 0);

	/* get the slot number from the message */
	pmp = (state & EM_MSG_LED_PMP_SLOT) >> 8;
	if (pmp < EM_MAX_SLOTS)
		emp = &pp->em_priv[pmp];
	else
		return -EINVAL;

	/* mask off the activity bits if we are in sw_activity
	 * mode, user should turn off sw_activity before setting
	 * activity led through em_message
	 */
	if (emp->blink_policy)
		state &= ~EM_MSG_LED_VALUE_ACTIVITY;

	return ahci_transmit_led_message(ap, state, size);
}

static ssize_t ahci_activity_store(struct ata_device *dev, enum sw_activity val)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];
	u32 port_led_state = emp->led_state;

	/* save the desired Activity LED behavior */
	if (val == OFF) {
		/* clear LFLAG */
		link->flags &= ~(ATA_LFLAG_SW_ACTIVITY);

		/* set the LED to OFF */
		port_led_state &= EM_MSG_LED_VALUE_OFF;
		port_led_state |= (ap->port_no | (link->pmp << 8));
		ahci_transmit_led_message(ap, port_led_state, 4);
	} else {
		link->flags |= ATA_LFLAG_SW_ACTIVITY;
		if (val == BLINK_OFF) {
			/* set LED to ON for idle */
			port_led_state &= EM_MSG_LED_VALUE_OFF;
			port_led_state |= (ap->port_no | (link->pmp << 8));
			port_led_state |= EM_MSG_LED_VALUE_ON; /* check this */
			ahci_transmit_led_message(ap, port_led_state, 4);
		}
	}
	emp->blink_policy = val;
	return 0;
}

static ssize_t ahci_activity_show(struct ata_device *dev, char *buf)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];

	/* display the saved value of activity behavior for this
	 * disk.
	 */
	return sprintf(buf, "%d\n", emp->blink_policy);
}

static void ahci_port_init(struct platform_device *pdev, struct ata_port *ap,
			   int port_no, void __iomem *mmio,
			   void __iomem *port_mmio)
{
	const char *emsg = NULL;
	int rc;
	u32 tmp;

	/* make sure port is not active */
	rc = ahci_deinit_port(ap, &emsg);
	if (rc)
		dev_printk(KERN_WARNING, &pdev->dev,
			   "%s (%d)\n", emsg, rc);

	/* clear SError */
	tmp = readl(port_mmio + PORT_SCR_ERR);
	VPRINTK("PORT_SCR_ERR 0x%x\n", tmp);
	writel(tmp, port_mmio + PORT_SCR_ERR);

	/* clear port IRQ */
	tmp = readl(port_mmio + PORT_IRQ_STAT);
	VPRINTK("PORT_IRQ_STAT 0x%x\n", tmp);
	if (tmp)
		writel(tmp, port_mmio + PORT_IRQ_STAT);

	writel(1 << port_no, mmio + HOST_IRQ_STAT);
}

static void ahci_init_controller(struct ata_host *host)
{
	struct platform_device *pdev = to_platform_device(host->dev);
	void __iomem *mmio = (void __iomem*)host->iomap;
	int i;
	void __iomem *port_mmio;
	u32 tmp;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		port_mmio = ahci_port_base(ap);
		if (ata_port_is_dummy(ap))
			continue;

		ahci_port_init(pdev, ap, i, mmio, port_mmio);
	}

	tmp = readl(mmio + HOST_CTL);
	VPRINTK("HOST_CTL 0x%x\n", tmp);
	writel(tmp | HOST_IRQ_EN, mmio + HOST_CTL);
	tmp = readl(mmio + HOST_CTL);
	VPRINTK("HOST_CTL 0x%x\n", tmp);
}

static void ahci_dev_config(struct ata_device *dev)
{
         return ;
}

static unsigned int ahci_dev_classify(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ata_taskfile tf;
	u32 tmp;

	tmp = readl(port_mmio + PORT_SIG);
	tf.lbah		= (tmp >> 24)	& 0xff;
	tf.lbam		= (tmp >> 16)	& 0xff;
	tf.lbal		= (tmp >> 8)	& 0xff;
	tf.nsect	= (tmp)		& 0xff;

	return ata_dev_classify(&tf);
}

static void ahci_fill_cmd_slot(struct ahci_port_priv *pp, unsigned int tag,
			       u32 opts)
{
	dma_addr_t cmd_tbl_dma;
	volatile u32 temp;

	cmd_tbl_dma = pp->cmd_tbl_dma + tag * AHCI_CMD_TBL_SZ;

	pp->cmd_slot[tag].opts = cpu_to_le32(opts);
	pp->cmd_slot[tag].status = 0;
	pp->cmd_slot[tag].tbl_addr = cpu_to_le32(cmd_tbl_dma & 0xffffffff);
	pp->cmd_slot[tag].tbl_addr_hi = cpu_to_le32((cmd_tbl_dma >> 16) >> 16);

	temp = pp->cmd_slot[tag].tbl_addr_hi;
	if( temp != pp->cmd_slot[tag].tbl_addr_hi) {
		printk( " It should not happened! %s:#%d \n", __FILE__, __LINE__);
		while( 1 );
	}

}

static int ahci_kick_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	u8 status = readl(port_mmio + PORT_TFDATA) & 0xFF;
	u32 tmp;
	int busy, rc;

	/* stop engine */
	rc = ahci_stop_engine(ap);
	if (rc)
		goto out_restart;

	/* need to do CLO?
	 * always do CLO if PMP is attached (AHCI-1.3 9.2)
	 */
	busy = status & (ATA_BUSY | ATA_DRQ);
	if (!busy && !sata_pmp_attached(ap)) {
		rc = 0;
		goto out_restart;
	}

	if (!(hpriv->cap & HOST_CAP_CLO)) {
		rc = -EOPNOTSUPP;
		goto out_restart;
	}

	/* perform CLO */
	tmp = readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_CLO;
	writel(tmp, port_mmio + PORT_CMD);

	rc = 0;
	tmp = ata_wait_register(ap, port_mmio + PORT_CMD,
				PORT_CMD_CLO, PORT_CMD_CLO, 1, 500);
	if (tmp & PORT_CMD_CLO)
		rc = -EIO;

	/* restart engine */
 out_restart:
	ahci_start_engine(ap);
	return rc;
}

static int ahci_exec_polled_cmd(struct ata_port *ap, int pmp,
				struct ata_taskfile *tf, int is_cmd, u16 flags,
				unsigned long timeout_msec)
{
	const u32 cmd_fis_len = 5; /* five dwords */
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u8 *fis = pp->cmd_tbl;
	u32 tmp;

	/* prep the command */
	ata_tf_to_fis(tf, pmp, is_cmd, fis);
	ahci_fill_cmd_slot(pp, 0, cmd_fis_len | flags | (pmp << 12));

	/* issue & wait */
	writel(1, port_mmio + PORT_CMD_ISSUE);

	if (timeout_msec) {
		tmp = ata_wait_register(ap, port_mmio + PORT_CMD_ISSUE, 0x1, 0x1,
					1, timeout_msec);
		if (tmp & 0x1) {
			ahci_kick_engine(ap);
			return -EBUSY;
		}
	} else
		readl(port_mmio + PORT_CMD_ISSUE);	/* flush */

	return 0;
}

#define __ELASTIC_BUFFER__

static int ahci_do_softreset(struct ata_link *link, unsigned int *class,
			     int pmp, unsigned long deadline,
			     int (*check_ready)(struct ata_link *link))
{
	struct ata_port *ap = link->ap;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	const char *reason = NULL;
	unsigned long now, msecs;
	struct ata_taskfile tf;
	int rc;

	DPRINTK("ENTER\n");

	/* prepare for SRST (AHCI-1.1 10.4.1) */
	rc = ahci_kick_engine(ap);
	if (rc && rc != -EOPNOTSUPP)
		ata_link_printk(link, KERN_WARNING,
				"failed to reset engine (errno=%d)\n", rc);

#if  defined( __ELASTIC_BUFFER__) 
	writel(0x40002002,
		PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + ap->port_no * 0x4000 + 200 ) ;

	msleep(1);
#endif


	ata_tf_init(link->device, &tf);

	/* issue the first D2H Register FIS */
	msecs = 0;
	now = jiffies;
	if (time_after(now, deadline))
		msecs = jiffies_to_msecs(deadline - now);

	tf.ctl |= ATA_SRST;
	if (ahci_exec_polled_cmd(ap, pmp, &tf, 0,
				 AHCI_CMD_RESET | AHCI_CMD_CLR_BUSY, msecs)) {
		rc = -EIO;
		reason = "1st FIS failed";
		goto fail;
	}

	/* spec says at least 5us, but be generous and sleep for 1ms */
	msleep(1);

	/* issue the second D2H Register FIS */
	tf.ctl &= ~ATA_SRST;
	ahci_exec_polled_cmd(ap, pmp, &tf, 0, 0, 0);

	/* wait for link to become ready */
	rc = ata_wait_after_reset(link, deadline, check_ready);
	if (rc == -EBUSY && hpriv->flags & AHCI_HFLAG_SRST_TOUT_IS_OFFLINE) {
		/*
		 * Workaround for cases where link online status can't
		 * be trusted.  Treat device readiness timeout as link
		 * offline.
		 */
		ata_link_printk(link, KERN_INFO,
				"device not ready, treating as offline\n");
		*class = ATA_DEV_NONE;
	} else if (rc) {
		/* link occupied, -ENODEV too is an error */
		reason = "device not ready";
		goto fail;
	} else
		*class = ahci_dev_classify(ap);

	DPRINTK("EXIT, class=%u\n", *class);
	return 0;

 fail:
	ata_link_printk(link, KERN_ERR, "softreset failed (%s)\n", reason);
	return rc;
}

static int ahci_check_ready(struct ata_link *link)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	u8 status = readl(port_mmio + PORT_TFDATA) & 0xFF;

	return ata_check_ready(status);
}

static int ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	int pmp = sata_srst_pmp(link);

	DPRINTK("ENTER\n");

	return ahci_do_softreset(link, class, pmp, deadline, ahci_check_ready);
}

static int ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	const unsigned long *timing = sata_ehc_deb_timing(&link->eh_context);
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;
	struct ata_taskfile tf;
	bool online;
	int rc;

	DPRINTK("ENTER\n");

	ahci_stop_engine(ap);

	/* clear D2H reception area to properly wait for D2H FIS */
	ata_tf_init(link->device, &tf);
	tf.command = 0x80;
	ata_tf_to_fis(&tf, 0, 0, d2h_fis);

	rc = sata_link_hardreset(link, timing, deadline, &online,
				 ahci_check_ready);

	ahci_start_engine(ap);

	if (online)
		*class = ahci_dev_classify(ap);

	DPRINTK("EXIT, rc=%d, class=%u\n", rc, *class);
	return rc;
}

static void ahci_postreset(struct ata_link *link, unsigned int *class)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 new_tmp, tmp;

	ata_std_postreset(link, class);

	/* Make sure port's ATAPI bit is set appropriately */
	new_tmp = tmp = readl(port_mmio + PORT_CMD);
	if (*class == ATA_DEV_ATAPI)
		new_tmp |= PORT_CMD_ATAPI;
	else
		new_tmp &= ~PORT_CMD_ATAPI;
	if (new_tmp != tmp) {
		writel(new_tmp, port_mmio + PORT_CMD);
		readl(port_mmio + PORT_CMD); /* flush */
	}
}

static unsigned int ahci_fill_sg(struct ata_queued_cmd *qc, void *cmd_tbl)
{
	struct scatterlist *sg;
	struct ahci_sg *ahci_sg = cmd_tbl + AHCI_CMD_TBL_HDR_SZ;
	unsigned int si;

	VPRINTK("ENTER\n");

	/*
	 * Next, the S/G list.
	 */
	for_each_sg(qc->sg, sg, qc->n_elem, si) {
		dma_addr_t addr = sg_dma_address(sg);
		u32 sg_len = sg_dma_len(sg);

		ahci_sg[si].addr = cpu_to_le32(addr & 0xffffffff);
		ahci_sg[si].addr_hi = cpu_to_le32((addr >> 16) >> 16);
		ahci_sg[si].flags_size = cpu_to_le32(sg_len - 1);
	}

	return si;
}


static void ahci_qc_prep(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ahci_port_priv *pp = ap->private_data;
	int is_atapi = ata_is_atapi(qc->tf.protocol);
	void *cmd_tbl;
	u32 opts;
	const u32 cmd_fis_len = 5; /* five dwords */
	unsigned int n_elem;

	/*
	 * Fill in command table information.  First, the header,
	 * a SATA Register - Host to Device command FIS.
	 */
	cmd_tbl = pp->cmd_tbl + qc->tag * AHCI_CMD_TBL_SZ;

	ata_tf_to_fis(&qc->tf, qc->dev->link->pmp, 1, cmd_tbl);
	if (is_atapi) {
		memset(cmd_tbl + AHCI_CMD_TBL_CDB, 0, 32);
		memcpy(cmd_tbl + AHCI_CMD_TBL_CDB, qc->cdb, qc->dev->cdb_len);
	}

	n_elem = 0;
	if (qc->flags & ATA_QCFLAG_DMAMAP) {
		n_elem = ahci_fill_sg(qc, cmd_tbl);
	}

	/*
	 * Fill in command slot information.
	 */
	opts = cmd_fis_len | n_elem << 16 | (qc->dev->link->pmp << 12);
	if (qc->tf.flags & ATA_TFLAG_WRITE)
		opts |= AHCI_CMD_WRITE;
	if (is_atapi)
		opts |= AHCI_CMD_ATAPI | AHCI_CMD_PREFETCH;

	ahci_fill_cmd_slot(pp, qc->tag, opts);
}

static void ahci_error_intr(struct ata_port *ap, u32 irq_stat)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	struct ata_eh_info *host_ehi = &ap->link.eh_info;
	struct ata_link *link = NULL;
	struct ata_queued_cmd *active_qc;
	struct ata_eh_info *active_ehi;
	u32 serror;

	/* determine active link */
	ata_for_each_link(link, ap, EDGE)
		if (ata_link_active(link))
			break;
	if (!link)
		link = &ap->link;

	active_qc = ata_qc_from_tag(ap, link->active_tag);
	active_ehi = &link->eh_info;

	/* record irq stat */
	ata_ehi_clear_desc(host_ehi);
	ata_ehi_push_desc(host_ehi, "irq_stat 0x%08x", irq_stat);

	/* AHCI needs SError cleared; otherwise, it might lock up */
	ahci_scr_read(&ap->link, SCR_ERROR, &serror);
	ahci_scr_write(&ap->link, SCR_ERROR, serror);
	host_ehi->serror |= serror;

	/* some controllers set IRQ_IF_ERR on device errors, ignore it */
	if (hpriv->flags & AHCI_HFLAG_IGN_IRQ_IF_ERR)
		irq_stat &= ~PORT_IRQ_IF_ERR;

	if (irq_stat & PORT_IRQ_TF_ERR) {
		/* If qc is active, charge it; otherwise, the active
		 * link.  There's no active qc on NCQ errors.  It will
		 * be determined by EH by reading log page 10h.
		 */
		if (active_qc)
			active_qc->err_mask |= AC_ERR_DEV;
		else
			active_ehi->err_mask |= AC_ERR_DEV;

		if (hpriv->flags & AHCI_HFLAG_IGN_SERR_INTERNAL)
			host_ehi->serror &= ~SERR_INTERNAL;
	}

	if (irq_stat & PORT_IRQ_UNK_FIS) {
		u32 *unk = (u32 *)(pp->rx_fis + RX_FIS_UNK);

		active_ehi->err_mask |= AC_ERR_HSM;
		active_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(active_ehi,
				  "unknown FIS %08x %08x %08x %08x" ,
				  unk[0], unk[1], unk[2], unk[3]);
	}

	if (sata_pmp_attached(ap) && (irq_stat & PORT_IRQ_BAD_PMP)) {
		active_ehi->err_mask |= AC_ERR_HSM;
		active_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(active_ehi, "incorrect PMP");
	}

	if (irq_stat & (PORT_IRQ_HBUS_ERR | PORT_IRQ_HBUS_DATA_ERR)) {
		host_ehi->err_mask |= AC_ERR_HOST_BUS;
		host_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(host_ehi, "host bus error");
	}

	if (irq_stat & PORT_IRQ_IF_ERR) {
		host_ehi->err_mask |= AC_ERR_ATA_BUS;
		host_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(host_ehi, "interface fatal error");
	}

	if (irq_stat & (PORT_IRQ_CONNECT | PORT_IRQ_PHYRDY)) {

#ifdef __ELASTIC_BUFFER__
		writel(0x4000a002,
			PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + ap->port_no * 0x4000 + 200 ) ;
#endif


		ata_ehi_hotplugged(host_ehi);
		ata_ehi_push_desc(host_ehi, "%s",
			irq_stat & PORT_IRQ_CONNECT ?
			"connection status changed" : "PHY RDY changed");
	}

	/* okay, let's hand over to EH */

	if (irq_stat & PORT_IRQ_FREEZE)
		ata_port_freeze(ap);
	else
		ata_port_abort(ap);
}

static void ahci_port_intr(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ata_eh_info *ehi = &ap->link.eh_info;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	int resetting = !!(ap->pflags & ATA_PFLAG_RESETTING);
	u32 status, qc_active;
	int rc;

	status = readl(port_mmio + PORT_IRQ_STAT);
	writel(status, port_mmio + PORT_IRQ_STAT);

	/* ignore BAD_PMP while resetting */
	if (unlikely(resetting))
		status &= ~PORT_IRQ_BAD_PMP;

	/* If we are getting PhyRdy, this is
 	 * just a power state change, we should
 	 * clear out this, plus the PhyRdy/Comm
 	 * Wake bits from Serror
 	 */
	if ((hpriv->flags & AHCI_HFLAG_NO_HOTPLUG) &&
		(status & PORT_IRQ_PHYRDY)) {
		status &= ~PORT_IRQ_PHYRDY;
		ahci_scr_write(&ap->link, SCR_ERROR, ((1 << 16) | (1 << 18)));
	}

	if (unlikely(status & PORT_IRQ_ERROR)) {
		ahci_error_intr(ap, status);
		return;
	}

	if (status & PORT_IRQ_SDB_FIS) {
		/* If SNotification is available, leave notification
		 * handling to sata_async_notification().  If not,
		 * emulate it by snooping SDB FIS RX area.
		 *
		 * Snooping FIS RX area is probably cheaper than
		 * poking SNotification but some constrollers which
		 * implement SNotification, ICH9 for example, don't
		 * store AN SDB FIS into receive area.
		 */
		if (hpriv->cap & HOST_CAP_SNTF)
			sata_async_notification(ap);
		else {
			/* If the 'N' bit in word 0 of the FIS is set,
			 * we just received asynchronous notification.
			 * Tell libata about it.
			 */
			const __le32 *f = pp->rx_fis + RX_FIS_SDB;
			u32 f0 = le32_to_cpu(f[0]);

			if (f0 & (1 << 15))
				sata_async_notification(ap);
		}
	}

	/* pp->active_link is valid iff any command is in flight */
	if (ap->qc_active && pp->active_link->sactive)
		qc_active = readl(port_mmio + PORT_SCR_ACT);
	else
		qc_active = readl(port_mmio + PORT_CMD_ISSUE);

	rc = ata_qc_complete_multiple(ap, qc_active);

	/* while resetting, invalid completions are expected */
	if (unlikely(rc < 0 && !resetting)) {
		ehi->err_mask |= AC_ERR_HSM;
		ehi->action |= ATA_EH_RESET;
		ata_port_freeze(ap);
	}
}

static irqreturn_t ahci_interrupt(int irq, void *dev_instance)
{
	struct ata_host *host = dev_instance;
	struct ahci_host_priv *hpriv;
	unsigned int i, handled = 0;
	void __iomem *mmio;
	u32 irq_stat, irq_masked;

	VPRINTK("ENTER\n");

	hpriv = host->private_data;
	mmio = (void __iomem*)host->iomap;


	/* sigh.  0xffffffff is a valid return from h/w */
	irq_stat = readl(mmio + HOST_IRQ_STAT);
	if (!irq_stat)
		return IRQ_NONE;

	irq_masked = irq_stat & hpriv->port_map;

	spin_lock(&host->lock);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap;

		if (!(irq_masked & (1 << i)))
			continue;

		ap = host->ports[i];
		if (ap) {
			ahci_port_intr(ap);
			VPRINTK("port %u\n", i);
		} else {
			VPRINTK("port %u (no irq)\n", i);
			if (ata_ratelimit())
				dev_printk(KERN_WARNING, host->dev,
					"interrupt on disabled port %u\n", i);
		}

		handled = 1;
	}

	/* HOST_IRQ_STAT behaves as level triggered latch meaning that
	 * it should be cleared after all the port events are cleared;
	 * otherwise, it will raise a spurious interrupt after each
	 * valid one.  Please read section 10.6.2 of ahci 1.1 for more
	 * information.
	 *
	 * Also, use the unmasked value to clear interrupt as spurious
	 * pending event on a dummy port might cause screaming IRQ.
	 */
	writel(irq_stat, mmio + HOST_IRQ_STAT);

	spin_unlock(&host->lock);

	VPRINTK("EXIT\n");

	return IRQ_RETVAL(handled);
}

static unsigned int ahci_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;

	/* Keep track of the currently active link.  It will be used
	 * in completion path to determine whether NCQ phase is in
	 * progress.
	 */
	pp->active_link = qc->dev->link;

	if (qc->tf.protocol == ATA_PROT_NCQ) {
		writel(1 << qc->tag, port_mmio + PORT_SCR_ACT);
	}
	writel(1 << qc->tag, port_mmio + PORT_CMD_ISSUE);

	ahci_sw_activity(qc->dev->link);

	return 0;
}

static bool ahci_qc_fill_rtf(struct ata_queued_cmd *qc)
{
	struct ahci_port_priv *pp = qc->ap->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;

	ata_tf_from_fis(d2h_fis, &qc->result_tf);
	return true;
}

static void ahci_freeze(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);

	/* turn IRQ off */
	writel(0, port_mmio + PORT_IRQ_MASK);
}

static void ahci_thaw(struct ata_port *ap)
{
	void __iomem *mmio = (void __iomem*)ap->host->iomap;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;
	struct ahci_port_priv *pp = ap->private_data;

	/* clear IRQ */
	tmp = readl(port_mmio + PORT_IRQ_STAT);
	writel(tmp, port_mmio + PORT_IRQ_STAT);
	writel(1 << ap->port_no, mmio + HOST_IRQ_STAT);

	/* turn IRQ back on */
	writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
}

static void ahci_error_handler(struct ata_port *ap)
{
	if (!(ap->pflags & ATA_PFLAG_FROZEN)) {
		/* restart engine */
		ahci_stop_engine(ap);
		ahci_start_engine(ap);
	}

	sata_pmp_error_handler(ap);
}

static void ahci_post_internal_cmd(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;

	/* make DMA engine forget about the failed command */
	if (qc->flags & ATA_QCFLAG_FAILED)
		ahci_kick_engine(ap);
}

static void ahci_pmp_attach(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;
	u32 cmd;

	cmd = readl(port_mmio + PORT_CMD);
	cmd |= PORT_CMD_PMP;
	writel(cmd, port_mmio + PORT_CMD);

	pp->intr_mask |= PORT_IRQ_BAD_PMP;
	writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
}

static void ahci_pmp_detach(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;
	u32 cmd;

	cmd = readl(port_mmio + PORT_CMD);
	cmd &= ~PORT_CMD_PMP;
	writel(cmd, port_mmio + PORT_CMD);

	pp->intr_mask &= ~PORT_IRQ_BAD_PMP;
	writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
}

static int ahci_port_resume(struct ata_port *ap)
{
	ahci_power_up(ap);
	ahci_start_port(ap);

	if (sata_pmp_attached(ap))
		ahci_pmp_attach(ap);
	else
		ahci_pmp_detach(ap);

	return 0;
}

#ifdef CONFIG_PM
static int ahci_port_suspend(struct ata_port *ap, pm_message_t mesg)
{
	const char *emsg = NULL;
	int rc;

	rc = ahci_deinit_port(ap, &emsg);
	if (rc == 0)
		ahci_power_down(ap);
	else {
		ata_port_printk(ap, KERN_ERR, "%s (%d)\n", emsg, rc);
		ahci_start_port(ap);
	}

	return rc;
}

static int ahci_platform_device_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = (void __iomem*)host->iomap;
	u32 ctl;
	int rc;

	if (mesg.event & PM_EVENT_SUSPEND &&
	    hpriv->flags & AHCI_HFLAG_NO_SUSPEND) {
		dev_printk(KERN_ERR, &pdev->dev,
			   "BIOS update required for suspend/resume\n");
		return -EIO;
	}

	if (mesg.event & PM_EVENT_SLEEP) {
		/* AHCI spec rev1.1 section 8.3.3:
		 * Software must disable interrupts prior to requesting a
		 * transition of the HBA to D3 state.
		 */
		ctl = readl(mmio + HOST_CTL);
		ctl &= ~HOST_IRQ_EN;
		writel(ctl, mmio + HOST_CTL);
		readl(mmio + HOST_CTL); /* flush */
	}

	rc = ata_host_suspend(host, mesg);
	if (rc)
		return rc;
         return 0;

}

static int ahci_platform_device_resume(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;


//	if (pdev->dev.power.power_state.event == PM_EVENT_SUSPEND) {
		rc = ahci_reset_controller(host);
		if (rc)
			return rc;

		ahci_init_controller(host);
//	}

	ata_host_resume(host);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int ahci_platform_device_runtime_suspend(struct device *dev)
{
	u32 reg_v;
	int i;
	char *ptr;
	unsigned char *addr;

	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("%s: no SB_PHY found !!!\r\n", __func__);
		return 0;
	}
	ptr += strlen("SB_PHY") + 1;

	for (i = 0; i < 4; i++) {
		if (ptr[i] == 'S') {
			//printk("runtime suspend port:%x\n",i);
			addr = ahci_base + 0x100 + 0x80*i;

			/*
			 * set to slumber mode
			 */
			reg_v = readl(addr + 0x2C);
			writel(reg_v & 0xFFFFFCFF,addr + 0x2C);

			reg_v = readl(addr + 0x14);
			writel(reg_v & 0xffbfffff,addr + 0x14);

			writel(0, addr + 0x2C);

			reg_v = readl(addr + 0x18);
			reg_v &= 0x8fffffff;
			reg_v |= 0x60000000;
			writel(reg_v, addr + 0x18);
		}
	}


	 return 0;
}

static int ahci_platform_device_runtime_resume(struct device *dev)
{
	u32 reg_v;
	int i;
	char *ptr;
	unsigned char *addr;

	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("%s: no SB_PHY found !!!\r\n", __func__);
		return 0;
	}
	ptr += strlen("SB_PHY") + 1;

	for (i = 0; i < 4; i++) {
		if (ptr[i] == 'S') {
			//printk("runtime resume port:%x\n",i);
			addr = ahci_base + 0x100 + 0x80*i;

			/*
			 * set to active
			 */
			reg_v = readl(addr + 0x18);
			reg_v &= 0x8fffffff;
			reg_v |= 0x10000000;

			writel(reg_v,addr + 0x18);
			//writel(((reg_v & 0x8fffffff) | 0x10000000),ahci_base + 0x298);
			msleep(300);

			writel(0x300, addr + 0x2C);
			writel(0xFFFFFFFF, addr + 0x30);
			writel(0xFFFFFFFF, addr + 0x10);

			reg_v = readl(addr + 0x14);
			writel(reg_v & 0xffbfffff,addr + 0x14);
		}
	}

	//writel(0, ahci_base + 0x2AC);


	return 0;
}

static int ahci_platform_device_runtime_idle(struct device *dev)
{
	u32 reg_v=0;

#if 0
	/*
	 * set to partial mode
	 */
	reg_v = readl(ahci_base + 0x2AC);
	writel(reg_v & 0xFFFFFCFF,ahci_base + 0x2AC);

	reg_v = readl(ahci_base + 0x294);
	writel(reg_v & 0xffbfffff,ahci_base + 0x294);

	writel(0, ahci_base + 0x2AC);

	reg_v = readl(ahci_base + 0x298);
	printk("reg_v - %x \n",reg_v);

	reg_v &= 0x8fffffff;
	printk("reg_v - %x \n",reg_v);

	reg_v |= 0x20000000;
	printk("reg_v - %x \n",reg_v);

	writel(reg_v,ahci_base + 0x298);

	//writel(((reg_v & 0x8fffffff)| 0x20000000),ahci_base + 0x298);

	printk("%s : 298 - %x\n",__func__,reg_v);

	int i;

	for(i=0;i<20;i++)
	{
		if (i%4==0)
			printk("0x%x : ",(ahci_base + 0x280+i*4));
		reg_v = readl(ahci_base + 0x280+i*4);
		printk("  %08x  ",reg_v);

		if (i%4==3)
			printk("\n");

	}
	printk("\n");
#endif


    return pm_runtime_suspend(dev);

}

#include <mach/cs_pwrmgt.h>

#define PM_MAX_PCIE_SATA_PORT	4
extern int pm_phy_mode; /* variable defined in kernel/power/main.c */
extern cs_status_t cs_pm_sata_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  sata_id,
CS_IN cs_pm_state_t  state);

static int ahci_platform_device_suspend_noirq(struct device *dev)
{
	unsigned int	i;

	for (i = 0; i < PM_MAX_PCIE_SATA_PORT; i++) {
		if (pm_phy_mode & (1 << (i+8)))
			cs_pm_sata_state_set(0,i,CS_PM_STATE_POWER_DOWN);
	}
	
	return 0;	
}

static int ahci_platform_device_resume_noirq(struct device *dev)
{
	unsigned int	i;

	for (i = 0; i < PM_MAX_PCIE_SATA_PORT; i++) {
		if (pm_phy_mode & (1 << (i+8)))
			cs_pm_sata_state_set(0,i,CS_PM_STATE_NORMAL);
	}

	return 0;	
}

#endif //CONFIG_PM_RUNTIME

#endif

static int ahci_port_start(struct ata_port *ap)
{
	struct device *dev = ap->host->dev;
	struct ahci_port_priv *pp;
	void *mem;
	dma_addr_t mem_dma;

	pp = devm_kzalloc(dev, sizeof(*pp), GFP_KERNEL);
	if (!pp)
		return -ENOMEM;

#ifdef CONFIG_CS752X_PROC
	if( cs_acp_enable & CS75XX_ACP_ENABLE_AHCI){
		int i = 0;
		mem = kmalloc(AHCI_PORT_PRIV_DMA_SZ, GFP_KERNEL);
		while((int)mem & 0x3ff){
			kfree(mem);
			mem = kmalloc(AHCI_PORT_PRIV_DMA_SZ, GFP_KERNEL);
			if( i++ > 1000 ){
				cs_acp_enable &= ~CS75XX_ACP_ENABLE_AHCI;
				kfree(mem);
				mem = dmam_alloc_coherent(dev, AHCI_PORT_PRIV_DMA_SZ, &mem_dma,
				  GFP_KERNEL);
				goto Align_fail;
			}
		}

		mem_dma = virt_to_phys(mem)|GOLDENGATE_ACP_BASE;
	}
	else {
#endif
		mem = dmam_alloc_coherent(dev, AHCI_PORT_PRIV_DMA_SZ, &mem_dma,
				  GFP_KERNEL);
#ifdef CONFIG_CS752X_PROC
	}
Align_fail:
#endif

	if (!mem)
		return -ENOMEM;
	memset(mem, 0, AHCI_PORT_PRIV_DMA_SZ);

	/*
	 * First item in chunk of DMA memory: 32-slot command table,
	 * 32 bytes each in size
	 */
	pp->cmd_slot = mem;
	pp->cmd_slot_dma = mem_dma;

	mem += AHCI_CMD_SLOT_SZ;
	mem_dma += AHCI_CMD_SLOT_SZ;

	/*
	 * Second item: Received-FIS area
	 */
	pp->rx_fis = mem;
	pp->rx_fis_dma = mem_dma;

	mem += AHCI_RX_FIS_SZ;
	mem_dma += AHCI_RX_FIS_SZ;

	/*
	 * Third item: data area for storing a single command
	 * and its scatter-gather table
	 */
	pp->cmd_tbl = mem;
	pp->cmd_tbl_dma = mem_dma;

	/*
	 * Save off initial list of interrupts to be enabled.
	 * This could be changed later
	 */
	pp->intr_mask = DEF_PORT_IRQ;

	ap->private_data = pp;

	/* engage engines, captain */
	return ahci_port_resume(ap);
}

static void ahci_port_stop(struct ata_port *ap)
{
	const char *emsg = NULL;
	int rc;

	/* de-initialize port */
	rc = ahci_deinit_port(ap, &emsg);
	if (rc)
		ata_port_printk(ap, KERN_WARNING, "%s (%d)\n", emsg, rc);
}

static int ahci_configure_dma_masks(struct platform_device *pdev, int using_dac)
{
/*
	int rc;

	if (using_dac &&
	    !pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (rc) {
			rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
			if (rc) {
				dev_printk(KERN_ERR, &pdev->dev,
					   "64-bit DMA enable failed\n");
				return rc;
			}
		}
	} else {
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc) {
			dev_printk(KERN_ERR, &pdev->dev,
				   "32-bit DMA enable failed\n");
			return rc;
		}
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc) {
			dev_printk(KERN_ERR, &pdev->dev,
				   "32-bit consistent DMA enable failed\n");
			return rc;
		}
	}
*/	return 0;
}

static void ahci_print_info(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	struct platform_device *pdev = to_platform_device(host->dev);
	void __iomem *mmio = (void __iomem*)host->iomap;
	u32 vers, cap, cap2, impl, speed;
	const char *speed_s;

	vers = readl(mmio + HOST_VERSION);
	cap = hpriv->cap;
	cap2 = hpriv->cap2;
	impl = hpriv->port_map;

	speed = (cap >> 20) & 0xf;
	if (speed == 1)
		speed_s = "1.5";
	else if (speed == 2)
		speed_s = "3";
	else if (speed == 3)
		speed_s = "6";
	else
		speed_s = "?";

	dev_printk(KERN_INFO, &pdev->dev,
		"AHCI %02x%02x.%02x%02x "
		"%u slots %u ports %s Gbps 0x%x impl SATA mode\n"
		,

		(vers >> 24) & 0xff,
		(vers >> 16) & 0xff,
		(vers >> 8) & 0xff,
		vers & 0xff,

		((cap >> 8) & 0x1f) + 1,
		(cap & 0x1f) + 1,
		speed_s,
		impl);

	dev_printk(KERN_INFO, &pdev->dev,
		"flags: "
		"%s%s%s%s%s%s%s"
		"%s%s%s%s%s%s%s"
		"%s%s%s%s%s%s\n"
		,

		cap & HOST_CAP_64 ? "64bit " : "",
		cap & HOST_CAP_NCQ ? "ncq " : "",
		cap & HOST_CAP_SNTF ? "sntf " : "",
		cap & HOST_CAP_MPS ? "ilck " : "",
		cap & HOST_CAP_SSS ? "stag " : "",
		cap & HOST_CAP_ALPM ? "pm " : "",
		cap & HOST_CAP_LED ? "led " : "",
		cap & HOST_CAP_CLO ? "clo " : "",
		cap & HOST_CAP_ONLY ? "only " : "",
		cap & HOST_CAP_PMP ? "pmp " : "",
		cap & HOST_CAP_FBS ? "fbs " : "",
		cap & HOST_CAP_PIO_MULTI ? "pio " : "",
		cap & HOST_CAP_SSC ? "slum " : "",
		cap & HOST_CAP_PART ? "part " : "",
		cap & HOST_CAP_CCC ? "ccc " : "",
		cap & HOST_CAP_EMS ? "ems " : "",
		cap & HOST_CAP_SXS ? "sxs " : "",
		cap2 & HOST_CAP2_APST ? "apst " : "",
		cap2 & HOST_CAP2_NVMHCI ? "nvmp " : "",
		cap2 & HOST_CAP2_BOH ? "boh " : ""
		);
}


#ifdef CONFIG_CORTINA_FPGA
static int ahci_phy_initial( void __iomem *mmio)
{
	GLOBAL_PHY_CONTROL_t control;

	control.wrd= readl( GLOBAL_PHY_CONTROL);
	control.bf.phy_3_por_n_i  = 1;
	control.bf.phy_3_ln0_resetn_i  = 1;
	control.bf.phy_3_cmu_resetn_i  = 1;
	control.bf.phy_2_por_n_i= 1;
	control.bf.phy_2_ln0_resetn_i  = 1;
	control.bf.phy_2_cmu_resetn_i  = 1;
	writel( control.wrd , GLOBAL_PHY_CONTROL);

	// GLOBAL_GLOBAL_CONFIG : select between SATA/PCIe
	writel(  0xea00, 0xf0000010);

	// PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1
	writel(  0x8000027a, 0xf00a0d00 ) ;

	// TESTR enable OOBR write first
	writel( 0x00020000, mmio + 0xf4 );

	// OOBR overwritten with 75MHz OOB detection parameter
	writel(  0x80000000, mmio + 0xbc );
	writel(  0x060a111e, mmio + 0xbc);
	writel(  0x00000000, mmio + 0xf4);

	// PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1
	writel(  0x8000027a, 0xf00a0d80) ;

	// TESTR enable OOBR write first
	writel( 0x00030000, mmio + 0xf4 );

	// OOBR overwritten with 75MHz OOB detection parameter
	writel(  0x80000000, mmio + 0xbc );
	writel(  0x060a111e, mmio + 0xbc );
	writel(  0x00000000, mmio + 0xf4 );

	// G.CAP
	// stagger spin-up enable
	writel(  0x6726ff83, mmio);

	// G.PI   P2 P3 implemented only
	writel(  0xc, mmio + 0x0c );

	return 0;
}

#else

struct PhyConfig
{
	u16 offset;
	u32 value;
};

//#define _SAMSUNG_SUGGESTION_2_
//#define _SAMSUNG_SUGGESTION_3_
//#define _SAMSUNG_SUGGESTION_4_
//#define _SAMSUNG_SUGGESTION_5_
//#define _SAMSUNG_SUGGESTION_6_
//#define _SAMSUNG_SUGGESTION_6_1_

static sb_phy_program( u32 port, u32 external_clock)
{
	static const struct PhyConfig snowbush_24_setting[] = {
		{0, 0x00890006},
		{4, 0x0003c97b},
		{32, 0x54a00000},
		{44, 0x50040000},
		{48, 0x40250270},
		{52, 0x00004001},
#ifdef _SAMSUNG_SUGGESTION_2_
		{96, 0x5e082e00},
#else
		{96, 0x5e002e00},
#endif
		{100, 0xf0d14200},
		{104, 0xb92c7828},
		{108, 0x0000035e},
		{120, 0x00000002},
		{124, 0x04841000},
		{128, 0x000000e0},
#if	defined( _SAMSUNG_SUGGESTION_2_)
		{132, 0x05400023},
#elif 	defined( _SAMSUNG_SUGGESTION_5_ )
		{132, 0x07400003},
#elif 	defined( _SAMSUNG_SUGGESTION_6_ )
		{132, 0x06400003},
#elif 	defined( _SAMSUNG_SUGGESTION_6_1_ )
		{132, 0x06400043},
#else
		{132, 0x07400023},
#endif
		{136, 0x680017d0},
		{140, 0x0d181ef2},
#ifdef _SAMSUNG_SUGGESTION_5_
		{144, 0x0400000c},
#else
		{144, 0x0000000c},
#endif
		{196, 0x0f600000},
#if	defined( __ELASTIC_BUFFER__ )
		{200, 0x4000a000},
#else
		{200, 0x40002000},
#endif
		{204, 0x4919ae24},
		{208, 0xc54b8304},
#if  defined( _SAMSUNG_SUGGESTION_4_)
		{212, 0xf8280301},
#elif  defined( _SAMSUNG_SUGGESTION_6_) || defined( _SAMSUNG_SUGGESTION_6_1_)
		{212, 0xf8280301},
#else
		{212, 0x98280301},
#endif

#if	defined( __ELASTIC_BUFFER__ )
		{216, 0x00000019},
		{220, 0x0000d0f2},
#else
		{216, 0x80000019},
		{220, 0x0000d0f0},
#endif
		{232, 0xa0a0a000},
		{236, 0xa0a0a0a0},
		{240, 0x58800054},
		{244, 0x865c4400},
		{248, 0x9009d08d},
		{252, 0x00004007},
		{256, 0x00322000},
		{300, 0xd8000000},
		{304, 0x0011ff1a},
		{308, 0xf0000000},
		{312, 0xffffffff},
#if defined( _SAMSUNG_SUGGESTION_5_)
		{316, 0x3fc3c210},
#elif defined( _SAMSUNG_SUGGESTION_6_)
		{316, 0x3fc3c210},
#elif defined( _SAMSUNG_SUGGESTION_6_1_ )
		{316, 0x3fc3c21C},
#else
		{316, 0x3fc3c21c},
#endif
		{320, 0x0000000a},
		{324, 0x00f80000},
		{0, 0x00890007},
	};

	static const struct PhyConfig snowbush_60_setting[] = {
		{ 0   ,0x00810006},
		{ 32  ,0x64a00000},
		{ 44  ,0x50040000},
		{ 48  ,0x40250270},
		{ 52  ,0x00004001},
		{ 96  ,0x5e002e00},
		{ 100 ,0x20d14200},
		{ 104 ,0xce447828},
		{ 108 ,0x0000000b},
		{ 120 ,0x00000002},
		{ 124 ,0x04841000},
		{ 128 ,0x000000e0},
		{ 132 ,0x05000023},
		{ 136 ,0x68000438},
		{ 140 ,0x0d181ef2},
		{ 144 ,0x0000000d},
		{ 196 ,0x0f600000},
		{ 200 ,0x40002000},
		{ 204 ,0x4919ae24},
		{ 208 ,0xc6482304},
		{ 212 ,0x98280301},
		{ 216 ,0x80000019},
		{ 220 ,0x0000d0f0},
		{ 232 ,0xa0a0a000},
		{ 236 ,0xa0a0a0a0},
		{ 240 ,0x58800064},
		{ 244 ,0x865c4400},
		{ 248 ,0x9009d08d},
		{ 252 ,0x00004007},
		{ 256 ,0x00322000},
		{ 300 ,0xd8000000},
		{ 304 ,0x0011ff1a},
		{ 308 ,0xf0000000},
		{ 312 ,0xffffffff},
		{ 316 ,0x3fc3c21c},
		{ 320 ,0x0000000a},
		{ 324 ,0x00f80000},
		{ 0,   0x00810007},
	};

	const struct PhyConfig *p;
	const struct PhyConfig *endp;

	if( external_clock) {
		p = snowbush_60_setting;
		endp = &snowbush_60_setting[ ARRAY_SIZE(snowbush_60_setting)];
	} else {
		p = snowbush_24_setting;
		endp = &snowbush_24_setting[ ARRAY_SIZE(snowbush_24_setting)];
	}


	for (; p != endp; ++p) {
		writel(p->value, SNOW_PHY_BASE_ADDR + port * 0x4000 + p->offset);
	}
}

#define AHCI_CAP                0x00
#define AHCI_CTL                0x04
#define AHCI_PI                 0x0C
#define AHCI_TESTER 		0xf4
#define AHCI_OOBR   		0xBC
#define AHCI_BISTCR		0xA4


static int ahci_phy_initial_port( u32 port, u32 external_clock)
{
	int i, sb_clock_ref;
	GLOBAL_PHY_CONTROL_t phy_control;
	GLOBAL_GLOBAL_CONFIG_t global_config;
	GLOBAL_BLOCK_RESET_t block_reset;
	PCIE_SATA_SATA2_PHY_SATA_CORE_PHY_STAT_t phy_stat;


	/* step 0 */
	phy_control.wrd = readl(GLOBAL_PHY_CONTROL);
	switch (port) {
	case 0:
		phy_control.bf.phy_0_por_n_i = 0;
		phy_control.bf.phy_0_pd = 1;
		phy_control.bf.phy_0_cmu_resetn_i = 0;
		phy_control.bf.phy_0_ln0_resetn_i = 0;
		break;
	case 1:
		phy_control.bf.phy_1_por_n_i = 0;
		phy_control.bf.phy_1_pd = 1;
		phy_control.bf.phy_1_cmu_resetn_i = 0;
		phy_control.bf.phy_1_ln0_resetn_i = 0;
		break;
	case 2:
		phy_control.bf.phy_2_por_n_i = 0;
		phy_control.bf.phy_2_pd = 1;
		phy_control.bf.phy_2_cmu_resetn_i = 0;
		phy_control.bf.phy_2_ln0_resetn_i = 0;
		break;
	case 3:
		phy_control.bf.phy_3_por_n_i = 0;
		phy_control.bf.phy_3_pd = 1;
		phy_control.bf.phy_3_cmu_resetn_i = 0;
		phy_control.bf.phy_3_ln0_resetn_i = 0;
		break;
	}

	writel(phy_control.wrd, GLOBAL_PHY_CONTROL);
	mdelay(100);

	if( external_clock ) {
		sb_clock_ref = 1;
	} else {
		sb_clock_ref = 2;
	}

	/* step1 :Release the power on reset by programming por_n_i to high */
	phy_control.wrd = readl(GLOBAL_PHY_CONTROL);
	switch (port) {
	case 0:
		phy_control.bf.phy_0_por_n_i = 1;
		phy_control.bf.phy_0_pd = 0;
		phy_control.bf.phy_0_refclksel = sb_clock_ref;
		break;
	case 1:
		phy_control.bf.phy_1_por_n_i = 1;
		phy_control.bf.phy_1_pd = 0;
		phy_control.bf.phy_1_refclksel = sb_clock_ref;
		break;
	case 2:
		phy_control.bf.phy_2_por_n_i = 1;
		phy_control.bf.phy_2_pd = 0;
		phy_control.bf.phy_2_refclksel = sb_clock_ref;
		break;
	case 3:
		phy_control.bf.phy_3_por_n_i = 1;
		phy_control.bf.phy_3_pd = 0;
		phy_control.bf.phy_3_refclksel = sb_clock_ref;
		break;
	}

	writel(phy_control.wrd, GLOBAL_PHY_CONTROL);
	mdelay(1);

	/* step 2: Program the Snowbush PHY registers */
	sb_phy_program(port, external_clock);


	/* step 5: Global_CONFIG */
	if (port < 3) {
		global_config.wrd = readl(GLOBAL_GLOBAL_CONFIG);

		switch (port) {
		case 0:
			global_config.bf.cfg_sata_0_clken = 1;
			global_config.bf.cfg_pcie_0_clken = 0;
			break;
		case 1:
			global_config.bf.cfg_sata_1_clken = 1;
			global_config.bf.cfg_pcie_1_clken = 0;
			break;
		case 2:
			global_config.bf.cfg_sata_2_clken = 1;
			global_config.bf.cfg_pcie_2_clken = 0;
			break;
		}

		writel(global_config.wrd, GLOBAL_GLOBAL_CONFIG);
	}

	/* step 6:  After completing the register programming (3) release
	   the cmu reset */

	phy_control.wrd = readl(GLOBAL_PHY_CONTROL);

	switch (port) {
	case 0:
		phy_control.bf.phy_0_cmu_resetn_i = 1;
		break;
	case 1:
		phy_control.bf.phy_1_cmu_resetn_i = 1;
		break;
	case 2:
		phy_control.bf.phy_2_cmu_resetn_i = 1;
		break;
	case 3:
		phy_control.bf.phy_3_cmu_resetn_i = 1;
		break;
	}

	writel(phy_control.wrd, GLOBAL_PHY_CONTROL);

	/* step7: Wait for CMU OK to be set by reading the following register */

	for (i = 0; i < 1000; i++) {
		phy_stat.wrd =
		    readl(PCIE_SATA_SATA2_PHY_SATA_CORE_PHY_STAT + port * 0x80);
		if (phy_stat.bf.cmu_ok == 1) {
			break;
		}

		udelay(1000);
	}
	if ( i >= 1000 ) {
		printk( "SATA PHY initialization, CMU failed\n" );
	}

	/* step8: Release the lane reset */
#ifdef __ELASTIC_BUFFER__
	writel(0x4000a002,
		PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + port * 0x4000 + 200 ) ;
#else
	writel(0x40002002,
		PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + port * 0x4000 + 200 ) ;
#endif

	phy_control.wrd = readl(GLOBAL_PHY_CONTROL);

	switch (port) {
	case 0:
		phy_control.bf.phy_0_ln0_resetn_i = 1;
		break;
	case 1:
		phy_control.bf.phy_1_ln0_resetn_i = 1;
		break;
	case 2:
		phy_control.bf.phy_2_ln0_resetn_i = 1;
		break;
	case 3:
		phy_control.bf.phy_3_ln0_resetn_i = 1;
		break;
	}
	writel(phy_control.wrd, GLOBAL_PHY_CONTROL);

	/* step9 : Wait for lane OK signal of PHY to be set */
	for (i = 0; i < 1000; i++) {
		phy_stat.wrd =
		    readl(PCIE_SATA_SATA2_PHY_SATA_CORE_PHY_STAT + port * 0x80);
		if (phy_stat.bf.lane_ok == 1) {
			break;
		}

		udelay(1000);
	}

	if ( i >= 1000 ) {
		printk( "SATA PHY initialization, lan_ok failed: port=%d\n", port );
	}
}

#ifdef CONFIG_AHCI_EXTERNAL
extern  int si5338_init(void);
#endif

static int ahci_phy_initial( void __iomem *mmio)
{
	int i;
	char *ptr, *sb_extern_ptr;
	u32 count;
	u32 tmp;
	u32 port_implement;


	GLOBAL_BLOCK_RESET_t block_reset;
	GLOBAL_STRAP_t global_strap;

	PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1_t sata_ctrl;

	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("%s: no SB_PHY found !!!\r\n", __func__);
		return -EINVAL;
	}

	sb_extern_ptr = strstr(saved_command_line, "SB_EXTERN");

#ifdef CONFIG_AHCI_EXTERNAL
	if( sb_extern_ptr ) {
		si5338_init();
		msleep(1000);
	}
#endif

	ptr += strlen("SB_PHY") + 1;

	count = 0;
	port_implement = 0;

	for (i = 0; i < 4; i++) {
		if (ptr[i] == 'S') {
			ahci_phy_initial_port(i, sb_extern_ptr );

			port_implement |= (0x01 << i);
			msleep(200);	/* TODO */
			++count;
		}
	}

	if (count == 0) {
		return 1;
	}

	block_reset.wrd = readl(GLOBAL_BLOCK_RESET);
	block_reset.bf.reset_phy = 1;
	writel(block_reset.wrd, GLOBAL_BLOCK_RESET);

	msleep(1);

	block_reset.wrd = readl(GLOBAL_BLOCK_RESET);
	block_reset.bf.reset_phy = 0;
	writel(block_reset.wrd, GLOBAL_BLOCK_RESET);



	for (i = 0; i < 4; i++) {
		if (ptr[i] == 'S') {

			sata_ctrl.wrd =
			    readl(PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1 +
				  i * 0x80);

#ifdef _SAMSUNG_SUGGESTION_3_
			sata_ctrl.bf.phy_rx_data_vld_mctrl = 0;
#else
			sata_ctrl.bf.phy_rx_data_vld_mctrl = 1;
#endif
			sata_ctrl.bf.ctrl_spd_mode = 1;
			sata_ctrl.bf.ctrl_calibrated = 1;
			writel(sata_ctrl.wrd,
			       PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1 + i * 0x80);

			sata_ctrl.wrd =
			    readl(PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1 +
				  i * 0x80);
			mb();
		}
	}

	/* G.CAP , set it capability */
	writel(0x6f26ff80 | (count - 1), mmio);

	/* G.PI */
	writel(port_implement, mmio + AHCI_PI);

	global_strap.wrd = readl(GLOBAL_STRAP);
	switch (global_strap.bf.speed) {
	case 0:		/* 133 MHZ */
		tmp = 0x0A132039;
		break;
	case 2:		/* 140 MHZ */
		tmp = 0x0A14223C;
		break;
	case 1:
	case 4:
	case 5:		/* 150 MHZ */
		tmp = 0x0B152540;
		break;
	case 3:		/* 160 MHZ */
		tmp = 0x0B172744;
		break;
	case 6:		/* 141.67 MHZ */
		tmp = 0x0A14223C;
		break;
	default:
		printk("Unknown frequency!");
		return 1;
	}

	for (i = 0; i < 4; i++) {
		if (ptr[i] == 'S') {
			/* TESTR enable OOBR write first */
			writel(i << 16, mmio + AHCI_TESTER);

			writel(0x80000000, mmio + AHCI_OOBR);
			writel(tmp, mmio + AHCI_OOBR);

			writel(0x00000000, mmio + AHCI_TESTER);
		}
	}

	writel(0x00003700, mmio + AHCI_BISTCR);
	return 0;
}

#endif
static int ahci_platform_probe(struct platform_device *pdev)
{
	static int printed_version;
	struct ata_port_info pi = ahci_port_info[0];
	const struct ata_port_info *ppi[] = { &pi, NULL };
	struct device *dev = &pdev->dev;
	struct ahci_host_priv *hpriv;
	struct ata_host *host;
	struct resource *res;
	int n_ports, i, rc;

	VPRINTK("ENTER\n");

	WARN_ON(ATA_MAX_QUEUE > AHCI_MAX_CMDS);

	if (!printed_version++)
		dev_printk(KERN_DEBUG, &pdev->dev, "version " DRV_VERSION "\n");

	/*
	 * Simple resource validation ..
	 */
	if (unlikely(pdev->num_resources != 2)) {
		dev_err(&pdev->dev, "invalid number of resources\n");
		return -EINVAL;
	}

	/*
	 * Get the register base first
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;

	/* AHCI controllers often implement SFF compatible interface.
	 * Grab all PCI BARs just in case.
	 */
	hpriv = devm_kzalloc(dev, sizeof(*hpriv), GFP_KERNEL);
	if (!hpriv)
		return -ENOMEM;

	hpriv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	hpriv->irq = platform_get_irq(pdev, 0);

	if (ahci_phy_initial(hpriv->base)) {
		devm_iounmap(&pdev->dev, hpriv->base);
		devm_kfree(dev, hpriv);
		return -EINVAL;
	}

#ifdef CONFIG_PM_RUNTIME
	ahci_base = hpriv->base;
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
#endif

	/* save initial config */
	ahci_save_initial_config(pdev, hpriv);

	/* prepare host */
	if (hpriv->cap & HOST_CAP_NCQ)
		pi.flags |= ATA_FLAG_NCQ | ATA_FLAG_FPDMA_AA;

	if (hpriv->cap & HOST_CAP_PMP)
		pi.flags |= ATA_FLAG_PMP;

	if (ahci_em_messages && (hpriv->cap & HOST_CAP_EMS)) {
		u8 messages;
		void __iomem *mmio = hpriv->base;
		u32 em_loc = readl(mmio + HOST_EM_LOC);
		u32 em_ctl = readl(mmio + HOST_EM_CTL);

		messages = (em_ctl & EM_CTRL_MSG_TYPE) >> 16;

		/* we only support LED message type right now */
		if ((messages & 0x01) && (ahci_em_messages == 1)) {
			/* store em_loc */
			hpriv->em_loc = ((em_loc >> 16) * 4);
			pi.flags |= ATA_FLAG_EM;
			if (!(em_ctl & EM_CTL_ALHD))
				pi.flags |= ATA_FLAG_SW_ACTIVITY;
		}
	}

	/* CAP.NP sometimes indicate the index of the last enabled
	 * port, at other times, that of the last possible port, so
	 * determining the maximum port number requires looking at
	 * both CAP.NP and port_map.
	 */
	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));

	host = ata_host_alloc_pinfo(&pdev->dev, ppi, n_ports);
	if (!host)
		return -ENOMEM;
	host->iomap = hpriv->base;
	host->private_data = hpriv;

	if (!(hpriv->cap & HOST_CAP_SSS) || ahci_ignore_sss)
		host->flags |= ATA_HOST_PARALLEL_SCAN;
	else
		printk(KERN_INFO
		       "ahci: SSS flag set, parallel bus scan disabled\n");

	if (pi.flags & ATA_FLAG_EM)
		ahci_reset_em(host);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		//ata_port_pbar_desc(ap, AHCI_PCI_BAR, -1, "abar");
		//ata_port_pbar_desc(ap, AHCI_PCI_BAR,
		//                 0x100 + ap->port_no * 0x80, "port");

		/* set initial link pm policy */
		ap->target_lpm_policy = ATA_LPM_UNKNOWN;

		/* set enclosure management message type */
		if (ap->flags & ATA_FLAG_EM)
			ap->em_message_type = ahci_em_messages;

		/* disabled/not-implemented port */
		if (!(hpriv->port_map & (1 << i)))
			ap->ops = &ata_dummy_port_ops;
	}

	/* initialize adapter */
	rc = ahci_configure_dma_masks(pdev, hpriv->cap & HOST_CAP_64);
	if (rc)
		return rc;

	rc = ahci_reset_controller(host);
	if (rc)
		return rc;

	ahci_init_controller(host);
	ahci_print_info(host);

	return ata_host_activate(host, hpriv->irq, ahci_interrupt, IRQF_SHARED,
				 &ahci_sht);
}

/*
 *
 *      ahci_platform_remove    -       unplug a platform interface
 *      @pdev: platform device
 *
 *      A platform bus SATA device has been unplugged. Perform the needed
 *      cleanup. Also called on module unload for any active devices.
 */
static int __devexit ahci_platform_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ata_host *host = dev_get_drvdata(dev);

#ifdef	CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#endif

	ata_host_detach(host);
	return 0;
}

#ifdef	CONFIG_PM_RUNTIME
static const struct dev_pm_ops ahci_pm_ops = {
	.runtime_suspend= ahci_platform_device_runtime_suspend,
	.runtime_resume = ahci_platform_device_runtime_resume,
	.runtime_idle   = ahci_platform_device_runtime_idle,
	.suspend_noirq 	= ahci_platform_device_suspend_noirq,
	.resume_noirq 	= ahci_platform_device_resume_noirq,
};
#endif

static struct platform_driver ahci_platform_driver = {
	.probe		= ahci_platform_probe,
	.remove		= __devexit_p(ahci_platform_remove),
	.driver = {
		.name		= DRV_NAME,
		.bus		= &platform_bus_type,
		.owner		= THIS_MODULE,
#ifdef CONFIG_PM_RUNTIME
        	.pm = &ahci_pm_ops,

#endif		
	},
#ifdef CONFIG_PM
	.suspend		= ahci_platform_device_suspend,
	.resume		= ahci_platform_device_resume,
#endif
};

static int __init ahci_platform_init(void)
{
	return platform_driver_register(&ahci_platform_driver);
}

static void __exit ahci_platform_exit(void)
{
	platform_driver_unregister(&ahci_platform_driver);
}


MODULE_AUTHOR("Jason Li");
MODULE_DESCRIPTION("Golden Gate AHCI SATA low-level driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_init(ahci_platform_init);
module_exit(ahci_platform_exit);
