/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

/* This header file is for GoldenGate Traffic Manager, Traffic
   Control Sub-module */
#ifndef __CS_TM_TC_H__
#define __CS_TM_TC_H__

typedef struct {
	unsigned char bm_int;
	unsigned char pol_int;
	unsigned char pm_int;
} cs_tm_tc_int_t;

typedef struct {
	unsigned char clear_cnt_on_read;
	unsigned char pol_enable;
	unsigned char pol_mode; /* FALSE = LEVEL mode, TRUE = PULSE mode */
	unsigned char bm_warning_enable;
	unsigned char bm_dying_gasp_enable;
} cs_tm_tc_param_t;

typedef struct {
	unsigned char clear_cnt_on_read;
	unsigned char pol_enable;
	unsigned char pol_mode;
	unsigned char pol_status;
	unsigned char pol_pause_cnt;
	unsigned char bm_warning_enable;
	unsigned char bm_dying_gasp_enable;
	unsigned char bm_status;
	unsigned char bm_pause_cnt;
} cs_tm_tc_status_t;

/* interrupt 0 */
/* set/get interrupt enable configuration. */
int cs_tm_tc_set_intenable_0(cs_tm_tc_int_t *int_cfg);
int cs_tm_tc_get_intenable_0(cs_tm_tc_int_t *int_cfg);

/* set/get what interrupt sources are causing an interrupt to be generated. */
int cs_tm_tc_set_interrupt_0(cs_tm_tc_int_t *int_status_ptr);
int cs_tm_tc_get_interrupt_0(cs_tm_tc_int_t *int_status_ptr);

/* interrupt 1 */
/* set/get interrupt enable configuration. */
int cs_tm_tc_set_intenable_1(cs_tm_tc_int_t *int_cfg);
int cs_tm_tc_get_intenable_1(cs_tm_tc_int_t *int_cfg);

/* set/get what interrupt sources are causing an interrupt to be generated. */
int cs_tm_tc_set_interrupt_1(cs_tm_tc_int_t *int_status_ptr);
int cs_tm_tc_get_interrupt_1(cs_tm_tc_int_t *int_status_ptr);

/* set/get pause frame for this port. Only work for GigE port. */
int cs_tm_tc_set_port_pause_frame(cs_port_id_t port_id,
		cs_tm_tc_param_t *p_param);
int cs_tm_tc_get_port_pause_frame(cs_port_id_t port_id,
		cs_tm_tc_status_t *status_ptr);

/* Initialization */
int cs_tm_tc_init(void);

/*
 * there are 3 table entries that can be used for traffic control.
 * If there is one currently used by the specific port, re-use the
 * table. If not, create one when there is a free table.  If no
 * free table, return error.
 */

#endif /* __CS_TM_TC_H__ */
