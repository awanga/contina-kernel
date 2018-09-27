#ifndef __CS_FE_TABLE_GENERIC_H__
#define __CS_FE_TABLE_GENERIC_H__

#include "cs_fe_table_int.h"

int fe_table_inc_entry_refcnt(cs_fe_table_t *table_type_ptr, unsigned int idx);
int fe_table_dec_entry_refcnt(cs_fe_table_t *table_type_ptr, unsigned int idx);
int fe_table_get_entry_refcnt(cs_fe_table_t *table_type_ptr, unsigned int idx,
		unsigned int *p_cnt);
void *fe_table_malloc_table_entry(cs_fe_table_t *table_type_ptr);
int fe_table_alloc_entry(cs_fe_table_t *table_type_ptr, unsigned int *rslt_idx,
		unsigned int start_offset);
int fe_table_set_entry(cs_fe_table_t *table_type_ptr, unsigned int idx,
		void *entry);
int fe_table_add_entry(cs_fe_table_t *table_type_ptr, void *entry,
		unsigned int *rslt_idx);
int fe_table_find_entry(cs_fe_table_t *table_type_ptr, void *entry,
		unsigned int *rslt_idx, unsigned int start_offset);
int fe_table_del_entry_by_idx(cs_fe_table_t *table_type_ptr, unsigned int idx,
		bool f_force);
int fe_table_del_entry(cs_fe_table_t *table_type_ptr, void *entry,
		bool f_force);
int fe_table_get_entry(cs_fe_table_t *table_type_ptr, unsigned int idx,
		void *entry);
int fe_table_flush_table(cs_fe_table_t *table_type_ptr);
int fe_table_get_avail_count(cs_fe_table_t *table_type_ptr);

#endif /* __CS_FE_TABLE_GENERIC_H__ */
