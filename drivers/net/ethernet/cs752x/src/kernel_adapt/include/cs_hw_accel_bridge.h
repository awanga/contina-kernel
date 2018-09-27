#ifndef __CS_HW_ACCEL_BRIDGE_H__
#define __CS_HW_ACCEL_BRIDGE_H__

#include <linux/skbuff.h>
#include <linux/netdevice.h>


int cs_bridge_init(void);
int cs_bridge_exit(void);

#endif
