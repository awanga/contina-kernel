/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_table_util.c
 *
 * $Id: cs_fe_table_util.c,v 1.5 2011/12/27 08:56:45 bhsieh Exp $
 *
 * It contains the utility API implementation for controlling FE tables.
 */

#include <linux/module.h>
#include <linux/types.h>
#include "cs_fe.h"
#include "cs_fe_table_int.h"

static cs_fe_table_t *cs_fe_table_manager[FE_TABLE_MAX];

int cs_fe_table_register(cs_fe_hw_table_e type_id, cs_fe_table_t *fe_tbl)
{
	if (cs_fe_table_manager[type_id] != NULL)
		return FE_TABLE_EDUPLICATE;
	cs_fe_table_manager[type_id] = fe_tbl;
	return FE_TABLE_OK;
} /* cs_fe_table_register */

int cs_fe_table_unregister(unsigned int type_id)
{
	if (cs_fe_table_manager[type_id] == NULL)
		return FE_TABLE_ETBLNOTEXIST;
	cs_fe_table_manager[type_id] = NULL;
	return FE_TABLE_OK;
} /* cs_fe_table_unregister */

bool cs_fe_table_is_registered(cs_fe_hw_table_e type_id)
{
	if (cs_fe_table_manager[type_id] != NULL)
		return true;
	else return false;
} /* cs_fe_table_is_registered */

cs_fe_table_t *cs_fe_table_get(cs_fe_hw_table_e type_id)
{
	return cs_fe_table_manager[type_id];
} /* cs_fe_table_get */

int cs_fe_table_manager_init(void)
{
	int i;

	for (i = 0; i < FE_TABLE_MAX; i++)
		cs_fe_table_manager[i] = NULL;
	return FE_TABLE_OK;
} /* cs_fe_table_manager_init */
EXPORT_SYMBOL(cs_fe_table_manager_init);

int cs_fe_table_alloc_entry(cs_fe_hw_table_e type_id, unsigned int *rslt_idx,
		unsigned int start_offset)
{
	cs_fe_table_t *p_fe_tbl;

	p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.alloc_entry != NULL)
		return p_fe_tbl->op.alloc_entry(rslt_idx, start_offset);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_alloc_entry */

int cs_fe_table_set_entry(cs_fe_hw_table_e type_id, unsigned int idx,
		void *entry)
{
	cs_fe_table_t *p_fe_tbl;

	p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.set_entry != NULL)
		return p_fe_tbl->op.set_entry(idx, entry);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_set_entry */

int cs_fe_table_add_entry(cs_fe_hw_table_e type_id, void *entry,
		unsigned int *rslt_idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.add_entry != NULL) {
		return p_fe_tbl->op.add_entry(entry, rslt_idx);
	} else {
#if 1
		return FE_TABLE_EOPNOTSUPP;
#else
		if ((p_fe_tbl->op.set_entry != NULL) &&
				(p_fe_tbl->op.alloc_entry != NULL) &&
				(p_fe_tbl->op.find_entry != NULL) &&
				(p_fe_tbl->op.inc_entry_refcnt != NULL)) {
			int ret_code;

			rdel_entryet_code = p_fe_tbl->op.find_entry(entry,
					rslt_idx, 0);
			if (ret_code < 0) {
				/* error performing find */
				return ret_code;
			} else if (ret_code == 0) {
				/* found a matching entry */
				p_fe_tbl->op.inc_entry_refcnt(*rslt_idx);
				return 0;
			} else {
				/* did not find a matching entry */
				ret_code = p_fe_tbl->op.alloc_entry(
						rslt_idx, 0);
				if (ret_code != 0)
					return ret_code;

				ret_code = p_fe_tbl->op.set_entry(*rslt_idx,
						entry);
				if (ret_code != 0)
					return ret_code;

				return p_fe_tbl->op.inc_entry_refcnt(*rslt_idx);
			}
		} else {
			return FE_TABLE_EOPNOTSUPP;
		}
#endif
	}
} /* cs_fe_table_add_entry */

int cs_fe_table_del_entry_by_idx(cs_fe_hw_table_e type_id, unsigned int idx,
		bool f_force)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.del_entry_by_idx != NULL)
		return p_fe_tbl->op.del_entry_by_idx(idx, f_force);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_del_entry_by_idx */

int cs_fe_table_del_entry(cs_fe_hw_table_e type_id, void *entry, bool f_force)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.del_entry != NULL)
		return p_fe_tbl->op.del_entry(entry, f_force);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_del_entry */

int cs_fe_table_find_entry(cs_fe_hw_table_e type_id, void *entry,
		unsigned int *rslt_idx, unsigned int start_offset)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.find_entry != NULL)
		return p_fe_tbl->op.find_entry(entry, rslt_idx, start_offset);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_find_entry */

int cs_fe_table_get_entry(cs_fe_hw_table_e type_id, unsigned int idx,
		void *entry)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_entry != NULL)
		return p_fe_tbl->op.get_entry(idx, entry);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_get_entry */

int cs_fe_table_inc_entry_refcnt(cs_fe_hw_table_e type_id, unsigned int idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.inc_entry_refcnt != NULL)
		return p_fe_tbl->op.inc_entry_refcnt(idx);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_inc_entry_refcnt */

int cs_fe_table_dec_entry_refcnt(cs_fe_hw_table_e type_id, unsigned int idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.dec_entry_refcnt != NULL)
		return p_fe_tbl->op.dec_entry_refcnt(idx);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_dec_entry_refcnt */

int cs_fe_table_get_entry_refcnt(cs_fe_hw_table_e type_id, unsigned int idx,
		unsigned int *p_cnt)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_entry_refcnt != NULL)
		return p_fe_tbl->op.get_entry_refcnt(idx, p_cnt);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_get_entry_refcnt */

int cs_fe_table_set_field(cs_fe_hw_table_e type_id, unsigned int idx,
		unsigned int field, __u32 *p_value)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.set_field != NULL)
	//if (p_fe_tbl->op.get_entry_refcnt != NULL)
		return p_fe_tbl->op.set_field(idx, field, p_value);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_set_field */

int cs_fe_table_get_field(cs_fe_hw_table_e type_id, unsigned int idx,
		unsigned int field, __u32 *p_value)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_field != NULL)
	//if (p_fe_tbl->op.get_entry_refcnt != NULL)
		return p_fe_tbl->op.get_field(idx, field, p_value);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_get_field */

int cs_fe_table_flush_table(cs_fe_hw_table_e type_id)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.flush_table != NULL)
		return p_fe_tbl->op.flush_table();
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_flush_table */

int cs_fe_table_get_avail_count(cs_fe_hw_table_e type_id)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_avail_count != NULL)
		return p_fe_tbl->op.get_avail_count();
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_get_avail_count */

int cs_fe_table_print_entry(cs_fe_hw_table_e type_id, unsigned int idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.print_entry != NULL) {
		p_fe_tbl->op.print_entry(idx);
		return FE_TABLE_OK;
	} else {
		return FE_TABLE_EOPNOTSUPP;
	}
} /* cs_fe_table_print_entry */

int cs_fe_table_print_range(cs_fe_hw_table_e type_id, unsigned int start_idx,
		unsigned int end_idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.print_range != NULL) {
		p_fe_tbl->op.print_range(start_idx, end_idx);
		return FE_TABLE_OK;
	} else {
		return FE_TABLE_EOPNOTSUPP;
	}
} /* cs_fe_table_print_range */

int cs_fe_table_print_table(cs_fe_hw_table_e type_id)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.print_table != NULL) {
		p_fe_tbl->op.print_table();
		return FE_TABLE_OK;
	} else {
		return FE_TABLE_EOPNOTSUPP;
	}
} /* cs_fe_table_print_table */

int cs_fe_table_print_table_used_count(cs_fe_hw_table_e type_id)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(type_id);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_avail_count != NULL)
		printk("\t avail_entry=%d ",
			p_fe_tbl->op.get_avail_count());
	printk("\t\t used_entry=%d \t\t max_entry=%d \n",
		p_fe_tbl->used_entry, p_fe_tbl->max_entry);
	return FE_TABLE_OK;
} /* cs_fe_table_print_table_used_count */


/* L2 result table specific */
int cs_fe_table_add_l2_mac(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L2_MAC);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.add_l2_mac != NULL)
		return p_fe_tbl->op.add_l2_mac(p_sa, p_da, p_idx);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_add_l2_mac */

int cs_fe_table_del_l2_mac(unsigned int idx, bool f_sa, bool f_da)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L2_MAC);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.del_l2_mac != NULL)
		return p_fe_tbl->op.del_l2_mac(idx, f_sa, f_da);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_del_l2_mac */

int cs_fe_table_find_l2_mac(unsigned char *p_sa, unsigned char *p_da,
		unsigned int *p_idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L2_MAC);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.find_l2_mac != NULL)
		return p_fe_tbl->op.find_l2_mac(p_sa, p_da, p_idx);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_find_l2_mac */

int cs_fe_table_get_l2_mac(unsigned int idx, unsigned char *p_sa,
		unsigned char *p_da)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L2_MAC);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_l2_mac != NULL)
		return p_fe_tbl->op.get_l2_mac(idx, p_sa, p_da);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_get_l2_mac */

int cs_fe_table_inc_l2_mac_refcnt(unsigned int idx, bool f_sa, bool f_da)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L2_MAC);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.inc_l2_mac_refcnt != NULL)
		return p_fe_tbl->op.inc_l2_mac_refcnt(idx, f_sa, f_da);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_inc_l2_mac_refcnt */

/* L3 result table specific */
int cs_fe_table_add_l3_ip(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L3_IP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.add_l3_ip != NULL)
		return p_fe_tbl->op.add_l3_ip(p_ip_addr, p_idx, f_is_v6);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_add_l3_ip */

int cs_fe_table_del_l3_ip(unsigned int idx, bool f_is_v6)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L3_IP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.del_l3_ip != NULL)
		return p_fe_tbl->op.del_l3_ip(idx, f_is_v6);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_del_l3_ip */

int cs_fe_table_find_l3_ip(__u32 *p_ip_addr, unsigned int *p_idx, bool f_is_v6)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L3_IP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.find_l3_ip != NULL)
		return p_fe_tbl->op.find_l3_ip(p_ip_addr, p_idx, f_is_v6);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_find_l3_ip */

int cs_fe_table_get_l3_ip(unsigned int idx, __u32 *p_ip_addr, bool f_is_v6)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L3_IP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_l3_ip != NULL)
		return p_fe_tbl->op.get_l3_ip(idx, p_ip_addr, f_is_v6);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_get_l3_ip */

int cs_fe_table_inc_l3_ip_refcnt(unsigned int idx, bool f_is_v6)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_L3_IP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.inc_l3_ip_refcnt != NULL)
		return p_fe_tbl->op.inc_l3_ip_refcnt(idx, f_is_v6);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_inc_l3_ip_refcnt */

/* PE VOQDRP table specific */
int cs_fe_table_enbl_voqdrp(unsigned int voq_idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_PE_VOQ_DROP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.enbl_voqdrp != NULL)
		return p_fe_tbl->op.enbl_voqdrp(voq_idx);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_enbl_voqdrp */

int cs_fe_table_dsbl_voqdrp(unsigned int voq_idx)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_PE_VOQ_DROP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.dsbl_voqdrp != NULL)
		return p_fe_tbl->op.dsbl_voqdrp(voq_idx);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_enbl_voqdrp */

int cs_fe_table_get_voqdrp(unsigned int voq_idx, __u8 *p_enbl)
{
	cs_fe_table_t *p_fe_tbl = cs_fe_table_get(FE_TABLE_PE_VOQ_DROP);
	if (p_fe_tbl == NULL)
		return FE_TABLE_ETBLNOTEXIST;

	if (p_fe_tbl->op.get_voqdrp != NULL)
		return p_fe_tbl->op.get_voqdrp(voq_idx, p_enbl);
	else
		return FE_TABLE_EOPNOTSUPP;
} /* cs_fe_table_enbl_voqdrp */

