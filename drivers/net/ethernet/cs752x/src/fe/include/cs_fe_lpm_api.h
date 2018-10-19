/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_lpm_drv_api.h
 *
 * $Id: cs_fe_lpm_api.h,v 1.1 2011/10/12 01:55:21 bhsieh Exp $
 *
 * It contains PUBLIC prototypes for LPM APIs
 */
#ifndef __CS_LPM_API_H__
#define __CS_LPM_API_H__
#include <mach/cs75xx_fe_core_table.h>

/*  LPM table :
 *
 *   Col3    Col2     Col1    Col0
 * 64 entries to store up to 64 IPv4 route rules
 * One IPv6 rule occupy 4 entries (4 entry alignment)
 * There are ip,mask,priority,hash index per route rule.
 * There are also 64 hit count which map to 64 LPM entries.
 */
#define MAX_LPM_ENTRY_CNT4          64
#define MAX_LPM_ENTRY_CNT6          16

typedef struct lpm_entity_info
{
	u8 is_empty;	/* Only useful in hw_cs_get_lpm_entity() to check if entity be occupied
				  0 -> this entry was occupied
				  1 -> this entry is empty */
	u8 is_v6;		/* Caller use this to indicate if IP is V6
				  0 -> IPv4
				  1 -> IPv6 */
	union
	{
		u8	addr8[16];
		u16	addr16[8];
		u32	addr32[4];
	} ip_u;
	u8 index;		/* maintain by lower driver*/
	u8 mask;		/* mask length */
	u8 priority;	/* 0~63, 0 is highest priority */
	u16 hash_index;	/* 0~8191 */
	u64 hit_count;	/* 0~0xFFFFFFFFFFFFFFFF
				  return by lower api, may need to overwrite when swap table */
} lpm_entity_info_t;

struct in4_addr
{
	union
	{
		u8		u6_addr8[4];
		u32	u6_addr32[1];
	} in4_u;
	u8 index;		/* mantain by lower driver*/
	u8 mask;		/* mask length */
	u8 priority;	/* 0~63 */
	u16 hash_index;	/* 0~8191 */
	u32 hit_count;	/* 0~0x0FFFFFFF */
};

/* This layer use this structure to maintain lpm table */
typedef struct lpm_tbl
{
	int init_done;	/* 0 -> not init yet
				   1 -> init complete */
	u8 active_tlb;	/* 1 -> table_0 active , table_1 shadow
				   2 -> table_0 shadow , table_1 active */
	u8 v4_entry_cnt;	/* number of IPv4 entry in lpm table */
	u8 v6_entry_cnt;	/* number of IPv6 entry in lpm table */
	lpm_entity_info_t	lpm_entries[MAX_LPM_ENTRY_CNT4];
} lpm_tbl_t;

/* Initialize lpm module, debug_mode means Start/End pointer will specified in lpm register instead of SDB rule */
extern int cs_lpm_init(u8 debug_mode);

/* Get lpm module status, returned whole table and some status  */
extern int cs_lpm_get_status(lpm_tbl_t *lpm_table);

/* Add one rule */
extern int cs_lpm_add_entry(lpm_entity_info_t *entity);

/* Delete specified rule if exist */
extern int cs_lpm_delete_entry(lpm_entity_info_t *entity);

/* Grab specific rule(index no.) */
extern int cs_lpm_get_entity_by_idx(u8 index, lpm_entity_info_t *entity);

/* Grab specific rule(rule content) */
extern int cs_lpm_get_entity_by_rule(lpm_entity_info_t *entity);

/* when table be modified, we may need to change SDB table too.
 * After SDB and other related table updated, we can swap shadow table to active
 */
extern int cs_lpm_active_table(u8 table);

/* Disable LPM search engine */
extern int cs_lpm_disable(void);

/* Search table */
cs_int8 cs_lpm_search_entry(lpm_entity_info_t *entity);

/* delete index from LPM table */
int cs_lpm_delete_index(u8 index);

/* Update whole SW table to HW LPM registers  */
int cs_lpm_flush_all(u8 table);

/* Flush entity to HW register */
int cs_lpm_flush_entity(lpm_entity_info_t *entity,u8 table);


/* api for ioctl */
int cs_fe_ioctl_lpm(struct net_device *dev, void *pdata, void *cmd);

/* debug_Aaron */
int cs_lpm_print_entry(unsigned int idx);
void cs_lpm_swap_active_table(void);	
int cs_lpm_range_add_entry(int ipv6,int start_ptr,
                int end_ptr,lpm_entity_info_t *entity);

#endif /* __CS_LPM_API_H__ */
