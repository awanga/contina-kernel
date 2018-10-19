/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

/* This header file is for GoldenGate Traffic Manager,
 * Policer Sub-module */

#ifndef __CS_TM_POL_H__
#define __CS_TM_POL_H__

#define	CS_TM_POL_RETRY_TIMES		(256)
#define CS_TM_POL_ACCESS_BIT		(1<<31)

#define CS_TM_POL_IPG_IDX_MIN		(1)
#define CS_TM_POL_IPG_IDX_MAX		(6)
#define CS_TM_POL_PORT_ID_MAX		(7)

#define CS_TM_POL_DEF_FLOW_POL_ID	(0)
#define CS_TM_POL_FLOW_POL_MAX		(127)

#ifdef CONFIG_CORTINA_FPGA
#define CS_TM_POL_MAX_CYCLE_RATE	(208 / 15)
#else
#define CS_TM_POL_MAX_CYCLE_RATE	(320)
#endif
#define CS_TM_POL_RESOL1_MIN_RATE	(320)
#define CS_TM_POL_RESOL1_MAX_RATE	(2621440)
#define CS_TM_POL_RESOL4_MIN_RATE	(80)
#define CS_TM_POL_RESOL4_MAX_RATE	(655360)
#define CS_TM_POL_RESOL16_MIN_RATE	(20)
#define CS_TM_POL_RESOL16_MAX_RATE	(163840)
#define CS_TM_POL_RESOL64_MIN_RATE	(5)
#define CS_TM_POL_RESOL64_MAX_RATE	(40960)

typedef enum {
	CS_TM_POL_DISABLE = 0,
	CS_TM_POL_RFC_2698 = 1,
	CS_TM_POL_RFC_2697 = 2,
	CS_TM_POL_RFC_4115 = 3,
	CS_TM_POL_TYPE_MAX
} cs_tm_pol_type_t;

typedef enum {
	CS_TM_POL_40_KHZ = 0,
	CS_TM_POL_10_KHZ = 1,
	CS_TM_POL_2_5_KHZ = 2,
	CS_TM_POL_0_625_KHZ = 3
} cs_tm_pol_freq_select_t;

typedef enum {
	CS_TM_POL_DIV_40_KHZ = 1,
	CS_TM_POL_DIV_10_KHZ = 4,
	CS_TM_POL_DIV_2_5_KHZ = 16,
	CS_TM_POL_DIV_0_625_KHZ = 64
} cs_tm_pol_divisor_select_t;

typedef struct
{
	unsigned char pol_disable;
	unsigned char pol_bypass_red;
	unsigned char pol_bypass_yellow;
	unsigned char pol_color_blind;
	unsigned char init;
} cs_tm_pol_cfg_t;

typedef struct {
	unsigned char bypass_yellow;
	unsigned char bypass_red;
	unsigned char disable;
	unsigned char color_blind;
	unsigned char bypass_yellow_define;
	unsigned char update_mode;
	unsigned char nest_level;
	unsigned char commit_override_enable;
} cs_tm_pol_cfg_spid_t;

typedef struct {
	unsigned char bypass_yellow;
	unsigned char bypass_red;
	unsigned char disable;
	unsigned char color_blind;
	unsigned char bypass_yellow_define;
	unsigned char update_mode;
	unsigned char nest_level;
	unsigned char commit_override_enable;
} cs_tm_pol_cfg_flow_t;

typedef struct {
	unsigned char bypass_yellow;
	unsigned char bypass_red;
	unsigned char disable;
	unsigned char color_blind;
	unsigned char bypass_yellow_define;
	unsigned char update_mode;
	unsigned char nest_level;
	unsigned char commit_override_enable;
	unsigned char lspid0;
	unsigned char lspid1;
	unsigned char lspid2;
	unsigned char lspid3;
} cs_tm_pol_cfg_pkt_type_t;

typedef struct {
	unsigned char bypass_yellow;
	unsigned char bypass_red;
	unsigned char disable;
	unsigned char color_blind;
	unsigned char bypass_yellow_define;
	unsigned char update_mode;
	unsigned char nest_level;
	unsigned char commit_override_enable;
	unsigned char voq0;
	unsigned char voq1;
} cs_tm_pol_cfg_cpu_t;

typedef enum {
	CS_TM_POL_FLOW_PROFILE_MEM,
	CS_TM_POL_FLOW_STATUS_MEM,
	CS_TM_POL_SPID_PROFILE_MEM,
	CS_TM_POL_SPID_STATUS_MEM,
	CS_TM_POL_CPU_PROFILE_MEM,
	CS_TM_POL_CPU_STATUS_MEM,
	CS_TM_POL_PKT_TYPE_PROFILE_MEM,
	CS_TM_POL_PKT_TYPE_STATUS_MEM,
} cs_tm_pol_mem_type_t;

typedef struct {
	unsigned int cir_token_bucket;
	unsigned int pir_token_bucket;
} cs_tm_pol_status_mem_t;

typedef struct {
	cs_tm_pol_type_t policer_type;
	cs_tm_pol_freq_select_t range;
	unsigned short cir_credit;
	unsigned short cir_max_credit;
	unsigned short pir_credit;
	unsigned short pir_max_credit;
	unsigned char bypass_yellow;
	unsigned char bypass_red;
} cs_tm_pol_profile_mem_t;

typedef volatile union {
	struct {
#ifdef CS_BIG_ENDIAN
		unsigned int pir_credit		:  2; /* bits 31:30 */
		unsigned int cir_max_credit	: 13; /* bits 29:17 */
		unsigned int cir_credit		: 13; /* bits 16:4 */
		unsigned int range		:  2; /* bits 3:2 */
		unsigned int policer_type	:  2; /* bits 1:0 */
#else /* CS_LITTLE_ENDIAN */
		unsigned int policer_type	:  2; /* bits 1:0 */
		unsigned int range		:  2; /* bits 3:2 */
		unsigned int cir_credit		: 13; /* bits 16:4 */
		unsigned int cir_max_credit	: 13; /* bits 29:17 */
		unsigned int pir_credit		:  2; /* bits 31:30 */
#endif
	} bf;
	unsigned int wrd;
} TM_POL_PROFILE_MEM_DATA0_t;

typedef volatile union {
	struct {
#ifdef CS_BIG_ENDIAN
		unsigned int rsrvd1		:  6;
		unsigned int bypass_red		:  1; /* bits 25:25 */
		unsigned int bypass_yellow	:  1; /* bits 24:24 */
		unsigned int pir_max_credit	: 13; /* bits 23:11 */
		unsigned int pir_credit		: 11; /* bits 10:0 */
#else /* CS_LITTLE_ENDIAN */
		unsigned int pir_credit		: 11; /* bits 10:0 */
		unsigned int pir_max_credit	: 13; /* bits 23:11 */
		unsigned int bypass_yellow	:  1; /* bits 24:24 */
		unsigned int bypass_red		:  1; /* bits 25:25 */
		unsigned int rsrvd1		:  6;
#endif
	} bf;
	unsigned int wrd;
} TM_POL_PROFILE_MEM_DATA1_t;

typedef struct cs_tm_pol_flow_pol_s {
	struct cs_tm_pol_flow_pol_s *next;
	unsigned char id;
	unsigned int ref_count;
	cs_tm_pol_type_t policer_type;
	unsigned char range;
	unsigned short cir_credit;
	unsigned short cir_max_credit;
	unsigned short pir_credit;
	unsigned short pir_max_credit;
	unsigned char bypass_yellow : 1,
			bypass_red : 1,
			rsrv : 6;
} cs_tm_pol_flow_pol_t;

typedef struct {
	cs_tm_pol_flow_pol_t *head;
	unsigned int mask[4];
	unsigned char used_count;
} cs_tm_pol_flow_pol_db_t;

int cs_tm_pol_set_cfg(cs_tm_pol_cfg_t *cfg);

int cs_tm_pol_get_cfg(cs_tm_pol_cfg_t *cfg);

int cs_tm_pol_set_spid_cfg(cs_tm_pol_cfg_spid_t *cfg);

int cs_tm_pol_get_spid_cfg(cs_tm_pol_cfg_spid_t *cfg);

int cs_tm_pol_set_flow_cfg(cs_tm_pol_cfg_flow_t *cfg);

int cs_tm_pol_get_flow_cfg(cs_tm_pol_cfg_flow_t *cfg);

int cs_tm_pol_set_pkt_type_cfg(cs_tm_pol_cfg_pkt_type_t *cfg);

int cs_tm_pol_get_pkt_type_cfg(cs_tm_pol_cfg_pkt_type_t *cfg);

int cs_tm_pol_set_cpu_cfg(cs_tm_pol_cfg_cpu_t *cfg);

int cs_tm_pol_get_cpu_cfg(cs_tm_pol_cfg_cpu_t *cfg);

int cs_tm_pol_set_ipq_dest(unsigned char index, unsigned char ipq_val);

int cs_tm_pol_get_ipq_dest(unsigned char index, unsigned char *ipg_val);

int cs_tm_pol_set_ipq_dest_map(unsigned char port_id, unsigned char index);

int cs_tm_pol_get_ipq_dest_map(unsigned char port_id, unsigned char *index);

int cs_tm_pol_set_ipq_src(unsigned char index, unsigned char ipq_val);

int cs_tm_pol_get_ipq_src(unsigned char index, unsigned char *ipg_val);

int cs_tm_pol_set_ipq_src_map(unsigned char port_id, unsigned char index);

int cs_tm_pol_get_ipq_src_map(unsigned char port_id, unsigned char *index);

int cs_tm_pol_get_pol_mem_status(cs_tm_pol_mem_type_t type,
		unsigned char *err_addr, unsigned char *err_correct_addr);

int cs_tm_pol_get_interrupt_0(unsigned int *int_status);

int cs_tm_pol_set_interrupt_0(unsigned int int_status);

int cs_tm_pol_get_intenable_0(unsigned int *int_enable);

int cs_tm_pol_set_intenable_0(unsigned int int_enable);

int cs_tm_pol_get_interrupt_1(unsigned int *int_status);

int cs_tm_pol_set_interrupt_1(unsigned int int_status);

int cs_tm_pol_get_intenable_1(unsigned int *int_enable);

int cs_tm_pol_set_intenable_1(unsigned int int_enable);

int cs_tm_pol_get_status_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_status_mem_t *bucket);

int cs_tm_pol_set_status_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_status_mem_t *bucket);

int cs_tm_pol_get_profile_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_profile_mem_t *profile);

int cs_tm_pol_set_profile_mem(cs_tm_pol_mem_type_t type, unsigned char addr,
		cs_tm_pol_profile_mem_t *profile);

int cs_tm_pol_rate_divisor_to_credit(unsigned int rate_bps,
		cs_tm_pol_freq_select_t freq_sel, unsigned short *p_credit);

int cs_tm_pol_convert_rate_to_hw_value(unsigned int rate_bps,
		unsigned char f_4_modes, cs_tm_pol_freq_select_t *p_freq_sel,
		unsigned short *p_credit);

/* Flow Policer Table Management */
int cs_tm_pol_get_avail_flow_policer(unsigned char *p_id);
int cs_tm_pol_get_flow_policer_used_count(unsigned char id,
		unsigned int *p_count);
int cs_tm_pol_inc_flow_policer_used_count(unsigned char id);
int cs_tm_pol_find_flow_policer(cs_tm_pol_profile_mem_t *p_pol_prof,
		unsigned char *p_id);
int cs_tm_pol_set_flow_policer(unsigned char id,
		cs_tm_pol_profile_mem_t *p_pol_profile);
int cs_tm_pol_del_flow_policer(unsigned char id);

int cs_tm_pol_init(void);
#endif

