/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

/* This header file is for GoldenGate Traffic Manager, Performance
   Monitor sub-module */

/*
 * 	BLOCKs			SECTIONs
 *
 * 1)	FLOW		--|	FLOW[128]
 * 2)	SPID		--|	SPID[8]
 * 3) 	VOQ		--|	VOQ[31]
 * 4)	PKT TYPE	--|	PKT_TYPE[20]
 * 5)	CPU		--|	CPU[10]
 * 			 -|	CPU_COPY[64]
 * 6)	GLOBAL		--|	RX_ALL
 * 			 -|	YELLOW
 * 			 -|	RED
 * 			 -|	PRE_YELLOW
 * 			 -|	PRE_RED
 * 			 -|	BM1_YELLOW
 * 			 -|	BM1_RED
 * 			 -|	BM2_YELLOW
 * 			 -|	BM2_RED
 * 			 -|	POL_YELLOW
 * 			 -|	POL_RED
 * 			 -|	BYPASS
 * 			 -|	BYPASS_FLOW_POL
 * 			 -|	BYPASS_POL
 * 			 -|	CE
 * */
#ifndef __CS_TM_PM_H__
#define __CS_TM_PM_H__

#define CS_TM_PM_RETRY_TIMES	(256)
#define CS_TM_PM_ACCESS_BIT	(1<<31)

typedef enum {
	CS_TM_PM_READ_MODE_NO_CLEAR = 0,
	CS_TM_PM_READ_MODE_CLEAR_ALL = 1,
	CS_TM_PM_READ_MODE_CLEAR_MSB = 2,
	CS_TM_PM_READ_MODE_MAX,
} cs_tm_pm_read_mode_e;

typedef struct
{
	unsigned char disable;
	unsigned char bypass_disable_byte_cntrs;
	unsigned char mark_mode;
	unsigned char cnt_mode;
	unsigned char auto_clear_on_read_mode;
	unsigned char init;
} cs_tm_pm_cfg_t;

typedef enum {
	CS_TM_PM_FLOW_SIZE = 128,
	CS_TM_PM_SPID_SIZE = 8,
	CS_TM_PM_VOQ_SIZE = 32,
	CS_TM_PM_CPU_SIZE = 10,
	CS_TM_PM_PKT_TYPE_SIZE = 20,
	CS_TM_PM_CPU_COPY_SIZE = 64,
} cs_tm_pm_cntr_mem_size_t;

typedef enum {
	/* General Blocks that Performance Monitor has to keep count on
	 * pkts/bytes received, pkts/bytes marked, and pkt/bytes dropped. */
	CS_TM_PM_FLOW_0 = 0,
	CS_TM_PM_FLOW_127 = 0x7f,
	CS_TM_PM_SPID_0 = 0x400,
	CS_TM_PM_SPID_7 = 0x407,
	CS_TM_PM_VOQ_0 = 0x600,
	CS_TM_PM_VOQ_31 = 0x61f,
	CS_TM_PM_CPU_0 = 0x680,
	CS_TM_PM_CPU_9 = 0x689,
	CS_TM_PM_PKT_TYPE_0 = 0x700,
	CS_TM_PM_PKT_TYPE_19 = 0x713,
	CS_TM_PM_CPU_COPY_0 = 0x780,
	CS_TM_PM_CPU_COPY_63 = 0x7bf,
	/* msb counters */
	CS_TM_PM_FLOW_MSB_0 = 0x800,
	CS_TM_PM_FLOW_MSB_127 = 0x87f,
	CS_TM_PM_SPID_MSB_0 = 0xc00,
	CS_TM_PM_SPID_MSB_7 = 0xc07,
	CS_TM_PM_VOQ_MSB_0 = 0xe00,
	CS_TM_PM_VOQ_MSB_31 = 0xe1f,
	CS_TM_PM_CPU_MSB_0 = 0xe80,
	CS_TM_PM_CPU_MSB_9 = 0xe89,
	CS_TM_PM_PKT_TYPE_MSB_0 = 0xf00,
	CS_TM_PM_PKT_TYPE_MSB_19 = 0xf13,
	CS_TM_PM_CPU_COPY_MSB_0 = 0xf80,
	CS_TM_PM_CPU_COPY_MSB_63 = 0xfbf,

} cs_tm_pm_cntr_mem_id_t;

typedef enum {
	/*
	 * Global Block that Performance Monitor only has to keey count on
	 * pkts and bytes received.
	 */
	CS_TM_PM_GLB_RX_ALL = 0,
	CS_TM_PM_GLB_YELLOW,
	CS_TM_PM_GLB_RED,
	CS_TM_PM_GLB_PRE_YELLOW,
	CS_TM_PM_GLB_PRE_RED,
	CS_TM_PM_GLB_BM1_YELLOW,
	CS_TM_PM_GLB_BM1_RED,
	CS_TM_PM_GLB_BM2_YELLOW,
	CS_TM_PM_GLB_BM2_RED,
	CS_TM_PM_GLB_POL_YELLOW,
	CS_TM_PM_GLB_POL_RED,
	CS_TM_PM_GLB_BYPASS,
	CS_TM_PM_GLB_BYPASS_FLOW_POL,
	CS_TM_PM_GLB_BYPASS_POL,
	CS_TM_PM_GLB_CE,
	/* ignoring those msb counters */
	CS_TM_PM_GLB_MSB = 31,
} cs_tm_pm_glb_cntr_mem_id_t;

typedef enum {
	/* Block ID */
	CS_TM_PM_FLOW_CNTR,
	CS_TM_PM_SPID_CNTR,
	CS_TM_PM_VOQ_CNTR,
	CS_TM_PM_CPU_CNTR,
	CS_TM_PM_PKT_TYPE_CNTR,
	CS_TM_PM_CPU_COPY_CNTR,
	CS_TM_PM_GLB_CNTR,
	CS_TM_PM_ALL_CNTR,
} cs_tm_pm_cntr_id_t;

typedef volatile union {
	struct {
#ifdef CS_BIG_ENDIAN
		unsigned int rsrvd1		: 26; /* bits 31:6 */
		unsigned int msb_byte_drop	:  1; /* bits 5 */
		unsigned int msb_byte_mark	:  1; /* bits 4 */
		unsigned int msb_byte		:  1; /* bits 3 */
		unsigned int msb_pkt_drop	:  1; /* bits 2 */
		unsigned int msb_pkt_mark	:  1; /* bits 1 */
		unsigned int msb_pkt		:  1; /* bits 0 */
#else /* CS_LITTLE_ENDIAN */
		unsigned int msb_pkt		:  1; /* bits 0 */
		unsigned int msb_pkt_mark	:  1; /* bits 1 */
		unsigned int msb_pkt_drop	:  1; /* bits 2 */
		unsigned int msb_byte		:  1; /* bits 3 */
		unsigned int msb_byte_mark	:  1; /* bits 4 */
		unsigned int msb_byte_drop	:  1; /* bits 5 */
		unsigned int rsrvd1		: 26; /* bits 31:6 */
#endif
	} bf;
	unsigned int     wrd;
} cs_tm_pm_cntr_msb_t;

typedef volatile union {
	struct {
#ifdef CS_BIG_ENDIAN
		unsigned int rsrvd1			:  2; /* bits 31:30 */
		unsigned int msb_ce_byte		:  1; /* bits 29 */
		unsigned int msb_ce_pkt			:  1; /* bits 28 */
		unsigned int msb_bypass_pol_byte	:  1; /* bits 27 */
		unsigned int msb_bypass_pol_pkt		:  1; /* bits 26 */
		unsigned int msb_bypass_flow_pol_byte	:  1; /* bits 25 */
		unsigned int msb_bypass_flow_pol_pkt	:  1; /* bits 24 */
		unsigned int msb_bypass_byte		:  1; /* bits 23 */
		unsigned int msb_bypass_pkt		:  1; /* bits 22 */
		unsigned int msb_pol_red_byte		:  1; /* bits 21 */
		unsigned int msb_pol_red_pkt		:  1; /* bits 20 */
		unsigned int msb_pol_yel_byte		:  1; /* bits 19 */
		unsigned int msb_pol_yel_pkt		:  1; /* bits 18 */
		unsigned int msb_bm2_red_byte		:  1; /* bits 17 */
		unsigned int msb_bm2_red_pkt		:  1; /* bits 16 */
		unsigned int msb_bm2_yel_byte		:  1; /* bits 15 */
		unsigned int msb_bm2_yel_pkt		:  1; /* bits 14 */
		unsigned int msb_bm1_red_byte		:  1; /* bits 13 */
		unsigned int msb_bm1_red_pkt		:  1; /* bits 12 */
		unsigned int msb_bm1_yel_byte		:  1; /* bits 11 */
		unsigned int msb_bm1_yel_pkt		:  1; /* bits 10 */
		unsigned int msb_pre_red_byte		:  1; /* bits 9 */
		unsigned int msb_pre_red_pkt		:  1; /* bits 8 */
		unsigned int msb_pre_yel_byte		:  1; /* bits 7 */
		unsigned int msb_pre_yel_pkt		:  1; /* bits 6 */
		unsigned int msb_red_byte		:  1; /* bits 5 */
		unsigned int msb_red_pkt		:  1; /* bits 4 */
		unsigned int msb_yel_byte		:  1; /* bits 3 */
		unsigned int msb_yel_pkt		:  1; /* bits 2 */
		unsigned int msb_glb_byte		:  1; /* bits 1 */
		unsigned int msb_glb_pkt		:  1; /* bits 0 */
#else /* CS_LITTLE_ENDIAN */
		unsigned int msb_glb_pkt		:  1; /* bits 0 */
		unsigned int msb_glb_byte		:  1; /* bits 1 */
		unsigned int msb_yel_pkt		:  1; /* bits 2 */
		unsigned int msb_yel_byte		:  1; /* bits 3 */
		unsigned int msb_red_pkt		:  1; /* bits 4 */
		unsigned int msb_red_byte		:  1; /* bits 5 */
		unsigned int msb_pre_yel_pkt		:  1; /* bits 6 */
		unsigned int msb_pre_yel_byte		:  1; /* bits 7 */
		unsigned int msb_pre_red_pkt		:  1; /* bits 8 */
		unsigned int msb_pre_red_byte		:  1; /* bits 9 */
		unsigned int msb_bm1_yel_pkt		:  1; /* bits 10 */
		unsigned int msb_bm1_yel_byte		:  1; /* bits 11 */
		unsigned int msb_bm1_red_pkt		:  1; /* bits 12 */
		unsigned int msb_bm1_red_byte		:  1; /* bits 13 */
		unsigned int msb_bm2_yel_pkt		:  1; /* bits 14 */
		unsigned int msb_bm2_yel_byte		:  1; /* bits 15 */
		unsigned int msb_bm2_red_pkt		:  1; /* bits 16 */
		unsigned int msb_bm2_red_byte		:  1; /* bits 17 */
		unsigned int msb_pol_yel_pkt		:  1; /* bits 18 */
		unsigned int msb_pol_yel_byte		:  1; /* bits 19 */
		unsigned int msb_pol_red_pkt		:  1; /* bits 20 */
		unsigned int msb_pol_red_byte		:  1; /* bits 21 */
		unsigned int msb_bypass_pkt		:  1; /* bits 22 */
		unsigned int msb_bypass_byte		:  1; /* bits 23 */
		unsigned int msb_bypass_flow_pol_pkt	:  1; /* bits 24 */
		unsigned int msb_bypass_flow_pol_byte	:  1; /* bits 25 */
		unsigned int msb_bypass_pol_pkt		:  1; /* bits 26 */
		unsigned int msb_bypass_pol_byte	:  1; /* bits 27 */
		unsigned int msb_ce_pkt			:  1; /* bits 28 */
		unsigned int msb_ce_byte		:  1; /* bits 29 */
		unsigned int rsrvd1			:  2; /* bits 31:30 */
#endif
	} bf;
	unsigned int wrd;
} cs_tm_pm_glb_msb_blk_t;

typedef struct {
	cs_tm_pm_cntr_msb_t flow[128];
} cs_tm_pm_flow_msb_blk_t;

typedef struct {
	cs_tm_pm_cntr_msb_t spid[8];
} cs_tm_pm_spid_msb_blk_t;

typedef struct {
	cs_tm_pm_cntr_msb_t voq[32];
} cs_tm_pm_voq_msb_blk_t;

typedef struct {
	cs_tm_pm_cntr_msb_t pkt_type[20];
} cs_tm_pm_pkt_type_msb_blk_t;

typedef struct {
	cs_tm_pm_cntr_msb_t cpu[10];
} cs_tm_pm_cpu_msb_blk_t;

typedef struct {
	cs_tm_pm_cntr_msb_t cpu_copy[64];
} cs_tm_pm_cpu_copy_msb_blk_t;

typedef struct {
	unsigned int pkts;
	unsigned int pkts_mark;
	unsigned int pkts_drop;
	unsigned long long bytes;
	unsigned long long bytes_mark;
	unsigned long long bytes_drop;
} cs_tm_pm_cntr_t;

typedef struct {
	cs_tm_pm_cntr_t flow[128];
} cs_tm_pm_flow_blk_t;

typedef struct {
	cs_tm_pm_cntr_t spid[8];
} cs_tm_pm_spid_blk_t;

typedef struct {
	cs_tm_pm_cntr_t voq[32];
} cs_tm_pm_voq_blk_t;

typedef struct {
	cs_tm_pm_cntr_t pkt_type[20];
} cs_tm_pm_pkt_type_blk_t;

typedef struct {
	cs_tm_pm_cntr_t cpu[10];
} cs_tm_pm_cpu_blk_t;

typedef struct {
	cs_tm_pm_cntr_t cpu_copy[64];
} cs_tm_pm_cpu_copy_blk_t;


typedef struct {
	unsigned int pkts;
	unsigned long long bytes;
} cs_tm_pm_glb_cntr_t;

typedef struct {
#if 0
	cs_tm_pm_glb_cntr_t rx_all;
	cs_tm_pm_glb_cntr_t yellow;
	cs_tm_pm_glb_cntr_t red;
	cs_tm_pm_glb_cntr_t pre_yellow;
	cs_tm_pm_glb_cntr_t pre_red;
	cs_tm_pm_glb_cntr_t bm1_yellow;
	cs_tm_pm_glb_cntr_t bm1_red;
	cs_tm_pm_glb_cntr_t bm2_yellow;
	cs_tm_pm_glb_cntr_t bm2_red;
	cs_tm_pm_glb_cntr_t pol_yellow;
	cs_tm_pm_glb_cntr_t pol_red;
	cs_tm_pm_glb_cntr_t bypass;
	cs_tm_pm_glb_cntr_t bypass_flow_pol;
	cs_tm_pm_glb_cntr_t bypass_pol;
	cs_tm_pm_glb_cntr_t ce;
#else
	cs_tm_pm_glb_cntr_t glb_cntr[CS_TM_PM_GLB_CE+1];
#endif
} cs_tm_pm_glb_blk_t;

/* struct definition */
typedef struct {
	cs_tm_pm_flow_blk_t flow_blk;
	cs_tm_pm_spid_blk_t spid_blk;
	cs_tm_pm_voq_blk_t voq_blk;
	cs_tm_pm_cpu_blk_t cpu_blk;
	cs_tm_pm_pkt_type_blk_t pkt_type_blk;
	cs_tm_pm_cpu_copy_blk_t cpu_copy_blk;
	cs_tm_pm_glb_blk_t tm_glb_blk;
} cs_tm_pm_t;

typedef struct {
	cs_tm_pm_flow_msb_blk_t flow_blk;
	cs_tm_pm_spid_msb_blk_t spid_blk;
	cs_tm_pm_voq_msb_blk_t voq_blk;
	cs_tm_pm_cpu_msb_blk_t cpu_blk;
	cs_tm_pm_pkt_type_msb_blk_t pkt_type_blk;
	cs_tm_pm_cpu_copy_msb_blk_t cpu_copy_blk;
	cs_tm_pm_glb_msb_blk_t tm_glb_blk;
} cs_tm_pm_msb_t;


/**************************************************/
/******************* API section ******************/
/**************************************************/
/* initialization */
int cs_tm_pm_init(void);

/* Configure performance monitoring operation mode */
int cs_tm_pm_set_cfg(cs_tm_pm_cfg_t *cfg);

/* Get the operation mode of PM */
int cs_tm_pm_get_cfg(cs_tm_pm_cfg_t *cfg);

/* Get status of Counter Memory */
int cs_tm_pm_get_cntr_mem_status(unsigned char *err_cntl,
		unsigned short *err_addr);

/* Get status of Global Counter Memory */
int cs_tm_pm_get_glb_cntr_mem_status(unsigned int *err_addr);

/* get the counter statistics of a specific counter */
int cs_tm_pm_get_cntr(cs_tm_pm_cntr_id_t cntr_type, unsigned int start_adr,
		unsigned int size, void *counter);

/* reset the counter statistics of a specific counter */
int cs_tm_pm_reset_cntr(cs_tm_pm_cntr_id_t cntr_type, unsigned int start_adr,
		unsigned int size);

/* Get the counters of PM Counter Memory. */
int cs_tm_pm_get_cntr_by_type(cs_tm_pm_cntr_id_t cntr_type,
		void *cntr_mem);

/* Get the MSB counters of PM Counter Memory. */
int cs_tm_pm_get_msb_counter(cs_tm_pm_cntr_id_t cntr_type,
		void *cntr_mem);

#endif /* __CS_TM_PM_H__ */
