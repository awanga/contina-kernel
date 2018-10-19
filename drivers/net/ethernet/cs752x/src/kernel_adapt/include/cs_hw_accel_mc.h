#ifndef CS_KERNEL_MC_HW_ACC_SUPPORT
#define CS_KERNEL_MC_HW_ACC_SUPPORT 1

#include <linux/skbuff.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_nat.h>
#include <linux/list.h>


// Initialization
int cs_mc_init(void);
int cs_mc_exit(void);
void cs_mc_ipv4_post_routing(struct sk_buff *skb);
void cs_mc_ipv6_post_routing(struct sk_buff *skb);

#endif

