#ifndef CS_KERNEL_WIRELESS_HW_ACC_SUPPORT
#define CS_KERNEL_WIRELESS_HW_ACC_SUPPORT 1

enum CS_WIRELESS_MODE_TYPE {
	CS_WIRELESS_MODE_4_DEV		= 0,
	CS_WIRELESS_MODE_2_DEV
};

int cs_hw_accel_wireless_init(void);
int cs_hw_accel_wireless_exit(void);
int cs_hw_accel_wireless_handle(int voq, struct sk_buff *skb, unsigned int sw_action);
void cs_hw_accel_wireless_set_mode(int mode);

#endif

