/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>	/* copy_*_user */
#include <linux/inet.h>
#include <linux/socket.h>

#include <mach/cs_mcast.h>

#include "cs_mut.h"

#ifdef CONFIG_CS75XX_MTU_CHECK
extern cs_port_id_t cs_wan_port_id;
#endif

/* file name */
#define CS_L2_MCAST_MEMBER_ADD			"cs_l2_mcast_member_add"
#define CS_L2_MCAST_MEMBER_DEL			"cs_l2_mcast_member_delete"
#define CS_L2_MCAST_PORT_MEMBER_GET		"cs_l2_mcast_port_member_get"
#define CS_L2_MCAST_PORT_MEMBER_CLEAR		"cs_l2_mcast_port_member_clear"
#define CS_L2_MCAST_MEMBER_ALL_CLEAR		"cs_l2_mcast_member_all_clear"
#define CS_L2_MCAST_MODE_SET			"cs_l2_mcast_mode_set"
#define CS_L2_MCAST_WAN_PORT_SET			"cs_l2_mcast_wan_port_set"


#define CS_L2_MCAST_MEMBER_ADD_HELP_MSG \
			"Purpose: Add L2 multicast foward entry to H/W\n" \
			"WRITE Usage: echo [dev_id] [port_id] [afi] [sub_port]\n" \
			"                  [mcast_vlan] [grp_addr] [mode]\n" \
			"                  [dest_mac] [src_num] [number of src] > %s\n" \
			"dev_id: Device ID\n" \
			"port_id: Port ID\n" \
			"        0: GE0\n" \
			"        1: GE1\n" \
			"        2: GE2\n" \
			"        3: CPU\n" \
			"        4: PE0\n" \
			"        5: PE1\n" \
			"        6: MCAST\n" \
			"        7: MIRROR\n" \
			"afi: \n" \
			"        0: IPv4\n" \
			"        1: IPv6\n" \
			"sub_port: Use In GPON mode to indicate GEM port\n" \
			"mcast_vlan: \n" \
			"grp_addr: Group Address, ex 225.1.2.3, FF02::1\n" \
			"mode: Filter Mode\n" \
			"        0: CS_MCAST_EXCLUDE\n" \
			"        1: CS_MCAST_INCLUDE\n" \
			"dest_mac: Unicast MAC address of joining host, xx:xx:xx:xx:xx:xx\n" \
			"src_num:  \n" \
			"number of src: \n"

#define CS_L2_MCAST_MEMBER_DEL_HELP_MSG \
			"Purpose: Delete L2 multicast foward entry to H/W\n" \
			"WRITE Usage: echo [dev_id] [port_id] [afi] [sub_port]\n" \
			"                  [mcast_vlan] [grp_addr] [mode]\n" \
			"                  [dest_mac] [src_num] [number of src] > %s\n" \
			"dev_id: Device ID\n" \
			"port_id: Port ID\n" \
			"        0: GE0\n" \
			"        1: GE1\n" \
			"        2: GE2\n" \
			"        3: CPU\n" \
			"        4: PE0\n" \
			"        5: PE1\n" \
			"        6: MCAST\n" \
			"        7: MIRROR\n" \
			"afi: \n" \
			"        0: IPv4\n" \
			"        1: IPv6\n" \
			"sub_port: Use In GPON mode to indicate GEM port\n" \
			"mcast_vlan: \n" \
			"grp_addr: Group Address, ex 225.1.2.3, FF02::1\n" \
			"mode: Filter Mode\n" \
			"        0: CS_MCAST_EXCLUDE\n" \
			"        1: CS_MCAST_INCLUDE\n" \
			"dest_mac: Unicast MAC address of joining host, xx:xx:xx:xx:xx:xx\n" \
			"src_num:  \n" \
			"number of src: \n"

#define CS_L2_MCAST_PORT_MEMBER_GET_HELP_MSG \
			"Purpose: Retrieves all multicast addresses present on the identified port\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"[value]: Port\n"

#define CS_L2_MCAST_PORT_MEMBER_CLEAR_HELP_MSG \
			"Purpose: Deletes the identified port from all multicast destination addresses present in the layer2 table\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"[value]: Port\n"

#define CS_L2_MCAST_MEMBER_ALL_CLEAR_HELP_MSG \
			"Purpose: Clears all multicast addresses from layer2 table on the device\n" \
			"WRITE Usage: echo 1 > %s\n"

#define CS_L2_MCAST_MODE_SET_HELP_MSG \
			"Purpose: sets the replication mode to port replication or arbitrary replication mode\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"[value]: Port\n"

#define CS_L2_MCAST_WAN_PORT_SET_HELP_MSG \
			"Purpose: sets the wan port \n" \
			"WRITE Usage: echo [value] > %s\n" \
			"[value]: Port ID\n\n" \
			"Port ID:\n" \
			"        0: GE0 (voq#55)\n" \
			"        1: GE1 (voq#63)\n" \
			"        2: GE2 (voq#71)\n"

/* entry pointer */
extern struct proc_dir_entry *proc_driver_cs752x_mcast;
extern int cs752x_add_proc_handler(char *name,
			    read_proc_t * hook_func_read,
			    write_proc_t * hook_func_write,
			    struct proc_dir_entry *parent);


static inline void dump_mcast_member_src_list(cs_uint16 src_num, cs_ip_address_t *src_list)
{
	cs_uint16 i;

	for (i = 0; i < src_num; i++) {
		if (src_list[i].afi == CS_IPV6)
			printk(KERN_INFO "src_list[%u]=%pI6\n", i, &src_list[i].ip_addr.ipv6_addr);
		else
			printk(KERN_INFO "src_list[%u]=%pI4\n", i, &src_list[i].ip_addr.ipv4_addr);
	}
}

/*
 * dump_mcast_member
 */
static void dump_mcast_member(cs_mcast_member_t *mcast)
{
	int i;

	if (mcast->afi == CS_IPV6) {
		printk(KERN_INFO "grp_addr=%pI6\n", &mcast->grp_addr.ip_addr.ipv6_addr);
	} else {
		printk(KERN_INFO "grp_addr=%pI4\n", &mcast->grp_addr.ip_addr.ipv4_addr);
	}

	for (i = 0; i < mcast->src_num; i++) {
		if (mcast->src_list[i].afi == CS_IPV6)
			printk(KERN_INFO "src_list[%u]=%pI6\n", i, &mcast->src_list[i].ip_addr.ipv6_addr);
		else
			printk(KERN_INFO "src_list[%u]=%pI4\n", i, &mcast->src_list[i].ip_addr.ipv4_addr);
	}

//	dump_mcast_member_src_list(mcast->src_num, mcast->src_list);

	printk(KERN_INFO
		"afi=%d, sub_port=%d\n"
		"mcast_vlan=%d\n"
		"mode=%d, dest_mac=%pM\n"
		"src_num=%d\n",
		mcast->afi, mcast->sub_port,
		mcast->mcast_vlan, mcast->mode,
		mcast->dest_mac, mcast->src_num);
}

/*
 * cs_proc_l2_mcast_member_add_write_proc
 */
static int cs_proc_l2_mcast_member_add_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[256];
	cs_port_id_t dev_id;
	cs_port_id_t port_id;
	cs_mcast_member_t mc_addr;
	int para_no;
	char str_grp_addr[64];
	char str_src_ip[64];
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCAST_ADDRESS_ADD_INVAL_EXIT;

	memset(&mc_addr, 0, sizeof(mc_addr));

	buf[len] = '\0';
	para_no = sscanf(buf, "%x %u %d %u %u %s %d %x:%x:%x:%x:%x:%x %u %s\n",
		&dev_id, &port_id, &(mc_addr.afi),
		&(mc_addr.sub_port),
		&(mc_addr.mcast_vlan), str_grp_addr, &(mc_addr.mode),
		&(mc_addr.dest_mac[0]), &(mc_addr.dest_mac[1]),
		&(mc_addr.dest_mac[2]), &(mc_addr.dest_mac[3]),
		&(mc_addr.dest_mac[4]), &(mc_addr.dest_mac[5]),
		&(mc_addr.src_num), str_src_ip);

	if (para_no < 15)
		goto MCAST_ADDRESS_ADD_INVAL_EXIT;

	if ((mc_addr.afi != CS_IPV6) && (mc_addr.afi != CS_IPV4))
		goto MCAST_ADDRESS_ADD_INVAL_EXIT;

	if ((mc_addr.mode != CS_MCAST_EXCLUDE) && (mc_addr.mode != CS_MCAST_INCLUDE))
		goto MCAST_ADDRESS_ADD_INVAL_EXIT;

	// only allow 1 source through proc entry
	mc_addr.src_num = 1;

	if (mc_addr.afi == CS_IPV6) {
		//IPv6

		mc_addr.grp_addr.afi = CS_IPV6;
		if (in6_pton(str_grp_addr, -1, (void *) &mc_addr.grp_addr.ip_addr.ipv6_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in6_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_ADD_INVAL_EXIT;
		}

		mc_addr.src_list[0].afi = CS_IPV6;
		if (in6_pton(str_src_ip, -1, (void *) &mc_addr.src_list[0].ip_addr.ipv6_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in6_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_ADD_INVAL_EXIT;
		}

	} else {
		//IPv4

		mc_addr.grp_addr.afi = CS_IPV4;
		if (in4_pton(str_grp_addr, -1, (void *) &mc_addr.grp_addr.ip_addr.ipv4_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in4_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_ADD_INVAL_EXIT;
		}

		mc_addr.src_list[0].afi = CS_IPV4;
		if (in4_pton(str_src_ip, -1, (void *) &mc_addr.src_list[0].ip_addr.ipv4_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in4_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_ADD_INVAL_EXIT;
		}

	}

	dump_mcast_member(&mc_addr);

	cs_l2_mcast_member_add(dev_id, port_id, &mc_addr);

	return count;

MCAST_ADDRESS_ADD_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_MEMBER_ADD_HELP_MSG, CS_L2_MCAST_MEMBER_ADD);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}/* cs_proc_l2_mcast_member_add_write_proc() */


/*
 * cs_proc_l2_mcast_member_del_write_proc
 */
static int cs_proc_l2_mcast_member_del_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[256];
	cs_port_id_t dev_id;
	cs_port_id_t port_id;
	cs_mcast_member_t mc_addr;
	int para_no;
	char str_grp_addr[64];
	char str_src_ip[64];
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCAST_ADDRESS_DEL_INVAL_EXIT;

	memset(&mc_addr, 0, sizeof(mc_addr));

	buf[len] = '\0';
	para_no = sscanf(buf, "%x %u %d %u %u %s %d %x:%x:%x:%x:%x:%x %u %s\n",
		&dev_id, &port_id, &(mc_addr.afi),
		&(mc_addr.sub_port),
		&(mc_addr.mcast_vlan), str_grp_addr, &(mc_addr.mode),
		&(mc_addr.dest_mac[0]), &(mc_addr.dest_mac[1]),
		&(mc_addr.dest_mac[2]), &(mc_addr.dest_mac[3]),
		&(mc_addr.dest_mac[4]), &(mc_addr.dest_mac[5]),
		&(mc_addr.src_num), str_src_ip);

	if (para_no < 15)
		goto MCAST_ADDRESS_DEL_INVAL_EXIT;

	if ((mc_addr.afi != CS_IPV6) && (mc_addr.afi != CS_IPV4))
		goto MCAST_ADDRESS_DEL_INVAL_EXIT;

	if ((mc_addr.mode != CS_MCAST_EXCLUDE) && (mc_addr.mode != CS_MCAST_INCLUDE))
		goto MCAST_ADDRESS_DEL_INVAL_EXIT;

	// only allow 1 source through proc entry
	mc_addr.src_num = 1;

	if (mc_addr.afi == CS_IPV6) {
		//IPv6

		mc_addr.grp_addr.afi = CS_IPV6;
		if (in6_pton(str_grp_addr, -1, (void *) &mc_addr.grp_addr.ip_addr.ipv6_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in6_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_DEL_INVAL_EXIT;
		}

		mc_addr.src_list[0].afi = CS_IPV6;
		if (in6_pton(str_src_ip, -1, (void *) &mc_addr.src_list[0].ip_addr.ipv6_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in6_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_DEL_INVAL_EXIT;
		}

	} else {
		//IPv4

		mc_addr.grp_addr.afi = CS_IPV4;
		if (in4_pton(str_grp_addr, -1, (void *) &mc_addr.grp_addr.ip_addr.ipv4_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in4_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_DEL_INVAL_EXIT;
		}

		mc_addr.src_list[0].afi = CS_IPV4;
		if (in4_pton(str_src_ip, -1, (void *) &mc_addr.src_list[0].ip_addr.ipv4_addr, '\0', NULL) == 0) {
			printk(KERN_INFO "%s: in4_pton() fails.\n", __func__);
			goto MCAST_ADDRESS_DEL_INVAL_EXIT;
		}

	}

	dump_mcast_member(&mc_addr);

	cs_l2_mcast_member_delete(dev_id, port_id, &mc_addr);

	return count;

MCAST_ADDRESS_DEL_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_MEMBER_DEL_HELP_MSG, CS_L2_MCAST_MEMBER_DEL);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}/* cs_proc_l2_mcast_member_del_write_proc() */


static const char* cs_ip_afi_text[2] = {
	"IPv4",
	"IPv6",
};

static const char* cs_mcast_filter_mode_text[2] = {
	"MCAST EXCLUDE",
	"MCAST INCLUDE",
};

static inline void __dump_src_list(cs_ip_afi_t afi, cs_uint16 src_num, cs_ip_address_t *src_list)
{
	int i;

	for (i = 0; i < src_num; i++) {
		if (afi == CS_IPV6) {
			printk("src_list[%u]\t: %pI6\n", i, &(src_list[i].ip_addr.ipv6_addr));
		} else {
			printk("src_list[%u]\t: %pI4\n", i, &(src_list[i].ip_addr.ipv4_addr));
		}
	}
}

static int cs_l2_mcast_port_member_get_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	cs_port_id_t port;
	ssize_t len;
	cs_uint16 num = 0;
	cs_mcast_member_t *entry_p;
	cs_mcast_member_t *entry_org_p = NULL;
	int i;


	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, (long unsigned int*) &port))
		goto MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT;


	if (port > 8) {
		goto MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT;
	}

	if (cs_l2_mcast_port_member_num_get(0, port, &num) != CS_E_OK) {
		return count;
	}

	if (num == 0) {
		return count;
	}

	if ((entry_p = (cs_mcast_member_t *) cs_zalloc(sizeof(cs_mcast_member_t) * num, GFP_KERNEL)) == NULL) {
		printk(KERN_INFO "%s: Can not alloc memory.\n", __func__);
		return count;
	}

	if (cs_l2_mcast_port_member_entry_get(0, port, &num, entry_p) == CS_E_OK) {
		printk("### L2 Multicast Port %d, Address number %d\n", port, num);
		if (num != 0) {
			if (entry_p != NULL) {
				entry_org_p = entry_p;
				for (i = 0; i < num; i++) {
					printk("=====================================\n");
					printk("afi\t\t: %s\n", cs_ip_afi_text[entry_p->afi]);
//					printk("sub_port\t: %u\n", entry_p->sub_port);
//					printk("mcast_vlan\t: %u\n", entry_p->mcast_vlan);
					if (entry_p->afi == CS_IPV4) {
						printk("grp_addr\t: %pI4\n", &entry_p->grp_addr.ip_addr.ipv4_addr);
					} else {
						printk("grp_addr\t: %pI6\n", &entry_p->grp_addr.ip_addr.ipv6_addr);
					}
					printk("src_num\t\t: %u\n", entry_p->src_num);
					__dump_src_list(entry_p->afi, entry_p->src_num, entry_p->src_list);
					printk("mode\t\t: %s\n", cs_mcast_filter_mode_text[entry_p->mode]);
//					printk("dest_mac\t: %pM\n", entry_p->dest_mac);
					entry_p++;
				} /* for (i=0; i< num; i++) */
				printk("=====================================\n");
//				cs_free(entry_org_p);
			} /* if (entry_pp!= NULL) */
		} /* if (num != 0) */
	} /* if (cs_l2_mcast_port_member_get) */

	if (entry_org_p != NULL)
		cs_free(entry_org_p);

	return count;

MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_PORT_MEMBER_GET_HELP_MSG, CS_L2_MCAST_PORT_MEMBER_GET);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_l2_mcast_port_member_clear_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long port;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &port))
		goto MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT;

	if (port > 8) {
		goto MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT;
	}

	printk(KERN_WARNING "Clear Port %u multicast\n", port);

	cs_l2_mcast_port_member_clear(0, port);

	return count;

MCS_L2_MCAST_PORT_MEMBER_CLEAR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_PORT_MEMBER_CLEAR_HELP_MSG, CS_L2_MCAST_PORT_MEMBER_CLEAR);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_l2_mcast_member_all_clear_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCAST_ADDRESS_ALL_CLEAR_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto MCAST_ADDRESS_ALL_CLEAR_INVAL_EXIT;

	if (mask != 1) {
		goto MCAST_ADDRESS_ALL_CLEAR_INVAL_EXIT;
	}

	printk(KERN_WARNING "CS_L2_MCAST_MEMBER_ALL_CLEAR\n");

	cs_l2_mcast_member_all_clear(0);

	return count;

MCAST_ADDRESS_ALL_CLEAR_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_MEMBER_ALL_CLEAR_HELP_MSG, CS_L2_MCAST_MEMBER_ALL_CLEAR);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_l2_mcast_mode_set_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long port;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCAST_MODE_SET_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &port))
		goto MCAST_MODE_SET_INVAL_EXIT;


	printk(KERN_WARNING "Set %s multicast mode as %u\n", CS_L2_MCAST_MODE_SET, port);

	cs_l2_mcast_mode_set(0, port);

	return count;

MCAST_MODE_SET_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_MODE_SET_HELP_MSG, CS_L2_MCAST_MODE_SET);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_l2_mcast_wan_port_set_write_proc(struct file *file, const char *buffer,
				    unsigned long count, void *data)
{
	char buf[32];
	unsigned long port;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto MCAST_MODE_SET_INVAL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &port))
		goto MCAST_MODE_SET_INVAL_EXIT;


	printk(KERN_WARNING "Set %s multicast WAN port as %u\n", CS_L2_MCAST_MODE_SET, port);

	cs_l2_mcast_wan_port_id_set(0, port);

#ifdef CONFIG_CS75XX_MTU_CHECK
	cs_wan_port_id = port;
#endif

	return count;

MCAST_MODE_SET_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_L2_MCAST_WAN_PORT_SET_HELP_MSG, CS_L2_MCAST_WAN_PORT_SET);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}

static int cs_l2_mcast_wan_port_set_read_proc(char *buf, char **start, off_t offset,
				       int count, int *eof, void *data)
{
	u32 len = 0;
	u32 wan_port_id;

	cs_l2_mcast_wan_port_id_get(0, &wan_port_id);
	len += sprintf(buf + len, "\n%s = 0x%08x\n", CS_L2_MCAST_WAN_PORT_SET,
			wan_port_id);
	printk(KERN_WARNING "%s: multicast WAN port is %d\n", CS_L2_MCAST_MODE_SET, wan_port_id);
	*eof = 1;

	return len;
}


void cs_mcast_proc_init_module(void)
{

	cs752x_add_proc_handler(CS_L2_MCAST_MEMBER_ADD,
				NULL,
				cs_proc_l2_mcast_member_add_write_proc,
				proc_driver_cs752x_mcast);

	cs752x_add_proc_handler(CS_L2_MCAST_MEMBER_DEL,
				NULL,
				cs_proc_l2_mcast_member_del_write_proc,
				proc_driver_cs752x_mcast);

	cs752x_add_proc_handler(CS_L2_MCAST_PORT_MEMBER_GET,
				NULL,
				cs_l2_mcast_port_member_get_write_proc,
				proc_driver_cs752x_mcast);

	cs752x_add_proc_handler(CS_L2_MCAST_PORT_MEMBER_CLEAR,
				NULL,
				cs_l2_mcast_port_member_clear_write_proc,
				proc_driver_cs752x_mcast);

	cs752x_add_proc_handler(CS_L2_MCAST_MEMBER_ALL_CLEAR,
				NULL,
				cs_l2_mcast_member_all_clear_write_proc,
				proc_driver_cs752x_mcast);

	cs752x_add_proc_handler(CS_L2_MCAST_MODE_SET,
				NULL,
				cs_l2_mcast_mode_set_write_proc,
				proc_driver_cs752x_mcast);

	cs752x_add_proc_handler(CS_L2_MCAST_WAN_PORT_SET,
				NULL,
				cs_l2_mcast_wan_port_set_write_proc,
				proc_driver_cs752x_mcast);

}/* cs_qos_proc_init_module() */

void cs_mcast_proc_exit_module(void)
{
	/* no problem if it was not registered */
	/* remove file entry */
	remove_proc_entry(CS_L2_MCAST_MODE_SET, proc_driver_cs752x_mcast);
	remove_proc_entry(CS_L2_MCAST_MEMBER_ALL_CLEAR, proc_driver_cs752x_mcast);
	remove_proc_entry(CS_L2_MCAST_PORT_MEMBER_CLEAR, proc_driver_cs752x_mcast);
	remove_proc_entry(CS_L2_MCAST_PORT_MEMBER_GET, proc_driver_cs752x_mcast);
	remove_proc_entry(CS_L2_MCAST_MEMBER_ADD, proc_driver_cs752x_mcast);
	remove_proc_entry(CS_L2_MCAST_MEMBER_DEL, proc_driver_cs752x_mcast);

}/* cs_qos_proc_exit_module () */
