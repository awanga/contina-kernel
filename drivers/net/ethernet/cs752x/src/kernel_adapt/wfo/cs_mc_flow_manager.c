/**
  Copyright © 2014 GreenWave Systems Inc.
  @file cs_mc_flow_manager.c
  @brief Multicast flow manager for Cortina CS75xx
  @author Stefan Hallas Mulvad
  @author Bobby Bouche
  */

#include <mach/cs_flow_api.h>
#include <mach/cs_network_types.h>
#include <linux/inetdevice.h>
#include <linux/types.h>
#include <linux/mroute.h>
#include <net/ip.h>

static struct {
	const char *name;
	const cs_uint8_t phy_port;
	const cs_uint8_t strip_8021p_tag;
	const cs_uint16_t eth_type;
	const cs_tpid_encap_type_t tpid_encap_type;
	const cs_uint16_t vlan_id;
	const cs_uint16_t voq_offset;
} interface_port_map[] = {
	{ "wl0", CS_PORT_OFLD0, 1, 0x0800, CS_VLAN_TPID_NONE, 0x0, 0 },
	{ "wl0.1", CS_PORT_CUSTOM1, 1, 0x0800, CS_VLAN_TPID_8100, 0x0, 24 },
	{ "wl1", CS_PORT_CUSTOM0, 1, 0x0800, CS_VLAN_TPID_NONE, 0x0, 72 },
	{ "eth2.1", CS_PORT_GMAC2, 0, 0x8100, CS_VLAN_TPID_8100, 0x1, 0 },
	{ "eth2.2", CS_PORT_CUSTOM2, 0, 0x8100, CS_VLAN_TPID_8100, 0x2, 16 }
};

static char *has_outer_tag[] = {
	"eth2",
};

/* prototypes */
void cs75xx_update_multicast_flow_entry(struct net_device *, struct net_device *, struct mfcctl *, struct mfc_cache *, int);

void cs75xx_update_multicast_flow_entry(struct net_device *ingress_dev, struct net_device *egress_dev, struct mfcctl *mfc, struct mfc_cache *c, int vifi)
{
	unsigned int i, j, routed = 1;
	struct in_device *ipv4_dev;
	cs_flow_t flow_entry;

	if ((c->mfc_un.res.flow_vifs[vifi >> 0x3] & (1 << (vifi & 0x7))) == 0) {
		if (mfc != NULL && mfc->mfcc_ttls[vifi] > 0 && (c->mfc_un.res.ttls[vifi] == 0 || c->mfc_un.res.ttls[vifi] == 255)) {
			for (
				i = 0;
				i < (sizeof(interface_port_map) / sizeof(interface_port_map[0])) &&
				strcmp(egress_dev->name, interface_port_map[i].name) != 0;
				++i
			);

			if (i < (sizeof(interface_port_map) / sizeof(interface_port_map[0]))) {
				rcu_read_lock();
				while (egress_dev->master != NULL)
					egress_dev = egress_dev->master;
				ipv4_dev = __in_dev_get_rcu(egress_dev);
				if (ipv4_dev != NULL) {
					for_primary_ifa(ipv4_dev)
						if (inet_ifa_match(mfc->mfcc_origin.s_addr, ifa)) {
							routed = 0;
							break;
						}
					endfor_ifa(ipv4_dev);
				}
				rcu_read_unlock();

				memset(&flow_entry, 0, sizeof(cs_flow_t));
				flow_entry.flow_type = CS_FLOW_TYPE_MC_MEMBER;
				flow_entry.ingress_pkt.phy_port = 0;
				ip_eth_mc_map(mfc->mfcc_mcastgrp.s_addr, (char *)&flow_entry.ingress_pkt.da_mac);
				flow_entry.ingress_pkt.eth_type = 0x8100;
				if (interface_port_map[i].strip_8021p_tag) {
					flow_entry.ingress_pkt.tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
					flow_entry.ingress_pkt.tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
				} else {
					if (ingress_dev != NULL)
						for (
							j = 0;
							j < (sizeof(has_outer_tag) / sizeof(has_outer_tag[0])) &&
							strcmp(ingress_dev->name, has_outer_tag[j]) != 0;
							++j
						);
					else
						j = sizeof(has_outer_tag) / sizeof(has_outer_tag[0]);

					if (j < (sizeof(has_outer_tag) / sizeof(has_outer_tag[0])))
						/* Packets ingressing this interface have an outer 802.1p tag added by the switch */
						flow_entry.ingress_pkt.tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
					else
						flow_entry.ingress_pkt.tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
					flow_entry.ingress_pkt.tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
				}
				flow_entry.ingress_pkt.tag[0].vlan_id = 0;
				flow_entry.ingress_pkt.tag[1].vlan_id = 0;
				flow_entry.ingress_pkt.da_ip.afi = CS_IPV4;
				flow_entry.ingress_pkt.da_ip.ip_addr.ipv4_addr = mfc->mfcc_mcastgrp.s_addr;
				flow_entry.ingress_pkt.da_ip.addr_len = 32;
				flow_entry.ingress_pkt.sa_ip.afi = CS_IPV4;
				flow_entry.ingress_pkt.sa_ip.ip_addr.ipv4_addr = mfc->mfcc_origin.s_addr;
				flow_entry.ingress_pkt.sa_ip.addr_len = 32;
				flow_entry.ingress_pkt.protocol = 0x11;
				flow_entry.egress_pkt.phy_port = interface_port_map[i].phy_port;
				ip_eth_mc_map(mfc->mfcc_mcastgrp.s_addr, (char *)&flow_entry.egress_pkt.da_mac);
				if (routed)
					memcpy(&flow_entry.egress_pkt.sa_mac, egress_dev->dev_addr, sizeof(flow_entry.egress_pkt.sa_mac));
				flow_entry.egress_pkt.eth_type = interface_port_map[i].eth_type;
				flow_entry.egress_pkt.tag[0].tpid_encap_type = interface_port_map[i].tpid_encap_type;
				flow_entry.egress_pkt.tag[0].vlan_id = interface_port_map[i].vlan_id;
				flow_entry.egress_pkt.tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
				flow_entry.egress_pkt.tag[1].vlan_id = 0;
				flow_entry.egress_pkt.da_ip.afi = CS_IPV4;
				flow_entry.egress_pkt.da_ip.ip_addr.ipv4_addr = mfc->mfcc_mcastgrp.s_addr;
				flow_entry.egress_pkt.da_ip.addr_len = 32;
				flow_entry.egress_pkt.sa_ip.afi = CS_IPV4;
				flow_entry.egress_pkt.sa_ip.ip_addr.ipv4_addr = mfc->mfcc_origin.s_addr;
				flow_entry.egress_pkt.sa_ip.addr_len = 32;
				flow_entry.egress_pkt.protocol = 0x11;
				flow_entry.dec_ttl = routed;
				flow_entry.voq_offset = interface_port_map[i].voq_offset;
				flow_entry.flag = 3;
				if (cs_flow_add(0, &flow_entry) == CS_OK) {
					c->mfc_un.res.flow_ids[vifi] = flow_entry.flow_id;
					c->mfc_un.res.flow_vifs[vifi >> 0x3] |= (1 << (vifi & 0x7));
				}
			}
		}
	} else if (mfc == NULL || mfc->mfcc_ttls[vifi] == 0) {
		if (cs_flow_delete(0, c->mfc_un.res.flow_ids[vifi]) == CS_OK)
			c->mfc_un.res.flow_vifs[vifi >> 0x3] &= ~(1 << (vifi & 0x7));
	}
}
