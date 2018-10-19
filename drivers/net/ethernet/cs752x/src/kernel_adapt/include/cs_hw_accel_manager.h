/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#ifndef CS752X_HW_ACCEL_MANAGER_H
#define CS752X_HW_ACCEL_MANAGER_H

#include <linux/export.h>

enum CS752X_ADAPT_ENABLE {
	CS752X_ADAPT_ENABLE_DISABLE		= 0x00000000,
	CS752X_ADAPT_ENABLE_8021Q	        = 0x00000002,
	CS752X_ADAPT_ENABLE_BRIDGE 	        = 0x00000004,
	CS752X_ADAPT_ENABLE_IPV4_MULTICAST	= 0x00000008,
	CS752X_ADAPT_ENABLE_IPV6_MULTICAST 	= 0x00000010,
	CS752X_ADAPT_ENABLE_IPV4_FORWARD 	= 0x00000020,
	CS752X_ADAPT_ENABLE_IPV6_FORWARD	= 0x00000040,
	CS752X_ADAPT_ENABLE_ARP 	        = 0x00000080,
	CS752X_ADAPT_ENABLE_IPSEC	      	= 0x00000100,
	CS752X_ADAPT_ENABLE_PPPOE_SERVER 	= 0x00000200,
	CS752X_ADAPT_ENABLE_PPPOE_CLIENT 	= 0x00000400,
	CS752X_ADAPT_ENABLE_HW_ACCEL_DC		= 0x00000800,
	CS752X_ADAPT_ENABLE_PPPOE_KERNEL 	= 0x00001000,
	CS752X_ADAPT_ENABLE_QoS_REMARK 		= 0x00002000,
	CS752X_ADAPT_ENABLE_NF_DROP 		= 0x00004000,
	CS752X_ADAPT_ENABLE_WFO 		= 0x00008000,
	CS752X_ADAPT_ENABLE_TUNNEL		= 0x00010000,
	CS752X_ADAPT_ENABLE_WIRELESS		= 0x00020000,
	CS752X_ADAPT_ENABLE_L2TP_CTRL		= 0x00040000,
	CS752X_ADAPT_ENABLE_IPSEC_CTRL		= 0x00080000,
	CS752X_ADAPT_ENABLE_LOCAL_IN		= 0x00100000,
	CS752X_ADAPT_ENABLE_MAX 	        = 0x001FFFFF	/* the max. value when all flags are set */
};

typedef enum {
	CS_HAM_ACTION_MODULE_ENABLE,
	CS_HAM_ACTION_MODULE_DISABLE,
	CS_HAM_ACTION_MODULE_REMOVE,
	CS_HAM_ACTION_MODULE_INSERT,
	CS_HAM_ACTION_CLEAN_HASH_ENTRY,
} cs_ham_notify_event;

#define IPSEC_OFFLOAD_MODE_BOTH 1
#define IPSEC_OFFLOAD_MODE_PE0 2
#define IPSEC_OFFLOAD_MODE_PE1 3

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
typedef enum {
	CS_FUNC_TYPE_NONE = 0,
	CS_FUNC_TYPE_WIRELESS_ACCEL_11AC,
	CS_FUNC_TYPE_WIRELESS_ACCEL_11N,
	CS_FUNC_TYPE_WFO_11AC,
	CS_FUNC_TYPE_WFO_11N,
	CS_FUNC_TYPE_VPN_OFFLOAD_PE0,
	CS_FUNC_TYPE_VPN_OFFLOAD_PE1,
	CS_FUNC_TYPE_IPLIP_OFFLOAD_PE1,
	CS_FUNC_TYPE_MAX,
} cs_acceleration_func_type_e;
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) ||\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
typedef enum {
	CS_VPN_OFFLOAD_NONE			= 0x00000000,
	CS_VPN_OFFLOAD_IPSEC			= 0x00000001,
	CS_VPN_OFFLOAD_L2TP			= 0x00000002,
	CS_VPN_OFFLOAD_L2TP_OVER_IPSEC		= 0x00000004,
	CS_VPN_OFFLOAD_MAX	 	        = 0x00000007	/* the max. value when all flags are set */
} cs_vpn_offload_mode_e;
#endif
void cs_hw_accel_mgr_register_proc_callback(unsigned long mod_type,
			void (*function)(unsigned long,unsigned long));
void cs_hw_accel_mgr_delete_flow_based_hash_entry(void);
u32 cs_accel_kernel_module_enable(u32 mod_type);
void cs_accel_kernel_module_enable_set(u32 mod_type, bool mode);
u32 cs_accel_hw_accel_enable(u32 mod_mask);
u32 cs_accel_fastnet_enable(u32 mod_mask);
u32 cs_accel_kernel_enable(void);

int cs_core_logic_add_l4_port_fwd_hash(u16 port, u8 ip_prot);
int cs_core_logic_del_l4_port_fwd_hash(u16 port, u8 ip_prot);

#endif /* CS752X_HW_ACCEL_MANAGER_H */
