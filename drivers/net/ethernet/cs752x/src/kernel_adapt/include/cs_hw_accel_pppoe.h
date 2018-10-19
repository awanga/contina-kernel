/******************************************************************************
     Copyright (c) 2010, Cortina Systems, Inc.  All rights reserved.

 ******************************************************************************
   Module      : cs_hw_accel_pppoe.h
   Date        : 2010-09-24
   Description : This header file defines the data structures and APIs for CS
                 PPPoE Offload.
   Author      : Axl Lee <axl.lee@cortina-systems.com>
   Remarks     :

 *****************************************************************************/

#ifndef __CS_HW_ACCEL_PPPOE_H__
#define __CS_HW_ACCEL_PPPOE_H__

#include <mach/cs_types.h>

typedef enum
{
	DIR_LAN2WAN,
	DIR_WAN2LAN,
} DIRECTION_DEF;

// Jump Table Entires
void cs_pppoe_skb_recv_hook(struct sk_buff *skb, u16 pppoe_session_id, u8 direction);
void cs_pppoe_skb_xmit_hook(struct sk_buff *skb, u16 pppoe_session_id, u8 direction);
void cs_pppoe_del_hash_hook(char *pppoe_dev_name);
int cs_pppoe_kernel_set_input_cb(struct sk_buff *skb);
int cs_pppoe_kernel_set_output_cb(struct sk_buff *skb);


void cs_pppoe_init(void);
void cs_pppoe_exit(void);

#endif /* __CS_HW_ACCEL_PPPOE_H__ */
