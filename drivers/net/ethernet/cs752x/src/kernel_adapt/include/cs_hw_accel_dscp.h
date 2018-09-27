/******************************************************************************
     Copyright (c) 2013, Cortina Systems, Inc.  All rights reserved.

 ******************************************************************************
   Module      : cs_hw_accel_dscp.h
   Date        : 2010-09-24
   Description : This header file defines the data structures and APIs for CS
                 DSCP Offload for L2.
   Author      : Axl Lee <axl.lee@cortina-systems.com>
   Remarks     :

 *****************************************************************************/

#ifndef __CS_HW_ACCEL_DSCP_H__
#define __CS_HW_ACCEL_DSCP_H__

#include <mach/cs_types.h>


// Jump Table Entires
int cs_dscp_set_input_cb(struct sk_buff *skb);
int cs_dscp_set_output_cb(struct sk_buff *skb);

#endif /* __CS_HW_ACCEL_DSCP_H__ */
