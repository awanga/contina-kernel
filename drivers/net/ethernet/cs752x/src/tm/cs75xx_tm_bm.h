/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

/* This header file is for GoldenGate Traffic Manager,
 * Buffer Manager Sub-module */

#ifndef __CS_TM_BM_H__
#define __CS_TM_BM_H__

#include <linux/pkt_sched.h>

#define CS_TM_BM_RETRY_TIMES	(256)
#define CS_TM_BM_ACCESS_BIT	(1<<31)

#define CS_MAX_VOQ_NO		112
#define CS_MAX_VOQ_PROFILE	32
#define CS_MAX_WRED_PROFILE	32

#define CS_TM_BM_DEF_VOQ_MIN_DEPTH	48	/*   48 cells = 1536 bytes */
#define CS_TM_BM_DEF_VOQ_MAX_DEPTH_INT_BUFF	2048	/* 2048 cells =   64 KB */
#define CS_TM_BM_DEF_VOQ_MAX_DEPTH_EXT_BUFF	65530
#define CS_TM_BM_DEF_CPU_VOQ_MIN_DEPTH	128
#define CS_TM_BM_DEF_CPU_VOQ_MAX_DEPTH	32768

#define CS_TM_BM_DEF_DYING_GASP_INT	(6 * 24)
#define CS_TM_BM_DEF_LIVING_THSHLD_INT	(CS_TM_BM_DEF_DYING_GASP_INT + 6 * 3)

typedef struct {
	unsigned char disable;
	unsigned char recalculate_global_buffers_used;
	unsigned char bm2_bypass_ca_process;
	unsigned char bm2_bypass_dest_port_process;
	unsigned char bm2_bypass_voq_process;
	unsigned char bm1_bypass_ca_process;
	unsigned char bm1_bypass_dest_port_process;
	unsigned char bm1_bypass_voq_process;
	unsigned char enque_hdr_adjust;
	unsigned char init;
} cs_tm_bm_cfg_t;

typedef struct {
	unsigned short reserve_buffers;
	unsigned short reserve_buffers_inflight_pkts;
	unsigned short reserve_cpu_buffers;
	unsigned short reserve_linux0_buffers;
	unsigned short reserve_linux1_buffers;
	unsigned short reserve_linux2_buffers;
	unsigned short reserve_linux3_buffers;
	unsigned short reserve_linux4_buffers;
	unsigned short reserve_linux5_buffers;
	unsigned short reserve_linux6_buffers;
	unsigned short reserve_linux7_buffers;
} cs_tm_bm_reserve_buffer_t;

typedef struct {
	unsigned int wred_ad_update_period;
	unsigned char wred_buffer_alloc_only;
	unsigned char wred_agbd_weight;
	unsigned char wred_drop_mark_mode;
	unsigned char  wred_profile_select_mode;
	unsigned char wred_ad_update_mode;
	unsigned char wred_mode;
	unsigned char wred_adjust_range_index;
} cs_tm_bm_wred_cfg_t;


typedef struct {
    unsigned char wred_aqd_min_pct_baseline;
    unsigned char wred_aqd_max_pct_baseline;
    unsigned char wred_aqd_min_pct_buffers0;
    unsigned char wred_aqd_max_pct_buffers0;
    unsigned char wred_aqd_max_drop_probability;
} cs_tm_bm_wred_profile_t;

typedef struct {
	unsigned short warning_threshold;
	unsigned short dying_gasp_threshold;
	unsigned short living_threshold;
} cs_tm_bm_traffic_control_t;

typedef enum {
	CS_TM_BM_VOQ_MEM,
	CS_TM_BM_VOQ_STATUS_MEM,
	CS_TM_BM_DEST_PORT_MEM,
	CS_TM_BM_DEST_PORT_STATUS_MEM,
	CS_TM_BM_VOQ_PROFILE_MEM,
	CS_TM_BM_WRED_PROFILE_MEM,
} cs_tm_bm_mem_type_t;

typedef struct {
	unsigned char voq_profile;
	unsigned char wred_profile;
	unsigned char dest_port;
	unsigned char voq_cntr_enable;
	unsigned char voq_cntr;
	unsigned char wred_enable;
} cs_tm_bm_voq_mem_t;

typedef struct {
	unsigned short voq_write_ptr;
	unsigned short voq_depth;
	unsigned short voq_aqd;
} cs_tm_bm_voq_status_mem_t;

typedef struct {
	unsigned short dest_port_min_global_buffers;
	unsigned short dest_port_max_global_buffers;
} cs_tm_bm_dest_port_mem_t;

typedef struct {
	unsigned short voq_min_depth;
	unsigned short voq_max_depth;
	unsigned char wred_adjust_range_index;
	unsigned char wred_aqd_weight;
	unsigned char ref_cnt;
} cs_tm_bm_voq_profile_mem_t;

typedef struct {
	unsigned short min_pct_base;
	unsigned short max_pct_base;
	unsigned char min_pct_buffer;
	unsigned char max_pct_buffer;
	unsigned char drop_probility;
	unsigned char ref_cnt;
} cs_wred_profile_t;

typedef struct {
	unsigned char dest_port;
	unsigned char voq_prof_idx;
	unsigned char wred_prof_idx;
	unsigned char voq_cnt_enbl;
	unsigned char voq_cnt_idx;
	unsigned char wred_enbl;
} bm_voq_info_t;

/* VOQ-wise APIs */
int cs_tm_voq_reset(unsigned char voq_id);

int cs_tm_bm_set_voq_profile(unsigned char voq_id,
		unsigned short min_depth, unsigned short max_depth,
		void *wred_args);
int cs_tm_bm_set_voq_cntr(unsigned char voq_id, unsigned char voq_cntr_enbl,
		unsigned char voq_cntr_id);
int cs_tm_bm_get_voq_depth(unsigned char voq_id,
		unsigned short *p_min_depth, unsigned short *p_max_depth);

/* Port-wise APIs */
int cs_tm_port_reset(unsigned char port_id);

int cs_tm_bm_set_dest_port_mem(unsigned char addr,
		cs_tm_bm_dest_port_mem_t *dest_port);
int cs_tm_bm_get_dest_port_mem(unsigned char addr,
		cs_tm_bm_dest_port_mem_t *dest_port);

/* WRED configuration API */
int cs_tm_bm_set_wred_cfg(cs_tm_bm_wred_cfg_t *cfg);
int cs_tm_bm_get_wred_cfg(cs_tm_bm_wred_cfg_t *cfg);

/* Initialization */
int cs_tm_bm_init(void);

#endif /* __CS_TM_BM_H__ */
