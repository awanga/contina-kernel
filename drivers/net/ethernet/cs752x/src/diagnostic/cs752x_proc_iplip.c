/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
//Bug#40328
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
#include <linux/export.h>
#endif

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/registers.h>
#include <mach/cs75xx_mibs.h>
#include <mach/cs75xx_mibs_iplip.h>



/* file name */
#define CS_PE1_IPLIP_MIBS	"cs_pe1_iplip_mibs"
#define CS_PE1_IPLIP_UTIL	"cs_pe1_iplip_util"
#define CS_PE1_IPLIP_CURRCCOUNT	"cs_pe1_iplip_currcyclecount"

static const char* cs_iplip_mibs_type_text[CS_IPLIP_MIBS_MAX] = {
	"CS_IPLIP_MIBS_ALL",
	"CS_IPLIP_MIBS_NI",
	"CS_IPLIP_MIBS_IPC",
};


/* help message */
#define CS_PE1_IPLIP_MIBS_HELP_MSG \
			"Purpose: GET/CLEAR PE1 IPLIP MIBS\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"    value 0: Clear all PE1 IPLIP MIBS\n" \
			"    value 1: Clear PE1 IPLIP NI MIBS\n" \
			"    value 2: Clear PE1 IPLIP IPC MIBS\n"

/* entry pointer */
extern struct proc_dir_entry *proc_driver_cs752x_ne_iplip;
extern int cs752x_add_proc_handler(char *name,
			const struct file_operations *ops,
			struct proc_dir_entry *parent);


void cs_iplip_dump_mibs(cs_iplip_mib_pe_t *pmib)
{
    printk("=============================================\n");
    printk("IPLIP MIBs:: NIRxGEFrameCount      0x%8.8x\n", pmib->iplip_ni_mib.NIRxGEFrameCount);
    printk("IPLIP MIBs:: NIRxA9FrameCount      0x%8.8x\n", pmib->iplip_ni_mib.NIRxA9FrameCount);
    printk("IPLIP MIBs:: NITxGEFrameCount      0x%8.8x\n", pmib->iplip_ni_mib.NITxGEFrameCount);
    printk("IPLIP MIBs:: NITxA9FrameCount      0x%8.8x\n", pmib->iplip_ni_mib.NITxA9FrameCount);
    printk("IPLIP MIBs:: NIRxGEByteCount       0x%8.8x\n", pmib->iplip_ni_mib.NIRxGEByteCount);
    printk("IPLIP MIBs:: NIRxA9ByteCount       0x%8.8x\n", pmib->iplip_ni_mib.NIRxA9ByteCount);
    printk("IPLIP MIBs:: NITxGEByteCount       0x%8.8x\n", pmib->iplip_ni_mib.NITxGEByteCount);
    printk("IPLIP MIBs:: NITxA9ByteCount       0x%8.8x\n", pmib->iplip_ni_mib.NITxA9ByteCount);
    printk("IPLIP MIBs:: NIRxGEDropFrameCount  0x%8.8x\n", pmib->iplip_ni_mib.NIRXGEDropFrameCount);
    printk("IPLIP MIBs:: NIRxA9DropFrameCount  0x%8.8x\n\n", pmib->iplip_ni_mib.NIRXA9DropFrameCount);

    printk("IPLIP MIBs:: IPCRcvCnt             0x%8.8x\n", pmib->iplip_ipc_mib.IpcRcvCnt);
    printk("IPLIP MIBs:: IPCRspCnt             0x%8.8x\n", pmib->iplip_ipc_mib.IpcRspCnt);
    printk("IPLIP MIBs:: FifoStatus            0x%8.8x\n", readl(RECIRC_TOP_RECPU_RX_ENCAP_DST_FF_STS));
    printk("IPLIP MIBs:: NIXferCnt             0x%8.8x\n", readl(RECIRC_TOP_RECPU_ENCAP_TX_PACKET_COUNT));
    printk("IPLIP MIBs:: BusyAccum             0x%8.8x\n", pmib->iplip_ipc_mib.BusyAccum);
    printk("IPLIP MIBs:: CurCcount             0x%8.8x\n", pmib->iplip_ipc_mib.CurCcount);
    printk("IPLIP MIBs:: CPU usage             %d%%\n", pmib->iplip_ipc_mib.BusyAccum /
	    ((pmib->iplip_ipc_mib.CurCcount/100)?:1));

    return;
} /* cs_iplip_dump_mibs() */

/*
 * IPLIP APIs
 */
void cs_iplip_get_mibs(cs_iplip_mib_pe_t *pmib)
{
    cs_iplip_mib_pe_t *pmib_rram1 = NULL;

    pmib_rram1 = (cs_iplip_mib_pe_t *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE1);    //Bug#40475
    
    if (!pmib_rram1) {
        printk("(%s, %d)ERROR: MIBs share memory, check PE!!\n", __func__, __LINE__);
    } else {
        cs_iplip_dump_mibs(pmib_rram1);
        if (pmib) {
            memcpy (pmib, pmib_rram1, sizeof(cs_iplip_mib_pe_t));
        }
    }
    
    return;
} /* cs_iplip_get_mibs() */
EXPORT_SYMBOL(cs_iplip_get_mibs);

void cs_iplip_dump_util(void)
{
    cs_iplip_mib_pe_t *pmib = (cs_iplip_mib_pe_t *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE1); 

    printk("IPLIP MIBs:: CPU usage             %d%%\n", pmib->iplip_ipc_mib.BusyAccum / ((pmib->iplip_ipc_mib.CurCcount/100)?:1));
}

void cs_iplip_dump_currccount(void)
{
    cs_iplip_mib_pe_t *pmib = (cs_iplip_mib_pe_t *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE1);

    printk("IPLIP MIBs:: CurCcount             0x%8.8x\n", pmib->iplip_ipc_mib.CurCcount);
}


void cs_iplip_clear_mibs(cs_iplip_mib_pe_t *pmib, cs_iplip_mibs_e type)
{
    pmib = (cs_iplip_mib_pe_t *)cs_get_pe_mibs_phy_addr(CS_MIBs_ID_PE1);    //Bug#40475

//++BUG#40475
    if (!pmib) {
        printk("(%s, %d)ERROR: MIBs share memory, check PE!!\n", __func__, __LINE__);
        return;
    }
//--BUG#40475

    
    switch(type) {
        case CS_IPLIP_MIBS_ALL:
    	    memset((void *)&pmib->iplip_ni_mib, 0, sizeof(cs_iplip_mib_pe_t));
            break;
        case CS_IPLIP_MIBS_NI:
    	    memset((void *)&pmib->iplip_ni_mib, 0, sizeof(cs_iplip_mib_ni_t));
            break;
        case CS_IPLIP_MIBS_IPC:
    	    memset((void *)&pmib->iplip_ipc_mib, 0, sizeof(cs_iplip_mib_ipc_t));
            break;
        default:
            break;
    }
    
    return;
} /* cs_iplip_clear_mibs() */
EXPORT_SYMBOL(cs_iplip_clear_mibs);


/*
 * IPLIP PROC
 */
 
/*
 * IPLIP PE Utilization
 */
static int cs_proc_iplip_util_read_proc(struct seq_file *m, void *v)
{
	cs_iplip_mib_pe_t mib_counter;

	cs_iplip_dump_util();
	return 0;
} /* cs_proc_iplip_util_read_proc() */

/*
 * IPLIP PE Current Cycle Count
 */
static int cs_proc_iplip_currccount_read_proc(struct seq_file *m, void *v)
{
	cs_iplip_mib_pe_t mib_counter;

	cs_iplip_dump_currccount();
	return 0;
} /* cs_proc_iplip_currccount_read_proc() */

/*
 * IPLIP MIBs
 */
static int cs_proc_iplip_mibs_read_proc(struct seq_file *m, void *v)
{
	cs_iplip_mib_pe_t mib_counter;

	printk(KERN_WARNING CS_PE1_IPLIP_MIBS_HELP_MSG, CS_PE1_IPLIP_MIBS, CS_PE1_IPLIP_MIBS);
	cs_iplip_get_mibs(&mib_counter);
	return 0;
} /* cs_proc_iplip_mibs_read_proc() */

static int cs_proc_iplip_mibs_write_proc(struct file *file,
		const char __user *buffer, size_t count, loff_t *off)
{
	char buf[32];
	unsigned long type;
	ssize_t len;
	cs_iplip_mib_pe_t mib_counter;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto IPLIP_MIBS_INVAL_EXIT;

	buf[len] = '\0';
	if (kstrtoul(buf, 0, &type))
		goto IPLIP_MIBS_INVAL_EXIT;

	if (type >= CS_IPLIP_MIBS_MAX)
		goto IPLIP_MIBS_INVAL_EXIT;

	printk(KERN_WARNING "\n%s: Clear %s\n", CS_PE1_IPLIP_MIBS,
			cs_iplip_mibs_type_text[type]);
	cs_iplip_clear_mibs(&mib_counter, (cs_iplip_mibs_e)type);

	return count;

IPLIP_MIBS_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_PE1_IPLIP_MIBS_HELP_MSG,
			CS_PE1_IPLIP_MIBS, CS_PE1_IPLIP_MIBS);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
} /* cs_proc_iplip_mibs_write_proc() */


#define _CS752X_DEFINE_PROC_OPS(x, rd, wr) \
static int x##_proc_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, rd, NULL); \
} \
static const struct file_operations x##_proc_fops = { \
	.open		= x##_proc_open, \
	.read		= seq_read, \
	.llseek		= seq_lseek, \
	.release	= single_release, \
	.write		= wr, \
};

#define CS752X_DEFINE_RO_PROC_OPS(x) \
	_CS752X_DEFINE_PROC_OPS(x, x##_read_proc, NULL)

#define CS752X_DEFINE_RW_PROC_OPS(x) \
	_CS752X_DEFINE_PROC_OPS(x, x##_read_proc, x##_write_proc)

CS752X_DEFINE_RW_PROC_OPS(cs_proc_iplip_mibs)
CS752X_DEFINE_RO_PROC_OPS(cs_proc_iplip_util)
CS752X_DEFINE_RO_PROC_OPS(cs_proc_iplip_currccount)

void cs_iplip_proc_init_module(void)
{
	cs752x_add_proc_handler(CS_PE1_IPLIP_MIBS,
				&cs_proc_iplip_mibs_proc_fops,
				proc_driver_cs752x_ne_iplip);

	cs752x_add_proc_handler(CS_PE1_IPLIP_UTIL,
				&cs_proc_iplip_util_proc_fops,
				proc_driver_cs752x_ne_iplip);

	cs752x_add_proc_handler(CS_PE1_IPLIP_CURRCCOUNT,
				&cs_proc_iplip_currccount_proc_fops,
				proc_driver_cs752x_ne_iplip);

	return;
}/* cs_iplip_proc_init_module() */

void cs_iplip_proc_exit_module(void)
{
	/* no problem if it =was not registered */
	/* remove file entry */
	remove_proc_entry(CS_PE1_IPLIP_MIBS, proc_driver_cs752x_ne_iplip);

	return;
}/* cs_iplip_proc_exit_module () */
