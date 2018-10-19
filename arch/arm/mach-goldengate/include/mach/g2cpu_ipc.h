#ifndef __G2_IPC_H_RFLEE_2010_08_16_RF1012208E36FF__
#define __G2_IPC_H_RFLEE_2010_08_16_RF1012208E36FF__

#include <mach/cs_types.h>


#define	G2_IPC_OK		0x00
#define	G2_IPC_ETIMEOUT		0x01
#define	G2_IPC_EINVAL		0x02
#define	G2_IPC_EEXIST		0x03
#define	G2_IPC_ENOMEM		0x04
#define	G2_IPC_ENOCLIENT	0x05
#define	G2_IPC_EQFULL		0x06
#define	G2_IPC_EINTERNAL	0x07

// Message Priority
#define G2_IPC_LPRIO 0x0
#define G2_IPC_HPRIO 0x1

// CPU_ID
#define CPU_ARM     0x0
#define CPU_RCPU0   0x1
#define CPU_RCPU1   0x2

struct ipc_addr {
	cs_uint8	client_id;
	cs_uint8	cpu_id;
}__attribute__((__packed__));

struct ipc_context;
typedef int (*ipc_msg_proc)( struct ipc_addr peer, cs_uint16 msg_no, 
		const void *msg_data, cs_uint16 msg_size, 
		struct ipc_context* context);

struct g2_ipc_msg
{
	cs_uint8 msg_no;
	unsigned long proc;
};

int g2_ipc_register( cs_uint8 client_id, const struct g2_ipc_msg *msg_procs,
	       	cs_uint16 msg_count, cs_uint16 invoke_count, void *private_data,
	       	struct ipc_context **context);

int g2_ipc_send( struct ipc_context *context, cs_uint8 cpu_id,
		cs_uint8 client_id, cs_uint8 priority, cs_uint16 msg_no,
		const void *msg_data, cs_uint16 msg_size);


#define _G2_IPC_SYNC__
#ifdef _G2_IPC_SYNC__ 

typedef int (*ipc_invoke_proc)( struct ipc_addr peer, cs_uint16 msg_no, 
		const void *msg_data, cs_uint16 msg_size, 
		void *result_data, cs_uint16 *result_data_size, 
		struct ipc_context* context );

int g2_ipc_invoke( struct ipc_context *context, cs_uint8 cpu_id,
		cs_uint8 client_id, cs_uint8 priority, cs_uint16 msg_no,
		const void *msg_data, cs_uint16 msg_size,
		void *result_data, cs_uint16 *result_size );
#endif

int g2_ipc_deregister(struct ipc_context *context);
void cs_ipc_print_status(cs_uint8 cpu_id);
void cs_ipc_reset_list_info(cs_uint8 cpu_id);

#endif 
