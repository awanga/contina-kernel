#ifndef G2_FLOW_HASH_API_H
#define G2_FLOW_HASH_API_H

#include "cs_common_api.h"

typedef enum {
	CS_FLOW_TYPE_L2 = 1,           /* CORE_FWD_APP_TYPE_L2_FLOW */
	CS_FLOW_TYPE_L4 = 2,           /* CORE_FWD_APP_TYPE_L3_GENERIC */
	CS_FLOW_TYPE_L3_MC = 3,        /* CORE_FWD_APP_TYPE_L3_MCAST */
	CS_FLOW_TYPE_L4_NATT = 4,      /* CORE_FWD_APP_TYPE_TUNNEL_L4_L7 */ 
	CS_FLOW_TYPE_L2_PASS = 5,      /* CORE_FWD_APP_TYPE_L2_PASS */
	CS_FLOW_TYPE_MC_MEMBER = 6,    /* CORE_FWD_APP_TYPE_MCAST_CTRL */
	CS_FLOW_TYPE_L2_DA_SA = 7,           /* CORE_FWD_APP_TYPE_L2_DA_SA */
	CS_FLOW_TYPE_MC_L7_FILTER = 8,           /* CORE_FWD_APP_TYPE_MC_L7_FILTER */
	CS_FLOW_TYPE_MC_SW_FILTER = 9,           /* CORE_FWD_APP_TYPE_MC_SW_FILTER */
	CS_FLOW_TYPE_MC_HASH = 10,           /* CORE_FWD_APP_TYPE_MC_L7_FILTER */
} cs_flow_type_t;

typedef enum {
	CS_VLAN_TPID_NONE = -1,		/* no vlan tag */
	CS_VLAN_TPID_8100 = 0, 		/* TPID 0x8100 */
	CS_VLAN_TPID_9100 = 1, 		/* TPID 0x9100 */
	CS_VLAN_TPID_88A8 = 2, 		/* TPID 0x88a8 */
	CS_VLAN_TPID_9200 = 3, 		/* TPID 0x9200 */
} cs_tpid_encap_type_t;

typedef enum {
	CS_FLOW_SWID_BRIDGE = 0, 
	CS_FLOW_SWID_FORWARD = 1, 
	CS_FLOW_SWID_VPN = 2, 
	CS_FLOW_SWID_MAX = 3,
} cs_flow_swid_type_t;

typedef struct cs_vlan {
	cs_tpid_encap_type_t	tpid_encap_type;
	cs_uint16_t		vlan_id;
	cs_uint8_t		priority;
} cs_vlan_t;

typedef struct cs_l4_tcp {
	cs_uint16_t	sport;
	cs_uint16_t	dport;
} cs_l4_tcp_t;

typedef struct cs_l4_udp {
        cs_uint16_t     sport;
        cs_uint16_t     dport;
} cs_l4_udp_t;

typedef struct cs_l4_esp {
        cs_uint32_t     spi_idx;
} cs_l4_esp_t;

typedef union cs_l4_header {
	cs_l4_tcp_t	tcp;
	cs_l4_udp_t	udp;
	cs_l4_esp_t	esp;
} cs_l4_header_t;

typedef struct cs_pkt_info {
	cs_uint8_t	phy_port;	/* port id: please refer to cs_phy_port_id_t, 0xff=drop the packet */

	/* L2 layer */
	cs_uint8_t	da_mac[CS_ETH_ADDR_LEN];
	cs_uint8_t	sa_mac[CS_ETH_ADDR_LEN];
	cs_uint16_t	eth_type;	
	cs_vlan_t	tag[CS_VLAN_TAG_MAX];
	cs_uint8_t      pppoe_session_id_valid;
	cs_uint16_t	pppoe_session_id;
	
	/* L3/L4 layer */
	cs_ip_address_t	da_ip;
	cs_ip_address_t	sa_ip;
	cs_uint8_t	tos;	/* DSCP[7..2]+ECN[1:0] */
	cs_uint8_t	protocol;
	cs_l4_header_t	l4_header;		

	/* L7 layer */
	cs_uint32_t	natt_4_bytes; /* 4 bytes which locates after TCP or UDP header */
} cs_pkt_info_t;

#define FLOW_FLAG_SKIP_HMU		0x0001		/* Skip hooking into Cortina hash monitor tree (flow-based HMU) */
#define FLOW_FLAG_QOS_POL		0x0002		/* Enable QoS policy; the VoQ offset will be added into VoQ base of fwd_result */
typedef struct cs_flow {
	cs_uint16_t	flow_id;			/* unique flow hash ID */
	cs_flow_type_t	flow_type;
	cs_pkt_info_t	ingress_pkt;
	cs_pkt_info_t	egress_pkt;
	cs_uint16_t	dec_ttl; 			/* 0: keep original ttl, 1: decrease ttl*/
	cs_uint16_t	voq_offset;			/* VoQ offset for egress phy_port, VoQ index = (egress phy_port) * 8 + voq_offset, if phy_port <= 4 */
	cs_uint32_t	life_time; 			/* 0 means no timeout, unit is tick (10ms) */
	cs_uint32_t	swid_array[CS_FLOW_SWID_MAX]; 	/* for flow mgmt. among apps. (such as arp or netfilter) */
	cs_uint32_t	sw_action_id;
	cs_uint32_t	flag;				/* bit mask, see comment of FLOW_FLAG_* */
} cs_flow_t;

cs_status_t cs_flow_add(CS_IN cs_dev_id_t device_id, CS_IN_OUT cs_flow_t *p_flow);
cs_status_t cs_flow_get(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t hash_index, CS_OUT cs_flow_t *p_flow);
cs_status_t cs_flow_delete(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t flow_id);
cs_status_t cs_flow_get_lastuse_tickcount(CS_IN cs_dev_id_t device_id, CS_IN cs_uint16_t flow_id, CS_OUT cs_uint32_t *lastuse_tickcount);

#endif /* G2_FLOW_HASH_API_H */

