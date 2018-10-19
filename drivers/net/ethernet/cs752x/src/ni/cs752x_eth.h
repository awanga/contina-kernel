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

#ifndef __CS752X_ETH_H__
#define __CS752X_ETH_H__

#include <mach/platform.h>
#include <mach/hardware.h>
#include <mach/registers.h>
#include <linux/export.h>
#include <linux/mii.h>
#include "cs75xx_qm.h"
#include "cs_fe.h"
#include "cs75xx_ethtool.h"
#ifdef CONFIG_DEBUG_KMEMLEAK
#include <linux/list.h>
#endif
//#include "cs75xx_reg.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ni_debug;
extern u32 cs_ni_fastbridge;
#endif

#define DRV_NAME	"CS75xx Gigabit Ethernet driver"
#define DRV_VERSION	"0.2.1"
#define NI_DRIVER_NAME	DRV_NAME DRV_VERSION

#ifndef CONFIG_CORTINA_FPGA
#define CS752X_ASIC	1
#endif
#ifdef CONFIG_CS75XX_WFO
#include <mach/cs75xx_pni.h>
#endif

//#define CS752X_MANAGEMENT_MODE 1

#ifndef CS752X_MANAGEMENT_MODE
#define CS752X_LINUX_MODE	1
#define CS752x_DMA_LSO_MODE 1
//#define BYPASS_FE	1		/* We do not enable BYPASS_FE yet */
#endif /* CS752X_MANAGEMENT_MODE */

#ifndef CONFIG_INTR_COALESCING
#define CONFIG_INTR_COALESCING		1
#endif

#ifdef CONFIG_CS752X_HW_ACCELERATION
#undef BYPASS_FE
#endif

#undef NI_WOL
/* XRAM BASE ADDRESS */
#define NI_XRAM_BASE 	IO_ADDRESS(GOLDENGATE_XRAM_BASE)
#define NI_TOP_BASE 	IO_ADDRESS(GOLDENGATE_NI_TOP_BASE)
#define FE_TOP_BASE 	IO_ADDRESS(GOLDENGATE_FE_TOP_BASE)
#define LPM_TOP_BASE 	IO_ADDRESS(FETOP_LPM_INTERRUPT_0)
#define HASH_TOP_BASE 	IO_ADDRESS(FETOP_HASH_STATUS)
#define QM_TOP_BASE 	IO_ADDRESS(GOLDENGATE_QM_TOP_BASE)
#define TM_TOP_BASE 	IO_ADDRESS(GOLDENGATE_TM_TOP_BASE)
#define SCH_BASE 	IO_ADDRESS(GOLDENGATE_SCH_BASE)
#define DMA_LSO_BASE 	IO_ADDRESS(GOLDENGATE_DMA_LSO_BASE)
#define MDIO_BASE 	IO_ADDRESS(GOLDENGATE_MDIO_BASE)
#define NI_INTER_OFFSET	IO_ADDRESS(NI_TOP_NI_DEBUG_2) /* 0x2f0 */

#define SKB_RESERVE_BYTES	128
#define XRAM_RX_INSTANCE	8 /* not include PTP */
#define XRAM_TX_INSTANCE	2 /* not include DMA LSO */
#define HEADER_E_OFFSET		8
#define CPU_HEADER_OFFSET	16
#define INSTANCE_NUM		8
#define RESET_NI		7
#define NI_PORT_NUM		8
#define QUEUE_PER_INSTANCE	8
#define CS_REGS_LEN		256
#define MIN_DMA_SIZE		24
#define SWAP_DWORD(x) (unsigned long)((((unsigned long)x & 0x000000FF) << 24) | \
					(((unsigned long)x & 0x0000FF00) << 8) | \
					(((unsigned long)x & 0x00FF0000) >> 8) | \
					(((unsigned long)x & 0xFF000000) >> 24))

/* MAC address length */
#define MAC_ADDR_LEN		6
#define CRCPOLY_LE 		0xedb88320

#define MAX_JUMBO_FRAME_SIZE	MAX_PKT_LEN  /* defined in cs75xx_qm.h */
#define MIN_FRAME_SIZE		64
#define MAX_FRAME_SIZE		MAX_PKT_LEN
#define NI_MAX_PORT_CALENDAR	96
#define MIN_PKT_SIZE		64
#define RX_DESC_NUM		32
#define XRAM_FRAME_SIZE		(56 * RX_DESC_NUM) /* magic number? */
#define QM_RESERVED_OFFSET	256

#define CS_ENABLE		1
#define CS_DISABLE		0
#define NI_CONFIG_0		0
#define NI_CONFIG_1		1
#define NI_CONFIG_2		2
#define NI_RX_FLOW_CTRL		0
#define NI_TX_FLOW_CTRL		1

#define CS75XX_MDIOBUS_NAME	"cs75xx_mdiobus"
#define CS75XX_MDIOBUS_ID	"0"

/*
 * XRAM total size is 1M, 0x000FFFFF
 * channel 1 RX:   0~128k, 0x00000 ~ 0x1FFFF
 * channel 1 TX: 128~256k, 0x20000 ~ 0x3FFFF
 */
#define RX_BASE_ADDR		0x00000000
#define RX_TOP_ADDR		0x000003FF
#define TX_BASE_ADDR		0x00000400
#define TX_TOP_ADDR		0x000007FF

#define GE_PORT_NUM		3

/* Currently this is fixed. */
#ifdef CONFIG_CORTINA_FPGA
#define GE_PORT0_PHY_ADDR	0x01 /* GMAC-0 */
#define GE_PORT1_PHY_ADDR	0x02 /* GMAC-1 */
#define GE_PORT2_PHY_ADDR	0x03 /* GMAC-2 */
#else
#define GE_PORT0_PHY_ADDR	CONFIG_CS75XX_PHY_ADDR_GMAC0 /* GMAC-0 */
#define GE_PORT1_PHY_ADDR	CONFIG_CS75XX_PHY_ADDR_GMAC1 /* GMAC-1 */
#define GE_PORT2_PHY_ADDR	CONFIG_CS75XX_PHY_ADDR_GMAC2 /* GMAC-2 */
#endif

#define CS_MAC_EXISTED_FLAG	0x00000001
#define CS_MDIOBUS_INITED	0x00000002
#define CS_PHYFLG_IS_CONNECTED	0x00000004
#define CS_PHYFLG_10_100_ONLY	0x00000008

/*
 DO NOT change NI_NAPI_WEIGHT to 64. That will cause packet loss.
 */
#define NI_NAPI_WEIGHT		16
#define NI_DBG			1
#define DMA_TX_INTR_DISABLED	1
#define SOFTWARE		1
#define	HARDWARE		0
#define NI_TX_TIMEOUT		(6 * HZ)

#define NI_MAC_PHY_MII		1
#define NI_MAC_PHY_RGMII_1000	2
#define NI_MAC_PHY_RGMII_100	3
#define NI_MAC_PHY_RMII		5
#define NI_MAC_PHY_SSSMII	7
#define NORMAL_MODE		0 /* Normal MAC mode */
#define PHY_MODE		1 /* PHY mode */

#if defined(CONFIG_CS75XX_GMAC0_MII)
#define GMAC0_PHY_MODE	NI_MAC_PHY_MII
#elif defined(CONFIG_CS75XX_GMAC0_RGMII_1000)
#define GMAC0_PHY_MODE	NI_MAC_PHY_RGMII_1000
#elif defined(CONFIG_CS75XX_GMAC0_RGMII_100)
#define GMAC0_PHY_MODE	NI_MAC_PHY_RGMII_100
#elif defined(CONFIG_CS75XX_GMAC0_RMII)
#define GMAC0_PHY_MODE	NI_MAC_PHY_RMII
#else
#define GMAC0_PHY_MODE	NI_MAC_PHY_RGMII_1000
#endif

#if defined(CONFIG_CS75XX_GMAC1_MII)
#define GMAC1_PHY_MODE	NI_MAC_PHY_MII
#elif defined(CONFIG_CS75XX_GMAC1_RGMII_1000)
#define GMAC1_PHY_MODE	NI_MAC_PHY_RGMII_1000
#elif defined(CONFIG_CS75XX_GMAC1_RGMII_100)
#define GMAC1_PHY_MODE	NI_MAC_PHY_RGMII_100
#elif defined(CONFIG_CS75XX_GMAC1_RMII)
#define GMAC1_PHY_MODE	NI_MAC_PHY_RMII
#else
#define GMAC1_PHY_MODE	NI_MAC_PHY_RGMII_1000
#endif

#if defined(CONFIG_CS75XX_GMAC2_MII)
#define GMAC2_PHY_MODE	NI_MAC_PHY_MII
#elif defined(CONFIG_CS75XX_GMAC2_RGMII_1000)
#define GMAC2_PHY_MODE	NI_MAC_PHY_RGMII_1000
#elif defined(CONFIG_CS75XX_GMAC2_RGMII_100)
#define GMAC2_PHY_MODE	NI_MAC_PHY_RGMII_100
#elif defined(CONFIG_CS75XX_GMAC2_RMII)
#define GMAC2_PHY_MODE	NI_MAC_PHY_RMII
#else
#define GMAC2_PHY_MODE	NI_MAC_PHY_RGMII_1000
#endif


#define CS_READ			0
#define CS_WRITE		1

#define RX_BASE_ADDR_LOC	0x07FF
#define RX_TOP_ADDR_LOC		0x07FF0000
#define HW_WR_PTR		0x07FF
#define SW_RD_PTR		0x07FF

#define RWPTR_ADVANCE_ONE(x, max)	((x == (max -1)) ? 0 : x+1)
#define RWPTR_RECEDE_ONE(x, max)	((x == 0) ? (max -1) : x-1)
#define SET_WPTR(addr, data)		(*(volatile u16 * const)((u32)(addr)+2) = (u16)data)
#define SET_RPTR(addr, data)		(*(volatile u16 * const)((u32)(addr)) = (u16)data)


#define MAXIMUM_ETHERNET_FRAME_SIZE	1518 /* With FCS */
#define MINIMUM_ETHERNET_FRAME_SIZE	64   /* With FCS */
#define ENET_HEADER_SIZE		14
#define ETHERNET_FCS_SIZE		4

#define DMA_BASE_MASK 			(~0x0f)
/* FIXME: TXQ 6 and 7 for memory copy, check with Jason */
#define NI_DMA_LSO_RXQ_NUM		8	/* Maximum is 8 */
#define NI_DMA_LSO_TXQ_NUM		6	/* Maximum is 8 */
#define NI_DMA_LSO_TXDESC		10
#define NI_DMA_LSO_TXDESC_NUM		(1 << NI_DMA_LSO_TXDESC)
#define CPU_XRAM_BUFFER			1	/* maximum is 8 */
#define CPU_XRAM_BUFFER_NUM		(1 << CPU_XRAM_BUFFER)

#define ACCEPTBROADCAST			0
#define ACCEPTMULTICAST			1
#define ACCEPTALLPACKET			2

#define RW_PTR_MASK			0x07FF

#define MAX_QM_SEG_LEN 3584
#ifdef CONFIG_CS75XX_NO_JUMBO_FRAMES
#define SKB_PKT_LEN			(SKB_WITH_OVERHEAD(2048) - 0x100 - NET_SKB_PAD)
#else
#define SKB_PKT_LEN			3584 /* 3.5 * 1024 */
#endif
#define REG32(addr)			(*(volatile unsigned long  * const)(addr))

#define CHECKSUM_ERROR			1

typedef enum {
	XRAM_INST_0 = 0,
	XRAM_INST_1,
	XRAM_INST_2, /* Instance 2 - 9 is valid only for RX direction */
	XRAM_INST_3,
	XRAM_INST_4,
	XRAM_INST_5,
	XRAM_INST_6,
	XRAM_INST_7,
	XRAM_INST_8,
	XRAM_INST_MAX,
} xram_inst_t;

typedef enum {
	XRAM_DIRECTION_RX = 0,
	XRAM_DIRECTION_TX,
} xram_direction_t;

typedef enum {
	XRAM_READ = 0,
	XRAM_WRITE,
} xram_rw_t;

/*
 * DMA LSO Tx Description Word 0
 */
typedef union {
	u32 bits32;
	struct bit_901d8 {
		u32 buffer_size	: 16;	/* bits 15:0  */
		u32 desc_cnt	: 6;	/* bits 21:16 */
		u32 sgm		: 5;	/* bits 26:22 */
		u32 sof_eof	: 2;	/* bits 28:27 */
		u32 cache	: 1;	/* bits 29:29 */
		u32 share	: 1;	/* bits 30:30 */
		u32 own		: 1;	/* bits 31:31 */
	} bits;
} dma_txdesc_0_t;

#define OWN_BIT		0x80000000
#define SOF_BIT		0x10000000 /* 10: first descriptor */
#define EOF_BIT		0x08000000 /* 01: last descriptor  */
#define ONE_BIT		0x18000000 /* 11: only one descriptor */
#define LINK_BIT	0x00000000 /* 00: link descriptor  */

typedef union {
	u32 bits32;
	struct bit_901dc {
		u32 buf_addr;	/* bits 31:0 */
	} bits;
} dma_txdesc_1_t;

typedef union {
	u32 bits32;
	struct bit_901e0 {
		u32 frame_size	: 16;	/* bits 15:0  */
		u32 jumbo_size	: 16;	/* bits 31:16 */
	} bits;
} dma_txdesc_2_t;

typedef union {
	u32 bits32;
	struct bit_901e4 {
		u32 segment_size	: 14;	/* bits 13:0  */
		u32 tq_flag		: 2;	/* bits 15:14 */
		u32 segment_en 		: 1;	/* bits 16:16 */
		u32 ipv4_en 		: 1;	/* bits 17:17 */
		u32 ipv6_en		: 1;	/* bits 18:18 */
		u32 tcp_en		: 1;	/* bits 19:19 */
		u32 udp_en		: 1;	/* bits 20:20 */
		u32 bypass_en		: 1;	/* bits 21:21 */
		u32 lenfix_en		: 1;	/* bits 22:22 */
		u32 tq_flag1		: 9;	/* bits 31:23 */
	} bits;
} dma_txdesc_3_t;

#ifndef __BIT
#define __BIT(x)		(1 << (x))
#endif
#define LSO_SEGMENT_EN		__BIT(16)
#define LSO_IPV4_FRAGMENT_EN	__BIT(17)
#define LSO_IPV6_FREGMENT_EN	__BIT(18)
#define LSO_TCP_CHECKSUM_EN	__BIT(19)
#define LSO_UDP_CHECKSUM_EN	__BIT(20)
#define LSO_BYPASS_EN		__BIT(21)
#define LSO_IP_LENFIX_EN	__BIT(22)

typedef union {
	u32 bits32;
	struct bit_901e8 {
		u32 header_valid	: 1;	/* bits 0:0   */
		u32 no_stuff		: 1;	/* bits 1:1   */
		u32 no_crc		: 1;	/* bits 2:2   */
		u32 dvoq		: 8;	/* bits 10:3  */
		u32 bypass_cos		: 3;	/* bits 13:11 */
		u32 mark		: 1;	/* bits 14:14 */
		u32 cpu_ptp_flag	: 1;	/* bits 15:15 */
		u32 pspid		: 4;	/* bits 19:16 */
		u32 fwd_type		: 4;	/* bits 23:20 */
		u32 TqFlag2		: 8;	/* bits 31:24 */
	} bits;
} dma_txdesc_4_t;

typedef union {
	u32 bits32;
	struct bit_901ec {
		u32 recirc_idx	: 10;	/* bits 9:0   */
		u32 mcgrpid	: 9;	/* bits 18:10 */
		u32 tq_flag3 	: 13;	/* bits 31:19 */
	} bits;
} DMA_TXDESC_5_T;

typedef struct {
	u32 tq_flag4		: 32;	/* bits 31:0 */
} DMA_TXDESC_6_T;

typedef struct {
	u32 tq_flag5		: 32;	/* bits 31:0 */
} DMA_TXDESC_7_T;

typedef struct {
	dma_txdesc_0_t	word0;
	dma_txdesc_1_t	word1;
	dma_txdesc_2_t	word2;
	dma_txdesc_3_t	word3;
	dma_txdesc_4_t	word4;
	DMA_TXDESC_5_T	word5;
	DMA_TXDESC_6_T	word6;
	DMA_TXDESC_7_T	word7;
} dma_txdesc_t;

typedef union {
	u32 bits32;
	struct bit_90098 {
		u32 wptr		: 13;	/* bits 12:0  */
		u32 reserved		: 19;	/* bits 31:13 */
	} bits;
} dma_wptr_t;

typedef union {
	u32 bits32;
	struct bit_9009c {
		u32 rptr		: 13;	/* bits 12:0  */
		u32 reserved		: 19;	/* bits 31:13 */
	} bits;
} dma_rptr_t;

enum features {
	NI_FEATURE_WOL	= (1 << 0),
	NI_FEATURE_MII	= (1 << 1),
	NI_FEATURE_GMII	= (1 << 2),
};

typedef enum {
	PLAIN_READ = 0,
	READ_MSB_CLEAR,
	READ_ALL_CLEAR,
} ni_mib_access_t;

typedef enum {
	NI_ACCESS_READ = 0,
	NI_ACCESS_WRITE,
} ni_access_t;

typedef enum {
	NI_VOQ_DID_GE0 = 0x0,
	NI_VOQ_DID_GE1 = 0x1,
	NI_VOQ_DID_GE2 = 0x2,
	NI_VOQ_DID_CRYPTO = 0x4,
	NI_VOQ_DID_ENCAP = 0x5,
	NI_VOQ_DID_MC = 0x6,
	NI_VOQ_DID_CPU0 = 0x8,
	NI_VOQ_DID_CPU1 = 0x9,
	NI_VOQ_DID_CPU2 = 0xA,
	NI_VOQ_DID_CPU3 = 0xB,
	NI_VOQ_DID_CPU4 = 0xC,
	NI_VOQ_DID_CPU5 = 0xD,
	NI_VOQ_DID_CPU6 = 0xE,
	NI_VOQ_DID_CPU7 = 0xF,
} ni_voq_did_t; /* The destination selection for the VoQ */

typedef struct {
/* CS_LITTLE_ENDIAN */
	unsigned long long pkt_size		: 14;
	unsigned long long fwd_type		: 4;
	unsigned long long pspid		: 4;
	unsigned long long original_lspid	: 4;
	unsigned long long original_lspid_valid	: 1;
	unsigned long long recirc_idx		: 10;
	unsigned long long mc_grp_id		: 9;
	unsigned long long mc_index		: 5;
	unsigned long long flags		: 14;
	unsigned long long cpu_ptp_flag		: 1;
	unsigned long long ts_flag		: 1;
	unsigned long long mark			: 1;
	unsigned long long addr_cnt		: 7;
	unsigned long long bypass_cos		: 3;
	unsigned long long dvoq			: 8;
	unsigned long long no_crc		: 1;
	unsigned long long no_stuff		: 1;
	unsigned long long ni_port_id		: 3;
	unsigned long long replace_l4_chksum	: 1;
	unsigned long long spare		: 4;
} __attribute__((__packed__)) HEADER_A_T;

typedef union {
	u32 bits32;
	struct bit_header_dma_0 {
		u32 header_valid	: 1;	/* bits 0:0 */
		u32 no_stuff		: 1;	/* bits 1:1 */
		u32 no_crc		: 1;	/* bits 2:2 */
		u32 dvoq		: 8;	/* bits 10:3 */
		u32 bypass_cos		: 3;	/* bits 13:11 */
		u32 mark		: 1;	/* bits 14:14 */
		u32 cpu_ptp_flag	: 1;	/* bits 15:15 */
		u32 pspid		: 4;	/* bits 19:16 */
		u32 fwd_type		: 4;	/* bits 23:20 */
		u32 TqFlag2		: 8;	/* bits 31:24 */
	} bits;
} ni_header_a_0_t;

typedef union {
	u32 bits32;
	struct bit_header_dma_1 {
		u32 recirc_idx		: 10;	/* bits 9:0 */
		u32 mcgripid		: 9;	/* bits 18:10 */
		u32 TqFlag3		: 13;	/* bits 31:19 */
	} bits;
} ni_header_a_1_t;

typedef union {
	u32 bits32[2];
	struct bit_header_xr {
		u32 rsv1		: 32;	/* bits 0:31 */
		u32 next_link		: 12;	/* bits 32:43 */
		u32 bytes_valid		: 4;	/* bits 34:47 */
		u32 rsv2		: 12;	/* bits 48:59 */
		u32 invalid_pkt_len	: 1;	/* bits 60 */
		u32 error_flag		: 1;	/* bits 61 */
		u32 hdr_a		: 1;	/* bits 62 */
		u32 ownership		: 1;	/* bits 63 */
	} bits;
} HEADER_XR_T;

typedef struct {
	HEADER_XR_T header_xr;
	HEADER_E_T header_e;
	CPU_HEADER0_T cpu_header0;
	CPU_HEADER1_T cpu_header1;
	u32 payload_0;
	u32 payload_1;
	u32 buf0;
	u32 buf1;
	u32 buf2;
	u32 buf3;
} xram_rxdesc_t;

typedef enum {
	NI_GE0 = 0,
	NI_GE1 = 1,
	NI_GE2 = 2,
	NI_CPU = 3,
	NI_CCORE = 4,
	NI_ECORE = 5,
	NI_MCAST = 6,
	NI_MIRROR = 7
} ni_port_t;

enum cfg_port {
	GE_PORT0_CFG = 0,
	GE_PORT1_CFG,
	GE_PORT2_CFG
};

struct ni_link_config {
	u8 link;
	u32 advertising;
	u16 speed;
	u8 duplex;
	u8 autoneg;
	u8 flowctrl;
	u32 lcl_adv;    // BUG#37140
	u32 rmt_adv;    // BUG#37140
};

typedef struct mac_private {
	struct net_device *dev;
	u32 existed;
	u32 base_addr;
	u32 msg_enable;
	struct phy_device *phydev;
	struct ni_link_config link_config;
	u32 *mac_addr;
	u32 irq;
	u8 port_id;
	u8 status;
	u32 phy_mode;
	u32 phy_addr;
	u8 rmii_clk_src;	/* map to rmii_clksrc_ge0/1/2 of
				 * NI_TOP_NI_ETH_INT_CONFIG1_t */
	struct mii_if_info mii;
	struct mii_bus *mdio_bus;
	struct napi_struct napi;
	//void (*napi_poll)(struct napi_struct *napi, int budget);
	u32 linux0_sdram_addr;
	u32 linux1_sdram_addr;
	u32 linux2_sdram_addr;
	u32 linux3_sdram_addr;
	u32 linux4_sdram_addr;
	u32 linux5_sdram_addr;
	u32 linux6_sdram_addr;
	u32 linux7_sdram_addr;
	u32 rx_xram_intr;
	u32 rx_curr_desc;
	u8 rx_checksum;
	struct sk_buff *curr_rx_skb;
	struct net_device_stats ifStatics;
	struct cs_ethtool_stats stats;
	spinlock_t lock;
	spinlock_t stats_lock;
	//spinlock_t link_lock;
	/* FE related info */
	unsigned int an_bng_mac_idx;
	unsigned int an_bng_mac_idx_pe0;
	unsigned int an_bng_mac_idx_pe1;
//	struct work_struct reset_task;
	spinlock_t *mdio_lock;
	int ni_driver_state; /* 1: ready, 0: initializing */
	int (*sw_phy_mode)(int mode); /* 1: enable, 0: disable */

#ifndef CS752X_NI_TX_COMPLETE_INTERRUPT
	unsigned long tx_completion_queues;
	struct timer_list tx_completion_timer;
#endif
} mac_info_t;

typedef struct {
	u32 rptr_reg;
	u32 wptr_reg;
	//u32 desc_base;
	dma_txdesc_t *desc_base;
	u32 total_desc_num;
	u16 wptr;
	unsigned short finished_idx;
	struct sk_buff *tx_skb[NI_DMA_LSO_TXDESC_NUM];
	//unsigned long total_sent;
	unsigned long total_finished;
	unsigned long intr_cnt;
#ifdef CONFIG_CS75XX_WFO
	void	*xmit_pkt[NI_DMA_LSO_TXDESC_NUM];
	u16	wfo_pe_id[NI_DMA_LSO_TXDESC_NUM];
#endif
	spinlock_t lock;
	//dma_addr_t txq_base_dma;
} dma_swtxq_t;

#ifdef CONFIG_INTR_COALESCING
typedef struct cs_ni_coalescing_config {
	u32 delay_usec; /* actually this value x 100 nano second. */
	u16 delay_num_pkts;
	u16 int_first_pkt; /* if 1, int on first packet. */
} cs_ni_coalescing_cfg_s;
#endif

#ifdef CONFIG_DEBUG_KMEMLEAK
typedef struct ni_skb_list_s {
	struct sk_buff *skb;
	struct list_head list;
} ni_skb_list_t;
#endif


#define CS_NI_IRQ_PE 3
#define CS_NI_IRQ_ARP 4

#define CS_NI_IRQ_WFO_PE0 5
#define CS_NI_IRQ_WFO_PE1 6
#define CS_NI_IRQ_WFO 7

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_NI_INSTANCE_WFO 5
#define CS_NI_INSTANCE_WFO_802_3_PE0 3
#define CS_NI_INSTANCE_WFO_802_3_PE1 4
#else
#define CS_NI_INSTANCE_WFO_PE0 3
#define CS_NI_INSTANCE_WFO_PE1 4
#define CS_NI_INSTANCE_WFO 5
#define CS_NI_INSTANCE_WFO_802_3 6
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#ifdef CONFIG_CS75XX_WFO
#define CS_NI_VIRTUAL_DEV 3
#else
#define CS_NI_VIRTUAL_DEV 0
#endif

#define NI_DMA_LSO_TXQ_IDX 0
#define PE_DMA_LSO_TXQ_IDX 5
#define CS_NI_IRQ_DEV (GE_PORT_NUM + 2)

typedef struct ni_private {
	//mac_info_t *mac[CS_NI_IRQ_DEV];
	dma_swtxq_t swtxq[NI_DMA_LSO_TXQ_NUM];

	u8 linux_skb_ptr_offset;
	u32 intr_mask;
	u32 intr_port_mask[INSTANCE_NUM];
	u32 intr_rxfifo_mask;
	u32 intr_txem_mask;
	u32 intr_cpuxram_cnt_mask;
	u32 intr_cpuxram_err_mask;
	u32 intr_cpuxram_rxpkt_mask;
	u32 intr_cpuxram_txpkt_mask;
	u32 intr_qm_mask;
	u32 intr_sch_mask;
	u32 intr_dma_lso_mask;
	u32 intr_dma_lso_desc_mask;
	u32 intr_lso_rx_mask[NI_DMA_LSO_RXQ_NUM];
	u32 intr_lso_tx_mask[NI_DMA_LSO_TXQ_NUM];
	u32 intr_dma_lso_bmc_mask;
	struct net_device *dev[CS_NI_IRQ_DEV + CS_NI_VIRTUAL_DEV];
#ifdef CONFIG_INTR_COALESCING
	cs_ni_coalescing_cfg_s intr_coalescing_cfg;
#endif
	//struct napi_struct napi;
	struct net_device *dev_arp;
	struct net_device *dev_re;

	struct work_struct reset_task;

	//u8 napi_rx_status[9];
	//spinlock_t rx_lock;
	spinlock_t tx_lock;
#ifdef CONFIG_DEBUG_KMEMLEAK
	struct list_head ni_skb_list_head[LINUX_FREE_BUF];
#endif
} ni_info_t;

/*
 * packet_type
 *  0 : normal
 *  1 : wfo
 *  2 : ipsec
*/
typedef enum {
	CS_APP_PKT_NORMAL	= 0,
	CS_APP_PKT_WFO		= 1,
	CS_APP_PKT_IPSEC	= 2,
	CS_APP_PKT_TUNNEL	= 3,
	CS_APP_PKT_WIRELESS = 4
} cs_app_pkt_type_t;

extern ni_info_t ni_private_data;

#define PER_CLOCK			100 /* 10 ns Time period units*/
#define PER_MDIO_MAX_PHY_ADDR		63
#define PER_MDIO_MAX_GPIO_PHY		32
#define PER_MDIO_AUTO_POLL_INTVAL	100
#define PER_MAX_TMR			5
#define PER_WDT_DEF_CLK			0xFFFFFFFF
#define PER_MDIO_POLL_TIME		1000
#define PER_BLOCK			0
#define PER_NON_BLOCK			1

#define PER_MDIO_MAX_REG_ADDR		31
#define PER_MDIO_SELECT_GPIO		31

int cs_ni_open(struct net_device *dev);
int cs_ni_close(struct net_device *dev);
void cs_ni_set_mac_speed_duplex(mac_info_t *tp, int mac_interface);
void cs_ni_flow_control(mac_info_t *tp, u8 rx_tx, u8 flag);
void cs_ni_set_rx_mode(struct net_device *dev);
void cs_ni_set_eth_cfg(mac_info_t * tp, int config);
void cs_ni_set_short_term_shaper(mac_info_t *tp);
struct cs_ethtool_stats *cs_ni_update_stats(mac_info_t *tp);
int ni_special_start_xmit(struct sk_buff *skb, HEADER_A_T *header_a, u16 voq);
int ni_start_xmit_to_re(struct sk_buff *skb, struct net_device *dev);
int ni_xmit_dma_lso_txq(struct sk_buff *skb, struct net_device *dev,
		ni_header_a_0_t *header_a0, ni_header_a_1_t *header_a1, u8 qid);
void ni_dm_byte(u32 location, int length);
void ni_dm_short(u32 location, int length);
void ni_dm_long(u32 location, int length);
int ni_mdio_write(int phy_addr, int reg_addr, u16 value);
int ni_mdio_read(int phy_addr, int reg_addr);
void ni_set_ne_enabled(u8 enabled);
struct net_device *ni_get_device(unsigned char port_id);

#endif /* __CS752X_ETH_H__ */
