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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <mach/cs_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_fe_table_api.h>
#include <cs_fe_head_table.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#include "cs752x_eth.h"
#include "cs_core_logic_data.h"
#include "cs_core_vtable.h"
#include "cs_fe_hash.h"
#include <mach/cs_mcast.h>
#include "cs_mut.h"

#define PROTO_IGMP	0x02
#define PROTO_ICMPV6	0x3A

#ifdef CONFIG_CS752X_PROC
extern u32 cs_ne_core_logic_debug;
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_CORE_VTABLE) x;}
#else
#define DBG(x) { }
#endif

fe_fwd_result_entry_t igmp_fwdrslt_entry[GE_PORT_NUM];
fe_voq_pol_entry_t igmp_voqpol_entry[GE_PORT_NUM];
unsigned int igmp_fwdrslt_idx[GE_PORT_NUM], igmp_voqpol_idx[GE_PORT_NUM];
__u8 igmp_hm_idx = ~(0x0);

fe_fwd_result_entry_t icmpv6_fwdrslt_entry[GE_PORT_NUM];
fe_voq_pol_entry_t icmpv6_voqpol_entry[GE_PORT_NUM];
unsigned int icmpv6_fwdrslt_idx[GE_PORT_NUM], icmpv6_voqpol_idx[GE_PORT_NUM];
__u8 icmpv6_hm_idx = ~(0x0);

unsigned int rootport_fwdrslt_idx = ~(0x0), rootport_voqpol_idx = ~(0x0);
__u8 mcast_without_src_hm_idx = ~(0x0);
__u8 mcast_with_src_hm_idx = ~(0x0);
__u8 mcast_to_dest_hm_idx = ~(0x0);

/* fwdrslt is to drop */
fe_fwd_result_entry_t drop_fwdrslt_entry;
unsigned int drop_fwdrslt_idx;

cs_uint8_t mcast_mode = CS_MCAST_PORT_REPLICATION;

#define CS_MC_BIT_UNUSED	0x01
#define CS_MC_BIT_PORT		0x02	/* dest is port base index */
#define CS_MC_BIT_STA		0x04	/* dest is station, dest_mac[] is available */
#define CS_MC_BIT_DEV		0x08	/* call dev->start_xmit() */
#define CS_MC_BIT_SWONLY	0x10	/* to CPU port */

#define CS_MCGID_SIZE		8	/* size of ni_port_t */

struct cs_mcgid_bit {
	uint8_t valid;
	uint8_t ifidx;
	uint32_t port;
	uint32_t mode;	// CS_MC_BIT_XXX
	netdev_tx_t (*start_xmit) (struct sk_buff *skb, struct net_device *dev);

	fe_voq_pol_entry_t voqpol_entry;
	unsigned int voqpol_idx;

	fe_fwd_result_entry_t fwdrslt_entry;
	unsigned int fwdrslt_idx;

	uint16_t hash_idx;			/* hash index that forward to dest port */
};

//int cs_mc_wan_port_id = NI_GE0;
int cs_mc_wan_port_id = 0xFF;   // means not set
struct cs_mcgid_bit mcgid_bit[CS_MCGID_SIZE] = {
	[NI_GE0] = {
		.valid = 0,
		.ifidx = 0,
		.port = GE_PORT0_VOQ_BASE + 7,
		.mode = CS_MC_BIT_PORT,
	},
	[NI_GE1] = {
		.valid = 0,
		.ifidx = 1,
		.port = GE_PORT1_VOQ_BASE + 7,
		.mode = CS_MC_BIT_PORT,
	},
	[NI_GE2] = {
		.valid = 0,
		.ifidx = 2,
		.port = GE_PORT2_VOQ_BASE + 7,
		.mode = CS_MC_BIT_PORT,
	},
	[NI_CPU] = {
		.valid = 0,
		.ifidx = 3,
		.port = CPU_PORT0_VOQ_BASE + 7, /*FIXME: the CPU port should be defined by user*/
		.mode = CS_MC_BIT_PORT,
	},
	[NI_CCORE] = {
		.valid = 0,
		.ifidx = 4,
		.port = ENCRYPTION_VOQ_BASE,
		.mode = CS_MC_BIT_PORT,
	},
	[NI_ECORE] = {
		.valid = 0,
		.ifidx = 5,
		.port = ENCAPSULATION_VOQ_BASE,
		.mode = CS_MC_BIT_PORT,
	},
	[NI_MCAST] = {
		.valid = 0,
		.mode = CS_MC_BIT_UNUSED,
	},
	[NI_MIRROR] = {
		.valid = 0,
		.mode = CS_MC_BIT_UNUSED,
	},

};

#define CS_NODE_GROUP	0x1
#define CS_NODE_SOURCE	0x2

struct cs_mcast_node {
	cs_ip_address_t addr;
	cs_mcast_filter_mode mode;
	uint8_t type;				/* node type; CS_NODE_* */
	uint32_t mcgid;

	struct cs_mcast_node *next_group;
	struct cs_mcast_node *next_src;
	struct cs_mcast_node *group;		/* for source node, point to group node */

	unsigned int fwdrslt_idx;		/* forward result index that forward to root port and modify mcgid */
	uint16_t hash_idx;			/* hash index that forward to root port */
};

struct cs_mcast_node *head;

spinlock_t list_lock;

/* Compare the IPv4 address.
 * Return value:
 *   0: equal
 *   1: not equal
 */
static int compare_addr_v4(cs_ip_address_t *addr1, cs_ip_address_t *addr2)
{
	DBG(printk(KERN_INFO "addr1=%08x\n", addr1->ip_addr.ipv4_addr));
	DBG(printk(KERN_INFO "addr2=%08x\n", addr2->ip_addr.ipv4_addr));

	if (addr1->ip_addr.ipv4_addr != addr2->ip_addr.ipv4_addr)
		return 1;

	return 0;
}

/* Compare the IPv6 address.
 * Return value:
 *   0: equal
 *   1: not equal
 */
static int compare_addr_v6(cs_ip_address_t *addr1, cs_ip_address_t *addr2)
{
	int i;

	DBG(printk(KERN_INFO "addr1=%08x %08x %08x %08x\n",
		addr1->ip_addr.ipv6_addr[0], addr1->ip_addr.ipv6_addr[1], addr1->ip_addr.ipv6_addr[2], addr1->ip_addr.ipv6_addr[3]));
	DBG(printk(KERN_INFO "addr2=%08x %08x %08x %08x\n",
		addr2->ip_addr.ipv6_addr[0], addr2->ip_addr.ipv6_addr[1], addr2->ip_addr.ipv6_addr[2], addr2->ip_addr.ipv6_addr[3]));

	for (i = 0; i < 4; i++) {
		if (addr1->ip_addr.ipv6_addr[i] != addr2->ip_addr.ipv6_addr[i])
			return 1;
	}

	return 0;
}

static int compare_addr(cs_ip_address_t *addr1, cs_ip_address_t *addr2)
{
	if (addr1->afi != addr2->afi)
		return 1;

	switch (addr1->afi) {
	case CS_IPV4:
		return compare_addr_v4(addr1, addr2);
		break;

	case CS_IPV6:
		return compare_addr_v6(addr1, addr2);
		break;
	default:
		return 1;
	}

	return 0;
}

/* Check if IP address is all zero.
 * Return:
 *   0: not all zero.
 *   1: all zero.
 */
static int is_addr_empty(cs_ip_address_t *addr)
{
	if (addr->afi == CS_IPV4) {

		return (addr->ip_addr.ipv4_addr == 0);

	} else if (addr->afi == CS_IPV6) {

		return ((addr->ip_addr.ipv6_addr[0] == 0) &&
			(addr->ip_addr.ipv6_addr[1] == 0) &&
			(addr->ip_addr.ipv6_addr[2] == 0) &&
			(addr->ip_addr.ipv6_addr[3] == 0));

	}

	return 0;
}

static int mcast_free_group(struct cs_mcast_node *group_node)
{
	struct cs_mcast_node *tmp = group_node;
	struct cs_mcast_node *next;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);
	while (tmp != NULL) {
		next = tmp->next_src;
		cs_free(tmp);
		tmp = next;
	}
	spin_unlock_irqrestore(&list_lock, flags);

	return 0;
}

static struct cs_mcast_node* mcast_search_group(cs_ip_address_t *grp_addr)
{
	struct cs_mcast_node *iter = head->next_group;

	while (iter != NULL) {
		if (compare_addr(&iter->addr, grp_addr) == 0)
			return iter;

		iter = iter->next_group;
	}

	return NULL;
}

static struct cs_mcast_node* mcast_search_src(struct cs_mcast_node *group_node, cs_ip_address_t *src_addr)
{
	struct cs_mcast_node *iter = group_node->next_src;

	while (iter != NULL) {
		if (compare_addr(&iter->addr, src_addr) == 0)
			return iter;

		iter = iter->next_src;
	}

	return NULL;
}

static struct cs_mcast_node* mcast_add_group_by_node(struct cs_mcast_node *group_node)
{
	struct cs_mcast_node *node;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	if ((node = mcast_search_group(&group_node->addr)) != NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return node;
	}

	group_node->next_group = head->next_group;
	head->next_group = group_node;

	spin_unlock_irqrestore(&list_lock, flags);

	return group_node;
}

static struct cs_mcast_node* mcast_add_group_by_ip(cs_ip_address_t *grp_addr)
{
	struct cs_mcast_node *node;

	if ((node = mcast_search_group(grp_addr)) != NULL)
		return node;

	if ((node = cs_zalloc(sizeof(struct cs_mcast_node), GFP_KERNEL)) == NULL)
		return NULL;

	if (grp_addr->afi == CS_IPV4)
		memcpy(&node->addr.ip_addr.ipv4_addr, &grp_addr->ip_addr.ipv4_addr, 4);
	else
		memcpy(&node->addr.ip_addr.ipv6_addr, &grp_addr->ip_addr.ipv6_addr, 16);
	node->addr.afi = grp_addr->afi;
	node->type = CS_NODE_GROUP;
	node->next_group = NULL;
	node->next_src = NULL;
	node->group = NULL;

	return mcast_add_group_by_node(node);
}

static int mcast_del_group_by_node(struct cs_mcast_node *group_node, int force)
{
	struct cs_mcast_node *tmp = head->next_group;
	struct cs_mcast_node *prev = head;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);
	while (tmp != NULL) {
		if (compare_addr(&tmp->addr, &group_node->addr) == 0)
			break;

		prev = tmp;
		tmp = tmp->next_group;
	}

	if (tmp == NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return 1;
	}

	if ((tmp->next_src != NULL) && (force == 0)) {
		spin_unlock_irqrestore(&list_lock, flags);
		return 1;
	}

	prev->next_group = tmp->next_group;

	spin_unlock_irqrestore(&list_lock, flags);

	mcast_free_group(tmp);

	return 0;
}

static int mcast_del_group_by_ip(cs_ip_address_t *grp_addr, int force)
{
	struct cs_mcast_node *tmp = head->next_group;
	struct cs_mcast_node *prev = head;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);
	while (tmp != NULL) {
		if (compare_addr(&tmp->addr, grp_addr) == 0)
			break;

		prev = tmp;
		tmp = tmp->next_group;
	}

	if (tmp == NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return 1;
	}

	if ((tmp->next_src != NULL) && (force == 0)) {
		spin_unlock_irqrestore(&list_lock, flags);
		return 1;
	}

	prev->next_group = tmp->next_group;

	spin_unlock_irqrestore(&list_lock, flags);

	mcast_free_group(tmp);

	return 0;
}

static struct cs_mcast_node* mcast_add_src(cs_ip_address_t *grp_addr, struct cs_mcast_node *src_node)
{
	struct cs_mcast_node *group_node;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	group_node = mcast_search_group(grp_addr);
	if (group_node == NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return NULL;
	}

	if (mcast_search_src(group_node, &src_node->addr) != NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return src_node;
	}

	src_node->group = group_node;
	src_node->next_src = group_node->next_src;
	group_node->next_src = src_node;

	spin_unlock_irqrestore(&list_lock, flags);

	return src_node;
}

static int mcast_del_src(cs_ip_address_t *grp_addr, struct cs_mcast_node *src)
{
	struct cs_mcast_node *group_node;
	struct cs_mcast_node *tmp, *prev;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	group_node = mcast_search_group(grp_addr);
	if (group_node == NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return 1;
	}

	tmp = group_node->next_src;
	prev = group_node;

	while (tmp != NULL) {
		if (compare_addr(&tmp->addr, &src->addr) == 0)
			break;

		prev = tmp;
		tmp = tmp->next_src;
	}

	if (tmp == NULL) {
		spin_unlock_irqrestore(&list_lock, flags);
		return 1;
	}

	prev->next_src = tmp->next_src;

	spin_unlock_irqrestore(&list_lock, flags);

	cs_free(tmp);
}

static void mcast_dump_list(void)
{
	struct cs_mcast_node *group_node, *src_node;
	char *mode_str[] = {"EXCLUDE", "INCLUDE"};

	group_node = head->next_group;

	DBG(printk(KERN_INFO "--- mcast list ---\n"));
	while (group_node != NULL) {
		src_node = group_node->next_src;

		if (group_node->addr.afi == CS_IPV6) {
			DBG(printk(KERN_INFO "group=%pI6\n", &group_node->addr.ip_addr.ipv6_addr));
		} else {
			DBG(printk(KERN_INFO "group=%pI4\n", &group_node->addr.ip_addr.ipv4_addr));
		}

		while (src_node != NULL) {
			if (src_node->addr.afi == CS_IPV6) {
				DBG( printk(KERN_INFO "  src=%pI6, mcgid=0x%04x, mode=%s\n",
					&src_node->addr.ip_addr.ipv6_addr, src_node->mcgid, mode_str[!!(src_node->mode)]) );
			} else {
				DBG( printk(KERN_INFO "  src=%pI4, mcgid=0x%04x, mode=%s\n",
					&src_node->addr.ip_addr.ipv4_addr, src_node->mcgid, mode_str[!!(src_node->mode)]) );
			}
			src_node = src_node->next_src;
		}

		group_node = group_node->next_group;
	}
}

/*************************************************************************/

/* forward result type; internal use */
enum {
	RSLT_FWD,
	RSLT_DROP,
};

/* Add hash according to the fwdrslt_type, addr, in src_node.
 * But use the parameter 'mcgid' to override src_node->mcgid.
 */
static cs_status_t cs_mcast_add_hash(struct cs_mcast_node *src_node, unsigned int mcgid, int fwdrslt_type)
{
	fe_fwd_result_entry_t rootport_fwdrslt_entry;
	fe_sw_hash_t key;
	u32 crc32;
	u16 crc16;

	unsigned char *cp;
	int ret;

	if (fwdrslt_type == RSLT_FWD) {
		memset(&rootport_fwdrslt_entry, 0x0, sizeof(fe_fwd_result_entry_t));
		rootport_fwdrslt_entry.l2.mcgid = mcgid | (1 << 8); /* port replication mode */
		rootport_fwdrslt_entry.l2.mcgid_valid = 1;
		rootport_fwdrslt_entry.dest.voq_policy = 0;
		rootport_fwdrslt_entry.dest.voq_pol_table_index = rootport_voqpol_idx;
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &rootport_fwdrslt_entry,
				&src_node->fwdrslt_idx)) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
		}
	}

	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.ip_valid = 1;
	key.lspid = cs_mc_wan_port_id;
	if (src_node->addr.afi == CS_IPV4) {
		key.ip_version = 0;
		key.da[0] = ntohl(src_node->group->addr.ip_addr.ipv4_addr);

		if (! is_addr_empty(&src_node->addr))
			key.sa[0] = ntohl(src_node->addr.ip_addr.ipv4_addr);

		/* multicast group address 01-00-5E-XX-XX-XX */
		cp = (unsigned char*) &src_node->group->addr.ip_addr.ipv4_addr;
		key.mac_da[0] = cp[3];
		key.mac_da[1] = cp[2];
		key.mac_da[2] = cp[1] & 0x7f;
		key.mac_da[3] = 0x5e;
		key.mac_da[4] = 0x00;
		key.mac_da[5] = 0x01;
	} else if (src_node->addr.afi == CS_IPV6) {
		key.ip_version = 1;
		key.da[0] = ntohl(src_node->group->addr.ip_addr.ipv6_addr[3]);
		key.da[1] = ntohl(src_node->group->addr.ip_addr.ipv6_addr[2]);
		key.da[2] = ntohl(src_node->group->addr.ip_addr.ipv6_addr[1]);
		key.da[3] = ntohl(src_node->group->addr.ip_addr.ipv6_addr[0]);

		if (!is_addr_empty(&src_node->addr)) {
			key.sa[0] = ntohl(src_node->addr.ip_addr.ipv6_addr[3]);
			key.sa[1] = ntohl(src_node->addr.ip_addr.ipv6_addr[2]);
			key.sa[2] = ntohl(src_node->addr.ip_addr.ipv6_addr[1]);
			key.sa[3] = ntohl(src_node->addr.ip_addr.ipv6_addr[0]);
		}

		/* multicast group address 33-33-XX-XX-XX-XX */
		cp = (unsigned char*) &src_node->group->addr.ip_addr.ipv6_addr[3];
		key.mac_da[0] = cp[3];
		key.mac_da[1] = cp[2];
		key.mac_da[2] = cp[1];
		key.mac_da[3] = cp[0];
		key.mac_da[4] = 0x33;
		key.mac_da[5] = 0x33;
	} else {
		/* impossible path */
		return CS_E_ERROR;
	}

	if (is_addr_empty(&src_node->addr)) {
		key.mask_ptr_0_7 = mcast_without_src_hm_idx;
	} else {
		key.mask_ptr_0_7 = mcast_with_src_hm_idx;
	}

	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
		// TODO
		return ret;
	}

	if (fwdrslt_type == RSLT_FWD) {
		ret = cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
			src_node->fwdrslt_idx, &src_node->hash_idx);
	} else if (fwdrslt_type == RSLT_DROP) {
		ret = cs_fe_hash_add_hash(crc32, crc16, key.mask_ptr_0_7,
			drop_fwdrslt_idx, &src_node->hash_idx);
	} else {
		printk(KERN_INFO "%s: Invalid fwdrslt_type %d\n", __func__, fwdrslt_type);
		return CS_E_ERROR;
	}
	if (ret != 0) {
		printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
		// TODO
		return ret;
	}

	return CS_E_OK;
}

/* Delete the hash in src_node */
static cs_status_t cs_mcast_del_hash(struct cs_mcast_node *src_node)
{
	if (src_node->mcgid != 0x0) {
		/* delete existing hash entry; mcgid is not 0, that means there exists hash */
		DBG(printk(KERN_INFO "%s: delete hash_idx=%u\n", __func__, src_node->hash_idx));
		if (src_node->hash_idx != (uint16_t) ~(0x0)) {
			cs_fe_hash_del_hash(src_node->hash_idx);
			src_node->hash_idx = ~(0x0);
		}
		if (src_node->fwdrslt_idx != (unsigned int) ~(0x0)) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, src_node->fwdrslt_idx, false);
			src_node->fwdrslt_idx = ~(0x0);
		}
	}

	return CS_E_OK;
}

/* update the hash that packet from upstream port to root port */
static cs_status_t cs_mcast_add_update_hash(struct cs_mcast_node *src_node, cs_uint16_t downstream_port, cs_mcast_filter_mode mode)
{
	struct cs_mcast_node *iter_node;
	unsigned int tmp_mcgid;

	cs_mcast_del_hash(src_node);

	src_node->mcgid |= (1 << downstream_port);
	src_node->mode = mode;

	if (src_node->mode == CS_MCAST_INCLUDE) {

		if (is_addr_empty(&src_node->addr)) {
			/* add WITHOUT_SRC hash to drop */
			/* src_node->mcgid contains the port bit to drop */

			// It is not necessary to add hash for this case beacuse default rule is
			// to drop all unknown multicast.
			//cs_mcast_add_hash(src_node, src_node->mcgid, RSLT_DROP);

		} else { /* address is not zero */
			/* add WITH_SRC hash to fwd */
			/* src_node->mcgid contains the port bit to forward */

			cs_mcast_add_hash(src_node, src_node->mcgid, RSLT_FWD);

		}

	} else if (src_node->mode == CS_MCAST_EXCLUDE) {

		if (is_addr_empty(&src_node->addr)) {
			/* add WITHOUT_SRC hash to fwd */
			/* src_node->mcgid contains the port bit to forward */

			cs_mcast_add_hash(src_node, src_node->mcgid, RSLT_FWD);

			/* Even the port joins the group but it may exclude/drop specified src.
			 * Search all nodes with specified src, and update the hash according to
			 * the combined mcgid.
			 */
			iter_node = src_node->group->next_src;
			while (iter_node != NULL) {
				if (iter_node == src_node) {
					iter_node = iter_node->next_src;
					continue;
				}

				tmp_mcgid = src_node->mcgid & ~(iter_node->mcgid);
				cs_mcast_del_hash(iter_node);
				cs_mcast_add_hash(iter_node, tmp_mcgid, RSLT_FWD);

				iter_node = iter_node->next_src;
			}

		} else { /* src_node->addr is not zero */
			/* add WITH_SRC hash to drop */
			/* src_node->mcgid contains the port bit to drop */

			/* search the node with empty addr for the mcgid */
			iter_node = src_node->group->next_src;
			while (iter_node != NULL) {
				if (is_addr_empty(&iter_node->addr)) {
					break;
				}

				iter_node = iter_node->next_src;
			}

			if (iter_node != NULL) {
				tmp_mcgid = iter_node->mcgid & ~(src_node->mcgid);
				cs_mcast_add_hash(src_node, tmp_mcgid, RSLT_FWD);
			} else {
				/* no src_node with empty addr, it means there is no host joins 
				 * this group and the multicast stream is dropped.
				 */
			}

		}

	} else {
		/* impossible path */
		return CS_E_ERROR;
	}

	return CS_E_OK;
}

/* update the hash that packet from upstream port to root port */
static cs_status_t cs_mcast_del_update_hash(struct cs_mcast_node *src_node, cs_uint16_t downstream_port)
{
	struct cs_mcast_node *iter_node;
	unsigned int tmp_mcgid;

	/* delete existing hash entry */
	cs_mcast_del_hash(src_node);

	src_node->mcgid &= ~(1 << downstream_port);

	/* check if this is last member to leave */
	if (src_node->mcgid == 0)
		return CS_E_OK;

	if (src_node->mode == CS_MCAST_INCLUDE) {

		if (is_addr_empty(&src_node->addr)) {
			/* add WITHOUT_SRC hash to drop */
			/* src_node->mcgid contains the port bit to drop */

			// It is not necessary to add hash for this case beacuse default rule is
			// to drop all unknown multicast.
			//cs_mcast_add_hash(src_node, src_node->mcgid, RSLT_DROP);

		} else { /* address is not zero */
			/* add WITH_SRC hash to fwd */
			/* src_node->mcgid contains the port bit to forward */

			cs_mcast_add_hash(src_node, src_node->mcgid, RSLT_FWD);

		}

	} else if (src_node->mode == CS_MCAST_EXCLUDE) {

		if (is_addr_empty(&src_node->addr)) {
			/* add WITHOUT_SRC hash to fwd */
			/* src_node->mcgid contains the port bit to forward */

			cs_mcast_add_hash(src_node, src_node->mcgid, RSLT_FWD);

			/* Even the port joins the group but it may exclude/drop specified src.
			 * Search all nodes with specified src, and update the hash according to
			 * the combined mcgid.
			 */
			iter_node = src_node->group->next_src;
			while (iter_node != NULL) {
				if (iter_node == src_node) {
					iter_node = iter_node->next_src;
					continue;
				}

				iter_node->mcgid &= ~(1 << downstream_port);
				tmp_mcgid = src_node->mcgid & ~(iter_node->mcgid); // filter out the excluded port
				cs_mcast_del_hash(iter_node);
				cs_mcast_add_hash(iter_node, tmp_mcgid, RSLT_FWD);

				iter_node = iter_node->next_src;
			}

		} else { /* src_node->addr is not zero */
			/* add WITH_SRC hash to drop */
			/* src_node->mcgid contains the port bit to drop */

			/* search the node with empty addr for the mcgid */
			iter_node = src_node->group->next_src;
			while (iter_node != NULL) {
				if (is_addr_empty(&iter_node->addr)) {
					break;
				}

				iter_node = iter_node->next_src;
			}

			if (iter_node != NULL) {
				tmp_mcgid = iter_node->mcgid & ~(src_node->mcgid); // filter out the excluded port
				cs_mcast_add_hash(src_node, tmp_mcgid, RSLT_FWD);
			} else {
				/* no src_node with empty addr, it means there is no host joins 
				 * this group and the multicast stream is dropped.
				 */
			}

		}

	} else {
		/* impossible path */
		return CS_E_ERROR;
	}

	return CS_E_OK;
}

/* search the src_node with src_addr, or create a new empty src_node belonged to group_node */
static struct cs_mcast_node* cs_mcast_get_src(struct cs_mcast_node *group_node, cs_ip_address_t *src_addr)
{
	struct cs_mcast_node *src_node;

	if ((src_node = mcast_search_src(group_node, src_addr)) == NULL) {
		if (src_addr->afi == CS_IPV4) {
			DBG(printk(KERN_INFO "%s: can not find src node for addr %pI4\n", __func__, &src_addr->ip_addr.ipv4_addr));
		} else {
			DBG(printk(KERN_INFO "%s: can not find src node for addr %pI6\n", __func__, &src_addr->ip_addr.ipv4_addr));
		}

		if ((src_node = cs_zalloc(sizeof(struct cs_mcast_node), GFP_KERNEL)) == NULL) {
			printk(KERN_INFO "%s: Can not alloc memory.\n", __func__);
			return NULL;
		}

		if (group_node->addr.afi == CS_IPV4) {
			memcpy(&src_node->addr.ip_addr.ipv4_addr, &src_addr->ip_addr.ipv4_addr, 4);
		} else {
			memcpy(&src_node->addr.ip_addr.ipv6_addr, &src_addr->ip_addr.ipv6_addr, 16);
		}

		src_node->addr.afi = group_node->addr.afi;
		src_node->type = CS_NODE_SOURCE;
		src_node->next_src = NULL;
		src_node->next_group = NULL;
		src_node->group = group_node;
		src_node->mcgid = 0;
		src_node->hash_idx = ~(0x0);
		src_node->fwdrslt_idx = ~(0x0);

		if (mcast_add_src(&group_node->addr, src_node) == NULL) {
			printk(KERN_INFO "%s: mcast_add_src fail\n", __func__);
			cs_free(src_node);
			return NULL;
		}
	}

	return src_node;
}

cs_status_t cs_l2_mcast_member_add( CS_IN cs_dev_id_t dev_id, 
                                   CS_IN cs_port_id_t port_id, 
                                   CS_IN cs_mcast_member_t *entry )
{
	int i;
	struct cs_mcast_node *group_node, *src_node;

	DBG(printk(KERN_INFO "%s: dev_id=%u, port_id=%u\n", __func__, dev_id, port_id));
	DBG(printk(KERN_INFO "%s: afi=%u, mode=%u, sub_port=%u, src_num=%u\n", __func__,
		entry->afi, entry->mode, entry->sub_port, entry->src_num));

	if (entry->afi == CS_IPV4) {
		DBG(printk(KERN_INFO "\t grp_addr=%pI4\n", &entry->grp_addr.ip_addr.ipv4_addr));
		for (i = 0; i < entry->src_num; i++) {
			DBG(printk(KERN_INFO "\t  src_list[%u]=%pI4\n", i, &entry->src_list[i].ip_addr.ipv4_addr));
		}
	} else {
		DBG(printk(KERN_INFO "\t grp_addr=%pI6\n", &entry->grp_addr.ip_addr.ipv6_addr));
		for (i = 0; i < entry->src_num; i++) {
			DBG(printk(KERN_INFO "\t  src_list[%u]=%pI6\n", i, &entry->src_list[i].ip_addr.ipv6_addr));
		}
	}

	group_node = mcast_add_group_by_ip(&entry->grp_addr);
	if (group_node == NULL) {
		printk(KERN_INFO "%s: Can not add group %pI6.\n", __func__, &entry->grp_addr.ip_addr.ipv6_addr);
		return CS_E_MCAST_ADDR_ADD_FAIL;
	}

	for (i = 0; i < entry->src_num; i++) {
		src_node = cs_mcast_get_src(group_node, &entry->src_list[i]);
		cs_mcast_add_update_hash(src_node, port_id, entry->mode);

		mcast_dump_list();
	}

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_member_add);

cs_status_t cs_l2_mcast_member_delete( CS_IN cs_dev_id_t dev_id, 
                                      CS_IN cs_port_id_t port_id, 
                                      CS_IN cs_mcast_member_t *entry )
{
	int i;
	struct cs_mcast_node *group_node, *src_node;

	DBG(printk(KERN_INFO "%s: dev_id=%u, port_id=%u\n", __func__, dev_id, port_id));
	DBG(printk(KERN_INFO "%s: afi=%u, grp_addr=0x%08x, mode=%u, sub_port=%u, src_num=%u\n", __func__,
		entry->afi, entry->grp_addr.ip_addr.ipv4_addr, entry->mode, entry->sub_port, entry->src_num));

	if (entry->afi == CS_IPV4) {
		DBG(printk(KERN_INFO "\t grp_addr=%pI4 \n", &entry->grp_addr.ip_addr.ipv4_addr));
		for (i = 0; i < entry->src_num; i++) {
			DBG(printk(KERN_INFO "\t  src_list[%u]=%pI4\n", i, &entry->src_list[i].ip_addr.ipv4_addr));
		}
	} else {
		DBG(printk(KERN_INFO "\t grp_addr=%pI6 \n", &entry->grp_addr.ip_addr.ipv6_addr));
		for (i = 0; i < entry->src_num; i++) {
			DBG(printk(KERN_INFO "\t  src_list[%u]=%pI6\n", i, &entry->src_list[i].ip_addr.ipv6_addr));
		}
	}

	group_node = mcast_search_group(&entry->grp_addr);
	if (group_node == NULL) {
		printk(KERN_INFO "%s: Can not leave nonexistent group %pI6.\n", __func__, &entry->grp_addr.ip_addr.ipv6_addr);
		return CS_E_MCAST_ADDR_DELETE_FAIL;
	}

	for (i = 0; i < entry->src_num; i++) {
		src_node = cs_mcast_get_src(group_node, &entry->src_list[i]);
		cs_mcast_del_update_hash(src_node, port_id);

		mcast_dump_list();
	}

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_member_delete);

cs_status cs_l2_mcast_port_member_num_get(CS_IN cs_dev_id_t dev_id,
					   CS_IN cs_port_id_t port_id,
					   CS_OUT cs_uint16_t *num)
{
	struct cs_mcast_node *group_node, *src_node;
	int count = 0;
	int port_bit = (1 << port_id);

	DBG(printk(KERN_INFO "%s: dev_id=%u, port_id=%u\n", __func__, dev_id, port_id));

	/* search how many groups are subscribed from the port_id */
	group_node = head->next_group;

	while (group_node != NULL) {
		src_node = group_node->next_src;

		while (src_node != NULL) {
			if (src_node->mcgid & port_bit) {
				count++;
				break;
			}

			src_node = src_node->next_src;
		}

		group_node = group_node->next_group;
	}

	*num = count;

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_port_member_num_get);

cs_status cs_l2_mcast_port_member_entry_get(CS_IN cs_dev_id_t dev_id,
					     CS_IN cs_port_id_t port_id,
					     CS_IN_OUT cs_uint16_t *num,
					     CS_OUT cs_mcast_member_t *entry)
{
	struct cs_mcast_node *group_node, *src_node;
	int count = 0;
	cs_mcast_member_t *entry_p = entry;
	int port_bit = (1 << port_id);
	cs_uint16_t src_num;
        int found = 0; // found matched port_bit in this src_node

	DBG(printk(KERN_INFO "%s: dev_id=%u, port_id=%u, num=%u\n", __func__, dev_id, port_id, *num));

	if (entry == NULL) {
		*num = 0;
		return CS_E_NONE;
	}

	group_node = head->next_group;

	while ((group_node != NULL) && (count < *num)) {
		src_node = group_node->next_src;

		src_num = 0;

		while (src_node != NULL) {
			if (src_node->mcgid & port_bit) {
				found = 1;
				entry_p->afi = src_node->addr.afi;
				memcpy(&entry_p->grp_addr, &group_node->addr, sizeof(cs_ip_address_t));
				memcpy(&entry_p->src_list[src_num], &src_node->addr, sizeof(cs_ip_address_t));
				entry_p->mode = src_node->mode;
				src_num++;
				count++;
			}
			src_node = src_node->next_src;
		}

		entry_p->src_num = src_num;
		if (found != 0) {
			entry_p++;
			found = 0;
		}
		group_node = group_node->next_group;
	}

	*num = count;

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_port_member_entry_get);

cs_status_t cs_l2_mcast_port_member_clear( CS_IN cs_dev_id_t dev_id, 
                                          CS_IN cs_port_id_t port_id )
{
	struct cs_mcast_node *group_node, *src_node;

	DBG(printk(KERN_INFO "%s: dev_id=%u, port_id=%u\n", __func__, dev_id, port_id));

	group_node = head->next_group;

	while (group_node != NULL) {
		src_node = group_node->next_src;

		while (src_node != NULL) {
			cs_mcast_del_update_hash(src_node, port_id);
			src_node = src_node->next_src;
		}

		group_node = group_node->next_group;
	}

	mcast_dump_list();

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_port_member_clear);

cs_status_t cs_l2_mcast_member_all_clear(CS_IN cs_dev_id_t dev_id)
{
	struct cs_mcast_node *group_node, *src_node;
	struct cs_mcast_node *group_tmp, *src_tmp;

	DBG(printk(KERN_INFO "%s: dev_id=%u\n", __func__, dev_id));

	group_node = head->next_group;

	while (group_node != NULL) {
		src_node = group_node->next_src;

		while (src_node != NULL) {
			if (src_node->mcgid != 0) {
				printk(KERN_INFO "%s: delete hash_idx=%u\n", __func__, src_node->hash_idx);
				cs_fe_hash_del_hash(src_node->hash_idx);
				cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, src_node->fwdrslt_idx, false);
			}

			src_tmp = src_node;
			src_node = src_node->next_src;
			cs_free(src_tmp);
		}

		group_tmp = group_node;
		group_node = group_node->next_group;
		cs_free(group_tmp);
	}

	head->next_group = NULL;
	head->next_src = NULL;

	mcast_dump_list();

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_member_all_clear);

cs_status_t cs_l2_mcast_mode_set( CS_IN cs_dev_id_t dev_id, 
                                CS_IN cs_mcast_mode mode )
{
	DBG(printk(KERN_INFO "%s: dev_id=%u, mode=%d\n", __func__, dev_id, mode));

	switch (mode) {
	case CS_MCAST_PORT_REPLICATION:
		mcast_mode = mode;
		//TODO
		break;
	case CS_MCAST_ARBITRARY_REPLICATION:
		mcast_mode = mode;
		//TODO
	default:
		printk(KERN_INFO "%s: unknown multicast mode=%d\n", __func__, mode);
		return CS_E_ERROR;
	}

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l2_mcast_mode_set);

int cs_mcast_create_mcgid_hash(struct cs_mcgid_bit * mcgid_entry) {

	int i = mcgid_entry->ifidx;
	fe_sw_hash_t key;
	u32 crc32;
	u16 crc16;
	int ret;

	if ((mcgid_entry->mode == CS_MC_BIT_UNUSED) || (mcgid_entry->mode == CS_MC_BIT_SWONLY))
			return 0;
	if (mcgid_entry->valid == 1) {
		mcgid_entry->valid = 0;
		if (mcgid_entry->hash_idx != (uint16_t) ~(0x0)) {
			cs_fe_hash_del_hash(mcgid_entry->hash_idx);
			mcgid_entry->hash_idx = ~(0x0);
		}
		if (mcgid_entry->fwdrslt_idx != (unsigned int) ~(0x0)) {
			cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, mcgid_entry->fwdrslt_idx, false);
			mcgid_entry->fwdrslt_idx = ~(0x0);
		}
	}

	memset(&mcgid_entry->voqpol_entry, 0x0, sizeof(fe_voq_pol_entry_t));
	mcgid_entry->voqpol_entry.voq_base = mcgid_entry->port;
	if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &(mcgid_entry->voqpol_entry),
			&(mcgid_entry->voqpol_idx))) {
		printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		return -1;
	}

	memset(&mcgid_entry->fwdrslt_entry, 0x0, sizeof(fe_fwd_result_entry_t));
	mcgid_entry->fwdrslt_entry.dest.voq_policy = 0;
	mcgid_entry->fwdrslt_entry.dest.voq_pol_table_index = mcgid_entry->voqpol_idx;
	if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &mcgid_entry->fwdrslt_entry,
			&mcgid_entry->fwdrslt_idx)) {
		printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, mcgid_entry->voqpol_idx, false);
		return -1;
	}

	// CS_HASHMASK_MCAST_TO_DEST
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.lspid = MCAST_PORT;
	key.mc_idx = i | (1 << 4); // port replication mode
	key.ip_valid = 1;
	//key.ip_version = 0; /* reuse hash for ipv4/ipv6 */
	key.mask_ptr_0_7 = mcast_to_dest_hm_idx;

	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
		return ret;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, mcast_to_dest_hm_idx,
		mcgid_entry->fwdrslt_idx, &mcgid_entry->hash_idx);
	if (ret != 0) {
		printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
		return ret;
	}
	printk(KERN_DEBUG "%s: mc_idx=%u, hm_idx=%u, voqpol_idx[%d]=%u, fwdrslt_idx[%d]=%u, hash_idx=%u\n",
		__func__, key.mc_idx, mcast_to_dest_hm_idx, i, mcgid_entry->voqpol_idx, i, mcgid_entry->fwdrslt_idx, mcgid_bit->hash_idx);

	cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT, mcgid_entry->fwdrslt_idx);
	mcgid_entry->valid = 1;
	return 0;
}

cs_status_t cs_l2_mcast_wan_port_id_set( CS_IN cs_dev_id_t dev_id, 
                                       CS_IN cs_port_id_t port_id )
{
	if (port_id == NI_GE0)
		mcgid_bit[NI_CPU].port = CPU_PORT0_VOQ_BASE + 7;
	else if (port_id == NI_GE1)
		mcgid_bit[NI_CPU].port = CPU_PORT1_VOQ_BASE + 7;
	else if (port_id == NI_GE2)
		mcgid_bit[NI_CPU].port = CPU_PORT2_VOQ_BASE + 7;
	else if (port_id == 0xFF) {
    	cs_mc_wan_port_id = port_id;
    	cs_l2_mcast_member_all_clear(0);
    	return CS_E_OK;
	} else {
		printk(KERN_INFO "%s: doesn't support port_id=%u\n", __func__, port_id);
		return CS_E_ERROR;
	}
	cs_mc_wan_port_id = port_id;
	cs_l2_mcast_member_all_clear(0);

	if (cs_mcast_create_mcgid_hash(&mcgid_bit[NI_CPU]))
		return CS_E_ERROR;
	else
		return CS_E_OK;
}
EXPORT_SYMBOL(cs_l2_mcast_wan_port_id_set);

cs_status_t cs_l2_mcast_wan_port_id_get( CS_IN  cs_dev_id_t dev_id,
                                       CS_OUT cs_port_id_t *port_id)
{
    if (!port_id) {
        return CS_E_ERROR;
    }
    
    *port_id = (cs_port_id_t)cs_mc_wan_port_id;
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l2_mcast_wan_port_id_get);

int cs_mcast_init(void)
{
	fe_voq_pol_entry_t rootport_voqpol_entry;
	fe_sw_hash_t key;
	u16 hash_index;
	u32 crc32;
	u16 crc16;
	int i;
	int ret;

	spin_lock_init(&list_lock);

	if ((head = cs_zalloc(sizeof(struct cs_mcast_node), GFP_KERNEL)) == NULL) {
		printk(KERN_INFO "%s: Can not alloc memory.\n", __func__);
		return CS_E_INIT;
	}
	head->next_group = NULL;
	head->next_src = NULL;
	head->group = NULL;

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_IP_PROT, &igmp_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_IP_PROT\n", __func__);
		return CS_E_INIT;
	}

	icmpv6_hm_idx = igmp_hm_idx;

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_MCAST_WITHOUT_SRC, &mcast_without_src_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_MCAST_WITHOUT_SRC\n", __func__);
		return CS_E_INIT;
	}

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_MCAST_WITH_SRC, &mcast_with_src_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_MCAST_WITH_SRC\n", __func__);
		return CS_E_INIT;
	}

	if (cs_core_vtable_get_hashmask_index_from_apptype(
			CORE_FWD_APP_TYPE_MCAST_TO_DEST, &mcast_to_dest_hm_idx)) {
		printk(KERN_INFO "%s: Can not get hash mask for CORE_FWD_APP_TYPE_MCAST_TO_DEST\n", __func__);
		return CS_E_INIT;
	}

	/*** IGMP ***/

	/* hash forward result for each GE port */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&igmp_voqpol_entry[i], 0x0, sizeof(fe_voq_pol_entry_t));
		igmp_voqpol_entry[i].voq_base = CPU_PORT0_VOQ_BASE + 5 + i * 8; // CPU VoQ#5
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &igmp_voqpol_entry[i],
				&igmp_voqpol_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		}

		memset(&igmp_fwdrslt_entry[i], 0x0, sizeof(fe_fwd_result_entry_t));
		igmp_fwdrslt_entry[i].dest.voq_policy = 0;
		igmp_fwdrslt_entry[i].dest.voq_pol_table_index = igmp_voqpol_idx[i];
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &igmp_fwdrslt_entry[i],
				&igmp_fwdrslt_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, igmp_voqpol_idx[i], false);
		}
	}

	/* hash entry for IGMP packet */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&key, 0x0, sizeof(fe_sw_hash_t));
		key.lspid = GE_PORT0 + i;
		key.ip_prot = PROTO_IGMP;
		key.ip_valid = 1;
		key.mask_ptr_0_7 = igmp_hm_idx;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
			return CS_E_INIT;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, igmp_hm_idx,
			igmp_fwdrslt_idx[i], &hash_index);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
			return CS_E_INIT;
		}

		DBG(printk(KERN_INFO "%s: igmp_hm_idx=%u, igmp_fwdrslt_idx[%d]=%u, hash_index=%u\n",
				__func__, igmp_hm_idx, i, igmp_fwdrslt_idx[i], hash_index));

		cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT, igmp_fwdrslt_idx[i]);
	}

	/*** ICMPv6 ***/

	/* hash forward result for each GE port; same forward result as IGMP */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&icmpv6_voqpol_entry[i], 0x0, sizeof(fe_voq_pol_entry_t));
		icmpv6_voqpol_entry[i].voq_base = CPU_PORT0_VOQ_BASE + 5 + i * 8; // CPU VoQ#5
		if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &icmpv6_voqpol_entry[i],
				&icmpv6_voqpol_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
		}

		memset(&icmpv6_fwdrslt_entry[i], 0x0, sizeof(fe_fwd_result_entry_t));
		icmpv6_fwdrslt_entry[i].dest.voq_policy = 0;
		icmpv6_fwdrslt_entry[i].dest.voq_pol_table_index = icmpv6_voqpol_idx[i];
		if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &icmpv6_fwdrslt_entry[i],
				&icmpv6_fwdrslt_idx[i])) {
			printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, icmpv6_voqpol_idx[i], false);
		}
	}

	/* hash entry for ICMPv6 packet */
	for (i = 0; i < GE_PORT_NUM; i++) {
		memset(&key, 0x0, sizeof(fe_sw_hash_t));
		key.lspid = GE_PORT0 + i;
		key.ip_prot = PROTO_ICMPV6;
		key.ip_valid = 1;
		key.mask_ptr_0_7 = icmpv6_hm_idx;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_calc_crc() fails!\n", __func__);
			return CS_E_INIT;
		}

		ret = cs_fe_hash_add_hash(crc32, crc16, icmpv6_hm_idx,
			icmpv6_fwdrslt_idx[i], &hash_index);
		if (ret != 0) {
			printk(KERN_INFO "%s: cs_fe_hash_add_hash() fails!\n", __func__);
			return CS_E_INIT;
		}

		DBG(printk(KERN_INFO "%s: icmpv6_hm_idx=%u, icmpv6_fwdrslt_idx[%d]=%u, hash_index=%u\n",
				__func__, icmpv6_hm_idx, i, icmpv6_fwdrslt_idx[i], hash_index));

		cs_fe_table_inc_entry_refcnt(FE_TABLE_FWDRSLT, icmpv6_fwdrslt_idx[i]);
	}

	/*** static voqpol result: redirect to root port ***/

	memset(&rootport_voqpol_entry, 0x0, sizeof(fe_voq_pol_entry_t));
	rootport_voqpol_entry.voq_base = ROOT_PORT_VOQ_BASE;
	if (cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, &rootport_voqpol_entry,
			&rootport_voqpol_idx)) {
		printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) error\n", __func__);
	}

//	/*** static forward result: from root port to downstream port ***/
//	mcgid_bit[NI_CPU].port = CPU_PORT0_VOQ_BASE + (cs_mc_wan_port_id * 8) + 7;

	for (i = 0; i < CS_MCGID_SIZE; i++) {
		cs_mcast_create_mcgid_hash(&mcgid_bit[i]);
	}

	/*** static forward result: from upstream port to drop result ***/

	memset(&drop_fwdrslt_entry, 0x0, sizeof(fe_fwd_result_entry_t));
	drop_fwdrslt_entry.act.drop = 1;
	if (cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &drop_fwdrslt_entry, &drop_fwdrslt_idx)) {
		printk(KERN_INFO "%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) error\n", __func__);
	}

	return CS_E_NONE;
}

