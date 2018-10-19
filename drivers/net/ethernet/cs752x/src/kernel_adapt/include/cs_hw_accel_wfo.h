#ifndef CS_KERNEL_WFO_HW_ACC_SUPPORT
#define CS_KERNEL_WFO_HW_ACC_SUPPORT 1



int cs_hw_accel_wfo_init(void);
int cs_hw_accel_wfo_exit(void);
int cs_hw_accel_wfo_enable(void);
int cs_hw_accel_wfo_handle_rx(int instance, int voq, struct sk_buff *skb);
int cs_hw_accel_wfo_handle_tx(u8 port_id, struct sk_buff *skb);

#endif

