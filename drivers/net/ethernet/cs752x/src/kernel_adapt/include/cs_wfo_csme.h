#ifndef CS_KERNEL_WFO_CSME_SUPPORT
#define CS_KERNEL_WFO_CSME_SUPPORT 1


typedef enum {
    CS_WFO_CSME_STATUS_SUCCESS,
    CS_WFO_CSME_STATUS_FAIL,
} cs_wfo_csme_status_e;



typedef enum {
    CS_WFO_CSME_MAC_ENTRY_INVALID,
    CS_WFO_CSME_MAC_ENTRY_VALID,
} cs_wfo_csme_sta_mac_entry_status_e;


#define CS_WFO_CSME_STA_SUPPPORT_MAX    5
#define CS_WFO_CSME_STA_ENTRY_SIZE      13
typedef struct cs_wfo_csme_sta_mac_entry {
	unsigned char mac_da[ETH_ALEN];	            // Station MAC DA
	cs_wfo_csme_sta_mac_entry_status_e status;
	unsigned char egress_port_id;	            // Egress Port ID for core logic
} cs_wfo_csme_sta_mac_entry_s;
//A9 local MCAL MAC DA mapping table for CSME feature
static cs_wfo_csme_sta_mac_entry_s csme_sta_table_for_hw[CS_WFO_CSME_STA_ENTRY_SIZE];
// EGRESS PORT ID DEFINITION, cs_mc_sta_mac_entry.egress_port_id
#define CS_WFO_CSME_EGRESS_PORT_GMAC_0   0x00
#define CS_WFO_CSME_EGRESS_PORT_GMAC_1   0x01
#define CS_WFO_CSME_EGRESS_PORT_GMAC_2   0x02
#define CS_WFO_CSME_EGRESS_PORT_STA1     0x03
#define CS_WFO_CSME_EGRESS_PORT_STA2     0x04
#define CS_WFO_CSME_EGRESS_PORT_STA3     0x05
#define CS_WFO_CSME_EGRESS_PORT_STA4     0x06
#define CS_WFO_CSME_EGRESS_PORT_STA5     0x07


#define CS_WFO_CSME_GROUP_MEMBER_NUMBER  256
typedef struct cs_wfo_csme_sta_list {
	unsigned char hw_count;	            // Station numbers which assigned egress port id
	unsigned char sw_count;	            // Station numbers which didn't assign egress port id
	unsigned char total_count;	        // Station total numbers
	unsigned char reserved;	            // Reserved
	cs_wfo_csme_sta_mac_entry_s sta_list[CS_WFO_CSME_GROUP_MEMBER_NUMBER];
} cs_wfo_csme_sta_list_s;


//++BUG#39828
typedef enum {
    CS_WFO_CSME_WIFI_0,
    CS_WFO_CSME_WIFI_1,
} cs_wfo_csme_ad_id_e;
//--BUG#39828


void cs_wfo_csme_init(void);
cs_wfo_csme_status_e cs_wfo_csme_group_add_member(
            unsigned char *pgrp_addr, 
            unsigned char *pmember_addr,
            unsigned char *p_egress_port_id,
            __be32 group_ip,
            unsigned char ad_id);   //BUG#39828
cs_wfo_csme_status_e cs_wfo_csme_group_get_member(
            unsigned char *pgrp_addr, 
            unsigned char ad_id,    //BUG#39828
            cs_wfo_csme_sta_list_s *pcsme_send_list);
cs_wfo_csme_status_e cs_wfo_csme_del_group_member(
            unsigned char *pgrp_addr, 
            unsigned char *pmember_addr);
cs_wfo_csme_status_e cs_wfo_csme_del_member(unsigned char *pmember_addr);
void cs_wfo_csme_del_all_group(void);
unsigned char cs_wfo_csme_query_egress_port_id(unsigned char *pmember_addr);
int cs_wfo_csme_SnoopInspecting(struct sk_buff *skb, unsigned char ad_id);  //BUG#39828

#endif

