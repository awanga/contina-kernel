#ifndef _CS75XX_QOS_H_
#define _CS75XX_QOS_H_
#include <linux/types.h>
#include "cs_types.h"

#define CS_QOS_PREF_FLOW 0
#define CS_QOS_PREF_PORT 1

typedef enum {
    CS_QOS_MODE_DOT1P = 0, 
    CS_QOS_MODE_DSCP_TC = 1
} cs_qos_mode_t;

#define CS_QOS_DOT1P_DEFAULT_PRIORITY   0
#define CS_QOS_DSCP_DEFAULT_PRIORITY    0
#define CS_QOS_ARP_DEFAULT_PRIORITY     6
#define CS_QOS_EAPOL_DEFAULT_PRIORITY   6

typedef enum {
    CS_QOS_INGRESS_PORT_GMAC0,
    CS_QOS_INGRESS_PORT_GMAC1,
    CS_QOS_INGRESS_PORT_GMAC2,
    CS_QOS_INGRESS_PORT_CPU,
    CS_QOS_INGRESS_PORT_WLAN0,
    CS_QOS_INGRESS_PORT_WLAN1,
    CS_QOS_INGRESS_PORT_MAX_,
    CS_QOS_INGRESS_PORT_ALL = 0xFFFF,
} cs_qos_ingress_port_e;


#ifdef CONFIG_CS752X_ACCEL_KERNEL
cs_status_t cs_qos_map_mode_set( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_IN  cs_qos_mode_t mode );
cs_status_t cs_qos_map_mode_get( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_OUT cs_qos_mode_t *mode );
cs_status_t cs_qos_dot1p_map_set( CS_IN  cs_dev_id_t dev_id, 
                                  CS_IN  cs_port_id_t port_id, 
                                  CS_IN  cs_uint8_t dot1p, 
                                  CS_IN  cs_uint8_t priority);
cs_status_t cs_qos_dot1p_map_get( CS_IN  cs_dev_id_t dev_id, 
                                  CS_IN  cs_port_id_t port_id, 
                                  CS_IN  cs_uint8_t dot1p, 
                                  CS_OUT cs_uint8_t *priority);
cs_status_t cs_qos_dscp_map_set( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_IN  cs_uint8_t dscp, 
                                 CS_IN  cs_uint8_t priority );
cs_status_t cs_qos_dscp_map_get( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_IN  cs_uint8_t dscp, 
                                 CS_OUT cs_uint8_t *priority);
#endif //CONFIG_CS752X_ACCEL_KERNEL

#endif // _CS75XX_QOS_H_

