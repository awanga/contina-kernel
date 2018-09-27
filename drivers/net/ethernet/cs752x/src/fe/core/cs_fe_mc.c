/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_mc.c
 *
 * $Id: cs_fe_mc.c,v 1.4 2012/03/28 23:38:28 whsu Exp $
 *
 * This file maintain resource of MCGID/replication vectors
 * In arbitray replication mode, MCGID represents the index of an entry 
 * in NI.MCAL. 
 * Each entry is a 16-bits vector a.k.a replication vector or mc vector.
 * Number of "1"s of a replication vector means number of copies in next round.
 * Each copy of packet comes with an unique MCIDX corresponding to 1 bit in 
 * replication vector.
 * In port replication mode, MCGID represents the replication vector directly.
 */

#include <linux/errno.h>
#include <linux/export.h>
#include "cs_fe_mc.h"

struct cs_fe_mcg_resource_info cs_fe_mcg_tbl;

extern void cs_ni_set_mc_table(u8 mc_index, u16 mc_vec);
extern void cs_ni_get_mc_table(u8 mc_index);

int cs_fe_mcg_init(void)
{
	int i, j;

	for (i = 0; i < MCAL_TABLE_SIZE; i++) {
		cs_fe_mcg_tbl.mc_vec[i] = 0;
		cs_ni_set_mc_table(i, 0);
	}
	for (i = 0; i < MAX_MCGID_VTABLE; i++) {
		cs_fe_mcg_tbl.vtable_usage[i] = CS_MCGID_VTABLE_INVALID;
		for (j = 0; j < MAX_MCGID; j++)
			cs_fe_mcg_tbl.group_usage[i][j] =
			    CS_MCGID_GROUP_INVALID;
	}
	spin_lock_init(&cs_fe_mcg_tbl.lock);

#ifdef CS_FE_MC_DEBUG
	{
		int vtable = 0;
		int group = 0;
		int ret;
		int vector;
		for (i = 0; i < MAX_MCGID_VTABLE; i++) {
			ret = cs_fe_allocate_mcg_vtable_id(&vtable, 0);
			if (ret) {
				printk("Allocate V-Table[%d] fail, ret = %d\n", i, ret);
				continue;
			}
			else {
				printk("Allocate V-Table[%d] = %d successfully\n", i, vtable);
			}
				
			for (j = 0; j < MAX_MCGID; j++) {
				ret = cs_fe_allocate_mcg_group_id(vtable, &group);
				if (ret) {
					printk("Allocate group[%d] fail, ret = %d\n", j, ret);
					continue;
				}
				else {
					printk("Allocate group[%d] = %d successfully\n", j, group);
				}
				vector = i + j + 1;
				ret = cs_fe_set_mcg_mc_vec( vtable, group, vector);
				
				if (ret) {
					printk("Set mc vector[%d, %d] = %d fail, ret = %d\n", vtable, group, vector, ret);
					continue;
				}
				else {
					printk("Set mc vector[%d, %d] = %d  successfully\n", vtable, group, vector);
				}

				ret = cs_fe_get_mcg_mc_vec( vtable, group, &vector);
				
				if (ret) {
					printk("Get mc vector[%d, %d] fail, ret = %d\n", vtable, group, ret);
					continue;
				}
				else {
					printk("Get mc vector[%d, %d] = %d  successfully\n", vtable, group, vector);
				}
			}

			
		}

		printk("***Get MCAL table***\n");
		for (i = 0; i < MCAL_TABLE_SIZE; i++)
			cs_ni_get_mc_table(i);

		printk("***free V-Table 6\n");
		ret = cs_fe_free_mcg_vtable_id(6);
		
		if (ret) {
			printk("Free V-Table[6] fail, ret = %d\n", ret);
		}
		else {
			printk("Free V-Table[6] successfully\n");
		}

		printk("***free group 8 of V-Table 2\n");
		ret = cs_fe_free_mcg_group_id(2, 8);

		if (ret) {
			printk("Free group[2, 8] fail, ret = %d\n", ret);
		}
		else {
			printk("Free group[2, 8] successfully\n");
		}

		printk("***Get MCAL table***\n");
		for (i = 0; i < MCAL_TABLE_SIZE; i++)
			cs_ni_get_mc_table(i);
	
		
	}
#endif /* CS_FE_MC_DEBUG */
	return 0;
}
EXPORT_SYMBOL(cs_fe_mcg_init);

int cs_fe_allocate_mcg_vtable_id(int *vtable_id, int forced)
{
	int i;

	spin_lock(&(cs_fe_mcg_tbl.lock));
	if (forced) {
		if (*vtable_id >= MAX_MCGID_VTABLE ||
		    cs_fe_mcg_tbl.vtable_usage[*vtable_id] !=
		    CS_MCGID_VTABLE_INVALID) {
			spin_unlock(&(cs_fe_mcg_tbl.lock));
			return -EPERM;
		}
	} else {
		for (i = 0; i < MAX_MCGID_VTABLE; i++)
			if (cs_fe_mcg_tbl.vtable_usage[i] ==
			    CS_MCGID_VTABLE_INVALID)
				break;
		if (i == MAX_MCGID_VTABLE) {
			spin_unlock(&cs_fe_mcg_tbl.lock);
			return -EPERM;
		}
		*vtable_id = i;
	}
	cs_fe_mcg_tbl.vtable_usage[*vtable_id] = CS_MCGID_VTABLE_VALID;
	spin_unlock(&cs_fe_mcg_tbl.lock);
	return 0;
}
EXPORT_SYMBOL(cs_fe_allocate_mcg_vtable_id);

int cs_fe_free_mcg_vtable_id(int vtable_id)
{
	int i, j;

	spin_lock(&cs_fe_mcg_tbl.lock);
	if (cs_fe_mcg_tbl.vtable_usage[vtable_id] != CS_MCGID_VTABLE_VALID) {
		spin_unlock(&cs_fe_mcg_tbl.lock);
		return -EINVAL;
	}
	cs_fe_mcg_tbl.vtable_usage[vtable_id] = CS_MCGID_VTABLE_INVALID;

	for (i = 0; i < MAX_MCGID; i++) {
		j = vtable_id * MCG_INCREMENT + i;
		if (cs_fe_mcg_tbl.group_usage[vtable_id][i] ==
		    CS_MCGID_GROUP_VALID) {
			cs_fe_mcg_tbl.group_usage[vtable_id][i] = 
				CS_MCGID_GROUP_INVALID;
			cs_fe_mcg_tbl.mc_vec[j] = 0;
			cs_ni_set_mc_table(j, 0);
		}
	}
	spin_unlock(&cs_fe_mcg_tbl.lock);
	return 0;
}
EXPORT_SYMBOL(cs_fe_free_mcg_vtable_id);

int cs_fe_allocate_mcg_group_id(int vtable_id, int *group_id)
{
	int i;

	spin_lock(&(cs_fe_mcg_tbl.lock));
	for (i = 0; i < MAX_MCGID; i++)
		if (cs_fe_mcg_tbl.group_usage[vtable_id][i] ==
		    CS_MCGID_GROUP_INVALID)
			break;
	if (i == MAX_MCGID) {
		spin_unlock(&cs_fe_mcg_tbl.lock);
		return -EPERM;
	}
	*group_id = i;
	cs_fe_mcg_tbl.group_usage[vtable_id][*group_id] = CS_MCGID_GROUP_VALID;
	spin_unlock(&cs_fe_mcg_tbl.lock);
	return 0;
}
EXPORT_SYMBOL(cs_fe_allocate_mcg_group_id);

int cs_fe_free_mcg_group_id(int vtable_id, int group_id)
{
	int i;
	spin_lock(&cs_fe_mcg_tbl.lock);
	if (cs_fe_mcg_tbl.group_usage[vtable_id][group_id] !=
	    CS_MCGID_GROUP_VALID) {
		spin_unlock(&cs_fe_mcg_tbl.lock);
		return -EINVAL;
	}
	cs_fe_mcg_tbl.group_usage[vtable_id][group_id] = CS_MCGID_GROUP_INVALID;

	i = vtable_id * MCG_INCREMENT + group_id;
	cs_fe_mcg_tbl.mc_vec[i] = 0;
	cs_ni_set_mc_table(i, 0);
	spin_unlock(&cs_fe_mcg_tbl.lock);
	return 0;
}
EXPORT_SYMBOL(cs_fe_free_mcg_group_id);

int cs_fe_set_mcg_mc_vec(int vtable_id, int group_id, int mc_vec)
{
	int i;

	spin_lock(&cs_fe_mcg_tbl.lock);
	if (cs_fe_mcg_tbl.vtable_usage[vtable_id] != CS_MCGID_VTABLE_VALID ||
	    cs_fe_mcg_tbl.group_usage[vtable_id][group_id] !=
	    CS_MCGID_GROUP_VALID ||
	    mc_vec > CS_FE_MC_ARBITRARY_MAX) {
		spin_unlock(&cs_fe_mcg_tbl.lock);
		return -EINVAL;
	}

	i = vtable_id * MCG_INCREMENT + group_id;
	cs_fe_mcg_tbl.mc_vec[i] = mc_vec;
	cs_ni_set_mc_table(i, mc_vec);
	spin_unlock(&cs_fe_mcg_tbl.lock);
	return 0;
}
EXPORT_SYMBOL(cs_fe_set_mcg_mc_vec);

int cs_fe_get_mcg_mc_vec(int vtable_id, int group_id, int *mc_vec)
{
	spin_lock(&cs_fe_mcg_tbl.lock);
	if (cs_fe_mcg_tbl.vtable_usage[vtable_id] != CS_MCGID_VTABLE_VALID ||
	    cs_fe_mcg_tbl.group_usage[vtable_id][group_id] !=
	    CS_MCGID_GROUP_VALID) {
		spin_unlock(&cs_fe_mcg_tbl.lock);
		return -EINVAL;
	}

	*mc_vec = cs_fe_mcg_tbl.mc_vec[vtable_id * MCG_INCREMENT + group_id];
	spin_unlock(&cs_fe_mcg_tbl.lock);
	return 0;
}
EXPORT_SYMBOL(cs_fe_get_mcg_mc_vec);

int cs_fe_allocate_port_rep_mcgid(int *mcgid, int port_map)
{
	if (port_map > CS_FE_MC_PORT_MAX)
		return -EINVAL;

	*mcgid = 0x100 | port_map;
	return 0;
}
EXPORT_SYMBOL(cs_fe_allocate_port_rep_mcgid);

