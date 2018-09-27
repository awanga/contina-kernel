#ifndef G2_LPM_API_H
#define G2_LPM_API_H

#include "cs_common_api.h"

typedef enum {
        CS_APP_L3_ROUTE         = 0, /* For IPv4/6 Routing APP */
        CS_APP_IPSEC            = 1, /* For IPSEC APP */
        CS_APP_PPTP             = 2, /* For PPTP APP */
} cs_app_type_t;


typedef struct cs_fwd_result {
        cs_app_type_t             app_type;  			/* which application created this forward result */
        cs_uint32_t               port_id;   			/* Physical port ID that would give base_voq */
        cs_uint32_t               voq_base;  			/* packet will go to which voq_base */
        cs_uint32_t               voq_pol_rslt_idx; 		/* VoQ policy result index, kept by this layer, upper layer no need to fill it */
        cs_uint32_t               fwd_rslt_idx; 		/* kept by this layer, upper layer no need to fill it */
	cs_port_encap_type_t   	  enc_type;		        /* encyption type */		
	cs_uint32_t		  fvlan_idx;		        /* index to vlan tag for replace */
	cs_uint32_t		  mac_idx;		        /* index to MAC address for replace */
	cs_uint8_t                src_mac[CS_ETH_ADDR_LEN];     /* source MAC address for replace */
        cs_uint8_t                dest_mac[CS_ETH_ADDR_LEN];    /* destination MAC address for replace */
        cs_uint32_t               tag[CS_VLAN_TAG_MAX];         /* VLAN tag for replace */
        cs_uint16_t               pppoe_session_id;             /* PPPoE seesion ID for replace */
	cs_uint8_t		  mac_type;		        /* type for MAC replacement */
} cs_fwd_result_t;

typedef struct cs_lpm {
        cs_app_type_t             app_type;  			/* which application created this LPM */
        cs_ip_address_t           prefix;    			/* prefix data for LPM */
        cs_uint32_t               fwd_rslt_idx; 		/* kept by this layer, upper layer no need to fill it */
        //cs_uint32_t               lpm_idx;   			/* kept by this layer, upper layer no need to fill it */
} cs_lpm_t;

cs_status_t cs_lpm_fwd_result_init(CS_IN cs_dev_id_t device_id);
cs_status_t cs_lpm_fwd_result_shut(CS_IN cs_dev_id_t device_id);
cs_status_t cs_fwd_result_add(CS_IN cs_dev_id_t device_id, CS_IN cs_fwd_result_t *fwd_result, CS_IN cs_uint32_t *index);
cs_status_t cs_fwd_result_search(CS_IN cs_dev_id_t device_id, CS_IN cs_fwd_result_t *fwd_result, CS_IN cs_uint32_t *index);
cs_status_t cs_fwd_result_get_by_index(CS_IN cs_dev_id_t device_id, CS_IN cs_uint32_t index, CS_IN cs_fwd_result_t *fwd_result);
cs_status_t cs_fwd_result_update(CS_IN cs_dev_id_t device_id,  CS_IN cs_uint32_t index, CS_IN cs_fwd_result_t *fwd_result);
cs_status_t cs_fwd_result_delete(CS_IN cs_dev_id_t device_id, CS_IN cs_fwd_result_t *fwd_result);
cs_status_t cs_fwd_result_delete_entry_by_index(CS_IN cs_dev_id_t device_id, CS_IN cs_uint32_t index);
cs_status_t cs_lpm_add(CS_IN cs_dev_id_t device_id, CS_IN cs_lpm_t *lpm);
cs_status_t cs_lpm_get(CS_IN cs_dev_id_t device_id, CS_IN cs_lpm_t *lpm, cs_uint32_t *lpm_index);
cs_status_t cs_lpm_get_by_index(CS_IN cs_dev_id_t device_id, CS_IN cs_uint32_t index, CS_IN cs_lpm_t *lpm);
cs_status_t cs_lpm_delete(CS_IN cs_dev_id_t device_id, CS_IN cs_lpm_t *lpm);

#endif /* G2_LPM_API_H */
