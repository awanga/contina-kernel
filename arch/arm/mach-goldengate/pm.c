/*
 * arch/arm/mach-goldengate/pm.c
 * CS75xx Power Management
 *
 * Copyright (C) 2011 Cortina-Systems Co. LTD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
 
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/atomic.h>
//#include <asm/mach/time.h>
//#include <asm/mach/irq.h>

//#include <mach/cs75xx_pmc.h>
//#include <mach/gpio.h>
#include <mach/platform.h>
#include <mach/hardware.h>
#include <asm/hardware/gic.h>
//#include "generic.h"
//#include "pm.h"
#include <mach/cs_cpu.h>
#include <mach/cs_list.h>
#include <mach/cs_clk_change.h>

u64 gic_wakeups0;
u64 gic_backups0;
u16 gic_wakeups1;
u16 gic_backups1;

u16 regbus_wakeups;
u16 regbus_backups;

/* ----------------------------------------------------------------------------------- */
/* Start of IROS PM functions*/
#include <linux/errno.h>
#include <mach/cs_types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif
#include <mach/cs_pwrmgt.h>
#include <linux/cpu.h>
#include <linux/delay.h>

static DEFINE_SPINLOCK(pm_api_mutex);

#define SNOW_PHY_BASE_ADDR PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0
#define AHCI_PORT_PRIV_SZ	1024
#define AHCI_RX_FIS_SZ		256
#define AHCI_CMD_TBL_HDR	0x80
#define AHCI_CAP                0x00
#define AHCI_CTL                0x04
#define AHCI_PI                 0x0C
#define AHCI_TESTER 		0xf4
#define AHCI_OOBR   		0xBC
#define AHCI_BISTCR		0xA4
#define AHCI_CTL_HR		(1 << 0) 

#define AHCI_PxCLB		0x00 
#define AHCI_PxFB		0x08 
#define AHCI_PxCMD		0x18 
#define AHCI_PxSIG	 	0x24	
#define AHCI_PxSSTS		0x28 
#define AHCI_PxSCTL		0x2C
#define AHCI_PxSERR		0x30 
#define AHCI_PxCI		0x38 

#define AHCI_PxCMD_ICC		(1 << 28)
#define AHCI_PxCMD_CR		(1 << 15)
#define AHCI_PxCMD_FR		(1 << 14)
#define AHCI_PxCMD_FRE		(1 << 4)
#define AHCI_PxCMD_POD		(1 << 2)
#define AHCI_PxCMD_SUD		(1 << 1)
#define AHCI_PxCMD_ST		(1 << 0)

#define CS_SETF_CMD_XFER	0x03
#define SET_FEATURE_TIMEOUT 	10000
#define IDEN_BUF_SIZE		512
#define IDENTIFY_TIMEOUT 	30000
#define SPINUP_TIMEOUT 		20000

struct ahci_cmd_hdr {
	unsigned int	descInfo;
	unsigned int	cmdStatus;
	unsigned int	tblBaseAddr;
	unsigned int	reserved[8];
};

struct ahci_sg {
	unsigned int	addr;
	unsigned int	addr_hi;
	unsigned int	reserved;
	unsigned int	data_byte_count;
};

struct ahci_private {
	unsigned int	port_mmio;
	struct ahci_cmd_hdr	*cmd_slot;
	struct ahci_sg		*cmd_sg;
	unsigned int	cmd_tbl;
};

static cs_peripheral_freq_t cs_peripheral_freq[] = {
	CS_PERIPHERAL_FREQUENCY_100,	/* ARM clock 400MHz */
	CS_PERIPHERAL_FREQUENCY_100,	/* ARM clock 600MHz */
	CS_PERIPHERAL_FREQUENCY_100,	/* ARM clock 700MHz */
	CS_PERIPHERAL_FREQUENCY_150,	/* ARM clock 750MHz */
	CS_PERIPHERAL_FREQUENCY_100,	/* ARM clock 800MHz */
	CS_PERIPHERAL_FREQUENCY_170,	/* ARM clock 850MHz */
	CS_PERIPHERAL_FREQUENCY_100,	/* ARM clock 900MHz */
};

static cs_axi_freq_t cs_axi_freq[] = {
	CS_AXI_FREQUENCY_133,		/* ARM clock 400MHz */
	CS_AXI_FREQUENCY_150,		/* ARM clock 600MHz */
	CS_AXI_FREQUENCY_140,		/* ARM clock 700MHz */
	CS_AXI_FREQUENCY_150,		/* ARM clock 750MHz */
	CS_AXI_FREQUENCY_160,		/* ARM clock 800MHz */
	CS_AXI_FREQUENCY_142,		/* ARM clock 850MHz */
	CS_AXI_FREQUENCY_150,		/* ARM clock 900MHz */
};

static void cs_pm_freq_notify_transition(cs_pm_freq_notifier_event_t event,
					 cs_cpu_freq_t old, cs_cpu_freq_t new);

static struct ahci_private priv;
static int waiting_register (unsigned int addr, unsigned int mask, unsigned int val, unsigned int timeout)
{
	unsigned int i;
	volatile unsigned int status;

	for( i = 0; i < timeout; ++i) {
		status= readl(IO_ADDRESS(addr));
		if((status & mask) == val ){
			return 1;
		}
		mdelay( 1 );
	}

	return 0;
}

cs_boolean_t  cs_suspend = CS_FALSE;
extern cs_status_t cs_pm_ne_suspend ( void );
extern cs_status_t cs_pm_ne_resume ( void );
/*
 * Configure NE running mode (run or suspend)
 * ddev_id	CS device ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_ne_suspend_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN cs_boolean_t  suspend)
{
	printk(KERN_INFO "%s: Dev-team should handle this function. suspend %d\n", __func__, suspend);
	
    if (cs_suspend == suspend) {
	    return CS_E_OK;
    }

	switch (suspend) {
	    case CS_FALSE:
	        // resume
	        printk("%s: suspend CS_FALSE\n", __func__);
	        cs_pm_ne_resume();
	        break;

	    case CS_TRUE:
	        // shutdown
	        printk("%s: suspend CS_TRUE\n", __func__);
	        cs_pm_ne_suspend();
	        break;

	    default:
	        printk("%s: suspend ERROR %d\n", __func__, suspend);
	        return CS_E_PARAM;
	}

	cs_suspend = suspend;
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_ne_suspend_set);

/*
 * Retrieve NE running mode (run or suspend) setting
 * dev_id	CS device ID
 * pe_id	PE ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */ 
cs_status_t cs_pm_ne_suspend_get (
CS_IN cs_dev_id_t  dev_id,
CS_OUT cs_boolean_t  *suspend)
{
	printk(KERN_INFO "%s: Dev-team should handle this function.\n", __func__);
	*suspend = cs_suspend;
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_ne_suspend_get);

/*
 * Configure USB working state setting of given USB ID.
 * dev_id	CS device ID
 * usb_id	USB ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_usb_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  usb_id,
CS_IN cs_pm_state_t  state)
{
	GLOBAL_PHY_CONTROL_t glo_phy_ctrl;

	if(usb_id >= 0x2 || state >= 0x3)
	{
		printk("Error parameter!!!\n");
		return CS_E_ERROR;
	}
	glo_phy_ctrl.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));

	if (cs_soc_is_cs7522()) {
    		switch (state) {
		case CS_PM_STATE_POWER_DOWN:
			if(usb_id==0)
			{
				printk("Unacceptable Instruction~\n");
				return CS_E_ERROR;
			}
			else
			{
				printk("USB[0] & USB[1] will power down at the same time!!!\n");
				glo_phy_ctrl.bf.usb_phy0_por = 1;
				glo_phy_ctrl.bf.usb_phy1_por = 1;
				writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
			}	
			break;
		case CS_PM_STATE_SUSPEND:
			printk( "Not support ~\n");
			break;
		case CS_PM_STATE_NORMAL:
			if(usb_id==0)
			{
				printk("Unacceptable Instruction~\n");
				return CS_E_ERROR;
			}
			else
			{
				printk("USB[0] & USB[1] will power on at the same time!!!\n");
				glo_phy_ctrl.bf.usb_phy0_por = 0;
				glo_phy_ctrl.bf.usb_phy1_por = 0;
				writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
			}
			break;
		default:
			printk( "Not supported state : %x ~\n",state);
			return CS_E_ERROR;
			
		}
    		
    	}
    	else			//7542
    	{
    		switch (state) {
		case CS_PM_STATE_POWER_DOWN:
			switch (usb_id) {
				case 0:
					if(glo_phy_ctrl.bf.usb_phy0_por == 1)
					{
						printk("Usb[0] already PD ~\n");
						return CS_E_OK;
					}
					
					if(glo_phy_ctrl.bf.usb_phy1_por == 0)
					{
						printk("Unacceptable Instruction, Usb[1] is active ~\n");
						return CS_E_ERROR;
					}
					
					glo_phy_ctrl.bf.usb_phy0_por = 1;
					printk("Usb[0] PD ~\n");
					
					break;
				case 1:
					glo_phy_ctrl.bf.usb_phy1_por = 1;
					printk("Usb[1] PD ~\n");
					break;
			}
			writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
			break;
		case CS_PM_STATE_SUSPEND:
			printk( "Not support ~\n");
			break;
		case CS_PM_STATE_NORMAL:
			switch (usb_id) {
				case 0:
					glo_phy_ctrl.bf.usb_phy0_por = 0;
					break;
				case 1:
					if(glo_phy_ctrl.bf.usb_phy0_por == 1)
					{
						printk("Unacceptable Instruction, Usb[0] is PD ~\n");
						return CS_E_ERROR;
					}
					glo_phy_ctrl.bf.usb_phy1_por = 0;
					
					break;
			}
			writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
			break;
		default:
			printk( "Not supported state : %x ~\n",state);
			return CS_E_ERROR;
			
		}
    		
    	}
		
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_usb_state_set);


/*
 * Retrieve USB running mode (run, suspend or power down) setting
 * dev_id	CS device ID
 * usb_id	USB ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_usb_state_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  usb_id,
CS_OUT cs_pm_state_t  *state)
{
	GLOBAL_PHY_CONTROL_t glo_phy_ctrl;
	
	if(usb_id >= 0x2)
	{
		printk("Error parameter!!!\n");
		return CS_E_ERROR;
	}
	
	glo_phy_ctrl.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
	
	switch (usb_id) {
		case 0:
			if(glo_phy_ctrl.bf.usb_phy0_por == 1)			
				*state = CS_PM_STATE_POWER_DOWN;
			else
				*state = CS_PM_STATE_NORMAL;
			return CS_E_OK;
			
			//break;
		case 1:
			if(glo_phy_ctrl.bf.usb_phy1_por == 1)			
				*state = CS_PM_STATE_POWER_DOWN;
			else
				*state = CS_PM_STATE_NORMAL;
			return CS_E_OK;
			
			//break;
		default:
			printk("error usb_id : %x \n",usb_id);
			return CS_E_ERROR;		
		
	}
	
}
EXPORT_SYMBOL(cs_pm_usb_state_get);

/* pcie */
static void sb_phy_program_24mhz(unsigned int base_addr)
{
	unsigned int value;

	writel(0x00090006, IO_ADDRESS(base_addr));
	writel(0x000e0960, IO_ADDRESS(base_addr + 4));

   	writel(0x68a00000, IO_ADDRESS(base_addr + 32));     /* Samsung's suggestion */
   	/* writel(0x680a0000, base_addr + 32); */
	printk("%s: base_addr + 32 = 0x%x\r\n", __func__, readl(IO_ADDRESS(base_addr + 32)));

	writel(0x50040000, IO_ADDRESS(base_addr + 44));
	writel(0x40250270, IO_ADDRESS(base_addr + 48));
	writel(0x00004001, IO_ADDRESS(base_addr + 52));

	/* debug_Aaron on 2012/04/05 for A1 chip checking */
	if (cs_soc_is_cs7522a0() || cs_soc_is_cs7542a0())
		value = 0x5e082e00;
	else if (cs_soc_is_cs7522a1() || cs_soc_is_cs7542a1())
		value = 0x5e080600;
	else
		printk("%s: Wrong JTAG ID:%x (FPGA ?)\n", __func__, IO_ADDRESS(GLOBAL_JTAG_ID));
	writel(value, IO_ADDRESS(base_addr + 96));	 /* Samsung's suggestion */

        /* writel(0x5e002e00, base_addr + 96); */
	printk("%s: base_addr + 96 = 0x%x\r\n", __func__, readl(IO_ADDRESS(base_addr + 96)));

        writel(0xF0914200, IO_ADDRESS(base_addr + 100));	 /* Samsung's suggestion */
        /* writel(0x10914200, base_addr + 100); */
	printk("%s: base_addr + 100 = 0x%x\r\n", __func__, readl(IO_ADDRESS(base_addr + 100)));

	writel(0x4c0c9048, IO_ADDRESS(base_addr + 104));
	writel(0x00000373, IO_ADDRESS(base_addr + 108));  //REF clock selection
	writel(0x04841000, IO_ADDRESS(base_addr + 124));

        writel(0x000000e0, IO_ADDRESS(base_addr + 128));    /* Samsung's suggestion */
        /* writel(0x0000000e, base_addr + 128); */
	printk("%s: base_addr + 128 = 0x%x\r\n", __func__, readl(IO_ADDRESS(base_addr + 128)));

	writel(0x04000023, IO_ADDRESS(base_addr + 132));
	writel(0x68001038, IO_ADDRESS(base_addr + 136));
	writel(0x0d181ea2, IO_ADDRESS(base_addr + 140));
	writel(0x0000000c, IO_ADDRESS(base_addr + 144));
	writel(0x0f600000, IO_ADDRESS(base_addr + 196));
	writel(0x400290c0, IO_ADDRESS(base_addr + 200));
	writel(0x0000003c, IO_ADDRESS(base_addr + 204));
	writel(0xc68b8300, IO_ADDRESS(base_addr + 208));
	writel(0x98280301, IO_ADDRESS(base_addr + 212));
	writel(0xe1782819, IO_ADDRESS(base_addr + 216));
	writel(0x00f410f0, IO_ADDRESS(base_addr + 220));
	writel(0xa0a0a000, IO_ADDRESS(base_addr + 232));
	writel(0xa0a0a0a0, IO_ADDRESS(base_addr + 236));
	writel(0x9fc00068, IO_ADDRESS(base_addr + 240));
	writel(0x00000001, IO_ADDRESS(base_addr + 244));
	writel(0x00000000, IO_ADDRESS(base_addr + 248));
	writel(0xd07e4130, IO_ADDRESS(base_addr + 252));
	writel(0x935285cc, IO_ADDRESS(base_addr + 256));
	writel(0xb0dd49e0, IO_ADDRESS(base_addr + 260));
	writel(0x0000020b, IO_ADDRESS(base_addr + 264));
	writel(0xd8000000, IO_ADDRESS(base_addr + 300));
	writel(0x0001ff1a, IO_ADDRESS(base_addr + 304));
	writel(0xf0000000, IO_ADDRESS(base_addr + 308));
	writel(0xffffffff, IO_ADDRESS(base_addr + 312));
	writel(0x3fc3c21c, IO_ADDRESS(base_addr + 316));
	writel(0x0000000a, IO_ADDRESS(base_addr + 320));
	writel(0x00f80000, IO_ADDRESS(base_addr + 324));

	//release AHB reset
	writel(0x00090007, IO_ADDRESS(base_addr));
}

static void sb_phy_program_100mhz(unsigned int base_addr)
{
    unsigned int value;

    writel(0x00010006, IO_ADDRESS(base_addr));   	//rate prog
    writel(0x64a00000, IO_ADDRESS(base_addr + 32));
    writel(0x50040000, IO_ADDRESS(base_addr + 44));
    writel(0x40250270, IO_ADDRESS(base_addr + 48));
    writel(0x00004001, IO_ADDRESS(base_addr + 52));


	/* debug_Aaron on 2012/04/05 for A1 chip checking */
    if (cs_soc_is_cs7522a0() || cs_soc_is_cs7542a0())
	    value = 0x5e002e00;
    else if (cs_soc_is_cs7522a1() || cs_soc_is_cs7542a1())
	    value = 0x5e000600;
    else
	    printk("%s: Wrong JTAG ID:%x (FPGA ?)\n",__func__,  IO_ADDRESS(GLOBAL_JTAG_ID));
    writel(value, IO_ADDRESS(base_addr + 96));   /* Samsung's suggestion */

    writel(0x90914200, IO_ADDRESS(base_addr + 100));	 /* Samsung's suggestion */
    /* writel(0x10914200, base_addr + 100); */
    printk("%s: base_addr + 100 = 0x%x\r\n", __func__, readl(IO_ADDRESS(base_addr + 100)));	

    writel(0xce449048, IO_ADDRESS(base_addr + 104));
    writel(0x0000000b, IO_ADDRESS(base_addr + 108));  //REF clock selection
    writel(0x04841000, IO_ADDRESS(base_addr + 124));
    writel(0x000000e0, IO_ADDRESS(base_addr + 128));
    writel(0x04000023, IO_ADDRESS(base_addr + 132));
    writel(0x68000438, IO_ADDRESS(base_addr + 136));
    writel(0x0d181ea2, IO_ADDRESS(base_addr + 140));
    writel(0x0000000d, IO_ADDRESS(base_addr + 144));
    writel(0x0f600000, IO_ADDRESS(base_addr + 196));
    writel(0x400290c0, IO_ADDRESS(base_addr + 200));  //8b/10 enc. enable, DW-bits
    writel(0x0000003c, IO_ADDRESS(base_addr + 204));
    writel(0xc6496300, IO_ADDRESS(base_addr + 208));
    writel(0x98280301, IO_ADDRESS(base_addr + 212));
    writel(0xe1782819, IO_ADDRESS(base_addr + 216));
    writel(0x00f410f0, IO_ADDRESS(base_addr + 220));
    writel(0xa0a0a000, IO_ADDRESS(base_addr + 232));
    writel(0xa0a0a0a0, IO_ADDRESS(base_addr + 236));
    writel(0x9fc00064, IO_ADDRESS(base_addr + 240));
    writel(0x00000001, IO_ADDRESS(base_addr + 244));
    writel(0xd07e4130, IO_ADDRESS(base_addr + 252));
    writel(0x935285cc, IO_ADDRESS(base_addr + 256));
    writel(0xb0dd49e0, IO_ADDRESS(base_addr + 260));
    writel(0x0000020b, IO_ADDRESS(base_addr + 264));
    writel(0xd8000000, IO_ADDRESS(base_addr + 300));
    writel(0x0001ff1a, IO_ADDRESS(base_addr + 304));
    writel(0xf0000000, IO_ADDRESS(base_addr + 308));
    writel(0xffffffff, IO_ADDRESS(base_addr + 312));
    writel(0x3fc3c21c, IO_ADDRESS(base_addr + 316));
    writel(0x0000000a, IO_ADDRESS(base_addr + 320));
    writel(0x00f80000, IO_ADDRESS(base_addr + 324));

    //release AHB reset
    writel(0x00010007, IO_ADDRESS(base_addr));
}

static pcie_sb_phy_program(u32 is_24mhz, u32 phy_number)
{
	int i;
	GLOBAL_PHY_CONTROL_t phy_control;
    PCIE_SATA_PCIE_GLBL_CMU_OK_CORE_DEBUG_13_t cmu_ok;
    PCIE_SATA_SNOW_PHY_COM_LANE_REG3_REG2_REG1_REG0_t com_lane;
	unsigned int reg_offset;
	GLOBAL_BLOCK_RESET_t block_reset;

	/* Before do the configuration for Snow Bush PHY, power down the PHY first */
    /* to avoid system panic */
	 /* Release the power on reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
            por_n_i = 1'b1
            pd = 0'b0
       If reference clock is 24MHz, then program refclksel = 2'b10
       If reference clock is 100MHz, then program refclksel = 2'b00
    */
    phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
    switch (phy_number)
    {   
        case 0: 
            phy_control.bf.phy_0_por_n_i = 0;
            phy_control.bf.phy_0_pd = 1;
            phy_control.bf.phy_0_refclksel = 2;  /* if 24MHz */
	    	phy_control.bf.phy_0_ln0_resetn_i = 0;
	    	phy_control.bf.phy_0_cmu_resetn_i = 0;
            break;      
        case 1: 
            phy_control.bf.phy_1_por_n_i = 0;
            phy_control.bf.phy_1_pd = 1;
            phy_control.bf.phy_1_refclksel = 2;  /* if 24MHz */
	    	phy_control.bf.phy_1_ln0_resetn_i = 0;
	    	phy_control.bf.phy_1_cmu_resetn_i = 0;
            break;      
        case 2: 
            phy_control.bf.phy_2_por_n_i = 0;
            phy_control.bf.phy_2_pd = 1;
            phy_control.bf.phy_2_refclksel = 2;  /* if 24MHz */
	    	phy_control.bf.phy_2_ln0_resetn_i = 0;
	    	phy_control.bf.phy_2_cmu_resetn_i = 0;
            break;      
    }
    writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
	mdelay(100);

	/* Release the power on reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
            por_n_i = 1'b1
            pd = 1'b0
       If reference clock is 24MHz, then program refclksel = 2'b10
       If reference clock is 100MHz, then program refclksel = 2'b00
    */
    phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
	switch (phy_number)
	{
		case 0:
        	phy_control.bf.phy_0_por_n_i = 1;
        	phy_control.bf.phy_0_pd = 0;
        	phy_control.bf.phy_0_refclksel = 2;  /* if 24MHz */
			break;
		case 1:
        	phy_control.bf.phy_1_por_n_i = 1;
        	phy_control.bf.phy_1_pd = 0;
        	phy_control.bf.phy_1_refclksel = 2;  /* if 24MHz */
			break;
		case 2:
        	phy_control.bf.phy_2_por_n_i = 1;
        	phy_control.bf.phy_2_pd = 0;
        	phy_control.bf.phy_2_refclksel = 2;  /* if 24MHz */
			break;
    }
    writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
	udelay(10);

	switch (phy_number)
    {
        case 0:
			if (is_24mhz)
				sb_phy_program_24mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0);
			else
				sb_phy_program_100mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0);
			break;
		case 1:
			if (is_24mhz)
        		sb_phy_program_24mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x4000);
			else
        		sb_phy_program_100mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x4000);
			break;
		case 2:
			if (is_24mhz)
        		sb_phy_program_24mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x8000);
			else
        		sb_phy_program_100mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x8000);
			break;
    }
	/* Release the cmu reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
            cmu_resetn_i = 1'b1
    */      
    phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
	switch (phy_number)
    {
        case 0:
        	phy_control.bf.phy_0_cmu_resetn_i = 1;
			break;
		case 1:
        	phy_control.bf.phy_1_cmu_resetn_i = 1;
			break;
		case 2:
        	phy_control.bf.phy_2_cmu_resetn_i = 1;
			break;
    }
    writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));

    /* Wait for CMU OK */ 
    for (i = 0; i < 1000; i++)
    {
        cmu_ok.wrd = readl(IO_ADDRESS(PCIE_SATA_PCIE_GLBL_CMU_OK_CORE_DEBUG_13 + phy_number * 0x400));
        if (cmu_ok.bf.phy_cmu_ok == 1)
            break;
    
        udelay(100);
    }


    /* Release Lane0 master reset */
    /* Register: Common Lane Register 0 Base address +'d200 = 32'h400290c2 */
    com_lane.wrd = 0x400290c2;
    writel(com_lane.wrd, IO_ADDRESS(PCIE_SATA_SNOW_PHY_COM_LANE_REG3_REG2_REG1_REG0 + phy_number * 0x4000));


    /* Release the lane reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
                ln0_resetn_i = 1'b1
    */
    phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));

	switch (phy_number)
    {	
		case 0:
        	phy_control.bf.phy_0_ln0_resetn_i = 1;
			break;
		case 1:
        	phy_control.bf.phy_1_ln0_resetn_i = 1;
			break;
		case 2:
        	phy_control.bf.phy_2_ln0_resetn_i = 1;
			break;
    }
    writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
    mdelay(100);

    //reg_offset = GLOBAL_BLOCK_RESET;
    //block_reset.wrd = readl(IO_ADDRESS(reg_offset));
    //if (phy_number == 0)
    //	block_reset.bf.reset_pcie0 = 1;
    //else if (phy_number == 1)
    //    block_reset.bf.reset_pcie1 = 1;
    //else if (phy_number == 2)
    //    block_reset.bf.reset_pcie2 = 1;
    //writel(block_reset.wrd, IO_ADDRESS(GLOBAL_BLOCK_RESET));
    //msleep(10);
	//if (phy_number == 0)
    //	block_reset.bf.reset_pcie0 = 0;
    //else if (phy_number == 1)
    //	block_reset.bf.reset_pcie1 = 0;
    //else if (phy_number == 2)
    //    block_reset.bf.reset_pcie2 = 0;
    //writel(block_reset.wrd, IO_ADDRESS(GLOBAL_BLOCK_RESET));
    //msleep(10);
}

extern void g2_pcie_reinit(unsigned int phy_id);

/*
 * Configure PCIE running mode (run, suspend or power down)
 * dev_id	CS device ID
 * pcie_id	PCIE ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_pcie_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pcie_id,
CS_IN cs_pm_state_t  state)
{
	GLOBAL_PHY_CONTROL_t glo_phy_ctrl;
	char *ptr;
	u32 sb_clock_ref;
	
	if(pcie_id >= 0x4  || state >= 0x3)
	{
		printk("Error parameter!!!\n");
		return CS_E_ERROR;
	}
	
	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("no SB_PHY found !!!\r\n");
		return CS_E_ERROR;
	}
	ptr += strlen("SB_PHY") + 1;

	if (ptr[pcie_id] == 'P') {
		glo_phy_ctrl.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
		
		switch (state) {
			case CS_PM_STATE_POWER_DOWN:
				/* powerdown phy*/
				switch (pcie_id) {
					case 0:
						glo_phy_ctrl.bf.phy_0_por_n_i = 0;
						glo_phy_ctrl.bf.phy_0_pd = 1;
						glo_phy_ctrl.bf.phy_0_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_0_ln0_resetn_i = 0;
						printk("PCIe[0] PD ~\n");
						break;
					case 1:
						glo_phy_ctrl.bf.phy_1_por_n_i = 0;
						glo_phy_ctrl.bf.phy_1_pd = 1;
						glo_phy_ctrl.bf.phy_1_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_1_ln0_resetn_i = 0;
						printk("PCIe[1] PD ~\n");
						break;
					case 2:
						glo_phy_ctrl.bf.phy_2_por_n_i = 0;
						glo_phy_ctrl.bf.phy_2_pd = 1;
						glo_phy_ctrl.bf.phy_2_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_2_ln0_resetn_i = 0;
						printk("PCIe[2] PD ~\n");
						break;
					case 3:
						glo_phy_ctrl.bf.phy_3_por_n_i = 0;
						glo_phy_ctrl.bf.phy_3_pd = 1;
						glo_phy_ctrl.bf.phy_3_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_3_ln0_resetn_i = 0;
						printk( "PCIe[3] PD ~\n");
						break;
				}
				writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
				break;
			case CS_PM_STATE_SUSPEND:
				printk( "Not support ~\n");
				break;
			case CS_PM_STATE_NORMAL:
				
//#ifndef CONFIG_PCIE_EXTERNAL_CLOCK 
//				pcie_sb_phy_program(1, pcie_id);
//#else
//				pcie_sb_phy_program(0, pcie_id);
//#endif
				g2_pcie_reinit(pcie_id);
				break;
		}
		
		return CS_E_OK;
	}
	else
	{
		printk("port[%x] is not PCIe port !!!\r\n",pcie_id);
		return CS_E_ERROR;
	}
	
}
EXPORT_SYMBOL(cs_pm_pcie_state_set);

/*
 * Retrieve PCIE running mode (run, suspend or power down) setting
 * dev_id	CS device ID
 * pcie_id	PCIE ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_pcie_state_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pcie_id,
CS_OUT cs_pm_state_t  *state)
{
	GLOBAL_PHY_CONTROL_t glo_phy_ctrl;
	char *ptr;
	
	if(pcie_id >= 0x4)
	{
		printk("Error parameter!!!\n");
		return CS_E_ERROR;
	}
	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("no SB_PHY found !!!\r\n");
		return CS_E_ERROR;
	}
	ptr += strlen("SB_PHY") + 1;

	if (ptr[pcie_id] == 'P') {
		glo_phy_ctrl.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
		
		switch (pcie_id) {
			case 0:
				if(glo_phy_ctrl.bf.phy_0_pd == 1)
					*state = CS_PM_STATE_POWER_DOWN;
				else
					*state = CS_PM_STATE_NORMAL;
					
				return CS_E_OK;
				//break;
			case 1:
				if(glo_phy_ctrl.bf.phy_1_pd == 1)
					*state = CS_PM_STATE_POWER_DOWN;
				else
					*state = CS_PM_STATE_NORMAL;
					
				return CS_E_OK;
				//break;
			case 2:
				if(glo_phy_ctrl.bf.phy_2_pd == 1)
					*state = CS_PM_STATE_POWER_DOWN;
				else
					*state = CS_PM_STATE_NORMAL;
					
				return CS_E_OK;
				//break;
			case 3:
				if(glo_phy_ctrl.bf.phy_3_pd == 1)
					*state = CS_PM_STATE_POWER_DOWN;
				else
					*state = CS_PM_STATE_NORMAL;
					
				return CS_E_OK;
		}
	}
	else
	{
		printk("port[%x] is not PCIe port !!!\r\n",pcie_id);
		return CS_E_ERROR;
	}
	
}
EXPORT_SYMBOL(cs_pm_pcie_state_get);

struct PhyConfig
{
	u16 offset;
	u32 value;
};

static int issueAHCICmd(unsigned int port_mmio, const unsigned char *fis,
			int fis_len, void *buf, int buf_len,
			unsigned int timeout)
{

	memcpy((unsigned char *)priv.cmd_tbl, fis, fis_len);

	priv.cmd_sg->addr = (unsigned int)buf;

	if (buf_len > 0) {
		priv.cmd_sg->data_byte_count = buf_len - 1;
		priv.cmd_slot->descInfo = (fis_len >> 2) | (1 << 16);
	} else {
		priv.cmd_slot->descInfo = fis_len >> 2;
		priv.cmd_sg->data_byte_count = 0;
	}

	writel(1, IO_ADDRESS(port_mmio + AHCI_PxCI));

	if (waiting_register(port_mmio + AHCI_PxCI, 0x01, 0x0, timeout)) {
		return 1;
	}

	writel(0, IO_ADDRESS(port_mmio + AHCI_PxCI));
	return 0;
}

static int do_ahci_set_feature(unsigned int port_mmio)
{
	static const unsigned char fis[20] = {
		[0] = 0x27,
		[1] = 1 << 7,
		[2] = 0xEF,
		[3] = CS_SETF_CMD_XFER,
		[12] = 0x46,
	};

	if(issueAHCICmd(port_mmio, fis, sizeof(fis), NULL, 0,
			    SET_FEATURE_TIMEOUT))
	{
		printk("issue set feature ok.\n");
		return 0;
	}
	else
	{
		printk("issue set feature fail.\n");
		return 1;
	}
}

static int do_ahci_identify(unsigned int port_mmio)
{
	unsigned short *tmpid;
	volatile unsigned int tmp;
	int i;

	tmpid = (u16) kmalloc( IDEN_BUF_SIZE, GFP_KERNEL);

	static const unsigned char fis[20] = {
		[0] = 0x27,
		[1] = 1 << 7,
		[2] = 0xEC,
	};

	for (i = 0; i < IDENTIFY_TIMEOUT; ++i) {
		tmp = readl(IO_ADDRESS(port_mmio + AHCI_PxSIG));
		mdelay(1);
		if (tmp != 0xffffffff) {
			break;
		}
	}

	if (i >= IDENTIFY_TIMEOUT) {
		kfree( tmpid);
		return 1;
	}

	if (issueAHCICmd
	    (port_mmio, &fis[0], sizeof(fis), tmpid, IDEN_BUF_SIZE,
	     IDENTIFY_TIMEOUT) == 0 ) {
		printk("issue IDEN_BUF_SIZE fail.\n");
		kfree( tmpid);
		return 1;
	}

	if (tmpid[2] == 0x37c8 || tmpid[2] == 0x738c) {

		static const u8 SPINUP_fis[20] = {
			[0] = 0x27,
			[1] = 1 << 7,
			[2] = 0xEF,
			[3] = 0x07,
			[7] = 0xA0,
		};

		if (issueAHCICmd
		    (port_mmio, &SPINUP_fis[0], sizeof(SPINUP_fis), 0, 0, 
		     SPINUP_TIMEOUT) == 0) {
		     	printk("issue spin up fail.\n");
			kfree( tmpid);
			return 1;
		}
	}

	kfree( tmpid);
	return 0;
}

static int hostport_init(int port, u32 ai_mem)
{
	unsigned int i, tmp, port_mmio;
	unsigned int mem ;
	static unsigned int ahci_mem= 0;
	
	port_mmio = GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80;
	
	priv.port_mmio = port_mmio;
		
	static const unsigned int value[] = { 0x304, 0x301, 0x300 };
	/* Phy reset  */
	for (i = 0; i < sizeof(value) / sizeof(value[0]); ++i) {
		tmp = readl(IO_ADDRESS(port_mmio + AHCI_PxSCTL));
		tmp = (tmp & 0x0f0) | value[i];
		writel(tmp, IO_ADDRESS(port_mmio + AHCI_PxSCTL));
		mdelay(1);
	}
	
	/* make sure port is not active */
	tmp = readl(IO_ADDRESS(port_mmio + AHCI_PxCMD));
	i = AHCI_PxCMD_CR | AHCI_PxCMD_FR | AHCI_PxCMD_FRE | AHCI_PxCMD_ST;
	if (tmp & i) {
		writel(tmp & ~i, IO_ADDRESS(port_mmio + AHCI_PxCMD));
		waiting_register(port_mmio + AHCI_PxCMD, AHCI_PxCMD_CR, 0,
				 1000);
	}
	
	writel(AHCI_PxCMD_SUD, IO_ADDRESS(port_mmio + AHCI_PxCMD));

	tmp = readl(IO_ADDRESS(port_mmio + AHCI_PxSERR));
	writel(tmp, IO_ADDRESS(port_mmio + AHCI_PxSERR));

	if (!waiting_register(port_mmio + AHCI_PxSSTS, 0x0f, 0x03, 5000)) {
		printk("link failed\n");
		return -1;
	}

	tmp = readl(IO_ADDRESS(port_mmio + AHCI_PxSERR));
	writel(tmp, IO_ADDRESS(port_mmio + AHCI_PxSERR));

	if ( ahci_mem == 0) {
		ahci_mem = (u32) kmalloc(AHCI_PORT_PRIV_SZ + 0x400, GFP_KERNEL);
		ahci_mem = (ahci_mem + 0x400) & (~0x3ff); /* Aligned to 1024-bytes */
	}

	printk("ai_mem (%x) ahci_mem(%x)\n",ai_mem, ahci_mem);
	
	mem = ahci_mem;
	memset((u8 *) mem, 0, AHCI_PORT_PRIV_SZ);

	writel(mem, IO_ADDRESS(port_mmio + AHCI_PxCLB));
	priv.cmd_slot = (struct ahci_cmd_hdr *)mem;

	mem += 256;
	writel(mem, IO_ADDRESS(port_mmio + AHCI_PxFB));

	mem += AHCI_RX_FIS_SZ;
	priv.cmd_slot->cmdStatus = 0;
	priv.cmd_slot->tblBaseAddr = mem;
	priv.cmd_tbl = mem;

	mem += AHCI_CMD_TBL_HDR;
	priv.cmd_sg = (struct ahci_sg *)mem;
	priv.cmd_sg->addr_hi = 0;

	tmp =
	    AHCI_PxCMD_ICC | AHCI_PxCMD_FRE | AHCI_PxCMD_POD | AHCI_PxCMD_SUD |
	    AHCI_PxCMD_ST;
	writel(tmp, IO_ADDRESS(port_mmio + AHCI_PxCMD));


	return 0;
	
}

static sb_phy_init( u32 port, u32 external_clock)
{
	static const struct PhyConfig snowbush_24_setting[] = {
		{0, 0x00890006},
		{4, 0x0003c97b},
		{32, 0x54a00000},
		{44, 0x50040000},
		{48, 0x40250270},
		{52, 0x00004001},
		{96, 0x5e002e00},
		{100, 0xf0d14200},
		{104, 0xb92c7828},
		{108, 0x0000035e},
		{120, 0x00000002},
		{124, 0x04841000},
		{128, 0x000000e0},
		{132, 0x04400023},
		{136, 0x680017d0},
		{140, 0x0d181ef2},
		{144, 0x0000000c},
		{196, 0x0f600000},
		{200, 0x4000a000},
		{204, 0x4919ae24},
		{208, 0xc54b8304},
		{212, 0x98280301},
		{216, 0x00000019},
		{220, 0x0000d0f2},
		{232, 0xa0a0a000},
		{236, 0xa0a0a0a0},
		{240, 0x58800054},
		{244, 0x865c4400},
		{248, 0x9009d08d},
		{252, 0x00004007},
		{256, 0x00322000},
		{300, 0xd8000000},
		{304, 0x0011ff1a},
		{308, 0xf0000000},
		{312, 0xffffffff},
		{316, 0x3fc3c21c},
		{320, 0x0000000a},
		{324, 0x00f80000},
		{0, 0x00890007},
	};

	static const struct PhyConfig snowbush_60_setting[] = {
		{ 0   ,0x00810006},
		{ 32  ,0x64a00000},
		{ 44  ,0x50040000},
		{ 48  ,0x40250270},
		{ 52  ,0x00004001},
		{ 96  ,0x5e002e00},
		{ 100 ,0x20d14200},
		{ 104 ,0xce447828},
		{ 108 ,0x0000030b},
		{ 120 ,0x00000002},
		{ 124 ,0x04841000},
		{ 128 ,0x000000e0},
		{ 132 ,0x04000023},
		{ 136 ,0x68000438},
		{ 140 ,0x0d181ef2},
		{ 144 ,0x0000000d},
		{ 196 ,0x0f600000},
		{ 200 ,0x40002000},
		{ 204 ,0x4919ae24},
		{ 208 ,0xc6482304},
		{ 212 ,0x98280301},
		{ 216 ,0x80000019},
		{ 220 ,0x0000d0f0},
		{ 232 ,0xa0a0a000},
		{ 236 ,0xa0a0a0a0},
		{ 240 ,0x58800064},
		{ 244 ,0x865c4400},
		{ 248 ,0x9009d08d},
		{ 252 ,0x00004007},
		{ 256 ,0x00322000},
		{ 300 ,0xd8000000},
		{ 304 ,0x0011ff1a},
		{ 308 ,0xf0000000},
		{ 312 ,0xffffffff},
		{ 316 ,0x3fc3c21c},
		{ 320 ,0x0000000a},
		{ 324 ,0x00f80000},
		{ 0,   0x00810007},
	};

	const struct PhyConfig *p;
	const struct PhyConfig *endp;

	if( external_clock) {
		p = snowbush_60_setting;
		endp = &snowbush_60_setting[ ARRAY_SIZE(snowbush_60_setting)];
	} else {
		p = snowbush_24_setting;
		endp = &snowbush_24_setting[ ARRAY_SIZE(snowbush_24_setting)];
	}


	for (; p != endp; ++p) {
		writel(p->value, IO_ADDRESS(SNOW_PHY_BASE_ADDR + port * 0x4000 + p->offset));
	}
}

static int ahci_phy_init( u32 port, u32 external_clock, u32 count, u32 port_implement)
{
	int i, sb_clock_ref,tmp,tmp_int;
	unsigned int mem ;
	GLOBAL_PHY_CONTROL_t phy_control;
	GLOBAL_GLOBAL_CONFIG_t global_config;
	GLOBAL_BLOCK_RESET_t block_reset;
	PCIE_SATA_SATA2_PHY_SATA_CORE_PHY_STAT_t phy_stat;
	PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1_t sata_ctrl;
	GLOBAL_STRAP_t global_strap;
	
	tmp_int = readl(IO_ADDRESS(0xF4100294));
	writel(0, IO_ADDRESS(0xF4100294));
	
	
	mem = readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80 + AHCI_PxCLB));
	
	/* step 0 */
	phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
	switch (port) {
	case 0:
		phy_control.bf.phy_0_por_n_i = 0;
		phy_control.bf.phy_0_pd = 1;
		phy_control.bf.phy_0_cmu_resetn_i = 0;
		phy_control.bf.phy_0_ln0_resetn_i = 0;
		break;
	case 1:
		phy_control.bf.phy_1_por_n_i = 0;
		phy_control.bf.phy_1_pd = 1;
		phy_control.bf.phy_1_cmu_resetn_i = 0;
		phy_control.bf.phy_1_ln0_resetn_i = 0;
		break;
	case 2:
		phy_control.bf.phy_2_por_n_i = 0;
		phy_control.bf.phy_2_pd = 1;
		phy_control.bf.phy_2_cmu_resetn_i = 0;
		phy_control.bf.phy_2_ln0_resetn_i = 0;
		break;
	case 3:
		phy_control.bf.phy_3_por_n_i = 0;
		phy_control.bf.phy_3_pd = 1;
		phy_control.bf.phy_3_cmu_resetn_i = 0;
		phy_control.bf.phy_3_ln0_resetn_i = 0;
		break;
	}

	writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
	mdelay(100);
	
	printk("1: %x \n", readl(IO_ADDRESS(0xF4100294)));

	if( external_clock ) {
		sb_clock_ref = 1;
	} else {
		sb_clock_ref = 2;
	}

	/* step1 :Release the power on reset by programming por_n_i to high */
	phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
	switch (port) {
	case 0:
		phy_control.bf.phy_0_por_n_i = 1;
		phy_control.bf.phy_0_pd = 0;
		phy_control.bf.phy_0_refclksel = sb_clock_ref;
		break;
	case 1:
		phy_control.bf.phy_1_por_n_i = 1;
		phy_control.bf.phy_1_pd = 0;
		phy_control.bf.phy_1_refclksel = sb_clock_ref;
		break;
	case 2:
		phy_control.bf.phy_2_por_n_i = 1;
		phy_control.bf.phy_2_pd = 0;
		phy_control.bf.phy_2_refclksel = sb_clock_ref;
		break;
	case 3:
		phy_control.bf.phy_3_por_n_i = 1;
		phy_control.bf.phy_3_pd = 0;
		phy_control.bf.phy_3_refclksel = sb_clock_ref;
		break;
	}

	writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
	mdelay(1);
	printk("2: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/* step 2: Program the Snowbush PHY registers */
	sb_phy_init(port, external_clock);
	printk("3: %x \n", readl(IO_ADDRESS(0xF4100294)));

	/* step 5: Global_CONFIG */
	if (port < 3) {
		global_config.wrd = readl(IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));

		switch (port) {
		case 0:
			global_config.bf.cfg_sata_0_clken = 1;
			global_config.bf.cfg_pcie_0_clken = 0;
			break;
		case 1:
			global_config.bf.cfg_sata_1_clken = 1;
			global_config.bf.cfg_pcie_1_clken = 0;
			break;
		case 2:
			global_config.bf.cfg_sata_2_clken = 1;
			global_config.bf.cfg_pcie_2_clken = 0;
			break;
		}

		writel(global_config.wrd, IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));
	}
	printk("4: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/* step 6:  After completing the register programming (3) release
	   the cmu reset */

	phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));

	switch (port) {
	case 0:
		phy_control.bf.phy_0_cmu_resetn_i = 1;
		break;
	case 1:
		phy_control.bf.phy_1_cmu_resetn_i = 1;
		break;
	case 2:
		phy_control.bf.phy_2_cmu_resetn_i = 1;
		break;
	case 3:
		phy_control.bf.phy_3_cmu_resetn_i = 1;
		break;
	}

	writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
printk("5: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/* step7: Wait for CMU OK to be set by reading the following register */

	for (i = 0; i < 1000; i++) {
		phy_stat.wrd =
		    readl(IO_ADDRESS(PCIE_SATA_SATA2_PHY_SATA_CORE_PHY_STAT + port * 0x80));
		if (phy_stat.bf.cmu_ok == 1) {
			break;
		}

		udelay(1000);
	}
	if ( i >= 1000 ) {
		printk( "SATA PHY initialization, CMU failed\n" );
		return 1;
	}
printk("6: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/* step8: Release the lane reset */
	writel(0x4000a002,
		IO_ADDRESS(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + port * 0x4000 + 200 )) ;

	phy_control.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));

	switch (port) {
	case 0:
		phy_control.bf.phy_0_ln0_resetn_i = 1;
		break;
	case 1:
		phy_control.bf.phy_1_ln0_resetn_i = 1;
		break;
	case 2:
		phy_control.bf.phy_2_ln0_resetn_i = 1;
		break;
	case 3:
		phy_control.bf.phy_3_ln0_resetn_i = 1;
		break;
	}
	writel(phy_control.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
printk("7: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/* step9 : Wait for lane OK signal of PHY to be set */
	for (i = 0; i < 1000; i++) {
		phy_stat.wrd =
		    readl(IO_ADDRESS(PCIE_SATA_SATA2_PHY_SATA_CORE_PHY_STAT + port * 0x80));
		if (phy_stat.bf.lane_ok == 1) {
			break;
		}

		udelay(1000);
	}

	if ( i >= 1000 ) {
		printk( "SATA PHY initialization, lan_ok failed: port=%d\n", port );
		return 1;
	}
	
	
printk("8: %x \n", readl(IO_ADDRESS(0xF4100294)));	
	///////
	//block_reset.wrd = readl(IO_ADDRESS(GLOBAL_BLOCK_RESET));
	//block_reset.bf.reset_phy = 1;
	//writel(block_reset.wrd, IO_ADDRESS(GLOBAL_BLOCK_RESET));
        //
	//msleep(1);
        //
	//block_reset.wrd = readl(IO_ADDRESS(GLOBAL_BLOCK_RESET));
	//block_reset.bf.reset_phy = 0;
	//writel(block_reset.wrd, IO_ADDRESS(GLOBAL_BLOCK_RESET));
	//msleep(1);
	/////
		writel(tmp_int, IO_ADDRESS(0xF4100294));
	sata_ctrl.wrd = readl(IO_ADDRESS(PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1 +
				  i * 0x80));

	sata_ctrl.bf.phy_rx_data_vld_mctrl = 1;

	sata_ctrl.bf.ctrl_spd_mode = 1;
	sata_ctrl.bf.ctrl_calibrated = 1;
	writel(sata_ctrl.wrd,
	       IO_ADDRESS(PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1 + i * 0x80));

	sata_ctrl.wrd =
	    readl(IO_ADDRESS(PCIE_SATA_SATA2_CTRL_SATA_CORE_CTRL1 +
		  i * 0x80));
	mb();
printk("9: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/////* G.CAP , set it capability */
	//writel(0x6f26ff80 | (count - 1), IO_ADDRESS(GOLDENGATE_AHCI_BASE));
        //
	///* G.PI */
	//writel(port_implement, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_PI));
	//printk("base (%x)   GPI(%x) (c:%x po:%x) \n",readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE)),readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_PI)), count, port_implement);

	global_strap.wrd = readl(IO_ADDRESS(GLOBAL_STRAP));
	switch (global_strap.bf.speed) {
	case 0:		/* 133 MHZ */
		tmp = 0x0A132039;
		break;
	case 2:		/* 140 MHZ */
		tmp = 0x0A14223C;
		break;
	case 1:
	case 4:
	case 5:		/* 150 MHZ */
		tmp = 0x0B152540;
		break;
	case 3:		/* 160 MHZ */
		tmp = 0x0B172744;
		break;
	case 6:		/* 141.67 MHZ */
		tmp = 0x0A14223C;
		break;
	default:
		printk("Unknown frequency!");
		return 1;
	}
printk("a: %x \n", readl(IO_ADDRESS(0xF4100294)));
	/* TESTR enable OOBR write first */
	writel(port << 16, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_TESTER));

	writel(0x80000000, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_OOBR));
	writel(tmp, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_OOBR));

	writel(0x00000000, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_TESTER));

	tmp = readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_CTL));
	tmp |= AHCI_CTL_HR;
	writel(tmp, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_CTL));
printk("b: %x \n", readl(IO_ADDRESS(0xF4100294)));
	for (i = 0; i < 3000; i++) {
		tmp =
		    readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_CTL));
		if ( (tmp & AHCI_CTL_HR)== 0) {
			break;
		}

		udelay(1000);
	}

	if ( i >= 3000 ) {
		printk( "SATA PHY initialization, AHCI_CTL_HR failed: port=%d\n", port );
		return 1;
	}

	writel(0x00003700, IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_BISTCR));
	
	
printk("c: %x start reset port\n", readl(IO_ADDRESS(0xF4100294)));	

	
	static const unsigned int value[] = { 0x304, 0x301, 0x300 };
	/* Phy reset  */
	for (i = 0; i < sizeof(value) / sizeof(value[0]); ++i) {
		tmp = readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80 + AHCI_PxSCTL));
		tmp = (tmp & 0x0f0) | value[i];
		writel(tmp, IO_ADDRESS(GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80 + AHCI_PxSCTL));
		mdelay(1);
	}
	
	i=5;
	do {
		tmp = readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80 + AHCI_PxSCTL));
		
	}while((tmp & 0xf0f) != 0x300 && --i);

	//if(hostport_init(port, mem))
	//{
	//	printk( "Host port initial fail.\n");
	//	return 1;
	//}
	//
	//if(do_ahci_identify(GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80))
	//{
	//	printk( "do ahci identify fail.\n");
	//	return 1;
	//}
	//
	//if(do_ahci_set_feature(GOLDENGATE_AHCI_BASE + 0x100 + port * 0x80))
	//{
	//	printk( "do ahci set feature fail.\n");
	//	return 1;
	//}
	if ((tmp & 0xf0f) != 0x300) {
		printk("failed to resume link (SControl %X)\n",tmp);
		return CS_E_ERROR;
	}
	else
		return CS_E_OK;
}

/*
 * Configure SATA running mode (run, suspend or power down)
 * dev_id	CS device ID
 * sata_id	SATA ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_sata_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  sata_id,
CS_IN cs_pm_state_t  state)
{
	u32 reg_v, i, count=0,port_implement=0;	
	char *ptr, *sb_extern_ptr;
	unsigned char *addr;
	GLOBAL_PHY_CONTROL_t glo_phy_ctrl;

	if(sata_id >= 0x4 || state >=3)
	{
		printk("Error parameter!!!\n");
		return CS_E_ERROR;
	}


	sb_extern_ptr = strstr(saved_command_line, "SB_EXTERN");
	/*
	// if need to initial extern device then initial it.
	if( sb_extern_ptr ) {
		si5338_init();
		msleep(1000);
	}
	*/
	
	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("%s: no SB_PHY found !!!\r\n");
		return CS_E_ERROR;
	}
	ptr += strlen("SB_PHY") + 1; 
	
	for (i = 0; i < 4; i++) {
		if (ptr[i] == 'S') {
			count++;
			port_implement |= (0x01 << i);
		}
	}
	if (ptr[sata_id] == 'S') {
		glo_phy_ctrl.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
		addr = GOLDENGATE_AHCI_BASE + 0x100 + 0x80*sata_id;
		switch (state) {
			case CS_PM_STATE_POWER_DOWN:
				//printk("CS_PM_STATE_POWER_DOWN\n");
				if(((glo_phy_ctrl.wrd&0xf00)&((1<<sata_id)<<8))>0)
				{
					printk("port[%x] has been power down!!!\n",sata_id);
					break;
				}
#if 0
				/* enter slumber mode */
				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x2C));
				writel(reg_v & 0xFFFFFCFF, IO_ADDRESS((unsigned int)addr + 0x2C));

				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x14));
				writel(reg_v & 0xffbfffff, IO_ADDRESS((unsigned int)addr + 0x14));

				writel(0, addr + 0x2C);

				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x18));
				reg_v &= 0x8fffffff;
				reg_v |= 0x60000000;
				writel(reg_v, IO_ADDRESS((unsigned int)addr + 0x18));
				
				udelay(100);
				i=0;
				do{
					reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x28));
					//printk("%s: port[%x] reg_v(%x)  (%x)<11\n",sata_id,reg_v,readl(IO_ADDRESS((unsigned int)addr + 0x28)));
					i++;
				
				}while(((reg_v&0xf00) != 0x600) && i<20);
#endif				
				/* powerdown phy*/
				switch (sata_id) {
					case 0:
						glo_phy_ctrl.bf.phy_0_por_n_i = 0;
						glo_phy_ctrl.bf.phy_0_pd = 1;
						glo_phy_ctrl.bf.phy_0_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_0_ln0_resetn_i = 0;
						//printk("PD 0~\n");
						break;
					case 1:
						glo_phy_ctrl.bf.phy_1_por_n_i = 0;
						glo_phy_ctrl.bf.phy_1_pd = 1;
						glo_phy_ctrl.bf.phy_1_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_1_ln0_resetn_i = 0;
						//printk("PD 1~\n");
						break;
					case 2:
						glo_phy_ctrl.bf.phy_2_por_n_i = 0;
						glo_phy_ctrl.bf.phy_2_pd = 1;
						glo_phy_ctrl.bf.phy_2_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_2_ln0_resetn_i = 0;
						//printk("PD 2~\n");
						break;
					case 3:
						glo_phy_ctrl.bf.phy_3_por_n_i = 0;
						glo_phy_ctrl.bf.phy_3_pd = 1;
						glo_phy_ctrl.bf.phy_3_cmu_resetn_i = 0;
						glo_phy_ctrl.bf.phy_3_ln0_resetn_i = 0;
						//printk("PD 3~\n");
						break;
				}
				writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
				
				break;

			case CS_PM_STATE_SUSPEND:
				//printk("CS_PM_STATE_SUSPEND\n");
				if(((glo_phy_ctrl.wrd&0xf00)&((1<<sata_id)<<8))>0)
				{
					printk("port[%x] has been power down!!!\n",sata_id);
					break;
				}

				//printk("pd(%x) !!!\r\n",((glo_phy_ctrl.wrd&0xf00)&((1<<sata_id)<<8)));

				/* check if active mode then change to slumber mode */
				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x28));
				reg_v &= 0xf00;
				//printk("port[%x] reg_v(%x)!!\n",sata_id,reg_v);
				
				if((reg_v & 0x600) == 0x600)
				{
					//printk("port[%x] alreay in slumber mode(reg_v)!!\n",sata_id,reg_v);
					break;
				}

				/*
				 * suspend
				 * set to slumber mode
				 */
				//printk("set to slumber mode\n ");
				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x2C));
				writel(reg_v & 0xFFFFFCFF, IO_ADDRESS((unsigned int)addr + 0x2C));

				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x14));
				writel(reg_v & 0xffbfffff, IO_ADDRESS((unsigned int)addr + 0x14));

				writel(0, addr + 0x2C);

				reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x18));
				reg_v &= 0x8fffffff;
				reg_v |= 0x60000000;
				writel(reg_v, IO_ADDRESS((unsigned int)addr + 0x18));

				i=0;
				do{
					reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x28));
					//printk("port[%x] reg_v(%x)  (%x)<22\n", sata_id,reg_v,readl(IO_ADDRESS((unsigned int)addr + 0x28)));
					i++;
				
				}while(((reg_v&0xf00) != 0x600) && i<20);
				break;

			default : /** CS_PM_STATE_NORMAL **/ 
				//printk("CS_PM_STATE_NORMAL!!!\r\n");
				/** check if need to re-initial phy **/
				if(((glo_phy_ctrl.wrd&0xf00)&((1<<sata_id)<<8))>0)
				{
#if 0					
					//printk("Please insmod ahci module\r\n");
					//printk("initial phy!!! sb_extern_ptr(%x)\r\n",sb_extern_ptr);
					if(ahci_phy_init(sata_id, sb_extern_ptr, count, port_implement))
					{
						printk(" PHY initial error !!!\r\n");
						return CS_E_ERROR;
					}	
#else

				switch (sata_id) {
					case 0:
						glo_phy_ctrl.bf.phy_0_por_n_i = 1;
						glo_phy_ctrl.bf.phy_0_pd = 0;
						break;
					case 1:
						glo_phy_ctrl.bf.phy_1_por_n_i = 1;
						glo_phy_ctrl.bf.phy_1_pd = 0;
						break;
					case 2:
						glo_phy_ctrl.bf.phy_2_por_n_i = 1;
						glo_phy_ctrl.bf.phy_2_pd = 0;
						break;
					case 3:
						glo_phy_ctrl.bf.phy_3_por_n_i = 1;
						glo_phy_ctrl.bf.phy_3_pd = 0;
						break;
				}
				writel(glo_phy_ctrl.wrd, IO_ADDRESS(GLOBAL_PHY_CONTROL));
#endif						
				}
				else
				{
					/* check if slumber mode then change to active mode */
					reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x28));
					reg_v &= 0xf00;
					if((reg_v & 0x100) == 0x100)
					{
						//printk("port[%x] already in active mode!!\n",sata_id);
						break;
					}
                                	
					/*
					 * resume 
					 * set to active mode
					 */ 
					 //printk("set to active mode \n ");
					reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x18));
					reg_v &= 0x8fffffff;
					reg_v |= 0x10000000;
					//printk("reg_v(%x)!!\n",reg_v);
					writel(reg_v, IO_ADDRESS((unsigned int)addr + 0x18));
			        	
					//writel(((reg_v & 0x8fffffff) | 0x10000000),ahci_base + 0x298);
					msleep(300);
                                	
					writel(0x300, addr + 0x2C);
					writel(0xFFFFFFFF, IO_ADDRESS((unsigned int)addr + 0x30));
					writel(0xFFFFFFFF, IO_ADDRESS((unsigned int)addr + 0x10));
                                	
					reg_v = readl((unsigned int)addr + 0x14);
					writel(reg_v & 0xffbfffff, IO_ADDRESS((unsigned int)addr + 0x14));
					
					i=0;
					do{
						reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x28));
						//printk("port[%x] reg_v(%x)  (%x)<33\n",sata_id,reg_v,readl(IO_ADDRESS((unsigned int)addr + 0x28)));
						i++;
					
					}while(((reg_v&0xf00) != 0x100) && i<20);
				}

			break;

		}
		return CS_E_OK;
	}
	else
	{
		printk("port[%x] is not SATA port !!!\r\n",sata_id);
		return CS_E_ERROR;
	}

	
}
EXPORT_SYMBOL(cs_pm_sata_state_set);

/*
 * Retrieve SATA running mode (run, suspend or power down) setting
 * dev_id	CS device ID
 * sata_id	SATA ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_sata_state_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  sata_id,
CS_OUT cs_pm_state_t  *state)
{
	u32 reg_v;
	char *ptr;
	unsigned char *addr;
	GLOBAL_PHY_CONTROL_t glo_phy_ctrl;

	if(sata_id >= 0x4)
	{
		printk("Error parameter!!!\n");
		return CS_E_ERROR;
	}

	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL) {
		printk("no SB_PHY found !!!\r\n");
		return CS_E_ERROR;
	}
	ptr += strlen("SB_PHY") + 1;

	if (ptr[sata_id] == 'S') {

		glo_phy_ctrl.wrd = readl(IO_ADDRESS(GLOBAL_PHY_CONTROL));
		addr = GOLDENGATE_AHCI_BASE + 0x100 + 0x80*sata_id;
		
//printk("sata_id(%x)  pd(%x) !!!\r\n", sata_id, ((glo_phy_ctrl.wrd&0xf00)&((1<<sata_id)<<8)));

		/* powerdown phy*/
		switch (sata_id) {
			case 0:
				if(glo_phy_ctrl.bf.phy_0_pd == 1)
				{
					*state = CS_PM_STATE_POWER_DOWN;
					//printk(" Get PD 0~\n");
					return CS_E_OK;
				}
				break;
			case 1:
				if(glo_phy_ctrl.bf.phy_1_pd == 1)
				{
					*state = CS_PM_STATE_POWER_DOWN;
					//printk(" Get PD 1~\n");
					return CS_E_OK;
				}
				break;
			case 2:
				if(glo_phy_ctrl.bf.phy_2_pd == 1)
				{
					*state = CS_PM_STATE_POWER_DOWN;
					//printk(" Get PD 2~\n");
					return CS_E_OK;
				}
				break;
			case 3:
				if(glo_phy_ctrl.bf.phy_3_pd == 1)
				{
					*state = CS_PM_STATE_POWER_DOWN;
					//printk(" Get PD 3~\n");
					return CS_E_OK;
				}
				break;
		}
//printk("get base (%x)   GPI(%x) \n",readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE)),readl(IO_ADDRESS(GOLDENGATE_AHCI_BASE + AHCI_PI)));
		/* check if slumber mode then change to active mode */
		reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x28));
		reg_v &= 0xf00;
		//printk("port[%x] reg_v(%x)<66\n",sata_id,reg_v);
		if((reg_v & 0x600) == 0x600)
		{
			*state = CS_PM_STATE_SUSPEND;
			return CS_E_OK;
		}
		/* check if active mode then change to slumber mode */
		//reg_v = readl(IO_ADDRESS((unsigned int)addr + 0x18));
		//reg_v &= 0xf0000000;
		//printk("port[%x] reg_v(%x)<77\n",sata_id,reg_v);
		if((reg_v & 0x100) == 0x100)
		{
			*state = CS_PM_STATE_NORMAL;
			return CS_E_OK;
		}
		return CS_E_OK;
	}
	else
	{
		printk("port[%x] is not SATA port !!!\r\n",sata_id);
		return CS_E_ERROR;
	}

	
}
EXPORT_SYMBOL(cs_pm_sata_state_get);

/*
 * Configure PE running mode (run or suspend)
 * dev_id	CS device ID
 * pe_id	PE ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_pe_suspend_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pe_id,
CS_IN cs_boolean_t  suspend)
{
	//GLOBAL_GLOBAL_CONFIG_t glb_cfg;
	GLOBAL_RECIRC_CPU_CTL_t glb_rcpu_ctl;

	/* */
	//glb_cfg.wrd = readl(IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));
	//glb_cfg.bf.recirc_pd = 1;

	if(pe_id >= 0x2)
	{
		printk("pe_id(%x) should be 0~1 !!!\n",pe_id);
		return CS_E_ERROR;
	}

	glb_rcpu_ctl.wrd = readl(IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));
	if(pe_id==0)
		glb_rcpu_ctl.bf.rcpu0_RunStall = suspend;
	else
		glb_rcpu_ctl.bf.rcpu1_RunStall = suspend;

	writel(glb_rcpu_ctl.wrd, IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_pe_suspend_set);

/*
 * Retrieve PE running mode (run or suspend) setting
 * ddev_id	CS device ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_pe_suspend_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pe_id,
CS_OUT cs_boolean_t  *suspend)
{
	GLOBAL_RECIRC_CPU_CTL_t glb_rcpu_ctl;

	if(pe_id >= 0x2)
	{
		printk("pe_id(%x) should be 0~1 !!!\n",pe_id);
		return CS_E_ERROR;
	}

	glb_rcpu_ctl.wrd = readl(IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));

	if(pe_id==0)
		*suspend = glb_rcpu_ctl.bf.rcpu0_RunStall;
	else
		*suspend = glb_rcpu_ctl.bf.rcpu1_RunStall;


	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_pe_suspend_get);

/*
 * Configure CPU to power down
 * dev_id	CS device ID
 * cpu_id	CPU Core ID
 *
 */
cs_status_t cs_pm_cpu_power_down (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  cpu_id)
{
	GLOBAL_ARM_POWER_CONTROL_CPU1_t glb_pwr_cpu1;
	
	/*
	 * set poweroff of GLOBAL_ARM_POWER_CONTROL_CPU0 & GLOBAL_ARM_POWER_CONTROL_CPU1
	 * But it seems can't back after power down.
	 *
	 */
	 if(cpu_id==0)
	{
		printk("CPU0 should not to shutdown!\n");
		return CS_E_ERROR;
	}
	// printk("Please force CPU1 to WFI first!\nEcho 0 to /sys/devices/system/cpu/cpu1/online.\n");
	
	if(cpu_online(cpu_id)==1){
		printk("Cannot power down online CPU..\n");
		return CS_E_INUSING;
	}
 
	 glb_pwr_cpu1.wrd = 0;
	 glb_pwr_cpu1.bf.cpuclamp1 = 1;
	 writel(glb_pwr_cpu1.wrd, IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU1));
	 mdelay(10);
	 glb_pwr_cpu1.bf.poweroff = 1;
	 writel(glb_pwr_cpu1.wrd, IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU1));
/*	
	if(cpu_id==0)
	{
		printk("Can't shutdown CPU0!\n");
		return CS_E_ERROR;
	}

	cpu_hotplug_driver_lock();

	cpu_down(cpu_id);

	cpu_hotplug_driver_unlock();
*/
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_cpu_power_down);

/*
 * Configure CPU to power up
 * dev_id	CS device ID
 * cpu_id	CPU Core ID
 *
 */
cs_status_t cs_pm_cpu_power_up (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  cpu_id)
{
	GLOBAL_ARM_POWER_CONTROL_CPU1_t glb_pwr_cpu1;
	
	 if(cpu_id==0)
	{
		printk("CPU0 not support!\n");
		return CS_E_ERROR;
	}
	
	glb_pwr_cpu1.wrd = readl(IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU1));
	if(glb_pwr_cpu1.bf.poweroff)
	{
		//printk("CPU1 not support!\n");
		glb_pwr_cpu1.wrd = 0;
	 	glb_pwr_cpu1.bf.cpuclamp1 = 0;
	 	writel(glb_pwr_cpu1.wrd, IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU1));
	 	mdelay(10);
	 	glb_pwr_cpu1.bf.poweroff = 0;
	 	writel(glb_pwr_cpu1.wrd, IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU1));
	 	mdelay(10);
		
	}
	else
		printk("CPU1 is online!\n");
		
		
	  	 
/*	 
	if(cpu_id==0)
	{
		printk("CPU0 online!\n");
		return CS_E_ERROR;
	}

	cpu_hotplug_driver_lock();

	cpu_up(cpu_id);

	cpu_hotplug_driver_unlock();
*/
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_cpu_power_up);

/*
 * Configure CPU running frequency
 * dev_id	CS device ID
 * freq	CPU frequency, defined in cs_cpu_freq_t structure
 *
 */
cs_status_t cs_pm_cpu_frequency_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN cs_cpu_freq_t  freq)
{
	GLOBAL_STRAP_t glb_strap;
	unsigned int tmp;
	unsigned long flags;
	static unsigned short speeds[] = {
		[0] 0, /* 400 */
		[1] 1, /* 600 */
		[2] 2, /* 700 */
		[3] 5, /* 750 */
		[4] 3, /* 800 */
		[5] 6, /* 850 */
		[6] 4, /* 900 */
	};
	cs_pm_cpu_info_t cpu_info;

#if 0
	switch (freq) {
		case CS_CPU_FREQUENCY_400:
			printk("CS_CPU_FREQUENCY_400\n");
			break;
		case CS_CPU_FREQUENCY_600:
			printk("CS_CPU_FREQUENCY_600\n");
			break;
		case CS_CPU_FREQUENCY_700:
			printk("CS_CPU_FREQUENCY_700\n");
			break;
		case CS_CPU_FREQUENCY_800:
			printk("CS_CPU_FREQUENCY_800\n");
			break;
		case CS_CPU_FREQUENCY_900:
			printk("CS_CPU_FREQUENCY_900\n");
			break;
		case CS_CPU_FREQUENCY_750:
			printk("CS_CPU_FREQUENCY_750\n");
			break;
		case CS_CPU_FREQUENCY_850:
			printk("CS_CPU_FREQUENCY_850\n");
			break;
	}
#endif
	if(freq >= 0x7)
	{
		printk("freq(%x) should be 0~6 !!!\n",freq);
		return CS_E_ERROR;
	}

	cs_pm_cpu_info_get(dev_id, &cpu_info);
	cs_pm_freq_notify_transition(CS_PM_FREQ_PRECHANGE,
				     cpu_info.freq, freq);

	spin_lock_irqsave(&pm_api_mutex, flags);

	tmp = readl(IO_ADDRESS(GLOBAL_SPEED_OVERRIDE));

	/* Need to switch to override speed function for the first time. */
	if(tmp==0)
	{
		glb_strap.wrd = readl(IO_ADDRESS(GLOBAL_STRAP));
		tmp = glb_strap.bf.speed;
		//printk("Original CPU Speed : %d MHz\n", speeds[tmp]);
		/* write strap speed first */
		//writel(tmp, IO_ADDRESS(GLOBAL_SPEED_OVERRIDE));
		asm("isb");
		writel(tmp|0x9555AC90, IO_ADDRESS(GLOBAL_SPEED_OVERRIDE));
	}

	/* then write new setting */
	asm("isb");
	writel(speeds[freq]|0x9555AC90, IO_ADDRESS(GLOBAL_SPEED_OVERRIDE));

	/* Wait for bus clock stabilization in such manner */
	tmp = readl(IO_ADDRESS(GLOBAL_JTAG_ID));

	//printk("CPU Speed : %d MHz\n", speeds[freq]);
	spin_unlock_irqrestore(&pm_api_mutex, flags);

	cs_pm_freq_notify_transition(CS_PM_FREQ_POSTCHANGE,
				     cpu_info.freq, freq);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_cpu_frequency_set);

/*
 * Retrieve CPU power and frequency configuraion
 * dev_id	CS device ID
 * cpu_info	CPU information, such as frequency, each core power down or up, refer cs_pm_cpu_info_t
 *
 */
cs_status_t cs_pm_cpu_info_get (
CS_IN cs_dev_id_t  dev_id,
CS_OUT cs_pm_cpu_info_t 	*cpu_info)
{
	GLOBAL_STRAP_t glb_strap;
	GLOBAL_ARM_POWER_CONTROL_CPU1_t glb_pwr_cpu1;
	GLOBAL_ARM_POWER_CONTROL_CPU0_t glb_pwr_cpu0;
	unsigned int tmp;
	static unsigned short strap_speeds[] = {
		[0] 0, /* 400 */
		[1] 1, /* 600 */
		[2] 2, /* 700 */
		[3] 4, /* 800 */
		[4] 6, /* 900 */
		[5] 3, /* 750 */
		[6] 5, /* 850 */
	};

	
	glb_strap.wrd = readl(IO_ADDRESS(GLOBAL_STRAP));
	cpu_info->freq = strap_speeds[glb_strap.bf.speed];
	
	glb_pwr_cpu0.wrd = readl(IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU0));
	glb_pwr_cpu1.wrd = readl(IO_ADDRESS(GLOBAL_ARM_POWER_CONTROL_CPU1));
	
	cpu_info->cpu_run[0] = (glb_pwr_cpu0.bf.poweroff) ? 0 : 1;
	cpu_info->cpu_run[1] = (glb_pwr_cpu1.bf.poweroff) ? 0 : 1;
/*
	cpu_info->cpu_run[0] = (cpu_online(0)) ? 1 : 0;
	cpu_info->cpu_run[1] = (cpu_online(1)) ? 1 : 0;
*/
	//printk("freq(%x) cpu0(%x) cpu1(%x)\n",cpu_info->freq,cpu_info->cpu_run[0],cpu_info->cpu_run[1]);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_cpu_info_get);

/*
* Put system in standby.
* DDR will switch to self refresh mode eventually
* dev_id CS device ID
*/
cs_status_t cs_pm_system_suspend_enter (
CS_IN cs_dev_id_t   dev_id)
{
        if (pm_suspend(PM_SUSPEND_STANDBY) == 0) {
                return CS_E_OK;
        }
        return(CS_E_ERROR);
}
EXPORT_SYMBOL(cs_pm_system_suspend_enter);

/*
 *
 * Configure CPU power state 
 * dev_id	CS device ID
 * cpu_id	CPU Core ID
 * Cpu_state = CS_PM_STATE_SUSPEND  - enter WFI
 * Cpu_state = CS_PM_STATE_NORMAL  - enter active-online state     
 *
 */
cs_status_t 
cs_pm_cpu_power_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN cs_uint32_t  cpu_id,
CS_IN cs_pm_state_t    cpu_state)
{
	
	cpu_hotplug_driver_lock();

	if(cpu_state==CS_PM_STATE_NORMAL)
	{
		if(cpu_online(cpu_id)==1)
			printk("CPU%x already online!\n",cpu_id);
		else
			cpu_up(cpu_id);
	}
	else if(cpu_state==CS_PM_STATE_SUSPEND)
	{
		if(cpu_id==0)
		{
			printk("Can't suspend CPU0!\n");
			return CS_E_ERROR;
		}	
		
		if(cpu_online(cpu_id)==1)
			cpu_down(cpu_id);
		else
			printk("CPU%x already suspend!\n",cpu_state);
	}
	else
	{
		printk("Not Supported state %x !\n",cpu_state);
		cpu_hotplug_driver_unlock();
		return CS_E_ERROR;
	}

	cpu_hotplug_driver_unlock();
	return CS_E_OK;

}
EXPORT_SYMBOL(cs_pm_cpu_power_state_set);

static inline cs_peripheral_freq_t cs_pm_get_peripheral_freq(cs_cpu_freq_t cpu_freq)
{
	return cs_peripheral_freq[cpu_freq];
}

static inline cs_axi_freq_t cs_pm_get_axi_freq(cs_cpu_freq_t cpu_freq)
{
	return cs_axi_freq[cpu_freq];
}

static DEFINE_MUTEX(pm_freq_lock);
static unsigned int notifiers_count;
static CS_LIST_HEAD(notifiers);

typedef struct {
	cs_pm_freq_notifier_t notifier;
	cs_uint32_t priority;
	struct cs_list_node list;
} cs_notifiers_node_t;

cs_status_t cs_pm_freq_register_notifier(cs_pm_freq_notifier_t *n,
					 cs_uint32_t priority)
{
	cs_notifiers_node_t *node, *ptr, *next = NULL;

	node = kmalloc(sizeof(cs_notifiers_node_t), GFP_KERNEL);
	if (!node)
		return CS_E_RESOURCE;

	mutex_lock(&pm_freq_lock);
	cs_list_for_each(&notifiers, ptr, list) {
		/* Not allowed to register callback/data pair twice */
		if (ptr->notifier.notifier == n->notifier &&
		    ptr->notifier.data == n->data) {
			mutex_unlock(&pm_freq_lock);
			kfree(node);
			return CS_E_EXISTS;
		}

		/* The lowest priority is 0 */
		if (!next && ptr->priority < priority)
			next = ptr;
	}

	if (!next)
		next = ptr;

	node->notifier.notifier = n->notifier;
	node->notifier.data = n->data;
	node->priority = priority;

	node->list.next = &next->list;
	node->list.prev = next->list.prev;
	next->list.prev->next = &node->list;
	next->list.prev = &node->list;

	notifiers_count++;
	mutex_unlock(&pm_freq_lock);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pm_freq_register_notifier);

cs_status_t cs_pm_freq_unregister_notifier(cs_pm_freq_notifier_t *n)
{
	cs_notifiers_node_t *node, *ptr;
	cs_status_t val = CS_E_NOT_FOUND;

	mutex_lock(&pm_freq_lock);
	cs_list_for_each_safe(&notifiers, node, ptr, list) {
		if (node->notifier.notifier == n->notifier &&
		    node->notifier.data == n->data) {
			cs_list_del(&node->list);
			kfree(node);
			val = CS_E_OK;
			break;
		}
	}

	if (val == CS_E_OK)
		notifiers_count--;
	mutex_unlock(&pm_freq_lock);

	return val;
}
EXPORT_SYMBOL(cs_pm_freq_unregister_notifier);

static void cs_pm_freq_notify_transition(cs_pm_freq_notifier_event_t event,
					 cs_cpu_freq_t old, cs_cpu_freq_t new)
{
	cs_notifiers_node_t *node;
	cs_pm_freq_notifier_data_t d, tmp;

	tmp.event = event;
	tmp.old_peripheral_clk = cs_pm_get_peripheral_freq(old);
	tmp.new_peripheral_clk = cs_pm_get_peripheral_freq(new);
	tmp.old_axi_clk = cs_pm_get_axi_freq(old);
	tmp.new_axi_clk = cs_pm_get_axi_freq(new);

	/* The notifiers list is sorted by priority value, therefore callbacks are called */
	mutex_lock(&pm_freq_lock);
	cs_list_for_each(&notifiers, node, list) {
		/* Notifier may overwrite internal fields */
		memcpy(&d, &tmp, sizeof(cs_pm_freq_notifier_data_t));
		d.data = node->notifier.data;

		/* Notifier callback may sleep */
		node->notifier.notifier(&d);
	}
	mutex_unlock(&pm_freq_lock);
}

/* End of IROS PM functions*/
/* ----------------------------------------------------------------------------------- */

void cs75xx_irq_suspend(void)
{
	unsigned int gic_dist_base;

	if(regbus_wakeups != 0)
		gic_wakeups0 |= (u64)1 << IRQ_PERI_REGBUS;

	/* Anyway, let UART0 can wake up system */
	gic_wakeups0 |= (u64)1 << GOLDENGATE_IRQ_UART0;

	/* 1. Disable unnessary peripheral interrupt */
	regbus_backups = readl(IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));
	writel(regbus_wakeups, IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));


	/* 2. Disable unnessary interrupt in GIC */
	gic_dist_base = IO_ADDRESS(GOLDENGATE_GIC_DIST_BASE);
	gic_backups0 = readl(gic_dist_base + GIC_DIST_ENABLE_SET);
	gic_backups0 |= (u64)readl(gic_dist_base + GIC_DIST_ENABLE_SET + 4) << 32;
	gic_backups1 = readl(gic_dist_base + GIC_DIST_ENABLE_SET + 8);

	writel(0xffff0000 & gic_backups0, gic_dist_base + GIC_DIST_ENABLE_CLEAR);
	writel(0xFFFFFFFF, gic_dist_base + GIC_DIST_ENABLE_CLEAR + 4);
	writel(0xFFFF, gic_dist_base + GIC_DIST_ENABLE_CLEAR + 8);

	gic_wakeups0 |= gic_backups0 & 0xFFFF;
	writel(gic_wakeups0, gic_dist_base + GIC_DIST_ENABLE_SET);
	writel(gic_wakeups0 >> 32, gic_dist_base + GIC_DIST_ENABLE_SET + 4);
	writel(gic_wakeups1, gic_dist_base + GIC_DIST_ENABLE_SET + 8);

}

void cs75xx_irq_resume(void)
{
	unsigned int gic_dist_base;

	/* Enable GIC */
	gic_dist_base = IO_ADDRESS(GOLDENGATE_GIC_DIST_BASE);
	writel(gic_backups0, gic_dist_base + GIC_DIST_ENABLE_SET);
	writel(gic_backups0 >> 32, gic_dist_base + GIC_DIST_ENABLE_SET + 4);
	writel(gic_backups1, gic_dist_base + GIC_DIST_ENABLE_SET + 8);

	/* Enable peripheral IRQ */
	writel(regbus_backups, IO_ADDRESS(PER_PERIPHERAL_INTENABLE_0));

	/* Clear setting for next suspend */
	gic_wakeups0 = 0;
	gic_wakeups1 = 0;
	regbus_wakeups = 0;

}

static int cs75xx_pm_valid_state(suspend_state_t state)
{
	switch (state) {
		case PM_SUSPEND_ON:
		case PM_SUSPEND_STANDBY:
		case PM_SUSPEND_MEM:
			return 1;

		default:
			return 0;
	}
}


static suspend_state_t target_state;

/*
 * Called after processes are frozen, but before we shutdown devices.
 */
static int cs75xx_pm_begin(suspend_state_t state)
{
	target_state = state;
	return 0;
}

int cs75xx_pm_enter(suspend_state_t state)
{
	unsigned int	tmp, loop=10;
	unsigned int	rcpu_run_stall;
	unsigned int	wdt_control_reg;	
	register unsigned int *pc;
//	cs75xx_gpio_suspend();
	cs75xx_irq_suspend();

	switch (state) {
		/*
		 * Suspend-to-RAM is like STANDBY plus slow clock mode, so
		 * drivers must suspend more deeply:  only the master clock
		 * controller may be using the main oscillator.
		 */
		case PM_SUSPEND_MEM:
			/*
			 * Ensure that clocks are in a valid state.
			 */

			/*
			 * Enter slow clock mode by switching over to clk32k and
			 * turning off the main oscillator; reverse on wakeup.
			 */

		/*
		 * STANDBY mode has *all* drivers suspended; ignores irqs not
		 * marked as 'wakeup' event sources; and reduces DRAM power.
		 * But otherwise it's identical to PM_SUSPEND_ON:  cpu idle, and
		 * nothing fancy done with main or cpu clocks.
		 */
		case PM_SUSPEND_STANDBY:
			/*
			 * NOTE: the Wait-for-Interrupt instruction needs to be
			 * in icache so no SDRAM accesses are needed until the
			 * wakeup IRQ occurs and self-refresh is terminated.
			 */
//			asm("b 1f; .align 5; 1:");
//			asm("mcr p15, 0, r0, c7, c10, 4");	/* drain write buffer */
//			saved_lpr = sdram_selfrefresh_enable();
//			asm("mcr p15, 0, r0, c7, c0, 4");	/* wait for interrupt */
//			sdram_selfrefresh_disable(saved_lpr);
//			break;

		case PM_SUSPEND_ON:
			asm volatile("mov    %0, pc"
				:
				:"r"(pc)
				:"memory", "cc");
			/* Prefetch 10 cache line for instrution, just enough to resume DDR */
			for(loop = 0; loop < 32; loop++){
				prefetch(pc);
				pc += 8;
			}
			loop = 1000000;

			/* save watchdog state */
			wdt_control_reg = __raw_readl(IO_ADDRESS(GOLDENGATE_TWD_BASE) + 0x28);
			/* switch from watchdog mode to timer mode */
			__raw_writel(0x12345678, IO_ADDRESS(GOLDENGATE_TWD_BASE) + 0x34);
			__raw_writel(0x87654321, IO_ADDRESS(GOLDENGATE_TWD_BASE) + 0x34);
			/* watchdog is disabled */
			__raw_writel(0x0, IO_ADDRESS(GOLDENGATE_TWD_BASE) + 0x28);

			/* Set RCPU to stall */
			rcpu_run_stall = __raw_readl(IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));
			tmp = rcpu_run_stall | 0x00000108;
			__raw_writel(tmp, IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));

			/* Disable core clock & RCPU clock */
			tmp = __raw_readl(IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));
			tmp = tmp | 0xc0000000;
			__raw_writel(tmp, IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));

			/* To force DDR into self-reflash mode */
			tmp = __raw_readl(IO_ADDRESS(SDRAM_DENALI_CTL_081));
			tmp = tmp | 0x00010000;
			__raw_writel(tmp, IO_ADDRESS(SDRAM_DENALI_CTL_081));

			/* Turn off all DDR IO execpt RESET# and CKE */
			tmp = __raw_readl(IO_ADDRESS(SDRAM_PHY_CTL_67_0));
			tmp = tmp | 0x00000010;
			__raw_writel(tmp, IO_ADDRESS(SDRAM_PHY_CTL_67_0));
			tmp = __raw_readl(IO_ADDRESS(SDRAM_PHY_CTL_67_1));
			tmp = tmp | 0x00001f00;
			__raw_writel(tmp, IO_ADDRESS(SDRAM_PHY_CTL_67_1));

			/* Hold DDR PLL in reset */
			tmp = __raw_readl(IO_ADDRESS(GLOBAL_DDR_PLL));
			tmp = tmp | 0x00000001;
			__raw_writel(tmp, IO_ADDRESS(GLOBAL_DDR_PLL));

			asm("wfi");	/* wait for interrupt */

			/* Enable DDR PLL */
			tmp = __raw_readl(IO_ADDRESS(GLOBAL_DDR_PLL));
			tmp = tmp & ~0x00000001;
			__raw_writel(tmp, IO_ADDRESS(GLOBAL_DDR_PLL));

			/* Wait a moment */
			while(loop--)
				asm("nop");

			/* Turn on all DDR IO */
			tmp = __raw_readl(IO_ADDRESS(SDRAM_PHY_CTL_67_0));
			tmp = tmp & ~0x00000010;
			__raw_writel(tmp, IO_ADDRESS(SDRAM_PHY_CTL_67_0));
			tmp = __raw_readl(IO_ADDRESS(SDRAM_PHY_CTL_67_1));
			tmp = tmp & ~0x00001f00;
			__raw_writel(tmp, IO_ADDRESS(SDRAM_PHY_CTL_67_1));

			/* Resume DDR from self-reflash mode */
			tmp = __raw_readl(IO_ADDRESS(SDRAM_DENALI_CTL_081));
			tmp = tmp & ~0x00010000;
			__raw_writel(tmp, IO_ADDRESS(SDRAM_DENALI_CTL_081));

			/* Enable core clock & RCPU clock */
			tmp = __raw_readl(IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));
			tmp = tmp & ~0xc0000000;
			__raw_writel(tmp, IO_ADDRESS(GLOBAL_GLOBAL_CONFIG));

			/* Restore RCPU to run or stall state*/
			__raw_writel(rcpu_run_stall, IO_ADDRESS(GLOBAL_RECIRC_CPU_CTL));

			/* restore watchdog state */
			__raw_writel(wdt_control_reg, IO_ADDRESS(GOLDENGATE_TWD_BASE) + 0x28);

			break;

		default:
			pr_debug("CS75xx: PM - bogus suspend state %d\n", state);
			goto error;
	}

//	pr_debug("AT91: PM - wakeup %08x\n",
//			cs75xx_sys_read(AT91_AIC_IPR) & cs75xx_sys_read(AT91_AIC_IMR));

error:
	target_state = PM_SUSPEND_ON;
	cs75xx_irq_resume();
	return 0;
}

/*
 * Called right prior to thawing processes.
 */
static void cs75xx_pm_end(void)
{
	target_state = PM_SUSPEND_ON;
}


static struct platform_suspend_ops cs75xx_pm_ops ={
	.valid	= cs75xx_pm_valid_state,
	.begin	= cs75xx_pm_begin,
	.enter	= cs75xx_pm_enter,
	.end	= cs75xx_pm_end,
};

static int __init cs75xx_pm_init(void)
{
	gic_wakeups0 = 0;
	gic_backups0 = 0;
	gic_wakeups1 = 0;
	gic_backups1 = 0;

	regbus_wakeups = 0;
	regbus_backups = 0;

	suspend_set_ops(&cs75xx_pm_ops);

	return 0;
}
arch_initcall(cs75xx_pm_init);
