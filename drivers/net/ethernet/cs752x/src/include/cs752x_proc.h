/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#ifndef CS752X_PROC_H
#define CS752X_PROC_H

#include <linux/proc_fs.h>

#ifndef TRUE
#define TRUE	(1 == 1)
#endif

#ifndef FALSE
#define FALSE	(1 == 0)
#endif

#ifndef NULL
#define NULL	0
#endif

enum CS752X_DEBUG {
	CS752X_DISABLE = 0,
	CS752X_ENABLE = 1,
	CS752X_MAX = CS752X_ENABLE
};

enum CS752X_NI_DEBUG {
	DBG_DIASBLE	= 0,
	DBG_NI		= 0x00000001,
	DBG_IRQ		= 0x00000002,
	DBG_NI_IRQ	= 0x00000003,
	DBG_NI_LSO	= 0x00000004,
	DBG_NI_DUMP_RX	= 0x00000008,
	DBG_NI_DUMP_TX	= 0x00000010,
	DBG_NI_IO_CTL	= 0x00000020,
	DBG_NI_VIR_NI	= 0x00000040,
	DBG_NI_ETHTOOL	= 0x00000080,
	DBG_NI_PHY	= 0x00000100,
	CS752X_NI_MAX	= DBG_NI_PHY
};

enum CS752X_FE_DEBUG {
	CS752X_FE_ACL		= 0x00000001,	/* offset 0 */
	CS752X_FE_AN_BNG_MAC	= 0x00000002,
	CS752X_FE_CHECKMEM	= 0x00000004,
	CS752X_FE_CLASS		= 0x00000008,
	CS752X_FE_ETYPE		= 0x00000010,	/* offset 4 */
	CS752X_FE_FWDRSLT	= 0x00000020,
	CS752X_FE_VOQPOL	= 0x00000040,
	CS752X_FE_HW		= 0x00000080,
	CS752X_FE_LLC_HDR	= 0x00000100,	/* offset 8 */
	CS752X_FE_LPB		= 0x00000200,
	CS752X_FE_LPM		= 0x00000400,
	CS752X_FE_MC		= 0x00000800,
	CS752X_FE_PE_VOQ_DRP	= 0x00001000,	/* offset 12 */
	CS752X_FE_PKTLEN_RNGS	= 0x00002000,
	CS752X_FE_PORT_RNGS	= 0x00004000,
	CS752X_FE_QOSRSLT	= 0x00008000,
	CS752X_FE_RSLT_FVLAN_TBL = 0x00010000,	/* offset 16 */
	CS752X_FE_RSLT_L2_TBL	= 0x00020000,
	CS752X_FE_RSLT_L3_TBL	= 0x00040000,
	CS752X_FE_SDB		= 0x00080000,
	CS752X_FE_VLAN		= 0x00100000,	/* offset 20 */
	CS752X_FE_HASH_MASK	= 0x00200000,
	CS752X_FE_HASH_STATUS	= 0x00400000,
	CS752X_FE_HASH_OVERFLOW	= 0x00800000,
	CS752X_FE_HASH_CHECK	= 0x01000000,
	CS752X_FE_HASH_HASH	= 0x02000000,
	CS752X_FE_TABLE_CORE	= 0x04000000,
	CS752X_FE_TABLE_GENERIC	= 0x08000000,
	CS752X_FE_TABLE_UINTTEST= 0x10000000,
	CS752X_FE_TABLE_UTILITY	= 0x20000000,
	CS752X_FE_MAX		= 0x3FFFFFFF	/* the max. value when all flags are set */
};

enum CS752X_ADAPT_DEBUG {
	CS752X_ADAPT_CORE		= 0x00000001,
	CS752X_ADAPT_8021Q		= 0x00000002,
	CS752X_ADAPT_BRIDGE		= 0x00000004,
	CS752X_ADAPT_IPV4_MULTICAST	= 0x00000008,
	CS752X_ADAPT_IPV6_MULTICAST	= 0x00000010,
	CS752X_ADAPT_IPV4_FORWARD	= 0x00000020,
	CS752X_ADAPT_IPV6_FORWARD	= 0x00000040,
	CS752X_ADAPT_QOS_INGRESS	= 0x00000080,
	CS752X_ADAPT_QOS_EGRESS		= 0x00000100,
	CS752X_ADAPT_ARP		= 0x00000200,
	CS752X_ADAPT_IPSEC		= 0x00000400,
	CS752X_ADAPT_PPPOE		= 0x00000800,
	CS752X_ADAPT_HW_ACCEL_DC	= 0x00001000,
	CS752X_ADAPT_ETHERIP    	= 0x00002000,
	CS752X_ADAPT_NF_DROP    	= 0x00004000,
	CS752X_ADAPT_WFO    		= 0x00008000,
	CS752X_ADAPT_TUNNEL		= 0x00010000,
	CS752X_ADAPT_DSCP		= 0x00020000,   //Bug#40322
	CS752X_ADAPT_WIRELESS		= 0x00040000,
	CS752X_ADAPT_LPM                = 0x00080000,
        CS752X_ADAPT_ROUTE              = 0x00100000,
        CS752X_ADAPT_RULE_HASH          = 0x00200000,
	CS752X_ADAPT_MAX		= 0x003FFFFF	/* the max. value when all flags are set */
};

enum CS752X_CORE_LOGIC_DEBUG {
	CS752X_CORE_LOGIC_MCU		= 0x00000001,
	CS752X_CORE_LOGIC_CORE_VTABLE	= 0x00000002,
	CS752X_CORE_LOGIC_VTABLE_FW	= 0x00000004,
	CS752X_CORE_LOGIC_CORE_HMU	= 0x00000008,
	CS752X_CORE_LOGIC_HMU_FW	= 0x00000010,
	CS752X_CORE_LOGIC_FASTNET	= 0x00000020,
	CS752X_CORE_LOGIC_SKB_CB	= 0x00000040,
	CS752X_CORE_LOGIC_CORE_RULE_HMU = 0x00000080,
	CS752X_CORE_LOGIC_MAX		= 0x000000FF
};

#ifdef CONFIG_CS75XX_WFO
//++ For WFO
enum CS752X_WIFI_OFFLOAD_DEBUG {
	CS752X_WFO_DBG_IPC		= 0x00000001,
	CS752X_WFO_DBG_PNI	    = 0x00000002,
	CS752X_WFO_DBG_CSME	    = 0x00000004,
	CS752X_WFO_DBG_MAX		= 0x00000007
};

enum CS752X_WIFI_OFFLOAD_CSME {
	CS752X_WFO_CSME_ENABLE  = 0x00000001,
	CS752X_WFO_CSME_IGMPV3  = 0x00000002
};
//-- For WFO
#endif

#ifdef CONFIG_CS752X_PROC

int cs752x_str_paser(char *src_str, int max_tok_num,
		     char *tok_idx_list[] /*output */,
		     int *tok_cnt /*output */);
int cs752x_add_proc_handler(char *name, read_proc_t * hook_func_read,
			    write_proc_t * hook_func_write,
			    struct proc_dir_entry *parent);

#else /* CONFIG_PROC_FS */

static inline int cs752x_str_paser(char *src_str, int max_tok_num,
				   char *tok_idx_list[] /*output */ ,
				   int *tok_cnt /*output */ )
{
}

static inline int cs752x_add_proc_handler(char *name,
					  read_proc_t * hook_func_read,
					  write_proc_t * hook_func_write,
					  struct proc_dir_entry *parent)
{
}

#endif /* CONFIG_CS752X_PROC */

#endif /* CS752X_PROC_H */
