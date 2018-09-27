/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *				CH Hsu <ch.hsu@cortina-systems.com>
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
/*
 *
 *
 * This file contains the implementation for CS Forwarding Engine Offload
 * Kernel Module.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>	/*read_proc_t */
#include <linux/stat.h>
#include <asm/uaccess.h>	/* copy_*_user */

#if 0
#include "cs_hw_accel_forward.h"
#include "cs_hw_accel_bridge.h"
#include "cs_hw_accel_qos.h"
#include "cs_hw_accel_pppoe.h"
#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
#include "cs_hw_accel_ipsec.h"
#endif
#endif

#include "cs_hw_accel_manager.h"	/* local definitions */
#include "cs_core_hmu.h"
#include "cs_core_vtable.h"
#include "../fe/include/cs_fe_table_api.h"
#include "../fe/include/cs_fe_head_table.h"
#include "cs_core_logic_data.h"
#include "cs_core_fastnet.h"
#include "cs_hw_accel_forward.h"
#include "cs_hw_nf_drop.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
#endif	/* CONFIG_CS752X_PROC */

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
#include "cs_hw_accel_ipsec.h"
#define CS752X_NODE_IPSEC_SADB_TABLE	"ipsec_sadb_table"
#define CS752X_NODE_IPSEC_PE_SADB_TABLE	"ipsec_pe_sadb_table"
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
#include "cs_hw_accel_tunnel.h"
#define CS_NODE_IPLIP_PE_CMD		"iplip_pe_cmd"
#define CS_NODE_IPLIP_KERN_CMD		"iplip_kernel_cmd"
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
#include "cs_hw_accel_tunnel.h"
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
#include "cs_hw_accel_wireless.h"
#define CS_NODE_WIRELESS_HW_ACCEL_MODE		"wireless_hw_accel_mode"

#endif

#define CS752X_PATH_CS		"driver/cs752x"
#define CS752X_PATH_CS_NE	"ne"
#define CS752X_PATH_CS_MANAGER	"accel_manager"

#define CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE		"hw_accel_enable"
#define CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE		"fastnet_enable"

#define CS752X_NODE_HASH_HW_ENTRY			"hash_hw_entry"
#define CS752X_NODE_CORE_HMU_TABLE			"core_hmu_table"
#define CS752X_NODE_CORE_VTABLE				"core_vtable"
#define CS752X_NODE_HASH_TIMER_PERIOD			"hash_timer_period"
#define CS752X_NODE_HASH_DEFAULT_LIFETIME		"hash_default_lifetime"
#define CS752X_NODE_TCP_BYPASS_PORT			"tcp_bypass_port"
#define CS752X_NODE_UDP_BYPASS_PORT			"udp_bypass_port"
#define CS752X_NODE_EXPECTED_MASTER_BYPASS_PORT		"expected_master_bypass_port"
#define CS752X_NODE_NF_DROP_CHECK_TRY			"nf_drop_try"
#define CS752X_NODE_NF_DROP_CHECK_LIFE			"nf_drop_life"
#define CS752X_NODE_NF_DROP_CHECK_TABLE			"nf_drop_table"
#define CS752X_NODE_TCP_PRIORITY_PORT			"tcp_priority_port"
#define CS752X_NODE_UDP_PRIORITY_PORT			"udp_priority_port"
#define CS752X_NODE_IPSEC_OFFLOAD_MODE			"ipsec_offload_mode"
#define CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED	"udp_offload_after_established"
#define CS752X_NODE_VPN_OFFLOAD_MODE			"vpn_offload_mode"
#define CS752X_NODE_ALLOW_LOCALIN_PORT			"allow_localin_port"

u32 hmu_table_num = 0;


static void (*cs_kernel_module_ctl_callback_function[32])(
		unsigned long notify_event, unsigned long value) = { 0 };


static char *module_names[32] = {"NULL", "8021Q", "BRIDGE", "IPV4_MULTICAST",
		"IPV6_MULTICAST", "IPV4_FORWARD", "IPV6_FORWARD", "ARP", "IPSEC",
		"PPPOE_SERVER",	"PPPOE_CLIENT", "HW_ACCEL_DC", "PPPOE_KERNEL",
		"QOS_REMARK", "NF_DROP", "WFO", "TUNNEL", "HW_ACCEL_WIRELESS",
		"L2TP_CTRL", "IPSEC_CTRL", "LOCAL_IN"};

#ifdef CONFIG_CS752X_HW_ACCELERATION
u32 cs_kernel_mod_hw_accel_enable = CS752X_ADAPT_ENABLE_MAX;
#else
u32 cs_kernel_mod_hw_accel_enable = 0;
#endif

EXPORT_SYMBOL(cs_kernel_mod_hw_accel_enable);

#ifdef CONFIG_CS752X_FASTNET
u32 cs_kernel_mod_fastnet_enable = CS752X_ADAPT_ENABLE_MAX;
#else
u32 cs_kernel_mod_fastnet_enable = 0;
#endif
EXPORT_SYMBOL(cs_kernel_mod_fastnet_enable);

struct proc_dir_entry *proc_driver_cs752x_ham = NULL;
EXPORT_SYMBOL(proc_driver_cs752x_ham);

#ifdef CONFIG_CS752X_PROC

extern struct proc_dir_entry *proc_driver_cs752x;
extern struct proc_dir_entry *proc_driver_cs752x_ne;

#else

struct proc_dir_entry *proc_driver_cs752x = NULL;
struct proc_dir_entry *proc_driver_cs752x_ne = NULL;

#endif

u32 cs_ne_hash_timer_period = 0;
u32 cs_ne_default_lifetime = 0;
EXPORT_SYMBOL(cs_ne_hash_timer_period);
EXPORT_SYMBOL(cs_ne_default_lifetime);

u32 cs_hw_nf_drop_check_try = 3;
u32 cs_hw_nf_drop_check_life = 600;
EXPORT_SYMBOL(cs_hw_nf_drop_check_try);
EXPORT_SYMBOL(cs_hw_nf_drop_check_life);

u32 cs_hw_ipsec_offload_mode = IPSEC_OFFLOAD_MODE_BOTH;
EXPORT_SYMBOL(cs_hw_ipsec_offload_mode);

u32 cs_hw_udp_offload_after_established = 0;
EXPORT_SYMBOL(cs_hw_udp_offload_after_established);

u32 cs_hw_accel_wireless_mode = CS_WIRELESS_MODE_4_DEV;
EXPORT_SYMBOL(cs_hw_accel_wireless_mode);


#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) ||\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
u32 cs_vpn_offload_mode = CS_VPN_OFFLOAD_IPSEC | CS_VPN_OFFLOAD_L2TP_OVER_IPSEC;
#else
u32 cs_vpn_offload_mode = 0;
#endif
EXPORT_SYMBOL(cs_vpn_offload_mode);

extern void cs_fe_l3_result_print_counter(void);
extern void cs_fe_hash_print_counter(void);

#define CS752X_NE_ADAPT_ENABLE_HELP_MSG  "Purpose: Enable Kernel Adapt Modules \n" \
			"READ Usage:  \tcat /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"WRITE Usage: \techo [bitwise flag] > /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"\t flag 0x00000001: (Reserved)     \n" \
			"\t flag 0x00000002: Adapt Module 802.1q VLAN      \n" \
			"\t flag 0x00000004: Adapt Module Bridge	   \n" \
			"\t flag 0x00000008: Adapt Module IPv4 Multicast   \n" \
			"\t flag 0x00000010: Adapt Module IPv6 Multicast   \n" \
			"\t flag 0x00000020: Adapt Module IPv4 Forward     \n" \
			"\t flag 0x00000040: Adapt Module IPv6 Forward     \n" \
			"\t flag 0x00000080: Adapt Module Arp              \n" \
			"\t flag 0x00000100: Adapt Module IPSec            \n" \
			"\t flag 0x00000200: Adapt Module PPPoE Server (USER Mode)    \n" \
			"\t flag 0x00000400: Adapt Module PPPoE Client (USER Mode)     \n" \
			"\t flag 0x00000800: HW Accel Double check  \n" \
			"\t flag 0x00001000: Adapt Module PPPoE (KERNEL Mode)\n" \
			"\t flag 0x00002000: QoS Remark\n" \
			"\t flag 0x00004000: NF Drop\n" \
			"\t flag 0x00008000: WFO wifi offload\n" \
			"\t flag 0x00010000: Tunnel offload\n" \
			"\t flag 0x00020000: Adapt Module Wireless\n" \
			"\t flag 0x00040000: Adapt Module L2TP Ctrl Plane \n" \
			"\t flag 0x00080000: Adapt Module IPSec Ctrl Plane \n" \
			"\t flag 0x00100000: Adapt Module Local In Test Mode\n"

#define CS752X_NE_ADAPT_FASTNET_ENABLE_HELP_MSG  "Purpose: Enable Kernel Adapt Modules \n" \
			"READ Usage:  \tcat /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"WRITE Usage: \techo [bitwise flag] > /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"\t flag 0x00000001: (Reserved)     \n" \
			"\t flag 0x00000002: Adapt Module 802.1q VLAN      \n" \
			"\t flag 0x00000004: Adapt Module Bridge	   \n" \
			"\t flag 0x00000008: Adapt Module IPv4 Multicast   \n" \
			"\t flag 0x00000010: Adapt Module IPv6 Multicast   \n" \
			"\t flag 0x00000020: Adapt Module IPv4 Forward     \n" \
			"\t flag 0x00000040: Adapt Module IPv6 Forward     \n" \
			"\t flag 0x00000080: Adapt Module Arp              \n" \
			"\t flag 0x00000100: Adapt Module IPSec            \n" \
			"\t flag 0x00000200: (Reserved)                    \n" \
			"\t flag 0x00000400: Adapt Module PPPoE Client     \n" \
			"\t flag 0x00000800: (Reserved)                    \n" \
			"\t flag 0x00001000: Adapt Module PPPoE (KERNEL Mode)\n" \
			"\t flag 0x00002000: QoS Remark\n"


#define CS752X_NE_HASH_HW_ENTRY_READ_HELP_MSG  \
			"Usage:  \techo help > /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"\t Echo any STRING (no number) to this proc entry for detailed help.\n\n"

#define CS752X_NE_HASH_HW_ENTRY_WRITE_HELP_MSG  "Purpose: Clear/get hash hw entry list\n" \
			"READ Usage:  \tcat /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"\t Print out hash HW entries information from start index\n"\
			"WRITE Usage: \techo [value] > /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"\t 0:  Clear all (flow-based + rule-based) HW entries\n" \
			"\t 1:  Clear flow-based HW entries\n" \
			"\t \"table_type idx n\":\n" \
			"\t     Dump the table `table_type' from index `idx' to `idx + n'\n" \
			"\t     Example: echo \"8 0 3\" > %s\n" \


#define CS752X_NE_HASH_TIMER_PERIOD_HELP_MSG "Hash Timer Scan Period\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value is in the unit of second and must be larger than 0\n"

#define CS752X_NE_HASH_DEFAULT_LIFETIME_HELP_MSG "Hash Default Life Time\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value is in the unit of second. If it is 0, it means " \
			"infinite life time.\n"

#define CS752X_NE_NF_DROP_CHECK_TRY_HELP_MSG "NF_DROP Check Try\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value is in the unit of tries. If it is 0, it means " \
			"no try at all.\n"

#define CS752X_NE_NF_DROP_CHECK_LIFE_HELP_MSG "NF_DROP Check Entry life\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value is in the unit of second. If it is 0, it means " \
			"no cleanup at all.\n"

#define CS752X_NE_IPSEC_OFFLOAD_MODE_HELP_MSG "IPSec Offload Mode\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"1: ipsec offload pe#0 encryption, pe#1 decryption \n" \
			"2: ipsec offload pe#0 encryption and decryption \n" \
			"3: ipsec offload pe#1 encryption and decryption \n"

#define CS752X_NE_UDP_OFFLOAD_AFTER_ESTABLISHED  "Purpose: create hash when UDP conntrack is under ASSURED state \n" \
			"READ Usage:  \tcat /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"WRITE Usage: \techo [bitwise flag] > /proc/driver/cs752x/ne/accel_manager/%s\n" \
			"\t 0: Disable: not check UDP nf conntrack status when creating hash \n" \
			"\t 1: Enable:  create hash when UDP conntrack is under ASSURED state\n"

#define CS_VPN_OFFLOAD_MODE_HELP_MSG "VPN Offload Mode\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"flag 0x1: IPSec offload in dual PEs\n" \
			"flag 0x2: L2TP offload in dual PEs\n" \
			"flag 0x4: L2TP over IPSec offload in dual PEs\n"


void cs_hw_accel_mgr_delete_flow_based_hash_entry()
{
	int i, j;
	/*need to invalid all flow-based hash entry */
	for (i = CS752X_ADAPT_ENABLE_8021Q, j = 1;
		 i < CS752X_ADAPT_ENABLE_MAX; i = i<<1, j++ ) {
		if (cs_kernel_module_ctl_callback_function[j] != NULL)
			cs_kernel_module_ctl_callback_function[j](
					CS_HAM_ACTION_CLEAN_HASH_ENTRY, 0);
	}
}
EXPORT_SYMBOL(cs_hw_accel_mgr_delete_flow_based_hash_entry);    //BUG#39672: 2. QoS

/* file handler for cs_ne_hash_timer_period */
static int cs_proc_node_hash_timer_period_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_HASH_TIMER_PERIOD_HELP_MSG,
			CS752X_NODE_HASH_TIMER_PERIOD,
			CS752X_NODE_HASH_TIMER_PERIOD);
	len += sprintf(buf + len, "\n%s = %d\n", CS752X_NODE_HASH_TIMER_PERIOD,
			cs_ne_hash_timer_period);
	*eof = 1;
	return len;
}

static int cs_proc_node_hash_timer_period_write(struct file *file,
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

	if (mask == 0)
		goto HW_ACCEL_INVAL_EXIT;

	cs_ne_hash_timer_period = mask;
	printk(KERN_WARNING "\nSet %s as %d\n", CS752X_NODE_HASH_TIMER_PERIOD,
			cs_ne_hash_timer_period);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_HASH_TIMER_PERIOD_HELP_MSG,
			CS752X_NODE_HASH_TIMER_PERIOD,
			CS752X_NODE_HASH_TIMER_PERIOD);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* file handler for cs_ne_default_lifetime */
static int cs_proc_node_hash_default_lifetime_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_HASH_DEFAULT_LIFETIME_HELP_MSG,
			CS752X_NODE_HASH_DEFAULT_LIFETIME,
			CS752X_NODE_HASH_DEFAULT_LIFETIME);
	len += sprintf(buf + len, "\n%s = %d\n",
			CS752X_NODE_HASH_DEFAULT_LIFETIME,
			cs_ne_default_lifetime);
	*eof = 1;
	return len;
}

static int cs_proc_node_hash_default_lifetime_write(struct file *file,
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

	cs_ne_default_lifetime = mask;
	printk(KERN_WARNING "\nSet %s as %d\n",
			CS752X_NODE_HASH_DEFAULT_LIFETIME,
			cs_ne_default_lifetime);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_HASH_DEFAULT_LIFETIME_HELP_MSG,
			CS752X_NODE_HASH_DEFAULT_LIFETIME,
			CS752X_NODE_HASH_DEFAULT_LIFETIME);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}

u32 cs_accel_kernel_enable(void)
{
	return cs_kernel_mod_hw_accel_enable | cs_kernel_mod_fastnet_enable;
}

u32 cs_accel_kernel_module_enable(u32 mod_type)
{
	return ((cs_kernel_mod_hw_accel_enable | cs_kernel_mod_fastnet_enable) &
			mod_type);
}

void cs_accel_kernel_module_enable_set(u32 mod_type, bool mode)
{
	if (mod_type > CS752X_ADAPT_ENABLE_MAX);
		return;

	if (mode > 0)
		cs_kernel_mod_hw_accel_enable |= mod_type;
	else
		cs_kernel_mod_hw_accel_enable &= ~mod_type;
	return;
}

u32 __cs_accel_app_enable(u32 mod_mask, u32 enable_value)
{
	if ((mod_mask & CS_MOD_MASK_VLAN) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_8021Q)))
		return 0;

	if ((mod_mask & CS_MOD_MASK_BRIDGE) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_BRIDGE)))
		return 0;

	if ((mod_mask & CS_MOD_MASK_IPV4_MULTICAST) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_IPV4_MULTICAST)))
		return 0;

	if ((mod_mask & CS_MOD_MASK_IPV6_MULTICAST) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_IPV6_MULTICAST)))
		return 0;

	if ((mod_mask & CS_MOD_MASK_NAT) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_IPV4_FORWARD)))
		return 0;

	if ((mod_mask & CS_MOD_MASK_IPV6_ROUTING) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_IPV6_FORWARD)))
		return 0;

	if ((mod_mask & CS_MOD_MASK_IPSEC) &&
			((!(enable_value & CS752X_ADAPT_ENABLE_IPSEC)) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_IPSEC_CTRL))))
		return 0;

	if ((mod_mask & CS_MOD_MASK_L2TP) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_L2TP_CTRL)))
		return 0;

	// FIXME!! Please revise the following logic, it doesn't make sense
	if (mod_mask & CS_MOD_MASK_PPPOE)
		if (((enable_value & CS752X_ADAPT_ENABLE_PPPOE_SERVER) ||
		  	 (enable_value & CS752X_ADAPT_ENABLE_PPPOE_CLIENT) ||
		  	 (enable_value & CS752X_ADAPT_ENABLE_PPPOE_KERNEL)) == 0)
			return 0;

	if ((mod_mask & CS_MOD_MASK_QOS_FIELD_CHANGE) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_QoS_REMARK)))
			return 0;

	if ((mod_mask & CS_MOD_MASK_WFO) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_WFO)))
			return 0;

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	if ((mod_mask & CS_MOD_MASK_WIRELESS) &&
			(!(enable_value & CS752X_ADAPT_ENABLE_WIRELESS)))
			return 0;
#endif

	return 1;
}

u32 cs_accel_hw_accel_enable(u32 mod_mask)
{
	return __cs_accel_app_enable(mod_mask, cs_kernel_mod_hw_accel_enable);
}

u32 cs_accel_fastnet_enable(u32 mod_mask)
{
	return __cs_accel_app_enable(mod_mask, cs_kernel_mod_fastnet_enable);
}

int cs_accel_kernel_enable_notification(u32 old_value, u32 new_value)
{
	int i, j;
	/*rule check */

	int adapt_enable = cs_accel_kernel_enable();

	/*rule1: pppoe server and client cannot enable at the same time */
	if ((adapt_enable & CS752X_ADAPT_ENABLE_PPPOE_SERVER) &&
			(adapt_enable & CS752X_ADAPT_ENABLE_PPPOE_CLIENT)) {
		printk(KERN_WARNING "ERR - pppoe server and client cannot "
				"enable at the same time\n");
		return -1;
	}
	/*rule2: WFO and IPSec cannot enable at the same time */
	if ((adapt_enable & CS752X_ADAPT_ENABLE_WFO) &&
			(adapt_enable & CS752X_ADAPT_ENABLE_IPSEC)) {
		if (cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH) {
			printk(KERN_WARNING "ERR - IPSec and WFO cannot "
				"enable at the same time and ipsec_offload_mode=%d\n", cs_hw_ipsec_offload_mode);

			return -1;
		}
	}

	/*rule3: Tunnel (PE#1) and IPSec cannot enable at the same time */
	if ((adapt_enable & CS752X_ADAPT_ENABLE_TUNNEL) &&
			(adapt_enable & CS752X_ADAPT_ENABLE_IPSEC)) {
		printk(KERN_WARNING "ERR - Tunnel and IPSec cannot "
				"enable at the same time\n");

		return -1;
	}

#if 0	/* Remove rule#4 to support concurrent WFO and WIRELESS_ADAPT */
	/*rule4: WFO and WIRELESS cannot enable at the same time */
	if ((adapt_enable & CS752X_ADAPT_ENABLE_WFO) &&
			(adapt_enable & CS752X_ADAPT_ENABLE_WIRELESS)) {
		printk(KERN_WARNING "ERR - WFO and HW_ACCEL_WIRELESS cannot "
				"enable at the same time\n");

		return -1;
	}
#endif
	/*rule5: L2TP/IPSec ctrl plane is depended on Tunnel */
	if (((adapt_enable & CS752X_ADAPT_ENABLE_L2TP_CTRL) || \
			(adapt_enable & CS752X_ADAPT_ENABLE_IPSEC_CTRL)) &&
			!(adapt_enable & CS752X_ADAPT_ENABLE_TUNNEL)) {
		printk(KERN_WARNING "ERR - L2TP and IPSec control planes are "
				"depended on Tunnel\n");

		return -1;
	}

	if (old_value == new_value)
		return -1;

	for (i = CS752X_ADAPT_ENABLE_8021Q, j = 1;
			i < CS752X_ADAPT_ENABLE_MAX; i = i << 1, j++ ) {
		if ((old_value & i) != (new_value & i)) {
			printk(KERN_WARNING "%s kernel module %s(0x%04X) \n",
				(new_value & i) ? "enable" : "disable",
				module_names[j], i);
			if (cs_kernel_module_ctl_callback_function[j] != NULL)
				cs_kernel_module_ctl_callback_function[j](
					(new_value & i) ?
					CS_HAM_ACTION_MODULE_ENABLE :
					CS_HAM_ACTION_MODULE_DISABLE,
					new_value);
			else
				printk(KERN_WARNING "\tno callback function\n");
		}
	}
	/*
	 * when adapt_enable == 0, then remove kernel adapt module hook
	 */
	if (adapt_enable == 0) {
		printk(KERN_WARNING "delete kernel module hooks\n");
		for (i = CS752X_ADAPT_ENABLE_8021Q, j = 1;
			i < CS752X_ADAPT_ENABLE_MAX; i = i << 1, j++ ) {
			if (cs_kernel_module_ctl_callback_function[j] != NULL)
				cs_kernel_module_ctl_callback_function[j](
					CS_HAM_ACTION_MODULE_REMOVE,
					0);
		}
	} else if (((cs_kernel_mod_hw_accel_enable == 0) ||
		(cs_kernel_mod_fastnet_enable == 0)) && (old_value == 0)) {
		/*
	 	 * when adapt_enable != 0 and old_value == 0,
	 	 * then insert kernel adapt module hook
		 */
			printk(KERN_WARNING "insert kernel module hooks\n");
			for (i = CS752X_ADAPT_ENABLE_8021Q, j = 1;
				i < CS752X_ADAPT_ENABLE_MAX; i = i << 1, j++ ) {
				if (cs_kernel_module_ctl_callback_function[j] != NULL)
					cs_kernel_module_ctl_callback_function[j](
						CS_HAM_ACTION_MODULE_INSERT,
						0);
			}

	}

	return 0;
}

static int cs_proc_node_kernel_mod_hw_accel_enable_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_ADAPT_ENABLE_HELP_MSG,
			CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE,
			CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n",
			CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE,
			cs_kernel_mod_hw_accel_enable);
	*eof = 1;

	return len;
}

static int cs_proc_node_kernel_mod_hw_accel_enable_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	unsigned long new_value, old_value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &new_value))
		goto NI_INVAL_EXIT;

	old_value = cs_kernel_mod_hw_accel_enable;
	cs_kernel_mod_hw_accel_enable = new_value;

	if (cs_accel_kernel_enable_notification(old_value, new_value) != 0) {
		cs_kernel_mod_hw_accel_enable = old_value;
		goto NI_INVAL_EXIT;
	}

	printk(KERN_WARNING "Set %s as 0x%08x\n",
			CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE,
			cs_kernel_mod_hw_accel_enable);

	printk(KERN_WARNING "Set accel kernel enable status as 0x%08lx\n",
			new_value);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_ADAPT_ENABLE_HELP_MSG,
			CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE,
			CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_proc_node_allow_localin_port_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_forward_port_list_dump(ALLOW_LOCALIN_TUPE);
	return len;
}

static int cs_proc_node_allow_localin_port_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0)
		cs_forward_port_list_add(ALLOW_LOCALIN_TUPE, value);
	else
		cs_forward_port_list_del(ALLOW_LOCALIN_TUPE, -value);

NI_INVAL_EXIT:
	return count;
}

u32 acl_indx;
u32 tr143_acl_enable = 0;
u32 tr143_acl_port = 0;
u32 tr143_acl_protocol = 0;
u32 tr143_acl_ip_da = 0;
static int cs_proc_acl_add_read(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;

	len += sprintf(buf + len, "%d\n",
			acl_indx);
	*eof = 1;
	return len;
}

static int cs_proc_acl_add_write(struct file *file, const char *buffer,
				  unsigned long count, void *data)
{
	char buf[128];
	unsigned long mask;
	unsigned long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	printk("%s %d len %d \n", __func__, __LINE__, len);
	if (copy_from_user(buf, buffer, len))
		goto FE_INVAL_EXIT;

	buf[len] = '\0';
	printk("%s %d \n", __func__, __LINE__);

	char *pch = NULL, *pch2 = NULL;
	int action= 0; /*0: add 1: delete*/
	int idx= 0;
	cs_acl_entry_t entry;
	entry.proto = 0x11;
	entry.src_port_lo = 0;
	entry.src_port_hi= 65535;
	entry.dst_port_lo = 123;
	entry.dst_port_hi = 123;
	/*
	 * action
	 * 0: add acl
	 * 1: dele acl
	 * 3: acl enable
	 * else: acl disable
	 */
	entry.action_flow_id = 1;

	if ((pch = strstr(buf, " ")) != NULL) {
			value = -1;
			*pch = '\0';
			pch++;
			printk("%s %d %p %p \n", __func__, __LINE__, buf, pch);
			if ((pch2 = strstr(pch, " ")) != NULL) {
				*pch2 = '\0';
				pch2++;
			}
			printk("%s %d %p\n", __func__, __LINE__, pch2);

			if (strict_strtoul(pch, 0, &value)) {
				printk("%s %d \n", __func__, __LINE__);
				return count;
			}
			action = value;
			printk("%s %d value %d\n", __func__, __LINE__, value);
			if ((pch = strstr(pch2, " ")) != NULL) {
					*pch = '\0';
					pch++;
			}

			if (strict_strtoul(pch2, 0, &value))
			{
				printk("%s %d \n", __func__, __LINE__);
				return count;
			}
			idx = value;

			if (action == 0) {

				if ((pch2 = strstr(pch, " ")) != NULL) {
					*pch2 = '\0';
					pch2++;
				}
				if (strict_strtoul(pch, 0, &value))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.src_port_lo = value;
				if ((pch = strstr(pch2, " ")) != NULL) {
					*pch = '\0';
					pch++;
				}

				if (strict_strtoul(pch2, 0, &value))
					goto FE_INVAL_EXIT;
				entry.src_port_hi = value;


				if ((pch2 = strstr(pch, " ")) != NULL) {
					*pch2 = '\0';
					pch2++;
				}
				if (strict_strtoul(pch, 0, &value))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.dst_port_lo = value;
				if ((pch = strstr(pch2, " ")) != NULL) {
					*pch = '\0';
					pch++;
				}

				if (strict_strtoul(pch2, 0, &value))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.dst_port_hi = value;

				if ((pch2 = strstr(pch, " ")) != NULL) {
					*pch2 = '\0';
					pch2++;
				}
				if (strict_strtoul(pch, 0, &value))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.proto = value;


				if ((pch = strstr(pch2, " ")) != NULL) {
						*pch = '\0';
						pch++;
				}
				if (strict_strtoul(pch2, 0, &value))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.action_dst_port = value;

				if ((pch2 = strstr(pch, " ")) != NULL) {
					*pch2 = '\0';
					pch2++;
				}
				if (strict_strtoul(pch, 0, &value))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.action_dst_voq = value;
				entry.action_flow_id = idx;
				/*pch2*/
				if ((pch = strstr(pch2, " ")) != NULL) {
					*pch = '\0';
					pch++;
				}
				if (strict_strtoul(pch2, 0, &entry.ip_da[0]))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}

				/*pch1*/
				if ((pch2 = strstr(pch, " ")) != NULL) {
					*pch2 = '\0';
					pch2++;
				}
				if (strict_strtoul(pch, 0, &entry.ip_da[1]))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}

				/*pch2*/
				if ((pch = strstr(pch2, " ")) != NULL) {
					*pch = '\0';
					pch++;
				}
				if (strict_strtoul(pch2, 0, &entry.ip_da[2]))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				/*pch1*/
				if ((pch2 = strstr(pch, " ")) != NULL) {
					*pch2 = '\0';
					pch2++;
				}
				if (strict_strtoul(pch, 0, &entry.ip_da[3]))
				{
					printk("%s %d \n", __func__, __LINE__);
					return count;
				}
				entry.ip_ver = 0; /*ip_v4*/

				printk("%s %d action_flow_id  %d proto %d action_dst_port %d action_dst_voq %d  \n" \
					"src_port_lo %d  src_port_hi %d, \n" \
					"dst_port_lo %d, dst_port_hi %d ip_da %x %x %x %x\n", __func__, __LINE__, entry.action_flow_id,
					entry.proto, entry.action_dst_port, entry.action_dst_voq,
					entry.src_port_lo, entry.src_port_hi,
					entry.dst_port_lo, entry.dst_port_hi,
					entry.ip_da[0], entry.ip_da[1], entry.ip_da[2], entry.ip_da[3]);
				tr143_acl_port = entry.action_dst_port;
				tr143_acl_protocol = entry.proto;
				tr143_acl_ip_da = ntohl(entry.ip_da[0]);
				acl_indx = cs_acl_add(&entry);
			}else if (action == 1) {
				printk("%s %d %d\n", __func__, __LINE__, idx);
				cs_acl_del(idx);
			} else if (action == 3) {
				cs_core_vtable_set_acl_enable(CORE_VTABLE_TYPE_CPU, 1);
				cs_core_vtable_set_acl_enable(CORE_VTABLE_TYPE_CPU_L3, 1);
				tr143_acl_enable = 1;
			} else {
				cs_core_vtable_set_acl_enable(CORE_VTABLE_TYPE_CPU, 0);
				cs_core_vtable_set_acl_enable(CORE_VTABLE_TYPE_CPU_L3, 0);
				tr143_acl_enable = 0;
			}

	}
	return count;

FE_INVAL_EXIT:
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}


#define CS752X_NODE_LOCALIN_UDP_DROP_STATISTIC			"localin_udp_drop_statistic"
u64 cs_localin_udp_payload_drop_bytes = 0;
u64 cs_localin_udp_traffic_drop_bytes = 0;
u64 cs_localin_udp_drop_packets = 0;
EXPORT_SYMBOL(cs_localin_udp_payload_drop_bytes);
EXPORT_SYMBOL(cs_localin_udp_traffic_drop_bytes);
EXPORT_SYMBOL(cs_localin_udp_drop_packets);
static int cs_proc_node_localin_udp_drop_statistic_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	
	len += sprintf(buf + len, "\n UDP Data Bytes discarded = 0x%016llx\n",						
			cs_localin_udp_payload_drop_bytes);
	len += sprintf(buf + len, "\n Traffic Bytes discarded = 0x%016llx\n",						
			cs_localin_udp_traffic_drop_bytes);
	len += sprintf(buf + len, "\n Packets discarded = 0x%016llx\n",						
			cs_localin_udp_drop_packets);
	*eof = 1;
	return len;
}

static int cs_proc_node_localin_udp_drop_statistic_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value == 1) {
		/*read and clean the counter*/
		printk("\n UDP Data Bytes discarded = 0x%016llx\n",						
			cs_localin_udp_payload_drop_bytes);
		printk("\n Traffic Bytes discarded = 0x%016llx\n",						
			cs_localin_udp_traffic_drop_bytes);
		printk("\n Packets discarded = 0x%016llx\n",						
			cs_localin_udp_drop_packets);	
	} 
	cs_localin_udp_payload_drop_bytes = 0;
	cs_localin_udp_traffic_drop_bytes = 0;
	cs_localin_udp_drop_packets = 0;
	
NI_INVAL_EXIT:
	return count;
}


static int cs_proc_node_tcp_bypass_port_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_forward_port_list_dump(BYPASS_LIST_TUPE_TCP);
	return len;
}

static int cs_proc_node_tcp_bypass_port_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0)
		cs_forward_port_list_add(BYPASS_LIST_TUPE_TCP, value);
	else
		cs_forward_port_list_del(BYPASS_LIST_TUPE_TCP, -value);

NI_INVAL_EXIT:
	return count;
}

static int cs_proc_node_udp_bypass_port_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_forward_port_list_dump(BYPASS_LIST_TUPE_UDP);
	return len;
}

static int cs_proc_node_udp_bypass_port_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0)
		cs_forward_port_list_add(BYPASS_LIST_TUPE_UDP, value);
	else
		cs_forward_port_list_del(BYPASS_LIST_TUPE_UDP, -value);

NI_INVAL_EXIT:
	return count;
}

static int cs_proc_node_expected_bypass_port_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_forward_port_list_dump(BYPASS_LIST_TUPE_EXPECTED_MASTER);
	return len;
}

static int cs_proc_node_expected_bypass_port_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0)
		cs_forward_port_list_add(BYPASS_LIST_TUPE_EXPECTED_MASTER, value);
	else
		cs_forward_port_list_del(BYPASS_LIST_TUPE_EXPECTED_MASTER, -value);

NI_INVAL_EXIT:
	return count;
}

static int cs_proc_node_tcp_priority_port_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_forward_port_list_dump(PRIORITY_LIST_TUPE_TCP);
	return len;
}

static int cs_proc_node_tcp_priority_port_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;
	int ret;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0) {
		ret = cs_forward_port_list_add(PRIORITY_LIST_TUPE_TCP, value);
		if (ret == 0)
			ret = cs_core_logic_add_l4_port_fwd_hash((u16) value, SOL_TCP);
	} else {
		ret = cs_forward_port_list_del(PRIORITY_LIST_TUPE_TCP, -value);
		if (ret == 0)
			ret = cs_core_logic_del_l4_port_fwd_hash((u16) -value, SOL_TCP);
	}

NI_INVAL_EXIT:
	return count;
}

static int cs_proc_node_udp_priority_port_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_forward_port_list_dump(PRIORITY_LIST_TUPE_UDP);
	return len;
}

static int cs_proc_node_udp_priority_port_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;
	int ret;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0) {
		ret = cs_forward_port_list_add(PRIORITY_LIST_TUPE_UDP, value);
		if (ret == 0)
			ret = cs_core_logic_add_l4_port_fwd_hash((u16) value, SOL_UDP);
	} else {
		ret = cs_forward_port_list_del(PRIORITY_LIST_TUPE_UDP, -value);
		if (ret == 0)
			ret = cs_core_logic_del_l4_port_fwd_hash((u16) -value, SOL_UDP);
	}

NI_INVAL_EXIT:
	return count;
}

/* file handler for cs_nf_drop_try */
static int cs_proc_node_nf_drop_check_try_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_NF_DROP_CHECK_TRY_HELP_MSG,
			CS752X_NODE_NF_DROP_CHECK_TRY,
			CS752X_NODE_NF_DROP_CHECK_TRY);
	len += sprintf(buf + len, "\n%s = %d\n", CS752X_NODE_NF_DROP_CHECK_TRY,
			cs_hw_nf_drop_check_try);
	*eof = 1;
	return len;
}

static int cs_proc_node_nf_drop_check_try_write(struct file *file,
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

	if (mask == 0)
		goto HW_ACCEL_INVAL_EXIT;

	cs_hw_nf_drop_check_try = mask;
	printk(KERN_WARNING "\nSet %s as %d\n", CS752X_NODE_NF_DROP_CHECK_TRY,
			cs_hw_nf_drop_check_try);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_NF_DROP_CHECK_TRY_HELP_MSG,
			CS752X_NODE_NF_DROP_CHECK_TRY,
			CS752X_NODE_NF_DROP_CHECK_TRY);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}

/* file handler for cs_nf_drop_life */
static int cs_proc_node_nf_drop_check_life_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_NF_DROP_CHECK_LIFE_HELP_MSG,
			CS752X_NODE_NF_DROP_CHECK_LIFE,
			CS752X_NODE_NF_DROP_CHECK_LIFE);
	len += sprintf(buf + len, "\n%s = %d\n",
			CS752X_NODE_NF_DROP_CHECK_LIFE,
			cs_hw_nf_drop_check_life);
	*eof = 1;
	return len;
}

static int cs_proc_node_nf_drop_check_life_write(struct file *file,
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

	if (mask == 0)
		goto HW_ACCEL_INVAL_EXIT;

	cs_hw_nf_drop_check_life = mask;
	printk(KERN_WARNING "\nSet %s as %d\n",
			CS752X_NODE_NF_DROP_CHECK_LIFE,
			cs_hw_nf_drop_check_life);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_NF_DROP_CHECK_LIFE_HELP_MSG,
			CS752X_NODE_NF_DROP_CHECK_LIFE,
			CS752X_NODE_NF_DROP_CHECK_LIFE);
	return count;
	/* if we return error code here, PROC fs may reperiod up to 3 times. */
}

static int cs_proc_node_nf_drop_check_table_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_hw_nf_drop_check_table_dump();
	return len;
}

static int cs_proc_node_nf_drop_check_table_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;
	if (value > 0)
		cs_hw_nf_drop_check_table_clean();
	else
		printk(KERN_WARNING "\necho [1+] > /proc/driver/cs752x/ne"
				"/accel_manager/nf_drop_check_table to clean "
				"the existing table.");

NI_INVAL_EXIT:
	return count;
}


#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
static int cs_proc_node_ipsec_sadb_table_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_hw_accel_ipsec_print_sadb_status();
	return len;
}

static int cs_proc_node_ipsec_sadb_table_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	cs_hw_accel_ipsec_clean_sadb_status();

	return count;
}

static int cs_proc_node_ipsec_pe_sadb_table_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	*eof = 1;
	cs_hw_accel_ipsec_print_pe_sadb_status();
	return len;
}

static int cs_proc_node_ipsec_pe_sadb_table_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	cs_hw_accel_ipsec_clean_pe_sadb_status();

	return count;
}
#endif

#ifdef CONFIG_CS752X_FASTNET
static int cs_proc_node_kernel_mod_fastnet_enable_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_ADAPT_FASTNET_ENABLE_HELP_MSG,
			CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE,
			CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE);
	len += sprintf(buf + len, "\n%s = 0x%08x\n",
			CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE,
			cs_kernel_mod_fastnet_enable);
	*eof = 1;

	return len;
}

static int cs_proc_node_kernel_mod_fastnet_enable_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	u32 old_value;
	unsigned long new_value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &new_value))
		goto NI_INVAL_EXIT;
	if (new_value & CS752X_ADAPT_ENABLE_HW_ACCEL_DC)
		goto NI_INVAL_EXIT;

	if (new_value & CS752X_ADAPT_ENABLE_ARP)
		goto NI_INVAL_EXIT;

	old_value = cs_kernel_mod_fastnet_enable;
	cs_kernel_mod_fastnet_enable = (u32)new_value;

	if (cs_accel_kernel_enable_notification(old_value, (u32)new_value)
			!= 0) {
		cs_kernel_mod_fastnet_enable = old_value;
		goto NI_INVAL_EXIT;
	}

	printk(KERN_WARNING "Set %s as 0x%08x\n",
			CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE,
			cs_kernel_mod_fastnet_enable);

	printk(KERN_WARNING "Set accel kernel enable status as 0x%08x\n",
			(u32)new_value);

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_ADAPT_FASTNET_ENABLE_HELP_MSG,
			CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE,
			CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}
#endif

static int cs_proc_node_hash_hw_entry_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	printk(KERN_WARNING CS752X_NE_HASH_HW_ENTRY_READ_HELP_MSG,
			CS752X_NODE_HASH_HW_ENTRY);

	printk("L2 MAC table used count :");
	cs_fe_table_print_table_used_count(FE_TABLE_L2_MAC);
	printk("-------------------------------------------------\n");
	printk("L3 IP  table used count :");
	cs_fe_table_print_table_used_count(FE_TABLE_L3_IP);
	cs_fe_l3_result_print_counter();
	printk("-------------------------------------------------\n");
	printk("FwdRslt table used count :");
	cs_fe_table_print_table_used_count(FE_TABLE_FWDRSLT);
	printk("-------------------------------------------------\n");
	printk("Hash Hash table used count :");
	cs_fe_table_print_table_used_count(FE_TABLE_HASH_HASH);
	cs_fe_hash_print_counter();
	printk("-------------------------------------------------\n");
	printk("Hash Overflow table used count :");
	cs_fe_table_print_table_used_count(FE_TABLE_HASH_OVERFLOW);
	printk("-------------------------------------------------\n");
	printk("QoS Rslt table used count :");
	cs_fe_table_print_table_used_count(FE_TABLE_QOSRSLT);
	printk("\n");

	printk("-------------------------------------------------\n");
	printk("FastNet table used count :");
	cs_core_fastnet_print_table_used_count();
	printk("\n");

	/*FIXME: to print out hash entry by cs_ham_hw_entry_idx, cs_ham_hw_entry_number*/

	*eof = 1;
	return len;
}

static int cs_proc_node_hash_hw_entry_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	unsigned long value;
	ssize_t len;
	char *pch = NULL, *pch2 = NULL;
	u32 cs_ham_hw_entry_table_type = 0;
	u32 cs_ham_hw_entry_idx = 0;
	u32 cs_ham_hw_entry_number = 0;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
//	printk(KERN_DEBUG "get %s \n",buf);

	if ((pch = strstr(buf, " ")) != NULL) {
		value = -1;
		*pch = '\0';
		pch++;
		if ((pch2 = strstr(pch, " ")) != NULL) {
			*pch2 = '\0';
			pch2++;
		}
		if (strict_strtoul(buf, 0, &value))
			goto NI_INVAL_EXIT;
		cs_ham_hw_entry_table_type = value;

		if (strict_strtoul(pch, 0, &value))
			goto NI_INVAL_EXIT;
		cs_ham_hw_entry_idx = value;

		if (strict_strtoul(pch2, 0, &value))
			goto NI_INVAL_EXIT;
		cs_ham_hw_entry_number = value;
	} else if (strict_strtoul(buf, 0, &value)) {
		goto NI_INVAL_EXIT;
	}

	if (pch == NULL) {
		if (value == 0) {
			//cs_core_hmu_delete_all_hash();
			printk(KERN_WARNING "%s Doesn't support 0 setup!\n",
				CS752X_NODE_HASH_HW_ENTRY);
		} else if (value == 1) {
			/*need to invalid all flow-based hash entry */
			cs_hw_accel_mgr_delete_flow_based_hash_entry();
		}
		printk(KERN_WARNING "Set %s as 0x%08lx\n",
				CS752X_NODE_HASH_HW_ENTRY, value);
	} else {
		printk(KERN_WARNING "Set read hash HW entry table type =%d "
				"idx=%d number=%d\n",
				cs_ham_hw_entry_table_type, cs_ham_hw_entry_idx,
				cs_ham_hw_entry_number);
		cs_fe_table_print_range(cs_ham_hw_entry_table_type,
				cs_ham_hw_entry_idx,
				cs_ham_hw_entry_idx + cs_ham_hw_entry_number);
	}

	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_HASH_HW_ENTRY_WRITE_HELP_MSG,
			CS752X_NODE_HASH_HW_ENTRY,
			CS752X_NODE_HASH_HW_ENTRY,
			CS752X_NODE_HASH_HW_ENTRY);
	printk("\ttable_type:\n");
	printk("\t FE_TABLE_AN_BNG_MAC = %d\n", FE_TABLE_AN_BNG_MAC);
	printk("\t FE_TABLE_PORT_RANGE = %d\n", FE_TABLE_PORT_RANGE);
	printk("\t FE_TABLE_ETYPE = %d\n", FE_TABLE_ETYPE);
	printk("\t FE_TABLE_LLC_HDR = %d\n", FE_TABLE_LLC_HDR);
	printk("\t FE_TABLE_LPB = %d\n", FE_TABLE_LPB);
	printk("\t FE_TABLE_CLASS = %d\n", FE_TABLE_CLASS);
	printk("\t FE_TABLE_SDB = %d\n", FE_TABLE_SDB);
	printk("\t FE_TABLE_VLAN = %d\n", FE_TABLE_VLAN);
	printk("\t FE_TABLE_FWDRSLT = %d\n", FE_TABLE_FWDRSLT);
	printk("\t FE_TABLE_QOSRSLT = %d\n", FE_TABLE_QOSRSLT);
	printk("\t FE_TABLE_VOQ_POLICER = %d\n", FE_TABLE_VOQ_POLICER);
	printk("\t FE_TABLE_FVLAN = %d\n", FE_TABLE_FVLAN);
	printk("\t FE_TABLE_L3_IP = %d\n", FE_TABLE_L3_IP);
	printk("\t FE_TABLE_L2_MAC = %d\n", FE_TABLE_L2_MAC);
	printk("\t FE_TABLE_ACL_RULE = %d\n", FE_TABLE_ACL_RULE);
	printk("\t FE_TABLE_ACL_ACTION = %d\n", FE_TABLE_ACL_ACTION);
	printk("\t FE_TABLE_PE_VOQ_DROP = %d\n", FE_TABLE_PE_VOQ_DROP);
	printk("\t FE_TABLE_LPM_LPMTBL0_UPPER = %d\n", FE_TABLE_LPM_LPMTBL0_UPPER);
	printk("\t FE_TABLE_LPM_HCTBL0_UPPER = %d\n", FE_TABLE_LPM_HCTBL0_UPPER);
	printk("\t FE_TABLE_LPM_LPMTBL0_LOWER = %d\n", FE_TABLE_LPM_LPMTBL0_LOWER);
	printk("\t FE_TABLE_LPM_HCTBL0_LOWER = %d\n", FE_TABLE_LPM_HCTBL0_LOWER);
	printk("\t FE_TABLE_LPM_LPMTBL1_UPPER = %d\n", FE_TABLE_LPM_LPMTBL1_UPPER);
	printk("\t FE_TABLE_LPM_HCTBL1_UPPER = %d\n", FE_TABLE_LPM_HCTBL1_UPPER);
	printk("\t FE_TABLE_LPM_LPMTBL1_LOWER = %d\n", FE_TABLE_LPM_LPMTBL1_LOWER);
	printk("\t FE_TABLE_LPM_HCTBL1_LOWER = %d\n", FE_TABLE_LPM_HCTBL1_LOWER);
	printk("\t FE_TABLE_HASH_HASH = %d\n", FE_TABLE_HASH_HASH);
	printk("\t FE_TABLE_HASH_OVERFLOW = %d\n", FE_TABLE_HASH_OVERFLOW);
	printk("\t FE_TABLE_HASH_STATUS = %d\n", FE_TABLE_HASH_STATUS);
	printk("\t FE_TABLE_HASH_MASK = %d\n", FE_TABLE_HASH_MASK);
	printk("\t FE_TABLE_HASH_DBG_FIFO = %d\n", FE_TABLE_HASH_DBG_FIFO);
	printk("\t FE_TABLE_HASH_CHECK_MEM = %d\n", FE_TABLE_HASH_CHECK_MEM);
	printk("\t FE_TABLE_PKTLEN_RANGE = %d\n", FE_TABLE_PKTLEN_RANGE);
	printk("\n");
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_proc_node_core_hmu_tbl_read(char *buf, char **start, off_t offset,
					  int count, int *eof, void *data)
{
	u32 len = 0;
	if (hmu_table_num != 0)
		cs_core_hmu_dump_table(hmu_table_num);
	else
		cs_core_hmu_dump_all_table();
	*eof = 1;
	return len;
}

static int cs_proc_node_core_hmu_tbl_write(struct file *file,
	const char *buffer, unsigned long count, void *data){
	char buf[32];
	unsigned long value;
	ssize_t len;
	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;
	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &value))
		goto NI_INVAL_EXIT;
	hmu_table_num = value;
	printk(KERN_WARNING "set table num to %ld\n", value);
	return count;
NI_INVAL_EXIT:
		printk(KERN_WARNING "Invalid argument\n");
		return count;
}

static int cs_proc_node_core_vtbl_read(char *buf, char **start, off_t offset,
					  int count, int *eof, void *data)
{
	u32 len = 0;

	cs_core_vtable_dump();
	*eof = 1;
	return len;
}

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
/* file handler for cs_ne_ipsec_offload_mode*/
static int cs_proc_node_ipsec_offload_mode_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS752X_NE_IPSEC_OFFLOAD_MODE_HELP_MSG,
			CS752X_NODE_IPSEC_OFFLOAD_MODE,
			CS752X_NODE_IPSEC_OFFLOAD_MODE);
	len += sprintf(buf + len, "\n%s = %d\n",
			CS752X_NODE_IPSEC_OFFLOAD_MODE,
			cs_hw_ipsec_offload_mode);
	*eof = 1;
	return len;
}

static int cs_proc_node_ipsec_offload_mode_write(struct file *file,
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

	if ((mask < IPSEC_OFFLOAD_MODE_BOTH) || (mask > IPSEC_OFFLOAD_MODE_PE1))
		goto HW_ACCEL_INVAL_EXIT;
	if ((mask == IPSEC_OFFLOAD_MODE_BOTH)
		&& (cs_kernel_mod_hw_accel_enable & CS752X_ADAPT_ENABLE_WFO)) {
		printk(KERN_WARNING "hw_accel_enable is enable WFO and \
			cannot enable IPSec Offload mode %d\n", IPSEC_OFFLOAD_MODE_BOTH);
		goto HW_ACCEL_INVAL_EXIT;
	}

	cs_hw_ipsec_offload_mode = mask;
	printk(KERN_WARNING "\nSet %s as %d\n",
			CS752X_NODE_IPSEC_OFFLOAD_MODE,
			cs_hw_ipsec_offload_mode);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS752X_NE_IPSEC_OFFLOAD_MODE_HELP_MSG,
			CS752X_NODE_IPSEC_OFFLOAD_MODE,
			CS752X_NODE_IPSEC_OFFLOAD_MODE);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}
#endif /* CONFIG_CS752X_HW_ACCELERATION_IPSEC */

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
static int cs_proc_node_iplip_pe_cmd_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, "IPv6 over PPP over L2TP over IPv4 over PPPoE"
		" (IPLIP) PE command usage:\n"
		"\t%s [cmd] ([index])\n"
		" cmd:\n"
		" 1             : reset\n"
		" 2             : stop\n"
		" 3 [index]     : set entry\n"
		" 4 [index]     : delete entry\n"
		" 5             : dump table\n"
		" 6             : echo\n"
		" 7 [0|1]       : enable(1)/disable(0) MIB\n",
		CS_NODE_IPLIP_PE_CMD);

	*eof = 1;
	return len;
}

static int cs_proc_node_iplip_pe_cmd_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	char *token_list[2];
	int tok_cnt = 0;
	ssize_t len;
	unsigned long cmd = 0, index = 0, i;
	cs_iplip_entry2_t iplip_entry;
	unsigned long enbl = 0; /* 0: disable, 1: enable */

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto IPLIP_INVAL_EXIT;

	buf[len] = '\0';

	/* split each token */
	if (cs752x_str_paser(buf, 2, token_list, &tok_cnt) || tok_cnt != 2)
		goto IPLIP_INVAL_EXIT;

	/* analyze meaning of each token */


	if (strict_strtoul(token_list[0], 0, &cmd) ||
			cmd > CS_IPLIP_IPC_PE_MAX || cmd < CS_IPLIP_IPC_PE_RESET)
		goto IPLIP_INVAL_EXIT;

	/* Execute the command here */
	switch (cmd) {
#ifdef CS_IPC_ENABLED
	case CS_IPLIP_IPC_PE_RESET:
		cs_iplip_ipc_send_reset();
		break;

	case CS_IPLIP_IPC_PE_STOP:
		cs_iplip_ipc_send_stop();
		break;

	case CS_IPLIP_IPC_PE_SET_ENTRY:
		if (strict_strtoul(token_list[1], 0, &index) ||
				index > CS_IPLIP_TBL_SIZE)
			goto IPLIP_INVAL_EXIT;

		/* create pseudo input data */
		memset(&iplip_entry, 0, sizeof(cs_iplip_entry_t));
		iplip_entry.crc32 = 0x01020304;
		iplip_entry.valid = 1;
		for (i = 0; i < sizeof(struct cs_iplip_hdr2_s); i++)
			iplip_entry.iplip_octet[i] = i;
		iplip_entry.iplip_hdr.l2tph.ver = htons(0x4002);

		cs_iplip_ipc_send_set_entry(index, &iplip_entry);
		break;

	case CS_IPLIP_IPC_PE_DEL_ENTRY:
		if (strict_strtoul(token_list[1], 0, &index) ||
				index > CS_IPLIP_TBL_SIZE)
			goto IPLIP_INVAL_EXIT;

		cs_iplip_ipc_send_del_entry(index);
		break;

	case CS_IPLIP_IPC_PE_DUMP_TBL:
		cs_iplip_ipc_send_dump();
		break;

	case CS_IPLIP_IPC_PE_ECHO:
		cs_iplip_ipc_send_echo();
		break;

	case CS_IPLIP_IPC_PE_MIB_EN:
		if (strict_strtoul(token_list[1], 0, &enbl) || enbl > 1)
			goto IPLIP_INVAL_EXIT;

		cs_iplip_ipc_send_mib_en(enbl);
		break;
#endif /* CS_IPC_ENABLED */
	default:
		goto IPLIP_INVAL_EXIT;
	}

	/* End of the execution */

	return count;

IPLIP_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_proc_node_iplip_kernel_cmd_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, "IPv6 over PPP over L2TP over IPv4 over PPPoE"
		" (IPLIP) kernel command usage:\n"
		"\t%s [cmd] ([parameters...])\n"
		" cmd:\n"
		" 1 [pppoe port ID] [pppoe ifindex]  : set pppoe ifindex\n"
		" 2 [pppoe port ID] [ppp ifindex]    : set ppp ifindex\n",
		CS_NODE_IPLIP_PE_CMD);

	*eof = 1;
	return len;
}

#define CS_IPLIP_PPPOE_IFINDEX_SET	1
#define CS_IPLIP_PPP_IFINDEX_SET	2

static int cs_proc_node_iplip_kernel_cmd_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	char *token_list[3];
	int tok_cnt = 0;
	ssize_t len;
	unsigned long cmd = 0, pppoe_port = 0, ifindex;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto IPLIP_INVAL_EXIT;

	buf[len] = '\0';

	/* split each token */
	if (cs752x_str_paser(buf, 3, token_list, &tok_cnt) || tok_cnt != 3)
		goto IPLIP_INVAL_EXIT;

	/* analyze meaning of each token */


	if (strict_strtoul(token_list[0], 0, &cmd))
		goto IPLIP_INVAL_EXIT;

	if (strict_strtoul(token_list[1], 0, &pppoe_port))
		goto IPLIP_INVAL_EXIT;

	if (strict_strtoul(token_list[2], 0, &ifindex))
		goto IPLIP_INVAL_EXIT;

	/* Execute the command here */
	switch (cmd) {
	case CS_IPLIP_PPPOE_IFINDEX_SET:
		cs_iplip_pppoe_ifindex_set(pppoe_port, ifindex);
		break;

	case CS_IPLIP_PPP_IFINDEX_SET:
		cs_iplip_ppp_ifindex_set(pppoe_port, ifindex);
		break;

	default:
		goto IPLIP_INVAL_EXIT;
	}

	/* End of the execution */

	return count;

IPLIP_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

#endif

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) ||\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
/* file handler for cs_ne_ipsec_offload_mode*/
static int cs_proc_node_vpn_offload_mode_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, CS_VPN_OFFLOAD_MODE_HELP_MSG,
			CS752X_NODE_VPN_OFFLOAD_MODE,
			CS752X_NODE_VPN_OFFLOAD_MODE);
	len += sprintf(buf + len, "\n%s = %d\n",
			CS752X_NODE_VPN_OFFLOAD_MODE,
			cs_vpn_offload_mode);
	*eof = 1;
	return len;
}

static int cs_proc_node_vpn_offload_mode_write(struct file *file,
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

	if (mask > CS_VPN_OFFLOAD_MAX)
		goto HW_ACCEL_INVAL_EXIT;

	cs_vpn_offload_mode = mask;
	printk(KERN_WARNING "\nSet %s as %d\n",
			CS752X_NODE_VPN_OFFLOAD_MODE,
			cs_vpn_offload_mode);

	return count;

HW_ACCEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_VPN_OFFLOAD_MODE_HELP_MSG,
			CS752X_NODE_VPN_OFFLOAD_MODE,
			CS752X_NODE_VPN_OFFLOAD_MODE);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}
#endif /* CONFIG_CS752X_HW_ACCELERATION_IPSEC */


static int cs_proc_node_udp_offload_after_established_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;
	len += sprintf(buf + len, CS752X_NE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED);
	len += sprintf(buf + len, "\n%s = %d\n",
			CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			cs_hw_udp_offload_after_established);
	*eof = 1;

	return len;
}

static int cs_proc_node_udp_offload_after_established_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto NI_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto NI_INVAL_EXIT;

	cs_hw_udp_offload_after_established = value;

	printk(KERN_WARNING "Set %s as 0x%08x\n",
			CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			cs_hw_udp_offload_after_established);
	return count;

NI_INVAL_EXIT:
	printk(KERN_WARNING CS752X_NE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED);
	return count;
}

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS

static int cs_proc_node_wireless_mode_read(char *buf, char **start,
		off_t offset, int count, int *eof, void *data)
{
	u32 len = 0;

	len += sprintf(buf + len, " cs_hw_accel_wireless_mode = %d support %d ssid\n",
		cs_hw_accel_wireless_mode,
		(cs_hw_accel_wireless_mode == CS_WIRELESS_MODE_4_DEV)?4:2);

	*eof = 1;

	return len;
}

static int cs_proc_node_wireless_mode_write(struct file *file,
		const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	long value;
	ssize_t len;
	int i, j;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtol(buf, 0, &value))
		goto INVAL_EXIT;

	cs_hw_accel_wireless_set_mode(value);

INVAL_EXIT:
	return count;
}
#endif

int cs_proc_node_add(char *name, read_proc_t *hook_func_read,
		write_proc_t *hook_func_write, struct proc_dir_entry *parent)
{
	struct proc_dir_entry *node;

	node = create_proc_entry(name, S_IRUGO | S_IWUGO, parent);
	if (node) {
		node->read_proc = hook_func_read;
		node->write_proc = hook_func_write;
	} else {
		printk(KERN_ERR "ERROR in creating proc entry (%s)!\n", name);
		return -EINVAL;
	}

	return 0;
}

void cs_hw_accel_mgr_register_proc_callback(unsigned long mod_type,
		void (*function) (unsigned long, unsigned long))
{
	int pos = 0;

	/*get bit position of mod_type */
	while ((pos <= 32) && (mod_type != 1)) {
		pos++;
		mod_type = mod_type >> 1;
	}

	printk("Set callback %08lx at pos %d\n", mod_type, pos);

	if (pos <= 32)
		cs_kernel_module_ctl_callback_function[pos] = function;
}

int __init cs_accel_mgr_init_module(void)
{
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_PPPOE_SERVER;
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_PPPOE_CLIENT;
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_HW_ACCEL_DC;
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_WFO;
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_NF_DROP;
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_LOCAL_IN;

	cs_kernel_mod_fastnet_enable &= ~CS752X_ADAPT_ENABLE_PPPOE_SERVER;
	cs_kernel_mod_fastnet_enable &= ~CS752X_ADAPT_ENABLE_HW_ACCEL_DC;
	cs_kernel_mod_fastnet_enable &= ~CS752X_ADAPT_ENABLE_ARP;
	cs_kernel_mod_fastnet_enable &= ~CS752X_ADAPT_ENABLE_WFO;
	/*For apply MC data plane, disable flow-based MC by default*/
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_IPV4_MULTICAST;
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_IPV6_MULTICAST;
	cs_kernel_mod_fastnet_enable &= ~CS752X_ADAPT_ENABLE_IPV4_MULTICAST;
	cs_kernel_mod_fastnet_enable &= ~CS752X_ADAPT_ENABLE_IPV6_MULTICAST;

#ifndef CONFIG_CS752X_HW_ACCELERATION_IPSEC
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_IPSEC;
#endif

#ifndef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_IPSEC_CTRL;
#endif  

#ifndef CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_L2TP_CTRL;
#endif  

#ifdef CONFIG_CS75XX_WFO
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_IPSEC;
	cs_kernel_mod_hw_accel_enable |= CS752X_ADAPT_ENABLE_WFO;
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	cs_kernel_mod_hw_accel_enable &= ~CS752X_ADAPT_ENABLE_WFO;
	cs_kernel_mod_hw_accel_enable |= CS752X_ADAPT_ENABLE_WIRELESS;
#endif
	if (proc_driver_cs752x == NULL)
		proc_driver_cs752x = proc_mkdir(CS752X_PATH_CS, NULL);
	else
		atomic_inc(&proc_driver_cs752x->count);

	if (proc_driver_cs752x_ne == NULL)
		proc_driver_cs752x_ne =
			proc_mkdir(CS752X_PATH_CS_NE, proc_driver_cs752x);
	else
		atomic_inc(&proc_driver_cs752x_ne->count);

	proc_driver_cs752x_ham =
		proc_mkdir(CS752X_PATH_CS_MANAGER, proc_driver_cs752x_ne);

#ifdef CONFIG_CS752X_HW_ACCELERATION
	cs_proc_node_add(CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE,
		cs_proc_node_kernel_mod_hw_accel_enable_read,
		cs_proc_node_kernel_mod_hw_accel_enable_write,
		proc_driver_cs752x_ham);
#endif

#ifdef CONFIG_CS752X_FASTNET
	cs_proc_node_add(CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE,
		cs_proc_node_kernel_mod_fastnet_enable_read,
		cs_proc_node_kernel_mod_fastnet_enable_write,
		proc_driver_cs752x_ham);
#endif

	cs_proc_node_add(CS752X_NODE_HASH_HW_ENTRY,
		cs_proc_node_hash_hw_entry_read,
		cs_proc_node_hash_hw_entry_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_CORE_HMU_TABLE,
		cs_proc_node_core_hmu_tbl_read,
		cs_proc_node_core_hmu_tbl_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_CORE_VTABLE,
		cs_proc_node_core_vtbl_read,
		NULL,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_HASH_TIMER_PERIOD,
		cs_proc_node_hash_timer_period_read,
	 	cs_proc_node_hash_timer_period_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_HASH_DEFAULT_LIFETIME,
		cs_proc_node_hash_default_lifetime_read,
		cs_proc_node_hash_default_lifetime_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_TCP_BYPASS_PORT,
		cs_proc_node_tcp_bypass_port_read,
		cs_proc_node_tcp_bypass_port_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_UDP_BYPASS_PORT,
		cs_proc_node_udp_bypass_port_read,
		cs_proc_node_udp_bypass_port_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_EXPECTED_MASTER_BYPASS_PORT,
		cs_proc_node_expected_bypass_port_read,
		cs_proc_node_expected_bypass_port_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_NF_DROP_CHECK_TRY,
		cs_proc_node_nf_drop_check_try_read,
		cs_proc_node_nf_drop_check_try_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_NF_DROP_CHECK_LIFE,
		cs_proc_node_nf_drop_check_life_read,
		cs_proc_node_nf_drop_check_life_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_NF_DROP_CHECK_TABLE,
		cs_proc_node_nf_drop_check_table_read,
		cs_proc_node_nf_drop_check_table_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_TCP_PRIORITY_PORT,
		cs_proc_node_tcp_priority_port_read,
		cs_proc_node_tcp_priority_port_write,
		proc_driver_cs752x_ham);

	cs_proc_node_add(CS752X_NODE_UDP_PRIORITY_PORT,
		cs_proc_node_udp_priority_port_read,
		cs_proc_node_udp_priority_port_write,
		proc_driver_cs752x_ham);

#ifdef CONFIG_CS752X_HW_ACCELERATION_IPSEC
	cs_proc_node_add(CS752X_NODE_IPSEC_SADB_TABLE,
		cs_proc_node_ipsec_sadb_table_read,
		cs_proc_node_ipsec_sadb_table_write,
		proc_driver_cs752x_ham);
	cs_proc_node_add(CS752X_NODE_IPSEC_PE_SADB_TABLE,
		cs_proc_node_ipsec_pe_sadb_table_read,
		cs_proc_node_ipsec_pe_sadb_table_write,
		proc_driver_cs752x_ham);
	cs_proc_node_add(CS752X_NODE_IPSEC_OFFLOAD_MODE,
		cs_proc_node_ipsec_offload_mode_read,
		cs_proc_node_ipsec_offload_mode_write,
		proc_driver_cs752x_ham);
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	cs_proc_node_add(CS_NODE_IPLIP_PE_CMD,
		cs_proc_node_iplip_pe_cmd_read,
		cs_proc_node_iplip_pe_cmd_write,
		proc_driver_cs752x_ham);
	cs_proc_node_add(CS_NODE_IPLIP_KERN_CMD,
		cs_proc_node_iplip_kernel_cmd_read,
		cs_proc_node_iplip_kernel_cmd_write,
		proc_driver_cs752x_ham);
#endif
	cs_proc_node_add(CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED,
		cs_proc_node_udp_offload_after_established_read,
		cs_proc_node_udp_offload_after_established_write,
		proc_driver_cs752x_ham);

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) ||\
		defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
	cs_proc_node_add(CS752X_NODE_VPN_OFFLOAD_MODE,
		cs_proc_node_vpn_offload_mode_read,
		cs_proc_node_vpn_offload_mode_write,
		proc_driver_cs752x_ham);
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	cs_proc_node_add(CS_NODE_WIRELESS_HW_ACCEL_MODE,
		cs_proc_node_wireless_mode_read,
		cs_proc_node_wireless_mode_write,
		proc_driver_cs752x_ham);
#endif

	cs_proc_node_add(CS752X_NODE_ALLOW_LOCALIN_PORT,
			cs_proc_node_allow_localin_port_read,
			cs_proc_node_allow_localin_port_write,
			proc_driver_cs752x_ham);
			
	cs_proc_node_add(CS752X_NODE_LOCALIN_UDP_DROP_STATISTIC,
			cs_proc_node_localin_udp_drop_statistic_read,
			cs_proc_node_localin_udp_drop_statistic_write,
			proc_driver_cs752x_ham);

	cs_proc_node_add("acl_add",
			cs_proc_acl_add_read,
			cs_proc_acl_add_write,
			proc_driver_cs752x_ham);

	return 0;

}

void __exit cs_accel_mgr_exit_module(void)
{
	/* remove file entry */
	remove_proc_entry(CS752X_NODE_CORE_HMU_TABLE, proc_driver_cs752x_ham);
	remove_proc_entry(CS752X_NODE_CORE_VTABLE, proc_driver_cs752x_ham);
	remove_proc_entry(CS752X_NODE_HASH_HW_ENTRY,
			  proc_driver_cs752x_ham);

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) ||\
		defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)
	remove_proc_entry(CS752X_NODE_VPN_OFFLOAD_MODE,	proc_driver_cs752x_ham);
#endif

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	remove_proc_entry(CS_NODE_IPLIP_PE_CMD,	proc_driver_cs752x_ham);
	remove_proc_entry(CS_NODE_IPLIP_KERN_CMD, proc_driver_cs752x_ham);
#endif

#ifdef CONFIG_CS752X_HW_ACCELERATION
	remove_proc_entry(CS752X_NODE_KERNEL_MOD_HW_ACCEL_ENABLE,
			proc_driver_cs752x_ham);
#endif

#ifdef CONFIG_CS752X_FASTNET
	remove_proc_entry(CS752X_NODE_KERNEL_MOD_FASTNET_ENABLE,
			proc_driver_cs752x_ham);
#endif

	remove_proc_entry(CS752X_NODE_HASH_TIMER_PERIOD,
				  proc_driver_cs752x_ham);
	remove_proc_entry(CS752X_NODE_HASH_DEFAULT_LIFETIME,
			  proc_driver_cs752x_ham);

	remove_proc_entry(CS752X_NODE_UDP_OFFLOAD_AFTER_ESTABLISHED,
			  proc_driver_cs752x_ham);

	remove_proc_entry(CS752X_PATH_CS_MANAGER, proc_driver_cs752x_ne);

	proc_driver_cs752x_ham = NULL;

	if (atomic_read(&proc_driver_cs752x_ne->count) == 1) {
		printk("%s() remove proc dir %s \n", __func__,
				CS752X_PATH_CS_NE);
		remove_proc_entry(CS752X_PATH_CS_NE, proc_driver_cs752x);
		proc_driver_cs752x_ne = NULL;
	} else {
		atomic_dec(&proc_driver_cs752x_ne->count);
	}

	if (atomic_read(&proc_driver_cs752x->count) == 1) {
		printk("%s() remove proc dir %s\n", __func__, CS752X_PATH_CS);
		remove_proc_entry(CS752X_PATH_CS, NULL);
		proc_driver_cs752x = NULL;
	} else {
		atomic_dec(&proc_driver_cs752x->count);
	}
}

module_init(cs_accel_mgr_init_module);
module_exit(cs_accel_mgr_exit_module);
MODULE_AUTHOR("Bird Hsieh <bird.hsieh@cortina-systems.com>");
MODULE_LICENSE("GPL");

