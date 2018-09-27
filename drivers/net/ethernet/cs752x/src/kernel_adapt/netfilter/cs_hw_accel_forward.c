#include <mach/cs_types.h>
#include "cs_core_logic.h"

#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/rculist_nulls.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter/ipset/ip_set.h>
#include <net/ipv6.h>

#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
//#include <linux/sysctl.h>
#include <linux/export.h>

#include <linux/skbuff.h>
#include "cs_hw_accel_manager.h"
#include "cs_hw_accel_mc.h"
#include "cs_hw_accel_forward.h"
#include "cs_core_logic.h"
#include "cs_core_hmu.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;

#define DBG(x) {if (cs_adapt_debug & (CS752X_ADAPT_IPV4_FORWARD|CS752X_ADAPT_IPV6_FORWARD)) x;}
#define DBG_IPv4(x) {if (cs_adapt_debug & CS752X_ADAPT_IPV4_FORWARD) x;}
#define DBG_IPv6(x) {if (cs_adapt_debug & CS752X_ADAPT_IPV6_FORWARD) x;}
#else
#define DBG(x) {}
#define DBG_IPv4(x) {}
#define DBG_IPv6(x) {}
#endif
extern u32 cs_hw_udp_offload_after_established;

extern struct nf_conntrack_l4proto nf_conntrack_l4proto_tcp4;
extern struct nf_conntrack_l4proto nf_conntrack_l4proto_udp4;

cs_core_hmu_t ipv4_fwd_hmu_entry;
cs_core_hmu_value_t ipv4_fwd_hmu_value;
cs_core_hmu_t ipv6_fwd_hmu_entry;
cs_core_hmu_value_t ipv6_fwd_hmu_value;

/* TCP BYPASS PORT */
#define FTP_CTRL_PORT	(21)
#define H323_PORT		(1720)
#define T120_PORT		(1503)
#define PPTP_PORT		(1723)
#define UPNP_PORT		(5000)
#define RTSP_PORT		(554)
#define MSN_PORT		(1863)


/* UDP BYPASS PORT */
#define TFTP_PORT 		(69)
#define DNS_PORT 		(53)
#define NTP_PORT		(123)
#ifndef RAS_PORT
#define RAS_PORT		(1719)
#endif
#define BOOTP67_PORT	(67)
#define BOOTP68_PORT	(68)
#define ISAKMP_PORT		(500)
#define UPNP_DISC_PORT	(1900)
#define L2TP_CTRL_PORT	(1701)
#ifndef SIP_PORT
#define SIP_PORT		(5060)
#endif

static unsigned short allow_localin_port_list[MAX_PORT_LIST_SIZE + 1] = {
	0
};

static unsigned short bypass_tcp_port_list[MAX_PORT_LIST_SIZE + 1] = {
	FTP_CTRL_PORT,
	H323_PORT,
	T120_PORT,
	PPTP_PORT,
	UPNP_PORT,
	RTSP_PORT,
	MSN_PORT,
	0
};

static unsigned short bypass_udp_port_list[MAX_PORT_LIST_SIZE + 1] = {
	DNS_PORT,
	NTP_PORT,
	TFTP_PORT,
	RAS_PORT,
	BOOTP67_PORT,
	BOOTP68_PORT,
	ISAKMP_PORT,
	UPNP_DISC_PORT,
#ifndef CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL
	L2TP_CTRL_PORT,
#endif
	SIP_PORT,
	0
};

static unsigned short bypass_expected_master_port_list[MAX_PORT_LIST_SIZE + 1] = {
	H323_PORT,
	0
};

static unsigned short priority_tcp_port_list[MAX_PORT_LIST_SIZE + 1] = {
	80,
	FTP_CTRL_PORT,
	H323_PORT,
	UPNP_PORT,
	RTSP_PORT,
	MSN_PORT,
	0
};

static unsigned short priority_udp_port_list[MAX_PORT_LIST_SIZE + 1] = {
	DNS_PORT,
	NTP_PORT,
	TFTP_PORT,
	RAS_PORT,
	BOOTP67_PORT,
	BOOTP68_PORT,
	ISAKMP_PORT,
	UPNP_DISC_PORT,
	L2TP_CTRL_PORT,
	SIP_PORT,
	0
};

int cs_forward_check_bypass_expected_master_port(struct nf_conn *ct,
	cs_kernel_accel_cb_t *cs_cb) {
	u16 sport, dport, *port_ptr;
	/*
	 * The Linux conntrack will check its master at init_conntrack()
	 * in nf_conntrack_core.c
	 */

	if ((ct) && (ct->master)) {
		struct nf_conntrack_tuple * tuple;

		tuple = nf_ct_tuple(ct->master, IP_CT_DIR_ORIGINAL);
		sport = ntohs(tuple->src.u.all);
		dport = ntohs(tuple->dst.u.all);

		for (port_ptr = bypass_expected_master_port_list ; *port_ptr; port_ptr++) {
			if (sport == *port_ptr || dport == *port_ptr) {
				cs_cb->common.sw_only = CS_SWONLY_STATE;
				return -1;
			}
		}
	}
	return 1;
}

u16* cs_forward_port_list_get(int list_type) {
	switch (list_type) {
	case BYPASS_LIST_TUPE_TCP:
		return bypass_tcp_port_list;
	case BYPASS_LIST_TUPE_UDP:
		return bypass_udp_port_list;
	case BYPASS_LIST_TUPE_EXPECTED_MASTER:
		return bypass_expected_master_port_list;
	case PRIORITY_LIST_TUPE_TCP:
		return priority_tcp_port_list;
	case PRIORITY_LIST_TUPE_UDP:
		return priority_udp_port_list;
	case ALLOW_LOCALIN_TUPE:
		return allow_localin_port_list;
	}

	return bypass_expected_master_port_list;
}

void cs_forward_port_list_dump(int list_type)
{
	int i;
	u16 *port_ptr = cs_forward_port_list_get(list_type);

	switch (list_type) {
	case BYPASS_LIST_TUPE_TCP:
		printk("Dump bypass_tcp_port_list: \n");
		break;
	case BYPASS_LIST_TUPE_UDP:
		printk("Dump bypass_udp_port_list: \n");
		break;
	case BYPASS_LIST_TUPE_EXPECTED_MASTER:
		printk("Dump bypass_expected_master_port_list: \n");
		break;
	case PRIORITY_LIST_TUPE_TCP:
		printk("Dump priority_tcp_port_list: \n");
		break;
	case PRIORITY_LIST_TUPE_UDP:
		printk("Dump priority_udp_port_list: \n");
		break;
	case ALLOW_LOCALIN_TUPE:
		printk("Dump allow_localin_port_list: \n");
		break;
	}

	for (i = 0; i < MAX_PORT_LIST_SIZE; port_ptr++, i++) {
		if (*port_ptr == 0) {
			break;
		}
		printk("\t%d ", *port_ptr);
		if ((i % 10) == 9)
			printk("\n");
	}
	printk("\n\ttotal list size:%d \n", i);
}

int cs_forward_port_list_add(int list_type, unsigned short port)
{
	int i;
	u16 *port_ptr = cs_forward_port_list_get(list_type);
	int ret = -1;

	for (i = 0; i < MAX_PORT_LIST_SIZE; port_ptr++, i++) {
		if (*port_ptr == port) {
			break;
		} else if (*port_ptr == 0) {
			*port_ptr = port;
			*(port_ptr + 1) = 0;
			DBG(printk("%s add port:%d at index(0~%d):%d\n",
				__func__, port,	MAX_PORT_LIST_SIZE - 1, i));
			ret = 0;
			break;
		}
	}

	return ret;
}

int cs_forward_port_list_del(int list_type, unsigned short port)
{
	int i;
	u16 *port_ptr = cs_forward_port_list_get(list_type);
	u16 *del_port_ptr = NULL;
	int ret = -1;

	for (i = 0; i < MAX_PORT_LIST_SIZE; port_ptr++, i++) {
		if (*port_ptr == 0) {
			if (del_port_ptr == NULL) {
				DBG(printk("%s cannot del port:%d the current size(1~%d):%d\n",
					__func__, port, MAX_PORT_LIST_SIZE, i));
			} else {
				*del_port_ptr = *(port_ptr - 1);
				*(port_ptr - 1) = 0;
				DBG(printk("%s del port:%d the current size(1~%d):%d\n",
					__func__, port, MAX_PORT_LIST_SIZE, i - 1));
				ret = 0;
			}
			break;
		} else if (*port_ptr == port) {
			del_port_ptr = port_ptr;
		}
	}

	return ret;
}


static inline int cs_forward4_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPV4_FORWARD);
}

static inline int cs_forward6_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPV6_FORWARD);
}

/*
 *return by sec
 */
int __cs_get_ct_lifetime(int protonum, struct nf_conn *ct)
{
	if (protonum == IPPROTO_TCP) {
#ifdef CONFIG_SYSCTL
		/*Get Establish timeout */
		return (*(int *)nf_conntrack_l4proto_tcp4.ctl_table[2].data) / HZ;
#else
		return 300;
#endif
	} else if (protonum == IPPROTO_UDP) {
#ifdef CONFIG_SYSCTL
		if (test_bit(IPS_ASSURED_BIT, &ct->status))
			return (*(int *)(nf_conntrack_l4proto_udp4.ctl_table[1].data)) / HZ;
		else
			return (*(int *)(nf_conntrack_l4proto_udp4.ctl_table[0].data)) / HZ;
#else
		return 300;
#endif
	}
	return 0;

}

/*export this function form net/netfilter/nf_conntrack_core.c */
extern void death_by_timeout(unsigned long ul_conntrack);

static void cs_forward_callback_ct_timeout(unsigned long ul_conntrack)
{
	struct nf_conn *ct = (struct nf_conn *)ul_conntrack;
	int protonum = nf_ct_protonum(ct);
	int l3num = nf_ct_l3num(ct);
	cs_core_hmu_value_t hmu_value;
	unsigned long last_use;
	int ret;

	DBG(printk("%s() %ld, ct = %x - \n", __func__, jiffies, (u32)ct));

	hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	hmu_value.mask = 0x08;

	if (l3num == NFPROTO_IPV4)
		hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD);

	else
		hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD);

	hmu_value.value.swid64 |= (u64)ul_conntrack;

	DBG(printk("\tswid=%llx \n", hmu_value.value.swid64));

	if ((protonum == IPPROTO_TCP) &&
			(ct->proto.tcp.state != TCP_CONNTRACK_ESTABLISHED)) {
		ret = -1;
		DBG(printk("\tdelete timer and hash(protocol TCP, status %d)\n",
				ct->proto.tcp.state));
	} else {
		ret = cs_core_hmu_get_last_use(CS_CORE_HMU_WATCH_SWID64,
				&hmu_value, &last_use);
		DBG(printk("\tcs_core_hmu_get_last_use (ret=%d) %ld\n", ret,
				(ret == 0) ? last_use: 0));
	}

	if (ret != 0) {
			DBG(printk("\t call death_by_timeout\n"));
			/*no more hash status in HW, so delete the ct
			 *equal to linux death_by_timeout(ct);
			 */
			death_by_timeout((unsigned long)ct);
	} else {
		unsigned long extra_jiffies = __cs_get_ct_lifetime(protonum, ct) * HZ;
		unsigned long expire_time = last_use + extra_jiffies;
		DBG(printk("\t timer hash->lifetime(%ld) , idle(%ld) \n",
				extra_jiffies, jiffies - last_use));

		if (time_before(expire_time, jiffies)) {
			DBG(printk("\t delete timer becasue %ld < %ld \n",
					extra_jiffies, jiffies - last_use));
			/*
			 * delete timer becasue the hash is expired.
			 */
			death_by_timeout((unsigned long)ct);
		} else {
			/* refer to kernel code __nf_ct_refresh_acct()*/
			if (!nf_ct_is_confirmed(ct)) {
				ct->timeout.expires = extra_jiffies;
				DBG(printk("\t delete timer becasue nf_ct_"
						"is_confirmed(ct)\n"));
				/*
			    * delete hash becasue the ct status is not nf_ct_is_confirmed.
			    */
				cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64,
						&hmu_value);
				ct->timeout.function = death_by_timeout;
			} else {
				unsigned long newtime = jiffies + extra_jiffies;
				if (time_after(newtime, expire_time)) {
					newtime = expire_time;
					/* need to change lifetime in all vlash entries
				 	 * based on the up-to-date lifetime */
					DBG(printk("\t extend %s timer extra_jiffies %ld %ld\n",
						timer_pending(&ct->timeout)?"pending":"@not pending",
						newtime - jiffies, expire_time));
				}
				mod_timer(&ct->timeout, newtime);
				DBG(printk("\t now %ld  - expired %ld  = %ld\n",
					jiffies, ct->timeout.expires, jiffies - ct->timeout.expires));

				DBG(printk("\t %s\n",
					timer_pending(&ct->timeout)?"pending":"@not pending"));
			}
		}
	}

	return;
}

/*
 * Purpose: get last used time (in jiffies) of L3 hash entry from hmu. Refresh ct timer if
 *          last_used + configured timeout < current jiffies. Else do nothing, let kernel age out
 *          this CT
 * Called by: cs_seq_show (nf_conntrack_l3proto_ipv4_compact.c)
 */
void cs_hw_accel_forward_update_ct_timeout(struct nf_conn *ct)
{
	int protonum = nf_ct_protonum(ct);
	int l3num = nf_ct_l3num(ct);
	cs_core_hmu_value_t hmu_value;
	unsigned long last_use;
	int ret;
	if (protonum == IPPROTO_TCP) {
		if (ct->proto.tcp.state != TCP_CONNTRACK_ESTABLISHED) {
			return;
		}
	}

	DBG(printk("%s() %ld, ct = %x - \n", __func__, jiffies, (u32)ct));

	hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	hmu_value.mask = 0x08;

	if (l3num == NFPROTO_IPV4)
		hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD);
	else
		hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD);

	hmu_value.value.swid64 |= (unsigned long) ct;

	DBG(printk("\tswid=%llx \n", hmu_value.value.swid64));


	ret = cs_core_hmu_get_last_use(CS_CORE_HMU_WATCH_SWID64,
			&hmu_value, &last_use);
	DBG(printk("\tcs_core_hmu_get_last_use (ret=%d) %ld\n", ret,
			(ret == 0) ? last_use:0));


	if (ret == 0) {
		unsigned long extra_jiffies = __cs_get_ct_lifetime(protonum, ct) * HZ;
		unsigned long expire_time = last_use + extra_jiffies;
		DBG(printk("\t timer hash->lifetime(%ld) , idle(%ld) \n",
				extra_jiffies, jiffies - last_use));
		if (time_before(expire_time, jiffies)) {
			/*if this is already expired, we don't update timer*/
		} else {
			/* refer to kernel code __nf_ct_refresh_acct()*/
			if (!nf_ct_is_confirmed(ct)) {
				DBG(printk("\t !nf_ct_is_confirmed(ct)\n"));
			} else {
				unsigned long newtime = jiffies + extra_jiffies;
				if (time_after(newtime, expire_time)) {
					newtime = expire_time;
					/* need to change lifetime in all vlash entries
				 	 * based on the up-to-date lifetime */
					DBG(printk("\t extend timer extra_jiffies %ld %ld\n",
						newtime - jiffies, expire_time));
				}

				mod_timer(&ct->timeout, newtime);

				DBG(printk("\t now %ld  - expired %ld  = %ld\n",
					jiffies, ct->timeout.expires, jiffies - ct->timeout.expires));

			}
		}
	}
	return;
}
EXPORT_SYMBOL(cs_hw_accel_forward_update_ct_timeout);

/*
 * Purpose: when ct is going to be detelted, we will delete hash at the meantime.
 * Called by: nf_ct_delete_from_lists (nf_conntrack_core.c)
 */
void cs_hw_accel_forward_delete_ct_hash(struct nf_conn *ct)
{
	//int protonum = nf_ct_protonum(ct);
	int l3num = nf_ct_l3num(ct);
	cs_core_hmu_value_t hmu_value;

	DBG(printk("%s() %ld, ct = %x ", __func__, jiffies, (u32)ct));

	hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	hmu_value.mask = 0x08;

	if (l3num == NFPROTO_IPV4)
		hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD);
	else
		hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD);

	hmu_value.value.swid64 |= (unsigned long) ct;

	DBG(printk("swid=%llx \n", hmu_value.value.swid64));

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64,
						&hmu_value);

	return;
}
EXPORT_SYMBOL(cs_hw_accel_forward_delete_ct_hash);

struct ct_range_ip4_t {
	struct in_addr lo, hi;
	struct ct_range_ip4_t *next;
};

struct ct_range_ip6_t {
	struct in6_addr lo, hi;
	struct ct_range_ip6_t *next;
};

struct ct_range_net_t {
	struct net *net;
	struct ct_range_ip4_t *ip4;
	struct ct_range_ip6_t *ip6;
	struct ct_range_net_t *next;
};

static spinlock_t cs_hw_accel_forward_delete_range_lock;
static struct ct_range_net_t cs_hw_accel_forward_delete_ct_queue = {0};

static void cs_hw_accel_forward_delete_ct_hash_by_src_work_fn(struct work_struct *work)
{
	struct ct_range_net_t ct_range_net_head, *cur_range_net;
	struct ct_range_ip4_t *cur_ip4;
	struct ct_range_ip6_t *cur_ip6;
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_node *n;
	struct nf_conn *ct;
	struct nf_conntrack_man *match;
	cs_core_hmu_value_t hmu_value;
	size_t i;
	int matched;

	spin_lock(&cs_hw_accel_forward_delete_range_lock);
	memcpy(&ct_range_net_head, &cs_hw_accel_forward_delete_ct_queue, sizeof(struct ct_range_net_t));
	memset(&cs_hw_accel_forward_delete_ct_queue, 0, sizeof(struct ct_range_net_t));
	spin_unlock(&cs_hw_accel_forward_delete_range_lock);

	rcu_read_lock();
	for (cur_range_net = &ct_range_net_head; cur_range_net != NULL; cur_range_net = ct_range_net_head.next) {
		if (cur_range_net->net != NULL) {
			for (i = 0; i < cur_range_net->net->ct.htable_size; ++i) {
				matched = 0;
				hlist_nulls_for_each_entry_rcu(h, n, &cur_range_net->net->ct.hash[i], hnnode) {
					ct = nf_ct_tuplehash_to_ctrack(h);
					match = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src;
					if (match->l3num == NFPROTO_IPV4) {
						for (
							cur_ip4 = cur_range_net->ip4;
							cur_ip4 != NULL &&
							memcmp(&cur_ip4->hi, &match->u3.in, sizeof(struct in_addr)) < 0;
							cur_ip4 = cur_ip4->next
						);
						if (cur_ip4 != NULL && memcmp(&cur_ip4->lo, &match->u3.in, sizeof(struct in_addr)) <= 0) {
							hmu_value.value.swid64 = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD) | ((uintptr_t)ct & 0xFFFFFFFF);
							matched = 1;
						}
					} else if (match->l3num == NFPROTO_IPV6) {
						for (
							cur_ip6 = cur_range_net->ip6;
							cur_ip6 != NULL &&
							memcmp(&cur_ip6->hi, &match->u3.in6, sizeof(struct in6_addr)) < 0;
							cur_ip6 = cur_ip6->next
						);
						if (cur_ip6 != NULL && memcmp(&cur_ip6->lo, &match->u3.in6, sizeof(struct in6_addr)) <= 0) {
							hmu_value.value.swid64 = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD) | ((uintptr_t)ct & 0xFFFFFFFF);
							matched = 1;
						}
					}
				}
				if (matched) {
					hmu_value.next = NULL;
					hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
					hmu_value.mask = 0x08;
					cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64, &hmu_value);
				}
			}
		}
		for (cur_ip4 = cur_range_net->ip4; cur_ip4 != NULL; cur_ip4 = cur_range_net->ip4) {
			cur_range_net->ip4 = cur_range_net->ip4->next;
			kfree(cur_ip4);
		}
		for (cur_ip6 = cur_range_net->ip6; cur_ip6 != NULL; cur_ip6 = cur_range_net->ip6) {
			cur_range_net->ip6 = cur_range_net->ip6->next;
			kfree(cur_ip6);
		}
		put_net(cur_range_net->net);
		if (cur_range_net != &ct_range_net_head) {
			ct_range_net_head.next = cur_range_net->next;
			kfree(cur_range_net);
		}
	}
	rcu_read_unlock();
}
static DECLARE_DELAYED_WORK(cs_hw_accel_forward_delete_ct_hash_by_src_work, cs_hw_accel_forward_delete_ct_hash_by_src_work_fn);

static void cs_hw_accel_forward_delete_ct_range_queue_ip4(struct ct_range_ip4_t **range, struct in_addr *lo, struct in_addr *hi)
{
	struct ct_range_ip4_t **cur, *next;

	for (
		cur = range;
		*cur != NULL &&
		memcmp(&(*cur)->hi, lo, sizeof(struct in_addr)) < 0;
		cur = &(*cur)->next
	);

	if (*cur != NULL && memcmp(&(*cur)->lo, hi, sizeof(struct in_addr)) < 0) {
		if (memcmp(&(*cur)->lo, lo, sizeof(struct in_addr)) > 0)
			memcpy(&(*cur)->lo, lo, sizeof(struct in_addr));
		for (
			next = (*cur)->next;
			next != NULL &&
			memcmp(&next->lo, &(*cur)->hi, sizeof(struct in_addr)) > 0;
			next = (*cur)->next
		) {
			if (memcmp(&(*cur)->hi, &next->hi, sizeof(struct in_addr)) < 0)
				memcpy(&(*cur)->hi, &next->hi, sizeof(struct in_addr));
			(*cur)->next = next->next;
			kfree(next);
		}
	} else {
		next = (struct ct_range_ip4_t *)kmalloc(sizeof(struct ct_range_ip4_t), GFP_ATOMIC);
		if (next != NULL) {
			next->next = *cur;
			memcpy(&next->lo, lo, sizeof(struct in_addr));
			memcpy(&next->hi, hi, sizeof(struct in_addr));
			*cur = next;
		}
	}
}

static void cs_hw_accel_forward_delete_ct_range_queue_ip6(struct ct_range_ip6_t **range, struct in6_addr *lo, struct in6_addr *hi)
{
	struct ct_range_ip6_t **cur, *next;

	for (
		cur = range;
		*cur != NULL &&
		memcmp(&(*cur)->hi, lo, sizeof(struct in6_addr)) < 0;
		cur = &(*cur)->next
	);

	if (*cur != NULL && memcmp(&(*cur)->lo, hi, sizeof(struct in6_addr)) < 0) {
		if (memcmp(&(*cur)->lo, lo, sizeof(struct in6_addr)) > 0)
			memcpy(&(*cur)->lo, lo, sizeof(struct in6_addr));
		for (
			next = (*cur)->next;
			next != NULL &&
			memcmp(&next->lo, &(*cur)->hi, sizeof(struct in6_addr)) > 0;
			next = (*cur)->next
		) {
			if (memcmp(&(*cur)->hi, &next->hi, sizeof(struct in6_addr)) < 0)
				memcpy(&(*cur)->hi, &next->hi, sizeof(struct in6_addr));
			(*cur)->next = next->next;
			kfree(next);
		}
	} else {
		next = (struct ct_range_ip6_t *)kmalloc(sizeof(struct ct_range_ip6_t), GFP_ATOMIC);
		if (next != NULL) {
			next->next = *cur;
			memcpy(&next->lo, lo, sizeof(struct in6_addr));
			memcpy(&next->hi, hi, sizeof(struct in6_addr));
			*cur = next;
		}
	}
}

static void cs_hw_accel_forward_delete_ct_hash_by_src(struct net *net, u16 l3proto, void *l3addr_lo, void *l3addr_hi)
{
	struct ct_range_net_t *range_net = &cs_hw_accel_forward_delete_ct_queue;

	if (l3proto == NFPROTO_IPV4 || l3proto == NFPROTO_IPV6) {
		spin_lock(&cs_hw_accel_forward_delete_range_lock);
		if (range_net->net == NULL) {
			cs_hw_accel_forward_delete_ct_queue.net = net;
			get_net(net);
		} else if (cs_hw_accel_forward_delete_ct_queue.net != net) {
			while (range_net->next != NULL && range_net->next->net != net)
				range_net = range_net->next;
			if (range_net->next == NULL) {
				range_net->next = (struct ct_range_net_t *)kmalloc(sizeof(struct ct_range_net_t), GFP_ATOMIC);
				if (range_net->next != NULL) {
					memset(range_net->next, 0, sizeof(struct ct_range_net_t));
					range_net->next->net = net;
					get_net(net);
				}
			}
			range_net = range_net->next;
		}
		if (range_net != NULL) {
			if (l3proto == NFPROTO_IPV4)
				cs_hw_accel_forward_delete_ct_range_queue_ip4(&range_net->ip4, (struct in_addr *)l3addr_lo, (struct in_addr *)l3addr_hi);
			else if (l3proto == NFPROTO_IPV6)
				cs_hw_accel_forward_delete_ct_range_queue_ip6(&range_net->ip6, (struct in6_addr *)l3addr_lo, (struct in6_addr *)l3addr_hi);
			if (!queue_delayed_work(system_long_wq, &cs_hw_accel_forward_delete_ct_hash_by_src_work, HZ * .5)) {
				cancel_delayed_work(&cs_hw_accel_forward_delete_ct_hash_by_src_work);
				queue_delayed_work(system_long_wq, &cs_hw_accel_forward_delete_ct_hash_by_src_work, HZ * .5);
			}
		}
		spin_unlock(&cs_hw_accel_forward_delete_range_lock);
	}
}

static int cs_forward_callback_core_hmu_notify(u32 watch_bitmask,
		cs_core_hmu_value_t *value, u32 status)
{
	struct nf_conn *ct;
	int protonum, l3num;

	if (watch_bitmask == CS_CORE_HMU_WATCH_SWID64) {
		DBG(printk("%s() %ld core_hmu_notify status=%x", __func__,
					jiffies, status));
		if (value != NULL)
			DBG(printk(" type=%x, swid64=0x%llx", value->type,
						value->value.swid64));
		DBG(printk("\n"));
	} else {
		printk("%s() ERR core_hmu_notify status=%x,strange "
				"watch_bitmask=%x ??\n", __func__, status,
				watch_bitmask);
	}

	if (value == NULL) {
		printk("%s() ERR value == NULL\n", __func__);
		return -1;
	}

	if (value->type == CS_CORE_HMU_WATCH_SWID64) {
		if (value->value.swid64 & 0x0000000F00000000 != 0) {

			/* TODO: customer should add some action code here for flow base API */
			return 0;
		}

		ct = (struct nf_conn *)((u32)(value->value.swid64 &
					0x00000000FFFFFFFF));
		if (ct == NULL)
			return 0;
	} else {
		printk("%s() ERR value->type == 0x%x\n", __func__, value->type);
		return -1;
	}

	protonum = nf_ct_protonum(ct);
	l3num = nf_ct_l3num(ct);

	DBG(printk("\t ct=0x%x protonum=0x%x, l3num=0x%x\n", (u32)ct,
			protonum, l3num));

	if (status == CS_CORE_HMU_RET_STATUS_CREATE_SUCCEED) {
		if (ct->timeout.function != NULL) {
			if (death_by_timeout != ct->timeout.function) {
				if (ct->timeout.function !=	cs_forward_callback_ct_timeout) {
					DBG(printk("%s() ERR death_by_timeout "
						"!= ct->timeout.function \n", __func__));
					return -1;
				} else {
					return 0;
				}
			}
			ct->timeout.function = cs_forward_callback_ct_timeout;
			/* Because the TCP ct flow has been offload to HW
			 * and we need to liberal window checking to handle
			 * window scaling or that will fail at tcp_in_window() check.
			 */
			if (protonum == IPPROTO_TCP) {
				ct->proto.tcp.seen[0].flags |= IP_CT_TCP_FLAG_BE_LIBERAL;
				ct->proto.tcp.seen[1].flags |= IP_CT_TCP_FLAG_BE_LIBERAL;
			}
		} else {
			printk("%s() ERR ct->timeout.function == NULL \n", __func__);
			return -1;
		}
	}
	return 0;
}

void cs_forward_print_tcp(struct tcphdr *tcphdr, struct nf_conn *ct)
{
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug &
			(CS752X_ADAPT_IPV6_FORWARD | CS752X_ADAPT_IPV4_FORWARD)) {
		printk("tcp src_port=%d dest_port=%d hdr=",
		       ntohs(tcphdr->source), ntohs(tcphdr->dest));
		if (tcphdr->fin)
			printk("+fin");
		if (tcphdr->syn)
			printk("+syn");
		if (tcphdr->rst)
			printk("+rst");
		if (tcphdr->psh)
			printk("+psh");
		if (tcphdr->ack)
			printk("+ack");
		if (tcphdr->urg)
			printk("+urg");
		if (tcphdr->ece)
			printk("+ece");
		if (tcphdr->cwr)
			printk("+cwr");

		if (ct) {
			printk(" tcp_conntrack=");

			if (ct->proto.tcp.state == TCP_CONNTRACK_NONE)
				printk("NONE ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_SYN_SENT)
				printk("SYN_SENT ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_SYN_RECV)
				printk("SYN_RECV ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_ESTABLISHED)
				printk("ESTABLISHED ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_FIN_WAIT)
				printk("FIN_WAIT ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_CLOSE_WAIT)
				printk("CLOSE_WAIT ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_LAST_ACK)
				printk("LAST_ACK ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_TIME_WAIT)
				printk("TIME_WAIT ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_CLOSE)
				printk("CLOSE ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_LISTEN)
				printk("LISTEN ");
			if (ct->proto.tcp.state == TCP_CONNTRACK_IGNORE)
				printk("IGNORE ");
		}
	}
#endif
}

void cs_forward_print_udp(struct udphdr *udphdr, struct nf_conn *ct)
{
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug &
	    (CS752X_ADAPT_IPV6_FORWARD | CS752X_ADAPT_IPV4_FORWARD)) {
		printk("udp src_port=%d dest_port=%d ", ntohs(udphdr->source),
		       ntohs(udphdr->dest));
	}
#endif
}

void cs_forward_print_esp(struct ip_esp_hdr * esphdr) {
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug &
	    (CS752X_ADAPT_IPV6_FORWARD | CS752X_ADAPT_IPV4_FORWARD)) {
		printk("esp spi=%x seq_no=%x ", esphdr->spi,
				   ntohl(esphdr->seq_no));

	}
#endif

}

int cs_localin_check_allow_localin_port(cs_kernel_accel_cb_t *cs_cb) {
	u16 sport, dport, *port_ptr;
	u16 input_sport, input_dport;
	sport = ntohs(cs_cb->output.l4_h.th.sport);
	dport = ntohs(cs_cb->output.l4_h.th.sport);
	input_sport = ntohs(cs_cb->input.l4_h.th.sport);
	input_dport = ntohs(cs_cb->input.l4_h.th.dport);

	for (port_ptr = allow_localin_port_list ; *port_ptr; port_ptr++) {
		if (sport == *port_ptr || dport == *port_ptr ||
			input_sport == *port_ptr || input_dport == *port_ptr) {
			return 1;
		}
	}
	cs_cb->common.sw_only = CS_SWONLY_STATE;
	return -1;
}

int cs_forward_check_bypass_tcp_port(struct tcphdr *tcphdr,
		cs_kernel_accel_cb_t *cs_cb)
{
	u16 sport, dport, *port_ptr;
	u16 input_sport, input_dport;

	sport = ntohs(tcphdr->source);
	dport = ntohs(tcphdr->dest);
	input_sport = ntohs(cs_cb->input.l4_h.th.sport);
	input_dport = ntohs(cs_cb->input.l4_h.th.dport);

	for (port_ptr = bypass_tcp_port_list ; *port_ptr; port_ptr++) {
		if (sport == *port_ptr || dport == *port_ptr ||
			input_sport == *port_ptr || input_dport == *port_ptr) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}
	}
	return 1;
}

int cs_forward_check_bypass_udp_port(struct udphdr *udphdr,
		cs_kernel_accel_cb_t *cs_cb)
{
	u16 sport, dport, *port_ptr;
	u16 input_sport, input_dport;

	sport = ntohs(udphdr->source);
	dport = ntohs(udphdr->dest);
	input_sport = ntohs(cs_cb->input.l4_h.uh.sport);
	input_dport = ntohs(cs_cb->input.l4_h.uh.dport);

	for (port_ptr = bypass_udp_port_list; *port_ptr; port_ptr++) {
		if (sport == *port_ptr || dport == *port_ptr||
			input_sport == *port_ptr || input_dport == *port_ptr) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}
	}
	return 1;
}

int cs_forward_fill_input_tcp(struct tcphdr *tcphdr,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->input.l4_h.th.sport = tcphdr->source;
	cs_cb->input.l4_h.th.dport = tcphdr->dest;

	return 1;
}

int cs_forward_fill_output_tcp(struct tcphdr *tcphdr, struct nf_conn *ct,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->output.l4_h.th.sport = tcphdr->source;
	cs_cb->output.l4_h.th.dport = tcphdr->dest;

	if (cs_cb->common.module_mask & CS_MOD_MASK_LOCAL_IN) {
		if (cs_localin_check_allow_localin_port(cs_cb) <= 0)
			return -1;
	} else if (cs_forward_check_bypass_tcp_port(tcphdr, cs_cb) <= 0)
		return -1;

	if (ct) {
		cs_cb->common.state = ct->proto.tcp.state;
		/*
		 * Once TCP enter FIN_WAIT status, the packet with ACK flg will be checked for
		 * status change. (CLOSE_WAIT and TIME_WAIT)
		 * So once after FIN_WAIT status, we need to let traffic go over SW path and
		 * SW will decide to forward or drop the packet.
		 */
		if (cs_cb->common.state >= TCP_CONNTRACK_FIN_WAIT) {
			if (ct->timeout.function == cs_forward_callback_ct_timeout) {
				/*need to delete hash entry based on ct*/
				cs_core_hmu_value_t hmu_value;
				int l3num = nf_ct_l3num(ct);
				hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
				hmu_value.mask = 0x08;
				DBG(printk("%s() delete hash for TCP state "
						"after CP_CONNTRACK_FIN_WAIT \n",
						__func__));


				if (l3num == NFPROTO_IPV4)
					hmu_value.value.swid64 =
						CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD);
				else
					hmu_value.value.swid64 =
						CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD);

				hmu_value.value.swid64 |= (u64)((u32)ct);
				cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64, &hmu_value);
				ct->timeout.function = death_by_timeout;
			}
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		} else if (cs_cb->common.state != TCP_CONNTRACK_ESTABLISHED) {
			/*
			 * we offload the ct only when it is under established state
			 */
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}
	} else {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return -1;
	}

	if (tcp_flag_word(tcphdr) &
			(TCP_FLAG_SYN | TCP_FLAG_FIN | TCP_FLAG_RST)) {
		/*
		 * we don't offload the packet with SYN/FIN/RST flag
		 */
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return -1;
	}

	if ((cs_cb->input.l4_h.th.sport ^ tcphdr->source)) {	/* tcp sport changed */
		cs_cb->output_mask |= CS_HM_L4_SP_MASK;
	}
	if ((cs_cb->input.l4_h.th.dport ^ tcphdr->dest)) {	/* tcp dport changed */
		cs_cb->output_mask |= CS_HM_L4_DP_MASK;
	}

	return 1;
}

int cs_forward_fill_input_udp(struct udphdr *udphdr,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->input.l4_h.uh.sport = udphdr->source;
	cs_cb->input.l4_h.uh.dport = udphdr->dest;

	return 1;
}

int cs_forward_fill_output_udp(struct udphdr *udphdr, struct nf_conn *ct,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->output.l4_h.uh.sport = udphdr->source;
	cs_cb->output.l4_h.uh.dport = udphdr->dest;

	if (cs_cb->common.module_mask & CS_MOD_MASK_LOCAL_IN) {
			if (cs_localin_check_allow_localin_port(cs_cb) <= 0)
				return -1;
	} else	if (cs_forward_check_bypass_udp_port(udphdr, cs_cb) <= 0)
		return -1;

	if (cs_hw_udp_offload_after_established == 1) {
		if (ct) {
			DBG(printk("\t	udp ct->status=%x %x %x \n", ct->status, IPS_ASSURED, IPS_ASSURED & (u_int8_t) ct->status));
			if (!test_bit(IPS_ASSURED_BIT, &ct->status)) {
				cs_cb->common.sw_only = CS_SWONLY_STATE;
				return -1;
			}
		} else {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return -1;
		}
	}


	if ((cs_cb->input.l4_h.uh.sport ^ udphdr->source)) {	/* udp sport changed */
		cs_cb->output_mask |= CS_HM_L4_SP_MASK;
	}

	if ((cs_cb->input.l4_h.uh.dport ^ udphdr->dest)) {	/* udp dport changed */
		cs_cb->output_mask |= CS_HM_L4_DP_MASK;
	}

	return 1;
}

int cs_forward_fill_input_esp(struct ip_esp_hdr *esph,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->input.vpn_h.ah_esp.spi = esph->spi;

	return 1;
}

int cs_forward_fill_output_esp(struct ip_esp_hdr *esph,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->output.vpn_h.ah_esp.spi = esph->spi;

	return 1;
}

int cs_forward_fill_input_ah(struct ip_auth_hdr *ahh,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->input.vpn_h.ah_esp.spi = ahh->spi;

	return 1;
}

int cs_forward_fill_output_ah(struct ip_auth_hdr *ahh,
		cs_kernel_accel_cb_t *cs_cb)
{
	cs_cb->output.vpn_h.ah_esp.spi = ahh->spi;

	return 1;
}

int cs_forward_fill_swid(struct nf_conn *ct, cs_kernel_accel_cb_t *cs_cb)
{
	unsigned int lifetime;
	u64 swid = 0;

	if (ct) {
		lifetime = __cs_get_ct_lifetime(cs_cb->input.l3_nh.iph.protocol, ct);
		cs_core_logic_set_lifetime(cs_cb, lifetime);
		swid = (u64)((u32)ct);
	} else {
		swid = 0;
		lifetime = cs_cb->lifetime;
	}

	if (cs_cb->input.l3_nh.iph.ver == 4)
		swid = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD) | swid;
	else if (cs_cb->input.l3_nh.iph.ver == 6)
		swid = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD) | swid;

	cs_core_logic_add_swid64(cs_cb, swid);
	DBG(printk("\t	swid=0x%llx lifetime=%d sec \n", swid, lifetime));

	return 1;
}

int cs_forward_check_input_ip_protocol(u_int8_t protonum, void *l4hdr,
		cs_kernel_accel_cb_t *cs_cb)
{
	if (protonum == IPPROTO_TCP) { /* TCP */
		return cs_forward_fill_input_tcp((struct tcphdr *)l4hdr, cs_cb);
	} else if (protonum == IPPROTO_UDP) { /* UDP */
		return cs_forward_fill_input_udp((struct udphdr *)l4hdr, cs_cb);
	}  else if (protonum == IPPROTO_ESP) { /* ESP */
		return cs_forward_fill_input_esp((struct ip_esp_hdr *)l4hdr, cs_cb);
	} else if (protonum == IPPROTO_AH) { /* AH */
		return cs_forward_fill_input_ah((struct ip_auth_hdr *)l4hdr, cs_cb);
	} else {	/* Only TCP/UDP ESP(IPSec) be offloaded to FE */
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return -1;
	}
	return -1;
}

int cs_forward_check_output_ip_protocol(u_int8_t protonum, void *l4hdr,
		cs_kernel_accel_cb_t *cs_cb, struct nf_conn *ct)
{
	if (protonum == IPPROTO_TCP) {
		return cs_forward_fill_output_tcp((struct tcphdr *)l4hdr, ct, cs_cb);
	} else if (protonum == IPPROTO_UDP) {
		return cs_forward_fill_output_udp((struct udphdr *)l4hdr, ct, cs_cb);
	} else if (protonum == IPPROTO_ESP) { /* ESP */
		return cs_forward_fill_output_esp((struct ip_esp_hdr *)l4hdr, cs_cb);
	} else if (protonum == IPPROTO_AH) { /* AH */
		return cs_forward_fill_output_ah((struct ip_auth_hdr *)l4hdr, cs_cb);
	} else {
		/* for IPSec, if the protocol in prerouting is ESP.
		 * after decryption, the protocol could be not TCP/UDP */
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return -1;
	}
}

void cs_forward_ipv4_nf_pre_routing(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	unsigned int hdroff = iph->ihl * 4;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	void *l4hdr = skb->data + hdroff;

	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	if (iph->protocol == IPPROTO_ESP) {
		DBG(printk("\n%s() %ld from %pI4 to %pI4 ipv%d protocol=%d ",
			__func__, jiffies, &iph->saddr, &iph->daddr,
			iph->version, iph->protocol));
		cs_forward_print_esp((struct ip_esp_hdr *)l4hdr);
		DBG(printk("\n"));
	}

	if (cs_cb->common.module_mask & CS_MOD_MASK_NAT ||
			cs_cb->common.module_mask & CS_MOD_MASK_IPV6_ROUTING) {
		/* this packet enter pre-routing again.
		 * In IPSec, after decryption, the outer IP header is stripped off into
		 * a inner packet and protocol will be "IPPROTO_RAW"(0xFF)
		 * So we should not fill cb input information again. */
		return;
	}

	cs_cb->input.l3_nh.iph.ver = iph->version;
	cs_cb->input.l3_nh.iph.sip = iph->saddr;
	cs_cb->input.l3_nh.iph.dip = iph->daddr;
	cs_cb->input.l3_nh.iph.protocol = iph->protocol;
	cs_cb->input.l3_nh.iph.tos = iph->tos;

	cs_cb->common.module_mask |= CS_MOD_MASK_NAT;

	if (cs_forward_check_input_ip_protocol(iph->protocol, l4hdr, cs_cb) <= 0)
		return;

	/* For SW_ONLY packet, IPSec may still need this information
	 * create hash for RE encryption/decryption
	 */
	//if (cs_cb->common.sw_only == CS_SWONLY_STATE)
	//	return;

	if (ipv4_is_multicast(iph->daddr)) {
		DBG(printk("%s() ipv4_is_multicast(%pI4) \n", __func__,
				&iph->daddr));
		cs_cb->common.module_mask |= CS_MOD_MASK_IPV4_MULTICAST;
	}
}

void cs_forward_ipv4_nf_post_routing(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	unsigned int hdroff = iph->ihl * 4;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	void *l4hdr = skb->data + hdroff;

	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	ct = nf_ct_get(skb, &ctinfo);

	if (iph->protocol == IPPROTO_TCP) {
		if (tcp_flag_word(l4hdr) & (TCP_FLAG_SYN)) {
			/* Don't print SYN packet during debug
			 * but still print FIN or RST packet */
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return;
		}
		DBG(printk("\n%s() %ld from %pI4 to %pI4 ipv%d ", __func__,
				jiffies, &iph->saddr, &iph->daddr,
				iph->version));
		cs_forward_print_tcp((struct tcphdr *)l4hdr, ct);
	} else {
		DBG(printk("\n%s() %ld from %pI4 to %pI4 ipv%d ", __func__,
			jiffies, &iph->saddr, &iph->daddr,
			iph->version));
		switch (iph->protocol) {
			case IPPROTO_UDP:
				cs_forward_print_udp((struct udphdr *)l4hdr, ct);
				break;
			case IPPROTO_ESP:
				cs_forward_print_esp((struct ip_esp_hdr *)l4hdr);
				break;
			default:
				DBG(printk("protocol=%d ", iph->protocol));
			}
	}

	DBG(printk(" module_mask 0x%x sw_only 0x%x\n",
		cs_cb->common.module_mask, cs_cb->common.sw_only));

	if (cs_cb->common.module_mask & CS_MOD_MASK_NAT ||
			cs_cb->common.module_mask & CS_MOD_MASK_IPV6_ROUTING) {
		cs_cb->output.l3_nh.iph.ver = iph->version;
		cs_cb->output.l3_nh.iph.protocol = iph->protocol;
		cs_cb->output.l3_nh.iph.sip = iph->saddr;
		cs_cb->output.l3_nh.iph.dip = iph->daddr;
		cs_cb->output.l3_nh.iph.tos = iph->tos;

		if (cs_forward_check_output_ip_protocol(iph->protocol,
				l4hdr, cs_cb, ct) <= 0)
			return;

		/* For SW_ONLY packet, IPSec may still need this information
		 * create hash for RE encryption/decryption
	 	 */
		if (cs_cb->common.sw_only == CS_SWONLY_STATE)
			return;

		if (cs_forward_check_bypass_expected_master_port(ct, cs_cb) <= 0) {
			return;
		}

		if (cs_forward_fill_swid(ct, cs_cb) <= 0)
			return;

		/* SIP changed */
		if ((cs_cb->input.l3_nh.iph.sip ^ iph->saddr)) {
			cs_cb->output_mask |= CS_HM_IP_SA_MASK;
		}
		/* DIP changed */
		if ((cs_cb->input.l3_nh.iph.dip ^ iph->daddr)) {
			cs_cb->output_mask |= CS_HM_IP_DA_MASK;
		}

		if ((cs_cb->input.l3_nh.iph.tos ^ iph->tos)) {
			cs_cb->output_mask |= CS_HM_DSCP_MASK;
			cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
		}

		if (cs_cb->common.module_mask & CS_MOD_MASK_IPV4_MULTICAST) {
			cs_mc_ipv4_post_routing(skb);
			return;
		}

		if (cs_forward4_enable() == 0) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return;
		}

		cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;
		cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;
	} else {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
	}
}

void cs_forward_ipv4_nf_mark_swonly(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	unsigned int hdroff = iph->ihl * 4;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	void *l4hdr = skb->data + hdroff;

	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	if (skb->mark == 0x1e) {
		// if using ipp2p to set SKB mark of P2P streams to 0x1e
		cs_cb->common.sw_only = CS_SWONLY_STATE;
	}
}

static int __cs_skip_ipv6_exthdr(const struct sk_buff *skb, int start,
				 u8 *nexthdrp, int len)
{
	u8 nexthdr = *nexthdrp;

	while (ipv6_ext_hdr(nexthdr)) {
		struct ipv6_opt_hdr hdr;
		int hdrlen;

		if (len < (int)sizeof(struct ipv6_opt_hdr))
			return -1;
		if (nexthdr == NEXTHDR_NONE)
			break;
		if (nexthdr == NEXTHDR_FRAGMENT)
			break;
		if (skb_copy_bits(skb, start, &hdr, sizeof(hdr)))
			BUG();
		if (nexthdr == NEXTHDR_AUTH)
			hdrlen = (hdr.hdrlen + 2) << 2;
		else
			hdrlen = ipv6_optlen(&hdr);

		nexthdr = hdr.nexthdr;
		len -= hdrlen;
		start += hdrlen;
	}

	*nexthdrp = nexthdr;
	return start;
}

void cs_forward_ipv6_nf_pre_routing(struct sk_buff *skb)
{
#ifdef CONFIG_IPV6
	struct ipv6hdr *hdr;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	unsigned int dataoff, extoff;
	u_int8_t protonum;
	void *l4hdr;
	int addr_type;

	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	if (cs_cb->common.module_mask & CS_MOD_MASK_IPV6_ROUTING ||
			cs_cb->common.module_mask & CS_MOD_MASK_NAT) {
		/* this packet enter pre-routing again.
		 * In IPSec, after decryption, the outer IP header is stripped off into
		 * a inner packet.
		 * So we should not fill cb input information again. */
		return;
	}

	hdr = ipv6_hdr(skb);

	protonum = hdr->nexthdr;
	extoff = skb_network_offset(skb) + sizeof(struct ipv6hdr);
	dataoff = __cs_skip_ipv6_exthdr(skb, extoff, &protonum,
					skb->len - extoff);
	l4hdr = skb->data + dataoff;

	cs_cb->input.l3_nh.ipv6h.protocol = protonum;
	memcpy(cs_cb->input.l3_nh.ipv6h.dip, &hdr->daddr, 16);
	memcpy(cs_cb->input.l3_nh.ipv6h.sip, &hdr->saddr, 16);
	memcpy(cs_cb->input.l3_nh.ipv6h.flow_lbl, hdr->flow_lbl, 3);
	cs_cb->input.l3_nh.ipv6h.ver = hdr->version;
	cs_cb->input.l3_nh.ipv6h.tc = ((hdr->priority << 4) & 0xf0) |
			((hdr->flow_lbl[0] >> 4) & 0x0f);

	cs_cb->common.module_mask |= CS_MOD_MASK_IPV6_ROUTING;

	if (cs_forward_check_input_ip_protocol(hdr->nexthdr, l4hdr, cs_cb) <= 0)
		return;

	addr_type = ipv6_addr_type(&hdr->daddr);

	if (addr_type & IPV6_ADDR_MULTICAST) {
		DBG(printk("%s() ipv6_is_multicast(%pI64) \n", __func__,
				&hdr->daddr));
		cs_cb->common.module_mask |= CS_MOD_MASK_IPV6_MULTICAST;
	}

	/* In IPv6, we don't support control nexthdr in hw accel acceleration.
	 * So just support the packet which doesn't contain control header.
	 * That means we just support IPv6 packet which hdr->nexthdr == protonum */

#endif
}

void cs_forward_ipv6_nf_post_routing(struct sk_buff *skb)
{
#ifdef CONFIG_IPV6
	struct ipv6hdr *hdr;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	unsigned int dataoff, extoff;
	u_int8_t protonum;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	void *l4hdr;

	if ((cs_cb == NULL) || (cs_cb->common.tag != CS_CB_TAG))
		return;

	ct = nf_ct_get(skb, &ctinfo);
	hdr = ipv6_hdr(skb);
	protonum = hdr->nexthdr;
	extoff = skb_network_offset(skb) + sizeof(struct ipv6hdr);
	dataoff = __cs_skip_ipv6_exthdr(skb, extoff, &protonum,
					skb->len - extoff);
	l4hdr = skb->data + dataoff;

	/* If this is meant for Ipsec return */
	//if (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC)
	//	return;

	if (protonum == IPPROTO_TCP) {
		if (tcp_flag_word(l4hdr) & (TCP_FLAG_SYN)) {
			/* Don't print SYN packet during debug
			 * but still print FIN or RST packet */
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return;
		}
		DBG(printk("\n%s() from %pI64 to %pI64 ipv%d", __func__,
				&hdr->saddr, &hdr->daddr, hdr->version));
		cs_forward_print_tcp((struct tcphdr *)l4hdr, ct);
	} else if (protonum == IPPROTO_UDP) {
		DBG(printk("\n%s() from %pI64 to %pI64 ipv%d", __func__,
				&hdr->saddr, &hdr->daddr, hdr->version));
		cs_forward_print_udp((struct udphdr *)l4hdr, ct);
	} else {
		DBG(printk("\n%s() from %pI64 to %pI64 ipv%d protocol=%d",
				__func__, &hdr->saddr, &hdr->daddr,
				hdr->version, protonum));
	}
	DBG(printk(" module_mask 0x%x\n", cs_cb->common.module_mask));

	if (cs_cb->common.module_mask & CS_MOD_MASK_IPV6_ROUTING ||
			cs_cb->common.module_mask & CS_MOD_MASK_NAT) {
		cs_cb->output.l3_nh.ipv6h.ver = hdr->version;
		cs_cb->output.l3_nh.ipv6h.protocol = protonum;
		memcpy(cs_cb->output.l3_nh.ipv6h.sip, &hdr->saddr, 16);
		memcpy(cs_cb->output.l3_nh.ipv6h.dip, &hdr->daddr, 16);
		cs_cb->output.l3_nh.ipv6h.tc = ((hdr->priority << 4) & 0xf0) |
				((hdr->flow_lbl[0] >> 4) & 0x0f);

		if (cs_forward_check_output_ip_protocol(protonum,
				l4hdr, cs_cb, ct) <= 0)
			return;

		if (cs_cb->common.sw_only == CS_SWONLY_STATE)
			return;

		if (cs_forward_check_bypass_expected_master_port(ct, cs_cb) <= 0) {
			return;
		}

		if (cs_forward_fill_swid(ct, cs_cb) <= 0)
			return;

		if (memcmp(cs_cb->input.l3_nh.ipv6h.sip, &hdr->saddr, 16) != 0) {
			cs_cb->output_mask |= CS_HM_IP_SA_MASK;
		}

		if (memcmp(cs_cb->input.l3_nh.ipv6h.dip, &hdr->daddr, 16) != 0) {
			cs_cb->output_mask |= CS_HM_IP_DA_MASK;
		}

		if (cs_cb->input.l3_nh.ipv6h.tc != cs_cb->output.l3_nh.ipv6h.tc) {
			cs_cb->output_mask |= CS_HM_DSCP_MASK;
			cs_cb->common.module_mask |= CS_MOD_MASK_QOS_FIELD_CHANGE;
		}

		if (cs_cb->common.module_mask & CS_MOD_MASK_IPV6_MULTICAST) {
			cs_mc_ipv6_post_routing(skb);
			return;
		}

		if (cs_forward6_enable() == 0) {
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return;
		}

		cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;
		cs_cb->common.dec_ttl = CS_DEC_TTL_ENABLE;

	} else {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
	}
#endif
}

static unsigned int ipv4_nf_cs_ne_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn) (struct sk_buff *))
{
	cs_forward_ipv4_nf_pre_routing(skb);
	return NF_ACCEPT;
}

static unsigned int ipv4_nf_cs_ne_confirm(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int (*okfn) (struct sk_buff *))
{
	cs_forward_ipv4_nf_post_routing(skb);
	cs_forward_ipv4_nf_mark_swonly(skb);
	return NF_ACCEPT;
}

static unsigned int ipv6_nf_cs_ne_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn) (struct sk_buff *))
{
	cs_forward_ipv6_nf_pre_routing(skb);
	return NF_ACCEPT;
}

static unsigned int ipv6_nf_cs_ne_confirm(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int (*okfn) (struct sk_buff *))
{
	cs_forward_ipv6_nf_post_routing(skb);
	return NF_ACCEPT;
}

static struct nf_hook_ops ipv4_cs_ne_ops[] __read_mostly = {
	{
		.hook = ipv4_nf_cs_ne_in,
		.owner = THIS_MODULE,
		.pf = NFPROTO_IPV4,
		.hooknum = NF_INET_PRE_ROUTING,
		.priority = NF_IP_PRI_CONNTRACK,
	},
	{
		.hook = ipv4_nf_cs_ne_confirm,
		.owner = THIS_MODULE,
		.pf = NFPROTO_IPV4,
		.hooknum = NF_INET_POST_ROUTING,
		.priority = NF_IP_PRI_CONNTRACK_CONFIRM,
	},
};

static struct nf_hook_ops ipv6_cs_ne_ops[] __read_mostly = {
	{
		.hook = ipv6_nf_cs_ne_in,
		.owner = THIS_MODULE,
		.pf = NFPROTO_IPV6,
		.hooknum = NF_INET_PRE_ROUTING,
		.priority = NF_IP6_PRI_CONNTRACK,
	},
	{
		.hook = ipv6_nf_cs_ne_confirm,
		.owner = THIS_MODULE,
		.pf = NFPROTO_IPV6,
		.hooknum = NF_INET_POST_ROUTING,
		.priority = NF_IP6_PRI_LAST,
	},
};

int cs_forward_ipv4_hook_remove(void)
{
	cs_core_hmu_unregister_watch(&ipv4_fwd_hmu_entry);
	nf_unregister_hooks(ipv4_cs_ne_ops, ARRAY_SIZE(ipv4_cs_ne_ops));

	return 0;
}

int cs_forward_ipv4_hook_insert(void)
{
	int ret = 0;

	/*for linux netfilter hook */
	ret = nf_register_hooks(ipv4_cs_ne_ops, ARRAY_SIZE(ipv4_cs_ne_ops));
	if (ret < 0) {
		pr_err("nf_conntrack_ipv4: can't register hooks.\n");
		return -1;
	}

	/*for core hmu */
	memset(&ipv4_fwd_hmu_entry, 0, sizeof(ipv4_fwd_hmu_entry));
	memset(&ipv4_fwd_hmu_value, 0, sizeof(ipv4_fwd_hmu_value));
	ipv4_fwd_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	ipv4_fwd_hmu_value.mask = 0x08;
	ipv4_fwd_hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD);
	ipv4_fwd_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;
	ipv4_fwd_hmu_entry.value_mask = &ipv4_fwd_hmu_value;
	ipv4_fwd_hmu_entry.callback = cs_forward_callback_core_hmu_notify;

	cs_core_hmu_register_watch(&ipv4_fwd_hmu_entry);

	return 0;
}

int cs_forward_ipv6_hook_remove(void)
{
	cs_core_hmu_unregister_watch(&ipv6_fwd_hmu_entry);
	nf_unregister_hooks(ipv6_cs_ne_ops, ARRAY_SIZE(ipv6_cs_ne_ops));

	return 0;
}

int cs_forward_ipv6_hook_insert(void)
{
	int ret = 0;

	/*for linux netfilter hook */
	ret = nf_register_hooks(ipv6_cs_ne_ops, ARRAY_SIZE(ipv6_cs_ne_ops));
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't register hooks.\n");
		return -1;
	}
	/*for core hmu */
	memset(&ipv6_fwd_hmu_entry, 0, sizeof(ipv6_fwd_hmu_entry));
	memset(&ipv6_fwd_hmu_value, 0, sizeof(ipv6_fwd_hmu_value));
	ipv6_fwd_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	ipv6_fwd_hmu_value.mask = 0x08;
	ipv6_fwd_hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD);
	ipv6_fwd_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;
	ipv6_fwd_hmu_entry.value_mask = &ipv6_fwd_hmu_value;
	ipv6_fwd_hmu_entry.callback = cs_forward_callback_core_hmu_notify;

	cs_core_hmu_register_watch(&ipv6_fwd_hmu_entry);

	return 0;
}

void cs_forward_callback_hma_ipv4(unsigned long notify_event,
		unsigned long value)
{
	DBG(printk("%s() cs hw accel ipv4 forward event%ld\n", __func__,
			notify_event));
	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			cs_core_hmu_clean_watch(&ipv4_fwd_hmu_entry);
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			cs_forward_ipv4_hook_remove();
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			cs_forward_ipv4_hook_insert();
			break;
	}

}

void cs_forward_callback_hma_ipv6(unsigned long notify_event,
		unsigned long value)
{
	DBG(printk("%s() cs hw accel ipv6 forward event %ld\n", __func__,
			notify_event));
	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			cs_core_hmu_clean_watch(&ipv6_fwd_hmu_entry);
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			cs_forward_ipv6_hook_remove();
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			cs_forward_ipv6_hook_insert();
			break;
	}
}

static unsigned int ipv4_nf_cs_local_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn) (struct sk_buff *))
{
	cs_kernel_accel_cb_t *cs_cb;
	void * skb_data_ptr;

	cs_cb = CS_KERNEL_SKB_CB(skb);
	if ((cs_cb != NULL) && (cs_cb->common.tag == CS_CB_TAG)) {
		if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_LOCAL_IN) == 0){
			cs_cb->common.sw_only = CS_SWONLY_STATE;
			return NF_ACCEPT;
		}
		if (cs_cb->input.raw.vlan_tpid != 0){
					cs_cb->common.sw_only = CS_SWONLY_STATE;
					return NF_ACCEPT;
		}
		cs_cb->common.module_mask |= CS_MOD_MASK_LOCAL_IN;

		cs_forward_ipv4_nf_post_routing(skb);

		if (cs_cb->common.sw_only == CS_SWONLY_HW_ACCEL) {
			skb_data_ptr = skb->data;
			skb->data = (void *) eth_hdr(skb);
			if (cs_cb->output.l3_nh.ipv6h.protocol == IPPROTO_TCP)
				cs_cb->common.sw_action_id = 1;
			else {
				cs_cb->common.sw_action_id = 2;
				extern u64 cs_localin_udp_payload_drop_bytes;
				extern u64 cs_localin_udp_traffic_drop_bytes;
				extern u64 cs_localin_udp_drop_packets;
				struct ethhdr *eth;
				eth = skb->data;
				u32 hdr_len = 0;
				if (eth->h_proto == 0x0008) {
					hdr_len = 14;
				} else {
					hdr_len = 18;
					eth = (struct ethhdr *) ((char *) skb->data + 18);
					if (eth->h_proto != 0x0008) {
						hdr_len += 4;
					}
				}
				cs_localin_udp_drop_packets++;
				cs_localin_udp_traffic_drop_bytes += skb->len + hdr_len;
				cs_localin_udp_payload_drop_bytes += (skb->len - 28);
			}
			cs_cb->action.voq_pol.d_voq_id =
					cs_cb->key_misc.orig_lspid * 8 + 48;
			cs_core_logic_add_connections(skb);

			skb->data = skb_data_ptr;

			DBG(printk("%s skb %p skb id %x dst voq %d\n", __func__, skb, ip_hdr(skb)->id,
					cs_cb->action.voq_pol.d_voq_id));
		}
	}

	return NF_ACCEPT;
}

static struct nf_hook_ops cs_localin_ne_ops[] __read_mostly = {
	{
		.hook = ipv4_nf_cs_local_in,
		.owner = THIS_MODULE,
		.pf = NFPROTO_IPV4,
		.hooknum = NF_INET_LOCAL_IN,
		.priority = NF_IP_PRI_CONNTRACK_DEFRAG,
	},
};


void cs_localin_insert(void) {
	int ret = 0;
	ret = nf_register_hooks(cs_localin_ne_ops, ARRAY_SIZE(cs_localin_ne_ops));
	if (ret < 0) {
		pr_err("nf_conntrack_localin: can't register hooks.\n");
		return -1;
	}
}


void cs_localin_remove(void) {
	nf_unregister_hooks(cs_localin_ne_ops, ARRAY_SIZE(cs_localin_ne_ops));
}

void cs_forward_callback_hma_localin(unsigned long notify_event,
		unsigned long value)
{
	DBG(printk("%s() cs hw accel localin event%ld\n", __func__,
			notify_event));
	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_ENABLE:
			break;
		case CS_HAM_ACTION_MODULE_DISABLE:
			break;
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			cs_core_hmu_clean_watch(&ipv4_fwd_hmu_entry);
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			cs_localin_remove();
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			cs_localin_insert();
			break;
	}

}

void cs_localin_init(void) {
	cs_localin_insert();
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_LOCAL_IN,
							   cs_forward_callback_hma_localin);

}

int cs_forward_init(void)
{
	cs_forward_ipv4_hook_insert();
	cs_forward_ipv6_hook_insert();
	cs_localin_init();
	/*for hw_accel_manager */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV4_FORWARD,
					       cs_forward_callback_hma_ipv4);
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV6_FORWARD,
					       cs_forward_callback_hma_ipv6);

	spin_lock_init(&cs_hw_accel_forward_delete_range_lock);
	ip_set_modified = &cs_hw_accel_forward_delete_ct_hash_by_src;
#if 0
	/*for core hmu */

	memset(&ipv4_fwd_hmu_entry, 0, sizeof(ipv4_fwd_hmu_entry));
	memset(&ipv4_fwd_hmu_value, 0, sizeof(ipv4_fwd_hmu_value));
	memset(&ipv6_fwd_hmu_entry, 0, sizeof(ipv6_fwd_hmu_entry));
	memset(&ipv6_fwd_hmu_value, 0, sizeof(ipv6_fwd_hmu_value));

	ipv4_fwd_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	ipv4_fwd_hmu_value.mask = 0x08;
	ipv4_fwd_hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD);
	ipv4_fwd_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;
	ipv4_fwd_hmu_entry.value_mask = &ipv4_fwd_hmu_value;
	ipv4_fwd_hmu_entry.callback = cs_forward_callback_core_hmu_notify;

	cs_core_hmu_register_watch(&ipv4_fwd_hmu_entry);

	ipv6_fwd_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	ipv6_fwd_hmu_value.mask = 0x08;
	ipv6_fwd_hmu_value.value.swid64 =
			CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD);
	ipv6_fwd_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;
	ipv6_fwd_hmu_entry.value_mask = &ipv6_fwd_hmu_value;
	ipv6_fwd_hmu_entry.callback = cs_forward_callback_core_hmu_notify;

	cs_core_hmu_register_watch(&ipv6_fwd_hmu_entry);
#endif
	return 0;
}

int cs_forward_exit(void)
{
	cs_forward_ipv4_hook_remove();
	cs_forward_ipv6_hook_remove();
	cs_localin_remove();

	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV4_FORWARD,
					       NULL);
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_IPV6_FORWARD,
					       NULL);
	ip_set_modified = NULL;
	return 0;
}

int cs_localout_check_allow_localout_port(u16 hdr_source, u16 hdr_dest) {
        u16 *port_ptr;
        u16 sport = ntohs(hdr_source);
        u16 dport = ntohs(hdr_dest);
        for (port_ptr = allow_localin_port_list ; *port_ptr; port_ptr++) {
                if (sport == *port_ptr || dport == *port_ptr) {
                        return 1;
                }
        }
        return -1;
}

