#ifndef G2_COMMON_API_H
#define G2_COMMON_API_H

#include "cs_network_types.h"

#define CS_IN 

#define CS_ETH_ADDR_LEN 6
#define CS_VLAN_TAG_MAX 2

typedef enum {
        CS_PORT_ENCAP_INTERNAL  = 0,
        CS_PORT_ENCAP_PPPOE_E   = 1,
        CS_PORT_ENCAP_ETH_E     = 2,
        CS_PORT_ENCAP_ETH_1Q_E  = 3,
        CS_PORT_ENCAP_ETH_QinQ_E        = 4,
} cs_port_encap_type_t;

#endif /* G2_COMMON_API_H */

