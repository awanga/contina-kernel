#include <mach/cs_types.h>
#include "cs_core_logic.h"

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <net/ipv6.h>
#include <linux/mroute.h>
#include <linux/mroute6.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>


#include <linux/skbuff.h>
#include "cs_hw_accel_manager.h"

#include "cs_core_logic_data.h"
#include "cs_core_vtable.h"
#include <mach/cs75xx_fe_core_table.h>

#include "cs_core_logic.h"
#include "cs_fe_mc.h"
#include "cs_core_hmu.h"

#include "cs_wfo_csme.h"

#include "cs_mut.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;

#define DBG(x) {if (cs_adapt_debug & (CS752X_ADAPT_IPV4_MULTICAST | CS752X_ADAPT_IPV6_MULTICAST)) x;}
#define DBG_IPv4(x) {if (cs_adapt_debug & CS752X_ADAPT_IPV4_MULTICAST) x;}
#define DBG_IPv6(x) {if (cs_adapt_debug & CS752X_ADAPT_IPV6_MULTICAST) x;}
#else
#define DBG(x) {}
#define DBG_IPv4(x) {}
#define DBG_IPv6(x) {}
#endif

cs_core_hmu_t ipv4_mc_hmu_entry;
cs_core_hmu_value_t ipv4_mc_hmu_value_ip_da;
cs_core_hmu_value_t ipv4_mc_hmu_value_egress_port_id;

cs_core_hmu_t ipv6_mc_hmu_entry;
cs_core_hmu_value_t ipv6_mc_hmu_value_ip_da;
cs_core_hmu_value_t ipv6_mc_hmu_value_egress_port_id;


#if 0
struct mr6_table {
	struct list_head	list;
#ifdef CONFIG_NET_NS
	struct net		*net;
#endif
	u32			id;
	struct sock		*mroute6_sk;
	struct timer_list	ipmr_expire_timer;
	struct list_head	mfc6_unres_queue;
	struct list_head	mfc6_cache_array[MFC6_LINES];
	struct mif_device	vif6_table[MAXMIFS];
	int			maxvif;
	atomic_t		cache_resolve_queue_len;
	int			mroute_do_assert;
	int			mroute_do_pim;
#ifdef CONFIG_IPV6_PIMSM_V2
	int			mroute_reg_vif_num;
#endif
};
#endif

/*FIXME: check if can export struct mr_table from ipmr.c */

#define CS_MC_TABLE_MAX_ENTRY (256)
//#define CS_MC_TABLE_MAX_PHYSICAL_PORT (3)
#define CS_MC_TABLE_MAX_PHYSICAL_PORT (8)
#define CS_MC_TABLE_HASH_CREATE_NONE (0)
#define CS_MC_TABLE_HASH_CREATE_FAIL (1)
#define CS_MC_TABLE_HASH_CREATE_PASS (2)

#define MCAL_TABLE_SIZE         256

static struct list_head cs_mc_groupip_lookup_table_base[CS_MC_TABLE_MAX_ENTRY];
static spinlock_t cs_mc_groupip_lookup_table_lock;

//A9 local MCAL MAC DA mapping table
static cs_mc_sta_mac_entry_s mac_table_mc[CS_MC_STA_ENTRY_SIZE];
static spinlock_t cs_mc_mac_lookup_table_lock;


typedef struct cs_mc_groupip_lookup_table_s {
	u32 ip_addr[4];
	bool is_v6;
	u16 physical_port[CS_MC_TABLE_MAX_PHYSICAL_PORT];
	struct list_head list;
} cs_mc_groupip_lookup_table_t;

void cs_mc_groupip_lookup_table_init(void)
{
	int i;

	for (i = 0; i < CS_MC_TABLE_MAX_ENTRY; i++)
		INIT_LIST_HEAD(&cs_mc_groupip_lookup_table_base[i]);

	spin_lock_init(&cs_mc_groupip_lookup_table_lock);
}

u16 cs_mc_crc16(u8 *input_data, u16 len)
{
	int i;
	u16 crc16 = 0;
	u8 *data = input_data;

	for (i = 0; i < len; i++) {
		crc16 = crc16 + (*data);
		data++;
	}
	return (crc16 & 0xFF);
}

void cs_mc_groupip_lookup_table_dump(void)
{
	int i, j;
	cs_mc_groupip_lookup_table_t *entry;
	struct list_head *table;

	DBG(printk("%s start - \n", __func__));
	for (i = 0; i < CS_MC_TABLE_MAX_ENTRY; i++) {
		table = &cs_mc_groupip_lookup_table_base[i];
		if (!list_empty(table)) {
			list_for_each_entry(entry, table, list) {
				if (entry->is_v6) {
					DBG(printk("\t %d entry %pI64,", i, entry->ip_addr));
				} else {
					DBG(printk("\t %d entry %pI4,", i, entry->ip_addr));
				}

				for (j = 0; j < CS_MC_TABLE_MAX_PHYSICAL_PORT; j++) {
					DBG(printk("port%d (%d),", j , entry->physical_port[j]));
				}

				DBG(printk("\n"));
			}
		}
	}
	DBG(printk("%s end - \n", __func__));
}

void cs_mc_groupip_lookup_table_delete(u32 *ip_addr, bool is_v6)
{
	u16 crc_idx;
	u32 ip_addr_tmp[4];
	u8 *data;
	cs_mc_groupip_lookup_table_t *entry;
	struct list_head *table;
	int len = is_v6 ? 16 : 4;

	memset(ip_addr_tmp, 0, sizeof(ip_addr_tmp));

	memcpy(ip_addr_tmp, ip_addr, len);

	data = (u8*)ip_addr_tmp;

	crc_idx = cs_mc_crc16(data, len);

	spin_lock(&cs_mc_groupip_lookup_table_lock);
	table = &cs_mc_groupip_lookup_table_base[crc_idx];
	if (!list_empty(table)) {
		list_for_each_entry(entry, table, list) {
			if ((entry->is_v6 == is_v6) &&
				(memcmp(entry->ip_addr,
						ip_addr_tmp, len) == 0)) {
				list_del_init(&entry->list);
				cs_free(entry);
				spin_unlock(&cs_mc_groupip_lookup_table_lock);
				return;
			}
		}
	}
	spin_unlock(&cs_mc_groupip_lookup_table_lock);
}

cs_mc_groupip_lookup_table_t *__cs_mc_groupip_lookup_table_search(u32 *ip_addr,
		bool is_v6, struct list_head ** ptable)

{
	u16 crc_idx;
	u32 ip_addr_tmp[4];
	u8 *data;
	int len = is_v6 ? 16 : 4;
	cs_mc_groupip_lookup_table_t *entry;
	struct list_head *table;

	*ptable = NULL;
	memset(ip_addr_tmp, 0, sizeof(ip_addr_tmp));
	memcpy(ip_addr_tmp, ip_addr, len);
	data = (u8*)ip_addr_tmp;
	crc_idx = cs_mc_crc16(data, len);
	table = &cs_mc_groupip_lookup_table_base[crc_idx];

	DBG(printk("\t %s search crc=%d %s  - ", __func__, crc_idx,
		is_v6? "v6":"v4"));

	*ptable = table;
	if (!list_empty(table)) {
		list_for_each_entry(entry, table, list) {
			if ((entry->is_v6 == is_v6) &&
				(memcmp(entry->ip_addr,
						ip_addr_tmp, len) == 0)) {
				DBG(printk(" found entry %d %d %d\n",
					entry->physical_port[0], entry->physical_port[1],
					entry->physical_port[2]));
				return entry;
			}
		}
	}
	DBG(printk(" not found entry\n"));
	return NULL;
}

bool cs_mc_groupip_lookup_table_update(u32 *ip_addr, bool is_v6,
	int hash_result, int physical_port)
{
	int i;
	struct list_head *table;
	cs_mc_groupip_lookup_table_t *entry;

	if ((physical_port >= CS_MC_TABLE_MAX_PHYSICAL_PORT) ||
			(physical_port < 0))
		return false;

	DBG(printk("%s - \n", __func__));
	spin_lock(&cs_mc_groupip_lookup_table_lock);

	entry = __cs_mc_groupip_lookup_table_search(ip_addr, is_v6, &table);
	if (entry == NULL) {
		if (hash_result == CS_MC_TABLE_HASH_CREATE_NONE) {
			spin_unlock(&cs_mc_groupip_lookup_table_lock);
			return true;
		} else {
			/*if not find entry, create a new one*/
			entry = cs_zalloc(sizeof(cs_mc_groupip_lookup_table_t), GFP_ATOMIC);
			if (entry == NULL) {
				spin_unlock(&cs_mc_groupip_lookup_table_lock);
				return false;
			}

			list_add_tail(&entry->list, table);
			memcpy(entry->ip_addr, ip_addr, (is_v6 ? 16 : 4));
			entry->is_v6 = is_v6;
		}
	}

	if (hash_result != CS_MC_TABLE_HASH_CREATE_PASS) {
		entry->physical_port[physical_port] = hash_result;
	} else {
		for (i = 0; i < CS_MC_TABLE_MAX_PHYSICAL_PORT ; i++) {
			if ((physical_port != i) &&
					(entry->physical_port[i] ==
						CS_MC_TABLE_HASH_CREATE_FAIL)) {
				/*previous create hash fail*/
				spin_unlock(&cs_mc_groupip_lookup_table_lock);
				return false;
			}
		}
		entry->physical_port[physical_port] = CS_MC_TABLE_HASH_CREATE_PASS;
	}
	DBG(printk("\t update port[%d] = %d \n", physical_port,
			entry->physical_port[physical_port]));
	spin_unlock(&cs_mc_groupip_lookup_table_lock);
	return true;
}

void cs_mc_groupip_lookup_table_reset(u32 *ip_addr, bool is_v6)
{
	cs_mc_groupip_lookup_table_t *entry;
	struct list_head *table;

	DBG(printk("%s - \n", __func__));

	spin_lock(&cs_mc_groupip_lookup_table_lock);
	entry = __cs_mc_groupip_lookup_table_search(ip_addr, is_v6, &table);
	if (entry != NULL)
		memset(entry->physical_port, 0, sizeof(entry->physical_port));
	spin_unlock(&cs_mc_groupip_lookup_table_lock);
}

void cs_mc_groupip_lookup_table_flush(bool is_v6)
{
	int i;
	cs_mc_groupip_lookup_table_t *entry;
	struct list_head *pos, *q;
	struct list_head *table;

	DBG(printk("%s start - \n", __func__));
	spin_lock(&cs_mc_groupip_lookup_table_lock);
	for (i = 0; i < CS_MC_TABLE_MAX_ENTRY; i++) {
		table = &cs_mc_groupip_lookup_table_base[i];

		if (!list_empty(table)) {
			list_for_each_safe(pos, q, table){
				entry = list_entry(pos, cs_mc_groupip_lookup_table_t, list);
				if (entry->is_v6 == is_v6) {
					list_del_init(&entry->list);
					cs_free(entry);
				}
			}
		}
	}
	spin_unlock(&cs_mc_groupip_lookup_table_lock);
	DBG(printk("%s end - \n", __func__));
}

int cs_mc_ipv4_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPV4_MULTICAST);
}

int cs_mc_ipv6_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPV6_MULTICAST);
}

void k_jt_cs_mc_ipmr_set_opt_mrt_init(struct mr_table *mrt)
{
	/*lunch ipv4 multicast daemon*/
}

void k_jt_cs_mc_ipmr_set_opt_mrt_done(struct mr_table *mrt)
{

	/*remove ipv4 multicast daemon*/
	if (cs_mc_ipv4_enable() == 0)
		return;

	cs_core_hmu_clean_watch(&ipv4_mc_hmu_entry);
	cs_mc_groupip_lookup_table_flush(false);
}

#if 0 /* not used */
static struct mfc_cache *__cs_mc_ipmr_cache_find(struct mr_table *mrt,
					 __be32 origin,
					 __be32 mcastgrp)
{
	int line = MFC_HASH(mcastgrp, origin);
	struct mfc_cache *c;

	list_for_each_entry(c, &mrt->mfc_cache_array[line], list) {
		// FIXME!! Wen. The following logic might confuse compiler
		// Code maintainer please double check
		if (c->mfc_origin == origin && c->mfc_mcastgrp == mcastgrp)
			return c;
	}
	return NULL;
}
#endif

void __cs_mc_delete_hash_by_ipv6_group(u32 *origin, u32 *mcastgrp)
{
	cs_core_hmu_value_t hmu_value_ip_da;
	cs_core_hmu_value_t hmu_value_dst_port_id;
	struct list_head *table;
	u16 port_status[CS_MC_TABLE_MAX_PHYSICAL_PORT];
	int i;
	cs_mc_groupip_lookup_table_t *entry;

	DBG_IPv6(printk("%s\n", __func__));

	memset(&hmu_value_ip_da, 0, sizeof(hmu_value_ip_da));
	memset(&hmu_value_dst_port_id, 0, sizeof(hmu_value_dst_port_id));

	hmu_value_ip_da.type = CS_CORE_HMU_WATCH_IN_IPV6_DA;
	hmu_value_ip_da.mask = 8;
	memcpy(&hmu_value_ip_da.value.ip_addr[0], mcastgrp, 16);

	hmu_value_ip_da.next = &hmu_value_dst_port_id;
	hmu_value_dst_port_id.type = CS_CORE_HMU_WATCH_DST_PHY_PORT;

	spin_lock(&cs_mc_groupip_lookup_table_lock);
	entry = __cs_mc_groupip_lookup_table_search((u32 *)mcastgrp, true,
			&table);
	if (entry != NULL) {
		for (i = 0; i < CS_MC_TABLE_MAX_PHYSICAL_PORT; i++) {
			if (entry->physical_port[i] == CS_MC_TABLE_HASH_CREATE_PASS)
				port_status[i] = CS_MC_TABLE_HASH_CREATE_PASS;
			else
				port_status[i] = 0;
		}
	}
	spin_unlock(&cs_mc_groupip_lookup_table_lock);

	for (i = 0; i < CS_MC_TABLE_MAX_PHYSICAL_PORT; i++) {
		if (port_status[i] == CS_MC_TABLE_HASH_CREATE_PASS) {
			hmu_value_dst_port_id.value.index = i;
			cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_IPV6_DA
					   | CS_CORE_HMU_WATCH_DST_PHY_PORT, &hmu_value_ip_da);
		}
	}
}


void __cs_mc_delete_hash_by_ipv4_group(__be32 origin, __be32 mcastgrp)
{
	cs_core_hmu_value_t hmu_value_ip_da;
	cs_core_hmu_value_t hmu_value_dst_port_id;
	struct list_head *table;
	u16 port_status[CS_MC_TABLE_MAX_PHYSICAL_PORT];
	int i;
	cs_mc_groupip_lookup_table_t *entry;

	DBG_IPv4(printk("%s\n", __func__));

	memset(&hmu_value_ip_da, 0, sizeof(hmu_value_ip_da));
	memset(&hmu_value_dst_port_id, 0, sizeof(hmu_value_dst_port_id));

	hmu_value_ip_da.type = CS_CORE_HMU_WATCH_IN_IPV4_DA;
	hmu_value_ip_da.mask = 4;
	hmu_value_ip_da.value.ip_addr[0] = mcastgrp;
	hmu_value_dst_port_id.type = CS_CORE_HMU_WATCH_DST_PHY_PORT;
	hmu_value_dst_port_id.value.index = 0;
	hmu_value_ip_da.next = &hmu_value_dst_port_id;

	spin_lock(&cs_mc_groupip_lookup_table_lock);
	entry = __cs_mc_groupip_lookup_table_search(&mcastgrp, false, &table);
	if (entry != NULL) {
		for (i = 0; i < CS_MC_TABLE_MAX_PHYSICAL_PORT; i++) {
			if (entry->physical_port[i] == CS_MC_TABLE_HASH_CREATE_PASS)
				port_status[i] = CS_MC_TABLE_HASH_CREATE_PASS;
			else
				port_status[i] = 0;
		}
	}
	spin_unlock(&cs_mc_groupip_lookup_table_lock);

	for (i = 0; i < CS_MC_TABLE_MAX_PHYSICAL_PORT; i++) {
		if (port_status[i] == CS_MC_TABLE_HASH_CREATE_PASS) {
			hmu_value_dst_port_id.value.index = i;
			DBG(printk("\t delete ipaddr %pI4 port %d\n",
					hmu_value_ip_da.value.ip_addr,
					hmu_value_dst_port_id.value.index));
			cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_IPV4_DA
					   | CS_CORE_HMU_WATCH_DST_PHY_PORT, &hmu_value_ip_da);
		}
	}
}


void k_jt_cs_mc_ipmr_set_opt_add_mfc_before(struct mr_table *mrt,
		struct mfcctl *mfc)
{
	if (cs_mc_ipv4_enable() == 0)
		return;

	if (mfc == NULL)
		return;

	DBG_IPv4(printk("\t origin=%pI4 mcastgr=%pI4\n",
			&mfc->mfcc_origin.s_addr, &mfc->mfcc_mcastgrp.s_addr));
	/*put the kernel hook to the place after check MFC cache is not NULL*/

	__cs_mc_delete_hash_by_ipv4_group(mfc->mfcc_origin.s_addr,
			mfc->mfcc_mcastgrp.s_addr);
	cs_mc_groupip_lookup_table_reset(&mfc->mfcc_mcastgrp.s_addr, false);
}

void k_jt_cs_mc_ipmr_set_opt_del_mfc(struct mr_table *mrt, struct mfcctl *mfc)
{
	if (cs_mc_ipv4_enable() == 0)
		return;

	if (mfc == NULL)
		return;

	DBG_IPv4(printk("%s	origin=%pI4 mcastgr=%pI4\n", __func__,
				&mfc->mfcc_origin.s_addr, &mfc->mfcc_mcastgrp.s_addr));

	__cs_mc_delete_hash_by_ipv4_group(mfc->mfcc_origin.s_addr,
		mfc->mfcc_mcastgrp.s_addr);
	cs_mc_groupip_lookup_table_delete(&mfc->mfcc_mcastgrp.s_addr, false);
}

void cs_hw_accel_mc_delete_hash_by_ipv4_group(__be32 mcastgrp)
{
	__cs_mc_delete_hash_by_ipv4_group(NULL, mcastgrp);
}

void cs_hw_accel_mc_delete_hash_by_ipv6_group(__be32 * p_mcastgrp)
{
	__cs_mc_delete_hash_by_ipv6_group(NULL, p_mcastgrp);
}
EXPORT_SYMBOL(cs_hw_accel_mc_delete_hash_by_ipv4_group);
EXPORT_SYMBOL(cs_hw_accel_mc_delete_hash_by_ipv6_group);


EXPORT_SYMBOL(k_jt_cs_mc_ipmr_set_opt_mrt_init);
EXPORT_SYMBOL(k_jt_cs_mc_ipmr_set_opt_mrt_done);
EXPORT_SYMBOL(k_jt_cs_mc_ipmr_set_opt_add_mfc_before);
EXPORT_SYMBOL(k_jt_cs_mc_ipmr_set_opt_del_mfc);

void k_jt_cs_mc_ip6mr_set_opt_mrt_init(struct mr6_table *mrt)
{
	/*lunch ipv6 multicast daemon*/
}

void k_jt_cs_mc_ip6mr_set_opt_mrt_done(struct mr6_table *mrt)
{
	/*remove ipv6 multicast daemon*/
	if (cs_mc_ipv6_enable() == 0)
		return;

	cs_core_hmu_clean_watch(&ipv4_mc_hmu_entry);
	cs_mc_groupip_lookup_table_flush(false);
}

void k_jt_cs_mc_ip6mr_set_opt_add_mfc_before(struct mr6_table *mrt, struct mf6cctl *mfc)
{
	if (cs_mc_ipv6_enable() == 0)
		return;

	if (mfc == NULL)
		return;

	DBG_IPv6(printk("\t origin=%pI64 mcastgr=%pI64\n",
			mfc->mf6cc_origin.sin6_addr.s6_addr32,
			mfc->mf6cc_mcastgrp.sin6_addr.s6_addr32));
	/*put the kernel hook to the place after check MFC cache is not NULL*/

	__cs_mc_delete_hash_by_ipv6_group(mfc->mf6cc_origin.sin6_addr.s6_addr32,
			mfc->mf6cc_mcastgrp.sin6_addr.s6_addr32);
	cs_mc_groupip_lookup_table_reset(
			(u32 *)&mfc->mf6cc_mcastgrp.sin6_addr.s6_addr32, true);
}

void k_jt_cs_mc_ip6mr_set_opt_del_mfc(struct mr6_table *mrt, struct mf6cctl *mfc)
{
	if (cs_mc_ipv6_enable() == 0)
		return;

	if (mfc == NULL)
		return;

	DBG_IPv6(printk("%s	origin=%pI64 mcastgr=%pI64\n", __func__,
				&mfc->mf6cc_origin, &mfc->mf6cc_mcastgrp));

	__cs_mc_delete_hash_by_ipv6_group(mfc->mf6cc_origin.sin6_addr.s6_addr32,
			mfc->mf6cc_mcastgrp.sin6_addr.s6_addr32);
	cs_mc_groupip_lookup_table_delete(
			(u32 *)mfc->mf6cc_mcastgrp.sin6_addr.s6_addr32, true);
}

EXPORT_SYMBOL(k_jt_cs_mc_ip6mr_set_opt_mrt_init);
EXPORT_SYMBOL(k_jt_cs_mc_ip6mr_set_opt_mrt_done);
EXPORT_SYMBOL(k_jt_cs_mc_ip6mr_set_opt_add_mfc_before);
EXPORT_SYMBOL(k_jt_cs_mc_ip6mr_set_opt_del_mfc);

void cs_mc_ipv4_post_routing(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct iphdr *iph = ip_hdr(skb);

	if (cs_cb == NULL)
		return;

	if (cs_mc_ipv4_enable() == 0) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}

	DBG_IPv4(printk("%s\n", __func__));

	if (iph->protocol != IPPROTO_UDP) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}
	cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;
	cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;
}

void cs_mc_ipv6_post_routing(struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (cs_cb == NULL)
		return;

	if (cs_mc_ipv6_enable() == 0) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}

	DBG_IPv6(printk("%s\n", __func__));

	if (cs_cb->output.l3_nh.ipv6h.protocol != IPPROTO_UDP) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return;
	}
	cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;
	cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;
}

static int cs_mc_callback_core_hmu_notify(u32 watch_bitmask,
		cs_core_hmu_value_t *value, u32 status)
{
	cs_core_hmu_value_t *p = value;
	u32 * p_ip_addr = NULL;
	u16 phy_port = -1;
	bool is_v6 = false;

	DBG(printk("%s() %ld core_hmu_notify status=%x\n", __func__, jiffies,
			status));

	if ((watch_bitmask != (CS_CORE_HMU_WATCH_IN_IPV4_DA |
				CS_CORE_HMU_WATCH_DST_PHY_PORT)) &&
			(watch_bitmask != (CS_CORE_HMU_WATCH_IN_IPV6_DA |
				CS_CORE_HMU_WATCH_DST_PHY_PORT))) {
		printk("%s() ERR watch_bitmask(0x%x) != CS_CORE_HMU_WATCH"
				"_IN_IP_DA\n", __func__, watch_bitmask);
		return 0;
	}
	while (p != NULL) {
		if (p->type == CS_CORE_HMU_WATCH_IN_IPV4_DA) {
			DBG(printk("\t value->type=0x%x, mask=0x%x, ipaddr=%pI4\n",
					p->type, p->mask, p->value.ip_addr));
			p_ip_addr = p->value.ip_addr;
		}

		if (p->type == CS_CORE_HMU_WATCH_IN_IPV6_DA) {
			DBG(printk("\t value->type=0x%x, mask=0x%x, ipaddr=%pI64\n",
					p->type, p->mask, p->value.ip_addr));
			p_ip_addr = p->value.ip_addr;
			is_v6 = true;
		}

		if (p->type == CS_CORE_HMU_WATCH_DST_PHY_PORT) {
			DBG(printk("\t value->type=0x%x, mask=0x%x, dst_phy_port=%d\n",
					p->type, p->mask, p->value.index));
			phy_port = p->value.index;
		}
		p = p->next;
	}

	if (status == CS_CORE_HMU_RET_STATUS_CREATE_FAIL) {
		/* if hash for packet to 2nd physical port fail, need to delete
		 * previous packet */
		if (value == NULL) {
			printk("%s() ERR value == NULL\n", __func__);
			return 0;
		}
		cs_mc_groupip_lookup_table_update(p_ip_addr, is_v6,
				CS_MC_TABLE_HASH_CREATE_FAIL, phy_port);
		if (is_v6)
			__cs_mc_delete_hash_by_ipv6_group(0, p_ip_addr);
		else
			__cs_mc_delete_hash_by_ipv4_group(0, *p_ip_addr);

	} else if (status == CS_CORE_HMU_RET_STATUS_CREATE_SUCCEED) {
		/* if hash for packet to 1st physical port fail, but 2nd succeed,
		 * need to delete hash for this packet */
		if (cs_mc_groupip_lookup_table_update(p_ip_addr, is_v6,
				CS_MC_TABLE_HASH_CREATE_PASS, phy_port) == false)
			cs_core_hmu_delete_hash(watch_bitmask, value);
	} else if ((status == CS_CORE_HMU_RET_STATUS_TIMEOUT_ALL)||
			(status == CS_CORE_HMU_RET_STATUS_DELETED_ALL)) {
		cs_mc_groupip_lookup_table_update(p_ip_addr, is_v6,
				CS_MC_TABLE_HASH_CREATE_NONE, phy_port);
	}

	return 0;
}


void cs_mc_callback_hma_ipv4(unsigned long notify_event,
		unsigned long value)
{
	DBG(printk("%s() cs hw accel ipv4 multicast event %ld\n", __func__,
			notify_event));

	if ((notify_event == CS_HAM_ACTION_MODULE_DISABLE) ||
			(notify_event == CS_HAM_ACTION_CLEAN_HASH_ENTRY)) {
		cs_core_hmu_clean_watch(&ipv4_mc_hmu_entry);
		cs_mc_groupip_lookup_table_flush(false);
	}
}

void cs_mc_callback_hma_ipv6(unsigned long notify_event,
		unsigned long value)
{
	DBG(printk("%s() cs hw accel ipv6 multicast event %ld\n", __func__,
			notify_event));

	if ((notify_event == CS_HAM_ACTION_MODULE_DISABLE) ||
			(notify_event == CS_HAM_ACTION_CLEAN_HASH_ENTRY)) {
		cs_core_hmu_clean_watch(&ipv6_mc_hmu_entry);
		cs_mc_groupip_lookup_table_flush(true);
	}
}


static void dump_mc_table(void)
{
    int i;
    DBG(printk("==================================\n"));
    for (i=0; i<CS_MC_STA_ENTRY_SIZE; i++) {
        DBG(printk("Index: %d\n", i));
        DBG(printk("mac_da        : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n", mac_table_mc[i].mac_da[0]
            , mac_table_mc[i].mac_da[1], mac_table_mc[i].mac_da[2], mac_table_mc[i].mac_da[3]
            , mac_table_mc[i].mac_da[4], mac_table_mc[i].mac_da[5]));
        DBG(printk("status        : %x\n", mac_table_mc[i].status));
        DBG(printk("egress_port_id: 0x%2.2x\n", mac_table_mc[i].egress_port_id));
        DBG(printk("==================================\n"));
    }

    return;
}/* dump_mc_table() */


static int cs_wfo_mc_mac_table_get_empty_entry(unsigned char *mac_da, unsigned char *p_index, unsigned char *p_egress_port_id)
{
    int i;
    unsigned char empty_index = 0xFF, index = 0xFF;
    unsigned char found_empty = 0, found_entry = 0;

    spin_lock(&cs_mc_mac_lookup_table_lock);
    for (i=0; i<CS_MC_STA_ENTRY_SIZE; i++) {
        if (mac_table_mc[i].status == CS_MC_MAC_ENTRY_INVALID) {
            if (found_empty)
                continue;
            found_empty = 1;
            empty_index = i;
        } else {
            if (memcmp(mac_table_mc[i].mac_da, mac_da, sizeof(mac_table_mc[i].mac_da)) == 0) {
                found_entry = 1;
                index = i;
                break;
            }
        }
    }
    spin_unlock(&cs_mc_mac_lookup_table_lock);
    
    DBG(printk("%s:: found_entry %d , found_empty %d, empty_index 0x%x\n", 
            __func__, found_entry, found_empty, empty_index));
    if (found_entry) {
        spin_lock(&cs_mc_mac_lookup_table_lock);
        *p_index = index;
        *p_egress_port_id = mac_table_mc[index].egress_port_id;
        spin_unlock(&cs_mc_mac_lookup_table_lock);
        DBG(printk("%s: CS_MC_MAC_ENTRY_STATUS_EXIST *p_index %d, *p_egress_port_id 0x%x\n", 
                __func__, *p_index, *p_egress_port_id));
        return CS_MC_MAC_ENTRY_STATUS_EXIST;
    }
    if (empty_index == 0xFF) {
        return CS_MC_MAC_ENTRY_STATUS_FULL;
    }

    spin_lock(&cs_mc_mac_lookup_table_lock);
    *p_index = empty_index;
    *p_egress_port_id = mac_table_mc[empty_index].egress_port_id;
    spin_unlock(&cs_mc_mac_lookup_table_lock);
    DBG(printk("%s: *p_index %d, *p_egress_port_id 0x%x\n", 
            __func__, *p_index, *p_egress_port_id));
    
    
    return CS_MC_MAC_ENTRY_STATUS_SUCCESS;
}/* cs_wfo_mc_mac_table_get_empty_entry() */



// **********************************
//

void cs_wfo_mc_mac_table_init(void)
{
    int i;

    printk("cs_wfo_mc_mac_table_init\n");
    for (i=0; i<CS_MC_STA_ENTRY_SIZE; i++) {
        memset(mac_table_mc[i].mac_da, 0, sizeof(mac_table_mc[i].mac_da));
		mac_table_mc[i].status = CS_MC_MAC_ENTRY_INVALID;
		mac_table_mc[i].egress_port_id = i + CS_EGRESS_PORT_STA1;;
    }
    spin_lock_init(&cs_mc_mac_lookup_table_lock);
    
    return;
}/* cs_wfo_mc_mac_table_init() */
//EXPORT_SYMBOL(cs_wfo_mc_mac_table_init);


int cs_wfo_mc_mac_table_add_entry(unsigned char* mac_da, unsigned char* p_egress_port_id)
{
    int retStatus = CS_MC_MAC_ENTRY_STATUS_FAIL;
    unsigned char index = -1;
    
    if(unlikely(!mac_da))
        return retStatus;
    
    retStatus = cs_wfo_mc_mac_table_get_empty_entry(mac_da, &index, p_egress_port_id);
    DBG(printk("%s: mac_da %pM, retStatus 0x%x, index %d , p_egress_port_id 0x%x\n", 
            __func__, mac_da, retStatus, index, *p_egress_port_id));
    switch (retStatus) {
        case CS_MC_MAC_ENTRY_STATUS_SUCCESS:
            spin_lock(&cs_mc_mac_lookup_table_lock);
            memcpy(mac_table_mc[index].mac_da, mac_da, sizeof(mac_table_mc[index].mac_da));
            mac_table_mc[index].status = CS_MC_MAC_ENTRY_VALID;
            mac_table_mc[index].egress_port_id = *p_egress_port_id;
            spin_unlock(&cs_mc_mac_lookup_table_lock);
            dump_mc_table();
            break;
        case CS_MC_MAC_ENTRY_STATUS_EXIST:
            retStatus = CS_MC_MAC_ENTRY_STATUS_SUCCESS;
            dump_mc_table();
            break;
    }// end switch()

    return retStatus;
}/* cs_wfo_mc_mac_table_add_entry() */
EXPORT_SYMBOL(cs_wfo_mc_mac_table_add_entry);


int cs_wfo_mc_mac_table_del_entry(unsigned char* mac_da)
{
    int i;
    
    spin_lock(&cs_mc_mac_lookup_table_lock);
    for (i=0; i<CS_MC_STA_ENTRY_SIZE; i++) {
        if (memcmp(mac_table_mc[i].mac_da, mac_da, sizeof(mac_table_mc[i].mac_da)) == 0) {
//            memset(mac_table_mc[i].mac_da, 0, sizeof(mac_table_mc[i].mac_da));
    		mac_table_mc[i].status = CS_MC_MAC_ENTRY_INVALID;
            spin_unlock(&cs_mc_mac_lookup_table_lock);
            return CS_MC_MAC_ENTRY_STATUS_SUCCESS;
        }
    }
    spin_unlock(&cs_mc_mac_lookup_table_lock);

    return CS_MC_MAC_ENTRY_STATUS_FAIL;
}/* cs_wfo_mc_mac_table_del_entry() */
EXPORT_SYMBOL(cs_wfo_mc_mac_table_del_entry);



void cs_wfo_mc_mac_table_del_all_entry(void)
{
    int i;
    
    DBG(printk("%s\n", __func__));
    spin_lock(&cs_mc_mac_lookup_table_lock);
    for (i=0; i<CS_MC_STA_ENTRY_SIZE; i++) {
//        memset(mac_table_mc[i].mac_da, 0, sizeof(mac_table_mc[i].mac_da));
		mac_table_mc[i].status = CS_MC_MAC_ENTRY_INVALID;
    }
    spin_unlock(&cs_mc_mac_lookup_table_lock);

    dump_mc_table();

    return;
}
EXPORT_SYMBOL(cs_wfo_mc_mac_table_del_all_entry);


cs_mc_sta_mac_entry_s* cs_wfo_mc_mac_table_get_entry(unsigned char* mac_da)
{
    int i;
    cs_mc_sta_mac_entry_s* pEntry = &mac_table_mc[0];

    spin_lock(&cs_mc_mac_lookup_table_lock);
    for (i=0; i<CS_MC_STA_ENTRY_SIZE; i++) {
        if (memcmp(pEntry->mac_da, mac_da, sizeof(pEntry->mac_da)) == 0) {
            if (pEntry->status == CS_MC_MAC_ENTRY_VALID) {
                 spin_unlock(&cs_mc_mac_lookup_table_lock);
                 return pEntry;
            }
        }
        pEntry++;
    }
    spin_unlock(&cs_mc_mac_lookup_table_lock);

    return NULL;
}/* cs_wfo_mc_mac_table_get_entry() */
EXPORT_SYMBOL(cs_wfo_mc_mac_table_get_entry);



extern void cs_ni_set_mc_table(u8 mc_index, u16 mc_vec);
extern void cs_ni_get_mc_table(u8 mc_index);

int cs_mc_init(void)
{
	int i;

	/*init NI_TOP_NI_MC_INDX_LKUP*/
	for (i = 0; i < MCAL_TABLE_SIZE / 2; i++) {
		cs_ni_set_mc_table(i,i);
	}

	cs_mc_groupip_lookup_table_init();
    cs_wfo_mc_mac_table_init();

	/*for hw_accel_manager */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV4_MULTICAST,
					       cs_mc_callback_hma_ipv4);
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV6_MULTICAST,
					       cs_mc_callback_hma_ipv6);

	/*for core hmu */
	memset(&ipv4_mc_hmu_entry, 0, sizeof(ipv4_mc_hmu_entry));
	memset(&ipv4_mc_hmu_value_ip_da, 0, sizeof(ipv4_mc_hmu_value_ip_da));
	memset(&ipv4_mc_hmu_value_egress_port_id, 0, sizeof(ipv4_mc_hmu_value_egress_port_id));

	memset(&ipv6_mc_hmu_entry, 0, sizeof(ipv6_mc_hmu_entry));
	memset(&ipv6_mc_hmu_value_ip_da, 0, sizeof(ipv6_mc_hmu_value_ip_da));

	ipv4_mc_hmu_value_ip_da.type = CS_CORE_HMU_WATCH_IN_IPV4_DA;
	ipv4_mc_hmu_value_ip_da.mask = 4;
	ipv4_mc_hmu_value_ip_da.value.ip_addr[0] = htonl(0xe0000000); /*network order*/
	ipv4_mc_hmu_value_ip_da.next = &ipv4_mc_hmu_value_egress_port_id;

	ipv4_mc_hmu_value_egress_port_id.type = CS_CORE_HMU_WATCH_DST_PHY_PORT;

	ipv4_mc_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_IN_IPV4_DA
		                            | CS_CORE_HMU_WATCH_DST_PHY_PORT;
	ipv4_mc_hmu_entry.value_mask = &ipv4_mc_hmu_value_ip_da;
	ipv4_mc_hmu_entry.callback = cs_mc_callback_core_hmu_notify;

	cs_core_hmu_register_watch(&ipv4_mc_hmu_entry);

	ipv6_mc_hmu_value_ip_da.type = CS_CORE_HMU_WATCH_IN_IPV6_DA;
	ipv6_mc_hmu_value_ip_da.mask = 8;
	ipv6_mc_hmu_value_ip_da.value.ip_addr[0] = htonl(0xFF000000); /*network order*/
	ipv6_mc_hmu_value_ip_da.next = &ipv6_mc_hmu_value_egress_port_id;

	ipv6_mc_hmu_value_egress_port_id.type = CS_CORE_HMU_WATCH_DST_PHY_PORT;

	ipv6_mc_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_IN_IPV6_DA
				| CS_CORE_HMU_WATCH_DST_PHY_PORT;;
	ipv6_mc_hmu_entry.value_mask = &ipv6_mc_hmu_value_ip_da;
	ipv6_mc_hmu_entry.callback = cs_mc_callback_core_hmu_notify;

	cs_core_hmu_register_watch(&ipv6_mc_hmu_entry);

	return 0;
}

int cs_mc_exit(void)
{
	cs_core_hmu_unregister_watch(&ipv4_mc_hmu_entry);
	cs_core_hmu_unregister_watch(&ipv6_mc_hmu_entry);

	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV4_MULTICAST,
					       NULL);
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV6_MULTICAST,
					       NULL);

	cs_mc_groupip_lookup_table_flush(false);
	cs_mc_groupip_lookup_table_flush(true);

	return 0;
}

