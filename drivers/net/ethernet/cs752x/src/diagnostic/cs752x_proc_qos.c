/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#ifdef CONFIG_CS752X_PROC
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>	/* copy_*_user */

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <cs_core_logic.h>
#include <mach/cs75xx_qos.h>
#include <../net/8021q/vlan.h>


#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
u32 cs_qos_preference = CS_QOS_PREF_FLOW; 

EXPORT_SYMBOL(cs_qos_preference);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS


#ifdef CONFIG_CS752X_ACCEL_KERNEL
static const char* cs_qos_ingress_port_text[CS_QOS_INGRESS_PORT_MAX_] = {
	"GMAC0",
	"GMAC1",
	"GMAC2",
	"CPU  ",
	"WLAN0",
	"WLAN1",
};
static const char* cs_qos_mode_text[2] = {
	"CS_QOS_MODE_DOT1P",
	"CS_QOS_MODE_DSCP_TC",
};

#define CS_QOS_DSCP_NO                  64
#define CS_QOS_DOT1P_NO                 8
#define CS_QOS_VOQP_NO                  8



// Mapping Table
cs_qos_mode_t cs_qos_current_mode[CS_QOS_INGRESS_PORT_MAX_];
cs_uint8_t cs_qos_dot1p_map_tbl[CS_QOS_INGRESS_PORT_MAX_][CS_QOS_DOT1P_NO];
cs_uint8_t cs_qos_dscp_map_tbl[CS_QOS_INGRESS_PORT_MAX_][CS_QOS_DSCP_NO];


/* file name */
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_QOS_PREF             "cs_qos_pref"
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_QOS_MODE             "cs_qos_mode"
#define CS_QOS_DOT1P_MAP        "cs_qos_dot1p_map"
#define CS_QOS_DSCP_MAP         "cs_qos_dscp_map"


#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_QOS_PREF_HELP_MSG "Purpose: Enable QoS preference\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [value] > %s\n" \
			"value 0: Enable flow QoS preference\n" \
			"value 1: Enable port QoS preference\n"
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_QOS_MODE_HELP_MSG  "Purpose: Set Qos Mode\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [port] [mode] > %s\n" \
			"port:  0~5\n" \
			"    0 - CS_QOS_INGRESS_PORT_GMAC0\n" \
			"    1 - CS_QOS_INGRESS_PORT_GMAC1\n" \
			"    2 - CS_QOS_INGRESS_PORT_GMAC2\n" \
			"    3 - CS_QOS_INGRESS_PORT_CPU\n" \
			"    4 - CS_QOS_INGRESS_PORT_WLAN0\n" \
			"    5 - CS_QOS_INGRESS_PORT_WLAN1\n" \
			"    0xFFFF - All Port\n" \
			"mode: 0~1\n" \
			"    0 - CS_QOS_MODE_DOT1P\n" \
			"    1 - CS_QOS_MODE_DSCP_TC\n"
#define CS_QOS_DOT1P_MAP_HELP_MSG  "Purpose: Set QoS DOT1P mappings\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [port] [dot1p] [priority] > %s\n" \
			"port:  0~5\n" \
			"    0 - CS_QOS_INGRESS_PORT_GMAC0\n" \
			"    1 - CS_QOS_INGRESS_PORT_GMAC1\n" \
			"    2 - CS_QOS_INGRESS_PORT_GMAC2\n" \
			"    3 - CS_QOS_INGRESS_PORT_CPU\n" \
			"    4 - CS_QOS_INGRESS_PORT_WLAN0\n" \
			"    5 - CS_QOS_INGRESS_PORT_WLAN1\n" \
			"    0xFFFF - All Port\n" \
			"dot1p   : 0~7\n" \
			"priority: 0~7\n"
#define CS_QOS_DSCP_MAP_HELP_MSG  "Purpose: Set QoS DSCP mappings\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [port] [dscp] [priority] > %s\n" \
			"port:  0~5\n" \
			"    0 - CS_QOS_INGRESS_PORT_GMAC0\n" \
			"    1 - CS_QOS_INGRESS_PORT_GMAC1\n" \
			"    2 - CS_QOS_INGRESS_PORT_GMAC2\n" \
			"    3 - CS_QOS_INGRESS_PORT_CPU\n" \
			"    4 - CS_QOS_INGRESS_PORT_WLAN0\n" \
			"    5 - CS_QOS_INGRESS_PORT_WLAN1\n" \
			"    0xFFFF - All Port\n" \
			"dscp    : 0~63\n" \
			"priority: 0~7\n"

/* entry pointer */
extern struct proc_dir_entry *proc_driver_cs752x_qos;
extern int cs752x_add_proc_handler(const char *name,
                            const struct file_operations *proc_fops,
                            struct proc_dir_entry *parent);
extern void cs_hw_accel_mgr_delete_flow_based_hash_entry(void);

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
extern int cs_offset_based_qos_mode_update(u8 port_id);
extern int cs_offset_based_qos_dscp_update(u8 port_id, u8 dscp);
extern int cs_offset_based_qos_dot1p_update(u8 port_id, u8 dot1p);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

static void cs_qos_del_hash(void)
{
	cs_hw_accel_mgr_delete_flow_based_hash_entry();
}


/*
 * QoS APIs
 */
#define ETH_P_8021Q	    0x8100          /* 802.1Q VLAN Extended Header  */
#define ETH_P_QINQ1	    0x9100
#define ETH_P_QINQ2	    0x9200
#define ETH_P_8021AD	0x88A8
#define VLAN_TAG_LEN        4

#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
cs_port_id_t cs_qos_get_voq_id(struct sk_buff *skb)
{
    u8 dscp=0, dot1p=0;
    u8 qid_idx=0;
    cs_qos_mode_t qos_mode = CS_QOS_MODE_DOT1P;
    cs_uint8_t priority = 0;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
    struct iphdr *iph = NULL;
    struct ethhdr *eth = NULL;
    struct vlan_ethhdr *vlan_eth_hdr = NULL;	
    u16 *proto_type = NULL;

   // prepare VoQ
    qid_idx = 7 - CS_QOS_DSCP_DEFAULT_PRIORITY;

    if(cs_qos_preference != CS_QOS_PREF_PORT) { 
        return qid_idx;
    }

    if (skb->protocol == htons(ETH_P_ARP)) {
//        printk("### ETH_P_ARP\n");
        return (7 - CS_QOS_ARP_DEFAULT_PRIORITY);
    }

	if (cs_cb != NULL) {
        if (cs_qos_map_mode_get(0, cs_cb->key_misc.orig_lspid, &qos_mode) == CS_E_OK) {
            switch (qos_mode) {
                case CS_QOS_MODE_DOT1P:
                    if ((cs_cb->input.raw.vlan_tpid == ETH_P_8021Q) ||
                			(cs_cb->input.raw.vlan_tpid == ETH_P_8021AD) ||
                			(cs_cb->input.raw.vlan_tpid == ETH_P_QINQ1) ||
                			(cs_cb->input.raw.vlan_tpid == ETH_P_QINQ2)) {
                	    // Get 802.1p Tag(Bridge)
                	    dot1p = (cs_cb->input.raw.vlan_tci >> 13) & 0x07;
                	    cs_qos_dot1p_map_get(0, cs_cb->key_misc.orig_lspid, dot1p, &priority);
            	        qid_idx = 7 - priority;
                	}
                    break;
                case CS_QOS_MODE_DSCP_TC:
                    if (cs_cb->common.module_mask & CS_MOD_MASK_NAT) {
                        // Has L3 DSCP (NAT)
                		dscp = (cs_cb->input.l3_nh.iph.tos >> 2) & 0x3f;
                    //Bug#40322
                	} else if (cs_cb->common.module_mask & CS_MOD_MASK_DSCP) {
                	    // Get DSCP (Bridge)
               		    dscp = (cs_cb->input.l3_nh.iph.tos >> 2) & 0x3f;
                	}
            		cs_qos_dscp_map_get(0, cs_cb->key_misc.orig_lspid, dscp, &priority);
        		    qid_idx = 7 - priority;
                    break;
            }/* switch (qos_mode) */
        } /* if (cs_qos_map_mode_get()) */
	}/* if (cs_cb != NULL) */

    return qid_idx;
} /* cs_qos_get_voq_id() */
EXPORT_SYMBOL(cs_qos_get_voq_id);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS


cs_status_t cs_qos_map_mode_set( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_IN  cs_qos_mode_t mode )
{
    int i;
    
    if(mode > CS_QOS_MODE_DSCP_TC) 
        return CS_E_PARAM;
        
    if (port_id == CS_QOS_INGRESS_PORT_ALL) {
        for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++){
            cs_qos_current_mode[i] = mode;
        }
        // delete hash
        cs_qos_del_hash();

        return CS_E_OK;
    }
    
    if (port_id >= CS_QOS_INGRESS_PORT_MAX_) {
        return CS_E_PARAM;
    }
    
    cs_qos_current_mode[port_id] = mode;
    // delete hash
    cs_qos_del_hash();
    
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    cs_offset_based_qos_mode_update(port_id);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
    
    return CS_E_OK;
}/* cs_qos_map_mode_set() */
EXPORT_SYMBOL(cs_qos_map_mode_set);


cs_status_t cs_qos_map_mode_get( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_OUT cs_qos_mode_t *mode )
{
    if (mode == NULL){
        return CS_E_PARAM;
    }
    
    if (port_id >= CS_QOS_INGRESS_PORT_MAX_) {
        return CS_E_PARAM;
    }

    *mode = cs_qos_current_mode[port_id];

    return CS_E_OK;
}/* cs_qos_map_mode_get() */
EXPORT_SYMBOL(cs_qos_map_mode_get);


cs_status_t cs_qos_dot1p_map_set( CS_IN  cs_dev_id_t dev_id, 
                                  CS_IN  cs_port_id_t port_id, 
                                  CS_IN  cs_uint8_t dot1p, 
                                  CS_IN  cs_uint8_t priority)
{
    int i;
    
    if (dot1p >= CS_QOS_DOT1P_NO) {
        return CS_E_PARAM;
    }

    if(priority >= CS_QOS_VOQP_NO) {    
        return CS_E_PARAM;
    }    

    if (port_id == CS_QOS_INGRESS_PORT_ALL) {
        for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++){
            cs_qos_dot1p_map_tbl[i][dot1p] = priority;
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
            cs_offset_based_qos_dot1p_update(i, dot1p);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
        }
        // delete hash
        cs_qos_del_hash();

        return CS_E_OK;
    }
    
    if (port_id >= CS_QOS_INGRESS_PORT_MAX_) {
        return CS_E_PARAM;
    }
    
    cs_qos_dot1p_map_tbl[port_id][dot1p] = priority;
    // delete hash
    cs_qos_del_hash();
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    cs_offset_based_qos_dot1p_update(port_id, dot1p);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
    
    return CS_E_OK;
}/* cs_qos_dot1p_map_set() */
EXPORT_SYMBOL(cs_qos_dot1p_map_set);
 
cs_status_t cs_qos_dot1p_map_get( CS_IN  cs_dev_id_t dev_id, 
                                  CS_IN  cs_port_id_t port_id, 
                                  CS_IN  cs_uint8_t dot1p, 
                                  CS_OUT cs_uint8_t *priority)
{
    if (port_id >= CS_QOS_INGRESS_PORT_MAX_) {
        return CS_E_PARAM;
    }

    if (dot1p >= CS_QOS_DOT1P_NO) {
        return CS_E_PARAM;
    }

    *priority = cs_qos_dot1p_map_tbl[port_id][dot1p];

    return CS_E_OK;
}/* cs_qos_dot1p_map_get() */
EXPORT_SYMBOL(cs_qos_dot1p_map_get);

cs_status_t cs_qos_dscp_map_set( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_IN  cs_uint8_t dscp, 
                                 CS_IN  cs_uint8_t priority )
{
    int i;
    
    if (dscp >= CS_QOS_DSCP_NO) {
        return CS_E_PARAM;
    }
    
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    if (dscp & 0x01) {
        return CS_E_PARAM;
    }
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
    
    if(priority >= CS_QOS_VOQP_NO) {    
        return CS_E_PARAM;
    }

    if (port_id == CS_QOS_INGRESS_PORT_ALL) {
        for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++){
            cs_qos_dscp_map_tbl[i][dscp] = priority;
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
            cs_offset_based_qos_dscp_update(i, dscp);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
        }
        // delete hash
        cs_qos_del_hash();

        return CS_E_OK;
    }
    
    if (port_id >= CS_QOS_INGRESS_PORT_MAX_) {
        return CS_E_PARAM;
    }

    cs_qos_dscp_map_tbl[port_id][dscp] = priority;
    
    // delete hash
    cs_qos_del_hash();
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    cs_offset_based_qos_dscp_update(port_id, dscp);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

    return CS_E_OK;
}/* cs_qos_dscp_map_set() */
EXPORT_SYMBOL(cs_qos_dscp_map_set);
 
cs_status_t cs_qos_dscp_map_get( CS_IN  cs_dev_id_t dev_id, 
                                 CS_IN  cs_port_id_t port_id, 
                                 CS_IN  cs_uint8_t dscp, 
                                 CS_OUT cs_uint8_t *priority)
{
    if(priority == NULL)
        return CS_E_PARAM;
    
    if (port_id >= CS_QOS_INGRESS_PORT_MAX_) {
        return CS_E_PARAM;
    }

    if (dscp >= CS_QOS_DSCP_NO) {
        return CS_E_PARAM;
    }

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    dscp &= ~0x01;
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

    *priority = cs_qos_dscp_map_tbl[port_id][dscp];

    return CS_E_OK;
}/* cs_qos_dscp_map_get() */
EXPORT_SYMBOL(cs_qos_dscp_map_get);

/*
 * QoS PROC
 */
 
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
/*
 * Qos Preference
 */
static int cs_proc_qos_pref_read_proc(struct seq_file *m, void *v)
{
	seq_printf(m, CS_QOS_PREF_HELP_MSG,
			CS_QOS_PREF, CS_QOS_PREF);
	seq_printf(m, "\n%s = %d\n", CS_QOS_PREF,
			cs_qos_preference);
	return 0;
}

static int cs_proc_qos_pref_write_proc(struct file *file,
		const char __user *buffer, size_t count, loff_t *off)
{
	char buf[32];
	unsigned long mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto QOS_PREF_INVAL_EXIT;

	buf[len] = '\0';
	if (kstrtoul(buf, 0, &mask))
		goto QOS_PREF_INVAL_EXIT;

	cs_qos_preference = mask;

	printk(KERN_WARNING "\nSet %s as %d\n", CS_QOS_PREF,
			cs_qos_preference);

	return count;

QOS_PREF_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_QOS_PREF_HELP_MSG,
			CS_QOS_PREF, CS_QOS_PREF);
	return count;
	/* if we return error code here, PROC fs may retry up to 3 times. */
}
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS


#ifdef CONFIG_CS752X_ACCEL_KERNEL
/*
 * Qos mode
 */
static int cs_proc_qos_mode_read_proc(struct seq_file *m, void *v)
{
    int i;
    cs_qos_mode_t mode;

	seq_printf(m, CS_QOS_MODE_HELP_MSG, CS_QOS_MODE,
			CS_QOS_MODE);
    seq_printf(m, "\nport_id     Mode");
    seq_printf(m, "\n----------------------------");
    for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++) {
	    if (cs_qos_map_mode_get(0, i, &mode) == CS_E_OK)
	    {
    	    seq_printf(m, "\n%s    %s", 
    	            cs_qos_ingress_port_text[i], cs_qos_mode_text[mode]);
	    }
    }
	seq_printf(m, "\n");

	return 0;
}/* cs_proc_qos_mode_read_proc() */


static int cs_proc_qos_mode_write_proc(struct file *file, const char __user *buffer,
					size_t count, loff_t *off)
{
	char buf[32];
	int ingress_port;
	int mode;
	ssize_t len;
    int para_no;


	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto QOS_MODE_INVAL_EXIT;


	buf[len] = '\0';
	para_no = sscanf(buf, "%x %d\n", &ingress_port, &mode);
	if (para_no != 2)
	    goto QOS_MODE_INVAL_EXIT;

    if ( (ingress_port != CS_QOS_INGRESS_PORT_ALL) &&
         (ingress_port >= CS_QOS_INGRESS_PORT_MAX_)) {
        goto QOS_MODE_INVAL_EXIT;
    }
    if ((mode < 0) || (mode > CS_QOS_MODE_DSCP_TC)) {
        goto QOS_MODE_INVAL_EXIT;
    }

    printk(KERN_WARNING "Set ingress_port#%d to %s\n", (u8)ingress_port, 
                cs_qos_ingress_port_text[mode]);

    cs_qos_map_mode_set( 0,    //device_id: reserved
                     (cs_port_id_t)ingress_port, 
                        (cs_qos_mode_t) mode );

	return count;

QOS_MODE_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_QOS_MODE_HELP_MSG, CS_QOS_MODE, 
	        CS_QOS_MODE);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}/* cs_proc_qos_mode_write_proc() */

/*
 * Qos dot1p map
 */
static int cs_proc_qos_dot1p_map_read_proc(struct seq_file *m, void *v)
{
	u32 len = 0;
    int i, j;
    cs_uint8_t priority;

	seq_printf(m, CS_QOS_DOT1P_MAP_HELP_MSG, CS_QOS_DOT1P_MAP,
			CS_QOS_DOT1P_MAP);

    for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++) {
	    seq_printf(m, "\n[%s]", cs_qos_ingress_port_text[i]);
	    seq_printf(m, "\ndot1p#   priority#");
	    seq_printf(m, "\n------------------");

    	for (j=0; j<CS_QOS_DOT1P_NO; j++) {
    	    if (cs_qos_dot1p_map_get(0, i, j, &priority) == CS_E_OK)
    	    {
        	    seq_printf(m, "\n %2d      %d", 
        	            j, priority);
    	    }
    	}
    	seq_printf(m, "\n");
    }
	return 0;
}/* cs_proc_qos_dot1p_map_read_proc() */


static int cs_proc_qos_dot1p_map_write_proc(struct file *file, const char __user *buffer,
				    size_t count, loff_t *off)
{
	char buf[32];
	int ingress_port;
	int dot1p;
	int priority;
	ssize_t len;
    int para_no;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto QOS_DOT1P_MAP_INVAL_EXIT;


	buf[len] = '\0';
	para_no = sscanf(buf, "%x %d %d\n", &ingress_port, &dot1p, &priority);
	if (para_no != 3)
	    goto QOS_DOT1P_MAP_INVAL_EXIT;
    
    if ( ((cs_port_id_t)ingress_port != CS_QOS_INGRESS_PORT_ALL) &&
         ((cs_port_id_t)ingress_port >= CS_QOS_INGRESS_PORT_MAX_)) {
        goto QOS_DOT1P_MAP_INVAL_EXIT;
    }
    if ((dot1p < 0) || (dot1p >= CS_QOS_DOT1P_NO)) {
        goto QOS_DOT1P_MAP_INVAL_EXIT;
    }
    if ((priority < 0) || (priority >= CS_QOS_VOQP_NO)) {
        goto QOS_DOT1P_MAP_INVAL_EXIT;
    }

    printk(KERN_WARNING "Set ingress_port#%d dot1p#%d as priority#%d\n", (u8)ingress_port, 
                dot1p, priority);

    cs_qos_dot1p_map_set( 0,    //device_id: reserved
                          (cs_port_id_t)ingress_port, 
                          (cs_uint8_t)dot1p,
                          (cs_uint8_t)priority );

	return count;

QOS_DOT1P_MAP_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_QOS_DOT1P_MAP_HELP_MSG, CS_QOS_DOT1P_MAP, 
	        CS_QOS_DOT1P_MAP);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}/* cs_proc_qos_dot1p_map_write_proc() */

/*
 * Qos dscp map
 */
static int cs_proc_qos_dscp_map_read_proc(struct seq_file *m, void *v)
{
	u32 len = 0;
    int i, j;
    cs_uint8_t priority;
    
    
	seq_printf(m, CS_QOS_DSCP_MAP_HELP_MSG, CS_QOS_DSCP_MAP,
			CS_QOS_DSCP_MAP);
    
    for (i=0; i<CS_QOS_INGRESS_PORT_WLAN1; i++) {
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
        if (i == CS_QOS_INGRESS_PORT_CPU) {
            continue;
        }
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
	    seq_printf(m, "\n[%s]", cs_qos_ingress_port_text[i]);
	    seq_printf(m, "\ndscp#   priority#");
	    seq_printf(m, "\n------------------");

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    	for (j=0; j<CS_QOS_DSCP_NO; j+=2) {
#else
    	for (j=0; j<CS_QOS_DSCP_NO; j++) {
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
    	    if (cs_qos_dscp_map_get(0, i, j, &priority) == CS_E_OK)
    	    {
        	    seq_printf(m, "\n %2d      %d", 
        	            j, priority);
    	    }
    	}
    	seq_printf(m, "\n");
    }
	return 0;
}/* cs_proc_qos_dscp_map_read_proc() */


static int cs_proc_qos_dscp_map_write_proc(struct file *file, const char __user *buffer,
				    size_t count, loff_t *off)
{
	char buf[32];
	int ingress_port;
	int dscp ;
	int priority;
	ssize_t len;
    int para_no;
  
  
	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto QOS_DSCP_MAP_INVAL_EXIT;


	buf[len] = '\0';
	para_no = sscanf(buf, "%x %d %d\n", &ingress_port, &dscp, &priority);
	if (para_no != 3)
	    goto QOS_DSCP_MAP_INVAL_EXIT;
    
    if ( (ingress_port != CS_QOS_INGRESS_PORT_ALL) &&
         (ingress_port >= CS_QOS_INGRESS_PORT_MAX_)) {
        goto QOS_DSCP_MAP_INVAL_EXIT;
    }
    if ((dscp < 0) || (dscp >= CS_QOS_DSCP_NO)) {
        goto QOS_DSCP_MAP_INVAL_EXIT;
    }
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
    if (dscp & 0x01) {
        printk(KERN_WARNING "ERROR: Don't support DSCP Experimental or Local Use\n");
        goto QOS_DSCP_MAP_INVAL_EXIT;
    }
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
    if ((priority < 0) || (priority >= CS_QOS_VOQP_NO)) {
        goto QOS_DSCP_MAP_INVAL_EXIT;
    }

    printk(KERN_WARNING "Set ingress_port#%d dscp#%d as priority#%d\n", (u8)ingress_port, 
                dscp, priority);

    cs_qos_dscp_map_set( 0,    //device_id: reserved
                         (cs_port_id_t)ingress_port, 
                         (cs_uint8_t) dscp,
                         (cs_uint8_t) priority );

	return count;

QOS_DSCP_MAP_INVAL_EXIT:
	printk(KERN_WARNING "Invalid argument\n");
	printk(KERN_WARNING CS_QOS_DSCP_MAP_HELP_MSG, CS_QOS_DSCP_MAP, 
	        CS_QOS_DSCP_MAP);
	/* if we return error code here, PROC fs may retry up to 3 times. */
	return count;
}/* cs_proc_qos_dscp_map_write_proc() */

static void cs_init_mode(void)
{
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_GMAC0] = CS_QOS_MODE_DOT1P;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_GMAC1] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_GMAC2] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_WLAN0] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_WLAN1] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_CPU] = CS_QOS_MODE_DSCP_TC;
    
    return;
}/* cs_init_mode() */

static void cs_init_mappings(void)
{
    int i, j;

    //Set default DOT1P default priority
    // DOT1P     Priority
    // ==================
    //   0          0
    //   1          1
    //   2          2
    //   3          3
    //   4          4
    //   5          5
    //   6          6
    //   7          7
    for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++) {
        for (j=0; j<CS_QOS_DOT1P_NO; j++) {
            cs_qos_dot1p_map_tbl[i][j] = j;
        }
    }
    //Set default DSCP default priority
    //  DSCP      Priority
    // ==================
    // 000xxx        0
    // 001xxx        1
    // 010xxx        2
    // 011xxx        3
    // 100xxx        4
    // 101xxx        5
    // 110xxx        6
    // 111xxx        7
    for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++) {
        for (j=0; j<CS_QOS_DSCP_NO; j++) {
            cs_qos_dscp_map_tbl[i][j] = (j >> 3) & 0x07;
        }
    }

    return;
}/* cs_init_mappings() */

static void cs_init_qos_config_default(void)
{
    cs_init_mode();
    cs_init_mappings();
    return;
}/* cs_init_qos_config_default() */
#endif //CONFIG_CS752X_ACCEL_KERNEL

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

#define CS752X_DEFINE_RW_PROC_OPS(x) \
	static int x##_read_proc(struct seq_file *, void *); \
	static int x##_write_proc(struct file *, const char __user *, \
			size_t, loff_t *); \
	_CS752X_DEFINE_PROC_OPS(x, x##_read_proc, x##_write_proc)

CS752X_DEFINE_RW_PROC_OPS(cs_proc_qos_pref)
CS752X_DEFINE_RW_PROC_OPS(cs_proc_qos_mode)
CS752X_DEFINE_RW_PROC_OPS(cs_proc_qos_dot1p_map)
CS752X_DEFINE_RW_PROC_OPS(cs_proc_qos_dscp_map)

void cs_qos_proc_init_module(void)
{
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
    cs_init_qos_config_default();

    cs752x_add_proc_handler(CS_QOS_PREF,
				&cs_proc_qos_pref_proc_fops,
				proc_driver_cs752x_qos);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
	cs752x_add_proc_handler(CS_QOS_MODE,
				&cs_proc_qos_mode_proc_fops,
				proc_driver_cs752x_qos);
	cs752x_add_proc_handler(CS_QOS_DOT1P_MAP,
				&cs_proc_qos_dot1p_map_proc_fops,
				proc_driver_cs752x_qos);
	cs752x_add_proc_handler(CS_QOS_DSCP_MAP,
				&cs_proc_qos_dscp_map_proc_fops,
				proc_driver_cs752x_qos);

	return;
}/* cs_qos_proc_init_module() */

void cs_qos_proc_exit_module(void)
{
	/* no problem if it =was not registered */
	/* remove file entry */
	remove_proc_entry(CS_QOS_DSCP_MAP, proc_driver_cs752x_qos);
	remove_proc_entry(CS_QOS_DOT1P_MAP, proc_driver_cs752x_qos);
	remove_proc_entry(CS_QOS_MODE, proc_driver_cs752x_qos);
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
	remove_proc_entry(CS_QOS_PREF, proc_driver_cs752x_qos);
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

	return;
}/* cs_qos_proc_exit_module () */

#endif /* CONFIG_CS752X_PROC */
#endif /* CONFIG_CS752X_ACCEL_KERNEL */
