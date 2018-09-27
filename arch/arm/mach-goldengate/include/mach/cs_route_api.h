#ifndef G2_ROUTE_API_H
#define G2_ROUTE_API_H

#include "cs_common_api.h"
#include "cs_tunnel.h"

#define CS_IPV4_ROUTE_MAX 	64
#define CS_IPV6_ROUTE_MAX  	8

extern cs_uint32_t  g_cs_ipv4_route_max;
extern cs_uint32_t  g_cs_ipv6_route_max;

typedef cs_uint32_t 	cs_intf_id_t;

typedef enum {
	CS_L3_NEXTHOP_UNKNOWN		= 0, /* For Ethernet unknown result points to CPU */
	CS_L3_NEXTHOP_DIRECT		= 1, /* Resolved and valid port encapsmust be present */
	CS_L3_NEXTHOP_INTF		= 2, /* For broadcast media INTF result points to CPU */
	CS_L3_NEXTHOP_TUNNEL_IP_IN_IP	= 3, /* send to PE */
  	CS_L3_NEXTHOP_TUNNEL_IPSEC	= 4, /* send to PE */
	CS_L3_NEXTHOP_TUNNEL_L2TP_IPSEC = 5, /* send to PE */
	CS_L3_NEXTHOP_TUNNEL_PPTP	= 6, /* send to PE */
	CS_L3_NEXTHOP_TUNNEL_GRE 	= 7, /* send to PE */
	CS_L3_NEXTHOP_TUNNEL_L2TP	= 8, /* send to PE */
	CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE	= 9, /* send to PE */
} cs_l3_nexthop_type_t;

typedef struct cs_pppoe_port_encap {
	cs_uint8_t			src_mac[CS_ETH_ADDR_LEN];
	cs_uint8_t			dest_mac[CS_ETH_ADDR_LEN];
	cs_uint32_t			tag[CS_VLAN_TAG_MAX];
	cs_uint16_t			pppoe_session_id;
} cs_pppoe_port_encap_t;

typedef struct cs_eth_port_encap {
	cs_uint8_t			src_mac[CS_ETH_ADDR_LEN];
	cs_uint32_t			tag[CS_VLAN_TAG_MAX];
} cs_eth_port_encap_t;

typedef union cs_port_union_encap {
        cs_pppoe_port_encap_t  pppoe;
        cs_eth_port_encap_t    eth;
} cs_port_union_encap_t;

typedef struct cs_port_encap {
	cs_port_encap_type_t 	type; /* address family identifier */
	cs_port_union_encap_t port_encap;
} cs_port_encap_t;

typedef struct cs_l3_nexthop_id {
	cs_l3_nexthop_type_t		nhop_type;
	cs_ip_address_t		addr;
	cs_uint32_t			intf_id;  /* 32-bit number: */
} cs_l3_nexthop_id_t;

typedef union cs_l3_nexthop_intf_binding_s {
	cs_uint32_t             port_id;  /* Physical port ID that would give base_voq */
        cs_uint32_t             tunnel_id; /* apply when nhop_type is TUNNEL */
        cs_uint32_t             ipsec_said; /* apply when nhop_type is IPSEC TUNNEL */
} cs_l3_nexthop_intf_binding_t;

typedef union cs_l3_nexthop_encap_s {
        cs_port_encap_t port_encap; /* this is filled by callback function */
	cs_tunnel_cfg_t tunnel_cfg;
} cs_l3_nexthop_encap_t;

typedef struct cs_l3_nexthop {
	cs_l3_nexthop_id_t		nhid;
	cs_uint8_t			da_mac[CS_ETH_ADDR_LEN];
	/* The following are filled by the call back function */
	cs_l3_nexthop_intf_binding_t    id;
	cs_l3_nexthop_encap_t	encap;
} cs_l3_nexthop_t;

typedef struct cs_l3_route {
	cs_ip_address_t		prefix;
	cs_l3_nexthop_t		nexthop;
} cs_l3_route_t;

cs_status_t cs_l3_route_init_0(CS_IN cs_dev_id_t device_id);
cs_status_t cs_l3_route_shut(CS_IN cs_dev_id_t device_id);
cs_status_t cs_l3_nexthop_add(CS_IN cs_dev_id_t device_id, CS_IN cs_l3_nexthop_t *nexthop, CS_OUT cs_uint32_t *index);
cs_status_t cs_l3_nexthop_get(CS_IN cs_dev_id_t device_id, CS_IN cs_uint32_t index, CS_OUT cs_l3_nexthop_t *nexthop);
cs_status_t cs_l3_nexthop_update(CS_IN cs_dev_id_t device_id, CS_IN cs_l3_nexthop_t *nexthop);
cs_status_t cs_l3_nexthop_delete(CS_IN cs_dev_id_t device_id, CS_IN cs_l3_nexthop_t *nexthop);
cs_status_t cs_l3_nexthop_show(CS_IN cs_dev_id_t device_id, CS_IN_OUT cs_l3_nexthop_t *nexthop, CS_IN cs_boolean_t next);
cs_status_t cs_l3_route_add(CS_IN cs_dev_id_t device_id, CS_IN cs_l3_route_t *route);
cs_status_t cs_l3_route_get(CS_IN cs_dev_id_t device_id, CS_IN cs_l3_route_t *route);
cs_status_t cs_l3_route_delete(CS_IN cs_dev_id_t device_id, CS_IN  cs_l3_route_t *route);
cs_status_t cs_l3_nhintf_all_route_delete(CS_IN cs_dev_id_t device_id, CS_IN  cs_uint32_t intf_id);
cs_status_t cs_l3_route_show(CS_IN cs_dev_id_t device_id, CS_IN_OUT cs_l3_route_t *route, CS_IN cs_boolean_t next);
cs_status_t cs_nat_subnet_add(CS_IN cs_dev_id_t device_id, CS_IN cs_uint32_t intf_id, CS_IN cs_ip_address_t *nat_subnet);
cs_status_t cs_nat_subnet_delete(CS_IN cs_dev_id_t device_id, CS_IN cs_uint32_t intf_id, CS_IN cs_ip_address_t *nat_subnet);


#endif /* G2_ROUTE_API_H */

