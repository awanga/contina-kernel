#include <linux/list.h>		/* list_head structure */
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/jiffies.h>

#include <mach/cs_types.h>
#include "cs_core_logic.h"
#include "cs_hw_accel_manager.h"
#include "cs_fe_mc.h"
#include "cs_core_hmu.h"
#include "cs752x_eth.h"
#include "cs_wfo_csme.h"
#include <mach/cs75xx_pni.h>
#include <mach/cs75xx_ipc_wfo.h>


#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_wfo_csme;
extern u32 cs_wfo_debug;
#define DBG_PNI(x)	if (cs_wfo_debug & CS752X_WFO_DBG_PNI) x
#define DBG_IPC(x)	if (cs_wfo_debug & CS752X_WFO_DBG_IPC) x

extern u32 cs_adapt_debug;
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_WFO) x

#else
#define DBG(x)	{}
#endif /* CONFIG_CS752X_PROC */

extern u32 cs_hw_ipsec_offload_mode;

#if 0
int cs_wfo_rt3593_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_WFO_RT3593);
}
#endif

int cs_hw_accel_wfo_enable(void)
{
	return cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_WFO);
	//return cs_wfo_rt3593_enable();
}


static u8 cs_wfo_map_port_id_to_wfo_egress_port(u8 port_id)
{
	/* get the destination VOQ */
	switch (port_id) {
		case GE_PORT0:
			return CS_WFO_EGRESS_PORT_GMAC_0;
		case GE_PORT1:
			return CS_WFO_EGRESS_PORT_GMAC_1;
		case GE_PORT2:
			return CS_WFO_EGRESS_PORT_GMAC_2;
		case ENCRYPTION_PORT:
			return CS_WFO_EGRESS_PORT_PE_0;
		case ENCAPSULATION_PORT:
			return CS_WFO_EGRESS_PORT_PE_1;
		default:
			printk("%s:%d:Unacceptable port_id %d\n", __func__, __LINE__,
				port_id);
		break;
	}
	return CS_WFO_EGRESS_PORT_GMAC_0;
}

int cs_hw_accel_create_802_3_entry(u8 port_id, struct sk_buff *skb) {
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u8 egress_port_id = cs_wfo_map_port_id_to_wfo_egress_port(port_id);
	u8 pe_id ;
	pe_id = (cs_cb->key_misc.orig_lspid == ENCAPSULATION_PORT)? \
				CS_WFO_IPC_PE1_CPU_ID:CS_WFO_IPC_PE0_CPU_ID;

	wfo_mac_entry_s mac_entry;
	/*FIXME: the mac_da should be the input da*/
	//mac_entry.mac_da = skb->data;
	mac_entry.mac_da = cs_cb->input.raw.da;
	//mac_entry.mac_sa = cs_cb->input.raw.sa;
	mac_entry.egress_port = egress_port_id;
	mac_entry.da_type = WFO_MAC_TYPE_802_3;
	mac_entry.pe_id = pe_id;
	mac_entry.priority = 0;
	mac_entry.frame_type = 4; // TX_AMPDU_FRAME
	DBG(printk("add 802.3 MAC table\n"));
	DBG(printk("add MAC table IN SA: %pM\n", cs_cb->input.raw.sa));
	DBG(printk("add MAC table IN DA: %pM\n", cs_cb->input.raw.da));

	return cs_wfo_set_mac_entry(&mac_entry);
}

int cs_hw_accel_wfo_wifi_rx(int wifi_chip, int pe_id, struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb;
	unsigned char * temp_data;
//Bug#40276	u8 orig_lspid;

	if ((cs_hw_accel_wfo_enable()) && (cs_accel_cb_add(skb) == 0)) {

		if (skb_mac_header_was_set(skb)) {
			temp_data = skb->data;
			skb->data = skb->mac_header;
			//DBG(printk("%s skb already have mac header\n", __func__));
		} else
			temp_data = skb->data;

		//DBG(printk("\t %s mac header=da %pM sa %pM pe_id=%d\n", __func__,
		//	skb->data, skb->data + 6, pe_id));
		cs_cb = CS_KERNEL_SKB_CB(skb);
//Bug#40276:		orig_lspid = cs_cb->key_misc.orig_lspid;

		/*need to make sure skb->data is point to da mac header*/
		cs_core_logic_input_set_cb(skb);

		skb->data = temp_data;
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
        if (pe_id == CS_WFO_IPC_PE1_CPU_ID) {
            cs_cb->key_misc.orig_lspid = ENCAPSULATION_PORT;
        } else {
            cs_cb->key_misc.orig_lspid = ENCRYPTION_PORT;
        }
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

		/*if support multi wifi chip, need to put different module mask*/
		//if (wifi_chip == CS_WFO_CHIP_RT3593)
		cs_cb->common.module_mask |= CS_MOD_MASK_WFO;
	}
	return 0;

}

int cs_hw_accel_wfo_wifi_tx(int wifi_chip, int pe_id, struct sk_buff *skb,
	u8 tx_qid, wfo_mac_entry_s * mac_entry)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u8 d_voq_id;
	int rc;

	if (cs_cb == NULL) {
		return 0;
	}

	if (!cs_hw_accel_wfo_enable())
		return 0;
#if 0
	if (wifi_chip == CS_WFO_CHIP_RT3593) {
		if (!cs_wfo_rt3593_enable())
			return 0;
	}
#endif
	/*TODO: WFO only support bridge mode only */
	//if ((cs_cb->common.module_mask & 0xff) != CS_MOD_MASK_BRIDGE) {
	//	return 0;
	//}
	d_voq_id = tx_qid + WFO_GE2PE_P0_OFFSET;

	cs_cb->action.voq_pol.d_voq_id = d_voq_id;
	cs_cb->action.voq_pol.cos_nop = 1;
	// 802.3 -> 802.11 IPC message
	// egress_port = tx_qid / 8 - 2;

	/*
	 *FIXME: it is better to prepare ipc_mac_entry but
	 *now it is handled by wifi driver
	 */
#if 0
	wfo_mac_entry_s mac_entry;
	mac_entry.mac_da = cs_cb->input.raw.da;
	mac_entry.egress_port = egress_port;
	mac_entry.da_type = WFO_MAC_TYPE_802_11;
	mac_entry.pe_id = pe_id;
	mac_entry.p802_11_hdr = phdr_802_11;
	mac_entry.len = pTxBlk->MpduHeaderLen + pTxBlk->HdrPadLen - 8;
	mac_entry.txwi = ptxwi;
	mac_entry.priority = 0;
	mac_entry.frame_type = pTxBlk->TxFrameType;
	mac_entry.orig_lspid = cs_cb->key_misc.orig_lspid;
#endif

	if (is_multicast_ether_addr(cs_cb->input.raw.da) || is_broadcast_ether_addr(cs_cb->input.raw.da)) {
		printk("%s::SA BC/MC!\n", __func__);
	}
	else {
		rc = cs_core_logic_add_connections(skb);
		if (rc == CS_ACCEL_HASH_SUCCESS) {
			if (wifi_chip == CS_WFO_CHIP_RT3593) {
				DBG(printk("%s %d: \n", __func__, __LINE__));
				DBG(printk("%s add 802.11 mac table\n", __func__));
				DBG(printk(" module_mask=%X\n", cs_cb->common.module_mask));
				DBG(printk("add MAC table IN SA: %pM\n", cs_cb->input.raw.sa));
				DBG(printk("add MAC table IN DA: %pM\n", cs_cb->input.raw.da));
				rc = cs_wfo_set_mac_entry(mac_entry);
				DBG(printk("%s add 802.11 mac table result=%d\n", __func__, rc));
				/*need to create 802.3 table when src device is wifi*/
				if ((cs_cb->key_misc.orig_lspid == ENCRYPTION_PORT) ||
					(cs_cb->key_misc.orig_lspid == ENCAPSULATION_PORT)) {
					rc = cs_hw_accel_create_802_3_entry(tx_qid / 8 - 2, skb);
					DBG(printk("%s add 802.3 mac table result=%d\n", __func__, rc));
				}
			}
		}
	}

	return 0;

}

extern void __cs_mc_delete_hash_by_ipv4_group(__be32 origin, __be32 mcastgrp);

int cs_hw_accel_wfo_wifi_tx_voq(int wifi_chip, int pe_id, struct sk_buff *skb,
	u8 tx_qid, wfo_mac_entry_s * mac_entry)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	u8 d_voq_id;
	u8 egress_port_id;
	int rc;

	if (cs_cb == NULL) {
		return -1;
	}

	if (!cs_hw_accel_wfo_enable())
		return -1;

	d_voq_id = tx_qid;

	cs_cb->action.voq_pol.d_voq_id = d_voq_id;
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    cs_cb->action.voq_pol.voq_policy = 1;
#else
	cs_cb->action.voq_pol.cos_nop = 1;
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

	if (wifi_chip == CS_WFO_CHIP_RT3593) {
		if (is_multicast_ether_addr(cs_cb->input.raw.da) || is_broadcast_ether_addr(cs_cb->input.raw.da)) {
			printk("%s::CS_WFO_CHIP_RT3593 doesn't support multicast\n", __func__);
			return -1;
		}
	}

    DBG(printk("### %s::%d, cs_cb->input.raw.da %pM, module_mask 0x%x, egress_port_id 0x%x\n"
            , __func__, __LINE__, cs_cb->input.raw.da
            , cs_cb->common.module_mask, cs_cb->common.egress_port_id));
    if ( (cs_cb->common.module_mask & CS_MOD_MASK_IPV4_MULTICAST) ||
         (cs_cb->common.module_mask & CS_MOD_MASK_IPV6_MULTICAST) ) {
        if (cs_wfo_csme & CS752X_WFO_CSME_ENABLE) {
            if (cs_cb->common.egress_port_id == 0xFF) {
                DBG(printk("### %s::%d CS_MOD_MASK_WFO_SW_MULTICAST sw_only 0x%x\n" ,
                        __func__, __LINE__, cs_cb->common.sw_only));
                cs_cb->common.module_mask |= CS_MOD_MASK_WFO_SW_MULTICAST;
                __cs_mc_delete_hash_by_ipv4_group(0, cs_cb->input.raw.da);
            }
            DBG(printk("### %s::%d, da %pM, sw_only 0x%x, module_mask 0x%x, egress_port_id 0x%x\n"
                    , __func__, __LINE__, skb->data, cs_cb->common.sw_only
                    , cs_cb->common.module_mask, cs_cb->common.egress_port_id));
        } else {
            DBG(printk("### %s::%d, cs_cb->input.raw.da %pM, cs_cb->output.raw.da %pM, module_mask 0x%x, egress_port_id 0x%x\n"
                    , __func__, __LINE__, cs_cb->input.raw.da, cs_cb->output.raw.da
                    , cs_cb->common.module_mask, cs_cb->common.egress_port_id));
            rc = cs_wfo_mc_mac_table_add_entry(cs_cb->input.raw.da, &egress_port_id);
            if ( rc == CS_MC_MAC_ENTRY_STATUS_SUCCESS) {
                DBG(printk("### %s::%d, da %pM, sw_only 0x%x, module_mask 0x%x, egress_port_id 0x%x\n"
                        , __func__, __LINE__, cs_cb->input.raw.da
                        , cs_cb->common.sw_only, cs_cb->common.module_mask
                        , egress_port_id));
                cs_cb->common.egress_port_id = egress_port_id;
            } else {
                DBG(printk("### %s::%d CS_MOD_MASK_WFO_SW_MULTICAST sw_only 0x%x\n"
                        , __func__, __LINE__, cs_cb->common.sw_only));
                cs_cb->common.module_mask |= CS_MOD_MASK_WFO_SW_MULTICAST;
                __cs_mc_delete_hash_by_ipv4_group(0, cs_cb->input.raw.da);
            }
        }/* if (cs_wfo_csme & CS752X_WFO_CSME_ENABLE) */
    }/* if ( (cs_cb->common.module_mask) */

	rc = cs_core_logic_add_connections(skb);
	if (rc == CS_ACCEL_HASH_SUCCESS) {
		if (wifi_chip == CS_WFO_CHIP_RT3593) {
			DBG(printk("%s %d: \n", __func__, __LINE__));
			DBG(printk("%s add 802.11 mac table\n", __func__));
			DBG(printk(" module_mask=%X\n", cs_cb->common.module_mask));
			DBG(printk("add MAC table IN SA: %pM\n", cs_cb->input.raw.sa));
			DBG(printk("add MAC table IN DA: %pM\n", cs_cb->input.raw.da));
			rc = cs_wfo_set_mac_entry(mac_entry);
			DBG(printk("%s add 802.11 mac table result=%d\n", __func__, rc));
			/*need to create 802.3 table when src device is wifi*/
			if ((cs_cb->key_misc.orig_lspid == ENCRYPTION_PORT) ||
				(cs_cb->key_misc.orig_lspid == ENCAPSULATION_PORT)) {
				rc = cs_hw_accel_create_802_3_entry(tx_qid / 8 - 2, skb);
				DBG(printk("%s add 802.3 mac table result=%d\n", __func__, rc));
			}
		}
	}

	return rc;

}

EXPORT_SYMBOL(cs_hw_accel_wfo_wifi_rx);
EXPORT_SYMBOL(cs_hw_accel_wfo_wifi_tx);
EXPORT_SYMBOL(cs_hw_accel_wfo_wifi_tx_voq);

extern int cs_wfo_csme_SnoopInspecting(struct sk_buff *skb, unsigned char ad_id);   //BUG#39828

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
/*
 * return  1: wfo kernel adapt skip the packet (this packet should not handle in wfo)
 * return  0: wfo kernel adapt handle the packet
 * return -1: wfo kernel adapt handle the packet but have err
 */
int cs_hw_accel_wfo_handle_rx(int instance, int voq, struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	bool b_wfo_packet = true;
	/*TODO: need to distingusih wifi device for rt3593 or atheros, etc.. */

	if (instance == CS_NI_INSTANCE_WFO) {
		/*802.11 packet and direct send to pni*/
		//DBG(printk("%s receive 802.11 packet at instance %d, voq=%d, lspid=%d\n",
		//	__func__, instance, voq, cs_cb->key_misc.orig_lspid));
		if (cs_cb != NULL) {
			if (!cs_hw_accel_wfo_enable()) {
				cs_cb->common.sw_only = CS_SWONLY_STATE;
			}
			cs_cb->common.module_mask |= CS_MOD_MASK_WFO;
		}
		cs75xx_pni_rx((cs_cb->key_misc.orig_lspid == CS_FE_LSPID_4) ? 0 : 1, voq, skb);
		return 0;

	} else if ((instance == CS_NI_INSTANCE_WFO_802_3_PE0) ||
	           (instance == CS_NI_INSTANCE_WFO_802_3_PE1)) {

			/* packet for WFO and need to:
			 *  1. set src dev as wifi dev
			 *  2. send to kernel
			 */
		 	if (cs_hw_accel_wfo_enable() == 0) {
				if (cs_cb != NULL)
					cs_cb->common.sw_only = CS_SWONLY_STATE;
		 	}
 			DBG(printk("%s:%d:: receive 802.3 packet at voq %d, lspid=%d, da %pM sa %pM sw_only=%d\n",
 				__func__, __LINE__, voq, cs_cb->key_misc.orig_lspid, skb->data, skb->data + 6,
 				cs_cb->common.sw_only));

			/* need to check voq to decide wifi chip model*/
			if (cs_cb != NULL)
				cs_cb->common.module_mask |= CS_MOD_MASK_WFO;

            if (cs_wfo_csme & CS752X_WFO_CSME_ENABLE) {
    			if (is_multicast_ether_addr(skb->data) && !is_broadcast_ether_addr(skb->data)) {
    			    //++BUG#39828
    			    if (voq == 96) {
    			        cs_wfo_csme_SnoopInspecting(skb, CS_WFO_CSME_WIFI_0);
    			    } else if (voq == 97) {
    			        cs_wfo_csme_SnoopInspecting(skb, CS_WFO_CSME_WIFI_1);
    			    }
    			    //--BUG#39828
    			}
    		}
			/* BUG#39672: WFO NEC related features (Mutliple BSSID) */
			cs75xx_pni_rx_8023(instance - CS_NI_INSTANCE_WFO_802_3_PE0, voq, skb);
			return 0;
	}
	return 1;
}
#else //CONFIG_CS75XX_OFFSET_BASED_QOS
/*
 * return  1: wfo kernel adapt skip the packet (this packet should not handle in wfo)
 * return  0: wfo kernel adapt handle the packet
 * return -1: wfo kernel adapt handle the packet but have err
 */
int cs_hw_accel_wfo_handle_rx(int instance, int voq, struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	bool b_wfo_packet = true;
	/*TODO: need to distingusih wifi device for rt3593 or atheros, etc.. */

	if ((instance == CS_NI_INSTANCE_WFO_PE0) ||
		(instance == CS_NI_INSTANCE_WFO_PE1)) {
		/*802.11 packet and direct send to pni*/
		DBG(printk("%s receive 802.11 packet at instance %d, voq=%d, lspid=%d\n",
			__func__, instance, voq, cs_cb->key_misc.orig_lspid));
		if (cs_cb != NULL) {
			if (!cs_hw_accel_wfo_enable()) {
				cs_cb->common.sw_only = CS_SWONLY_STATE;
			}
			cs_cb->common.module_mask |= CS_MOD_MASK_WFO;
		}
		cs75xx_pni_rx(instance, voq, skb);
		return 0;

	} else if (instance == CS_NI_INSTANCE_WFO_802_3) {

			/* packet for WFO and need to:
			 *  1. set src dev as wifi dev
			 *  2. send to kernel
			 */
		 	if (cs_hw_accel_wfo_enable() == 0) {
				if (cs_cb != NULL)
					cs_cb->common.sw_only = CS_SWONLY_STATE;
		 	}
 			DBG(printk("%s receive 802.3 packet at voq %d, lspid=%d, da %pM sa %pM sw_only=%d\n",
 				__func__, voq, cs_cb->key_misc.orig_lspid, skb->data, skb->data + 6,
 				cs_cb->common.sw_only));

			/* need to check voq to decide wifi chip model*/
			if (cs_cb != NULL)
				cs_cb->common.module_mask |= CS_MOD_MASK_WFO;

            if (cs_wfo_csme & CS752X_WFO_CSME_ENABLE) {
    			if (is_multicast_ether_addr(skb->data) && !is_broadcast_ether_addr(skb->data)) {
    			    //++BUG#39828
    			    if (voq == 96) {
    			        cs_wfo_csme_SnoopInspecting(skb, CS_WFO_CSME_WIFI_0);
    			    } else if (voq == 97) {
    			        cs_wfo_csme_SnoopInspecting(skb, CS_WFO_CSME_WIFI_1);
    			    }
    			    //--BUG#39828
    			}
    		}
			/* BUG#39672: WFO NEC related features (Mutliple BSSID) */
			cs75xx_pni_rx_8023(((voq % 2) == 0)? CS_NI_INSTANCE_WFO_PE0: CS_NI_INSTANCE_WFO_PE1,
                              voq, skb);
			return 0;
	}
	return 1;
}
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

/*
 * return value: the same definition as cs_hw_accel_wfo_handle_rx()
 */
int cs_hw_accel_wfo_handle_tx(u8 port_id, struct sk_buff *skb)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	int rc;

	DBG(printk("%s receive 802.3 packet at	lspid=%d, da %pM sa %pM sw_only=%d ingress_port_id=%d module_mask=0x%x\n",
		__func__, cs_cb->key_misc.orig_lspid, skb->data, skb->data + 6,
		cs_cb->common.sw_only, cs_cb->common.ingress_port_id, cs_cb->common.module_mask ));

	/*if support multi wifi chip, need to check different module mask*/
	if (cs_cb->common.module_mask & CS_MOD_MASK_WFO) {
		if (cs_hw_accel_wfo_enable()) {
			/*TODO: WFO only support bridge mode only */
			//if ((cs_cb->common.module_mask & 0xff) != CS_MOD_MASK_BRIDGE) {
			//	return 0;
			//}

			rc = cs_core_logic_add_connections(skb);
			if (rc == CS_ACCEL_HASH_SUCCESS) {
				if (cs75xx_pni_get_chip_type(cs_cb->common.ingress_port_id - CS_NI_IRQ_WFO_PE0)
					== CS_WFO_CHIP_RT3593) {
					DBG(printk("%s module_mask=%X\n",__func__, cs_cb->common.module_mask));
					DBG(printk("%s add 802.3 mac table\n", __func__));
					cs_hw_accel_create_802_3_entry(port_id, skb);
				}
			}
			return 0;
		} else
			return -1;
	}else
		return 1;


}

int cs_wfo_hook_remove(void)
{
	return 0;
}

int cs_wfo_hook_insert(void)
{
	return 0;
}

void cs_wfo_rt3593_callback_core_logic_notify(unsigned long notify_event,
					   unsigned long value)
{
	DBG(printk("%s() accel manager event %ld\n", __func__, notify_event));

	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			cs_wfo_del_all_entry();
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			cs_wfo_hook_remove();
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			cs_wfo_hook_insert();
			break;
	}
}

int cs_hw_accel_wfo_init(void)
{
	cs_wfo_hook_insert();

	/*for hw accel manager */
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_WFO,
					       cs_wfo_rt3593_callback_core_logic_notify);

    cs_wfo_csme_init();

	return 0;
} /* cs_wfo_init */

int cs_hw_accel_wfo_exit(void)
{
	cs_hw_accel_mgr_register_proc_callback(CS752X_ADAPT_ENABLE_WFO,
					       NULL);
	cs_wfo_hook_remove();
	return 0;
}

int cs_hw_accel_wfo_clean_fwd_path_by_mac(char * mac)
{
	cs_core_hmu_value_t hmu_value;
	int i;
	DBG(printk("%s delete mac=%pM\n", __func__, mac));

	hmu_value.type = CS_CORE_HMU_WATCH_IN_MAC_SA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = mac[5 - i];
	hmu_value.mask = 0;

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_MAC_SA, &hmu_value);


	hmu_value.type = CS_CORE_HMU_WATCH_OUT_MAC_DA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = mac[5 - i];

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_OUT_MAC_DA, &hmu_value);

	cs_wfo_del_mac_entry(mac, 0xFF ,0xFF);

	return 0;
}
EXPORT_SYMBOL(cs_hw_accel_wfo_clean_fwd_path_by_mac);

