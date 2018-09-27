/******************************************************************************
     Copyright (c) 2010, Cortina Systems, Inc.  All rights reserved.
 *****************************************************************************/

#ifndef __CS_HW_ACCEL_VLAN_H__
#define __CS_HW_ACCEL_VLAN_H__


// Initialization
int cs_vlan_init(void);
int cs_vlan_exit(void);

int cs_vlan_set_input_cb(struct sk_buff *skb);
int cs_vlan_set_output_cb(struct sk_buff *skb);

#endif /* __CS_HW_ACCEL_VLAN_H__ */
