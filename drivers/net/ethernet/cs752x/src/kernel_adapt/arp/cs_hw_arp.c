
#include <linux/export.h>
#include <net/neighbour.h>
#include <net/arp.h>
#include <net/ndisc.h>
#include <linux/jiffies.h>
#include <mach/cs_network_types.h>
#include "cs_hw_accel_arp.h"
#include "cs_core_hmu.h"
#include "cs_mut.h"

#define PFX     "CS_HW_ARP"
#define PRINT(format, args...) printk(KERN_WARNING PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)
#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#define DBG(x) {if (cs_adapt_debug & CS752X_ADAPT_ARP) x;}
#else
#define DBG(x) { }
#endif

static cs_core_hmu_value_t *construct_core_hmu_value_for_ipv4(u32 *ip)
{
	cs_core_hmu_value_t *new_core_value;

	new_core_value = cs_zalloc(sizeof(cs_core_hmu_value_t), GFP_ATOMIC);
	if (new_core_value == NULL)
		return NULL;

	new_core_value->type = CS_CORE_HMU_WATCH_OUT_IPV4_DA;
	new_core_value->value.ip_addr[0] = *ip;
	new_core_value->mask = 0;

	return new_core_value;
}

static cs_core_hmu_value_t *construct_core_hmu_value_for_ipv6(u32 *ip)
{
	cs_core_hmu_value_t *new_core_value;

	new_core_value = cs_zalloc(sizeof(cs_core_hmu_value_t), GFP_ATOMIC);
	if (new_core_value == NULL)
		return NULL;

	new_core_value->type = CS_CORE_HMU_WATCH_OUT_IPV6_DA;
	new_core_value->value.ip_addr[0] = ip[0];
	new_core_value->value.ip_addr[1] = ip[1];
	new_core_value->value.ip_addr[2] = ip[2];
	new_core_value->value.ip_addr[3] = ip[3];
	new_core_value->mask = 0;

	return new_core_value;
}

static int cs_neigh_get_ip_last_use(cs_ip_afi_t afi, u32 *ip,
						unsigned long *last_use)
{
	cs_core_hmu_value_t *core_value;
	unsigned long curr_last_use;
	int ret;

	if (afi == CS_IPV6) {
		core_value = construct_core_hmu_value_for_ipv6(ip);
		if (core_value == NULL)
			return -1;
		
		ret = cs_core_hmu_get_last_use(CS_CORE_HMU_WATCH_OUT_IPV6_DA,
				core_value, &curr_last_use);
	} else {
		/* afi == CS_IPV4 */
		core_value = construct_core_hmu_value_for_ipv4(ip);
		if (core_value == NULL)
			return -1;

		ret = cs_core_hmu_get_last_use(CS_CORE_HMU_WATCH_OUT_IPV4_DA,
				core_value, &curr_last_use);
	}
		

	if (ret == 0) {
		if (afi == CS_IPV6) {
			DBG(PRINT("IP %pI6 get last use = %ld\n", ip, curr_last_use));
		} else {
			DBG(PRINT("IP %pI4 get last use = %ld\n", ip, curr_last_use));
		}
		*last_use = curr_last_use;
	}

	cs_free(core_value);
	return ret;
}

void cs_neigh_delete(void *data)
{
	struct neighbour *neigh = (struct neighbour *)data;
	cs_core_hmu_value_t *core_value;
	u32 *ip;

	if (neigh == NULL || neigh->nud_state == NUD_NOARP)
		return;

	ip = (u32 *) neigh->primary_key;

	if (neigh->tbl == &nd_tbl && neigh->tbl->family == AF_INET6) {
		/* IPv6 */
		core_value = construct_core_hmu_value_for_ipv6(ip);
		if (core_value == NULL)
			return;
		
		DBG(PRINT("try to delete hashes by IP %pI6\n", ip));
		
		cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_OUT_IPV6_DA, core_value);
		
		core_value->type = CS_CORE_HMU_WATCH_IN_IPV6_SA;
		cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_IPV6_SA, core_value);
	} else if (neigh->tbl == &arp_tbl && neigh->tbl->family == AF_INET) {
		/* IPv4 */
		core_value = construct_core_hmu_value_for_ipv4(ip);
		if (core_value == NULL)
			return;

		DBG(PRINT("try to delete hashes by IP %pI4\n", ip));

		cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_OUT_IPV4_DA, core_value);
		core_value->type = CS_CORE_HMU_WATCH_IN_IPV4_SA;
		cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_IPV4_SA, core_value);
	}
		
	cs_free(core_value);

	return;
} /* cs_neigh_delete */
EXPORT_SYMBOL(cs_neigh_delete);

void cs_neigh_update_used(void *data)
{
	struct neighbour *neigh = (struct neighbour *)data;
	unsigned long use_jiffies = 0;
	int ret;
	cs_ip_afi_t afi;

	/*
	   neigh->nud_state is defined in neighbour.h,
	   neigh->type is defined in rtnetlink.h,
	   suppose nud_state = NUD_REACHABLE, NUD_PROBE, NUD_STALE, or NUD_DELAY
	   suppose type = RTN_UNICAST
	 */
	if (neigh == NULL || neigh->nud_state == NUD_NOARP)
		return;


	if (neigh->tbl == &nd_tbl)
		afi = CS_IPV6;
	else if (neigh->tbl == &arp_tbl)
		afi = CS_IPV4;
	else
		return;

	/*
	 * find the garbage collector of this neighbour entry.
	 * update neighbour's last used with the jiffies that's
	 * kept on garbage collector.
	 */
	ret = cs_neigh_get_ip_last_use(afi, (u32 *)neigh->primary_key,
								&use_jiffies);
	if (ret != 0)
		return;

	DBG(PRINT("jiffies neigh(%lu) vs gc(%lu)\n", neigh->used, use_jiffies));

	if (time_before(neigh->used, use_jiffies))
		neigh->used = use_jiffies;

	return;
} /* cs_neigh_update_used */
EXPORT_SYMBOL(cs_neigh_update_used);

static int cs_arp_core_hmu_callback(u32 watch_bitmask,
		cs_core_hmu_value_t * value, u32 status)
{
	/* well we don't really need callback here!! */
	DBG(PRINT("watch_bitmask = %x, status = %x\n", watch_bitmask, status));
	return 0;
}

static cs_core_hmu_t arp_hmu_out_ipv4_da = {
	.watch_bitmask = CS_CORE_HMU_WATCH_OUT_IPV4_DA,
	.value_mask = NULL,
	.callback = cs_arp_core_hmu_callback,
};

static cs_core_hmu_t arp_hmu_in_ipv4_sa = {
	.watch_bitmask = CS_CORE_HMU_WATCH_IN_IPV4_SA,
	.value_mask = NULL,
	.callback = cs_arp_core_hmu_callback,
};

static cs_core_hmu_t arp_hmu_out_ipv6_da = {
	.watch_bitmask = CS_CORE_HMU_WATCH_OUT_IPV6_DA,
	.value_mask = NULL,
	.callback = cs_arp_core_hmu_callback,
};

static cs_core_hmu_t arp_hmu_in_ipv6_sa = {
	.watch_bitmask = CS_CORE_HMU_WATCH_IN_IPV6_SA,
	.value_mask = NULL,
	.callback = cs_arp_core_hmu_callback,
};


void cs_hw_arp_init(void)
{
	int ret = 0;


	ret |= cs_core_hmu_register_watch(&arp_hmu_out_ipv4_da);
	ret |= cs_core_hmu_register_watch(&arp_hmu_in_ipv4_sa);
	ret |= cs_core_hmu_register_watch(&arp_hmu_out_ipv6_da);
	ret |= cs_core_hmu_register_watch(&arp_hmu_in_ipv6_sa);
	if (ret != 0) {
		PRINT("unable to register HMU for ARP!\n");
		return;
	}

	return;
} /* cs_hw_arp_init */

void cs_hw_arp_exit(void)
{
	int ret = 0;

	ret |= cs_core_hmu_unregister_watch(&arp_hmu_out_ipv4_da);
	ret |= cs_core_hmu_unregister_watch(&arp_hmu_in_ipv4_sa);
	ret |= cs_core_hmu_unregister_watch(&arp_hmu_out_ipv6_da);
	ret |= cs_core_hmu_unregister_watch(&arp_hmu_in_ipv6_sa);
	if (ret != 0) {
		PRINT("unable to unregister HMU for ARP!\n");
		return;
	}
} /* cs_hw_arp_exit */

