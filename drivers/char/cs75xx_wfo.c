#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <asm/cputime.h>
#include <mach/cs75xx_pni.h>

#include <mach/cs75xx_ipc_wfo.h>
#include <cs_core_logic.h>

#define WFO_MAC_ENTRY_SIZE    16
#define WFO_MAC_ENTRY_TIMER_PERIOD    300

//A9 local 802.3 MAC DA status table
static struct wfo_mac_table_entry mac_table_802_3[WFO_MAC_ENTRY_SIZE];
static spinlock_t table_802_3_lock;


//A9 local 802.11 MAC DA status table
static struct wfo_mac_table_entry mac_table_802_11[WFO_MAC_ENTRY_SIZE];
static spinlock_t table_802_11_lock;

//table entry aging timer
static struct timer_list wfo_mac_timer;
static wfo_mac_entry_status_e cs_wfo_get_80211_mac_entry(unsigned char *mac_da,
    unsigned char *p_index, unsigned char txwi[], unsigned char frame_type, unsigned char pe_id);
static wfo_mac_entry_status_e cs_wfo_get_8023_mac_entry(unsigned char *mac_da, unsigned char *p_index, unsigned char pe_id);

void cs_wfo_mac_table_timer_func(unsigned long data);

void cs_wfo_mac_table_timer_func(unsigned long data)
{
    int i;
    int resched = 0;

    //age 802.3 MACs
    spin_lock(&table_802_3_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        if(mac_table_802_3[i].status == WFO_MAC_ENTRY_VALID) {
            if(mac_table_802_3[i].timeout++ > WFO_MAX_MAC_AGING_COUNT)
               memset(&mac_table_802_3[i], 0, sizeof(struct wfo_mac_table_entry));
            else
                resched = 1;
        }
    }
    spin_unlock(&table_802_3_lock);

    //age 802.11 MACs
    spin_lock(&table_802_11_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        if(mac_table_802_11[i].status == WFO_MAC_ENTRY_VALID) {
            if(mac_table_802_11[i].timeout++ > WFO_MAX_MAC_AGING_COUNT)
                memset(&mac_table_802_11[i], 0, sizeof(struct wfo_mac_table_entry));
            else
               resched = 1;
        }
    }
    spin_unlock(&table_802_11_lock);

#if 0
    if (resched)
        mod_timer(&wfo_mac_timer, jiffies + secs_to_cputime(WFO_MAC_ENTRY_TIMER_PERIOD));
#endif

    return;
}


/*
 * check both MAC tables, specific check function also defined
 * called when A9 send frame to PE
 * called before A9 to PE frame send
 * if entry not found: send IPCs to PE, add NE hash then add local table entry
 */
static wfo_mac_entry_status_e cs_wfo_get_8023_mac_entry(unsigned char *mac_da, unsigned char *p_index, unsigned char pe_id)
{
    int i;
    unsigned char entry_pe_id = 0;
    wfo_mac_entry_status_e rc = WFO_MAC_ENTRY_VALID;

    if(unlikely(!mac_da || !p_index))
        return WFO_MAC_ENTRY_INVALID;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID))
        return WFO_MAC_ENTRY_INVALID;

    *p_index = -1;
    spin_lock(&table_802_3_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        if(!memcmp(mac_da, mac_table_802_3[i].mac_da, sizeof(mac_table_802_3[i].mac_da)))
        {
            if(WFO_MAC_ENTRY_INVALID == mac_table_802_3[i].status) {
                continue;
            }

            if (!mac_table_802_3[i].to_pe0 && !mac_table_802_3[i].to_pe1) {
                printk("%s:%d inv entry no PE\n", __func__, __LINE__);
                cs_wfo_dump_mac(mac_table_802_3[i].mac_da);
                //mac_table_802_3[i].status = WFO_MAC_ENTRY_INVALID;
                continue;
            }

            if(pe_id == CS_WFO_IPC_PE0_CLNT_ID) {
                if (mac_table_802_3[i].to_pe0 != TRUE) {
                    //mac_table_802_3[i].to_pe0 = TRUE;
                    rc = WFO_MAC_ENTRY_NEWPE;
                }
                *p_index = i;
                spin_unlock(&table_802_3_lock);
                return rc;
            } else if(pe_id == CS_WFO_IPC_PE1_CLNT_ID) {
                if (mac_table_802_3[i].to_pe1 != TRUE) {
                    //mac_table_802_3[i].to_pe1 = TRUE;
                    rc = WFO_MAC_ENTRY_NEWPE;
                }
                *p_index = i;
                spin_unlock(&table_802_3_lock);
                return rc;
            }
        }
    }
    spin_unlock(&table_802_3_lock);
    return WFO_MAC_ENTRY_INVALID;
}


/*
 * check both MAC tables, specific check function also defined
 * called when A9 send frame to PE
 * called before A9 to PE frame send
 * if entry not found: send IPCs to PE, add NE hash then add local table entry
 */
static wfo_mac_entry_status_e cs_wfo_get_80211_mac_entry(unsigned char *mac_da,
    unsigned char *p_index, unsigned char txwi[], unsigned char frame_type, unsigned char pe_id)
{
    int i;
    int txwi_match = true;
    wfo_mac_entry_status_e rc = WFO_MAC_ENTRY_VALID;

    if(unlikely(!mac_da || !p_index))
        return WFO_MAC_ENTRY_INVALID;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID))
        return WFO_MAC_ENTRY_INVALID;

    *p_index = -1;
    spin_lock(&table_802_11_lock);

    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {

        if(!memcmp(mac_da, mac_table_802_11[i].mac_da, sizeof(mac_table_802_3[i].mac_da))) {


            if(WFO_MAC_ENTRY_INVALID == mac_table_802_11[i].status) {
                continue;
            }

            if(!mac_table_802_11[i].to_pe1 && !mac_table_802_11[i].to_pe0) {
                printk("%s:%d inv entry no PE\n", __func__, __LINE__);
                cs_wfo_dump_mac(mac_table_802_11[i].mac_da);
                //mac_table_802_11[i].status = WFO_MAC_ENTRY_INVALID;
                continue;
            }

            if (pe_id == CS_WFO_IPC_PE0_CLNT_ID ) {
                if (mac_table_802_11[i].to_pe0 != TRUE) {
                    //mac_table_802_11[i].to_pe0 = TRUE;
                    rc = WFO_MAC_ENTRY_NEWPE;
                }
				*p_index = i;
                spin_unlock(&table_802_11_lock);
                return rc;
            } else if (pe_id == CS_WFO_IPC_PE1_CLNT_ID) {
                if (mac_table_802_11[i].to_pe1 != TRUE) {
                    //mac_table_802_11[i].to_pe1 = TRUE;
                    rc = WFO_MAC_ENTRY_NEWPE;
                }
				*p_index = i;
                spin_unlock(&table_802_11_lock);
                return rc;
            }


            //ralink wifi driver frame type (1,2,4)
            switch(frame_type) {
            case 1:
                txwi_match = memcmp(txwi, mac_table_802_11[i].t1_txwi, sizeof(mac_table_802_11[i].t1_txwi));

                //MAC DA match, TXWI doesn't, update txwi
                //memcpy(mac_table_802_11[i].t1_txwi, txwi, sizeof(mac_table_802_11[i].t1_txwi));
                break;
            case 2:
                txwi_match = memcmp(txwi, mac_table_802_11[i].t2_txwi, sizeof(mac_table_802_11[i].t2_txwi));

                //MAC DA match, TXWI doesn't, update txwi
                //memcpy(mac_table_802_11[i].t2_txwi, txwi, sizeof(mac_table_802_11[i].t2_txwi));
                break;
            default:
                txwi_match = memcmp(txwi, mac_table_802_11[i].t4_txwi, sizeof(mac_table_802_11[i].t4_txwi));

                //MAC DA match, TXWI doesn't, update txwi
                //memcpy(mac_table_802_11[i].t4_txwi, txwi, sizeof(mac_table_802_11[i].t4_txwi));
                break;
            }

            if(!txwi_match) {
                *p_index = i;
                spin_unlock(&table_802_11_lock);
                return WFO_MAC_ENTRY_VALID;
            } else {
                //MAC DA match, TXWI doesn't, updated txwi
                spin_unlock(&table_802_11_lock);
                return WFO_MAC_ENTRY_TXWI;
            }
        }
    }

    spin_unlock(&table_802_11_lock);
    return WFO_MAC_ENTRY_INVALID;
}

/*
 * check both MAC tables, specific check function also defined
 * called when A9 send frame to PE
 * called before A9 to PE frame send
 * if entry not found: send IPCs to PE, add NE hash then add local table entry
 */
static wfo_mac_entry_status_e cs_wfo_add_mac_entry(unsigned char *mac_da, u8 egress_port,
    wfo_mac_type_e da_type, unsigned char *p_txwi, unsigned char frame_type, unsigned char pe_id)
{
    int i;
    unsigned char index = -1;
    wfo_mac_entry_status_e rc = WFO_MAC_ENTRY_INVALID;

    if(unlikely(!mac_da))
        return WFO_MAC_ENTRY_INVALID;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID))
        return WFO_MAC_ENTRY_INVALID;


    if(WFO_MAC_TYPE_802_3 == da_type) {

        rc = cs_wfo_get_8023_mac_entry(mac_da, &index, pe_id);
        if (WFO_MAC_ENTRY_INVALID != rc) {
			if (WFO_MAC_ENTRY_NEWPE == rc) {
				if (pe_id == CS_WFO_IPC_PE0_CLNT_ID)
					mac_table_802_3[index].to_pe0 = TRUE;
				else
					mac_table_802_3[index].to_pe1 = TRUE;
				rc = WFO_MAC_ENTRY_VALID;
			}
            return rc;
        }

        spin_lock(&table_802_3_lock);
        for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
            if(mac_table_802_3[i].status == WFO_MAC_ENTRY_INVALID) {

                memcpy(mac_table_802_3[i].mac_da, mac_da, sizeof(mac_table_802_3[i].mac_da));
                mac_table_802_3[i].egress_port = egress_port;
                mac_table_802_3[i].retry_cnt = 0;
                mac_table_802_3[i].status = WFO_MAC_ENTRY_VALID;
                if (pe_id == CS_WFO_IPC_PE0_CLNT_ID) {
                    mac_table_802_3[i].to_pe0 = TRUE;
                } else {
                    mac_table_802_3[i].to_pe1 = TRUE;
                }
                spin_unlock(&table_802_3_lock);
                return WFO_MAC_ENTRY_VALID;
            }
        }
        //table full
        spin_unlock(&table_802_3_lock);
    }
    else if(WFO_MAC_TYPE_802_11 == da_type) {

        rc = cs_wfo_get_80211_mac_entry(mac_da, &index, p_txwi, frame_type, pe_id);
        if (WFO_MAC_ENTRY_INVALID != rc) {
			if (WFO_MAC_ENTRY_NEWPE == rc) {
				if (pe_id == CS_WFO_IPC_PE0_CLNT_ID)
					mac_table_802_11[index].to_pe0 = TRUE;
				else
					mac_table_802_11[index].to_pe1 = TRUE;
				rc = WFO_MAC_ENTRY_VALID;
			}
            return rc;
        }

        spin_lock(&table_802_11_lock);
        for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
            if(mac_table_802_11[i].status == WFO_MAC_ENTRY_INVALID){

                memcpy(mac_table_802_11[i].mac_da, mac_da, sizeof(mac_table_802_11[i].mac_da));
                if(p_txwi) {
                    switch(frame_type) {
                    case 1:
                        memcpy(mac_table_802_11[i].t1_txwi, p_txwi, sizeof(mac_table_802_11[i].t1_txwi));
                        break;
                    case 2:
                        memcpy(mac_table_802_11[i].t2_txwi, p_txwi, sizeof(mac_table_802_11[i].t2_txwi));
                        break;
                    default:
                        memcpy(mac_table_802_11[i].t4_txwi, p_txwi, sizeof(mac_table_802_11[i].t4_txwi));
                        break;
                    }
                }
                mac_table_802_11[i].egress_port = egress_port;
                mac_table_802_11[i].retry_cnt = 0;
                mac_table_802_11[i].status = WFO_MAC_ENTRY_VALID;
                mac_table_802_11[i].ba_created = FALSE;
                if (pe_id == CS_WFO_IPC_PE0_CLNT_ID) {
                    mac_table_802_11[i].to_pe0 = TRUE;
//                    mac_table_802_11[i].to_pe1 = FALSE;
                } else {
                    mac_table_802_11[i].to_pe1 = TRUE;
//                    mac_table_802_11[i].to_pe0 = FALSE;
                }
                spin_unlock(&table_802_11_lock);
                return WFO_MAC_ENTRY_VALID;
            }
        }
        //table full
        spin_unlock(&table_802_11_lock);
    }

    return WFO_MAC_ENTRY_INVALID;
}

/*
 * Just call this function once for 802.3 fwd entry (WFO_MAC_TYPE_802_3)
 * and once for 802.11 fwd entry (WFO_MAC_TYPE_802_11) before frame sent to PE
 * function returns true: IPCs have been sent, NE hash added
 * function returns false: just send the frame (no IPC/NE hash activities)
 * entry not found in local table,
 * send IPCs to PE, if send successful, then add NE hash, then add entry to local table
 * DON'T ADD IF IPC send failed
 * mac_da: MAC DA as loopup key.
 * egress_port: for 802.11 -> 802.3 entry, it's egress GE port number.
 * da_type: distinguish MAC DA is 802.3 or 802.11
 * pe_id: destination PE ID (?)
 * p802_11_hdr: pointer to 802.11 header buffer
 * len: 802.11 hdr length
 * vendor_txwi: pointer to vendor specific extra TX header
 * prio: connection egress priority
 * skb: used to add FE hash entry, for 802.3 -> 802.11 entry
 */
wfo_mac_entry_status_e cs_wfo_set_mac_entry(wfo_mac_entry_s *mac_entry)
{
    wfo_mac_entry_status_e rc = WFO_MAC_ENTRY_INVALID;
    unsigned char index = -1;
    u8 frame_type = mac_entry->frame_type;
    u8 pe_id = mac_entry->pe_id;
    u8 pe_id_fake;
    u8 egress_port = mac_entry->egress_port;
    wfo_mac_type_e local_type = mac_entry->da_type;
    u8 prio = mac_entry->priority;
    u8 *vendor_txwi = mac_entry->txwi;
    u8 *p802_11_hdr = mac_entry->p802_11_hdr;
    u8 *mac_da = mac_entry->mac_da;
    u16 len = mac_entry->len;
    int rc1 = 0;


    if(frame_type != 1 && frame_type != 2 && frame_type != 4)
            return rc;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID))
        return rc;

    //add MAC DAs to local table
    if(WFO_MAC_TYPE_802_3 == local_type) {

        rc = cs_wfo_get_8023_mac_entry(mac_da, &index, pe_id);
        if(WFO_MAC_ENTRY_TRY == rc)
            INC_8023_ENTRY_RETRY(index);
    }
    else {

        rc = cs_wfo_get_80211_mac_entry(mac_da, &index, vendor_txwi, frame_type, pe_id);
        if(WFO_MAC_ENTRY_TRY == rc)
            INC_80211_ENTRY_RETRY(index);
    }

    if(WFO_MAC_ENTRY_VALID == rc) {
        return rc;

    } else if(WFO_MAC_ENTRY_TXWI == rc){

            //re-send TXWI update
            if (WFO_MAC_TYPE_802_11 == local_type) {
                printk(KERN_INFO "\n!!!!!!!!!%s:%d pid %d TXWI changed\n", __func__, __LINE__, pe_id);
                //rc1 = cs_wfo_ipc_add_lookup_802_11(pe_id, p802_11_hdr, len, prio, vendor_txwi, frame_type);
            }
    }
    else {

        rc = cs_wfo_add_mac_entry(mac_da, egress_port, local_type, vendor_txwi, frame_type, pe_id);

        if(rc != WFO_MAC_ENTRY_INVALID) {

            //send 802.3 add entry IPC to PE
            if (WFO_MAC_TYPE_802_3 == local_type) {
                printk(KERN_DEBUG "\n********%s:%d, cs_wfo_ipc_add_lookup_802_3 pid %d\n", __func__, __LINE__, pe_id);
                rc1 = cs_wfo_ipc_add_lookup_802_3(pe_id, mac_da, egress_port);
            }
            else if (WFO_MAC_TYPE_802_11 == local_type) {
                printk(KERN_DEBUG "\n********%s:%d, cs_wfo_ipc_add_lookup_802_11 pid %d, mac_da %pM, orig_lspid %d\n", __func__, __LINE__, pe_id, mac_da, mac_entry->orig_lspid);
                rc1 = cs_wfo_ipc_add_lookup_802_11(pe_id, p802_11_hdr, len, prio, vendor_txwi, frame_type);
            }

            if (rc1 != 0) {
                cs_wfo_del_mac_entry(mac_da, local_type, pe_id);
                return WFO_MAC_ENTRY_INVALID;
            }
        }
    }

    printk(KERN_DEBUG "%s::return %d\n", __func__, rc);
    return rc;
}
EXPORT_SYMBOL(cs_wfo_set_mac_entry);

//delete entry after age-out or NE hash add failed
bool cs_wfo_del_mac_entry(unsigned char* mac_da, char da_type, char pe_id)
{
    int i = 0;

	if ((da_type == 0xFF) || (da_type == WFO_MAC_TYPE_802_3)) {
    	spin_lock(&table_802_3_lock);
	    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
    	    if(!memcmp(mac_da, mac_table_802_3[i].mac_da, sizeof(mac_table_802_3[i].mac_da))){
				if ((pe_id == CS_WFO_IPC_PE0_CLNT_ID) || (pe_id == 0xFF)) {
					if (mac_table_802_3[i].to_pe0) {
						cs_wfo_ipc_del_lookup_802_3(CS_WFO_IPC_PE0_CLNT_ID, mac_table_802_3[i].mac_da);
						mac_table_802_3[i].to_pe0 = FALSE;
					}
				}
				if ((pe_id == CS_WFO_IPC_PE1_CLNT_ID) || (pe_id == 0xFF)) {
					if (mac_table_802_3[i].to_pe1) {
						cs_wfo_ipc_del_lookup_802_3(CS_WFO_IPC_PE1_CLNT_ID, mac_table_802_3[i].mac_da);
						mac_table_802_3[i].to_pe1 = FALSE;
					}
				}
				if ((mac_table_802_3[i].to_pe0 == FALSE) && (mac_table_802_3[i].to_pe1 == FALSE)) {
					memset(mac_table_802_3[i].mac_da, 0, sizeof(mac_table_802_3[i].mac_da));
					mac_table_802_3[i].status = WFO_MAC_ENTRY_INVALID;
					mac_table_802_3[i].retry_cnt = 0;
				}
			}
    	}
	    spin_unlock(&table_802_3_lock);
	}

	if ((da_type == 0xFF) || (da_type == WFO_MAC_TYPE_802_11)) {
	    spin_lock(&table_802_11_lock);
    	for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        	if(!memcmp(mac_da, mac_table_802_11[i].mac_da, sizeof(mac_table_802_11[i].mac_da))){
	            if (mac_table_802_11[i].to_pe0 && mac_table_802_11[i].to_pe1) {
    	            printk("%s::duplicate 802.11 entry to PE0 and PE1\n", __func__);
        	        cs_wfo_dump_mac(mac_table_802_11[i].mac_da);
            	}
				if ((pe_id == CS_WFO_IPC_PE0_CLNT_ID) || (pe_id == 0xFF)) {
					if (mac_table_802_11[i].to_pe0) {
    		        	cs_wfo_ipc_del_lookup_802_11( CS_WFO_IPC_PE0_CLNT_ID,
        	                                  mac_table_802_11[i].mac_da,
        	                                  0 );
						mac_table_802_11[i].to_pe0 = FALSE;
					}
				}
				if ((pe_id == CS_WFO_IPC_PE1_CLNT_ID) || (pe_id == 0xFF)) {
					if (mac_table_802_11[i].to_pe1) {
        		    	cs_wfo_ipc_del_lookup_802_11( CS_WFO_IPC_PE1_CLNT_ID,
                	                          mac_table_802_11[i].mac_da,
                    	                      0 );
						mac_table_802_11[i].to_pe1 = FALSE;
					}
				}
				if ((mac_table_802_11[i].to_pe0 == FALSE) && (mac_table_802_11[i].to_pe1 == FALSE)) {
					memset(mac_table_802_11[i].mac_da, 0, sizeof(mac_table_802_11[i].mac_da));
					mac_table_802_11[i].status = WFO_MAC_ENTRY_INVALID;
					mac_table_802_11[i].retry_cnt = 0;
				}

        	}
    	}
	    spin_unlock(&table_802_11_lock);
	}
    return true;
}
EXPORT_SYMBOL(cs_wfo_del_mac_entry);


bool cs_wfo_del_all_entry()
{
    int i = 0;
    unsigned char entry_pe_id = 0;
	int chip_model_0 = cs75xx_pni_get_chip_type(0);
	int chip_model_1 = cs75xx_pni_get_chip_type(1);

    spin_lock(&table_802_3_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        if (mac_table_802_3[i].status == WFO_MAC_ENTRY_VALID){
			cs_wfo_del_hash_by_mac_da(mac_table_802_3[i].mac_da);
            cs_wfo_ipc_del_lookup_802_3(CS_WFO_IPC_PE0_CLNT_ID, mac_table_802_3[i].mac_da);
			cs_wfo_ipc_del_lookup_802_3(CS_WFO_IPC_PE1_CLNT_ID, mac_table_802_3[i].mac_da);
            memset(mac_table_802_3[i].mac_da, 0, sizeof(mac_table_802_3[i].mac_da));
            mac_table_802_3[i].status = WFO_MAC_ENTRY_INVALID;
            mac_table_802_3[i].retry_cnt = 0;
			mac_table_802_3[i].to_pe0 = FALSE;
			mac_table_802_3[i].to_pe1 = FALSE;
        }
    }
    spin_unlock(&table_802_3_lock);

    spin_lock(&table_802_11_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
         if (mac_table_802_11[i].status == WFO_MAC_ENTRY_VALID) {

            if (mac_table_802_11[i].to_pe0 && mac_table_802_11[i].to_pe1) {
                printk("%s::duplicate entry to PE0 and PE1\n", __func__);
                cs_wfo_dump_mac(mac_table_802_11[i].mac_da);
            }
			cs_wfo_del_hash_by_mac_da(mac_table_802_11[i].mac_da);

			if (chip_model_0 == CS_WFO_CHIP_RT3593)
	            cs_wfo_ipc_del_lookup_802_11( CS_WFO_IPC_PE0_CLNT_ID,
                                          mac_table_802_11[i].mac_da,
                                          0 );

			if (chip_model_1 == CS_WFO_CHIP_RT3593)
				cs_wfo_ipc_del_lookup_802_11( CS_WFO_IPC_PE1_CLNT_ID,
										  mac_table_802_11[i].mac_da,
										  0 );
			memset(mac_table_802_11[i].mac_da, 0, sizeof(mac_table_802_11[i].mac_da));
            mac_table_802_11[i].status = WFO_MAC_ENTRY_INVALID;
            mac_table_802_11[i].retry_cnt = 0;
			mac_table_802_11[i].to_pe0 = FALSE;
			mac_table_802_11[i].to_pe1 = FALSE;
        }
    }
    spin_unlock(&table_802_11_lock);
    return true;
}
EXPORT_SYMBOL(cs_wfo_del_all_entry);


void cs_wfo_dump_mac_entry(void)
{
    int i = 0;

    spin_lock(&table_802_3_lock);
    printk("=====================================================\n");
    printk("### A9 802.3 mac entries\n");
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        if (WFO_MAC_ENTRY_VALID == mac_table_802_3[i].status) {
            printk("(%d) mac_da: %pM\n",i , mac_table_802_3[i].mac_da);
            printk("(%d) to_pe0: %d\n",i , mac_table_802_3[i].to_pe0);
            printk("(%d) to_pe1: %d\n\n",i , mac_table_802_3[i].to_pe1);
        }
    }
    spin_unlock(&table_802_3_lock);

    spin_lock(&table_802_11_lock);
    printk("=====================================================\n");
    printk("### A9 802.11 mac entries\n");
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {
        if(WFO_MAC_ENTRY_VALID == mac_table_802_11[i].status) {
            printk("(%d) mac_da: %pM\n",i , mac_table_802_11[i].mac_da);
            printk("(%d) to_pe0: %d\n",i , mac_table_802_11[i].to_pe0);
            printk("(%d) to_pe1: %d\n\n",i , mac_table_802_11[i].to_pe1);
        }
    }
    printk("=====================================================\n");
    spin_unlock(&table_802_11_lock);

    return;
}
EXPORT_SYMBOL(cs_wfo_dump_mac_entry);



//increment BA send count for mac entry
unsigned char cs_wfo_mac_entry_ba_inc(unsigned char* mac_da, unsigned char pe_id)
{
    int i = 0;

    if(unlikely(!mac_da))
        return 0;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID)) {

        printk("BA INC inv PE %d", pe_id);
        return 0;
    }

    spin_lock(&table_802_11_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {

        if(WFO_MAC_ENTRY_INVALID == mac_table_802_11[i].status) {
            //cs_wfo_dump_mac_with_comemnts("skip inv entry", mac_table_802_11[i].mac_da);
            continue;
        }

        if(!memcmp(mac_da, mac_table_802_11[i].mac_da, sizeof(mac_table_802_11[i].mac_da))){

            if (mac_table_802_11[i].to_pe0 && mac_table_802_11[i].to_pe1) {
                printk("%s: %d duplicate entry to PE0 and PE1\n", __func__, __LINE__);
                cs_wfo_dump_mac(mac_table_802_11[i].mac_da);
            }

            mac_table_802_11[i].ba_count++;
            if(!mac_table_802_11[i].ba_count)
                mac_table_802_11[i].ba_count++;

            //cs_wfo_dump_mac_with_comemnts("BA INC MAC found", mac_da);
            spin_unlock(&table_802_11_lock);
            return mac_table_802_11[i].ba_count;
        }
    }

    spin_unlock(&table_802_11_lock);
    //cs_wfo_dump_mac_with_comemnts("BA INC MAC not found", mac_da);
    return 0;
}
EXPORT_SYMBOL(cs_wfo_mac_entry_ba_inc);



//Get BA session create status for mac entry
unsigned char cs_wfo_mac_entry_get_ba_status(unsigned char* mac_da, unsigned char pe_id)
{
    int i = 0;

    if(unlikely(!mac_da))
        return 0;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID)) {

        printk("BA INC inv PE %d", pe_id);
        return 0;
    }

    spin_lock(&table_802_11_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {

        if(WFO_MAC_ENTRY_INVALID == mac_table_802_11[i].status) {
            //cs_wfo_dump_mac_with_comemnts("skip inv entry", mac_table_802_11[i].mac_da);
            continue;
        }

        if(!memcmp(mac_da, mac_table_802_11[i].mac_da, sizeof(mac_table_802_11[i].mac_da))){

            if (mac_table_802_11[i].to_pe0 && mac_table_802_11[i].to_pe1) {
                printk("%s: %d duplicate entry to PE0 and PE1\n", __func__, __LINE__);
                cs_wfo_dump_mac(mac_table_802_11[i].mac_da);
            }

            //cs_wfo_dump_mac_with_comemnts("BA INC MAC found", mac_da);
            spin_unlock(&table_802_11_lock);
            return mac_table_802_11[i].ba_created;
        }
    }

    spin_unlock(&table_802_11_lock);
    //cs_wfo_dump_mac_with_comemnts("BA INC MAC not found", mac_da);
    return 0;
}
EXPORT_SYMBOL(cs_wfo_mac_entry_get_ba_status);


void cs_wfo_mac_entry_set_ba_status(unsigned char* mac_da, unsigned char pe_id, unsigned char ba_created)
{
    int i = 0;

    if(unlikely(!mac_da))
        return;

    if(unlikely(pe_id != CS_WFO_IPC_PE0_CLNT_ID && pe_id != CS_WFO_IPC_PE1_CLNT_ID)) {
       printk("BA INC inv PE %d", pe_id);
        return;
    }

    spin_lock(&table_802_11_lock);
    for(i = 0; i < WFO_MAC_ENTRY_SIZE; i++) {

        if(WFO_MAC_ENTRY_INVALID == mac_table_802_11[i].status) {
            continue;
        }

        if(!memcmp(mac_da, mac_table_802_11[i].mac_da, sizeof(mac_table_802_11[i].mac_da))){

            if (mac_table_802_11[i].to_pe0 && mac_table_802_11[i].to_pe1) {
                printk("%s: %d duplicate entry to PE0 and PE1\n", __func__, __LINE__);
                cs_wfo_dump_mac(mac_table_802_11[i].mac_da);
            }
            mac_table_802_11[i].ba_created = ba_created;
            spin_unlock(&table_802_11_lock);
            return;
        }
    }

    spin_unlock(&table_802_11_lock);
    return;
}
EXPORT_SYMBOL(cs_wfo_mac_entry_set_ba_status);

void cs_wfo_mac_table_init(void)
{
    memset((void*)(&mac_table_802_3[0]), 0, sizeof(struct wfo_mac_table_entry)*WFO_MAC_ENTRY_SIZE);
    memset((void*)(&mac_table_802_11[0]), 0, sizeof(struct wfo_mac_table_entry)*WFO_MAC_ENTRY_SIZE);

    spin_lock_init(&table_802_3_lock);
    spin_lock_init(&table_802_11_lock);

    //aging may not be needed right now, low priority
    init_timer(&wfo_mac_timer);

    wfo_mac_timer.expires = jiffies + secs_to_cputime(WFO_MAC_ENTRY_TIMER_PERIOD);
    wfo_mac_timer.function = &cs_wfo_mac_table_timer_func;
    return;
}
EXPORT_SYMBOL(cs_wfo_mac_table_init);
