/* For PE RPC testing.
 *
 *
 *
 */


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

/* RPC Frame formats */
/* |--------------||-------------|
 * RPC Header      RPC Payload
 *
 * 1) RPC Header:
 * |-------|--------|----------------|
 * 31      23       15               0
 * Type     Session  Transaction ID
 * = 0 Data
 * = 1 Return
 * = 2 Mgn
 *
 * 2) payload
 * Data and Return RPC payload is RPC all dependent
 *
 * Management frame formats:
 * |--------|--------|--------|--------|
 * Byte 0       4        8        12
 * Header     Action   Version  Reason
 *
 * Version is included only for following actions:
 * -- CONNECT
 * -- RESET
 * -- DOWN
 * -- CONNECT_ACK
 * -- CONNECT_NACK
 *
 * Reason sent only by BMAC for following actions:
 * -- CONNECT_ACK
 * -- CONNECT_NACk
 */
typedef u32 rpc_header_t;

#define BCM_RPC_TP_ENCAP_LEN	4	/* TP header is 4 bytes */

#define RPC_HDR_LEN		sizeof(rpc_header_t)
#define RPC_ACN_LEN		sizeof(u32)
#define RPC_VER_LEN		sizeof(u32)
#define RPC_RC_LEN		sizeof(u32)
#define RPC_CHIPID_LEN		sizeof(u32)

#define RPC_HDR_TYPE(_rpch) 	(((_rpch) >> 24) & 0xff)
#define RPC_HDR_SESSION(_rpch) 	(((_rpch) >> 16) & 0xff)
#define RPC_HDR_XACTION(_rpch) 	((_rpch) & 0xffff) /* When the type is data or return */

#define BCMSWAP32(val) \
		((u32)((((u32)(val) & (u32)0x000000ffU) << 24) | \
		(((u32)(val) & (u32)0x0000ff00U) <<  8) | \
		(((u32)(val) & (u32)0x00ff0000U) >>  8) | \
		(((u32)(val) & (u32)0xff000000U) >> 24)))

/* RPC Header defines -- attached to every RPC call */
typedef enum {
	RPC_TYPE_UNKNOWN, /* Unknown header type */
	RPC_TYPE_DATA,	  /* RPC call that go straight through */
	RPC_TYPE_RTN,	  /* RPC calls that are syncrhonous */
	RPC_TYPE_MGN,	  /* RPC state management */
} rpc_type_t;

typedef enum {
	RPC_RC_ACK =  0,
	RPC_RC_HELLO,
	RPC_RC_RECONNECT,
	RPC_RC_VER_MISMATCH
} rpc_rc_t;

/* Management actions */
typedef enum {
	RPC_NULL = 0,
	RPC_HELLO,
	RPC_CONNECT,		/* Master (high) to slave (low). Slave to copy current
				 * session id and transaction id (mostly 0)
				 */
	RPC_CONNECT_ACK,	/* Ack from LOW_RPC */
	RPC_DOWN,		/* Down the other-end. The actual action is
				 * end specific.
				 */
	RPC_CONNECT_NACK,	/* Nack from LOW_RPC. This indicates potentially that
				 * dongle could already be running
				 */
	RPC_RESET		/* Resync using other end's session id (mostly HIGH->LOW)
				 * Also, reset the oe_trans, and trans to 0
				 */
} rpc_acn_t;

/* RPC States */
typedef enum {
	UNINITED = 0,
	WAIT_HELLO,
	HELLO_RECEIVED,
	WAIT_INITIALIZING,
	ESTABLISHED,
	DISCONNECTED,
	ASLEEP,
	WAIT_RESUME
} rpc_state_t;

#define	HDR_STATE_MISMATCH	0x1
#define	HDR_SESSION_MISMATCH	0x2
#define	HDR_XACTION_MISMATCH	0x4

#define BRCM_MAX_PKT_LEN	0x40


int  wfo_ipc_rpc_asynccallback(	struct ipc_addr peer,
				u16 msg_no,
				const void *msg_data,
				u16 msg_size,
				struct ipc_context *context )
{
	u32 rpc_encap,rpc_header,rpc_mgn_frame,header_type;
	u32 action,i;
	u8  *data = (u8 *)msg_data;

	printk("wfo_ipc_rpc_asynccallback %x %x\r\n",msg_no,msg_size);
	for(i=0;i<16;i++)
		printk("data[%x]=%x ",i,data[i]);

}

int  wfo_ipc_rpc_synccallback(struct ipc_addr peer, cs_uint16 msg_no,
			const void *msg_data,
			cs_uint16 msg_size,
			void *result_data,
			cs_uint16 *result_data_size,
			struct ipc_context* context )
{
	u32 rpc_encap,rpc_header,rpc_mgn_frame,header_type;
	u32 action,i;
	u8  *data = (u8 *)msg_data;

	printk("wfo_ipc_rpc_synccallback %x %x\r\n",msg_no,msg_size);
	for(i=0;i<16;i++)
		printk("data[%x]=%c ",i,data[i]);

	rpc_encap  = ((u32)data[3]<<24)|((u32)data[2]<<16)|((u32)data[1]<<8)|((u32)data[0]);
	rpc_header = ((u32)data[7]<<24)|((u32)data[6]<<16)|((u32)data[5]<<8)|((u32)data[4]);

	header_type = RPC_HDR_TYPE(rpc_header);
	printk("wfo_ipc_rpc_response type:%x \r\n",header_type);


	switch(header_type)
	{
		case RPC_TYPE_MGN:
		{
			data = &msg_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
			action = ((u32)data[3]<<24)|((u32)data[2]<<16)|((u32)data[1]<<8)|((u32)data[0]);
			printk("wfo_ipc_rpc_response action %x\r\n",action);
			if(RPC_CONNECT == action){
				memcpy(result_data,msg_data,BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN+RPC_ACN_LEN+RPC_VER_LEN+RPC_RC_LEN);
				data = &result_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
				memset(data,0,RPC_ACN_LEN);
				data[0] = RPC_CONNECT_ACK&0xff;
			}else if (RPC_RESET == action){
				memcpy(result_data,msg_data,BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN+RPC_ACN_LEN+RPC_VER_LEN+RPC_RC_LEN);
				data = &result_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
				memset(data,0,RPC_ACN_LEN);
				data[0] = RPC_CONNECT_ACK&0xff;
			}
		}
		break;
		case RPC_TYPE_RTN:
		{
			printk("RPC_CALL with return \r\n");
			data = &msg_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
			action = ((u32)data[3]<<24)|((u32)data[2]<<16)|((u32)data[1]<<8)|((u32)data[0]);
			memcpy(result_data,msg_data,BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN+RPC_ACN_LEN+RPC_VER_LEN+RPC_RC_LEN);
			data = &result_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
			data[0] = 0xde; data[1] = 0xad; data[2] = 0xbe; data[3] = 0xef;
		}
		break;
		case RPC_TYPE_DATA:
		{
			printk("RPC_CALL \r\n");
#ifdef BRCM_TEST_REORDER_QUEUE
			data = &msg_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
			action = ((u32)data[3]<<24)|((u32)data[2]<<16)|((u32)data[1]<<8)|((u32)data[0]);
			memcpy(result_data,msg_data,BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN);
			// Set wrong transaction ID.
			data = &result_data[BCM_RPC_TP_ENCAP_LEN];
			data[1] = 0x00;//transaction ID
			data[0] = 0x03;//transaction ID
#endif
		}
		break;
		default:
		{
			//Just for test!
			printk("%s ",&data[4]);

			// We need send connect request to PE.
			data = &result_data[BCM_RPC_TP_ENCAP_LEN];
			data[0] = RPC_TYPE_MGN;
			data[1] = data[2] = data[3] = 0;

			data = &result_data[BCM_RPC_TP_ENCAP_LEN+RPC_HDR_LEN];
			data[0] = RPC_CONNECT;

		}
		break;
	}

}


