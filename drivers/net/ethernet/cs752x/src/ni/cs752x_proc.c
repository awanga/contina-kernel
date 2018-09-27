/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                CH Hsu <ch.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/completion.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ppp_defs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>

#include <asm/system.h>                     
#include <asm/io.h>                         
#include <asm/irq.h>  

#include "cs752x_eth.h"  

#ifdef CONFIG_NETFILTER
#include <net/netfilter/nf_conntrack.h>
#endif

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif


#ifdef CONFIG_PROC_FS
#define	proc_printf				printk
#define CS752x_NI_PROC_NAME		"cs752x_ni"
#define CS752x_FE_PROC_NAME		"cs752x_fe"
#define CS752x_TM_PROC_NAME		"cs752x_tm"
#define CS752x_QM_PROC_NAME		"cs752x_qm"
#define CS752x_SCH_PROC_NAME	"cs752x_sch"
#define CS752x_DMA_PROC_NAME	"cs752x_dma"
#define CS752x_MDIO_PROC_NAME	"cs752x_mdio"

static int fe_ct_open(struct inode *inode, struct file *file);
static void *fe_ct_seq_start(struct seq_file *s, loff_t *pos);
static void fe_ct_seq_stop(struct seq_file *s, void *v);
static void *fe_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int fe_ct_seq_show(struct seq_file *s, void *v);

static int tm_ct_open(struct inode *inode, struct file *file);
static void *tm_ct_seq_start(struct seq_file *s, loff_t *pos);
static void tm_ct_seq_stop(struct seq_file *s, void *v);
static void *tm_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int tm_ct_seq_show(struct seq_file *s, void *v);

static int ni_ct_open(struct inode *inode, struct file *file);
static void *ni_ct_seq_start(struct seq_file *s, loff_t *pos);
static void ni_ct_seq_stop(struct seq_file *s, void *v);
static void *ni_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int ni_ct_seq_show(struct seq_file *s, void *v);

static int qm_ct_open(struct inode *inode, struct file *file);
static void *qm_ct_seq_start(struct seq_file *s, loff_t *pos);
static void qm_ct_seq_stop(struct seq_file *s, void *v);
static void *qm_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int qm_ct_seq_show(struct seq_file *s, void *v);

static int dma_ct_open(struct inode *inode, struct file *file);
static void *dma_ct_seq_start(struct seq_file *s, loff_t *pos);
static void dma_ct_seq_stop(struct seq_file *s, void *v);
static void *dma_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int dma_ct_seq_show(struct seq_file *s, void *v);

static int mdio_ct_open(struct inode *inode, struct file *file);
static void *mdio_ct_seq_start(struct seq_file *s, loff_t *pos);
static void mdio_ct_seq_stop(struct seq_file *s, void *v);
static void *mdio_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int mdio_ct_seq_show(struct seq_file *s, void *v);

static int sch_ct_open(struct inode *inode, struct file *file);
static void *sch_ct_seq_start(struct seq_file *s, loff_t *pos);
static void sch_ct_seq_stop(struct seq_file *s, void *v);
static void *sch_ct_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int sch_ct_seq_show(struct seq_file *s, void *v);

#ifdef CONFIG_SYSCTL
// static struct ctl_table_header *ne_ct_sysctl_header;
#endif

unsigned int c_qid = 0, gmac_qid = 0, ig_qid =0, igmp_qid = 0;

static struct seq_operations fe_ct_seq_ops = {
	.start = fe_ct_seq_start,
	.next  = fe_ct_seq_next,
	.stop  = fe_ct_seq_stop,
	.show  = fe_ct_seq_show
};

static struct file_operations fe_file_ops= {
	.owner   = THIS_MODULE,
	.open    = fe_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct seq_operations tm_ct_seq_ops = {
	.start = tm_ct_seq_start,
	.next  = tm_ct_seq_next,
	.stop  = tm_ct_seq_stop,
	.show  = tm_ct_seq_show
};	

static struct file_operations tm_file_ops= {
	.owner   = THIS_MODULE,
	.open    = tm_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct seq_operations ni_ct_seq_ops = {
	.start = ni_ct_seq_start,
	.next  = ni_ct_seq_next,
	.stop  = ni_ct_seq_stop,
	.show  = ni_ct_seq_show
};

static struct file_operations ni_file_ops= {
	.owner   = THIS_MODULE,
	.open    = ni_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct seq_operations qm_ct_seq_ops = {
	.start = qm_ct_seq_start,
	.next  = qm_ct_seq_next,
	.stop  = qm_ct_seq_stop,
	.show  = qm_ct_seq_show
};

static struct file_operations qm_file_ops= {
	.owner   = THIS_MODULE,
	.open    = qm_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct seq_operations sch_ct_seq_ops = {
	.start = sch_ct_seq_start,
	.next  = sch_ct_seq_next,
	.stop  = sch_ct_seq_stop,
	.show  = sch_ct_seq_show
};

static struct file_operations sch_file_ops= {
	.owner   = THIS_MODULE,
	.open    = sch_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct seq_operations mdio_ct_seq_ops = {
	.start = mdio_ct_seq_start,
	.next  = mdio_ct_seq_next,
	.stop  = mdio_ct_seq_stop,
	.show  = mdio_ct_seq_show
};

static struct file_operations mdio_file_ops= {
	.owner   = THIS_MODULE,
	.open    = mdio_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct seq_operations dma_ct_seq_ops = {
	.start = dma_ct_seq_start,
	.next  = dma_ct_seq_next,
	.stop  = dma_ct_seq_stop,
	.show  = dma_ct_seq_show
};

static struct file_operations dma_file_ops= {
	.owner   = THIS_MODULE,
	.open    = dma_ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static void seq_dm_long(struct seq_file *s, u32 location, int length)
{
	u32		*start_p, *curr_p, *end_p;
	u32		*datap, data;
	int		i;

	//if (length > 1024)
	//	length = 1024;
		
	start_p = (u32 *)location;
	end_p = (u32 *)location + length;
	curr_p = (u32 *)((u32)location & 0xfffffff0);
	datap = (u32 *)location;
	while (curr_p < end_p)
	{
		cond_resched();
		seq_printf(s, "0x%08x: ",(u32)curr_p & 0xfffffff0);
		for (i=0; i<4; i++)
		{
			if (curr_p < start_p || curr_p >= end_p)
               seq_printf(s, "         ");
			else
			{
				data = *datap;
				seq_printf(s, "%08X ", data);
			}
			if (i==1)
              seq_printf(s, "- ");
			
			curr_p++;
			datap++;
		}
        seq_printf(s, "\n");
	} 
}

static int fe_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fe_ct_seq_ops);
}

static void *fe_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}

static void fe_ct_seq_stop(struct seq_file *s, void *v)
{
}

static void *fe_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}

static int fe_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nFE Register\n");
			seq_dm_long(s, FE_TOP_BASE, 158);
			break;
		case 2:
			seq_printf(s, "\nLPM Register\n");
			seq_dm_long(s, LPM_TOP_BASE, 97);
			break;
		case 3:
			seq_printf(s, "\nHASH Register\n");
			seq_dm_long(s, HASH_TOP_BASE, 44);
			break;	
		default:
			return -ENOSPC;
	}	
	return 0;	
}


static int tm_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &tm_ct_seq_ops);
}

static void *tm_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}

static void tm_ct_seq_stop(struct seq_file *s, void *v)
{
}

static void *tm_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}

static int tm_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nTM-BM Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x100, 53);
			break;
		case 2:
			seq_printf(s, "\nTM-POL Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x200, 62);
			break;
		case 3:
			seq_printf(s, "\nTM-TC Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x300, 4);
			break;	
		default:
			return -ENOSPC;
	}
	return 0;	
}

static int qm_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &qm_ct_seq_ops);
}


static void *qm_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}
  

static void qm_ct_seq_stop(struct seq_file *s, void *v)
{
}


static void *qm_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}


static int qm_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nQM Register\n");
			seq_dm_long(s, QM_TOP_BASE, 57);
			break;
		default:
			return -ENOSPC;
	}		
	return 0;
}

static int sch_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &sch_ct_seq_ops);
}


static void *sch_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}
  

static void sch_ct_seq_stop(struct seq_file *s, void *v)
{
}


static void *sch_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}


static int sch_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nSCH Registers\n");
			seq_dm_long(s, SCH_BASE, 54);
			break;
		default:
			return -ENOSPC;
	}		
	return 0;
}
static int mdio_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mdio_ct_seq_ops);
}


static void *mdio_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}
  

static void mdio_ct_seq_stop(struct seq_file *s, void *v)
{
}


static void *mdio_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}


static int mdio_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nMDIO Registers\n");
			seq_dm_long(s, MDIO_BASE, 68);
			break;
		default:
			return -ENOSPC;
	}		
	return 0;
}

static int dma_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &dma_ct_seq_ops);
}


static void *dma_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}
  

static void dma_ct_seq_stop(struct seq_file *s, void *v)
{
}


static void *dma_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}


static int dma_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nDMA LSO Registers\n");
			seq_dm_long(s, DMA_LSO_BASE, 178);
			break;
		default:
			return -ENOSPC;
	}		
	return 0;
}

static int ni_ct_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ni_ct_seq_ops);
}


static void *ni_ct_seq_start(struct seq_file *s, loff_t *pos)
{
	int i;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}
  

static void ni_ct_seq_stop(struct seq_file *s, void *v)
{
}


static void *ni_ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int i;
	
	(*pos)++;
	i = (int)*pos + 1;;
	
	if (i > 9)
		return NULL;
	else		
		return (void *)i;
}


static int ni_ct_seq_show(struct seq_file *s, void *v)
{
	switch ((int)v)
	{
		case 1:
			seq_printf(s, "\nNI Registers\n");
			seq_dm_long(s, NI_TOP_BASE, 266);
			break;
#if 0			
		case 2:
			seq_printf(s, "\nFE Register\n");
			seq_dm_long(s, FE_TOP_BASE, 158);
			break;
		case 3:
			seq_printf(s, "\nLPM Register\n");
			seq_dm_long(s, LPM_TOP_BASE, 97);
			break;
		case 4:
			seq_printf(s, "\nHASH Register\n");
			seq_dm_long(s, HASH_TOP_BASE, 44);
			break;
		case 5:
			seq_printf(s, "\nQM Register\n");
			seq_dm_long(s, QM_TOP_BASE, 125);
			break;
		case 7:
			seq_printf(s, "\nTM-BM Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x100, 53);
			break;
		case 8:
			seq_printf(s, "\nTM-POL Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x200, 62);
			break;
		case 9:
			seq_printf(s, "\nTM-TC Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x300, 4);
			break;		
		case 10:
			seq_printf(s, "\nTM-BM Register\n");
			seq_dm_long(s, TM_TOP_BASE + 0x400, 43);
			break;			
		case 11:
			seq_printf(s, "\nSCH Registers\n");
			seq_dm_long(s, SCH_BASE, 54);
			break;
		case 12:
			seq_printf(s, "\nDMA LSO Registers\n");
			seq_dm_long(s, DMA_LSO_BASE, 178);
			break;
		case 13:
			seq_printf(s, "\nMDIO Registers\n");
			seq_dm_long(s, MDIO_BASE, 68);
			break;
#endif	/* #if 0 */			
		default:
			return -ENOSPC;
	}		
	return 0;
}


int cs_ni_init_proc(void)
{
	struct proc_dir_entry *proc_ni=NULL, *proc_qm=NULL, *proc_dma=NULL, 
			*proc_mdio=NULL, *proc_sch=NULL;
	
	struct proc_dir_entry *proc_fe=NULL;

	struct proc_dir_entry *proc_tm=NULL;

#ifdef CONFIG_SYSCTL
	// ne_ct_sysctl_header = NULL;
#endif
	proc_ni = proc_net_fops_create(&init_net, CS752x_NI_PROC_NAME, S_IRUGO, &ni_file_ops);
	if (!proc_ni) goto init_bad;
	proc_sch = proc_net_fops_create(&init_net, CS752x_SCH_PROC_NAME, S_IRUGO, &sch_file_ops);
	if (!proc_sch) goto init_bad;
	proc_dma = proc_net_fops_create(&init_net, CS752x_DMA_PROC_NAME, S_IRUGO, &dma_file_ops);
	if (!proc_dma) goto init_bad;			
	proc_mdio = proc_net_fops_create(&init_net, CS752x_MDIO_PROC_NAME, S_IRUGO, &mdio_file_ops);
	if (!proc_mdio) goto init_bad;
	proc_qm = proc_net_fops_create(&init_net, CS752x_QM_PROC_NAME, S_IRUGO, &qm_file_ops);
	if (!proc_qm) goto init_bad;	
	proc_fe = proc_net_fops_create(&init_net, CS752x_FE_PROC_NAME, S_IRUGO, &fe_file_ops);
	if (!proc_fe) goto init_bad;

	proc_tm = proc_net_fops_create(&init_net, CS752x_TM_PROC_NAME, S_IRUGO, &tm_file_ops);
	if (!proc_tm) goto init_bad;

#ifdef CONFIG_SYSCTL
	// ne_ct_sysctl_header = register_sysctl_table(fe_ct_net_table, 0);
	// if (!ne_ct_sysctl_header) goto init_bad;
#endif
	
	
	return 0;
	
init_bad:
	if (proc_ni) proc_net_remove(&init_net, CS752x_NI_PROC_NAME);
	if (proc_ni) proc_net_remove(&init_net, CS752x_QM_PROC_NAME);
	if (proc_ni) proc_net_remove(&init_net, CS752x_SCH_PROC_NAME);
	if (proc_ni) proc_net_remove(&init_net, CS752x_DMA_PROC_NAME);		
	if (proc_ni) proc_net_remove(&init_net, CS752x_MDIO_PROC_NAME);
	if (proc_ni) proc_net_remove(&init_net, CS752x_FE_PROC_NAME);	
	if (proc_tm) proc_net_remove(&init_net, CS752x_TM_PROC_NAME);

#ifdef CONFIG_SYSCTL
	// if (ne_ct_sysctl_header) unregister_sysctl_table(ne_ct_sysctl_header);
#endif
	proc_printf("CS752x NE Proc: can't create proc or register sysctl.\n");
	return -ENOMEM;
}


void cs_ni_fini_proc(void)
{
	proc_net_remove(&init_net, CS752x_NI_PROC_NAME);
	proc_net_remove(&init_net, CS752x_QM_PROC_NAME);
	proc_net_remove(&init_net, CS752x_DMA_PROC_NAME);
	proc_net_remove(&init_net, CS752x_SCH_PROC_NAME);
	proc_net_remove(&init_net, CS752x_MDIO_PROC_NAME);
	proc_net_remove(&init_net, CS752x_FE_PROC_NAME);
	proc_net_remove(&init_net, CS752x_TM_PROC_NAME);

#ifdef CONFIG_SYSCTL
	// unregister_sysctl_table(ne_ct_sysctl_header);
#endif	
}


//module_init(init);
//module_exit(fini);

#endif	/* CONFIG_PROC_FS */
