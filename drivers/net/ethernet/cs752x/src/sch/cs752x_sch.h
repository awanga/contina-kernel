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
#ifndef __CS_SCHED_H__
#define __CS_SCHED_H__

#include <mach/platform.h>

#define SCH_MAX_ETH_PORT	5
#define SCH_MAX_PORT_QUEUE	8

#define SCH_MAX_ROOT_PORT	1
#define SCH_MAX_ROOT_QUEUE	8

#define SCH_MAX_CPU_PORT	8
#define SCH_MAX_CPU_QUEUE	8

#define SCH_PORT_NO (SCH_MAX_ETH_PORT+SCH_MAX_ROOT_PORT+SCH_MAX_CPU_PORT)

#define SCH_VOQ_NO (SCH_MAX_ETH_PORT*SCH_MAX_PORT_QUEUE + \
			SCH_MAX_ROOT_PORT*SCH_MAX_ROOT_QUEUE + \
			SCH_MAX_CPU_PORT*SCH_MAX_CPU_QUEUE)
#define SCH_DRR_BASE_ID	SCH_VOQ_NO
#define SCH_LT_BASE_ID (SCH_VOQ_NO + SCH_PORT_NO)
#define SCH_ST_BASE_ID (SCH_LT_BASE_ID + SCH_PORT_NO)

#define SCH_MAX_CALENDAR_LENGTH	32
#define SCH_CALENDAR1_OFFSET	32

#define SCH_MAX_PRIORITY	8
#define SCH_MAX_DRR_QUANTUM	32768
#define MAIN_TDM_A		0
#define MAIN_TDM_B		1
#define ENABLE_PORT		0
#define DISABLE_PORT		1

#define VOQ_AS_SP		0
#define VOQ_AS_DRR		1
#define DISABLE_VOQ		2
#define MOVE_VOQ_SP_DRR		3
#define MOVE_VOQ_DRR_SP		4
#define CHANGE_SHAPER_PARA	5
#define CHANGE_DRR_PARA		6
#define SET_BURST_SIZE		7
#define RECOVER_PARITY_ERROR	8
#define CHAHGE_PORT_LEVEL_MASK	9
#define SCHE_DEFAULT_QUANTA	9132
#define SCH_DEFAULT_RPT		0

#define CS_SCH_ETH_PORT0	0
#define CS_SCH_ETH_PORT1	1
#define CS_SCH_ETH_PORT2	2
#define CS_SCH_ETH_CRYPT	3
#define CS_SCH_ETH_ENCAP	4
#define CS_SCH_ROOT_PORT	5
#define CS_SCH_CPU_PORT0	6
#define CS_SCH_CPU_PORT1	(CS_SCH_CPU_PORT0 + 1)
#define CS_SCH_CPU_PORT2	(CS_SCH_CPU_PORT1 + 1)
#define CS_SCH_CPU_PORT3	(CS_SCH_CPU_PORT2 + 1)
#define CS_SCH_CPU_PORT4	(CS_SCH_CPU_PORT3 + 1)
#define CS_SCH_CPU_PORT5	(CS_SCH_CPU_PORT4 + 1)
#define CS_SCH_CPU_PORT6	(CS_SCH_CPU_PORT5 + 1)
#define CS_SCH_CPU_PORT7	(CS_SCH_CPU_PORT6 + 1)

/*
 * Change tmax from 0x3FFF to 0x4650 to fix packet loss when LT shaper is 
 * greater than input packets.
 * It works when (packet size <= 9000) and (LT shapping rate <= 1000Mbps).
 * However, the throughput is abnormal if (packet size = 9000) and 
 * (LT shapping rate = 2000Mbps).
 */
#define CS_SCH_TMAX_MAX		(0x4650)

#ifdef CONFIG_CORTINA_FPGA
#define SCHE_CLK (26000/3)	/* in unit of KHz */
#else
#define SCHE_CLK 200000		/* in unit of KHz */
#endif

#define BITS_CLEAR(data,start_pos,end_pos)\
		data &=  ~((0xFFFFFFFF >> (32 - (end_pos - start_pos + 1))) \
		<< start_pos)

typedef struct {
	unsigned char command;

	union {
		unsigned char voq_num;
		unsigned char shap_num;
		unsigned char burst_size;
		unsigned char mask;
	} arg0;

	union {
		unsigned int tsize;
		unsigned int quanta;
	} arg1;

	union {
		unsigned char cpu_port_num;
		unsigned char port_num;
	} arg2;

	union {
		unsigned int tmax;
	} arg3;

	union {
		unsigned char rpt;
	} arg4;
} sched_atomic_com_t;

typedef enum {
	PORT_SCH_ID = 0,
	ROOT_SCH_ID,
	CPU_SCH_ID,
	INVALD_SLOT,
} sched_id_t;

//typedef struct {
//	unsigned int tsize;	/* token_size */
//	unsigned int tmax;	/* max_tokens */
//	unsigned char rpt;	/* clks_per_token */
//} cpu_shaper_params_t;

typedef struct {
//	unsigned char port_id;	/* port id */
//	unsigned char queue_id;	/* queue id */
	unsigned char enabled;
	unsigned char is_drr;	/* 0:SP, 1:DRR */
	unsigned int quanta;
	/* long term shaper, for queue base and port base */
//	unsigned int lt_bandwidth;
	/* short term shaper, only applied in port based */
//	unsigned int st_bandwidth;
//	unsigned char shaper_id;
	/* 0~111 : VOQ shaper
	 * 112~125 : DRR Shaper
	 * 126~139 : LT shaper
	 * 140~153 : ST shaper
	 */
//	cpu_shaper_params_t lt_shaper;
} sched_queue_info_t;

typedef struct {
//	unsigned char port_id;
	sched_queue_info_t queue[SCH_MAX_PORT_QUEUE];
//	cpu_shaper_params_t lt_shaper;
//	cpu_shaper_params_t st_shaper;
} sched_port_info_t;

typedef struct {
	sched_port_info_t port[SCH_PORT_NO];
	/* 0~40: port voq 
	 * 41~47: Root voq 
	 * 48~111: CPU voq 
	 */
//	queue_params_t voq[SCH_MAX_ETH_PORT * SCH_MAX_PORT_QUEUE + 
//		SCH_MAX_ROOT_QUEUE + SCH_MAX_CPU_PORT * SCH_MAX_CPU_QUEUE];
//	cpu_shaper_params_t drr_shaper[SCH_MAX_ETH_PORT + SCH_MAX_ROOT_PORT +
//		SCH_MAX_CPU_PORT];
//	cpu_shaper_params_t lt_shaper[SCH_MAX_ETH_PORT + SCH_MAX_ROOT_PORT +
//		SCH_MAX_CPU_PORT];
//	cpu_shaper_params_t st_shaper[SCH_MAX_ETH_PORT + SCH_MAX_ROOT_PORT +
//		SCH_MAX_CPU_PORT];
} sched_info_t;

/* Schedule API */
/* Get the current TDM in use */
int cs752x_sch_get_curr_tdm_id(unsigned char * p_tdm_id);
/* switch the calendar */
int cs752x_sch_switch_calendar(void);
/* Changing TDM calendar */
int cs752x_sch_set_calendar(sched_id_t * p_calendar, unsigned char cal_len);
/* Retriving the current TDM calendar */
int cs752x_sch_get_calendar(sched_id_t * p_calendar, unsigned char cal_len);
/* Enabling a port */
int cs752x_sch_enable_port(unsigned char port_id);
/* Disabling a port */
int cs752x_sch_disable_port(unsigned char port_id);
/* Enable/Configure a queue to DRR */
int cs752x_sch_set_queue_drr(unsigned char port_id, unsigned char q_id,
		unsigned int quanta, unsigned int bandwidth);
/* Enable/Configure a queue to SP */
int cs752x_sch_set_queue_sp(unsigned char port_id, unsigned char q_id,
		unsigned int bandwidth);
/* Configure the rate on the queue */
int cs752x_sch_set_queue_rate(unsigned char port_id, unsigned char q_id,
		unsigned int bandwidth);
/* Disable a queue */
int cs752x_sch_disable_queue(unsigned char port_id, unsigned char q_id);
/* Configure the long-term rate on the port */
int cs752x_sch_set_port_rate_lt(unsigned char port_id, unsigned int lt_bw);
/* Configure the shjort-term rate on the port */
int cs752x_sch_set_port_rate_st(unsigned char port_id, unsigned int st_bw);
/* Configure the burst size for the port */
int cs752x_sch_set_port_burst_size(unsigned char port_id,
		unsigned char burst_size);
/* Reset the queue */
int cs752x_sch_reset_queue(unsigned char port_id, unsigned char q_id);
/* Reset the port */
int cs752x_sch_reset_port(unsigned char port_id);
/* API to all the support atomic command */
int cs752x_sch_atomic_command(sched_atomic_com_t * command);
/* API that handles scheduler initialization */
int cs752x_sch_init(void);
#endif /* __CS_SCHED_H__ */
