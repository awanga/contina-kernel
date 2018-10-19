#include <linux/list.h>		/* list_head structure */
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/jiffies.h>
#include <linux/export.h>

#include <mach/cs_types.h>
#include "cs_core_logic.h"
#include "cs_hw_accel_bridge.h"

#include <linux/netfilter_bridge.h>
#include <linux/rcupdate.h>
#include <linux/ip.h>
#include <../net/bridge/br_private.h>
#include "cs_hw_accel_manager.h"
#include "cs_core_hmu.h"

/*extern struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,
						 const unsigned char *addr);*/

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_BRIDGE) x
#else
#define DBG(x)	{}
#endif /* CONFIG_CS752X_PROC */

#define CS_MOD_BRIDGE_SWID (CS_SWID64_MASK(CS_SWID64_MOD_ID_BRIDGE) | 0x49444745)

cs_core_hmu_t bridge_hmu_entry;
cs_core_hmu_value_t bridge_hmu_value;

int cs_bridge_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_BRIDGE);
}

void cs_bridge_print(const char *func_name, struct sk_buff *skb,
		     const struct net_device *in, const struct net_device *out)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct iphdr *iph;

	if (!cs_cb)
		return;

	if (cs_adapt_debug & CS752X_ADAPT_BRIDGE) {

		printk("\n%s() %ld ", func_name, jiffies);
		if (in)
			printk("in dev: %s, ", in->name);
		if (out)
			printk("out dev: %s ", out->name);

		if (eth) {
			printk("sa_mac %pM ,da_mac %pM proto %x ",
			       eth->h_source, eth->h_dest, ntohs(eth->h_proto));
			if (skb->protocol == htons(ETH_P_IP)) {
				iph = ip_hdr(skb);
				printk(" from %pI4 to %pI4 ipv%d ", &iph->saddr,
				       &iph->daddr, iph->version);
			} else if (skb->protocol == htons(ETH_P_IPV6)) {
				printk(" IPv6 packet");
			}
		}
		printk("\n");
	}

}

#define br_port_get_rcu(dev) \
	((struct net_bridge_port *) rcu_dereference(dev->rx_handler_data))

static unsigned int cs_br_nf_pre_routing(unsigned int hook,
					 struct sk_buff *skb,
					 const struct net_device *in,
					 const struct net_device *out,
					 int (*okfn) (struct sk_buff *))
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	if (!cs_cb)
		return NF_ACCEPT;

	if (cs_cb->common.tag != CS_CB_TAG)
		return NF_ACCEPT;

	cs_cb->common.module_mask |= CS_MOD_MASK_BRIDGE;

	return NF_ACCEPT;
}

static unsigned int cs_br_nf_post_routing(unsigned int hook,
					  struct sk_buff *skb,
					  const struct net_device *in,
					  const struct net_device *out,
					  int (*okfn) (struct sk_buff *))
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	struct ethhdr *eth = eth_hdr(skb);
	const unsigned char *dest = eth->h_dest;
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	struct net_bridge *br;
	struct net_bridge_fdb_entry *dst;
	u16 vid = 0;

	if (!cs_cb)
		return NF_ACCEPT;

	if (cs_cb->common.tag != CS_CB_TAG)
		return NF_ACCEPT;

	/* If module mask contain L4 or PPPoE or VLAN,
	 * it would not use L2 Flow HW acceleration
	 * This is already judge in get_app_type_from_module_type()
	 */
	/*if (cs_cb->common.module_mask != CS_MOD_MASK_BRIDGE)
		return NF_ACCEPT;
	*/
	if (cs_bridge_enable() == 0) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return NF_ACCEPT;
	}

	if (cs_cb->common.sw_only == CS_SWONLY_STATE) {
		return NF_ACCEPT;
	}

	/*If there is no dst object, that will be flood packet */
	if (p == NULL) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return NF_ACCEPT;
	}

	if (!br_allowed_ingress(br, br_vlan_group_rcu(br), skb, &vid))
		return NF_ACCEPT;

	br = p->br;
	dst = __br_fdb_get(br, dest, vid);

	if (dst == NULL) {
		cs_cb->common.sw_only = CS_SWONLY_STATE;
		return NF_ACCEPT;
	}
	cs_bridge_print(__func__, skb, in, out);

	DBG(printk("\t module_mask 0x%x ageing_time=%d \n", cs_cb->common.module_mask,
		br->ageing_time));

	cs_cb->common.output_dev = (struct net_device *)dst->dst->dev;
	cs_cb->common.sw_only = CS_SWONLY_HW_ACCEL;
	cs_core_logic_add_swid64(cs_cb, CS_MOD_BRIDGE_SWID);
	cs_core_logic_set_lifetime(cs_cb, br->ageing_time / HZ);

	return NF_ACCEPT;
}

static struct nf_hook_ops br_cs_nf_ops[] __read_mostly = {
	{
		.hook = cs_br_nf_pre_routing,
		.pf = PF_BRIDGE,
	 	.hooknum = NF_BR_PRE_ROUTING,
	 	.priority = NF_BR_PRI_BRNF + 1,
	},
	{
		.hook = cs_br_nf_post_routing,
		.pf = PF_BRIDGE,
		.hooknum = NF_BR_POST_ROUTING,
		.priority = NF_BR_PRI_LAST - 1,
	},

};

static int cs_bridge_callback_core_logic_notify(u32 watch_bitmask,
						cs_core_hmu_value_t * value,
						u32 status)
{
	if (watch_bitmask == CS_CORE_HMU_WATCH_SWID64) {
		DBG(printk("%s() core_hmu_notify status=%x \n", __func__,
				status));

		if (value != NULL)
			DBG(printk("%s() type=%x, swid64=0x%016llx ", __func__,
					value->type, value->value.swid64));
		DBG(printk("\n"));
	}
	return 0;
}

static cs_core_hmu_t bridge_hmu_in_mac_sa = {
	.watch_bitmask = CS_CORE_HMU_WATCH_IN_MAC_SA,
	.value_mask = NULL,
	.callback = cs_bridge_callback_core_logic_notify,
};

static cs_core_hmu_t bridge_hmu_out_mac_da = {
	.watch_bitmask = CS_CORE_HMU_WATCH_OUT_MAC_DA,
	.value_mask = NULL,
	.callback = cs_bridge_callback_core_logic_notify,
};



int cs_bridge_hook_remove(void)
{
	nf_unregister_hooks(br_cs_nf_ops, ARRAY_SIZE(br_cs_nf_ops));
	cs_core_hmu_unregister_watch(&bridge_hmu_out_mac_da);
	cs_core_hmu_unregister_watch(&bridge_hmu_in_mac_sa);
	cs_core_hmu_unregister_watch(&bridge_hmu_entry);
	return 0;
}

int cs_bridge_hook_insert(void)
{
	/*for linux netfilter */
	nf_register_hooks(br_cs_nf_ops, ARRAY_SIZE(br_cs_nf_ops));

	/*for core hmu */
	memset(&bridge_hmu_entry, 0, sizeof(bridge_hmu_entry));
	memset(&bridge_hmu_value, 0, sizeof(bridge_hmu_value));
	bridge_hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	bridge_hmu_value.mask = 0xFF;
	bridge_hmu_value.value.swid64 = CS_MOD_BRIDGE_SWID;
	bridge_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_SWID64;

	bridge_hmu_entry.value_mask = &bridge_hmu_value;
	bridge_hmu_entry.callback = cs_bridge_callback_core_logic_notify;
	cs_core_hmu_register_watch(&bridge_hmu_entry);
	cs_core_hmu_register_watch(&bridge_hmu_in_mac_sa);
	cs_core_hmu_register_watch(&bridge_hmu_out_mac_da);

	return 0;
}

void cs_bridge_callback_enable_proc_notify(unsigned long notify_event,
					   unsigned long value)
{
	DBG(printk("%s() cs hw accel bridge event %ld\n", __func__, notify_event));

	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			cs_core_hmu_clean_watch(&bridge_hmu_entry);
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			cs_bridge_hook_remove();
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			cs_bridge_hook_insert();
			break;
	}
}

int cs_bridge_init(void)
{
	cs_bridge_hook_insert();

	/*for hw accel manager */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_BRIDGE,
					       cs_bridge_callback_enable_proc_notify);


	return 0;
} /* cs_bridge_init */

int cs_bridge_exit(void)
{
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_BRIDGE,
					       NULL);
	cs_bridge_hook_remove();
	return 0;
}

/* 
 * Purpose: return the last use time (in jiffies) of a MAC SA hash entry. This function is hooked to
 *          kernel bridge entry timeout function to refresh bridge entry timeout value.
 *          hmu polls for hash entry activity every <cs_ne_hash_timer_period, CS_HAHS_TIMER_PERIOD, 10> 
 *          (configurable) seconds. Here we just get
 *          the last hash entry used time from hmu and return to kernel in f->updated
 *          NOTE: design changed - we only get the last used time for MAC SA since MAC aging should only
 *          be applied to MAC SA 
 * Called by: br_fdb_cleanup, br_fdb_fillbuf 
 */
void k_jt_cs_bridge_br_fdb_get_lastuse(struct net_bridge_fdb_entry *f)
{
	cs_core_hmu_value_t hmu_value;
	unsigned long last_use;
	int i;
	int ret;

	hmu_value.type = CS_CORE_HMU_WATCH_IN_MAC_SA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = f->addr.addr[5 - i];
	hmu_value.mask = 0;

	DBG(printk("%s get last use fdb=%pM\n", __func__, f->addr.addr));

	ret = cs_core_hmu_get_last_use(CS_CORE_HMU_WATCH_IN_MAC_SA, &hmu_value,
				&last_use);
	if (ret == 0) {
		DBG(printk("\t%s get last use by in_mac_sa %pM = %ld\n", __func__,
			f->addr.addr, last_use));
		if (time_after(last_use, f->updated))
			f->updated = last_use;
	} else {
		DBG(printk("\t%s unable to get last use by in_mac_sa, ret = %d\n", __func__, ret));
	}
#if 1
	hmu_value.type = CS_CORE_HMU_WATCH_OUT_MAC_DA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = f->addr.addr[5 - i];

	ret = cs_core_hmu_get_last_use(CS_CORE_HMU_WATCH_OUT_MAC_DA, &hmu_value,
				&last_use);
	if (ret == 0) {
		DBG(printk("\t%s get last use by out_mac_da %pM = %ld\n", __func__,
			f->addr.addr, last_use));
		f->updated = last_use;
	} else {
		DBG(printk("\t%s unable to get last use by out_mac_da, ret = %d\n", __func__, ret));
	}
#endif
}

void k_jt_cs_bridge_br_fdb_delete(struct net_bridge_fdb_entry *f)
{
	cs_core_hmu_value_t hmu_value;
	unsigned long last_use;
	int i;
	int ret;
	DBG(printk("%s delete fdb=%pM\n", __func__, f->addr.addr));

	hmu_value.type = CS_CORE_HMU_WATCH_IN_MAC_SA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = f->addr.addr[5 - i];
	hmu_value.mask = 0;

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_MAC_SA, &hmu_value);


	hmu_value.type = CS_CORE_HMU_WATCH_OUT_MAC_DA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = f->addr.addr[5 - i];

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_OUT_MAC_DA, &hmu_value);


}

EXPORT_SYMBOL(k_jt_cs_bridge_br_fdb_get_lastuse);
EXPORT_SYMBOL(k_jt_cs_bridge_br_fdb_delete);



