#ifndef _CS75XX_WFO_BRCM_RPC_H_
#define _CS75XX_WFO_BRCM_RPC_H_


//int  wfo_ipc_rpc_callback(struct ipc_addr,cs_uint16,const void *,cs_uint16,void *,cs_uint16 *,struct ipc_context*);
int  wfo_ipc_rpc_synccallback(struct ipc_addr,cs_uint16,const void *,cs_uint16,void *,cs_uint16 *,struct ipc_context*);
int  wfo_ipc_rpc_asynccallback(struct ipc_addr ,u16 ,const void *,u16 ,struct ipc_context *);

#endif
