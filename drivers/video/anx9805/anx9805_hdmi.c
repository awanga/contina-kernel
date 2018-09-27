/*
 *  linux/drivers/video/anx9805_hdmi.c
 *
 * Copyright (c) Cortina-Systems Limited 2010-2011.  All rights reserved.
 *                Joe Hsu <joe.hsu@cortina-systems.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  ANX9805 HDMI Transmitter
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>
#include "../edid.h"
#include "anx9805_hdmi.h"
// Moved to this by SJ 20120313: Start
#include "DP_TX_DRV.h"
#include "DP_TX_Reg.h"
// Moved to this by SJ 20120313: End

struct anx9805_info *pANX9805 = NULL;

#ifdef CONFIG_HDMI_ANX9805_DEBUG
static int dbg_indent;
static int dbg_cnt;
static spinlock_t dbg_spinlock = SPIN_LOCK_UNLOCKED;
#endif

//Global Variables Declaration by SJ 20120313: Start 
BYTE enable_debug_output;
BYTE debug_mode;

#define MAX_BUF_CNT 6
BYTE ByteBuf[MAX_BUF_CNT];

DP_SW_LINK_State eSW_Link_state;
static BYTE CRLoop0, CRLoop1, CRLoop2, CRLoop3;
static BYTE bEQLoopcnt;
static BYTE bMaxSwingCnt;

BYTE dp_tx_system_state;
BYTE timer_slot = 3;			// Joe20110922
BYTE dp_tx_video_id;
BYTE colordepth_packet_mode;
BYTE checksum;
BYTE DP_TX_EDID_PREFERRED[18];
BYTE edid_pclk_out_of_range;
BYTE mode_dp, mode_hdmi;
BYTE hdmi_tx_video_id;
BYTE hdmi_tx_in_pix_rpt, hdmi_tx_tx_pix_rpt;
BYTE hdmi_tx_RGBorYCbCr;

//BYTE hdcp_rx_repeater;
BYTE HDCP_AUTH_DONE;
HDMI_CONFIG_PACKETS Hdmi_packet_config;

//BYTE switch_value_backup;
BYTE dp_tx_link_config_done;
BYTE dp_tx_bw, dp_tx_lane_count;
BYTE dp_tx_test_link_training;
BYTE dp_tx_test_lane_count, dp_tx_test_bw;
BYTE dp_tx_final_lane_count;
BYTE dp_tx_hdcp_enable = 0;
BYTE dp_tx_ssc_enable = 0;
BYTE dp_tx_hdcp_capable_chk, dp_tx_hw_hdcp_en;
BYTE hdcp_auth_pass;
BYTE avmute_enable;
BYTE send_blue_screen;
BYTE hdcp_encryption;
BYTE hdcp_auth_fail_counter;
BYTE USE_FW_LINK_TRAINING;
BYTE RST_ENCODER;

BYTE bForceSelIndex;
BYTE bForceSelIndex_backup;
BYTE dp_tx_edid_err_code;
BYTE hdcp_err_counter;

WORD h_res, h_act, v_res, v_act;
WORD h_res_bak, h_act_bak, v_res_bak, v_act_bak;
BYTE I_or_P, I_or_P_bak;
//middle
//float pclk;
int pclk;
long int M_val, N_val;
BYTE Hdmi_Edid_RGB30bit, Hdmi_Edid_RGB36bit, Hdmi_Edid_RGB48bit;
struct Packet_AVI DP_TX_Packet_AVI;
struct Packet_MPEG DP_TX_Packet_MPEG;
struct Packet_SPD DP_TX_Packet_SPD;
BYTE edid_break, safe_mode, video_config_done;
BYTE bBW_Lane_Adjust;
struct VideoFormat DP_TX_Video_Input;

//Global Variables Declaration by SJ 20120313: End


static inline void anx9805_msleep(unsigned int ms)
{
	if (in_atomic())
		mdelay(ms);
	else
		msleep(ms);	/* can't sleep */
}

int anx9805_i2c_read(struct i2c_client *i2c_cli, u8 reg, u8 *value_p)
{
	struct i2c_msg msg[2] = {
		{
		 .addr = i2c_cli->addr,
		 .flags = 0,
		 .buf = &reg,
		 .len = 1,
		 },
		{
		 .addr = i2c_cli->addr,
		 .flags = I2C_M_RD,
		 .buf = value_p,
		 .len = 1,
		 }
	};
	int ret = 0;

	ret = i2c_transfer(i2c_cli->adapter, msg, 2);
	DBGPRINT(1, "<%s:%d> [%2x:%2x]=%2x\n", __func__, __LINE__,
		 i2c_cli->addr << 1, reg, *value_p);

	return ret;
}

int anx9805_i2c_write(struct i2c_client *i2c_cli, u8 reg, u8 value)
{
	struct i2c_msg msg;
	unsigned char buf[2];

	buf[0] = reg;
	buf[1] = value;

	msg.addr = i2c_cli->addr;
	msg.buf = buf;
	msg.len = 2;
	msg.flags = 0;

	DBGPRINT(1, "<%s:%d> [%2x:%2x]=%2x\n", __func__, __LINE__,
		 i2c_cli->addr << 1, reg, value);
	return i2c_transfer(i2c_cli->adapter, &msg, 1);
}

static void anx9805_reg(struct anx9805_info *anx9805, unsigned int flag,
			union iodata *io)
{
	u8 regvalue;
	int result;
	struct i2c_client *i2client;

	if ((flag == ANX9805_HDMI_GET_REG) || (flag == ANX9805_HDMI_SET_REG)) {
		i2client = anx9805->cli_hdmi;
	} else if ((flag == ANX9805_DP_GET_REG) || (flag == ANX9805_DP_SET_REG)) {
		i2client = anx9805->cli_dp;
	} else {
		i2client = anx9805->cli_sys;
	}

	if ((flag == ANX9805_HDMI_GET_REG) || (flag == ANX9805_SYS_GET_REG)
	    || (flag == ANX9805_DP_GET_REG)) {
		// TODO: reg offset need boundary check!
		result = anx9805_i2c_read(i2client, io->regs.off, &regvalue);
		if (result < 0)
			printk("<%s:%d> err=%d\n", __func__, __LINE__, result);
		io->regs.val = regvalue;
	}

	if ((flag == ANX9805_HDMI_SET_REG) || (flag == ANX9805_SYS_SET_REG)
	    || (flag == ANX9805_DP_SET_REG)) {
		regvalue = io->regs.val;
		result = anx9805_i2c_write(i2client, io->regs.off, regvalue);
		if (result < 0)
			printk("<%s:%d> err=%d\n", __func__, __LINE__, result);
	}

	if (anx9805->delay) {
		anx9805_msleep(anx9805->delay);
	}
}

/* Duplicated: marked by SJ 20120313 */
#if 0

static int anx9805_i2c_rreg(struct anx9805_info *anx9805, u32 flag, u8 regoff,
			    u8 *regval)
{
	union iodata io;

	io.regs.off = regoff;
	io.regs.val = *regval;

	anx9805_reg(anx9805, flag, &io);
	*regval = io.regs.val;

	return 0;
}

static int anx9805_i2c_wreg(struct anx9805_info *anx9805, u32 flag, u8 regoff,
			    u8 regval)
{
	union iodata io;

	io.regs.off = regoff;
	io.regs.val = regval;

	anx9805_reg(anx9805, flag, &io);

	return 0;
}
#endif

static void anx9805_bits(struct anx9805_info *anx9805, unsigned int flag,
			 union iodata *io)
{
	u8 regvalue;
	int result;
	struct i2c_client *i2client;

	if ((flag == ANX9805_HDMI_CLR_BITS) || (flag == ANX9805_HDMI_SET_BITS)) {
		i2client = anx9805->cli_hdmi;
	} else if ((flag == ANX9805_DP_CLR_BITS)
		   || (flag == ANX9805_DP_SET_BITS)) {
		i2client = anx9805->cli_dp;
	} else {
		i2client = anx9805->cli_sys;
	}

	// TODO: reg offset need boundary check!
	result = anx9805_i2c_read(i2client, io->regs.off, &regvalue);
	if (result < 0)
		printk("<%s:%d> err=%d\n", __func__, __LINE__, result);

	if (anx9805->delay) {
		anx9805_msleep(anx9805->delay);
	}

	if ((flag == ANX9805_HDMI_CLR_BITS) || (flag == ANX9805_SYS_CLR_BITS)
	    || (flag == ANX9805_DP_CLR_BITS))
		regvalue &= ~io->regs.val;

	if ((flag == ANX9805_HDMI_SET_BITS) || (flag == ANX9805_SYS_SET_BITS)
	    || (flag == ANX9805_DP_SET_BITS))
		regvalue |= io->regs.val;

	result = anx9805_i2c_write(i2client, io->regs.off, regvalue);
	if (result < 0)
		printk("<%s:%d> err=%d\n", __func__, __LINE__, result);

	if (anx9805->delay) {
		anx9805_msleep(anx9805->delay);
	}
}

/* No use: marked by SJ 20120401 */
#if 0
static int anx9805_i2c_bits(struct anx9805_info *anx9805, u32 flag, u8 regoff,
			    u8 regval)
{
	union iodata io;

	io.regs.off = regoff;
	io.regs.val = regval;

	anx9805_bits(anx9805, flag, &io);

	return 0;
}
#endif

/* Duplicated: marked by SJ 20120313 */
#if 0
static void anx9805_dump_reg(struct anx9805_info *anx9805, int i2cdev, int base,
			     int len)
{
	u8 reg_val;
	u32 cnt;

	for (cnt = base; cnt < (base + len); cnt++) {
		if (i2cdev == ANX9805_SYS_D0_ADDR)
			anx9805_i2c_rreg(anx9805, ANX9805_SYS_GET_REG, cnt,
					 &reg_val);
		if (i2cdev == ANX9805_DP_D0_ADDR)
			anx9805_i2c_rreg(anx9805, ANX9805_DP_GET_REG, cnt,
					 &reg_val);
		if (i2cdev == ANX9805_HDMI_D0_ADDR)
			anx9805_i2c_rreg(anx9805, ANX9805_HDMI_GET_REG, cnt,
					 &reg_val);
		printk("anx9805 dev:reg [%02x:%02x]=%02x\n", i2cdev << 1, cnt,
		       reg_val);
	}
	if (len > 1) {
		printk("\n");
	}
	anx9805_msleep(1);
}
#endif

static int anx9805_gpio_init(void)
{
	/* INT - GPIO group 2, bit 27 */
	if (gpio_request(ANX9805_GPIO_INT, "ANX9805_INT")) {
		printk("<%s:%d> Can't reserve GPIO %d for anx9805 interrupt\n",
			__func__, __LINE__, ANX9805_GPIO_INT);	// 36
		return -ENODEV;
	}
	gpio_direction_input(ANX9805_GPIO_INT);

	return 0;
}

/* No use: marked by SJ 20120401 */
#if 0
static void anx9805_gpio_release(void)
{
	gpio_free(ANX9805_GPIO_INT);
}
#endif

static void g2_clr_anx9805_int(struct anx9805_info *anx9805)
{
	u8 value;
	u32 cnt;

	for (cnt = 1; cnt < 8; cnt++) {
		anx9805_i2c_read(anx9805->cli_sys, INT_STATE_REG + cnt, &value);	/* 72:f0-f7 */
		anx9805_i2c_write(anx9805->cli_sys, INT_STATE_REG + cnt, value);
	}
	for (cnt = 8; cnt < 0xf; cnt++) {
		anx9805_i2c_read(anx9805->cli_sys, INT_STATE_REG + cnt, &value);	/* 72:f8-fe */
		anx9805_i2c_write(anx9805->cli_sys, INT_STATE_REG + cnt, 0);
	}
}

static void anx9805_hdmi_work(struct work_struct *hdmi_work)
{
	struct anx9805_info *anx9805 =
	    container_of(hdmi_work, struct anx9805_info, work);
	u8 value, gpioval;
	u32 cnt;

	printk("<%s:%d>anx9805 hdmi workqueue enter, anx9805=%p\n", __func__,
	       __LINE__, anx9805);

	gpioval = gpio_get_value(ANX9805_GPIO_INT);
	printk("<%s:%d> GPIO: %x\n", __func__, __LINE__, gpioval);
	if (gpioval) {
		goto ENABLE_IRQ;
	}

	/* read and clear the interrupt flags */
	anx9805_i2c_read(anx9805->cli_sys, INT_STATE_REG, &value);	/* 72:f0-f7 */
	anx9805_i2c_write(anx9805->cli_sys, INT_STATE_REG, value);
	printk("<%s:%d> INT_STATE_REG=0x%02x\n", __func__, __LINE__, value);
	if (value & INT_STATE) {	/* check Interrupt Service Requested */
		anx9805_i2c_write(anx9805->cli_sys, INT_STATE_REG, value);
		for (cnt = 1; cnt < 8; cnt++) {
			anx9805_i2c_read(anx9805->cli_sys, INT_STATE_REG + cnt, &value);	/* 72:f0-f7 */
			anx9805->int_status[cnt] = (u32) value;
			printk("<%s:%d> hdmi interrupt status: [%02x]=0x%02x\n",
			       __func__, __LINE__, 0xf0 + cnt,
			       anx9805->int_status[cnt]);
			anx9805_i2c_write(anx9805->cli_sys, INT_STATE_REG + cnt,
					  value);
		}
		if ((anx9805->
		     int_status[4] & (HOTPLUG_CHG | HPD_LOST | PLUG_IN)) ==
		    0x05) {
			printk("<%s:%d> hotplug in event happened!\n", __func__,
			       __LINE__);
			anx9805_msleep(200);
			g2_anx9805_init();
		}
	} else {
		printk("<%s:%d> no interrupt event found\n", __func__,
		       __LINE__);
		for (cnt = 1; cnt < 16; cnt++) {
			anx9805_i2c_read(anx9805->cli_sys, INT_STATE_REG + cnt, &value);	/* 72:f0-f7 */
			printk("<%s:%d> hdmi interrupt status: [%02x]=0x%02x\n",
			       __func__, __LINE__, 0xf0 + cnt, value);
			//anx9805_i2c_write(anx9805->cli_sys, INT_STATE_REG + cnt, value);
		}
	}

ENABLE_IRQ:
	printk("<%s:%d> enable gpio irq and leave\n", __func__, __LINE__);
	enable_irq(gpio_to_irq(ANX9805_GPIO_INT));
}

static irqreturn_t anx9805_hdmi_irq(int irq, void *dev_id)
{
	struct anx9805_info *anx9805 = (struct anx9805_info *)dev_id;
	
	printk("<%s:%d> anx9805 hdmi interrupt happened, anx9805=%p\n", __func__, __LINE__, anx9805);
	//DBGPRINT(1, "<%s:%d> anx9805 hdmi interrupt happened, anx9805=%p\n", __func__, __LINE__, anx9805);
	disable_irq_nosync(gpio_to_irq(ANX9805_GPIO_INT));
	schedule_work(&anx9805->work);
	return IRQ_HANDLED;
}

static void anx9805_i2c_client_remove(struct anx9805_info *anx9805)
{
	if (anx9805->cli_hdmi)
		i2c_unregister_device(anx9805->cli_hdmi);
}

static int anx9805_release(struct anx9805_info *anx9805)
{
	free_irq(gpio_to_irq(ANX9805_GPIO_INT), anx9805);
	gpio_free(ANX9805_GPIO_INT);

	return 0;
}

static int anx9805_open(struct inode *inode, struct file *file)
{
	printk("%s open\n", ANX9805_HDMI_DRIVER_NAME);

	//inode->i_private = inode;
	file->private_data = pANX9805;
	return 0;
}

static int anx9805_close(struct inode *inode, struct file *file)
{
	printk("%s close\n", ANX9805_HDMI_DRIVER_NAME);

	//inode->i_private = NULL;
	file->private_data = NULL;
	return 0;
}

static long anx9805_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct anx9805_info *anx9805 = file->private_data;
	int ret = 0;
	union iodata io;

	if (_IOC_TYPE(cmd) != CS75XX_HDMI_IOCTL_MAGIC) {
		return -ENOTTY;
	}
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&io, (union iodata *)arg, sizeof(io))) {
			return -EFAULT;
		}
	}

	switch (cmd) {
	case ANX9805_SYS_GET_REG:
	case ANX9805_SYS_SET_REG:
	case ANX9805_HDMI_GET_REG:
	case ANX9805_HDMI_SET_REG:
	case ANX9805_DP_GET_REG:
	case ANX9805_DP_SET_REG:
		anx9805_reg(anx9805, cmd, &io);	/* TODO: need boundary check */
		break;
	case ANX9805_SYS_CLR_BITS:
	case ANX9805_SYS_SET_BITS:
	case ANX9805_HDMI_CLR_BITS:
	case ANX9805_HDMI_SET_BITS:
	case ANX9805_DP_CLR_BITS:
	case ANX9805_DP_SET_BITS:
		anx9805_bits(anx9805, cmd, &io);	/* TODO: need boundary check */
		break;
	case ANX9805_REG_DELAY_MS:
		anx9805->delay = (u32) io.dat;	/* TODO: need boundary check */
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &io, sizeof(io))) {
			return -EFAULT;
		}
	}

	return ret;
}

struct file_operations anx9805_fops = {
	.open = anx9805_open,
	.release = anx9805_close,
	.owner = THIS_MODULE,
	.compat_ioctl = anx9805_ioctl,
};

static struct miscdevice anx9805_hdmi_miscdev = {
	HDMI_MINOR,
	"anx9805",
	&anx9805_fops
};

static int register_anx9805(const struct file_operations *fops,
			    struct anx9805_info *anx9805)
{
	int misc_ret;

	misc_ret = misc_register(&anx9805_hdmi_miscdev);
	if (misc_ret == 0) {
		anx9805->miscdev = &anx9805_hdmi_miscdev;
	}
	return misc_ret;
}

static void unregister_anx9805(const struct file_operations *fops,
			       struct anx9805_info *anx9805)
{
	misc_deregister(&anx9805_hdmi_miscdev);
	printk(KERN_ALERT "%s driver cleanup.\n", ANX9805_HDMI_DRIVER_NAME);
}

static const struct i2c_device_id anx9805_id[] = {
	{"anx9805-sys", 0},
	{"anx9805-dp", 1},
	{"anx9805-hdmi", 2},
	{}
};

MODULE_DEVICE_TABLE(i2c, anx9805_id);

static int anx9805_sys_i2c_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct anx9805_info *anx9805 = NULL;
	int reg_ret = 0, gpio_ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		return -ENODEV;
	}

	anx9805 = kzalloc(sizeof(struct anx9805_info), GFP_KERNEL);
	if (!anx9805) {
		printk("anx9805 kzalloc failed\n");
		return -ENOMEM;
	}

	pANX9805 = anx9805;
	anx9805->cli_sys = client;
	anx9805->id = id;
	anx9805->delay = 0;

	i2c_set_clientdata(client, anx9805);

	reg_ret = register_anx9805(&anx9805_fops, anx9805);
	if (reg_ret)
		goto error_reg;

	INIT_WORK(&anx9805->work, anx9805_hdmi_work);

	g2_clr_anx9805_int(anx9805);
	gpio_ret = anx9805_gpio_init();
	if (gpio_ret)
		goto error_gpio;

	printk("anx9805 hdmi transmitter created and attached\n");
	return 0;

error_gpio:
	if (reg_ret == 0)
		unregister_anx9805(&anx9805_fops, anx9805);

error_reg:
	printk("anx9805 hdmi transmitter probing failed\n");
	anx9805_i2c_client_remove(anx9805);
	i2c_set_clientdata(client, NULL);
	kfree(anx9805);
	anx9805 = pANX9805 = NULL;
	return -ENODEV;
}

static int anx9805_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	int ret, irq_ret = 0;

	if (!strcmp(client->name, "anx9805-sys")) {
		ret = anx9805_sys_i2c_probe(client, id);
		return ret;
	} else if (!strcmp(client->name, "anx9805-dp")) {
		if (pANX9805) {
			pANX9805->cli_dp = client;
		} else {
			return -1;
		}
		return 0;
	} else if (!strcmp(client->name, "anx9805-hdmi")) {
		if (pANX9805) {
			pANX9805->cli_hdmi = client;
			pANX9805->init_done = 0;
			/* Init the anx9805 hdmi transmitter */
			g2_anx9805_init();	// check gpio interrupt signal
			g2_clr_anx9805_int(pANX9805);
			irq_ret =
			    request_irq(gpio_to_irq(ANX9805_GPIO_INT),
					anx9805_hdmi_irq, IRQ_TYPE_LEVEL_LOW,
					ANX9805_HDMI_DRIVER_NAME, pANX9805);
			if (irq_ret) {
				goto error_irq;
			}
			/* enable the interrupts */
			anx9805_i2c_write(pANX9805->cli_sys, INT_COM_MSK3_REG,
					  R0_CHK_FLAG | RI_NO_UPDATE |
					  SYNC_POST_CHK_FAIL);
			anx9805_i2c_write(pANX9805->cli_sys, INT_COM_MSK4_REG,
					  ANX_HOTPLUG_CHG | ANX_HPD_LOST |
					  ANX_PLUG);
		} else {
			printk("<%s:%d> pANX9805 is not assigned\n", __func__,
			       __LINE__);
			return -1;
		}
		return 0;
	} else {
		printk("unknown anx9805 i2c client\n");
		return -1;
	}

error_irq:
	gpio_free(ANX9805_GPIO_INT);
	unregister_anx9805(&anx9805_fops, pANX9805);
	anx9805_i2c_client_remove(pANX9805);
	i2c_set_clientdata(client, NULL);
	kfree(pANX9805);
	pANX9805 = NULL;

	return -1;
}

// Moved by SJ 20120313: Start
int g2_anx9805_init(void)
{
	BYTE fw_restart;
	BYTE c;
	int timeoutcnt;

	timer_slot = 3;	

	if (!pANX9805->init_done) 
	{
		pANX9805->init_done = 1;
		
FW_Start:
		fw_restart = 0;
		enable_debug_output = 1;
		debug_mode = 0;

		printk("DP_HDMI TX FW Version is %.2f, ", DP_HDMI_TX_FW_VER);
		printk("Build at: %s,%s\n", __DATE__, __TIME__);

		if (DP_TX_Chip_Located())
		{
			printk("************Chip found\n");
		}
		else 
		{
			printk("************chip not found\n");
			return -1;
		}

		/*if(!BIST_EN)
		{
			printk("waiting Rx...");
			mdelay(2000);//wait for HDMI RX stable
		} */

		DP_TX_Initialization();
		DP_TX_InputSet(VIP_CSC_RGB, COLOR_8);

		//DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_MASK3, 0xaa);	// Joe20111227 disable Audio FIFO underflow temporary
		//DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_MASK3, AFIFO_UNDER | R0_CHK_FLAG | RI_NO_UPDATE | SYNC_POST_CHK_FAIL);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_MASK3, R0_CHK_FLAG | RI_NO_UPDATE | SYNC_POST_CHK_FAIL);

		// Joe20111227, enable hotplug detection interrupt
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_MASK4, ANX_HOTPLUG_CHG | ANX_HPD_LOST | ANX_PLUG);

		if (!SWITCH2)
		{
			dp_tx_ssc_enable = 1;
		}
		else
		{
			dp_tx_ssc_enable = 0;
		}

		if (!SWITCH3)
		{
			dp_tx_hdcp_enable = 1;
		}
		else
		{
			dp_tx_hdcp_enable = 0;
		}

		//use HW link training default
		USE_FW_LINK_TRAINING = 0;

		//disable auto reset 8b/10b encoder before sending link training patter 2
		RST_ENCODER = 1;

		bForceSelIndex = 0;

		//when set to 1, will adjust BW and lane count before link training
		bBW_Lane_Adjust = 0;

		if (!BIST_EN) 
		{
			//printk("PRBS pattern!");
			while (SWITCH4) 
			{
				printk("<%s:%d> SWITCH4=%d\n", __func__, __LINE__, (WORD)SWITCH4);
				DP_TX_PBBS7_Test();
				while (SWITCH3) 
				{
					DP_TX_Insert_Err();
					//middle
					mdelay(1000);
				}
			}
		}
	}
	
	timeoutcnt=0;
	while (1) 
	{
		if (!debug_mode) 
		{
			DP_TX_Task();
			DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_MASK3, &c);
			//if (c != 0xaa){/* disable Audio FIFO underflow check temporary */
			if (c != (R0_CHK_FLAG | RI_NO_UPDATE | SYNC_POST_CHK_FAIL))
			{
					printk("<%s:%d> goto FW_Start\n", __func__, __LINE__);
					goto FW_Start;
			}
		}
		//VESA_Tming_Set();
		
		if ((timer_slot>=3) && ((dp_tx_system_state == DP_TX_PLAY_BACK) ||
				        (dp_tx_system_state == DP_TX_WAIT_HOTPLUG))) 
		{
			printk("<%s:%d> anx9805 config finished\n", __func__, __LINE__);
			break;
		}
		
		if ((timer_slot==2) && (dp_tx_system_state == DP_TX_CONFIG_VIDEO)) 
		{
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_STATUS, &c);
			if ((c & HDMI_SYS_STATE_PLL_LOCK) == 0) 
			{
				timeoutcnt++;
				if (timeoutcnt>5) 
				{
					printk("<%s:%d> anx9805 config video timeout\n",
						__func__, __LINE__);
					pANX9805->init_done = 0;
					break;
				}
			}
		}
	}
#if defined(CONFIG_CORTINA_PON) || defined(CONFIG_CORTINA_WAN)
#if defined(CONFIG_FB_CS752X_SWAP_HDMI_LANE)
	anx9805_i2c_write(pANX9805->cli_sys, LANE_MAP_REG, 0xc6);
#else
	anx9805_i2c_write(pANX9805->cli_sys, LANE_MAP_REG, 0xe4);
#endif
#endif
	return 0;
}

BYTE DP_TX_Chip_Located(void)
{
	BYTE m;
	BYTE n;

	printk("<%s:%d>\n", __func__, __LINE__);
#if 0
	DP_TX_Resetn_Pin = 0;
	mdelay(2);
	DP_TX_Resetn_Pin = 1;

#endif /*  */
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_DEV_IDL_REG, &m);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_DEV_IDH_REG, &n);
	printk("<%s:%d>\n", __func__, __LINE__);
	
	if ((m == 0x05) && (n == 0x98))
	{
		return 1;
	}
	
	return 0;
}

u8 DP_TX_Read_Reg(BYTE I2cSlaveAddr, BYTE RegAddr, BYTE *DataPtr)
{
#if 0
  return 0;
#else
	struct i2c_client *client;

	// TODO: Joe20111229 change to switch
	if (I2cSlaveAddr == HDMI_TX_PORT0_ADDR)
	{
		client = pANX9805->cli_sys;
	}
	else if (I2cSlaveAddr == DP_TX_PORT0_ADDR)
	{
		client = pANX9805->cli_dp;
	}
	else
	{
		client = pANX9805->cli_hdmi;
	}

	return anx9805_i2c_read(client, RegAddr, DataPtr);
#endif
}

u8 DP_TX_Write_Reg(BYTE I2cSlaveAddr, BYTE RegAddr, BYTE RegData)
{
#if 0
  return 0;
#else
	struct i2c_client *client;

	// TODO: Joe20111229 change to switch
	if (I2cSlaveAddr == HDMI_TX_PORT0_ADDR)
	{
		client = pANX9805->cli_sys;
	}
	else if (I2cSlaveAddr == DP_TX_PORT0_ADDR)
	{
		client = pANX9805->cli_dp;
	}
	else
	{
		client = pANX9805->cli_hdmi;
	}

	return anx9805_i2c_write(client,RegAddr,RegData);
#endif
}

void anx9805_dump_reg(BYTE dev_addr, BYTE base, BYTE len)
{
	BYTE reg_val;
	int cnt;

	for (cnt=base; cnt<(base+len); cnt++) 
	{
		DP_TX_Read_Reg(dev_addr, (BYTE)cnt, &reg_val);
		DBGPRINT(1, "[%02x:%02x]=%02x\n", (WORD) dev_addr, (WORD) cnt, (WORD) reg_val);
	}
	if (len > 1) 
	{
		DBGPRINT(1, "\n");
	}
}

void video_bit_ctrl(BYTE colorcomp)
{
	BYTE bitcnt, reg_val;
	
	struct rgb_base {
		BYTE r, g, b;
	} rgb_base;

	rgb_base.r = rgb_base.g = rgb_base.b = 0;
	//printk("<%s:%d>\n", __func__, __LINE__);
	switch (colorcomp & 0x07) {
	case RGB:
		rgb_base.r = 16;	rgb_base.g = 8;	rgb_base.b = 0;
		break;
	case RBG:
		rgb_base.r = 16;	rgb_base.b = 8;	rgb_base.g = 0;
		break;
	case GRB:
		rgb_base.g = 16;	rgb_base.r = 8;	rgb_base.b = 0;
		break;
	case BRG:
		rgb_base.b = 16;	rgb_base.r = 8;	rgb_base.g = 0;
		break;
	case GBR:
		rgb_base.g = 16;	rgb_base.b = 8;	rgb_base.r = 0;
		break;
	case BGR:
		rgb_base.b = 16;	rgb_base.g = 8;	rgb_base.r = 0;
		break;
	case NO_MAP:
	default:
		break;
	}

#if 1
	for (bitcnt=0; bitcnt<8; bitcnt++) {
	// red
		reg_val = 16 + bitcnt;
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG + rgb_base.r + bitcnt, reg_val);
		//anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG + rgb_base.r + bitcnt, 1);
	// green
		reg_val = 8 + bitcnt;
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG + rgb_base.g + bitcnt, reg_val);
		//anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG + rgb_base.g + bitcnt, 1);
	// blue
		reg_val = 0 + bitcnt;
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG + rgb_base.b + bitcnt, reg_val);
		//anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG + rgb_base.b + bitcnt, 1);
	}
#endif

	// Other configuration
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL10_REG, &reg_val);
	//reg_val |= 0x10;	// Select video format information from ivdeo capture
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL10_REG, reg_val);
}

void DP_TX_Task(void)
{
	DBGPRINT(1, "<%s:%d> enter\n", __func__, __LINE__);
	DP_TX_Int_Process();
	DP_TX_TimerProcess();
	DBGPRINT(1, "<%s:%d> leave\n", __func__, __LINE__);
}

void DP_TX_TimerProcess(void)
{
	if (timer_slot == 3)
	{
		timer_slot = 0;
	}
	else
	{
		timer_slot++;
	}
	
	DBGPRINT(1, "<%s:%d> timer_slot=%d\n", __func__, __LINE__, (WORD)timer_slot);
	if (timer_slot == 0) 
	{
		DP_TX_Timer_Slot1();
	}
	else if (timer_slot == 1) 
	{
		DP_TX_Timer_Slot2();
	}
	else if (timer_slot == 2) 
	{
		DP_TX_Timer_Slot3();
	}
}

void DP_TX_Int_Process(void)
{
	BYTE c, c1, c2, c3;

	DBGPRINT(1, "<%s:%d> enter\n", __func__, __LINE__);
	if (dp_tx_system_state == DP_TX_WAIT_HOTPLUG) 
	{
		DBGPRINT(1, "<%s:%d> state=DP_TX_WAIT_HOTPLUG\n", __func__, __LINE__);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS4, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS4, c);
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, &c1);
		if (((c1 & DP_TX_SYS_CTRL3_HPD_STATUS) == 0x40)
			|| ((c & DP_COMMON_INT4_PLUG) == 0x01))
		{
			DP_TX_HPD_Int_Handler(1);
		}
	}
	else 
	{
		DBGPRINT(1, "<%s:%d> state=%d\n", __func__, __LINE__, dp_tx_system_state);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS1, &c3);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS1, c3);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS2, &c2);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS2, c2);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS4, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS4, c);

		//printk("DP_COMMON_INT_STATUS4 = %.2x\n", (WORD)c);
		//DP_TX_Int_Info(c, 2);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_INT_STATUS1, &c1);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_INT_STATUS1, c1);

		//DP_TX_Int_Info(c1, 1);
		if (c & DP_COMMON_INT4_PLUG)	//PLUG, hpd changed
		{
			DP_TX_HPD_Int_Handler(1);
		}
		
		if (c1 & DP_TX_INT_STATUS1_HPD)	//IRQ, HPD
		{
			printk("IRQ!!!!!!!!!!!!!");
			if ((dp_tx_system_state != DP_TX_INITIAL)
				&& (dp_tx_system_state != DP_TX_WAIT_HOTPLUG)
				&& (dp_tx_system_state !=
					DP_TX_LINK_TRAINING) && (dp_tx_system_state != DP_TX_PARSE_EDID)
				&& (dp_tx_system_state != DP_TX_CONFIG_VIDEO)) 
			{
				DP_TX_HPD_Int_Handler(0);
			}
		}
		
		if (c & DP_COMMON_INT4_HPDLOST)	//cable lost
		{
			DP_TX_HPD_Int_Handler(2);
		}
		
		if (c2 & DP_COMMON_INT2_AUTHDONE)
		{
			HDCP_Auth_Done_Interrupt();
		}
		
		if (c2 & DP_COMMON_INT2_AUTHCHG)
		{
			HDCP_Auth_Change_Interrupt();
		}
		
		if ((c3 & DP_COMMON_INT1_AUDIO_CLK_CHG)) 
		{

			//DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, 0x3d, &c);
			//printk("0x072:0x3D = %.2x\n", (WORD)c);
			if (!SWITCH1) 
			{
				if (dp_tx_system_state >= DP_TX_CONFIG_AUDIO) 
				{
					mdelay(10);
					DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_STATUS0, &c);
					if ((c & 0x81) == 0x81) 
					{
						printk("Audio changed!");

						//DP_TX_Clean_HDCP();
						DP_TX_Set_System_State(DP_TX_CONFIG_AUDIO);

						//DP_TX_Config_Audio();
					}
				}
			}
		}

		/*
		   if(c3 & (DP_COMMON_INT1_VIDEO_CLOCK_CHG | DP_COMMON_INT1_VIDEO_FORMAT_CHG))//video clock change + video format change
		   {
		   if(dp_tx_system_state >= DP_TX_CONFIG_VIDEO)
		   {
		   printk("Video format changed");
		   //DP_TX_Video_Changed_Handler();

		   video_config_done = 0;
		   if(mode_dp)
		   {
		   DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
		   DP_TX_Video_Disable();

		   }
		   else
		   {
		   //mute TMDS
		   DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CLKCH_CONFIG_REG, &c);
		   DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CLKCH_CONFIG_REG, c & (~HDMI_TMDS_CLKCH_MUTE));

		   //set AVMUTE
		   DP_TX_SET_AVMUTE();

		   //disable HW HDCP
		   DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
		   DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c & (~DP_TX_HDCP_CONTROL_0_HARD_AUTH_EN));

		   DP_TX_RST_DDC();

		   DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);

		   }//guochuncheng add 08.11.13
		   }
		   }
		 */
		//if(c3 & DP_COMMON_INT1_VIDEO_FORMAT_CHG)//
		//DP_TX_Video_Changed_Int_Handler();

/*        if(c3 & DP_COMMON_INT1_PLL_LOCK_CHG)//pll lock change
        {
            if(mode_dp_or_hdmi)
                DP_TX_PLL_Changed_Int_Handler();
        }*/

		//if(c1 & DP_TX_INT_STATUS1_TRAINING_Finish)
		//DP_TX_EnhaceMode_Set();
	}
	DBGPRINT(1, "<%s:%d> leave\n", __func__, __LINE__);
}

void DP_TX_HPD_Int_Handler(BYTE hpd_source)
{
	BYTE c, c1, sl_cr, test_irq, test_lt, al, lane0_1_status, lane2_3_status;
	BYTE IRQ_Vector, test_vector;

	DBGPRINT(1, "<%s:%d> interrupt handling (dp_tx_system_state=%d, hpd_source=%d)\n", __func__, __LINE__, (WORD)dp_tx_system_state, hpd_source);

	if (dp_tx_system_state == DP_TX_INITIAL)
	{
		return;
	}
	else 
	{

		//mdelay(200);
		switch (hpd_source) 
		{
		
			case 0:
				if (DP_TX_AUX_DPCDRead_Bytes(0x000201, 1, ByteBuf))
				{
					return;
				}
				IRQ_Vector = ByteBuf[0];
				printk("IRQ_VECTOR = %.2x\n", (WORD) IRQ_Vector);
				DP_TX_AUX_DPCDWrite_Bytes(0x000201, 1, ByteBuf);	//write clear IRQ
				if (IRQ_Vector & 0x04)	//HDCP IRQ
				{
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CTRL, 0x01);
					DP_TX_AUX_DPCDRead_Bytes(0x068029, 1, ByteBuf);	//0x68029:Bstatus DPCD address
					c1 = ByteBuf[0];

					//printk("68029 = %.2x\n",(WORD)c1);
					if (c1 & 0x04)	//link intergrity failure
					{

						/*if((dp_tx_system_state != DP_TX_INITIAL) && (dp_tx_system_state != DP_TX_WAIT_HOTPLUG) && (dp_tx_system_state != DP_TX_LINK_TRAINING)
					   	&& (dp_tx_system_state != DP_TX_PARSE_EDID) && (dp_tx_system_state != DP_TX_CONFIG_VIDEO)) */
						{
							DP_TX_Clean_HDCP();
							DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
							printk("HPD:____HDCP Sync lost!");
						}
					}
				}
				
				if (IRQ_Vector & 0x02)
				{
					test_irq = 1;
				}
				else
				{
					test_irq = 0;
				}
				DP_TX_AUX_DPCDRead_Bytes(0x000204, 1, ByteBuf);
				al = ByteBuf[0];
				DP_TX_AUX_DPCDRead_Bytes(0x000202, 1, ByteBuf);
				lane0_1_status = ByteBuf[0];
				DP_TX_AUX_DPCDRead_Bytes(0x000203, 1, ByteBuf);
				lane2_3_status = ByteBuf[0];
				
				if (dp_tx_final_lane_count == 0x01) 
				{
					if (((lane0_1_status & 0x01) == 0)
						|| ((lane0_1_status & 0x04) == 0))
					{
						sl_cr = 0;
					}
					else
					{
						sl_cr = 1;
					}
				}
				else if (dp_tx_final_lane_count == 0x02) 
				{
					if (((lane0_1_status & 0x10) == 0)
						|| ((lane0_1_status & 0x40) == 0)
						|| ((lane0_1_status & 0x01) == 0)
						|| ((lane0_1_status & 0x04) == 0))
					{
						sl_cr = 0;
					}
					else
					{
						sl_cr = 1;
					}
				}
				else if (dp_tx_final_lane_count == 0x04) 
				{
					if (((lane0_1_status & 0x10) == 0)
						|| ((lane0_1_status & 0x40) == 0)
						|| ((lane0_1_status & 0x01) == 0)
						|| ((lane0_1_status & 0x04) == 0)
						|| ((lane2_3_status & 0x10) == 0)
						|| ((lane2_3_status & 0x40) == 0)
						|| ((lane2_3_status & 0x01) == 0)
						|| ((lane2_3_status & 0x04) == 0))
					{
						sl_cr = 0;
					}
					else
					{
						sl_cr = 1;
					}
				}
				else
				{
					sl_cr = 1;
				}

				if (test_irq) 
				{
					DP_TX_AUX_DPCDRead_Bytes(0x000218, 1, ByteBuf);
					test_vector = ByteBuf[0];
					
					if (test_vector & 0x01) 
					{
						dp_tx_test_link_training = 1;
						test_lt = 1;
						DP_TX_AUX_DPCDRead_Bytes(0x000220, 1, ByteBuf);
						dp_tx_test_lane_count = ByteBuf[0];
						printk("test_lc = %.2x\n", (WORD)dp_tx_test_lane_count);
						dp_tx_test_lane_count = dp_tx_test_lane_count & 0x0f;
						DP_TX_AUX_DPCDRead_Bytes(0x000219, 1, ByteBuf);
						dp_tx_test_bw = ByteBuf[0];
						printk("test_bw = %.2x\n", (WORD) dp_tx_test_bw);
						DP_TX_AUX_DPCDRead_Bytes(0x000260, 1, ByteBuf);
						ByteBuf[0] = ByteBuf[0] | TEST_ACK;
						DP_TX_AUX_DPCDWrite_Bytes(0x000260, 1, ByteBuf);
						printk("Set TEST_ACK!");

						#if 0
					i	f (dp_tx_test_lane_count == 1) {
							DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, &c);
							DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, c & 0x7f);
							DP_TX_AUX_DPCDRead_Bytes(0x00, 0x01, 0x01, 1, ByteBuf);
							ByteBuf[0] = ByteBuf[0] & 0xf0 | 0x01;
							DP_TX_AUX_DPCDWrite_Bytes(0x00, 0x01, 0x01, 1, ByteBuf);
							dp_tx_lane_count = 1;
							DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, dp_tx_lane_count);
							test_lt = 0;
							printk("Test LC reduce w/o LT");

							//DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
							DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
						}
						#endif /*  */
					}
					else 
					{
						dp_tx_test_link_training = 0;
						test_lt = 0;
					}
					
					if (test_vector & 0x04)	//test edid
					{
						DP_TX_Set_System_State(DP_TX_PARSE_EDID);
						printk("Test EDID Requested!");
					}
					
					if (test_vector & 0x02)	//test pattern
					{
						DP_TX_AUX_DPCDRead_Bytes(0x000260, 1, ByteBuf);
						ByteBuf[0] = ByteBuf[0] | 0x01;
						printk("respone = %.2x\n", (WORD) c);
						DP_TX_AUX_DPCDWrite_Bytes(0x000260, 1, ByteBuf);
						DP_TX_AUX_DPCDRead_Bytes(0x000260, 1, ByteBuf);
						while ((ByteBuf[0] & 0x03) == 0) 
						{
							ByteBuf[0] = ByteBuf[0] | 0x01;
							DP_TX_AUX_DPCDWrite_Bytes(0x000260, 1, ByteBuf);
							DP_TX_AUX_DPCDRead_Bytes(0x000260, 1, ByteBuf);

							//c = ByteBuf[0];
						}
						printk("respone = %.2x\n", (WORD) c);
					}
				}
				else 
				{
					dp_tx_test_link_training = 0;
					test_lt = 0;
				}
				
				if (((al & 0x01) == 0) || (sl_cr == 0) || (test_lt == 1))	//!(c & 0x01)
				{
					if ((al & 0x01) == 0)
					{
						printk("AL lost\n");
					}

					if (sl_cr == 0)
					{
						printk("CR lost\n");
					}
					
					if (test_lt == 1)
					{
						printk("Test LT requested\n");
					}

					/*if((dp_tx_system_state != DP_TX_INITIAL) && (dp_tx_system_state != DP_TX_WAIT_HOTPLUG)
				   	&& (dp_tx_system_state != DP_TX_PARSE_EDID) && (dp_tx_system_state != DP_TX_CONFIG_VIDEO)
				   	&& (dp_tx_system_state != DP_TX_LINK_TRAINING)) */
					{
						dp_tx_link_config_done = 0;
						DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
						DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
						printk("IRQ:____re-LT request!");

						//DP_TX_HW_LT(dp_tx_bw, dp_tx_lane_count);
					}
				}
				break;
				
			case 1:
				//printk("HPD:_____plug!");
				printk("<%s:%d> HPD:_____plug!\n", __func__, __LINE__);
				dp_tx_link_config_done = 0;
				dp_tx_hdcp_capable_chk = 0;

				//DP_TX_Clean_HDCP();
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, &c);
				if (c & DP_TX_SYS_CTRL3_HPD_STATUS) 
				{
					DP_TX_Power_On();

					//mdelay(200);
					if (Sink_Is_DP_HDMI())
					{
						return;
					}
					DP_TX_Set_System_State(DP_TX_PARSE_EDID);
				}
				else 
				{
					DP_TX_Power_Down();
					DP_TX_Set_System_State(DP_TX_WAIT_HOTPLUG);
				}
				break;
				
			case 2:

				/*
			   	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_INT_STATUS1, &c);
			  	 DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_INT_STATUS1, c);
			   	if(c & DP_TX_INT_STATUS1_HPD)//HPD detected
			   	return;

			   	DP_TX_Clean_HDCP();//20090702.cguo. In the case, 9805 downstream is repeater. Unplug downstream of repater.
			   	DP_TX_Power_Down();
			   	//hdcp_rx_repeater = 0;
			   	printk("HPD:_____HPD_Lost!");
			   	DP_TX_Set_System_State(DP_TX_WAIT_HOTPLUG);
			 	*/

				//mdelay(2);
				DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_INT_STATUS1, &c);
				DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_INT_STATUS1, c);
				if (c & DP_TX_INT_STATUS1_HPD)	//HPD detected
				{
					return;
				}
				else 
				{
					DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, &c);
					if (c & DP_TX_SYS_CTRL3_HPD_STATUS)
					{
						return;
					}
					else 
					{
						DP_TX_Clean_HDCP();	//20090702.cguo. In the case, 9805 downstream is repeater. Unplug downstream of repater.
						DP_TX_Power_Down();

						//hdcp_rx_repeater = 0;
						printk("<%s:%d> HPD:_____HPD_Lost!\n", __func__, __LINE__);
						//printk("HPD:_____HPD_Lost!");
						DP_TX_Set_System_State(DP_TX_WAIT_HOTPLUG);
					}
				}
				break;
				
			default:
				break;
				
		}
	}
}

void DP_TX_FORCE_HPD(void)
{
	BYTE c;
	
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, c | 0x30);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
}

void DP_TX_UNFORCE_HPD(void)
{
	BYTE c;

	//unforce HPD
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, c & 0xcf);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_SYS_CTRL3_REG, 1);
}

void DP_TX_HDCP_Process(void)
{
	BYTE c;

	//WORD i;
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_SET_AVMUTE();
	if (dp_tx_hdcp_enable) {
		if (mode_dp) {
			if (!dp_tx_hdcp_capable_chk) {
				DP_TX_AUX_DPCDRead_Bytes(0x068028, 1, ByteBuf);	//read Bcaps
				c = ByteBuf[0];
				if (!(c & 0x01)) {
					printk("RX not capable HDCP");
					return;
				}
				if ((ByteBuf[0] & 0x02) == 0x02)	// for HDCP cascade.cguo
				{

					//hdcp_rx_repeater = 1;
					printk("DP downstream is a repeater");
				}
				dp_tx_hdcp_capable_chk = 1;
			}
		}
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c | 0x03);
		if ((c & DP_TX_HDCP_CONTROL_0_HARD_AUTH_EN) == 0x00)
			dp_tx_hw_hdcp_en = 0;
		if (!dp_tx_hw_hdcp_en) {
			DP_TX_HW_HDCP_Enable();
			dp_tx_hw_hdcp_en = 1;

			//mdelay(100);
			/*
			   if(hdcp_rx_repeater)
			   {
			   //printk("Waiting HDCP repeater...");
			   for (i = 0; i < 1400; i++)
			   {
			   DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_COMMON_INT_STATUS2, &c);
			   if(c & DP_COMMON_INT2_AUTHDONE)
			   {
			   DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
			   DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c & 0xfc);
			   HDCP_AUTH_DONE = 1;
			   break;
			   }
			   else
			   {
			   mdelay(3);
			   }
			   }

			   }
			 */
		}
		//DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CTRL, 0x03);
		if (HDCP_AUTH_DONE) {
			if (hdcp_auth_pass) {
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c & 0xfc);
				DP_TX_HDCP_Encryption_Enable();
				DP_TX_Blue_Screen_Disable();

				//DP_TX_CLEAR_AVMUTE();
				printk("@@@auth_pass@@@");
				hdcp_err_counter = 0;
			}

			else {
				DP_TX_HDCP_Encryption_Disable();
				DP_TX_Blue_Screen_Enable();
				DP_TX_CLEAR_AVMUTE();

				//printk("***auth_failed***");
				if (hdcp_auth_fail_counter > 5) {
					DP_TX_Clean_HDCP();
				}
				return;
			}
		}
		else
			return;
	}
	DP_TX_CLEAR_AVMUTE();
	DP_TX_Set_System_State(DP_TX_PLAY_BACK);

	//DP_TX_Set_System_State(DP_TX_CONFIG_AUDIO);
	DP_TX_RST_AUX();

	//mdelay(20);
	DP_TX_Show_Infomation();
}

void DP_TX_HW_HDCP_Enable(void)
{
	BYTE c;
	
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c | DP_TX_HDCP_CONTROL_0_HARD_AUTH_EN);
}

void DP_TX_Clean_HDCP(void)
{
	BYTE c;
	printk("<%s:%d> HDCP Function Reset\n", __func__, __LINE__);
	//printk("HDCP Function Reset");

	// hdcp pass, clear all related variable for next time hdcp
	dp_tx_hw_hdcp_en = 0;
	hdcp_err_counter = 0;
	HDCP_AUTH_DONE = 0;
	hdcp_auth_fail_counter = 0;
	DP_TX_HDCP_Encryption_Disable();

	//disable HW HDCP
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, 0x00);	//c & ~DVI_TX_HDCP_CONTROL_0_HARD_AUTH_EN);

	//reset HDCP block
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL_REG, c | DP_TX_RST_HDCP_REG);
	mdelay(10);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL_REG, c & ~DP_TX_RST_HDCP_REG);

	//set re-auth
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c | DP_TX_HDCP_CONTROL_0_RE_AUTH);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c & ~DP_TX_HDCP_CONTROL_0_RE_AUTH);
}

void DP_TX_HDCP_Encryption_Disable(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, c & ~DP_TX_HDCP_CONTROL_0_HDCP_ENC_EN);
}

void DP_TX_HDCP_Encryption_Enable(void)
{
	BYTE c;
	
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG,c | DP_TX_HDCP_CONTROL_0_HDCP_ENC_EN);
}

void HDCP_Auth_Done_Interrupt()
{
	BYTE c;

	DBGPRINT(1, "<%s:%d> (dp_tx_system_state=%d)\n", __func__, __LINE__, dp_tx_system_state);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_STATUS, &c);
	if (c & DP_TX_HDCP_AUTH_PASS) {
		DP_TX_HDCP_Encryption_Disable();

		//printk("INT:Auth_Done = Pass");
		if (send_blue_screen) {
			DP_TX_Blue_Screen_Disable();
		}
		if (avmute_enable) {
			DP_TX_CLEAR_AVMUTE();
		}
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, c | SPDIF_AUDIO_CTRL0_SPDIF_IN |  SPDIF_AUDIO_CTRL0_SPDIF0);	// Joe20111012
		hdcp_auth_pass = 1;
		hdcp_auth_fail_counter = 0;
	}

	else {

		//printk("INT:Auth_Done = Fail");
		printk("***auth_failed***");
		hdcp_auth_pass = 0;
		hdcp_auth_fail_counter++;
		if (hdcp_auth_fail_counter >= DP_TX_HDCP_FAIL_THRESHOLD) {
			printk("HDCP_FAIL_THRESHOLD reached. ");
			hdcp_auth_fail_counter = 0;

			// TODO: Reset link;
			if (!send_blue_screen) {
				DP_TX_Blue_Screen_Enable();
			}
			if (avmute_enable) {
				DP_TX_CLEAR_AVMUTE();
			}
			DP_TX_HDCP_Encryption_Disable();
			DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, c & ~SPDIF_AUDIO_CTRL0_SPDIF_IN);
		}
	}
	HDCP_AUTH_DONE = 1;
	if (dp_tx_system_state >= DP_TX_HDCP_AUTHENTICATION) {
		DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
	}
}
void HDCP_Auth_Change_Interrupt()
{
	BYTE c;

	DBGPRINT(1, "<%s:%d> (dp_tx_system_state=%d)\n", __func__, __LINE__, dp_tx_system_state);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_STATUS, &c);
	if (c & DP_TX_HDCP_AUTH_PASS) {
		DP_TX_HDCP_Encryption_Disable();

		//printk("INT:Auth_CHG = Pass");
		if (send_blue_screen) {
			DP_TX_Blue_Screen_Disable();
		}
		if (avmute_enable) {
			DP_TX_CLEAR_AVMUTE();
		}
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, c | SPDIF_AUDIO_CTRL0_SPDIF_IN | SPDIF_AUDIO_CTRL0_SPDIF0);	// Joe20111012
		hdcp_auth_pass = 1;
	}

	else {						//printk("INT:Auth_CHG = Fail");
		if (!send_blue_screen) {
			DP_TX_Blue_Screen_Enable();
		}
		if (avmute_enable) {
			DP_TX_CLEAR_AVMUTE();
		}
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, c & ~SPDIF_AUDIO_CTRL0_SPDIF_IN);
		hdcp_auth_pass = 0;
		dp_tx_hw_hdcp_en = 0;
		DP_TX_HDCP_Encryption_Disable();
		if (dp_tx_system_state == DP_TX_PLAY_BACK) {
			DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
		}
	}
}

void DP_TX_Initialization(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d> enter\n", __func__, __LINE__);
	printk("<%s:%d> DP_TX_initing (dp_tx_system_state=%x)\n", __func__, __LINE__, dp_tx_system_state);
	//video_bpc = 8;
	DP_TX_Variable_Init();
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, ((c & (~DP_POWERD_REGISTER_REG)) | DP_POWERD_AUDIO_REG));
	DP_TX_Video_Disable();

	//software reset
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL_REG, c | DP_TX_RST_SW_RST);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL_REG, c & ~DP_TX_RST_SW_RST);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PLL_CTRL_REG, 0x07);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_EXTRA_ADDR_REG, 0x50);

	//12bit DDR,negedge latch, and wait video stable
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, 0x05);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_FILTER_CTRL3, 0x19);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_CTRL3, 0xd9);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_PLL_MISC_CTRL1, 0x10);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_PLL_MISC_CTRL2, 0x20);

	//disable DDC level shift 08.11.11
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, 0x65, 0x00);

	//serdes ac mode.
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c | DP_TX_AC_MODE);

	//set channel output amplitude for DP PHY CTS
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CH0_REG, 0x10);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CH1_REG, 0x10);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CH2_REG, 0x10);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CH3_REG, 0x10);

	//set termination
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, ANALOG_DEBUG_REG1, 0xf0);

	//set duty cycle
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, ANALOG_DEBUG_REG3, 0x99);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_FILTER_CTRL1, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_FILTER_CTRL1, c | 0x2a);

	//DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CTRL, 0x01);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_DEBUG_REG, 0x30);

	//for DP link CTS
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_GNS_CTRL_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_GNS_CTRL_REG, c | 0x40);

	//power down  PLL filter
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_FILTER_CTRL, 0x06);

	//set system-state to "wait hot plug"
	DP_TX_Set_System_State(DP_TX_WAIT_HOTPLUG);
	DBGPRINT(1, "<%s:%d> DP_TX_inited\n", __func__, __LINE__);
}

void DP_TX_Variable_Init(void)
{
	BYTE i;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	for (i = 0; i < MAX_BUF_CNT; i++)
		ByteBuf[i] = 0;
	dp_tx_edid_err_code = 0;
	edid_pclk_out_of_range = 0;
	dp_tx_link_config_done = 0;
	dp_tx_final_lane_count = 0;
	dp_tx_test_link_training = 0;
	dp_tx_test_lane_count = 0;
	dp_tx_test_bw = 0;

	//LT_finish = 0;
	dp_tx_video_id = 0;

	//hdcp_rx_repeater = 0;
	dp_tx_hw_hdcp_en = 0;
	dp_tx_hdcp_capable_chk = 0;
	hdcp_auth_pass = 0;
	avmute_enable = 1;
	hdcp_auth_fail_counter = 0;
	hdcp_encryption = 0;
	send_blue_screen = 0;
	HDCP_AUTH_DONE = 0;
	mode_dp = 0;
	mode_hdmi = 0;
	hdcp_err_counter = 0;
	hdmi_tx_RGBorYCbCr = 0;
	hdmi_tx_in_pix_rpt = 0;		//jh 10/27/08
	hdmi_tx_tx_pix_rpt = 0;		//jh 10/27/08
	Hdmi_Edid_RGB30bit = 0;
	Hdmi_Edid_RGB36bit = 0;
	Hdmi_Edid_RGB48bit = 0;
	Hdmi_packet_config.packets_need_config = 0x03;	//new avi infoframe
	Hdmi_packet_config.avi_info.type = 0x82;	//0x82;
	Hdmi_packet_config.avi_info.version = 0x02;	//0x02;
	Hdmi_packet_config.avi_info.length = 0x0d;	//0x0d;
	Hdmi_packet_config.avi_info.pb_byte[1] = 0x01;	//0x00;//0x21;//YCbCr422
	Hdmi_packet_config.avi_info.pb_byte[2] = 0x08;	//0x08;
	Hdmi_packet_config.avi_info.pb_byte[3] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[4] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[5] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[6] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[7] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[8] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[9] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[10] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[11] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[12] = 0x00;
	Hdmi_packet_config.avi_info.pb_byte[13] = 0x00;

	// audio infoframe
	Hdmi_packet_config.audio_info.type = 0x84;
	Hdmi_packet_config.audio_info.version = 0x01;
	Hdmi_packet_config.audio_info.length = 0x0a;
	Hdmi_packet_config.audio_info.pb_byte[1] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[2] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[3] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[4] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[5] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[6] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[7] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[8] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[9] = 0x00;
	Hdmi_packet_config.audio_info.pb_byte[10] = 0x00;

	//dp_tx_hdcp_enable = 0;
	edid_break = 0;
	safe_mode = 0;
	video_config_done = 0;
}

void DP_TX_Timer_Slot1(void)
{
	BYTE c;
	DBGPRINT(1, "<%s:%d> enter! state=%d\n", __func__, __LINE__, (WORD)dp_tx_system_state);
	
	if (dp_tx_system_state == DP_TX_PARSE_EDID) 
	{
		printk("<%s:%d> state=DP_TX_PARSE_EDID\n", __func__, __LINE__);
		if (mode_dp) 
		{
			printk("<%s:%d> in dp mode\n", __func__, __LINE__);
			edid_break = 0;
			DP_TX_LL_CTS_Test();
			if (edid_break) {
				dp_tx_edid_err_code = 0xff;
				printk(" edid corruption");
			}
			//DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
			DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		}
		else 
		{
			printk("<%s:%d> in hdmi mode\n", __func__, __LINE__);

			//force HDMI HPD during EDID reading.
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_CHIP_DEBUG_CTRL1, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_CHIP_DEBUG_CTRL1, c | HDMI_FORCE_HOTPLUG);
			anx9805_dump_reg(HDMI_TX_PORT1_ADDR, HDMI_CHIP_DEBUG_CTRL1, 1);
			DP_TX_FORCE_HPD();

			//printk("force");
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			HDMI_TX_Parse_EDID();
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_CHIP_DEBUG_CTRL1, c & 0xfe);
			anx9805_dump_reg(HDMI_TX_PORT1_ADDR, HDMI_CHIP_DEBUG_CTRL1, 1);
			DP_TX_UNFORCE_HPD();

			//printk("unforce");
			DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		}
	}
	DBGPRINT(1, "<%s:%d> leave\n", __func__, __LINE__);
	return;
}

void DP_TX_Timer_Slot2(void)
{

	//BYTE c;//i,a,j;
	printk("<%s:%d> enter DP_TX_Timer_Slot2 (dp_tx_system_state=%d)\n", __func__, __LINE__, dp_tx_system_state);
	
	if (dp_tx_system_state == DP_TX_LINK_TRAINING) 
	{
		if (DP_TX_LT_Pre_Config())
		{	//pre-config not done
			DBGPRINT(1, "<%s:%d> leave1\n", __func__, __LINE__);
			return;
		}
		//force HPD to ignore IRQ during AUX OP
		DP_TX_FORCE_HPD();
		if (USE_FW_LINK_TRAINING)
		{
			DP_TX_SW_LINK_Process();
		}
		else
		{
			DP_TX_Link_Training();
		}

		//LT_finish = 1;
		//unforce HPD
		DP_TX_UNFORCE_HPD();
	}
	if (dp_tx_system_state == DP_TX_CONFIG_VIDEO) 
	{
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		DP_TX_Config_Video();
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	}
	if (dp_tx_system_state == DP_TX_CONFIG_AUDIO) 
	{
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		DP_TX_Config_Audio();
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	}
	if (dp_tx_system_state == DP_TX_HDCP_AUTHENTICATION) 
	{
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		DP_TX_HDCP_Process();
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	}
	DBGPRINT(1, "<%s:%d> leave2\n", __func__, __LINE__);
	return;
}

void DP_TX_Timer_Slot3(void)
{
	DBGPRINT(1, "<%s:%d> enter\n", __func__, __LINE__);
	printk("DP_TX_Timer_Slot3\n");
	
	if (dp_tx_system_state == DP_TX_PLAY_BACK)
	{
		DP_TX_PlayBack_Process();
	}
	
	DBGPRINT(1, "<%s:%d> leave\n", __func__, __LINE__);
	return;
}

void DP_TX_Config_Video(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	safe_mode = 0;
	if (!video_config_done) 
	{

		/*
		   if(mode_dp)//DP MODE
		   {
		   DP_TX_AUX_DPCDRead_Bytes(0x000001,2,ByteBuf);
		   dp_tx_bw = ByteBuf[0];
		   dp_tx_lane_count = ByteBuf[1] & 0x0f;
		   printk("max_bw = %.2x\n",(WORD)dp_tx_bw);
		   printk("max_lc = %.2x\n",(WORD)dp_tx_lane_count);
		   }
		 */
		if (BIST_EN) 
		{
			printk("<%s:%d> BIST_EN\n", __func__, __LINE__);
			if (!SWITCH4)
			{
				bForceSelIndex = 0;
			}
			else
			{
				bForceSelIndex_backup = bForceSelIndex;
			}
			DP_TX_BIST_Config_CLK_Genarator(bForceSelIndex);
		}
		
		if (mode_dp)			
		{
			//DP MODE
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
			printk("<%s:%d> DP mode\n", __func__, __LINE__);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL1_REG, &c);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL1_REG, c);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL1_REG, &c);
			if (!(c & DP_TX_SYS_CTRL1_DET_STA)) 
			{
				printk("No pclk\n");
				return;
			}
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL2_REG, &c);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL2_REG, c);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL2_REG, &c);
			if (c & DP_TX_SYS_CTRL2_CHA_STA) 
			{
				printk("pclk not stable!\n");
				return;
			}
			DP_TX_AUX_DPCDRead_Bytes(0x000001, 2, ByteBuf);
			dp_tx_bw = ByteBuf[0];
			dp_tx_lane_count = ByteBuf[1] & 0x0f;
			printk("max_bw = %.2x\n", (WORD) dp_tx_bw);
			printk("max_lc = %.2x\n", (WORD) dp_tx_lane_count);
		}
		else					
		{
			//HDMI MODE
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_STATUS, &c);
			printk("<%s:%d> hdmi mode [7a:01=%02x]\n", __func__, __LINE__, (WORD)c);
			if ((c & HDMI_SYS_STATE_CLK_DET) == 0) 
			{
				printk("no clk\n");
				return;
			}
			if ((c & HDMI_SYS_STATE_PLL_LOCK) == 0) 
			{
				DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_CTRL1, &c);
				DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_PLL_CTRL1, c & 0x7f);
				printk("pll not lock\n");
				return;
			}
		}
		
		video_config_done = 1;
		if (mode_dp)			
		{
			//DP MODE
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
			if (!BIST_EN) 
			{
				//use HBR as default to get m/n/pclk
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, 0x0a);
				PCLK_Calc(1);
			}
			
			if (dp_tx_test_link_training) 
			{
				dp_tx_test_link_training = 0;
				dp_tx_bw = dp_tx_test_bw;
				dp_tx_lane_count = dp_tx_test_lane_count;
			}

			/*
			   else
			   {
			   DP_TX_AUX_DPCDRead_Bytes(0x000001,1,ByteBuf);
			   dp_tx_bw = ByteBuf[0];
			   DP_TX_AUX_DPCDRead_Bytes(0x000002,1,ByteBuf);
			   dp_tx_lane_count = ByteBuf[0];
			   dp_tx_lane_count = dp_tx_lane_count & 0x0f;

			   }
			   printk("max_bw = %.2x\n",(WORD)dp_tx_bw);
			   printk("max_lc = %.2x\n",(WORD)dp_tx_lane_count);
			 */
			if (bBW_Lane_Adjust) 
			{
				if (!dp_tx_test_link_training) 
				{

					//if(video_bpc == 8)
					if (DP_TX_Video_Input.bColordepth == COLOR_8)
					{
						BW_LC_Sel();
					}
					
					if (BIST_EN & (dp_tx_lane_count == 0x01))
					{
						DP_TX_BIST_Config_CLK_Genarator(bForceSelIndex);
					}
				}
			}
		}
		
		if (BIST_EN) 
		{
			DP_TX_BIST_Format_Config(bForceSelIndex);
		}
		
		if (!mode_dp)
		{
			HDMI_TX_RepeatTimes_Setting();
			HDMI_TX_CSCandColorDepth_Setting();
		}
		else 
		{
			if (!safe_mode) 
			{
				DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
				anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);

				/*
				   DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);
				   switch(DP_TX_Video_Input.bColordepth)
				   {
				   case COLOR_6:
				   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, c & 0x8f);
				   break;
				   case COLOR_8:
				   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, (c & 0x8f) | 0x10);
				   break;
				   case COLOR_10:
				   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, (c & 0x8f) | 0x20);
				   break;
				   case COLOR_12:
				   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, (c & 0x8f) | 0x30);
				   break;
				   default:
				   break;
				   } */
				anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
				//set Input BPC mode & color space
				DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);
				c &= 0x8c;
				c = c | (DP_TX_Video_Input.bColordepth << 4);
				c |= DP_TX_Video_Input.bColorSpace;
				DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, c);
				DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
				anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
			}
		}

		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		/*
		   if(mode_dp)//DP MODE
		   {
		   DP_TX_Enable_Video_Input();
		   if(!SWITCH1)
		   DP_TX_Set_System_State(DP_TX_CONFIG_AUDIO);
		   else
		   DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
		   return;
		   }
		 */
	}
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	
	//enable video input
	DP_TX_Enable_Video_Input();
	
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	
	if (!mode_dp) 
	{
		DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_CTRL1, &c);
		if ((c & HDMI_VID_STATUS_VID_STABLE) != 0x01) 
		{
			printk("Vid not stable [7a:02]=%02x\n", (WORD)c);
			return;
		}
		DP_TX_Clean_HDCP();

		//reset TMDS link to align 4 channels
		DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_RST_REG, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_RST_REG, c | HDMI_TMDS_CHNL_ALIGN);
		DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_RST_REG, c & ~HDMI_TMDS_CHNL_ALIGN);

		//Enable TMDS clock output
		DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CLKCH_CONFIG_REG, &c);
		if (!(c & HDMI_TMDS_CLKCH_MUTE))
		{
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR,
							HDMI_TMDS_CLKCH_CONFIG_REG, c | HDMI_TMDS_CLKCH_MUTE);
		}
		if (mode_hdmi) 
		{
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
			HDMI_GCP_PKT_Enable();
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
			HDMI_TX_Config_Packet();
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		}

		/*
		   if(!SWITCH1)
		   DP_TX_Set_System_State(DP_TX_CONFIG_AUDIO);
		   //DP_TX_Config_Audio();
		   else
		   DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
		 */
	}
	if (mode_dp)
	{
		DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
	}
	else
	{
		DP_TX_Set_System_State(DP_TX_CONFIG_AUDIO);
	}
	//DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
}

void DP_TX_Config_Audio(void)
{
	BYTE c, fs;
	WORD ACR_N;

	printk("********* SJ :<%s:%d>\n ********", __func__, __LINE__);
	
	if (!SWITCH1) 
	{
		printk("<%s:%d>\n", __func__, __LINE__);
		
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, (c & ~DP_POWERD_AUDIO_REG));	// power up audio
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_CTRL0, (c | SPDIF_AUDIO_CTRL0_SPDIF_IN | SPDIF_AUDIO_CTRL0_SPDIF0));	// enable SPDIF input, Joe20111012
		mdelay(20);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_STATUS0, &c);
		
		if ((c & 0x81) != 0x81) 
		{
			printk("SPDIF_AUDIO_STATUS0 = %.2x\n", (WORD) c);
		}

		/*
		   if ( ( c & SPDIF_AUDIO_STATUS0_CLK_DET ) == 0 )
		   return;

		   if ( ( c & SPDIF_AUDIO_STATUS0_AUD_DET ) == 0 )
		   return;
		 */
		printk("###Config AUD ###");
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AUD_TYPE, 0x84);	// Audio infoframe
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AUD_VER, 0x01);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AUD_LEN, 0x0A);
		
		if (!mode_dp)
		{
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AUD_DB0, 0x71);
		}
		if (!mode_dp) {
			
			if (mode_hdmi) {
				
				printk("<%s:%d>\n", __func__, __LINE__);

				//set ACR N
				DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, SPDIF_AUDIO_STATUS1, &c);
				fs = c & 0xf0;
				fs = fs >> 4;

				// set default value to N
				ACR_N = HDMI_TX_N_48k;
				switch (fs) {
				case (0x00):	//44.1k
					ACR_N = HDMI_TX_N_44k;
					break;
				case (0x02):	//48k
					ACR_N = HDMI_TX_N_48k;
					break;
				case (0x03):	//32k
					ACR_N = HDMI_TX_N_32k;
					break;
				case (0x08):	//88k
					ACR_N = HDMI_TX_N_88k;
					break;
				case (0x0a):	//96k
					ACR_N = HDMI_TX_N_96k;
					break;
				case (0x0c):	//176k
					ACR_N = HDMI_TX_N_176k;
					break;
				case (0x0e):	//192k
					ACR_N = HDMI_TX_N_192k;
					break;
				default:
					break;
				}
				c = ACR_N;
				DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_ACR_N1_SW_REG, c);
				c = ACR_N >> 8;
				DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_ACR_N2_SW_REG, c);
				DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_ACR_N3_SW_REG, 0x00);

				//enable ACR packet
				DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, &c);
				DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, c | 0x01);
				HDMI_AIF_PKT_Enable();
				DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_AUDIO_CTRL1, &c);
				DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR,
								HDMI_AUDIO_CTRL1, (c & ~HDMI_PD_RING_OSC) | HDMI_AUDIO_EN);

				//DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
			}
		}
		else 
		{
			printk("<%s:%d>\n", __func__, __LINE__);

			//printk("enable dp audio");
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, (c | 0x81));	// update the audio info-frame
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUD_CTRL, &c);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUD_CTRL, (c | DP_TX_AUD_CTRL_AUD_EN));	// enable DP audio

			//DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
		}
	}

	/*
	   if(mode_dp)
	   DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
	   else
	   DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
	 */
	mdelay(10);

	//DP_TX_Set_System_State(DP_TX_PLAY_BACK);
	DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);

	//DP_TX_Clean_HDCP();
	//DP_TX_Show_Infomation();
}

void DP_TX_CLOCK_Recovery_Process(void)
{
	BYTE bLane0_1_Status, bLane2_3_Status;
	BYTE c0, c1, c2, c3;
	BYTE bAdjust_Req_0_1, bAdjust_Req_2_3;

	BYTE s1, s2, s3, s4;
	BYTE cCRloopCount;

//      BYTE bStatus_Updated;
	udelay(50);
	if (dp_tx_lane_count == 4) 
	{
		
		DP_TX_AUX_DPCDRead_Bytes(0x000202, 6, ByteBuf);
		bLane0_1_Status = ByteBuf[0];
		bLane2_3_Status = ByteBuf[1];

		#ifdef CR_LOOP
		printk("Reading lane status: lane0_1 = %.2x, lane2_3 = %.2x\n", (WORD) bLane0_1_Status, (WORD) bLane2_3_Status);
		#endif /*  */
		
		bAdjust_Req_0_1 = ByteBuf[4];
		bAdjust_Req_2_3 = ByteBuf[5];
		if (((bLane0_1_Status & 0x11) == 0x11) && ((bLane2_3_Status & 0x11) == 0x11))	//all channel CR done
		{

			#ifdef CR_LOOP
			printk("CR training succeed\n");
			#endif /*  */
			
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x22);

			//Lane 0 setting
			c0 = bAdjust_Req_0_1 & 0x0f;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s1 = c1 | c2;

			//Lane 1 Setting
			c0 = (bAdjust_Req_0_1 & 0xf0) >> 4;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s2 = c1 | c2;

			//Lane2 Setting
			c0 = bAdjust_Req_2_3 & 0x0f;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s3 = c1 | c2;

			//Lane 3 Setting
			c0 = (bAdjust_Req_2_3 & 0xf0) >> 4;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s4 = c1 | c2;
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, s2);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, s3);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, s4);

			//Write 5 bytes
			ByteBuf[0] = 0x22;
			ByteBuf[1] = s1;
			ByteBuf[2] = s2;
			ByteBuf[3] = s3;
			ByteBuf[4] = s4;
			DP_TX_AUX_DPCDWrite_Bytes(0x000102, 5, ByteBuf);
			DP_TX_Set_Link_state(EQ_TRAINING_PROCESS);
		}
		else 
		{
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, &c0);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, &c1);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, &c2);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, &c3);

			#ifdef CR_LOOP
			printk("@@@@@@@@@@@@@@@@@@@@Reading lane adjust request: bAdjust_Req_0_1 = %.2x, bAdjust_Req_2_3 = %.2x\n",
				 (WORD) bAdjust_Req_0_1, (WORD) bAdjust_Req_2_3);
			#endif /*  */
			
			if (((c0 & 0x03) == (bAdjust_Req_0_1 & 0x03)) && ((c0 & 0x18) == ((bAdjust_Req_0_1 & 0x0c) << 1)))	//lane 0 same voltage count
			{
				CRLoop0++;
			}
			if (((c1 & 0x03) == ((bAdjust_Req_0_1 & 0x30) >> 4)) && ((c1 & 0x18) == ((bAdjust_Req_0_1 & 0xc0) >> 3)))	//lane1 same voltage count
			{
				CRLoop1++;
			}
			if (((c2 & 0x03) == (bAdjust_Req_2_3 & 0x03)) && ((c2 & 0x18) == ((bAdjust_Req_2_3 & 0x0c) << 1)))	//lane 0 same voltage count
			{
				CRLoop2++;
			}
			if (((c3 & 0x03) == ((bAdjust_Req_2_3 & 0x30) >> 4)) && ((c3 & 0x18) == ((bAdjust_Req_2_3 & 0xc0) >> 3)))	//lane1 same voltage count
			{
				CRLoop3++;
			}

			//if(dp_tx_bw == 0x06) //for 1.62G
			//cCRloopCount = CR_LOOP_TIME -1;
			//else
			cCRloopCount = CR_LOOP_TIME;

			//if max swing reached or same voltage 5 times, try reduced bit-rate
			//if(((c0&0x03)==0x03)||((c1&0x03)==0x03)||((c2&0x03)==0x03)||((c3&0x03)==0x03)
			//      ||(CRLoop0 == CR_LOOP_TIME)||(CRLoop1 == CR_LOOP_TIME)||(CRLoop2 == CR_LOOP_TIME)||(CRLoop3 == CR_LOOP_TIME))
			if (((c0 & 0x03) == 0x03) || ((c1 & 0x03) == 0x03)
				|| ((c2 & 0x03) == 0x03)
				|| ((c3 & 0x03) == 0x03) || (CRLoop0 == cCRloopCount)
				|| (CRLoop1 == cCRloopCount)
				|| (CRLoop2 == cCRloopCount)
				|| (CRLoop3 == cCRloopCount)) 
			{

				#ifdef CR_LOOP
				printk("CR training failed due to loop > 5");
				#endif /*  */

				//try reduced bit rate
				if (dp_tx_bw == 0x0a)
				{
					dp_tx_bw = 0x06;
					
					#ifdef CR_LOOP
					printk("set to bw %.2x\n", (WORD) dp_tx_bw);
					#endif /*  */
					//DP_TX_HW_LT(dp_tx_bw, dp_tx_lane_count);

					//DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
					DP_TX_Set_Link_state(LINKTRAINING_START);
				}
				else			//already in reduced bit-rate
				{

					//Set to Normal, and enable scramble
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
					ByteBuf[0] = 0x00;
					DP_TX_AUX_DPCDWrite_Bytes(0x000102, 1, ByteBuf);

					#ifdef CR_LOOP
					printk("Set PT0 to terminate LT");
					#endif /*  */

					#ifdef SW_LT_DEBUG
					printk("CR failed, loop > 5");
					#endif /*  */
					
					DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
				}
			}

			else				//increase voltage swing as requested,write an updated value
			{

				//Lane 0 setting
				c0 = bAdjust_Req_0_1 & 0x0f;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s1 = c1 | c2;

				//Lane 1 Setting
				c0 = (bAdjust_Req_0_1 & 0xf0) >> 4;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s2 = c1 | c2;

				//Lane2 Setting
				c0 = bAdjust_Req_2_3 & 0x0f;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s3 = c1 | c2;

				//Lane 3 Setting
				c0 = (bAdjust_Req_2_3 & 0xf0) >> 4;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s4 = c1 | c2;
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, s2);
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, s3);
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, s4);

				//Write 4 bytes
				ByteBuf[0] = s1;
				ByteBuf[1] = s2;
				ByteBuf[2] = s3;
				ByteBuf[3] = s4;
				DP_TX_AUX_DPCDWrite_Bytes(0x000103, 4, ByteBuf);

				#ifdef CR_LOOP
				printk("\n--------------------lane set is being writen to : %.2x\n", (WORD) c3);
				#endif /*  */
				
			}
		}
	}
	else if (dp_tx_lane_count == 2) 
	{
	
		DP_TX_AUX_DPCDRead_Bytes(0x000202, 6, ByteBuf);
		bLane0_1_Status = ByteBuf[0];

		#ifdef CR_LOOP
		printk("Reading lane status: lane0_1 = %.2x, lane2_3 = %.2x\n",
			 (WORD) bLane0_1_Status, (WORD) bLane2_3_Status);

		#endif /*  */
		
		bAdjust_Req_0_1 = ByteBuf[4];

		#ifdef CR_LOOP
		printk("Reading lane status: bLane0_1_Status = %.2x\n", (WORD) bLane0_1_Status);
		#endif /*  */
		
		if ((bLane0_1_Status & 0x11) == 0x11)	//all channel CR done
		{

			#ifdef CR_LOOP
			printk("CR training succeed\n");
			#endif /*  */
			
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x22);

			//Lane 0 setting
			c0 = bAdjust_Req_0_1 & 0x0f;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s1 = c1 | c2;

			//Lane 1 Setting
			c0 = (bAdjust_Req_0_1 & 0xf0) >> 4;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s2 = c1 | c2;
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, s2);

			//Write 3 bytes
			ByteBuf[0] = 0x22;
			ByteBuf[1] = s1;
			ByteBuf[2] = s2;
			DP_TX_AUX_DPCDWrite_Bytes(0x000102, 3, ByteBuf);
			DP_TX_Set_Link_state(EQ_TRAINING_PROCESS);
		}
		else 
		{
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, &c0);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, &c1);

			#ifdef CR_LOOP
			printk("@@@@@@@@@@@@@@@@@@@@@@@@Reading lane adjust request: bAdjust_Req_0_1 = \n",
				 (WORD) bAdjust_Req_0_1);
			#endif /*  */
			
			if (((c0 & 0x03) == (bAdjust_Req_0_1 & 0x03)) && ((c0 & 0x18) == ((bAdjust_Req_0_1 & 0x0c) << 1)))	//lane 0 same voltage count
			{
				CRLoop0++;
			}
			if (((c1 & 0x03) == ((bAdjust_Req_0_1 & 0x30) >> 4)) && ((c1 & 0x18) == ((bAdjust_Req_0_1 & 0xc0) >> 3)))	//lane1 same voltage count
			{
				CRLoop1++;
			}
			
			if (dp_tx_bw == 0x06)	//for 1.62G
			{
				cCRloopCount = CR_LOOP_TIME - 1;
			}
			else
			{
				cCRloopCount = CR_LOOP_TIME;
			}
			
			//if max swing reached or same voltage 5 times, try reduced bit-rate
			//if(((c0&0x03)==0x03)||((c1&0x03)==0x03)
			//      ||(CRLoop0 ==CR_LOOP_TIME)||(CRLoop1 ==CR_LOOP_TIME))
			if (((c0 & 0x03) == 0x03) || ((c1 & 0x03) == 0x03) || (CRLoop0 == cCRloopCount)
				|| (CRLoop1 == cCRloopCount)) 
			{

				#ifdef CR_LOOP
				printk("CR training failed due to loop > 5");
				#endif /*  */

				//try reduced bit rate
				if (dp_tx_bw == 0x0a) 
				{

					//set to reduced bit rate
					dp_tx_bw = 0x06;

					#ifdef CR_LOOP
					printk("set to bw %.2x\n", (WORD) dp_tx_bw);
					#endif /*  */
					
					DP_TX_HW_LT(dp_tx_bw, dp_tx_lane_count);
					DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
				}
				else			//already in reduced bit-rate
				{

					#ifdef CR_LOOP
					printk("CR training failed due to loop > 5");
					#endif /*  */
					
					//Set to Normal, and enable scramble
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
					ByteBuf[0] = 0;
					DP_TX_AUX_DPCDWrite_Bytes(0x000102, 1, ByteBuf);

					#ifdef CR_LOOP
					printk("Set PT0 to terminate LT");
					#endif /*  */

					#ifdef SW_LT_DEBUG
					printk("CR failed, loop > 5");
					#endif /*  */
					
					DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
				}
			}
			else				//increase voltage swing as requested,write an updated value
			{

				//Lane 0 setting
				c0 = bAdjust_Req_0_1 & 0x0f;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s1 = c1 | c2;

				//Lane 1 Setting
				c0 = (bAdjust_Req_0_1 & 0xf0) >> 4;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s2 = c1 | c2;
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, s2);

				//Write 2 bytes
				ByteBuf[0] = s1;
				ByteBuf[1] = s2;
				DP_TX_AUX_DPCDWrite_Bytes(0x000103, 2, ByteBuf);
			}
		}
	}
	else						//one lane
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000202, 6, ByteBuf);
		bLane0_1_Status = ByteBuf[0];
		bAdjust_Req_0_1 = ByteBuf[4];

		#ifdef CR_LOOP
		printk("Reading lane status: bLane0_1_Status = %.2x\n", (WORD) bLane0_1_Status);
		#endif /*  */
		
		if ((bLane0_1_Status & 0x01) == 0x01)	//all channel CR done
		{

			//#ifdef CR_LOOP
			printk("CR succeed");
			//#endif
			
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x22);

			//Lane 0 setting
			c0 = bAdjust_Req_0_1 & 0x0f;	//level
			c1 = c0 & 0x03;		//swing
			c2 = (c0 & 0x0c) << 1;	//pre-emphasis
			if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
			{
				c1 |= 0x04;
				c2 |= 0x20;
			}
			s1 = c1 | c2;
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);

			//Write 2 bytes
			ByteBuf[0] = 0x22;
			ByteBuf[1] = s1;
			DP_TX_AUX_DPCDWrite_Bytes(0x000102, 2, ByteBuf);
			DP_TX_Set_Link_state(EQ_TRAINING_PROCESS);
		}
		else 
		{
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, &c0);
			DP_TX_AUX_DPCDRead_Bytes(0x000206, 1, ByteBuf);
			bAdjust_Req_0_1 = ByteBuf[0];
			if (((c0 & 0x03) == (bAdjust_Req_0_1 & 0x03)) && ((c0 & 0x18) == ((bAdjust_Req_0_1 & 0x0c) << 1)))	//lane 0 same voltage count
			{
				CRLoop0++;
			}

			#ifdef CR_LOOP
			printk("@@@@@@@@@@@@@@@@@@@@@Reading lane adjust request: bAdjust_Req_0_1 = %.2X\n",
				 (WORD) bAdjust_Req_0_1);
			#endif /*  */
			
			if (dp_tx_bw == 0x06)	//for 1.62G
			{
				cCRloopCount = CR_LOOP_TIME - 1;
			}
			else
			{
				cCRloopCount = CR_LOOP_TIME;
			}
			
			//if max swing reached or same voltage 5 times, try reduced bit-rate
			if (((c0 & 0x03) == 0x03)
				|| (CRLoop0 == cCRloopCount))
				//if(((c0&0x03)==0x03)||(CRLoop0 ==CR_LOOP_TIME))
			{

				#ifdef CR_LOOP
				printk("CR training failed due to loop > 5");
				#endif /*  */
				
				//try reduced bit rate
				if (dp_tx_bw == 0x0a) 
				{

					//set to reduced bit rate
					dp_tx_bw = 0x06;
					
					#ifdef CR_LOOP
					printk("set to bw %.2x\n", (WORD) dp_tx_bw);
					#endif /*  */
					
					DP_TX_HW_LT(dp_tx_bw, dp_tx_lane_count);
					DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
				}
				else			//already in reduced bit-rate
				{

					#ifdef CR_LOOP
					printk("CR training failed due to loop > 5");
					#endif /*  */
					
					//Set to Normal, and enable scramble
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
					ByteBuf[0] = 0;
					DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

					#ifdef CR_LOOP
					printk("Set PT0 to terminate LT");
					#endif /*  */

					#ifdef SW_LT_DEBUG
					printk("CR failed, loop > 5");
					#endif /*  */
					
					DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
				}
			}
			else				//increase voltage swing as requested,write an updated value
			{

				//Lane 0 setting
				c0 = bAdjust_Req_0_1 & 0x0f;	//level
				c1 = c0 & 0x03;	//swing
				c2 = (c0 & 0x0c) << 1;	//pre-emphasis
				if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
				{
					c1 |= 0x04;
					c2 |= 0x20;
				}
				s1 = c1 | c2;
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);

				//Write 1 bytes
				ByteBuf[0] = s1;

				//ByteBuf[1] = s2;
				DP_TX_AUX_DPCDWrite_Bytes(0x000103, 1, ByteBuf);
			}
		}
	}
}

void DP_TX_EQ_Process(void)
{
	BYTE bLane0_1_Status, bLane2_3_Status;
	BYTE bAdjust_Req_0_1 = 0, bAdjust_Req_2_3;
	BYTE bLane_align_Status;
	BYTE c0, c1, c2 /*,c3 */ ;
	BYTE s1, s2, s3, s4;
	udelay(80);

	//Read lane cr done
	if (dp_tx_lane_count == 4) 
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000202, 6, ByteBuf);
		bLane0_1_Status = ByteBuf[0];
		bLane2_3_Status = ByteBuf[1];

		#ifdef CR_LOOP
		printk("Reading lane status: lane0_1 = %.2x, lane2_3 = %.2x\n",
			 (WORD) bLane0_1_Status, (WORD) bLane2_3_Status);
		#endif /*  */
		
		bAdjust_Req_0_1 = ByteBuf[4];
		bAdjust_Req_2_3 = ByteBuf[5];
		bLane_align_Status = ByteBuf[2];

		//bLane0_1_Status = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_LANE0_1_STATUS);
		//bLane2_3_Status = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_LANE2_3_STATUS);
		//bLane_align_Status = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED);

		#ifdef SW_LT_DEBUG
		printk("Readding lane status lane0_1 = %.2x, lane2_3 = %.2x, align = %.2x\n",
			 (WORD) bLane0_1_Status, (WORD) bLane2_3_Status, (WORD) bLane_align_Status);
		#endif /*  */
		
		if (((bLane0_1_Status & 0x11) == 0x11) && ((bLane2_3_Status & 0x11) == 0x11))	//all channel CR done
		{
			if (((bLane0_1_Status & 0x66) != 0x66)
				|| ((bLane2_3_Status & 0x66) != 0x66) || (bLane_align_Status & 0x01) != 0x01) 
			{
				bEQLoopcnt++;
				if (bEQLoopcnt > EQ_LOOP_TIME)
				{

					//try reduced bit rate
					if (dp_tx_bw == 0x0a) 
					{
						dp_tx_bw = 0x06;
						DP_TX_Set_Link_state(LINKTRAINING_START);
					}
					else		//already in reduced bit rate
					{

						#ifdef SW_LT_DEBUG
						printk("EQ training failed due to loop > 5");
						#endif /*  */
						
						//Set to Normal, and enable scramble
						DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
						ByteBuf[0] = 0;
						DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

						#ifdef SW_LT_DEBUG
						printk("EQ failed, loop > 5");
						#endif /*  */
						
						DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
					}
				}
				else			//adjust pre-emphasis level
				{

					//bAdjust_Req_0_1 = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_ADJUST_REQUEST_LANE0_1);
					//bAdjust_Req_2_3 = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_ADJUST_REQUEST_LANE2_3);

					#ifdef SW_LT_DEBUG
					printk("@@@@@@@@@@@@Reading lane adjust bAdjust_Req01 = %.2x, bAdjust_Req23 = %.2x\n",
						 (WORD) bAdjust_Req_0_1, (WORD) bAdjust_Req_2_3);
					#endif /*  */

					//Lane 0 setting
					c0 = bAdjust_Req_0_1 & 0x0f;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s1 = c1 | c2;

					//Lane 1 Setting
					c0 = (bAdjust_Req_0_1 & 0xf0) >> 4;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s2 = c1 | c2;

					//Lane2 Setting
					c0 = bAdjust_Req_2_3 & 0x0f;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s3 = c1 | c2;

					//Lane 3 Setting
					c0 = (bAdjust_Req_2_3 & 0xf0) >> 4;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s4 = c1 | c2;
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, s2);
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, s3);
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, s4);

					//Write 4 bytes
					ByteBuf[0] = s1;
					ByteBuf[1] = s2;
					ByteBuf[2] = s3;
					ByteBuf[3] = s4;
					DP_TX_AUX_DPCDWrite_Bytes(0x000103, 4, ByteBuf);

/*
					//set EQ when not lock
					if(((bLane0_1_Status&0x06)!= 0x06)||((bLane_align_Status &0x01) != 0x01))
						DP_TX_LT_VS_EQ_Set(0x00, (bAdjust_Req_0_1&0x0f));
					if(((bLane0_1_Status&0x60)!= 0x60)||((bLane_align_Status &0x01) != 0x01))
						//DP_TX_LT_VS_EQ_Set(0x01, (bAdjust_Req_0_1&0xf0)); bug found at 08.5.25
						DP_TX_LT_VS_EQ_Set(0x01, ((bAdjust_Req_0_1&0xf0) >> 4));
					if(((bLane2_3_Status&0x06)!= 0x06)||((bLane_align_Status &0x01) != 0x01))
						DP_TX_LT_VS_EQ_Set(0x02, (bAdjust_Req_2_3&0x0f));
					if(((bLane2_3_Status&0x60)!= 0x60)||((bLane_align_Status &0x01) != 0x01))
						//DP_TX_LT_VS_EQ_Set(0x03, (bAdjust_Req_2_3&0xf0));bug found at 08.5.25
						DP_TX_LT_VS_EQ_Set(0x03, ((bAdjust_Req_2_3&0xf0) >> 4));
*/
				}
			}
			else				//EQ succeed
			{

				//#ifdef SW_LT_DEBUG
				printk("EQ succeed");
				//#endif
				
				//Set to Normal, and enable scramble
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
				ByteBuf[0] = 0;
				DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

				//#ifdef SW_LT_DEBUG
				printk("SW LT success!\n");
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, &c0);
				printk("dp_tx_final_bw = %.2x\n", (WORD) c0);
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, &dp_tx_final_lane_count);
				printk("dp_tx_final_lane_count = %.2x\n", (WORD) dp_tx_final_lane_count);
				//#endif
				
				DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
			}
		}
		else if (dp_tx_bw == 0x0a)	//not all channel CR done, retry CR training
		{

			//try reduced bit rate and return to CR training
			dp_tx_bw = 0x06;
			DP_TX_Set_Link_state(LINKTRAINING_START);
		}
		else					//already in reduced bit rate
		{

			#ifdef SW_LT_DEBUG
			printk("EQ training failed due to CR loss");
			#endif /*  */
			
			//Set to Normal, and enable scramble
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
			ByteBuf[0] = 0;
			DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

			#ifdef SW_LT_DEBUG
			printk("LT failed0!\n");
			#endif /*  */
			
			DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
		}
	}
	else if (dp_tx_lane_count == 2) 
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000202, 6, ByteBuf);
		bLane0_1_Status = ByteBuf[0];

		//bLane2_3_Status = ByteBuf[1];

		#ifdef CR_LOOP
		printk("Reading lane status: lane0_1 = %.2x, lane2_3 = %.2x\n",
			 (WORD) bLane0_1_Status, (WORD) bLane2_3_Status);
		#endif /*  */
		
		bAdjust_Req_0_1 = ByteBuf[4];

		//bAdjust_Req_2_3 = ByteBuf[5];
		bLane_align_Status = ByteBuf[2];

		//bLane0_1_Status = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_LANE0_1_STATUS);
		//bLane_align_Status = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED);
		#ifdef SW_LT_DEBUG
		printk("Readding lane status lane0_1 = %.2x, align = %.2x\n",
			 (WORD) bLane0_1_Status, (WORD) bLane_align_Status);
		#endif /*  */
		
		if ((bLane0_1_Status & 0x11) == 0x11)	//all channel CR done
		{
			if (((bLane0_1_Status & 0x66) != 0x66) || (bLane_align_Status & 0x01) != 0x01)	//not all locked
			{
				bEQLoopcnt++;
				if (bEQLoopcnt > EQ_LOOP_TIME) 
				{

					//try reduced bit rate
					if (dp_tx_bw == 0x0a) 
					{
						dp_tx_bw = 0x06;
						DP_TX_Set_Link_state(LINKTRAINING_START);
					}
					else		//already in reduced bit rate
					{

						#ifdef SW_LT_DEBUG
						printk("EQ training failed due to loop > 5");
						#endif /*  */
						
						//Set to Normal, and enable scramble
						DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
						ByteBuf[0] = 0;
						DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

						#ifdef SW_LT_DEBUG
						printk("EQ failed, loop > 5");
						#endif /*  */
						
						DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
					}
				}
				else			//adjust pre-emphasis level
				{

					//bAdjust_Req_0_1 = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_ADJUST_REQUEST_LANE0_1);

					#ifdef SW_LT_DEBUG
					printk("@@@@@@@@@@@@Reading lane adjust bAdjust_Req01 = %.2x, \n",
						 (WORD) bAdjust_Req_0_1);
					#endif /*  */
					
					//Lane 0 setting
					c0 = bAdjust_Req_0_1 & 0x0f;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s1 = c1 | c2;

					//Lane 1 Setting
					c0 = (bAdjust_Req_0_1 & 0xf0) >> 4;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s2 = c1 | c2;
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, s2);

					//Write 2 bytes
					ByteBuf[0] = s1;
					ByteBuf[1] = s2;
					DP_TX_AUX_DPCDWrite_Bytes(0x000103, 2, ByteBuf);

					/*
					   //set EQ when not lock
					   if(((bLane0_1_Status&0x06)!= 0x06)||((bLane_align_Status &0x01) != 0x01))
					   DP_TX_LT_VS_EQ_Set(0x00, (bAdjust_Req_0_1&0x0f));
					   if(((bLane0_1_Status&0x60)!= 0x60)||((bLane_align_Status &0x01) != 0x01))
					   //DP_TX_LT_VS_EQ_Set(0x01, (bAdjust_Req_0_1&0xf0)); bug found at 08.5.28
					   DP_TX_LT_VS_EQ_Set(0x01, ((bAdjust_Req_0_1&0xf0) >> 4));
					 */
				}
			}
			else				//EQ succeed
			{

				#ifdef SW_LT_DEBUG
				printk("EQ training succeed!\n");
				#endif /*  */
				
				//Set to Normal, and enable scramble
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
				ByteBuf[0] = 0;
				DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

				//#ifdef SW_LT_DEBUG
				printk("SW LT success!\n");
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, &c0);
				printk("dp_tx_final_bw = %.2x\n", (WORD) c0);
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, &dp_tx_final_lane_count);
				printk("dp_tx_final_lane_count = %.2x\n", (WORD) dp_tx_final_lane_count);
				//#endif
				
				DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
			}
		}
		else if (dp_tx_bw == 0x0a) 
		{
			//try reduced bit rate and return to CR training
			dp_tx_bw = 0x06;
			DP_TX_Set_Link_state(LINKTRAINING_START);
		}
		else					//already in reduced bit rate
		{

			#ifdef SW_LT_DEBUG
			printk("EQ training failed due to CR loss");
			#endif /*  */

			//Set to Normal, and enable scramble
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
			ByteBuf[0] = 0;
			DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

			#ifdef SW_LT_DEBUG
			printk("LT failed1!\n");
			#endif /*  */
			
			DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
		}
	}
	////////////////////////////////////////////////////////////////////////////////////////
	else						//one lane
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000202, 1, ByteBuf);
		bLane0_1_Status = ByteBuf[0];

		#ifdef SW_LT_DEBUG
		printk("########### Reading DPCD_LANE0_1_STATUS = %.2x\n", (WORD) bLane0_1_Status);
		#endif /*  */
		
		DP_TX_AUX_DPCDRead_Bytes(0x000204, 1, ByteBuf);
		bLane_align_Status = ByteBuf[0];

		#ifdef SW_LT_DEBUG
		printk("~~~~~~~~~~~ Reading DPCD_LANE_ALIGN_STATUS_UPDATED = %.2x\n", (WORD) bLane_align_Status);
		#endif /*  */
		
		if ((bLane0_1_Status & 0x01) == 0x01)	//all channel CR done
		{
			if (((bLane0_1_Status & 0x06) != 0x06) || (bLane_align_Status & 0x01) != 0x01)	//not all locked
			{
				bEQLoopcnt++;
				if (bEQLoopcnt > EQ_LOOP_TIME) 
				{

					//try reduced bit rate
					if (dp_tx_bw == 0x0a) 
					{
						dp_tx_bw = 0x06;
						DP_TX_Set_Link_state(LINKTRAINING_START);
					}
					else 
					{

						#ifdef SW_LT_DEBUG
						printk("EQ training failed due to loop > 5");
						#endif /*  */
						
						//Set to Normal, and enable scramble
						DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
						ByteBuf[0] = 0;
						DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

						#ifdef SW_LT_DEBUG
						printk("1write pt0");
						printk("Set PT0 to terminate LT");
						#endif /*  */
						
						#ifdef SW_LT_DEBUG
						printk("EQ failed, loop > 5");
						#endif /*  */
						
						DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
					}
				}
				else			//adjust pre-emphasis level
				{

					//bAdjust_Req_0_1 = DP_TX_AUX_DPCDRead_Byte(0x00,0x02,DPCD_ADJUST_REQUEST_LANE0_1);
					#ifdef SW_LT_DEBUG
					printk("%%%%%%%%%% Reading DPCD_ADJUST_REQUEST_LANE0_1 = %.2x\n", (WORD) bAdjust_Req_0_1);
					#endif /*  */
					
					//Lane 0 setting
					c0 = bAdjust_Req_0_1 & 0x0f;	//level
					c1 = c0 & 0x03;	//swing
					c2 = (c0 & 0x0c) << 1;	//pre-emphasis
					if (((c0 & 0x0c) == 0x0c) || ((c0 & 0x03) == 0x03))	//maxswing reached or max pre-emphasis
					{
						c1 |= 0x04;
						c2 |= 0x20;
					}
					s1 = c1 | c2;
					DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, s1);

					//Write 1 bytes
					ByteBuf[0] = s1;

					//ByteBuf[1] = s2;
					DP_TX_AUX_DPCDWrite_Bytes(0x000103, 1, ByteBuf);

					/*
					   //set EQ when not lock
					   if(((bLane0_1_Status&0x06)!= 0x06)||((bLane_align_Status &0x01) != 0x01))
					   DP_TX_LT_VS_EQ_Set(0x00, (bAdjust_Req_0_1&0x0f));
					 */
				}
			}
			else				//EQ succeed
			{

				#ifdef SW_LT_DEBUG
				printk("EQ training succeed!\n");
				#endif /*  */
				
				//Set to Normal, and enable scramble
				DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
				ByteBuf[0] = 0;
				DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

				#ifdef SW_LT_DEBUG
				printk("2write pt0");
				printk("Set PT0 to terminate LT");
				#endif /*  */

				//#ifdef SW_LT_DEBUG
				printk("SW LT success!\n");
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, &c0);
				printk("dp_tx_final_bw = %.2x\n", (WORD) c0);
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, &dp_tx_final_lane_count);
				printk("dp_tx_final_lane_count = %.2x\n", (WORD) dp_tx_final_lane_count);
				//#endif
				
				DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
			}
		}
		else if (dp_tx_bw == 0x0a) 
		{
			//try reduced bit rate and return to CR training
			dp_tx_bw = 0x06;
			DP_TX_Set_Link_state(LINKTRAINING_START);
		}
		else					//already in reduced bit rate
		{

			#ifdef SW_LT_DEBUG
			printk("EQ training failed due to CR loss");
			#endif /*  */
			
			//Set to Normal, and enable scramble
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x00);
			ByteBuf[0] = 0;
			DP_TX_AUX_DPCDWrite_Bytes(0x000102, 0x01, ByteBuf);

			#ifdef SW_LT_DEBUG
			printk("3write pt0");
			printk("Set PT0 to terminate LT");

			printk("LT failed2!\n");
			#endif /*  */
			
			DP_TX_Set_Link_state(LINKTRAINING_FINISHED);
		}
	}
}

void DP_TX_Link_Start_Process(void)
{

	//BYTE c1;
	ByteBuf[0] = 0x01;
	DP_TX_AUX_DPCDWrite_Bytes(0x000600, 0x01, ByteBuf);

	//Set TX and RX BW and Lane count
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, dp_tx_bw);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, dp_tx_lane_count);
	ByteBuf[0] = dp_tx_bw;
	ByteBuf[1] = dp_tx_lane_count;
	DP_TX_AUX_DPCDWrite_Bytes(0x000100, 2, ByteBuf);

	//Set TX
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, 0x00);

	//Set RX
	//Start CR training, set LT PT1,enable scramble
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x21);
	ByteBuf[0] = 0x21;
	ByteBuf[1] = 0x00;
	ByteBuf[2] = 0x00;
	ByteBuf[3] = 0x00;
	ByteBuf[4] = 0x00;
	DP_TX_AUX_DPCDWrite_Bytes(0x000102, 5, ByteBuf);

	//initialize the CR loop counter
	CRLoop0 = 0;
	CRLoop1 = 0;
	CRLoop2 = 0;
	CRLoop3 = 0;
	bEQLoopcnt = 0;
	bMaxSwingCnt = 0;
	DP_TX_Set_Link_state(CLOCK_RECOVERY_PROCESS);
}

BYTE Sink_Is_DP_HDMI(void)
{
	BYTE c, c1;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, c & 0xfc);
	DP_TX_AUX_DPCDRead_Bytes(0x000000, 1, ByteBuf);
	c1 = ByteBuf[0];
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_STATUS, &c);

	//printk("c = %.2x, c1 = %.2x\n",(WORD)c,(WORD)c1);
	c &= 0x0f;
	if ((c1 != 0x00) && (c == 0x00)) {
		
		mode_dp = 1;			//dp mode
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, c & 0xfe);
		printk("DP mode");

		//set video FIFO threshold as 0x0a, only work in DP mode
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_FIFO_THRESHOLD, 0x0a);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_FIFO_THRESHOLD, 0x2a);
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_DEBUG_1_REG, &c);
		if (!(c & DP_TX_DEBUG_1_PLL_LOCK)) {
			printk("PLL not lock!");
			return 1;
		}
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_TEST_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_TEST_REG, c | 0x20);

		//mdelay(2);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_TEST_REG, (c & ~0x20));
		return 0;
	}
	//AUX CH reset
	DP_TX_RST_AUX();
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, c | 0x01);
	printk("HDMI/DVI mode");
	DP_TX_RST_DDC();
	return 0;

	/*
	   DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_STATUS, &c);
	   c = c & HDMI_SYS_STATE_HP;
	   if((HDMI_Parse_EDIDHeader() == 1) || (c)
	   {
	   mode_dp_or_hdmi = 0;//hdmi\dvi mode
	   DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, c | 0x01);
	   printk("HDMI/DVI mode");
	   return 0;
	   }
	   else
	   {
	   DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_ACC_CMD_REG, 0x00);
	   DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, &c);
	   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c | 0x10);
	   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c);
	   DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	   DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, c & 0xfc);

	   printk("unknown device attached");
	   return 1;
	   } */
}

BYTE HDMI_TX_Config_Packet(void)
{
	BYTE exe_result = 0x00;
	BYTE info_packet_sel;
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	info_packet_sel = Hdmi_packet_config.packets_need_config;
	printk("config packet");

	printk("HDMI____info_packet_sel = 0x%.2x\n",(WORD) info_packet_sel);

	// New packet?
	if (info_packet_sel != 0x00) 
	{

		// avi infoframe
		printk("<%s:%d> avi infoframe\n", __func__, __LINE__);
		if (info_packet_sel & HDMI_TX_avi_sel) 
		{
			c = Hdmi_packet_config.avi_info.pb_byte[1];	//color space
			c &= 0x9f;
			c |= (hdmi_tx_RGBorYCbCr << 5);

			printk("HDMI____In config packet state, color space = %x\n", hdmi_tx_RGBorYCbCr);
			Hdmi_packet_config.avi_info.pb_byte[1] = c | 0x10;
			c = Hdmi_packet_config.avi_info.pb_byte[4];	// vid ID
			c = c & 0x80;

			printk("hdmi_tx_video_id is  %.2x\n", (WORD)hdmi_tx_video_id);
			Hdmi_packet_config.avi_info.pb_byte[4] = c | hdmi_tx_video_id;
			c = Hdmi_packet_config.avi_info.pb_byte[5];	//repeat times
			c = c & 0xf0;
			c |= (hdmi_tx_tx_pix_rpt & 0x0f);
			Hdmi_packet_config.avi_info.pb_byte[5] = c;

			printk("HDMI____config avi infoframe packet.");

			// Disable AVI repeater
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR,
							HDMI_INFO_PKTCTRL1_REG, c & ~HDMI_INFO_PKTCTRL1_AVI_RPT);

			// Enable?wait:go
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, &c);
			if (c & HDMI_INFO_PKTCTRL1_AVI_EN) 
			{
				return exe_result;
			}
			// load packet data to regs
			HDMI_TX_Load_Infoframe(HDMI_avi_infoframe, &(Hdmi_packet_config.avi_info));

			// Enable and repeater
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, c | 0x30);

			// complete avi packet
			Hdmi_packet_config.packets_need_config &= ~HDMI_TX_avi_sel;
		}
		// audio infoframe
		if (info_packet_sel & HDMI_TX_audio_sel) 
		{
			printk("<%s:%d> audio infoframe\n", __func__, __LINE__);

			printk("HDMI____config audio infoframe packet.");

			// Disable repeater
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL2_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR,
							HDMI_INFO_PKTCTRL2_REG, c & ~HDMI_INFO_PKTCTRL2_AIF_RPT);

			// Enable?wait:go
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL2_REG, &c);
			if (c & HDMI_INFO_PKTCTRL2_AIF_EN) 
			{
				return exe_result;
			}
			// load packet data to regs
			HDMI_TX_Load_Infoframe(HDMI_audio_infoframe, &(Hdmi_packet_config.audio_info));

			// Enable and repeater
			HDMI_AIF_PKT_Enable();
			Hdmi_packet_config.packets_need_config &= ~HDMI_TX_audio_sel;
		}
		// config other 4 packets
	}
	//DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
	//HDMI_HDCP_enable = 1;
	return exe_result;
}

BYTE DP_TX_LT_Pre_Config()
{
	BYTE c, legel_bw, legel_lc;
	
	legel_bw = legel_lc = 1;
	
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_DEBUG_1_REG, &c);
	if (!(c & DP_TX_DEBUG_1_PLL_LOCK)) 
	{
		printk("PLL not lock!");
		return 1;
	}
	
	if (!dp_tx_link_config_done) 
	{
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_POWER_DOWN_REG, 0x00);

		/*if(dp_tx_test_link_training)
		   {
		   dp_tx_test_link_training = 0;
		   dp_tx_bw = dp_tx_test_bw;
		   dp_tx_lane_count = dp_tx_test_lane_count;
		   }
		   else
		   {
		   dp_tx_bw = DP_TX_AUX_DPCDRead_Byte(0x00, 0x00,DPCD_MAX_LINK_RATE);
		   dp_tx_lane_count = DP_TX_AUX_DPCDRead_Byte(0x00, 0x00,DPCD_MAX_LANE_COUNT);
		   dp_tx_lane_count = dp_tx_lane_count & 0x0f;
		   }
		   printk("max_bw = %.2x\n",(WORD)dp_tx_bw);
		   printk("max_lc = %.2x\n",(WORD)dp_tx_lane_count); */
		if ((dp_tx_bw != 0x0a) && (dp_tx_bw != 0x06))
		{
			legel_bw = 0;
		}
		if (  (dp_tx_lane_count != 0x01) && (dp_tx_lane_count != 0x02)
		 && (dp_tx_lane_count != 0x04)  )
		{
			legel_lc = 0;
		}
		if ((legel_bw == 0) || (legel_lc == 0))
		{
			return 1;
		}

		//M value select, initialed as clock without downspreading
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_M_CALCU_CTRL, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_M_CALCU_CTRL, c & 0xfe);
		
		if (dp_tx_ssc_enable)
		{
			DP_TX_CONFIG_SSC();
		}
		if (dp_tx_lane_count == 0x01)
		{
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_POWER_DOWN_REG, 0x0e);
		}
		else if (dp_tx_lane_count == 0x02)
		{
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_POWER_DOWN_REG, 0x0c);
		}
		else
		{
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_POWER_DOWN_REG, 0x00);
		}
		
		//enable or disable the 8b/10b encoder
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_DEBUG_REG, &c);
		if (RST_ENCODER) 
		{
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_DEBUG_REG, (c & (~DP_TX_DIS_AUTO_RST_ENCODER)));
		}
		else 
		{
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_DEBUG_REG, (c | DP_TX_DIS_AUTO_RST_ENCODER));
		}
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, 0x00);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, 0x00);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, 0x00);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, 0x00);
		DP_TX_HDCP_Encryption_Disable();
		dp_tx_link_config_done = 1;
	}
	
	return 0;					// pre-config done
}

void DP_TX_Link_Training(void)
{
	BYTE c;

	//printk("LT..");
	printk("<%s:%d> LT...\n", __func__, __LINE__);

	//set bandwidth
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, dp_tx_bw);

	//set lane conut
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, dp_tx_lane_count);

	/*
	   DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_TEST_REG, &c);
	   DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_TEST_REG, c | 0x20);
	   mdelay(2);
	   DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_TEST_REG, (c & ~0x20));
	 */
	ByteBuf[0] = 0x01;
	DP_TX_AUX_DPCDWrite_Bytes(0x000600, 1, ByteBuf);	//set sink to D0 mode.
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_TRAINING_CTRL_REG, DP_TX_LINK_TRAINING_CTRL_EN);
	mdelay(5);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_TRAINING_CTRL_REG, &c);
	while (c & DP_TX_LINK_TRAINING_CTRL_EN)
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_TRAINING_CTRL_REG, &c);
	if (c & 0x70) {
		c = (c & 0x70) >> 4;
		printk("HW LT failed, ERR code = %.2x\n", (WORD) c);

		//return;//keep return. added at 08.5.28
	}
	DP_TX_EnhaceMode_Set();		//guo .add 08.11.14
/*
    if(c & 0x70)
    {
        c = (c & 0x70) >> 4;
        printk("Link training error! Return error code = %.2x\n",(WORD)c);
		//if(c == 0x01)
		{
			//printk("Much deff error!");
			if(dp_tx_bw == 0x0a)
			{
				printk("Force to RBR");
				DP_TX_RST_AUX();
				dp_tx_bw = 0x06;
				DP_TX_HW_LT(dp_tx_bw, dp_tx_lane_count);
			}
		}
    }
     */
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, &dp_tx_final_lane_count);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, &c);
	printk("LANE0_SET = %.2x\n", (WORD) c);
	if (dp_tx_final_lane_count > 1) {
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, &c);
		printk("LANE1_SET = %.2x\n", (WORD) c);
	}
	if (dp_tx_final_lane_count > 2) {
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, &c);
		printk("LANE2_SET = %.2x\n", (WORD) c);
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, &c);
		printk("LANE3_SET = %.2x\n", (WORD) c);
	}
	printk("HW LT done");

	//DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
	//DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
	DP_TX_Set_System_State(DP_TX_CONFIG_AUDIO);
	return;
}

void DP_TX_SW_LINK_Process(void)
{
	BYTE bLinkFinished;
	BYTE c;
		
	eSW_Link_state = LINKTRAINING_START;
	bLinkFinished = 0;
	while (!bLinkFinished) {
		
		switch (eSW_Link_state) {
		case LINKTRAINING_START:
			DP_TX_Link_Start_Process();
			break;
		case CLOCK_RECOVERY_PROCESS:
			DP_TX_CLOCK_Recovery_Process();
			break;
		case EQ_TRAINING_PROCESS:
			DP_TX_EQ_Process();
			break;
		case LINKTRAINING_FINISHED:
			bLinkFinished = 1;
			DP_TX_EnhaceMode_Set();
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, &c);
			printk("LANE0_SET = %.2x\n", (WORD) c);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, &c);
			printk("LANE1_SET = %.2x\n", (WORD) c);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, &c);
			printk("LANE2_SET = %.2x\n", (WORD) c);
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, &c);
			printk("LANE3_SET = %.2x\n", (WORD) c);

			//DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
			DP_TX_Set_System_State(DP_TX_HDCP_AUTHENTICATION);
			DP_TX_Enable_Video_Input();
			break;
		default:
			break;
		}
	}
	
}

BYTE HDMI_TX_Load_Infoframe(HDMI_PACKET_TYPE member, HDMI_INFOFRAME_STRUCT * p)
{
	BYTE exe_result = 0x00;
	BYTE address[6] = {
		0x70, 0x83, 0x91, 0xb0, 0xa0, 0xc0
	};
	BYTE i;
	BYTE c;

	printk("<%s:%d> HDMI____hdmi load infoframe data\n", __func__, __LINE__);
	//printk("HDMI____hdmi load infoframe data");
	p->pb_byte[0] = HDMI_TX_Checksum(p);

	// write infoframe to according regs
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, address[member], p->type);

	//printk("HDMI____p->type = 0x%.2x\n",(WORD) p->type);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, address[member] + 1, p->version);

	//printk("HDMI____p->version = 0x%.2x\n",(WORD) p->version);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, address[member] + 2, p->length);

	//printk("HDMI____p->length = 0x%.2x\n",(WORD) p->length);
	for (i = 0; i <= p->length; i++) 
	{
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, address[member] + 3 + i, p->pb_byte[i]);

		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, address[member]+3+i, &c);
		//printk("HDMI____pb-byte = 0x%.2x\n",(WORD) c);
	}
	return exe_result;
}

BYTE HDMI_TX_Checksum(HDMI_INFOFRAME_STRUCT * p)
{
	BYTE checksum = 0x00;
	BYTE i;
	checksum = p->type + p->length + p->version;
	for (i = 1; i <= p->length; i++) 
	{
		checksum += p->pb_byte[i];
	}
	checksum = ~checksum;
	checksum += 0x01;
	return checksum;
}
void HDMI_TX_RepeatTimes_Setting(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	if ((hdmi_tx_video_id == hdmi_tx_V720x480i_60Hz_16x9)
		|| (hdmi_tx_video_id == hdmi_tx_V720x576i_50Hz_16x9)
		|| (hdmi_tx_video_id == hdmi_tx_V720x480i_60Hz_4x3)
		|| (hdmi_tx_video_id == hdmi_tx_V720x576i_50Hz_4x3))
		hdmi_tx_tx_pix_rpt = 1;

	else
		hdmi_tx_tx_pix_rpt = 0;

	printk("hdmi_tx_tx_pix_rpt: %.2x\n", (WORD)hdmi_tx_tx_pix_rpt);
	printk("hdmi_tx_video_id: %.2x\n", (WORD)hdmi_tx_video_id);

	//set input pixel repeat times
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL6_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL6_REG,
					((c & 0xcf) | (hdmi_tx_in_pix_rpt << 4)));

	//set link pixel repeat times
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_VID_CTRL2, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_VID_CTRL2, ((c & 0xcf) | (hdmi_tx_tx_pix_rpt << 4)));
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_VID_CTRL2, &c);

	printk("   HDMI_VID_CTRL2 1 is  %.2x \n", (WORD)c);
}

void DP_TX_Wait_AUX_Finished(void)
{
	BYTE c;
	BYTE cnt;
	cnt = 0;
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	while (c & 0x01) {

		//mdelay(20);
		cnt++;
		if (cnt == 100) {
			printk("aux break");
			DP_TX_RST_AUX();
			cnt = 0;
			break;
		}
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	}
}

void DP_TX_EDID_Read(void)
{
	BYTE c, i, edid_block = 0;
	
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_7_0_REG, 0x50);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_15_8_REG, 0);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_19_16_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_19_16_REG, c & 0xf0);
	checksum = 0;
	edid_block = DP_TX_Get_EDID_Block();
	edid_block = 8 * (edid_block + 1);
	for (i = 0; i < edid_block; i++) {
		if (!edid_break)
			DP_TX_AUX_EDIDRead_Byte(i * 16);
	}

	printk("EDID error code = %.2x\n",(WORD)dp_tx_edid_err_code);

	//clear the address only command
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, (c & 0xfd));
	DP_TX_RST_AUX();
	DP_TX_AUX_DPCDRead_Bytes(0x000218, 1, ByteBuf);

	//test_vector = ;
	if (ByteBuf[0] & 0x04)		//test edid
	{
		printk("check sum = %.2x\n", (WORD) checksum);

		//c = DP_TX_AUX_DPCDRead_Byte(0x00, 0x02,0x18);
		//printk("c = %.2x\n", (WORD)c);
		//if(c&0x04)
		{
			ByteBuf[0] = checksum;
			DP_TX_AUX_DPCDWrite_Bytes(0x000261, 1, ByteBuf);

			/*c = DP_TX_AUX_DPCDRead_Byte(0x00, 0x02,0x61);
			   while(c != checksum)
			   {
			   DP_TX_AUX_DPCDWrite_Byte(0x00,0x02,0x61,checksum);
			   c = DP_TX_AUX_DPCDRead_Byte(0x00, 0x02,0x61);
			   }
			   printk("check sum write value = %.2x\n", (WORD)c); */
			ByteBuf[0] = 0x04;
			DP_TX_AUX_DPCDWrite_Bytes(0x000260, 1, ByteBuf);

			/*c = DP_TX_AUX_DPCDRead_Byte(0x00, 0x02,0x60);
			   while((c & 0x04) == 0)
			   {
			   DP_TX_AUX_DPCDWrite_Byte(0x00,0x02,0x60,0x04);
			   c = DP_TX_AUX_DPCDRead_Byte(0x00, 0x02,0x60);
			   }
			   printk("check_sum_write flag = %.2x\n", (WORD)c); */
		}
		printk("Test EDID done");
	}
}

BYTE HDMI_Parse_EDIDHeader(void)
{
	BYTE i, temp, c;
	temp = 0;

#ifdef	QUICK_BOOT_NO_CHECK
	printk("<%s:%d> Assume good header\n", __func__, __LINE__);
	HDMI_InitDDC_Read();
	return 1;
#else
	printk("<%s:%d>\n", __func__, __LINE__);
	HDMI_InitDDC_Read();

	// the EDID header should begin with 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00
	c = HDMI_Read_EDID_BYTE(0);
	if (c == 0x00) {
		c = HDMI_Read_EDID_BYTE(7);
		if (c == 0x00) {
			for (i = 1; i < 7; i++) {
				c = HDMI_Read_EDID_BYTE(i);
				if (c != 0xff) {
					temp = 0x01;
					break;
				}
			}
		}
		else {
			temp = 0x01;
		}
	}
	else {
		temp = 0x01;
	}
	
	if (temp == 0x01) {
		printk("Bad header\n");
		dp_tx_edid_err_code = 0x01;
		return 0;
	}
	else {
		printk("Good header\n");
		return 1;
	}
#endif
}

BYTE DP_TX_Get_EDID_Block(void)
{
	BYTE c;
	DP_TX_AUX_WR(0x00);
	DP_TX_AUX_RD(0x01);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_0_REG, &c);

	printk("[a0:00] = %.2x\n", (WORD)c);
	DP_TX_AUX_WR(0x7e);
	DP_TX_AUX_RD(0x01);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_0_REG, &c);
	if (c == 0) {
		printk("EDID Block = 1");
		return 0;
	}
	printk("EDID Block = 2");
	return 1;
}

void DP_TX_AUX_WR(BYTE offset)
{
	BYTE c, cnt;
	cnt = 0;

	//load offset to fifo
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_0_REG, offset);

	//set I2C write com 0x04 mot = 1
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG, 0x04);

	//enable aux
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, 0x01);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	while (c & 0x01) {
		mdelay(10);
		cnt++;

		//printk("cntwr = %.2x\n",(WORD)cnt);
		if (cnt == 10) {
			printk("write break");
			DP_TX_RST_AUX();
			cnt = 0;
			edid_break = 1;
			break;
		}
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	}
}

void DP_TX_AUX_RD(BYTE len_cmd)
{
	BYTE c, cnt;
	cnt = 0;
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG, len_cmd);

	//enable aux
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, 0x01);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	while (c & 0x01) {
		mdelay(10);
		cnt++;

		//printk("cntrd = %.2x\n",(WORD)cnt);
		if (cnt == 10) {
			printk("read break");
			DP_TX_RST_AUX();
			edid_break = 1;
			break;
		}
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	}
}

void DP_TX_AUX_EDIDRead_Byte(BYTE offset)
{
	BYTE c, i, edid[16], data_cnt, cnt;

	//printk("***************************offset = %.2x\n", (unsigned int)offset);
	cnt = 0;
	DP_TX_AUX_WR(offset);		//offset
	if ((offset == 0x00) || (offset == 0x80))
	{
		checksum = 0;
	}
	
	DP_TX_AUX_RD(0xf5);			//set I2C read com 0x05 mot = 1 and read 16 bytes
	data_cnt = 0;
	while (data_cnt < 16) {
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_COUNT_REG, &c);
		c = c & 0x1f;

		//printk("cnt_d = %.2x\n",(WORD)c);
		if (c != 0) {
			for (i = 0; i < c; i++) {
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_0_REG + i, &edid[i + data_cnt]);

				//printk("edid[%.2x] = %.2x\n",(WORD)(i + offset),(WORD)edid[i + data_cnt]);
				checksum = checksum + edid[i + data_cnt];
			}
		}
		else {
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG, 0x01);

			//enable aux
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, 0x03);	//set address only
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
			while (c & 0x01) {
				mdelay(2);
				cnt++;
				if (cnt == 10) {

					//printk("read break");
					DP_TX_RST_AUX();
					edid_break = 1;
					return;
				}
				DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
			}

			//printk("cnt_d = 0, break");
			dp_tx_edid_err_code = 0xff;
			return;				// for fixing bug leading to dead lock in loop "while(data_cnt < 16)"
		}
		data_cnt = data_cnt + c;
		
		if (data_cnt < 16)		// 080610. solution for handle case ACK + M byte
		{
			//DP_TX_AUX_WR(offset);
			DP_TX_RST_AUX();
			mdelay(10);
			c = 0x05 | ((0x0f - data_cnt) << 4);	//Read MOT = 1
			DP_TX_AUX_RD(c);
			printk("M < 16");
		}
	}
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG, 0x01);

	//enable aux
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, 0x03);	//set address only to stop EDID reading
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	while (c & 0x01)
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);

	//printk("***************************offset %.2x reading completed\n", (unsigned int)offset);
	if (offset == 0x00) {
		if (  (edid[0] == 0) && (edid[7] == 0) && (edid[1] == 0xff)
		 && (edid[2] == 0xff) && (edid[3] == 0xff) && (edid[4] == 0xff)
		 && (edid[5] == 0xff) && (edid[6] == 0xff)  )
		{
			printk("Good EDID header!");
		}
		else 
		{
			printk("Bad EDID header!");
			dp_tx_edid_err_code = 0x01;
		}
	}
	else if (offset == 0x30) 
	{
		for (i = 0; i < 10; i++)
		{
			DP_TX_EDID_PREFERRED[i] = edid[i + 6];	//edid[0x36]~edid[0x3f]
		}
	}
	else if (offset == 0x40) 
	{
		for (i = 0; i < 8; i++)
		{
			DP_TX_EDID_PREFERRED[10 + i] = edid[i];	//edid[0x40]~edid[0x47]
		}
	}
	else if (offset == 0x70) 
	{
		checksum = checksum - edid[15];
		checksum = ~checksum + 1;
		if (checksum != edid[15]) {
			printk("Bad EDID check sum1!");
			dp_tx_edid_err_code = 0x02;
			checksum = edid[15];
		}
		else
		{
			printk("Good EDID check sum1!");
		}
	}
	else if (offset == 0xf0) 
	{
		checksum = checksum - edid[15];
		checksum = ~checksum + 1;
		if (checksum != edid[15]) {
			printk("Bad EDID check sum2!");
			dp_tx_edid_err_code = 0x02;
		}
		else
		{
			printk("Good EDID check sum2!");
		}
	}
}

void HDMI_Parse_DTD()
{
	BYTE i;
	
	printk("<%s:%d> parsing dtd ...\n", __func__, __LINE__);
	for (i = 0; i < 10; i++)
		DP_TX_EDID_PREFERRED[i] = HDMI_Read_EDID_BYTE(0x36 + i);	//edid[0x36]~edid[0x3f]
	for (i = 0; i < 8; i++)
		DP_TX_EDID_PREFERRED[10 + i] = HDMI_Read_EDID_BYTE(0x40 + i);	//edid[0x40]~edid[0x47]
	printk("<%s:%d> parsed dtd\n", __func__, __LINE__);
}

BYTE HDMI_Parse_CheckSum()
{
	BYTE chk_sum, i;

#ifdef	QUICK_BOOT_NO_CHECK
	printk("<%s:%d>: bypass it, assume checksum good.\n", __func__, __LINE__);
#else
	//printk("<%s:%d>: wait a while, take a cup of coffee\n", __func__, __LINE__);
	chk_sum = 0;

	for (i = 0; i < 127; i++) {
		//printk("<%s:%d> i=%d\n", __func__, __LINE__, i);
		//printk(".");
		chk_sum = chk_sum + HDMI_Read_EDID_BYTE(i);
	}
	printk("\n<%s:%d>\n", __func__, __LINE__);
	
	chk_sum = ~chk_sum + 1;
	if (chk_sum != HDMI_Read_EDID_BYTE(0x7f)) {
		printk("Bad check sum1!\n");
		dp_tx_edid_err_code = 0x02;
		printk("<%s:%d>\n", __func__, __LINE__);
		return 0;
	}
	else
	{
		printk("Good check sum1!\n");
	}
	
	printk("<%s:%d>\n", __func__, __LINE__);
#endif
	return 1;
}

void HDMI_TX_CSCandColorDepth_Setting(void)
{
	BYTE c;

	printk("<%s:%d>\n", __func__, __LINE__);
	if ((Hdmi_Edid_RGB30bit == 0) && (Hdmi_Edid_RGB36bit == 0) && (Hdmi_Edid_RGB48bit == 0))	//not deep color mode
	{
		// color mapping Joe20110923
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL3_REG, &c);	// 72:0a
		printk("<%s:%d> old [72:0a]=%02x\n", __func__, __LINE__, (WORD) c);
		c &= 0xf1;
		c |= (BGR << 1);	// 00h: RGB, 02h=RBG, 04h=GRB, 06h=BRG, 08h=GBR, 0ah=BGR, 0ch=don't change
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL3_REG, c);
		printk("<%s:%d> I write %02x\n", __func__, __LINE__, (WORD) c);

		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL3_REG, &c);	// 72:0a
		printk("<%s:%d> new [72:0a]=%02x\n", __func__, __LINE__, (WORD) c);

		//set 24 bit mode
		printk("<%s:%d> (24bit mode)\n", __func__, __LINE__);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);	// 72:09
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, ((c & 0x8f) | 0x10));
		DP_TX_Video_Input.bColordepth = COLOR_8;
		printk("<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, 1);

#if 0
		//reset 16 bit mode, Joe20111019
		printk("<%s:%d> (reset 16bit mode)\n", __func__, __LINE__);
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);	// 72:09
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, (c & 0x8f));	// set 6 bits
		DP_TX_Video_Input.bColordepth = COLOR_6;
		printk("<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, 1);
#endif

		//set the GCP packet for 24 bits
		DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, ((c & 0x03) | 0xd0));
		printk("<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	}
	else						//deep color mode
	{
		printk("<%s:%d> (deep color mode)\n", __func__, __LINE__);
		if ((Hdmi_Edid_RGB48bit == 1) || (Hdmi_Edid_RGB36bit == 1)) {

			//set 36 bit mode
			DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, ((c & 0x8f) | 0x30));
			DP_TX_Video_Input.bColordepth = COLOR_12;

			//set the GCP packet for 36 bits
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, ((c & 0x03) | 0xd8));
		}

		else {

			//set 30 bit mode
			DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, ((c & 0x8f) | 0x20));
			DP_TX_Video_Input.bColordepth = COLOR_10;

			//set the GCP packet for 30 bits
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, ((c & 0x03) | 0xd4));
		}
	}
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
}

void DP_TX_InputSet(BYTE bColorSpace, BYTE cBpc)
{
	DP_TX_Video_Input.bColordepth = cBpc;
	DP_TX_Video_Input.bColorSpace = bColorSpace;
}

void DP_TX_Enable_Video_Input(void)
{
	BYTE c, i;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	//if(mode_dp_or_hdmi)
	//EnhacedMode_Clear();
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, &c);
	if (BIST_EN) {
		if ((dp_tx_lane_count == 0x01)
			|| (DP_TX_Video_Input.bColordepth == COLOR_12)
			|| (mode_dp == 0))
		{
			c &= 0xf7;
		}
		else
		{
			c |= 0x08;
		}
	}

	else {
		c &= 0xf7;

		//printk("not one lane\n");
	}
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, (c | DP_TX_VID_CTRL1_VID_EN));

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	//switch(video_bpc)
	switch (DP_TX_Video_Input.bColordepth) {
	case COLOR_6:
		for (i = 0; i < 8; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x40 + i, 0x04 + i);
		}
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x48, 0x10);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x49, 0x11);
		for (i = 0; i < 6; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x4a + i, 0x18 + i);
		}
		for (i = 0; i < 8; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x50 + i, 0x22 + i);
		}
		break;
	case COLOR_8:
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		video_bit_ctrl(BGR);
#if 0	// Joe20110926
		for (i = 0; i < 8; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x40 + i, 0x04 + i);
		}
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x48, 0x10);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x49, 0x11);
		for (i = 0; i < 6; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x4a + i, 0x18 + i);
		}
		for (i = 0; i < 8; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x50 + i, 0x22 + i);
		}
#endif
		break;
	case COLOR_10:
		for (i = 0; i < 10; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x40 + i, 0x02 + i);
		}
		for (i = 0; i < 4; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x4a + i, 0x0e + i);
		}
		for (i = 0; i < 6; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x4e + i, 0x18 + i);
		}
		for (i = 0; i < 10; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x54 + i, 0x20 + i);
		}
		break;
	case COLOR_12:
		for (i = 0; i < 18; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x40 + i, 0x00 + i);
		}
		for (i = 0; i < 18; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, 0x52 + i, 0x18 + i);
		}
		break;
	default:
		break;
	}
	mdelay(1000);
	printk("Video Enabled!");
	if (mode_dp)				//DP MODE
	{
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		DP_TX_Clean_HDCP();
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
		DP_TX_Config_Packets(AVI_PACKETS);
		DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);

		/*
		   if ( !SWITCH1 )
		   DP_TX_Config_Audio();
		 */
	}
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
}

void DP_TX_EnhaceMode_Set(void)
{
	BYTE c;
	
	DP_TX_AUX_DPCDRead_Bytes(0x000002, 1, ByteBuf);

	//c = ;
	if (ByteBuf[0] & 0x80) 
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000101, 1, ByteBuf);
		ByteBuf[0] = ByteBuf[0] | 0x80;
		DP_TX_AUX_DPCDWrite_Bytes(0x000101, 1, ByteBuf);
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL4_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL4_REG, c | DP_TX_SYS_CTRL4_ENHANCED);
		printk("Enhance mode");
	}
	else
	{
		EnhacedMode_Clear();
	}
}

void DP_TX_Show_Infomation(void)
{
	BYTE c;
	//middle
	//float fresh_rate;
	int fresh_rate;

	if (BIST_EN)
	{
		printk("********BIST Info********");
	}
	else
	{
		printk("\n********Normal Info********\n");
	}
	
	if (mode_dp)				//dp
	{
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, &c);
		if (c == 0x01)
		{
			printk("   LC = 1");
		}
		else if (c == 0x02)
		{
			printk("   LC = 2");
		}
		else
		{
			printk("   LC = 4");
		}
		
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, &c);
		
		if (c == 0x06) 
		{
			printk("   BW = 1.62G");
			PCLK_Calc(0);		//str_clk = 162;
		}
		else 
		{
			printk("   BW = 2.7G");
			PCLK_Calc(1);		//str_clk = 270;
		}
		
		if (dp_tx_ssc_enable)
		{
			printk("   SSC On");
		}
		else
		{
			printk("   SSC Off");
		}
		
		printk("   M = %ld, N = %ld, PCLK = %d MHz\n", M_val, N_val, pclk);
	}
	else 
	{
		if (mode_hdmi)
		{
			printk("   HDMI Mode");
		}
		else
		{
			printk("   DVI Mode");
		}
	}
	
	Video_Timing();
	printk("   Total  %d * %d \n", h_res, v_res);
	printk("   Active  %d * %d ", h_act, v_act);
	if (mode_dp) {
		fresh_rate = pclk * 1000;
		fresh_rate = fresh_rate / h_res;
		fresh_rate = fresh_rate * 1000;
		fresh_rate = fresh_rate / v_res;
		printk(" @ %.2dHz", fresh_rate);
	}
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_STATUS, &c);
	
	if (I_or_P)
	{
		printk("\n   Interlace, ");
	}
	else
	{
		printk("\n   Progressive, ");
	}
	
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL2_REG, &c);
	
	if ((c & 0x70) == 0x00)
	{
		printk("6 BPC");
	}
	else if ((c & 0x70) == 0x10)
	{
		printk("8 BPC");
	}
	else if ((c & 0x70) == 0x20)
	{
		printk("10 BPC");
	}
	else if ((c & 0x70) == 0x30)
	{
		printk("12 BPC");
	}
	printk("**************************");
	h_res_bak = h_res;
	v_res_bak = v_res;
	h_act_bak = h_act;
	v_act_bak = v_act;
	I_or_P_bak = I_or_P;

	//printk("Type cmd timing to set any video timing!");

#ifdef CONFIG_HDMI_ANX9805_DEBUG
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, 0, 255);
	anx9805_dump_reg(DP_TX_PORT0_ADDR, 0, 255);
	anx9805_dump_reg(HDMI_TX_PORT1_ADDR, 0, 255);
#endif
}

void PCLK_Calc(BYTE hbr_rbr)
{
	long int str_clk;
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	
	if (hbr_rbr)
	{
		str_clk = 270;
	}
	else
	{
		str_clk = 162;
	}
	
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, M_VID_2, &c);
	M_val = c * 0x10000;
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, M_VID_1, &c);
	M_val = M_val + c * 0x100;
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, M_VID_0, &c);
	M_val = M_val + c;
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, N_VID_2, &c);
	N_val = c * 0x10000;
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, N_VID_1, &c);
	N_val = N_val + c * 0x100;
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, N_VID_0, &c);
	N_val = N_val + c;
	str_clk = str_clk * M_val;
	pclk = str_clk;
	pclk = pclk / N_val;
}

void BW_LC_Sel()
{
	BYTE over_bw;
	int pixel_clk;
	over_bw = 0;
	pixel_clk = pclk;
	if (BIST_EN) {
		if ((dp_tx_lane_count != 0x01)
			|| (DP_TX_Video_Input.bColordepth == COLOR_12)) {

			//if(bForceSelIndex != 4)
			pixel_clk = 2 * pixel_clk;
		}
	}
	printk("pclk = %d\n", (WORD) pixel_clk);
	
	if (pixel_clk <= 54) {
		dp_tx_bw = 0x06;
		dp_tx_lane_count = 0x01;

		//printk("rbr&lc=1");
	}
	else if ((54 < pixel_clk) && (pixel_clk <= 90)) 
	{
		if (dp_tx_bw == 0x0a) 
		{
			dp_tx_bw = 0x0a;
			dp_tx_lane_count = 0x01;

			//printk("hbr&lc=1");
		}
		else 
		{
			if (dp_tx_lane_count >= 0x02) 
			{
				dp_tx_bw = 0x06;
				dp_tx_lane_count = 0x02;

				//printk("1 rbr&lc=2");
			}	
			else
			{
				over_bw = 1;
			}
		}
	}
	else if ((90 < pixel_clk) && (pixel_clk <= 108)) 
	{
		if (dp_tx_lane_count < 2)
		{
			over_bw = 1;
		}
		else 
		{
			dp_tx_bw = 0x06;
			dp_tx_lane_count = 0x02;

			///printk("2 rbr&lc=2");
		}
	}

	else if ((108 < pixel_clk) && (pixel_clk <= 180)) 
	{
		if (dp_tx_bw == 0x0a) 
		{
			
			if (dp_tx_lane_count < 2)
			{
				over_bw = 1;
			}
			else 
			{
				dp_tx_bw = 0x0a;
				dp_tx_lane_count = 0x02;

				//printk("hbr&lc=2");
			}
		}
		else 
		{
			if (dp_tx_lane_count >= 0x02) {
				dp_tx_bw = 0x06;
				dp_tx_lane_count = 0x04;

				//printk("1 rbr&lc=4");
			}
			else
			{
				over_bw = 1;
			}
		}
	}

	else if ((180 < pixel_clk) && (pixel_clk <= 216)) {
		
		if (dp_tx_lane_count < 4)
		{
			over_bw = 1;
		}
		else {
			dp_tx_bw = 0x06;
			dp_tx_lane_count = 0x04;

			//printk("2 rbr&lc=4");
		}
	}

	else
	{
		if ((dp_tx_lane_count < 4) || (dp_tx_bw < 0x0a))
		{
			over_bw = 1;
		}
	}
	
	if (over_bw)
	{
		printk("over bw!");
	}
	
	printk("BW =%.2x, Lane cnt=%.2x\n", (WORD) dp_tx_bw, (WORD) dp_tx_lane_count);
}

void DP_TX_LL_CTS_Test(void)
{
	BYTE i;

	//force HPD to ignore IRQ during AUX OP
	DP_TX_FORCE_HPD();
	for (i = 0; i <= 0x0b; i++)
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000000 + i, 1, ByteBuf);
	}
	
	//DP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x06,5,ByteBuf);
	DP_TX_EDID_Read();
	DP_TX_RST_AUX();

	//unforce HPD
	DP_TX_UNFORCE_HPD();
}

BYTE DP_TX_AUX_DPCDRead_Bytes(long addr, BYTE cCount, pByte pBuf)
{
	BYTE c, i;

	//BYTE c1;

	//clr buffer
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_COUNT_REG, 0x80);

	//set read cmd and count
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG, ((cCount - 1) << 4) | 0x09);

	//set aux address15:0
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_7_0_REG, addr);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_15_8_REG, addr >> 8);

	//set address19:16 and enable aux
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_19_16_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_19_16_REG, (c & 0xf0) | (addr >> 16));

	//Enable Aux
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, c | 0x01);

	//mdelay(2);
	DP_TX_Wait_AUX_Finished();

/*
    DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_STATUS, &c);
    if(c != 0x00)
    {
        DP_TX_RST_AUX();
        printk("aux rd fail");
        return 1;
    }*/
	for (i = 0; i < cCount; i++) {

		// SJ fixed the warning 20120401.
		if (i >= MAX_BUF_CNT)
		{
			return 1;
		}
		
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_0_REG + i, &c);

		//printk("c = %.2x\n",(WORD)c);
		*(pBuf + i) = c;

		//c1 = *(pBuf +i);

		//printk("(pBuf+i)  = %.2x\n",(WORD)c1);

		//pBuf++;
		
		//break;
	}
	return 0;
}

void DP_TX_AUX_DPCDWrite_Bytes(long addr, BYTE cCount, pByte pBuf)
{
	BYTE c, i;

	//clr buffer
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_COUNT_REG, 0x80);

	//set write cmd and count;
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG, ((cCount - 1) << 4) | 0x08);

	//set aux address15:0
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_7_0_REG, addr);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_15_8_REG, addr >> 8);

	//set address19:16
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_19_16_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_ADDR_19_16_REG, (c & 0xf0) | (addr >> 16));

	//write data to buffer
	for (i = 0; i < cCount; i++) {
		c = *pBuf;
		pBuf++;
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_BUF_DATA_0_REG + i, c);
		if (i >= MAX_BUF_CNT)
			break;
	}

	//Enable Aux
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_AUX_CTRL_REG2, c | 0x01);

	//printk("L004w\n");
	DP_TX_Wait_AUX_Finished();

	//printk("L0005w\n");
	return;
}

//-------------------------------------------------------------------
//Function:     DP_TX_Config_Packets
//
//DESCRIPTION: configure the packets
//RETURN:
//
//NOTE:
//-------------------------------------------------------------------
void DP_TX_Config_Packets(PACKETS_TYPE bType)
{
	BYTE c;
	switch (bType) {
		
	case AVI_PACKETS:

		//clear packet enable
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c & (~DP_TX_PKT_AVI_EN));

		//get input color space
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_VID_CTRL, &c);
		DP_TX_Packet_AVI.AVI_data[0] = DP_TX_Packet_AVI.AVI_data[0] & 0x9f;
		DP_TX_Packet_AVI.AVI_data[0] = DP_TX_Packet_AVI.AVI_data[0] | c << 4;
		DP_TX_Load_Packet(AVI_PACKETS);

		//send packet update
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c | DP_TX_PKT_AVI_UD);

		//enable packet
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c | DP_TX_PKT_AVI_EN);
		break;
	case SPD_PACKETS:

		//clear packet enable
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c & (~DP_TX_PKT_SPD_EN));
		DP_TX_Load_Packet(SPD_PACKETS);

		//send packet update
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c | DP_TX_PKT_SPD_UD);

		//enable packet
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c | DP_TX_PKT_SPD_EN);
		break;
	case MPEG_PACKETS:

		//clear packet enable
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c & (~DP_TX_PKT_MPEG_EN));
		DP_TX_Load_Packet(MPEG_PACKETS);

		//send packet update
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c | DP_TX_PKT_MPEG_UD);

		//enable packet
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PKT_EN_REG, c | DP_TX_PKT_MPEG_EN);
		break;
	default:
		break;
	}
}

void DP_TX_CONFIG_SSC(void)
{
	BYTE c;
	
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_CTRL_REG1, 0x00);	// disable SSC first
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, 0x00);	//disable speed first
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, ANALOG_DEBUG_REG3, 0x99);	//set duty cycle
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_PLL_CTRL_REG, &c);	//reset DP PLL
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PLL_CTRL_REG, c & (~DP_TX_PLL_CTRL_PLL_RESET));

	//printk("############### Config SSC ####################");
	//c = DP_TX_AUX_DPCDRead_Byte(0x00, 0x00,DPCD_MAX_DOWNSPREAD);
	if (1)						//c == 0x01)
	{
		//M value select, select clock with downspreading
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_M_CALCU_CTRL, &c);
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_M_CALCU_CTRL, (c | 0x01));
		
		printk("#####Config SSC #####");
		if (dp_tx_bw == 0x0a) 
		{	//2.7G
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PLL_CTRL_REG, 0x05);	//PLL power 1.9V
			//DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_D_VALUE, 0x4a);                 // ssc d  0.35%
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_D_VALUE, 0x77);	// ssc d 0.5%
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_CTRL_REG2, 0x75);	// ctrl_th 31.3895K
		}
		else 
		{					//1.62G
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_PLL_CTRL_REG, 0x02);	//PLL power 1.7V
			//DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_D_VALUE, 0x78);                 // ssc d 0.35%
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_D_VALUE, 0xb8);	// ssc d 0.5%
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_CTRL_REG2, 0x6D);	// ctrl_th 30.4237K
		}
		
		DP_TX_Write_Reg(DP_TX_PORT0_ADDR, SSC_CTRL_REG1, 0x10);	// enable SSC
		DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c | DP_TX_RST_SSC);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c & (~DP_TX_RST_SSC));
		DP_TX_AUX_DPCDRead_Bytes(0x000107, 1, ByteBuf);
		ByteBuf[0] = ByteBuf[0] | 0x10;
		DP_TX_AUX_DPCDWrite_Bytes(0x000107, 1, ByteBuf);
	}
	else 
	{
		DP_TX_AUX_DPCDRead_Bytes(0x000107, 1, ByteBuf);
		ByteBuf[0] = ByteBuf[0] & 0x7f;
		DP_TX_AUX_DPCDWrite_Bytes(0x000107, 1, ByteBuf);
	}
}

//-------------------------------------------------------------------
//Function:     DP_TX_Load_Packet
//
//DESCRIPTION: load the packets
//RETURN:
//
//NOTE:
//-------------------------------------------------------------------
void DP_TX_Load_Packet(PACKETS_TYPE type)
{
	BYTE i;
	switch (type) {
		
	case AVI_PACKETS:
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AVI_TYPE, 0x82);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AVI_VER, 0x02);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AVI_LEN, 0x0d);
		for (i = 0; i < 13; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_AVI_DB1 + i, DP_TX_Packet_AVI.AVI_data[i]);
		}
		break;
	case SPD_PACKETS:
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_SPD_TYPE, 0x83);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_SPD_VER, 0x01);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_SPD_LEN, 0x19);
		for (i = 0; i < 25; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_SPD_DATA1 + i, DP_TX_Packet_SPD.SPD_data[i]);
		}
		break;
	case MPEG_PACKETS:
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_MPEG_TYPE, 0x85);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_MPEG_VER, 0x01);
		DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_MPEG_LEN, 0x0a);
		for (i = 0; i < 10; i++) {
			DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_MPEG_DATA1 + i, DP_TX_Packet_MPEG.MPEG_data[i]);
		}
		break;
	default:
		break;
	}
}

void DP_TX_PlayBack_Process(void)
{
	//BYTE switch_value;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	
	DP_TX_Video_Changed_Handler();
	if (BIST_EN) 
	{
		if (Force_Video_Resolution) 
		{
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);

			//switch_value = SWITCH2;
			//switch_value = (switch_value << 1) | SWITCH3;
			if (bForceSelIndex_backup != bForceSelIndex) 
			{
				bForceSelIndex_backup = bForceSelIndex;

				//switch_value_backup = switch_value;
				//VESA_timing_CMD_Set = 0;
				DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
			}
		}

		/*
		   else
		   {

		   if(bakup_VESA_timing_CMD_Set != VESA_timing_CMD_Set)
		   {
		   VESA_timing_CMD_Set = 1;
		   DP_TX_Set_System_State(DP_TX_LINK_TRAINING);
		   }
		   } */
	}
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
}

void DP_TX_Video_Changed_Handler(void)
{
	BYTE video_changed;
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
	
	if (BIST_EN)
	{
		return;
	}
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	
	video_changed = 0;
	Video_Timing();
	if (((h_res_bak - 10) > h_res) || ((h_res_bak + 10) < h_res)) 
	{
		video_changed = 1;

		//printk("h_res chg");
		printk("h_res_bak = %.2x, h_res = %.2x\n", (WORD)h_res_bak, (WORD)h_res);
	}
	if (((v_res_bak - 10) > v_res) || ((v_res_bak + 10) < v_res)) 
	{
		video_changed = 1;

		//printk("v_res chg");
		printk("v_res_bak = %.2x, v_res = %.2x\n", (WORD)v_res_bak, (WORD)v_res);
	}
	if (((h_act_bak - 10) > h_act) || ((h_act_bak + 10) < h_act)) 
	{
		video_changed = 1;

		//printk("h_act chg");
		printk("h_act_bak = %.2x, h_act = %.2x\n", (WORD)h_act_bak, (WORD)h_act);
	}
	if (((v_act_bak - 10) > v_act) || ((v_act_bak + 10) < v_act)) 
	{
		video_changed = 1;

		//printk("v_act chg");
		printk("v_act_bak = %.2x, v_act = %.2x\n", (WORD)v_act_bak, (WORD)v_act);
	}
	if (I_or_P_bak != I_or_P) 
	{
		video_changed = 1;

		printk("I_or_P chg");
	}
	if (video_changed) 
	{
		printk("video chg!");
		video_config_done = 0;
		if (mode_dp) 
		{
			DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
			DP_TX_Video_Disable();
		}
		else 
		{
			//mute TMDS
			DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_TMDS_CLKCH_CONFIG_REG, &c);
			DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR,
							HDMI_TMDS_CLKCH_CONFIG_REG, c & (~HDMI_TMDS_CLKCH_MUTE));

			//set AVMUTE
			DP_TX_SET_AVMUTE();

			//disable HW HDCP
			DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_0_REG, &c);
			DP_TX_Write_Reg(DP_TX_PORT0_ADDR,
							DP_TX_HDCP_CONTROL_0_REG, c & (~DP_TX_HDCP_CONTROL_0_HARD_AUTH_EN));
			DP_TX_RST_DDC();
			DP_TX_Set_System_State(DP_TX_CONFIG_VIDEO);
			DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
		}						//guochuncheng add 08.11.13
	}
	//mdelay(300);//wait for HDMI RX stable
	h_res_bak = h_res;
	v_res_bak = v_res;
	h_act_bak = h_act;
	v_act_bak = v_act;
	I_or_P_bak = I_or_P;
	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	anx9805_dump_reg(HDMI_TX_PORT0_ADDR, DP_TX_VIDEO_BIT_CTRL_0_REG, 1);
}

void DP_TX_RST_AUX(void)
{
	BYTE c;
	
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c | DP_TX_AUX_RST);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c & (~DP_TX_AUX_RST));
}

void DP_TX_RST_DDC(void)
{
	BYTE c;
	
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, 0x43, 0x00);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c | DP_TX_DDC_RST);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_RST_CTRL2_REG, c & (~DP_TX_DDC_RST));

	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, 0x46, &c);
	printk("[7a:46] = %.2x\n",(WORD)c);
}

void HDMI_TX_Parse_EDID(void)
{
	printk("<%s:%d>\n", __func__, __LINE__);
	
	HDMI_InitDDC_Read();
	if (!HDMI_Parse_EDIDHeader())
	{
		return;
	}
	
	if (!HDMI_Parse_CheckSum())
	{
		return;
	}
	HDMI_Parse_STD();
	HDMI_Parse_DTD();
	
	printk("<%s:%d>\n", __func__, __LINE__);
}

int get_hdmi_info(struct fb_info *fb_info)
{
	u8 edid[128],i;
	struct fb_var_screeninfo tmp_var;

	if(dp_tx_system_state <= DP_TX_WAIT_HOTPLUG) {
		printk("no link stat\n");
	        return -1;
	}

	/* Read EDID */
	for (i = 0; i < 128; i++) {
		edid[i] = HDMI_Read_EDID_BYTE(i);
	}
	fb_parse_edid(edid, &tmp_var);

	fb_edid_to_monspecs(edid, &fb_info->monspecs);

	fb_videomode_to_modelist(fb_info->monspecs.modedb,fb_info->monspecs.modedb_len,&fb_info->modelist);

        return 0;
}

EXPORT_SYMBOL(get_hdmi_info);

void HDMI_Parse_STD(void)
{
	BYTE c;
	BYTE DTDbeginAddr, Hdmi_stdaddr, Hdmi_stdreg;
	printk("<%s:%d> parsing std...\n", __func__, __LINE__);
	Hdmi_stdaddr = 0x84;

	printk("Hdmi_stdaddr= %.2x\n", (WORD)Hdmi_stdaddr);
	c = HDMI_Read_EDID_BYTE(0x82);

	printk("EDID[82]= %.2x\n", (unsigned int)c);
	DTDbeginAddr = HDMI_Read_EDID_BYTE(0x82) + 0x80;

	printk("DTDbeginAddr= %.2x\n", (WORD)DTDbeginAddr);
	while (Hdmi_stdaddr < DTDbeginAddr) {
		Hdmi_stdreg = HDMI_Read_EDID_BYTE(Hdmi_stdaddr);

		printk("Hdmi_stdreg= %.2x\n", (WORD)Hdmi_stdreg);
		switch (Hdmi_stdreg & 0xe0) {
		case 0x60:
			HDMI_Parse_VendorSTD(Hdmi_stdaddr);
			break;
		default:
			break;
		}
		Hdmi_stdaddr = Hdmi_stdaddr + (Hdmi_stdreg & 0x1f) + 0x01;
	}
	printk("<%s:%d> parsed std\n", __func__, __LINE__);
}

BYTE HDMI_Read_EDID_BYTE(BYTE offset)
{
	BYTE c;
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_SLV_OFFADDR_REG, offset);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_ACC_CMD_REG, HDMI_DDC_ACC_DDC_READ);
	mdelay(3);
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_FIFO_ACC_REG, &c);

	//printk("offset[%.2x]=0x%.2x\n",(WORD)offset,(WORD)c);
	return c;
}
void HDMI_InitDDC_Read(void)
{
	printk("<%s:%d>\n", __func__, __LINE__);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_SLV_ADDR_REG, 0xa0);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_ACCNUM0_REG, 0x01);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_ACCNUM1_REG, 0x00);

	//Clear FIFO
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_DDC_ACC_CMD_REG, 0x05);
}

void HDMI_Parse_VendorSTD(BYTE std)
{
	BYTE c;
	printk("<%s:%d>\n", __func__, __LINE__);
	if ((HDMI_Read_EDID_BYTE((std + 1)) == 0x03)
		&& (HDMI_Read_EDID_BYTE((std + 2)) == 0x0c) && (HDMI_Read_EDID_BYTE((std + 3)) == 0x00)) {
		mode_hdmi = 1;			//hdmi
		DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_CTRL1, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_CTRL1, c | HDMI_MODE_ENABLE);
		printk("HDMI mode\n");
	}

	else {
		mode_hdmi = 0;
		DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_CTRL1, &c);
		DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_SYS_CTRL1, c & (~HDMI_MODE_ENABLE));
		printk("DVI mode\n");
	}

	//deep color mode information
	c = HDMI_Read_EDID_BYTE(std) & 0x1f;
	if (c >= 6) {
		c = HDMI_Read_EDID_BYTE((std + 6));
		Hdmi_Edid_RGB30bit = (c & 0x10) >> 4;
		Hdmi_Edid_RGB36bit = (c & 0x20) >> 5;
		Hdmi_Edid_RGB48bit = (c & 0x40) >> 6;
	}
}

void HDMI_GCP_PKT_Enable(void)
{
	BYTE c;

	printk("<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL1_REG, c | 0x0c);
}

void HDMI_AIF_PKT_Enable(void)
{
	BYTE c;

	printk("<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL2_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_INFO_PKTCTRL2_REG, c | 0x03);
}

void Video_Timing()
{
	BYTE c, c1;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_TOTAL_LINE_STA_L, &c);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_TOTAL_LINE_STA_H, &c1);
	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_ACT_LINE_STA_L, &c);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_ACT_LINE_STA_H, &c1);
	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_TOTAL_PIXEL_STA_L, &c);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_TOTAL_PIXEL_STA_H, &c1);
	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_ACT_PIXEL_STA_L, &c);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_ACT_PIXEL_STA_H, &c1);
	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_STATUS, &c);
	if (c & 0x04)
	{
		I_or_P = 1;
	}
	else
	{
		I_or_P = 0;
	}
}

void DP_TX_PBBS7_Test(void)
{
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x30);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_ANALOG_POWER_DOWN_REG, 0x00);

	//set bandwidth
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, 0x0a);

	//set lane conut
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, 0x04);

	//Link quality pattern setting
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x0c);
	DP_TX_Power_On();
}

void DP_TX_Insert_Err(void)
{
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_DEBUG_REG, 0x02);
}

void DP_TX_Blue_Screen_Enable(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_1_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_1_REG,
					c | DP_TX_HDCP_CONTROL_1_HDCP_EMB_SCREEN_EN);
	send_blue_screen = 1;
}

void DP_TX_Blue_Screen_Disable(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_1_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_HDCP_CONTROL_1_REG,
					c & ~DP_TX_HDCP_CONTROL_1_HDCP_EMB_SCREEN_EN);
	send_blue_screen = 0;
}

void DP_TX_SET_AVMUTE(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG,
					((c & 0xfc) | HDMI_GNRL_CTRL_SET_AVMUTE));
	HDMI_GCP_PKT_Enable();

	avmute_enable = 1;
}

void DP_TX_CLEAR_AVMUTE(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT1_ADDR, HDMI_GNRL_CTRL_PKT_REG,
					((c & 0xfc) | HDMI_GNRL_CTRL_CLR_AVMUTE));
	HDMI_GCP_PKT_Enable();

	avmute_enable = 0;
}

void DP_TX_Power_Down(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Video_Disable();
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, c | DP_POWERD_TOTAL_REG);
}

void DP_TX_Video_Disable(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_TX_VID_CTRL1_REG, (c & ~DP_TX_VID_CTRL1_VID_EN));
}

void DP_TX_Power_On(void)
{
	BYTE c;

	DBGPRINT(1, "<%s:%d>\n", __func__, __LINE__);
	DP_TX_Read_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, &c);
	DP_TX_Write_Reg(HDMI_TX_PORT0_ADDR, DP_POWERD_CTRL_REG, (c & ~DP_POWERD_TOTAL_REG));
}

void DP_TX_Set_System_State(BYTE ss)
{
	printk("9805: ");
	switch (ss) {
	case DP_TX_WAIT_HOTPLUG:
		dp_tx_system_state = DP_TX_WAIT_HOTPLUG;
		DP_TX_Variable_Init();

		//printk("WAIT_HOTPLUG");
		printk("<%s:%d> to WAIT_HOTPLUG\n", __func__, __LINE__);
		break;
	case DP_TX_PARSE_EDID:
		dp_tx_system_state = DP_TX_PARSE_EDID;

		//printk("PARSE_EDID");
		printk("<%s:%d> to PARSE_EDID\n", __func__, __LINE__);
		break;
	case DP_TX_CONFIG_VIDEO:
		dp_tx_system_state = DP_TX_CONFIG_VIDEO;
		video_config_done = 0;

		//printk("CONFIG_VIDEO");
		printk("<%s:%d> to CONFIG_VIDEO\n", __func__, __LINE__);
		break;
	case DP_TX_CONFIG_AUDIO:
		dp_tx_system_state = DP_TX_CONFIG_AUDIO;

		//printk("CONFIG_AUDIO");
		printk("<%s:%d> to CONFIG_AUDIO\n", __func__, __LINE__);
		break;
	case DP_TX_LINK_TRAINING:
		dp_tx_system_state = DP_TX_LINK_TRAINING;
		dp_tx_link_config_done = 0;

		//printk("LINK_TRAINING");
		printk("<%s:%d> to LINK_TRAINING\n", __func__, __LINE__);
		break;
	case DP_TX_HDCP_AUTHENTICATION:
		dp_tx_system_state = DP_TX_HDCP_AUTHENTICATION;

		//printk("HDCP_AUTH");
		printk("<%s:%d> to HDCP_AUTH\n", __func__, __LINE__);
		break;
	case DP_TX_PLAY_BACK:
		dp_tx_system_state = DP_TX_PLAY_BACK;

		//printk("PLAY_BACK");
		printk("<%s:%d> to PLAY_BACK\n", __func__, __LINE__);
		break;
	default:
		break;
	}
}

void DP_TX_Set_Link_state(DP_SW_LINK_State eState)
{
	switch (eState) 
	{
		case LINKTRAINING_START:
			eSW_Link_state = LINKTRAINING_START;

			#ifdef CR_LOOP
			printk("LINKTRAINING_START\n");
			#endif /*  */
			
			break;
			
		case CLOCK_RECOVERY_PROCESS:
			eSW_Link_state = CLOCK_RECOVERY_PROCESS;

			#ifdef CR_LOOP
			printk("CLOCK_RECOVERY_PROCESS\n");
			#endif /*  */
			
			break;
			
		case EQ_TRAINING_PROCESS:
			eSW_Link_state = EQ_TRAINING_PROCESS;

			//write EQ training pattern
			//DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_PTN_SET_REG, 0x22);
			//DP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_TRAINING_PATTERN_SET, 0x22);
			#ifdef CR_LOOP
			printk("EQ_TRAINING_PROCESS");
			printk("PT2 set");
			#endif /*  */
			
			break;
			
		case LINKTRAINING_FINISHED:
			eSW_Link_state = LINKTRAINING_FINISHED;
			
			#ifdef CR_LOOP
			printk("LINKTRAINING_FINISHED");
			#endif /*  */
			
			break;
			
		default:
			break;
	}
}

void DP_TX_HW_LT(BYTE bw, BYTE lc)
{
	BYTE c;
	
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE0_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE1_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE2_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_TRAINING_LANE3_SET_REG, 0x00);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_BW_SET_REG, bw);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LANE_COUNT_SET_REG, lc);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_TRAINING_CTRL_REG, DP_TX_LINK_TRAINING_CTRL_EN);
	mdelay(2);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_TRAINING_CTRL_REG, &c);
	
	while (c & DP_TX_LINK_TRAINING_CTRL_EN)
	{
		DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_LINK_TRAINING_CTRL_REG, &c);
	}
	printk("HW LT Completed!");
}

void EnhacedMode_Clear()
{
	BYTE c;
	
	DP_TX_AUX_DPCDRead_Bytes(0x000101, 1, ByteBuf);
	ByteBuf[0] = ByteBuf[0] & (~0x80);
	DP_TX_AUX_DPCDWrite_Bytes(0x000101, 1, ByteBuf);
	DP_TX_Read_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL4_REG, &c);
	DP_TX_Write_Reg(DP_TX_PORT0_ADDR, DP_TX_SYS_CTRL4_REG, c & (~DP_TX_SYS_CTRL4_ENHANCED));
}

// Moved by SJ 20120313: End

static int anx9805_sys_i2c_remove(struct i2c_client *client)
{
	struct anx9805_info *anx9805 = i2c_get_clientdata(client);

	anx9805_release(anx9805);
	anx9805_i2c_client_remove(anx9805);
	unregister_anx9805(&anx9805_fops, anx9805);
	i2c_set_clientdata(client, NULL);
	kfree(anx9805);
	pANX9805 = NULL;

	printk("anx9805 hdmi transmitter cleanup\n");

	return 0;
}

static int anx9805_i2c_remove(struct i2c_client *client)
{
	struct anx9805_info *anx9805 = i2c_get_clientdata(client);

	if (!strcmp(client->name, "anx9805-dp")) {
		if (anx9805->cli_dp) {
			anx9805->cli_dp = NULL;
			return 0;
		}
		return -1;
	} else if (!strcmp(client->name, "anx9805-hdmi")) {
		if (anx9805->cli_hdmi) {
			anx9805->cli_hdmi = NULL;
			return 0;
		}
		return -1;
	} else if (!strcmp(client->name, "anx9805-sys")) {
		anx9805_sys_i2c_remove(client);
		return 0;
	} else {
		printk("unknown anx9805 i2c client\n");
		return -1;
	}
}

static struct i2c_driver anx9805_sys_i2c_driver = {
	.driver = {
		   .name = "anx9805-sys",
		   .owner = THIS_MODULE,
		   },
	.probe = anx9805_i2c_probe,
	.remove = anx9805_i2c_remove,
	.id_table = anx9805_id,
};

static struct i2c_driver anx9805_dp_i2c_driver = {
	.driver = {
		   .name = "anx9805-dp",
		   .owner = THIS_MODULE,
		   },
	.probe = anx9805_i2c_probe,
	.remove = anx9805_i2c_remove,
	.id_table = anx9805_id,
};

static struct i2c_driver anx9805_hdmi_i2c_driver = {
	.driver = {
		   .name = "anx9805-hdmi",
		   .owner = THIS_MODULE,
		   },
	.probe = anx9805_i2c_probe,
	.remove = anx9805_i2c_remove,
	.id_table = anx9805_id,
};

static int __init 
anx9805_hdmi_init(void)
{
	int ret;

	ret = i2c_add_driver(&anx9805_sys_i2c_driver);
	if (!ret) {
		i2c_add_driver(&anx9805_dp_i2c_driver);
		i2c_add_driver(&anx9805_hdmi_i2c_driver);
	}
	printk("anx9805 hdmi transmitter%s initialized\n", ret ? " fail" : "");

	return ret;
}
module_init(anx9805_hdmi_init);

static void __exit 
anx9805_hdmi_exit(void)
{
	i2c_del_driver(&anx9805_hdmi_i2c_driver);
	i2c_del_driver(&anx9805_dp_i2c_driver);
	i2c_del_driver(&anx9805_sys_i2c_driver);
}
module_exit(anx9805_hdmi_exit);

MODULE_DESCRIPTION("Cortina ANX9805 HDMI driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joe Hsu <Joe.Hsu@cortina-systems.com>");
