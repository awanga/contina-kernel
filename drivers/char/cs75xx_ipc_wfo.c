#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/module.h>	/* Specifically, a module */
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for get_user and put_user */
#include <linux/miscdevice.h>
#include <linux/timer.h>
#include <linux/list.h>		/* list_head structure	*/

#include <cs_core_hmu.h>
#include <mach/hardware.h>
#include <linux/etherdevice.h>
#include <mach/g2cpu_ipc.h>
#include <mach/cs75xx_ipc_wfo.h>
#include <mach/cs75xx_pni.h>
#include "cs75xx_wfo_brcm_rpc.h"

#ifdef CONFIG_CS752X_PROC
#include <cs752x_proc.h>
extern u32 cs_wfo_debug;
extern u32 cs_wfo_enable;

//++Bug#40475
#include <mach/cs75xx_mibs.h>
#include <mach/cs75xx_mibs_wfo.h>
//--Bug#40475

#define IPC_WFO_DEBUG(x) {if (cs_wfo_debug & CS752X_WFO_DBG_IPC) x;}
#else
#define IPC_WFO_DEBUG(x) {}
#endif /* CONFIG_CS752X_PROC */

static struct ipc_context *wfo_ipc_ctxt0, *wfo_ipc_ctxt1;
static u8 f_re_status[CS_WFO_IPC_CLNT_ID_MAX];

typedef struct phy_addr_s {
	u32 pcie_phy_addr_start[6];
	u32 pcie_phy_addr_end[6];
	u8  addr_mask;
	u8  pcie_phy_set;
};

#define G2_IPC_RETRY_LISY_SUPPORT   1
#ifdef G2_IPC_RETRY_LISY_SUPPORT
static struct timer_list wfo_ipc_retry_timer;
typedef struct ipc_retry_s {
    struct ipc_retry_s *p_next;
    u8 seq_no;
    u8 pe_id;
    u8 retry;
    u8 msg_payload[IPC_MSG_SIZE];
}ipc_retry_t;
ipc_retry_t *p_ipc_retry_list = NULL;

#define G2_IPC_WFO_SEND_RETRY       5

static int wfo_ipc_send( u8 pe_id,
                         u8 msg_type,
                         u8 *p_msg_payload );
#endif //G2_IPC_RETRY_LISY_SUPPORT

struct phy_addr_s phy_addr[CS_WFO_IPC_CLNT_ID_MAX];

struct cs_wfo_ipc_del_802_11_queue {
	__u32               pe_id;
	__u8                wfo_cmd_seq;
	struct list_head	list;
};

struct list_head cs_wfo_ipc_del_802_11_queue;


// Wi-Fi adapter
struct wifi_adapter_addr_s {
	void  *wfo_wifi_adatper_addr;
	__u8  valid;
};
struct wifi_adapter_addr_s wifi_handler[CS_WFO_IPC_CLNT_ID_MAX];


#define G2_IPC_WFO_SEND_TIMEOUT     1
__u8 wfo_cmd_seq = 1;
__u8 wfo_cmd_resp_seq = 0xFF;


// wfo definitions for hmu watch
cs_core_hmu_t wfo_mac_da_hmu_entry;
cs_core_hmu_value_t wfo_mac_da_hmu_value;
__u8 cs_wfo_init_hmu_watch_init = 0;
void cs_wfo_init_hmu_watch(void);


#ifdef G2_IPC_RETRY_LISY_SUPPORT
// IPC retry helper functions
void wfo_ipc_hlp_add_to_retry_list(u8 pe_id, u8 *p_msg)
{
    ipc_retry_t *p_retry = NULL;
    ipc_retry_t *p_rt = NULL;
    cs_wfo_ipc_msg_t *p_wfo_ipc_msg = NULL;


    if (!p_msg || pe_id >= CS_WFO_IPC_CLNT_ID_MAX) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> +++++:: !p_msg\n"));
        return;
    }

    p_retry = kzalloc( sizeof( ipc_retry_t ), GFP_ATOMIC);
    if (!p_retry) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> +++++:: p_retry kmalloc fail\n"));
        return;
    }

//    memset(p_retry, 0, sizeof(ipc_retry_t));
    p_wfo_ipc_msg = (cs_wfo_ipc_msg_t *)p_msg;
    p_retry->pe_id = pe_id;
    p_retry->seq_no = p_wfo_ipc_msg->hdr.pe_msg.wfo_cmd_seq;
    p_retry->retry = 0;
    memcpy(p_retry->msg_payload, p_msg, sizeof(IPC_MSG_SIZE));

    IPC_WFO_DEBUG(printk("<IPC_WFO> +++++:: pe_id %d, seq_no %d\n", p_retry->pe_id, p_retry->seq_no));
    //need to add lock if calling from multiple context
    if (!p_ipc_retry_list) {
        p_ipc_retry_list = p_retry;
        p_retry->p_next = NULL;

        IPC_WFO_DEBUG(printk("<IPC_WFO> --------------> Queue %d\n", p_retry->seq_no));
        //release lock
        return;
    }

    p_rt = p_ipc_retry_list;
    while(p_rt->p_next) {
        p_rt = p_rt->p_next;
    };

    p_rt->p_next = p_retry;

    IPC_WFO_DEBUG(printk("<IPC_WFO> --------------> Queue %d\n", p_retry->seq_no));
    //release lock

    return;
} /* wfo_ipc_hlp_add_to_retry_list */

void wfo_ipc_hlp_del_from_retry_list(cs_wfo_ipc_msg_t *p_msg)
{
    ipc_retry_t *p_rt = NULL;
    ipc_retry_t *p_prev = NULL;
    u8 seq_no = 0;


    if(!p_ipc_retry_list || !p_msg) {
        return;
    }

    seq_no = p_msg->hdr.pe_msg_complete.wfo_ack_seq;
    IPC_WFO_DEBUG(printk("<IPC_WFO> -----:: seq_no %d\n", seq_no));

    p_rt = p_ipc_retry_list;
    p_prev = NULL;
    while(p_rt)
    {
        if (p_rt->seq_no == seq_no) {
            if (p_prev) {
                p_prev->p_next = p_rt->p_next;
            } else {
                p_ipc_retry_list = p_rt->p_next;
            }
            IPC_WFO_DEBUG(printk("<IPC_WFO> <-------------- DeQueue %d\n", p_rt->seq_no));

            kfree(p_rt);
            return;
        } /* if */

        p_prev = p_rt;
        p_rt = p_rt->p_next;
    } /* while(p_rt) */

    return;
} /* wfo_ipc_hlp_del_from_retry_list */


void cs_wfo_ipc_retry_timer_func(unsigned long data)
{
    ipc_retry_t *p_rt = p_ipc_retry_list;
    cs_wfo_ipc_msg_t    *p_wfo_ipc_msg = NULL;
    u8 pe_id;

    if (!p_rt) {
        return; //just return without rescheduling timer
    }

    pe_id = p_rt->pe_id;
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: pe_id %x, seq_no %d\n", __func__, pe_id, p_rt->seq_no));
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s::pe_id 0x%x CS_WFO_IPC_PE_DEAD\n", __FILE__, pe_id));
        wfo_ipc_hlp_del_from_retry_list((cs_wfo_ipc_msg_t*)p_rt->msg_payload);
        return;
    }

    //update seq no
    p_wfo_ipc_msg = (cs_wfo_ipc_msg_t *)p_rt->msg_payload;
    p_wfo_ipc_msg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    //resend oldest un-ack message
    printk("\n**********%s:%d IPC resend\n", __func__, __LINE__);
    if (wfo_ipc_send(p_rt->pe_id, CS_WFO_IPC_PE_MESSAGE, p_rt->msg_payload) == -1)
        return; //send failed, something wrong with IPC

    if (p_rt->retry++ > G2_IPC_WFO_SEND_RETRY) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> retry failed %s:%d\n", __FILE__, __LINE__));
        wfo_ipc_hlp_del_from_retry_list((cs_wfo_ipc_msg_t*)p_rt->msg_payload);
    }

    //re-schedule timer
    wfo_ipc_retry_timer.expires = jiffies + secs_to_cputime(G2_IPC_WFO_SEND_TIMEOUT);
    add_timer(&wfo_ipc_retry_timer);

	return;
} /* cs_wfo_ipc_retry_timer_func */
#endif //G2_IPC_RETRY_LISY_SUPPORT


//=========================================================================================
// Local Functions
//
static int wfo_ipc_start_complete_callback( struct ipc_addr peer,
					                        u16 msg_no,
					                        const void *msg_data,
					                        u16 msg_size,
					                        struct ipc_context *context )
{
	u8 pe_id;
	cs_wfo_ipc_msg_t *p_msg = (cs_wfo_ipc_msg_t *)msg_data;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: [%d] msg_size %d, status %d\n",
            __func__,  p_msg->pe_id, msg_size, p_msg->hdr.pe_msg_complete.wfo_status));

	if (NULL == p_msg) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: p_msg fail\n", __func__));
		return -1;
	}

	pe_id = p_msg->pe_id;

    if (p_msg->hdr.pe_msg_complete.wfo_status != CS_WFO_OP_SUCCESS) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: status fail %d\n", __func__, p_msg->hdr.pe_msg_complete.wfo_status));
	    f_re_status[pe_id] = CS_WFO_IPC_PE_DEAD;
		return -1;
    }

	if ((pe_id == CS_WFO_IPC_PE0_CLNT_ID) || (pe_id == CS_WFO_IPC_PE1_CLNT_ID)) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: pe_id = %d CS_WFO_IPC_PE_ACTIVE\n", __func__, pe_id));

	    f_re_status[pe_id] = CS_WFO_IPC_PE_ACTIVE;

	    // Initial hmu watch table
	    if (!cs_wfo_init_hmu_watch_init) {
	        cs_wfo_init_hmu_watch();
	        cs_wfo_init_hmu_watch_init = 1;
	    }

		// send PCIe bar address if the PCIe address saved in IPC
		if (phy_addr[pe_id].pcie_phy_set) {
			cs_wfo_ipc_send_pcie_phy_addr(pe_id, phy_addr[pe_id].addr_mask,
							phy_addr[pe_id].pcie_phy_addr_start, phy_addr[pe_id].pcie_phy_addr_end);
		}
	}

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: START got here! pe_id = %d\n", __func__, pe_id));

	return 0;
} /* wfo_ipc_start_complete_callback */


static void wfo_ipc_stop_complete_callback( struct ipc_addr peer,
					                        u16 msg_no,
					                        const void *msg_data,
					                        u16 msg_size,
					                        struct ipc_context *context )
{
	u8 pe_id;
	cs_wfo_ipc_msg_t *p_msg = (cs_wfo_ipc_msg_t *)msg_data;

    if (cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return;
    }

	if (NULL == p_msg) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: p_msg fail\n", __func__));
		return;
	}

	pe_id = p_msg->pe_id;

	if ((pe_id == CS_WFO_IPC_PE0_CLNT_ID) || (pe_id == CS_WFO_IPC_PE1_CLNT_ID))
	    f_re_status[pe_id] = CS_WFO_IPC_PE_DEAD;

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: STOP got here! pe_id = %d\n", __func__, pe_id));

	return;
} /* wfo_ipc_stop_complete_callback */


// update 802.11 sequence control callback function
WFOUpdateSCCallback wfo_update_sc_cb = NULL;
void register_updateSC_callback(__u8 (*WFOUpdateSCCallback))
{
    wfo_update_sc_cb = WFOUpdateSCCallback;
    return;
} /* register_updateSC_callback */
EXPORT_SYMBOL(register_updateSC_callback);


static int wfo_ipc_message_complete_callback( struct ipc_addr peer,
					                          u16 msg_no,
					                          const void *msg_data,
					                          u16 msg_size,
					                          struct ipc_context *context )
{
	/* FIXME! implementation
	 * What do we do here? do we want to stop the running hash and
	 * change the status of sa? then at update complete, change the
	 * status back to active? */

	cs_wfo_ipc_msg_t *p_msg = (cs_wfo_ipc_msg_t *)msg_data;
	struct list_head *next;
	struct cs_wfo_ipc_del_802_11_queue *pdel_802_11_queue;

	if (!cs_wfo_enable) {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
		return -1;
	}

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: [%d] msg_size %d, sequence %d, status %d\n",
			__func__,  p_msg->pe_id, msg_size, p_msg->hdr.pe_msg_complete.wfo_ack_seq,
			p_msg->hdr.pe_msg_complete.wfo_status));

	if (p_msg->hdr.pe_msg_complete.wfo_status == CS_WFO_OP_SUCCESS ||
		p_msg->hdr.pe_msg_complete.wfo_status == CS_WFO_ENTRY_EXISTS ||
		p_msg->hdr.pe_msg_complete.wfo_status == CS_WFO_ENTRY_NOT_FOUND ) {

		wfo_cmd_resp_seq = p_msg->hdr.pe_msg_complete.wfo_ack_seq;

		/* Traverse all elements of the list */
		list_for_each(next, &cs_wfo_ipc_del_802_11_queue)
		{
			pdel_802_11_queue = (struct cs_wfo_ipc_del_802_11_queue*) list_entry(next, struct cs_wfo_ipc_del_802_11_queue, list);
			if(wfo_cmd_resp_seq == pdel_802_11_queue->wfo_cmd_seq)
			{
				IPC_WFO_DEBUG(printk("<IPC_WFO> pe_id %d, mac %pM, SC:[0x%4.4x 0x%4.4x 0x%4.4x 0x%4.4x 0x%4.4x 0x%4.4x 0x%4.4x 0x%4.4x] \n",
				pdel_802_11_queue->pe_id,
				p_msg->paras.resp_802_11_del.mac_da_address, p_msg->paras.resp_802_11_del.seq_control[0],
				p_msg->paras.resp_802_11_del.seq_control[1], p_msg->paras.resp_802_11_del.seq_control[2],
				p_msg->paras.resp_802_11_del.seq_control[3], p_msg->paras.resp_802_11_del.seq_control[4],
				p_msg->paras.resp_802_11_del.seq_control[5], p_msg->paras.resp_802_11_del.seq_control[6],
				p_msg->paras.resp_802_11_del.seq_control[7]));

				if (wifi_handler[pdel_802_11_queue->pe_id].valid == 1) {
					if (wfo_update_sc_cb != NULL) {
						wfo_update_sc_cb( wifi_handler[pdel_802_11_queue->pe_id].wfo_wifi_adatper_addr,
						p_msg->paras.resp_802_11_del.mac_da_address,
						&(p_msg->paras.resp_802_11_del.seq_control[0]) );
					}
				}

				// delete list and free resource
				list_del(&(pdel_802_11_queue->list));
				kfree(pdel_802_11_queue);
				break;
			}
		} /* list_for_each */

#ifdef G2_IPC_RETRY_LISY_SUPPORT
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: wfo_ipc_hlp_del_from_retry_list\n", __func__));
        //delay from retry list
        wfo_ipc_hlp_del_from_retry_list(p_msg);
#endif //G2_IPC_RETRY_LISY_SUPPORT
	}

	return 0;
} /* wfo_ipc_message_complete_callback */


struct g2_ipc_msg wfo_invoke_procs[] = {
	{CS_WFO_IPC_PE_START_COMPLETE,
	 (u32)wfo_ipc_start_complete_callback},
	{CS_WFO_IPC_PE_STOP_COMPLETE,
	 (u32)wfo_ipc_stop_complete_callback},
	{CS_WFO_IPC_PE_MESSAGE_COMPLETE,
	 (u32)wfo_ipc_message_complete_callback},
	{CS_WFO_RPC_MESSAGE,
	 (u32)wfo_ipc_rpc_asynccallback},
};

int cs_wfo_rpc( u8 pe_id,u8 msg_type, u8 *pkt_buf, u8 pkt_len )
{

	int rc;
	struct ipc_context *ipc_ctx = wfo_ipc_ctxt0;

// Just for test
#if 0
	// Does PE active
	if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
	    return -1;
	} /* if */
#endif

	if (NULL == pkt_buf) {
	   printk("<IPC_WFO> %s:: p_msg == NULL\n", __func__);
	   return -1;
	}

	if (pe_id == CS_WFO_IPC_PE1_CPU_ID) {
		ipc_ctx = wfo_ipc_ctxt1;
	}


	rc = g2_ipc_send( ipc_ctx,
	                  pe_id,
	 	          pe_id,
	 	          G2_IPC_HPRIO,
	 	          msg_type,
	 	          pkt_buf,
	 	          pkt_len );
	wfo_cmd_seq ++;
	//printk("<IPC_WFO> wfo_cmd_seq=%d msg=%d PE %d payload_size=%d\n", wfo_cmd_seq, msg_type, pe_id, pkt_len);

    	if (G2_IPC_OK == rc)
	        return 0;

	printk("<IPC_WFO> %s:%d pe_id %d failed\n", __FILE__, __LINE__, pe_id);
	return -1;

} /* cs_wfo_rpc */
EXPORT_SYMBOL(cs_wfo_rpc);

static int wfo_register_ipc(void)
{
	short status;

	status = g2_ipc_register(CS_WFO_IPC_PE0_CLNT_ID, wfo_invoke_procs,
				 4, 0, NULL, &wfo_ipc_ctxt0);
	if (status != G2_IPC_OK) {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: Failed to register IPC for Wi-Fi Offload PE0\n",
		       __func__));
		return -1;
	} else {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: successfully register IPC for PE0\n", __func__));
    }

	status = g2_ipc_register(CS_WFO_IPC_PE1_CLNT_ID, wfo_invoke_procs,
				 4, 0, NULL, &wfo_ipc_ctxt1);
	if (status != G2_IPC_OK) {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: Failed to register IPC for Wi-Fi Offload PE1\n",
		       __func__));
		return -1;
	} else {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: successfully register IPC for PE1\n", __func__));
    }

	return 0;
} /* wfo_register_ipc */


static void wfo_deregister_ipc(void)
{
	g2_ipc_deregister(wfo_ipc_ctxt0);
	g2_ipc_deregister(wfo_ipc_ctxt1);
	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: Done deregister IPC for Wi-Fi Offload\n", __func__));
} /* wfo_unregister_ipc */


static int wfo_ipc_send( u8 pe_id,
                         u8 msg_type,
                         u8 *p_msg_payload )
{
    int rc;
    cs_wfo_ipc_msg_t *p_msg;
    struct ipc_context *ipc_ctx = wfo_ipc_ctxt0;

    p_msg = (cs_wfo_ipc_msg_t *)p_msg_payload;

    if (p_msg == NULL) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: p_msg == NULL\n", __func__));
        return -1;
    }

    p_msg->pe_id = pe_id;
    if (pe_id == CS_WFO_IPC_PE1_CPU_ID) {
        ipc_ctx = wfo_ipc_ctxt1;
    }

    rc = g2_ipc_send( ipc_ctx,
                      pe_id,
    	              pe_id,
    	              G2_IPC_HPRIO,
    	              msg_type,
    	              p_msg,
    	              IPC_MSG_SIZE );

    if (G2_IPC_OK == rc)
        return 0;

    //IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: g2_ipc_send fail\n", __func__));
	return -1;
} /* wfo_ipc_send */

static int wfo_ipc_send_sync( u8 pe_id,
		u8 msg_type,
		u8 *p_msg_payload )
{
	int rc;
	cs_wfo_ipc_msg_t *p_msg;
	struct ipc_context *ipc_ctx = wfo_ipc_ctxt0;
	char buf[512];
	int size;

	p_msg = (cs_wfo_ipc_msg_t *)p_msg_payload;

	if (p_msg == NULL) {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: p_msg == NULL\n", __func__));
		return -1;
	}

	p_msg->pe_id = pe_id;
	if (pe_id == CS_WFO_IPC_PE1_CPU_ID) {
		ipc_ctx = wfo_ipc_ctxt1;
	}

	rc = g2_ipc_invoke( ipc_ctx,
			pe_id,
			pe_id,
			G2_IPC_HPRIO,
			msg_type,
			p_msg,
			IPC_MSG_SIZE,
			buf, &size);

	if (G2_IPC_OK == rc)
		return 0;

	return -1;
} /* wfo_ipc_send_sync */

static int wfo_ipc_wait_send_complete( u8 pe_id,
                                       u8 msg_type,
                                       u8 *pmsg )
{
#ifdef G2_IPC_RETRY_LISY_SUPPORT
    wfo_ipc_hlp_add_to_retry_list(pe_id, pmsg);
#endif //G2_IPC_RETRY_LISY_SUPPORT

    if (wfo_ipc_send(pe_id, CS_WFO_IPC_PE_MESSAGE, pmsg) == -1) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:%d pe_id %d failed\n", __FILE__, __LINE__, pe_id));
        return -1;
    } /* if */

    return 0;
} /* wfo_ipc_wait_send_complete */

int cs_wfo_ipc_wait_send_complete( u8 pe_id,
                                   u8 msg_type,
                                   u8 *p_msg_payload, u8 payload_size )
{

    int rc;
    cs_wfo_ipc_msg_t *p_msg;
    struct ipc_context *ipc_ctx = wfo_ipc_ctxt0;

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    p_msg = (cs_wfo_ipc_msg_t *)p_msg_payload;

	p_msg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    if (p_msg == NULL) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: p_msg == NULL\n", __func__));
        return -1;
    }

    p_msg->pe_id = pe_id;
    if (pe_id == CS_WFO_IPC_PE1_CPU_ID) {
        ipc_ctx = wfo_ipc_ctxt1;
    }

    rc = g2_ipc_send( ipc_ctx,
                      pe_id,
    	              pe_id,
    	              G2_IPC_HPRIO,
    	              msg_type,
    	              p_msg,
    	              payload_size );
	wfo_cmd_seq ++;
	IPC_WFO_DEBUG(printk("<IPC_WFO> wfo_cmd_seq=%d msg=%d PE %d payload_size=%d\n", wfo_cmd_seq, msg_type, pe_id, payload_size));

    if (G2_IPC_OK == rc)
        return 0;

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:%d pe_id %d failed\n", __FILE__, __LINE__, pe_id));
	return -1;
} /* cs_wfo_ipc_wait_send_complete */
EXPORT_SYMBOL(cs_wfo_ipc_wait_send_complete);

int cs_wfo_mac_da_hmu_watch_callback(u32 watch_bitmask, cs_core_hmu_value_t *value,
		u32 status)
{
	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: watch_bitmask 0x%8.8x, status 0x%x\n",
			__func__, watch_bitmask, status));

	/*if hash timeout becasue bridge lifetime,
	  we should not delete ipc for specific hash
	  if the specific hash create fail, it would not send ipc and this logic is impelemented
	  in cs_hw_accel_wfo.
	  */

	return 0;
} /* cs_wfo_mac_da_hmu_watch_callback */


void cs_wfo_init_hmu_watch(void)
{
	int ret;

	/* Register WFO MAC DA HMU watch callback function */
	memset(&wfo_mac_da_hmu_entry, 0, sizeof(wfo_mac_da_hmu_entry));
	memset(&wfo_mac_da_hmu_value, 0, sizeof(wfo_mac_da_hmu_value));

	wfo_mac_da_hmu_value.type = CS_CORE_HMU_WATCH_OUT_MAC_DA;
	wfo_mac_da_hmu_value.mask = 48;     // means watch 48 bits
	wfo_mac_da_hmu_entry.watch_bitmask = CS_CORE_HMU_WATCH_OUT_MAC_DA;
	wfo_mac_da_hmu_entry.value_mask = &wfo_mac_da_hmu_value;
	wfo_mac_da_hmu_entry.callback = cs_wfo_mac_da_hmu_watch_callback;
	ret = cs_core_hmu_register_watch(&wfo_mac_da_hmu_entry);

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: status %d\n", __func__, ret));

	return;
} /* cs_wfo_init_hmu_watch */


//=========================================================================================
// Global Functions
//
int cs_wfo_ipc_send_start( __u8 pe_id, __u8 *p_msg_payload )
{
    int                 status = -1;
    cs_wfo_ipc_msg_t    *pmsg = (cs_wfo_ipc_msg_t*)p_msg_payload;


    if (!cs_wfo_enable) {
//	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Enabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_ACTIVE) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_PE_ACTIVE\n", __func__));
        return 0;
    } /* if */

    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    status = wfo_ipc_send(pe_id, CS_WFO_IPC_PE_START, p_msg_payload);
//    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: pe_id %d, wfo_cmd_seq %d, status %d\n", __func__, pe_id, wfo_cmd_seq, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_send_start */
EXPORT_SYMBOL(cs_wfo_ipc_send_start);


int cs_wfo_ipc_send_stop( __u8 pe_id, __u8 *p_msg_payload )
{
    int                 status = -1;
    cs_wfo_ipc_msg_t    *pmsg = (cs_wfo_ipc_msg_t*)p_msg_payload;


    if (cs_wfo_enable) {
//	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_PE_DEAD\n", __func__));
        return 0;
    } /* if */

    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    status = wfo_ipc_send(pe_id, CS_WFO_IPC_PE_STOP, p_msg_payload);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: pe_id %d, wfo_cmd_seq %d, status %d\n", __func__, pe_id, wfo_cmd_seq, status));

    wfo_cmd_seq++;


	return status;
} /* cs_wfo_ipc_send_stop */
EXPORT_SYMBOL(cs_wfo_ipc_send_stop);


int cs_wfo_ipc_add_lookup_802_3( __u8 pe_id,
                                 __u8 *pmac_da_address,
                                 __u8 egress_port_id )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;


    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    // skip broadcast and multicast
    if ( is_multicast_ether_addr(pmsg->paras.cmd_802_11.mac_da_address) ||
         is_broadcast_ether_addr(pmsg->paras.cmd_802_11.mac_da_address) ) {
        return -1;
    }

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_3;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    memcpy(pmsg->paras.cmd_802_3.mac_da_address, pmac_da_address, 6);
    pmsg->paras.cmd_802_3.egress_port_id = egress_port_id;

    IPC_WFO_DEBUG(printk("IPC Add 802.3 PE %d MAC %pM\n", pe_id, pmac_da_address));

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_3 wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_3 status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_add_lookup_802_3 */
EXPORT_SYMBOL(cs_wfo_ipc_add_lookup_802_3);


void cs_wfo_dump_mac(__u8 mac[])
{
    if(mac)
        printk("MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
EXPORT_SYMBOL(cs_wfo_dump_mac);


void cs_wfo_dump_mac_with_comemnts(__u8 *p_comment, __u8 mac[])
{
    if(p_comment)
        printk("%s ", p_comment);
    if(mac)
        printk("MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
EXPORT_SYMBOL(cs_wfo_dump_mac_with_comemnts);


void cs_wfo_dump_frame(__u8 *p_comment, __u8 p_frame[], __u16 size)
{
    int i = 0;
    if(!p_frame || !size) return;

    if(p_comment)
        printk("\n%s Frame: %d\n", p_comment, size);

    for(i = 0; i < size; i++) {
        if(i && !(i % 8))
            printk("\n");
        printk("0x%02x ", p_frame[i]);
    }
}
EXPORT_SYMBOL(cs_wfo_dump_frame);


int cs_wfo_ipc_del_lookup_802_3( __u8 pe_id,
                                 __u8 *pmac_da_address )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;


    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: Enter\n", __func__));
    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: pe_id %d CS_WFO_IPC_PE_DEAD\n", __func__, pe_id));
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_3;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    memcpy(pmsg->paras.cmd_802_3.mac_da_address, pmac_da_address, 6);

    IPC_WFO_DEBUG(printk("IPC Del 802.3 PE %d %pM\n", pe_id, pmac_da_address));

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_3 wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_3 status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_del_lookup_802_3 */
EXPORT_SYMBOL(cs_wfo_ipc_del_lookup_802_3);


/* FIXME dp_type to be added to specify whether this entry is to forward
 * frames to Wifi or to GE */
int cs_wfo_ipc_add_lookup_802_11( __u8 pe_id,
                                  __u8 *phdr_802_11,
                                  __u8 hdr_802_11_len,
                                  __u8 prio,
                                  __u8 *pvendor_spec,
		                          __u8 frame_type )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    pheader_802_11      phdr_11=(pheader_802_11)phdr_802_11;
    int                 status = -1;


    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_11;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    if ((phdr_11->fc.toDs == 0) && (phdr_11->fc.frDs == 0)) {
        memcpy(pmsg->paras.cmd_802_11.mac_da_address, phdr_11->addr1, 6);
    } else if ((phdr_11->fc.toDs == 0) && (phdr_11->fc.frDs == 1)) {
        memcpy(pmsg->paras.cmd_802_11.mac_da_address, phdr_11->addr1, 6);
    } else if ((phdr_11->fc.toDs == 1) && (phdr_11->fc.frDs == 0)) {
        memcpy(pmsg->paras.cmd_802_11.mac_da_address, phdr_11->addr3, 6);
    } else {
        memcpy(pmsg->paras.cmd_802_11.mac_da_address, phdr_11->addr3, 6);
    }

    // skip broadcast and multicast
    if ( is_multicast_ether_addr(pmsg->paras.cmd_802_11.mac_da_address) ||
         is_broadcast_ether_addr(pmsg->paras.cmd_802_11.mac_da_address) ) {
        return -1;
    }

    //802.11 include header padding (4B aligned)
    memcpy(pmsg->paras.cmd_802_11.header_802_11, phdr_802_11, hdr_802_11_len);
    pmsg->paras.cmd_802_11.header_802_11_len = hdr_802_11_len; //len must include header padding
    pmsg->paras.cmd_802_11.prio = prio;
    pmsg->paras.cmd_802_11.starting_seq_control = phdr_11->sequence;
	pmsg->paras.cmd_802_11.chip_model = cs75xx_pni_get_chip_type(pe_id - CS_WFO_IPC_PE0_CPU_ID);

    //FIXME: vendor spec. (for RT3593 only)
    if (pmsg->paras.cmd_802_11.chip_model == CS_WFO_CHIP_RT3593) {
		IPC_WFO_DEBUG(printk("IPC Add 802.11 for CS_WFO_CHIP_RT3593\n"));
		// skip broadcast and multicast for RT3593 chip only
	    if ( is_multicast_ether_addr(pmsg->paras.cmd_802_11.mac_da_address) ||
    	     is_broadcast_ether_addr(pmsg->paras.cmd_802_11.mac_da_address) ) {
        	return -1;
	    }
		memcpy(&(pmsg->paras.cmd_802_11.vendor_spec.txwi), pvendor_spec, sizeof(rt3593_txwi_t));
    }

    IPC_WFO_DEBUG(printk("IPC Add 802.11 PE %d %pM\n", pe_id, pmsg->paras.cmd_802_11.mac_da_address));

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_11 wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_11 status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_add_lookup_802_11 */
EXPORT_SYMBOL(cs_wfo_ipc_add_lookup_802_11);


int cs_wfo_ipc_del_lookup_802_11( __u8 pe_id,
                                  __u8 *pmac_da_address,
                                  __u8 prio )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;
	struct cs_wfo_ipc_del_802_11_queue *pdel_802_11_queue;


    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_11;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    memcpy(pmsg->paras.cmd_802_11.mac_da_address, pmac_da_address, 6);
    pmsg->paras.cmd_802_11.prio = prio;

    IPC_WFO_DEBUG(printk("IPC Del 802.11 PE %d %pM\n", pe_id, pmac_da_address));

	// add wfo_cmd_seq into pdel_802_11_queue list
	pdel_802_11_queue = (struct cs_wfo_ipc_del_802_11_queue*)kmalloc(sizeof(struct cs_wfo_ipc_del_802_11_queue),
	        GFP_ATOMIC);
	if (pdel_802_11_queue) {
		pdel_802_11_queue->pe_id = pe_id;
		pdel_802_11_queue->wfo_cmd_seq = wfo_cmd_seq;
		list_add(&(pdel_802_11_queue->list), &cs_wfo_ipc_del_802_11_queue);
	} else {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: ERROR - allocate  cs_wfo_ipc_del_802_11_queue %d fail\n", __func__, wfo_cmd_seq));
	}

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_11 wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_11 status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_del_lookup_802_11 */
EXPORT_SYMBOL(cs_wfo_ipc_del_lookup_802_11);


int cs_wfo_ipc_update_rt3593_txwi( __u8 pe_id,
                                   __u8 *pmac_da_address,
                                   __u8 prio,
                                   rt3593_txwi_t *ptxwi )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

	if (pe_id == -1) {
		IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: pe_id is -1\n", __func__));
		return -1;
	}
    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_UPDATED_TXWI;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    memcpy(pmsg->paras.cmd_update_txwi.mac_da_address, pmac_da_address, 6);
    pmsg->paras.cmd_update_txwi.prio = prio;
    memcpy((u8*)&(pmsg->paras.cmd_update_txwi.txwi), (u8*)ptxwi, sizeof(rt3593_txwi_t));

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_UPDATED_TXWI wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_UPDATED_TXWI status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_update_rt3593_txwi */
EXPORT_SYMBOL(cs_wfo_ipc_update_rt3593_txwi);


int cs_wfo_ipc_pe_send_dump_fwtbl( __u8 pe_id )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_DUMP_FWTBL;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DUMP_FWTBL wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DUMP_FWTBL status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_pe_send_dump_fwtbl */
EXPORT_SYMBOL(cs_wfo_ipc_pe_send_dump_fwtbl);


void cs_wfo_ipc_pe_dump_fwtbl(__u8 pe_id)
{
    char *pAddr = (char *)CS_WFO_FWTBL_DMESG_BASE0;
    fwtbl_hdr_t *pfwtbl_hdr;
    fwtbl_rec_t *pfwtbl_rec;
    int i;

    if (pe_id == CS_WFO_IPC_PE1_CLNT_ID) {
        pAddr = (char *)CS_WFO_FWTBL_DMESG_BASE1;
    }

    pfwtbl_hdr = (fwtbl_hdr_t*)pAddr;
    if (pfwtbl_hdr->signature != CS_WFO_FWTBL_MAGIC) {
        printk("WFO PE%d FWTBL DUMP ERROR:: Signature\n", (pe_id-1));
        return;
    }

    printk("=========================================================\n");
//    printk("WFO PE%d FWTBL Header:: Signature 0x%8.8x\n", (pe_id-1), pfwtbl_hdr->signature);
//    printk("WFO PE%d FWTBL Header:: owner     %d\n", (pe_id-1), pfwtbl_hdr->owner);
    printk("WFO PE%d FWTBL Header:: tbl_size  %d\n", (pe_id-1), pfwtbl_hdr->tbl_size);

    if (pfwtbl_hdr->tbl_size) {
        pfwtbl_rec = (fwtbl_rec_t*)(pAddr+sizeof(fwtbl_hdr_t));
        for (i=0; i<pfwtbl_hdr->tbl_size; i++) {
            printk("---------------------------------------------------------\n");
            printk("WFO PE%d FWTBL Recoder:: entry_valid    %d\n", (pe_id-1), pfwtbl_rec->entry_valid);
            printk("WFO PE%d FWTBL Recoder:: wfo_lkup_type  %d\n", (pe_id-1), pfwtbl_rec->wfo_lkup_type);
            printk("WFO PE%d FWTBL Recoder:: mac_addr       %pM\n", (pe_id-1), pfwtbl_rec->mac_addr);
            printk("WFO PE%d FWTBL Recoder:: out_port       %d\n", (pe_id-1), pfwtbl_rec->out_port);
            printk("WFO PE%d FWTBL Recoder:: dp_type        %d\n", (pe_id-1), pfwtbl_rec->dp_type);
            pfwtbl_rec++;
        } /* for */
    } /* if */
    printk("=========================================================\n");

    return;
}/* cs_wfo_ipc_pe_dump_fwtbl */
EXPORT_SYMBOL(cs_wfo_ipc_pe_dump_fwtbl);


static const char* cs_pe_text[CS_WFO_IPC_CPU_ID_MAX] = {
	"",
	"PE0",
	"PE1",
};
void cs_wfo_ipc_pe_get_mibs(cs_mib_pe_s *pmib, __u8 pe_id)
{
    cs_mib_pe_s *pmib_rram1 = NULL;
    
    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return;
    }

//++Bug#40475
    //Bug#40475 pmib = (cs_mib_pe_s *)CS_WFO_MIB_PHY_ADDR;
    if ( pe_id == CS_WFO_IPC_PE0_CPU_ID) {
        pmib_rram1 = (cs_mib_pe_s *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE0);
    } else {
        pmib_rram1 = (cs_mib_pe_s *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE1);
    }
        
    if (!pmib_rram1) {
        printk("(%s, %d)ERROR: MIBs share memory, check PE!!\n", __func__, __LINE__);
        return;
    }
    printk("WFO MIBs:: %s PCIeRxFrameCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxFrameCount);
    printk("WFO MIBs:: %s PCIeTxFrameCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxFrameCount);
    printk("WFO MIBs:: %s PCIeRxByteCount       0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxByteCount);
    printk("WFO MIBs:: %s PCIeTxByteCount       0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxByteCount);
    printk("WFO MIBs:: %s PCIeTxErrorCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxErrorCount);
    printk("WFO MIBs:: %s PCIeTxPEFrameCount    0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxPEFrameCount);
    printk("WFO MIBs:: %s PCIeDropFrameCount    0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeDropFrameCount);
    printk("WFO MIBs:: %s PCIeRxEOLCount        0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxEOLCount);
    printk("WFO MIBs:: %s PCIeRxORNCount        0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxORNCount);
    printk("WFO MIBs:: %s PCIeRxsErrorCount     0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxsErrorCount);
    printk("WFO MIBs:: %s PCIeTxsErrorCount     0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxsErrorCount);
    printk("WFO MIBs:: %s PCIeRxBcastFrameCount 0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxBcastFrameCount);
    printk("WFO MIBs:: %s PCIeTxBcastFrameCount 0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxBcastFrameCount);
    printk("WFO MIBs:: %s PCIeRxMcastFrameCount 0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeRxMcastFrameCount);
    printk("WFO MIBs:: %s PCIeTxMcastFrameCount 0x%8.8x\n\n", cs_pe_text[pe_id], pmib_rram1->pe_pci.PCIeTxMcastFrameCount);

    printk("WFO MIBs:: %s NIRxGEFrameCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRxGEFrameCount);
    printk("WFO MIBs:: %s NIRxA9FrameCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRxA9FrameCount);
    printk("WFO MIBs:: %s NITxGEFrameCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NITxGEFrameCount);
    printk("WFO MIBs:: %s NITxA9FrameCount      0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NITxA9FrameCount);
    printk("WFO MIBs:: %s NIRxGEByteCount       0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRxGEByteCount);
    printk("WFO MIBs:: %s NIRxA9ByteCount       0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRxA9ByteCount);
    printk("WFO MIBs:: %s NITxGEByteCount       0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NITxGEByteCount);
    printk("WFO MIBs:: %s NITxA9ByteCount       0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NITxA9ByteCount);
    printk("WFO MIBs:: %s NIRxGEDropFrameCount  0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRXGEDropFrameCount);
    printk("WFO MIBs:: %s NIRxA9DropFrameCount  0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRXA9DropFrameCount);
    printk("WFO MIBs:: %s NIRxBcastFrameCount   0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRxBcastFrameCount);
    printk("WFO MIBs:: %s NITxBcastFrameCount   0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NITxBcastFrameCount);
    printk("WFO MIBs:: %s NIRxMcastFrameCount   0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NIRxMcastFrameCount);
    printk("WFO MIBs:: %s NITxMcastFrameCount   0x%8.8x\n\n", cs_pe_text[pe_id], pmib_rram1->pe_ni.NITxMcastFrameCount);

    printk("WFO MIBs:: %s IPCRcvCnt             0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ipc.IpcRcvCnt);
    printk("WFO MIBs:: %s IPCRspCnt             0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ipc.IpcRspCnt);
    printk("WFO MIBs:: %s FifoStatus            0x%8.8x\n", cs_pe_text[pe_id], readl(RECIRC_TOP_RECPU_RX_CRYPTO_DST_FF_STS));
    printk("WFO MIBs:: %s NIXferCnt             0x%8.8x\n", cs_pe_text[pe_id], readl(RECIRC_TOP_RECPU_CRYPTO_TX_PACKET_COUNT));
    printk("WFO MIBs:: %s BusyAccum             0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ipc.BusyAccum);
    printk("WFO MIBs:: %s CurCcount             0x%8.8x\n", cs_pe_text[pe_id], pmib_rram1->pe_ipc.CurCcount);
	printk("WFO MIBs:: %s CPU usage             %d%%\n", cs_pe_text[pe_id], pmib_rram1->pe_ipc.BusyAccum /
		((pmib_rram1->pe_ipc.CurCcount/100)?:1));

	if (pmib) {
	    memcpy(pmib, pmib_rram1, sizeof(cs_mib_pe_s));
	}
//--Bug#40475
	
	return;
} /* cs_wfo_ipc_pe_get_mibs */
EXPORT_SYMBOL(cs_wfo_ipc_pe_get_mibs);

void cs_wfo_ipc_pe_set_mibs( cs_mib_pe_s *pmib, __u8 pe_id)
{
    cs_mib_pe_s *pmib_rram1 = NULL;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return;
    }

//++Bug#40475
//    pmib = (cs_mib_pe_s *)CS_WFO_MIB_PHY_ADDR;
    if ( pe_id == CS_WFO_IPC_PE0_CPU_ID) {
        pmib_rram1 = (cs_mib_pe_s *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE0);
    } else {
        pmib_rram1 = (cs_mib_pe_s *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE1);
    }
        
    if (!pmib_rram1) {
        printk("(%s, %d)ERROR: MIBs share memory, check PE!!\n", __func__, __LINE__);
        return;
    }

    memset((void *)&pmib_rram1->pe_pci, 0, sizeof(cs_mib_pcie_t));
    memset((void *)&pmib_rram1->pe_ni, 0, sizeof(cs_mib_ni_t));
    memset((void *)&pmib_rram1->pe_ipc, 0, sizeof(cs_mib_ipc_t));
//--Bug#40475
    return;
} /* cs_wfo_ipc_pe_set_mibs */
EXPORT_SYMBOL(cs_wfo_ipc_pe_set_mibs);

void cs_wfo_ipc_pe_get_logs( void *plog, __u8 pe_id )
{
	int i, off;
	char *ptr;
    unsigned  int *p_value;
    
    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return;
    }

    plog = (void *)CS_WFO_LOG_PHY_ADDR;
    if (pe_id == CS_WFO_IPC_PE1_CLNT_ID) {
        plog += CS_WFO_LOG_BUF_SIZE;
    }

	/* Check magic */
	p_value = (unsigned int*)plog;
	if (*p_value != CS_PRINT_MAGIC) {
		printk("Magic is not match\n");
		return;
	}

	off = *(p_value+1);
	ptr = p_value+3;

	while(ptr < (char *)p_value + off) {
		printk("%c", *ptr);
		ptr++;
	}
//	if ((argc == 3) && ((strcmp(argv[2], "-c") == 0))) {
//		/* Clean buffer */
//		ptr = p_value+3;
//		*ptr = 0;
//		*(p_value+1) = 12;
//	}

	return;
} /* cs_wfo_ipc_pe_get_logs */
EXPORT_SYMBOL(cs_wfo_ipc_pe_get_logs);


int cs_wfo_ipc_pe_clear_mib( __u8 pe_id, __u8 mib_type )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;


    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_CLR_MIB;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    pmsg->paras.cmd_clear_mib.mib_type = mib_type;

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_CLR_MIB wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_CLR_MIB status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_pe_clear_mib */
EXPORT_SYMBOL(cs_wfo_ipc_pe_clear_mib);


int cs_wfo_ipc_pe_clear_err_log(__u8 pe_id)
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_PE_CLR_ERR_LOG;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_PE_CLR_ERR_LOG wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_PE_CLR_ERR_LOG status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_pe_clear_err_log */
EXPORT_SYMBOL(cs_wfo_ipc_pe_clear_err_log);


int cs_wfo_ipc_pe_start_wfo(__u8 pe_id)
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_PE_START_WFO;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_PE_START_WFO wfo_cmd_seq %d\n", __func__, wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_PE_START_WFO status = %d\n", __func__, status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_pe_start_wfo */
EXPORT_SYMBOL(cs_wfo_ipc_pe_start_wfo);


int cs_wfo_ipc_send_pcie_phy_addr( __u8 pe_id,
                                   __u8 valid_addr_mask,
                                   __u32 addr_start[],
                                   __u32 addr_end[] )
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;
    cs_wfo_ipc_msg_t    *pmsg = &wfo_ipc_msg;
    int                 status = -1, i;


    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

    // Does PE active
    if (f_re_status[pe_id] == CS_WFO_IPC_PE_DEAD) {
		for (i=0; i<6; i++) {
			phy_addr[pe_id].pcie_phy_addr_start[i] = addr_start[i];
			phy_addr[pe_id].pcie_phy_addr_end[i]   = addr_end[i];
		}
		phy_addr[pe_id].addr_mask = valid_addr_mask;
		phy_addr[pe_id].pcie_phy_set = 1;

        return -1;
    } /* if */

    memset(pmsg, 0, sizeof(cs_wfo_ipc_msg_t));
    pmsg->pe_id = pe_id;
    pmsg->hdr.pe_msg.wfo_cmd = CS_WFO_IPC_MSG_CMD_SEND_PCIE_PHY_ADDR;
    pmsg->hdr.pe_msg.wfo_cmd_seq = wfo_cmd_seq;
    pmsg->paras.cmd_pcie_phy_addr.valid_addr_mask = valid_addr_mask;

    for (i=0; i<6; i++) {
        if (valid_addr_mask & 0x01) {
            pmsg->paras.cmd_pcie_phy_addr.pcie_phy_addr_start[i] = addr_start[i];
            pmsg->paras.cmd_pcie_phy_addr.pcie_phy_addr_end[i] = addr_end[i];
        }
        valid_addr_mask >>= 1;
    }


    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_SEND_PCIE_PHY_ADDR start 0x%4.4x, end 0x%4.4x wfo_cmd_seq %d\n",
        __func__, addr_start[0], addr_end[0], wfo_cmd_seq));
    status = wfo_ipc_wait_send_complete(pe_id, CS_WFO_IPC_PE_MESSAGE, (u8*)pmsg);
    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_SEND_PCIE_PHY_ADDR start 0x%4.4x, end 0x%4.4x status = %d\n",
        __func__, addr_start[0], addr_end[0], status));

    wfo_cmd_seq++;

	return status;
} /* cs_wfo_ipc_send_pcie_phy_addr */
EXPORT_SYMBOL(cs_wfo_ipc_send_pcie_phy_addr);


void cs_wfo_del_hash_by_mac_da(__u8 *mac)
{
    int ret;
	int i;
	cs_core_hmu_value_t hmu_value;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return;
    }

	hmu_value.type = CS_CORE_HMU_WATCH_IN_MAC_SA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = mac[5 - i];
	hmu_value.mask = 0;

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_IN_MAC_SA, &hmu_value);


	hmu_value.type = CS_CORE_HMU_WATCH_OUT_MAC_DA;
	for (i = 0; i < 6; i++)
		hmu_value.value.mac_addr[i] = mac[5 - i];

	cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_OUT_MAC_DA, &hmu_value);

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: cs_core_hmu_delete_hash %d\n", __func__, ret));

	return;
} /* cs_wfo_del_hash_by_mac_da */
EXPORT_SYMBOL(cs_wfo_del_hash_by_mac_da);


void cs_wfo_ipc_set_wifi_adatper_addr( __u8 pe_id,
                                       void* pAd )
{
    wifi_handler[pe_id].wfo_wifi_adatper_addr = pAd;
    wifi_handler[pe_id].valid = 1;

    return;
} /* cs_wfo_ipc_set_wifi_adatper_addr */
EXPORT_SYMBOL(cs_wfo_ipc_set_wifi_adatper_addr);


void cs_wfo_ipc_send_start_stop_command(__u8 cmd)
{
    cs_wfo_ipc_msg_t    wfo_ipc_msg;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
    } else {
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Enabled\n", __func__));
    }

    switch (cmd) {
        case CS_WFO_IPC_PE_START:
            if (f_re_status[CS_WFO_IPC_PE0_CPU_ID] == CS_WFO_IPC_PE_DEAD) {
                cs_wfo_ipc_send_start(CS_WFO_IPC_PE0_CPU_ID, (u8*)(&wfo_ipc_msg));
            } /* if */

            if (f_re_status[CS_WFO_IPC_PE1_CPU_ID] == CS_WFO_IPC_PE_DEAD) {
                cs_wfo_ipc_send_start(CS_WFO_IPC_PE1_CPU_ID, (u8*)(&wfo_ipc_msg));
            } /* if */
            break;

        case CS_WFO_IPC_PE_STOP:
            if (f_re_status[CS_WFO_IPC_PE0_CPU_ID] == CS_WFO_IPC_PE_ACTIVE) {
                cs_wfo_ipc_send_stop(CS_WFO_IPC_PE0_CPU_ID, (u8*)(&wfo_ipc_msg));
            } /* if */

            if (f_re_status[CS_WFO_IPC_PE1_CPU_ID] == CS_WFO_IPC_PE_ACTIVE) {
                cs_wfo_ipc_send_stop(CS_WFO_IPC_PE1_CPU_ID, (u8*)(&wfo_ipc_msg));
            } /* if */
            break;
    }

	return;
} /* cs_wfo_ipc_send_start_stop_command */
EXPORT_SYMBOL(cs_wfo_ipc_send_start_stop_command);


//=========================================================================================
// Driver Functions
//
static long wfo_ipc_ioctl( struct file *file,
                           unsigned int cs_magic,
                           unsigned long arg )
{
    cs_wfo_ipc_ioctl_t *pioctl_hdr = (cs_wfo_ipc_ioctl_t*)arg;
    int i;
    __u8 valid_addr_mask;
    cs_mib_pe_s *pmib = NULL;
    void *plog = NULL;

    if (!cs_wfo_enable) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: WFO Disabled\n", __func__));
	    return -1;
    }

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: Enter\n", __func__));

	if (cs_magic != CS_WFO_IPC_MAGIC) {
	    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: NOT my IOCTL\n", __func__));
	    return -1;
	}

	switch (pioctl_hdr->cmd) {
	    case CS_WFO_IPC_PE_START:
	        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: IOCTL CS_WFO_IPC_PE_START\n", __func__));
	        cs_wfo_ipc_send_start(pioctl_hdr->pmsg.pe_id, (u8*)(&(pioctl_hdr->pmsg)));
	        break;

	    case CS_WFO_IPC_PE_STOP:
	        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: IOCTL CS_WFO_IPC_PE_STOP\n", __func__));
	        cs_wfo_ipc_send_stop(pioctl_hdr->pmsg.pe_id, (u8*)(&(pioctl_hdr->pmsg)));
	        break;

	    case CS_WFO_IPC_PE_MESSAGE:
	        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: IOCTL CS_WFO_IPC_PE_MESSAGE\n", __func__));
	        switch(pioctl_hdr->pmsg.hdr.pe_msg.wfo_cmd) {
	            case CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_3:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_3\n", __func__));
	                cs_wfo_ipc_add_lookup_802_3( pioctl_hdr->pmsg.pe_id,
	                                             pioctl_hdr->pmsg.paras.cmd_802_3.mac_da_address,
	                                             pioctl_hdr->pmsg.paras.cmd_802_3.egress_port_id );
	                break;

	            case CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_3:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_3\n", __func__));
	                cs_wfo_del_mac_entry( pioctl_hdr->pmsg.paras.cmd_802_3.mac_da_address,
	                                      WFO_MAC_TYPE_802_3,
	                                      pioctl_hdr->pmsg.pe_id );
	                break;

	            case CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_11:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_11\n", __func__));
                    cs_wfo_ipc_add_lookup_802_11( pioctl_hdr->pmsg.pe_id,
                                                  pioctl_hdr->pmsg.paras.cmd_802_11.header_802_11,
                                                  pioctl_hdr->pmsg.paras.cmd_802_11.header_802_11_len,
                                                  pioctl_hdr->pmsg.paras.cmd_802_11.prio,
                                                  pioctl_hdr->pmsg.paras.cmd_802_11.vendor_spec.reserved,
					                              4); // temporarily hardcoded for RT AMPDU type.
	                break;

	            case CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_11:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_11\n", __func__));
	                cs_wfo_del_mac_entry( pioctl_hdr->pmsg.paras.cmd_802_3.mac_da_address,
	                                      WFO_MAC_TYPE_802_11,
	                                      pioctl_hdr->pmsg.pe_id );
	                break;

	            case CS_WFO_IPC_MSG_CMD_UPDATED_TXWI:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_UPDATED_TXWI\n", __func__));
	                cs_wfo_ipc_update_rt3593_txwi( pioctl_hdr->pmsg.pe_id,
                                                   pioctl_hdr->pmsg.paras.cmd_update_txwi.mac_da_address,
                                                   pioctl_hdr->pmsg.paras.cmd_update_txwi.prio,
                                                   &(pioctl_hdr->pmsg.paras.cmd_update_txwi.txwi) );

	                break;

	            case CS_WFO_IPC_MSG_CMD_CLR_MIB:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_CLR_MIB\n", __func__));
	                cs_wfo_ipc_pe_clear_mib(pioctl_hdr->pmsg.pe_id, pioctl_hdr->pmsg.paras.cmd_clear_mib.mib_type);
	                break;

	            case CS_WFO_IPC_MSG_CMD_PE_CLR_ERR_LOG:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_PE_CLR_ERR_LOG\n", __func__));
	                cs_wfo_ipc_pe_clear_err_log(pioctl_hdr->pmsg.pe_id);
	                break;

	            case CS_WFO_IPC_MSG_CMD_PE_START_WFO:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_PE_START_WFO\n", __func__));
	                cs_wfo_ipc_pe_start_wfo(pioctl_hdr->pmsg.pe_id);
	                break;

	            case CS_WFO_IPC_MSG_CMD_SEND_PCIE_PHY_ADDR:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_SEND_PCIE_PHY_ADDR, "
	                    "pe_id 0x%2.2x, valid_addr_mask 0x%x\n", __func__,
	                    pioctl_hdr->pmsg.pe_id, pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.valid_addr_mask));

	                valid_addr_mask =  pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.valid_addr_mask;
	                for (i=0; i<6; i++) {
	                    if (valid_addr_mask & 0x01) {
	                        IPC_WFO_DEBUG(printk("      [%d] pcie_phy_addr_start 0x%4.4x, pcie_phy_addr_end 0x%4.4x\n",
	                            i,
                                pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.pcie_phy_addr_start[i],
                                pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.pcie_phy_addr_end[i]));
	                    }
	                    valid_addr_mask >>= 1;
	                }

	                cs_wfo_ipc_send_pcie_phy_addr( pioctl_hdr->pmsg.pe_id,
	                                               pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.valid_addr_mask,
	                                               pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.pcie_phy_addr_start,
	                                               pioctl_hdr->pmsg.paras.cmd_pcie_phy_addr.pcie_phy_addr_end );
	                break;

	            case CS_WFO_IPC_MSG_CMD_DUMP_FWTBL:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_IPC_MSG_CMD_DUMP_FWTBL\n", __func__));
                    cs_wfo_ipc_pe_send_dump_fwtbl(pioctl_hdr->pmsg.pe_id);
                    cs_wfo_ipc_pe_dump_fwtbl(pioctl_hdr->pmsg.pe_id);
	                break;

	            case CS_WFO_GET_MIB_COUNTER:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_GET_MIB_COUNTER\n", __func__));
                    cs_wfo_ipc_pe_get_mibs(pmib, pioctl_hdr->pmsg.pe_id);
	                break;

	            case CS_WFO_GET_CRITICAL_LOGS:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_GET_CRITICAL_LOGS\n", __func__));
                    cs_wfo_ipc_pe_get_logs(plog, pioctl_hdr->pmsg.pe_id);
	                break;

	            case CS_WFO_DUMP_A9_MAC_ENTRY:
	                IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: CS_WFO_DUMP_A9_MAC_ENTRY\n", __func__));
                    cs_wfo_dump_mac_entry();
	                break;

	        } /* switch */
	        break;

	    case CS_WFO_DEL_HWFWD:
	        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: IOCTL CS_WFO_DEL_HWFWD\n", __func__));
	        cs_wfo_del_hash_by_mac_da(pioctl_hdr->pmsg.paras.cmd_802_11.mac_da_address);
	        break;
	} /* switch (cmd) */

	IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: Left\n", __func__));
    return 0;
}/* wfo_ipc_ioctl */


static int wfo_ipc_open(struct inode *inode, struct file *file)
{
    return 0;
}/* wfo_ipc_open */

static int wfo_ipc_release( struct inode *inode, struct file *file )
{
    return 0;
}/* wfo_ipc_release */

static struct file_operations wfo_ipc_fops =
{
    .owner          = THIS_MODULE,
    .unlocked_ioctl = wfo_ipc_ioctl,
    .open           = wfo_ipc_open,
    .release        = wfo_ipc_release,
};

#define WFO_MINOR    246     // Documents/devices.txt suggest to use 240~255 for local driver!!
static struct miscdevice wfo_ipc_miscdev =
{
    .minor  = WFO_MINOR,
    .name   = "wfo_ipc",
    .fops   = &wfo_ipc_fops,
};


static int __init cs_wfoipc_init(void)
{
    int     ret = 0, i;
	int     status_c;

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: INIT/START: cs_wfoipc_init enter\n", __func__));

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: wfo_register_ipc...\n", __func__));
	status_c = wfo_register_ipc();
    if (status_c != 0) {
        IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: ERROR: wfo_register_ipc: status_c %d\n", __func__, status_c));
    }

    for (i=0; i<CS_WFO_IPC_CLNT_ID_MAX; i++) {
        f_re_status[i] = CS_WFO_IPC_PE_DEAD;
    }

    ret = misc_register(&wfo_ipc_miscdev);

    // Init mac_table_802_3 and mac_table_802_11
    cs_wfo_mac_table_init();

	/* init pppoe session id - guid mapping list */
	INIT_LIST_HEAD(&cs_wfo_ipc_del_802_11_queue);

//    // Add timer to send CS_WFO_IPC_PE_START command
//	init_timer(&wfo_ipc_start_timer);
//
//	wfo_ipc_start_timer.expires = jiffies + secs_to_cputime(WFO_IPC_START_TIMER_PERIOD);
//	wfo_ipc_start_timer.function = &cs_wfo_ipc_start_timer_func;
//    add_timer(&wfo_ipc_start_timer);


#ifdef G2_IPC_RETRY_LISY_SUPPORT
    // Add timer for IPC retry
	init_timer(&wfo_ipc_retry_timer);
	wfo_ipc_retry_timer.expires = jiffies + secs_to_cputime(G2_IPC_WFO_SEND_TIMEOUT);
	wfo_ipc_retry_timer.function = &cs_wfo_ipc_retry_timer_func;
    add_timer(&wfo_ipc_retry_timer);
#endif //G2_IPC_RETRY_LISY_SUPPORT


	// Clear local phy_addr[]
	memset((void*)(&phy_addr[0]), 0, sizeof(struct phy_addr_s)*CS_WFO_IPC_CLNT_ID_MAX);

	// Clear Wi-Fi driver adapter information block
	memset((void*)(&wifi_handler[0]), 0, sizeof(struct wifi_adapter_addr_s)*CS_WFO_IPC_CLNT_ID_MAX);

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: INIT/START: cs_wfoipc_init left, ret %d\n", __func__, ret));

    return ret;
}//end cs_wfoipc_init()

static void __exit cs_wfoipc_exit(void)
{
	/* Unregister PPPoE HMU watch callback function */
	cs_core_hmu_unregister_watch(&wfo_mac_da_hmu_entry);

    misc_deregister(&wfo_ipc_miscdev);

    IPC_WFO_DEBUG(printk("<IPC_WFO> %s:: wfo_deregister_ipc...\n", __func__));
	wfo_deregister_ipc();
    return;
}//end cs_wfoipc_exit()

module_init(cs_wfoipc_init);
module_exit(cs_wfoipc_exit);

MODULE_DESCRIPTION("Module: Cortina-Systems Wi-Fi Offload IPC Driver.");
MODULE_LICENSE("GPL");
