/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_core.c
 *
 * $Id: cs_fe_table_core.c,v 1.4 2011/05/14 02:49:37 whsu Exp $
 *
 * It contains the core implementation for controlling FE tables.
 */

#include <linux/module.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"

extern int cs_fe_table_manager_init(void);
extern int cs_fe_table_an_bng_mac_init(void);
extern int cs_fe_table_class_init(void);
extern int cs_fe_table_fwdrslt_init(void);
extern int cs_fe_table_hashhash_init(void);
extern int cs_fe_table_hashmask_init(void);
extern int cs_fe_table_hashoverflow_init(void);
extern int cs_fe_table_hashstatus_init(void);
extern int cs_fe_table_l2mac_init(void);
extern int cs_fe_table_l3ip_init(void);
extern int cs_fe_table_lpb_init(void);
extern int cs_fe_table_sdb_init(void);
extern int cs_fe_table_vlan_init(void);
extern int cs_fe_table_voqpol_init(void);
extern int cs_fe_table_acl_init(void);
extern int cs_fe_table_etype_init(void);
extern int cs_fe_table_fvlan_init(void);
extern int cs_fe_table_hashcheck_init(void);
extern int cs_fe_table_llchdr_init(void);
extern int cs_fe_table_pktlen_init(void);
extern int cs_fe_table_qosrslt_init(void);
extern int cs_fe_table_voqdrp_init(void);
extern void cs_fe_table_an_bng_mac_exit(void);
extern void cs_fe_table_class_exit(void);
extern void cs_fe_table_fwdrslt_exit(void);
extern void cs_fe_table_hashhash_exit(void);
extern void cs_fe_table_hashmask_exit(void);
extern void cs_fe_table_hashoverflow_exit(void);
extern void cs_fe_table_hashstatus_exit(void);
extern void cs_fe_table_l2mac_exit(void);
extern void cs_fe_table_l3ip_exit(void);
extern void cs_fe_table_lpb_exit(void);
extern void cs_fe_table_sdb_exit(void);
extern void cs_fe_table_vlan_exit(void);
extern void cs_fe_table_voqpol_exit(void);
extern void cs_fe_table_acl_exit(void);
extern void cs_fe_table_etype_exit(void);
extern void cs_fe_table_fvlan_exit(void);
extern void cs_fe_table_hashcheck_exit(void);
extern void cs_fe_table_llchdr_exit(void);
extern void cs_fe_table_pktlen_exit(void);
extern void cs_fe_table_qosrslt_exit(void);
extern void cs_fe_table_voqdrp_exit(void);

int cs_fe_table_init(void)
{
	int ret = 0;

	if (cs_fe_table_manager_init() != 0) {
		printk("%s: table manager initialization failed!\n", __func__);
		return -1;
	}

	/* Must needed FE Tables */
	if (cs_fe_table_an_bng_mac_init() != 0) {
		printk("%s:AN_BNG_MAC table initialization failed!\n",
				__func__);
		ret = -1;
	}

	if (cs_fe_table_class_init() != 0) {
		printk("%s:CLASSIFIER table initialization failed!\n",
				__func__);
		ret = -1;
	}

	if (cs_fe_table_fwdrslt_init() != 0) {
		printk("%s:FWDRSLT table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_hashhash_init() != 0) {
		printk("%s:Hash Hash table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_hashmask_init() != 0) {
		printk("%s:Hash Mask table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_hashoverflow_init() != 0) {
		printk("%s:Hash Overflow table initialization failed!\n",
				__func__);
		ret = -1;
	}

	if (cs_fe_table_hashstatus_init() != 0) {
		printk("%s:Hash Status table initialization failed!\n",
				__func__);
		ret = -1;
	}

	if (cs_fe_table_l2mac_init() != 0) {
		printk("%s:L2 MAC table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_l3ip_init() != 0) {
		printk("%s:L3 IP table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_lpb_init() != 0) {
		printk("%s:LPB table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_sdb_init() != 0) {
		printk("%s:SDB table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_vlan_init() != 0) {
		printk("%s:VLAN table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_voqpol_init() != 0) {
		printk("%s:VOQ Policer table initialization failed!\n",
				__func__);
		ret = -1;
	}

	/* Tables that might not be needed */
	if (cs_fe_table_acl_init() != 0) {
		printk("%s:ACL table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_etype_init() != 0) {
		printk("%s:FWDRSLT table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_fvlan_init() != 0) {
		printk("%s:Flow VLAN table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_hashcheck_init() != 0) {
		printk("%s:Hash Check table initialization failed!\n",
				__func__);
		ret = -1;
	}

	if (cs_fe_table_llchdr_init() != 0) {
		printk("%s:LLC HDR table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_pktlen_init() != 0) {
		printk("%s:Packet Length Range Vector table initialization "
				"failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_qosrslt_init() != 0) {
		printk("%s:QOSRSLT table initialization failed!\n", __func__);
		ret = -1;
	}

	if (cs_fe_table_voqdrp_init() != 0) {
		printk("%s:PE VOQ DROP table initialization failed!\n",
				__func__);
		ret = -1;
	}

	return ret;
} /* cs_fe_table_init */

void cs_fe_table_exit(void)
{
	/* Must needed FE Tables */
	cs_fe_table_an_bng_mac_exit();
	cs_fe_table_class_exit();
	cs_fe_table_fwdrslt_exit();
	cs_fe_table_hashhash_exit();
	cs_fe_table_hashmask_exit();
	cs_fe_table_hashoverflow_exit();
	cs_fe_table_hashstatus_exit();
	cs_fe_table_l2mac_exit();
	cs_fe_table_l3ip_exit();
	cs_fe_table_lpb_exit();
	cs_fe_table_sdb_exit();
	cs_fe_table_vlan_exit();
	cs_fe_table_voqpol_exit();
	/* Tables that might not be needed */
	cs_fe_table_acl_exit();
	cs_fe_table_etype_exit();
	cs_fe_table_fvlan_exit();
	cs_fe_table_hashcheck_exit();
	cs_fe_table_llchdr_exit();
	cs_fe_table_pktlen_exit();
	cs_fe_table_qosrslt_exit();
	cs_fe_table_voqdrp_exit();
} /* cs_fe_table_exit */

