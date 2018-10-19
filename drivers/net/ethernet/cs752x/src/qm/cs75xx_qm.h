/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CS_QM_API_H__
#define __CS_QM_API_H__

#include <mach/platform.h>
#include <linux/skbuff.h>

//#define ACP_QM	1
#ifdef CONFIG_CS75XX_NO_JUMBO_FRAMES
#define MAX_PKT_LEN (SKB_WITH_OVERHEAD(2048) - 0x100 - 26)	/* 3 * 8 (CPU_HEADER) + 2 linux align */
#else
#define MAX_PKT_LEN 9022	/* 64 ~ 9200 */
#endif
#define MAX_CPU_PKT_LEN (MAX_PKT_LEN + 16)	/* MAX_PKT_LEN + CPU_HDR_SIZE */
//#define MAX_PKT_LEN 3800	/* 64 ~ 9200 */
#define MAX_CPU_BUFFERS	4096	/* 32 ~ 4096 , HW buffer poll */
#define LINUX_FREE_BUF			8
#define LINUX_FREE_BUF_LIST		10	/* each linux free buff */
#define LINUX_FREE_BUF_LIST_SIZE        (16 << LINUX_FREE_BUF_LIST)
#define MAXIMUM_MTU			1500

#define CS_QM_RETRY_TIMES	(256)
#define CS_QM_ACCESS_BIT	(1<<31)

/* enum type of QM_CONFIG_0_t.egress_flow_cntl_int_threshold */
typedef enum {
	CS_QM_EGRESS_FC_QTR_DEPTH,
	CS_QM_EGRESS_FC_HALF_DEPTH,
	CS_QM_EGRESS_FC_THREE_QTR_DEPTH,
	CS_QM_EGRESS_FC_EIGHT_PKT_DEPTH
} cs_qm_config0_egress_fc_threshold_e;

/* enum type of QM_CONFIG_0_t.egress_flow_cntl_mode */
typedef enum {
	CS_QM_EGRESS_FC_MODE_DISABLED,
	CS_QM_EGRESS_FC_MODE_BUCKET_TEST,
	CS_QM_EGRESS_FC_MODE_LOAD_SENSE,
	CS_QM_EGRESS_FC_MODE_INTERNAL_FEEDBACK
} cs_qm_config0_egress_fc_mode_e;

/* enum type of mem_config */
typedef enum {
	CS_QM_MEM_CFG_1G,
	CS_QM_MEM_CFG_2G,
	CS_QM_MEM_CFG_4G,
	CS_QM_MEM_CFG_8G
} cs_qm_config1_mem_cfg_e;

/* enum type of QM_STATUS_0_t.bufferSize */
typedef enum {
	CS_QM_BUFFER_SIZE_4K,
	CS_QM_BUFFER_SIZE_8K,
	CS_QM_BUFFER_SIZE_16K,
	CS_QM_BUFFER_SIZE_32K,
	CS_QM_BUFFER_SIZE_64K,
	CS_QM_BUFFER_SIZE_128K,
	CS_QM_BUFFER_SIZE_256K,
	CS_QM_BUFFER_SIZE_512K
} cs_qm_status0_buf_size_e;

/* enum type of QM_STATUS_0_t.buffers */
typedef enum {
	CS_QM_BUFFERS_1280,
	CS_QM_BUFFERS_1536,
	CS_QM_BUFFERS_2048
} cs_qm_status0_buffers_e;

/* enum type of QM_FLUSH_STATUS_t.flush_active */
typedef enum {
	CS_QM_FLUSH_STATUS_INACTIVE,
	CS_QM_FLUSH_STATUS_ACTIVE
} cs_qm_flush_status_e;

/* enum type of QM_FLUSH_VOQ_t.flush_immediate */
typedef enum {
	CS_QM_FLUSH_VOQ_IMMEDIATE,
	CS_QM_FLUSH_VOQ_CLEAN
} cs_qm_flush_voq_flush_type_t;

typedef enum {
	CS_QM_INT_BUFF_0,
	CS_QM_INT_BUFF_128,
	CS_QM_INT_BUFF_192,
	CS_QM_INT_BUFF_256,
} cs_qm_int_buff_e;
extern int internal_buff_size;


typedef struct {
	//cs_qm_cofnig0_egress_fc_mode_e fc_mode;
	cs_qm_config0_egress_fc_threshold_e fc_threshold;
	cs_qm_config1_mem_cfg_e mem_cfg;

	QM_CONFIG_0_t cfg_0;
	QM_CONFIG_1_t cfg_1;
	QM_CPU_PATH_CONFIG_0_t cpu_path_cfg_0;
	QM_CPU_PATH_CONFIG_1_t cpu_path_cfg_1;

	//cs_bool pkt_age_test_enable;
	//cs_bool que_age_test_enable;
	//cs_bool voq_disable_active;
	//cs_bool cpu_full_access;
} cs_qm_dev_cfg_t;

typedef volatile union {
	struct {
#ifdef CS_BIG_ENDIAN
		u32 rsrvd2:3;
		u32 high:13;
		u32 rsrvd1:3;
		u32 low:13;
#else
		u32 low:13;
		u32 rsrvd1:3;
		u32 high:13;
		u32 rsrvd2:3;
#endif
	} bf;
	u32 wrd;
} qm_cpu_path_buffers_t;

typedef enum {
	QM_EGRESS_FC_MODE_DISABLED,
	QM_EGRESS_FC_MODE_BUCKET_TEST,
	QM_EGRESS_FC_MODE_LOAD_SENSE,
	QM_EGRESS_FC_MODE_INT_FEED_BACK
} qm_egress_fc_mode_t;

typedef enum {
	QM_EGRESS_FC_THR_1BY4TH_FIFO,
	QM_EGRESS_FC_THR_2BY4TH_FIFO,
	QM_EGRESS_FC_THR_3BY4TH_FIFO,
	QM_EGRESS_FC_THR_FIFO_FULL
} qm_egress_fc_thr_t;

typedef union {
	struct {
#ifdef CS_BIG_EDIAN
		u32 rsrvd:18;
		u32 priority:3;
		u32 ds_speed:9;
		u32 packet_port:1;
		u32 disable:1;
#else
		u32 disable:1;
		u32 packet_port:1;
		u32 ds_speed:9;
		u32 priority:3;
		u32 rsrvd:18;
#endif
	} bf;
	u32 wrd;
} cs_qm_profile_mem_t;

/* Control registers used during DV are skipped here. */
void cs_qm_init(void);
void cs_qm_init_cfg(void);
void cs_qm_init_cpu_path_cfg(void);
int cs_qm_exit(void);

int cs_qm_set_gbl_params(cs_qm_dev_cfg_t * cfg);
int cs_qm_get_gbl_params(cs_qm_dev_cfg_t * cfg);

/* get status registers */
int cs_qm_get_status_0(u32 * status);
int cs_qm_get_status_1(u32 * status);

int cs_qm_get_ingress_status_path_p0(u32 * status);
int cs_qm_get_ingress_status_path_p1(u32 * status);

int cs_qm_get_ingress_status_cpu_path0(u32 * status);
int cs_qm_get_ingress_status_cpu_path1(u32 * status);

int cs_qm_get_egress_status0(u32 * status);
int cs_qm_get_egress_status1(u32 * status);

int cs_qm_egress_fc_bucket_init(cs_qm_dev_cfg_t * cfg);

int cs_qm_get_free_cpu_buffer_cnt(u32 * cnt);
int cs_qm_get_fifo_wr_cnt(u32 * fifo_wr_cnt);

int cs_qm_get_cpu_free_buffer_cnt(u32 qid, u32 * cnt);
int cs_qm_get_cpu_buffer_cnt(u32 qid, u32 * cnt);

/* read commit registers */
int cs_qm_set_rdcom_lp_fb_th(QM_RDCOM_LP_FB_TH_t val);
int cs_qm_get_rdcom_lp_s_sat(QM_RDCOM_LP_S_SAT_t * reg);
int cs_qm_get_rdcom_lp_0_sat(QM_RDCOM_LP_O_SAT_t * reg);
int cs_qm_get_rdcom_lp_ld_th(QM_RDCOM_LP_LD_TH_t * reg);
int cs_qm_set_rdcom_lp_config(QM_RDCOM_LP_CONFIG_t * reg);
int cs_qm_get_rdcom_lp_config(QM_RDCOM_LP_CONFIG_t * reg);
int cs_qm_get_rdcom_lp_so_bk(QM_RDCOM_LP_SO_BK_t * reg);
int cs_qm_get_rdcom_lp_obbk_tav(QM_RDCOM_LP_OBBK_TAV_t * reg);

int cs_qm_get_buffer_list_mem_status(QM_BUFFER_LIST_MEM_STATUS_t * reg);
int cs_qm_get_cpu_buffer_list_mem_status(QM_CPU_BUFFER_LIST_MEM_STATUS_t *reg);
int cs_qm_get_profile_mem_status(QM_PROFILE_MEM_STATUS_t * reg);
int cs_qm_get_status_mem_status(QM_STATUS_MEM_STATUS_t * reg);
int cs_qm_get_status_sdram_addr_mem_status(
		QM_STATUS_SDRAM_ADDR_MEM_STATUS_t * reg);
int cs_qm_status_get_pkt_age_old(QM_PKT_AGE_OLD_t * reg);
int cs_qm_queue_age_old(QM_QUE_AGE_OLD_t * reg);

int cs_qm_get_flush_status(u32 * status);
int cs_qm_flush_voq(QM_FLUSH_VOQ_t * reg);

/* QM interrupt status */
int cs_qm_get_int0(QM_INTERRUPT_0_t * int0);
int cs_qm_get_int1(QM_INTERRUPT_1_t * int1);
int cs_qm_set_int_enable(u32 reg, u32 val, u32 mask);

/* Send CPU buffers to QM */
int cs_qm_enqueue_cpu_buffer_list(unsigned char qid, u32 buf_addr);
void write_reg(u32 data, u32 bit_mask, u32 address);

int cs_qm_get_profile_mem(unsigned char addr, cs_qm_profile_mem_t *profile_mem);
int cs_qm_set_profile_mem(unsigned char addr, cs_qm_profile_mem_t *profile_mem);


#endif /* __CS_QM_API_H__ */
