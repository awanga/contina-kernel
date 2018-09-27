/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

/*
 * cs_fe_table_ut.c
 *
 * $Id: cs_fe_table_ut.c,v 1.7 2011/05/12 02:12:17 ewang Exp $
 *
 * Unit Test procedure of FE table management.
 */

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe_table_generic.h"
#include "cs752x_ioctl.h"
#include <mach/cs75xx_fe_core_table.h>

#include "cs_fe_table_ut.h"	/* local definitions */

#define PFX     "CS_UT"
#define PRINT(format, args...) printk(KERN_WARNING PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)

#define MESSAGE(format, args...) if(level>=1) \
	printk(KERN_WARNING format, ## args); \
	else printk(KERN_WARNING ".")

#define DBG(format, args...) if(level>=2) printk(KERN_ERR PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)

#define REG_UT_PLAN(x) do { \
	if( (ret=cs_fe_reg_ut_plan(x))!=0 ) { \
		DBG("register (%s) fail, ret = %d\n", #x, ret); \
		goto init_fail; \
	} \
} while(0)

#define VERIFY_OP(x) if( (ret=x)!=0 ) { \
	DBG("error code = %d\n", ret); \
	return ret; \
}

#define PRINT_CURR_STAGE(TABLE, STAGE) printk( \
	"= %s test of %s table\n===========================\n", STAGE, TABLE)

#define RPINT_RESULT_PASS if(level==0) printk( KERN_WARNING " pass\n")
#define RPINT_RESULT_FAIL if(level==0) printk( KERN_WARNING " fail\n")
#define SHOW(x) if(level>=1) x; \
	else printk(KERN_WARNING ".")

#define CHECK_STEP do { \
	curr_step++; \
	if( curr_step >= end_step ) return 0; \
} while(0)

typedef int (*ut_plan_func_t) (void);

/* extern functions */

/* AN BNG MAC table */
int fe_an_bng_mac_alloc_entry_ut(unsigned int *rslt_idx,
				 unsigned int start_offset);
int fe_an_bng_mac_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
					   unsigned int size);
int fe_an_bng_mac_set_entry_ut(unsigned int idx, void *entry);
int fe_an_bng_mac_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_an_bng_mac_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_an_bng_mac_del_entry_ut(void *entry, bool f_force);
int fe_an_bng_mac_find_entry_ut(void *entry, unsigned int *rslt_idx,
				unsigned int start_offset);
int fe_an_bng_mac_get_entry_ut(unsigned int idx, void *entry);
int fe_an_bng_mac_inc_entry_refcnt_ut(unsigned int idx);
int fe_an_bng_mac_dec_entry_refcnt_ut(unsigned int idx);
int fe_an_bng_mac_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_an_bng_mac_set_field_ut(unsigned int idx, unsigned int field,
			       __u32 *p_value);
int fe_an_bng_mac_get_field_ut(unsigned int idx, unsigned int field,
			       __u32 *p_value);
int fe_an_bng_mac_flush_table_ut(void);
int fe_an_bng_mac_get_avail_count_ut(void);
void fe_an_bng_mac_print_entry_ut(unsigned int idx);
void fe_an_bng_mac_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_an_bng_mac_print_table_ut(void);

/* class table */
int fe_class_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_class_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				      unsigned int size);
int fe_class_set_entry_ut(unsigned int idx, void *entry);
int fe_class_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_class_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_class_del_entry_ut(void *entry, bool f_force);
int fe_class_find_entry_ut(void *entry, unsigned int *rslt_idx,
			   unsigned int start_offset);
int fe_class_get_entry_ut(unsigned int idx, void *entry);
int fe_class_inc_entry_refcnt_ut(unsigned int idx);
int fe_class_dec_entry_refcnt_ut(unsigned int idx);
int fe_class_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_class_set_field_ut(unsigned int idx, unsigned int field,
			  __u32 *p_value);
int fe_class_get_field_ut(unsigned int idx, unsigned int field,
			  __u32 *p_value);
int fe_class_flush_table_ut(void);
int fe_class_get_avail_count_ut(void);
void fe_class_print_entry_ut(unsigned int idx);
void fe_class_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_class_print_table_ut(void);

/* fwdrslt table */
int fe_fwdrslt_alloc_entry_ut(unsigned int *rslt_idx,
			      unsigned int start_offset);
int fe_fwdrslt_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
					unsigned int size);
int fe_fwdrslt_set_entry_ut(unsigned int idx, void *entry);
int fe_fwdrslt_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_fwdrslt_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_fwdrslt_del_entry_ut(void *entry, bool f_force);
int fe_fwdrslt_find_entry_ut(void *entry, unsigned int *rslt_idx,
			     unsigned int start_offset);
int fe_fwdrslt_get_entry_ut(unsigned int idx, void *entry);
int fe_fwdrslt_inc_entry_refcnt_ut(unsigned int idx);
int fe_fwdrslt_dec_entry_refcnt_ut(unsigned int idx);
int fe_fwdrslt_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_fwdrslt_set_field_ut(unsigned int idx, unsigned int field,
			    __u32 *p_value);
int fe_fwdrslt_get_field_ut(unsigned int idx, unsigned int field,
			    __u32 *p_value);
int fe_fwdrslt_flush_table_ut(void);
int fe_fwdrslt_get_avail_count_ut(void);
void fe_fwdrslt_print_entry_ut(unsigned int idx);
void fe_fwdrslt_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_fwdrslt_print_table_ut(void);

/* lpb table */
int fe_lpb_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_lpb_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				    unsigned int size);
int fe_lpb_set_entry_ut(unsigned int idx, void *entry);
int fe_lpb_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_lpb_find_entry_ut(void *entry, unsigned int *rslt_idx,
			 unsigned int start_offset);
int fe_lpb_get_entry_ut(unsigned int idx, void *entry);
int fe_lpb_set_field_ut(unsigned int idx, unsigned int field, __u32 *p_value);
int fe_lpb_get_field_ut(unsigned int idx, unsigned int field, __u32 *p_value);
int fe_lpb_flush_table_ut(void);
void fe_lpb_print_entry_ut(unsigned int idx);
void fe_lpb_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_lpb_print_table_ut(void);

/* sdb table */
int fe_sdb_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_sdb_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				    unsigned int size);
int fe_sdb_set_entry_ut(unsigned int idx, void *entry);
int fe_sdb_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_sdb_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_sdb_del_entry_ut(void *entry, bool f_force);
int fe_sdb_find_entry_ut(void *entry, unsigned int *rslt_idx,
			 unsigned int start_offset);
int fe_sdb_get_entry_ut(unsigned int idx, void *entry);
int fe_sdb_inc_entry_refcnt_ut(unsigned int idx);
int fe_sdb_dec_entry_refcnt_ut(unsigned int idx);
int fe_sdb_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_sdb_set_field_ut(unsigned int idx, unsigned int field, __u32 *p_value);
int fe_sdb_get_field_ut(unsigned int idx, unsigned int field, __u32 *p_value);
int fe_sdb_flush_table_ut(void);
int fe_sdb_get_avail_count_ut(void);
void fe_sdb_print_entry_ut(unsigned int idx);
void fe_sdb_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_sdb_print_table_ut(void);

/* voqpol table */
int fe_voqpol_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_voqpol_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				       unsigned int size);
int fe_voqpol_set_entry_ut(unsigned int idx, void *entry);
int fe_voqpol_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_voqpol_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_voqpol_del_entry_ut(void *entry, bool f_force);
int fe_voqpol_find_entry_ut(void *entry, unsigned int *rslt_idx,
			    unsigned int start_offset);
int fe_voqpol_get_entry_ut(unsigned int idx, void *entry);
int fe_voqpol_inc_entry_refcnt_ut(unsigned int idx);
int fe_voqpol_dec_entry_refcnt_ut(unsigned int idx);
int fe_voqpol_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_voqpol_set_field_ut(unsigned int idx, unsigned int field,
			   __u32 *p_value);
int fe_voqpol_get_field_ut(unsigned int idx, unsigned int field,
			   __u32 *p_value);
int fe_voqpol_flush_table_ut(void);
int fe_voqpol_get_avail_count_ut(void);
void fe_voqpol_print_entry_ut(unsigned int idx);
void fe_voqpol_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_voqpol_print_table_ut(void);

/* hashhash table */
int fe_hashhash_alloc_entry_ut(unsigned int *rslt_idx,
			       unsigned int start_offset);
int fe_hashhash_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
					 unsigned int size);
int fe_hashhash_set_entry_ut(unsigned int idx, void *entry);
int fe_hashhash_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_hashhash_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_hashhash_del_entry_ut(void *entry, bool f_force);
int fe_hashhash_find_entry_ut(void *entry, unsigned int *rslt_idx,
			      unsigned int start_offset);
int fe_hashhash_get_entry_ut(unsigned int idx, void *entry);
int fe_hashhash_inc_entry_refcnt_ut(unsigned int idx);
int fe_hashhash_dec_entry_refcnt_ut(unsigned int idx);
int fe_hashhash_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_hashhash_set_field_ut(unsigned int idx, unsigned int field,
			     __u32 *p_value);
int fe_hashhash_get_field_ut(unsigned int idx, unsigned int field,
			     __u32 *p_value);
int fe_hashhash_flush_table_ut(void);
int fe_hashhash_get_avail_count_ut(void);
void fe_hashhash_print_entry_ut(unsigned int idx);
void fe_hashhash_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_hashhash_print_table_ut(void);

/* hashmask table */
int fe_hashmask_alloc_entry_ut(unsigned int *rslt_idx,
			       unsigned int start_offset);
int fe_hashmask_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
					 unsigned int size);
int fe_hashmask_set_entry_ut(unsigned int idx, void *entry);
int fe_hashmask_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_hashmask_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_hashmask_del_entry_ut(void *entry, bool f_force);
int fe_hashmask_find_entry_ut(void *entry, unsigned int *rslt_idx,
			      unsigned int start_offset);
int fe_hashmask_get_entry_ut(unsigned int idx, void *entry);
int fe_hashmask_inc_entry_refcnt_ut(unsigned int idx);
int fe_hashmask_dec_entry_refcnt_ut(unsigned int idx);
int fe_hashmask_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_hashmask_set_field_ut(unsigned int idx, unsigned int field,
			     __u32 *p_value);
int fe_hashmask_get_field_ut(unsigned int idx, unsigned int field,
			     __u32 *p_value);
int fe_hashmask_flush_table_ut(void);
int fe_hashmask_get_avail_count_ut(void);
void fe_hashmask_print_entry_ut(unsigned int idx);
void fe_hashmask_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_hashmask_print_table_ut(void);

/* fvlan table */
int fe_fvlan_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_fvlan_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				      unsigned int size);
int fe_fvlan_set_entry_ut(unsigned int idx, void *entry);
int fe_fvlan_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_fvlan_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_fvlan_del_entry_ut(void *entry, bool f_force);
int fe_fvlan_find_entry_ut(void *entry, unsigned int *rslt_idx,
			   unsigned int start_offset);
int fe_fvlan_get_entry_ut(unsigned int idx, void *entry);
int fe_fvlan_inc_entry_refcnt_ut(unsigned int idx);
int fe_fvlan_dec_entry_refcnt_ut(unsigned int idx);
int fe_fvlan_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_fvlan_set_field_ut(unsigned int idx, unsigned int field,
			  __u32 *p_value);
int fe_fvlan_get_field_ut(unsigned int idx, unsigned int field,
			  __u32 *p_value);
int fe_fvlan_flush_table_ut(void);
int fe_fvlan_get_avail_count_ut(void);
void fe_fvlan_print_entry_ut(unsigned int idx);
void fe_fvlan_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_fvlan_print_table_ut(void);

/* hashoverflow table */
int fe_hashoverflow_alloc_entry_ut(unsigned int *rslt_idx,
				   unsigned int start_offset);
int fe_hashoverflow_convert_sw_to_hw_data_ut(void *sw_entry,
					     __u32 *p_data_array,
					     unsigned int size);
int fe_hashoverflow_set_entry_ut(unsigned int idx, void *entry);
int fe_hashoverflow_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_hashoverflow_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_hashoverflow_del_entry_ut(void *entry, bool f_force);
int fe_hashoverflow_find_entry_ut(void *entry, unsigned int *rslt_idx,
				  unsigned int start_offset);
int fe_hashoverflow_get_entry_ut(unsigned int idx, void *entry);
int fe_hashoverflow_inc_entry_refcnt_ut(unsigned int idx);
int fe_hashoverflow_dec_entry_refcnt_ut(unsigned int idx);
int fe_hashoverflow_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_hashoverflow_set_field_ut(unsigned int idx, unsigned int field,
				 __u32 *p_value);
int fe_hashoverflow_get_field_ut(unsigned int idx, unsigned int field,
				 __u32 *p_value);
int fe_hashoverflow_flush_table_ut(void);
int fe_hashoverflow_get_avail_count_ut(void);
void fe_hashoverflow_print_entry_ut(unsigned int idx);
void fe_hashoverflow_print_range_ut(unsigned int start_idx,
				    unsigned int end_idx);
void fe_hashoverflow_print_table_ut(void);

/* hashstatus table */
int fe_hashstatus_set_field_ut(unsigned int idx, unsigned int field,
			       __u32 *p_value);
int fe_hashstatus_get_field_ut(unsigned int idx, unsigned int field,
			       __u32 *p_value);
int fe_hashstatus_flush_table_ut(void);

/* pktlen table */
int fe_pktlen_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_pktlen_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				       unsigned int size);
int fe_pktlen_set_entry_ut(unsigned int idx, void *entry);
int fe_pktlen_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_pktlen_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_pktlen_del_entry_ut(void *entry, bool f_force);
int fe_pktlen_find_entry_ut(void *entry, unsigned int *rslt_idx,
			    unsigned int start_offset);
int fe_pktlen_get_entry_ut(unsigned int idx, void *entry);
int fe_pktlen_inc_entry_refcnt_ut(unsigned int idx);
int fe_pktlen_dec_entry_refcnt_ut(unsigned int idx);
int fe_pktlen_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_pktlen_set_field_ut(unsigned int idx, unsigned int field,
			   __u32 *p_value);
int fe_pktlen_get_field_ut(unsigned int idx, unsigned int field,
			   __u32 *p_value);
int fe_pktlen_flush_table_ut(void);
int fe_pktlen_get_avail_count_ut(void);
void fe_pktlen_print_entry_ut(unsigned int idx);
void fe_pktlen_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_pktlen_print_table_ut(void);

/* qosrslt table */
int fe_qosrslt_alloc_entry_ut(unsigned int *rslt_idx,
			      unsigned int start_offset);
int fe_qosrslt_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
					unsigned int size);
int fe_qosrslt_set_entry_ut(unsigned int idx, void *entry);
int fe_qosrslt_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_qosrslt_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_qosrslt_del_entry_ut(void *entry, bool f_force);
int fe_qosrslt_find_entry_ut(void *entry, unsigned int *rslt_idx,
			     unsigned int start_offset);
int fe_qosrslt_get_entry_ut(unsigned int idx, void *entry);
int fe_qosrslt_inc_entry_refcnt_ut(unsigned int idx);
int fe_qosrslt_dec_entry_refcnt_ut(unsigned int idx);
int fe_qosrslt_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_qosrslt_set_field_ut(unsigned int idx, unsigned int field,
			    __u32 *p_value);
int fe_qosrslt_get_field_ut(unsigned int idx, unsigned int field,
			    __u32 *p_value);
int fe_qosrslt_flush_table_ut(void);
int fe_qosrslt_get_avail_count_ut(void);
void fe_qosrslt_print_entry_ut(unsigned int idx);
void fe_qosrslt_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_qosrslt_print_table_ut(void);

/* vlan table */
int fe_vlan_alloc_entry_ut(unsigned int *rslt_idx, unsigned int start_offset);
int fe_vlan_convert_sw_to_hw_data_ut(void *sw_entry, __u32 *p_data_array,
				     unsigned int size);
int fe_vlan_set_entry_ut(unsigned int idx, void *entry);
int fe_vlan_add_entry_ut(void *entry, unsigned int *rslt_idx);
int fe_vlan_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_vlan_del_entry_ut(void *entry, bool f_force);
int fe_vlan_find_entry_ut(void *entry, unsigned int *rslt_idx,
			  unsigned int start_offset);
int fe_vlan_get_entry_ut(unsigned int idx, void *entry);
int fe_vlan_inc_entry_refcnt_ut(unsigned int idx);
int fe_vlan_dec_entry_refcnt_ut(unsigned int idx);
int fe_vlan_get_entry_refcnt_ut(unsigned int idx, unsigned int *p_cnt);
int fe_vlan_set_field_ut(unsigned int idx, unsigned int field, __u32 *p_value);
int fe_vlan_get_field_ut(unsigned int idx, unsigned int field, __u32 *p_value);
int fe_vlan_flush_table_ut(void);
int fe_vlan_get_avail_count_ut(void);
void fe_vlan_print_entry_ut(unsigned int idx);
void fe_vlan_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_vlan_print_table_ut(void);

/* l2mac table */
int fe_l2mac_add_entry_ut(unsigned int idx, unsigned char *p_sa,
			  unsigned char *p_da);
int fe_l2mac_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_l2mac_get_entry_ut(unsigned int idx, void *entry);
int fe_l2mac_flush_table_ut(void);
int fe_l2mac_get_avail_count_ut(void);
void fe_l2mac_print_entry_ut(unsigned int idx);
void fe_l2mac_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_l2mac_print_table_ut(void);
int fe_l2mac_add_ut(unsigned char *p_sa, unsigned char *p_da,
		    unsigned int *p_idx);
int fe_l2mac_del_ut(unsigned int idx, bool f_sa, bool f_da);
int fe_l2mac_find_ut(unsigned char *p_sa, unsigned char *p_da,
		     unsigned int *p_idx);
int fe_l2mac_get_ut(unsigned int idx, unsigned char *p_sa, unsigned char *p_da);
int fe_l2mac_inc_refcnt_ut(unsigned int idx, bool f_sa, bool f_da);

/* l3ip table */
int fe_l3ip_add_entry_ut(unsigned int idx, unsigned char *p_sa,
			 unsigned char *p_da);
int fe_l3ip_del_entry_by_idx_ut(unsigned int idx, bool f_force);
int fe_l3ip_get_entry_ut(unsigned int idx, void *entry);
int fe_l3ip_flush_table_ut(void);
int fe_l3ip_get_avail_count_ut(void);
void fe_l3ip_print_entry_ut(unsigned int idx);
void fe_l3ip_print_range_ut(unsigned int start_idx, unsigned int end_idx);
void fe_l3ip_print_table_ut(void);
int fe_l3ip_add_ut(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6);
int fe_l3ip_del_ut(unsigned int idx, bool f_is_v6);
int fe_l3ip_find_ut(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6);
int fe_l3ip_get_ut(unsigned int idx, __u32 *p_ip_addr, bool f_is_v6);
int fe_l3ip_inc_refcnt_ut(unsigned int idx, bool f_is_v6);

/* end of extern functions */

/* global variables */

int (*cs_fe_ut_plan[FE_NUM_TEST_PLAN]) (void);

int start_idx = 1;
int end_idx = FE_NUM_TEST_PLAN + 1;
int end_step = 100;
/*
   level 0: only print "." and "pass"
		 1: print normal messages
		 2: print extra debug messages
 */
int level = 2;

/* Our parameters which can be set at load time. */

module_param(start_idx, int, S_IRUGO);
module_param(end_idx, int, S_IRUGO);
module_param(level, int, S_IRUGO);
module_param(end_step, int, S_IRUGO);

/* Utilities */

int cs_fe_reg_ut_plan(ut_plan_func_t func)
{
	int i;

	if (func == NULL) {
		DBG("Invalid func\n");
		return -EINVAL;
	}

	for (i = 0; i < FE_NUM_TEST_PLAN; i++) {
		if (cs_fe_ut_plan[i] != 0)
			continue;

		cs_fe_ut_plan[i] = func;
		break;
	}

	if (i >= FE_NUM_TEST_PLAN) {
		DBG("table full :%d\n", i);
		return -ENOMEM;
	}

	return 0;

}

int cs_fe_unreg_ut_plan(ut_plan_func_t func)
{
	int i;

	if (func == NULL) {
		DBG("Invalid func\n");
		return -EINVAL;
	}

	for (i = 0; i < FE_NUM_TEST_PLAN; i++) {
		if (cs_fe_ut_plan[i] == func) {
			cs_fe_ut_plan[i] = NULL;
			return 0;
		}
	}

	DBG("Can't find function :%08X\n", (unsigned int)func);

	return 0;

}

/* test plan and procedure */

static void fill_an_bng_mac_entry(int seed, fe_an_bng_mac_entry_t * ut_buff)
{
	ut_buff->mac[0] = 0;
	ut_buff->mac[1] = 0;
	ut_buff->mac[2] = 0;
	ut_buff->mac[3] = 0;
	ut_buff->mac[4] = 0;
	ut_buff->mac[5] = (seed + 1) & 0xFF;

	ut_buff->sa_da = seed % 2;
	ut_buff->pspid = seed % 16;
	ut_buff->pspid_mask = seed % 2;
	ut_buff->valid = 1;

	return;
}

int cs_fe_table_an_bng_mac_ut_basic(void)
{
	static fe_an_bng_mac_entry_t ut_entry[3];
	fe_an_bng_mac_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("AN BNG MAC", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_an_bng_mac_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_an_bng_mac_entry_t));
	for (i = 0; i < 3; i++) {
		fill_an_bng_mac_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_an_bng_mac_flush_table_ut());
	VERIFY_OP(fe_an_bng_mac_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_an_bng_mac_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_an_bng_mac_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_an_bng_mac_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_an_bng_mac_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_an_bng_mac_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_an_bng_mac_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_an_bng_mac_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_an_bng_mac_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);
	MESSAGE("\tMAC: %02x.%02x.%02x.%02x.%02x.%02x\n",
		ut_buff.mac[0], ut_buff.mac[1], ut_buff.mac[2],
		ut_buff.mac[3], ut_buff.mac[4], ut_buff.mac[5]);
	MESSAGE("\tSA_DA: %02x\n", ut_buff.sa_da);
	MESSAGE("\tPSPID: %02x\n", ut_buff.pspid);
	MESSAGE("\tPSPID MASK: %02x\n", ut_buff.pspid_mask);
	MESSAGE("\tValid: %02x\n\n", ut_buff.valid);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_an_bng_mac_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_an_bng_mac_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_an_bng_mac_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_an_bng_mac_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_an_bng_mac_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_an_bng_mac_set_field_ut(rslt_idx, FE_AN_BNG_MAC_PSPID,
					     &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_an_bng_mac_get_field_ut(rslt_idx, FE_AN_BNG_MAC_PSPID,
					     &rx_data));
	MESSAGE("== pspid: original (%d), target (%d), result (%d)\n",
		ut_buff.pspid, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.pspid = data;
	VERIFY_OP(fe_an_bng_mac_del_entry_ut(&ut_buff, false));
	SHOW(fe_an_bng_mac_print_table_ut());

	return 0;
}

int cs_fe_table_an_bng_mac_ut_boundary(void)
{
	fe_an_bng_mac_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("AN BNG MAC", "Boundary");

	VERIFY_OP(fe_an_bng_mac_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_an_bng_mac_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_AN_BNG_MAC_ENTRY_MAX; i++) {
		cond_resched();

		fill_an_bng_mac_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_an_bng_mac_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_an_bng_mac_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.mac[5] = FE_AN_BNG_MAC_ENTRY_MAX + 1;
	ret = fe_an_bng_mac_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE
		    ("Table is full and we can't add more that max. number of entries. ==> OK\n");
	} else {
		MESSAGE
		    ("Strange! We can add more than max. number of entries. ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_AN_BNG_MAC_ENTRY_MAX; i++) {
		cond_resched();

		fill_an_bng_mac_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_an_bng_mac_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_an_bng_mac_print_table_ut());

	return 0;
}

int cs_fe_table_an_bng_mac_ut_mix(void)
{
	fe_an_bng_mac_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("AN BNG MAC", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_an_bng_mac_entry_t));

	VERIFY_OP(fe_an_bng_mac_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_an_bng_mac_entry(i, &ut_buff);

		VERIFY_OP(fe_an_bng_mac_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_an_bng_mac_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_an_bng_mac_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_an_bng_mac_del_entry_by_idx_ut(7, false));

	SHOW(fe_an_bng_mac_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_an_bng_mac_entry(i, &ut_buff);

		VERIFY_OP(fe_an_bng_mac_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_an_bng_mac_print_table_ut());

	/* try to delete one nonexistent entry, but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_an_bng_mac_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_an_bng_mac_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_an_bng_mac_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_an_bng_mac_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_an_bng_mac_del_entry_by_idx_ut(13, false));

	SHOW(fe_an_bng_mac_print_table_ut());

	return 0;
}

static void fill_class_entry(int seed, fe_class_entry_t * ut_buff)
{
	unsigned int value;

	ut_buff->sdb_idx = seed % 64;
	ut_buff->rule_priority = seed % 64;
	ut_buff->entry_valid = 1;
	ut_buff->parity = 0;

	ut_buff->port.lspid = seed % 8;
	ut_buff->port.hdr_a_orig_lspid = seed % 8;
	switch (seed % 9) {
	case 0:
		value = FE_CLASS_FWDTYPE_NORMAL;
		break;
	case 1:
		value = FE_CLASS_FWDTYPE_CPU;
		break;
	case 2:
		value = FE_CLASS_FWDTYPE_MC;
		break;
	case 3:
		value = FE_CLASS_FWDTYPE_BC;
		break;
	case 4:
		value = FE_CLASS_FWDTYPE_UM;
		break;
	case 5:
		value = FE_CLASS_FWDTYPE_UU;
		break;
	case 6:
		value = FE_CLASS_FWDTYPE_MIRROR;
		break;
	case 7:
		value = FE_CLASS_FWDTYPE_BYPASS;
		break;
	case 8:
	default:
		value = FE_CLASS_FWDTYPE_DROP;
	}
	ut_buff->port.fwd_type = value;
	ut_buff->port.hdr_a_flags_crcerr = seed % 2;
	ut_buff->port.l3_csum_err = seed % 2;
	ut_buff->port.l4_csum_err = seed % 2;
	ut_buff->port.not_hdr_a_flags_stsvld = seed % 2;
	ut_buff->port.lspid_mask = seed % 2;
	ut_buff->port.hdr_a_orig_lspid_mask = seed % 2;
	ut_buff->port.fwd_type_mask = seed % 16;
	ut_buff->port.hdr_a_flags_crcerr_mask = seed % 2;
	ut_buff->port.l3_csum_err_mask = seed % 2;
	ut_buff->port.l4_csum_err_mask = seed % 2;
	ut_buff->port.not_hdr_a_flags_stsvld_mask = seed % 2;
	ut_buff->port.mcgid = seed % 32 | (0x1 << 8);
	ut_buff->port.mcgid_mask = seed % 32 | (0x1 << 8);

	ut_buff->l2.tpid_enc_1 = seed % 8;
	ut_buff->l2.vid_1 = seed % 4096;
	ut_buff->l2._8021p_1 = seed % 8;
	ut_buff->l2.tpid_enc_2 = seed % 8;
	ut_buff->l2.vid_2 = seed % 4096;
	ut_buff->l2._8021p_2 = seed % 8;
	ut_buff->l2.tpid_enc_1_msb_mask = seed % 2;
	ut_buff->l2.tpid_enc_1_lsb_mask = seed % 2;
	ut_buff->l2.vid_1_mask = seed % 2;
	ut_buff->l2._8021p_1_mask = seed % 2;
	ut_buff->l2.tpid_enc_2_msb_mask = seed % 2;
	ut_buff->l2.tpid_enc_2_lsb_mask = seed % 2;
	ut_buff->l2.vid_2_mask = seed % 2;
	ut_buff->l2._8021p_2_mask = seed % 2;
	ut_buff->l2.da_an_mac_sel = seed % 16;
	ut_buff->l2.da_an_mac_hit = seed % 2;
	ut_buff->l2.sa_bng_mac_sel = seed % 16;
	ut_buff->l2.sa_bng_mac_hit = seed % 2;
	ut_buff->l2.da_an_mac_sel_mask = seed % 2;
	ut_buff->l2.da_an_mac_hit_mask = seed % 2;
	ut_buff->l2.sa_bng_mac_sel_mask = seed % 2;
	ut_buff->l2.sa_bng_mac_hit_mask = seed % 2;
	ut_buff->l2.ethertype_enc = seed % 64;
	ut_buff->l2.ethertype_enc_mask = seed % 2;
	ut_buff->l2.da[5] = seed + 1;
	ut_buff->l2.sa[5] = seed + 0x71;
	ut_buff->l2.da_mask = seed % 64;
	ut_buff->l2.sa_mask = seed % 64;
	ut_buff->l2.mcast_da = seed % 2;
	ut_buff->l2.bcast_da = seed % 2;
	ut_buff->l2.mcast_da_mask = seed % 2;
	ut_buff->l2.bcast_da_mask = seed % 2;
	ut_buff->l2.len_encoded = seed % 2;
	ut_buff->l2.len_encoded_mask = seed % 2;

	ut_buff->l3.dscp = seed % 64;
	ut_buff->l3.ecn = seed % 4;
	ut_buff->l3.ip_prot = seed % 256;
	ut_buff->l3.sa[3] = seed + 1;
	ut_buff->l3.da[3] = seed + 0x71;
	ut_buff->l3.ip_valid = seed % 2;
	ut_buff->l3.ip_ver = seed % 2;
	ut_buff->l3.ip_frag = seed % 2;
	ut_buff->l3.dscp_mask = seed % 64;
	ut_buff->l3.ecn_mask = seed % 4;
	ut_buff->l3.ip_prot_mask = seed % 2;
	ut_buff->l3.ip_sa_mask = seed % 33;
	ut_buff->l3.ip_da_mask = seed % 33;
	ut_buff->l3.ip_valid_mask = seed % 2;
	ut_buff->l3.ip_ver_mask = seed % 2;
	ut_buff->l3.ip_frag_mask = seed % 2;
	ut_buff->l3.spi = seed + 100;
	ut_buff->l3.spi_valid = seed % 2;
	ut_buff->l3.spi_mask = seed % 2;
	ut_buff->l3.spi_valid_mask = seed % 2;

	ut_buff->l4.l4_sp = seed + 4000;
	ut_buff->l4.l4_dp = seed + 2000;
	ut_buff->l4.l4_valid = seed % 2;
	ut_buff->l4.l4_port_mask = seed % 4;
	ut_buff->l4.l4_valid_mask = seed % 2;

	return;
}

int cs_fe_table_class_ut_basic(void)
{
	static fe_class_entry_t ut_entry[3];
	fe_class_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("class", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_class_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_class_entry_t));
	for (i = 0; i < 3; i++) {
		fill_class_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_class_flush_table_ut());
	VERIFY_OP(fe_class_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_class_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_class_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_class_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_class_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_class_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_class_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_class_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_class_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* General */
	MESSAGE("  |- General: SDB Idx %02x\n", ut_buff.sdb_idx);
	MESSAGE("\trule_priority %01x\n", ut_buff.rule_priority);
	MESSAGE("\tentry_valid %01x\n", ut_buff.entry_valid);
	MESSAGE("\tParity %01x\n", ut_buff.parity);

	/* Port */
	MESSAGE("  |- Port: LSPID %02x, Mask %02x\n",
		ut_buff.port.lspid, ut_buff.port.lspid_mask);
	MESSAGE("\tHDR_A_ORIG_LSPID %02x, Mask %02x\n",
		ut_buff.port.hdr_a_orig_lspid,
		ut_buff.port.hdr_a_orig_lspid_mask);
	MESSAGE("\tFE_TYPE %02x, Mask %02x\n", ut_buff.port.fwd_type,
		ut_buff.port.fwd_type_mask);
	MESSAGE("\tHDR_A_FLAGS_CRCERR %02x, Mask %02x\n",
		ut_buff.port.hdr_a_flags_crcerr,
		ut_buff.port.hdr_a_flags_crcerr_mask);
	MESSAGE("\tL3_CSUM_ERR %02x, Mask %02x\n", ut_buff.port.l3_csum_err,
		ut_buff.port.l3_csum_err_mask);
	MESSAGE("\tL4_CSUM_ERR %02x, Mask %02x\n", ut_buff.port.l4_csum_err,
		ut_buff.port.l4_csum_err_mask);
	MESSAGE("\tNOT HDR A FLAGS STSVLD %02x, Mask %02x\n",
		ut_buff.port.not_hdr_a_flags_stsvld,
		ut_buff.port.not_hdr_a_flags_stsvld_mask);
	MESSAGE("\tMCG ID %02x, Mask %02x\n", ut_buff.port.mcgid,
		ut_buff.port.mcgid_mask);

	/* L2 */
	MESSAGE("  |- L2: TPID#1 %02x, MSB Mask %02x, LSB Mask %02x\n",
		ut_buff.l2.tpid_enc_1, ut_buff.l2.tpid_enc_1_msb_mask,
		ut_buff.l2.tpid_enc_1_lsb_mask);
	MESSAGE("\tVID#1 %04x, MASK %02x\n",
		ut_buff.l2.vid_1, ut_buff.l2.vid_1_mask);
	MESSAGE("\t8021P#1 %02x, MASK %02x",
		ut_buff.l2._8021p_1, ut_buff.l2._8021p_1_mask);
	MESSAGE("\tTPID#2 %02x, MSB Mask %02x, LSB Mask %02x\n",
		ut_buff.l2.tpid_enc_2, ut_buff.l2.tpid_enc_2_msb_mask,
		ut_buff.l2.tpid_enc_2_lsb_mask);
	MESSAGE("\tVID#2 %04x, MASK %02x\n",
		ut_buff.l2.vid_2, ut_buff.l2.vid_2_mask);
	MESSAGE("\t8021P#2 %02x, MASK %02x\n",
		ut_buff.l2._8021p_2, ut_buff.l2._8021p_2_mask);
	MESSAGE("\tDA AN MAC SEL %02x, MASK %02x\n",
		ut_buff.l2.da_an_mac_sel, ut_buff.l2.da_an_mac_sel_mask);
	MESSAGE("\tDA AN MAC HIT %02x, MASK %02x\n",
		ut_buff.l2.da_an_mac_hit, ut_buff.l2.da_an_mac_hit_mask);
	MESSAGE("\tSA BNG MAC SEL %02x, MASK %02x\n",
		ut_buff.l2.sa_bng_mac_sel, ut_buff.l2.sa_bng_mac_sel_mask);
	MESSAGE("\tSA BNG MAC HIT %02x, MASK %02x\n",
		ut_buff.l2.sa_bng_mac_hit, ut_buff.l2.sa_bng_mac_hit_mask);
	MESSAGE("\tEtherType_enc %02x, MASK %02x\n",
		ut_buff.l2.ethertype_enc, ut_buff.l2.ethertype_enc_mask);
	MESSAGE("\tDA %02x.%02x.%02x.%02x.%02x.%02x, Mask %02x\n",
		ut_buff.l2.da[0], ut_buff.l2.da[1], ut_buff.l2.da[2],
		ut_buff.l2.da[3], ut_buff.l2.da[4], ut_buff.l2.da[5],
		ut_buff.l2.da_mask);
	MESSAGE("\tSA %02x.%02x.%02x.%02x.%02x.%02x, Mask %02x\n",
		ut_buff.l2.sa[0], ut_buff.l2.sa[1], ut_buff.l2.sa[2],
		ut_buff.l2.sa[3], ut_buff.l2.sa[4], ut_buff.l2.sa[5],
		ut_buff.l2.sa_mask);

	MESSAGE("\tMCAST DA %02x, Mask %02x\n",
		ut_buff.l2.mcast_da, ut_buff.l2.mcast_da_mask);
	MESSAGE("\tBCAST DA %02x, Mask %02x\n",
		ut_buff.l2.bcast_da, ut_buff.l2.bcast_da_mask);
	MESSAGE("\tLEN encoded %02x, Mask %02x\n",
		ut_buff.l2.len_encoded, ut_buff.l2.len_encoded_mask);

	/* L3 */
	MESSAGE("  |- L3: DSCP %02x, MASK %02x\n",
		ut_buff.l3.dscp, ut_buff.l3.dscp_mask);
	MESSAGE("\tECN %02x, MASK %02x\n", ut_buff.l3.ecn, ut_buff.l3.ecn_mask);
	MESSAGE("\tIP Prot %02x, MASK %02x\n",
		ut_buff.l3.ip_prot, ut_buff.l3.ip_prot_mask);
	MESSAGE("\tSA %08x.%08x.%08x.%08x, Mask %04x\n",
		ut_buff.l3.sa[0], ut_buff.l3.sa[1], ut_buff.l3.sa[2],
		ut_buff.l3.sa[3], ut_buff.l3.ip_sa_mask);
	MESSAGE("\tDA %08x.%08x.%08x.%08x, MASK %04x\n",
		ut_buff.l3.da[0], ut_buff.l3.da[1], ut_buff.l3.da[2],
		ut_buff.l3.da[3], ut_buff.l3.ip_da_mask);
	MESSAGE("\tIP Valid %02x, MASK %02x\n",
		ut_buff.l3.ip_valid, ut_buff.l3.ip_valid_mask);
	MESSAGE("\tIP Ver %02x, MASK %02x\n",
		ut_buff.l3.ip_ver, ut_buff.l3.ip_ver_mask);
	MESSAGE("\tIP Frag %02x, MASK %02x\n",
		ut_buff.l3.ip_frag, ut_buff.l3.ip_frag_mask);
	MESSAGE("\tSPI %08x, MASK %02x\n", ut_buff.l3.spi, ut_buff.l3.spi_mask);
	MESSAGE("\tSPI Valid %02x, MASK %02x\n",
		ut_buff.l3.spi_valid, ut_buff.l3.spi_valid_mask);

	/* L4 */
	MESSAGE("  |- L4: Src Port %04x, Dst Port %04x, Mask %02x\n",
		ut_buff.l4.l4_sp, ut_buff.l4.l4_dp, ut_buff.l4.l4_port_mask);
	MESSAGE("\tL4 Valid %02x, Mask %02x\n",
		ut_buff.l4.l4_valid, ut_buff.l4.l4_valid_mask);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_class_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_class_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_class_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_class_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_class_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 10;
	VERIFY_OP(fe_class_set_field_ut(rslt_idx, FE_CLASS_RULE_PRI, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_class_get_field_ut(rslt_idx, FE_CLASS_RULE_PRI, &rx_data));
	MESSAGE("== rule_priority: original (%d), target (%d), result (%d)\n",
		ut_buff.rule_priority, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.rule_priority = data;
	VERIFY_OP(fe_class_del_entry_ut(&ut_buff, false));
	SHOW(fe_class_print_table_ut());

	return 0;
}

int cs_fe_table_class_ut_boundary(void)
{
	fe_class_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("class", "Boundary");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_class_entry_t));

	VERIFY_OP(fe_class_flush_table_ut());

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_CLASS_ENTRY_MAX; i++) {
		cond_resched();

		fill_class_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_class_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_class_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.l2.vid_1 = FE_CLASS_ENTRY_MAX + 1;
	ret = fe_class_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_CLASS_ENTRY_MAX; i++) {
		cond_resched();

		fill_class_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_class_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_class_print_table_ut());

	return 0;
}

int cs_fe_table_class_ut_mix(void)
{
	fe_class_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("class", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_class_entry_t));

	VERIFY_OP(fe_class_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_class_entry(i, &ut_buff);

		VERIFY_OP(fe_class_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_class_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_class_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_class_del_entry_by_idx_ut(7, false));

	SHOW(fe_class_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_class_entry(i, &ut_buff);

		VERIFY_OP(fe_class_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_class_print_table_ut());

	/* try to delete one nonexistent entry, 
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_class_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	fill_class_entry(2 /* we already delete it before */ , &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_class_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_class_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_class_del_entry_by_idx_ut(13, false));

	SHOW(fe_class_print_table_ut());

	return 0;
}

static void fill_fwdrslt_entry(int seed, fe_fwd_result_entry_t * ut_buff)
{
	unsigned int value;

	ut_buff->acl_dsbl = seed % 2;
	ut_buff->parity = 0;

	ut_buff->l2.mac_sa_replace_en = seed % 2;
	ut_buff->l2.mac_da_replace_en = seed % 2;
	ut_buff->l2.l2_index = seed % 512;
	ut_buff->l2.mcgid = seed % 32 | (0x1 << 8);
	ut_buff->l2.mcgid_valid = seed % 2;
	ut_buff->l2.flow_vlan_op_en = seed % 2;
	ut_buff->l2.flow_vlan_index = seed % 512;
	ut_buff->l2.pppoe_encap_en = seed % 2;
	ut_buff->l2.pppoe_decap_en = seed % 2;

	ut_buff->l3.ip_sa_replace_en = seed % 2;
	ut_buff->l3.ip_da_replace_en = seed % 2;
	ut_buff->l3.ip_sa_index = seed % 4096;
	ut_buff->l3.ip_da_index = seed % 4096;
	ut_buff->l3.decr_ttl_hoplimit = seed % 2;

	ut_buff->l4.sp_replace_en = seed % 2;
	ut_buff->l4.dp_replace_en = seed % 2;
	ut_buff->l4.sp = seed + 1001;
	ut_buff->l4.dp = seed + 2001;

	ut_buff->dest.pol_policy = seed % 2;
	ut_buff->dest.voq_policy = seed % 2;
	ut_buff->dest.voq_pol_table_index = seed % 512;

	ut_buff->act.fwd_type_valid = seed % 2;
	switch (seed % 9) {
	case 0:
		value = FE_CLASS_FWDTYPE_NORMAL;
		break;
	case 1:
		value = FE_CLASS_FWDTYPE_CPU;
		break;
	case 2:
		value = FE_CLASS_FWDTYPE_MC;
		break;
	case 3:
		value = FE_CLASS_FWDTYPE_BC;
		break;
	case 4:
		value = FE_CLASS_FWDTYPE_UM;
		break;
	case 5:
		value = FE_CLASS_FWDTYPE_UU;
		break;
	case 6:
		value = FE_CLASS_FWDTYPE_MIRROR;
		break;
	case 7:
		value = FE_CLASS_FWDTYPE_BYPASS;
		break;
	case 8:
	default:
		value = FE_CLASS_FWDTYPE_DROP;
	}
	ut_buff->act.fwd_type = value;
	ut_buff->act.drop = seed % 2;

	return;
}

int cs_fe_table_fwdrslt_ut_basic(void)
{
	static fe_fwd_result_entry_t ut_entry[3];
	fe_fwd_result_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("fwdrslt", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_fwd_result_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_fwd_result_entry_t));
	for (i = 0; i < 3; i++) {
		fill_fwdrslt_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_fwdrslt_flush_table_ut());
	VERIFY_OP(fe_fwdrslt_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_fwdrslt_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_fwdrslt_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_fwdrslt_print_entry_ut(rslt_idx));

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_fwdrslt_add_entry_ut(&ut_entry[2], &rslt_idx));
	SHOW(fe_fwdrslt_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_fwdrslt_find_entry_ut(&ut_entry[2], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_fwdrslt_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_fwdrslt_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* General */
	MESSAGE("  |- ACL Dsbl %01x\n", ut_buff.acl_dsbl);

	/* L2 */
	MESSAGE("  |- L2 Info SA_EN: %01x, DA_EN: %01x, L2 IDX: %x\n",
		ut_buff.l2.mac_sa_replace_en, ut_buff.l2.mac_da_replace_en,
		ut_buff.l2.l2_index);
	MESSAGE("\tMCG_ID: %x, MCG_ID_VLD: %01x\n",
		ut_buff.l2.mcgid, ut_buff.l2.mcgid_valid);
	MESSAGE("\tFVLAN_OP_EN: %01x, FVLAN_IDX: %x\n",
		ut_buff.l2.flow_vlan_op_en, ut_buff.l2.flow_vlan_index);
	MESSAGE("\tPPPoE_Encap_en: %01x, PPPoE_Decap_en: %01x\n",
		ut_buff.l2.pppoe_encap_en, ut_buff.l2.pppoe_decap_en);

	/* L3 */
	MESSAGE(" |- L3 Info decr_ttl: %01x, IP_SA Rep_EN: %01x, Index: %x\n",
		ut_buff.l3.decr_ttl_hoplimit, ut_buff.l3.ip_sa_replace_en,
		ut_buff.l3.ip_sa_index);
	MESSAGE("\tIP_DA Rep_EN: %01x, Index: %x\n",
		ut_buff.l3.ip_da_replace_en, ut_buff.l3.ip_da_index);

	/* L4 */
	MESSAGE(" |- L4 Info SP_Rep_EN: %01x, SrcPort: %x\n",
		ut_buff.l4.sp_replace_en, ut_buff.l4.sp);
	MESSAGE("\tDP_Rep_EN: %01x, DstPort: %x\n",
		ut_buff.l4.dp_replace_en, ut_buff.l4.dp);

	/* Dest */
	MESSAGE(" |- Destination Info: Pol_policy: %01x ",
		ut_buff.dest.pol_policy);
	MESSAGE("\tVOQ_Pol: %01x, VOQ_Pol_Table_index: %x\n",
		ut_buff.dest.voq_policy, ut_buff.dest.voq_pol_table_index);

	/* Action */
	MESSAGE(" |- Action Info: Fwd type valid: %01x, Fwd_type: %x\n",
		ut_buff.act.fwd_type_valid, ut_buff.act.fwd_type);
	MESSAGE("\tDrop: %01x\n", ut_buff.act.drop);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_fwdrslt_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_fwdrslt_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_fwdrslt_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_fwdrslt_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_fwdrslt_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 10;
	VERIFY_OP(fe_fwdrslt_set_field_ut(rslt_idx, FWD_L2_IDX, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_fwdrslt_get_field_ut(rslt_idx, FWD_L2_IDX, &rx_data));
	MESSAGE("== L2 IDX: original (%d), target (%d), result (%d)\n",
		ut_buff.l2.l2_index, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.l2.l2_index = data;
	VERIFY_OP(fe_fwdrslt_del_entry_ut(&ut_buff, false));
	SHOW(fe_fwdrslt_print_entry_ut(rslt_idx));

	return 0;
}

int cs_fe_table_fwdrslt_ut_boundary(void)
{
	fe_fwd_result_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("fwdrslt", "Boundary");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_fwd_result_entry_t));

	VERIFY_OP(fe_fwdrslt_flush_table_ut());

	/* insert max. number of entries */
	/* entry #0 is reserved */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 1; i < FE_FWD_RESULT_ENTRY_MAX; i++) {
		cond_resched();

		fill_fwdrslt_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_fwdrslt_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_fwdrslt_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.l2.l2_index = (FE_FWD_RESULT_ENTRY_MAX + 1) % 512;
	ret = fe_fwdrslt_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 1; i < FE_FWD_RESULT_ENTRY_MAX; i++) {
		cond_resched();

		fill_fwdrslt_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_fwdrslt_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_fwdrslt_print_table_ut());

	return 0;
}

int cs_fe_table_fwdrslt_ut_mix(void)
{
	fe_fwd_result_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("fwdrslt", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_fwd_result_entry_t));

	VERIFY_OP(fe_fwdrslt_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_fwdrslt_entry(i, &ut_buff);

		VERIFY_OP(fe_fwdrslt_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_fwdrslt_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_fwdrslt_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_fwdrslt_del_entry_by_idx_ut(7, false));

	SHOW(fe_fwdrslt_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_fwdrslt_entry(i, &ut_buff);

		VERIFY_OP(fe_fwdrslt_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_fwdrslt_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_fwdrslt_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	fill_fwdrslt_entry(1 /* we already delete it before */ , &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_fwdrslt_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk
	    ("====== Step 05. delete the #1 (first) and #14 (last) entries\n");

	VERIFY_OP(fe_fwdrslt_del_entry_by_idx_ut(1, false));
	VERIFY_OP(fe_fwdrslt_del_entry_by_idx_ut(14, false));

	SHOW(fe_fwdrslt_print_table_ut());

	return 0;
}

static void fill_lpb_entry(int seed, fe_lpb_entry_t * ut_buff)
{
	ut_buff->lspid = seed % 8;
	ut_buff->pvid = seed % 4096;
	ut_buff->pvid_tpid_enc = seed % 4;
	ut_buff->olspid_en = seed % 2;
	ut_buff->olspid = seed % 8;
	ut_buff->olspid_preserve_en = seed % 2;
	ut_buff->parity = 0;

	return;
}

int cs_fe_table_lpb_ut_basic(void)
{
	static fe_lpb_entry_t ut_entry[3];
	fe_lpb_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("lpb", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_lpb_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_lpb_entry_t));
	for (i = 0; i < 3; i++) {
		fill_lpb_entry(i, &ut_entry[i]);
	}

	/* set an entry */
	printk("====== Step 01. set entry#2\n");
	VERIFY_OP(fe_lpb_set_entry_ut(2, &ut_entry[2]));
	SHOW(fe_lpb_print_entry_ut(2));

	/* find an entry */
	printk("====== Step 02. find an entry\n");
	VERIFY_OP(fe_lpb_find_entry_ut(&ut_entry[2], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);
	if (rslt_idx != 2) {
		MESSAGE("find another entry with the same key ==> NOK\n");
		return -1;
	}

	/* get an entry */
	printk("====== Step 03. get an entry by index\n");
	VERIFY_OP(fe_lpb_get_entry_ut(2, &ut_buff));

	MESSAGE("== index: %04d\n", rslt_idx);
	MESSAGE("\tLSPID: %02x\n", ut_buff.lspid);
	MESSAGE("\tPVID: %04x\n", ut_buff.pvid);
	MESSAGE("\tPVID_TPID_ENC: %02x\n", ut_buff.pvid_tpid_enc);
	MESSAGE("\tOLSPID_EN: %02x\n", ut_buff.olspid_en);
	MESSAGE("\tOLSPID: %02x\n", ut_buff.olspid);
	MESSAGE("\tOLSPID_PRESERVE_EN: %02x\n", ut_buff.olspid_preserve_en);
	MESSAGE("\tMEM_PARITY: %02x\n", ut_buff.parity);

	/* set one field of the entry */
	printk("====== Step 04. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_lpb_set_field_ut(2, FE_LPB_LSPID, &data));

	/* get one field of the entry */
	printk("====== Step 05. get one field of the entry by index\n");
	VERIFY_OP(fe_lpb_get_field_ut(2, FE_LPB_LSPID, &rx_data));
	MESSAGE("== LSPID: original (%d), target (%d), result (%d)\n",
		ut_buff.lspid, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	return 0;
}

int cs_fe_table_lpb_ut_boundary(void)
{
	fe_lpb_entry_t ut_buff;
	unsigned int i, ret;

	PRINT_CURR_STAGE("lpb", "Boundary");

	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_lpb_entry_t));

	/* set max. number of entries */
	printk("====== Step 01. set max. number of entries\n");
	for (i = 0; i < FE_LPB_ENTRY_MAX; i++) {
		cond_resched();

		fill_lpb_entry(i, &ut_buff);

		MESSAGE("### set entry#%d\n", i);

		VERIFY_OP(fe_lpb_set_entry_ut(i, &ut_buff));
	}
	SHOW(fe_lpb_print_table_ut());

	return 0;
}

int cs_fe_table_lpb_ut_mix(void)
{
	fe_lpb_entry_t ut_buff;
	unsigned int i, ret, rslt_idx;

	PRINT_CURR_STAGE("lpb", "Mix");

	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_lpb_entry_t));

	/* set max. number of entries */
	printk("====== Step 01. set max. number of entries\n");
	for (i = 0; i < FE_LPB_ENTRY_MAX; i++) {
		cond_resched();

		fill_lpb_entry(i, &ut_buff);

		MESSAGE("### set entry#%d\n", i);

		VERIFY_OP(fe_lpb_set_entry_ut(i, &ut_buff));
	}
	SHOW(fe_lpb_print_table_ut());

	/* find an entry */
	printk("====== Step 02. find an entry\n");
	fill_lpb_entry(5, &ut_buff);
	VERIFY_OP(fe_lpb_find_entry_ut(&ut_buff, &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);
	if (rslt_idx != 5) {
		MESSAGE("find another entry with the same key ==> NOK\n");
		return -1;
	}

	/* get an entry */
	printk("====== Step 03. get an entry by index\n");
	VERIFY_OP(fe_lpb_get_entry_ut(rslt_idx, &ut_buff));

	MESSAGE("== index: %04d\n", rslt_idx);
	MESSAGE("\tLSPID: %02x\n", ut_buff.lspid);
	MESSAGE("\tPVID: %04x\n", ut_buff.pvid);
	MESSAGE("\tPVID_TPID_ENC: %02x\n", ut_buff.pvid_tpid_enc);
	MESSAGE("\tOLSPID_EN: %02x\n", ut_buff.olspid_en);
	MESSAGE("\tOLSPID: %02x\n", ut_buff.olspid);
	MESSAGE("\tOLSPID_PRESERVE_EN: %02x\n", ut_buff.olspid_preserve_en);
	MESSAGE("\tMEM_PARITY: %02x\n", ut_buff.parity);

	/* set an entry */
	printk("====== Step 04. set entry#7 as the same as #6\n");
	fill_lpb_entry(6, &ut_buff);
	VERIFY_OP(fe_lpb_set_entry_ut(7, &ut_buff));
	SHOW(fe_lpb_print_entry_ut(7));

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_lpb_get_entry_ut(7, &ut_buff));

	MESSAGE("== index: %04d\n", rslt_idx);
	MESSAGE("\tLSPID: %02x\n", ut_buff.lspid);
	MESSAGE("\tPVID: %04x\n", ut_buff.pvid);
	MESSAGE("\tPVID_TPID_ENC: %02x\n", ut_buff.pvid_tpid_enc);
	MESSAGE("\tOLSPID_EN: %02x\n", ut_buff.olspid_en);
	MESSAGE("\tOLSPID: %02x\n", ut_buff.olspid);
	MESSAGE("\tOLSPID_PRESERVE_EN: %02x\n", ut_buff.olspid_preserve_en);
	MESSAGE("\tMEM_PARITY: %02x\n", ut_buff.parity);

	return 0;
}

static void fill_sdb_entry(int seed, fe_sdb_entry_t * ut_buff)
{
	int i;

	ut_buff->lpm_en = seed % 2;
	ut_buff->parity = 0;

	for (i = 0; i < 8; i++) {
		ut_buff->sdb_tuple[i].mask_ptr = seed % 64;
		ut_buff->sdb_tuple[i].priority = seed % 16;
		ut_buff->sdb_tuple[i].enable = seed % 2;
	}

	for (i = 0; i < 2; i++) {
		ut_buff->sdb_lpm_v4[i].start_ptr = seed % 64;
		ut_buff->sdb_lpm_v4[i].end_ptr = seed % 64;
		ut_buff->sdb_lpm_v4[i].lpm_ptr_en = seed % 2;

		ut_buff->sdb_lpm_v6[i].start_ptr = seed % 64;
		ut_buff->sdb_lpm_v6[i].end_ptr = seed % 64;
		ut_buff->sdb_lpm_v6[i].lpm_ptr_en = seed % 2;
	}

	ut_buff->pvid.pvid = seed % 4096;
	ut_buff->pvid.pvid_tpid_enc = seed % 4;
	ut_buff->pvid.pvid_en = seed % 2;

	ut_buff->vlan.vlan_ingr_membership_en = seed % 2;
	ut_buff->vlan.vlan_egr_membership_en = seed % 2;
	ut_buff->vlan.vlan_egr_untag_chk_en = seed % 2;

	ut_buff->misc.use_egrlen_pkttype_policer = seed % 2;
	ut_buff->misc.use_egrlen_src_policer = seed % 2;
	ut_buff->misc.use_egrlen_flow_policer = seed % 2;
	ut_buff->misc.ttl_hop_limit_zero_discard_en = seed % 2;
	ut_buff->misc.key_rule = seed % 64;
	ut_buff->misc.uu_flowidx = seed % 8192;
	ut_buff->misc.hash_sts_update_ctrl = seed % 4;
	ut_buff->misc.bc_flowidx = seed % 8192;
	ut_buff->misc.um_flowidx = seed % 8192;
	ut_buff->misc.rsvd_202 = 0;
	ut_buff->misc.drop = seed % 2;
	ut_buff->misc.egr_vln_ingr_mbrshp_en = seed % 2;
	ut_buff->misc.acl_dsbl = seed % 2;

	return;
}

int cs_fe_table_sdb_ut_basic(void)
{
	static fe_sdb_entry_t ut_entry[3];
	fe_sdb_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("sdb", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_sdb_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_sdb_entry_t));
	for (i = 0; i < 3; i++) {
		fill_sdb_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_sdb_flush_table_ut());
	VERIFY_OP(fe_sdb_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_sdb_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_sdb_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_sdb_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_sdb_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_sdb_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_sdb_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_sdb_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_sdb_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	for (i = 0; i < 8; i++) {
		/* Tuples */
		MESSAGE("\tTUPLE#%d_EN: %01x, MASK_PTR: %04x, PRI: %01x\n",
			i, ut_buff.sdb_tuple[i].enable,
			ut_buff.sdb_tuple[i].mask_ptr,
			ut_buff.sdb_tuple[i].priority);
	}

	/* LPM */
	MESSAGE("\tLPM EN: %01x\n", ut_buff.lpm_en);

	for (i = 0; i < 2; i++) {
		/* IPv4 PTR */
		MESSAGE("\tIPv4 PTR#%d, EN: %01x, Start: %02x, End: %02x\n",
			i, ut_buff.sdb_lpm_v4[i].lpm_ptr_en,
			ut_buff.sdb_lpm_v4[i].start_ptr,
			ut_buff.sdb_lpm_v4[i].end_ptr);
	}

	for (i = 0; i < 2; i++) {
		/* IPv6 PTR */
		MESSAGE("\tIPv6 PTR#%d, EN: %01x, Start: %02x, End: %02x\n",
			i, ut_buff.sdb_lpm_v6[i].lpm_ptr_en,
			ut_buff.sdb_lpm_v6[i].start_ptr,
			ut_buff.sdb_lpm_v6[i].end_ptr);
	}

	/* PVID */
	MESSAGE("\tPVID: %04x, EN: %01x, TPID ENC: %01x\n",
		ut_buff.pvid.pvid, ut_buff.pvid.pvid_en,
		ut_buff.pvid.pvid_tpid_enc);

	/* VLAN */
	MESSAGE("\tVLAN: INGR_MBRSHP_EN: %01x, EGR_MBRSHP_EN: %01x, "
		"EGR_UNTAG_CHK_EN: %01x\n",
		ut_buff.vlan.vlan_ingr_membership_en,
		ut_buff.vlan.vlan_egr_membership_en,
		ut_buff.vlan.vlan_egr_untag_chk_en);

	/* Miscell */
	MESSAGE("\tMISC: PKTTYPE_POLICER: %01x, SRC_POLICER: %01x, "
		"FLOW_POLICER: %01x\n",
		ut_buff.misc.use_egrlen_pkttype_policer,
		ut_buff.misc.use_egrlen_src_policer,
		ut_buff.misc.use_egrlen_flow_policer);
	MESSAGE("\tHOPLIMIT_ZERO_DISCARD_EN: %01x\n",
		ut_buff.misc.ttl_hop_limit_zero_discard_en);
	MESSAGE("\tSDB Key Rule: %02x\n", ut_buff.misc.key_rule);
	MESSAGE("\tUU FlowIdx: %04x, BC FlowIdx: %04x, UM FlowIdx: %04x\n",
		ut_buff.misc.uu_flowidx, ut_buff.misc.bc_flowidx,
		ut_buff.misc.um_flowidx);
	MESSAGE("\tHash Status Update Ctrl: %02x, RSVD_202: %01x, DROP: %01x\n",
		ut_buff.misc.hash_sts_update_ctrl, ut_buff.misc.rsvd_202,
		ut_buff.misc.drop);
	MESSAGE("\tEGRVLN_INGR_MBRSHP_EN: %01x, ACL DISABLE: %01x\n",
		ut_buff.misc.egr_vln_ingr_mbrshp_en, ut_buff.misc.acl_dsbl);
	MESSAGE("\tMEM PARITY: %01x\n", ut_buff.parity);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_sdb_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_sdb_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_sdb_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_sdb_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_sdb_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_sdb_set_field_ut(rslt_idx, FE_SDB_PVID, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_sdb_get_field_ut(rslt_idx, FE_SDB_PVID, &rx_data));
	MESSAGE("== PVID: original (%d), target (%d), result (%d)\n",
		ut_buff.pvid.pvid, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.pvid.pvid = data;
	VERIFY_OP(fe_sdb_del_entry_ut(&ut_buff, false));
	SHOW(fe_sdb_print_table_ut());

	return 0;
}

int cs_fe_table_sdb_ut_boundary(void)
{
	fe_sdb_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("sdb", "Boundary");

	VERIFY_OP(fe_sdb_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_sdb_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_SDB_ENTRY_MAX; i++) {
		cond_resched();

		fill_sdb_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_sdb_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_sdb_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.pvid.pvid = FE_SDB_ENTRY_MAX + 1;
	ret = fe_sdb_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_SDB_ENTRY_MAX; i++) {
		cond_resched();

		fill_sdb_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_sdb_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_sdb_print_table_ut());

	return 0;
}

int cs_fe_table_sdb_ut_mix(void)
{
	fe_sdb_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("sdb", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_sdb_entry_t));

	VERIFY_OP(fe_sdb_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_sdb_entry(i, &ut_buff);

		VERIFY_OP(fe_sdb_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_sdb_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_sdb_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_sdb_del_entry_by_idx_ut(7, false));

	SHOW(fe_sdb_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_sdb_entry(i, &ut_buff);

		VERIFY_OP(fe_sdb_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_sdb_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_sdb_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_sdb_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_sdb_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #0 and #13 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_sdb_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_sdb_del_entry_by_idx_ut(13, false));

	SHOW(fe_sdb_print_table_ut());

	return 0;
}

static void fill_voqpol_entry(int seed, fe_voq_pol_entry_t * ut_buff)
{
	ut_buff->voq_base = seed % 112;
	ut_buff->pol_base = seed % 256;
	ut_buff->cpu_pid = seed % 8;
	ut_buff->ldpid = seed % 8;
	ut_buff->pppoe_session_id = seed + 1000;
	ut_buff->cos_nop = seed % 2;
	ut_buff->parity = 0;

	return;
}

int cs_fe_table_voqpol_ut_basic(void)
{
	static fe_voq_pol_entry_t ut_entry[3];
	fe_voq_pol_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("voqpol", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_voq_pol_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_voq_pol_entry_t));
	for (i = 0; i < 3; i++) {
		fill_voqpol_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_voqpol_flush_table_ut());
	VERIFY_OP(fe_voqpol_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_voqpol_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_voqpol_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_voqpol_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_voqpol_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_voqpol_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_voqpol_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_voqpol_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_voqpol_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	MESSAGE("\tVOQ Base: %02x\n", ut_buff.voq_base);
	MESSAGE("\tPOL Base: %02x\n", ut_buff.pol_base);
	MESSAGE("\tCPU PID: %02x\n", ut_buff.cpu_pid);
	MESSAGE("\tLDPID: %02x\n", ut_buff.ldpid);
	MESSAGE("\tPPPOE Session ID: %02x\n", ut_buff.pppoe_session_id);
	MESSAGE("\tCOS NOP: %02x\n", ut_buff.cos_nop);
	MESSAGE("\tParity: %02x\n", ut_buff.parity);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_voqpol_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_voqpol_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_voqpol_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_voqpol_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_voqpol_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_voqpol_set_field_ut(rslt_idx, FWD_VOQPOL_VOQ_BASE, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_voqpol_get_field_ut(rslt_idx, FWD_VOQPOL_VOQ_BASE,
					 &rx_data));
	MESSAGE("== VoQ Base: original (%d), target (%d), result (%d)\n",
		ut_buff.voq_base, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.voq_base = data;
	VERIFY_OP(fe_voqpol_del_entry_ut(&ut_buff, false));
	SHOW(fe_voqpol_print_table_ut());

	return 0;
}

int cs_fe_table_voqpol_ut_boundary(void)
{
	fe_voq_pol_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("voqpol", "Boundary");

	VERIFY_OP(fe_voqpol_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_voq_pol_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_VOQ_POL_ENTRY_MAX; i++) {
		cond_resched();

		fill_voqpol_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_voqpol_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_voqpol_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.pppoe_session_id = FE_VOQ_POL_ENTRY_MAX + 1;
	ret = fe_voqpol_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_VOQ_POL_ENTRY_MAX; i++) {
		cond_resched();

		fill_voqpol_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_voqpol_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_voqpol_print_table_ut());

	return 0;
}

int cs_fe_table_voqpol_ut_mix(void)
{
	fe_voq_pol_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("voqpol", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_voq_pol_entry_t));

	VERIFY_OP(fe_voqpol_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_voqpol_entry(i, &ut_buff);

		VERIFY_OP(fe_voqpol_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_voqpol_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_voqpol_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_voqpol_del_entry_by_idx_ut(7, false));

	SHOW(fe_voqpol_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_voqpol_entry(i, &ut_buff);

		VERIFY_OP(fe_voqpol_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_voqpol_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_voqpol_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_voqpol_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_voqpol_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #0 and #13 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_voqpol_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_voqpol_del_entry_by_idx_ut(13, false));

	SHOW(fe_voqpol_print_table_ut());

	return 0;
}

static void fill_hashhash_entry(int seed, fe_hash_hash_entry_t * ut_buff)
{
	ut_buff->crc32_0 = seed + 10000;
	ut_buff->crc32_1 = seed + 30000;
	ut_buff->result_index0 = seed % 8192;
	ut_buff->result_index1 = seed % 8192;
	ut_buff->mask_ptr0 = seed % 64;
	ut_buff->mask_ptr1 = seed % 64;
	ut_buff->entry0_valid = seed % 2;
	ut_buff->entry1_valid = seed % 2;
	ut_buff->crc16_0 = seed + 1000;
	ut_buff->crc16_1 = seed + 1000;

	return;
}

int cs_fe_table_hashhash_ut_basic(void)
{
	static fe_hash_hash_entry_t ut_entry[3];
	fe_hash_hash_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("hashhash", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_hash_hash_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_hash_hash_entry_t));
	for (i = 0; i < 3; i++) {
		fill_hashhash_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_hashhash_flush_table_ut());
	VERIFY_OP(fe_hashhash_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_hashhash_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_hashhash_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_hashhash_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_hashhash_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_hashhash_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_hashhash_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_hashhash_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_hashhash_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	MESSAGE(" |- CRC32: %08x\n", ut_buff.crc32_0);
	MESSAGE("\tRSLT_IDX: %03x\n", ut_buff.result_index0);
	MESSAGE("\tMASK_PTR: %02x\n", ut_buff.mask_ptr0);
	MESSAGE(" |- CRC32: %08x\n", ut_buff.crc32_1);
	MESSAGE("\tRSLT_IDX: %03x\n", ut_buff.result_index1);
	MESSAGE("\tMASK_PTR: %02x\n", ut_buff.mask_ptr1);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_hashhash_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_hashhash_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_hashhash_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_hashhash_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_hashhash_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_hashhash_set_field_ut(rslt_idx, FE_HASH_HASH_MASK_PTR_0,
					   &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_hashhash_get_field_ut(rslt_idx, FE_HASH_HASH_MASK_PTR_0,
					   &rx_data));
	MESSAGE("== Mask PTR 0: original (%d), target (%d), result (%d)\n",
		ut_buff.mask_ptr0, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.mask_ptr0 = data;
	VERIFY_OP(fe_hashhash_del_entry_ut(&ut_buff, false));
	SHOW(fe_hashhash_print_table_ut());

	return 0;
}

int cs_fe_table_hashhash_ut_boundary(void)
{
	fe_hash_hash_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("hashhash", "Boundary");

	VERIFY_OP(fe_hashhash_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_hash_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_HASH_ENTRY_MAX; i++) {
		cond_resched();

		fill_hashhash_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_hashhash_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashhash_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.crc32_0 = FE_HASH_ENTRY_MAX + 1;
	ret = fe_hashhash_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_HASH_ENTRY_MAX; i++) {
		cond_resched();

		fill_hashhash_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_hashhash_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_hashhash_print_table_ut());

	return 0;
}

int cs_fe_table_hashhash_ut_mix(void)
{
	fe_hash_hash_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("hashhash", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_hash_entry_t));

	VERIFY_OP(fe_hashhash_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_hashhash_entry(i, &ut_buff);

		VERIFY_OP(fe_hashhash_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashhash_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_hashhash_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_hashhash_del_entry_by_idx_ut(7, false));

	SHOW(fe_hashhash_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_hashhash_entry(i, &ut_buff);

		VERIFY_OP(fe_hashhash_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashhash_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_hashhash_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_hashhash_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_hashhash_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #0 and #13 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_hashhash_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_hashhash_del_entry_by_idx_ut(13, false));

	SHOW(fe_hashhash_print_table_ut());

	return 0;
}

static void fill_hashmask_entry(int seed, fe_hash_mask_entry_t * ut_buff)
{
	ut_buff->mac_da_mask = seed % 64;
	ut_buff->mac_sa_mask = seed % 64;
	ut_buff->ethertype_mask = seed % 2;
	ut_buff->llc_type_enc_msb_mask = seed % 2;
	ut_buff->llc_type_enc_lsb_mask = seed % 2;
	ut_buff->tpid_enc_1_msb_mask = seed % 2;
	ut_buff->tpid_enc_1_lsb_mask = seed % 2;
	ut_buff->_8021p_1_mask = seed % 2;
	ut_buff->dei_1_mask = seed % 2;
	ut_buff->vid_1_mask = seed % 2;
	ut_buff->tpid_enc_2_msb_mask = seed % 2;
	ut_buff->tpid_enc_2_lsb_mask = seed % 2;
	ut_buff->_8021p_2_mask = seed % 2;
	ut_buff->dei_2_mask = seed % 2;
	ut_buff->vid_2_mask = seed % 2;
	ut_buff->ip_da_mask = seed % 512;
	ut_buff->ip_sa_mask = seed % 512;
	ut_buff->ip_prot_mask = seed % 2;
	ut_buff->dscp_mask = seed % 64;
	ut_buff->ecn_mask = seed % 4;
	ut_buff->ip_fragment_mask = seed % 2;
	ut_buff->keygen_poly_sel = seed % 4;
	ut_buff->ipv6_flow_lbl_mask = seed % 2;
	ut_buff->ip_ver_mask = seed % 2;
	ut_buff->ip_vld_mask = seed % 2;
	ut_buff->l4_ports_rngd = seed % 4;
	ut_buff->l4_dp_mask = seed + 1000;
	ut_buff->l4_sp_mask = seed + 2000;
	ut_buff->tcp_ctrl_mask = seed % 64;
	ut_buff->tcp_ecn_mask = seed % 8;
	ut_buff->l4_vld_mask = seed % 2;
	ut_buff->lspid_mask = seed % 2;
	ut_buff->fwdtype_mask = seed % 16;
	ut_buff->pppoe_session_id_vld_mask = seed % 2;
	ut_buff->pppoe_session_id_mask = seed % 2;
	ut_buff->rsvd_109 = seed % 2;
	ut_buff->recirc_idx_mask = seed % 2;
	ut_buff->mcidx_mask = seed % 2;
	ut_buff->mc_da_mask = seed % 2;
	ut_buff->bc_da_mask = seed % 2;
	ut_buff->da_an_mac_sel_mask = seed % 2;
	ut_buff->da_an_mac_hit_mask = seed % 2;
	ut_buff->orig_lspid_mask = seed % 2;
	ut_buff->l7_field_mask = seed % 2;
	ut_buff->l7_field_vld_mask = seed % 2;
	ut_buff->hdr_a_flags_crcerr_mask = seed % 2;
	ut_buff->l3_chksum_err_mask = seed % 2;
	ut_buff->l4_chksum_err_mask = seed % 2;
	ut_buff->not_hdr_a_flags_stsvld_mask = seed % 2;
	ut_buff->hash_fid_mask = seed % 2;
	ut_buff->l7_field_sel = seed % 4;
	ut_buff->sa_bng_mac_sel_mask = seed % 2;
	ut_buff->sa_bng_mac_hit_mask = seed % 2;
	ut_buff->spi_vld_mask = seed % 2;
	ut_buff->spi_mask = seed % 2;
	ut_buff->ipv6_ndp_mask = seed % 2;
	ut_buff->ipv6_hbh_mask = seed % 2;
	ut_buff->ipv6_rh_mask = seed % 2;
	ut_buff->ipv6_doh_mask = seed % 2;
	ut_buff->ppp_protocol_vld_mask = seed % 2;
	ut_buff->ppp_protocol_mask = seed % 2;
	ut_buff->pktlen_rng_match_vector_mask = seed % 16;
	ut_buff->mcgid_mask = seed % 32 | (0x1 << 8);
	ut_buff->parity = 0;

	return;
}

int cs_fe_table_hashmask_ut_basic(void)
{
	static fe_hash_mask_entry_t ut_entry[3];
	fe_hash_mask_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("hashmask", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_hash_mask_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_hash_mask_entry_t));
	for (i = 0; i < 3; i++) {
		fill_hashmask_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_hashmask_flush_table_ut());
	VERIFY_OP(fe_hashmask_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_hashmask_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_hashmask_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_hashmask_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_hashmask_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_hashmask_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_hashmask_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_hashmask_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_hashmask_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	MESSAGE("mac_da_mask\t%x\tsa_mask\t%x\tethtype_mask\t%x\n",
		ut_buff.mac_da_mask, ut_buff.mac_sa_mask,
		ut_buff.ethertype_mask);

	MESSAGE("llc_type_enc_msb\t%x\tlsb\t%x\n",
		ut_buff.llc_type_enc_msb_mask, ut_buff.llc_type_enc_lsb_mask);

	MESSAGE("tpid_enc_1_msb_mask\t%x\ttpid_enc_1_lsb_mask\t%x\n",
		ut_buff.tpid_enc_1_msb_mask, ut_buff.tpid_enc_1_lsb_mask);
	MESSAGE("802_1p_1_mask\t%x\tdei_1_mask\t%x\tvid_1_mask\t%x\n",
		ut_buff._8021p_1_mask, ut_buff.dei_1_mask, ut_buff.vid_1_mask);
	MESSAGE("tpid_enc_2_msb_mask\t%x\ttpid_enc_2_lsb_mask\t%x\n",
		ut_buff.tpid_enc_2_msb_mask, ut_buff.tpid_enc_2_lsb_mask);
	MESSAGE("802_1p_2_mask\t%x\tdei_2_mask %x\tvid_2_mask\t%x\n\n",
		ut_buff._8021p_2_mask, ut_buff.dei_2_mask, ut_buff.vid_2_mask);

	MESSAGE("ip_da_mask\t%x\tip_sa_mask\t%x\tip_prot_mask\t%x\n",
		ut_buff.ip_da_mask, ut_buff.ip_sa_mask, ut_buff.ip_prot_mask);
	MESSAGE("dscp mask\t%x\tecn_mask\t%x\tip_fragment_mask\t%x\n",
		ut_buff.dscp_mask, ut_buff.ecn_mask, ut_buff.ip_fragment_mask);

	MESSAGE("keygen_pol_sel\t%x\tipv6_flow_lbl_mask\t%x\t",
		ut_buff.keygen_poly_sel, ut_buff.ipv6_flow_lbl_mask);
	MESSAGE("p_ver_mask\t%x\tip_vld_mask\t%x\n",
		ut_buff.ip_ver_mask, ut_buff.ip_vld_mask);

	MESSAGE("l4_port_rngd\t%x\tl4_dp_mask\t%x\tl4_sp_mask\t%x\n",
		ut_buff.l4_ports_rngd, ut_buff.l4_dp_mask, ut_buff.l4_sp_mask);
	MESSAGE("tcp_ctrl_mask\t%x\ttcp_ecn_mask\t%x\tl4_vld_mask\t%x\n",
		ut_buff.tcp_ctrl_mask, ut_buff.tcp_ecn_mask,
		ut_buff.l4_vld_mask);

	MESSAGE("lspid_mask\t%x\tfwd_type_mask\t%x\n",
		ut_buff.lspid_mask, ut_buff.fwdtype_mask);
	MESSAGE("pppoe_session_id_vld_mask\t%x\tpppoe_session_id_mask\t%x\n",
		ut_buff.pppoe_session_id_vld_mask,
		ut_buff.pppoe_session_id_mask);
	MESSAGE("mcgid_mask\t%x\trecirc_idx_mask\t%x\tmcidx_mask\t%x\n\n",
		ut_buff.mcgid_mask, ut_buff.recirc_idx_mask,
		ut_buff.mcidx_mask);

	MESSAGE("mc_da_mask\t%x\tbc_da_mask\t%x\t",
		ut_buff.mc_da_mask, ut_buff.bc_da_mask);
	MESSAGE("da_an_mac_sel_mask\t%x\tda_an_mac_hit_mask\t%x\n",
		ut_buff.da_an_mac_sel_mask, ut_buff.da_an_mac_hit_mask);
	MESSAGE("orig_lspid_mask\t%x\tl7_field_vld_mask\t%x\t",
		ut_buff.orig_lspid_mask, ut_buff.l7_field_vld_mask);
	MESSAGE("hdr_a_flags_crcerr_mask\t%x\n",
		ut_buff.hdr_a_flags_crcerr_mask);
	MESSAGE("l3_csum_err_mask\t%x\tl4_csum_err_mask\t%x\t",
		ut_buff.l3_chksum_err_mask, ut_buff.l4_chksum_err_mask);
	MESSAGE("not_hdr_a_flags_stsvld_mask\t%x\n",
		ut_buff.not_hdr_a_flags_stsvld_mask);

	MESSAGE("hash_fid_mask\t%x\tl7_field_sel\t%x\t",
		ut_buff.hash_fid_mask, ut_buff.l7_field_sel);
	MESSAGE("sa_bng_mac_sel_mask\t%x\tsa_bng_mac_hit_mask\t%x\n",
		ut_buff.sa_bng_mac_sel_mask, ut_buff.sa_bng_mac_hit_mask);
	MESSAGE("spi_vld_mask\t%x\tspi_mask\t%x\t",
		ut_buff.spi_vld_mask, ut_buff.spi_mask);
	MESSAGE("ipv6_ndp_mask\t%x\tipv6_hbh_mask\t%x\n",
		ut_buff.ipv6_ndp_mask, ut_buff.ipv6_hbh_mask);
	MESSAGE("ipv6_rh_mask\t%x\tipv6_doh_mask\t%x\t",
		ut_buff.ipv6_rh_mask, ut_buff.ipv6_doh_mask);
	MESSAGE("ppp_protocol_vld_mask\t%x\tppp_protocol_mask\t%x\n",
		ut_buff.ppp_protocol_vld_mask, ut_buff.ppp_protocol_mask);
	MESSAGE("pktlen_rng_match_vec_mask\t%x\tmcgid_mask\t%x\t",
		ut_buff.pktlen_rng_match_vector_mask, ut_buff.mcgid_mask);
	MESSAGE("parity\t%x\n", ut_buff.parity);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_hashmask_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_hashmask_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_hashmask_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_hashmask_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_hashmask_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_hashmask_set_field_ut(rslt_idx, FE_HM_IP_DA_MASK, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_hashmask_get_field_ut
		  (rslt_idx, FE_HM_IP_DA_MASK, &rx_data));
	MESSAGE("== IP DA Mask: original (%d), target (%d), result (%d)\n",
		ut_buff.ip_da_mask, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.ip_da_mask = data;
	VERIFY_OP(fe_hashmask_del_entry_ut(&ut_buff, false));
	SHOW(fe_hashmask_print_table_ut());

	return 0;
}

int cs_fe_table_hashmask_ut_boundary(void)
{
	fe_hash_mask_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("hashmask", "Boundary");

	VERIFY_OP(fe_hashmask_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_mask_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_HASH_MASK_ENTRY_MAX; i++) {
		fill_hashmask_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_hashmask_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_hashmask_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.ip_da_mask = FE_HASH_MASK_ENTRY_MAX + 1;
	ret = fe_hashmask_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_HASH_MASK_ENTRY_MAX; i++) {
		fill_hashmask_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_hashmask_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_hashmask_print_table_ut());

	return 0;
}

int cs_fe_table_hashmask_ut_mix(void)
{
	fe_hash_mask_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("hashmask", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_mask_entry_t));

	VERIFY_OP(fe_hashmask_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_hashmask_entry(i, &ut_buff);

		VERIFY_OP(fe_hashmask_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashmask_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_hashmask_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_hashmask_del_entry_by_idx_ut(7, false));

	SHOW(fe_hashmask_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_hashmask_entry(i, &ut_buff);

		VERIFY_OP(fe_hashmask_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashmask_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_hashmask_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_hashmask_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_hashmask_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #0 and #13 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_hashmask_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_hashmask_del_entry_by_idx_ut(13, false));

	SHOW(fe_hashmask_print_table_ut());

	return 0;
}

static void fill_fvlan_entry(int seed, fe_flow_vlan_entry_t * ut_buff)
{
	ut_buff->first_vlan_cmd = seed % 32;
	ut_buff->first_vid = seed % 4096;
	ut_buff->first_tpid_enc = seed % 4;
	ut_buff->second_vlan_cmd = seed % 32;
	ut_buff->second_vid = seed % 4096;
	ut_buff->second_tpid_enc = seed % 4;
	ut_buff->parity = 0;

	return;
}

int cs_fe_table_fvlan_ut_basic(void)
{
	static fe_flow_vlan_entry_t ut_entry[3];
	fe_flow_vlan_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("fvlan", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_flow_vlan_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_flow_vlan_entry_t));
	for (i = 0; i < 3; i++) {
		fill_fvlan_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_fvlan_flush_table_ut());
	VERIFY_OP(fe_fvlan_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_fvlan_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_fvlan_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_fvlan_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_fvlan_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_fvlan_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_fvlan_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_fvlan_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_fvlan_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* 1st VLAN */
	MESSAGE(" |- 1st VLAN cmd: %02x, ID: %04x, TPID Enc: %02x\n",
		ut_buff.first_vlan_cmd, ut_buff.first_vid, ut_buff.first_vid);

	/* 2nd VLAN */
	MESSAGE(" |- 2nd VLAN cmd: %02x, ID: %04x, TPID Enc: %02x\n",
		ut_buff.second_vlan_cmd, ut_buff.second_vid,
		ut_buff.second_tpid_enc);

	MESSAGE(" |- Parity: %02x\n", ut_buff.parity);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_fvlan_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_fvlan_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_fvlan_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_fvlan_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_fvlan_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 10;
	VERIFY_OP(fe_fvlan_set_field_ut(rslt_idx, FWD_FVLAN_FIRST_VLAN_CMD,
					&data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_fvlan_get_field_ut(rslt_idx, FWD_FVLAN_FIRST_VLAN_CMD,
					&rx_data));
	MESSAGE("== First VLAN Cmd: original (%d), target (%d), result (%d)\n",
		ut_buff.first_vlan_cmd, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.first_vlan_cmd = data;
	VERIFY_OP(fe_fvlan_del_entry_ut(&ut_buff, false));
	SHOW(fe_fvlan_print_table_ut());

	return 0;
}

int cs_fe_table_fvlan_ut_boundary(void)
{
	fe_flow_vlan_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("fvlan", "Boundary");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_flow_vlan_entry_t));

	VERIFY_OP(fe_fvlan_flush_table_ut());

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_FVLAN_ENTRY_MAX; i++) {
		cond_resched();

		fill_fvlan_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_fvlan_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_fvlan_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.first_vid = FE_FVLAN_ENTRY_MAX + 1;
	ret = fe_fvlan_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_FVLAN_ENTRY_MAX; i++) {
		cond_resched();

		fill_fvlan_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_fvlan_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_fvlan_print_table_ut());

	return 0;
}

int cs_fe_table_fvlan_ut_mix(void)
{
	fe_flow_vlan_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("fvlan", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_flow_vlan_entry_t));

	VERIFY_OP(fe_fvlan_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_fvlan_entry(i, &ut_buff);

		VERIFY_OP(fe_fvlan_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_fvlan_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_fvlan_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_fvlan_del_entry_by_idx_ut(7, false));

	SHOW(fe_fvlan_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_fvlan_entry(i, &ut_buff);

		VERIFY_OP(fe_fvlan_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_fvlan_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_fvlan_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	fill_fvlan_entry(2 /* we already delete it before */ , &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_fvlan_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_fvlan_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_fvlan_del_entry_by_idx_ut(13, false));

	SHOW(fe_fvlan_print_table_ut());

	return 0;
}

static void fill_hashoverflow_entry(int seed,
				    fe_hash_overflow_entry_t * ut_buff)
{
	ut_buff->crc32 = seed + 0xFFFFFF00;
	ut_buff->crc16 = seed + 0xF00;
	ut_buff->result_index = seed % 8192;
	ut_buff->mask_ptr = seed % 64;

	return;
}

int cs_fe_table_hashoverflow_ut_basic(void)
{
	static fe_hash_overflow_entry_t ut_entry[3];
	fe_hash_overflow_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("hashoverflow", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_hash_overflow_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_hash_overflow_entry_t));
	for (i = 0; i < 3; i++) {
		fill_hashoverflow_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_hashoverflow_flush_table_ut());
	VERIFY_OP(fe_hashoverflow_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_hashoverflow_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_hashoverflow_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_hashoverflow_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_hashoverflow_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_hashoverflow_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_hashoverflow_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_hashoverflow_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_hashoverflow_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	MESSAGE(" |- CRC32: %08x\n", ut_buff.crc32);
	MESSAGE("\tCRC16: %03x\n", ut_buff.crc16);
	MESSAGE("\tRSLT_IDX: %03x\n", ut_buff.result_index);
	MESSAGE("\tMASK_PTR: %02x\n", ut_buff.mask_ptr);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_hashoverflow_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_hashoverflow_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_hashoverflow_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_hashoverflow_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_hashoverflow_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_hashoverflow_set_field_ut
		  (rslt_idx, FE_HASH_OVERFLOW_CRC32, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_hashoverflow_get_field_ut
		  (rslt_idx, FE_HASH_OVERFLOW_CRC32, &rx_data));
	MESSAGE("== CRC32: original (%d), target (%d), result (%d)\n",
		ut_buff.crc32, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.crc32 = data;
	VERIFY_OP(fe_hashoverflow_del_entry_ut(&ut_buff, false));
	SHOW(fe_hashoverflow_print_table_ut());

	return 0;
}

int cs_fe_table_hashoverflow_ut_boundary(void)
{
	fe_hash_overflow_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("hashoverflow", "Boundary");

	VERIFY_OP(fe_hashoverflow_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_overflow_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_HASH_OVERFLOW_ENTRY_MAX; i++) {
		fill_hashoverflow_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_hashoverflow_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_hashoverflow_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.crc32 = FE_HASH_OVERFLOW_ENTRY_MAX + 1;
	ret = fe_hashoverflow_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_HASH_OVERFLOW_ENTRY_MAX; i++) {
		fill_hashoverflow_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_hashoverflow_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_hashoverflow_print_table_ut());

	return 0;
}

int cs_fe_table_hashoverflow_ut_mix(void)
{
	fe_hash_overflow_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("hashoverflow", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_overflow_entry_t));

	VERIFY_OP(fe_hashoverflow_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_hashoverflow_entry(i, &ut_buff);

		VERIFY_OP(fe_hashoverflow_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashoverflow_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_hashoverflow_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_hashoverflow_del_entry_by_idx_ut(7, false));

	SHOW(fe_hashoverflow_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_hashoverflow_entry(i, &ut_buff);

		VERIFY_OP(fe_hashoverflow_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_hashoverflow_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_hashoverflow_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_hashoverflow_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_hashoverflow_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #0 and #13 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_hashoverflow_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_hashoverflow_del_entry_by_idx_ut(13, false));

	SHOW(fe_hashoverflow_print_table_ut());

	return 0;
}

static void fill_hashstatus_entry(int seed, fe_hash_status_entry_t * ut_buff)
{
	ut_buff->data = seed + 0xFFFFFFFFFFFFFF00;

	return;
}

int cs_fe_table_hashstatus_ut_basic(void)
{
	unsigned int rslt_idx = 0, ret;
	__u64 data = 0, rx_data = 0;

	PRINT_CURR_STAGE("hashstatus", "Basic");

	VERIFY_OP(fe_hashstatus_flush_table_ut());

	/* set one field of the entry */
	printk("====== Step 01. set one field of the entry by index\n");
	rslt_idx = 1;
	data = 3;
	VERIFY_OP(fe_hashstatus_set_field_ut
		  (rslt_idx, FE_HASH_STATUS_MEM_DATA_DATA, (__u32 *) & data));

	/* get one field of the entry */
	printk("====== Step 02. get one field of the entry by index\n");
	VERIFY_OP(fe_hashstatus_get_field_ut
		  (rslt_idx, FE_HASH_STATUS_MEM_DATA_DATA,
		   (__u32 *) & rx_data));
	MESSAGE("== Data: target (%llu), result (%llu)\n", data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	return 0;
}

int cs_fe_table_hashstatus_ut_boundary(void)
{
	fe_hash_status_entry_t ut_buff;
	unsigned int i, ret;
	__u64 rx_data = 0;

	PRINT_CURR_STAGE("hashstatus", "Boundary");

	VERIFY_OP(fe_hashstatus_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_status_entry_t));

	/* set data to max. number of entries */
	printk("====== Step 01. set data to max. number of entries\n");
	for (i = 0; i < FE_HASH_STATUS_ENTRY_MAX; i++) {
		fill_hashstatus_entry(i, &ut_buff);

		MESSAGE("### Set data of entry (%d)\n", i);

		VERIFY_OP(fe_hashstatus_set_field_ut
			  (i, FE_HASH_STATUS_MEM_DATA_DATA,
			   (__u32 *) & (ut_buff.data)));
	}

	/* get data from max. number of entries */
	printk("====== Step 03. get data from max. number of entries\n");
	for (i = 0; i < FE_HASH_STATUS_ENTRY_MAX; i++) {
		fill_hashstatus_entry(i, &ut_buff);

		VERIFY_OP(fe_hashstatus_get_field_ut
			  (i, FE_HASH_STATUS_MEM_DATA_DATA,
			   (__u32 *) & (rx_data)));

		MESSAGE("### Data of entry (%d) = (%llu)\n", i, rx_data);

	}

	return 0;
}

int cs_fe_table_hashstatus_ut_mix(void)
{
	fe_hash_status_entry_t ut_buff;
	unsigned int i, ret;
	__u64 rx_data = 0;

	PRINT_CURR_STAGE("hashstatus", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_hash_status_entry_t));

	VERIFY_OP(fe_hashstatus_flush_table_ut());

	/* set data of 10 entries */
	printk("====== Step 01. set data of 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_hashstatus_entry(i, &ut_buff);

		VERIFY_OP(fe_hashstatus_set_field_ut
			  (i, FE_HASH_STATUS_MEM_DATA_DATA,
			   (__u32 *) & (ut_buff.data)));
	}

	/* clean data of the #2 and #7 entries */
	printk("====== Step 02. clean data of the #2 and #7 entries\n");
	ut_buff.data = 0;
	VERIFY_OP(fe_hashstatus_set_field_ut(2, FE_HASH_STATUS_MEM_DATA_DATA,
					     (__u32 *) & (ut_buff.data)));
	VERIFY_OP(fe_hashstatus_set_field_ut(7, FE_HASH_STATUS_MEM_DATA_DATA,
					     (__u32 *) & (ut_buff.data)));

	/* set data of other 4 entries */
	printk("====== Step 03. set data of other 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_hashstatus_entry(i, &ut_buff);

		VERIFY_OP(fe_hashstatus_set_field_ut
			  (i, FE_HASH_STATUS_MEM_DATA_DATA,
			   (__u32 *) & (ut_buff.data)));

	}

	/* clean data of the #0 and #13 entries */
	printk("====== Step 02. clean data of the #2 and #7 entries\n");
	ut_buff.data = 0;
	VERIFY_OP(fe_hashstatus_set_field_ut(0, FE_HASH_STATUS_MEM_DATA_DATA,
					     (__u32 *) & (ut_buff.data)));
	VERIFY_OP(fe_hashstatus_set_field_ut(13, FE_HASH_STATUS_MEM_DATA_DATA,
					     (__u32 *) & (ut_buff.data)));

	/* get data from max. number of entries */
	printk("====== Step 03. get data from max. number of entries\n");
	for (i = 0; i < FE_HASH_STATUS_ENTRY_MAX; i++) {
		fill_hashstatus_entry(i, &ut_buff);

		VERIFY_OP(fe_hashstatus_get_field_ut
			  (i, FE_HASH_STATUS_MEM_DATA_DATA,
			   (__u32 *) & (rx_data)));

		MESSAGE("### Data of entry (%d) = (%llu)\n", i, rx_data);

	}

	return 0;
}

static void fill_pktlen_entry(int seed, fe_pktlen_rngs_entry_t * ut_buff)
{
	ut_buff->low = seed + 0x3FF0;
	ut_buff->high = seed;
	ut_buff->valid = 1;

	return;
}

int cs_fe_table_pktlen_ut_basic(void)
{
	static fe_pktlen_rngs_entry_t ut_entry[3];
	fe_pktlen_rngs_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("PKTLEN RANGE", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_pktlen_rngs_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_pktlen_rngs_entry_t));
	for (i = 0; i < 3; i++) {
		fill_pktlen_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_pktlen_flush_table_ut());
	VERIFY_OP(fe_pktlen_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_pktlen_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_pktlen_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_pktlen_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_pktlen_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_pktlen_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_pktlen_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_pktlen_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_pktlen_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);
	MESSAGE("\tLow  %4d, High %4d, Valid %2d\n", ut_buff.low, ut_buff.high,
		ut_buff.valid);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_pktlen_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_pktlen_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_pktlen_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_pktlen_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_pktlen_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_pktlen_set_field_ut(rslt_idx, FE_PKTLEN_LOW, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_pktlen_get_field_ut(rslt_idx, FE_PKTLEN_LOW, &rx_data));
	MESSAGE("== Low: original (%d), target (%d), result (%d)\n",
		ut_buff.low, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.low = data;
	VERIFY_OP(fe_pktlen_del_entry_ut(&ut_buff, false));
	SHOW(fe_pktlen_print_table_ut());

	return 0;
}

int cs_fe_table_pktlen_ut_boundary(void)
{
	fe_pktlen_rngs_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("PKTLEN RANGE", "Boundary");

	VERIFY_OP(fe_pktlen_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_pktlen_rngs_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_PKTLEN_RANGE_ENTRY_MAX; i++) {
		cond_resched();

		fill_pktlen_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_pktlen_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_pktlen_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.low = FE_PKTLEN_RANGE_ENTRY_MAX + 1;
	ret = fe_pktlen_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_PKTLEN_RANGE_ENTRY_MAX; i++) {
		cond_resched();

		fill_pktlen_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_pktlen_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_pktlen_print_table_ut());

	return 0;
}

int cs_fe_table_pktlen_ut_mix(void)
{
	fe_pktlen_rngs_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("PKTLEN RANGE", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_pktlen_rngs_entry_t));

	VERIFY_OP(fe_pktlen_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 3 entries\n");
	for (i = 0; i < 3; i++) {
		fill_pktlen_entry(i, &ut_buff);

		VERIFY_OP(fe_pktlen_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_pktlen_print_table_ut());

	/* delete the entry #2 */
	printk("====== Step 02. delete the entry #2\n");

	VERIFY_OP(fe_pktlen_del_entry_by_idx_ut(2, false));

	SHOW(fe_pktlen_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 2 entries\n");
	for (i = 10; i < 12; i++) {
		fill_pktlen_entry(i, &ut_buff);

		VERIFY_OP(fe_pktlen_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_pktlen_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	i = 2;			/* we already delete it before */
	fill_pktlen_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_pktlen_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk("====== Step 05. delete the #0 (first) and #3 (last) entries\n");

	VERIFY_OP(fe_pktlen_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_pktlen_del_entry_by_idx_ut(3, false));

	SHOW(fe_pktlen_print_table_ut());

	return 0;
}

static void fill_qosrslt_entry(int seed, fe_qos_result_entry_t * ut_buff)
{
	ut_buff->wred_cos = seed % 8;
	ut_buff->voq_cos = seed % 8;
	ut_buff->pol_cos = seed % 8;
	ut_buff->premark = seed % 2;
	ut_buff->change_dscp_en = seed % 2;
	ut_buff->dscp = seed % 64;
	ut_buff->dscp_markdown_en = seed % 2;
	ut_buff->marked_down_dscp = (seed + (seed / 64)) % 64;
	ut_buff->ecn_en = seed % 2;
	ut_buff->top_802_1p = seed % 8;
	ut_buff->marked_down_top_802_1p = seed % 8;
	ut_buff->top_8021p_markdown_en = seed % 2;
	ut_buff->top_dei = seed % 2;
	ut_buff->marked_down_top_dei = seed % 2;
	ut_buff->inner_802_1p = seed % 8;
	ut_buff->marked_down_inner_802_1p = seed % 8;
	ut_buff->inner_8021p_markdown_en = seed % 2;
	ut_buff->inner_dei = seed % 2;
	ut_buff->marked_down_inner_dei = seed % 2;
	ut_buff->change_8021p_1_en = seed % 2;
	ut_buff->change_dei_1_en = seed % 2;
	ut_buff->change_8021p_2_en = seed % 2;
	ut_buff->change_dei_2_en = seed % 2;
	ut_buff->parity = 0;

	return;
}

int cs_fe_table_qosrslt_ut_basic(void)
{
	static fe_qos_result_entry_t ut_entry[3];
	fe_qos_result_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("QoS Result", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_qos_result_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_qos_result_entry_t));
	for (i = 0; i < 3; i++) {
		fill_qosrslt_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_qosrslt_flush_table_ut());
	VERIFY_OP(fe_qosrslt_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_qosrslt_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_qosrslt_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_qosrslt_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_qosrslt_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_qosrslt_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_qosrslt_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_qosrslt_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_qosrslt_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);
	MESSAGE("  |- WRED COS: %02x\n", ut_buff.wred_cos);
	MESSAGE("\tVOQ COS: %02x\n", ut_buff.voq_cos);
	MESSAGE("\tPOL COS: %02x\n", ut_buff.pol_cos);
	MESSAGE("\tPremark: %02x\n", ut_buff.voq_cos);
	MESSAGE("\tChange DSCP EN: %02x\n", ut_buff.change_dscp_en);
	MESSAGE("\tDSCP: %02x\n", ut_buff.dscp);
	MESSAGE("\tDSCP Markdown EN: %02x\n", ut_buff.dscp_markdown_en);
	MESSAGE("\tMarked Down DSCP: %02x\n", ut_buff.marked_down_dscp);
	MESSAGE("\tECN EN: %02x\n", ut_buff.ecn_en);
	MESSAGE("\tTOP 802.1p: %02x\n", ut_buff.top_802_1p);
	MESSAGE("\tMarked Down Top 802.1p: %02x\n",
		ut_buff.marked_down_top_802_1p);
	MESSAGE("\tTop 802.1p Markdown EN: %02x\n",
		ut_buff.top_8021p_markdown_en);
	MESSAGE("\tTop Dei: %02x\n", ut_buff.top_dei);
	MESSAGE("\tMarked Down Top Dei: %02x\n", ut_buff.marked_down_top_dei);
	MESSAGE("\tInner 802.1p: %02x\n", ut_buff.inner_802_1p);
	MESSAGE("\tMarked Down Inner 802.1p: %02x\n",
		ut_buff.marked_down_inner_802_1p);
	MESSAGE("\tInner 802.1p Markdown EN: %02x\n",
		ut_buff.inner_8021p_markdown_en);
	MESSAGE("\tInner Dei: %02x\n", ut_buff.inner_dei);
	MESSAGE("\tMarked Down Inner Dei: %02x\n",
		ut_buff.marked_down_inner_dei);
	MESSAGE("\tChange 802.1p 1 EN: %02x\n", ut_buff.change_8021p_1_en);
	MESSAGE("\tChange Dei 1 EN: %02x\n", ut_buff.change_dei_1_en);
	MESSAGE("\tChange 802.1p 2 EN: %02x\n", ut_buff.change_8021p_2_en);
	MESSAGE("\tChange Dei 2 EN: %02x\n", ut_buff.change_dei_2_en);
	MESSAGE("\tMem Parity: %02x\n", ut_buff.parity);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_qosrslt_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_qosrslt_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_qosrslt_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_qosrslt_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_qosrslt_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_qosrslt_set_field_ut(rslt_idx, FWD_QOS_DSCP, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_qosrslt_get_field_ut(rslt_idx, FWD_QOS_DSCP, &rx_data));
	MESSAGE("== DSCP: original (%d), target (%d), result (%d)\n",
		ut_buff.dscp, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.dscp = data;
	VERIFY_OP(fe_qosrslt_del_entry_ut(&ut_buff, false));
	SHOW(fe_qosrslt_print_table_ut());

	return 0;
}

int cs_fe_table_qosrslt_ut_boundary(void)
{
	fe_qos_result_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("QoS Result", "Boundary");

	VERIFY_OP(fe_qosrslt_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_qos_result_entry_t));

	/* insert max. number of entries */
	/* entry #0 is reserved */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 1; i < FE_QOS_RESULT_ENTRY_MAX; i++) {
		cond_resched();

		fill_qosrslt_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_qosrslt_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_qosrslt_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.dscp = FE_QOS_RESULT_ENTRY_MAX + 1;
	ret = fe_qosrslt_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 1; i < FE_QOS_RESULT_ENTRY_MAX; i++) {
		cond_resched();

		fill_qosrslt_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_qosrslt_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_qosrslt_print_table_ut());

	return 0;
}

int cs_fe_table_qosrslt_ut_mix(void)
{
	fe_qos_result_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("QoS Result", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_qos_result_entry_t));

	VERIFY_OP(fe_qosrslt_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_qosrslt_entry(i, &ut_buff);

		VERIFY_OP(fe_qosrslt_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_qosrslt_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_qosrslt_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_qosrslt_del_entry_by_idx_ut(7, false));

	SHOW(fe_qosrslt_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_qosrslt_entry(i, &ut_buff);

		VERIFY_OP(fe_qosrslt_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_qosrslt_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_qosrslt_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	fill_qosrslt_entry(1 /* we already delete it before */ , &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_qosrslt_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk
	    ("====== Step 05. delete the #1 (first) and #14 (last) entries\n");

	VERIFY_OP(fe_qosrslt_del_entry_by_idx_ut(1, false));
	VERIFY_OP(fe_qosrslt_del_entry_by_idx_ut(14, false));

	SHOW(fe_qosrslt_print_table_ut());

	return 0;
}

static void fill_vlan_entry(int seed, fe_vlan_entry_t * ut_buff)
{
	ut_buff->vlan_member = seed & 0x3F;
	ut_buff->vlan_egress_untagged = seed & 0x3F;
	ut_buff->vlan_fid = seed % 16;
	ut_buff->vlan_first_vlan_cmd = seed % 32;
	ut_buff->vlan_first_vid = seed % 4096;
	ut_buff->vlan_first_tpid_enc = seed % 4;
	ut_buff->vlan_second_vlan_cmd = seed % 32;
	ut_buff->vlan_second_vid = seed % 4096;
	ut_buff->vlan_second_tpid_enc = seed % 4;
	ut_buff->vlan_mcgid = seed % 32 | (0x1 << 8);
	ut_buff->parity = 0;

	return;
}

int cs_fe_table_vlan_ut_basic(void)
{
	static fe_vlan_entry_t ut_entry[3];
	fe_vlan_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret, ref_cnt, data, rx_data = 0xFFFF;
	int i;

	PRINT_CURR_STAGE("VLAN", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_vlan_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_vlan_entry_t));
	for (i = 0; i < 3; i++) {
		fill_vlan_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_vlan_flush_table_ut());
	VERIFY_OP(fe_vlan_add_entry_ut(&ut_entry[0], &rslt_idx));
	SHOW(fe_vlan_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_vlan_del_entry_by_idx_ut(rslt_idx, false));
	SHOW(fe_vlan_print_table_ut());

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_vlan_add_entry_ut(&ut_entry[1], &rslt_idx));
	SHOW(fe_vlan_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_vlan_find_entry_ut(&ut_entry[1], &rslt_idx, 0));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	MESSAGE(" |- VLAN MEMBER: %02x\n", ut_buff.vlan_member);
	MESSAGE("\tVLAN_EGR_UNTAG: %02x\n", ut_buff.vlan_egress_untagged);
	MESSAGE("\tVLAN_FID: %02x\n", ut_buff.vlan_fid);
	MESSAGE("\tVLAN first Cmd: %02x\n", ut_buff.vlan_first_vlan_cmd);
	MESSAGE("\tVLAN first VID: %04x\n", ut_buff.vlan_first_vid);
	MESSAGE("\tVLAN first TPID Enc: %02x\n", ut_buff.vlan_first_tpid_enc);
	MESSAGE("\tVLAN second Cmd: %02x\n", ut_buff.vlan_second_vlan_cmd);
	MESSAGE("\tVLAN second VID: %04x\n", ut_buff.vlan_second_vid);
	MESSAGE("\tVLAN second TPID Enc: %02x\n", ut_buff.vlan_second_tpid_enc);
	MESSAGE("\tVLAN MCGID: %02x\n", ut_buff.vlan_mcgid);
	MESSAGE("\tParity: %02x\n", ut_buff.parity);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_vlan_get_entry_ut(rslt_idx, &ut_buff));

	VERIFY_OP(fe_vlan_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_vlan_inc_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_vlan_inc_entry_refcnt_ut(rslt_idx));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_vlan_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_vlan_dec_entry_refcnt_ut(rslt_idx));
	VERIFY_OP(fe_vlan_get_entry_refcnt_ut(rslt_idx, &ref_cnt));
	MESSAGE("== index: %04d | refcnt: %d\n", rslt_idx, ref_cnt);

	/* set one field of the entry */
	printk("====== Step 09. set one field of the entry by index\n");
	data = 3;
	VERIFY_OP(fe_vlan_set_field_ut(rslt_idx, FE_VLN_FIRST_VID, &data));

	/* get one field of the entry */
	printk("====== Step 10. get one field of the entry by index\n");
	VERIFY_OP(fe_vlan_get_field_ut(rslt_idx, FE_VLN_FIRST_VID, &rx_data));
	MESSAGE("== 1st VID: original (%d), target (%d), result (%d)\n",
		ut_buff.vlan_first_vid, data, rx_data);
	if (rx_data != data) {
		MESSAGE("== unexpected value is taken. ==> NOK\n");
		return -1;
	}

	/* delete an entry by content */
	printk("====== Step 11. delete an entry by content\n");
	ut_buff.vlan_first_vid = data;
	VERIFY_OP(fe_vlan_del_entry_ut(&ut_buff, false));
	SHOW(fe_vlan_print_table_ut());

	return 0;
}

int cs_fe_table_vlan_ut_boundary(void)
{
	fe_vlan_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("VLAN", "Boundary");

	VERIFY_OP(fe_vlan_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_vlan_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_VLAN_ENTRY_MAX; i++) {
		cond_resched();

		fill_vlan_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_vlan_add_entry_ut(&ut_buff, &rslt_idx));
	}
	SHOW(fe_vlan_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	ut_buff.vlan_first_vid = FE_VLAN_ENTRY_MAX + 1;
	ret = fe_vlan_add_entry_ut(&ut_buff, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_VLAN_ENTRY_MAX; i++) {
		cond_resched();

		fill_vlan_entry(i, &ut_buff);

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_vlan_del_entry_ut(&ut_buff, 0));
	}
	SHOW(fe_vlan_print_table_ut());

	return 0;
}

int cs_fe_table_vlan_ut_mix(void)
{
	fe_vlan_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("VLAN", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_vlan_entry_t));

	VERIFY_OP(fe_vlan_flush_table_ut());

	/* insert 10 entries */
	printk("====== Step 01. insert 10 entries\n");
	for (i = 0; i < 10; i++) {
		fill_vlan_entry(i, &ut_buff);

		VERIFY_OP(fe_vlan_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_vlan_print_table_ut());

	/* delete the #2 and #7 entries */
	printk("====== Step 02. delete the #2 and #7 entries\n");

	VERIFY_OP(fe_vlan_del_entry_by_idx_ut(2, false));
	VERIFY_OP(fe_vlan_del_entry_by_idx_ut(7, false));

	SHOW(fe_vlan_print_table_ut());

	/* insert 4 entries */
	printk("====== Step 03. insert 4 entries\n");
	for (i = 10; i < 14; i++) {
		fill_vlan_entry(i, &ut_buff);

		VERIFY_OP(fe_vlan_add_entry_ut(&ut_buff, &rslt_idx));

	}
	SHOW(fe_vlan_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 04. try to delete one nonexistent entry\n");

	/* try to delete an nonexistent index */
	ret = fe_vlan_del_entry_by_idx_ut(2, false);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
		return -1;
	}

	i = 2;			/* we already delete it before */
	fill_vlan_entry(i, &ut_buff);

	/* try to delete a nonexistent entry */
	ret = fe_vlan_del_entry_ut(&ut_buff, false);
	if (ret == FE_TABLE_ENTRYNOTFOUND) {
		MESSAGE("We can't delete a nonexistent entry. ==> OK\n");
	} else {
		MESSAGE("Strange! We can delete it twice. ==> NOK\n");
	}

	/* delete the #1 and #14 entries */
	printk
	    ("====== Step 05. delete the #0 (first) and #13 (last) entries\n");

	VERIFY_OP(fe_vlan_del_entry_by_idx_ut(0, false));
	VERIFY_OP(fe_vlan_del_entry_by_idx_ut(13, false));

	SHOW(fe_vlan_print_table_ut());

	return 0;
}

static void fill_l2mac_entry(int seed, fe_l2_addr_pair_entry_t * ut_buff)
{
	ut_buff->mac_sa[0] = 0;
	ut_buff->mac_sa[1] = 0;
	ut_buff->mac_sa[2] = 0;
	ut_buff->mac_sa[3] = 0;
	ut_buff->mac_sa[4] = (seed + 1) / 256;
	ut_buff->mac_sa[5] = (seed + 1) & 0xFF;

	ut_buff->sa_count = 1;

	ut_buff->mac_da[0] = 0;
	ut_buff->mac_da[1] = 0;
	ut_buff->mac_da[2] = 0;
	ut_buff->mac_da[3] = 1;
	ut_buff->mac_da[4] = (seed + 1) / 256;
	ut_buff->mac_da[5] = (seed + 1) & 0xFF;

	ut_buff->da_count = 1;

	return;
}

int cs_fe_table_l2mac_ut_basic(void)
{
	static fe_l2_addr_pair_entry_t ut_entry[3];
	fe_l2_addr_pair_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret;
	int i;

	PRINT_CURR_STAGE("Layer2 MAC", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_l2_addr_pair_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_l2_addr_pair_entry_t));
	for (i = 0; i < 3; i++) {
		fill_l2mac_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry\n");

	VERIFY_OP(fe_l2mac_flush_table_ut());
	VERIFY_OP(fe_l2mac_add_ut
		  (ut_entry[0].mac_sa, ut_entry[0].mac_da, &rslt_idx));
	SHOW(fe_l2mac_print_entry_ut(rslt_idx));

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_l2mac_del_entry_by_idx_ut(rslt_idx, false));
	//SHOW(fe_l2mac_print_table_ut());
	SHOW(fe_l2mac_print_entry_ut(rslt_idx));

	/* add an entry */
	printk("====== Step 03. add an entry again\n");
	VERIFY_OP(fe_l2mac_add_ut
		  (ut_entry[1].mac_sa, ut_entry[1].mac_da, &rslt_idx));
	SHOW(fe_l2mac_print_entry_ut(rslt_idx));

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_l2mac_find_ut
		  (ut_entry[1].mac_sa, ut_entry[1].mac_da, &rslt_idx));
	MESSAGE("== find result index = %d\n\n", rslt_idx);

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_l2mac_get_entry_ut(rslt_idx, &ut_buff));

	MESSAGE("== index: %04d\n", rslt_idx);
	MESSAGE("\tSA MAC: %02x.%02x.%02x.%02x.%02x.%02x\n",
		ut_buff.mac_sa[0], ut_buff.mac_sa[1], ut_buff.mac_sa[2],
		ut_buff.mac_sa[3], ut_buff.mac_sa[4], ut_buff.mac_sa[5]);
	MESSAGE("\tsa_count: %02x\n", ut_buff.sa_count);
	MESSAGE("\tDA MAC: %02x.%02x.%02x.%02x.%02x.%02x\n",
		ut_buff.mac_da[0], ut_buff.mac_da[1], ut_buff.mac_da[2],
		ut_buff.mac_da[3], ut_buff.mac_da[4], ut_buff.mac_da[5]);
	MESSAGE("\tda_count: %02x\n", ut_buff.da_count);

	/* increase reference counter of the entry */
	printk("====== Step 06. increase refcnt\n");
	VERIFY_OP(fe_l2mac_inc_refcnt_ut(rslt_idx, true, false));

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_l2mac_get_entry_ut(rslt_idx, &ut_buff));
	MESSAGE("== index: %04d | sa_count: %d | da_count: %d\n",
		rslt_idx, ut_buff.sa_count, ut_buff.da_count);

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_l2mac_del_ut(rslt_idx, false, true));
	VERIFY_OP(fe_l2mac_get_entry_ut(rslt_idx, &ut_buff));
	MESSAGE("== index: %04d | sa_count: %d | da_count: %d\n",
		rslt_idx, ut_buff.sa_count, ut_buff.da_count);

	/* delete an entry by index */
	printk("====== Step 11. delete an entry by index\n");
	VERIFY_OP(fe_l2mac_del_ut(rslt_idx, true, false));
	SHOW(fe_l2mac_print_table_ut());

	return 0;
}

int cs_fe_table_l2mac_ut_boundary(void)
{
	fe_l2_addr_pair_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("Layer2 MAC", "Boundary");

	VERIFY_OP(fe_l2mac_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_l2_addr_pair_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_L2_ADDR_PAIR_ENTRY_MAX; i++) {
		cond_resched();

		fill_l2mac_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_l2mac_add_ut
			  (ut_buff.mac_sa, ut_buff.mac_da, &rslt_idx));
	}
	SHOW(fe_l2mac_print_table_ut());

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	fill_l2mac_entry(FE_L2_ADDR_PAIR_ENTRY_MAX + 1, &ut_buff);
	ret = fe_l2mac_add_ut(ut_buff.mac_sa, ut_buff.mac_da, &rslt_idx);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_L2_ADDR_PAIR_ENTRY_MAX; i++) {
		cond_resched();

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_l2mac_del_ut(i, true, true));
	}
	SHOW(fe_l2mac_print_table_ut());

	return 0;
}

int cs_fe_table_l2mac_ut_mix(void)
{
	fe_l2_addr_pair_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;

	PRINT_CURR_STAGE("Layer2 MAC", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_l2_addr_pair_entry_t));

	VERIFY_OP(fe_l2mac_flush_table_ut());

	/* insert 10 entries of SA MAC */
	printk("====== Step 01. insert 10 entries of SA MAC\n");
	for (i = 0; i < 10; i++) {
		fill_l2mac_entry(i, &ut_buff);

		VERIFY_OP(fe_l2mac_add_ut(ut_buff.mac_sa, 0, &rslt_idx));

	}
	SHOW(fe_l2mac_print_table_ut());

	/* insert 10 entries of SA and DA MAC */
	printk("====== Step 02. insert 10 entries of SA and DA MAC\n");
	for (i = 10; i < 20; i++) {
		fill_l2mac_entry(i, &ut_buff);

		VERIFY_OP(fe_l2mac_add_ut
			  (ut_buff.mac_sa, ut_buff.mac_da, &rslt_idx));

	}
	SHOW(fe_l2mac_print_table_ut());

	/* insert 10 entries of DA MAC */
	printk("====== Step 03. insert 10 entries of DA MAC\n");
	for (i = 20; i < 30; i++) {
		fill_l2mac_entry(i, &ut_buff);

		VERIFY_OP(fe_l2mac_add_ut(0, ut_buff.mac_da, &rslt_idx));

	}
	SHOW(fe_l2mac_print_table_ut());

	/* insert 20 entries of duplicate SA MAC */
	printk("====== Step 04. insert 20 entries of duplicate SA MAC\n");
	for (i = 0; i < 20; i++) {
		fill_l2mac_entry(i, &ut_buff);

		VERIFY_OP(fe_l2mac_add_ut(ut_buff.mac_sa, 0, &rslt_idx));

	}
	SHOW(fe_l2mac_print_table_ut());

	/* delete 20 entries of DA MAC */
	printk("====== Step 05. delete 20 entries of DA MAC\n");
	for (i = 0; i < 20; i++) {
		VERIFY_OP(fe_l2mac_del_ut(i, false, true));
	}
	SHOW(fe_l2mac_print_table_ut());

	/* try to delete one nonexistent entry,
	   but we expect it should be fail */
	printk("====== Step 06. try to delete 10 nonexistent entries\n");

	/* try to delete an nonexistent index */
	for (i = 50; i < 60; i++) {
		ret = fe_l2mac_del_ut(i, true, true);
		if (ret != FE_TABLE_OK /*ret == FE_TABLE_EENTRYNOTRSVD */ ) {
			MESSAGE
			    ("We can't delete a nonexistent entry. ==> OK\n");
		} else {
			MESSAGE("Strange! We can delete it twice. ==> NOK\n");
			return -1;
		}
	}

	/* insert 10 entries of DA MAC */
	printk("====== Step 07. insert 10 entries of DA MAC\n");
	for (i = 30; i < 40; i++) {
		fill_l2mac_entry(i, &ut_buff);

		VERIFY_OP(fe_l2mac_add_ut(0, ut_buff.mac_da, &rslt_idx));

	}
	SHOW(fe_l2mac_print_table_ut());

	return 0;
}

static void fill_l3ip_entry(int seed, fe_l3_addr_entry_t * ut_buff)
{
	ut_buff->ip_addr[0] = seed;
	ut_buff->ip_addr[1] = seed + 0x10000;
	ut_buff->ip_addr[2] = seed + 0x20000;
	ut_buff->ip_addr[3] = seed + 0x30000;

	ut_buff->count[0] = 1;
	ut_buff->count[1] = 1;
	ut_buff->count[2] = 1;
	ut_buff->count[3] = 1;

	return;
}

int cs_fe_table_l3ip_ut_basic(void)
{
	static fe_l3_addr_entry_t ut_entry[3];
	fe_l3_addr_entry_t ut_buff;
	unsigned int rslt_idx = 0, ret;
	int i;
	int curr_step = 0;

	PRINT_CURR_STAGE("Layer3 IP", "Basic");
	/* init local variables */
	memset(ut_entry, 0x0, sizeof(fe_l3_addr_entry_t) * 3);
	memset(&ut_buff, 0x0, sizeof(fe_l3_addr_entry_t));
	for (i = 0; i < 3; i++) {
		fill_l3ip_entry(i, &ut_entry[i]);
	}

	/* add an entry */
	printk("====== Step 01. add an entry of IPv6\n");

	VERIFY_OP(fe_l3ip_flush_table_ut());
	VERIFY_OP(fe_l3ip_add_ut(ut_entry[0].ip_addr, &rslt_idx, true));
	SHOW(fe_l3ip_print_range_ut(rslt_idx, rslt_idx + 3));
	CHECK_STEP;

	/* delete an entry by index */
	printk("====== Step 02. delete an entry by index\n");
	VERIFY_OP(fe_l3ip_del_ut(rslt_idx, true));
	SHOW(fe_l3ip_print_range_ut(rslt_idx, rslt_idx + 3));
	CHECK_STEP;

	/* add an entry */
	printk("====== Step 03. add an entry of IPv4\n");
	VERIFY_OP(fe_l3ip_add_ut(ut_entry[1].ip_addr, &rslt_idx, false));
	SHOW(fe_l3ip_print_range_ut(rslt_idx, rslt_idx + 3));
	CHECK_STEP;

	/* find an entry */
	printk("====== Step 04. find an entry\n");
	VERIFY_OP(fe_l3ip_find_ut(ut_entry[1].ip_addr, &rslt_idx, false));
	MESSAGE("== find result index = %d\n\n", rslt_idx);
	CHECK_STEP;

	/* get an entry */
	printk("====== Step 05. get an entry by index\n");
	VERIFY_OP(fe_l3ip_get_entry_ut(rslt_idx, &ut_buff));

	MESSAGE("== index: %04d\n", rslt_idx);
	MESSAGE("\tIP: %08x.%08x.%08x.%08x\n",
		ut_buff.ip_addr[0], ut_buff.ip_addr[1], ut_buff.ip_addr[2],
		ut_buff.ip_addr[3]);
	MESSAGE("\tcount #0: %04x\n", ut_buff.count[0]);
	MESSAGE("\tcount #1: %04x\n", ut_buff.count[1]);
	MESSAGE("\tcount #2: %04x\n", ut_buff.count[2]);
	MESSAGE("\tcount #3: %04x\n", ut_buff.count[3]);
	CHECK_STEP;

	/* increase reference counter of the entry twice */
	printk("====== Step 06. increase refcnt twice\n");
	VERIFY_OP(fe_l3ip_inc_refcnt_ut(rslt_idx, false));
	VERIFY_OP(fe_l3ip_inc_refcnt_ut(rslt_idx, false));
	CHECK_STEP;

	/* get reference counter of the entry */
	printk("====== Step 07. get refcnt by index\n");
	VERIFY_OP(fe_l3ip_get_entry_ut(rslt_idx, &ut_buff));
	MESSAGE("== index: %04d | count: %x\n",
		rslt_idx, ut_buff.count[rslt_idx % 4]);
	CHECK_STEP;

	/* decrease reference counter of the entry */
	printk("====== Step 08. decrease refcnt by index\n");
	VERIFY_OP(fe_l3ip_del_ut(rslt_idx, false));
	VERIFY_OP(fe_l3ip_get_entry_ut(rslt_idx, &ut_buff));
	MESSAGE("== index: %04d | count: %x\n",
		rslt_idx, ut_buff.count[rslt_idx % 4]);
	CHECK_STEP;

	/* delete an entry by index */
	printk("====== Step 11. delete an entry by index\n");
	VERIFY_OP(fe_l3ip_del_ut(rslt_idx, false));
	SHOW(fe_l3ip_print_range_ut(0, 11));
	CHECK_STEP;

	return 0;
}

int cs_fe_table_l3ip_ut_boundary(void)
{
	fe_l3_addr_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;
	int curr_step = 0;

	PRINT_CURR_STAGE("Layer3 IP", "Boundary");

	VERIFY_OP(fe_l3ip_flush_table_ut());
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_l3_addr_entry_t));

	/* insert max. number of entries */
	printk("====== Step 01. insert max. number of entries\n");
	for (i = 0; i < FE_L3_ADDR_ENTRY_MAX; i++) {
		cond_resched();

		fill_l3ip_entry(i, &ut_buff);

		MESSAGE("### Add entry (%d)\n", i);

		VERIFY_OP(fe_l3ip_add_ut(ut_buff.ip_addr, &rslt_idx, true));
	}
	SHOW(fe_l3ip_print_table_ut());
	CHECK_STEP;

	/* insert one more entry but we expect it should be fail */
	printk("====== Step 02. try to insert one more entry\n");
	fill_l3ip_entry(FE_L3_ADDR_ENTRY_MAX + 1, &ut_buff);
	ret = fe_l3ip_add_ut(ut_buff.ip_addr, &rslt_idx, true);
	if (ret != FE_TABLE_OK /*ret == FE_TABLE_ETBLFULL */ ) {
		MESSAGE("Table is full. We can't add more entries. ==> OK\n");
	} else {
		MESSAGE("Strange! We can add more than max. number of entries."
			" ==> NOK\n");
		return -1;
	}
	CHECK_STEP;

	/* delete max. number of entries */
	printk("====== Step 03. delete max. number of entries\n");
	for (i = 0; i < FE_L3_ADDR_ENTRY_MAX; i++) {
		cond_resched();

		MESSAGE("### Delete entry (%d)\n", i);

		VERIFY_OP(fe_l3ip_del_ut(i << 2, true));
	}
	SHOW(fe_l3ip_print_table_ut());
	CHECK_STEP;

	return 0;
}

int cs_fe_table_l3ip_ut_mix(void)
{
	fe_l3_addr_entry_t ut_buff;
	unsigned int i, rslt_idx = 0, ret;
	int curr_step = 0;

	PRINT_CURR_STAGE("Layer3 IP", "Mix");
	/* init local variables */
	memset(&ut_buff, 0x0, sizeof(fe_l3_addr_entry_t));

	VERIFY_OP(fe_l3ip_flush_table_ut());

	/* insert 10 entries of IPv4 addresses */
	printk("====== Step 01. insert 10 entries of IPv4 addresses\n");
	for (i = 0; i < 10; i++) {
		fill_l3ip_entry(i, &ut_buff);

		VERIFY_OP(fe_l3ip_add_ut(ut_buff.ip_addr, &rslt_idx, false));

	}
	SHOW(fe_l3ip_print_range_ut(0, 120));
	CHECK_STEP;

	/* insert 10 entries of IPv6 addresses */
	printk("====== Step 02. insert 10 entries of IPv6 addresses\n");
	for (i = 10; i < 20; i++) {
		fill_l3ip_entry(i, &ut_buff);

		VERIFY_OP(fe_l3ip_add_ut(ut_buff.ip_addr, &rslt_idx, true));

	}
	SHOW(fe_l3ip_print_range_ut(0, 120));
	CHECK_STEP;

	/* delete 5 entries of IPv4 addresses */
	printk("====== Step 03. delete 5 entries of IPv4 addresses\n");
	for (i = 0; i < 5; i++) {
		VERIFY_OP(fe_l3ip_del_ut(i, false));
	}
	SHOW(fe_l3ip_print_range_ut(0, 120));
	CHECK_STEP;

	/* insert 10 entries of IPv4 addresses */
	printk("====== Step 04. insert 10 entries of IPv4 addresses\n");
	for (i = 20; i < 30; i++) {
		fill_l3ip_entry(i, &ut_buff);

		VERIFY_OP(fe_l3ip_add_ut(ut_buff.ip_addr, &rslt_idx, false));

	}
	SHOW(fe_l3ip_print_range_ut(0, 120));
	CHECK_STEP;

	/* delete 5 entries of IPv6 addresses */
	printk("====== Step 05. delete 5 entries of IPv6 addresses\n");
	for (i = 3; i < 8; i++) {
		VERIFY_OP(fe_l3ip_del_ut(i << 2, true));
	}
	SHOW(fe_l3ip_print_range_ut(0, 120));
	CHECK_STEP;

	/* insert 10 entries of IPv6 addresses */
	printk("====== Step 06. insert 10 entries of IPv6 addresses\n");
	for (i = 30; i < 40; i++) {
		fill_l3ip_entry(i, &ut_buff);

		VERIFY_OP(fe_l3ip_add_ut(ut_buff.ip_addr, &rslt_idx, true));

	}
	SHOW(fe_l3ip_print_range_ut(0, 120));
	CHECK_STEP;

	return 0;
}

/*
 * Unit Test procedure of FE table management
 */

void __exit cs_fe_table_ut_cleanup_module(void)
{
	return;
}

int __init cs_fe_table_ut_init_module(void)
{
	int i, ret = 0;

	/* start_idx and end_idx are 1-based range,
	   and we should change them to 0-based range. */
	start_idx--;
	end_idx--;

	/* register unit test plans into list */
	/* 1 */
	REG_UT_PLAN(cs_fe_table_an_bng_mac_ut_basic);
	REG_UT_PLAN(cs_fe_table_an_bng_mac_ut_boundary);
	REG_UT_PLAN(cs_fe_table_an_bng_mac_ut_mix);
	/* 4 */
	REG_UT_PLAN(cs_fe_table_lpb_ut_basic);
	REG_UT_PLAN(cs_fe_table_lpb_ut_boundary);
	REG_UT_PLAN(cs_fe_table_lpb_ut_mix);
	/* 7 */
	REG_UT_PLAN(cs_fe_table_class_ut_basic);
	REG_UT_PLAN(cs_fe_table_class_ut_boundary);
	REG_UT_PLAN(cs_fe_table_class_ut_mix);
	/* 10 */
	REG_UT_PLAN(cs_fe_table_sdb_ut_basic);
	REG_UT_PLAN(cs_fe_table_sdb_ut_boundary);
	REG_UT_PLAN(cs_fe_table_sdb_ut_mix);
	/* 13 */
	REG_UT_PLAN(cs_fe_table_fwdrslt_ut_basic);
	REG_UT_PLAN(cs_fe_table_fwdrslt_ut_boundary);
	REG_UT_PLAN(cs_fe_table_fwdrslt_ut_mix);
	/* 16 */
	REG_UT_PLAN(cs_fe_table_voqpol_ut_basic);
	REG_UT_PLAN(cs_fe_table_voqpol_ut_boundary);
	REG_UT_PLAN(cs_fe_table_voqpol_ut_mix);
	/* 19 */
	REG_UT_PLAN(cs_fe_table_hashhash_ut_basic);
	REG_UT_PLAN(cs_fe_table_hashhash_ut_boundary);
	REG_UT_PLAN(cs_fe_table_hashhash_ut_mix);
	/* 22 */
	REG_UT_PLAN(cs_fe_table_hashmask_ut_basic);
	REG_UT_PLAN(cs_fe_table_hashmask_ut_boundary);
	REG_UT_PLAN(cs_fe_table_hashmask_ut_mix);
	/* 25 */
	REG_UT_PLAN(cs_fe_table_hashoverflow_ut_basic);
	REG_UT_PLAN(cs_fe_table_hashoverflow_ut_boundary);
	REG_UT_PLAN(cs_fe_table_hashoverflow_ut_mix);
	/* 28 */
	REG_UT_PLAN(cs_fe_table_hashstatus_ut_basic);
	REG_UT_PLAN(cs_fe_table_hashstatus_ut_boundary);
	REG_UT_PLAN(cs_fe_table_hashstatus_ut_mix);
	/* 31 */
	REG_UT_PLAN(cs_fe_table_fvlan_ut_basic);
	REG_UT_PLAN(cs_fe_table_fvlan_ut_boundary);
	REG_UT_PLAN(cs_fe_table_fvlan_ut_mix);
	/* 34 */
	REG_UT_PLAN(cs_fe_table_pktlen_ut_basic);
	REG_UT_PLAN(cs_fe_table_pktlen_ut_boundary);
	REG_UT_PLAN(cs_fe_table_pktlen_ut_mix);
	/* 37 */
	REG_UT_PLAN(cs_fe_table_qosrslt_ut_basic);
	REG_UT_PLAN(cs_fe_table_qosrslt_ut_boundary);
	REG_UT_PLAN(cs_fe_table_qosrslt_ut_mix);
	/* 40 */
	REG_UT_PLAN(cs_fe_table_vlan_ut_basic);
	REG_UT_PLAN(cs_fe_table_vlan_ut_boundary);
	REG_UT_PLAN(cs_fe_table_vlan_ut_mix);
	/* 43 */
	REG_UT_PLAN(cs_fe_table_l2mac_ut_basic);
	REG_UT_PLAN(cs_fe_table_l2mac_ut_boundary);
	REG_UT_PLAN(cs_fe_table_l2mac_ut_mix);
	/* 46 */
	REG_UT_PLAN(cs_fe_table_l3ip_ut_basic);
	REG_UT_PLAN(cs_fe_table_l3ip_ut_boundary);
	REG_UT_PLAN(cs_fe_table_l3ip_ut_mix);

	/* execute all unit test plans */
	for (i = start_idx; i < end_idx; i++) {
		/* execute registed unit test plan */
		if (cs_fe_ut_plan[i] != 0) {
			printk("========ITEM %04d==========\n", i + 1);
			if ((ret = (cs_fe_ut_plan[i]) ()) != 0)
				goto init_fail;
		}
	}

	PRINT("Unit tests of FE tables are OK.\n\tExit Module !!\n");
	return -EAGAIN;

      init_fail:
	PRINT("Unit tests of FE tables are fail !!\n");
	cs_fe_table_ut_cleanup_module();
	return -EPERM;
}

module_init(cs_fe_table_ut_init_module);
module_exit(cs_fe_table_ut_cleanup_module);
MODULE_AUTHOR("Eric Wang <eric.wang@cortina-systems.com>");
MODULE_LICENSE("GPL");

#endif /* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) ||
	  defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */
