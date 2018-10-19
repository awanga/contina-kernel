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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "cs752x_sch.h"

sched_info_t sched_queue_table;

/* Internal API */
static int sch_cpu_start_write(void)
{
	unsigned char cpu_cmd_executed;
	SCH_STATUS_t status;
	SCH_ATOMIC_COMMAND_CONTROL_t cmd_control;
	int timeout = 10000;

	cmd_control.wrd = 0;
	/* Write to scheduler atomic command access register */
	cmd_control.bf.start_write = 1;
	writel(cmd_control.wrd, SCH_ATOMIC_COMMAND_CONTROL);

	/* Wait for interrupt status to be set. */
	do {
		cpu_cmd_executed = (readl(SCH_INTERRUPT_0) & 0x1);
		udelay(1);
	} while ((cpu_cmd_executed == 0) && timeout--);

	/* Clear interrupt */
	if (cpu_cmd_executed != 0)
		writel(cpu_cmd_executed, SCH_INTERRUPT_0);

	if (timeout <= 0) {
		printk("CPU command executed fail!\n");
		return -1;
	}

	status.wrd = readl(SCH_STATUS);
	if (status.bf.cpu_cmd_status != 0) {
		printk("schedule CMD error:0x%x\n", status.bf.cpu_cmd_status);
		return -1;
	}
	//printk("CPU command executed completed!\n");

	return 0;
}

/*
 * This API is used to program the main TDM calendar in the scheduler. The
 * API takes the entries from the array cal_entries with entry_num given and
 * programs the main TDM calendar. There are 32 programmable slots in the main
 * TDM calendar. The slots should be assigned to the schedulers in proportion
 * to the output rate required, where PORT_SCH_ID = (0), ROOT_SCH_ID = (1),
 * and CPU_SCH_ID = (2).
 */
int sch_set_cal_entry(char tdm_id, unsigned char entry_num,
		sched_id_t * p_cal_entries)
{
	//SCH_MAIN_TDM_PORT_CALENDAR0_SETA_t clndr0_cfg;
	//SCH_MAIN_TDM_PORT_CALENDAR1_SETA_t clndr1_cfg;
	unsigned int cal0 = 0, cal1 = 0;
	unsigned short cal_cnt, cal_offset;

	if (entry_num > SCH_MAX_CALENDAR_LENGTH) {
		printk("Invalid entry_num, should be less or equal to 32 "
				"entry_num %d\n", entry_num);
		return -1;
	}

	if (p_cal_entries == NULL) {
		printk("Null pointer p_cal_entries \n");
		return -1;
	}

	for (cal_cnt = 0; cal_cnt < entry_num; cal_cnt++) {
		/*
		 * find the offset multiply by 2 will give you the exact bit
		 * location.
		 */
		cal_offset = (cal_cnt << 1);

		if (cal_offset >= SCH_CALENDAR1_OFFSET) {
			/* Need to clear the bits before setting */
			BITS_CLEAR(cal1, (cal_offset - 32),
				   (cal_offset - 32) + 1);
			cal1 |=
			    ((p_cal_entries[cal_cnt] & 0x3) <<
			     (cal_offset % SCH_CALENDAR1_OFFSET));
		} else {
			/* Need to clear the bits before setting */
			BITS_CLEAR(cal0, cal_offset, cal_offset + 1);
			cal0 |= ((p_cal_entries[cal_cnt] & 0x3) <<
				 (cal_offset % SCH_CALENDAR1_OFFSET));
		}
	}
	if (tdm_id == MAIN_TDM_A) {
		writel(cal0, SCH_MAIN_TDM_PORT_CALENDAR0_SETA);
		writel(cal1, SCH_MAIN_TDM_PORT_CALENDAR1_SETA);
		writel((entry_num - 1), SCH_MAIN_TDM_CONTROL_SETA);
	} else {
		writel(cal0, SCH_MAIN_TDM_PORT_CALENDAR0_SETB);
		writel(cal1, SCH_MAIN_TDM_PORT_CALENDAR1_SETB);
		writel((entry_num - 1), SCH_MAIN_TDM_CONTROL_SETB);
	}

	return 0;
}

/* This API is used to retrieve the entries in the main scheduler TDM calendar.
 * The API get all the entries (0 to 31) store  into the pointer with given
 * length of the array. */
static int sch_get_cal_entry(char tdm_id, sched_id_t * p_cal_entries,
				   unsigned char cal_len)
{
	unsigned int cal0, cal1;
	unsigned char cal_cnt, cal_end_idx;

	if (p_cal_entries == NULL) {
		printk("Null pointer p_calen_entries\n");
		return -1;
	}

	if (tdm_id == MAIN_TDM_A) {
		cal0 = readl(SCH_MAIN_TDM_PORT_CALENDAR0_SETA);
		cal1 = readl(SCH_MAIN_TDM_PORT_CALENDAR1_SETA);
	} else {
		cal0 = readl(SCH_MAIN_TDM_PORT_CALENDAR0_SETB);
		cal1 = readl(SCH_MAIN_TDM_PORT_CALENDAR1_SETB);
	}

	if (cal_len < 16)
		cal_end_idx = cal_len;
	else
		cal_end_idx = 16;

	/* first word of calendar entry */
	for (cal_cnt = 0; cal_cnt < cal_end_idx; cal_cnt++, cal0 >>= 2)
		*(p_cal_entries + cal_cnt) = (cal0 & 0x3);

	if (cal_len <= 16)
		cal_end_idx = 16;
	else if (cal_len < 32)
		cal_end_idx = cal_len;
	else
		cal_end_idx = 32;

	/* second word of calendar entry */
	for (cal_cnt = 16; cal_cnt < cal_end_idx; cal_cnt++, cal1 >>= 2)
		*(p_cal_entries + cal_cnt) = (cal1 & 0x3);

	return 0;
}

/* change port level mask */
static int sch_change_port_mask(unsigned char port_id, unsigned char mask)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (CHAHGE_PORT_LEVEL_MASK & 0xF);
	cmd_data0.wrd |= ((mask & 0x1) << 4);

	cmd_data1.wrd = (port_id & 0xF);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* enable a disabled VOQ into DRR mode */
static int sch_enable_voq_drr(unsigned char voq_id, unsigned int quanta,
		unsigned char cpu_port_id)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (VOQ_AS_DRR & 0xF);
	cmd_data0.wrd |= ((voq_id & 0x7F) << 4);
	cmd_data0.wrd |= ((quanta & 0x3FFFF) << 12);

	cmd_data1.wrd = (cpu_port_id & 0x7);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* enable a disable VOQ into SP mode */
static int sch_enable_voq_sp(unsigned char voq_id, unsigned int tsize,
		unsigned char cpu_port_id, unsigned int tmax, unsigned char rpt)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (VOQ_AS_SP & 0xF);
	cmd_data0.wrd |= ((voq_id & 0x7F) << 4);
	cmd_data0.wrd |= ((tsize & 0x3FFFF) << 12);

	cmd_data1.wrd = (cpu_port_id & 0x7);
	cmd_data1.wrd |= ((tmax & 0x3FFFF) << 4);
	cmd_data1.wrd |= ((rpt & 0x1F) << 22);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* Move an active VOQ from SP to DRR */
static int sch_voq_sp_to_drr(unsigned char voq_id, unsigned int quanta,
		unsigned char cpu_port_id)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (MOVE_VOQ_SP_DRR & 0xF);
	cmd_data0.wrd |= ((voq_id & 0x7F) << 4);
	cmd_data0.wrd |= ((quanta & 0x3FFFF) << 12);

	cmd_data1.wrd = (cpu_port_id & 0x7);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* Move an active VOQ from DRR to SP */
static int sch_voq_drr_to_sp(unsigned char voq_id, unsigned int tsize,
		unsigned char cpu_port_id, unsigned int tmax, unsigned char rpt)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (MOVE_VOQ_DRR_SP & 0xF);
	cmd_data0.wrd |= ((voq_id & 0x7F) << 4);
	cmd_data0.wrd |= ((tsize & 0x3FFFF) << 12);

	cmd_data1.wrd = (cpu_port_id & 0x7);
	cmd_data1.wrd |= ((tmax & 0x3FFFF) << 4);
	cmd_data1.wrd |= ((rpt & 0x1F) << 22);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* Disable a VOQ */
static int sch_disable_voq(unsigned char voq_id, unsigned char cpu_port_id)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (DISABLE_VOQ & 0xF);
	cmd_data0.wrd |= ((voq_id & 0x7F) << 4);

	cmd_data1.wrd = (cpu_port_id & 0x7);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* Change shaper param */
static int sch_change_shaper_param(unsigned char shaper_id, unsigned int tsize,
		unsigned char cpu_port_id, unsigned int tmax, unsigned char rpt)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (CHANGE_SHAPER_PARA & 0xF);
	cmd_data0.wrd |= ((shaper_id & 0xFF) << 4);
	cmd_data0.wrd |= ((tsize & 0x3FFFF) << 12);

	cmd_data1.wrd = (cpu_port_id & 0x7);
	cmd_data1.wrd |= ((tmax & 0x3FFFF) << 4);
	cmd_data1.wrd |= ((rpt & 0x1F) << 22);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* Change quanta for DRR VOQ */
static int sch_change_drr_quanta(unsigned char voq_id, unsigned int quanta,
		unsigned char cpu_port_id)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (CHANGE_DRR_PARA & 0xF);
	cmd_data0.wrd |= ((voq_id & 0x7F) << 4);
	cmd_data0.wrd |= ((quanta & 0x3FFFF) << 12);

	cmd_data1.wrd = (cpu_port_id & 0x7);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* Set Burst Size for port, burst size given should be within 16 to 256 byte */
int cs752x_sch_set_port_burst_size(unsigned char port_id,
		unsigned char burst_size)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	/* out of possible burst_size range */
	if ((burst_size > 0) && (burst_size < 16))
		return -1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (SET_BURST_SIZE & 0xF);
	cmd_data0.wrd |= ((burst_size & 0xFF) << 4);

	cmd_data1.wrd = (port_id & 0xF);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* recover port from parity error */
static int sch_recover_port_parity_err(unsigned char port_id)
{
	SCH_ATOMIC_COMMAND_DATA0_t cmd_data0;
	SCH_ATOMIC_COMMAND_DATA1_t cmd_data1;

	cmd_data0.wrd = 0;
	cmd_data1.wrd = 0;

	cmd_data0.wrd = (RECOVER_PARITY_ERROR & 0xF);
	cmd_data1.wrd = (port_id & 0xF);

	writel(cmd_data0.wrd, SCH_ATOMIC_COMMAND_DATA0);
	writel(cmd_data1.wrd, SCH_ATOMIC_COMMAND_DATA1);

	return sch_cpu_start_write();
}

/* reset the scheduler */
static int sch_reset(void)
{
	SCH_STATUS_t status;

	/* Soft Reset */
	writel(1, SCH_CONTROL);
	udelay(100);

	/* Clear Reset */
	writel(0, SCH_CONTROL);
	udelay(100);

	status.wrd = readl(SCH_STATUS);
	if (status.bf.sch_mem_init_done == 0) {
		printk("schedule mem init fail!(0x%x)\n", status.wrd);
		return -1;
	}
	return 0;
}

static unsigned int sch_get_max_share_factor(unsigned int num1,
		unsigned int num2)
{
	unsigned int a, b, c = 1;

	if ((num1 == 0) || (num2 == 0))
		return 0;
	if (num1 == num2)
		return num1;

	if (num1 > num2) {
		a = num1;
		b = num2;
	} else {
		a = num2;
		b = num1;
	}

	while (c) {
		c = a % b;
		a = b;
		b = c;
	}

	return (a);
}

static int sch_calc_rate_limiter_param(unsigned int rate_kbps,
		unsigned int clocks_per_round, unsigned int tmr_max,
		unsigned int delta_max, unsigned char * p_tmr_val,
		unsigned int * p_delta)
{
	unsigned int aa, bb;
	unsigned int a = 0, b = 0;
	unsigned int ff, c = 0, d = 0;
	unsigned int diff_rate = 0xFFFFFFFF, diff = 0, c_rate;

	if (rate_kbps == 0) {
		/* Disable the traffic */
		*p_tmr_val = 0x0;
		*p_delta = 0;
		return (0);
	}

	/*   Rate Calcualtion equation:
	 *                200*8 * Tokens * 1000
	 *   X kbps =   -------------------------
	 *              (timer)*clocks_per_round
	 *
	 *  Tokens(delta)       X *clocks_per_round      X * 32000      X
	 *   -----------    =   -------------------  =  ----------  =  ---
	 *  Timer (RPT+1)          200 * 8 * 1000       200*8*1000     50
	 */

	ff = sch_get_max_share_factor((SCHE_CLK * 8), clocks_per_round);
	c = (SCHE_CLK * 8) / ff;
	d = clocks_per_round / ff;

	for (bb = 1; bb <= (tmr_max + 1); bb++) {
		aa = ((rate_kbps * bb * d) / c) +
			(((rate_kbps * bb * d % c) > (c / 2)) ? 1 : 0);

		/* This is for calclulating 1 in upper range */
		if (aa == 0)
			aa = 1;

		if (aa > delta_max)
			aa = delta_max;

		/* crate = rate in kbs *100 */
		c_rate = ((aa * c) / (d * bb) * 100) +
			((((aa * c) % (d * bb)) * 100) / (d * bb));

		diff = (rate_kbps * 100 > c_rate) ? (rate_kbps * 100 - c_rate)
			: (c_rate - rate_kbps * 100);

#if 0
		printk("  *****> bb %d aa %d rate %d c_rate %d diff %d \n",
		       bb, aa, rate_kbps, c_rate, diff);
#endif

		if (diff < diff_rate) {
			diff_rate = diff;
			b = bb;
			a = aa;
		}

		/* Check whether the diff rate is less than 1% of the configured
		 * rate. In that case choose this rpt */
		if ((rate_kbps >= 100 && (diff_rate / 100 < rate_kbps / 100)) ||
		    ((rate_kbps < 100) && (diff_rate < (rate_kbps % 100))))
			break;
	}

	/* actual register value should be 1 less than real value */
	*p_tmr_val = (unsigned char) (b - 1);
	*p_delta = a;

	return (0);
}

/* External API */
int cs752x_sch_get_curr_tdm_id(unsigned char * p_tdm_id)
{
	if (p_tdm_id == NULL)
		return -1;

	*p_tdm_id = (readl(SCH_STATUS) & 0x2) > 1;

	return 0;
}

int cs752x_sch_switch_calendar(void)
{
	writel(1, SCH_SWITCH_TDM_CONTROL_SET);	/* Switch TDM */

	return 0;
}

int cs752x_sch_set_calendar(sched_id_t * p_calendar, unsigned char cal_len)
{
	unsigned char current_tdm;
	int status;

	if (cal_len > SCH_MAX_CALENDAR_LENGTH)
		return -1;
	if (p_calendar == NULL)
		return -1;

	cs752x_sch_get_curr_tdm_id(&current_tdm);

	if (current_tdm == MAIN_TDM_B)
		status = sch_set_cal_entry(MAIN_TDM_A, cal_len, p_calendar);
	else
		status = sch_set_cal_entry(MAIN_TDM_B, cal_len, p_calendar);

	if (status != 0)
		return status;

	cs752x_sch_switch_calendar();

	return 0;
}

int cs752x_sch_get_calendar(sched_id_t * p_calendar, unsigned char cal_len)
{
	unsigned char current_tdm;

	if (p_calendar == NULL)
		return -1;

	cs752x_sch_get_curr_tdm_id(&current_tdm);
	return sch_get_cal_entry(current_tdm, p_calendar, cal_len);
}

int cs752x_sch_enable_port(unsigned char port_id)
{
	return sch_change_port_mask(port_id, ENABLE_PORT);
}

int cs752x_sch_disable_port(unsigned char port_id)
{
	return sch_change_port_mask(port_id, DISABLE_PORT);
}

int cs752x_sch_set_queue_drr(unsigned char port_id, unsigned char q_id,
		unsigned int quanta, unsigned int bandwidth)
{
	sched_queue_info_t *p_voq;
	int status = 0;
	unsigned char rpt;
	unsigned int tsize, tmax;
	SCH_SHAPER_CONFIGURATION_t clock_per_round;

	if ((port_id >= SCH_PORT_NO) || (q_id >= SCH_MAX_PORT_QUEUE))
		return -1;

	p_voq = &sched_queue_table.port[port_id].queue[q_id];
	/* prepare atomic command */
	if (p_voq->enabled == 0) { /* not enabled yet, just enable as DRR */
		p_voq->is_drr = 1;
		p_voq->enabled = 1;
		p_voq->quanta = quanta;
		status = sch_enable_voq_drr((port_id * 8 + q_id), quanta,
				port_id >= CS_SCH_CPU_PORT0 ?
				(port_id - CS_SCH_CPU_PORT0) : 0);
	} else if (p_voq->is_drr == 0) { /* It's SP currently, move to DRR */
		p_voq->is_drr = 1;
		p_voq->quanta = quanta;
		status = sch_voq_sp_to_drr((port_id * 8 + q_id), quanta,
				port_id >= CS_SCH_CPU_PORT0 ?
				(port_id - CS_SCH_CPU_PORT0) : 0);
	} else if (p_voq->is_drr == 1) {	/* need to change DRR quanta */
		p_voq->quanta = quanta;
		status = sch_change_drr_quanta((port_id * 8 + q_id), quanta,
				port_id >= CS_SCH_CPU_PORT0 ?
				(port_id - CS_SCH_CPU_PORT0) : 0);
	}

	if (status != 0)
		return status;

	/* Set DRR lt shaper parameter based on bandwidth */
	clock_per_round.wrd = readl(SCH_SHAPER_CONFIGURATION);
	status = sch_calc_rate_limiter_param((bandwidth / 1000),
			clock_per_round.bf.clocks_per_round_lt, 0xF, 0x3FFFF,
			&rpt, &tsize);
	if (status != 0)
		return status;

	if (tsize == 0)
		tmax = 1;
	else
		tmax = CS_SCH_TMAX_MAX;

	/* Set shaper parameter */
	status = sch_change_shaper_param(port_id + SCH_DRR_BASE_ID, tsize,
			port_id >= CS_SCH_CPU_PORT0 ?
			port_id - CS_SCH_CPU_PORT0 : 0, tmax, rpt);

	return status;
}

int cs752x_sch_set_queue_sp(unsigned char port_id, unsigned char q_id,
		unsigned int bandwidth)
{
	sched_queue_info_t *p_voq;
	unsigned char rpt;
	int status = 0;
	unsigned int tsize, tmax;
	SCH_SHAPER_CONFIGURATION_t clock_per_round;

	if ((port_id >= SCH_PORT_NO) || (q_id >= SCH_MAX_PORT_QUEUE))
		return -1;

	/* Calculate parameters from bandwidth */
	clock_per_round.wrd = readl(SCH_SHAPER_CONFIGURATION);
	status = sch_calc_rate_limiter_param((bandwidth / 1000),
			clock_per_round.bf.clocks_per_round_lt, 0xF, 0x3FFFF,
			&rpt, &tsize);
	if (status != 0)
		return status;

	if (tsize == 0)
		tmax = 1;
	else
		tmax = CS_SCH_TMAX_MAX;

	p_voq = &sched_queue_table.port[port_id].queue[q_id];

	/* prepare atomic command */
	if (p_voq->enabled == 0) {	/* not enabled yet, just enable as SP */
		p_voq->is_drr = 0;
		p_voq->enabled = 1;
		status = sch_enable_voq_sp(port_id * 8 + q_id, tsize,
				port_id >= CS_SCH_CPU_PORT0 ?
				port_id - CS_SCH_CPU_PORT0 : 0, tmax, rpt);
	} else if (p_voq->is_drr == 1) { /* It's DRR currently, move to SP */
		p_voq->is_drr = 0;
		status = sch_voq_drr_to_sp(port_id * 8 + q_id, tsize,
				port_id >= CS_SCH_CPU_PORT0 ?
				port_id - CS_SCH_CPU_PORT0 : 0, tmax, rpt);
	} else if (p_voq->is_drr == 0) {
		/* It's already SP, but we might change its shaper para */
		status = sch_change_shaper_param(port_id * 8 + q_id, tsize,
				port_id >= CS_SCH_CPU_PORT0 ?
				port_id - CS_SCH_CPU_PORT0 : 0, tmax, rpt);
	}

	return status;
}

int cs752x_sch_set_queue_rate(unsigned char port_id, unsigned char q_id,
		unsigned int bandwidth)
{
	sched_queue_info_t *p_voq;
	int status;
	unsigned char rpt;
	unsigned int tsize, shap_id, tmax;
	SCH_SHAPER_CONFIGURATION_t clock_per_round;

	if ((port_id >= SCH_PORT_NO) || (q_id >= SCH_MAX_PORT_QUEUE))
		return -1;

	/* Calculate parameters from bandwidth */
	clock_per_round.wrd = readl(SCH_SHAPER_CONFIGURATION);
	status = sch_calc_rate_limiter_param((bandwidth / 1000),
			clock_per_round.bf.clocks_per_round_lt, 0xF, 0x3FFFF,
			&rpt, &tsize);
	if (status != 0)
		return status;

	if (tsize == 0)
		tmax = 1;
	else
		tmax = CS_SCH_TMAX_MAX;

	p_voq = &sched_queue_table.port[port_id].queue[q_id];

	/* parameters could be changed only if enabled */
	if (p_voq->enabled == 0)
		return -1;

	if (p_voq->is_drr == 0)	/* SP */
		shap_id = (port_id * 8) + q_id;
	else			/* DRR */
		shap_id = port_id + SCH_DRR_BASE_ID;

	status = sch_change_shaper_param(shap_id, tsize,
			port_id >= CS_SCH_CPU_PORT0 ?
			port_id - CS_SCH_CPU_PORT0 : 0, tmax, rpt);

	return status;
}

int cs752x_sch_disable_queue(unsigned char port_id, unsigned char q_id)
{
	sched_queue_info_t *p_voq;
	unsigned int ret_v;

	if ((port_id >= SCH_PORT_NO) || (q_id >= SCH_MAX_PORT_QUEUE))
		return -1;

	p_voq = &sched_queue_table.port[port_id].queue[q_id];

	/* parameters could be changed only if enabled */
	if (p_voq->enabled == 0)
		return 0;

	p_voq->enabled = 0;

	ret_v = sch_disable_voq(port_id * 8 + q_id,
			(port_id >= CS_SCH_CPU_PORT0) ?
			(port_id - CS_SCH_CPU_PORT0) : 0);
	return ret_v;
}

int cs752x_sch_set_port_rate_lt(unsigned char port_id, unsigned int lt_bw)
{
	int status;
	unsigned char rpt;
	unsigned int tsize, tmax, shap_id;
	SCH_SHAPER_CONFIGURATION_t clock_per_round;

	if (port_id >= SCH_PORT_NO)
		return -1;

	clock_per_round.wrd = readl(SCH_SHAPER_CONFIGURATION);

	/* LT. shaper parameters */
	status = sch_calc_rate_limiter_param((lt_bw / 1000),
			clock_per_round.bf.clocks_per_round_lt, 0xF, 0x3FFFF,
			&rpt, &tsize);
	if (status != 0)
		return status;

	if (tsize == 0)
		tmax = 1;
	else
		tmax = CS_SCH_TMAX_MAX;

	shap_id = port_id + SCH_LT_BASE_ID;
	//printk("SCH: LT: Shaper id:%d set to tsize:%d, tmax:%d, rpt:%d\n",
	//		shap_id,tsize,tmax,rpt);

	status = sch_change_shaper_param(shap_id, tsize, 0, tmax, rpt);

	return status;
}

int cs752x_sch_set_port_rate_st(unsigned char port_id, unsigned int st_bw)
{
	int status;
	unsigned char rpt;
	unsigned int tsize, tmax, shap_id;
	SCH_SHAPER_CONFIGURATION_t clock_per_round;

	if (port_id >= SCH_PORT_NO)
		return -1;

	clock_per_round.wrd = readl(SCH_SHAPER_CONFIGURATION);

	/* ST. shaper parameters */
	status = sch_calc_rate_limiter_param((st_bw / 1000),
			clock_per_round.bf.clocks_per_round_st, 0xF, 0x3FFFF,
			&rpt, &tsize);
	if (status != 0)
		return status;

	if (tsize == 0)
		tmax = 1;
	else
		tmax = tsize;

	shap_id = port_id + SCH_ST_BASE_ID;
	//printk("SCH: ST: Shaper id:%d set to tsize:%d, tmax:%d, rpt:%d\n",
	//		shap_id,tsize,tmax,rpt);
	status = sch_change_shaper_param(shap_id, tsize, 0, tmax, rpt);

	return status;
}

int cs752x_sch_set_port_burst(unsigned char port_id, unsigned char burst_size)
{
	if (port_id >= SCH_PORT_NO)
		return -1;

	/* burst size should be within 16 to 256 */
	if ((burst_size == 0) || ((burst_size >= 16) && (burst_size <= 256)))
		return cs752x_sch_set_port_burst_size(port_id,
				(burst_size == 256) ? 0 : burst_size);
	else
		return -1;
}

int cs752x_sch_reset_queue(unsigned char port_id, unsigned char q_id)
{
	/* Reset to DRR and bypass shaper */
	cs752x_sch_set_queue_drr(port_id, q_id, SCHE_DEFAULT_QUANTA, 0);

	return 0;
}

int cs752x_sch_reset_port(unsigned char port_id)
{
	int i;

	/* bypass port shaper with zero bandwidth */
	cs752x_sch_set_port_rate_lt(port_id, 0);
	if (port_id > CS_SCH_ETH_PORT2)
		cs752x_sch_set_port_rate_st(port_id, 0);

	/* Reset voq which belong to this port */
	for (i = SCH_MAX_PORT_QUEUE - 1; i >= 0; i--)
		cs752x_sch_reset_queue(port_id, i);
	return 0;
}

int cs752x_sch_atomic_command(sched_atomic_com_t * sch_command)
{
	int rc = -1;

	switch (sch_command->command) {
	case VOQ_AS_SP:
		rc = sch_enable_voq_sp(sch_command->arg0.voq_num,
				sch_command->arg1.tsize,
				sch_command->arg2.cpu_port_num,
				sch_command->arg3.tmax,
				sch_command->arg4.rpt);
		break;
	case VOQ_AS_DRR:
		rc = sch_enable_voq_drr(sch_command->arg0.voq_num,
					sch_command->arg1.quanta,
					sch_command->arg2.cpu_port_num);
		break;
	case DISABLE_VOQ:
		rc = sch_disable_voq(sch_command->arg0.voq_num,
				sch_command->arg2.cpu_port_num);
		break;
	case MOVE_VOQ_SP_DRR:
		rc = sch_voq_sp_to_drr(sch_command->arg0.voq_num,
				sch_command->arg1.quanta,
				sch_command->arg2.cpu_port_num);
		break;
	case MOVE_VOQ_DRR_SP:
		rc = sch_voq_drr_to_sp(sch_command->arg0.voq_num,
				sch_command->arg1.tsize,
				sch_command->arg2.cpu_port_num,
				sch_command->arg3.tmax, sch_command->arg4.rpt);
		break;
	case CHANGE_SHAPER_PARA:
		rc = sch_change_shaper_param(sch_command->arg0.shap_num,
				sch_command->arg1.tsize,
				sch_command->arg2.cpu_port_num,
				sch_command->arg3.tmax, sch_command->arg4.rpt);
		break;
	case CHANGE_DRR_PARA:
		rc = sch_change_drr_quanta(sch_command->arg0.voq_num,
				sch_command->arg1.quanta,
				sch_command->arg2.cpu_port_num);
		break;
	case SET_BURST_SIZE:
		rc = cs752x_sch_set_port_burst_size(sch_command->arg2.port_num,
				sch_command->arg0.burst_size);
		break;
	case RECOVER_PARITY_ERROR:
		rc = sch_recover_port_parity_err(sch_command->arg2.port_num);
		break;
	case CHAHGE_PORT_LEVEL_MASK:
		rc = sch_change_port_mask(sch_command->arg2.port_num,
				sch_command->arg0.mask);
		break;
	default:
		break;
	}

	return rc;
}

int cs752x_sch_init(void)
{
	int i, j, ret_v;
	sched_id_t cal_entries[SCH_MAX_CALENDAR_LENGTH];
	SCH_SHAPER_CONFIGURATION_t sch_shaper_cfg;
	SCH_DEBUG_CONTROL_t sch_dbg_ctrl;

	/* Reset Schedule */
	ret_v = sch_reset();
	if (ret_v != 0)
		return ret_v;

	memset(&sched_queue_table, 0, sizeof(sched_info_t));

	/*
	 * Initialize the main port calendar
	 * Total of 32 slots.  Clock speed is 200 MHz. 
	 * Each slot advances every 4 clocks.
	 * It takes a given slot will be accessed every (32 * 4 * 5) ns = 640 ns
	 * The slot size can be set from 16 bytes. to 256 bytes.
	 * BW for per slot will be ((bsize * 8) / (32 * 4)) * (200 Mhz) =
	 * (12.5 * bsize) mbps.
	 * The total BW for a slot will be ranged from 200 mbps to 3.2 Gbps
	 * In the best case scenario, total bandwidth is 102.4 Gbps shared among
	 * 32 slots.  However, it is not the real life case.  The real total
	 * bandwidth that we have is about 7.5 Gbps, and it has to be shared by
	 * 5 port, 1 root, and 1 CPU. Therefore, to be able to make all the port
	 * gets 5/7.5 BW, CPU gets 0.5/7.5,
	 * and root gets 2/7.5.  The following scheme is assigned to
	 * port/CPU/Root.
	 * Port gets 21
	 * ROOT gets 3
	 * CPU gets 8
	 *
	 * PS: DV script gives Port/Root/CPU = 20 / 4 / 8
	 * PS2: This might be modified based on application requirement, but as
	 * for now, we set a reference for RG design
	 */
	for (i = 0; i < SCH_MAX_CALENDAR_LENGTH; i++)
		cal_entries[i] = INVALD_SLOT;

	cal_entries[0] = PORT_SCH_ID;
	cal_entries[1] = PORT_SCH_ID;
	cal_entries[2] = PORT_SCH_ID;

	cal_entries[3] = CPU_SCH_ID;
	cal_entries[4] = CPU_SCH_ID;

	cal_entries[5] = PORT_SCH_ID;
	cal_entries[6] = PORT_SCH_ID;
	cal_entries[7] = PORT_SCH_ID;

	cal_entries[8] = CPU_SCH_ID;
	cal_entries[9] = CPU_SCH_ID;

	cal_entries[10] = PORT_SCH_ID;
	cal_entries[11] = PORT_SCH_ID;
	cal_entries[12] = PORT_SCH_ID;

	cal_entries[13] = ROOT_SCH_ID;

	cal_entries[14] = PORT_SCH_ID;
	cal_entries[15] = PORT_SCH_ID;
	cal_entries[16] = PORT_SCH_ID;

	cal_entries[17] = CPU_SCH_ID;
	cal_entries[18] = CPU_SCH_ID;

	cal_entries[19] = PORT_SCH_ID;
	cal_entries[20] = PORT_SCH_ID;
	cal_entries[21] = PORT_SCH_ID;

	cal_entries[22] = CPU_SCH_ID;
	cal_entries[23] = CPU_SCH_ID;

	cal_entries[24] = PORT_SCH_ID;
	cal_entries[25] = PORT_SCH_ID;
	cal_entries[26] = PORT_SCH_ID;

	cal_entries[27] = ROOT_SCH_ID;
	cal_entries[28] = ROOT_SCH_ID;

	cal_entries[29] = PORT_SCH_ID;
	cal_entries[30] = PORT_SCH_ID;
	cal_entries[31] = PORT_SCH_ID;

	if (cs752x_sch_set_calendar(cal_entries, 32) != 0)
		return -1;

	/* Change port mask, burst size, and rate.
	 * Also init all VoQ as DRR and bypass shaper */
	for (i = 0; i < SCH_PORT_NO; i++) {
		cs752x_sch_enable_port(i);

//		if ((i == CS_SCH_ETH_CRYPT) || (i == CS_SCH_ETH_ENCAP)) {
//			cs752x_sch_set_port_burst(i, 256);
//			printk("Setting SCH port:%d to 64 byte burst\n",i);
//			cs752x_sch_set_port_rate_lt(i, 1000000000); // 900mbps worked well
//			cs752x_sch_set_port_rate_st(i, 1000000000); // 900mbps worked well
//		} else {
//			printk("Setting SCH port:%d to 256 byte burst\n",i);
			cs752x_sch_set_port_burst(i, 0);
			cs752x_sch_set_port_rate_lt(i, 0);
			cs752x_sch_set_port_rate_st(i, 0);
//		}
		for (j = 0; j < SCH_MAX_PORT_QUEUE; j++)
			cs752x_sch_set_queue_drr(i, j, SCHE_DEFAULT_QUANTA, 0);
	}

	/* enable scrubing for shaper */
	sch_shaper_cfg.wrd = readl(SCH_SHAPER_CONFIGURATION);
	sch_shaper_cfg.bf.global_scrub_enable = 1;
	sch_shaper_cfg.bf.clocks_per_round_st = 0x100;
	writel(sch_shaper_cfg.wrd, SCH_SHAPER_CONFIGURATION);

	sch_dbg_ctrl.wrd = readl(SCH_DEBUG_CONTROL);
	sch_dbg_ctrl.bf.switch_expr_mode_lowlvl = 3;
	writel(sch_dbg_ctrl.wrd, SCH_DEBUG_CONTROL);

	return 0;
}
