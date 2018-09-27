/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_lpm_drv_api.c
 * This file contains the API for LPM module.
 *
 *
 * $Id: cs_fe_table_lpm.c,v 1.4 2012/08/10 01:50:25 whsu Exp $
 *
 */

#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/types.h>
#include "../ni/cs752x_ioctl.h"
#include "cs_fe_lpm_api.h"

#define LPM_INIT_TIMEOUT   1000     /* 1 second or trying count */

lpm_tbl_t lpm_table_info;

void cs_lpm_update_sdb_end_ptr(int is_ipv6, int end_ptr, int is_lpm_ptr_en);

/*
* Convert mask to pattern
*/
u32 cs_lpm_gen_ip_mask_v4(u8 mask)
{
	u8 i;
	u32 mask_pattern=0;

	for (i = 0; i < 32; i++, mask--) {
		if (mask == 0)
			break;
		mask_pattern |= 1 << (31 - i);
	}

	return mask_pattern;
}

int cs_lpm_init(u8 debug_mode)
{
	FETOP_LPM_MEM_STATUS_t lpm_mem_status;
	FETOP_LPM_MEM_INIT_t lpm_mem_init;
	FETOP_LPM_LPM_CONFIG_t lpm_config;
	int i, try_loop = 0;

	do {
		lpm_mem_status.wrd = readl(FETOP_LPM_MEM_STATUS);
		try_loop++;
		mdelay(1);
	} while (((lpm_mem_status.wrd & 0xFFFF) != 0x00FF) &&
			(try_loop < LPM_INIT_TIMEOUT));

	if (try_loop >= LPM_INIT_TIMEOUT) {
		printk("lpm_mem_status:0x%x\n", lpm_mem_status.wrd);
		printk("Something not ready to init!\n");
		return FE_TABLE_EOPNOTSUPP;
	}

	try_loop = 0;
	lpm_mem_init.bf.lpm0_lower_start_init = 1;
	lpm_mem_init.bf.lpm0_upper_start_init = 1;
	lpm_mem_init.bf.lpm1_upper_start_init = 1;
	lpm_mem_init.bf.lpm1_lower_start_init = 1;
	lpm_mem_init.bf.hc0_lower_start_init = 1;
	lpm_mem_init.bf.hc1_lower_start_init = 1;
	lpm_mem_init.bf.hc0_upper_start_init = 1;
	lpm_mem_init.bf.hc1_upper_start_init = 1;

	writel(lpm_mem_init.wrd, FETOP_LPM_MEM_INIT);
	do {
		lpm_mem_status.wrd = readl(FETOP_LPM_MEM_STATUS);
		try_loop++;
		mdelay(1);
	} while (((lpm_mem_status.wrd & 0xFF00) != 0x0000) &&
			(try_loop < LPM_INIT_TIMEOUT));

	if (try_loop >= LPM_INIT_TIMEOUT) {
		printk("lpm_mem_status:0x%x\n", lpm_mem_status.wrd);
		printk("Initialize fail !\n");
		return FE_TABLE_EOPNOTSUPP;
	}

	lpm_config.wrd = readl(FETOP_LPM_LPM_CONFIG);
	lpm_config.bf.sdb_en = (debug_mode == 1)? 0:1;
	lpm_config.bf.active_table = 1;
	writel(lpm_config.wrd, FETOP_LPM_LPM_CONFIG);

	lpm_table_info.init_done = 1;
	lpm_table_info.active_tlb = 1;

	/* Init lpm_table info structure */
	memset(&lpm_table_info, 0, sizeof(lpm_tbl_t));
	for (i = 0; i < MAX_LPM_ENTRY_CNT4; i++)
		lpm_table_info.lpm_entries[i].is_empty = 1;

	return FE_TABLE_OK;
}

int cs_lpm_get_status(lpm_tbl_t *lpm_table)
{
	u16 i;
	lpm_entity_info_t entity;

	for (i = 0; i < MAX_LPM_ENTRY_CNT4; i++) {
		if (lpm_table_info.lpm_entries[i].is_empty == 0)
			cs_lpm_get_entity_by_idx(i, &entity);
	}

	memcpy(lpm_table, &lpm_table_info, sizeof(lpm_tbl_t));

	return FE_TABLE_OK;
}

int cs_lpm_range_add_entry(int ipv6, int start_ptr, int end_ptr, lpm_entity_info_t *entity)
{
	int start_index, end_index;
	int i, j;
	u32 ip1, ip2;
	u8 mask1, mask2;

#if 0
	if (ipv6 == 1) {
		/*ipv6 search direction is high 16 to low 0*/
		start_index = start_ptr * 4;
		end_index = end_ptr * 4;
	} else {
		start_index = ((start_ptr > 2) & 0x7) * 4 + (start_ptr & 0x3);
		end_index = ((end_ptr > 2) & 0x7) * 4 + (end_ptr & 0x3);
	}
#endif

	if ((entity->is_v6) && (ipv6 == 0))
		return FE_TABLE_EOPNOTSUPP;

	if (entity->is_v6) 
	{
		start_index = MAX_LPM_ENTRY_CNT4 - 1;
		end_index = MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt - 1;

		/* check if the LPM table full */
		if ((MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt * 4) < (lpm_table_info.v4_entry_cnt + 4)) {
			printk("%s: IPv6 table full, lpm_table_info.v6_entry_cnt=%d, lpm_table_info.v4_entry_cnt=%d\n", 
				__func__, lpm_table_info.v6_entry_cnt, lpm_table_info.v4_entry_cnt);
			return FE_TABLE_ETBLFULL;
		}
	}
	else
	{
		start_index = 0;
		end_index = lpm_table_info.v4_entry_cnt;
	
		if (lpm_table_info.v4_entry_cnt > (MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt * 4)) {
		   	printk("%s: IPv4 table full, lpm_table_info.v6_entry_cnt=%d, lpm_table_info.v4_entry_cnt=%d\n",
                                __func__, lpm_table_info.v6_entry_cnt, lpm_table_info.v4_entry_cnt);
                        return FE_TABLE_ETBLFULL;
                }
	}

	if (lpm_table_info.lpm_entries[end_index].is_empty == 0)
	{
		printk("%s: LPM table is full, end_index=%d is not empty!!\n", __func__, end_index);
		return FE_TABLE_ETBLFULL;
	}

	if (entity->is_v6) {	/* IPv6 LPM*/
		i = start_index;
		mask1 = entity->mask;
		/* find start position of mask group */
		while ((i >= end_index) &&
				(lpm_table_info.lpm_entries[i].is_empty == 0) &&
				(lpm_table_info.lpm_entries[i].mask >= entity->mask)) {
			/* Ensure that there is no duplicated entry */
			if (lpm_table_info.lpm_entries[i].mask == entity->mask) {
				if (entity->mask > 96) {	/* Just compare addr32[0~3] */
					if ((entity->ip_u.addr32[3] == lpm_table_info.lpm_entries[i].ip_u.addr32[3]) &&
							(entity->ip_u.addr32[2] == lpm_table_info.lpm_entries[i].ip_u.addr32[2])&&
							(entity->ip_u.addr32[1] == lpm_table_info.lpm_entries[i].ip_u.addr32[1])&&
							((entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(128 - mask1)) == (lpm_table_info.lpm_entries[i].ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(128 - mask1))))
						return CS_ERROR;

				} else if (entity->mask > 64) {	/* Compare addr32[1~3] */
					if ((entity->ip_u.addr32[3] == lpm_table_info.lpm_entries[i].ip_u.addr32[3]) &&
							(entity->ip_u.addr32[2] == lpm_table_info.lpm_entries[i].ip_u.addr32[2]) &&
							((entity->ip_u.addr32[1] & cs_lpm_gen_ip_mask_v4(64 - mask1)) == (lpm_table_info.lpm_entries[i].ip_u.addr32[1] & cs_lpm_gen_ip_mask_v4(64 - mask1))))
						return CS_ERROR;

				} else if (entity->mask > 32) {	/* Compare addr32[2~3] */
					if ((entity->ip_u.addr32[3] == lpm_table_info.lpm_entries[i].ip_u.addr32[3]) &&
							((entity->ip_u.addr32[2] & cs_lpm_gen_ip_mask_v4(32 - mask1)) == (lpm_table_info.lpm_entries[i].ip_u.addr32[2] & cs_lpm_gen_ip_mask_v4(32 - mask1))))
						return CS_ERROR;

				} else if (entity->mask > 0) {	/* Compare addr32[3] */
					if ((entity->ip_u.addr32[3] & cs_lpm_gen_ip_mask_v4(mask1)) == (lpm_table_info.lpm_entries[i].ip_u.addr32[3] & cs_lpm_gen_ip_mask_v4(mask1)))
						return CS_ERROR;
				}
			}
			i -= 1;
		}

		/* if the entry at inserted point not empty should move the entry up to make a room for the input entry */
		if  (lpm_table_info.lpm_entries[i].is_empty == 0) {
			for (j = end_index; j <= i; j++) {
				//printk("%s: move from %d to %d\n", __func__, j, j+1);
				lpm_table_info.lpm_entries[j] = lpm_table_info.lpm_entries[j + 1];
				lpm_table_info.lpm_entries[j].index = j;
			}
		}

		/* No duplicated rule, let's add one */

		entity->index = i;
		entity->is_empty = 0;

		lpm_table_info.lpm_entries[i] =  *entity;

		lpm_table_info.v6_entry_cnt++;

		cs_lpm_update_sdb_end_ptr(entity->is_v6, MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt * 4, 1);

	} else {             /* IPv4 LPM */
		i = start_index;
		/* Ensure that there is no duplicated entry */
		mask1 = entity->mask;
		ip1 = entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask1);

		//printk("%s: end_index = %d\n", __func__, end_index);
		/* find start position of mask group */
		while ((i <= end_index) && (lpm_table_info.lpm_entries[i].is_empty == 0) &&
				lpm_table_info.lpm_entries[i].mask >= entity->mask) {
			/* Ensure that there is no duplicated entry */
			if (lpm_table_info.lpm_entries[i].mask ==entity->mask) {
				mask2 = lpm_table_info.lpm_entries[i].mask;
				ip2 = lpm_table_info.lpm_entries[i].ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask2);;
				if ((mask1 == mask2) && (ip1 == ip2)) {	/* Same rule */
					printk("DEBUG %s:%d\n", __func__, __LINE__);
					return CS_ERROR;
				}
			}
			i++;
		}

		//printk("%s: i = %d, lpm_table_info.lpm_entries[i].is_empty=%d\n", __func__, i, lpm_table_info.lpm_entries[i].is_empty);
		if (lpm_table_info.lpm_entries[i].is_empty == 0) {
			for (j = end_index - 1; j >= i; j--) {
				printk("%s: move from %d to %d\n", __func__, j, j+1);
				lpm_table_info.lpm_entries[j+1] = lpm_table_info.lpm_entries[j];
				lpm_table_info.lpm_entries[j+1].index = j+1;
			}
		}

		entity->index = i;
		entity->is_empty = 0;

		lpm_table_info.lpm_entries[i] = *entity;
		lpm_table_info.v4_entry_cnt++;

                cs_lpm_update_sdb_end_ptr(entity->is_v6, lpm_table_info.v4_entry_cnt - 1, 1);
	}

	return FE_TABLE_OK;

}

int cs_lpm_range_sort_mask(int ipv6,int start_ptr, int end_ptr)
{
	int start_index, end_index;
	int i,j, b_exchange;
	lpm_entity_info_t temp_entity;

	if (ipv6 == 1) {
		/*ipv6 search direction is high 16 to low 0*/
		start_index = end_ptr;
		end_index = start_ptr;
	} else {
		start_index = ((start_ptr > 2) & 0x7) * 4 + (start_ptr & 0x3);
		end_index = ((end_ptr > 2) & 0x7) * 4 + (end_ptr & 0x3);
	}
	printk("%s() %s sorting with index %d~%d", __func__,
			(ipv6==1)?"ipv6":"ipv4", start_index,end_index);

	for (i = start_index + 1; i <= end_index; (ipv6 == 1)?(i++):(i = i + 4)) {
		for (j = start_index ; j <= (i - 1); (ipv6 == 1)? (j++):(j = j + 4)) {
			b_exchange = 0;
			if (ipv6 == 1) {
				if (lpm_table_info.lpm_entries[i].mask > lpm_table_info.lpm_entries[j].mask)
					b_exchange = 1;
			} else {
				if (lpm_table_info.lpm_entries[i].mask < lpm_table_info.lpm_entries[j].mask)
					b_exchange = 1;
			}
			/*exchange i and j entry*/
			if (b_exchange == 1) {
				memcpy(&temp_entity,
						&lpm_table_info.lpm_entries[j],
						sizeof(lpm_entity_info_t));
				memcpy(&lpm_table_info.lpm_entries[j],
						&lpm_table_info.lpm_entries[i],
						sizeof(lpm_entity_info_t));
				memcpy(&lpm_table_info.lpm_entries[i],
						&temp_entity,
						sizeof(lpm_entity_info_t));
			}
		}
	}
	return FE_TABLE_OK;
}

int cs_lpm_add_entry(lpm_entity_info_t *entity)
{
	u8 i;
#if 0
	u32 ip1, ip2;
	u8 j, mask1, mask2;
#endif

	/* return error if reach max entry count */
	if ((lpm_table_info.v4_entry_cnt + lpm_table_info.v6_entry_cnt * 4 +
				(entity->is_v6? 4 : 1)) > MAX_LPM_ENTRY_CNT4)
	{
		printk("%s: lpm_table_info.v4_entry_cnt=%d,  lpm_table_info.v6_entry_cnt=%d, exceed count\n", __func__,
			lpm_table_info.v4_entry_cnt, lpm_table_info.v6_entry_cnt);
		return FE_TABLE_ETBLFULL;
	}

	if (entity->is_v6) {	/* IPv6 LPM*/
#if 0
		i = MAX_LPM_ENTRY_CNT4 - 1;
		/* find start position of mask group */
		while ((i >= (MAX_LPM_ENTRY_CNT4-lpm_table_info.v6_entry_cnt)) && \
				lpm_table_info.lpm_entries[i].mask < entity->mask) {
			i--;
		}

		j = i;
		/* Ensure that there is no duplicated entry */
		mask1 = entity->mask;
		ip1 = entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask1);
		for(;j>=(MAX_LPM_ENTRY_CNT4-lpm_table_info.v6_entry_cnt);j--){
			if (entity->mask == lpm_table_info.lpm_entries[j].mask){
				if (entity->mask > 96){    /* Just compare addr32[0~3] */
					if ((entity->ip_u.addr32[3]==lpm_table_info.lpm_entries[i].ip_u.addr32[3])&&
							(entity->ip_u.addr32[2]==lpm_table_info.lpm_entries[i].ip_u.addr32[2])&&
							(entity->ip_u.addr32[1]==lpm_table_info.lpm_entries[i].ip_u.addr32[1])&&
							((entity->ip_u.addr32[0]&cs_lpm_gen_ip_mask_v4(128-mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[0]&cs_lpm_gen_ip_mask_v4(128-mask1))))
						return CS_ERROR;

				}
				else if (entity->mask > 64){	/* Compare addr32[1~3] */
					if ((entity->ip_u.addr32[3]==lpm_table_info.lpm_entries[i].ip_u.addr32[3])&&
							(entity->ip_u.addr32[2]==lpm_table_info.lpm_entries[i].ip_u.addr32[2])&&
							((entity->ip_u.addr32[1]&cs_lpm_gen_ip_mask_v4(64-mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[1]&cs_lpm_gen_ip_mask_v4(64-mask1))))
						return CS_ERROR;

				}
				else if (entity->mask > 32){       /* Compare addr32[2~3] */
					if ((entity->ip_u.addr32[3]==lpm_table_info.lpm_entries[i].ip_u.addr32[3])&&
							((entity->ip_u.addr32[2]&cs_lpm_gen_ip_mask_v4(32-mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[2]&cs_lpm_gen_ip_mask_v4(32-mask1))))
						return CS_ERROR;

				}
				else if (entity->mask > 0){         /* Compare addr32[3] */
					if ((entity->ip_u.addr32[3]&cs_lpm_gen_ip_mask_v4(mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[3]&cs_lpm_gen_ip_mask_v4(mask1)))
						return CS_ERROR;
				}

 			}
		}

		/* No duplicated rule, let's add one */
		j = i;
		mask1 = entity->mask;
		mask2 = lpm_table_info.lpm_entries[j].mask;
		while((mask1==mask2) && (j>(MAX_LPM_ENTRY_CNT4-lpm_table_info.v6_entry_cnt))){
			j--;
			mask2 = lpm_table_info.lpm_entries[j].mask;
		}
		printk("DEBUG %s:%d\n",__func__,__LINE__);

		/* insert at last entry of mask group */
		for(i=MAX_LPM_ENTRY_CNT4-lpm_table_info.v6_entry_cnt-1; i>j ; i++)
			memcpy(&lpm_table_info.lpm_entries[i],&lpm_table_info.lpm_entries[i+1],sizeof(lpm_entity_info_t));
#endif
		i = MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt -1;

		entity->index = i;
		entity->is_empty = 0;
		entity->is_v6 = 1;

		memcpy(&lpm_table_info.lpm_entries[i], entity,
				sizeof(lpm_entity_info_t));

		lpm_table_info.v6_entry_cnt++;
	} else {             /* IPv4 LPM */
		i = 0;
#if 0
		/* find start position of mask group */
		while ((i<lpm_table_info.v4_entry_cnt) &&
				lpm_table_info.lpm_entries[i].mask >
				entity->mask) {
			i++;
		}

		j = i;
		/* Ensure that there is no duplicated entry */
		mask1 = entity->mask;
		ip1 = entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask1);
		for(;j<lpm_table_info.v4_entry_cnt;j++){
			mask2 = lpm_table_info.lpm_entries[j].mask;
			ip2 = lpm_table_info.lpm_entries[j].ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask2);;
			if ((mask1==mask2) && (ip1==ip2)){            /* Same rule */
				printk("DEBUG %s:%d\n",__func__,__LINE__);
				return CS_ERROR;
			}
		}
		printk("DEBUG %s:%d\n",__func__,__LINE__);

		/* No duplicated rule, let's add one */
		j = i;
		mask1 = entity->mask;
		mask2 = lpm_table_info.lpm_entries[j].mask;
		while((mask1==mask2) && (j<lpm_table_info.v4_entry_cnt)){
			j++;
			mask2 = lpm_table_info.lpm_entries[j].mask;
		}
		printk("DEBUG %s:%d\n",__func__,__LINE__);

		/* insert at last entry of mask group */
		for(i=lpm_table_info.v4_entry_cnt; i>j ; i--)
			memcpy(&lpm_table_info.lpm_entries[i],&lpm_table_info.lpm_entries[i-1],sizeof(lpm_entity_info_t));
#endif
		i = lpm_table_info.v4_entry_cnt;

		entity->index = i;
		entity->is_empty = 0;
		entity->is_v6 = 0;

		/* printk("%s: entity is copied to index=%d\n", __func__, i); */
		memcpy(&lpm_table_info.lpm_entries[i], entity,
				sizeof(lpm_entity_info_t));
		lpm_table_info.v4_entry_cnt++;
   }

	return FE_TABLE_OK;
}

signed char cs_lpm_search_entry(lpm_entity_info_t *entity)
{
	signed char i,j;
	u32 ip1,ip2;
	u8 mask1,mask2;

	if (entity->is_v6) {         /* IPv6 LPM*/
		i = MAX_LPM_ENTRY_CNT4 - 1;
		/* find start position of mask group */
		while ((i >= (MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt))
				&& (lpm_table_info.lpm_entries[i].mask <
					entity->mask))
			i--;

		j = i;
		/* compare entry */
		mask1 = entity->mask;
		ip1 = entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask1);
		for (; j >= (MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt);
				j--) {
			if (entity->mask == lpm_table_info.lpm_entries[j].mask) {
				if (entity->mask > 96) {	/* Just compare addr32[0~3] */
					if ((entity->ip_u.addr32[3] == lpm_table_info.lpm_entries[i].ip_u.addr32[3]) &&
							(entity->ip_u.addr32[2] == lpm_table_info.lpm_entries[i].ip_u.addr32[2]) &&
							(entity->ip_u.addr32[1] == lpm_table_info.lpm_entries[i].ip_u.addr32[1]) &&
							((entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(128 - mask1)) == (lpm_table_info.lpm_entries[i].ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(128 - mask1))))
						*entity = lpm_table_info.lpm_entries[j];
						entity->index = lpm_table_info.lpm_entries[j].index;
						return j;
				} else if (entity->mask > 64){	/* Compare addr32[1~3] */
					if ((entity->ip_u.addr32[3] == lpm_table_info.lpm_entries[i].ip_u.addr32[3])&&
							(entity->ip_u.addr32[2]==lpm_table_info.lpm_entries[i].ip_u.addr32[2])&&
							((entity->ip_u.addr32[1]&cs_lpm_gen_ip_mask_v4(64-mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[1]&cs_lpm_gen_ip_mask_v4(64-mask1))))
						*entity = lpm_table_info.lpm_entries[j];
						entity->index = lpm_table_info.lpm_entries[j].index;
						return j;

				}
				else if (entity->mask > 32){       /* Compare addr32[2~3] */
					if ((entity->ip_u.addr32[3]==lpm_table_info.lpm_entries[i].ip_u.addr32[3])&&
							((entity->ip_u.addr32[2]&cs_lpm_gen_ip_mask_v4(32-mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[2]&cs_lpm_gen_ip_mask_v4(32-mask1))))
						*entity = lpm_table_info.lpm_entries[j];
						entity->index = lpm_table_info.lpm_entries[j].index;
						return j;
				}
				else if (entity->mask > 0){         /* Compare addr32[3] */
					if ((entity->ip_u.addr32[3]&cs_lpm_gen_ip_mask_v4(mask1))==(lpm_table_info.lpm_entries[i].ip_u.addr32[3]&cs_lpm_gen_ip_mask_v4(mask1)))
						*entity = lpm_table_info.lpm_entries[j];
						entity->index = lpm_table_info.lpm_entries[j].index;
						return j;
				}
			}
		}
	} else {             /* IPv4 LPM */
		i=0;
		/* find start position of mask group */
		while((i<lpm_table_info.v4_entry_cnt) && \
				lpm_table_info.lpm_entries[i].mask > entity->mask ){
			i++;
		}
		j = i;
		/* compare entry */
		mask1 = entity->mask;
		ip1 = entity->ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask1);
		for(;j<lpm_table_info.v4_entry_cnt;j++){
			mask2 = lpm_table_info.lpm_entries[j].mask;
			ip2 = lpm_table_info.lpm_entries[j].ip_u.addr32[0] & cs_lpm_gen_ip_mask_v4(mask2);;


			/* printk("%s: ip1=0x%x, mask1=0x%x, ip2=0x%x, mask2=0x%x\n", __func__,
				ip1, mask1, ip2, mask2); */

			if ((mask1==mask2) && (ip1==ip2)){            /* Same rule */
				*entity = lpm_table_info.lpm_entries[j];

				/* printk("%s: LPM entry found at index=%d\n", __func__, j); */
				return j;
			}
		}
	}
  	//FIXME: to modify code to FE_TABLE_ENTRYNOTFOUND
	return FE_TABLE_EOUTRANGE;
}

int cs_lpm_delete_entry(lpm_entity_info_t *entity)
{
	signed char index;

	index = cs_lpm_search_entry(entity);

	if (index == CS_ERROR)	/* Not found */
		return CS_ERROR;
	else
		cs_lpm_delete_index((u8)index);

	return FE_TABLE_OK;
}

int cs_lpm_delete_index(u8 index)
{
	int i, j;

	if (lpm_table_info.lpm_entries[index].is_empty == 1)
		return FE_TABLE_ETBLNOTEXIST;

	if (lpm_table_info.lpm_entries[index].is_v6 == 1)
	{
		if (lpm_table_info.v6_entry_cnt < 1)
                {
                        return CS_ERROR;
                }

		j = index;
		for (i = 0; i < lpm_table_info.v6_entry_cnt - (MAX_LPM_ENTRY_CNT4 - index); i++)
		{
                        lpm_table_info.lpm_entries[j] = lpm_table_info.lpm_entries[j-1];
			lpm_table_info.lpm_entries[j].index = j;
			printk("%s: IPv6, move lpm_table_info.lpm_entries[] from %d to %d\n", __func__, j-1, j);
			j--;
		}
		memset(&lpm_table_info.lpm_entries[j], 0, sizeof(lpm_entity_info_t));
		lpm_table_info.lpm_entries[j].is_empty = 1;
		lpm_table_info.v6_entry_cnt--;

		if (!lpm_table_info.v6_entry_cnt)
			cs_lpm_update_sdb_end_ptr(1, MAX_LPM_ENTRY_CNT4 - 1, 0);
		else
			cs_lpm_update_sdb_end_ptr(1, MAX_LPM_ENTRY_CNT4 - lpm_table_info.v6_entry_cnt * 4, 1);
	}
	else
	{
		if (lpm_table_info.v4_entry_cnt < 1)
		{
			return CS_ERROR;
		}
		for (i = index; i < lpm_table_info.v4_entry_cnt - 1; i++)
		{
			lpm_table_info.lpm_entries[i] = lpm_table_info.lpm_entries[i+1];
			lpm_table_info.lpm_entries[i].index = i;
			printk("%s: IPv4, move lpm_table_info.lpm_entries[] from %d to %d\n", __func__, i+1, i);
		}

		memset(&lpm_table_info.lpm_entries[lpm_table_info.v4_entry_cnt - 1], 0, sizeof(lpm_entity_info_t));
        	lpm_table_info.lpm_entries[lpm_table_info.v4_entry_cnt - 1].is_empty = 1;
		lpm_table_info.v4_entry_cnt--;

		if (!lpm_table_info.v4_entry_cnt)
			cs_lpm_update_sdb_end_ptr(0, 0, 0);
		else
			cs_lpm_update_sdb_end_ptr(0, lpm_table_info.v4_entry_cnt - 1, 1);
	}

	return FE_TABLE_OK;
}

int cs_lpm_get_entity_by_idx(u8 index, lpm_entity_info_t *entity)
{
	u8 table;
	short i=0;
	FETOP_LPM_HC_TABLE0_UPPER_REGF_ACCESS_t hc_access;
	FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA0_t hc_data0;
	FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA1_t hc_data1;
	FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA2_t hc_data2;
	FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA3_t hc_data3;


	if (lpm_table_info.lpm_entries[index].is_empty == 1)
		return FE_TABLE_ETBLNOTEXIST;

	table = lpm_table_info.active_tlb;

	memcpy(entity, &lpm_table_info.lpm_entries[index], sizeof(lpm_entity_info_t));

  	/* printk("%s: index=%d, entity->index=%d,  entity->is_v6=%d, table=%d\n", __func__, index, entity->index,  entity->is_v6, table); */

	if (entity->is_v6){ /* check if it's V6 */
  		/* printk("%s: IPv6 index=%d, entity->index=%d\n", __func__, index, entity->index); */
		if (entity->index>(MAX_LPM_ENTRY_CNT4-MAX_LPM_ENTRY_CNT6/2-1)){ 	/* use upper table */

			hc_access.bf.ACCESS=1;
			hc_access.bf.w_rdn=0;
			hc_access.bf.parity_bypass=0;
			hc_access.bf.address = entity->index-(MAX_LPM_ENTRY_CNT4-MAX_LPM_ENTRY_CNT6/2) ;
			writel(hc_access.wrd,(table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_UPPER_REGF_ACCESS);
			/* Wait read complete*/
			while((hc_access.bf.ACCESS==1)&&(i<LPM_INIT_TIMEOUT))
				hc_access.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_UPPER_REGF_ACCESS);
			if (i>=LPM_INIT_TIMEOUT)
				return FE_TABLE_EOPNOTSUPP;  /* ACCESS can't complete */
			hc_data0.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA0);
			hc_data1.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA1);
			entity->hit_count = hc_data1.wrd;
			entity->hit_count = entity->hit_count<<32;
			entity->hit_count |= hc_data0.wrd;
			lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
		}
		else{                      /* use lower table */
			hc_access.bf.ACCESS=1;
			hc_access.bf.w_rdn=0;
			hc_access.bf.parity_bypass=0;
			hc_access.bf.address = entity->index-(MAX_LPM_ENTRY_CNT4-MAX_LPM_ENTRY_CNT6) ;
			writel(hc_access.wrd,(table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_LOWER_REGF_ACCESS);
 			/* Wait read complete*/
			while((hc_access.bf.ACCESS==1)&&(i<LPM_INIT_TIMEOUT))
				hc_access.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_LOWER_REGF_ACCESS);
			if (i>=LPM_INIT_TIMEOUT)
				return FE_TABLE_EOPNOTSUPP;  /* ACCESS can't complete */
			hc_data0.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA0);
			hc_data1.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA1);
			entity->hit_count = hc_data1.wrd;
			entity->hit_count = entity->hit_count<<32;
			entity->hit_count |= hc_data0.wrd;
			lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
		}

	}
	else {	/* IPv4 LPM */
  printk("%s: IPv4 index=%d, entity->index=%d\n", __func__, index, entity->index);
		if (entity->index>(MAX_LPM_ENTRY_CNT4/2-1)){ 	/* use upper table */
			hc_access.bf.ACCESS=1;
			hc_access.bf.w_rdn=0;
			hc_access.bf.parity_bypass=0;
			//hc_access.bf.address = (entity->index-MAX_LPM_ENTRY_CNT4/2)/4 ;


			hc_access.bf.address = 0 ;
			writel(hc_access.wrd,(table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_UPPER_REGF_ACCESS);
			/* Wait read complete*/
			while((hc_access.bf.ACCESS==1)&&(i<LPM_INIT_TIMEOUT))
				hc_access.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_UPPER_REGF_ACCESS);
			if (i>=LPM_INIT_TIMEOUT)
				return FE_TABLE_EOPNOTSUPP;  /* ACCESS can't complete */

			switch (entity->index%4){
				case 0:
					hc_data0.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA0);
					entity->hit_count = hc_data0.bf.up_hit_cnt00;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				case 1:
					hc_data0.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA0);
					hc_data1.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA1:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA1);
					entity->hit_count = (hc_data1.bf.up_hit_cnt01<<5)|hc_data0.bf.up_hit_cnt01;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				case 2:
					hc_data1.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA1:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA1);
					hc_data2.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA2:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA2);
					entity->hit_count = (hc_data2.bf.up_hit_cnt02<<10)|hc_data1.bf.up_hit_cnt02;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				case 3:
					hc_data2.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA2:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA2);
					hc_data3.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_UPPER_REGF_DATA3:FETOP_LPM_HC_TABLE0_UPPER_REGF_DATA3);
					entity->hit_count = (hc_data3.bf.up_hit_cnt03<<15)|hc_data2.bf.up_hit_cnt03;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				default:
	   				/* Impossible to here */
					break;
 			}
		}
		else{                      /* use lower table */
			hc_access.bf.ACCESS=1;
			hc_access.bf.w_rdn=0;
			hc_access.bf.parity_bypass=0;
			hc_access.bf.address = entity->index/4 ;


			printk("%s: user lower table, entity->index=%d\n", __func__, entity->index);
			writel(hc_access.wrd,(table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_LOWER_REGF_ACCESS);
			/* Wait read complete*/
			while((hc_access.bf.ACCESS==1)&&(i<LPM_INIT_TIMEOUT))
				hc_access.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_HC_TABLE0_LOWER_REGF_ACCESS);
			if (i>=LPM_INIT_TIMEOUT)
			{
				printk("%s: hit_count read failed\n", __func__);
				return FE_TABLE_EOPNOTSUPP;  /* ACCESS can't complete */
			}

			switch (entity->index%4){
				case 0:
					hc_data0.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA0);

					entity->hit_count = hc_data0.bf.up_hit_cnt00;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;

					/* printk("%s: hc_data0.wrd=0x%x, entity->hit_count=0x%x\n", __func__, (unsigned int)hc_data0.wrd, (unsigned int)entity->hit_count); */
					break;
				case 1:
					hc_data0.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA0);
					hc_data1.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA1:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA1);
					entity->hit_count = (hc_data1.bf.up_hit_cnt01<<5)|hc_data0.bf.up_hit_cnt01;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				case 2:
					hc_data1.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA1:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA1);
					hc_data2.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA2:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA2);
					entity->hit_count = (hc_data2.bf.up_hit_cnt02<<10)|hc_data1.bf.up_hit_cnt02;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				case 3:
					hc_data2.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA2:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA2);
					hc_data3.wrd = readl((table==2)? FETOP_LPM_HC_TABLE1_LOWER_REGF_DATA3:FETOP_LPM_HC_TABLE0_LOWER_REGF_DATA3);
					entity->hit_count = (hc_data3.bf.up_hit_cnt03<<15)|hc_data2.bf.up_hit_cnt03;
					lpm_table_info.lpm_entries[index].hit_count = entity->hit_count;
					break;
				default:
	   				/* Impossible to here */
					break;
			}
		}
	}

	return FE_TABLE_OK;
}

int cs_lpm_get_entity_by_rule(lpm_entity_info_t *entity)
{
	signed char index;

	index = cs_lpm_search_entry(entity);
	printk("DEBUG %s:%d\n",__func__,__LINE__);

	if (index==CS_ERROR)        /* Not found */
		return CS_ERROR;
	printk("DEBUG %s:%d\n",__func__,__LINE__);

	if (lpm_table_info.lpm_entries[index].is_empty==0)
		cs_lpm_get_entity_by_idx((u8)index, entity);

	return FE_TABLE_OK;
}

void cs_lpm_swap_active_table(void)
{
	int i;

	if (lpm_table_info.active_tlb == 1)
        	i = 2;
        else
                i = 1;
        cs_lpm_flush_all(i);    /* active table */

        /* printk("%s: lpm_table_info.active_tlb=%d, i = %d\n", __func__, lpm_table_info.active_tlb, i); */
        lpm_table_info.active_tlb = i;
        cs_lpm_active_table(i - 1);
}

int cs_lpm_flush_all(u8 table)
{
	u8 i;

	/* FIXME? if flush_entity fails returns? */
	for(i=0;i<MAX_LPM_ENTRY_CNT4;i++){
		if (lpm_table_info.lpm_entries[i].is_empty==0)  /* Not empty */
			cs_lpm_flush_entity(&lpm_table_info.lpm_entries[i],table);
	}
	return FE_TABLE_OK;
}

int cs_lpm_flush_entity(lpm_entity_info_t *entity,u8 table)
{
	signed char is_upper = 0, addr = 0;
	short i = 0;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_ACCESS_t access;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA8_t data8;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA7_t data7;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA6_t data6;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA5_t data5;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA4_t data4;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA3_t data3;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA2_t data2;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA1_t data1;
	FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA0_t data0;

	if (entity->is_v6) {
		/* IPv6 will overwrite all colume, no need to read first */
		data0.wrd = 0;
		data1.wrd = 0;
		data2.wrd = 0;
		data3.wrd = 0;
		data4.wrd = 0;
		data5.wrd = 0;
		data6.wrd = 0;
		data7.wrd = 0;
		data8.wrd = 0;

		data0.bf.up_nh00 = ((entity->hash_index & 0x01FFF) << 4) |
			(entity->priority & 0x0F);
		data3.bf.up_mask00 = entity->mask;
		data4.bf.up_mask01 = (entity->mask >> 6) & 0x3;
		data3.bf.up_ip00 = entity->ip_u.addr32[0] & 0x3FFF;
		data4.bf.up_ip00 = (entity->ip_u.addr32[0] >> 14) & 0x3FFFF;
		data4.bf.up_ip01 = entity->ip_u.addr32[1] & 0xFF;
		data5.bf.up_ip01 = (entity->ip_u.addr32[1] >> 8) & 0xFFFFFF;
		data5.bf.up_ip02 =  entity->ip_u.addr32[2] & 0x03;
		data6.bf.up_ip02 = (entity->ip_u.addr32[2] >> 2) & 0x3FFFFFFF;
		data7.bf.up_ip03 =  entity->ip_u.addr32[3] & 0x0FFFFFFF;
		data8.bf.up_ip03 = (entity->ip_u.addr32[3] >> 28) & 0x0F;
		if (entity->index > ((MAX_LPM_ENTRY_CNT4-MAX_LPM_ENTRY_CNT6 / 2) - 1)) {	/* Upper table */
			addr = entity->index - (MAX_LPM_ENTRY_CNT4 - (MAX_LPM_ENTRY_CNT6 / 2));
			is_upper = 1;

	   		writel(data0.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA0 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA0);
			writel(data1.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA1 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA1);
			writel(data3.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA3 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA3);
			writel(data4.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA4 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA4);
			writel(data5.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA5 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA5);
			writel(data6.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA6 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA6);
			writel(data7.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA7 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA7);
			writel(data8.wrd, (table == 2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA8 : FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA8);
		} else {	/* Lower table */
			is_upper = 0;
			addr = entity->index - (MAX_LPM_ENTRY_CNT4 - MAX_LPM_ENTRY_CNT6);
#if 0
			/* IPv6 will overwrite all colume, no need to read first */
			data0.wrd = 0;
			data1.wrd = 0;
			data2.wrd = 0;
			data3.wrd = 0;
			data4.wrd = 0;
			data5.wrd = 0;
			data6.wrd = 0;
			data7.wrd = 0;
			data8.wrd = 0;

	   		data0.bf.up_nh00 = ((entity->hash_index&0x01FFF)<<4)|(entity->priority & 0x0F);
			data3.bf.up_mask00 = entity->mask ;
			data4.bf.up_mask01 = entity->mask >> 6;
			data3.bf.up_ip00 = entity->ip_u.addr32[0]&0x00003FFF;
			data4.bf.up_ip00 = entity->ip_u.addr32[0]&0x0003FFFF;
			data4.bf.up_ip01 = entity->ip_u.addr32[1]&0x000000FF;
			data5.bf.up_ip01 = entity->ip_u.addr32[1]&0x00FFFFFF;
			data5.bf.up_ip02 = entity->ip_u.addr32[2]&0x00000003;
			data6.bf.up_ip02 = entity->ip_u.addr32[2]&0x3FFFFFFF;
			data7.bf.up_ip03 = entity->ip_u.addr32[3]&0x0FFFFFFF;
			data8.bf.up_ip03 = entity->ip_u.addr32[3]&0x0000000F;
#endif
			writel(data0.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA0);
			writel(data1.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA1);
			writel(data3.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA3);
			writel(data4.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA4);
			writel(data5.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA5);
			writel(data6.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA6);
			writel(data7.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA7:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA7);
			writel(data8.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA8:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA8);
		}
	} else {
		if (entity->index > (MAX_LPM_ENTRY_CNT4 / 2 - 1)) {	/* Upper table */
			is_upper = 1;
			addr = (entity->index-MAX_LPM_ENTRY_CNT4/2)/4 ;
			/* Read first then update necessary field */
			access.bf.address = addr;
			access.bf.parity_bypass=0;
			access.bf.w_rdn=0;
			access.bf.ACCESS=1;
			writel(access.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_UPPER_REGF_ACCESS );
			/* Check if access complete */
			while((i<LPM_INIT_TIMEOUT) && (access.bf.ACCESS==1))
				access.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_UPPER_REGF_ACCESS);
			if (i>=LPM_INIT_TIMEOUT)
				return FE_TABLE_EOPNOTSUPP;

			data0.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA0);
			data1.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA1);
			data2.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA2:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA2);
			data3.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA3);
			data4.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA4);
			data5.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA5);
			data6.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA6);
			data7.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA7:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA7);
			data8.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA8:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA8);

			switch (entity->index % 4) {
			case 0:
		   		data0.bf.up_nh00 = ((entity->hash_index&0x01FFF)<<4)|(entity->priority & 0x0F);
	   			data3.bf.up_mask00 = entity->mask ;
   				data3.bf.up_ip00 = entity->ip_u.addr32[0]&0x3FFF;
				data4.bf.up_ip00 = (entity->ip_u.addr32[0]>>14)&0x3FFFF;

				writel(data0.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA0);
				writel(data3.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA3);
				writel(data4.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA4);

				break;
			case 1:
   				data0.bf.up_nh01 = ((entity->hash_index&0x01)<<4)|(entity->priority & 0x0F);
				data1.bf.up_nh01 = (entity->hash_index>>1)&0xFFF;
				data4.bf.up_mask01 = entity->mask;
				data4.bf.up_ip01 =  entity->ip_u.addr32[0]&0xFF;
				data5.bf.up_ip01 = (entity->ip_u.addr32[0]>>8)&0xFFFFFF;

				writel(data0.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA0);
				writel(data1.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA1);
				writel(data4.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA4);
				writel(data5.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA5);
				break;
			case 2:
   				data1.bf.up_nh02 = ((entity->hash_index&0x3F)<<4)|(entity->priority & 0x0F);
				data2.bf.up_nh02 = (entity->hash_index>>6)&0x7F;
				data5.bf.up_mask02 = entity->mask;
				data5.bf.up_ip02 =  entity->ip_u.addr32[0]&0x03;
				data6.bf.up_ip02 = (entity->ip_u.addr32[0]>>2)&0x3FFFFFFF;

				writel(data1.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA1);
				writel(data2.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA2:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA2);
				writel(data5.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA5);
				writel(data6.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA6);
				break;
			case 3:
   				data2.bf.up_nh03 = ((entity->hash_index&0x7FF)<<4)|(entity->priority & 0x0F);
				data3.bf.up_nh03 = (entity->hash_index>>11)&0x03;
				data6.bf.up_mask03 = entity->mask&0x03;
				data7.bf.up_mask03 = (entity->mask>>2)&0xF;
				data7.bf.up_ip03 =  entity->ip_u.addr32[0]&0x0FFFFFFF;
				data8.bf.up_ip03 = (entity->ip_u.addr32[0]>>28)&0x0F;

				writel(data2.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA2:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA2);
				writel(data3.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA3);
				writel(data6.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA6);
				writel(data7.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA7:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA7);
				writel(data8.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_DATA8:FETOP_LPM_LPM_TABLE0_UPPER_REGF_DATA8);
				break;
			default:
				/* impossible to here */
				break;
			}
		} else {	/* Lower table */
			/* Read first then update necessary field */
			is_upper = 0;
			addr = entity->index/4;
			access.bf.address = addr;
			access.bf.parity_bypass = 0;
			access.bf.w_rdn = 0;
			access.bf.ACCESS = 1;
			writel(access.wrd, (table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_LOWER_REGF_ACCESS );
			/* Check if access complete */
			while ((i < LPM_INIT_TIMEOUT) && (access.bf.ACCESS == 1))
				access.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_LOWER_REGF_ACCESS);
			if (i >= LPM_INIT_TIMEOUT)
				return FE_TABLE_EOPNOTSUPP;

			data0.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA0);
			data1.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA1);
			data2.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA2:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA2);
			data3.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA3);
			data4.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA4);
			data5.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA5);
			data6.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA6);
			data7.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA7:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA7);
			data8.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA8:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA8);

			switch (entity->index % 4) {
			case 0:
				data0.bf.up_nh00 = ((entity->hash_index&0x01FFF)<<4)|(entity->priority & 0x0F);
				data3.bf.up_mask00 = entity->mask ;
				data3.bf.up_ip00 = entity->ip_u.addr32[0]&0x3FFF;
				data4.bf.up_ip00 = (entity->ip_u.addr32[0]>>14)&0x3FFFF;
				/*
				printk("data0=%x, data3=%x, hash_index=%x, "
						"ip_u.addr32=%x\n", data0.wrd,
						data3.wrd,
						entity->hash_index,
						(unsigned int)entity->ip_u.addr32);
				*/
				writel(data0.wrd, (table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA0);
				writel(data3.wrd, (table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA3);
				writel(data4.wrd, (table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA4);

				break;
			case 1:
				data0.bf.up_nh01 = ((entity->hash_index&0x01)<<4)|(entity->priority & 0x0F);
				data1.bf.up_nh01 = (entity->hash_index>>1)&0xFFF;
				data4.bf.up_mask01 = entity->mask;
				data4.bf.up_ip01 =  entity->ip_u.addr32[0]&0xFF;
				data5.bf.up_ip01 = (entity->ip_u.addr32[0]>>8)&0xFFFFFF;

				writel(data0.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA0:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA0);
				writel(data1.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA1);
				writel(data4.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA4:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA4);
				writel(data5.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA5);
				break;
			case 2:
				data1.bf.up_nh02 = ((entity->hash_index&0x3F)<<4)|(entity->priority & 0x0F);
				data2.bf.up_nh02 = (entity->hash_index>>6)&0x7F;
				data5.bf.up_mask02 = entity->mask;
				data5.bf.up_ip02 =  entity->ip_u.addr32[0]&0x03;
				data6.bf.up_ip02 = (entity->ip_u.addr32[0]>>2)&0x3FFFFFFF;

				writel(data1.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA1:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA1);
				writel(data2.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA2:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA2);
				writel(data5.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA5:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA5);
				writel(data6.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA6);
				break;
			case 3:
				data2.bf.up_nh03 = ((entity->hash_index&0x7FF)<<4)|(entity->priority & 0x0F);
				data3.bf.up_nh03 = (entity->hash_index>>11)&0x03;
				data6.bf.up_mask03 = entity->mask&0x03;
				data7.bf.up_mask03 = (entity->mask>>2)&0xF;
				data7.bf.up_ip03 =  entity->ip_u.addr32[0]&0x0FFFFFFF;
				data8.bf.up_ip03 = (entity->ip_u.addr32[0]>>28)&0x0F;

				writel(data2.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA2:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA2);
				writel(data3.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA3:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA3);
				writel(data6.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA6:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA6);
				writel(data7.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA7:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA7);
				writel(data8.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_DATA8:FETOP_LPM_LPM_TABLE0_LOWER_REGF_DATA8);
				break;
			default:
				/* impossible to here */
				break;
			}
		}
	}

	access.bf.address = addr;
	access.bf.parity_bypass=0;
	access.bf.w_rdn=1;
	access.bf.ACCESS=1;
	i = 0;
	if (is_upper==1){
		writel(access.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_UPPER_REGF_ACCESS );
		/* Check if access complete */
		while((i<LPM_INIT_TIMEOUT) && (access.bf.ACCESS==1))
   			access.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_UPPER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_UPPER_REGF_ACCESS);
		if (i>=LPM_INIT_TIMEOUT)
			return FE_TABLE_EOPNOTSUPP;           /* access not complete */
	}
	else{
		/* printk("Bird ctl=%x (@%x) \n",access.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_LOWER_REGF_ACCESS); */
		writel(access.wrd,(table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_LOWER_REGF_ACCESS );
		/* Check if access complete */
		while((i<LPM_INIT_TIMEOUT) && (access.bf.ACCESS==1))
			access.wrd = readl((table==2)? FETOP_LPM_LPM_TABLE1_LOWER_REGF_ACCESS:FETOP_LPM_LPM_TABLE0_LOWER_REGF_ACCESS);
		if (i>=LPM_INIT_TIMEOUT)
			return FE_TABLE_EOPNOTSUPP;           /* access not complete */
	}

	return FE_TABLE_OK;
}

int cs_lpm_active_table(u8 table)
{
	FETOP_LPM_LPM_CONFIG_t lpm_config;
	FETOP_LPM_LPM_STATUS_t lpm_status;
	int try_loop=0;

	/* Set Active table */
	lpm_config.wrd = readl(FETOP_LPM_LPM_CONFIG);
	lpm_config.bf.active_table = table + 1;
	writel(lpm_config.wrd, FETOP_LPM_LPM_CONFIG);

	/* Poll Status register to check if change has applied to HW  */
	do{
		lpm_status.wrd = readl(FETOP_LPM_LPM_STATUS);
	}while((lpm_status.bf.lpm_status != (table+1)) && (try_loop<LPM_INIT_TIMEOUT) );

	if (try_loop>=LPM_INIT_TIMEOUT)
		return FE_TABLE_EOPNOTSUPP;

	return FE_TABLE_OK;
}

int cs_lpm_disable(void)
{
	FETOP_LPM_LPM_CONFIG_t lpm_config;
	FETOP_LPM_LPM_STATUS_t lpm_status;
	int try_loop=0;

	/* Set Active table as 0 ==> LPM engine will pause search engine */
	lpm_config.wrd = readl(FETOP_LPM_LPM_CONFIG);
	lpm_config.bf.active_table = 0;
	writel(lpm_config.wrd, FETOP_LPM_LPM_CONFIG);

	/* Poll Status register to check if change has applied to HW  */
	do{
		lpm_status.wrd = readl(FETOP_LPM_LPM_STATUS);
		udelay(1000);
	}while((lpm_status.bf.lpm_status != 0) && (try_loop<LPM_INIT_TIMEOUT) );

	if (try_loop>=LPM_INIT_TIMEOUT)
		return FE_TABLE_EOPNOTSUPP;

	return FE_TABLE_OK;
}

int cs_lpm_print_entry(unsigned int idx)
{
        struct lpm_entity_info lpm_entry, *p_lpm;
        __u32 value;
        int status;
        unsigned int count;

	status = cs_lpm_get_entity_by_idx(idx, &lpm_entry);
        if (status == FE_TABLE_ETBLNOTEXIST)
	{
                printk("| index %04d | NOT USED\n", idx);
                printk("|----------------------------------------------|\n");
	}
        if (status != FE_TABLE_OK)
                return -1;

        p_lpm = &lpm_entry;

        printk("| index %04d\n", idx);

	printk("IS empty=%d\n", p_lpm->is_empty);
	printk("IS IPv6=%d\n", p_lpm->is_v6);
	printk("addr32[0-3]=0x%x-0x%x-0x%x-0x%x\n", p_lpm->ip_u.addr32[0],  p_lpm->ip_u.addr32[1],  p_lpm->ip_u.addr32[2],  p_lpm->ip_u.addr32[3]);
	printk("index=%d\n", p_lpm->index);
	printk("mask=%d\n", p_lpm->mask);
	printk("priority=0x%x\n", p_lpm->priority);
	printk("hash_index=%d\n", p_lpm->hash_index);
	printk("hit_count=%d\n", (unsigned int)p_lpm->hit_count);
	printk("|----------------------------------------------|\n");
	return 0;
                                                      
}
int cs_fe_ioctl_lpm(struct net_device *dev, void *pdata , void * cmd)
{
	fe_lpm_entry_t *lpm_entry = (fe_lpm_entry_t *)pdata;
	struct lpm_entity_info entity;
	NEFE_CMD_HDR_T *fe_cmd_hdr = (NEFE_CMD_HDR_T*)cmd;
	int i;
	int status;

	memset(&entity, 0, sizeof(struct lpm_entity_info));

	entity.is_v6 = lpm_entry->ipv6;
	entity.ip_u.addr32[0] = lpm_entry->ip_addr[0];
	entity.ip_u.addr32[1] = lpm_entry->ip_addr[1];
	entity.ip_u.addr32[2] = lpm_entry->ip_addr[2];
	entity.ip_u.addr32[3] = lpm_entry->ip_addr[3];
	entity.mask = lpm_entry->mask;
	entity.priority = lpm_entry->priority;
	entity.hash_index = lpm_entry->result_idx;

	printk("%s: fe_cmd_hdr->cmd=0x%x\n", __func__, fe_cmd_hdr->cmd);
	printk("%s: entity.is_v6=%d, entity.ip_u.addr32[0-3]=%d.%d.%d.%d, entity.mask=0x%x, entity.priority=0x%x,  entity.hash_index=%d\n", __func__,
		entity.is_v6, entity.ip_u.addr32[0], entity.ip_u.addr32[1], entity.ip_u.addr32[2], entity.ip_u.addr32[3], entity.mask, entity.priority, entity.hash_index);


	printk("%s: fe_cmd_hdr->idx_start=%d, fe_cmd_hdr->idx_end=%d\n", __func__, fe_cmd_hdr->idx_start, fe_cmd_hdr->idx_end);
	switch (fe_cmd_hdr->cmd) {
	case CMD_INIT:
		printk("DEBUG %s:%d\n", __func__, __LINE__);
		cs_lpm_init(0);
		break;
	case CMD_ADD:
		printk("DEBUG %s:%d\n", __func__, __LINE__);
		cs_lpm_add_entry(&entity);
		break;
	case CMD_DELETE:
		printk("DEBUG %s:%d\n", __func__, __LINE__);
		cs_lpm_delete_entry(&entity);
		break;
	case CMD_FLUSH:
		printk("DEBUG %s:%d\n", __func__, __LINE__);
		if (lpm_table_info.active_tlb == 1)
			i = 2;
		else
			i = 1;
		cs_lpm_flush_all(i);	/* active table */

		printk("%s: lpm_table_info.active_tlb=%d, i = %d\n", __func__, lpm_table_info.active_tlb, i); 
		lpm_table_info.active_tlb = i;
		cs_lpm_active_table(i - 1);
		break;
	case CMD_GET:
		printk("DEBUG %s:%d\n", __func__, __LINE__);
		//cs_lpm_get_entity_by_rule(&entity);


		  printk("\n\n ----------------- LPM Table ---------------\n");
        printk("|------------------------------------------------------\n");


		for (i = fe_cmd_hdr->idx_start; i <= fe_cmd_hdr->idx_end; i++)
		{
			
                	status = cs_lpm_print_entry(i);
			if (status == -1)
			    break;
                	cond_resched();
		}
        printk("|------------------------------------------------------\n");
		
		break;
	case CMD_REPLACE:
		break;
	default:
		return CS_ERROR;
	}

	return 0;
}

