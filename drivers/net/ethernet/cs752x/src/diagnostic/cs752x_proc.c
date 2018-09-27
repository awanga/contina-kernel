/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

/***********************************************************************
 Tree View of PROC                           Description
 Path prefix : proc/drivers/cs752x/ne
 File                 Type/Length    Definition
 ni/ni_debug          uint32         0: disable NI and IRQ
 				     1: enable NI
 				     2: enable IRQ
 				     3: enable NI and IRQ
 qm/qm_debug          uint32         0: disable, 1: enable
 tm/tm_debug          uint32         0: disable, 1: enable
 sch/sch_debug        uint32         0: disable, 1: enable
 switch/switch_debug  uint32         0: disable, 1: enable
 switch/register      string/uint32  Sample codes for multiple parameters
 fe/fe_debug          uint32         bitwise flags   0: disable, 1: enable
                                      Offset     Short Name      Full Name
                                      0          acl             ACL table
                                      1          an_bng_mac      AN BNG MAC table
                                      2          checkmem        Memory checking table
                                      3          class           Classifier table
                                      4          etype           Ethernet type table
                                      5          fwdrslt         Forwarding hash result table
                                      6          hash            HASH
                                      7          hw              Hardware table
                                      8          llc_hdr         LLC header table
                                      9          lpb             LPB table
                                      10         lpm             LPM module
                                      11         mc              Multicast group ID
                                      12         pe_voq_drp      Packet editor VoQ drop table
                                      13         pktlen_rngs     Packet length range table
                                      14         port_rngs       Port range table
                                      15         qosrslt         QoS hash result table
                                      16         rslt_fvlan_tbl  Flow VLAN table
                                      17         rslt_l2_tbl     L2 result table
                                      18         rslt_l3_tbl     L3 result table
                                      19         sdb             SDB table
                                      20         vlan            VLAN table
                                      21         voqpol          VoQ policer table

 adaptation/adaptation_debug    uint32  bitwise flags   0: disable, 1: enable
                                      Offset     Short Name      Full Name
                                      0          8021q           802.1q VLAN hooks
                                      1          bridge          Bridge hooks
                                      2          common          Basic functions
                                      3          core            Core hooks
                                      4          ipsec           IPSec hooks
                                      5          ipv6            IPv6 hooks
                                      6          multicast       IGMP hooks
                                      7          netfilter       Netfilter hooks
                                      8          pppoe           PPPoE hooks
                                      9          qos             QoS hooks

 hw_accel_debug                 uint32  bitwise flags   0: disable, 1: enable
                                     Offset     Short Name              Full Name
                                     0          hw_accel                HW acceleration
                                     1          hw_accel_bridge         Bridge acceleration
                                     2          hw_accel_nat            NAT acceleration
                                     3          hw_accel_vlan           VLAN acceleration
                                     4          hw_accel_pppoe          PPPoE acceleration
                                     5          hw_accel_ipsec          IPSec acceleration
                                     6          hw_accel_multicast      Multicast acceleration
                                     7          hw_accel_ipv6_routing   IPv6 Routing acceleration
                                     8          hw_accel_qos_ingress    OoS Ingress acceleration
                                     9          hw_accel_qos_engress    OoS Engress acceleration
                                     10         hw_accel_double_check   Double check enable

 Path prefix : proc/drivers/cs752x
 File                       Type/Length    Definition
 pcie/pcie_debug            uint32         0: disable, 1: enable
 sata/sata_debug            uint32         0: disable, 1: enable
 sd/sd_debug                uint32         0: disable, 1: enable
 spdif/spdif_debug          uint32         0: disable, 1: enable
 spi/spi_debug              uint32         0: disable, 1: enable
 ssp/ssp_debug              uint32         0: disable, 1: enable
 ts/ts_debug                uint32         0: disable, 1: enable
 usb_host/usb_host_debug    uint32         0: disable, 1: enable
 usb_dev/usb_dev_debug      uint32         0: disable, 1: enable
 fb/fb_debug                uint32         0: disable, 1: enable
 crypto/crypto_debug        uint32         0: disable, 1: enable
 pwr_ctrl/pwr_ctrl_debug    uint32         0: disable, 1: enable
 cir/cir_debug              uint32         0: disable, 1: enable
 ***********************************************************************/
#ifdef CONFIG_CS752X_PROC
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>

#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */

#include <mach/g2cpu_ipc.h>
#include <mach/g2-acp.h>

#include "cs752x_proc.h"	/* local definitions */
#include "cs75xx_qm.h"
#include "cs_accel_cb.h"

#include "cs_fe.h"

#include <mach/cs75xx_mibs.h>
#ifdef CONFIG_CS75XX_WFO
#include <mach/cs75xx_ipc_wfo.h>
#include <mach/cs75xx_mibs_wfo.h>
#endif
//++BUG#39672: 2. QoS
#include <mach/cs75xx_qos.h>
//--BUG#39672
//++BUG#40328
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
#include <mach/cs75xx_mibs_iplip.h>
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
//--BUG#40328

#include "cs_mut.h"

/*
 * Global Debug Flags
 */
u32 cs_ni_debug = 0;
u32 cs_ni_use_sendfile = 1;
u32 cs_ni_fastbridge = 0;
u32 cs_ni_clear_stats_port_id = 0;
u32 cs_ni_min_rsve_mem = 2048; /*2MB*/
u32 cs_ni_delay_ms_rsve_mem = 100; /* 100 ms */
u32 cs_qm_debug = 0;
u32 cs_tm_debug = 0;
u32 cs_sch_debug = 0;
u32 cs_switch_debug = 0;
u32 cs_fe_debug = 0;
#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
u32 cs_fe_double_chk = 0;
#endif
u32 cs_adapt_debug = 0x000;
u32 cs_ne_core_logic_debug = 0;

u32 cs_pcie_debug = 0;
u32 cs_sata_debug = 0;
u32 cs_sd_debug = 0;
u32 cs_spdif_debug = 0;
u32 cs_spi_debug = 0;
u32 cs_ssp_debug = 0;
u32 cs_ts_debug = 0;
u32 cs_usb_host_debug = 0;
u32 cs_usb_dev_debug = 0;
u32 cs_fb_debug = 0;
u32 cs_crypto_debug = 0;
u32 cs_pwr_ctrl_debug = 0;
u32 cs_cir_debug = 0;
u32 cs_rt3593_noncache = 0;
u32 cs_rt3593_tx_lock = 0;
u32 cs_rt3593_dma_sync = 0;
u32 cs_rt3593_dev_num = 0;
u32 cs_acp_enable = 0;

#ifdef CONFIG_CS75XX_WFO
//++ For WFO
u32 cs_wfo_debug = 0;
u32 cs_wfo_enable = 1;
u32 cs_wfo_csme = 0;        // CSME disable by default
u32 cs_wfo_rate_adjust = 0;
u32 cs_wfo_rate_adjust_period = 0;
//-- For WFO
#endif

EXPORT_SYMBOL(cs_ni_debug);
EXPORT_SYMBOL(cs_ni_use_sendfile);
EXPORT_SYMBOL(cs_ni_fastbridge);
EXPORT_SYMBOL(cs_ni_clear_stats_port_id);
EXPORT_SYMBOL(cs_ni_min_rsve_mem);
EXPORT_SYMBOL(cs_ni_delay_ms_rsve_mem);
EXPORT_SYMBOL(cs_qm_debug);
EXPORT_SYMBOL(cs_tm_debug);
EXPORT_SYMBOL(cs_sch_debug);
EXPORT_SYMBOL(cs_switch_debug);
EXPORT_SYMBOL(cs_fe_debug);
#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
EXPORT_SYMBOL(cs_fe_double_chk);
#endif
EXPORT_SYMBOL(cs_adapt_debug);
EXPORT_SYMBOL(cs_ne_core_logic_debug);

EXPORT_SYMBOL(cs_pcie_debug);
EXPORT_SYMBOL(cs_sata_debug);
EXPORT_SYMBOL(cs_sd_debug);
EXPORT_SYMBOL(cs_spdif_debug);
EXPORT_SYMBOL(cs_spi_debug);
EXPORT_SYMBOL(cs_ssp_debug);
EXPORT_SYMBOL(cs_ts_debug);
EXPORT_SYMBOL(cs_usb_host_debug);
EXPORT_SYMBOL(cs_usb_dev_debug);
EXPORT_SYMBOL(cs_fb_debug);
EXPORT_SYMBOL(cs_crypto_debug);
EXPORT_SYMBOL(cs_pwr_ctrl_debug);
EXPORT_SYMBOL(cs_cir_debug);
EXPORT_SYMBOL(cs_rt3593_noncache);
EXPORT_SYMBOL(cs_rt3593_tx_lock);
EXPORT_SYMBOL(cs_rt3593_dma_sync);
EXPORT_SYMBOL(cs_acp_enable);

#ifdef CONFIG_CS75XX_WFO
//++ For WFO
EXPORT_SYMBOL(cs_wfo_debug);
EXPORT_SYMBOL(cs_wfo_enable);
EXPORT_SYMBOL(cs_wfo_csme);
EXPORT_SYMBOL(cs_wfo_rate_adjust);
EXPORT_SYMBOL(cs_wfo_rate_adjust_period);
//-- For WFO
#endif

struct pci_dev *cs_rt3593_dev[4]={NULL};
static int __init config_default_value(void)
{
	if (cs_rt3593_dev_num >= 2){
		cs_rt3593_noncache = 1;
		cs_ni_fastbridge = 1;
		irq_set_affinity(cs_rt3593_dev[0]->bus->self->irq, cpumask_of(0));
		irq_set_affinity(cs_rt3593_dev[1]->bus->self->irq, cpumask_of(1));
	}
	else if (cs_rt3593_dev_num == 1) {
		cs_rt3593_noncache = 0;
		cs_ni_fastbridge = 1;
		cs_rt3593_dma_sync = 1;
		irq_set_affinity(cs_rt3593_dev[0]->bus->self->irq, cpu_all_mask);
	}
	else {
		cs_rt3593_noncache = 0;
		cs_ni_fastbridge = 0;
	}
	printk(KERN_INFO "G2 default proc value:\n"
		 "\tcs_rt3593_noncache=%d\n"
		 "\tcs_ni_fastbridge=%d\n",
		 cs_rt3593_noncache,
		 cs_ni_fastbridge);
	return 0;
}

/* directory name */
#define CS752X			"driver/cs752x"
#define CS752X_NE		"ne"
#define CS752X_NE_NI		"ni"
#define CS752X_NE_QM		"qm"
#define CS752X_NE_TM		"tm"
#define CS752X_NE_SCH		"sch"
#define CS752X_NE_SWITCH	"switch"
#define CS752X_NE_FE		"fe"
#define CS752X_NE_ADAPT		"adaptation"
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
#define CS752X_NE_IPLIP		"iplip"   //BUG#40328
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
#define CS752X_NE_CORE		"core"


#define CS752X_PCIE		"pcie"
#define CS752X_SATA		"sata"
#define CS752X_SD		"sd"
#define CS752X_SPDIF		"spdif"
#define CS752X_SPI		"spi"
#define CS752X_SSP		"ssp"
#define CS752X_TS		"ts"
#define CS752X_USB_HOST		"usb_host"
#define CS752X_USB_DEV		"usb_dev"
#define CS752X_FB		"fb"
#define CS752X_CRYPTO		"crypto"
#define CS752X_PWR_CTRL		"pwr_ctrl"
#define CS752X_CIR		"cir"
#define CS752X_RT3593		"rt3593"
#define CS752X_ACP		"acp"
#define CS752X_IPC		"ipc"
#define CS752X_QOS		"qos"   //BUG#39672: 2. QoS
#define CS752X_MCAST		"mcast" //BUG#39919
#define CS752X_WFO		"wfo"   // For WFO

/* file name */
#define NI_DEBUG		"ni_debug"
#define NI_USE_SENDFILE		"ni_use_sendfile"
#define NI_FASTBRIDGE		"ni_fastbridge"
#define NI_CLEAR_STATS          "ni_clear_stats"
#define NI_MIN_RSVE_MEM          "ni_min_rsve_mem"
#define NI_DELAY_MS_RSVE_MEM     "ni_delay_ms_rsve_mem"
#define NI_INSTANCE_BUF_LOW      "ni_instance_buf_low"
#define QM_DEBUG		"qm_debug"
#define QM_INT_BUFF		"qm_int_buff"
#define TM_DEBUG		"tm_debug"
#define SCH_DEBUG		"sch_debug"
#define SWITCH_DEBUG		"switch_debug"
#define SWITCH_REG		"register"
#define FE_DEBUG		"fe_debug"
#define FE_DOUBLE_CHECK		"double_check"
#define HASH_TIMER_PERIOD	"hash_timer_period"
#define ADAPT_DEBUG		"adaptation_debug"
#define CORE_LOGIC_DEBUG	"core_logic_debug"

#define PCIE_DEBUG		"pcie_debug"
#define SATA_DEBUG		"sata_debug"
#define SD_DEBUG		"sd_debug"
#define SPDIF_DEBUG		"spdif_debug"
#define SPI_DEBUG		"spi_debug"
#define SSP_DEBUG		"ssp_debug"
#define TS_DEBUG		"ts_debug"
#define USB_HOST_DEBUG		"usb_host_debug"
#define USB_DEV_DEBUG		"usb_dev_debug"
#define FB_DEBUG		"fb_debug"
#define CRYPTO_DEBUG		"crypto_debug"
#define PWR_CTRL_DEBUG		"pwr_ctrl_debug"
#define CIR_DEBUG		"cir_debug"
#define RT3593_NONCACHE		"noncacheable_buffer"
#define RT3593_TX_LOCK		"split_tx_lock"
#define RT3593_DMA_SYNC		"dma_sync"
#define ACP_ENABLE		"acp_enable"
#define CORE_CS_CB_DEBUG	"cs_cb_info"
#define NE_MEMTRACE		"ne_memtrace"
#define SKB_RECYCLE		"skb_recycle"
#define SKB_RECYCLE_MAX		"skb_recycle_max"
#define SKB_RECYCLE_QUEUE_LEN		"skb_recycle_queue"
#define IPC_LIST_STATUS		"ipc_list_status"
//++ For WFO
#define WFO_DEBUG		"wifi_offload_debug"
#define WFO_ENABLE		"wifi_offload_enable"
#define WFO_CSME		"wifi_offload_csme"
#define WFO_GET_PE0_MIBS	"pe0_get_mibs"
#define WFO_GET_PE1_MIBS	"pe1_get_mibs"
#define WFO_GET_PE0_LOGS	"pe0_get_logs"
#define WFO_GET_PE1_LOGS	"pe1_get_logs"
#define WFO_PE0_FWD_TBL		"pe0_fwd_tbl"
#define WFO_PE1_FWD_TBL		"pe1_fwd_tbl"
#define WFO_RATE_ADJUST		"wifi_offload_rate_adjust"
#define WFO_RATE_ADJUST_PERIOD		"wifi_offload_rate_adjust_period"
//-- For WFO

/* help message */
#define CS752X_CORE_CS_CB_DEBUG_MESG	"Purpose: Print CS Control Block " \
			"Debug Info\nRead Usage: cat %s\nWrite Usage: disable\n"

#define CS752X_HELP_MSG "READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value 0: Disable\n" \
			"value 1: Enable\n"

#define CS752X_NE_NI_HELP_MSG "Purpose: Enable NI driver DBG Message\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: Enable NI DBG Message\n" \
			"flag 0x00000002: Enable IRQ DBG Message\n" \
			"flag 0x00000004: Enable DMA LSO DBG Message\n" \
			"flag 0x00000008: Enable to dump RX packets\n" \
			"flag 0x00000010: Enable to dump TX packets\n" \
			"flag 0x00000020: Enable io ctl DBG Message\n" \
			"flag 0x00000040: Enable Virtual NI DBG Message\n" \
			"flag 0x00000080: Enable Ethtool DBG Message\n" \
			"flag 0x00000100: Enable PHY DBG Message\n"


#define CS752X_NI_USE_SENDFILE_HELP_MSG "READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value 0: Disable sendfile\n" \
			"value 1: Enable sendfile\n"

#define CS752X_NE_FE_HELP_MSG "Purpose: Enable FE table DBG Message\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: ACL table\n" \
			"flag 0x00000002: AN BNG MAC table\n" \
			"flag 0x00000004: Memory checking table (Reserved)\n" \
			"flag 0x00000008: Classifier table\n" \
			"flag 0x00000010: Ethernet type table\n" \
			"flag 0x00000020: Forwarding hash result table\n" \
			"flag 0x00000040: VoQ policer table\n" \
			"flag 0x00000080: Hardware table\n" \
			"flag 0x00000100: LLC header table\n" \
			"flag 0x00000200: LPB table\n" \
			"flag 0x00000400: LPM module\n" \
			"flag 0x00000800: Multicast group ID\n" \
			"flag 0x00001000: Packet editor VoQ drop table\n" \
			"flag 0x00002000: Packet length range table\n" \
			"flag 0x00004000: Port range table\n" \
			"flag 0x00008000: QoS hash result table\n" \
			"flag 0x00010000: Flow VLAN table\n" \
			"flag 0x00020000: L2 result table\n" \
			"flag 0x00040000: L3 result table\n" \
			"flag 0x00080000: SDB table\n" \
			"flag 0x00100000: VLAN table\n" \
			"flag 0x00200000: HASH Mask table\n" \
			"flag 0x00400000: HASH Status table\n" \
			"flag 0x00800000: HASH Overflow table\n" \
			"flag 0x01000000: HASH Check table\n" \
			"flag 0x02000000: HASH Hash table\n" \
			"flag 0x04000000: fe table core\n" \
			"flag 0x08000000: fe table generic\n" \
			"flag 0x10000000: fe table unit test\n" \
			"flag 0x20000000: fe table utility\n"

#define CS752X_FE_DOUBLE_CHECK_HELP_MSG "Purpose: Enable double check for FE hash entry\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value 0: Disable double check\n" \
			"value 1: Enable double check\n"


#define CS752X_NE_ADAPT_HELP_MSG  "Purpose: Enable Kernel Adapt Module DBG Message\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: Adapt Module Main/Core utility\n" \
			"flag 0x00000002: Adapt Module 802.1q VLAN\n" \
			"flag 0x00000004: Adapt Module Bridge\n" \
			"flag 0x00000008: Adapt Module IPv4 Multicast\n" \
			"flag 0x00000010: Adapt Module IPv6 Multicast\n" \
			"flag 0x00000020: Adapt Module IPv4 Forward\n" \
			"flag 0x00000040: Adapt Module IPv6 Forward\n" \
			"flag 0x00000080: Adapt Module QoS Ingress\n" \
			"flag 0x00000100: Adapt Module QoS Egress\n" \
			"flag 0x00000200: Adapt Module Arp\n" \
			"flag 0x00000400: Adapt Module IPSec\n" \
			"flag 0x00000800: Adapt Module PPPoE\n" \
			"flag 0x00001000: HW Accel Double check DBG Msg\n" \
			"flag 0x00002000: Adapt Module EtherIP\n" \
			"flag 0x00004000: Adapt Module Netfilter Drop\n" \
			"flag 0x00008000: Adapt Module WFO\n" \
			"flag 0x00010000: Adapt Module Tunnel\n" \
			"flag 0x00020000: Adapt Module DSCP\n"  \
			"flag 0x00040000: Adapt Module SW Wireless\n" \
			"flag 0x00080000: Adapt Module LPM Management Unit\n"  \
                        "flag 0x00100000: Adapt Module IPv4 and IPv6 Routing API\n" \
			"flag 0x00200000: Adapt Module Rule Hash Management API\n"

#define CS752X_NE_CORE_LOGIC_HELP_MSG  "Purpose: Enable NE Core Logic DBG Message\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: Core Logic main control unit\n" \
			"flag 0x00000002: Core Logic core vtable\n" \
			"flag 0x00000004: Core Logic vtable framework\n" \
			"flag 0x00000008: Core Logic core HMU\n" \
			"flag 0x00000010: Core Logic Hash Management Unit Framework\n" \
			"flag 0x00000020: Core Logic FastNet\n" \
			"flag 0x00000040: Core Logic SKB CB\n" \
			"flag 0x00000080: Core Logic core rule HMU\n"

#define CS752X_NE_HASH_TIMER_PERIOD_HELP_MSG "Hash Timer Scan Period\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value is in the unit of second and must be larger than 0\n"

#define REG_HELP_MSG "Command: echo write [block] [subblock] [reg] [value] > %s\n" \
			"Command: echo read [block] [subblock] [reg] > %s\n" \
			"Write example: echo write 1 1 4 0x000300ff > %s\n" \
			"Read example: echo read 1 1 4 > %s\n"

#define CS752X_ACP_ENABLE_HELP_MSG  "Purpose: Enable Peripheral ACP\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: AHCI Module\n" \
			"flag 0x00000002: SD/MMC Module \n" \
			"flag 0x00000004: USB Module\n" \
			"flag 0x00000008: LCD Module\n" \
			"flag 0x00000010: SPDIF/SSP Module\n" \
			"flag 0x00000020: Crypto Module\n" \
			"flag 0x00000040: TS Module\n" \
			"flag 0x00000080: DMA Module\n" \
			"flag 0x00000100: Flash Module\n" \
			"flag 0x00000200: NI Module\n" \
			"flag 0x00000400: PCI Module for DIRECTION FROM DEVICE\n" \
			"flag 0x00000800: PCI Module for DIRECTION TO DEVICE\n" \
			"flag 0xFFFFF000: Reservered\n"

//++ For WFO
#define CS752X_WFO_DEBUG_MSG  "Purpose: Enable Wi-Fi Offload DBG Message\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: WFO Module IPC\n" \
			"flag 0x00000002: WFO Module PNI \n" \
			"flag 0x00000004: WFO Module CSME \n" \
			"flag 0xFFFFFFF8: Reservered\n"
#define CS752X_WFO_ENABLE_MSG  "Purpose: Enable Wi-Fi Offload FUNCTION\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: Wi-Fi Offload\n" \
			"flag 0xFFFFFFFE: Reservered\n"
#define CS752X_WFO_CSME_MSG  "Purpose: Enable Wi-Fi Offload CSME (Multicast Enhancement) FUNCTION\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: Wi-Fi Offload CSME\n" \
			"flag 0x00000002: Wi-Fi Offload CSME IGMPv3 Support\n" \
			"flag 0xFFFFFFFC: Reservered\n"
//-- For WFO

#define CS752X_NI_CLEAR_STATS_HELP_MSG "Clear GMAC port statistics\n" \
                        "READ Usage: cat %s\n" \
                        "WRITE Usage: echo [value] > %s\n" \
                        "value is the GMAC port, 0=GMAC0, 1=GMAC1, 2=GMAC2\n"

/* entry pointer */
struct proc_dir_entry *proc_driver_cs752x = NULL,
		*proc_driver_cs752x_ne = NULL,
		*proc_driver_cs752x_ne_ni,
		*proc_driver_cs752x_ne_qm,
		*proc_driver_cs752x_ne_tm,
		*proc_driver_cs752x_ne_sch,
		*proc_driver_cs752x_ne_switch,
		*proc_driver_cs752x_ne_fe,
		*proc_driver_cs752x_ne_adaptation,
//++BUG#40328
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
		*proc_driver_cs752x_ne_iplip,
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
//--BUG#40328
		*proc_driver_cs752x_ne_core;

struct proc_dir_entry *proc_driver_cs752x_pcie,
		*proc_driver_cs752x_sata,
		*proc_driver_cs752x_sd,
		*proc_driver_cs752x_spdif,
		*proc_driver_cs752x_spi,
		*proc_driver_cs752x_ssp,
		*proc_driver_cs752x_ts,
		*proc_driver_cs752x_usb_host,
		*proc_driver_cs752x_usb_dev,
		*proc_driver_cs752x_fb,
		*proc_driver_cs752x_crypto,
		*proc_driver_cs752x_pwr_ctrl,
		*proc_driver_cs752x_cir,
		*proc_driver_cs752x_rt3593,
		*proc_driver_cs752x_acp,
		*proc_driver_cs752x_ipc,
		*proc_driver_cs752x_qos,    //BUG#39672: 2. QoS
		*proc_driver_cs752x_mcast,  //BUG#39919
		*proc_driver_cs752x_wfo;    // For WFO

EXPORT_SYMBOL(proc_driver_cs752x);
EXPORT_SYMBOL(proc_driver_cs752x_ne);
EXPORT_SYMBOL(proc_driver_cs752x_ne_ni);
EXPORT_SYMBOL(proc_driver_cs752x_ne_qm);
EXPORT_SYMBOL(proc_driver_cs752x_ne_tm);
EXPORT_SYMBOL(proc_driver_cs752x_ne_sch);
EXPORT_SYMBOL(proc_driver_cs752x_ne_switch);
EXPORT_SYMBOL(proc_driver_cs752x_ne_fe);
EXPORT_SYMBOL(proc_driver_cs752x_ne_adaptation);
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
EXPORT_SYMBOL(proc_driver_cs752x_ne_iplip);    //BUG#40328
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
EXPORT_SYMBOL(proc_driver_cs752x_ne_core);

EXPORT_SYMBOL(proc_driver_cs752x_pcie);
EXPORT_SYMBOL(proc_driver_cs752x_sata);
EXPORT_SYMBOL(proc_driver_cs752x_sd);
EXPORT_SYMBOL(proc_driver_cs752x_spdif);
EXPORT_SYMBOL(proc_driver_cs752x_spi);
EXPORT_SYMBOL(proc_driver_cs752x_ssp);
EXPORT_SYMBOL(proc_driver_cs752x_ts);
EXPORT_SYMBOL(proc_driver_cs752x_usb_host);
EXPORT_SYMBOL(proc_driver_cs752x_usb_dev);
EXPORT_SYMBOL(proc_driver_cs752x_fb);
EXPORT_SYMBOL(proc_driver_cs752x_crypto);
EXPORT_SYMBOL(proc_driver_cs752x_pwr_ctrl);
EXPORT_SYMBOL(proc_driver_cs752x_cir);
EXPORT_SYMBOL(proc_driver_cs752x_rt3593);
EXPORT_SYMBOL(proc_driver_cs752x_ipc);
EXPORT_SYMBOL(proc_driver_cs752x_qos);  //BUG#39672: 2. QoS
EXPORT_SYMBOL(proc_driver_cs752x_mcast);//BUG#39919
EXPORT_SYMBOL(proc_driver_cs752x_wfo);  // For WFO

EXPORT_SYMBOL(cs752x_add_proc_handler);
EXPORT_SYMBOL(cs752x_str_paser);


//++BUG#39919
extern void cs_mcast_proc_init_module(void);
extern void cs_mcast_proc_exit_module(void);
//--BUG#39919
#ifdef CONFIG_CS752X_ACCEL_KERNEL
extern void cs_qos_proc_init_module(void);
extern void cs_qos_proc_exit_module(void);
#endif //CONFIG_CS752X_ACCEL_KERNEL

/*
 * Wrapper Functions
 */

int cs752x_add_proc_handler(char *name,
			    read_proc_t * hook_func_read,
			    write_proc_t * hook_func_write,
			    struct proc_dir_entry *parent)
{
	struct proc_dir_entry *node;

	node = create_proc_entry(name, S_IRUGO | S_IWUGO, parent);
	if (node) {
		node->read_proc = hook_func_read;
		node->write_proc = hook_func_write;
	} else {
		printk(KERN_ERR "ERROR in creating proc entry (%s)! \n", name);
		return -EINVAL;
	}

	return 0;
}

/*
 * Purpose: Parse string into a list of tokens.
 *          Notice that the original string will be changed.
 *          All splitters will be replaced with '\0'.
 * Params:
 *          src_str:   original string. It will be modified after calling this function.
 *          max_tok_num: the max. number of tokens could be supported in output array.
 *          tok_idx_list[]: a list of pointers which point to each token found in the original string.
 *                      We don't allocate other buffers for tokens, and directly reuse the original one.
 *          tok_cnt:    The acutal number of tokens we found in the string.
 * Return:
 *          0:  success
 *          -1: fail
 */

int cs752x_str_paser(char *src_str, int max_tok_num,
		     char *tok_idx_list[] /*output */ ,
		     int *tok_cnt /*output */ )
{
	int tok_index = 0;
	char *curr, *tok_head;

	if (src_str == NULL || max_tok_num < 1 ||
			tok_idx_list == NULL || tok_cnt == NULL) {
		return EINVAL;
	}

	/* split each token into token buffer */
	curr = tok_head = src_str;
	while ((tok_index < max_tok_num)) {
		/* when we still have empty token buffer  */
		switch (*curr) {
		case '\0':	/* exit when end of string */
			goto STR_PARSER_EXIT;

		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case ',':
			if (tok_head != curr) {
				/* there is at least one character in the token */
				*curr = '\0';
				break;
			} else
				tok_head++;
			/* go through default case */

		default:
			curr++;
			continue;
		}
		tok_idx_list[tok_index] = tok_head;
		tok_index++;
		curr++;
		tok_head = curr;
	}

STR_PARSER_EXIT:
	*tok_cnt = tok_index;
	return 0;
}

/*
 * The proc filesystem: function to read and write entry
 */

/* file handler for cs_pcie_debug */
static int cs_pcie_debug_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, PCIE_DEBUG, PCIE_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", PCIE_DEBUG, cs_pcie_debug);
	*eof = 1;

	return len;
}

static int cs_pcie_debug_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto PCIE_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto PCIE_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto PCIE_INVAL_EXIT;

	cs_pcie_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", PCIE_DEBUG, cs_pcie_debug);

	return count;

PCIE_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, PCIE_DEBUG, PCIE_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_sata_debug */
static int cs_sata_debug_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SATA_DEBUG, SATA_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", SATA_DEBUG, cs_sata_debug);
	*eof = 1;

	return len;
}

static int cs_sata_debug_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SATA_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SATA_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto SATA_INVAL_EXIT;

	cs_sata_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SATA_DEBUG, cs_sata_debug);

	return count;

SATA_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SATA_DEBUG, SATA_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_acp_enable */
static int cs_acp_enable_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	if((system_rev & 0xFF) != 0xA1){
		len += sprintf(buf + len, "ACP not supported in A0 Chip\n");
		*eof = 1;
		return len;
	}

	len += sprintf(buf + len, CS752X_ACP_ENABLE_HELP_MSG, ACP_ENABLE, ACP_ENABLE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", ACP_ENABLE, cs_acp_enable);
	*eof = 1;

	return len;
}

static int cs_acp_enable_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;
	GLOBAL_SCRATCH_t global_scratch;

	if((system_rev & 0xFF) != 0xA1){
		printk("ACP not supported in A0 chip\n");
		return count;
	}

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto ACP_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto ACP_INVAL_EXIT;

	cs_acp_enable = mask;
	if(cs_acp_enable & 0x00000FFF) {
		/*enable the recirc ACP transactions "secured"*/
		global_scratch.wrd = readl(GLOBAL_SCRATCH);
		global_scratch.wrd |= 0x1000;
		writel(global_scratch.wrd, GLOBAL_SCRATCH);
	} else {
		global_scratch.wrd = readl(GLOBAL_SCRATCH);
		global_scratch.wrd &= ~0x1000;
		writel(global_scratch.wrd, GLOBAL_SCRATCH);
	}
	goldengate_acp_update();

	printk(KERN_WARNING "Set %s as 0x%08x\n", ACP_ENABLE, cs_acp_enable);

	return count;

ACP_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_ACP_ENABLE_HELP_MSG, ACP_ENABLE, ACP_ENABLE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}


/* file handler for cs_sd_debug */
static int cs_sd_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SD_DEBUG, SD_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", SD_DEBUG, cs_sd_debug);
	*eof = 1;

	return len;
}

#ifdef CONFIG_CS75XX_WFO
//++ For WFO
/* file handler for cs_wfo_debug */
static int cs_wfo_debug_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_WFO_DEBUG_MSG, WFO_DEBUG, WFO_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", WFO_DEBUG, cs_wfo_debug);
	*eof = 1;

	return len;
}

static int cs_wfo_debug_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto WFO_DEBUG_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto WFO_DEBUG_INVAL_EXIT;

	if (mask > CS752X_WFO_DBG_MAX)
		goto WFO_DEBUG_INVAL_EXIT;

	cs_wfo_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", WFO_DEBUG, cs_wfo_debug);

	return count;

WFO_DEBUG_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_WFO_DEBUG_MSG, WFO_DEBUG, WFO_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}


/* file handler for cs_wfo_enable */
static int cs_wfo_enable_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_WFO_ENABLE_MSG, WFO_ENABLE, WFO_ENABLE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", WFO_ENABLE, cs_wfo_enable);
	*eof = 1;

	return len;
}

static int cs_wfo_enable_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto WFO_ENABLE_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto WFO_ENABLE_INVAL_EXIT;

//	if (mask > CS752X_WFO_DBG_MAX)
//		goto WFO_INVAL_EXIT;

	cs_wfo_enable = mask;

    if (cs_wfo_enable & 0x01) {
        cs_wfo_ipc_send_start_stop_command(CS_WFO_IPC_PE_START);
    } else {
        cs_wfo_ipc_send_start_stop_command(CS_WFO_IPC_PE_STOP);
    }

	printk(KERN_WARNING "Set %s as 0x%08x\n", WFO_ENABLE, cs_wfo_enable);

	return count;

WFO_ENABLE_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_WFO_ENABLE_MSG, WFO_ENABLE, WFO_ENABLE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}


/* file handler for cs_wfo_csme */
static int cs_wfo_csme_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_WFO_CSME_MSG, WFO_CSME, WFO_CSME);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", WFO_CSME, cs_wfo_csme);
	*eof = 1;

	return len;
}

static int cs_wfo_csme_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto WFO_ENABLE_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto WFO_ENABLE_INVAL_EXIT;


	cs_wfo_csme = mask;

    if ((cs_wfo_csme & CS752X_WFO_CSME_ENABLE) != CS752X_WFO_CSME_ENABLE) {
        cs_wfo_csme = cs_wfo_csme & ~CS752X_WFO_CSME_IGMPV3;
    }

	printk(KERN_WARNING "Set %s as 0x%08x\n", WFO_CSME, cs_wfo_csme);

	return count;

WFO_ENABLE_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_WFO_CSME_MSG, WFO_CSME, WFO_CSME);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for WFO_GET_PE0_MIBS */
static int cs_wfo_get_pe0_mibs_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;
	cs_mib_pe_s mib_counter;

	cs_wfo_ipc_pe_get_mibs(&mib_counter, CS_WFO_IPC_PE0_CPU_ID);
	*eof = 1;
	return len;
}

static int cs_wfo_get_pe0_mibs_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	u32 len = 0;
	cs_mib_pe_s mib_counter;

	cs_wfo_ipc_pe_set_mibs(&mib_counter, CS_WFO_IPC_PE0_CPU_ID);
	return count;
}

/* file handler for WFO_GET_PE1_MIBS */
static int cs_wfo_get_pe1_mibs_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;
	cs_mib_pe_s mib_counter;

	cs_wfo_ipc_pe_get_mibs(&mib_counter, CS_WFO_IPC_PE1_CPU_ID);
	*eof = 1;
	return len;
}

static int cs_wfo_get_pe1_mibs_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	u32 len = 0;
	cs_mib_pe_s mib_counter;

	cs_wfo_ipc_pe_set_mibs(&mib_counter, CS_WFO_IPC_PE1_CPU_ID);
	return count;
}

/* file handler for WFO_GET_PE0_LOGS */
static int cs_wfo_get_pe0_logs_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;
	char logs[512];

    memset(&logs, 0, sizeof(logs));
    cs_wfo_ipc_pe_get_logs(logs, CS_WFO_IPC_PE1_CPU_ID);
	*eof = 1;
	return len;
}

/* file handler for WFO_GET_PE1_LOGS */
static int cs_wfo_get_pe1_logs_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;
	char logs[512];

    memset(&logs, 0, sizeof(logs));
    cs_wfo_ipc_pe_get_logs(logs, CS_WFO_IPC_PE1_CPU_ID);
	*eof = 1;
	return len;
}

/* file handler for WFO_PE0_FWD_TBL */
static int cs_wfo_get_pe0_fwd_tbl_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

    cs_wfo_ipc_pe_send_dump_fwtbl(CS_WFO_IPC_PE0_CPU_ID);
    cs_wfo_ipc_pe_dump_fwtbl(CS_WFO_IPC_PE0_CPU_ID);
	*eof = 1;
	return len;
}

/* file handler for WFO_PE1_FWD_TBL */
static int cs_wfo_get_pe1_fwd_tbl_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

    cs_wfo_ipc_pe_send_dump_fwtbl(CS_WFO_IPC_PE1_CPU_ID);
    cs_wfo_ipc_pe_dump_fwtbl(CS_WFO_IPC_PE1_CPU_ID);
	*eof = 1;
	return len;
}

/* file handler for WFO_RATE_ADJUST */
static int cs_wfo_rate_adjust_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, "Purpose: Enable Wi-Fi Offload Rate Adjustment");
	len += sprintf(buf + len, "\n%s = 0x%08x\n", WFO_RATE_ADJUST, cs_wfo_rate_adjust);
	*eof = 1;

	return len;
}

static int cs_wfo_rate_adjust_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto WFO_DEBUG_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto WFO_DEBUG_INVAL_EXIT;

	if (mask > 1)
		goto WFO_DEBUG_INVAL_EXIT;

	cs_wfo_rate_adjust = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", WFO_RATE_ADJUST, cs_wfo_rate_adjust);

	return count;

WFO_DEBUG_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_WFO_DEBUG_MSG, WFO_DEBUG, WFO_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_wfo_rate_adjust_period_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, "Purpose: Read Wi-Fi Offload Rate Adjustment Timer Period");
	len += sprintf(buf + len, "\n%s = %dms\n", WFO_RATE_ADJUST_PERIOD, cs_wfo_rate_adjust_period);
	*eof = 1;

	return len;
}

static int cs_wfo_rate_adjust_period_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto WFO_DEBUG_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto WFO_DEBUG_INVAL_EXIT;

	if (mask >= 10)
		cs_wfo_rate_adjust_period = mask;
	printk(KERN_WARNING "Set %s as %dms\n", WFO_RATE_ADJUST_PERIOD, cs_wfo_rate_adjust_period);

	return count;

WFO_DEBUG_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_WFO_DEBUG_MSG, WFO_DEBUG, WFO_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

//-- For WFO
#endif

static int cs_sd_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SD_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SD_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto SD_INVAL_EXIT;

	cs_sd_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SD_DEBUG, cs_sd_debug);

	return count;

SD_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SD_DEBUG, SD_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_spdif_debug */
static int cs_spdif_debug_read_proc(char *buf, char **start, off_t offset,
				    int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SPDIF_DEBUG, SPDIF_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", SPDIF_DEBUG,
			cs_spdif_debug);
	*eof = 1;

	return len;
}

static int cs_spdif_debug_write_proc(struct file *file, const char *buffer,
				     unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SPDIF_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SPDIF_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto SPDIF_INVAL_EXIT;

	cs_spdif_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SPDIF_DEBUG, cs_spdif_debug);

	return count;

SPDIF_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SPDIF_DEBUG, SPDIF_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_spi_debug */
static int cs_spi_debug_read_proc(char *buf, char **start, off_t offset,
				    int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SPI_DEBUG, SPI_DEBUG);
	len +=
	    sprintf(buf + len, "\n%s = 0x%08x\n", SPI_DEBUG, cs_spi_debug);
	*eof = 1;

	return len;
}

static int cs_spi_debug_write_proc(struct file *file, const char *buffer,
				     unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SPI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SPI_INVAL_EXIT;

//	if (mask > CS752X_MAX)
//		goto SPI_INVAL_EXIT;

	cs_spi_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SPI_DEBUG, cs_spi_debug);

	return count;

SPI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SPI_DEBUG, SPI_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_ssp_debug */
static int cs_ssp_debug_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SSP_DEBUG, SSP_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", SSP_DEBUG, cs_ssp_debug);
	*eof = 1;

	return len;
}

static int cs_ssp_debug_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SSP_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SSP_INVAL_EXIT;

//	if (mask > CS752X_MAX)
//		goto SSP_INVAL_EXIT;

	cs_ssp_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SSP_DEBUG, cs_ssp_debug);

	return count;

SSP_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SSP_DEBUG, SSP_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_ts_debug */
static int cs_ts_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, TS_DEBUG, TS_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", TS_DEBUG, cs_ts_debug);
	*eof = 1;

	return len;
}

static int cs_ts_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto TS_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto TS_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto TS_INVAL_EXIT;

	cs_ts_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", TS_DEBUG, cs_ts_debug);

	return count;

TS_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, TS_DEBUG, TS_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_usb_host_debug */
static int cs_usb_host_debug_read_proc(char *buf, char **start, off_t offset,
				       int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, USB_HOST_DEBUG,
			USB_HOST_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", USB_HOST_DEBUG,
			cs_usb_host_debug);
	*eof = 1;

	return len;
}

static int cs_usb_host_debug_write_proc(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto USB_HOST_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto USB_HOST_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto USB_HOST_INVAL_EXIT;

	cs_usb_host_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", USB_HOST_DEBUG,
			 cs_usb_host_debug);

	return count;

USB_HOST_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, USB_HOST_DEBUG, USB_HOST_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_usb_dev_debug */
static int cs_usb_dev_debug_read_proc(char *buf, char **start, off_t offset,
				      int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, USB_DEV_DEBUG,
			USB_DEV_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", USB_DEV_DEBUG,
			cs_usb_dev_debug);
	*eof = 1;

	return len;
}

static int cs_usb_dev_debug_write_proc(struct file *file, const char *buffer,
				       unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto USB_DEV_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto USB_DEV_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto USB_DEV_INVAL_EXIT;

	cs_usb_dev_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", USB_DEV_DEBUG,
			cs_usb_dev_debug);

	return count;

USB_DEV_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, USB_DEV_DEBUG, USB_DEV_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_fb_debug */
static int cs_fb_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, FB_DEBUG, FB_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", FB_DEBUG, cs_fb_debug);
	*eof = 1;

	return len;
}

static int cs_fb_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto FB_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto FB_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto FB_INVAL_EXIT;

	cs_fb_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", FB_DEBUG, cs_fb_debug);

	return count;

FB_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, FB_DEBUG, FB_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_crypto_debug */
static int cs_crypto_debug_read_proc(char *buf, char **start, off_t offset,
				     int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, CRYPTO_DEBUG, CRYPTO_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", CRYPTO_DEBUG,
			cs_crypto_debug);
	*eof = 1;

	return len;
}

static int cs_crypto_debug_write_proc(struct file *file, const char *buffer,
				      unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto CRYPTO_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto CRYPTO_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto CRYPTO_INVAL_EXIT;

	cs_crypto_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", CRYPTO_DEBUG,
			cs_crypto_debug);

	return count;

CRYPTO_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, CRYPTO_DEBUG, CRYPTO_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_pwr_ctrl_debug */
static int cs_pwr_ctrl_debug_read_proc(char *buf, char **start, off_t offset,
				       int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, PWR_CTRL_DEBUG,
			PWR_CTRL_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", PWR_CTRL_DEBUG,
			cs_pwr_ctrl_debug);
	*eof = 1;

	return len;
}

static int cs_pwr_ctrl_debug_write_proc(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto PWR_CTRL_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto PWR_CTRL_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto PWR_CTRL_INVAL_EXIT;

	cs_pwr_ctrl_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", PWR_CTRL_DEBUG,
			cs_pwr_ctrl_debug);

	return count;

PWR_CTRL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, PWR_CTRL_DEBUG, PWR_CTRL_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_cir_debug */
static int cs_cir_debug_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, CIR_DEBUG, CIR_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", CIR_DEBUG, cs_cir_debug);
	*eof = 1;

	return len;
}

static int cs_cir_debug_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto CIR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto CIR_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto CIR_INVAL_EXIT;

	cs_cir_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", CIR_DEBUG, cs_cir_debug);

	return count;

CIR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, CIR_DEBUG, CIR_DEBUG);
	return count;	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* file handler for rt3593_noncache */
static int cs_rt3593_noncache_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, RT3593_NONCACHE, RT3593_NONCACHE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", RT3593_NONCACHE, cs_rt3593_noncache);
	*eof = 1;

	return len;
}

static int cs_rt3593_noncache_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto CIR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto CIR_INVAL_EXIT;

	//if (mask > CS752X_MAX)
	//	goto CIR_INVAL_EXIT;

	cs_rt3593_noncache = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", RT3593_NONCACHE, cs_rt3593_noncache);

	return count;

CIR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, RT3593_NONCACHE, RT3593_NONCACHE);
	return count;	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* file handler for rt3593_dma_sync */
static int cs_rt3593_dma_sync_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, RT3593_DMA_SYNC, RT3593_DMA_SYNC);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", RT3593_DMA_SYNC, cs_rt3593_dma_sync);
	*eof = 1;

	return len;
}

static int cs_rt3593_dma_sync_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto CIR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto CIR_INVAL_EXIT;

	//if (mask > CS752X_MAX)
	//	goto CIR_INVAL_EXIT;

	cs_rt3593_dma_sync = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", RT3593_DMA_SYNC, cs_rt3593_dma_sync);

	return count;

CIR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, RT3593_DMA_SYNC, RT3593_DMA_SYNC);
	return count;	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* file handler for rt3593_tx_lock */
static int cs_rt3593_tx_lock_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, RT3593_TX_LOCK, RT3593_TX_LOCK);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", RT3593_TX_LOCK, cs_rt3593_tx_lock);
	*eof = 1;

	return len;
}

static int cs_rt3593_tx_lock_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto CIR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto CIR_INVAL_EXIT;

	//if (mask > CS752X_MAX)
	//	goto CIR_INVAL_EXIT;

	cs_rt3593_tx_lock = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", RT3593_TX_LOCK, cs_rt3593_tx_lock);

	return count;

CIR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, RT3593_TX_LOCK, RT3593_TX_LOCK);
	return count;	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* file handler for ipc_list_status */
static int cs_ipc_list_status_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	/* FIXME! Move IPC debug proc to IPC module */
#ifdef CONFIG_CS75xx_IPC2RCPU
	/* call print status */
	cs_ipc_print_status(CPU_ARM);
	cs_ipc_print_status(CPU_RCPU0);
	cs_ipc_print_status(CPU_RCPU1);
#endif

	*eof = 1;

	return len;
}

static int cs_ipc_list_status_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	/* FIXME! Move IPC debug proc to IPC module */
#ifdef CONFIG_CS75xx_IPC2RCPU
	/* call reset list */
	cs_ipc_reset_list_info(CPU_ARM);
	cs_ipc_reset_list_info(CPU_RCPU0);
	cs_ipc_reset_list_info(CPU_RCPU1);
#endif

	return count;
}
/* file handler for cs_ni_debug */
static int cs_ni_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_NI_HELP_MSG, NI_DEBUG, NI_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", NI_DEBUG, cs_ni_debug);
	*eof = 1;

	return len;
}

static int cs_ni_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto NI_INVAL_EXIT;

	//if (mask > CS752X_MAX )
	//	goto NI_INVAL_EXIT;

	cs_ni_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", NI_DEBUG, cs_ni_debug);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_NI_HELP_MSG, NI_DEBUG, NI_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_net_use_sendfile_cs */
static int cs_ni_use_sendfile_read_proc(char *buf, char **start, off_t offset,
		int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NI_USE_SENDFILE_HELP_MSG,
			NI_USE_SENDFILE, NI_USE_SENDFILE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", NI_USE_SENDFILE,
			cs_ni_use_sendfile);
	*eof = 1;

	return len;
}

static int cs_ni_use_sendfile_write_proc(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	size_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto NI_INVAL_EXIT;

	//if (mask > CS752X_MAX)
	//	goto NI_INVAL_EXIT;

	cs_ni_use_sendfile = mask;
	printk(KERN_WARNING "\nSet %s as 0x%08x\n", NI_USE_SENDFILE,
			cs_ni_use_sendfile);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, NI_USE_SENDFILE, NI_USE_SENDFILE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}


/* file handler for cs_ni_fastbridge */
static int cs_ni_fastbridge_read_proc(char *buf, char **start, off_t offset,
		int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG,
			NI_FASTBRIDGE, NI_FASTBRIDGE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", NI_FASTBRIDGE,
			cs_ni_fastbridge);
	*eof = 1;

	return len;
}

static int cs_ni_fastbridge_write_proc(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	size_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto NI_INVAL_EXIT;

	cs_ni_fastbridge = mask;
	printk(KERN_WARNING "\nSet %s as 0x%08x\n", NI_FASTBRIDGE,
			cs_ni_fastbridge);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, NI_FASTBRIDGE, NI_FASTBRIDGE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_ni_clear_stats */
static int cs_ni_clear_stats_read_proc(char *buf, char **start, off_t offset,
                int count, int *eof, void *data)
{
        u32 len = 0;

        len += sprintf(buf + len, CS752X_NI_CLEAR_STATS_HELP_MSG,
                        NI_CLEAR_STATS, NI_CLEAR_STATS);
        len += sprintf(buf + len, "\n%s = 0x%08x\n", NI_CLEAR_STATS,
                        cs_ni_clear_stats_port_id);
        *eof = 1;

        return len;
}

void cs_ni_clear_stats(unsigned char port_id);
static int cs_ni_clear_stats_write_proc(struct file *file, const char *buffer,
                unsigned long count, void *data)
{
        char buf[32];
        unsigned long mask;
        size_t len;

        len = min(count, (unsigned long)(sizeof(buf) - 1));
        if (copy_from_user(buf, buffer, len))
                goto NI_INVAL_EXIT;

        buf[len] = '\0';
        if (strict_strtoul(buf, 0, &mask))
                goto NI_INVAL_EXIT;

        cs_ni_clear_stats_port_id = mask;

        cs_ni_clear_stats(cs_ni_clear_stats_port_id);

        printk(KERN_WARNING "\nSet %s as 0x%08x\n", NI_CLEAR_STATS,
                        cs_ni_clear_stats_port_id);

        return count;

NI_INVAL_EXIT:
        printk(KERN_WARNING "Invalid argument\n");
        printk(KERN_WARNING CS752X_HELP_MSG, NI_CLEAR_STATS, NI_CLEAR_STATS);
        /* if we return error code here, PROC fs may retry up to 3 times. */
        return count;
}

static int cs_ni_min_rsve_mem_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, "\n%s = 0x%08x\n", NI_MIN_RSVE_MEM, cs_ni_min_rsve_mem);
	*eof = 1;

	return len;
}

static int cs_ni_min_rsve_mem_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto NI_INVAL_EXIT;


	cs_ni_min_rsve_mem = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", NI_MIN_RSVE_MEM, cs_ni_min_rsve_mem);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	return count;
}

static int cs_ni_delay_ms_rsve_mem_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, "\n%s = 0x%08x\n", NI_DELAY_MS_RSVE_MEM, cs_ni_delay_ms_rsve_mem);
	*eof = 1;

	return len;
}

static int cs_ni_delay_ms_rsve_mem_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto NI_INVAL_EXIT;


	cs_ni_delay_ms_rsve_mem = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", NI_DELAY_MS_RSVE_MEM, cs_ni_delay_ms_rsve_mem);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	return count;
}

extern int cs_ni_instance_buf_low_read(int ins);
extern int cs_ni_instance_buf_low_clear();

static int cs_ni_instance_buf_low_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;
	int i;
	int v;
	for (i=0; (v = cs_ni_instance_buf_low_read(i)) >= 0; ++i) {
		len += sprintf(buf + len, "instance %d = 0x%08x\n", i, v);
	}
	*eof = 1;
	return len;
}

static int cs_ni_instance_buf_low_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	cs_ni_instance_buf_low_clear();
	return count;
}


#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
extern int cs_ni_skb_recycle_max;

static int cs_ni_skb_recycle_max_proc_read(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;
	len += sprintf(buf + len, "%d\n", cs_ni_skb_recycle_max);
	*eof = 1;
	return len;
}

static int cs_ni_skb_recycle_max_proc_write(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long val;
	ssize_t len;
	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;
	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &val))
		goto NI_INVAL_EXIT;
	cs_ni_skb_recycle_max = val;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SKB_RECYCLE_MAX, cs_ni_skb_recycle_max);
	return count;
NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	return count;
}
#endif

extern int cs_ni_skb_recycle_len_cpu0();
extern int cs_ni_skb_recycle_len_cpu1();

static int cs_ni_skb_recycle_queue_proc_read(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	int qlen0 = cs_ni_skb_recycle_len_cpu0();
	int qlen1 = cs_ni_skb_recycle_len_cpu1();
	u32 len = 0;
	if (qlen1 < 0) {
		len += sprintf(buf+len, "%d\n", qlen0);
	} else {
		len += sprintf(buf+len, "%d (C0=%d C1=%d)\n", qlen0+qlen1, qlen0, qlen1);
	}
	*eof = 1;
	return len;
}


/* file handler for cs_cb_debug */
static int cs_ne_cs_cb_debug_read_proc(char *buf, char **start, off_t offset,
				       int count, int *eof, void *data)
{
	u32 len = 0;

//	len += sprintf(buf + len, CS752X_CORE_CS_CB_DEBUG_MESG,
//			CORE_CS_CB_DEBUG);
	cs_accel_cb_print();
	*eof = 1;
	return len;
}

static int cs_ne_cs_cb_debug_write_proc(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto HW_ACCEL_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto HW_ACCEL_INVAL_EXIT;

	cs_accel_cb_print();

	return count;

HW_ACCEL_INVAL_EXIT:
	cs_accel_cb_print();
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}


/* file handler for cs_qm_debug */
static int cs_qm_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, QM_DEBUG, QM_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", QM_DEBUG, cs_qm_debug);
	*eof = 1;

	return len;
}

static int cs_qm_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto QM_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto QM_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto QM_INVAL_EXIT;

	cs_qm_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", QM_DEBUG, cs_qm_debug);

	return count;

QM_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, QM_DEBUG, QM_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}


/* file handler for QM_INT_BUFF */
static int cs_qm_int_buff_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

    switch (internal_buff_size) {
    	case CS_QM_INT_BUFF_0:
    		printk("Use external DDR for QM\n");
    	    break;
    	case CS_QM_INT_BUFF_128:
			printk("Half Size Internal Buffers ON - 128KB Packet\n");
			printk(" - 128kB free\n");
    	    break;
    	case CS_QM_INT_BUFF_192:
			printk("Half Size Internal Buffers ON - 192KB Packet\n");
			printk(" - 64kB free\n");
    	    break;
    	case CS_QM_INT_BUFF_256:
		    printk("Full Size Internal Buffers ON - 256KB Packet\n");
    	    break;
    }

	*eof = 1;
	return len;
}


/* file handler for cs_tm_debug */
static int cs_tm_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, TM_DEBUG, TM_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", TM_DEBUG, cs_tm_debug);
	*eof = 1;

	return len;
}

static int cs_tm_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto TM_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto TM_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto TM_INVAL_EXIT;

	cs_tm_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", TM_DEBUG, cs_tm_debug);

	return count;

TM_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, TM_DEBUG, TM_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_sch_debug */
static int cs_sch_debug_read_proc(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SCH_DEBUG, SCH_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", SCH_DEBUG, cs_sch_debug);
	*eof = 1;

	return len;
}

static int cs_sch_debug_write_proc(struct file *file, const char *buffer,
				   unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SCH_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SCH_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto SCH_INVAL_EXIT;

	cs_sch_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SCH_DEBUG, cs_sch_debug);

	return count;

SCH_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SCH_DEBUG, SCH_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_switch_debug */
static int cs_switch_debug_read_proc(char *buf, char **start, off_t offset,
				     int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_HELP_MSG, SWITCH_DEBUG, SWITCH_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", SWITCH_DEBUG,
			cs_switch_debug);
	*eof = 1;

	return len;
}

static int cs_switch_debug_write_proc(struct file *file, const char *buffer,
				      unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto SWITCH_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto SWITCH_INVAL_EXIT;

	if (mask > CS752X_MAX)
		goto SWITCH_INVAL_EXIT;

	cs_switch_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", SWITCH_DEBUG,
			cs_switch_debug);

	return count;

SWITCH_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_HELP_MSG, SWITCH_DEBUG, SWITCH_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_fe_debug */
static int cs_fe_debug_read_proc(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_FE_HELP_MSG, FE_DEBUG, FE_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", FE_DEBUG, cs_fe_debug);
	*eof = 1;

#if 0
	//FIXME: move below code to through ioctl, CH
	//"echo 6 > /proc/driver/cs752x/ne/cs_fe_debug"
	if (cs_fe_debug == 6) {
		for (i = 0; i < 12288 * 2; i++) {
			cs_hash_invalid_hash_entry_by_index(i);
		}
		printk("Delete all of Hash entry\n");
	}
#endif
	return len;
}

static int cs_fe_debug_write_proc(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto FE_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto FE_INVAL_EXIT;

	if (mask > CS752X_FE_MAX)
		goto FE_INVAL_EXIT;

	cs_fe_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", FE_DEBUG, cs_fe_debug);

	return count;

FE_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_FE_HELP_MSG, FE_DEBUG, FE_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
/* file handler for cs_fe_double_check */
static int cs_fe_double_check_read_proc(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_FE_DOUBLE_CHECK_HELP_MSG,
			FE_DOUBLE_CHECK, FE_DOUBLE_CHECK);
	len += sprintf(buf + len, "\n%s = %d\n", FE_DOUBLE_CHECK,
			cs_fe_double_chk);
	*eof = 1;
	return len;
}

extern void cs_hw_accel_mgr_delete_flow_based_hash_entry();
static int cs_fe_double_check_write_proc(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto HW_ACCEL_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto HW_ACCEL_INVAL_EXIT;

	if (cs_fe_double_chk != mask) {
		/* clean all hash entries */
		cs_hw_accel_mgr_delete_flow_based_hash_entry();
	}

	cs_fe_double_chk = mask;

	printk(KERN_WARNING "\nSet %s as %d\n", FE_DOUBLE_CHECK,
			cs_fe_double_chk);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_FE_DOUBLE_CHECK_HELP_MSG,
			FE_DOUBLE_CHECK, FE_DOUBLE_CHECK);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}
#endif

/* file handler for cs_adapt_debug */
static int cs_adapt_debug_read_proc(char *buf, char **start, off_t offset,
				    int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_ADAPT_HELP_MSG, ADAPT_DEBUG,
			ADAPT_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", ADAPT_DEBUG,
			cs_adapt_debug);
	*eof = 1;
	return len;
}

static int cs_adapt_debug_write_proc(struct file *file, const char *buffer,
				     unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto ADAPT_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto ADAPT_INVAL_EXIT;

	if (mask > CS752X_ADAPT_MAX)
		goto ADAPT_INVAL_EXIT;

	cs_adapt_debug = mask;
	printk(KERN_WARNING "Set %s as 0x%08x\n", ADAPT_DEBUG, cs_adapt_debug);

	return count;

ADAPT_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_ADAPT_HELP_MSG, ADAPT_DEBUG, ADAPT_DEBUG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/* file handler for cs_ne_core_logic_debug */
static int cs_ne_core_logic_debug_read_proc(char *buf, char **start, off_t offset,
				       int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_CORE_LOGIC_HELP_MSG,
			CORE_LOGIC_DEBUG, CORE_LOGIC_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", CORE_LOGIC_DEBUG,
			cs_ne_core_logic_debug);
	*eof = 1;
	return len;
}

static int cs_ne_core_logic_debug_write_proc(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto HW_ACCEL_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto HW_ACCEL_INVAL_EXIT;

	if (mask > CS752X_CORE_LOGIC_MAX)
		goto HW_ACCEL_INVAL_EXIT;

	cs_ne_core_logic_debug = mask;
	printk(KERN_WARNING "\nSet %s as 0x%08x\n", CORE_LOGIC_DEBUG,
			cs_ne_core_logic_debug);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_CORE_LOGIC_HELP_MSG, CORE_LOGIC_DEBUG,
			CORE_LOGIC_DEBUG);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* sample code to support multiple parameters in one proc file */

/*
 * --------------Sample code--------------
 * Read switch register
 */
static int cs_switch_reg_read_proc(char *buf, char **start, off_t offset,
				   int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, REG_HELP_MSG, SWITCH_REG, SWITCH_REG,
			SWITCH_REG, SWITCH_REG);
	*eof = 1;

	return len;
}

/*
 * --------------Sample code--------------
 * Switch register read/write (read from user space)
 * Command: echo "write [block] [subblock] [reg] [value]" > register
 *          echo "read  [block] [subblock] [reg]" > register
 * Params:  [block]:    [1-2]
 *          [subblock]: [1-8]
 *          [reg]:      [0x1-0xFF]
 *          [value]:    [0-0xFFFFFFFF]
 */

#define MAX_BUF_SIZE            128
#define MAX_NUM_OF_PARAMETERS   10

#define MAX_BLOCK_INDEX         2
#define MIN_BLOCK_INDEX         1

#define MAX_SUBBLOCK_INDEX      8
#define MIN_SUBBLOCK_INDEX      1

#define MAX_REG_ADDR            0xFF
#define MIN_REG_ADDR            0x1

static int cs_switch_reg_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[MAX_BUF_SIZE];
	char *token_list[MAX_NUM_OF_PARAMETERS];
	int tok_cnt = 0;
	ssize_t len;
	const char *write_cmd = "write";
	const char *read_cmd = "read";
	u8 cmd_mode;		/* 0:read, 1:write */
	//int idx;
	int nTokLen;
	unsigned long block, sub_block, reg, value;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto REG_INVAL_EXIT;

	buf[len] = '\0';

	/* split each token */
	if (cs752x_str_paser(buf, MAX_NUM_OF_PARAMETERS, token_list, &tok_cnt))
		goto REG_INVAL_EXIT;

	/* analyze meaning of each token */

	nTokLen = strlen(token_list[0]);
	if (!strncmp(write_cmd, token_list[0], nTokLen) && tok_cnt >= 5)
		cmd_mode = 1;	/* write */
	else if (!strncmp(read_cmd, token_list[0], nTokLen) && tok_cnt >= 4)
		cmd_mode = 0;	/* read */
	else
		goto REG_INVAL_EXIT;

	if (strict_strtoul(token_list[1], 0, &block) ||
			block > MAX_BLOCK_INDEX || block < MIN_BLOCK_INDEX)
		goto REG_INVAL_EXIT;

	if (strict_strtoul(token_list[2], 0, &sub_block) ||
			sub_block > MAX_SUBBLOCK_INDEX || sub_block < MIN_SUBBLOCK_INDEX)
		goto REG_INVAL_EXIT;

	if (strict_strtoul(token_list[3], 0, &reg) ||
			reg > MAX_REG_ADDR || reg < MIN_REG_ADDR)
		goto REG_INVAL_EXIT;

	if (cmd_mode == 1) {
		if (strict_strtoul(token_list[4], 0, &value))
			goto REG_INVAL_EXIT;

		/* for debug only */
		printk("Write: (block, subblock, reg, value) -> (%ld, %ld, %ld,"
				" 0x%08lX)\n", block, sub_block, reg, value);
	} else {
		/* for debug only */
		printk("Read: (block, subblock, reg) -> (%ld, %ld, %ld)\n",
				block, sub_block, reg);
	}

	/* Execute the command here */

	/* End of the execution */

	return count;

REG_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING REG_HELP_MSG, SWITCH_REG, SWITCH_REG, SWITCH_REG,
			SWITCH_REG);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

/*
 * Actually create (and remove) the /proc file(s).
 */

void __exit cs752x_proc_cleanup_module(void)
{
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	cs_mcast_proc_exit_module(); //BUG#39919
#endif

#ifdef CONFIG_CS752X_ACCEL_KERNEL
//++BUG#39672: 2. QoS
	cs_qos_proc_exit_module();
//--BUG#39672
#endif //CONFIG_CS752X_ACCEL_KERNEL

//++BUG#40328
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
    cs_iplip_proc_exit_module();
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
//--BUG#40328


	/* no problem if it was not registered */
	/* remove file entry */
	remove_proc_entry(PCIE_DEBUG, proc_driver_cs752x_pcie);
	remove_proc_entry(SATA_DEBUG, proc_driver_cs752x_sata);
	remove_proc_entry(SD_DEBUG, proc_driver_cs752x_sd);
	remove_proc_entry(SPDIF_DEBUG, proc_driver_cs752x_spdif);
	remove_proc_entry(SPI_DEBUG, proc_driver_cs752x_spi);
	remove_proc_entry(SSP_DEBUG, proc_driver_cs752x_ssp);
	remove_proc_entry(TS_DEBUG, proc_driver_cs752x_ts);
	remove_proc_entry(USB_HOST_DEBUG, proc_driver_cs752x_usb_host);
	remove_proc_entry(USB_DEV_DEBUG, proc_driver_cs752x_usb_dev);
	remove_proc_entry(FB_DEBUG, proc_driver_cs752x_fb);
	remove_proc_entry(CRYPTO_DEBUG, proc_driver_cs752x_crypto);
	remove_proc_entry(PWR_CTRL_DEBUG, proc_driver_cs752x_pwr_ctrl);
	remove_proc_entry(CIR_DEBUG, proc_driver_cs752x_cir);
	remove_proc_entry(RT3593_NONCACHE, proc_driver_cs752x_rt3593);
	remove_proc_entry(RT3593_TX_LOCK, proc_driver_cs752x_rt3593);
	remove_proc_entry(RT3593_DMA_SYNC, proc_driver_cs752x_rt3593);
	remove_proc_entry(IPC_LIST_STATUS, proc_driver_cs752x_ipc);
	remove_proc_entry(NI_DEBUG, proc_driver_cs752x_ne_ni);
	remove_proc_entry(NI_USE_SENDFILE, proc_driver_cs752x_ne_ni);
	remove_proc_entry(NI_FASTBRIDGE, proc_driver_cs752x_ne_ni);
	remove_proc_entry(NI_MIN_RSVE_MEM, proc_driver_cs752x_ne_ni);
	remove_proc_entry(NI_DELAY_MS_RSVE_MEM, proc_driver_cs752x_ne_ni);
	remove_proc_entry(NI_INSTANCE_BUF_LOW, proc_driver_cs752x_ne_ni);
	remove_proc_entry(QM_DEBUG, proc_driver_cs752x_ne_qm);
	remove_proc_entry(QM_INT_BUFF, proc_driver_cs752x_ne_qm);
	remove_proc_entry(TM_DEBUG, proc_driver_cs752x_ne_tm);
	remove_proc_entry(SCH_DEBUG, proc_driver_cs752x_ne_sch);
	remove_proc_entry(SWITCH_DEBUG, proc_driver_cs752x_ne_switch);
	remove_proc_entry(SWITCH_REG, proc_driver_cs752x_ne_switch);
	remove_proc_entry(FE_DEBUG, proc_driver_cs752x_ne_fe);
#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
	remove_proc_entry(FE_DOUBLE_CHECK, proc_driver_cs752x_ne_fe);
#endif
	remove_proc_entry(HASH_TIMER_PERIOD, proc_driver_cs752x_ne_fe);
	remove_proc_entry(ADAPT_DEBUG, proc_driver_cs752x_ne_adaptation);
	remove_proc_entry(CORE_LOGIC_DEBUG, proc_driver_cs752x_ne_core);
	remove_proc_entry(CORE_CS_CB_DEBUG, proc_driver_cs752x_ne_core);
#ifdef CONFIG_CS75XX_WFO
//++ For WFO
	remove_proc_entry(WFO_DEBUG, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_ENABLE, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_CSME, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_GET_PE0_MIBS, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_GET_PE1_MIBS, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_GET_PE0_LOGS, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_GET_PE1_LOGS, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_PE0_FWD_TBL, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_PE1_FWD_TBL, proc_driver_cs752x_wfo);
	remove_proc_entry(WFO_RATE_ADJUST, proc_driver_cs752x_wfo);
//-- For WFO
#endif

	/* remove dir entry */
	remove_proc_entry(CS752X_PCIE, proc_driver_cs752x);
	remove_proc_entry(CS752X_SATA, proc_driver_cs752x);
	remove_proc_entry(CS752X_SD, proc_driver_cs752x);
	remove_proc_entry(CS752X_SPDIF, proc_driver_cs752x);
	remove_proc_entry(CS752X_SPI, proc_driver_cs752x);
	remove_proc_entry(CS752X_SSP, proc_driver_cs752x);
	remove_proc_entry(CS752X_TS, proc_driver_cs752x);
	remove_proc_entry(CS752X_USB_HOST, proc_driver_cs752x);
	remove_proc_entry(CS752X_USB_DEV, proc_driver_cs752x);
	remove_proc_entry(CS752X_FB, proc_driver_cs752x);
	remove_proc_entry(CS752X_CRYPTO, proc_driver_cs752x);
	remove_proc_entry(CS752X_PWR_CTRL, proc_driver_cs752x);
	remove_proc_entry(CS752X_CIR, proc_driver_cs752x);
	remove_proc_entry(CS752X_RT3593, proc_driver_cs752x);
	remove_proc_entry(CS752X_NE_NI, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_NE_QM, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_NE_TM, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_NE_SCH, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_NE_SWITCH, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_NE_FE, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_NE_ADAPT, proc_driver_cs752x_ne);
//++BUG#40328
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	remove_proc_entry(CS752X_NE_IPLIP, proc_driver_cs752x_ne);
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
//--BUG#40328
	remove_proc_entry(CS752X_NE_CORE, proc_driver_cs752x_ne);
	remove_proc_entry(CS752X_ACP, proc_driver_cs752x);
	remove_proc_entry(CS752X_IPC, proc_driver_cs752x);
	remove_proc_entry(CS752X_QOS, proc_driver_cs752x); //BUG#39672: 2. QoS

#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	remove_proc_entry(CS752X_MCAST, proc_driver_cs752x); //BUG#39919
#endif

#ifdef CONFIG_CS75XX_WFO
	remove_proc_entry(CS752X_WFO, proc_driver_cs752x); // For WFO
#endif
#if 0
	remove_proc_entry(CS752X_NE, proc_driver_cs752x);
	remove_proc_entry(CS752X, NULL);
#else
	if (atomic_read(&proc_driver_cs752x_ne->count) == 1) {
		printk("%s() remove proc dir %s \n", __func__, CS752X_NE);
		remove_proc_entry(CS752X_NE, proc_driver_cs752x);
		proc_driver_cs752x_ne = NULL;
	} else
		atomic_dec(&proc_driver_cs752x_ne->count);

	if (atomic_read(&proc_driver_cs752x->count) == 1) {
		printk("%s() remove proc dir %s \n", __func__, CS752X);
		remove_proc_entry(CS752X, NULL);
		proc_driver_cs752x = NULL;
	} else
		atomic_dec(&proc_driver_cs752x->count);
#endif
}

int __init cs752x_proc_init_module(void)
{
	config_default_value();
	if (proc_driver_cs752x == NULL)
		proc_driver_cs752x = proc_mkdir(CS752X, NULL);
	else
		atomic_inc(&proc_driver_cs752x->count);

	if (proc_driver_cs752x_ne == NULL)
		proc_driver_cs752x_ne = proc_mkdir(CS752X_NE,
				proc_driver_cs752x);
	else
		atomic_inc(&proc_driver_cs752x_ne->count);

	proc_driver_cs752x_ne_ni =
	    proc_mkdir(CS752X_NE_NI, proc_driver_cs752x_ne);
	proc_driver_cs752x_ne_qm =
	    proc_mkdir(CS752X_NE_QM, proc_driver_cs752x_ne);
	proc_driver_cs752x_ne_tm =
	    proc_mkdir(CS752X_NE_TM, proc_driver_cs752x_ne);
	proc_driver_cs752x_ne_sch =
	    proc_mkdir(CS752X_NE_SCH, proc_driver_cs752x_ne);
	proc_driver_cs752x_ne_switch =
	    proc_mkdir(CS752X_NE_SWITCH, proc_driver_cs752x_ne);
	proc_driver_cs752x_ne_fe =
	    proc_mkdir(CS752X_NE_FE, proc_driver_cs752x_ne);
	proc_driver_cs752x_ne_adaptation =
	    proc_mkdir(CS752X_NE_ADAPT, proc_driver_cs752x_ne);
//++BUG#40328
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	proc_driver_cs752x_ne_iplip =
		proc_mkdir(CS752X_NE_IPLIP, proc_driver_cs752x_ne);
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
//--BUG#40328
	proc_driver_cs752x_ne_core =
			proc_mkdir(CS752X_NE_CORE, proc_driver_cs752x_ne);

	proc_driver_cs752x_pcie = proc_mkdir(CS752X_PCIE, proc_driver_cs752x);
	proc_driver_cs752x_sata = proc_mkdir(CS752X_SATA, proc_driver_cs752x);
	proc_driver_cs752x_sd = proc_mkdir(CS752X_SD, proc_driver_cs752x);
	proc_driver_cs752x_spdif = proc_mkdir(CS752X_SPDIF, proc_driver_cs752x);
	proc_driver_cs752x_spi = proc_mkdir(CS752X_SPI, proc_driver_cs752x);
	proc_driver_cs752x_ssp = proc_mkdir(CS752X_SSP, proc_driver_cs752x);
	proc_driver_cs752x_ts = proc_mkdir(CS752X_TS, proc_driver_cs752x);
	proc_driver_cs752x_usb_host =
	    proc_mkdir(CS752X_USB_HOST, proc_driver_cs752x);
	proc_driver_cs752x_usb_dev =
	    proc_mkdir(CS752X_USB_DEV, proc_driver_cs752x);
	proc_driver_cs752x_fb = proc_mkdir(CS752X_FB, proc_driver_cs752x);
	proc_driver_cs752x_crypto =
	    proc_mkdir(CS752X_CRYPTO, proc_driver_cs752x);
	proc_driver_cs752x_pwr_ctrl =
	    proc_mkdir(CS752X_PWR_CTRL, proc_driver_cs752x);
	proc_driver_cs752x_cir = proc_mkdir(CS752X_CIR, proc_driver_cs752x);
	proc_driver_cs752x_rt3593 =
			proc_mkdir(CS752X_RT3593, proc_driver_cs752x);
	proc_driver_cs752x_acp =
		proc_mkdir(CS752X_ACP, proc_driver_cs752x);
	proc_driver_cs752x_ipc =
		proc_mkdir(CS752X_IPC, proc_driver_cs752x);
//++BUG#39672: 2. QoS
	proc_driver_cs752x_qos =
		proc_mkdir(CS752X_QOS, proc_driver_cs752x);

//--BUG#39672
//++BUG#39919
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	proc_driver_cs752x_mcast =
		proc_mkdir(CS752X_MCAST, proc_driver_cs752x);
#endif

//--BUG#39919
#ifdef CONFIG_CS75XX_WFO
//++ For WFO
	proc_driver_cs752x_wfo =
		proc_mkdir(CS752X_WFO, proc_driver_cs752x);
//-- For WFO
#endif
	cs752x_add_proc_handler(NI_DEBUG, cs_ni_debug_read_proc,
				cs_ni_debug_write_proc,
				proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(NI_USE_SENDFILE, cs_ni_use_sendfile_read_proc,
				cs_ni_use_sendfile_write_proc,
				proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(NI_FASTBRIDGE, cs_ni_fastbridge_read_proc,
				cs_ni_fastbridge_write_proc,
				proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(NI_CLEAR_STATS, cs_ni_clear_stats_read_proc,
                                cs_ni_clear_stats_write_proc,
                                proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(NI_MIN_RSVE_MEM, cs_ni_min_rsve_mem_read_proc,
                                cs_ni_min_rsve_mem_write_proc,
                                proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(NI_DELAY_MS_RSVE_MEM, cs_ni_delay_ms_rsve_mem_read_proc,
                                cs_ni_delay_ms_rsve_mem_write_proc,
                                proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(NI_INSTANCE_BUF_LOW, cs_ni_instance_buf_low_read_proc,
                                cs_ni_instance_buf_low_write_proc,
                                proc_driver_cs752x_ne_ni);
#ifdef CONFIG_CS75XX_MUT
	cs752x_add_proc_handler(SKB_RECYCLE, mut_skb_recycle_proc_read,
                                mut_skb_recycle_proc_write,
                                proc_driver_cs752x_ne_ni);
#endif
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
	cs752x_add_proc_handler(SKB_RECYCLE_MAX, cs_ni_skb_recycle_max_proc_read,
                                cs_ni_skb_recycle_max_proc_write,
                                proc_driver_cs752x_ne_ni);
#endif
	cs752x_add_proc_handler(SKB_RECYCLE_QUEUE_LEN, cs_ni_skb_recycle_queue_proc_read,
                                NULL,
                                proc_driver_cs752x_ne_ni);
	cs752x_add_proc_handler(QM_DEBUG, cs_qm_debug_read_proc,
				cs_qm_debug_write_proc,
				proc_driver_cs752x_ne_qm);
	cs752x_add_proc_handler(QM_INT_BUFF, cs_qm_int_buff_read_proc,
				NULL,
				proc_driver_cs752x_ne_qm);
	cs752x_add_proc_handler(TM_DEBUG, cs_tm_debug_read_proc,
				cs_tm_debug_write_proc,
				proc_driver_cs752x_ne_tm);
	cs752x_add_proc_handler(SCH_DEBUG, cs_sch_debug_read_proc,
				cs_sch_debug_write_proc,
				proc_driver_cs752x_ne_sch);
	cs752x_add_proc_handler(SWITCH_DEBUG, cs_switch_debug_read_proc,
				cs_switch_debug_write_proc,
				proc_driver_cs752x_ne_switch);
	cs752x_add_proc_handler(SWITCH_REG, cs_switch_reg_read_proc,
				cs_switch_reg_write_proc,
				proc_driver_cs752x_ne_switch);
	cs752x_add_proc_handler(FE_DEBUG, cs_fe_debug_read_proc,
				cs_fe_debug_write_proc,
				proc_driver_cs752x_ne_fe);
#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
	cs752x_add_proc_handler(FE_DOUBLE_CHECK, cs_fe_double_check_read_proc,
				cs_fe_double_check_write_proc,
				proc_driver_cs752x_ne_fe);
#endif

	cs752x_add_proc_handler(ADAPT_DEBUG, cs_adapt_debug_read_proc,
				cs_adapt_debug_write_proc,
				proc_driver_cs752x_ne_adaptation);

	cs752x_add_proc_handler(CORE_LOGIC_DEBUG,
				cs_ne_core_logic_debug_read_proc,
				cs_ne_core_logic_debug_write_proc,
				proc_driver_cs752x_ne_core);

	cs752x_add_proc_handler(CORE_CS_CB_DEBUG,
					cs_ne_cs_cb_debug_read_proc,
					cs_ne_cs_cb_debug_write_proc,
					proc_driver_cs752x_ne_core);

#ifdef CONFIG_CS75XX_MUT
	cs752x_add_proc_handler(NE_MEMTRACE,
					mut_ne_memtrace_proc_read,
					mut_ne_memtrace_proc_write,
					proc_driver_cs752x_ne_core);
#endif

	cs752x_add_proc_handler(PCIE_DEBUG, cs_pcie_debug_read_proc,
				cs_pcie_debug_write_proc,
				proc_driver_cs752x_pcie);
	cs752x_add_proc_handler(SATA_DEBUG, cs_sata_debug_read_proc,
				cs_sata_debug_write_proc,
				proc_driver_cs752x_sata);
	cs752x_add_proc_handler(ACP_ENABLE, cs_acp_enable_read_proc,
				cs_acp_enable_write_proc,
				proc_driver_cs752x_acp);
#ifdef CONFIG_CS75XX_WFO
//++ For WFO
	cs752x_add_proc_handler(WFO_DEBUG, cs_wfo_debug_read_proc,
				cs_wfo_debug_write_proc,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_ENABLE, cs_wfo_enable_read_proc,
				cs_wfo_enable_write_proc,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_CSME, cs_wfo_csme_read_proc,
				cs_wfo_csme_write_proc,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_GET_PE0_MIBS, cs_wfo_get_pe0_mibs_read_proc,
				cs_wfo_get_pe0_mibs_write_proc,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_GET_PE1_MIBS, cs_wfo_get_pe1_mibs_read_proc,
				cs_wfo_get_pe1_mibs_write_proc,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_GET_PE0_LOGS, cs_wfo_get_pe0_logs_read_proc,
				NULL,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_GET_PE1_LOGS, cs_wfo_get_pe1_logs_read_proc,
				NULL,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_PE0_FWD_TBL, cs_wfo_get_pe0_fwd_tbl_read_proc,
				NULL,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_PE1_FWD_TBL, cs_wfo_get_pe1_fwd_tbl_read_proc,
				NULL,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_RATE_ADJUST, cs_wfo_rate_adjust_read_proc,
				cs_wfo_rate_adjust_write_proc,
				proc_driver_cs752x_wfo);
	cs752x_add_proc_handler(WFO_RATE_ADJUST_PERIOD, cs_wfo_rate_adjust_period_read_proc,
				cs_wfo_rate_adjust_period_write_proc,
				proc_driver_cs752x_wfo);
//-- For WFO
#endif
	cs752x_add_proc_handler(SD_DEBUG, cs_sd_debug_read_proc,
				cs_sd_debug_write_proc, proc_driver_cs752x_sd);
	cs752x_add_proc_handler(SPDIF_DEBUG, cs_spdif_debug_read_proc,
				cs_spdif_debug_write_proc,
				proc_driver_cs752x_spdif);
	cs752x_add_proc_handler(SPI_DEBUG, cs_spi_debug_read_proc,
				cs_spi_debug_write_proc,
				proc_driver_cs752x_spi);
	cs752x_add_proc_handler(SSP_DEBUG, cs_ssp_debug_read_proc,
				cs_ssp_debug_write_proc,
				proc_driver_cs752x_ssp);
	cs752x_add_proc_handler(TS_DEBUG, cs_ts_debug_read_proc,
				cs_ts_debug_write_proc, proc_driver_cs752x_ts);
	cs752x_add_proc_handler(USB_HOST_DEBUG, cs_usb_host_debug_read_proc,
				cs_usb_host_debug_write_proc,
				proc_driver_cs752x_usb_host);
	cs752x_add_proc_handler(USB_DEV_DEBUG, cs_usb_dev_debug_read_proc,
				cs_usb_dev_debug_write_proc,
				proc_driver_cs752x_usb_dev);
	cs752x_add_proc_handler(FB_DEBUG, cs_fb_debug_read_proc,
				cs_fb_debug_write_proc, proc_driver_cs752x_fb);
	cs752x_add_proc_handler(CRYPTO_DEBUG, cs_crypto_debug_read_proc,
				cs_crypto_debug_write_proc,
				proc_driver_cs752x_crypto);
	cs752x_add_proc_handler(PWR_CTRL_DEBUG, cs_pwr_ctrl_debug_read_proc,
				cs_pwr_ctrl_debug_write_proc,
				proc_driver_cs752x_pwr_ctrl);
	cs752x_add_proc_handler(CIR_DEBUG, cs_cir_debug_read_proc,
				cs_cir_debug_write_proc,
				proc_driver_cs752x_cir);
	cs752x_add_proc_handler(RT3593_NONCACHE, cs_rt3593_noncache_read_proc,
				cs_rt3593_noncache_write_proc,
				proc_driver_cs752x_rt3593);
	cs752x_add_proc_handler(RT3593_TX_LOCK, cs_rt3593_tx_lock_read_proc,
				cs_rt3593_tx_lock_write_proc,
				proc_driver_cs752x_rt3593);
	cs752x_add_proc_handler(RT3593_DMA_SYNC, cs_rt3593_dma_sync_read_proc,
				cs_rt3593_dma_sync_write_proc,
				proc_driver_cs752x_rt3593);
	cs752x_add_proc_handler(IPC_LIST_STATUS, cs_ipc_list_status_read_proc,
				cs_ipc_list_status_write_proc,
				proc_driver_cs752x_ipc);

    cs_pe_mibs_desc_init(); //Bug#40475

//++BUG#40328
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	cs_iplip_proc_init_module();
#endif //CONFIG_CS75XX_HW_ACCEL_IPLIP
//--BUG#40328

//++BUG#39672: 2. QoS
#ifdef CONFIG_CS752X_ACCEL_KERNEL //Bug#42574
	cs_qos_proc_init_module();
#endif //CONFIG_CS752X_ACCEL_KERNEL
//--BUG#39672
#ifdef CONFIG_CS75XX_DATA_PLANE_MULTICAST
	cs_mcast_proc_init_module(); //BUG#39919
#endif

	return 0;
}

module_init(cs752x_proc_init_module);
module_exit(cs752x_proc_cleanup_module);
MODULE_AUTHOR("Eric Wang <eric.wang@cortina-systems.com>");
MODULE_LICENSE("GPL");

#endif				/* CONFIG_CS752X_PROC */
