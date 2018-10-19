/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
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

#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/crc32.h>
#include <linux/string.h>
#include <asm/memory.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/cacheflush.h>
#include <asm/checksum.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/in6.h>
#include <linux/udp.h>
#include <linux/phy.h>
#include <linux/workqueue.h>
#include <linux/ethtool.h>
#include <linux/vmstat.h>
#include "cs752x_eth.h"
#include "cs75xx_ethtool.h"
#include "cs752x_ioctl.h"
#include "cs75xx_phy.h"
#include "cs_fe.h"
#include "cs75xx_tm.h"
#include "cs752x_sch.h"
#include "cs75xx_ne_irq.h"
#include "cs_core_logic.h"
#include "cs_core_fastnet.h"
#include "cs_core_vtable.h"
#include "cs752x_voq_cntr.h"
#include "cs_mut.h"
#include <linux/etherhook.h>

/* 3.4.11 Change for register as platform device */
#include <linux/platform_device.h>
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
#include "cs752x_virt_ni.h"
#endif /* CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE */

#include <mach/cs75xx_qos.h>        //BUG#39672: 2. QoS     Bug#42574
#ifdef CONFIG_CS752X_HW_ACCELERATION
#include "cs_hw_accel_qos.h"
#endif /* CONFIG_CS752X_HW_ACCELERATION */

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
#include "cs_hw_accel_ipsec.h"
#endif

#ifdef CS75XX_HW_ACCEL_TUNNEL
#include "cs_hw_accel_tunnel.h"
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
#include "cs_hw_accel_wireless.h"
#endif

#ifdef CONFIG_CS752X_ACCEL_KERNEL
#include "cs_hw_accel_manager.h"
#endif

#include "cs_hw_accel_forward.h"


#include <linux/ratelimit.h>
#include <../net/8021q/vlan.h>
/* force async access mode */
/* comment out define below to go back to sync mode */
//#define CONFIG_CS74XX_NI_ASYNC_IO_MODE 1

#ifdef CONFIG_CS74XX_NI_ASYNC_IO_MODE 
#define NI_READL_MODE readl_relaxed
#define NI_WRITEL_MODE writel_relaxed
#else /* CONFIG_CS74XX_NI_ASYNC_IO_MODE */
#define NI_READL_MODE readl
#define NI_WRITEL_MODE writel
#endif /* CONFIG_CS74XX_NI_ASYNC_IO_MODE */

#ifdef CONFIG_CS752X_PROC
#define DBG(x) {if (cs_ni_debug & DBG_NI || cs_ni_debug & DBG_NI_IRQ) x;}
	extern u32 cs_acp_enable;
#else
#define DBG(x) {}
#endif
#define CONFIG_GENERIC_IRQ 1

#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
#define CS752X_NI_TX_COMPLETION_TIMER
#endif

static const u32 default_msg = (NETIF_MSG_DRV		| \
				NETIF_MSG_PROBE		| \
	 			NETIF_MSG_LINK		| \
	 			NETIF_MSG_TIMER		| \
	 			NETIF_MSG_IFDOWN	| \
	 			NETIF_MSG_IFUP		| \
	 			NETIF_MSG_RX_ERR	| \
	 			NETIF_MSG_TX_ERR);
static int debug = -1;	/* defaults above */
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

#define CS752X_NI_NAPI 1
#ifdef CONFIG_CS75XX_KTHREAD_RX
 #include <linux/kthread.h>

static struct rx_task_info
{
	int instance;
	struct task_struct *task;
	struct completion interrupt_trigger;
	struct work_struct work;
} rx_tasks[8];


 #undef  CS752X_NI_NAPI
 #undef CONFIG_SMB_TUNING
#endif


#undef CS752X_NI_TX_COMPLETE_INTERRUPT
#ifdef CONFIG_SMB_TUNING
#define NAPI_GRO	1
#endif

#ifdef CONFIG_CS75XX_KTHREAD_RX
  #define NETIF_RX(napi,skb,dev)	{ netif_rx(skb); }
#else
  #ifdef CS752X_NI_NAPI
    #define NETIF_RX(napi,skb,dev)	{ if ((dev->features) & NETIF_F_GRO) napi_gro_receive(napi,skb); else netif_receive_skb(skb); }
  #else
    #define NETIF_RX(napi,skb,dev)	{ netif_rx(skb); }
  #endif
#endif

/* debug_Aaron on 2013/03/25 remove the CONFIG_INTR_COALESCING by default */
/* #ifndef CONFIG_INTR_COALESCING */
/* #define CONFIG_INTR_COALESCING 1 */
/* #endif */

#ifdef CONFIG_CS75XX_WFO
#include "cs_hw_accel_wfo.h"

#endif

/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
static int ni_rx_noncache = 0;
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
static int ni_napi_budget = 64;
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
static int ni_napi_budget = 16;
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

/* debug_Aaron 2013/05/15 implement non-cacheable for performace tuning  */
extern unsigned long consistent_base;

#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
int cs_ni_skb_recycle_max = 4096;
extern int br_fdb_test_forward(struct net_device *dev, unsigned char *addr);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
#define MIN_SKB_QUEUE_LEN 8 * 1024

/* debug_Aaron on 2013/03/11 for BUG#37961, when use RX non-cacheable buffer do not keep min skb queue */
static int ni_min_skb_queue_len = MIN_SKB_QUEUE_LEN;
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
int cs_ni_debug_dump_packet = 0;
ni_info_t ni_private_data;
static int ne_initialized = 0;
static int ne_irq_register = 0;
static int active_dev = 0;
spinlock_t active_dev_lock;

spinlock_t mdio_lock;
EXPORT_SYMBOL(mdio_lock);

u8 eth_mac[GE_PORT_NUM][6] = {{0x00, 0x50, 0xc2, 0x11, 0x22, 0x33},
				{0x00, 0x50, 0xc2, 0x44, 0x55, 0x66},
				{0x00, 0x50, 0xc2, 0x77, 0x88, 0x99}};
EXPORT_SYMBOL(eth_mac);

#define XRAM_PTR_WRAP_ARND(ptr, base, top_addr, new_value)	\
	if (((u32)ptr - (u32)base) >= ((top_addr + 1) << 3))	\
		(ptr = (u32*)new_value)

typedef enum {
	CPU_XRAM_CFG_PROFILE_SIMPLE_DEFAULT = 0,
	CPU_XRAM_CFG_PROFILE_SEPARATE_LSPID = 1,
	CPU_XRAM_CFG_PROFILE_PNI_WFO        = 2,
	CPU_XRAM_CFG_PROFILE_MAX
} cpu_xram_config_profile_e;

/*total rx+tx buf need to <= 2048*/
unsigned int
	cpuxram_rx_addr_cfg_prof[CPU_XRAM_CFG_PROFILE_MAX][XRAM_RX_INSTANCE + 1]
		= {	{581, 581, 581, 7, 7, 7, 7, 7, 56},
			{525, 525, 525, 7, 7, 7, 91, 91, 56},
			{448, 448, 448, 112, 112, 56, 56, 70, 70}};
unsigned int
	cpuxram_tx_addr_cfg_prof[CPU_XRAM_CFG_PROFILE_MAX][XRAM_TX_INSTANCE]
		= {	{214, 0},
			{214, 0},
			{214, 0}};

#ifndef CONFIG_CS75XX_WFO
/*total linux free buf need to <= 4096*/
u16 linux_free_buf_size_tbl[XRAM_RX_INSTANCE]
  	= {1 << 10, 1 << 10, 1 << 10, 0, 0,
		0, 1 << 6, 1 << 4};

#define XRAM_CFG_PROF	CPU_XRAM_CFG_PROFILE_SEPARATE_LSPID
#else
u16 linux_free_buf_size_tbl[XRAM_RX_INSTANCE]
	= {256, 256, 256, 256, 256, 256, 256, 256};

#define XRAM_CFG_PROF	CPU_XRAM_CFG_PROFILE_PNI_WFO
#endif

extern u32 cs_qos_preference;   //BUG#39672: 2. QoS

//u16 cpuxram_pkt_cnt[XRAM_RX_INSTANCE + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

extern int cs_ne_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
extern int cs_ni_init_proc(void);
extern void cs_ni_fini_proc(void);
extern void sch_init(void);
extern int cs_core_set_promiscuous_port(int port, int enbl);

static int cs_mdiobus_read(struct mii_bus *mii_bus, int phy_addr, int reg_addr);
static int cs_mdiobus_write(struct mii_bus *mii_bus, int phy_addr,
		int reg_addr, u16 val);
extern int netif_rx_cs(struct sk_buff *skb);
static u32 set_desc_word3_calc_l4_chksum(struct sk_buff *skb,
		struct net_device *dev, int tot_len, int frag_id);
extern u32 cs_hw_ipsec_offload_mode;

extern int qm_acp_enabled;
static int sw_qm_total_count[8] = { 0, 0, 0, 0, 0, 0, 0, 0};
spinlock_t sw_qm_cnt_lock;

/*recycle skb*/
#ifndef CONFIG_SMB_TUNING
#ifndef CONFIG_CS75XX_KTHREAD_RX
#define NI_RECYCLE_SKB_PER_CPU 1
struct sk_buff_head cs_ni_skb_recycle_cpu1_head;
#endif
#endif
struct sk_buff_head cs_ni_skb_recycle_cpu0_head;

/* skb recycling APi removed from mainline kernel */
#if 0 /*defined(CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT) */
static bool cs_ni_skb_recycle(struct sk_buff *skb);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
static int cs_ni_skb_recycle(struct sk_buff *skb);
static void cs_ni_prealloc_free_buffer(struct net_device *dev);
#endif /* ! CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

extern u32 cs_ni_min_rsve_mem;
extern u32 cs_ni_delay_ms_rsve_mem;
struct timer_list rx_fill_buffer_timer[XRAM_RX_INSTANCE];
static atomic_t cs_ni_instance_buf_low[XRAM_RX_INSTANCE];
int cs_ni_instance_buf_low_read(int ins)
{
	if (ins < 0 || ins >= XRAM_RX_INSTANCE) return -1;
	return atomic_read(&cs_ni_instance_buf_low[ins]);
}

void cs_ni_instance_buf_low_clear(void)
{
	int i;
	for (i=0; i < XRAM_RX_INSTANCE; ++i)
		atomic_set(&cs_ni_instance_buf_low[i], 0x7fff);
}

static void cs_ni_instance_buf_low_update(int ins, int val)
{
	int a = atomic_read(&cs_ni_instance_buf_low[ins]);
	if (val < a) atomic_set(&cs_ni_instance_buf_low[ins], val);
}

static const struct port_cfg_info {
	u8 auto_nego;
	u16 speed;
	u8 full_duplex;
	u8 flowctrl;
	u8 phy_mode;
	u8 port_id;
	u8 phy_addr;
	u8 irq;
	u8 *mac_addr;
	u8 rmii_clk_src;
} port_cfg_infos[] = {

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
#include <mach/custom_board/template/ni/cfg_ge_ports.h>

/* custom board patch end HERE: */

#else
	[GE_PORT0_CFG] = {
#if defined(CONFIG_CORTINA_FPGA)
		.auto_nego = AUTONEG_ENABLE,
		.speed = SPEED_100,
		.irq = IRQ_NET_ENG,
		.phy_mode = NI_MAC_PHY_RMII,
		.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
#elif defined(CONFIG_CORTINA_PON) || defined(CONFIG_CORTINA_BHR)
		.auto_nego = AUTONEG_DISABLE,
		.speed = SPEED_1000,
		.irq = IRQ_NI_RX_XRAM0,
		.phy_mode = NI_MAC_PHY_RGMII_1000,
		.flowctrl = 0,
#else

#if (GMAC0_PHY_MODE == NI_MAC_PHY_RGMII_1000)
		.speed = SPEED_1000,
#else
		.speed = SPEED_100,
#endif

#ifdef CONFIG_CS75XX_GMAC0_TO_EXT_SWITCH
		.auto_nego = AUTONEG_DISABLE,
		.flowctrl = 0,
#else
		.auto_nego = AUTONEG_ENABLE,
		// BUG#37140, disable Tx/Rx pause frame by default
		//.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.flowctrl = 0,
#endif

		.irq = IRQ_NI_RX_XRAM0,
		.phy_mode = GMAC0_PHY_MODE,
#endif
		.full_duplex = DUPLEX_FULL,
		.port_id = GE_PORT0,
		.phy_addr = GE_PORT0_PHY_ADDR,
		.mac_addr = (&(eth_mac[0][0])),
#if defined(CONFIG_CS75XX_GMAC0_RMII) && \
	defined(CONFIG_CS75XX_INT_CLK_SRC_RMII_GMAC0)
		.rmii_clk_src = 1,
#else
		.rmii_clk_src = 0,
#endif
	},
	[GE_PORT1_CFG] = {
#ifdef CONFIG_CORTINA_FPGA
		.auto_nego = AUTONEG_ENABLE,
		.speed = SPEED_100,
		.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.phy_mode = NI_MAC_PHY_RGMII_100,
		.irq = IRQ_NET_ENG,
#else

#if (GMAC1_PHY_MODE == NI_MAC_PHY_RGMII_1000)
		.speed = SPEED_1000,
#else
		.speed = SPEED_100,
#endif

#ifdef CONFIG_CS75XX_GMAC1_TO_EXT_SWITCH
		.auto_nego = AUTONEG_DISABLE,
		.flowctrl = 0,
#else
		.auto_nego = AUTONEG_ENABLE,
		// BUG#37140, disable Tx/Rx pause frame by default
		//.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.flowctrl = 0,
#endif

		.phy_mode = GMAC1_PHY_MODE,
		.irq = IRQ_NI_RX_XRAM1,
#endif
		.full_duplex = DUPLEX_FULL,
		.port_id = GE_PORT1,
		.phy_addr = GE_PORT1_PHY_ADDR,
		.mac_addr = (&(eth_mac[1][0])),
#if defined(CONFIG_CS75XX_GMAC1_RMII) && \
	defined(CONFIG_CS75XX_INT_CLK_SRC_RMII_GMAC1)
		.rmii_clk_src = 1,
#else
		.rmii_clk_src = 0,
#endif
	},
	[GE_PORT2_CFG] = {

#if defined(CONFIG_CORTINA_FPGA)
		.auto_nego = AUTONEG_ENABLE,
		.speed = SPEED_100,
		.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.phy_mode = NI_MAC_PHY_RGMII_100,
		.irq = IRQ_NET_ENG,
#elif defined(CONFIG_CORTINA_BHR)
		.auto_nego = AUTONEG_DISABLE,
		.speed = SPEED_1000,
		.irq = IRQ_NI_RX_XRAM2,
		.phy_mode = NI_MAC_PHY_RGMII_1000,
		.flowctrl = 0,
#else

#if (GMAC2_PHY_MODE == NI_MAC_PHY_RGMII_1000)
		.speed = SPEED_1000,
#else
		.speed = SPEED_100,
#endif

#ifdef CONFIG_CS75XX_GMAC2_TO_EXT_SWITCH
		.auto_nego = AUTONEG_DISABLE,
		.flowctrl = 0,
#else
		.auto_nego = AUTONEG_ENABLE,
		// BUG#37140, disable Tx/Rx pause frame by default
		//.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.flowctrl = 0,
#endif

		.phy_mode = GMAC2_PHY_MODE,
		.irq = IRQ_NI_RX_XRAM2,
#endif
		.full_duplex = DUPLEX_FULL,
		.port_id = GE_PORT2,
		.phy_addr = GE_PORT2_PHY_ADDR,
		.mac_addr = (&(eth_mac[2][0])),
#if defined(CONFIG_CS75XX_GMAC2_RMII) && \
	defined(CONFIG_CS75XX_INT_CLK_SRC_RMII_GMAC2)
		.rmii_clk_src = 1,
#else
		.rmii_clk_src = 0,
#endif
	}
#endif /* CONFIG_CORTINA_CUSTOM_BOARD end */
};

extern struct cs_ne_irq_info cs_ne_global_irq_info;

#if 0 /*defined(CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT)*/
static void clean_skb_recycle_buffer(void)
{
	struct sk_buff *skb;
	while ((skb = skb_dequeue(&cs_ni_skb_recycle_cpu0_head)) != NULL) {
		skb->skb_recycle = NULL;
		kfree_skb(skb);
	}
}
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
static void clean_skb_recycle_buffer(void * data){
#ifdef NI_RECYCLE_SKB_PER_CPU
	if (get_cpu() == 0) {
		skb_queue_purge(&cs_ni_skb_recycle_cpu0_head);
	} else {
		skb_queue_purge(&cs_ni_skb_recycle_cpu1_head);
	}
	put_cpu();
#else /* NI_RECYCLE_SKB_PER_CPU */
	skb_queue_purge(&cs_ni_skb_recycle_cpu0_head);
#endif /* NI_RECYCLE_SKB_PER_CPU */
}
EXPORT_SYMBOL(clean_skb_recycle_buffer);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

u32 calc_crc(u32 crc, u8 const *p, u32 len)
{
	int i;

	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
	}
	return crc;
}

#define TXQDMA_CLEAN_NUM	(NI_DMA_LSO_TXDESC_NUM >> 2)
static inline bool check_to_perform_tx_complete(int tx_qid)
{
	ni_info_t *ni = &ni_private_data;
	dma_swtxq_t *swtxq;
	dma_rptr_t rptr_reg;

	swtxq = &ni->swtxq[tx_qid];
	rptr_reg.bits32 = NI_READL_MODE(swtxq->rptr_reg);

	if (((rptr_reg.bits.rptr + swtxq->total_desc_num - swtxq->finished_idx)
				& (swtxq->total_desc_num - 1))
			>= TXQDMA_CLEAN_NUM)
		return true;
	else
		return false;
}


#ifdef CS752X_MANAGEMENT_MODE
/* this internal API is used by cs_mfe_start_xmit(), the management mode
 * FE xmit function */
static int ni_cpu_tx_packet(struct sk_buff *skb, u16 queue_id)
{
	HEADER_A_T ni_header_a;
	u16 pkt_len;

	if (skb == NULL)
		return -1;

	pkt_len = skb->len;

	/* Create and populate header A */
	memset(&ni_header_a, 0, sizeof(HEADER_A_T));
	ni_header_a.pkt_size = pkt_len;
	ni_header_a.fwd_type = CS_FWD_BYPASS; /* Always bypass */
	ni_header_a.pspid = 3; /* CPU */
	ni_header_a.dvoq = queue_id;
	ni_header_a.flags = 0x800;

	return ni_special_start_xmit(skb, &ni_header_a);
} /* ni_cpu_tx_packet */
#endif /* CS752X_MANAGEMENT_MODE */

/*
* Write MII register
* phy_addr -> physical address
* reg_addr -> register address
* value -> value to be write
*/
int ni_mdio_write(int phy_addr, int reg_addr, u16 value)
{
	u16 poll_time = 0;
	PER_MDIO_CFG_t cfg;
	PER_MDIO_ADDR_t addr;
	PER_MDIO_CTRL_t ctrl;
	PER_MDIO_WRDATA_t wrdata;
	//PER_GPIO0_CFG_t gpio_cfg;
	//PER_GPIO0_OUT_t gpio_out;

	if (phy_addr > PER_MDIO_MAX_PHY_ADDR) {
		printk("Invalid Phy Addr < 64");
		return -1;
	}

	if (reg_addr > PER_MDIO_MAX_REG_ADDR) {
		printk("Invalid MDIO Regiser Addr < 32");
		return -1;
	}

	cfg.wrd = NI_READL_MODE(PER_MDIO_CFG);
	/* Set the MDIO mode as Manual */
	cfg.bf.mdio_manual = 1;
	NI_WRITEL_MODE(cfg.wrd, PER_MDIO_CFG);
	addr.wrd = NI_READL_MODE(PER_MDIO_ADDR);
	/* Set the Operation as Write */
	addr.bf.mdio_rd_wr = mdio_rd_wr_WR;
	/* Set the MDIO Address */
	addr.bf.mdio_offset = reg_addr;
	/* Set the PHY Address */
	addr.bf.mdio_addr = phy_addr;
	NI_WRITEL_MODE(addr.wrd, PER_MDIO_ADDR);

	ctrl.wrd = NI_READL_MODE(PER_MDIO_CTRL);
	wrdata.wrd = NI_READL_MODE(PER_MDIO_WRDATA);
	wrdata.bf.mdio_wrdata = value;
	NI_WRITEL_MODE(wrdata.wrd, PER_MDIO_WRDATA);

	ctrl.bf.mdiodone = 0;
	/* Start the MDIO Operation */
	ctrl.bf.mdiostart = 1;
	NI_WRITEL_MODE(ctrl.wrd, PER_MDIO_CTRL);

	poll_time = PER_MDIO_POLL_TIME;
	do {
		ctrl.wrd = NI_READL_MODE(PER_MDIO_CTRL);
		if (ctrl.bf.mdiodone)
			break;
	} while (poll_time--);
	if (!poll_time)
		return -1;

	/* Clear MDIO done */
	ctrl.bf.mdiodone = 1;
	NI_WRITEL_MODE(ctrl.wrd, PER_MDIO_CTRL);

	return 1;
}
EXPORT_SYMBOL(ni_mdio_write);

/*
* Read MII register
* phy_addr -> physical address
* reg_addr -> register address
*/
int ni_mdio_read(int phy_addr, int reg_addr)
{
	int value = -1;
	u16 poll_time = 0;
	PER_MDIO_CFG_t cfg;
	PER_MDIO_ADDR_t addr;
	PER_MDIO_CTRL_t ctrl;
	//PER_GPIO0_CFG_t gpio_cfg;
	//PER_GPIO0_OUT_t gpio_out;

	if (phy_addr > PER_MDIO_MAX_PHY_ADDR) {
		printk("Invalid Phy Addr < 64");
		return -1;
	}

	if (reg_addr > PER_MDIO_MAX_REG_ADDR) {
		printk("Invalid MDIO Regiser Addr < 32");
		return -1;
	}

	cfg.wrd = NI_READL_MODE(PER_MDIO_CFG);
	/* Set the MDIO mode as Manual */
	cfg.bf.mdio_manual = 1;
	NI_WRITEL_MODE(cfg.wrd, PER_MDIO_CFG);
	addr.wrd = NI_READL_MODE(PER_MDIO_ADDR);
	/* Set the Operation as Read */
	addr.bf.mdio_rd_wr = mdio_rd_wr_RD;
	/* Set the MDIO Address */
	addr.bf.mdio_offset = reg_addr;
	/* Set the PHY Address */
	addr.bf.mdio_addr = phy_addr;
	NI_WRITEL_MODE(addr.wrd, PER_MDIO_ADDR);

	ctrl.wrd = NI_READL_MODE(PER_MDIO_CTRL);

	//ctrl.bf.mdiodone = 0;
	//printk("ctrl.bf.mdiodone = 0x%X\n",ctrl.bf.mdiodone);
	/* Start the MDIO Operation */
	ctrl.bf.mdiostart = 1;
	NI_WRITEL_MODE(ctrl.wrd, PER_MDIO_CTRL);

	poll_time = PER_MDIO_POLL_TIME;
	do {
		ctrl.wrd = NI_READL_MODE(PER_MDIO_CTRL);
		if (ctrl.bf.mdiodone)
			break;
	} while (poll_time--);
	if (!poll_time)
		return -1;

	value = NI_READL_MODE(PER_MDIO_RDDATA) & 0xffff;

	/* Clear MDIO done */
	ctrl.bf.mdiodone = 1;
	NI_WRITEL_MODE(ctrl.wrd, PER_MDIO_CTRL);

	return value;
}
EXPORT_SYMBOL(ni_mdio_read);



static inline void cs_ni_alloc_linux_free_buffer(struct net_device *dev,
						 int qid, int cnt)
{
	int i;
	struct sk_buff *skb;
	u32 phy_addr = 0;
	u32 hw_cnt;
	unsigned long free_page;

#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
	int is_skb_new_alloc = 0;
#endif /* ! CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

#ifdef CONFIG_DEBUG_KMEMLEAK
	ni_skb_list_t *skb_list_entry;
#endif
	cs_qm_get_cpu_free_buffer_cnt(qid, &hw_cnt);

	cnt = min_t(u16, linux_free_buf_size_tbl[qid] - sw_qm_total_count[qid],
			linux_free_buf_size_tbl[qid] - hw_cnt);
	DBG(printk("%s::alloc queue %d, buf cnt %d, hw_cnt %d, sw_ttl_cnt %d\n",
				__func__, qid, cnt, hw_cnt,
				sw_qm_total_count[qid]));
	cs_ni_instance_buf_low_update(qid, sw_qm_total_count[qid]);

	for (i = 0; i < cnt; i++) {
#ifdef	NI_RECYCLE_SKB_PER_CPU
		if (get_cpu() == 0)
		{
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT    // Bug#42574
			skb = __skb_dequeue(&cs_ni_skb_recycle_cpu0_head);
#else
			if(ni_min_skb_queue_len < skb_queue_len(&cs_ni_skb_recycle_cpu0_head))
				skb = __skb_dequeue(&cs_ni_skb_recycle_cpu0_head);
			else
				skb = NULL;
#endif
		}
		else
		{
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT    // Bug#42574
			skb = __skb_dequeue(&cs_ni_skb_recycle_cpu1_head);
#else
			if(ni_min_skb_queue_len < skb_queue_len(&cs_ni_skb_recycle_cpu1_head))
				skb = __skb_dequeue(&cs_ni_skb_recycle_cpu1_head);
			else
				skb = NULL;
#endif
		}
		put_cpu();
#else /* NI_RECYCLE_SKB_PER_CPU */
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		/* ...try to retrieve packet from recycle list first */
		skb = skb_dequeue(&cs_ni_skb_recycle_cpu0_head);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		// Bug 38880 - SW NAT throughput improvement when use_int_buffer=0 and QM_ACP off
		// 1. Keep the FIFO queue size >= MIN_SKB_QUEUE_LEN(8k)
		if(ni_min_skb_queue_len < skb_queue_len(&cs_ni_skb_recycle_cpu0_head))
			skb = skb_dequeue(&cs_ni_skb_recycle_cpu0_head);
		else
			skb = NULL;
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
#endif /* NI_RECYCLE_SKB_PER_CPU */
		if (skb == NULL) {

#define K(x) ((x) << (PAGE_SHIFT - 10))
			free_page = global_page_state(NR_FREE_PAGES);
			if ( K(free_page) < cs_ni_min_rsve_mem){
				printk_ratelimited(KERN_WARNING "%s.%d: Low memory, delaying buffer allocation\n",
					dev->name, qid);
				break;
			}

			/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
			if (ni_rx_noncache)
		 		skb = dev_alloc_skb_uncachable(SKB_PKT_LEN + 0x100);
			else
				skb = netdev_alloc_skb(dev, SKB_PKT_LEN + 0x100);
#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
			is_skb_new_alloc = 1;
#endif /* !CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		} else {
#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
			skb_reserve(skb, NET_SKB_PAD);
#endif /* !CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			skb->dev = dev;
		}

		/* 0x100 is reserve space */
		if (!skb) {
			pr_warning("%s: Could only allocate %d receive skb(s).\n", dev->name, cnt);
			break;
		}
#if 0 /*defined(CONFIG_SMB_TUNING)*/
		skb->skb_recycle = cs_ni_skb_recycle;
#endif

		/* first 256 bytes aligned address from skb->head */
		skb->data = (unsigned char *)((u32)
				(skb->head + 0x100) & 0xffffff00);
#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		/*
		 * Bug 37583: Cache invalidate - Header XR and Header E
		 * before  buffer recycle.
		 */

		// Bug 38880 - SW NAT throughput improvement when use_int_buffer=0 and QM_ACP off
		// 2. Use large FIFO queue to save invalidation time
		if ( qm_acp_enabled == 0 && ni_rx_noncache == 0 && is_skb_new_alloc == 1){  //QM_ACP is not enabled if use external DDR
                        dma_map_single(NULL, skb->data, 64, DMA_FROM_DEVICE);
                }
#endif /* !CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
		if (ni_rx_noncache){    //QM_ACP is not enabled if use external DDR
			/* Invalidate one cache line where will be skb pointer be saved */
			dma_map_single(dev, phys_to_virt(skb->head_pa) + 224, 32, DMA_FROM_DEVICE);
		}

		/* ...that memory region is not used by hardware, so it may remain "dirty" */
		REG32((u32)(skb->data - 4)) = (u32)skb;
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		skb_reset_tail_pointer(skb);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
#ifdef NET_SKBUFF_DATA_USES_OFFSET
		skb->tail = skb->data - skb->head;
#else
		skb->tail = skb->data;
#endif
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		 /* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
                if (ni_rx_noncache)
			phy_addr = skb->head_pa;
		else
			phy_addr = virt_to_phys(skb->head);

#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		/* Write to SDRAM address, physical adress */
		NI_WRITEL_MODE(phy_addr, QM_CPU_PATH_LINUX0_SDRAM_ADDR + (qid * 4));

		/* ...mark the buffer is not dirty (yet) */
		skb->dirty_buffer = 0;

		/* ...map first two cache lines (headers) */
		if ((qm_acp_enabled == 0) && (ni_rx_noncache == 0))
			dma_map_single(NULL, skb->data, 64, DMA_FROM_DEVICE);

		/* ...set mapping end address */
		skb->map_end = skb->data + 64;
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
#ifndef ACP_QM
		/* Invalid inner cache */
		//dmac_flush_range(skb->data, skb->data + SKB_PKT_LEN + 0x100);
		/* Invalid outer cache */
		//outer_flush_range(__pa(skb), __pa(skb)+SKB_PKT_LEN);
#endif

#ifdef CONFIG_CORTINA_FPGA
		phy_addr &= 0x0FFFFFFF;
#endif

		/* Write to SDRAM address, physical adress */
		NI_WRITEL_MODE(phy_addr, QM_CPU_PATH_LINUX0_SDRAM_ADDR + (qid * 4));

#ifdef CONFIG_DEBUG_KMEMLEAK
		skb_list_entry = cs_malloc(sizeof(ni_skb_list_t), GFP_ATOMIC);
		memset(skb_list_entry, 0x0, sizeof(ni_skb_list_t));
		skb_list_entry->skb = skb;
		list_add(&skb_list_entry->list,
				&ni_private_data.ni_skb_list_head[qid]);
#endif
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
	}
	sw_qm_total_count[qid] += i;
}

void write_reg(u32 data, u32 bit_mask, u32 address)
{
	volatile u32 reg_val;

	reg_val = (NI_READL_MODE(address) & (~bit_mask)) | (data & bit_mask);
	NI_WRITEL_MODE(reg_val, address);
	return;
}

#if defined(CONFIG_NET_POLL_CONTROLLER) || defined(CONFIG_CORTINA_FPGA) || defined(CONFIG_GENERIC_IRQ)
/* Handles NE interrupts. */
static irqreturn_t ni_generic_interrupt(int irq, void *dev_instance)
{
	u32 status;

	status = NI_READL_MODE(GLOBAL_NETWORK_ENGINE_INTERRUPT_0);
	NI_WRITEL_MODE(status, GLOBAL_NETWORK_ENGINE_INTERRUPT_0);

	if (status) {
#ifdef CONFIG_CS752X_PROC
		//if (cs_ni_debug & DBG_IRQ)
		//	printk(KERN_INFO "%s:: status %x\n", __func__, status);
#endif
		cs_ne_global_intr_handle(0, &cs_ne_global_irq_info, status);
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}
#endif

#ifdef CS_UU_TEST
static u8 cs_ni_dvoq_to_pspid(u8 d_voq)
{
	u8 pspid;

	/* at this moment, we only have 1 state machine assigned to each
	 * GE ports */
	if ((d_voq >= CPU_PORT0_VOQ_BASE) && (d_voq < CPU_PORT1_VOQ_BASE))
		return GE_PORT0;
	else if ((d_voq >= CPU_PORT1_VOQ_BASE) && (d_voq < CPU_PORT2_VOQ_BASE))
		return GE_PORT1;
	else if ((d_voq == CPU_PORT2_VOQ_BASE) && (d_voq < CPU_PORT3_VOQ_BASE))
		pspid = GE_PORT2;
	else
		printk("Not come from GE-0, GE-1, GE-2 \n");

	return pspid;
}
#endif

static inline struct net_device *get_dev_by_cpu_hdr(CPU_HEADER0_T *cpu_hdr_0,
		struct net_device *org_dev)
{
#ifdef CS_UU_TEST
	u8 o_pspid;
#endif

	if (cpu_hdr_0->bits.pspid > 2) {
		DBG(printk("%s::pspid %d\n", __func__, cpu_hdr_0->bits.pspid));
#ifdef CONFIG_CS75XX_WFO
		if (cpu_hdr_0->bits.pspid == PE0_PORT)
			return ni_private_data.dev[CS_NI_IRQ_WFO_PE0];

		if (cpu_hdr_0->bits.pspid == PE1_PORT)
			return ni_private_data.dev[CS_NI_IRQ_WFO_PE1];
#else
		if (cpu_hdr_0->bits.pspid == PE0_PORT)
			return ni_private_data.dev[CS_NI_IRQ_PE];
#endif		
#ifdef CS_UU_TEST
		if (cpu_hdr_0->bits.pspid == 6) {
			/* 6: MCast */
			/*
			 * Note!! In the future, we will need to maintain a
			 * DVOQ to O_LSPIDmapping table.
			 */
			o_pspid = cs_ni_dvoq_to_pspid(cpu_hdr_0->bits.dst_voq);
			DBG(printk("%s::pspid %d, d_voq = %d, o_pspid = %d\n",
				   __func__, cpu_hdr_0->bits.pspid,
				   cpu_hdr_0->bits.dst_voq, o_pspid));
			return ni_private_data.dev[o_pspid];
		}
#endif /* CS_UU_TEST */
	} else if (ni_private_data.dev[cpu_hdr_0->bits.pspid] != NULL) {
		DBG(printk("%s::cpu_hdr_0->bits.pspid = %d",
			__func__, cpu_hdr_0->bits.pspid));
		return ni_private_data.dev[cpu_hdr_0->bits.pspid];
	}
	return org_dev;
}

#ifdef CONFIG_DEBUG_KMEMLEAK
static int ni_remove_skb_from_list(u32 instance, struct sk_buff *skb)
{
	struct list_head *next;
	ni_skb_list_t *curr_skb_entry;

	list_for_each(next, &ni_private_data.ni_skb_list_head[instance]) {
		curr_skb_entry = (ni_skb_list_t *)list_entry(next,
				ni_skb_list_t, list);
		if (curr_skb_entry->skb == skb) {
			list_del(&curr_skb_entry->list);
			cs_free(curr_skb_entry);
			return 0;
		}
	}
	return 1;
} /* ni_remove_skb_from_list */
#endif

#include "../../../../../../net/bridge/br_private.h"
static int cs_ni_start_xmit(struct sk_buff *skb, struct net_device *dev);
#define CS_MOD_BRIDGE_SWID (CS_SWID64_MASK(CS_SWID64_MOD_ID_BRIDGE) | 0x49444745)
/* try fast bridgine, cxc */
int cs752x_fast_bridging(struct sk_buff *skb)
{
	struct net_bridge_fdb_entry *br_fdb_da = NULL, *br_fdb_sa = NULL;
	struct net_bridge_port *br_port;
	struct net_device *dst_nic;
	unsigned char *sa, *da;
	int rc;
	u16 vid;

	da = (unsigned char*)(skb->mac_header);
	sa = da + ETH_ALEN;

	br_port = br_port_get_rcu(skb->dev);
	if (br_port == NULL)
		return 1;

	if (!br_allowed_ingress(br_port->br, br_vlan_group_rcu(br_port->br), skb, &vid))
		return 1;

	br_fdb_da = __br_fdb_get(br_port->br, da, vid);
	if (br_fdb_da == NULL)
		goto no_hash_1;

	br_fdb_sa = __br_fdb_get(br_port->br, sa, vid);
	if (br_fdb_sa == NULL)
		goto no_hash_2;

	if (br_fdb_da->is_local)
		goto not_local;

	dst_nic = br_fdb_da->dst->dev;
#ifdef CONFIG_CS752X_ACCEL_KERNEL
	/* if source dest dev are both cs device and bridge accel enable,
	 * we will use hw acceleration*/
	if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_BRIDGE)) {
		cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
		if ((cs_cb) && (cs_cb->common.tag == CS_CB_TAG) &&
			(cs_cb->common.sw_only != CS_SWONLY_STATE)) {
			cs_cb->common.module_mask |= CS_MOD_MASK_BRIDGE;
			cs_cb->common.output_dev = (struct net_device *)dst_nic;
			cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;
			cs_core_logic_add_swid64(cs_cb, CS_MOD_BRIDGE_SWID);
			cs_core_logic_set_lifetime(cs_cb, br_port->br->ageing_time / HZ);
		}
	}
#endif
	br_fdb_sa->updated = jiffies;
	skb->dev = dst_nic;
	skb->data = skb->data - ETH_HLEN;
	skb->len += ETH_HLEN;

	netif_tx_lock(dst_nic);
#ifdef CONFIG_CS752X_PROC
	if (cs_ni_fastbridge & 0x01){
		rc =  dst_nic->netdev_ops->ndo_start_xmit(skb, dst_nic);
	}
	else if (cs_ni_fastbridge & 0x02) {
		rc = dev_queue_xmit(skb);
	}
#else
	rc =  dst_nic->netdev_ops->ndo_start_xmit(skb, dst_nic);
#endif
	if (rc != 0) {
		printk("fast_bridge fail %d\n",rc);
		/*if bridge fast xmit fail, we go normal kernel path*/
		skb->data += ETH_HLEN;
		skb->len -= ETH_HLEN;
		skb->dev = br_port->dev;
		netif_tx_unlock(dst_nic);
		return 1;
	}
	netif_tx_unlock(dst_nic);

	return 0;

not_local:
//	br_fdb_put(br_fdb_sa);
no_hash_2:
//	br_fdb_put(br_fdb_da);
no_hash_1:
	return 1;
}
EXPORT_SYMBOL(cs752x_fast_bridging);

u8 cs_ni_get_port_id(struct net_device * dev)
{
#if 1
	mac_info_t *tp = netdev_priv(dev);
	return tp->port_id;
#else
	int i;
	for (i = 0; i <GE_PORT_NUM ; i++) {
		if (dev == ni_private_data.dev[i]) {
			return i;
			break;
		}
	}
#ifdef CONFIG_CS75XX_WFO
	/*TBA: need PNIC to store net_device pointer and then
	 *     we can assign port_id by searching pnic
	 *     PORT_ID=4 ==> PE#0
	 *     PORT_ID=5 ==> PE#1
	 */
#endif
	return -1;
#endif	
}
EXPORT_SYMBOL(cs_ni_get_port_id);

#define HDRA_CPU_PKT	0xc30001
static int get_dma_lso_txqid(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	int lso_tx_qid = 0;
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	mac_info_t *phy_tp;
	cs_virt_ni_t *virt_ni_ptr;
#endif

#ifndef CONFIG_PREEMPT_NONE
	get_cpu();
#endif

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
//#ifdef CONFIG_SMP
	lso_tx_qid = (tp->port_id << 1) + smp_processor_id();
#else
	lso_tx_qid = tp->port_id;
#endif

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	/* This can be modified later for different port setting;
	 * however, since right now, VIRTUAL NETWORK Interface only works
	 * on Reference board where only DMA LSO TXQ#0 is being used by eth0,
	 * we can use the rest of TXQ#1~5 */
	virt_ni_ptr = cs_ni_get_virt_ni(tp->port_id, dev);
	if (virt_ni_ptr != NULL) {
		/* the current method to spread the load is per "switch port"
		 * based. different interfaces might still use the same LSO
		 * TXQ, but we try our best to spread out */
		phy_tp = (mac_info_t *)netdev_priv(tp->dev);
		if (phy_tp != NULL) {
			/*
			 * example eth1_3: tp->port_id = 3, phy_tp->port_id = 1
			 * lso_tx_qid @ cpu0 = 3*2 + 1*4 = 10 mod 6 = 4
			 * lso_tx_qid @ cpu1 = 3*2 + 1 + 1*4 = 11 mod 6 = 5
			 */
			lso_tx_qid = (phy_tp->port_id << 2) + lso_tx_qid;
			lso_tx_qid = lso_tx_qid % 6;
		}
	}
#endif

#ifndef CONFIG_PREEMPT_NONE
	put_cpu();
#endif

	return lso_tx_qid;
} /* get_dma_lso_txqid */

static u32  sw_next_link[8] = {
	(u32)-1, (u32)-1, (u32)-1, (u32)-1, (u32)-1, (u32)-1, (u32)-1, (u32)-1,
};

void cs_ni_fill_recycle_queue(u32 fill_size)
{
	int i;
	struct sk_buff *skb;
	for(i = 0; i < fill_size; i++)
	{
		if(fill_size < skb_queue_len(&cs_ni_skb_recycle_cpu0_head))
                        break;
		skb = netdev_alloc_skb(ni_private_data.dev[0], SKB_PKT_LEN + 0x100);
		if (!skb) {
			break;
		}
		dma_map_single(NULL, skb->data, SKB_PKT_LEN, DMA_FROM_DEVICE);
		skb_queue_tail(&cs_ni_skb_recycle_cpu0_head, skb);
	}
	printk("skb_queue_len=%d\n", skb_queue_len(&cs_ni_skb_recycle_cpu0_head));
}

EXPORT_SYMBOL(cs_ni_fill_recycle_queue);

/*
 * Receive packets on any CPU VoQs, and forward to dev per pspid.
 */
static int ni_complete_rx_instance(struct net_device *dev, u32 instance,
				   int budget, struct napi_struct *napi)
{
	HEADER_XR_T *hdr_x;
	HEADER_E_T xram_hdr_e;
	CPU_HEADER0_T xram_cpu_hdr0;
	CPU_HEADER1_T xram_cpu_hdr1;
	struct sk_buff *skb, *tmp_skb = NULL, *tail_skb = NULL;
	u32 hw_wr_ptr, next_link, tmp_data, *tmp_ptr, refill_cnt = 0;
	u32 jumbo_pkt_index = 0, seg_len, *xram_ptr = (u32 *)NI_XRAM_BASE;
	mac_info_t *tp = NULL;
	int pkt_len, done = 0;
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	struct net_device *in_dev = dev;
#endif
	dma_addr_t rx_dma_addr;
#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_kernel_accel_cb_t *cs_cb = NULL;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	int err_virt_ni = 0;
#endif

	cs_app_pkt_type_t packet_type = CS_APP_PKT_NORMAL;
	int voq;

	hw_wr_ptr = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_STA_RX_0 + (instance * 24)));
	hw_wr_ptr &= HW_WR_PTR;
	next_link = sw_next_link[instance];
	if ((int)(next_link) < 0) {
		next_link = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_CFG_RX_0 + (instance * 24)));
		next_link &= SW_RD_PTR;
	}
	xram_ptr += next_link * 2;

	while ((next_link != hw_wr_ptr) && (done < budget)) {
		hdr_x = (HEADER_XR_T *)xram_ptr;
		if (hdr_x->bits.ownership != 0) {
			printk("%s::Ownership by HW!\n", __func__);
			break;
		}
		next_link = hdr_x->bits.next_link;
		/*
		 * When sw pushes a buffer to qm,
		 * qm will write data at next 256 byte aligned address
		 * so if say skb->head is 0xc000a010, HW will write to 0xc000a100
		 */
		tmp_data = ((*(xram_ptr + 10)) & 0xffffff00) + 0x100 - 4;
		tmp_ptr = (u32 *)(phys_to_virt(tmp_data + GOLDENGATE_DRAM_BASE));
		skb = (struct sk_buff *)(*tmp_ptr);
		if (unlikely(skb == NULL)) {
			printk("%s:%d:something is not right! skb@0x%x, "
					"tmp_ptr@0x%x\n", __func__, __LINE__,
					(u32)skb, (u32)tmp_ptr);
			xram_ptr = (u32 *)NI_XRAM_BASE + next_link * 2;
			refill_cnt++;
			continue;
		}

#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		/* ...invalidate first two cache-lines */
		if ((qm_acp_enabled == 0) && (ni_rx_noncache == 0))
			dma_sync_single_for_cpu(NULL, virt_to_phys(skb->data),
					2 * SMP_CACHE_BYTES, DMA_FROM_DEVICE);

		/*
		 * Bug 29858: Read packet header from DDR (skb) instead of XRAM
		 * since XRAM content may be invalid and contain data from
		 * a previous packet.
		 */
		xram_hdr_e.bits32[0] = REG32((u32)(skb->data + 4));
		xram_hdr_e.bits32[1] = REG32((u32)(skb->data));

		if (xram_hdr_e.bits.cpu_header != 0) {
			xram_cpu_hdr0.bits32[0] = REG32((u32)(skb->data + 12));
			xram_cpu_hdr0.bits32[1] = REG32((u32)(skb->data + 8));
			xram_cpu_hdr1.bits32[0] = REG32((u32)(skb->data + 20));
			xram_cpu_hdr1.bits32[1] = REG32((u32)(skb->data + 16));
		}

		if (next_link == hw_wr_ptr) {
			/* Check if more packets were received while processing */
			u32 hw_wr_ptr_cur;
			hw_wr_ptr_cur = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_STA_RX_0 + (instance * 24)));
			hw_wr_ptr_cur &= HW_WR_PTR;
			if (hw_wr_ptr_cur == hw_wr_ptr) {
				/*
				 * If this is the very last packet in a queue
				 * wait 1us to make sure that packet data has
				 * arrived to DDR.
				 */
				udelay(16);
			}
		}

#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		/* WARNING *(xram_ptr+offset) may fail if xram instance size is
		 * not 56 bytes aligned, when CPU header is on!
		 * i.e., if RE sends a packet to NI w/FE_BYPASS mode, we will
		 * only see 40 bytes xram instead of 56 bytes, thus the
		 * alignment is gone. */
		if (likely(qm_acp_enabled == 0)) {
			xram_hdr_e.bits32[0] = *(xram_ptr + 3);
			xram_hdr_e.bits32[1] = *(xram_ptr + 2);

			if (xram_hdr_e.bits.cpu_header != 0) {
				xram_cpu_hdr0.bits32[0] = *(xram_ptr + 5);
				xram_cpu_hdr0.bits32[1] = *(xram_ptr + 4);
				xram_cpu_hdr1.bits32[0] = *(xram_ptr + 7);
				xram_cpu_hdr1.bits32[1] = *(xram_ptr + 6);
			}
		}
		else {
			/*
			 * Bug 29858: Read packet header from DDR (skb) instead of XRAM
			 * since XRAM content may be invalid and contain data from
			 * a previous packet.
			 */
			xram_hdr_e.bits32[0] = REG32((u32)(skb->data + 4));
			xram_hdr_e.bits32[1] = REG32((u32)(skb->data));

			if (xram_hdr_e.bits.cpu_header != 0) {
				xram_cpu_hdr0.bits32[0] = REG32((u32)(skb->data + 12));
				xram_cpu_hdr0.bits32[1] = REG32((u32)(skb->data + 8));
				xram_cpu_hdr1.bits32[0] = REG32((u32)(skb->data + 20));
				xram_cpu_hdr1.bits32[1] = REG32((u32)(skb->data + 16));
			}

			if (next_link == hw_wr_ptr) {
				/* Check if more packets were received while processing */
				u32 hw_wr_ptr_cur;
				hw_wr_ptr_cur = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_STA_RX_0 + (instance * 24)));
				hw_wr_ptr_cur &= HW_WR_PTR;
				if (hw_wr_ptr_cur == hw_wr_ptr) {
					/*
					 * If this is the very last packet in a queue
					 * wait 1us to make sure that packet data has
					 * arrived to DDR.
					 */
					udelay(16);
				}
			}
		}
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		DBG(printk("In VoQ %d, dst VoQ %d\n",
			xram_cpu_hdr0.bits.cpuhdr_voq,
			xram_cpu_hdr0.bits.dst_voq));

#ifdef CONFIG_DEBUG_KMEMLEAK
		/* buffer is used.. remove it from the list */
		ni_remove_skb_from_list(instance, skb);
#endif
		pkt_len = xram_hdr_e.bits.pkt_size;
//#ifndef ACP_QM
		/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
		if ((qm_acp_enabled == 0) && (ni_rx_noncache == 0)){
			//QM_ACP is not enabled if use external DDR
			seg_len = min_t(u16, SKB_PKT_LEN, pkt_len);
			rx_dma_addr = virt_to_phys(skb->data);
#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
			/* 3.4.11 pass NULL to force to do arm dma map if QM ACP disabled */
			dma_sync_single_for_cpu(NULL, rx_dma_addr,
				seg_len + 16, DMA_FROM_DEVICE);
			//dma_map_single(NULL, skb->data, pkt_len, DMA_FROM_DEVICE);
#endif /* !CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		}
//#endif
		if (hdr_x->bits.error_flag != 0) {
			/* error happens with this packet, jump to next
			 * packet! */
			dev_kfree_skb(skb);
			xram_ptr = (u32 *)NI_XRAM_BASE + next_link * 2;
			refill_cnt++;
			continue;
		}

		skb->data += sizeof(HEADER_E_T);

		/* the following if statement might not be satisfied in a rare
		 * case that when RE sends a packet marked with FE_BYPASS, CPU
		 * headers will not be inserted. */
		if (likely(xram_hdr_e.bits.cpu_header != 0)) {
			skb->data += sizeof(CPU_HEADER0_T) + sizeof(CPU_HEADER1_T);
			pkt_len -= sizeof(CPU_HEADER0_T) + sizeof(CPU_HEADER1_T);
		}

		/* linux 2 byte alignment */
		skb->data += 2;

		/* decrease pkt len by CRC length */
		pkt_len -= 4;

		seg_len = MAX_QM_SEG_LEN - ((u32)skb->data & 0xff);

		DBG(printk("%s::seg len %d\n", __func__,
			min_t(u16, pkt_len, seg_len)));
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		skb_reset_tail_pointer(skb);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
#ifdef NET_SKBUFF_DATA_USES_OFFSET
		skb->tail = skb->data - skb->head;
#else
		skb->tail = skb->data;
#endif
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		seg_len = min_t(u16, pkt_len, seg_len);
		skb_put(skb, seg_len);
		skb->len = pkt_len;
		pkt_len -= seg_len;
		skb->data_len = pkt_len;
		jumbo_pkt_index = 0;
		refill_cnt++;
		while (pkt_len > 0) {
			tmp_data = ((*(xram_ptr + 11 + jumbo_pkt_index)) &
					0xffffff00) + 0x100 - 4;
			tmp_ptr = (u32 *)(phys_to_virt(tmp_data
						+ GOLDENGATE_DRAM_BASE));
			tmp_skb = (struct sk_buff *)(*tmp_ptr);
#ifdef CONFIG_DEBUG_KMEMLEAK
			/* buffer is used.. remove it from the list */
			ni_remove_skb_from_list(instance, tmp_skb);
#endif
			seg_len = min_t(u16, SKB_PKT_LEN, pkt_len);
//#ifndef ACP_QM
			/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
			if ((qm_acp_enabled == 0) && (ni_rx_noncache == 0)){
				//QM_ACP is not enabled if use external DDR
				rx_dma_addr = virt_to_phys(tmp_skb->data);
				/* 3.4.11 pass NULL to force to do arm dma map if QM ACP disabled */
				dma_sync_single_for_cpu(NULL,
						rx_dma_addr, seg_len + 16,
						DMA_FROM_DEVICE);
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
				tmp_skb->dirty_buffer = 1;
				tmp_skb->map_end = NULL;
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			}
//#endif
			tmp_skb->len = 0;
			skb_put(tmp_skb, seg_len);
			tmp_skb->next = NULL;
			tmp_skb->prev = NULL;
			if (jumbo_pkt_index == 0) {
				skb_shinfo(skb)->frag_list = tmp_skb;
				tail_skb = tmp_skb;
			} else {
				tail_skb->next = tmp_skb;
				tail_skb = tmp_skb;
			}
			pkt_len -= seg_len;
			jumbo_pkt_index++;
			refill_cnt++;
		}
#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		skb->next = NULL;
		skb->prev = NULL;
#endif /* !CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		if (dev == NULL) {
			if (xram_hdr_e.bits.cpu_header != 0)
				skb->dev = get_dev_by_cpu_hdr(&xram_cpu_hdr0,
						ni_private_data.dev[0]);
			else
				skb->dev = ni_private_data.dev[0];
			dev = skb->dev;
		} else {
			skb->dev = dev;
		}

		skb->ip_summed = CHECKSUM_NONE;
		tp = netdev_priv(skb->dev);

		if ((instance < 3) &&
			(xram_cpu_hdr1.bits.sw_action == 1)) {

			/* If we don't care L7 data content
			 * So we don't need to map all data size
			 */
			u32 map_len = SKB_DATA_ALIGN(skb->tail - skb->map_end);
			dma_map_single(NULL, skb->map_end,
							(map_len>64)? 64: map_len,
							DMA_FROM_DEVICE);

			skb->ip_summed = CHECKSUM_UNNECESSARY;
			skb->protocol = eth_type_trans(skb, skb->dev);
			skb->pkt_type = PACKET_HOST;

			tp->ifStatics.rx_packets++;
			tp->ifStatics.rx_bytes += skb->len;

			//if(skb->len > 1400){
			//	dev_kfree_skb(skb);
			//	goto SKB_HANDLED;
			//}
			NETIF_RX(napi, skb, dev);

			goto SKB_HANDLED;
		}
		if ((instance < 3) &&
			(xram_cpu_hdr1.bits.sw_action == 2)) {
				extern u64 cs_localin_udp_payload_drop_bytes;
				extern u64 cs_localin_udp_traffic_drop_bytes;
				extern u64 cs_localin_udp_drop_packets;
				cs_localin_udp_drop_packets++;
				cs_localin_udp_traffic_drop_bytes += skb->len;
				struct ethhdr *eth;
				eth = skb->data;
				u32 hdr_len = 0;
				if (eth->h_proto == 0x0008) {
					hdr_len = 14 + 20 + 8;
				} else {
					hdr_len = 18 + 20 + 8;
					eth = (struct ethhdr *) ((char *) skb->data + 18);
					if (eth->h_proto != 0x0008) {
						hdr_len += 4; 
					}
				}
				
				cs_localin_udp_payload_drop_bytes += (skb->len - hdr_len);
				
				tp->ifStatics.rx_packets++;
				tp->ifStatics.rx_bytes += skb->len;
				dev_kfree_skb(skb);
				goto SKB_HANDLED;
		}
	/*
	 * Need to get cs cb information before remove Virtual interface tag
	 */
#ifdef CONFIG_CS752X_ACCEL_KERNEL
		if (cs_accel_kernel_enable() != 0) {
			if (cs_accel_cb_add(skb) == 0) {
				cs_cb = CS_KERNEL_SKB_CB(skb);
#ifdef CONFIG_CS75XX_WFO
				/*
				 *if packet is 802.11 , don't need to set cs_cb
				 */
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
				if (instance != CS_NI_INSTANCE_WFO)
#else
				if ((instance != CS_NI_INSTANCE_WFO_PE0)
					&& (instance != CS_NI_INSTANCE_WFO_PE1))
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
#endif
				cs_core_logic_input_set_cb(skb);
				// FIXME: pspid <--> lspid one to one map
				cs_cb->key_misc.orig_lspid = xram_hdr_e.bits.lspid;
				cs_cb->key_misc.lspid = cs_cb->key_misc.orig_lspid;
				cs_cb->common.ingress_port_id = cs_cb->key_misc.orig_lspid;
				cs_cb->common.in_ifindex = dev->ifindex;
#ifdef CONFIG_CS75XX_WFO
        		if (xram_hdr_e.bits.cpu_header) {
        			if (xram_cpu_hdr0.bits.dst_voq == CPU_PORT7_VOQ_BASE + 1) {
            			/*
            			 * WFO IPMC MCAL table full, all the packets goes through kernel
            			 */
            			DBG(printk("%s:%d:: IPMC CPU_PORT7_VOQ_BASE + 1, instance %d \n", __func__, __LINE__, instance));
            			cs_cb->common.sw_only = CS_SWONLY_STATE;
            		}
        		}
#endif

#ifdef CONFIG_CS752X_FASTNET
				cs_cb->key_misc.super_hash = xram_cpu_hdr1.bits.superhash;
				cs_cb->key_misc.super_hash_vld = xram_cpu_hdr1.bits.superhash_vld;
#endif /* CONFIG_CS752X_FASTNET */
			}
		}
		voq = xram_cpu_hdr0.bits.dst_voq;
		
		DBG(printk("%s:%d:: LSPID = %d, , VoQ = %d, instance = %d\n",
			__func__, __LINE__,
			xram_hdr_e.bits.lspid,
			voq,
			instance));
#ifdef CS75XX_HW_ACCEL_TUNNEL
		if (voq == CPU_PORT7_VOQ_BASE + 2) {
			/* tunnel control packets from VoQ 106 */
			if (cs_cb)
				cs_cb->common.sw_only = CS_SWONLY_STATE;

			cs_hw_accel_tunnel_ctrl_handle(voq, skb);
			goto SKB_HANDLED;
		}
#endif

		/*
		 * Check for etherhook, and possible let the hook
		 * take ownership of the skb
		 */
		if ((instance < 3)
			&& etherhook_has_hook(xram_cpu_hdr1.bits.sw_action)) {
			dma_map_single(NULL, skb->map_end, SKB_DATA_ALIGN(skb->tail - skb->map_end), DMA_FROM_DEVICE);
			etherhook_rx_skb(skb);
			goto SKB_HANDLED;
		}

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
        // Fill packet_type
        u32 func_type = xram_cpu_hdr1.bits.sw_action >> 16;
        switch(func_type) {
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
            case CS_FUNC_TYPE_WIRELESS_ACCEL_11AC: 
    		    packet_type = CS_APP_PKT_WIRELESS;
                break;

            case CS_FUNC_TYPE_WIRELESS_ACCEL_11N: 
                break;
#endif //CONFIG_CS75XX_HW_ACCEL_WIRELESS	

#ifdef CONFIG_CS75XX_WFO
            case CS_FUNC_TYPE_WFO_11AC:
            case CS_FUNC_TYPE_WFO_11N:
                packet_type = CS_APP_PKT_WFO;
                break;
#endif //CONFIG_CS75XX_WFO

            case CS_FUNC_TYPE_VPN_OFFLOAD_PE0: 
                break;

            case CS_FUNC_TYPE_VPN_OFFLOAD_PE1: 
                break;

            case CS_FUNC_TYPE_IPLIP_OFFLOAD_PE1: 
                break;
            
            default:
        		if (instance == CS_NI_INSTANCE_WFO) {
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS				
        			if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_WFO)) 
        				packet_type = CS_APP_PKT_WFO;
        			else
        				packet_type = CS_APP_PKT_WIRELESS;
#else					
			        packet_type = CS_APP_PKT_WFO;
#endif					
    			} else if (((voq == CPU_PORT7_VOQ_BASE) &&
			                ((xram_hdr_e.bits.lspid == CS_FE_LSPID_4)  ||
			                (xram_hdr_e.bits.lspid == CS_FE_LSPID_5))) ||
			               ((instance == CS_NI_INSTANCE_WFO_802_3_PE0) ||
			                (instance == CS_NI_INSTANCE_WFO_802_3_PE1))) {
                    
    	           	if (voq == CPU_PORT7_VOQ_BASE) {
        				voq = CPU_PORT5_VOQ_BASE;
    	           	}
                    if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_WFO))
                        packet_type = CS_APP_PKT_WFO;
                    else
                        packet_type = CS_APP_PKT_TUNNEL;

        			if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPSEC)) {
        			    //TODO: 
        			}

        			if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_TUNNEL)) {
        			    //TODO: 
        			}
			    }
                break;
        } /* switch(func_type) */
#else	//CONFIG_CS75XX_OFFSET_BASED_QOS
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
		if (xram_hdr_e.bits.lspid == CS_FE_LSPID_3) {
			packet_type = CS_APP_PKT_WIRELESS;
		} else 
#endif
		if ((instance == CS_NI_INSTANCE_WFO_PE0)
			|| (instance == CS_NI_INSTANCE_WFO_PE1)) {
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
			if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_WIRELESS))
				packet_type = CS_APP_PKT_WIRELESS;
			else
				packet_type = CS_APP_PKT_WFO;
#else
			packet_type = CS_APP_PKT_WFO;
#endif
		} else if (((voq == CPU_PORT7_VOQ_BASE) &&
			((xram_hdr_e.bits.lspid == CS_FE_LSPID_4) ||
			(xram_hdr_e.bits.lspid == CS_FE_LSPID_5))) ||
			(instance == CS_NI_INSTANCE_WFO_802_3)) {
	           	if (voq == CPU_PORT7_VOQ_BASE) {
				voq = (xram_hdr_e.bits.lspid == CS_FE_LSPID_4) ?
					CPU_PORT6_VOQ_BASE : (CPU_PORT6_VOQ_BASE+1);
	           	}
           		if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_WFO))
				packet_type = CS_APP_PKT_WFO;
			else
				packet_type = CS_APP_PKT_TUNNEL;

			if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPSEC)) {
				if (cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH) {
					packet_type = CS_APP_PKT_IPSEC;
				} else if ((cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0)
					&& ((voq % 2) == 0)) {
					packet_type = CS_APP_PKT_IPSEC;
				} else if ((cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1)
					&& ((voq % 2) == 1)) {
					packet_type = CS_APP_PKT_IPSEC;
				}
			}

			if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_TUNNEL)) {
				if ((voq % 2) == 1) {
					/* IPLIP */
					packet_type = CS_APP_PKT_TUNNEL;
				}
			}
		}
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

		DBG(printk("%s:%d:: packet_type = %d\n",
			__func__, __LINE__, packet_type));
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		if ((packet_type != CS_APP_PKT_WIRELESS)) {
			struct ethhdr *eth = skb->data;
			if(eth->h_proto == cpu_to_be16(ETH_P_8021Q)) {
				struct net_device *vlan_dev;
				if (instance < 2) {
					//For GREENWAVE to strip vlan tag from eth0 and eth1 port
					memmove(skb->data + VLAN_HLEN, skb->data, ETH_ALEN * 2);
					skb_pull(skb, VLAN_HLEN);
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
					skb->dirty_buffer = 1;
#endif
					vlan_dev = skb->dev;
				} else {
					struct vlan_hdr *vhdr = (struct vlan_hdr *) (skb->data + ETH_HLEN);
					unsigned short vlan_tci = ntohs(vhdr->h_vlan_TCI);
					unsigned short vlan_id = vlan_tci & VLAN_VID_MASK;
					if(vlan_id == 0)
						vlan_dev = skb->dev;
					else
						vlan_dev = vlan_find_dev(skb->dev, skb->protocol, vlan_id);
				}
			/* ...invalidate remainder of packet */
			if (skb->tail > skb->map_end &&
					!br_fdb_test_forward(vlan_dev, skb->data)) {
				dma_map_single(NULL, skb->map_end,
						SKB_DATA_ALIGN(skb->tail - skb->map_end),
						DMA_FROM_DEVICE);
				skb->dirty_buffer = 1;
				skb->map_end = NULL;
			}

		} else {
			/* ...invalidate remainder of packet */
			if ( skb->tail > skb->map_end &&
					!br_fdb_test_forward(skb->dev, skb->data)) {
				dma_map_single(NULL, skb->map_end,
						SKB_DATA_ALIGN(skb->tail - skb->map_end),
						DMA_FROM_DEVICE);
				skb->dirty_buffer = 1;
				skb->map_end = NULL;
			}
		}
	}
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		
		DBG(printk("%s:%d:: packet_type = %d\n",
			__func__, __LINE__, packet_type));
#ifdef CONFIG_CS75XX_WFO
		if (packet_type == CS_APP_PKT_WFO) {
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
            int lspid, vap, vap4bypass;
            lspid = (cs_cb->key_misc.orig_lspid == CS_FE_LSPID_4) ? CS_NI_INSTANCE_WFO_802_3_PE0 : CS_NI_INSTANCE_WFO_802_3_PE1;
			/* sw_action 33~48: 11AC 
			 *           49~64: 11N 
			 */
			vap4bypass = (cs_cb->key_misc.orig_lspid == CS_FE_LSPID_4) ? 0 : 1;
			if (xram_cpu_hdr1.bits.sw_action == 0) {
			    if (cs_cb->key_misc.orig_lspid == CS_FE_LSPID_4) {
			        vap = 0;
			    } else if (cs_cb->key_misc.orig_lspid == CS_FE_LSPID_5) {
			        vap = 1;
			    }
			} else 
			    vap = ((xram_cpu_hdr1.bits.sw_action & 0xffff) - 33) / 16;

			if (cs_hw_accel_wfo_handle_rx(
			        (xram_cpu_hdr0.bits.dst_voq == CPU_PORT7_VOQ_BASE) ?  lspid : instance, 
			        (xram_cpu_hdr0.bits.dst_voq == CPU_PORT7_VOQ_BASE) ?  vap4bypass : vap, 
					skb) != 1)
#else
			if (cs_hw_accel_wfo_handle_rx((xram_cpu_hdr0.bits.dst_voq == CPU_PORT7_VOQ_BASE) ?
					CS_NI_INSTANCE_WFO_802_3 : instance, voq, skb) != 1)
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
		                /*
		                * wfo already process skb if it return non 1
		                */
				goto SKB_HANDLED;
		}
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
		if (packet_type == CS_APP_PKT_WIRELESS) {
			if (cs_hw_accel_wireless_handle(voq, skb, xram_cpu_hdr1.bits.sw_action) != 1)
				goto SKB_HANDLED;
		}
#endif

#endif /* CONFIG_CS752X_ACCEL_KERNEL */

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
		in_dev = dev;
		/* in this mode checksum should not be enabled, because FE is
		 * not capable to locate the header offset properly */
		err_virt_ni = cs_ni_virt_ni_process_rx_skb(skb);
		/* check if the packet is dropped by the internal process */
		if (err_virt_ni < 0) {
			skb->dev = in_dev;
			dev_kfree_skb(skb);
			xram_ptr = (u32 *)NI_XRAM_BASE + next_link * 2;
			continue;
		}
#endif

		/* Default RX HW checksum enable */
		if (tp->rx_checksum == CS_ENABLE) {
			if (!((xram_cpu_hdr1.bits.ipv4_csum_err ==
							CHECKSUM_ERROR) ||
						(xram_cpu_hdr1.bits.l4_csum_err
						 == CHECKSUM_ERROR))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			} else {
				if (xram_cpu_hdr1.bits.ipv4_csum_err ==
						CHECKSUM_ERROR) {
					tp->ifStatics.rx_errors++;
					DBG(printk("IP CSUM error!\n"));
				}
				if (xram_cpu_hdr1.bits.l4_csum_err ==
						CHECKSUM_ERROR) {
					/* FIXME: HW BUG#26229. SW workaround.
					 * Do we need change? ask Alan Carr. */
					skb->ip_summed = CHECKSUM_NONE;
					DBG(printk("L4 CSUM error!\n"));
				}
			}
		}

		skb->protocol = eth_type_trans(skb, skb->dev);
		skb->pkt_type = PACKET_HOST;
#ifdef CONFIG_CS752X_PROC
		if (cs_ni_debug & DBG_NI_DUMP_RX) {
			//printk("%s:: RX: XRAM packet pkt_len %d, len %d, "
			//		"data_len %d, head %p, data %p, tail "
			//		"%p, end %p\n", __func__, pkt_len,
			//		skb->len, skb->data_len, skb->head,
			//		skb->data, skb->tail, skb->end);
			if (xram_hdr_e.bits.cpu_header) {
				printk("%s:%d:received port = %d, received voq "
						"= %d\n", __func__, __LINE__,
						xram_cpu_hdr0.bits.pspid,
						xram_cpu_hdr0.bits.dst_voq);

				printk("%s:%d:fwd_type = %d ", __func__, __LINE__,
						xram_hdr_e.bits.fwd_type);
				switch (xram_hdr_e.bits.fwd_type) {
				case 0:
					printk("(normal)");
					break;
				case 1:
					printk("(CPU)");
					break;
				case 4:
					printk("(MC)");
					break;
				case 5:
					printk("(BC)");
					break;
				case 6:
					printk("(UM)");
					break;
				case 7:
					printk("(UU)");
					break;
				case 8:
					printk("(Mirror)");
					break;
				case 12:
					printk("(Bypass)");
					break;
				case 13:
					printk("(Drop)");
					break;
				default:
					printk("(unknown)");
					break;
				}

				printk(", l2l3_flags = %d ", xram_cpu_hdr0.bits.l2l3_flags);
				if (xram_cpu_hdr0.bits.l2l3_flags & 0x8)
					printk("(IPv6)");
				if (xram_cpu_hdr0.bits.l2l3_flags & 0x4)
					printk("(IPv4)");
				if (xram_cpu_hdr0.bits.l2l3_flags & 0x2)
					printk("(Inner Tag Tag)");
				if (xram_cpu_hdr0.bits.l2l3_flags & 0x1)
					printk("(Outer Tag Valid)");
				printk("\n");

				if (xram_cpu_hdr1.bits.class_match != 0) {
					printk("%s:%d:Hit classifier, SDB index = %d\n",
						__func__, __LINE__, xram_cpu_hdr1.bits.svidx);
				} else {
					printk("%s:%d:No hit any classifier.\n",
						__func__, __LINE__);
				}

				printk("%s:%d:fwd_valids = %d ", __func__, __LINE__,
					xram_cpu_hdr0.bits.fwd_valids);
				switch (xram_cpu_hdr0.bits.fwd_valids) {
				case 0:
					printk("(No hit from either LPM or Hash)\n");
					break;
				case 1:
					printk("(Hit from Hash)\n");
					break;
				case 2:
					printk("(Hit from LPM)\n");
					break;
				case 3:
					printk("(Hit from LPM and Hash)\n");
					break;
				}

				if (xram_cpu_hdr0.bits.fwd_valids & 0x1) {
					printk("%s:%d:hash_result_idx = %d (FWD result index)\n",
						__func__, __LINE__, xram_cpu_hdr0.bits.hash_result_idx);
				}

				if (xram_cpu_hdr0.bits.fwd_valids & 0x2) {
					printk("%s:%d:lpm_result_idx = %d (LPM result index)\n",
						__func__, __LINE__, xram_cpu_hdr0.bits.lpm_result_idx);
				}
			}
			if (xram_cpu_hdr0.bits.acl_vld_dvoq_cpu != 0) {
				printk("%s:%d:ACL match.\n", __func__, __LINE__);
			}
			if (xram_cpu_hdr1.bits.ipv4_csum_err != 0) {
				printk("%s:%d:IPv4 checksum error\n", __func__, __LINE__);
			}
			if (xram_cpu_hdr1.bits.l4_csum_err != 0) {
				printk("%s:%d:Layer 4 checksum error\n", __func__, __LINE__);
			}

			printk("%s:: RX, seg_len %d\n", __func__, seg_len);
			ni_dm_byte((u32)skb->data - 14, seg_len + 14);
		}
#endif

		if (tp != NULL) {
			int extra_padding = 0;
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
			if (err_virt_ni)
				extra_padding = RTL_CPUTAG_LEN;
#else
			if (err_virt_ni)
				extra_padding = VLAN_HLEN;
#endif
#endif
			if (pkt_len > (dev->mtu + ETH_HLEN + extra_padding)) {
				tp->ifStatics.rx_errors++;
				tp->ifStatics.rx_bytes += pkt_len;
				pr_err("%s:: RX [GMAC %d] pkt_len %d > %d\n",
						__func__, tp->port_id, pkt_len,
						dev->mtu + ETH_HLEN +
						extra_padding);
			} else {
				tp->ifStatics.rx_packets++;
				tp->ifStatics.rx_bytes += skb->len;
			}
		}
#ifdef CS75XX_HW_ACCEL_TUNNEL
		if (packet_type == CS_APP_PKT_TUNNEL) {
			/*
			 * only PE->LAN UUFlow will enter here!!
			 * need to set net_dev as eth0
			 */
			if (skb->dev != ni_private_data.dev[0]) {
				printk("Why skb->dev is not dev[0]\n");
				skb->dev = ni_private_data.dev[0];
			}
#ifdef CONFIG_CS752X_PROC
			extern u32 cs_adapt_debug;
			if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) {
				DBG(printk("%s receive tunnel packet at instance %d, voq=%d, lspid=%d\n",
				__func__, instance, voq, cs_cb->key_misc.orig_lspid));
			}
#endif
			if (cs_hw_accel_tunnel_handle(voq, skb) == 0)
				goto SKB_HANDLED;
		}
#endif

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
		if (packet_type == CS_APP_PKT_IPSEC) {

			if (xram_hdr_e.bits.cpu_header)
				if ((xram_cpu_hdr0.bits.dst_voq == CPU_PORT6_VOQ_BASE)
					|| (xram_cpu_hdr0.bits.dst_voq == CPU_PORT6_VOQ_BASE + 1)
					) {
					cs_hw_accel_ipsec_handle_rx(skb, tp->port_id,
							xram_cpu_hdr0.bits.dst_voq,
							xram_cpu_hdr1.bits.sw_action);
					goto SKB_HANDLED;
				}

			if ((xram_cpu_hdr0.bits.dst_voq == CPU_PORT7_VOQ_BASE) &&
	        		((xram_hdr_e.bits.lspid == CS_FE_LSPID_4) ||
				(xram_hdr_e.bits.lspid == CS_FE_LSPID_5))) {
		                cs_hw_accel_ipsec_handle_rx(skb, tp->port_id,
					xram_hdr_e.bits.lspid == CS_FE_LSPID_4 ? CPU_PORT6_VOQ_BASE : (CPU_PORT6_VOQ_BASE+1),
					xram_cpu_hdr1.bits.sw_action);
				goto SKB_HANDLED;
	        	}
		}
#endif

#ifdef CONFIG_CS752X_FASTNET
		if (cs_core_fastnet_fast_xmit(skb) == 0)
			goto SKB_HANDLED;
#endif

#ifdef CONFIG_CS752X_PROC
		if ((cs_ni_fastbridge != 0) && (cs752x_fast_bridging(skb) == 0))
			goto SKB_HANDLED;
#endif
//#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
#if 0
		/* ...invalidate remainder of packet */
		if (skb->tail > skb->map_end &&
				!br_fdb_test_forward(skb->dev, skb->mac_header)) {
			dma_map_single(NULL, skb->map_end,
					SKB_DATA_ALIGN(skb->tail - skb->map_end),
					DMA_FROM_DEVICE);
			skb->dirty_buffer = 1;
			skb->map_end = NULL;
		}
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

		NETIF_RX(napi, skb, dev);

SKB_HANDLED:

		done++;
		xram_ptr = (u32 *)NI_XRAM_BASE + next_link * 2;
	} /* end of while loop */

	NI_WRITEL_MODE(next_link, NI_TOP_NI_CPUXRAM_CPU_CFG_RX_0 + (instance * 24));
	sw_next_link[instance] = next_link;

	/* FIXME!! which linux queue does QM fetch for PTP packet? */
	/* only refill when we are dealing with instance#0~7.
	 * Ignore instance#8 (PTP). */
	if (instance < LINUX_FREE_BUF) {
		sw_qm_total_count[instance] -= refill_cnt;

		if (refill_cnt != 0)
			cs_ni_alloc_linux_free_buffer(dev, instance, refill_cnt);

		if (sw_qm_total_count[instance] == 0) {
			//printk("%s start rx timer %d\n", __func__, instance);
			mod_timer(&rx_fill_buffer_timer[instance], jiffies + msecs_to_jiffies(cs_ni_delay_ms_rsve_mem));
		}
	}

	return done;
}

#ifdef CONFIG_CS75XX_KTHREAD_RX

static int has_many_rx_packets( int instance, int waterlevel)
{
	HEADER_XR_T *hdr_x;
	u32 hw_wr_ptr, next_link;
	int done = 0;
	u32 *xram_ptr;

	hw_wr_ptr = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_STA_RX_0 + (instance * 24)));
	hw_wr_ptr &= HW_WR_PTR;
	next_link = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_CFG_RX_0 + (instance * 24)));
	next_link &= SW_RD_PTR;
	xram_ptr = (u32 *) NI_XRAM_BASE + next_link * 2;

	while ((next_link != hw_wr_ptr) && (done < waterlevel)) {
		hdr_x = (HEADER_XR_T *) xram_ptr;
		if (hdr_x->bits.ownership != 0)
			break;

		next_link = hdr_x->bits.next_link;
		xram_ptr = (u32 *) NI_XRAM_BASE + next_link * 2;
		++done;
	}

	return done < waterlevel ? 0 : 1;
}

static struct net_dev* get_mapping_net_dev( int instance)
{
	/*
	 * instance# is not mapping to ni_private_data.dev arrary directly.
	 * For example , instance#3 uses ni_private_data.dev[5] actually.
	 * And instance#4 uses ni_private_data.dev[6] .
	 * instance#5, Instance#6, instance#7 uses NULL;
	 * It is magic.
	 */
	const int map[] = { 0, 1, 2, 5, 6 };

	if (3 <= instance)
		return NULL;
	return ni_private_data.dev[map[instance]];
}


static void rx_enable_interrupt( struct work_struct *work)
{
	struct rx_task_info *task = container_of( work, struct rx_task_info, work);

	NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + (task->instance << 3));
}

static int cs_ni_rx_process( void *data)
{
	int instance, packets_num;
	struct net_dev *dev;
	struct rx_task_info *rx_task;
	struct completion *interrupt_trigger;
	struct work_struct *work;

	instance = (int)(data);

	rx_task = rx_tasks + instance;
	interrupt_trigger = &(rx_task->interrupt_trigger);
	work = &(rx_task->work);
	dev = get_mapping_net_dev(instance);

	wait_for_completion_interruptible(interrupt_trigger);
	while (!kthread_should_stop()) {
		packets_num = ni_complete_rx_instance(dev, instance,
						      ni_napi_budget, NULL);

		if (packets_num < ni_napi_budget) {
			queue_work(system_wq, &(rx_task->work));
			wait_for_completion_interruptible(interrupt_trigger);
		}
	}

	return 0;
}

#else
#ifdef CONFIG_SMB_TUNING
static int cs_ni_poll_dev(int instance, struct net_device *dev,
		        struct napi_struct *napi, int budget)
{
	int received_pkts = 0;
	int t_budget;

	/* debug_Aaron on 2013/03/27 for BUG#38688 avoid TX/RX stall  */
	/* if (napi->last_loop)
		budget--; */

	t_budget = budget;
	received_pkts = ni_complete_rx_instance(dev, instance, budget, napi);

	/* if (received_pkts < budget || napi->last_loop) { */
	if (received_pkts < budget) {
		/*
		 * other driver has received_pkts == 0 to double confirm all
		 * the queues are empty, but in our current logic, when
		 * received_pkts is less than budget, it basically means
		 * there is no more packet in any of the queue.
		 */

		napi_complete(napi);
		if (sw_qm_total_count[instance] != 0)
			NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
				(instance << 3));

	}

	return received_pkts;
}

#else
static int cs_ni_poll_dev(int instance, struct net_device *dev,
	struct napi_struct *napi, int budget)
{
	int received_pkts = 0;

	received_pkts = ni_complete_rx_instance(dev, instance, budget, napi);
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
	if (received_pkts < budget) {
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
	if (received_pkts == 0) {
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

		/*
		 * other driver has received_pkts == 0 to double confirm all
		 * the queues are empty, but in our current logic, when
		 * received_pkts is less than budget, it basically means
		 * there is no more packet in any of the queue.
		 */

		napi_complete(napi);
		if (sw_qm_total_count[instance] != 0)
			NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
				(instance << 3));

		return 0;
	}


	return received_pkts;
}
#endif

static int cs_ni_poll_0(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(0, napi->dev, napi, budget);
}

static int cs_ni_poll_1(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(1, napi->dev, napi, budget);
}

static int cs_ni_poll_2(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(2, napi->dev, napi, budget);
}

static int cs_ni_poll_3(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(3, NULL, napi, budget);
}

static int cs_ni_poll_4(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(4, NULL, napi, budget);
}

static int cs_ni_poll_5(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(5, NULL, napi, budget);
}

static int cs_ni_poll_6(struct napi_struct *napi, int budget)
{
	return cs_ni_poll_dev(6, NULL, napi, budget);
}

static int cs_ni_poll_7(struct napi_struct *napi, int budget)
{

	return cs_ni_poll_dev(7, NULL, napi, budget);
}
#endif

struct net_device *ni_get_device(unsigned char port_id)
{
	struct net_device *dev;

	if (port_id > 2)
		return NULL;

	dev = ni_private_data.dev[port_id];
	if (dev == NULL)
		return NULL;
	else
		return dev;
}
EXPORT_SYMBOL(ni_get_device);

int cs_ni_skb_recycle_len_cpu0(void)
{
	return skb_queue_len(&cs_ni_skb_recycle_cpu0_head);
}
EXPORT_SYMBOL(cs_ni_skb_recycle_len_cpu0);
int cs_ni_skb_recycle_len_cpu1(void)
{
#if !defined(CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT) && defined(NI_RECYCLE_SKB_PER_CPU)
	return skb_queue_len(&cs_ni_skb_recycle_cpu1_head);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(cs_ni_skb_recycle_len_cpu1);

/* skb_recycle has been removed from mainline */
#if 0 /*defined(CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT) */
static bool cs_ni_skb_recycle(struct sk_buff *skb)
{
	if (!skb_is_recycleable(skb, SKB_PKT_LEN + 0x100))
		return false;

	if (skb_queue_len(&cs_ni_skb_recycle_cpu0_head) >= cs_ni_skb_recycle_max)
		return false;

	/* ...clean-up skb before recycling */
	skb_recycle(skb);

	/* ...put into packet pool */
	skb_queue_tail(&cs_ni_skb_recycle_cpu0_head, skb);

	return true;
}
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
static int cs_ni_skb_recycle(struct sk_buff *skb)
{
	struct sk_buff_head * tmp_sk_head;
	int ret = -1;

#ifdef	NI_RECYCLE_SKB_PER_CPU
	if (get_cpu() == 0)
		tmp_sk_head = &cs_ni_skb_recycle_cpu0_head;
	else
		tmp_sk_head = &cs_ni_skb_recycle_cpu1_head;

	if ((skb_queue_len(tmp_sk_head) <
				LINUX_FREE_BUF_LIST_SIZE) &&
			skb_recycle_check(skb,
				SKB_PKT_LEN + 0x100)) {

		__skb_queue_tail(tmp_sk_head, skb);
		ret = 0;
	}
	put_cpu();
#else
#if 0
	if ((skb_queue_len(&cs_ni_skb_recycle_cpu0_head) <
				LINUX_FREE_BUF_LIST_SIZE) &&
			skb_recycle_check(skb,
				SKB_PKT_LEN + 0x100)) {

		/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
		if (ni_rx_noncache && skb->head_pa == 0)
		{
			//printk("%s: In ni_rx_noncache, but skb->head_pa == 0!!!\n", __func__);
			return ret;
		}

		skb_queue_tail(&cs_ni_skb_recycle_cpu0_head, skb);
		ret = 0;
	}
#endif
#endif
	return ret;
}
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

#ifdef CONFIG_CS75XX_WFO
#include <mach/cs75xx_ipc_wfo.h>
extern pni_rxq_s pni_rxq[2];
#endif

static inline int cs_dma_tx_complete_easy(int tx_qid)
{
	ni_info_t *ni = &ni_private_data;
	dma_swtxq_t *swtxq;
	dma_rptr_t rptr_reg;
	unsigned int free_count = 0;
	struct sk_buff_head * tmp_sk_head;
#ifdef CONFIG_CS75XX_WFO
	int instance;
	void *xmit_pkt;
#endif
	/* get tx H/W completed descriptor virtual address */
	/* check tx status and accumulate tx statistics */

	swtxq = &ni->swtxq[tx_qid];
	spin_lock(&swtxq->lock);
	swtxq->intr_cnt++;
	rptr_reg.bits32 = NI_READL_MODE(swtxq->rptr_reg);

	while (rptr_reg.bits.rptr != swtxq->finished_idx) {
		if (swtxq->tx_skb[swtxq->finished_idx]) {
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
			dev_kfree_skb(swtxq->tx_skb[swtxq->finished_idx]);
#else /* CS752X_NI_TX_COMPLETE_INTERRUPT */
			dev_kfree_skb_any(swtxq->tx_skb[swtxq->finished_idx]);
#endif /*  CS752X_NI_TX_COMPLETE_INTERRUPT */
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			free_count++;

			if (cs_ni_skb_recycle(swtxq->tx_skb[swtxq->finished_idx]))
				dev_kfree_skb_any(swtxq->tx_skb[swtxq->finished_idx]);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			swtxq->tx_skb[swtxq->finished_idx] = NULL;
			swtxq->total_finished++;
		}
#ifdef CONFIG_CS75XX_WFO
		else if (swtxq->xmit_pkt[swtxq->finished_idx]) {
			instance = swtxq->wfo_pe_id[swtxq->finished_idx] - CS_WFO_IPC_PE0_CPU_ID;					
			xmit_pkt = swtxq->xmit_pkt[swtxq->finished_idx];
			if	(pni_rxq[instance].cb_fn_xmit_done != NULL)
					pni_rxq[instance].cb_fn_xmit_done(pni_rxq[instance].adapter, xmit_pkt);
			else {
				printk(KERN_ERR "%s ERR: cb_fn_xmit_done== NULL, there has risk that the resource doesn't free\n", __func__);
			}
			swtxq->xmit_pkt[swtxq->finished_idx] = NULL;
		}
#endif			
		swtxq->finished_idx = RWPTR_ADVANCE_ONE(swtxq->finished_idx,
				swtxq->total_desc_num);
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
		free_count++;
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
	}
	spin_unlock(&swtxq->lock);
	return free_count;
}

void cs_ni_tx_queue_complete_free(int tx_qid) {
	if (check_to_perform_tx_complete(tx_qid))
		cs_dma_tx_complete_easy(tx_qid);
}
EXPORT_SYMBOL(cs_ni_tx_queue_complete_free);

static inline void cs_dma_tx_complete(int tx_qid, struct net_device *dev)
{
	int free_count = cs_dma_tx_complete_easy(tx_qid);

	if (free_count > 0) {
		//smp_mb();
#ifdef CONFIG_NET_SCH_MULTIQ
		if (__netif_subqueue_stopped(dev, tx_qid))
			netif_wake_subqueue(dev, tx_qid);
#else
		if (netif_queue_stopped(dev)) {
			printk("%s:: \n",__func__);
			netif_wake_queue(dev);
		}
#endif
	}
	return;
}

static void rx_fill_buffer_timer_cb(unsigned long data)
{
	int instance = (int)data;
	if (sw_qm_total_count[instance] == 0) {
		cs_ni_alloc_linux_free_buffer(ni_private_data.dev[0],
			instance, linux_free_buf_size_tbl[instance]);
		if (sw_qm_total_count[instance] == 0) {
			mod_timer(&rx_fill_buffer_timer[instance], jiffies + msecs_to_jiffies(cs_ni_delay_ms_rsve_mem));
			//printk("\t reschedule timer[%d]\n", __func__, instance);
		} else {
			NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + (instance << 3));
		}
	}
}

#ifdef CS752X_NI_TX_COMPLETION_TIMER
static void tx_completion_timer_cb(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	mac_info_t *tp = netdev_priv(dev);
	int tx_qid;

	for (tx_qid = 0; tx_qid < NI_DMA_LSO_TXQ_NUM; ++tx_qid) {
		if (test_and_clear_bit(tx_qid, &tp->tx_completion_queues))
			cs_dma_tx_complete(tx_qid, dev);
	}
}

static void update_tx_completion_timer(int tx_qid, struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	set_bit(tx_qid, &tp->tx_completion_queues);
	mod_timer(&tp->tx_completion_timer, jiffies + msecs_to_jiffies(10));
}

static void cancel_tx_completion_timer(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	del_timer_sync(&tp->tx_completion_timer);
}
#else
static inline void tx_completion_timer_cb(unsigned long data)
{
}

static inline void update_tx_completion_timer(int tx_qid, struct net_device *dev)
{
}

static inline void cancel_tx_completion_timer(struct net_device *dev)
{
}
#endif

int ni_special_start_xmit_none_bypass_ne(u16 recirc_idx, u32 buf0, int len0, u32 buf1, int len1, struct sk_buff *skb)
{
	ni_header_a_0_t ni_hdra;
	ni_header_a_1_t ni_hdra1;
	dma_swtxq_t *swtxq;	
	unsigned int wptr, word0, word1, word2, word3, word4, word5, rptr;
	unsigned int desc_count;
	dma_txdesc_t *curr_desc;
	dma_txdesc_0_t	desc_word0;
	volatile dma_wptr_t wptr_reg;
	u32 free_desc;	
	int lso_tx_qid;
	
	if (smp_processor_id() == 0)
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX - 1;
	else
		lso_tx_qid = PE_DMA_LSO_TXQ_IDX;
			
	swtxq = &ni_private_data.swtxq[lso_tx_qid];
		
#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT	
	cs_ni_tx_queue_complete_free(lso_tx_qid);
#endif
	spin_lock(&swtxq->lock);
	if (swtxq->wptr >= swtxq->finished_idx)
		free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
			swtxq->finished_idx;
	else
		free_desc = swtxq->finished_idx - swtxq->wptr - 1;
	
	if (free_desc <= 0) {
		//spin_lock_bh(&pni_swtxq_lock);
		//printk("%s no more tx queue\n", __func__);
#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
		dev_kfree_skb(skb);
#else /* CS752X_NI_TX_COMPLETE_INTERRUPT */
		dev_kfree_skb_any(skb);
#endif /*  CS752X_NI_TX_COMPLETE_INTERRUPT */	
		spin_unlock(&swtxq->lock);	
		return NETDEV_TX_BUSY;
	}	
	ni_hdra.bits32 = HDRA_CPU_PKT;	
	ni_hdra.bits.fwd_type = 0; /* Normal */
	ni_hdra1.bits32 = 0;
	ni_hdra1.bits.recirc_idx = recirc_idx;
		
	wptr = swtxq->wptr;
	
	curr_desc = swtxq->desc_base + wptr;
	if ((len0 + len1) < MIN_DMA_SIZE)
		len0 = 64;

	word0 = len0 | OWN_BIT | SOF_BIT;

	if (buf1==0)
		word0 |= EOF_BIT;

	word3 = 0; /*cannot set LSO_BYPASS enable, that is for HW debug*/

	if ((len0 + len1) < 64)
		word3 |= LSO_IP_LENFIX_EN;
	word1 = buf0;
	word2 = ((len0 + len1)<<16);
	word4 = ni_hdra.bits32;
	word5 = ni_hdra1.bits32;
	wmb();

	curr_desc->word0.bits32 = word0;
	curr_desc->word1.bits32 = word1;
	curr_desc->word2.bits32 = word2;
	curr_desc->word3.bits32 = word3;
	curr_desc->word4.bits32 = word4;
	curr_desc->word5.bits32 = word5;
#ifdef CONFIG_CS75XX_WFO
	swtxq->xmit_pkt[wptr] = NULL;
	swtxq->wfo_pe_id[wptr] = 0;
#endif
	swtxq->tx_skb[wptr] = skb;
	
	if (buf1) {
		wptr = (wptr+1) & (swtxq->total_desc_num-1);
		curr_desc = swtxq->desc_base + wptr;
		word0 = len1 | EOF_BIT;
		word1 = buf1;
		wmb();
		curr_desc->word0.bits32 = word0;
		curr_desc->word1.bits32 = word1;
		curr_desc->word2.bits32 = 0;
		curr_desc->word3.bits32 = word3;
		curr_desc->word4.bits32 = word4;
		curr_desc->word5.bits32 = word5;
#ifdef CONFIG_CS75XX_WFO
		swtxq->xmit_pkt[wptr] = NULL;
		swtxq->wfo_pe_id[wptr] = 0;
#endif
		swtxq->tx_skb[wptr] = skb;
	}

	wptr = (wptr+1) & (swtxq->total_desc_num-1);
	wmb();
	writel(wptr, swtxq->wptr_reg);
	swtxq->wptr = wptr;
	spin_unlock(&swtxq->lock);
	//printk("\t*** %s::voq %d, skb=%p wptr=%d buf0 %08x, len0 %d, buf1 %08x, len1 %d\n",
	//	__func__, voq, skb, wptr, buf0, len0, buf1, len1);	
	return 0;
}
EXPORT_SYMBOL(ni_special_start_xmit_none_bypass_ne);

int ni_special_start_xmit(struct sk_buff *skb, HEADER_A_T *header_a, u16 voq)
{
		ni_info_t *ni = &ni_private_data;
		dma_rptr_t rptr_reg;
		dma_txdesc_t *curr_desc;
		struct net_device * dev;
		int snd_pages;
		int frag_id = 0, len, total_len, tx_qid = 0, lso_tx_qid;
		//struct net_device_stats *isPtr;
		u32 free_desc, rptr;
		dma_addr_t word1;
		u32 word0, word2, word3 = 0, word4, word5 = 0;
		dma_swtxq_t *swtxq;
		ni_header_a_0_t ni_header_a;
		u16 tx_queue = 0;
		struct iphdr *iph;
		char *pkt_datap;
		cs_kernel_accel_cb_t *cs_cb;
		int err = -1;
		struct vm_struct *area;

		cs_cb = CS_KERNEL_SKB_CB(skb);
		dev = ni_private_data.dev[0];
		//lso_tx_qid = get_dma_lso_txqid(dev);
		/* make sure not use the same tx queue as ni driver*/
		if (smp_processor_id() == 0)
			lso_tx_qid = PE_DMA_LSO_TXQ_IDX - 1;
		else
			lso_tx_qid = PE_DMA_LSO_TXQ_IDX;

		if (voq == 0xffff)
			tx_queue = ENCAPSULATION_VOQ_BASE; /* default voq */
		else
			tx_queue = voq;

		/* DMA_DMA_LSO_DMA_LSO_INTERRUPT_0, interrupt first level
		 * We are using the same tx_qid as for DMA LSO queue */
		swtxq = &ni->swtxq[lso_tx_qid];

		ni_header_a.bits32 = HDRA_CPU_PKT; /*ByPASS FE*/

#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
		cancel_tx_completion_timer(dev);
		if (check_to_perform_tx_complete(lso_tx_qid))
			cs_dma_tx_complete(lso_tx_qid, dev);
		else
			update_tx_completion_timer(lso_tx_qid, dev);
#endif
		spin_lock(&swtxq->lock);
		if (swtxq->wptr >= swtxq->finished_idx)
			free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
				swtxq->finished_idx;
		else
			free_desc = swtxq->finished_idx - swtxq->wptr - 1;

		/* try to reserve 1 descriptor in case skb is extended in xmit
		 * function */
		snd_pages = skb_shinfo(skb)->nr_frags + 1;

		if (free_desc <= snd_pages) {
			err = NETDEV_TX_BUSY;
			//printk("%s free_desc=%d <= snd_pages=%d \n", __func__, free_desc, snd_pages);
			goto free_skb;
		}

		total_len = skb->len;
		/* BUG#29162.Workaround. if packet length < 24, DMA LSO can not
		 * send packet out. */
		if (total_len < MIN_DMA_SIZE)
			goto free_skb;

		while (snd_pages != 0) {
			curr_desc = swtxq->desc_base + swtxq->wptr;

			if (frag_id == 0) {
				pkt_datap = skb->data;
				len = total_len - skb->data_len;
			} else {
				skb_frag_t *frag = &skb_shinfo(skb)->frags[frag_id - 1];
				pkt_datap =
					page_address(frag->page.p) + frag->page_offset;
				len = frag->size;
				if (len > total_len)
					printk("Fatal Error! Send Frag size %d > "
							"Total Size %d!!\n", len,
							total_len);
			}

			word0 = len | OWN_BIT;
			word2 = 0;

			skb->ip_summed = CHECKSUM_UNNECESSARY;
			word3 = set_desc_word3_calc_l4_chksum(skb, dev,
						total_len, frag_id);
			/* set_desc_word3_calc_l4_chksum may update checksum value,
			 * so we do dma_map_single after setting word3.
			 */
			area = find_vm_area(pkt_datap);
			if (((u32)pkt_datap >= (u32)area->phys_addr) &&
				((u32)pkt_datap <= ((u32)area->phys_addr + area->size)))
				word1 = skb->head_pa + (skb->data - skb->head);
			else{
#ifdef CONFIG_CS752X_PROC
			if( cs_acp_enable & CS75XX_ACP_ENABLE_NI){
				word1 = virt_to_phys(pkt_datap)|GOLDENGATE_ACP_BASE;
			}
			else
#endif

#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
				/* ...if data buffer is clean, flush only header */
				word1 = dma_map_single(NULL, (void *)pkt_datap,
						skb->dirty_buffer ? len : sizeof(struct ethhdr),
						DMA_TO_DEVICE);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
				word1 = dma_map_single(NULL, (void *)pkt_datap, len,
						DMA_TO_DEVICE);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			}

			if (snd_pages == 1) {
				word0 |= EOF_BIT; /* EOF */
				if (total_len < 64)
					word3 |= LSO_IP_LENFIX_EN;
				swtxq->tx_skb[swtxq->wptr] = skb;
			} else {
				swtxq->tx_skb[swtxq->wptr] = NULL;
				/* FIXME: if packet length > 1514, there are fragment or
				 * or segment, we need clean this bit */
				word3 &= ~LSO_IP_LENFIX_EN;
			}

			if (frag_id == 0) {
				word0 |= SOF_BIT; /* SOF */
				word2 = (total_len << 16) & 0xFFFF0000;
				/* Enable LSO Debug:
				 * "echo 4 > /proc/driver/cs752x/ne/ni/ni_debug" */
				/* Disable LSO Debug:
				 * "echo 0 > /proc/driver/cs752x/ne/ni/ni_debug" */
#ifdef CONFIG_CS752X_PROC
				if ((total_len > (dev->mtu + 14)) &&
						(cs_ni_debug & DBG_NI_LSO))
					printk("DMA LSO enable: MTU = %d, Packet "
							"Length %d\n", (dev->mtu + 14),
							total_len);
#endif
			}

			ni_header_a.bits.dvoq = tx_queue;
			word4 = ni_header_a.bits32;

			curr_desc->word1.bits32 = (u32)word1;
			curr_desc->word2.bits32 = word2;
			curr_desc->word3.bits32 = word3;
			curr_desc->word4.bits32 = word4;
			curr_desc->word0.bits32 = word0;

			free_desc--;

#ifdef CONFIG_CS752X_PROC
			if (cs_ni_debug & DBG_NI_DUMP_TX) {
				iph = ip_hdr(skb);
				printk("Word0:0x%08X, Word1:0x%08X, ", word0, word1);
				printk("Word2:0x%08X, Word3:0x%08X, ", word2, word3);
				printk("Word4:0x%08X, Word5:0x%08X, ", word4, word5);
				printk("iph->id = 0x%X\n", iph->id);
				printk("%s:: TX: DMA packet pkt_len %d, skb->data = 0x"
						"%p\n", __func__, len, skb->data);
				ni_dm_byte((u32)skb->data, len);
			}
#endif

			swtxq->wptr = RWPTR_ADVANCE_ONE(swtxq->wptr,
					swtxq->total_desc_num);
			frag_id++;
			snd_pages--;
		}
		smp_wmb();
		NI_WRITEL_MODE(swtxq->wptr, swtxq->wptr_reg);

#ifdef CONFIG_CS752X_PROC
		if (cs_ni_debug & DBG_NI_DUMP_TX) {
			rptr_reg.bits32 = NI_READL_MODE(swtxq->rptr_reg);
			rptr = rptr_reg.bits.rptr;
			printk("%s::tx reg wptr 0x%08x, rptr 0x%08x\n", __func__,
					swtxq->wptr, rptr_reg.bits32);
		}
#endif
		spin_unlock(&swtxq->lock);
		return 0;
free_skb:
		spin_unlock(&swtxq->lock);
		dev_kfree_skb(skb);
		return err;

}

int ni_start_xmit_to_re(struct sk_buff *skb, struct net_device * dev)
{
	ni_info_t *ni = &ni_private_data;
	dma_rptr_t rptr_reg;
	dma_txdesc_t *curr_desc;
	int snd_pages;
	int frag_id = 0, len, total_len, tx_qid = 0, lso_tx_qid;
	//struct net_device_stats *isPtr;
	u32 free_desc, rptr;
	dma_addr_t word1;
	u32 word0, word2, word3 = 0, word4, word5 = 0;
	dma_swtxq_t *swtxq;
	ni_header_a_0_t ni_header_a;
	u16 tx_queue = 0;
	struct iphdr *iph;
	char *pkt_datap;
	cs_kernel_accel_cb_t *cs_cb;
	mac_info_t *tp , *in_tp;
	struct vm_struct *area;

	cs_cb = CS_KERNEL_SKB_CB(skb);

	/*skb->dev is still input_dev*/
	skb->dev = dev;

	//struct netdev_queue *txq;

#ifdef CONFIG_NET_SCH_MULTIQ
	/*the current queue_mapping is still 0*/
	/*FIXME: how to get the correct output queue mapping*/
	netdev_pick_tx(dev, skb, NULL);
	tx_qid = skb_get_queue_mapping(skb);
#endif /* CONFIG_NET_SCH_MULTIQ */
	//txq = netdev_get_tx_queue(dev, tx_qid);
	tp = netdev_priv(dev);
	/* get the destination VOQ */
	switch (tp->port_id) {
		case GE_PORT0:
			tx_queue = GE_PORT0_VOQ_BASE;
			break;
		case GE_PORT1:
			tx_queue = GE_PORT1_VOQ_BASE;
			break;
		case GE_PORT2:
			tx_queue = GE_PORT2_VOQ_BASE;
			break;
		default:
			printk("%s:%d:Unacceptable port_id %d\n", __func__, __LINE__,
				tp->port_id);
			break;
	}
#ifdef CONFIG_NET_SCH_MULTIQ
	tx_queue += tx_qid;
#endif

	DBG(printk("%s dev name=%s tx_queue=%d , tx_qid=%d\n",
		__func__, dev->name, tx_queue, tx_qid));
	struct ethhdr *p_eth;
	p_eth = (struct ethhdr *)skb->data;
	p_eth->h_source[5] = tx_queue;
	cs_cb->action.voq_pol.d_voq_id = tx_queue;


	lso_tx_qid = get_dma_lso_txqid(dev);
	if (lso_tx_qid >= NI_DMA_LSO_TXQ_NUM)
		return 0;

	/* DMA_DMA_LSO_DMA_LSO_INTERRUPT_0, interrupt first level
	 * We are using the same tx_qid as for DMA LSO queue */
	swtxq = &ni->swtxq[lso_tx_qid];

	ni_header_a.bits32 = HDRA_CPU_PKT; /*ByPASS FE*/

	cancel_tx_completion_timer(dev);
	if (swtxq->wptr >= swtxq->finished_idx)
		free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
			swtxq->finished_idx;
	else
		free_desc = swtxq->finished_idx - swtxq->wptr - 1;
	update_tx_completion_timer(lso_tx_qid, dev);

	/* try to reserve 1 descriptor in case skb is extended in xmit
	 * function */
	snd_pages = skb_shinfo(skb)->nr_frags + 1;

	if (free_desc <= snd_pages) {
		return NETDEV_TX_BUSY;
	}

	total_len = skb->len;
	if (total_len < MIN_DMA_SIZE) {
		dev_kfree_skb(skb);
		return 0;
	}

#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_cb = CS_KERNEL_SKB_CB(skb);
	if ((cs_cb != NULL) && (cs_cb->common.tag == CS_CB_TAG)) {
		if (cs_cb->common.ingress_port_id <= GE_PORT2) {
			in_tp = netdev_priv(ni_private_data.dev[cs_cb->common.ingress_port_id]);
		} else {
			/*
			 * When not enable WFO Compile flag, 
			 * we also need to create hash from PE (for IPSec) ,
			 * so use out_dev as in_dev for PE offload case, 
			 * instead of using ni_private_data.dev[CS_NI_IRQ_WFO_PE0 / CS_NI_IRQ_WFO_PE1]
			 */	
			in_tp = tp; 
		}
		if ((tp->status == 1) && (in_tp->status == 1)) {
			cs_cb->common.egress_port_id = tp->port_id;
			if (!(cs_cb->common.module_mask & CS_MOD_MASK_WITH_QOS))
				cs_cb->action.voq_pol.d_voq_id = tx_queue;
			cs_core_logic_add_connections(skb);
		}
	}

#endif

	while (snd_pages != 0) {
		curr_desc = swtxq->desc_base + swtxq->wptr;
		if (frag_id == 0) {
			pkt_datap = skb->data;
			len = total_len - skb->data_len;
		} else {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[frag_id - 1];
			pkt_datap =
				page_address(frag->page.p) + frag->page_offset;
			len = frag->size;
			if (len > total_len)
				printk("Fatal Error! Send Frag size %d > "
						"Total Size %d!!\n", len,
						total_len);
		}

		word0 = len | OWN_BIT;
		word2 = 0;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		word3 = set_desc_word3_calc_l4_chksum(skb, dev,
			total_len, frag_id);
		/* set_desc_word3_calc_l4_chksum may update checksum value,
		 * so we do dma_map_single after setting word3.
		 */
		area = find_vm_area(pkt_datap);
		if (((u32)pkt_datap >= (u32)area->phys_addr) &&
				((u32)pkt_datap <= ((u32)area->phys_addr + area->size)))
			word1 = skb->head_pa + (skb->data - skb->head);
		else{
#ifdef CONFIG_CS752X_PROC
		if( cs_acp_enable & CS75XX_ACP_ENABLE_NI){
			word1 = virt_to_phys(pkt_datap)|GOLDENGATE_ACP_BASE;
		}
		else
#endif
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
			/* ...if data buffer is clean, flush only header */
			word1 = dma_map_single(NULL, (void *)pkt_datap,
					skb->dirty_buffer ? len : sizeof(struct ethhdr),
					DMA_TO_DEVICE);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			word1 = dma_map_single(NULL, (void *)pkt_datap, len,
					DMA_TO_DEVICE);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		}

		if (snd_pages == 1) {
			word0 |= EOF_BIT; /* EOF */
			if (total_len < 64)
				word3 |= LSO_IP_LENFIX_EN;
			swtxq->tx_skb[swtxq->wptr] = skb;
		} else {
			swtxq->tx_skb[swtxq->wptr] = NULL;
			/* FIXME: if packet length > 1514, there are fragment or
			 * or segment, we need clean this bit */
			word3 &= ~LSO_IP_LENFIX_EN;
		}

		if (frag_id == 0) {
			word0 |= SOF_BIT; /* SOF */
			word2 = (total_len << 16) & 0xFFFF0000;
			/* Enable LSO Debug:
			 * "echo 4 > /proc/driver/cs752x/ne/ni/ni_debug" */
			/* Disable LSO Debug:
			 * "echo 0 > /proc/driver/cs752x/ne/ni/ni_debug" */
#ifdef CONFIG_CS752X_PROC
			if ((total_len > (dev->mtu + 14)) &&
				(cs_ni_debug & DBG_NI_LSO))
				printk("DMA LSO enable: MTU = %d, Packet "
						"Length %d\n", (dev->mtu + 14),
						total_len);
#endif
}

		ni_header_a.bits.dvoq = ENCAPSULATION_VOQ_BASE;
		word4 = ni_header_a.bits32;
		curr_desc->word1.bits32 = (u32)word1;
		curr_desc->word2.bits32 = word2;
		curr_desc->word3.bits32 = word3;
		curr_desc->word4.bits32 = word4;
		curr_desc->word0.bits32 = word0;

		free_desc--;

#ifdef CONFIG_CS752X_PROC
		if (cs_ni_debug & DBG_NI_DUMP_TX) {
			iph = ip_hdr(skb);
			printk("Word0:0x%08X, Word1:0x%08X, ", word0, word1);
			printk("Word2:0x%08X, Word3:0x%08X, ", word2, word3);
			printk("Word4:0x%08X, Word5:0x%08X, ", word4, word5);
			printk("iph->id = 0x%X\n", iph->id);
			printk("%s:: TX: DMA packet pkt_len %d, skb->data = 0x"
					"%p\n", __func__, len, skb->data);
			ni_dm_byte((u32)skb->data, len);
		}
#endif

		swtxq->wptr = RWPTR_ADVANCE_ONE(swtxq->wptr,
				swtxq->total_desc_num);
		frag_id++;
		snd_pages--;
	}
	smp_wmb();
	NI_WRITEL_MODE(swtxq->wptr, swtxq->wptr_reg);

#ifdef CONFIG_CS752X_PROC
	if (cs_ni_debug & DBG_NI_DUMP_TX) {
		rptr_reg.bits32 = NI_READL_MODE(swtxq->rptr_reg);
		rptr = rptr_reg.bits.rptr;
		printk("%s::tx reg wptr 0x%08x, rptr 0x%08x\n", __func__,
				swtxq->wptr, rptr_reg.bits32);
	}
#endif
	return 0;

}

void ni_dm_byte(u32 location, int length)
{
	u8 *start_p, *end_p, *curr_p;
	u8 *datap, data;
	int i;

	start_p = datap = (u8 *)location;
	end_p = (u8 *)start_p + length;
	curr_p = (u8 *)((u32)location & 0xfffffff0);

	while (curr_p < end_p) {
		u8 *p1, *p2;
		printk("0x%08x: ", (u32)curr_p & 0xfffffff0);
		p1 = curr_p;
		p2 = datap;
		/* dump data */
		for (i = 0; i < 16; i++) {
			if (curr_p < start_p || curr_p >= end_p) {
				/* print space x 3 for padding */
				printk("   ");
			} else {
				data = *datap;
				printk("%02X ", data);
				datap++;
			}
			if (i == 7)
				printk("- ");
			curr_p++;
		}

		/* dump ascii */
		curr_p = p1;
		datap = p2;
		for (i = 0; i < 16; i++) {
			if (curr_p < start_p || curr_p >= end_p) {
				printk(".");
			} else {
				data = *datap;
				if ((data < 0x20) || (data > 0x7f) ||
						(data == 0x25))
					printk(".");
				else
					printk("%c", data);
				datap++;
			}
			curr_p++;
		}
		printk("\n");
	}
}
EXPORT_SYMBOL(ni_dm_byte);

void ni_dm_short(u32 location, int length)
{
	u16 *start_p, *curr_p, *end_p;
	u16 *datap, data;
	int i;

	/* start address should be a multiple of 2 */
	start_p = datap = (u16 *)(location & 0xfffffffe);
	end_p = (u16 *)location + length;
	curr_p = (u16 *)((u32)location & 0xfffffff0);

	while (curr_p < end_p) {
		printk("0x%08x: ", (u32)curr_p & 0xfffffff0);
		for (i = 0; i < 8; i++) {
			if (curr_p < start_p || curr_p >= end_p) {
				/* print space x 5 for padding */
				printk("     ");
			} else {
				data = *datap;
				printk("%04X ", data);
				datap++;
			}
			if (i == 3)
				printk("- ");
			curr_p++;
		}
		printk("\n");
	}
}
EXPORT_SYMBOL(ni_dm_short);

void ni_dm_long(u32 location, int length)
{
	u32 *start_p, *curr_p, *end_p;
	u32 *datap, data;
	int i;

	/* start address should be a multiple of 4 */
	start_p = datap = (u32 *)(location & 0xfffffffc);
	end_p = (u32 *)location + length;
	curr_p = (u32 *)((u32)location & 0xfffffff0);

	while (curr_p < end_p) {
		printk("0x%08x: ", (u32)curr_p & 0xfffffff0);
		for (i = 0; i < 4; i++) {
			if (curr_p < start_p || curr_p >= end_p) {
				/* print space x 9 for padding */
				printk("         ");
			} else {
				data = *datap;
				printk("%08X ", data);
				datap++;
			}
			if (i == 1)
				printk("- ");
			curr_p++;
		}
		printk("\n");
	}
}
EXPORT_SYMBOL(ni_dm_long);

#ifdef CONFIG_NET_SCH_MULTIQ
static uint16_t ni_select_queue(struct net_device *dev, struct sk_buff *skb)
{
    uint16_t queue_id;

    /* Get the queue id from skb->priority here 
     * It will update real queue id before create hash if
     * tc change the queue mapping.
     */
    queue_id = QUEUE_PER_INSTANCE - (skb->priority & 0x7) - 1;

	/* This API can be used to set the default TX queue */
#ifdef CONFIG_CS752X_HW_ACCELERATION
	cs_qos_set_voq_id_to_skb_cs_cb(skb, dev, (u8) queue_id);
#endif

	return (queue_id);
}
#endif

static void cs_ni_reset(void)
{
	u32 reg_val = 0;

	/* Apply resets */
	reg_val = NI_READL_MODE(NI_TOP_NI_INTF_RST_CONFIG);
	reg_val = reg_val | (RESET_NI);	/* ->1 */
	NI_WRITEL_MODE(reg_val, NI_TOP_NI_INTF_RST_CONFIG);
	/* Remove resets */
	reg_val = NI_READL_MODE(NI_TOP_NI_INTF_RST_CONFIG);
	reg_val = reg_val & (~(RESET_NI)); /* -> 0 */
	NI_WRITEL_MODE(reg_val, NI_TOP_NI_INTF_RST_CONFIG);
}

int cs_ni_set_port_calendar(u16 table_address, u8 pspid_ts)
{
	NI_TOP_NI_RX_PORT_CAL_ACCESS_t rx_port_cal_acc, rx_port_cal_acc_mask;
	NI_TOP_NI_RX_PORT_CAL_DATA_t rx_port_cal_data, rx_port_cal_data_mask;
	int access;

	rx_port_cal_data.wrd = 0;
	rx_port_cal_data_mask.wrd = 0;

	rx_port_cal_data.bf.pspid_ts = pspid_ts;
	rx_port_cal_data_mask.bf.pspid_ts = 0x7;
	write_reg(rx_port_cal_data.wrd, rx_port_cal_data_mask.wrd,
			NI_TOP_NI_RX_PORT_CAL_DATA);

	rx_port_cal_acc.wrd = 0;
	rx_port_cal_acc_mask.wrd = 0;

	rx_port_cal_acc.bf.access = 1;
	rx_port_cal_acc.bf.rxpc_page = 0;
	rx_port_cal_acc.bf.cpu_page = 0;
	rx_port_cal_acc.bf.rbw = CS_WRITE; /* 1: Write */
	rx_port_cal_acc.bf.address = table_address;

	rx_port_cal_acc_mask.bf.access = 1;
	rx_port_cal_acc_mask.bf.rxpc_page = 1;
	rx_port_cal_acc_mask.bf.cpu_page = 1;
	rx_port_cal_acc_mask.bf.rbw = 1;
	rx_port_cal_acc_mask.bf.address = 0x7F;
	write_reg(rx_port_cal_acc.wrd, rx_port_cal_acc_mask.wrd,
			NI_TOP_NI_RX_PORT_CAL_ACCESS);

	do {
		access = (NI_READL_MODE(NI_TOP_NI_TX_VOQ_LKUP_ACCESS) & 0x80000000);
		udelay(1);
	} while (access == 0x80000000);
	return 0;
}
EXPORT_SYMBOL(cs_ni_set_port_calendar);

/*
 * Initialize the NI port calendar
 * 	Goldengate supports 96 (maximum) time slot port calendar.
 * 	The source port numbering is as follows:
 * 	0 - GE0
 * 	1 - GE1
 * 	2 - GE2
 * 	3 - CPU
 * 	4 - Crypto
 * 	5 - Encap
 * 	6 - Mcast
 * 	7 - Mirror
 * 	GigE ports, 1.2 Gbps/100mbps = 12 slots * 3 ports = 36 slots
 * 	Encap and crypto ports each need 1.2 Gbps/100mbps =
 * 			12 slots * 2 ports = 24 slots
 * 	Multicast port needs 1.2Gbps/100mbps = 12 slots * 1 port = 12 slots
 * 	CPU port needs 1.2 Gbps/100 mbps=12 slots * 1 port = 12 slots
 * 	Mirror port needs 1.2 Gbps/100 mbps=12 slots * 1 port = 12 slots
 */

static void cs_ni_port_cal_cfg(void)
{
	u8 i, j;
	int slot_per_port = NI_MAX_PORT_CALENDAR / NI_PORT_NUM;

	for (i = 0; i < slot_per_port; i++) {
		for (j = 0; j < NI_PORT_NUM; j++) {
			cs_ni_set_port_calendar((i * NI_PORT_NUM) + j, j);
		}
	}

#if defined(CONFIG_CORTINA_CUSTOM_BOARD)
	/* We don't know customer's configuration.
	 * Just keep bandwidth equally
	 */
#else
	/* Overwrite bandwidth if ENG or BHR  */
#if !defined(CONFIG_CORTINA_ENGINEERING) && !defined(CONFIG_CORTINA_BHR)
	/* Allocate eth2 bandwidth to PE0 if not ENG or BHR board */
	for (i = 0; i < slot_per_port; i++)
		cs_ni_set_port_calendar((i * NI_PORT_NUM) + 2, 4);

#elif defined(CONFIG_CORTINA_BHR)
	/* MAC0 & MAC2 are MOCA, max bandwidth is 400mbps */
	for(i = 0; i < 5; i++ ){
		cs_ni_set_port_calendar(i * NI_PORT_NUM, 0);
		cs_ni_set_port_calendar((i *NI_PORT_NUM) + 2, 2);
	}
	for(; i<slot_per_port; i++ ){
		cs_ni_set_port_calendar(i * NI_PORT_NUM, 4);
		cs_ni_set_port_calendar((i *NI_PORT_NUM) + 2, 4);
	}
#endif
#endif

#ifdef CS75XX_HW_ACCEL_TUNNEL
	/* Allocate Mirror 8 slots bandwidth to PE1 for IPLIP */
	/* Total bandwidth of PE1 IPLIP: 1200 + 800 */
	for (i = 4; i < slot_per_port; i++)
                cs_ni_set_port_calendar((i * NI_PORT_NUM) + 7, 5);
#endif

	return;
}

int ni_get_port_calendar(u16 table_address)
{
	u8 access;
	u32 cal_data;
	NI_TOP_NI_RX_PORT_CAL_ACCESS_t rx_port_cal_acc, rx_port_cal_acc_mask;

	rx_port_cal_acc.wrd = 0;
	rx_port_cal_acc_mask.wrd = 0;

	rx_port_cal_acc.bf.access = 1;
	rx_port_cal_acc.bf.rxpc_page = 0;
	rx_port_cal_acc.bf.cpu_page = 0;
	rx_port_cal_acc.bf.rbw = CS_READ; /* 1: Write, 0:Read */
	rx_port_cal_acc.bf.address = table_address;

	rx_port_cal_acc_mask.bf.access = 1;
	rx_port_cal_acc_mask.bf.rxpc_page = 1;
	rx_port_cal_acc_mask.bf.cpu_page = 1;
	rx_port_cal_acc_mask.bf.rbw = 1;
	rx_port_cal_acc_mask.bf.address = 0x7F;
	write_reg(rx_port_cal_acc.wrd, rx_port_cal_acc_mask.wrd,
			NI_TOP_NI_RX_PORT_CAL_ACCESS);

	do {
		access = (NI_READL_MODE(NI_TOP_NI_RX_PORT_CAL_ACCESS) & 0x80000000);
		udelay(1);
	} while (access == 0x80000000);

	/* read data */
	cal_data = NI_READL_MODE(NI_TOP_NI_RX_PORT_CAL_DATA) & 0x7;
	printk("RX Port Cal Table\t %3d :\t  %d\n", table_address, cal_data);

	return 0;
}
EXPORT_SYMBOL(ni_get_port_calendar);

static void cs_ni_init_xram_mem(void)
{
	int i;
	unsigned int next_start;
	ni_info_t *ni;
	u32 *ni_xram_base;
	NI_TOP_NI_CPUXRAM_ADRCFG_RX_0_t xram_rx_addr, xram_rx_addr_mask;
	NI_TOP_NI_CPUXRAM_ADRCFG_TX_0_t xram_tx_addr, xram_tx_addr_mask;

	ni = &ni_private_data;
	ni_xram_base = (u32 *)NI_XRAM_BASE;

	/* clear XRAM */
	for (i = 0; i <= XRAM_RX_INSTANCE; i++)
		NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_ADRCFG_RX_0 + (i * 4));

	for (i = 0; i < XRAM_TX_INSTANCE; i++)
		NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_ADRCFG_TX_0 + (i * 4));

	xram_rx_addr.wrd = 0;
	xram_rx_addr_mask.wrd = 0;
	xram_tx_addr.wrd = 0;
	xram_tx_addr_mask.wrd = 0;

	/* for RX XRAM CFG */
	xram_rx_addr_mask.bf.rx_base_addr = 0x7FF;
	xram_rx_addr_mask.bf.rx_top_addr = 0x7FF;
	next_start = 0;
	for (i = 0; i <= XRAM_RX_INSTANCE; i++) {
		if (cpuxram_rx_addr_cfg_prof[XRAM_CFG_PROF][i] != 0) {

			xram_rx_addr.bf.rx_base_addr = next_start;
			next_start += cpuxram_rx_addr_cfg_prof[
					XRAM_CFG_PROF][i];

			xram_rx_addr.bf.rx_top_addr = next_start - 1;
		} else {
			xram_rx_addr.bf.rx_base_addr = 0;
			xram_rx_addr.bf.rx_top_addr = 0;
		}

		write_reg(xram_rx_addr.wrd, xram_rx_addr_mask.wrd,
				NI_TOP_NI_CPUXRAM_ADRCFG_RX_0 + (i * 4));
	}

	/* for TX XRAM CFG */
	xram_tx_addr_mask.bf.tx_base_addr = 0x7FF;
	xram_tx_addr_mask.bf.tx_top_addr = 0x7FF;
	for (i = 0; i < XRAM_TX_INSTANCE; i++) {
		if (cpuxram_tx_addr_cfg_prof[XRAM_CFG_PROF][i] != 0) {
			xram_tx_addr.bf.tx_base_addr = next_start;
			next_start += cpuxram_tx_addr_cfg_prof[
					XRAM_CFG_PROF][i];
			xram_tx_addr.bf.tx_top_addr = next_start - 1;
		} else {
			xram_tx_addr.bf.tx_base_addr = 0;
			xram_tx_addr.bf.tx_top_addr = 0;
		}

		write_reg(xram_tx_addr.wrd, xram_tx_addr_mask.wrd,
				NI_TOP_NI_CPUXRAM_ADRCFG_TX_0 + (i * 4));
	}
}

static int cs_ni_set_xram_cfg(xram_inst_t xram_inst,
		xram_direction_t xram_direction)
{
	NI_TOP_NI_CPUXRAM_CFG_t ni_xram_cfg, ni_xram_cfg_mask;

	ni_xram_cfg.wrd = 0;
	ni_xram_cfg_mask.wrd = 0;

	switch (xram_direction) {
	case XRAM_DIRECTION_TX:
		if (xram_inst > XRAM_INST_1) {
			printk("Only 0 and 1 instances in TX direction\n");
			return -1;
		}

		switch (xram_inst) {
		case XRAM_INST_0:
			ni_xram_cfg.bf.tx_0_cpu_pkt_dis = 0;	/* 0: enable */
			ni_xram_cfg_mask.bf.tx_0_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_1:
			ni_xram_cfg.bf.tx_1_cpu_pkt_dis = 0;
			ni_xram_cfg_mask.bf.tx_1_cpu_pkt_dis = 1;
			break;
		default:
			printk("Only 0 and 1 instances in TX direction\n");
			return -1;
		}
		write_reg(ni_xram_cfg.wrd, ni_xram_cfg_mask.wrd,
				NI_TOP_NI_CPUXRAM_CFG);
		break;
	case XRAM_DIRECTION_RX:
		switch (xram_inst) {
		case XRAM_INST_0:
			ni_xram_cfg.bf.rx_0_cpu_pkt_dis = 0; /* 0:enable */
			ni_xram_cfg_mask.bf.rx_0_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_1:
			ni_xram_cfg.bf.rx_1_cpu_pkt_dis = 0; /* 0:enable */
			ni_xram_cfg_mask.bf.rx_1_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_2:
			ni_xram_cfg.bf.rx_2_cpu_pkt_dis = 0; /* 0:enable */
			ni_xram_cfg_mask.bf.rx_2_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_3:
			ni_xram_cfg.bf.rx_3_cpu_pkt_dis = 0; /* 0:enable */
			ni_xram_cfg_mask.bf.rx_3_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_4:
			ni_xram_cfg.bf.rx_4_cpu_pkt_dis = 0; /* 0:enable */
			ni_xram_cfg_mask.bf.rx_4_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_5:
			ni_xram_cfg.bf.rx_5_cpu_pkt_dis = 0;
			ni_xram_cfg_mask.bf.rx_5_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_6:
			ni_xram_cfg.bf.rx_6_cpu_pkt_dis = 0;
			ni_xram_cfg_mask.bf.rx_6_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_7:
			ni_xram_cfg.bf.rx_7_cpu_pkt_dis = 0;
			ni_xram_cfg_mask.bf.rx_7_cpu_pkt_dis = 1;
			break;
		case XRAM_INST_8:
			ni_xram_cfg.bf.rx_8_cpu_pkt_dis = 0;
			ni_xram_cfg_mask.bf.rx_8_cpu_pkt_dis = 1;
			break;
		default:
			printk("Only 0 - 8 instances in RX direction\n");
			return -1;
		} /* switch(xram_inst) */
		write_reg(ni_xram_cfg.wrd, ni_xram_cfg_mask.wrd,
				NI_TOP_NI_CPUXRAM_CFG);
		break;
	} /* switch(xram_direction) */

	return 0;
}

static void cs_init_tx_dma_lso(void)
{
	int i;
	ni_info_t *ni;
	dma_addr_t txq_paddr[NI_DMA_LSO_TXQ_NUM];
	dma_txdesc_t *tx_desc[NI_DMA_LSO_TXQ_NUM];
	u32 rptr_addr, wptr_addr, dma_tx_base_addr;
	DMA_DMA_LSO_RXDMA_CONTROL_t rxdma_control, rxdma_control_mask;
	DMA_DMA_LSO_TXDMA_CONTROL_t txdma_control, txdma_control_mask;
	DMA_DMA_LSO_TXQ0_CONTROL_t txq0_control, txq0_control_mask;
	DMA_D_AXI_CONFIG_t axi_config, axi_config_mask;
#ifdef CONFIG_CS752X_VIRTUAL_NI_DBLTAG
	DMA_DMA_LSO_VLAN_TAG_TYPE0_t vlan_tag_type0, vlan_tag_type0_mask;
#endif

	ni = &ni_private_data;
	/* RX DMA LSO */
	rxdma_control.wrd = 0;
	rxdma_control_mask.wrd = 0;
	rxdma_control.bf.rx_dma_enable = 0; /* Disable RX DMA */
	rxdma_control.bf.rx_check_own = 0;
	rxdma_control.bf.rx_burst_len = 3; /* Sanders's suggestion 3 */
	rxdma_control_mask.bf.rx_dma_enable = 1;
	rxdma_control_mask.bf.rx_check_own = 0;
	rxdma_control_mask.bf.rx_burst_len = 3;	/* 8 * 64 bits */
	write_reg(rxdma_control.wrd, rxdma_control_mask.wrd,
			DMA_DMA_LSO_RXDMA_CONTROL);

	/* TX DMA LSO */
	txdma_control.wrd = 0;
	txdma_control_mask.wrd = 0;
	txdma_control.bf.tx_dma_enable = 1; /* Enable RX DMA */
	txdma_control.bf.tx_check_own = 0;
	txdma_control.bf.tx_burst_len = 3; /* Sanders's suggestion 3 */
	txdma_control_mask.bf.tx_dma_enable = 1;
	txdma_control_mask.bf.tx_check_own = 0;
	txdma_control_mask.bf.tx_burst_len = 3;	/* 8 * 64 bits */
	write_reg(txdma_control.wrd, txdma_control_mask.wrd,
			DMA_DMA_LSO_TXDMA_CONTROL);

	/* enable TXQ 0~7 w/TXQ#6 and #7 in Round Robin mode */
	/* FIXME: TXQ 6 and 7 are different for memory copy */
	txq0_control.wrd = 0;
	txq0_control_mask.wrd = 0;
	txq0_control.bf.txq0_en = 1;
	txq0_control_mask.bf.txq0_en = 1;
	for (i = 0; i < NI_DMA_LSO_TXQ_NUM; i++)
		write_reg(txq0_control.wrd, txq0_control_mask.wrd,
				DMA_DMA_LSO_TXQ0_CONTROL + (i * 4));

	dma_tx_base_addr = DMA_DMA_LSO_TXQ0_BASE_DEPTH;
	rptr_addr = DMA_DMA_LSO_TXQ0_RPTR;
	wptr_addr = DMA_DMA_LSO_TXQ0_WPTR;

	for (i = 0; i < NI_DMA_LSO_TXQ_NUM; i++) {

#ifdef CONFIG_CS752X_PROC
	if( cs_acp_enable & CS75XX_ACP_ENABLE_NI){
		tx_desc[i] = cs_malloc( NI_DMA_LSO_TXDESC_NUM * sizeof(dma_txdesc_t), GFP_KERNEL);
		txq_paddr[i] = virt_to_phys(tx_desc[i])|GOLDENGATE_ACP_BASE;
		if(txq_paddr[i]& 0xF)
			printk("@@@@@@@@@@@@@ TSO alignment error : %x @@@@@@@@@@@@@@@@@@\n",txq_paddr[i]);
	}
	else{
#endif
		tx_desc[i] = dma_alloc_coherent(NULL,
				NI_DMA_LSO_TXDESC_NUM * sizeof(dma_txdesc_t),
				&txq_paddr[i], GFP_KERNEL);
#ifdef CONFIG_CS752X_PROC
	}
#endif
		ni->swtxq[i].rptr_reg = rptr_addr;
		ni->swtxq[i].wptr_reg = wptr_addr;
		ni->swtxq[i].wptr = 0;
		ni->swtxq[i].total_desc_num = NI_DMA_LSO_TXDESC_NUM;
		spin_lock_init(&ni->swtxq[i].lock);

		ni->swtxq[i].desc_base = tx_desc[i];

		if (!tx_desc[i]) {
			printk("%s::TX %d dma_alloc_coherent fail !\n",
					__func__, i);
			dma_free_coherent(NULL,
				  NI_DMA_LSO_TXDESC_NUM * sizeof(dma_txdesc_t),
				  tx_desc,
				  txq_paddr[i]);
			return;
		}
		NI_WRITEL_MODE((txq_paddr[i] & DMA_BASE_MASK)
			| NI_DMA_LSO_TXDESC, dma_tx_base_addr);
		dma_tx_base_addr += 4;
		rptr_addr += 8;
		wptr_addr += 8;
	}

	/* for AXI debug purpose, check with Gordon */
	NI_WRITEL_MODE(0xFFFC0000, DMA_D_AXI_READ_TIMEOUT_THRESHOLD);
	NI_WRITEL_MODE(0xFFFC0000, DMA_D_AXI_WRITE_TIMEOUT_THRESHOLD);

	/* Sanders's suggestion */
	axi_config.wrd = 0;
	axi_config_mask.wrd = 0;
	axi_config.bf.axi_write_outtrans_nums = 3;
	axi_config_mask.bf.axi_write_outtrans_nums = 3;

	axi_config.bf.axi_read_channel0_arbscheme = 1;
	axi_config_mask.bf.axi_read_channel0_arbscheme = 1;
	axi_config.bf.axi_read_channel1_arbscheme = 1;
	axi_config_mask.bf.axi_read_channel1_arbscheme = 1;
	axi_config.bf.axi_read_channel2_arbscheme = 1;
	axi_config_mask.bf.axi_read_channel2_arbscheme = 1;

	axi_config.bf.axi_write_channel0_arbscheme = 1;
	axi_config_mask.bf.axi_write_channel0_arbscheme = 1;
	axi_config.bf.axi_write_channel1_arbscheme = 1;
	axi_config_mask.bf.axi_write_channel1_arbscheme = 1;
	axi_config.bf.axi_write_channel2_arbscheme = 1;
	axi_config_mask.bf.axi_write_channel2_arbscheme = 1;
	write_reg(axi_config.wrd, axi_config_mask.wrd, DMA_D_AXI_CONFIG);

	NI_WRITEL_MODE(0xFFFFFFFF, DMA_D_AXI_READ_CHANNEL_0_3_DRR_WEIGHT);
	NI_WRITEL_MODE(0x0000FFFF, DMA_D_AXI_READ_CHANNEL_4_7_DRR_WEIGHT);

	NI_WRITEL_MODE(0xFFFFFFFF, DMA_D_AXI_WRITE_CHANNEL_0_3_DRR_WEIGHT);
	NI_WRITEL_MODE(0x0000FFFF, DMA_D_AXI_WRITE_CHANNEL_4_7_DRR_WEIGHT);
#ifdef CONFIG_CS752X_VIRTUAL_NI_DBLTAG
	vlan_tag_type0.wrd = 0;
	vlan_tag_type0_mask.wrd = 0;
	vlan_tag_type0.bf.enable = 1;
	vlan_tag_type0_mask.bf.enable = 1;
	vlan_tag_type0.bf.value = ETH_P_8021AD;
	vlan_tag_type0_mask.bf.value = 0xffff;
	write_reg(vlan_tag_type0.wrd, vlan_tag_type0_mask.wrd,
			DMA_DMA_LSO_VLAN_TAG_TYPE0);
#endif
}

static void cs_ni_set_voq_map(u16 voq_number, u8 voq_did, u8 disable_crc)
{
	NI_TOP_NI_TX_VOQ_LKUP_ACCESS_t voq_lkup_access, voq_lkup_access_mask;
	NI_TOP_NI_TX_VOQ_LKUP_DATA0_t voq_lkup_data0, voq_lkup_data0_mask;
	u8 access;

	/* Prepare data */
	voq_lkup_data0.wrd = 0;
	voq_lkup_data0_mask.wrd = 0;
	voq_lkup_data0.bf.txem_voq_did = voq_did;
	voq_lkup_data0.bf.txem_discrc = disable_crc;
	voq_lkup_data0.bf.txem_crcstate = 0;
	voq_lkup_data0_mask.bf.txem_voq_did = 0xF;
	voq_lkup_data0_mask.bf.txem_discrc = 1;
	voq_lkup_data0_mask.bf.txem_crcstate = 0x7FFFFFF;
	//printk("00--> voq_did = %d, voq_lkup_data0.wrd =0x%X\n",
	//              voq_did,voq_lkup_data0.wrd);
	write_reg(voq_lkup_data0.wrd, voq_lkup_data0_mask.wrd,
			NI_TOP_NI_TX_VOQ_LKUP_DATA0);
	NI_WRITEL_MODE(0, NI_TOP_NI_TX_VOQ_LKUP_DATA1);

	voq_lkup_access.wrd = 0;
	voq_lkup_access_mask.wrd = 0;
	voq_lkup_access.bf.access = 1;
	/* access address is the same as the voq_number, range from 0 to 127 */
	voq_lkup_access.bf.address = voq_number;
	voq_lkup_access.bf.debug_mode = NORMAL_MODE; /* NORMAL_MODE = 0 */
	voq_lkup_access.bf.rbw = CS_WRITE; /* 0:read, 1:write */

	voq_lkup_access_mask.bf.access = 1;
	voq_lkup_access_mask.bf.address = 0x7F;
	voq_lkup_access_mask.bf.debug_mode = 1;
	voq_lkup_access_mask.bf.rbw = 1;
	write_reg(voq_lkup_access.wrd, voq_lkup_access_mask.wrd,
			NI_TOP_NI_TX_VOQ_LKUP_ACCESS);
	do {
		access = (NI_READL_MODE(NI_TOP_NI_TX_VOQ_LKUP_ACCESS) & 0x80000000);
		udelay(1);
	} while (access == 0x80000000);
}

static void cs_ni_voq_map_cfg(void)
{
	int i;

#ifdef CS752X_MANAGEMENT_MODE
	/* for management port only */
	/* GE1: 0x0001 */
	for (i = 0; i <= 7; i++)
		cs_ni_set_voq_map(i, NI_VOQ_DID_GE1, 0);

#else
	for (i = 0; i <= 7; i++) {
		/* GE0, Voq: 0 ~ 7 */
		cs_ni_set_voq_map(i + GE_PORT0_VOQ_BASE, NI_VOQ_DID_GE0, 1);
		/* GE1, Voq: 8 ~ 15 */
		cs_ni_set_voq_map(i + GE_PORT1_VOQ_BASE, NI_VOQ_DID_GE1, 1);
		/* GE2, Voq: 16 ~ 23 */
		cs_ni_set_voq_map(i + GE_PORT2_VOQ_BASE, NI_VOQ_DID_GE2, 1);
		/* Encap and Crypto Core, Voq: 24 ~ 31 & 32 ~ 39 */
		cs_ni_set_voq_map(i + ENCRYPTION_VOQ_BASE, NI_VOQ_DID_CRYPTO,
				  1);
		cs_ni_set_voq_map(i + ENCAPSULATION_VOQ_BASE, NI_VOQ_DID_ENCAP,
				  1);
		/* for Multicast: 40 ~ 47 */
		cs_ni_set_voq_map(i + ROOT_PORT_VOQ_BASE, NI_VOQ_DID_MC, 1);
		/* for CPU 0 Voq: 48 ~ 55 */
		cs_ni_set_voq_map(i + CPU_PORT0_VOQ_BASE, NI_VOQ_DID_CPU0, 1);
		/* for CPU 1 Voq: 56 ~ 63 */
		cs_ni_set_voq_map(i + CPU_PORT1_VOQ_BASE, NI_VOQ_DID_CPU1, 1);
		/* for CPU 2 Voq: 64 ~ 71 */
		cs_ni_set_voq_map(i + CPU_PORT2_VOQ_BASE, NI_VOQ_DID_CPU2, 1);
		/* for CPU 3 Voq: 72 ~ 79 */
		cs_ni_set_voq_map(i + CPU_PORT3_VOQ_BASE, NI_VOQ_DID_CPU3, 1);
		/* for CPU 4 Voq: 80 ~ 87 */
		cs_ni_set_voq_map(i + CPU_PORT4_VOQ_BASE, NI_VOQ_DID_CPU4, 1);
		/* for CPU 5 Voq: 88 ~ 95 */
		cs_ni_set_voq_map(i + CPU_PORT5_VOQ_BASE, NI_VOQ_DID_CPU5, 1);
		/* for CPU 6 Voq: 96 ~ 103 */
		cs_ni_set_voq_map(i + CPU_PORT6_VOQ_BASE, NI_VOQ_DID_CPU6, 1);
		/* for CPU 7 Voq: 104 ~ 111 */
		cs_ni_set_voq_map(i + CPU_PORT7_VOQ_BASE, NI_VOQ_DID_CPU7, 1);
	}

#endif
}

void cs_ni_set_mc_table(u8 mc_index, u16 mc_vec)
{
	NI_TOP_NI_MC_INDX_LKUP_ACCESS_t mc_lkup_access, mc_lkup_access_mask;
	NI_TOP_NI_MC_INDX_LKUP_DATA_t mc_lkup_data, mc_lkup_data_mask;
	u8 access;

	mc_lkup_data.wrd = 0;
	mc_lkup_data_mask.wrd = 0;
	mc_lkup_data.bf.mc_vec = mc_vec;
	mc_lkup_data_mask.bf.mc_vec = 0xFFFF;
	write_reg(mc_lkup_data.wrd, mc_lkup_data_mask.wrd,
			NI_TOP_NI_MC_INDX_LKUP_DATA);

	mc_lkup_access.wrd = 0;
	mc_lkup_access_mask.wrd = 0;

	mc_lkup_access.bf.access = 1;
	mc_lkup_access.bf.address = mc_index;
	mc_lkup_access.bf.rbw = CS_WRITE;

	mc_lkup_access_mask.bf.access = 1;
	mc_lkup_access_mask.bf.rbw = 1;
	mc_lkup_access_mask.bf.address = 0xFF;
	write_reg(mc_lkup_access.wrd, mc_lkup_access_mask.wrd,
			NI_TOP_NI_MC_INDX_LKUP_ACCESS);
	do {
		access = (NI_READL_MODE(NI_TOP_NI_MC_INDX_LKUP_ACCESS) & 0x80000000);
		udelay(1);
	} while (access == 0x80000000);
}

/* #if defined(MULTIPLE_VTABLE) || defined(CS_UU_TEST) */
u16 cs_ni_get_mc_table(u8 mc_index)
{
	NI_TOP_NI_MC_INDX_LKUP_ACCESS_t mc_lkup_access, mc_lkup_access_mask;
	u8 access;
	u32 mc_table_data;

	mc_lkup_access.wrd = 0;
	mc_lkup_access_mask.wrd = 0;

	mc_lkup_access.bf.access = 1;
	mc_lkup_access.bf.address = mc_index;
	mc_lkup_access.bf.rbw = CS_READ;

	mc_lkup_access_mask.bf.access = 1;
	mc_lkup_access_mask.bf.address = 0xFF;
	mc_lkup_access_mask.bf.rbw = 1;

	write_reg(mc_lkup_access.wrd, mc_lkup_access_mask.wrd,
			NI_TOP_NI_MC_INDX_LKUP_ACCESS);

	do {
		access = (NI_READL_MODE(NI_TOP_NI_TX_VOQ_LKUP_ACCESS) & 0x80000000);
		udelay(1);
	} while (access == 0x80000000);

	mc_table_data = NI_READL_MODE(NI_TOP_NI_MC_INDX_LKUP_DATA) & 0xFFFF;
	//printk("MC Lkup Table\t %3d :\t  0x%04X\n", mc_index, mc_table_data);
	return mc_table_data;
}
/* #endif */

static u32 cs_ni_get_phy_vendor(int phy_addr)
{
	u32 reg_val;
	reg_val = (ni_mdio_read(phy_addr, 0x02) << 16) +
		ni_mdio_read(phy_addr, 0x03);

	return reg_val;
}

static void cs_ne_init_cfg(void)
{
	int i;
	//u32 reg_val;
	ni_info_t *ni;
	NI_TOP_NI_CPUXRAM_CFG_t ni_xram_cfg, ni_xram_cfg_mask;
#ifdef BYPASS_FE
	NI_TOP_NI_RX_CNTRL_CONFIG1_0_t rx_ctl_cfg1, rx_ctl_cfg1_mask;
#endif
	NI_TOP_NI_RX_CNTRL_CONFIG0_0_t rx_ctl_cfg0, rx_ctl_cfg0_mask;
	NI_TOP_NI_PKT_LEN_CONFIG_t pkt_len_cfg, pkt_len_cfg_mask;
	NI_TOP_NI_ETH_MGMT_PT_CONFIG_t mgmt_pt_cfg, mgmt_pt_cfg_mask;
	NI_TOP_NI_MISC_CONFIG_t misc_config, misc_config_mask;
	PER_MDIO_CFG_t mdio_cfg, mdio_cfg_mask;
	NI_TOP_NI_RX_AGG_CONFIG_t rx_agg_config, rx_agg_config_mask;
#ifndef CONFIG_CORTINA_FPGA
	GLOBAL_IO_DRIVE_STRENGTH_t driver_strength, driver_strength_mask;
#endif
	cs_ni_reset();

	/*
	 * acarr - Mar-10-2011
	 * For FPGA reduce ready signal threshold from NI-->SCH.
	 * This is because the NI will get an TX FIFO Overflow if too much data
	 * is dequeued from the scheduler.
	 * Default value was: 0x082a00f8
	 * New value is:      0x08298078
	 * This will make the:
	 * rdy_low_thld_ge=>0xc0
	 * rdy_high_thld_ge=>0x78
	 */
#ifdef CONFIG_CORTINA_FPGA
	{
		// acarr - Mar-10-2011
		NI_TOP_NI_SCH_BP_THLD_ETH_t ni_sch_bp_thld_eth_cfg;
		NI_TOP_NI_SCH_BP_THLD_ETH_t ni_sch_bp_thld_eth_mask;
		ni_sch_bp_thld_eth_cfg.wrd = 0x08298078;
		ni_sch_bp_thld_eth_mask.wrd = 0xffffffff;
		write_reg(ni_sch_bp_thld_eth_cfg.wrd,
				ni_sch_bp_thld_eth_mask.wrd,
				NI_TOP_NI_SCH_BP_THLD_ETH);
	}
#else
#ifdef CS75XX_HW_ACCEL_TUNNEL
	/* 
	 * BUG#40558 - [R1.2] KNL2.6.36 REFS will generate TX CRC ERR frames 
	 * in HW IPLIP testing
	 * Solution:
	 * Change NI_TOP_NI_SCH_BP_THLD_ETH.rdy_high_thld_ge to 'd216 
	 * and NI_TOP_NI_SCH_BP_THLD_ETH.rdy_low_thld_ge to 'd224. 
	 * This will allow Ethernet TxFIFO to absorb 4 max bursts 
	 * from QM before becoming full.
	 */
	NI_WRITEL_MODE(0x0829c0d8, NI_TOP_NI_SCH_BP_THLD_ETH);
#else
	/* ASIC use default value */
	NI_WRITEL_MODE(0x082a00f8, NI_TOP_NI_SCH_BP_THLD_ETH);
#endif
#endif

	/* Hold all macs in reset */
	for (i = 0; i < GE_PORT_NUM; i++)
		NI_WRITEL_MODE(0xc0100800, NI_TOP_NI_ETH_MAC_CONFIG0_0 + (i * 12));

	if (ne_initialized == 0) {
		ne_initialized = 1;
		/* initli private data */
		ni = &ni_private_data;
		memset((void *)ni, 0, sizeof(ni_info_t));

		/* FIXME: We may need different value in ASIC */
		mdio_cfg.wrd = 0;
		mdio_cfg_mask.wrd = 0;
		mdio_cfg.bf.mdio_pre_scale = 0x20; /* MDC clock */
		mdio_cfg_mask.bf.mdio_pre_scale = 0xFFFF;
		write_reg(mdio_cfg.wrd, mdio_cfg_mask.wrd, PER_MDIO_CFG);
	}

#ifndef CONFIG_CORTINA_FPGA
	/* Engineer board adjust driver current to 12mA */
	driver_strength.wrd = 0;
	driver_strength_mask.wrd = 0;
	driver_strength.bf.gmac_ds = 3;
	driver_strength_mask.bf.gmac_ds = 3;
	write_reg(driver_strength.wrd, driver_strength.wrd,
			GLOBAL_IO_DRIVE_STRENGTH);
#endif

	/* Receive port calendar is enabled. */
	rx_agg_config.wrd = 0;
	rx_agg_config_mask.wrd = 0;
	rx_agg_config.bf.rx_port_cal_dis = 0;
	rx_agg_config_mask.bf.rx_port_cal_dis = 1;
	write_reg(rx_agg_config.wrd, rx_agg_config_mask.wrd,
			NI_TOP_NI_RX_AGG_CONFIG);
#ifdef CS752X_LINUX_MODE
	/* config port calendar */
	cs_ni_port_cal_cfg();
	//printk("Ni Rx Port Cal Table Map:\n");
	//printk("0 - GE0\n");
	//printk("1 - GE1\n");
	//printk("2 - GE2\n");
	//printk("3 - CPU\n");
	//printk("4 - Crypto\n");
	//printk("5 - Encap\n");
	//printk("6 - Mcast\n");
	//printk("7 - Mirror\n");
	//printk("*************************\n");
	//for (i = 0; i < NI_MAX_PORT_CALENDAR; i++)
	//	ni_get_port_calendar(i);
#endif

	/* initial QM */
	cs_qm_init_cfg();

	/* initial NI XRAM */
	cs_ni_init_xram_mem();

	for (i = 0; i < XRAM_RX_INSTANCE; i++)
		cs_ni_set_xram_cfg(i, XRAM_DIRECTION_RX);

	for (i = 0; i < XRAM_TX_INSTANCE; i++)
		cs_ni_set_xram_cfg(i, XRAM_DIRECTION_TX);

	ni_xram_cfg.wrd = 0x3ff;
	ni_xram_cfg_mask.wrd = 0x3ff;

	/* The xram resets needs a toggle to load the pointers correctly */
	write_reg(ni_xram_cfg.wrd, ni_xram_cfg_mask.wrd, NI_TOP_NI_CPUXRAM_CFG);
	ni_xram_cfg.wrd = 0x0;
	write_reg(ni_xram_cfg.wrd, ni_xram_cfg_mask.wrd, NI_TOP_NI_CPUXRAM_CFG);

#ifdef CS752X_LINUX_MODE
	cs_qm_init_cpu_path_cfg();
#endif

#ifdef CS752x_DMA_LSO_MODE
	/* Initial TX DMA LSO */
	cs_init_tx_dma_lso();
#endif

#ifdef BYPASS_FE
	/* By pass FE */
	rx_ctl_cfg1.wrd = 0;
	rx_ctl_cfg1_mask.wrd = 0;
	rx_ctl_cfg1.bf.rxctrl_byp_en = 1;
	rx_ctl_cfg1_mask.bf.rxctrl_byp_en = 1;
	rx_ctl_cfg1.bf.rxctrl_byp_cpuptp = 1;
	rx_ctl_cfg1_mask.bf.rxctrl_byp_cpuptp = 1;

	/* 8 instances, By pass FE */
	rx_ctl_cfg1.wrd = 0;
	rx_ctl_cfg1_mask.wrd = 0;
	for (i = 0; i < XRAM_RX_INSTANCE; i++)
		write_reg(rx_ctl_cfg1.wrd, rx_ctl_cfg1_mask.wrd,
				NI_TOP_NI_RX_CNTRL_CONFIG1_0 + (i * 8));
#else
	for (i = 0; i < XRAM_RX_INSTANCE; i++)
		NI_WRITEL_MODE(0, NI_TOP_NI_RX_CNTRL_CONFIG1_0 + (i * 8));

	/* Allow NI send small size packet from CPU port */
	rx_ctl_cfg0.wrd = 0;
	rx_ctl_cfg0_mask.wrd = 0;
	rx_ctl_cfg0.bf.runt_drop_dis = 1;
	rx_ctl_cfg0_mask.bf.runt_drop_dis = 1;
	/* For GE0, GE1, GE2, MCAST,
	 * MIRROR ports you should have a value of 0x00400400.
	 * Only CPU, Crypto, Encap ports should have 0x00400440
	 * 0 - GE0
	 * 1 - GE1
	 * 2 - GE2
	 * 3 - CPU
	 * 4 - Crypto
	 * 5 - Encap
	 * 6 - Mcast
	 * 7 - Mirror */
	for (i = 3; i < 6; i++)
		write_reg(rx_ctl_cfg0.wrd, rx_ctl_cfg0_mask.wrd,
			NI_TOP_NI_RX_CNTRL_CONFIG0_0 + (i * 8));
#endif /* BYPASS_FE */

	/* NI_SCH Back Pressure Reg, set for eth0,1,2 */
	for (i = 0; i < 8; i++) {
		NI_TOP_NI_CPUXRAM_SCH_BP_CFG_0_t sch_bp_reg;
		sch_bp_reg.bf.xram_sch_rdy_mode = 1;
		sch_bp_reg.bf.xram_sch_rdy_free_thld = 30;
		NI_WRITEL_MODE(sch_bp_reg.wrd, NI_TOP_NI_CPUXRAM_SCH_BP_CFG_0 + i * 4);
	}

	pkt_len_cfg.wrd = 0;
	pkt_len_cfg_mask.wrd = 0;
	/* Maximum allowed Receive Packet size */
	pkt_len_cfg.bf.max_pkt_size = MAX_FRAME_SIZE;
	/* Minimum allowed Receive Packet size */
	pkt_len_cfg.bf.min_pkt_size = MIN_FRAME_SIZE;

	pkt_len_cfg_mask.bf.max_pkt_size = 0x3FFF;
	pkt_len_cfg_mask.bf.min_pkt_size = 0x3FF;
	write_reg(pkt_len_cfg.wrd, pkt_len_cfg_mask.wrd,
			NI_TOP_NI_PKT_LEN_CONFIG);

	/* GE 1 assigned as management port */
	mgmt_pt_cfg.wrd = 0;
	mgmt_pt_cfg_mask.wrd = 0;
#ifdef CS752X_MANAGEMENT_MODE
	mgmt_pt_cfg.bf.port_to_cpu = GE_PORT1;
#else
	mgmt_pt_cfg.bf.port_to_cpu = 3;
#endif /* CS752X_MANAGEMENT_MODE */
	/* management port data is sent to XRAM only */
	mgmt_pt_cfg.bf.mgmt_pt_to_fe_also = 0;
	mgmt_pt_cfg_mask.bf.port_to_cpu = 0x3;
	mgmt_pt_cfg_mask.bf.mgmt_pt_to_fe_also = 1;
	write_reg(mgmt_pt_cfg.wrd, mgmt_pt_cfg_mask.wrd,
			NI_TOP_NI_ETH_MGMT_PT_CONFIG);

	/*
	 * Tx/RX MIB counters counts statistics from good and bad frames,
	 * for debug purpose.
	 * */
	misc_config.wrd = 0;
	misc_config_mask.wrd = 0;
	misc_config.bf.rxmib_mode = 1;
	misc_config.bf.txmib_mode = 1;
	misc_config_mask.bf.rxmib_mode = 1;
	misc_config_mask.bf.txmib_mode = 1;
	misc_config_mask.bf.mc_accept_all = 1;
	write_reg(misc_config.wrd, misc_config_mask.wrd, NI_TOP_NI_MISC_CONFIG);
#ifdef BYPASS_FE
	/* Bypass bypass_pr, bypass_pe for debugging QM linux mode */
	NI_WRITEL_MODE(0x800010d8, FETOP_FE_PE_CONFIG);
#endif /* BYPASS_FE */

#ifdef CS752X_LINUX_MODE
	/* Insert VoQ 48 ~ 111 CPU header for debugging QM linux mode */
#define PE_SET_MIN_CYCLE_PPARSER 0xC000
#define PE_SEND_ALL_CPU_HDR 0x3FF
	NI_WRITEL_MODE(PE_SEND_ALL_CPU_HDR | PE_SET_MIN_CYCLE_PPARSER,
			FETOP_FE_PE_CONFIG_1);

	cs_ni_voq_map_cfg();
	//FIXME: for debug temp disab RX flow control
	//for(i = 0; i < 3 ; i++) {
	//ni_disable_rx_flow_control(i);
	//}
#endif /* CS752X_LINUX_MODE */
} /* cs_ne_init_cfg */

static inline void cs_ni_netif_stop(mac_info_t *tp)
{
	/*
	 * FIXME!! need to worry about this.. since all 3 NIs share the
	 * same NAPI, we might not want to disable NAPI just because one
	 * of the interface goes down.
	 */
#ifdef CS752X_NI_NAPI
	//napi_disable(&ni_private_data.napi);
	napi_disable(&tp->napi);
#endif

	netif_tx_disable(tp->dev);
	netif_carrier_off(tp->dev);
} /* cs_ni_netif_stop */

static inline void cs_ni_netif_start(mac_info_t *tp)
{
	netif_tx_wake_all_queues(tp->dev);

	/*
	 * FIXME!! need to check phy link status before calling the following
	 * line.
	 */
	netif_carrier_on(tp->dev);

	/*
	 * FIXME! Making sure the NAPI wasn't enabled before calling the
	 * following, because other NIs might still be up and running.
	 */
#ifdef CS752X_NI_NAPI
	//napi_enable(&ni_private_data.napi);
	napi_enable(&tp->napi);
#endif

	//cs_enable_ints(tp);	/* FIXME: not finished */

} /* cs_ni_netif_start */
static void cs_ni_disable_interrupts(void)
{
	int i;
	NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_TXPKT_INTENABLE_0);

	for (i=0; i<= XRAM_INST_MAX; i++) {
		NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + (i * 8));
	}
}

static inline void cs_ni_enable_interrupts(void)
{
	int i;
	// tx complete interrupts are not enabled
	for (i = 0; i <= XRAM_INST_MAX; i++)
		NI_WRITEL_MODE(ni_private_data.intr_cpuxram_rxpkt_mask,
			NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + (i * 8));
}

/*
 * Reset all TX rings.
 */
static inline void cs_ni_reset_tx_ring(void)
{
	ni_info_t *ni = &ni_private_data;
	dma_swtxq_t *swtxq;
	dma_rptr_t rptr_reg;
	u32 desc_count;
	dma_txdesc_t *curr_desc, *tmp_desc;
	dma_txdesc_0_t word0;
	int i, new_idx;

	for (i = 0; i < NI_DMA_LSO_TXQ_NUM; i++) {
		swtxq = &ni->swtxq[i];
		// swtxq->intr_cnt++; // needed?
		rptr_reg.bits32 = NI_READL_MODE(swtxq->rptr_reg);

		while (swtxq->wptr != swtxq->finished_idx) {
			curr_desc = swtxq->desc_base + swtxq->finished_idx;
			word0.bits32 = curr_desc->word0.bits32;
			// we do not check ownership at all as in reset
			desc_count = word0.bits.desc_cnt;
			if (desc_count > 1)
				new_idx = (swtxq->finished_idx + desc_count -1) &
					(swtxq->total_desc_num - 1);
			else
				new_idx = swtxq->finished_idx;
			tmp_desc = swtxq->desc_base + new_idx;

			if (swtxq->tx_skb[new_idx]) {
				dev_kfree_skb_any(swtxq->tx_skb[new_idx]);
				swtxq->tx_skb[new_idx] = NULL;
				swtxq->total_finished ++; // or not update?
				swtxq->finished_idx = RWPTR_ADVANCE_ONE(new_idx,
						swtxq->total_desc_num);
			}
		}

		spin_lock_init(&swtxq->lock);
		swtxq->finished_idx = rptr_reg.bits32;
		swtxq->wptr = rptr_reg.bits.rptr;
		NI_WRITEL_MODE(rptr_reg.bits32, swtxq->wptr_reg);
		DBG(printk("\t%s::queue %d, set rptr & wptr to %d\n", __func__,
					i, rptr_reg.bits32));
	}

#if 0 /*defined(CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT)*/
	clean_skb_recycle_buffer();
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
	clean_skb_recycle_buffer(NULL);
	smp_call_function((void *)clean_skb_recycle_buffer, NULL, 1);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
}

static void cs_ni_reset_task(struct work_struct *work)
{
	mac_info_t *tp;
	struct net_device *dev;
	int i;
	DBG(printk("%s \n" , __func__));
	rtnl_lock();

#ifdef CS752X_NI_NAPI
	//napi_disable(&ni_private_data.napi);
	for (i = 0; i < CS_NI_IRQ_DEV; i ++) {
		dev = ni_private_data.dev[i];
		if (dev == NULL)
			continue;
		tp = netdev_priv(dev);
		napi_disable(&tp->napi);
	}
#endif
	cancel_tx_completion_timer(dev);
	// reset tx ring
	cs_ni_reset_tx_ring();

	for (i=0; i<GE_PORT_NUM; i++) {
		dev = ni_private_data.dev[i];
		if (dev == NULL)
			continue;
		tp = netdev_priv(dev);
		netif_tx_wake_all_queues(dev);	// enable tx
	}
#ifdef CS752X_NI_NAPI
	//napi_enable(&ni_private_data.napi);
	for (i = 0; i < CS_NI_IRQ_DEV; i ++) {
		dev = ni_private_data.dev[i];
		if (dev == NULL)
			continue;
		tp = netdev_priv(dev);
		napi_enable(&tp->napi);
	}
#endif
	cs_ni_enable_interrupts();

	rtnl_unlock();
} /* cs_ni_reset_task */

#ifdef CS752X_MANAGEMENT_MODE
static int cs_mfe_set_mac_address(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	u8 *dev_addr;
	u32 mac_fifth_byte, mac_sixth_byte, low_mac;
	NI_TOP_NI_ETH_MAC_CONFIG2_0_t mac_config2, mac_config2_mask;
	NI_TOP_NI_MAC_ADDR1_t mac_addr1, mac_addr1_mask;

	dev_addr = dev->dev_addr;

	low_mac = dev_addr[0] | (dev_addr[1] << 8) | (dev_addr[2] << 16) |
		(dev_addr[3] << 24);
	mac_fifth_byte = dev_addr[4];
	mac_sixth_byte = dev_addr[5];

	printk("Mgmt Setting MAC address for GE-%d: ", tp->port_id);
	printk("%02x:%02x:", dev->dev_addr[0], dev->dev_addr[1]);
	printk("%02x:%02x:", dev->dev_addr[2], dev->dev_addr[3]);
	printk("%02x:%02x\n", dev->dev_addr[4], dev->dev_addr[5]);

	/*
	 * used as SA while sending pause frames. Also used to detect WOL magic
	 * packets.
	 */
	NI_WRITEL_MODE(low_mac, NI_TOP_NI_MAC_ADDR0);

	mac_addr1.wrd = 0;
	mac_addr1_mask.wrd = 0;
	mac_addr1.bf.mac_addr1 = mac_fifth_byte;
	mac_addr1_mask.bf.mac_addr1 = 0xFF;
	write_reg(mac_addr1.wrd, mac_addr1_mask.wrd, NI_TOP_NI_MAC_ADDR1);

	mac_config2.wrd = 0;
	mac_config2_mask.wrd = 0;
	mac_config2.bf.mac_addr6 = mac_sixth_byte;
	mac_config2_mask.bf.mac_addr6 = mac_sixth_byte;
	switch (tp->port_id) {
	case GE_PORT0:
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0);
		break;
	case GE_PORT1:
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0 + 12);
		break;
	case GE_PORT2:
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0 + 24);
		break;
	}

	return 0;
}
#endif /* CS752X_MANAGEMENT_MODE */


static int cs_init_mac_address(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	u8 *dev_addr;
	u32 mac_fifth_byte, mac_sixth_byte, low_mac;
	NI_TOP_NI_ETH_MAC_CONFIG2_0_t mac_config2, mac_config2_mask;
	NI_TOP_NI_MAC_ADDR1_t mac_addr1, mac_addr1_mask;

	dev_addr = dev->dev_addr;

	low_mac = dev_addr[0] | (dev_addr[1] << 8) | (dev_addr[2] << 16) |
		(dev_addr[3] << 24);
	mac_fifth_byte = dev_addr[4];
	mac_sixth_byte = dev_addr[5];

	printk("Init MAC address for GE-%d: ", tp->port_id);
	printk("%02x:%02x:", dev->dev_addr[0], dev->dev_addr[1]);
	printk("%02x:%02x:", dev->dev_addr[2], dev->dev_addr[3]);
	printk("%02x:%02x\n", dev->dev_addr[4], dev->dev_addr[5]);

	/*
	 * used as SA while sending pause frames. Also used to detect WOL magic
	 * packets.
	 */
	NI_WRITEL_MODE(low_mac, NI_TOP_NI_MAC_ADDR0);

	mac_addr1.wrd = 0;
	mac_addr1_mask.wrd = 0;
	mac_addr1.bf.mac_addr1 = mac_fifth_byte;
	mac_addr1_mask.bf.mac_addr1 = 0xFF;
	write_reg(mac_addr1.wrd, mac_addr1_mask.wrd, NI_TOP_NI_MAC_ADDR1);

	mac_config2.wrd = 0;
	mac_config2_mask.wrd = 0;
	mac_config2.bf.mac_addr6 = mac_sixth_byte;
	mac_config2_mask.bf.mac_addr6 = mac_sixth_byte;
	switch (tp->port_id) {
	case GE_PORT0:
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0);
		break;
	case GE_PORT1:
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0 + 12);
		break;
	case GE_PORT2:
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0 + 24);
		break;
	}

	return 0;
}/* cs_init_mac_address() */

static int cs_ni_set_flowctrl(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	NI_TOP_NI_ETH_MAC_CONFIG0_0_t mac_config0, mac_config0_mask;

    printk("%s: flowctrl 0x%x\n", __func__ , tp->link_config.flowctrl);	
	mac_config0.wrd = 0;
	mac_config0_mask.wrd = 0;
	if (!(tp->link_config.flowctrl & FLOW_CTRL_TX)) {
	    mac_config0.bf.tx_flow_disable = 1;
	}

	if (!(tp->link_config.flowctrl & FLOW_CTRL_RX)) {
	    mac_config0.bf.rx_flow_disable = 1;
	}
    
    mac_config0_mask.bf.tx_flow_disable = 1;
    mac_config0_mask.bf.rx_flow_disable = 1;
    
	switch (tp->port_id) {
	case GE_PORT0:
		write_reg(mac_config0.wrd, mac_config0_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG0_0);
		break;
	case GE_PORT1:
		write_reg(mac_config0.wrd, mac_config0_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG0_0 + 12);
		break;
	case GE_PORT2:
		write_reg(mac_config0.wrd, mac_config0_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG0_0 + 24);
		break;
	}

	return 0;
}/* cs_ni_set_flowctrl() */

static void set_mac_swap_order(unsigned char *dest, unsigned char *src, int len)
{
	int i;
	for (i = 0; i < len; i++)
		*(dest + len - 1 - i) = *(src + i);
}

static void cs_init_lpb_an_bng_mac(void)
{
	mac_info_t *tp;
	fe_lpb_entry_t lpb_entry;
	fe_an_bng_mac_entry_t abm_entry;
	struct net_device *dev;
	int i, ret;

	/* setting up AN BNG MAC first */
	memset(&abm_entry, 0x0, sizeof(abm_entry));
	abm_entry.sa_da = 0;
	abm_entry.valid = 1;
	abm_entry.pspid_mask = 0;
	for (i = 0; i < 3; i++) {
		dev = ni_private_data.dev[i];
		tp = netdev_priv(dev);
		abm_entry.pspid = tp->port_id;
		set_mac_swap_order(abm_entry.mac,
				(unsigned char*)tp->mac_addr, 6);
		//memcpy(abm_entry.mac, tp->mac_addr, 6);
		if (tp->an_bng_mac_idx == 0xffff)
			ret = cs_fe_table_add_entry(FE_TABLE_AN_BNG_MAC,
					&abm_entry, &tp->an_bng_mac_idx);
		else
			ret = cs_fe_table_set_entry(FE_TABLE_AN_BNG_MAC,
					tp->an_bng_mac_idx, &abm_entry);
		if (ret != 0) {
			printk("%s:%d:unable to set up MAC to FE for port#%d "
					"ret %d\n", __func__, __LINE__,
					tp->port_id, ret);
			// FIXME! any debug method?
		} else {
//			printk("%s:%d:Setting port#%d with an_bng_mac idx %d\n",
//					__func__, __LINE__, tp->port_id,
//					tp->an_bng_mac_idx);
		}

		/* set AN_BNG_MAC for PE#0 */
		abm_entry.pspid = ENCRYPTION_PORT;
		if (tp->an_bng_mac_idx_pe0 == 0xffff)
			ret = cs_fe_table_add_entry(FE_TABLE_AN_BNG_MAC,
					&abm_entry, &tp->an_bng_mac_idx_pe0);
		else
			ret = cs_fe_table_set_entry(FE_TABLE_AN_BNG_MAC,
					tp->an_bng_mac_idx_pe0, &abm_entry);
		if (ret != 0) {
			printk("%s:%d:unable to set up MAC to FE for PE#0 "
					"ret %d\n", __func__, __LINE__,
					ret);
		}

		/* set AN_BNG_MAC for PE#1 */
		abm_entry.pspid = ENCAPSULATION_PORT;
		if (tp->an_bng_mac_idx_pe1 == 0xffff)
			ret = cs_fe_table_add_entry(FE_TABLE_AN_BNG_MAC,
					&abm_entry, &tp->an_bng_mac_idx_pe1);
		else
			ret = cs_fe_table_set_entry(FE_TABLE_AN_BNG_MAC,
					tp->an_bng_mac_idx_pe1, &abm_entry);
		if (ret != 0) {
			printk("%s:%d:unable to set up MAC to FE for PE#1 "
					"ret %d\n", __func__, __LINE__,
					ret);
		}
	}

	/* setting up LPB */
	memset(&lpb_entry, 0x0, sizeof(lpb_entry));
	lpb_entry.pvid = 4095;
	for (i = 0; i < FE_LPB_ENTRY_MAX; i++) {
		lpb_entry.lspid = i;
		ret = cs_fe_table_set_entry(FE_TABLE_LPB, i, &lpb_entry);
		if (ret != 0)
			printk("%s:error setting up LPB\n", __func__);
	}
	return;
}

static void cs_ne_global_interrupt(void)
{
#ifndef CONFIG_GENERIC_IRQ
	GLOBAL_NETWORK_ENGINE_INTENABLE_0_t global_interrupt;
	GLOBAL_NETWORK_ENGINE_INTENABLE_0_t global_interrupt_mask;

	/* enable golable NI, XRAM interrupt */
	global_interrupt.wrd = 0;
	global_interrupt_mask.wrd = 0;

#ifndef CONFIG_INTR_COALESCING
	global_interrupt.bf.NI_XRAM_RXe = 1;
	global_interrupt_mask.bf.NI_XRAM_RXe = 1;
#endif

	/* enable gloabl QM/SCH interrupt for QM linux mode */
	global_interrupt.bf.SCHe = 1;
	global_interrupt_mask.bf.SCHe = 1;
#ifdef CS752X_LINUX_MODE
	global_interrupt.bf.QMe = 0;
	global_interrupt_mask.bf.QMe = 1;
#endif	/* CS752X_LINUX_MODE */

	global_interrupt.bf.NI_XRAM_TXe = 1;
	global_interrupt_mask.bf.NI_XRAM_TXe = 1;
	global_interrupt.bf.NIe = 1;
	global_interrupt_mask.bf.NIe = 1;

	write_reg(global_interrupt.wrd, global_interrupt_mask.wrd,
			GLOBAL_NETWORK_ENGINE_INTENABLE_0);
#else
	extern int cs_ne_irq_init(const struct cs_ne_irq_info *irq_module);
	cs_ne_irq_init(&cs_ne_global_irq_info);
#endif
}

static void cs_ni_init_interrupt_cfg(void)
{
	int i;
	ni_info_t *ni;
#ifdef CONFIG_INTR_COALESCING
	NI_TOP_NI_CPUXRAM_INT_COLSC_CFG_0_t reg;
#endif

	ni = &ni_private_data;

	/* enable golable NI, XRAM interrupt */
	cs_ne_global_interrupt();

#ifndef CONFIG_GENERIC_IRQ
	/* Enable NI interrupt */
	for (i = 0; i < XRAM_INST_MAX; i++) {	/* not only 3, max is 8 */
		ni->intr_port_mask[i] = 0x1FF;
		NI_WRITEL_MODE(ni->intr_port_mask[i], NI_TOP_NI_PORT_0_INTENABLE_0
			+ (i * 8));
	}

	ni->intr_mask = 0xFFFFFFFF;
	ni->intr_txem_mask = 0x3FF;
#if 1
	ni->intr_rxfifo_mask = 0x0;
	ni->intr_cpuxram_cnt_mask = 0x0;
	ni->intr_cpuxram_err_mask = 0x0;
#else
	ni->intr_rxfifo_mask = 0x3FFF;
	ni->intr_cpuxram_cnt_mask = 0xFFFFFFFF;
	ni->intr_cpuxram_err_mask = 0xFFFFFF;
#endif
	//ni->intr_cpuxram_rxpkt_mask = 0x1FF;
	//ni->intr_cpuxram_txpkt_mask = 0x3; //FIXME: if enable, need clear
	ni->intr_cpuxram_rxpkt_mask = 0x1;

	NI_WRITEL_MODE(ni->intr_mask, NI_TOP_NI_INTENABLE_0);
	NI_WRITEL_MODE(ni->intr_rxfifo_mask, NI_TOP_NI_RXFIFO_INTENABLE_0);
	NI_WRITEL_MODE(ni->intr_txem_mask, NI_TOP_NI_TXEM_INTENABLE_0);
	//NI_WRITEL_MODE(ni->intr_cpuxram_txpkt_mask, NI_TOP_NI_CPUXRAM_TXPKT_INTENABLE_0);

	NI_WRITEL_MODE(ni->intr_cpuxram_cnt_mask, NI_TOP_NI_CPUXRAM_CNTR_INTENABLE_0);
	/* Keep Xram interrupt disabled till device is registered */
	NI_WRITEL_MODE(ni->intr_cpuxram_err_mask, NI_TOP_NI_CPUXRAM_ERR_INTENABLE_0);
#endif

#ifndef CONFIG_INTR_COALESCING
	NI_WRITEL_MODE(ni->intr_cpuxram_rxpkt_mask, NI_TOP_NI_CPUXRAM_RXPKT_INTENABLE_0);
#endif

#ifdef CS752X_LINUX_MODE
#ifndef CONFIG_GENERIC_IRQ
	/* Enable QM interrupt */
	ni->intr_qm_mask = 0x077703FF; /* enable all QM interrupt for debug */
	NI_WRITEL_MODE(ni->intr_qm_mask, QM_INTENABLE_0);
	/* Enable SCH interrupt */
	ni->intr_sch_mask = 0x1F; /* enable all SCH interrupt for debug */
	NI_WRITEL_MODE(ni->intr_sch_mask, SCH_INTENABLE_0);
#endif
#endif


#ifdef CS752x_DMA_LSO_MODE
	/* Enable DMA LSO interrupt */
	/* enable all LSO interrupt for debug */
	ni->intr_dma_lso_mask = 0x1FFFFFF;
	NI_WRITEL_MODE(ni->intr_sch_mask, DMA_DMA_LSO_DMA_LSO_INTENABLE_0);
	/* enable all LSO interrupt for debug */
	//ni->intr_dma_lso_desc_mask = 0x3;
	ni->intr_dma_lso_desc_mask = 0x0;
	NI_WRITEL_MODE(ni->intr_sch_mask, DMA_DMA_LSO_DESC_INTENABLE);

	for (i = 0; i < NI_DMA_LSO_RXQ_NUM; i++) {
		ni->intr_lso_rx_mask[i] = 0x3F;
		NI_WRITEL_MODE(ni->intr_lso_rx_mask[i],
				DMA_DMA_LSO_RXQ0_INTENABLE + (i * 8));
	}

	for (i = 0; i < NI_DMA_LSO_TXQ_NUM; i++) {
		ni->intr_lso_rx_mask[i] = 0xF;
		NI_WRITEL_MODE(ni->intr_lso_rx_mask[i],
				DMA_DMA_LSO_TXQ0_INTENABLE + (i * 8));
	}

	ni->intr_dma_lso_bmc_mask = 0x1;
	NI_WRITEL_MODE(ni->intr_dma_lso_bmc_mask, DMA_DMA_LSO_BMC0_INTENABLE);
#endif /* CS752x_DMA_LSO_MODE */

#ifdef CONFIG_INTR_COALESCING
	/* Interrupt on first packet and then number of packets */
	reg.bf.int_colsc_en = 0; /* disable as default */
	reg.bf.int_colsc_first_en = NI_INTR_COALESCING_FIRST_EN;
	reg.bf.int_colsc_pkt = NI_INTR_COALESCING_PKT;
	NI_WRITEL_MODE(reg.wrd, NI_TOP_NI_CPUXRAM_INT_COLSC_CFG_0);
#endif
}

static void ni_set_mac_tx_rx(u8 port, u8 flag)
{
	NI_TOP_NI_ETH_MAC_CONFIG0_0_t config0, config0_mask;
	u32 config_addr;

	config0.wrd = 0;
	config0_mask.wrd = 0;
	config0.bf.mac_tx_rst = 0; /* Normal operation */
	config0.bf.mac_rx_rst = 0; /* Normal operation */
	config0.bf.rx_en = flag; /* 0: disable Rx MAC */
	config0.bf.tx_en = flag; /* 0: disable Tx MAC */
	config0_mask.bf.mac_tx_rst = 1;
	config0_mask.bf.mac_rx_rst = 1;
	config0_mask.bf.rx_en = 1;
	config0_mask.bf.tx_en = 1;
	config_addr = NI_TOP_NI_ETH_MAC_CONFIG0_0;

	config_addr = (port * 12) + config_addr;

	write_reg(config0.wrd, config0_mask.wrd, config_addr);
}

void cs_ni_set_short_term_shaper(mac_info_t *tp)
{
	unsigned int shaper_rate;

	/* setting up short-term shaper in scheduler based on linkup sppeed */
	if (tp->link_config.speed == SPEED_10)
		shaper_rate = 20 * 1000000; /* 20 mbps */
	else if (tp->link_config.speed == SPEED_100)
		shaper_rate = 200 * 1000000; /* 200 mbps */
	else
		shaper_rate = 2000 * 1000000; /* 2 gbps */

	//printk("%s:: GE-%d shaper_rate %d Mbps\n",
	//		__func__, tp->port_id, (shaper_rate/1000000));
	cs752x_sch_set_port_rate_st(tp->port_id, shaper_rate);
}

void cs_ni_flow_control(mac_info_t *tp, u8 rx_tx, u8 flag)
{
	NI_TOP_NI_ETH_MAC_CONFIG0_0_t mac_status, mac_status_mask;
	u32 config_addr;
    u8 disabled;

    disabled = flag ? 0 : 1;

	ni_set_mac_tx_rx(tp->port_id, CS_DISABLE);

	mac_status.wrd = 0;
	mac_status_mask.wrd = 0;

	if (rx_tx == NI_RX_FLOW_CTRL) {
		mac_status.bf.rx_flow_disable = disabled;
		mac_status_mask.bf.rx_flow_disable = 0x1;
	}

	if (rx_tx == NI_TX_FLOW_CTRL) {
		mac_status.bf.tx_flow_disable = disabled;
		mac_status_mask.bf.tx_flow_disable = 0x1;
	}

	config_addr = NI_TOP_NI_ETH_MAC_CONFIG0_0 + (tp->port_id * 12);

	write_reg(mac_status.wrd, mac_status_mask.wrd, config_addr);

	ni_set_mac_tx_rx(tp->port_id, CS_ENABLE);

	return;
}

void cs_ni_set_mac_speed_duplex(mac_info_t *tp, int mac_interface)
{
	NI_TOP_NI_ETH_MAC_CONFIG0_0_t mac_status, mac_status_mask;
	NI_TOP_NI_ETH_INT_CONFIG1_t eth_cfg1, eth_cfg1_mask;
	NI_TOP_NI_ETH_INT_CONFIG2_t eth_cfg2, eth_cfg2_mask;
	u32 config_addr;
	int speed, duplex;

	ni_set_mac_tx_rx(tp->port_id, CS_DISABLE);

	mac_status.wrd = 0;
	mac_status_mask.wrd = 0;

	if (tp->link_config.speed == SPEED_10)
		speed = 1;
	else
		speed = 0;

	if (tp->link_config.duplex == DUPLEX_FULL)
		duplex = 0;
	else
		duplex = 1;

	mac_status.bf.speed = speed;
	mac_status.bf.duplex = duplex;
	mac_status_mask.bf.speed = 0x1;
	mac_status_mask.bf.duplex = 0x1;

	config_addr = NI_TOP_NI_ETH_MAC_CONFIG0_0 + (tp->port_id * 12);

	write_reg(mac_status.wrd, mac_status_mask.wrd, config_addr);

	printk(KERN_INFO "%s: speed %u Mb/s, %s duplex\n",
			tp->dev->name, tp->link_config.speed,
			tp->link_config.duplex ? "full" : "half");

	eth_cfg1.wrd = 0;
	eth_cfg1_mask.wrd = 0;
	eth_cfg2.wrd = 0;
	if (tp->port_id == GE_PORT0) {
		eth_cfg1.bf.int_cfg_ge0 = mac_interface;
		eth_cfg1_mask.bf.int_cfg_ge0 = 0x7;
		eth_cfg2.bf.tx_intf_lp_time_ge0 = 1;
		eth_cfg2_mask.wrd = 0xff;
	}
	if (tp->port_id == GE_PORT1) {
		eth_cfg1.bf.int_cfg_ge1 = mac_interface;
		eth_cfg1_mask.bf.int_cfg_ge1 = 0x7;
		eth_cfg2.bf.tx_intf_lp_time_ge1 = 1;
		eth_cfg2_mask.wrd = 0xff00;
	}
	if (tp->port_id == GE_PORT2) {
		eth_cfg1.bf.int_cfg_ge2 = mac_interface;
		eth_cfg1_mask.bf.int_cfg_ge2 = 0x7;
		eth_cfg2.bf.tx_intf_lp_time_ge2 = 1;
		eth_cfg2_mask.wrd = 0xff0000;
	}
	write_reg(eth_cfg1.wrd, eth_cfg1_mask.wrd, NI_TOP_NI_ETH_INT_CONFIG1);
	write_reg(eth_cfg2.wrd, eth_cfg2_mask.wrd, NI_TOP_NI_ETH_INT_CONFIG2);

	ni_set_mac_tx_rx(tp->port_id, CS_ENABLE);
}

int moca_set_phylink_status(int phy_link_status)
{
	struct net_device *pdev = ni_private_data.dev[GE_PORT1_CFG];
	int linkstat = (phy_link_status & 0x80) >> 7;
	int speedstat = (phy_link_status & 0x30) >> 4;
	int mac_interface;
	mac_info_t *tp = netdev_priv(pdev);

	if (unlikely(linkstat ^ netif_carrier_ok(pdev))) {
		tp->link_config.link = linkstat;
		printk(KERN_INFO "%s: link %s, ", pdev->name, linkstat ? "up" : "down");

		if (linkstat) {
			netif_tx_wake_all_queues(pdev);
			netif_carrier_on(pdev);

			/* config value to GMAC */
			/* bit 04, 05 means phy speed status 0x10 (1000), 0x01 (100), 0x00 (10) */
			switch (speedstat) {
			case 0x2: mac_interface = NI_MAC_PHY_RGMII_1000;
				  tp->link_config.speed = SPEED_1000;
				  break;
			case 0x1: mac_interface = NI_MAC_PHY_RGMII_100;
				  tp->link_config.speed = SPEED_100;
				  break;
			case 0x0: mac_interface = NI_MAC_PHY_RGMII_100;
				  tp->link_config.speed = SPEED_10;
				  break;
			default:
				  printk(KERN_WARNING "%s: MOCA_IOCTL_CHK_ETH_PHY got incorrect phy status!\n", __func__);
			}

			/* bit 06 means duplex */
			if ((phy_link_status & 0x40) >> 6)
				tp->link_config.duplex = DUPLEX_FULL;
			else
				tp->link_config.duplex = DUPLEX_HALF;

			cs_ni_set_mac_speed_duplex(tp, mac_interface);
		} else {
			netif_tx_disable(pdev);
			netif_carrier_off(pdev);
		}

		return (!!linkstat);

	}

	/* no change in netif */
	return 2;
}
EXPORT_SYMBOL(moca_set_phylink_status);

void cs_ni_set_eth_cfg(mac_info_t * tp, int config)
{
	NI_TOP_NI_ETH_INT_CONFIG1_t eth_config1, eth_config1_mask;
	NI_TOP_NI_ETH_INT_CONFIG2_t eth_config2, eth_config2_mask;

	eth_config1.wrd = 0;
	eth_config1_mask.wrd = 0;
	eth_config2.wrd = 0;
	eth_config2_mask.wrd = 0;
	switch (tp->port_id) {
	case GE_PORT0:
		if (config == NI_CONFIG_1) {
			eth_config1.bf.int_cfg_ge0 = tp->phy_mode;
			eth_config1.bf.phy_mode_ge0 = NORMAL_MODE;
			eth_config1.bf.tx_use_gefifo_ge0 = 1;
			eth_config1.bf.rmii_clksrc_ge0 = tp->rmii_clk_src;
			eth_config1_mask.bf.int_cfg_ge0 = 0x7;
			eth_config1_mask.bf.phy_mode_ge0 = 0x1;
			eth_config1_mask.bf.tx_use_gefifo_ge0 = 0x1;
			eth_config1_mask.bf.rmii_clksrc_ge0 = 0x1;
		} else {
			eth_config2.bf.inv_clk_in_ge0 = 0;
			eth_config2.bf.inv_rxclk_out_ge0 = 0;
			eth_config2.bf.power_dwn_rx_ge0 = 0;
			eth_config2.bf.power_dwn_tx_ge0 = 0;
			eth_config2.bf.tx_intf_lp_time_ge0 = 0;
			eth_config2_mask.bf.inv_clk_in_ge0 = 0x1;
			eth_config2_mask.bf.inv_rxclk_out_ge0 = 0x1;
			eth_config2_mask.bf.power_dwn_rx_ge0 = 0x1;
			eth_config2_mask.bf.power_dwn_tx_ge0 = 0x1;
			eth_config2_mask.bf.tx_intf_lp_time_ge0 = 0x1;
		}
		break;
	case GE_PORT1:
		if (config == NI_CONFIG_1) {
			eth_config1.bf.int_cfg_ge1 = tp->phy_mode;
			eth_config1.bf.phy_mode_ge1 = NORMAL_MODE;
			eth_config1.bf.tx_use_gefifo_ge1 = 1;
			eth_config1.bf.rmii_clksrc_ge1 = tp->rmii_clk_src;
			eth_config1_mask.bf.int_cfg_ge1 = 0x7;
			eth_config1_mask.bf.phy_mode_ge1 = 0x1;
			eth_config1_mask.bf.tx_use_gefifo_ge1 = 0x1;
			eth_config1_mask.bf.rmii_clksrc_ge1 = 0x1;
		} else {
			eth_config2.bf.inv_clk_in_ge1 = 0;
			eth_config2.bf.inv_rxclk_out_ge1 = 0;
			eth_config2.bf.power_dwn_rx_ge1 = 0;
			eth_config2.bf.power_dwn_tx_ge1 = 0;
			eth_config2.bf.tx_intf_lp_time_ge1 = 0;
			eth_config2_mask.bf.inv_clk_in_ge1 = 0x1;
			eth_config2_mask.bf.inv_rxclk_out_ge1 = 0x1;
			eth_config2_mask.bf.power_dwn_rx_ge1 = 0x1;
			eth_config2_mask.bf.power_dwn_tx_ge1 = 0x1;
			eth_config2_mask.bf.tx_intf_lp_time_ge1 = 0x1;
		}
		break;
	case GE_PORT2:
		if (config == NI_CONFIG_1) {
			eth_config1.bf.int_cfg_ge2 = tp->phy_mode;
			eth_config1.bf.phy_mode_ge2 = NORMAL_MODE;
			eth_config1.bf.tx_use_gefifo_ge2 = 1;
			eth_config1.bf.rmii_clksrc_ge2 = tp->rmii_clk_src;
			eth_config1_mask.bf.int_cfg_ge2 = 0x7;
			eth_config1_mask.bf.phy_mode_ge2 = 0x1;
			eth_config1_mask.bf.tx_use_gefifo_ge2 = 0x1;
			eth_config1_mask.bf.rmii_clksrc_ge2 = 0x1;
		} else {
			eth_config2.bf.inv_clk_in_ge2 = 0;
			eth_config2.bf.inv_rxclk_out_ge2 = 0;
			eth_config2.bf.power_dwn_rx_ge2 = 0;
			eth_config2.bf.power_dwn_tx_ge2 = 0;
			eth_config2.bf.tx_intf_lp_time_ge2 = 0;
			eth_config2_mask.bf.inv_clk_in_ge2 = 0x1;
			eth_config2_mask.bf.inv_rxclk_out_ge2 = 0x1;
			eth_config2_mask.bf.power_dwn_rx_ge2 = 0x1;
			eth_config2_mask.bf.power_dwn_tx_ge2 = 0x1;
			eth_config2_mask.bf.tx_intf_lp_time_ge2 = 0x1;
		}
		break;
	default:
		printk("%s:Unacceptable port id %d\n", __func__, tp->port_id);
		return;
	}
	if (config == NI_CONFIG_1)
		write_reg(eth_config1.wrd, eth_config1_mask.wrd,
				NI_TOP_NI_ETH_INT_CONFIG1);
	if (config == NI_CONFIG_2)
		write_reg(eth_config2.wrd, eth_config2_mask.wrd,
				NI_TOP_NI_ETH_INT_CONFIG2);

	return;
}


static void cs_ni_init_port(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	NI_TOP_NI_RX_CNTRL_CONFIG0_0_t rx_cntrl_config0, rx_cntrl_config0_mask;
	//int i;
#ifdef CONFIG_CORTINA_FPGA
	TM_TC_PAUSE_FRAME_PORT_t tm_pause_reg;
#endif
	/*
	printk("\t***** %s is INVOKED! ***** init %d\n", __func__,
			ne_initialized);
	*/
	if (ne_initialized != 0)
		return;

	tp->dev = dev;
	tp->rx_xram_intr = 0;

	/* The max number of buffers allocated to the port. */
	rx_cntrl_config0.wrd = 0;
	rx_cntrl_config0_mask.wrd = 0;
	//rx_cntrl_config0.bf.buff_use_thrshld = CPU_XRAM_BUFFER_NUM * (3 / 4);
#ifdef CS_UU_TEST
	rx_cntrl_config0.bf.runt_drop_dis = 1;	//FIXME: CH
	rx_cntrl_config0_mask.bf.runt_drop_dis = 1;	//FIXME: CH
#endif
	rx_cntrl_config0_mask.bf.buff_use_thrshld = 0x3FF;
	switch (tp->port_id) {
	case GE_PORT0:
		write_reg(rx_cntrl_config0.wrd, rx_cntrl_config0_mask.wrd,
				NI_TOP_NI_RX_CNTRL_CONFIG0_0);
		break;
	case GE_PORT1:
		write_reg(rx_cntrl_config0.wrd, rx_cntrl_config0_mask.wrd,
				NI_TOP_NI_RX_CNTRL_CONFIG0_0 + 8);
		break;
	case GE_PORT2:
		write_reg(rx_cntrl_config0.wrd, rx_cntrl_config0_mask.wrd,
				NI_TOP_NI_RX_CNTRL_CONFIG0_0 + 16);
		break;
	default:
		break;
	}

#ifdef CONFIG_CORTINA_FPGA
	/* Disable Pause frame */
	tm_pause_reg.wrd = NI_READL_MODE(TM_TC_PAUSE_FRAME_PORT + tp->port_id * 4);
	tm_pause_reg.bf.bm_dying_gasp_enable = 0;
	NI_WRITEL_MODE(tm_pause_reg.wrd, TM_TC_PAUSE_FRAME_PORT + tp->port_id * 4);
#endif
}

#ifdef CS752X_NI_TX_COMPLETE_INTERRUPT
static struct net_device *get_dev_from_lso_txqid(int lso_tx_qid)
{
	/* please check get_dma_lso_txqid(), these two functions have
	 * to match! */
	return ni_private_data.dev[(lso_tx_qid >> 1)];
}

static irqreturn_t cs_ni_tx_interrupt(int irq, void *dev_instance)
{
	int tx_qid = irq - GOLDENGATE_IRQ_DMA_TX0;
	struct net_device *dev;
	u32 status, tmp;

	status = NI_READL_MODE(DMA_DMA_LSO_TXQ0_INTERRUPT + tx_qid * 8);
	if (status != 0) {
		NI_WRITEL_MODE(0, DMA_DMA_LSO_TXQ0_INTENABLE + tx_qid * 8);
		NI_WRITEL_MODE(status, DMA_DMA_LSO_TXQ0_INTERRUPT + tx_qid * 8);
		dev = get_dev_from_lso_txqid(tx_qid);
		cs_dma_tx_complete(tx_qid, dev);
		NI_WRITEL_MODE(0x7, DMA_DMA_LSO_TXQ0_INTENABLE + tx_qid * 8);
	}

	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_SMB_TUNING
static inline int cs_ni_has_work(int instance)
{
	u32 hw_wr_ptr, next_link;
	hw_wr_ptr = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_STA_RX_0 + (instance * 24)));
	hw_wr_ptr &= HW_WR_PTR;
	next_link = (NI_READL_MODE(NI_TOP_NI_CPUXRAM_CPU_CFG_RX_0 + (instance * 24)));
	next_link &= SW_RD_PTR;
	return (next_link != hw_wr_ptr);
}
#endif

/*
 * Handles RX interrupts.
 */
static irqreturn_t cs_ni_rx_interrupt(int irq, void *dev_instance)
{
	u32 status;
	int i;
	mac_info_t *tp;

	//spin_lock_irqsave(&ni_private_data.rx_lock, flags);

#if defined(CONFIG_CS75XX_KTHREAD_RX)

	const int map[] = { 0, 1, 2, 5, 6 };

	i = irq - IRQ_NI_RX_XRAM0;
	status = NI_READL_MODE(NI_TOP_NI_CPUXRAM_RXPKT_0_INTERRUPT_0 + i * 8);

	if (status == 0)
		return IRQ_HANDLED;

	NI_WRITEL_MODE(status, NI_TOP_NI_CPUXRAM_RXPKT_0_INTERRUPT_0 + (i * 8));
	NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + i * 8);

	if (has_many_rx_packets(i, 2)) {
		complete(&(rx_tasks[i].interrupt_trigger));
	} else {
		ni_complete_rx_instance( get_mapping_net_dev(i), i,
					ni_napi_budget, NULL);
		NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + i * 8);
	}
#else
  #ifndef CS752X_NI_NAPI
	for (i = 0; i <= XRAM_INST_MAX; i++) {
		status = NI_READL_MODE(NI_TOP_NI_CPUXRAM_RXPKT_0_INTERRUPT_0 + i * 8);
		if (status != 0) {
			NI_WRITEL_MODE(0,
			       NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + i * 8);
			NI_WRITEL_MODE(status,
			       NI_TOP_NI_CPUXRAM_RXPKT_0_INTERRUPT_0 + i * 8);
			ni_complete_rx_instance(ni_private_data.dev[0], i, 16,
						NULL);
			NI_WRITEL_MODE(1,
			       NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + i * 8);
		}
	}
#else

	i = irq - IRQ_NI_RX_XRAM0;
	status = NI_READL_MODE(NI_TOP_NI_CPUXRAM_RXPKT_0_INTERRUPT_0 + i * 8);
	if (status != 0) {
		NI_WRITEL_MODE(0,
		       NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + i * 8);
		NI_WRITEL_MODE(status,
		       NI_TOP_NI_CPUXRAM_RXPKT_0_INTERRUPT_0 + i * 8);
#ifdef CONFIG_SMB_TUNING
		if (cs_ni_has_work(i)) {
			tp = netdev_priv((struct net_device *) dev_instance);
			napi_schedule(&tp->napi);
		}
		else {
			NI_WRITEL_MODE(1,
				NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + i * 8);
		}

#else
		tp = netdev_priv((struct net_device *) dev_instance);
		napi_schedule(&tp->napi);
#endif
	}

  #endif
#endif
	//spin_unlock_irqrestore(&ni_private_data.rx_lock, flags);
	return IRQ_HANDLED;
}

#ifdef CONFIG_CS75XX_KTHREAD_RX
static void init_rx_task( int instance )
{
	struct rx_task_info *rx_task = rx_tasks + instance;

	rx_task->task = kthread_create(cs_ni_rx_process,
				       instance, "cs752x_ni_rx%d", instance);

	if (rx_task->task) {
		rx_task->instance = instance;
		init_completion(&(rx_task->interrupt_trigger));
		INIT_WORK(&(rx_task->work), rx_enable_interrupt);
		wake_up_process(rx_task->task);
	}
}

static void complete_task( struct work_struct *work)
{
	struct rx_task_info *rx_task =
	    container_of(work, struct rx_task_info, work);

	complete(&(rx_task->interrupt_trigger));
}

static void exit_rx_kthread( int instance )
{
	struct rx_task_info *rx_task = rx_tasks + instance;

	INIT_WORK(&(rx_task->work), complete_task);
	queue_work(system_wq, &(rx_task->work));
	kthread_stop(rx_task->task);
}
#endif

int cs_ni_open(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	int retval = 0;
	struct net_device *tmp_dev;
	mac_info_t *tmp_tp;
	int jj;

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	mac_info_t *virt_tp = netdev_priv(dev);
	if (virt_tp->dev != dev)	/* virtual device now */
		return cs_ni_virt_ni_open(virt_tp->port_id, dev);
#endif

	if (tp->phydev) {
		phy_start(tp->phydev);
	} else {

#if defined(CONFIG_CORTINA_CUSTOM_BOARD)

#elif defined(CONFIG_CORTINA_FPGA)
		printk("%s::FPGA board, GMAC#%d does not have working "
				"phy\n", __func__, tp->port_id);
		netif_err(tp, drv, dev, "Open dev failure\n");
		return -EINVAL;
#elif defined(CONFIG_CORTINA_BHR)
		/* we have switch connect to GMAC#1, which does not
		 * work with Linux Kernel Phy framework. We should
		 * just ignore this. */
		/* we have MoCA connect to GMAC#0 and GMAC#2 in MAC-to-MAC mode.
		 * Just ignore this. */

#elif defined(CONFIG_CORTINA_PON)
		if ((tp->port_id == GE_PORT1) || (tp->port_id == GE_PORT0)) {
			/* we have switch connect to GMAC#1, which does not
			 * work with Linux Kernel Phy framework. We should
			 * just ignore this. */
			/* we have PON connect to GMAC#0 in MAC-to-MAC mode.
			 * Just ignore this. */
		} else {
			printk("%s::PON board, GMAC#%d does not have working "
					"phy\n", __func__, tp->port_id);
			netif_err(tp, drv, dev, "Open dev failure\n");
			return -EINVAL;
		}
#elif defined(CONFIG_CS75XX_GMAC0_TO_EXT_SWITCH)
		if (tp->port_id == GE_PORT0) {
			/* we have switch connect to GMAC#0, which does not
			 * work with Linux Kernel Phy framework. We should
			 * just ignore this. */
		} else {
			printk("%s::Target board, GMAC#%d does not have working "
					"phy\n", __func__, tp->port_id);
			netif_err(tp, drv, dev, "Open dev failure\n");
			return -EINVAL;
		}
#elif defined(CONFIG_CS75XX_GMAC1_TO_EXT_SWITCH)
		if (tp->port_id == GE_PORT1) {
			/* we have switch connect to GMAC#0, which does not
			 * work with Linux Kernel Phy framework. We should
			 * just ignore this. */
		} else {
			printk("%s::Target board, GMAC#%d does not have working "
					"phy\n", __func__, tp->port_id);
			netif_err(tp, drv, dev, "Open dev failure\n");
			return -EINVAL;
		}
#elif defined(CONFIG_CS75XX_GMAC2_TO_EXT_SWITCH)
		if (tp->port_id == GE_PORT2) {
			/* we have switch connect to GMAC#0, which does not
			 * work with Linux Kernel Phy framework. We should
			 * just ignore this. */
		} else {
			printk("%s::Target board, GMAC#%d does not have working "
					"phy\n", __func__, tp->port_id);
			netif_err(tp, drv, dev, "Open dev failure\n");
			return -EINVAL;
		}
#else /* defined(CONFIG_CS75XX_NONE_TO_EXT_SWITCH) */
		printk("%s::Target board, GMAC#%d does not have working "
				"phy\n", __func__, tp->port_id);
		netif_err(tp, drv, dev, "Open dev failure\n");
		return -EINVAL;
#endif

	}

#ifdef CONFIG_CORTINA_CUSTOM_BOARD
	/* Do not turn off port1 as that phy is handled by the bcm6802 kernel
	 * module */
	if ((tp->port_id == GE_PORT0) || (tp->port_id == GE_PORT2))
		netif_carrier_off(dev);
#else
	netif_carrier_off(dev);
#endif

	spin_lock(&active_dev_lock);
	active_dev++;
	spin_unlock(&active_dev_lock);
	if (active_dev == 1) {
		/*napi_enable for NI_ARP, NI_PE*/
		for (jj = GE_PORT_NUM; jj < CS_NI_IRQ_DEV; jj++) {
			tmp_dev = ni_private_data.dev[jj];
#ifdef CONFIG_CS75XX_KTHREAD_RX
			init_rx_task( tmp_dev->irq - IRQ_NI_RX_XRAM0);
#endif
			retval += request_irq(tmp_dev->irq, cs_ni_rx_interrupt,
					IRQF_SHARED, tmp_dev->name, tmp_dev);
			tmp_tp = netdev_priv(tmp_dev);
#ifdef CS752X_NI_NAPI
			napi_enable(&tmp_tp->napi);
#endif
			NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
				((tmp_tp->irq - IRQ_NI_RX_XRAM0) * 8));
		}
		if (retval != 0) {
			printk("%s::Error !", __func__);
			return retval;
		}
	}


	/* this cs_ni_init_port is not called at all? */
	cs_ni_init_port(dev);
	cs_ni_set_short_term_shaper(tp);

	retval = request_irq(tp->irq, cs_ni_rx_interrupt,
				IRQF_SHARED, dev->name, dev);

#ifdef CONFIG_CS75XX_KTHREAD_RX
	init_rx_task( tp->irq - IRQ_NI_RX_XRAM0);
#endif
	if (retval != 0) {
		printk("%s::Error !", __func__);
		return retval;
	}

#ifdef CS752X_NI_NAPI
	napi_enable(&tp->napi);
#endif

	/* Enable XRAM Rx interrupts at this point */
	//cs_ni_enable_xram_intr(tp->port_id, XRAM_DIRECTION_RX);
	NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
		((tp->irq - IRQ_NI_RX_XRAM0) * 8));



	if (ne_irq_register == 0) {
#ifdef CONFIG_CORTINA_FPGA
		retval += request_irq(dev->irq, ni_generic_interrupt,
				IRQF_SHARED, dev->name,
				(struct net_device *)&ni_private_data);
		/* temp hack, for irq coalescing and via irq_wol2 line */
		retval += request_irq(IRQ_WOL2, cs_ni_rx_interrupt,
				IRQF_SHARED, dev->name,
				(struct net_device *)&ni_private_data);
#elif defined(CONFIG_GENERIC_IRQ)
		retval += request_irq(IRQ_NET_ENG, ni_generic_interrupt,
				IRQF_SHARED, "NI generic",
				(struct net_device *)&ni_private_data);
#endif
#ifdef CS752X_NI_TX_COMPLETE_INTERRUPT
		for (jj = 0; jj < 6; jj++)
			retval += request_irq(GOLDENGATE_IRQ_DMA_TX0 + jj,
					cs_ni_tx_interrupt,
					IRQF_SHARED, dev->name,
					(struct net_device *)&ni_private_data);
#endif
		smp_mb();
		ne_irq_register++;
	}
#ifdef CS752X_NI_TX_COMPLETION_TIMER
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	if (tp->dev == dev) {	/* physical device */
		tp->tx_completion_queues = 0;
		setup_timer(&tp->tx_completion_timer, tx_completion_timer_cb,
				(unsigned long)dev);
	}
#else
	tp->tx_completion_queues = 0;
	setup_timer(&tp->tx_completion_timer, tx_completion_timer_cb,
			(unsigned long)dev);
#endif /* CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE */
#endif
	tp->status = 1;
	netif_tx_wake_all_queues(dev);

#if defined(CONFIG_CORTINA_CUSTOM_BOARD)
#warning CUSTOM_BOARD_REVIEW_ME
	if (tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#endif

#if defined(CONFIG_CORTINA_PON)
	/* We turn on the carrier for switch port and PON port after all the
	 * related SW entities have been set up properly */
	if ((tp->port_id == GE_PORT1) || (tp->port_id == GE_PORT0))
		netif_carrier_on(dev);
#elif defined(CONFIG_CORTINA_CUSTOM_BOARD)
	/* Do not turn on port1 as that phy is handled by the bcm6802 kernel
	 * module */
	if ((tp->port_id == GE_PORT0) || (tp->port_id == GE_PORT2))
		netif_carrier_on(dev);
#elif defined(CONFIG_CORTINA_BHR)
	/* We turn on the carrier for switch port and MoCA ports after all the
	 * related SW entities have been set up properly */
	netif_carrier_on(dev);

#elif defined(CONFIG_CS75XX_GMAC0_TO_EXT_SWITCH)
	/* We turn on the carrier for switch port after all the related SW
	 * entities have been set up properly */
	if (tp->port_id == GE_PORT0)
		netif_carrier_on(dev);
#elif defined(CONFIG_CS75XX_GMAC1_TO_EXT_SWITCH)
	/* We turn on the carrier for switch port after all the related SW
	 * entities have been set up properly */
	if (tp->port_id == GE_PORT1)
		netif_carrier_on(dev);
#elif defined(CONFIG_CS75XX_GMAC2_TO_EXT_SWITCH)
	/* We turn on the carrier for switch port after all the related SW
	 * entities have been set up properly */
	if (tp->port_id == GE_PORT2)
		netif_carrier_on(dev);
#endif

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	cs_ni_virt_ni_set_phy_port_active(tp->port_id, true);
#endif

    cs_ni_set_flowctrl(dev);

#if defined(CONFIG_CORTINA_PON)
	if ((tp->port_id == GE_PORT1 || tp->port_id == GE_PORT0) &&
			tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#elif defined(CONFIG_CORTINA_CUSTOM_BOARD)
	if (tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#elif defined(CONFIG_CORTINA_BHR)
	if (tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#elif defined(CONFIG_CS75XX_GMAC0_TO_EXT_SWITCH)
	if (tp->port_id == GE_PORT0 && tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#elif defined(CONFIG_CS75XX_GMAC1_TO_EXT_SWITCH)
	if (tp->port_id == GE_PORT1 && tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#elif defined(CONFIG_CS75XX_GMAC2_TO_EXT_SWITCH)
	if (tp->port_id == GE_PORT2 && tp->ni_driver_state == 0)
		tp->ni_driver_state = 1;
#endif
#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
	if(ni_rx_noncache == 0 && qm_acp_enabled == 0)
                cs_ni_prealloc_free_buffer(dev);
#endif /* ! CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
	return 0;
}

int cs_ni_close(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	struct net_device *tmp_dev;
	mac_info_t *tmp_tp;
	int jj;

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	if (cs_ni_virt_ni_close(dev) <= 0)
		return 0;
#endif

	/* update counters before going down */
	//ni_update_counters(dev);/* add later */
	netif_tx_disable(dev);

	if (tp->phydev)
		phy_stop(tp->phydev);
	spin_lock(&active_dev_lock);
	if (active_dev > 0)
		active_dev--;
	spin_unlock(&active_dev_lock);

#ifdef CS752X_NI_NAPI
	napi_disable(&tp->napi);
#endif
	free_irq(dev->irq, dev);

	cancel_tx_completion_timer(dev);

	NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
		((tp->irq - IRQ_NI_RX_XRAM0) * 8));

#ifdef CONFIG_CS75XX_KTHREAD_RX
	exit_rx_kthread( dev->irq - IRQ_NI_RX_XRAM0);
#endif
	if (active_dev == 0) {
		if (ne_irq_register != 0) {
			ne_irq_register = 0;
#ifdef CONFIG_CORTINA_FPGA
			free_irq(dev->irq,
				 (struct net_device *)&ni_private_data);
			free_irq(IRQ_WOL2,
				 (struct net_device *)&ni_private_data);
#elif defined(CONFIG_GENERIC_IRQ)
			free_irq(IRQ_NET_ENG,
				 (struct net_device *)&ni_private_data);
#endif
		}
#ifndef CONFIG_CORTINA_FPGA
		/* if eth0~2 all down, free PE+ARP irq*/
		for (jj = GE_PORT_NUM; jj < CS_NI_IRQ_DEV; jj++) {
			tmp_dev = ni_private_data.dev[jj];
			free_irq(tmp_dev->irq, tmp_dev);
			tmp_tp = netdev_priv(tmp_dev);

#ifdef CS752X_NI_NAPI
			napi_disable(&tmp_tp->napi);
#endif
			NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
				((tmp_tp->irq - IRQ_NI_RX_XRAM0) * 8));

#ifdef CONFIG_CS75XX_KTHREAD_RX
			exit_rx_kthread( tmp_dev->irq - IRQ_NI_RX_XRAM0);
#endif
		}
#endif
#if 0 /*defined(CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT)*/
		clean_skb_recycle_buffer();
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		clean_skb_recycle_buffer(NULL);
		smp_call_function(clean_skb_recycle_buffer, NULL, 1);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
	}

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	cs_ni_virt_ni_set_phy_port_active(tp->port_id, false);
#endif

	tp->status = 0;

#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_hw_accel_mgr_delete_flow_based_hash_entry();
#endif

	return 0;
}

/**
 * cs_ni_get_stats - Get GoldenGate read/write statistics
 * @dev: The Ethernet Device to get statistics for
 *
 * Get TX/RX statistics for GoldenGate
 */
static struct net_device_stats *cs_ni_get_stats(struct net_device *dev)
{
	mac_info_t *tp = (mac_info_t *) netdev_priv(dev);
	//unsigned long flags;

	return &tp->ifStatics;
}

static u32 set_desc_word3_calc_l4_chksum_ipv4(struct sk_buff *skb,
		u16 seg_size, int frag_id)
{
	u32 word3 = 0;
	struct iphdr *iph = ip_hdr(skb);
	struct udphdr *uh = udp_hdr(skb);
	__wsum csum = 0;

	if (iph == NULL)
		return 0;

	//FIXME: for IPv4, need consider vlan, pppoe, SNAP etc case:
	//segment_size = ip_skb_dst_mtu(skb);
	//word3 = segment_size & 0x00001FFF;

	/* obtain the udphdr location if the given value is NULL */
	if ((iph->protocol == IPPROTO_UDP) &&
			((uh == NULL) || ((u32)iph == (u32)uh)))
		uh = (struct udphdr *)((u32)iph + (iph->ihl << 2));

	/* Was the following conditional statement */
	//if (skb->len > ip_skb_dst_mtu(skb) &&
	//		(iph->protocol == IPPROTO_UDP)) {
	if ((skb->len <= seg_size) && (iph->protocol == IPPROTO_UDP)) {
		/* for example, Transport Stream packet = 188, no checksum */
		/*
		 * There might be case skb_transport_header()
		 * might not be assigned earlier; therefore, we
		 * need to perform the check and get a new uhp.
		 */
		if (uh->check != 0) {
			word3 |= LSO_UDP_CHECKSUM_EN | LSO_IPV4_FRAGMENT_EN;
		}
	} else if ((skb->len > seg_size) && (iph->protocol == IPPROTO_UDP)) {
		/* the above statement was as below: */
		//} else if (skb->len <= ip_skb_dst_mtu(skb) &&
		//		(iph->protocol == IPPROTO_UDP)) {
		word3 |= LSO_IPV4_FRAGMENT_EN;
		/*
		 * There might be case skb_transport_header()
		 * might not be assigned earlier; therefore, we
		 * need to perform the check and get a new uhp.
		 */
		/* SW do UDP checksum if length > MTU */
		if (frag_id == 0) {
			char *pkt_datap;
			int len;
			int page_num = skb_shinfo(skb)->nr_frags;
			/* Retrieve UDP data pointer */
			if (page_num == 0) {
				pkt_datap = skb->data;
				len = uh->len;
				csum = csum_partial(pkt_datap, len, 0);
			} else {
				int i;
				for (i = 0; i < page_num; i++) {
					skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
					pkt_datap = page_address(frag->page.p) +
							frag->page_offset;
					len = frag->size;
					csum = csum_partial(pkt_datap, len, csum);
				}
			}
			uh->check = csum_fold(csum_partial(uh,
						sizeof(struct udphdr), csum));
			smp_wmb();
		}
		if (uh->check == 0) {
			uh->check = CSUM_MANGLED_0;
			printk("Error: uh->check = 0x%X\n", uh->check);
		}
	} else if (iph->protocol == IPPROTO_TCP) {
		/* IPv4 TCP SEGMENT */
		word3 |= LSO_TCP_CHECKSUM_EN | LSO_IPV4_FRAGMENT_EN;
	} else if ((skb->len > seg_size) ||
			(iph->frag_off & htons(IP_MF | IP_OFFSET))) {
		word3 |= LSO_IPV4_FRAGMENT_EN; /* IPv4 IP FRAGMENT */
	}
	return word3;
} /* set_desc_word3_calc_l4_chksum_ipv4 */

static u32 set_desc_word3_calc_l4_chksum_ipv6(struct sk_buff *skb,
		struct net_device *dev, int total_len, int frag_id)
{
	u32 word3 = 0;
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	struct tcphdr *th = tcp_hdr(skb);
	int found_rhdr = 0, ip6_frag = 0;
	struct udphdr *uh = udp_hdr(skb);
	__wsum csum = 0;

	//if (skb->ip_summed != CHECKSUM_PARTIAL)
	//	return word3;

	do {
		if (ipv6h->nexthdr == NEXTHDR_TCP) {
			if ((found_rhdr != 0)) {
				/*
				 * FIXME:SW do tcp checksum, need verify packet
				 * length.
				 */
				//tcp_hdr(skb)->check =
				//	tcp_checksum_complete(skb);
				th->check = csum_tcpudp_magic(th->source,
						th->dest, skb->len - 38,
						IPPROTO_TCP, csum);
			} else {
				word3 |= LSO_TCP_CHECKSUM_EN |
					 LSO_IPV6_FREGMENT_EN;
			}
		} else if (ipv6h->nexthdr == NEXTHDR_UDP) {
			/* FIXME: total_len > seg_size? */
			if ((found_rhdr != 0) || (total_len > dev->mtu)) {
				word3 |= LSO_IPV6_FREGMENT_EN;
				if (frag_id == 0) {
					char *pkt_datap;
					int len;
					int page_num = skb_shinfo(skb)->nr_frags;
					/* Retrieve UDP data pointer */
					if (page_num == 0) {
						pkt_datap = skb->data;
						len = uh->len;
						csum = csum_partial(pkt_datap, len, 0);
					} else {
						int i;
						for (i = 0; i < page_num; i++) {
							skb_frag_t *frag =
								&skb_shinfo(skb)->frags[i];
							pkt_datap =
								page_address(frag->page.p) +
								frag->page_offset;
							len = frag->size;
							csum = csum_partial(pkt_datap,
									len, csum);
						}
					}
					uh->check = csum_fold(csum_partial(uh, sizeof(struct udphdr),
								csum));
					smp_wmb();
				}
			} else {
				if (uh->check != 0)
					word3 |= LSO_UDP_CHECKSUM_EN;
			}
		} else {
			word3 |= LSO_IPV6_FREGMENT_EN;
		}
		if (ipv6h->nexthdr == NEXTHDR_ROUTING) {
			found_rhdr = 1;
			word3 &= (!LSO_IPV6_FREGMENT_EN | !LSO_TCP_CHECKSUM_EN |
					!LSO_UDP_CHECKSUM_EN);
		}
		if (ipv6h->nexthdr == NEXTHDR_FRAGMENT) {
			ip6_frag = 1;
			word3 &= (!LSO_IPV6_FREGMENT_EN | !LSO_TCP_CHECKSUM_EN |
					!LSO_UDP_CHECKSUM_EN);
		}
		/*
		 * FIXME! Wen! there isn't any header moving to the next
		 * IPv6 header?
		 */
	} while (ipv6h->nexthdr == NEXTHDR_NONE); /* do */
	return word3;
} /* set_desc_word3_calc_l4_chksum_ipv6 */

static u32 set_desc_word3_calc_l4_chksum(struct sk_buff *skb,
		struct net_device *dev, int tot_len, int frag_id)
{
	u32 word3;
	u32 network_loc, mac_loc;
	u16 seg_size;
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *th = tcp_hdr(skb);
	u32 mtu;

	skb_reset_mac_header(skb);
	network_loc = (u32)skb_network_header(skb);
	mac_loc = (u32)skb_mac_header(skb);

	/* Only deal with TCP mss now. Let TSO support PMTU. */
	if (iph && iph->protocol == IPPROTO_TCP && skb_is_gso(skb) && th &&
	    skb_shinfo(skb)->gso_size + ((iph->ihl + th->doff) << 2) < dev->mtu)
		mtu = skb_shinfo(skb)->gso_size + ((iph->ihl + th->doff) << 2);
	else
		mtu = dev->mtu;

	if ((network_loc != 0) && ((network_loc - mac_loc) > 14))
		seg_size = mtu + network_loc - mac_loc;
	else
		seg_size = mtu + 14;

#ifdef CONFIG_CS752X_VIRTUAL_NI_CPUTAG
	// FIXME! need to see if this is really a CPU tagged packet
	seg_size += RTL_CPUTAG_LEN;
#endif

	word3 = LSO_SEGMENT_EN | seg_size;
	/* we don't want DMA LSO to modify packet len */

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return word3;

	if (iph == NULL)
		return word3;

	if (iph->version == 4)	/* IPv4 */
		word3 |= set_desc_word3_calc_l4_chksum_ipv4(skb, seg_size,
				frag_id);
	else if (iph->version == 6) /* IPv6 */
		word3 |= set_desc_word3_calc_l4_chksum_ipv6(skb, dev, tot_len,
				frag_id);
	if (word3 & LSO_IPV6_FREGMENT_EN)
		word3 -= mtu & 0x7;

	return word3;
} /* set_desc_word3_calc_l4_chksum */

#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
extern cs_port_id_t cs_qos_get_voq_id(struct sk_buff *skb);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

static int kern_ptr_validate(const void *ptr, unsigned long size)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long min_addr = PAGE_OFFSET;
	unsigned long align_mask = sizeof(void *) - 1;

	if (unlikely(addr < min_addr))
		goto out;

	if (unlikely(addr > (unsigned long)high_memory - size))
		goto out;

	if (unlikely(addr & align_mask))
		goto out;

	if (unlikely(!kern_addr_valid(addr)))
		goto out;

	if (unlikely(!kern_addr_valid(addr + size - 1)))
		goto out;

	return 1;

out:
	return 0;
}

static int cs_ni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	ni_info_t *ni = &ni_private_data;
	struct net_device *xmit_dev = dev;
	mac_info_t *tp = netdev_priv(xmit_dev);
	dma_rptr_t rptr_reg;
	dma_txdesc_t *curr_desc;
	int snd_pages = skb_shinfo(skb)->nr_frags + 1;
	int frag_id = 0, len, total_len, tx_qid = 0, lso_tx_qid;
	struct net_device_stats *isPtr;
	u32 free_desc, rptr;
	dma_addr_t word1;
	u32 word0, word2, word3 = 0, word4, word5 = 0;
	dma_swtxq_t *swtxq;
	ni_header_a_0_t ni_header_a;
	u16 tx_queue = 0;
	struct iphdr *iph;
	char *pkt_datap;
	//unsigned long flags;
	struct netdev_queue *txq;
	struct vm_struct *area;

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	int err_virt_ni = 0;
#endif
#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_kernel_accel_cb_t *cs_cb;
	mac_info_t *in_tp;
	u8 in_tp_status;
#endif

	if (skb == NULL) {
		printk("%s:: skb == NULL\n", __func__);
		return 0;
	}

#ifdef CONFIG_NET_SCH_MULTIQ
	tx_qid = skb_get_queue_mapping(skb);
#endif /* CONFIG_NET_SCH_MULTIQ */
	txq = netdev_get_tx_queue(dev, tx_qid);

	if (smp_processor_id() == 0)
		lso_tx_qid = NI_DMA_LSO_TXQ_IDX;
	else
		lso_tx_qid = NI_DMA_LSO_TXQ_IDX + 1;
	/* DMA_DMA_LSO_DMA_LSO_INTERRUPT_0, interrupt first level
	 * We are using the same tx_qid as for DMA LSO queue */
	swtxq = &ni->swtxq[lso_tx_qid];

	isPtr = (struct net_device_stats *)&tp->ifStatics;

	ni_header_a.bits32 = HDRA_CPU_PKT;

	/* The following should be called with interrupt;
	 * do that when ASIC is back. */
#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	cancel_tx_completion_timer(tp->dev);
	if (check_to_perform_tx_complete(lso_tx_qid))
		cs_dma_tx_complete(lso_tx_qid, tp->dev);
	else
		update_tx_completion_timer(lso_tx_qid, tp->dev);
#else
	cancel_tx_completion_timer(dev);
	if (check_to_perform_tx_complete(lso_tx_qid))
		cs_dma_tx_complete(lso_tx_qid, dev);
	else
		update_tx_completion_timer(lso_tx_qid, dev);
#endif /* CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE */
#endif
	spin_lock(&swtxq->lock);
	if (swtxq->wptr >= swtxq->finished_idx)
		free_desc = swtxq->total_desc_num - swtxq->wptr - 1 +
			swtxq->finished_idx;
	else
		free_desc = swtxq->finished_idx - swtxq->wptr - 1;

	/* try to reserve 1 descriptor in case skb is extended in xmit function */
	if (free_desc <= snd_pages) {
		spin_unlock(&swtxq->lock);
		return NETDEV_TX_BUSY;
	}

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
	err_virt_ni = cs_ni_virt_ni_process_tx_skb(skb, dev, txq);
	if (err_virt_ni < 0) {
		dev_kfree_skb(skb);
		spin_unlock(&swtxq->lock);
		return 0;
	}
	dev = skb->dev;
	tp = netdev_priv(dev);
	txq = netdev_get_tx_queue(dev, tx_qid);
	/* in case we introduce any new send_page */
	snd_pages = skb_shinfo(skb)->nr_frags + 1;
#endif

	//For GREENWAVE to strip vlan tag on eth0 and eth1 port
#if 1
		if (tp->port_id == GE_PORT1 || tp->port_id == GE_PORT0) {
			struct ethhdr *eth;
			eth = (struct ethhdr *)skb->data;
			char tmp[ETH_ALEN * 2];
			if ((eth->h_proto == htons(ETH_P_8021Q)) ||
				(eth->h_proto == htons(0x88a8)) ||
				(eth->h_proto == htons(0x9100)) ||
				(eth->h_proto == htons(0x9200))) {
				memcpy(tmp, skb->data, ETH_ALEN * 2);
				skb_pull(skb, 4);
				memcpy(skb->data, tmp, ETH_ALEN * 2);
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
				skb->dirty_buffer = 1;
#endif
			}
		}
#endif

	total_len = skb->len;
	/* BUG#29162.Workaround. if packet length < 24, DMA LSO can not
	 * send packet out. */
	if (total_len < MIN_DMA_SIZE) {
		dev_kfree_skb(skb);
		spin_unlock(&swtxq->lock);
		return 0;
	}

	/* get the destination VOQ */
	switch (tp->port_id) {
	case GE_PORT0:
		tx_queue = GE_PORT0_VOQ_BASE;
		break;
	case GE_PORT1:
		tx_queue = GE_PORT1_VOQ_BASE;
		break;
	case GE_PORT2:
		tx_queue = GE_PORT2_VOQ_BASE;
		break;
	default:
		printk("%s:%d:Unacceptable port_id %d\n", __func__, __LINE__,
				tp->port_id);
		break;
	}

#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
#ifdef CONFIG_NET_SCH_MULTIQ
    if(cs_qos_preference == CS_QOS_PREF_FLOW)   //BUG#39672: 2. QoS
	tx_queue += tx_qid;
#endif
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#ifdef CONFIG_CS752X_ACCEL_KERNEL
	cs_cb = CS_KERNEL_SKB_CB(skb);
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
        if ((cs_cb == NULL) && (skb->protocol == htons(ETH_P_ARP))&&(cs_qos_preference == CS_QOS_PREF_PORT)) {      
            tx_queue += 7 - CS_QOS_ARP_DEFAULT_PRIORITY;      
            //printk("### tx_queue: %d\n", tx_queue);   
        }
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

	if ((cs_cb != NULL) && (cs_cb->common.tag == CS_CB_TAG)) {
		if (cs_cb->fastnet.word3_valid) {
			/*packet sent from core fastnet*/
			word3 = cs_cb->fastnet.word3;
		} else {
			if (cs_cb->common.ingress_port_id <= GE_PORT2) {
				in_tp = netdev_priv(ni_private_data.dev[cs_cb->common.ingress_port_id]);
			} else {
				/*
		 		 * When not enable WFO Compile flag, 
		 		 * we also need to create hash from PE (for IPSec) ,
		 		 * so use out_dev as in_dev for PE offload case, 
		 		 * instead of using ni_private_data.dev[CS_NI_IRQ_WFO_PE0 / CS_NI_IRQ_WFO_PE1]
		 		 */	
				in_tp = tp; 
			}
			in_tp_status = in_tp->status;
			if ((tp->status == 1) && (in_tp_status == 1)) {
				cs_cb->common.egress_port_id = tp->port_id;
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
//++BUG#39672: 2. QoS
                    if(cs_qos_preference == CS_QOS_PREF_PORT) {
                        tx_queue += cs_qos_get_voq_id(skb);
                    }
//--BUG#39672
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
				if (!(cs_cb->common.module_mask & CS_MOD_MASK_WITH_QOS)) {
					cs_cb->action.voq_pol.d_voq_id = tx_queue;
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
                    cs_cb->action.voq_pol.voq_policy = 1;
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
				}
				cs_cb->common.output_dev = dev;
#ifdef CONFIG_CS75XX_WFO
				if (cs_hw_accel_wfo_handle_tx(tp->port_id, skb) == 1)
#endif
				cs_core_logic_add_connections(skb);

				/* anything else? */
			}
		}
	}else {
		if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_LOCAL_IN)){
			struct iphdr *iph = ip_hdr(skb);
			struct tcphdr * tcphdr = tcp_hdr(skb);
			extern u32 tr143_acl_enable;
			extern u32 tr143_acl_port;
			extern u32 tr143_acl_protocol;
			extern u32 tr143_acl_ip_da;
			if (tr143_acl_enable
				&& (tp->port_id == tr143_acl_port) /*for bhr4*/
				&& iph && (kern_ptr_validate(iph, sizeof(struct iphdr)))
				&& (iph->daddr == tr143_acl_ip_da)
				&& (iph->protocol == tr143_acl_protocol)
				&& tcphdr && (kern_ptr_validate(tcphdr, sizeof(struct tcphdr)))
				&& (cs_localout_check_allow_localout_port(tcphdr->source, tcphdr->dest) == 1)) {
					ni_header_a.bits.fwd_type = 0;
			}
		}
	}
#endif

	while (snd_pages != 0) {
		curr_desc = swtxq->desc_base + swtxq->wptr;
		if (frag_id == 0) {
			pkt_datap = skb->data;
			len = total_len - skb->data_len;
		} else {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[frag_id - 1];
			pkt_datap = page_address(frag->page.p) + frag->page_offset;
			len = frag->size;
			if (len > total_len)
				printk("Fatal Error! Send Frag size %d > "
						"Total Size %d!!\n", len,
						total_len);
		}

		//if (len < 64) len = 64;
		word0 = len | OWN_BIT;
		word2 = 0;
#ifdef CONFIG_CS752X_ACCEL_KERNEL
		if ((cs_cb == NULL) || (cs_cb->fastnet.word3_valid == false))
#endif
			word3 = set_desc_word3_calc_l4_chksum(skb, dev,
					total_len, frag_id);
		/* set_desc_word3_calc_l4_chksum may update checksum value,
		 * so we do dma_map_single after setting word3.
		 */
		area = find_vm_area(pkt_datap);
		if (((u32)pkt_datap >= (u32)area->phys_addr) &&
				((u32)pkt_datap <= ((u32)area->phys_addr + area->size)))
			word1 = skb->head_pa + (skb->data - skb->head);
		else{
#ifdef CONFIG_CS752X_PROC
		if( cs_acp_enable & CS75XX_ACP_ENABLE_NI){
			word1 = virt_to_phys(pkt_datap)|GOLDENGATE_ACP_BASE;
		}
		else
#endif
#ifdef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
			word1 = dma_map_single(NULL, (void *)pkt_datap,
					skb->dirty_buffer ? len : SMP_CACHE_BYTES,
					DMA_TO_DEVICE);
#else /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
			word1 = dma_map_single(NULL, (void *)pkt_datap, len,
					DMA_TO_DEVICE);
#endif /* CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */
		}

		if (snd_pages == 1) {
			word0 |= EOF_BIT; /* EOF */
			if (total_len < 64)
				word3 |= LSO_IP_LENFIX_EN;
			swtxq->tx_skb[swtxq->wptr] = skb;
		} else {
			swtxq->tx_skb[swtxq->wptr] = NULL;
			/* FIXME: if packet length > 1514, there are fragment or
			 * or segment, we need clean this bit */
			word3 &= ~LSO_IP_LENFIX_EN;
		}

		if (frag_id == 0) {
			word0 |= SOF_BIT; /* SOF */
			word2 = (total_len << 16) & 0xFFFF0000;
			/* Enable LSO Debug:
			 * "echo 4 > /proc/driver/cs752x/ne/ni/ni_debug" */
			/* Disable LSO Debug:
			 * "echo 0 > /proc/driver/cs752x/ne/ni/ni_debug" */
#ifdef CONFIG_CS752X_PROC
			if ((total_len > (dev->mtu + 14)) &&
					(cs_ni_debug & DBG_NI_LSO))
				pr_info("DMA LSO enable: MTU = %d, Packet Length %d\n",
						(dev->mtu + 14), total_len);
#endif
		}

		tp->ifStatics.tx_packets++;
		tp->ifStatics.tx_bytes += len;

		ni_header_a.bits.dvoq = tx_queue;
		word4 = ni_header_a.bits32;
		//word5 = 0;	/* it's not used in the standard TX */

		curr_desc->word1.bits32 = (u32)word1;
		curr_desc->word2.bits32 = word2;
		curr_desc->word3.bits32 = word3;
		curr_desc->word4.bits32 = word4;
		/* skip the next line, because it's repeated */
		//curr_desc->word5.bits32 = word5;
		curr_desc->word0.bits32 = word0;

		free_desc--;

#ifdef CONFIG_CS752X_PROC
		if (cs_ni_debug & DBG_NI_DUMP_TX) {
			iph = ip_hdr(skb);
			printk("Word0:0x%08X, Word1:0x%08X, ", word0, word1);
			printk("Word2:0x%08X, Word3:0x%08X, ", word2, word3);
			printk("Word4:0x%08X, Word5:0x%08X, ", word4, word5);
			printk("iph->id = 0x%X\n", iph->id);
			printk("%s:: TX: DMA packet pkt_len %d, skb->data = 0x"
					"%p\n", __func__, len, skb->data);
			ni_dm_byte((u32)skb->data, len);
		}
#endif

		swtxq->wptr = RWPTR_ADVANCE_ONE(swtxq->wptr,
				swtxq->total_desc_num);
		frag_id++;
		snd_pages--;
	}
	smp_wmb();
	NI_WRITEL_MODE(swtxq->wptr, swtxq->wptr_reg);

#ifdef CONFIG_CS752X_PROC
	if (cs_ni_debug & DBG_NI_DUMP_TX) {
		rptr_reg.bits32 = NI_READL_MODE(swtxq->rptr_reg);
		rptr = rptr_reg.bits.rptr;
		printk("%s::tx reg wptr 0x%08x, rptr 0x%08x\n", __func__,
				swtxq->wptr, rptr_reg.bits32);
	}
#endif
	/* need check every swtxq->wptr_reg */
	//SET_WPTR(swtxq->wptr_reg, swtxq->wptr);
	/* according to include/linux/netdevice.h, trans_start is expensive for
	 * high speed device on SMP, please use netdev_queue->trans_start */
	//dev->trans_start = jiffies;
	txq_trans_update(txq);
	spin_unlock(&swtxq->lock);
	return 0;
} /* cs_ni_start_xmit */

#ifdef CS752X_MANAGEMENT_MODE
static int cs_mfe_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	ni_info_t *ni;
	unsigned long flags;
	int tx_qid = 0;
	u16 tx_queue = 0;
	dma_swtxq_t *swtxq;

	ni = &ni_private_data;
	if (!skb) {
		printk("%s: Fatal Error SKB \n", __func__);
		return 0;
	}
#ifdef CONFIG_NET_SCH_MULTIQ
	tx_qid = skb_get_queue_mapping(skb);
#endif

	if (tp != NULL) {
		tp->ifStatics.tx_packets++;
		tp->ifStatics.tx_bytes += skb->len;
		skb->dev = tp->dev;
	}

	/* How to find out the destination queue */
	swtxq = &ni->swtxq[tx_qid]; /* how to know which tx_qid ? */

	spin_lock_irqsave(&tp->lock, flags);

	if (tp->port_id == GE_PORT0)
		tx_queue = GE_PORT0_VOQ_BASE;
	else if (tp->port_id == GE_PORT1)
		tx_queue = GE_PORT1_VOQ_BASE;
	else if (tp->port_id == GE_PORT2)
		tx_queue = GE_PORT2_VOQ_BASE;

#ifdef CONFIG_NET_SCH_MULTIQ
	tx_queue += tx_qid;
#endif

	if (ni_cpu_tx_packet(skb, tx_queue) != 0) {
		spin_unlock_irqrestore(&tp->lock, flags);
		printk("%s: Error \n", __func__);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&tp->lock, flags);
	dev->trans_start = jiffies;
	return 0;
}
#endif /* CS752X_MANAGEMENT_MODE */

static void cs_ni_tx_timeout(struct net_device *dev)
{
	mac_info_t *tp;
	//mac_info_t *tp = netdev_priv(dev);
	int i;
	struct net_device *tmp_dev;

	printk("%s::\n", __func__);

	for (i=0; i<GE_PORT_NUM; i++) {
		tmp_dev = ni_private_data.dev[i];

		if (tmp_dev == NULL)
			continue;

		tp = netdev_priv(tmp_dev);
		netif_tx_stop_all_queues(tmp_dev);
	}

	cs_ni_disable_interrupts();
	//cs_ni_hw_reset(tp);
	/* FIXME!! not finish! what else needs to be done here? besides
	 * dumping out more precise error log? */

	printk("%s::schedule reset task\n", __func__);
	schedule_work(&ni_private_data.reset_task);
}

static int cs_ni_change_mtu(struct net_device *dev, int new_mtu)
{
	mac_info_t *tp = netdev_priv(dev);

	if ((new_mtu < MINIMUM_ETHERNET_FRAME_SIZE) ||
			(new_mtu > (MAX_PKT_LEN - (ENET_HEADER_SIZE +
						       ETHERNET_FCS_SIZE +
						       VLAN_HLEN)))) {
		printk("Invalid MTU setting, MTU= %d > Maximum mtu %d\n",
			 new_mtu, (MAX_PKT_LEN - (ENET_HEADER_SIZE +
						       ETHERNET_FCS_SIZE +
						       VLAN_HLEN)));
		return -EINVAL;
	}

	printk("%s changing MTU from %d to %d\n", dev->name, dev->mtu, new_mtu);

	dev->mtu = new_mtu;
	if (!netif_running(dev))
		goto out;

	if (tp->phydev != NULL) {
		netif_tx_disable(dev);
		phy_stop(tp->phydev);
		phy_start(tp->phydev);
		netif_carrier_off(dev);
		netif_tx_wake_all_queues(dev);
	}

out:
	return 0;
}

#define HDRA_CPU_PKT	0xc30001
static int cs_ni_set_mac_address(struct net_device *dev, void *p)
{
	mac_info_t *tp = netdev_priv(dev);
	fe_an_bng_mac_entry_t abm_entry;
	struct sockaddr *addr = p;
	NI_TOP_NI_ETH_MAC_CONFIG2_0_t mac_config2, mac_config2_mask;
	NI_TOP_NI_MAC_ADDR1_t mac_addr1, mac_addr1_mask;
	__u32 low_mac;
	int ret;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	set_mac_swap_order(abm_entry.mac, (unsigned char*)dev->dev_addr, dev->addr_len);
	//memcpy(abm_entry.mac, dev->dev_addr, dev->addr_len);
	abm_entry.sa_da = 0; /* 0: DA, 1: SA */
	abm_entry.pspid = tp->port_id;
	abm_entry.pspid_mask = 0;
	abm_entry.valid = 1;
	if (tp->an_bng_mac_idx == 0xffff)
		ret = cs_fe_table_add_entry(FE_TABLE_AN_BNG_MAC, &abm_entry,
				&tp->an_bng_mac_idx);
	else
		ret = cs_fe_table_set_entry(FE_TABLE_AN_BNG_MAC,
				tp->an_bng_mac_idx, &abm_entry);
	if (ret != 0) {
		printk("%s:unable to set up MAC to FE for port#%d\n", __func__,
				tp->port_id);
		// FIXME! any debug method?
	} else {
		/* Debug message.. can be removed later */
		printk("%s:setting up MAC address for Port#%d ", __func__,
				tp->port_id);
		printk("using FE AN_BNG_MAC idx %d.\n", tp->an_bng_mac_idx);
	}

	/* set for PE#0 */
	abm_entry.pspid = ENCRYPTION_PORT;
	if (tp->an_bng_mac_idx_pe0 == 0xffff)
		ret = cs_fe_table_add_entry(FE_TABLE_AN_BNG_MAC, &abm_entry,
				&tp->an_bng_mac_idx_pe0);
	else
		ret = cs_fe_table_set_entry(FE_TABLE_AN_BNG_MAC,
				tp->an_bng_mac_idx_pe0, &abm_entry);
	if (ret != 0) {
		printk("%s:unable to set up MAC to FE for PE#0\n", __func__);
		// FIXME! any debug method?
	} else {
		/* Debug message.. can be removed later */
		printk("%s:setting up MAC address for PE#0 ", __func__);
		printk("using FE AN_BNG_MAC idx %d.\n", tp->an_bng_mac_idx_pe0);
	}

	/* set for PE#1 */
	abm_entry.pspid = ENCAPSULATION_PORT;
	if (tp->an_bng_mac_idx_pe1 == 0xffff)
		ret = cs_fe_table_add_entry(FE_TABLE_AN_BNG_MAC, &abm_entry,
				&tp->an_bng_mac_idx_pe1);
	else
		ret = cs_fe_table_set_entry(FE_TABLE_AN_BNG_MAC,
				tp->an_bng_mac_idx_pe1, &abm_entry);
	if (ret != 0) {
		printk("%s:unable to set up MAC to FE for PE#1\n", __func__);
		// FIXME! any debug method?
	} else {
		/* Debug message.. can be removed later */
		printk("%s:setting up MAC address for PE#1 ", __func__);
		printk("using FE AN_BNG_MAC idx %d.\n", tp->an_bng_mac_idx_pe1);
	}

	printk("NI Setting MAC address for %s, %02x:%02x:%02x:%02x:%02x:%02x\n",
			dev->name, dev->dev_addr[0], dev->dev_addr[1],
			dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4],
			dev->dev_addr[5]);

	/* used as SA while sending pause frames. Also used to detect WOL magic
	 * packets. */
	low_mac = dev->dev_addr[0] | (dev->dev_addr[1] << 8) |
		(dev->dev_addr[2] << 16) | (dev->dev_addr[3] << 24);
	NI_WRITEL_MODE(low_mac, NI_TOP_NI_MAC_ADDR0);

	mac_addr1.wrd = 0;
	mac_addr1_mask.wrd = 0;
	mac_addr1.bf.mac_addr1 = dev->dev_addr[5];
	mac_addr1_mask.bf.mac_addr1 = 0xff;
	write_reg(mac_addr1.wrd, mac_addr1_mask.wrd, NI_TOP_NI_MAC_ADDR1);

	mac_config2.wrd = 0;
	mac_config2_mask.wrd = 0;
	mac_config2.bf.mac_addr6 = dev->dev_addr[6];
	mac_config2_mask.bf.mac_addr6 = dev->dev_addr[6];
	if (tp->port_id == GE_PORT0)
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0);
	if (tp->port_id == GE_PORT1)
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0 + 12);
	if (tp->port_id == GE_PORT2)
		write_reg(mac_config2.wrd, mac_config2_mask.wrd,
				NI_TOP_NI_ETH_MAC_CONFIG2_0 + 24);

	return 0;
}

static int cs_ni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCDNEPRIVATE:
		return cs_ne_ioctl(dev, ifr, cmd);
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
	default:
		return -EOPNOTSUPP;
	}
}

void cs_ni_set_rx_mode(struct net_device *dev)
{
	u32 mc_filter[2];
	unsigned long flags;
	mac_info_t *tp = netdev_priv(dev);
	NI_TOP_NI_CPUXRAM_CFG_t rx_mode, rx_mode_mask;
	NI_TOP_NI_MISC_CONFIG_t misc_cfg, misc_cfg_mask;

	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		netif_notice(tp, link, dev, "Promiscuous mode enabled\n");
		rx_mode.bf.xram_mgmt_promisc_mode = ACCEPTALLPACKET;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if (dev->flags & IFF_ALLMULTI) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode.bf.xram_mgmt_promisc_mode = ACCEPTMULTICAST;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct netdev_hw_addr *ha;
		rx_mode.bf.xram_mgmt_promisc_mode = ACCEPTBROADCAST;
		mc_filter[1] = mc_filter[0] = 0;
		netdev_for_each_mc_addr(ha, dev) {
			int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
			rx_mode.bf.xram_mgmt_promisc_mode = ACCEPTMULTICAST;
		}
	}


	rx_mode.wrd = 0;
	rx_mode_mask.wrd = 0;
	misc_cfg.wrd = 0;
	misc_cfg_mask.wrd = 0;
	/* Valid only when GE port is directly connected to the XRAM
	 * (i.e. when NI_ETH_MGMT_PT_CONFIG.port_to_cpu[1:0] != 2'b11).
	 */
	rx_mode_mask.bf.xram_mgmt_promisc_mode = 0x3;
	/* Does not check the fwd_type to accept a packetfor multicast */
	misc_cfg.bf.mc_accept_all = 1;
	misc_cfg_mask.bf.mc_accept_all = 0x1;

	spin_lock_irqsave(&tp->lock, flags);
	write_reg(rx_mode.wrd, rx_mode_mask.wrd, NI_TOP_NI_CPUXRAM_CFG);
	write_reg(misc_cfg.wrd, misc_cfg_mask.wrd, NI_TOP_NI_MISC_CONFIG);
	spin_unlock_irqrestore(&tp->lock, flags);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void cs_ni_poll_controller(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);

	disable_irq(tp->irq);
	ni_generic_interrupt(tp->irq, dev);
#ifdef CONFIG_CORTINA_FPGA
	cs_ni_rx_interrupt(IRQ_WOL2, dev);
#else
	cs_ni_rx_interrupt(tp->irq, dev);
#endif
	enable_irq(tp->irq);
}
#endif	/* CONFIG_NET_POLL_CONTROLLER */

static void cs_ni_change_rx_flags(struct net_device *dev, int flags)
{
	mac_info_t *tp = netdev_priv(dev);
	int enbl;

	switch (flags) {
	case IFF_PROMISC:
		enbl = (dev->flags & IFF_PROMISC) ? 1 : 0;

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
		if (dev != tp->dev) {
			/* it is a virtual interface */
			/* change to tp of physical device */
			tp = netdev_priv(tp->dev);

			/* we always enable promiscuous mode for 
			   virtual inteface */
			if (enbl != 1)
				return;
		} else {
			/* it is a physical interface */
			/* we always enable promiscuous mode for 
			   virtual inteface */
#ifdef CONFIG_CS752X_VIRTUAL_ETH0
			if (tp->port_id == 0 && enbl != 1)
				return;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH1
			if (tp->port_id == 1 && enbl != 1)
				return;
#endif
#ifdef CONFIG_CS752X_VIRTUAL_ETH2
			if (tp->port_id == 2 && enbl != 1)
				return;
#endif
		}
		
#endif /* CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE */
		
		cs_core_set_promiscuous_port(tp->port_id, enbl);
	}
}

static int cs_set_rx_csum(struct net_device *dev, u64 data)
{
	mac_info_t *tp = netdev_priv(dev);
	unsigned long flags;
	u32 val = 0;
	
	spin_lock_irqsave(&tp->lock, flags);

#define L4RxChkSum	(1 << 8)
	if (data) {
		val = NI_READL_MODE(FETOP_FE_PRSR_CFG_0) & ~L4RxChkSum;
		tp->rx_checksum = CS_ENABLE;
	} else {
		val = NI_READL_MODE(FETOP_FE_PRSR_CFG_0) | L4RxChkSum;
		tp->rx_checksum = CS_DISABLE;
	}
	
	NI_WRITEL_MODE(val, FETOP_FE_PRSR_CFG_0);

	spin_unlock_irqrestore(&tp->lock, flags);
	return 0;
}

static int cs_ni_set_features(struct net_device *netdev, netdev_features_t features)
{
	if ((netdev->features ^ features) & NETIF_F_RXCSUM)
		cs_set_rx_csum(netdev, features & NETIF_F_RXCSUM);
	return 0;
}

static const struct net_device_ops ni_netdev_ops = {
	.ndo_open = cs_ni_open,
	.ndo_stop = cs_ni_close,
	.ndo_get_stats = cs_ni_get_stats,
#ifdef CS752x_DMA_LSO_MODE
	.ndo_start_xmit = cs_ni_start_xmit,
#endif /* CS752x_DMA_LSO_MODE */
#ifdef CS752X_MANAGEMENT_MODE
	.ndo_start_xmit = cs_mfe_start_xmit,
#endif /* CS752X_MANAGEMENT_MODE */
	.ndo_tx_timeout = cs_ni_tx_timeout,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_change_mtu = cs_ni_change_mtu,
	.ndo_set_mac_address = cs_ni_set_mac_address,
	.ndo_do_ioctl = cs_ni_ioctl,
	.ndo_set_rx_mode = cs_ni_set_rx_mode,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = cs_ni_poll_controller,
#endif
#ifdef CONFIG_NET_SCH_MULTIQ
	.ndo_select_queue = ni_select_queue,
#endif
	.ndo_change_rx_flags = cs_ni_change_rx_flags,
	.ndo_set_features = cs_ni_set_features,
};

static int cs_mdiobus_read(struct mii_bus *mii_bus, int phy_addr, int reg_addr)
{
	int ret;
	//unsigned long flags;

	spin_lock(&mdio_lock);
	ret = ni_mdio_read(phy_addr, reg_addr);
	spin_unlock(&mdio_lock);
	return ret;
}

static int cs_mdiobus_write(struct mii_bus *mii_bus, int phy_addr,
		int reg_addr, u16 val)
{
	int ret = 0;
	//unsigned long flags;

	spin_lock(&mdio_lock);
	ret = ni_mdio_write(phy_addr, reg_addr, val);
	spin_unlock(&mdio_lock);
	return ret;
}

static int cs_mdio_reset(struct mii_bus *mii_bus)
{
	return 0;
}

static int cs_mdio_init(ni_info_t *ni, mac_info_t * tp)
{
	int i, err;

	if (tp->existed & CS_MDIOBUS_INITED)
		return 0;

	tp->mdio_bus = mdiobus_alloc();
	if (tp->mdio_bus == NULL) {
		err = -ENOMEM;
		goto err_out;
	}

	tp->mdio_bus->name = CS75XX_MDIOBUS_NAME;
	if (tp->port_id == GE_PORT0) {
		snprintf(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%s", "0");
	}
	if (tp->port_id == GE_PORT1) {
		snprintf(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%s", "1");
	}
	if (tp->port_id == GE_PORT2) {
		snprintf(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%s", "2");
	}

	tp->mdio_bus->priv = tp;
	//tp->mdio_bus->parent = &tp->dev->dev;
	tp->mdio_bus->read = &cs_mdiobus_read;
	tp->mdio_bus->write = &cs_mdiobus_write;
	tp->mdio_bus->reset = &cs_mdio_reset;
	tp->mdio_bus->phy_mask = ~(1 << tp->phy_addr);

	for (i = 0; i < PHY_MAX_ADDR; i++)
		tp->mdio_bus->irq[i] = PHY_POLL;

	/*
	 * The bus registration will look for all the PHYs on the mdio bus.
	 * Unfortunately, it does not ensure the PHY is powered up before
	 * accessing the PHY ID registers.
	 */
	err = mdiobus_register(tp->mdio_bus);
	if (err)
		goto err_out_free_mdio_irq;

	tp->existed |= CS_MDIOBUS_INITED;

	return 0;

err_out_free_mdio_irq:
	cs_free(tp->mdio_bus->irq);
err_out_free_mdio_bus:
	mdiobus_free(tp->mdio_bus);
err_out:
	return err;
}

static unsigned long ascii2hex(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	else
		return (0xffffffff);
}

static unsigned long string2hex(char *str_p)
{
	unsigned long i, result = 0;

	if (*str_p == '0' && toupper(*(str_p + 1)) == 'X')
		str_p += 2;

	while ((i = ascii2hex(*str_p)) != 0xffffffff) {
		result = (result) * 16 + i;
		str_p++;
	}

	while (*str_p == ' ' || *str_p == '.' || *str_p == ':')
		str_p++;

	return result;
}

static void ni_mac_parse(void)
{
	int i;
	char *ptr_0;

	ptr_0 = strstr(saved_command_line, "ethaddr0");
	if (ptr_0 == NULL) {
		printk("%s: No GE MAC found in U-boot !!\n", __func__);
		return;
	}
	ptr_0 += strlen("ethaddr0") + 1;
	for (i = 0; i < 6; i++) {
		while (*ptr_0 == ' ' || *ptr_0 == '.' || *ptr_0 == ':')
			ptr_0++;
		eth_mac[0][i] = (u8)string2hex(ptr_0);
		ptr_0 += 2;
	}

	printk("MAC0: %02x:%02x:", eth_mac[0][0], eth_mac[0][1]);
	printk("%02x:%02x:", eth_mac[0][2], eth_mac[0][3]);
	printk("%02x:%02x\n", eth_mac[0][4], eth_mac[0][5]);

	ptr_0 = strstr(saved_command_line, "ethaddr1");
	ptr_0 += strlen("ethaddr1") + 1;
	for (i = 0; i < 6; i++) {
		while (*ptr_0 == ' ' || *ptr_0 == '.' || *ptr_0 == ':')
			ptr_0++;

		eth_mac[1][i] = (u8)string2hex(ptr_0);
		ptr_0 += 2;
	}
	printk("MAC1: %02x:%02x:", eth_mac[1][0], eth_mac[1][1]);
	printk("%02x:%02x:", eth_mac[1][2], eth_mac[1][3]);
	printk("%02x:%02x\n", eth_mac[1][4], eth_mac[1][5]);

	ptr_0 = strstr(saved_command_line, "ethaddr2");
	ptr_0 += strlen("ethaddr2") + 1;
	for (i = 0; i < 6; i++) {
		while (*ptr_0 == ' ' || *ptr_0 == '.' || *ptr_0 == ':')
			ptr_0++;

		eth_mac[2][i] = (u8)string2hex(ptr_0);
		ptr_0 += 2;
	}
	printk("MAC2: %02x:%02x:", eth_mac[2][0], eth_mac[2][1]);
	printk("%02x:%02x:", eth_mac[2][2], eth_mac[2][3]);
	printk("%02x:%02x\n", eth_mac[2][4], eth_mac[2][5]);
}

static u64 ni_mib_access(u32 reg, u8 read_write, u8 op_code,
		u8 port_id, u8 counter_id)
{
	NI_TOP_NI_RXMIB_ACCESS_t mib_access, mib_access_mask;
	u8 access_executed;
	u64 val_bottom, val_top;

	mib_access.wrd = 0;
	mib_access_mask.wrd = 0;
	mib_access.bf.rbw = read_write;
	mib_access.bf.op_code = op_code;
	mib_access.bf.port_id = port_id;
	mib_access.bf.counter_id = counter_id;
	mib_access.bf.access = 1;
	mib_access_mask.bf.rbw = 0x1;
	mib_access_mask.bf.op_code = 0x3;
	mib_access_mask.bf.port_id = 0x7;
	mib_access_mask.bf.counter_id = 0x1F;
	mib_access_mask.bf.access = 0x1;

	write_reg(mib_access.wrd, mib_access_mask.wrd, reg);

	do {
		access_executed = (NI_READL_MODE(reg) & 0x80000000);
		udelay(10);
	} while (access_executed == 1);

	/* FIXME: need verify. which one is the top. */
	val_bottom = NI_READL_MODE(reg + 8);
	val_top = NI_READL_MODE(reg + 4);
	val_bottom |= val_top << 32;

	return val_bottom;
}

void cs_ni_clear_stats(u8 port_id)
{
        u8 i;

        /* clear RX counters */
        for (i = RXUCPKTCNT; i <= RXBYTECOUNT_HI; i++) {
                ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ, READ_ALL_CLEAR, port_id, i);
        }

        /* clear TX counters */
        for (i = TXUCPKTCNT; i <= TXBYTECOUNT_HI; i++) {
                ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ, READ_ALL_CLEAR, port_id, i);
        }
}

struct cs_ethtool_stats *cs_ni_update_stats(mac_info_t *tp)
{
	struct cs_ethtool_stats *stats = &tp->stats;
	unsigned long flags;
	u64 val = 0;

	spin_lock_irqsave(&tp->stats_lock, flags);

	/* RX mib */
	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXUCPKTCNT);
	tp->stats.rxucpktcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXMCFRMCNT);
	tp->stats.rxmcfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXBCFRMCNT);
	tp->stats.rxbcfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXOAMFRMCNT);
	tp->stats.rxoamfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXJUMBOFRMCNT);
	tp->stats.rxjumbofrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXPAUSEFRMCNT);
	tp->stats.rxpausefrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXUNKNOWNOCFRMCNT);
	tp->stats.rxunknownocfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXCRCERRFRMCNT);
	tp->stats.rxcrcerrfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXUNDERSIZEFRMCNT);
	tp->stats.rxundersizefrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXRUNTFRMCNT);
	tp->stats.rxruntfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXOVSIZEFRMCNT);
	tp->stats.rxovsizefrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXJABBERFRMCNT);
	tp->stats.rxjabberfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXINVALIDFRMCNT);
	tp->stats.rxinvalidfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM64OCT);
	tp->stats.rxstatsfrm64oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM65TO127OCT);
	tp->stats.rxstatsfrm65to127oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM128TO255OCT);
	tp->stats.rxstatsfrm128to255oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM256TO511OCT);
	tp->stats.rxstatsfrm256to511oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM512TO1023OCT);
	tp->stats.rxstatsfrm512to1023oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM1024TO1518OCT);
	tp->stats.rxstatsfrm1024to1518oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM1519TO2100OCT);
	tp->stats.rxstatsfrm1519to2100oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM2101TO9200OCT);
	tp->stats.rxstatsfrm2101to9200oct = val;

	val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, RXSTATSFRM9201TOMAXOCT);
	tp->stats.rxstatsfrm9201tomaxoct = val;

        val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
                        PLAIN_READ, tp->port_id, RXBYTECOUNT_LO);
        tp->stats.rxbytecount_lo = val & 0xffffffff;

        val = ni_mib_access(NI_TOP_NI_RXMIB_ACCESS, CS_READ,
                        PLAIN_READ, tp->port_id, RXBYTECOUNT_HI);
        tp->stats.rxbytecount_hi = (val & 0xffffffff00000000) >> 32;

	/* TX mib */
	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXUCPKTCNT);
	tp->stats.txucpktcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXMCFRMCNT);
	tp->stats.txmcfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXBCFRMCNT);
	tp->stats.txbcfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXOAMFRMCNT);
	tp->stats.txoamfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXJUMBOFRMCNT);
	tp->stats.txjumbofrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXPAUSEFRMCNT);
	tp->stats.txpausefrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXCRCERRFRMCNT);
	tp->stats.txcrcerrfrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXOVSIZEFRMCNT);
	tp->stats.txovsizefrmcnt = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSINGLECOLFRM);
	tp->stats.txsinglecolfrm = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXMULTICOLFRM);
	tp->stats.txmulticolfrm = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXLATECOLFRM);
	tp->stats.txlatecolfrm = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXEXESSCOLFRM);
	tp->stats.txexesscolfrm = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM64OCT);
	tp->stats.txstatsfrm64oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM65TO127OCT);
	tp->stats.txstatsfrm65to127oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM128TO255OCT);
	tp->stats.txstatsfrm128to255oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM256TO511OCT);
	tp->stats.txstatsfrm256to511oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM512TO1023OCT);
	tp->stats.txstatsfrm512to1023oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM1024TO1518OCT);
	tp->stats.txstatsfrm1024to1518oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM1519TO2100OCT);
	tp->stats.txstatsfrm1519to2100oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM2101TO9200OCT);
	tp->stats.txstatsfrm2101to9200oct = val;

	val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
			PLAIN_READ, tp->port_id, TXSTATSFRM9201TOMAXOCT);
	tp->stats.txstatsfrm9201tomaxoct = val;


        val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
                        PLAIN_READ, tp->port_id, TXBYTECOUNT_LO);
        tp->stats.txbytecount_lo = val & 0xffffffff;

        val = ni_mib_access(NI_TOP_NI_TXMIB_ACCESS, CS_READ,
                        PLAIN_READ, tp->port_id, TXBYTECOUNT_HI);
        tp->stats.txbytecount_hi = (val & 0xffffffff00000000) >> 32;

	spin_unlock_irqrestore(&tp->stats_lock, flags);
	return stats;
}

static void cs_ni_init_virtual_instance(int dev_idx, int irq, char * name)
{
	struct net_device *dev;
	mac_info_t *tp;

#ifdef CONFIG_NET_SCH_MULTIQ
	dev = alloc_etherdev_mq(sizeof(*tp), QUEUE_PER_INSTANCE);
#else
	dev = alloc_etherdev(sizeof(*tp));
#endif

	ni_private_data.dev[dev_idx] = dev;
	dev->irq = irq;
	tp = netdev_priv(dev);
	tp->dev = dev;
	tp->irq = dev->irq;
#ifdef CS752X_NI_NAPI
	tp->napi.dev = dev;
#endif
	tp->port_id = dev_idx;
	snprintf(dev->name, IFNAMSIZ, name);
}

#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
static void cs_ni_prealloc_free_buffer(struct net_device *dev)
{
#ifndef  NI_RECYCLE_SKB_PER_CPU
	int i;
	struct sk_buff *skb;

	for(i = 0; i < ni_min_skb_queue_len; i++)
	{
		if(ni_min_skb_queue_len < skb_queue_len(&cs_ni_skb_recycle_cpu0_head))
                        break;
		skb = netdev_alloc_skb(dev, SKB_PKT_LEN + 0x100);
		if (!skb) {
                        printk(KERN_WARNING "%s: Could only allocate  "
                                        "receive skb(s).\n", dev->name);
                        break;
                }
		dma_map_single(NULL, skb->data, SKB_PKT_LEN, DMA_FROM_DEVICE);
		skb_queue_tail(&cs_ni_skb_recycle_cpu0_head, skb);
	}
	DBG(printk("skb_queue_len=%d\n", skb_queue_len(&cs_ni_skb_recycle_cpu0_head)));
#endif
}
#endif /* ! CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

/* 3.4.11 Change for register as platform device */
static int cs_ni_init_module_probe(struct platform_device *pdev)
{
	int i, err = 0;
	mac_info_t *tp;
	struct net_device *dev;
	const struct port_cfg_info *cfg;
	ni_info_t *ni = &ni_private_data;
	u32 phy_vendor;
	FETOP_FE_PRSR_CFG_0_t fe_prsr_cfg0, fe_prsr_cfg0_mask;
	bool dev_has_phy;

	/*printk(KERN_INFO NI_DRIVER_NAME " built at %s %s\n", __DATE__,
			__TIME__);*/

	cs_kmem_cache_create();

	/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
#ifndef CONFIG_CS75XX_WFO
        if (ni_rx_noncache)
        {
                linux_free_buf_size_tbl[0] = 1 << 8;
                linux_free_buf_size_tbl[1] = 1 << 8;
                linux_free_buf_size_tbl[2] = 1 << 6;
        }
#endif

#ifndef CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT
	/* debug_Aaron on 2013/03/11 for BUG#37961, when use RX non-cacheable buffer do not keep min skb queue */
        if (ni_rx_noncache)
		ni_min_skb_queue_len = 0;
#endif /* ! CONFIG_CS75XX_NI_EXPERIMENTAL_SW_CACHE_MANAGEMENT */

	ni_mac_parse();
	//cs_ne_system_reset();
	spin_lock_init(&mdio_lock);
	spin_lock_init(&sw_qm_cnt_lock);
	spin_lock_init(&active_dev_lock);

	skb_queue_head_init(&cs_ni_skb_recycle_cpu0_head);
#ifdef  NI_RECYCLE_SKB_PER_CPU
	skb_queue_head_init(&cs_ni_skb_recycle_cpu1_head);
#endif 

	cs_ne_init_cfg();

	cs_fe_init();

#ifdef CS752X_LINUX_MODE
	cs_qm_init();
	cs752x_sch_init();
#endif
	cs75xx_tm_init();
	cs_core_logic_init();
	cs_diag_voq_cntr_init();

	/* create virtual device for PE and ARP*/
	cs_ni_init_virtual_instance(CS_NI_IRQ_PE, IRQ_NI_RX_XRAM6, "NI_PE");
	cs_ni_init_virtual_instance(CS_NI_IRQ_ARP, IRQ_NI_RX_XRAM7, "NI_ARP");
#ifdef CONFIG_CS75XX_WFO
	cs_ni_init_virtual_instance(CS_NI_IRQ_WFO_PE0, IRQ_NI_RX_XRAM3, "NI_WFO_PE0");
	cs_ni_init_virtual_instance(CS_NI_IRQ_WFO_PE1,  IRQ_NI_RX_XRAM4, "NI_WFO_PE1");
	cs_ni_init_virtual_instance(CS_NI_IRQ_WFO,  IRQ_NI_RX_XRAM5, "NI_WFO");
#endif
	/* Hold all macs in Normal */
	for (i = 0; i < GE_PORT_NUM; i++)
		NI_WRITEL_MODE(0x00100800, NI_TOP_NI_ETH_MAC_CONFIG0_0 + (i * 12));

	for (i = 0; i < GE_PORT_NUM; i++) {
		cfg = &port_cfg_infos[i];
		//tp->dev = NULL;
		//if (tp->existed != CS_MAC_EXISTED_FLAG) continue;
#ifdef CONFIG_NET_SCH_MULTIQ
		dev = alloc_etherdev_mq(sizeof(*tp), QUEUE_PER_INSTANCE);
#else
		dev = alloc_etherdev(sizeof(*tp));
#endif
		if (dev == NULL) {
			printk(KERN_ERR "Unable to alloc new ethernet device #"
					"%d .\n", i);
			return -ENOMEM;
		}
		/* 3.4.11 Change for register as platform device */
		SET_NETDEV_DEV(dev, &pdev->dev);
		ni_private_data.dev[i] = dev;
		tp = netdev_priv(dev);
		tp->dev = dev;
		tp->link_config.advertising =
			(ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
		 	ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
		 	ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full |
		 	ADVERTISED_Autoneg | ADVERTISED_MII);
		tp->link_config.link = 0; /* LINK_DOWN */
		tp->link_config.autoneg = cfg->auto_nego;
		tp->link_config.speed = cfg->speed;
		tp->link_config.duplex = cfg->full_duplex;
		tp->link_config.flowctrl = cfg->flowctrl;
		tp->link_config.lcl_adv = 0;    // BUG#37140
		tp->link_config.rmt_adv = 0;    // BUG#37140
		tp->phy_mode = cfg->phy_mode;
		tp->port_id = cfg->port_id;
		tp->phy_addr = cfg->phy_addr;
		tp->irq = cfg->irq;
		tp->mac_addr = (u32 *)cfg->mac_addr;
		tp->rmii_clk_src = cfg->rmii_clk_src;
		tp->rx_checksum = CS_ENABLE;
		//tp->rx_checksum = CS_DISABLE;
		tp->an_bng_mac_idx = 0xffff;
		tp->an_bng_mac_idx_pe0 = 0xffff;
		tp->an_bng_mac_idx_pe1 = 0xffff;
		tp->msg_enable = netif_msg_init(debug, default_msg);
		tp->mdio_lock = &mdio_lock;
#ifdef CS752X_NI_NAPI
		tp->napi.dev = dev;
#endif

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE

#if defined(CONFIG_CS75XX_GMAC0_TO_EXT_SWITCH)
		if (tp->port_id == GE_PORT0)
			dev->mtu = ETH_DATA_LEN + 4 /* SVLAN */;
#elif defined(CONFIG_CS75XX_GMAC1_TO_EXT_SWITCH)
		if (tp->port_id == GE_PORT1)
			dev->mtu = ETH_DATA_LEN + 4 /* SVLAN */;
#elif defined(CONFIG_CS75XX_GMAC2_TO_EXT_SWITCH)
		if (tp->port_id == GE_PORT2)
			dev->mtu = ETH_DATA_LEN + 4 /* SVLAN */;
#endif

#endif /* CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE */

		dev->netdev_ops = &ni_netdev_ops;
		dev->irq = tp->irq;
		dev->watchdog_timeo = NI_TX_TIMEOUT;
		dev->features = NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_RXCSUM |
				NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_UFO;
		dev->hw_features = dev->features;
#if NAPI_GRO
		dev->features |= NETIF_F_GRO;
#endif
		//dev->base_addr = tp->base_addr;
		/* Get MAC address */
		memcpy(&dev->dev_addr[0], &eth_mac[tp->port_id][0],
				dev->addr_len);
		memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
		spin_lock_init(&tp->stats_lock);
		spin_lock_init(&tp->lock);
		cs_ni_set_ethtool_ops(dev);

#if defined(CONFIG_CORTINA_CUSTOM_BOARD)
		dev_has_phy = false;
#elif defined(CONFIG_CORTINA_FPGA)
		dev_has_phy = true;
#elif defined(CONFIG_CORTINA_PON)
		if (tp->phy_addr == GE_PORT2_PHY_ADDR)
			dev_has_phy = true;
		else
			dev_has_phy = false;
#elif defined(CONFIG_CORTINA_BHR)
		dev_has_phy = false;
#elif defined(CONFIG_CS75XX_GMAC0_TO_EXT_SWITCH)
		if (tp->phy_addr == GE_PORT0_PHY_ADDR)
			dev_has_phy = false;
		else
			dev_has_phy = true;
#elif defined(CONFIG_CS75XX_GMAC1_TO_EXT_SWITCH)
		if (tp->phy_addr == GE_PORT1_PHY_ADDR)
			dev_has_phy = false;
		else
			dev_has_phy = true;
#elif defined(CONFIG_CS75XX_GMAC2_TO_EXT_SWITCH)
		if (tp->phy_addr == GE_PORT2_PHY_ADDR)
			dev_has_phy = false;
		else
			dev_has_phy = true;
#else /* defined(CONFIG_CS75XX_NONE_TO_EXT_SWITCH) */
		dev_has_phy = true;
#endif

		if (dev_has_phy == true) {
			/* ASIC will autodetect PHY status */
			err = cs_mdio_init(ni, tp);
			/* FIXME: need free net_device */
			if (err)
				return err;

			err = cs_phy_init(tp);
			/* FIXME: need free mdio, net_device */

			phy_vendor = cs_ni_get_phy_vendor(tp->phy_addr);
			printk("----> GE-%d: phy_id 0x%08x\n",
					tp->port_id, phy_vendor);
		} else {
			cs_ni_set_eth_cfg(tp, NI_CONFIG_1);
			cs_ni_set_mac_speed_duplex(tp, tp->phy_mode);

		    // BUG#37140
			if (cfg->flowctrl & FLOW_CTRL_TX)
			    cs_ni_flow_control(tp, NI_TX_FLOW_CTRL, CS_ENABLE);
			else
			    cs_ni_flow_control(tp, NI_TX_FLOW_CTRL, CS_DISABLE);

			if (cfg->flowctrl & FLOW_CTRL_RX)
			    cs_ni_flow_control(tp, NI_RX_FLOW_CTRL, CS_ENABLE);
			else
			    cs_ni_flow_control(tp, NI_RX_FLOW_CTRL, CS_DISABLE);
		}

#ifndef CS752X_LINUX_MODE
		cs_mfe_set_mac_address(dev);
#else
		cs_init_mac_address(dev);
#endif

		err = register_netdev(dev);
		if (err) {
			printk(KERN_ERR "%s: Cannot register net device, "
					"aborting.\n", dev->name);
			if (tp->phydev)
				phy_disconnect(tp->phydev);
			free_netdev(dev);
			return err;
		}

#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
		err = cs_ni_virt_ni_create_if(i, dev, tp);
		if (err)
			return err;
#endif /* CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE */

		//spin_lock_init(&tp->link_lock);
	}
	INIT_WORK(&ni_private_data.reset_task, cs_ni_reset_task);

	if (tp->rx_checksum == CS_ENABLE) {
		/* l4_chksum_chk_enable */
		fe_prsr_cfg0.wrd = 0;
		fe_prsr_cfg0_mask.wrd = 0;
		fe_prsr_cfg0.bf.l4_chksum_chk_enable = 0; /* 0: mean enable */
		fe_prsr_cfg0_mask.bf.l4_chksum_chk_enable = 1;
		write_reg(fe_prsr_cfg0.wrd, fe_prsr_cfg0_mask.wrd,
				FETOP_FE_PRSR_CFG_0);
		/* printk("%s::--> RX HW CHECKSUM Enable\n", __func__); */
	}

	for (i = 0; i < XRAM_RX_INSTANCE; i++) {
#ifdef CONFIG_DEBUG_KMEMLEAK
		INIT_LIST_HEAD(&ni_private_data.ni_skb_list_head[i]);
#endif
#ifdef CS752X_LINUX_MODE
		cs_ni_alloc_linux_free_buffer(ni_private_data.dev[0], i,
				linux_free_buf_size_tbl[i]);
#endif
	}

	cs_ni_init_interrupt_cfg();
#ifdef NI_WOL
	//if ((NI_READL_MODE(NI_TOP_NI_ETH_MAC_CONFIG1_0) & WOL_PKT_DET_EN) == 1)
	//tp->features |= NI_FEATURE_WOL;
	//device_set_wakeup_enable(&dev->dev, tp->features & RTL_FEATURE_WOL);
#endif /* NI_WOL */

	spin_lock_init(&ni_private_data.tx_lock);
#ifdef CS752X_NI_NAPI
	/* in napi_add , even dev, napi pointers are different but
	 * poll function pointer is the same, it would not start executing
	 * the poll function if there has the different irq is under polling
	 */
	/* debug_Aaron on 2013/03/25 for BUG#38688 adjust NAPI budget by ni_napi_budget */
	tp = netdev_priv(ni_private_data.dev[0]);
	netif_napi_add(ni_private_data.dev[0], &tp->napi,
			cs_ni_poll_0, ni_napi_budget);

	tp = netdev_priv(ni_private_data.dev[1]);
	netif_napi_add(ni_private_data.dev[1], &tp->napi,
			cs_ni_poll_1, ni_napi_budget);

	tp = netdev_priv(ni_private_data.dev[2]);
	netif_napi_add(ni_private_data.dev[2], &tp->napi,
			cs_ni_poll_2, ni_napi_budget);

	tp = netdev_priv(ni_private_data.dev[3]);
	netif_napi_add(ni_private_data.dev[3], &tp->napi,
			cs_ni_poll_6, ni_napi_budget);

	tp = netdev_priv(ni_private_data.dev[4]);
	netif_napi_add(ni_private_data.dev[4], &tp->napi,
			cs_ni_poll_7, ni_napi_budget);
#endif

#ifdef CONFIG_CS75XX_WFO
#ifdef CS752X_NI_NAPI
	tp = netdev_priv(ni_private_data.dev[5]);
	netif_napi_add(ni_private_data.dev[5], &tp->napi,
			cs_ni_poll_3, NI_NAPI_WEIGHT);

	tp = netdev_priv(ni_private_data.dev[6]);
	netif_napi_add(ni_private_data.dev[6], &tp->napi,
			cs_ni_poll_4, NI_NAPI_WEIGHT);

	tp = netdev_priv(ni_private_data.dev[7]);
	netif_napi_add(ni_private_data.dev[7], &tp->napi,
			cs_ni_poll_5, NI_NAPI_WEIGHT);
#endif
	struct net_device *tmp_dev;

	for (i = CS_NI_IRQ_WFO_PE0; i <= CS_NI_IRQ_WFO; i++) {
		tmp_dev = ni_private_data.dev[i];
		request_irq(tmp_dev->irq, cs_ni_rx_interrupt,
						IRQF_SHARED, tmp_dev->name, tmp_dev);

#ifdef CONFIG_CS75XX_KTHREAD_RX
		init_rx_task( tmp_dev->irq - IRQ_NI_RX_XRAM0);
#endif
#ifdef CS752X_NI_NAPI
		tp = netdev_priv(tmp_dev);
		tp->status = 1;
		napi_enable(&tp->napi);
#endif
		NI_WRITEL_MODE(1, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
					((tp->irq - IRQ_NI_RX_XRAM0) * 8));
	}

	cs75xx_pni_init();
#endif

	cs_init_lpb_an_bng_mac();

	cs_ni_instance_buf_low_clear();
	for (i = 0 ; i < XRAM_RX_INSTANCE; i++) {
		setup_timer(&rx_fill_buffer_timer[i], rx_fill_buffer_timer_cb, i);
	}

	return 0;
}
/* 3.4.11 Change for register as platform device */
/* module_init(cs_ni_init_module); */

/* debug_Aaron 2012/12/11 implement non-cacheable for performace tuning */
static int __init get_ni_rx_noncache(char *str)
{
        get_option(&str, &ni_rx_noncache);

        printk("%s: ni_rx_noncache=%d\n", __func__, ni_rx_noncache);
        /* if use RX non-cacheable buffer, QM ACP is disabled */
	if (ni_rx_noncache)
		qm_acp_enabled = 0;

        return 1;
}
__setup("ni_rx_noncache=", get_ni_rx_noncache);

/* debug_Aaron 2013/03/25 adjustable NAPI budget for BUG#38688 */
static int __init get_ni_napi_budget(char *str)
{
        get_option(&str, &ni_napi_budget);

        printk("%s: ni_napi_budget=%d\n", __func__, ni_napi_budget);

        return 1;
}
__setup("ni_napi_budget=", get_ni_napi_budget);
/* 3.4.11 Change for register as platform device */
static int cs_ni_cleanup_module_exit(struct platform_device *pdev)
{
	int i;
	ni_info_t *ni;
	mac_info_t *tp;

	ni = &ni_private_data;

	for (i = 0; i < GE_PORT_NUM; i++) {
#ifdef CONFIG_CS752X_VIRTUAL_NETWORK_INTERFACE
		cs_ni_virt_ni_remove_if(i);
#endif
		tp = netdev_priv(ni_private_data.dev[i]);
		if (tp->existed & CS_MDIOBUS_INITED) {
			tp->existed &= ~CS_MDIOBUS_INITED;
			if (tp->phydev)
				phy_disconnect(tp->phydev);
			mdiobus_unregister(tp->mdio_bus);
			cs_free(tp->mdio_bus->irq);
			mdiobus_free(tp->mdio_bus);
		}
		unregister_netdev(tp->dev);
		free_netdev(ni_private_data.dev[i]);
	}

#ifdef CONFIG_CS75XX_WFO
	struct net_device *tmp_dev;

	for (i = CS_NI_IRQ_WFO_PE0; i <= CS_NI_IRQ_WFO; i++) {
		tmp_dev = ni_private_data.dev[i];
		free_irq(tmp_dev->irq, tmp_dev);
		tp = netdev_priv(tmp_dev);
		napi_disable(&tp->napi);
		NI_WRITEL_MODE(0, NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 +
			((tp->irq - IRQ_NI_RX_XRAM0) * 8));
		free_netdev(tmp_dev);
	}
#endif
	//cs_ni_fini_proc();
	return 0;
}
/* 3.4.11 Change for register as platform device */
/* module_exit(cs_ni_cleanup_module); */

//Bug#42747++
#ifdef CONFIG_PM
static int cs_ne_suspend(struct device *device)
{
	struct net_device *ndev;
	mac_info_t *tp;
    int i;

    printk("%s: In\n", __func__);

    for (i=0; i<GE_PORT_NUM; i++) {
        ndev = ni_private_data.dev[i];
        if (ndev == NULL)
            continue;

        if (netif_running(ndev)) {
    	    netif_device_detach(ndev);
    	    netif_stop_queue(ndev);
    	    
    	    rtnl_lock();
    	    tp = netdev_priv(ndev);
    	    if (tp->phydev) {
#ifdef CS752X_NI_NAPI
	            napi_disable(&tp->napi);
#endif
                phy_stop(tp->phydev);

                cancel_tx_completion_timer(ndev);
    	    }
        	ni_set_mac_tx_rx(tp->port_id, CS_DISABLE);
    	    rtnl_unlock();
        }
    }
	// reset tx ring
	cs_ni_reset_tx_ring();
    cs_ni_disable_interrupts();
    
    printk("%s: Out\n", __func__);
	return 0;
} /* cs_ne_suspend() */

static int cs_ne_resume(struct device *device)
{
	struct net_device *ndev;
	mac_info_t *tp;
    int i;

    printk("%s: In\n", __func__);
    
    for (i=0; i<GE_PORT_NUM; i++) {
        ndev = ni_private_data.dev[i];
        if (ndev == NULL)
            continue;
            
        if (netif_running(ndev))
        {
            netif_device_attach(ndev);
            tp = netdev_priv(ndev);
        	ni_set_mac_tx_rx(tp->port_id, CS_ENABLE);
            if (tp->phydev) {
                rtnl_lock();
                phy_start(tp->phydev);
#ifdef CS752X_NI_NAPI
                napi_enable(&tp->napi);
#endif
                rtnl_unlock();
            }
        }
    }
    
    /* There is a bug in cs_ni_enable_interrupts()
     * enable interrupt here directly
     */
    //cs_ni_enable_interrupts();
	for (i = 0; i <= XRAM_INST_MAX; i++) {
		NI_WRITEL_MODE(1,
			NI_TOP_NI_CPUXRAM_RXPKT_0_INTENABLE_0 + (i * 8));
	}
    printk("%s: Out\n", __func__);
	return 0;
} /* cs_ne_resume() */


static const struct dev_pm_ops cs_ne_pm_ops = {
	.suspend		= cs_ne_suspend,
	.resume			= cs_ne_resume,
};


#define CS_DEV_NE_PM_OPS    (&cs_ne_pm_ops)

// Power management data plane APIs
cs_status_t cs_pm_ne_suspend ( void )
{
    cs_ne_suspend(NULL);
    
    return CS_E_OK;
} /* cs_pm_ne_suspend() */
EXPORT_SYMBOL(cs_pm_ne_suspend);


cs_status_t cs_pm_ne_resume ( void )
{
    cs_ne_resume(NULL);
    
    return CS_E_OK;
} /* cs_pm_ne_resume() */
EXPORT_SYMBOL(cs_pm_ne_resume);

#else /* !CONFIG_PM */

#define CS_DEV_NE_PM_OPS    NULL

#endif /* !CONFIG_PM */
//Bug#42747--



/* 3.4.11 platform_driver register */
static struct platform_driver cs_ni_platform_driver = {
	.probe		= cs_ni_init_module_probe,
	.remove		= cs_ni_cleanup_module_exit,
	.driver = {
		.name		= "g2-ne",
		.bus		= &platform_bus_type,
		.owner		= THIS_MODULE,
		.pm         = CS_DEV_NE_PM_OPS,    //Bug#42747
	},
};


static int __init cs_ni_init_module(void)
{
        return platform_driver_register(&cs_ni_platform_driver);
}

static void __exit cs_ni_cleanup_module(void)
{
        platform_driver_unregister(&cs_ni_platform_driver);
}

MODULE_AUTHOR("Jason Li");
module_exit(cs_ni_cleanup_module);
module_init(cs_ni_init_module);

MODULE_LICENSE("GPL");
