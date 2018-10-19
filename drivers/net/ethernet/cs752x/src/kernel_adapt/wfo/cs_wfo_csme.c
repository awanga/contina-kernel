#include <linux/mutex.h>
#include <linux/list.h>		/* list_head structure */
#include <linux/if_ether.h>
#include <linux/if.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/igmp.h>
#include <linux/export.h>

#include <mach/cs_types.h>
#include "cs_wfo_csme.h"
#include "cs_mut.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_wfo_csme;
extern u32 cs_wfo_debug;
#define DBG(x)	if (cs_wfo_debug & CS752X_WFO_DBG_CSME) x
#else
#define DBG(x)	{}
#endif /* CONFIG_CS752X_PROC */


static DEFINE_MUTEX(csme_list_mutex);


/* Is packet type is either leave or report */
#define IS_IGMPV2_REPORT_LEAVE_PACKET(type) (\
                      (IGMP_HOST_MEMBERSHIP_REPORT == type)\
                   || (IGMPV2_HOST_MEMBERSHIP_REPORT == type)\
                   || (IGMP_HOST_LEAVE_MESSAGE  == type)\
                  )

#define IS_IGMPV3_REPORT_LEAVE_PACKET(type) (\
                      (IGMPV3_HOST_MEMBERSHIP_REPORT == type)\
                  )

/* Calculate the group record length*/
#define IGMPV3_GRP_REC_LEN(x) (8 + (4 * x->grec_nsrcs) + (4 * x->grec_auxwords) )
 
/* List: Group for key.
 * Translate mac address from multicast to unicast by this lsit.
 */
struct csme_grp_member_list {
	struct list_head        grp_member_list;
	unsigned char           grp_member_addr[ETH_ALEN];  /* DA MAC address */
	unsigned char           egress_port_id;             // Value: 0x03~0x07
	                                                    //        0xFF - Invalid
	unsigned char           ad_id;                      //BUG#39828
};

struct csme_grp_list {
    struct list_head        grp_list;
    unsigned char           group_addr[ETH_ALEN];
    __be32                  group_ip;
    
    struct list_head        grp_member_head;
};

struct list_head grp_head;


/* List: MAC DA for key.
 * Remove the station from the list for all groups quickly 
 * if that station disassociate from G2
 */
struct csme_mac_da_grp_list {
	struct list_head            mac_da_grp_list;
	unsigned char               group_addr[ETH_ALEN];
};

struct csme_mac_da_list {
	struct list_head            mac_da_list;
	unsigned char               mac_da[ETH_ALEN];
	unsigned char               egress_port_id;     // Value: 0x03~0x07
	                                                //        0xFF - Invalid
    struct list_head            mac_da_grp_head;
};

struct list_head mac_da_head;


/*
 *   Local Functions
 */
typedef enum {
    CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS,
    CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL,
    CS_WFO_CSME_LIST_ENTRY_STATUS_EXIST,
    CS_WFO_CSME_LIST_ENTRY_STATUS_FULL,
} cs_wfo_csme_list_entry_status_e;


static cs_wfo_csme_list_entry_status_e cs_wfo_csme_get_empty_entry( 
            unsigned char *pmac_da, 
            unsigned char *p_egress_port_id )
{
    int i;
    unsigned char empty_index = 0xFF, index = 0xFF;
    unsigned char found_empty = 0, found_entry = 0;

    
    for (i=0; i<CS_WFO_CSME_STA_ENTRY_SIZE; i++) {
        if (csme_sta_table_for_hw[i].status == CS_WFO_CSME_MAC_ENTRY_INVALID) {
            if (found_empty)
                continue;
            found_empty = 1;
            empty_index = i;
        } else {
            if (memcmp( csme_sta_table_for_hw[i].mac_da, pmac_da, ETH_ALEN) == 0) {
                found_entry = 1;
                index = i;
                break;
            }
        }/* if (csme_sta_table_for_hw[i].status == CS_WFO_CSME_MAC_ENTRY_INVALID) */
    }/* for (i=0; i<CS_WFO_CSME_STA_ENTRY_SIZE; i++) */

    DBG(printk("%s:: found_entry %d , found_empty %d, empty_index 0x%x\n", 
            __func__, found_entry, found_empty, empty_index));


    if (found_entry) {
        *p_egress_port_id = csme_sta_table_for_hw[index].egress_port_id;
        DBG(printk("%s: EXIST egress_port_id 0x%x, index %d\n", 
                __func__, *p_egress_port_id, index));
        return CS_WFO_CSME_LIST_ENTRY_STATUS_EXIST;
    }
    if (empty_index == 0xFF) {
        return CS_WFO_CSME_LIST_ENTRY_STATUS_FULL;
    }

    
    memcpy(csme_sta_table_for_hw[empty_index].mac_da, pmac_da, ETH_ALEN);
    csme_sta_table_for_hw[empty_index].status = CS_WFO_CSME_MAC_ENTRY_VALID;
    *p_egress_port_id = csme_sta_table_for_hw[empty_index].egress_port_id;
    DBG(printk("%s: EMPTY egress_port_id 0x%x, empty_index %d\n", 
            __func__, *p_egress_port_id, empty_index));
    
    
    return CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS;
}/* cs_wfo_csme_get_empty_entry() */


static cs_wfo_csme_list_entry_status_e cs_wfo_csme_add_mac_da_grp_list(
            unsigned char *pgrp_addr, 
            unsigned char *pmember_addr,
            unsigned char *p_egress_port_id)
{
	struct list_head            *nextMACDA, *nextMACDATmp;
	struct list_head            *nextMACDAGrp, *nextMACDAGrpTmp;
	struct csme_mac_da_list     *pMACDA;
	struct csme_mac_da_grp_list *pMACDAGrp;
    unsigned char macdaNotFound = true;
    cs_wfo_csme_list_entry_status_e retStatus = CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL;

    DBG(printk("%s:: pgrp_addr %pM, pmember_addr %pM\n", 
                __func__, pgrp_addr, pmember_addr));

    /* Add MAC DA for key list */
    /* Try to find pgrp_addr in MAC DA for key list */
    if (!list_empty(&mac_da_head)) {
        /* Traverse all elements of the csme_mac_da_list list */
        list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) {
            pMACDA = (struct csme_mac_da_list*) list_entry( 
                                                nextMACDA, 
                                                struct csme_mac_da_list, 
                                                mac_da_list );
            
            // Find pmember_addr in csme_mac_da_list list
            if (memcmp( pMACDA->mac_da, 
                        pmember_addr, 
                        ETH_ALEN ) == 0) {
                            
                macdaNotFound = false;
                *p_egress_port_id = pMACDA->egress_port_id;
                DBG(printk("%s:: egress_port_id 0x%x\n", 
                            __func__, *p_egress_port_id));
                
                if (!list_empty(&(pMACDA->mac_da_grp_head))) {
                    /* Traverse all elements of the csme_mac_da_grp_list list */
                    list_for_each_safe( nextMACDAGrp, 
                                        nextMACDAGrpTmp, 
                                        &(pMACDA->mac_da_grp_head) ) {
                        pMACDAGrp = (struct csme_mac_da_grp_list*) list_entry( 
                                                    nextMACDAGrp, 
                                                    struct csme_mac_da_grp_list, 
                                                    mac_da_grp_list);
                        
                        // found group_addr in csme_mac_da_grp_list
                        if (memcmp( pMACDAGrp->group_addr, 
                                    pgrp_addr, 
                                    ETH_ALEN ) == 0) {
                            return CS_WFO_CSME_LIST_ENTRY_STATUS_EXIST;
                        }
                    }/* list_for_each_safe(nextMACDAGrp) */
                }/* if (!list_empty(&(pMACDA->mac_da_grp_head))) */
                break;
            }/* if (memcmp()) */
        }/* list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) */
    }/* if (!list_empty(&mac_da_head)) */

    if (macdaNotFound) {
   	    // add pmember_addr into MAC DA for key list
    	pMACDA = (struct csme_mac_da_list*)
    	            cs_malloc(sizeof(struct csme_mac_da_list), GFP_ATOMIC);

    	if (!pMACDA) {
            DBG(printk("%s,%d:: ERROR allocate memory for pMACDA fail\n",
                     __func__, __LINE__));
    	    return CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL;
    	}
        
        // Get egress_port_id from MCAL mapping table
        retStatus = cs_wfo_csme_get_empty_entry(pmember_addr, p_egress_port_id);
        switch (retStatus) {
            case CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS:
            case CS_WFO_CSME_LIST_ENTRY_STATUS_EXIST:
                break;
            case CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL:
            case CS_WFO_CSME_LIST_ENTRY_STATUS_FULL:
                *p_egress_port_id = 0xFF;
               break;
        }/* switch */
        
        INIT_LIST_HEAD(&(pMACDA->mac_da_grp_head));
    	memcpy(pMACDA->mac_da, pmember_addr, ETH_ALEN);
    	pMACDA->egress_port_id = *p_egress_port_id;
    	list_add(&(pMACDA->mac_da_list), &mac_da_head);
    }/* if (macdaNotFound) */
    
    // add pgrp_addr into csme_mac_da_grp_list list
	pMACDAGrp = (struct csme_mac_da_grp_list*)
	                cs_malloc(sizeof(struct csme_mac_da_grp_list), GFP_ATOMIC);

	if (!pMACDAGrp) {
        DBG(printk("%s,%d:: ERROR allocate memory for pMACDAGrp fail\n",
                 __func__, __LINE__));
	    return CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL;
	}
    
	memcpy(pMACDAGrp->group_addr, pgrp_addr, ETH_ALEN);
	list_add(&(pMACDAGrp->mac_da_grp_list), &(pMACDA->mac_da_grp_head));

    return CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS;
}/* cs_wfo_csme_add_mac_da_grp_list() */


static cs_wfo_csme_list_entry_status_e cs_wfo_csme_add_grp_member_list(
            unsigned char *pgrp_addr, 
            unsigned char *pmember_addr,
            __be32 group_ip,
            unsigned char *p_egress_port_id,
            unsigned char ad_id )       //BUG#39828
{
	struct list_head            *nextGrp, *nextGrpTmp;
	struct csme_grp_list        *pGrp;
	struct csme_grp_member_list *pGrpMember;
    unsigned char grpNotFound = true;
    cs_wfo_csme_list_entry_status_e retStatus = CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL;
    

    DBG(printk("%s:: pgrp_addr %pM, pmember_addr %pM, group_ip %pI4, egress_port_id 0x%x\n", 
                __func__, pgrp_addr, pmember_addr, &group_ip, *p_egress_port_id));

    /* Add to Group for key list */
    /* Try to find pmember_addr in Group for key list */
    if (!list_empty(&grp_head)) {
        /* Traverse all elements of the csme_grp_list list */
        list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) {
            pGrp = (struct csme_grp_list*) list_entry( nextGrp, 
                                                       struct csme_grp_list, 
                                                       grp_list );
                                                       
            if (memcmp( pGrp->group_addr, pgrp_addr, ETH_ALEN ) == 0) {
                grpNotFound = false;
                break;
            }/* if (memcmp( pGrp->group_addr, pgrp_addr, ETH_ALEN ) == 0) */
        }/* list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) */
    }/* if (!list_empty(&grp_head)) */
    
    if (grpNotFound) {
   	    // add pgrp_addr into Group for key list
    	pGrp = (struct csme_grp_list*)
    	            cs_malloc(sizeof(struct csme_grp_list), GFP_ATOMIC);

    	if (!pGrp) {
            DBG(printk("%s,%d:: ERROR allocate memory for pGrp fail\n",
                     __func__, __LINE__));
    	    return CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL;
    	}

        INIT_LIST_HEAD(&(pGrp->grp_member_head));
    	memcpy(pGrp->group_addr, pgrp_addr, ETH_ALEN);
    	pGrp->group_ip = group_ip;
    	list_add(&(pGrp->grp_list), &grp_head);
     }/* if (grpNotFound) */
    
    // add pmember_addr into csme_grp_member_list list
	pGrpMember = (struct csme_grp_member_list*)
	                cs_malloc(sizeof(struct csme_grp_member_list), GFP_ATOMIC);

	if (!pGrpMember) {
        DBG(printk("%s,%d:: ERROR allocate memory for pGrpMember fail\n",
                 __func__, __LINE__));
	    return CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL;
	}
    
	memcpy(pGrpMember->grp_member_addr, pmember_addr, ETH_ALEN);
	pGrpMember->egress_port_id = *p_egress_port_id;
	pGrpMember->ad_id = ad_id;      //BUG#39828
	list_add(&(pGrpMember->grp_member_list), &(pGrp->grp_member_head));
    
    return CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS;
}/* cs_wfo_csme_add_grp_member_list */

void cs_wfo_csme_dump_table(void)
{
	struct list_head            *nextGrp, *nextGrpTmp;
	struct list_head	        *nextGrpMember, *nextGrpMemberTmp;
	struct csme_grp_list        *pGrp;
	struct csme_grp_member_list *pGrpMember;

	struct list_head            *nextMACDA, *nextMACDATmp;
	struct list_head            *nextMACDAGrp, *nextMACDAGrpTmp;
	struct csme_mac_da_list     *pMACDA;
	struct csme_mac_da_grp_list *pMACDAGrp;
    unsigned char               index = 1;
    
    DBG(printk("===========================================\n"));
    DBG(printk("%s:: Group for key list\n", __func__));

    /* Delete Group for key list */
    if (!list_empty(&grp_head)) {
        /* Traverse all elements of the csme_grp_list list */
        list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) {
            pGrp = (struct csme_grp_list*) list_entry( nextGrp, 
                                                       struct csme_grp_list, 
                                                       grp_list );
            DBG(printk("   (%d)\n", index++));
            DBG(printk("   group_addr     : %pM\n", pGrp->group_addr));
            DBG(printk("   group_ip       : %pI4\n", &(pGrp->group_ip)));
                    
            if (!list_empty(&(pGrp->grp_member_head))) {
                /* Traverse all elements of the csme_grp_member_list list */
                list_for_each_safe( nextGrpMember, 
                                    nextGrpMemberTmp, 
                                    &(pGrp->grp_member_head) ) {
                    pGrpMember = (struct csme_grp_member_list*) list_entry( 
                                           nextGrpMember, 
                                           struct csme_grp_member_list, 
                                           grp_member_list);
                    DBG(printk("   grp_member_addr: %pM\n", pGrpMember->grp_member_addr));
                    DBG(printk("   egress_port_id : 0x%2.2x\n", pGrpMember->egress_port_id));
                }/* list_for_each_safe(nextGrpMember) */
            }/* if (!list_empty(&(pGrp->grp_list)) */
         }/* list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) */
    }/* if (!list_empty(&mac_da_head)) */
    
	
    index = 1;
    DBG(printk("===========================================\n"));
    DBG(printk("%s:: MAC DA for key list\n", __func__));
    /* Delete MAC DA for key list */
    if (!list_empty(&mac_da_head)) {
        /* Traverse all elements of the csme_mac_da_list list */
        list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) {
            pMACDA = (struct csme_mac_da_list*) list_entry( 
                                    nextMACDA, 
                                    struct csme_mac_da_list, 
                                    mac_da_list );
            DBG(printk("   (%d)\n", index++));
            DBG(printk("   mac_da        : %pM\n", pMACDA->mac_da));
            DBG(printk("   egress_port_id: 0x%2.2x\n", pMACDA->egress_port_id));
            if (!list_empty(&(pMACDA->mac_da_grp_head))) {
                /* Traverse all elements of the csme_mac_da_grp_list list */
                list_for_each_safe( nextMACDAGrp, 
                                    nextMACDAGrpTmp, 
                                    &(pMACDA->mac_da_grp_head) ) {
                    pMACDAGrp = (struct csme_mac_da_grp_list*) list_entry( 
                                            nextMACDAGrp, 
                                            struct csme_mac_da_grp_list, 
                                            mac_da_grp_list);

                    DBG(printk("   group_addr    : %pM\n", pMACDAGrp->group_addr));
                }/* list_for_each_safe(nextMACDAGrp, nextMACDAGrpTmp, &(pMACDA->mac_da_list)) */
            }/* if (!list_empty(&(pMACDA->mac_da_list))) */
        }/* list_for_each_safe(nextMACDATmp, nextMACDAGrpTmp, &grp_member_head) */
    }/* if (!list_empty(&mac_da_grp_head)) */
    DBG(printk("===========================================\n"));

    return;
}/* cs_wfo_csme_dump_table() */


static void cs_wfo_csme_dump_mac_entry(void)
{
    int i;
    DBG(printk("==================================\n"));
    for (i=0; i<5; i++) {
        DBG(printk("Index: %d\n", i));
        DBG(printk("mac_da        : %pM\n", csme_sta_table_for_hw[i].mac_da));
        DBG(printk("status        : %x\n", csme_sta_table_for_hw[i].status));
        DBG(printk("egress_port_id: 0x%2.2x\n", csme_sta_table_for_hw[i].egress_port_id));
        DBG(printk("==================================\n"));
    }

    return;
}/* cs_wfo_csme_dump_mac_entry() */


/*
 *   Global Functions
 */
extern void __cs_mc_delete_hash_by_ipv4_group(__be32 origin, __be32 mcastgrp);
extern void cs_mc_groupip_lookup_table_reset(u32 *ip_addr, bool is_v6);

/* Parameter    unsigned char *pgrp_addr        Group address
 *              unsigned char *pmember_addr     MAC DA
 *              unsigned char *p_egress_port_id Egress Port ID
 * Description	This API will add a member (MAC DA) into 
 *              the specify group and assign a MCAL table bit
 *              (egress port id)to replicate packets.
 * Return value cs_wfo_csme_status_e
 */
cs_wfo_csme_status_e cs_wfo_csme_group_add_member(
            unsigned char *pgrp_addr, 
            unsigned char *pmember_addr,
            unsigned char *p_egress_port_id,
            __be32 group_ip,
            unsigned char ad_id )       //BUG#39828
{
    cs_wfo_csme_status_e retStatus = CS_WFO_CSME_STATUS_FAIL;
    cs_wfo_csme_list_entry_status_e retListStatus;


    DBG(printk("### %s:: pgrp_addr %pM, pmember_addr %pM, ad_id %d\n", 
                __func__, pgrp_addr, pmember_addr, ad_id));

	mutex_lock(&csme_list_mutex);
    retListStatus = cs_wfo_csme_add_mac_da_grp_list( pgrp_addr, 
                                                     pmember_addr, 
                                                     p_egress_port_id);
                                                     
    switch (retListStatus) {
        case CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS:
            retListStatus = cs_wfo_csme_add_grp_member_list( pgrp_addr, 
                                                             pmember_addr,
                                                             group_ip,
                                                             p_egress_port_id,
                                                             ad_id );     //BUG#39828
            
            if (retListStatus == CS_WFO_CSME_LIST_ENTRY_STATUS_SUCCESS) {
                retStatus = CS_WFO_CSME_STATUS_SUCCESS;
            } else {
                // delete pmember_addr from pgrp_addr if add fail
                cs_wfo_csme_del_group_member(pgrp_addr, pmember_addr);
            }
            break;

        case CS_WFO_CSME_LIST_ENTRY_STATUS_EXIST:
            retStatus = CS_WFO_CSME_STATUS_SUCCESS;

        case CS_WFO_CSME_LIST_ENTRY_STATUS_FAIL:
        case CS_WFO_CSME_LIST_ENTRY_STATUS_FULL:
           retStatus = CS_WFO_CSME_STATUS_FAIL;
            break;
    }/* switch */
	mutex_unlock(&csme_list_mutex);
 
    return retStatus;
}/* cs_wfo_csme_group_add_member() */
EXPORT_SYMBOL(cs_wfo_csme_group_add_member);


/* Parameter    unsigned char 		    *pgrp_addr		    Group address
 *              cs_wfo_csme_sta_list_s 	*pcsme_send_list	MAC DA List
 * Description	This API will get all members (MAC DA) information   
 *              from the specify group.
 * Return value cs_wfo_csme_status_e
 */
cs_wfo_csme_status_e cs_wfo_csme_group_get_member(
            unsigned char *pgrp_addr, 
            unsigned char ad_id,    //BUG#39828
            cs_wfo_csme_sta_list_s *pcsme_send_list)
{
	struct list_head            *nextGrp, *nextGrpTmp;
	struct list_head	        *nextGrpMember, *nextGrpMemberTmp;
	struct csme_grp_list        *pGrp;
	struct csme_grp_member_list *pGrpMember;
	cs_wfo_csme_status_e retStatus = CS_WFO_CSME_STATUS_FAIL;
    unsigned char has_sw_count = false;


    pcsme_send_list->hw_count = 0;
    pcsme_send_list->sw_count = 0;
    pcsme_send_list->total_count = 0;
    
    if (!(cs_wfo_csme & CS752X_WFO_CSME_ENABLE)){
        return retStatus;
    }

//    DBG(printk("### %s:: pgrp_addr %pM\n", __func__, pgrp_addr));

	mutex_lock(&csme_list_mutex);
    /* 
     * lookup pgrp_addr in csme_grp_list and then 
     * copy pmember_addr to pcsme_send_list
     */
    if (!list_empty(&grp_head)) {
        /* lookup pgrp_addr in csme_grp_list */
        list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) {
            pGrp = (struct csme_grp_list*) 
                        list_entry(nextGrp, struct csme_grp_list, grp_list);

            if (memcmp(pGrp->group_addr, pgrp_addr, ETH_ALEN) == 0) {
                if (!list_empty(&(pGrp->grp_member_head))) {
                    /* lookup pmember_addr in csme_grp_member_list */
                    list_for_each_safe( nextGrpMember, 
                                        nextGrpMemberTmp, 
                                        &(pGrp->grp_member_head) ) {
                        pGrpMember = (struct csme_grp_member_list*) list_entry( 
                                                           nextGrpMember, 
                                                           struct csme_grp_member_list, 
                                                           grp_member_list);
                        if (pGrpMember->ad_id == ad_id) {       //BUG#39828
                        pcsme_send_list->sta_list[pcsme_send_list->total_count].egress_port_id
                                   = pGrpMember->egress_port_id;
                        pcsme_send_list->sta_list[pcsme_send_list->total_count].status
                                   = CS_WFO_CSME_MAC_ENTRY_VALID;
                        memcpy( pcsme_send_list->sta_list[pcsme_send_list->total_count].mac_da,
                                pGrpMember->grp_member_addr,
                                ETH_ALEN );
                        if (pGrpMember->egress_port_id == 0xFF) {
                            pcsme_send_list->sw_count++;
                            has_sw_count = true;
                        } else { 
                            pcsme_send_list->hw_count++;
                        }
                        pcsme_send_list->total_count++;
                        }//BUG#39828
                    }/* list_for_each_safe(nextGrpMember) */
                }/* if (!list_empty(&(pGrp->grp_member_head)) */
            }/* if (memcmp( pGrp->group_addr, pgrp_addr, ETH_ALEN) == 0) */
        }/* list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) */
    }/* if (!list_empty(&grp_head)) */
	mutex_unlock(&csme_list_mutex);
    
    if ( (pcsme_send_list->total_count) &&
         (has_sw_count == false) ) {
        retStatus = CS_WFO_CSME_STATUS_SUCCESS;
    }
    
    return retStatus;
}/* cs_wfo_csme_group_get_member() */
EXPORT_SYMBOL(cs_wfo_csme_group_get_member);


/* Parameter    unsigned char *pgrp_addr    Group address
 *              unsigned char *pmember_addr MAC DA
 * Description  This API will delete a member (MAC DA) from 
 *              the specify group and remove MCAL table bit
 *              (egress port id) mapping.
 * Return value cs_wfo_csme_status_e
 */
cs_wfo_csme_status_e cs_wfo_csme_del_group_member(
            unsigned char *pgrp_addr, 
            unsigned char *pmember_addr )
{
	struct list_head            *nextGrp, *nextGrpTmp;
	struct list_head	        *nextGrpMember, *nextGrpMemberTmp;
	struct csme_grp_list        *pGrp;
	struct csme_grp_member_list *pGrpMember;

	struct list_head            *nextMACDA, *nextMACDATmp;
	struct list_head            *nextMACDAGrp, *nextMACDAGrpTmp;
	struct csme_mac_da_list     *pMACDA;
	struct csme_mac_da_grp_list *pMACDAGrp;
	
	cs_wfo_csme_status_e retStatus = CS_WFO_CSME_STATUS_FAIL;
	unsigned char               index;
	
    DBG(printk("### %s:: pgrp_addr %pM, pmember_addr %pM\n", 
                __func__, pgrp_addr, pmember_addr));

	mutex_lock(&csme_list_mutex);
    /* 
     * lookup pgrp_addr in csme_grp_list and then 
     * delete pmember_addr from csme_grp_member_list
     */
    if (!list_empty(&grp_head)) {
        /* lookup pgrp_addr in csme_grp_list */
        list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) {
            pGrp = (struct csme_grp_list*) 
                        list_entry(nextGrp, struct csme_grp_list, grp_list);

            if (memcmp(pGrp->group_addr, pgrp_addr, ETH_ALEN) == 0) {
                if (!list_empty(&(pGrp->grp_member_head))) {
                    /* lookup pmember_addr in csme_grp_member_list */
                    list_for_each_safe( nextGrpMember, 
                                        nextGrpMemberTmp, 
                                        &(pGrp->grp_member_head) ) {
                        pGrpMember = (struct csme_grp_member_list*) list_entry( 
                                                           nextGrpMember, 
                                                           struct csme_grp_member_list, 
                                                           grp_member_list);

                        if (memcmp( pGrpMember->grp_member_addr, 
                                    pmember_addr, 
                                    ETH_ALEN ) == 0) {
                            // delete node from grp_member_list list and free resource
                            list_del(&(pGrpMember->grp_member_list));
                            cs_free(pGrpMember);

            	            // delete hash
            	            __cs_mc_delete_hash_by_ipv4_group(0, pGrp->group_ip);
            	            cs_mc_groupip_lookup_table_reset(&(pGrp->group_ip), false);

                            retStatus = CS_WFO_CSME_STATUS_SUCCESS;
                            break;
                        }/* if (memcmp()) */
                    }/* list_for_each_safe(nextGrpMember) */
                }/* if (!list_empty(&(pGrp->grp_member_head)) */

                // check it again
                // delete the node from grp list if it's empty.
                if (list_empty(&(pGrp->grp_member_head))) {
                    // delete list and free resource
                    list_del(&(pGrp->grp_list));
                    cs_free(pGrp);
                    break;
                }
            }/* if (memcmp( pGrp->group_addr, pgrp_addr, ETH_ALEN) == 0) */
        }/* list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) */
    }/* if (!list_empty(&grp_head)) */
    

    /* 
     * lookup pmember_addr in csme_mac_da_list and then 
     * delete pgrp_addr from csme_mac_da_grp_list
     */
    if (!list_empty(&mac_da_head)) {
        /* lookup pmember_addr in csme_mac_da_list */
        list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) {
            pMACDA = (struct csme_mac_da_list*) list_entry( 
                                                nextMACDA, 
                                                struct csme_mac_da_list, 
                                                mac_da_list );
            if (memcmp(pMACDA->mac_da, pmember_addr, ETH_ALEN) == 0) {
                if (!list_empty(&(pMACDA->mac_da_grp_head))) {
                    /* lookup pgrp_addr in csme_mac_da_grp_list */
                    list_for_each_safe( nextMACDAGrp, 
                                        nextMACDAGrpTmp, 
                                        &(pMACDA->mac_da_grp_head) ) {
                        pMACDAGrp = (struct csme_mac_da_grp_list*) list_entry( 
                                                    nextMACDAGrp, 
                                                    struct csme_mac_da_grp_list, 
                                                    mac_da_grp_list);
                        if (memcmp( pMACDAGrp->group_addr, 
                                    pgrp_addr, 
                                    ETH_ALEN ) == 0) {
                            // delete list and free resource
                            list_del(&(pMACDAGrp->mac_da_grp_list));
                            cs_free(pMACDAGrp);
                            retStatus = CS_WFO_CSME_STATUS_SUCCESS;
                            break;
                        }/* if (memcmp) */
                    }/* list_for_each_safe(nextMACDAGrp) */
                }/* if (!list_empty(&(pMACDA->mac_da_list))) */
                
                if (list_empty(&(pMACDA->mac_da_grp_head))) {
                    // Clean egress port id mapping
                    if (pMACDA->egress_port_id != 0xFF) {
                        index = pMACDA->egress_port_id - 
                                    CS_WFO_CSME_EGRESS_PORT_STA1;
                        if (memcmp( csme_sta_table_for_hw[index].mac_da, 
                                    pmember_addr, 
                                    ETH_ALEN ) == 0) {
                            memset( csme_sta_table_for_hw[index].mac_da, 
                                    0, 
                                    ETH_ALEN);
                            csme_sta_table_for_hw[index].status = 
                                    CS_WFO_CSME_MAC_ENTRY_INVALID;
                        }/*if (memcmp()) */
                    }/* if (pMACDA->egress_port_id != 0xFF) */

                    // delete list and free resource
                    list_del(&(pMACDA->mac_da_list));
                    cs_free(pMACDA);
                }
            }/* if (memcmp(pMACDA->mac_da, pmember_addr, ETH_ALEN) == 0) */
        }/* list_for_each_safe(nextMACDATmp, nextMACDAGrpTmp, &grp_member_head) */
    }/* if (!list_empty(&mac_da_grp_head)) */
	mutex_unlock(&csme_list_mutex);
    
        
    return retStatus;
}/* cs_wfo_csme_del_group_member() */
EXPORT_SYMBOL(cs_wfo_csme_del_group_member);


/* Parameter    unsigned char *pmember_addr	MAC DA
 * Description  This API will delete the specify member 
 *              (MAC DA) from all groups and remove MCAL
 *              table bit (egress port id) mapping
 * Return value cs_wfo_csme_status_e
 */
cs_wfo_csme_status_e cs_wfo_csme_del_member(unsigned char *pmember_addr)
{
	struct list_head            *nextMACDA, *nextMACDATmp;
	struct list_head            *nextMACDAGrp, *nextMACDAGrpTmp;
	struct csme_mac_da_list     *pMACDA;
	struct csme_mac_da_grp_list *pMACDAGrp;

	struct list_head            *nextGrp, *nextGrpTmp;
	struct list_head	        *nextGrpMember, *nextGrpMemberTmp;
	struct csme_grp_list        *pGrp;
	struct csme_grp_member_list *pGrpMember;

	cs_wfo_csme_status_e retStatus = CS_WFO_CSME_STATUS_FAIL;
	unsigned char               index;
	
	mutex_lock(&csme_list_mutex);
    /* Delete MAC DA for key list */
    if (!list_empty(&mac_da_head)) {
        /* Traverse all elements of the csme_mac_da_list list */
        list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) {
            pMACDA = (struct csme_mac_da_list*) list_entry( 
                                                nextMACDA, 
                                                struct csme_mac_da_list, 
                                                mac_da_list );
            if (memcmp(pMACDA->mac_da, pmember_addr, ETH_ALEN) == 0) {
                if (!list_empty(&(pMACDA->mac_da_grp_head))) {
                    /* Traverse all elements of the csme_mac_da_grp_list list */
                    list_for_each_safe( nextMACDAGrp, 
                                        nextMACDAGrpTmp, 
                                        &(pMACDA->mac_da_grp_head) ) {
                        pMACDAGrp = (struct csme_mac_da_grp_list*) list_entry( 
                                                    nextMACDAGrp, 
                                                    struct csme_mac_da_grp_list, 
                                                    mac_da_grp_list);

                        // lookup pMACDAGrp->group_addr in grp_member_head list
                        if (!list_empty(&grp_head)) {
                            list_for_each_safe( nextGrp, 
                                                nextGrpTmp, 
                                                &grp_head) {
                                pGrp = (struct csme_grp_list*) list_entry( 
                                                    nextGrp, 
                                                    struct csme_grp_list, 
                                                    grp_list );
                                if (memcmp( pGrp->group_addr, 
                                            pMACDAGrp->group_addr, 
                                            ETH_ALEN ) == 0) {
                                    if (!list_empty(&(pGrp->grp_member_head))) {
                                        list_for_each_safe( nextGrpMember, 
                                                            nextGrpMemberTmp, 
                                                            &(pGrp->grp_member_head)) {
                                            pGrpMember = (struct csme_grp_member_list*) 
                                                            list_entry( nextGrpMember, 
                                                                        struct csme_grp_member_list, 
                                                                        grp_member_list );
                                            if (memcmp( pGrpMember->grp_member_addr, 
                                                        pmember_addr, 
                                                        ETH_ALEN ) == 0) {

                                	            // delete hash
                                	            __cs_mc_delete_hash_by_ipv4_group(0, pGrp->group_ip);
                                	            cs_mc_groupip_lookup_table_reset(&(pGrp->group_ip), false);

                                                // delete list and free resource
                                                list_del(&(pGrpMember->grp_member_list));
                                                cs_free(pGrpMember);

                                                // delete list and free resource
                                                list_del(&(pMACDAGrp->mac_da_grp_list));
                                                cs_free(pMACDAGrp);

                                                retStatus = CS_WFO_CSME_STATUS_SUCCESS;
                                                break;
                                            }
                                        }/* list_for_each_safe */
                                    }/* if (!list_empty(&(pGrp->grp_member_head))) */

                                    if (list_empty(&(pGrp->grp_member_head))) {
                                        // Clean egress port id mapping
                                        if (pMACDA->egress_port_id != 0xFF) {
                                            index = pMACDA->egress_port_id - 
                                                        CS_WFO_CSME_EGRESS_PORT_STA1;
                                            if (memcmp( csme_sta_table_for_hw[index].mac_da, 
                                                        pmember_addr, 
                                                        ETH_ALEN ) == 0) {
                                                memset( csme_sta_table_for_hw[index].mac_da, 
                                                        0, 
                                                        ETH_ALEN);
                                                csme_sta_table_for_hw[index].status = 
                                                        CS_WFO_CSME_MAC_ENTRY_INVALID;
                                            }/*if (memcmp()) */
                                        }/* if (pMACDA->egress_port_id != 0xFF) */

                                        // delete list and free resource
                                        list_del(&(pGrp->grp_list));
                                        cs_free(pGrp);
                                    }
                                }/*if (memcmp()) */
                            }/* list_for_each_safe */
                        }/* if (!list_empty(&grp_head)) */
                    } /* list_for_each_safe */
                }/* if (!list_empty(&(pMACDA->mac_da_list))) */

                if (list_empty(&(pMACDA->mac_da_grp_head))) {
                    // delete list and free resource
                    list_del(&(pMACDA->mac_da_list));
                    cs_free(pMACDA);
                }
            }/* if (memcmp(pMACDA->mac_da, pMACDA->mac_da, ETH_ALEN) == 0) */
        }/* list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) */
    }/* if (!list_empty(&mac_da_head)) */
	mutex_unlock(&csme_list_mutex);


    return retStatus;
}/* cs_wfo_csme_del_member() */
EXPORT_SYMBOL(cs_wfo_csme_del_member);


/* Parameter    void
 * Description  This API will delete the all the members and groups and clear all the 
 *              Station mapping in MCAL table 
 * Return value void
 */
void cs_wfo_csme_del_all_group(void)
{
	struct list_head            *nextGrp, *nextGrpMember;
	struct list_head	        *nextGrpTmp, *nextGrpMemberTmp;
	struct csme_grp_list        *pGrp;
	struct csme_grp_member_list *pGrpMember;

	struct list_head            *nextMACDA, *nextMACDAGrp;
	struct list_head            *nextMACDATmp, *nextMACDAGrpTmp;
	struct csme_mac_da_list     *pMACDA;
	struct csme_mac_da_grp_list *pMACDAGrp;
    int i;
	
	
    DBG(printk("### %s\n", __func__));

	mutex_lock(&csme_list_mutex);
    /* Delete Group for key list */
    if (!list_empty(&grp_head)) {
        /* Traverse all elements of the csme_grp_list list */
        list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) {
            pGrp = (struct csme_grp_list*) list_entry( nextGrp, 
                                                       struct csme_grp_list, 
                                                       grp_list );
            if (!list_empty(&(pGrp->grp_member_head))) {
                /* Traverse all elements of the csme_grp_member_list list */
                list_for_each_safe( nextGrpMember, 
                                    nextGrpMemberTmp, 
                                    &(pGrp->grp_member_head) ) {
                    pGrpMember = (struct csme_grp_member_list*) list_entry( 
                                           nextGrpMember, 
                                           struct csme_grp_member_list, 
                                           grp_member_list);
                    // delete list and free resource
                    list_del(&(pGrpMember->grp_member_list));
                    cs_free(pGrpMember);
                }/* list_for_each_safe(nextGrpMember) */
            }/* if (!list_empty(&(pGrp->grp_list)) */
            
            // delete hash
            __cs_mc_delete_hash_by_ipv4_group(0, pGrp->group_ip);
            cs_mc_groupip_lookup_table_reset(&(pGrp->group_ip), false);

            // delete list and free resource
            list_del(&(pGrp->grp_list));
            cs_free(pGrp);
        }/* list_for_each_safe(nextGrp, nextGrpTmp, &grp_head) */
    }/* if (!list_empty(&mac_da_head)) */
    
	
    /* Delete MAC DA for key list */
    if (!list_empty(&mac_da_head)) {
        /* Traverse all elements of the csme_mac_da_list list */
        list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) {
            pMACDA = (struct csme_mac_da_list*) list_entry( 
                                    nextMACDA, 
                                    struct csme_mac_da_list, 
                                    mac_da_list );
            if (!list_empty(&(pMACDA->mac_da_grp_head))) {
                /* Traverse all elements of the csme_mac_da_grp_list list */
                list_for_each_safe( nextMACDAGrp, 
                                    nextMACDAGrpTmp, 
                                    &(pMACDA->mac_da_grp_head) ) {
                    pMACDAGrp = (struct csme_mac_da_grp_list*) list_entry( 
                                            nextMACDAGrp, 
                                            struct csme_mac_da_grp_list, 
                                            mac_da_grp_list);

                    // delete list and free resource
                    list_del(&(pMACDAGrp->mac_da_grp_list));
                    cs_free(pMACDAGrp);
                }/* list_for_each_safe(nextMACDAGrp, nextMACDAGrpTmp, &(pMACDA->mac_da_list)) */
            }/* if (!list_empty(&(pMACDA->mac_da_list))) */
            
            // delete list and free resource
            list_del(&(pMACDA->mac_da_list));
            cs_free(pMACDA);
        }/* list_for_each_safe(nextMACDATmp, nextMACDAGrpTmp, &grp_member_head) */
    }/* if (!list_empty(&mac_da_grp_head)) */
    
        
    for (i=0; i<CS_WFO_CSME_STA_ENTRY_SIZE; i++) {
        memset(csme_sta_table_for_hw[i].mac_da, 0, sizeof(csme_sta_table_for_hw[i].mac_da));
		csme_sta_table_for_hw[i].status = CS_WFO_CSME_MAC_ENTRY_INVALID;
		if (i >= CS_WFO_CSME_STA_SUPPPORT_MAX) {
		    csme_sta_table_for_hw[i].egress_port_id = 0xFF;
		} else {
		    csme_sta_table_for_hw[i].egress_port_id = i + CS_WFO_CSME_EGRESS_PORT_STA1;
		}
    }
	mutex_unlock(&csme_list_mutex);

    return;    
}/* cs_wfo_csme_del_all_group() */
EXPORT_SYMBOL(cs_wfo_csme_del_all_group);


/* Parameter	unsigned char *pmember_addr	MAC DA
 * Description  This API will query the egress port id  
 *              of a member (MAC DA) 
 * Return value unsigned char			Egress Port ID
 */
unsigned char cs_wfo_csme_query_egress_port_id(unsigned char *pmember_addr)
{
	struct list_head            *nextMACDA, *nextMACDATmp;
	struct csme_mac_da_list     *pMACDA;

    DBG(printk("%s:: pmember_addr %pM\n", __func__, pmember_addr));

    /* Add MAC DA for key list */
    /* Try to find pgrp_addr in MAC DA for key list */
    if (!list_empty(&mac_da_head)) {
        /* Traverse all elements of the csme_mac_da_list list */
        list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) {
            pMACDA = (struct csme_mac_da_list*) list_entry( 
                                                nextMACDA, 
                                                struct csme_mac_da_list, 
                                                mac_da_list );
            
            // Find pmember_addr in csme_mac_da_list list
            if (memcmp( pMACDA->mac_da, 
                        pmember_addr, 
                        ETH_ALEN ) == 0) {
                DBG(printk("%s:: egress_port_id 0x%x\n", __func__, pMACDA->egress_port_id));
                return pMACDA->egress_port_id;
            }/* if (memcmp()) */
        }/* list_for_each_safe(nextMACDA, nextMACDATmp, &mac_da_head) */
    }/* if (!list_empty(&mac_da_head)) */


    return 0xFF;
}/* cs_wfo_csme_query_egress_port_id() */
EXPORT_SYMBOL(cs_wfo_csme_query_egress_port_id);


/* Parameter    void
 * Description  Initialize all the lists for CSME
 * Return value NONE
 */
void cs_wfo_csme_init(void)
{
    int i;

	/* init csme list -  Group for key */
	INIT_LIST_HEAD(&grp_head);

	/* init csme list -  MAC DA for key */
	INIT_LIST_HEAD(&mac_da_head);


    for (i=0; i<CS_WFO_CSME_STA_ENTRY_SIZE; i++) {
        memset(csme_sta_table_for_hw[i].mac_da, 0, sizeof(csme_sta_table_for_hw[i].mac_da));
		csme_sta_table_for_hw[i].status = CS_WFO_CSME_MAC_ENTRY_INVALID;
		if (i >= CS_WFO_CSME_STA_SUPPPORT_MAX) {
		    csme_sta_table_for_hw[i].egress_port_id = 0xFF;
		} else {
		    csme_sta_table_for_hw[i].egress_port_id = i + CS_WFO_CSME_EGRESS_PORT_STA1;
		}
    }
    

    return;
}/* cs_wfo_csme_init() */
EXPORT_SYMBOL(cs_wfo_csme_init);




int cs_wfo_csme_SnoopInspecting(struct sk_buff *skb, unsigned char ad_id)   //BUG#39828
{
	struct ethhdr *eh;
	u8 egress_port_id;
    cs_wfo_csme_status_e retStatus = CS_WFO_CSME_STATUS_FAIL;
    
    
//	int i;
//	printk("\n== Dump Pkt ============================================\n");
//	for (i=0; i<64; i++) {
//	    printk("%2.2x ", *(skb->data+i));
//	    if ((i!=0) && ((i+1)%16 == 0))
//	         printk("\n");
//	}
//	printk("========================================================\n");

    eh = (struct ethhdr *)(skb->data);
    if (ntohs(eh->h_proto) == ETH_P_IP) {
        const struct iphdr *ip = (struct iphdr *)(skb->data + sizeof(struct ethhdr));
        if (ip->protocol == IPPROTO_IGMP) {
            const struct igmphdr *igmp;     /* igmp header for v1 and v2*/
            u_int8_t groupAddrL2[ETH_ALEN]; /*group multicast mac address*/
            u_int32_t groupIPAddr = 0;        /* to hold group address from group record*/

            /* v1 & v2 */    
            igmp = (struct igmphdr *) (skb->data + sizeof(struct ethhdr) + (4 * ip->ihl));

            /* If packet is not IGMP report or IGMP leave don't handle it*/
            if(!IS_IGMPV2_REPORT_LEAVE_PACKET(igmp->type)){
                /* IGMPv3 support*/
                u_int32_t *src_ip_addr;
                const struct igmpv3_report *igmpv3;            /* igmp header for v3*/
                const struct igmpv3_grec   *grec;              /* igmp group record*/
                u_int16_t no_grec; /* no of group records  */
                u_int16_t no_srec; /* no of source records */

	            DBG(printk("### %s:: NOTE!!!  IGMPV3\n", __func__));

                if (cs_wfo_csme & CS752X_WFO_CSME_IGMPV3){
                    if(!IS_IGMPV3_REPORT_LEAVE_PACKET(igmp->type)) {
                        // CS752X_WFO_CSME_IGMPV3 enable, NOT IGMPv2 and NOT IGMPv3
                        return -1;
                    }
                    // parser IGMPv3 packet here
                    igmpv3 = (struct igmpv3_report *) igmp;
                    /*  V3 report handling */
                    no_grec = igmpv3->ngrec;
                    grec = (struct igmpv3_grec*)((u_int8_t*)igmpv3 + 8);
                    while(no_grec){
                        groupIPAddr = grec->grec_mca;
                        no_srec = grec->grec_nsrcs;
                        src_ip_addr = (u_int32_t*)((u_int8_t*)grec + sizeof(struct igmpv3_grec));
                        groupAddrL2[3] = (groupIPAddr >>  8) & 0x7f;
                        groupAddrL2[4] = (groupIPAddr >> 16) & 0xff;
                        groupAddrL2[5] = (groupIPAddr >> 24) & 0xff;
                        
                        // FIXME:
                        switch (grec->grec_type) {
                            case IGMPV3_CHANGE_TO_EXCLUDE:
                            case IGMPV3_MODE_IS_EXCLUDE:
                                /* remove old member entries as new member entries are received */
                                
                                /* if no source record is there then it is include for all source */
                                
                                break;

                            case IGMPV3_CHANGE_TO_INCLUDE:
                            case IGMPV3_MODE_IS_INCLUDE:
                                /* Add new member entries are received */
                                break;
                            
                            case IGMPV3_ALLOW_NEW_SOURCES:
                                /* Add new member entries are received */
                                break;
                        }/* switch (grec->grec_type) */
                        
                        while(no_srec){
                            // FIXME:
                            /* Source record handling*/
                            src_ip_addr++;
                            no_srec--;
                        } /* while of no_srec*/
                        /* move the grec to next group record*/
                        grec = (struct igmpv3_grec*)((u_int8_t*)grec + IGMPV3_GRP_REC_LEN(grec));
                        no_grec--;
                    } /*while of no_grec*/
                    
                    return 0;
                } else {
                    // Not IGMPv2 and CS752X_WFO_CSME_IGMPV3 disable
                    return -1;
                }
            }

            /* IGMPv2 support*/
            groupIPAddr = igmp->group;
            /* Init multicast group address conversion */
            groupAddrL2[0] = 0x01;
            groupAddrL2[1] = 0x00;
            groupAddrL2[2] = 0x5e;
            groupAddrL2[3] = (groupIPAddr >>  8) & 0x7f;
            groupAddrL2[4] = (groupIPAddr >> 16) & 0xff;
            groupAddrL2[5] = (groupIPAddr >> 24) & 0xff;

            if(igmp->type == IGMP_HOST_LEAVE_MESSAGE){
                // IGMP Leave
	            DBG(printk("### %s:: IGMP Leave MAC %pM, IP %pI4, Source MAC %pM\n", 
	                        __func__, groupAddrL2, &groupIPAddr, eh->h_source));
	            cs_wfo_csme_del_group_member(groupAddrL2, eh->h_source);
                cs_wfo_csme_dump_table();
                cs_wfo_csme_dump_mac_entry();
            } else {
                // IGMP Join
	            DBG(printk("### %s:: IGMP Join  MAC %pM, IP %pI4, Source MAC %pM, ad_id %d\n", 
	                        __func__, groupAddrL2, &groupIPAddr, eh->h_source, ad_id));

	            // Add group and member into list and allocate egress port id for MCAL
                retStatus = cs_wfo_csme_group_add_member( groupAddrL2, 
                                                          eh->h_source, 
                                                          &egress_port_id,
                                                          groupIPAddr,
                                                          ad_id );      //BUG#39828
//                printk("### %s:%d:: retStatus 0x%x, ad_id %d\n", __func__, __LINE__, retStatus, ad_id);
                cs_wfo_csme_dump_table();
                cs_wfo_csme_dump_mac_entry();

	            // delete hash
	            __cs_mc_delete_hash_by_ipv4_group(0, groupIPAddr);
	            cs_mc_groupip_lookup_table_reset(&groupIPAddr, false);

            } /* if(igmp->type == IGMP_HOST_LEAVE_MESSAGE) */
        } /* if (ip->protocol == IPPROTO_IGMP) */
    } /* if (ntohs(eh->h_proto) == ETH_P_IP) */
   
    return 0;
}/* cs_wfo_csme_SnoopInspecting() */
EXPORT_SYMBOL(cs_wfo_csme_SnoopInspecting);
