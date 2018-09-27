/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs75xx_reg.c
 *
 * $Id: 2060-g2-regrw.patch,v 1.1.2.1 2013/02/21 05:31:25 jflee Exp $
 *
 * Support to read/write register in command line for
 * Cortex-A9 CPU on Cortina-Systems Baseboard.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/sockios.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#include "cs75xx_reg.h"


#define RW_MINOR    242     // Documents/devices.txt suggest to use 240~255 for local driver!!

#define PFX     "REG_RW"
#define DBG(format, args...) printk(KERN_ERR PFX \
                                ":%s:%d: " format, __func__, __LINE__ , ## args)

/*----------------------------------------------------------------------
* cs_dm_long_1
*   gmac read mem -b 0xc1ff4740 -l 8 -4
*
*   0xc1ff4740: E5D24001 E5D21004 - E59D302C E1A05000
*   0xc1ff4750: E5D20005 E0643003 - E1A05435 E1814400
*----------------------------------------------------------------------*/
void cs_dm_long_1(u32 location, int length)
{
    u32     *start_p, *curr_p, *end_p;
    u32     *datap, data;
    int     i;

    /* start address should be a multiple of 4 */
    start_p = datap = (u32 *)(location & 0xfffffffc);
    end_p = (u32 *)location + length;
    curr_p = (u32 *)((u32)location & 0xfffffff0);
    // datap = (u32 *)location;
    while (curr_p < end_p)
    {
        printk("0x%08x: ",(u32)curr_p & 0xfffffff0);
        for (i=0; i<4; i++)
        {
            if (curr_p < start_p || curr_p >= end_p)
               printk("         ");  // print space x 9 for padding
            else
            {
                data = *datap;
                printk("%08X ", data);

                datap++;
            }
            if (i==1)
              printk("- ");

            curr_p++;
        }
        printk("\n");
    }
}
/*----------------------------------------------------------------------
* cs_dm_byte
*   gmac read mem -b 0xc1ff4740 -l 64 -1
*
*   0xc1ff4740: 01 40 D2 E5 04 10 D2 E5 - 2C 30 9D E5 00 50 A0 E1 .@......,0...P..
*   0xc1ff4750: 05 00 D2 E5 03 30 64 E0 - 35 54 A0 E1 00 44 81 E1 .....0d.5T...D..
*   0xc1ff4760: 0F 00 54 E3 04 B0 A0 91 - 2C 30 8D E5 01 60 86 92 ..T.....,0...`..
*   0xc1ff4770: 70 B9 08 95 04 80 88 92 - 20 20 8D E5 56 00 00 9A p.......  ..V...
*
*----------------------------------------------------------------------*/
void cs_dm_byte(u32 location, int length)
{
    u8      *start_p, *end_p, *curr_p;
    u8      *datap, data;
    int     i;

    start_p = datap = (u8 *)location;
    end_p = (u8 *)start_p + length;
    curr_p=(u8 *)((u32)location & 0xfffffff0);
    //datap = (u8 *)location;

    while (curr_p < end_p)
    {
        u8 *p1, *p2;
        printk("0x%08x: ",(u32)curr_p & 0xfffffff0);
        p1 = curr_p;
        p2 = datap;
        // dump data
        for (i=0; i<16; i++)
        {
            if (curr_p < start_p || curr_p >= end_p)
            {
                 printk("   "); // print space x 3 for padding
            }
            else
            {
                data = *datap;
                printk("%02X ", data);

                datap++;
            }
            if (i==7)
                printk("- ");

            curr_p++;
        }


        // dump ascii
        curr_p = p1;
        datap = p2;
        for (i=0; i<16; i++)
        {
            if (curr_p < start_p || curr_p >= end_p)
                printk(".");
            else
            {
                data = *datap ;
                if (data<0x20 || data>0x7f || data==0x25)
                    printk(".");
                else
                    printk("%c", data);;

                datap++;
            }

            curr_p++;
        }
        printk("\n");
    }
}

/*----------------------------------------------------------------------
* dm_short
*    gmac read mem -b 0xc1ff4740 -l 24 -2
*
*   0xc1ff4740: 4001 E5D2 1004 E5D2 - 302C E59D 5000 E1A0
*   0xc1ff4750: 0005 E5D2 3003 E064 - 5435 E1A0 4400 E181
*   0xc1ff4760: 000F E354 B004 91A0 - 302C E58D 6001 9286
*
*----------------------------------------------------------------------*/
void cs_dm_short(u32 location, int length)
{
    u16     *start_p, *curr_p, *end_p;
    u16     *datap, data;
    int     i;

    /* start address should be a multiple of 2 */
    start_p = datap = (u16 *)(location & 0xfffffffe);
    end_p =  (u16 *)location + length;
    curr_p = (u16 *)((u32)location & 0xfffffff0);
    //datap = (u16 *)location;

    while (curr_p < end_p)
    {
        printk("0x%08x: ",(u32)curr_p & 0xfffffff0);
        for (i=0; i<8; i++)
        {
            if (curr_p < start_p || curr_p >= end_p)
                printk("     ");  // print space x 5 for padding
            else
            {
                data = *datap;
                printk("%04X ", data);

                datap++;
            }
            if (i==3)
              printk("- ");

            curr_p++;
        }
        printk("\n");
    }
}

int cs_reg_ioctl(struct file *file, unsigned long arg, unsigned int cmd)
{
    int err = 0, j;
    void __user *argp = (void __user *)arg;
    CS_REGCMD_HDR_T cs_reg_hdr;
    CS_REG_REQ_E ctrl;
    unsigned char *req_datap;
    unsigned int location_r = -1,length_r = -1,location_w = -1,data_w = -1;
    unsigned int size_r,size_w;
    unsigned short table_address, ni_port_cal, pspid;

#ifdef MII_DEBUG
	int  i;
    u32 phy_data_s = -1;
    u16 phy_addr = -1,phy_reg = -1,phy_len = -1, phy_addr_s = 0, phy_reg_s = 0;
#endif
    /* We don't care the value of cmd. */
#if 0
    if(cmd != SIOCDEVCS75XX) {
        DBG("It is not private command (0x%X). cmd = (0x%X)\n", SIOCDEVCS75XX, cmd);
        //return -EOPNOTSUPP;
    }
#endif
    if (copy_from_user((void *)&cs_reg_hdr, argp, sizeof(cs_reg_hdr))) {
        DBG("Copy from user space fail\n");
        return -EFAULT;
    }
    req_datap = (unsigned char *)argp + sizeof(cs_reg_hdr);

    switch (cs_reg_hdr.cmd) {

    case REGREAD:
        if (cs_reg_hdr.len != sizeof(CS_REGREAD)) {
            DBG("Incorrect size. cs_reg_hdr.len (%d) is not equal to CS_REGREAD (%d)\n",
                cs_reg_hdr.len,
                sizeof(CS_REGREAD) );
            return -EPERM;
        }

        if (copy_from_user((void *)&ctrl.reg_read, req_datap, sizeof(ctrl.reg_read))) {
            DBG("Copy from user space fail\n");
            return -EFAULT;
        }
        location_r = ctrl.reg_read.location;
        length_r = ctrl.reg_read.length;
        size_r = ctrl.reg_read.size;

        if (size_r == 1)
            cs_dm_byte(location_r, length_r);
        if (size_r == 2)
            cs_dm_short(location_r, length_r);
        if (size_r == 4)
            cs_dm_long_1(location_r, length_r);
        break;
    case REGWRITE:
        if (cs_reg_hdr.len != sizeof(CS_REGWRITE)) {
            DBG("Incorrect size. cs_reg_hdr.len (%d) is not equal to CS_REGREAD (%d)\n",
                cs_reg_hdr.len,
                sizeof(CS_REGREAD) );
            return -EPERM;
        }
        if (copy_from_user((void *)&ctrl.reg_write, req_datap, sizeof(ctrl.reg_write))) {
            DBG("Copy from user space fail\n");
            return -EFAULT;
        }
        location_w = ctrl.reg_write.location;
        data_w = ctrl.reg_write.data;
        size_w = ctrl.reg_write.size;
        if (size_w == 1)
        {
            if (data_w > 0xff)
                err = 1;
            else
            {
                writeb(data_w,location_w);
                printk("Write Data 0x%X to Location 0x%X\n",(u32)data_w, location_w);
            }
        }
        if (size_w == 2)
        {
            if (data_w > 0xffff)
                err = 1;
            else
            {
                writew(data_w,location_w);
                printk("Write Data 0x%X to Location 0x%X\n",(u32)data_w, location_w);
            }
        }
        if (size_w == 4)
        {
            if (data_w > 0xffffffff)
                err = 1;
            else
            {
                writel(data_w,location_w);
                printk("Write Data 0x%X to Location 0x%X\n",(u32)data_w, location_w);
            }
        }
        if (err == 1)
        {
            printk("Syntax: reg_rw write mem [-b <location>] [-d <data>] [-1|2|4]\n");
            printk("Options:\n");
            printk("\t-b  Register Address\n");
            printk("\t-d  Data Vaule\n");
            if (size_w == 1)
                printk("\t-1  Data 0x%X < 0xFF\n",data_w);
            if (size_w == 2)
                printk("\t-2  Data 0x%X < 0xFFFF\n",data_w);
            if (size_w == 4)
                printk("\t-4  Data 0x%X < 0xFFFFFFFF\n",data_w);
        }
        break;
#ifdef MII_DEBUG
	case GMIIREG:
		if (cs_reg_hdr.len != sizeof(GMIIREG_T_1))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.get_mii_reg, req_datap, sizeof(ctrl.get_mii_reg)))
			return -EFAULT; /* Invalid argument */
		phy_addr = ctrl.get_mii_reg.phy_addr;
		phy_reg = ctrl.get_mii_reg.phy_reg;
		phy_len = ctrl.get_mii_reg.phy_len;
		if (phy_addr < 32) {
			for (i = 0; i < phy_len ; i++)	{
				unsigned int data;
				data = ni_mdio_read((int)phy_addr, (int)phy_reg);
				printk("MII Phy %d Reg %d Data = 0x%x\n", phy_addr, phy_reg++, data);
			}
		} else {
			err = 1;
		}

		if (err == 1) {
			printk("Syntax error!\n");
			printk("Syntax: MII read [-a phy addr] [-r phy reg] [-l length]\n");
			printk("Options:\n");
			printk("\t-a  Phy address\n");
			printk("\t-r  Phy registers\n");
			printk("\t-l  Display total registers\n");
			printk("MII Phy address -a %d error !! Phy address must be smaller than 32.\n", phy_addr);
		}
		break;
	case SMIIREG:
		if (cs_reg_hdr.len != sizeof(SMIIREG_T_1))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.set_mii_reg, req_datap, sizeof(ctrl.set_mii_reg)))
			return -EFAULT;

		phy_addr_s = ctrl.set_mii_reg.phy_addr;
		phy_reg_s = ctrl.set_mii_reg.phy_reg;
		phy_data_s = ctrl.set_mii_reg.phy_data;
		if (phy_addr_s < 32) {
				ni_mdio_write((int)phy_addr_s, (int)phy_reg_s, (int)phy_data_s);
				printk("Write MII Phy %d Reg %d Data = 0x%x\n", phy_addr_s, phy_reg_s, phy_data_s);
		} else {
			err = 1;
		}
		if (err == 1) {
			printk("Syntax error!\n");
			printk("Syntax: MII write [-a phy addr] [-r phy reg] [-d data]\n");
			printk("Options:\n");
			printk("\t-a  Phy address\n");
			printk("\t-r  Phy registers\n");
			printk("\t-d  date\n");
			printk("MII Phy address -a %d error !! Phy address must be smaller than 32.\n", phy_addr_s);
		}
		break;
#endif
	case NIGETPORTCAL:
		if (cs_reg_hdr.len != sizeof(NIGETPORTCAL_T))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.get_ni_cal, req_datap,
					sizeof(ctrl.get_ni_cal)))
			return -EFAULT;
		ni_port_cal = ctrl.get_ni_cal.get_port_cal;
		if (ni_port_cal == 1) {
			printk("**** Ni Rx Port Cal Table Map ****\n");
			printk("0 - GE0\n");
			printk("1 - GE1\n");
			printk("2 - GE2\n");
			printk("3 - CPU\n");
			printk("4 - Crypto\n");
			printk("5 - Encap\n");
			printk("6 - Mcast\n");
			printk("7 - Mirror\n");
			printk("*************************\n");
			for (j = 0; j < 96; j++)
				ni_get_port_calendar(j);
		} else {
			err = 1;
		}
		if (err == 1) {
			printk("Syntax error!\n");
			printk("Syntax: ni get [-e flag]\n");
			printk("Options:\n");
			printk("\t-e  1: get port calendar\n");
		}
		break;
	case NISETPORTCAL:
		if (cs_reg_hdr.len != sizeof(NISETPORTCAL_T))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.set_ni_cal, req_datap,
					sizeof(ctrl.set_ni_cal)))
			return -EFAULT;

		table_address = ctrl.set_ni_cal.table_address;
		pspid = ctrl.set_ni_cal.pspid_ts;
		if (0 <= table_address && table_address < 96) {
			cs_ni_set_port_calendar(table_address, pspid);
			printk("RX Port Cal Table         %d :    %d\n",
				table_address, pspid);
		} else {
			err = 1;
		}
		if (err == 1) {
			printk("Syntax error!\n");
			printk("Syntax: ni set [-t table] [-p pspid]\n");
			printk("Options:\n");
			printk("\t-t  table address 0 ~ 95\n");
			printk("\t	0 - GE0\n");
			printk("\t	1 - GE1\n");
			printk("\t	2 - GE2\n");
			printk("\t	3 - CPU\n");
			printk("\t	4 - Crypto\n");
			printk("\t	5 - Encap\n");
			printk("\t	6 - Mcast\n");
			printk("\t	7 - Mirror\n");
		}
		break;
    default:
        DBG("Unkown command (%d)\n", cs_reg_hdr.cmd);
        return -EPERM;
    }

    return 0;
}


static long cs_rw_ioctl(struct file *file,
    unsigned int cmd, unsigned long arg)
{
    return cs_reg_ioctl(file, arg, cmd);
}


static int cs_rw_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int cs_rw_release(struct inode *inode, struct file *file)
{
    return 0;
}

static struct file_operations cs_rw_fops =
{
    .owner          = THIS_MODULE,
    .unlocked_ioctl = cs_rw_ioctl,
    .open           = cs_rw_open,
    .release        = cs_rw_release,
};

static struct miscdevice cs_rw_miscdev =
{
    .minor  = RW_MINOR,
    .name   = "reg_rw",
    .fops   = &cs_rw_fops,
};

static int __init cs_rw_init_module(void)
{
   int rc;

    rc = misc_register(&cs_rw_miscdev);
    if(rc != 0) {
        printk("%s:: cs75xx rw miscdevice register error :%x \n", __func__, rc);
    }
    return 0;
}


static void __exit cs_rw_cleanup_module(void)
{
    misc_deregister(&cs_rw_miscdev);
}

MODULE_LICENSE("GPL");

EXPORT_SYMBOL(cs_dm_long_1);
EXPORT_SYMBOL(cs_dm_byte);
EXPORT_SYMBOL(cs_dm_short);

module_init(cs_rw_init_module);
module_exit(cs_rw_cleanup_module);
