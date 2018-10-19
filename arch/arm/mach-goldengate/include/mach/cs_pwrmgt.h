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

#ifndef __CS_PWRMGT_H__
#define __CS_PWRMGT_H__

#include "cs_types.h"

/* Power Management (PM) states */
typedef enum {
	CS_PM_STATE_NORMAL 	= 0,
	CS_PM_STATE_SUSPEND 	= 1,
	CS_PM_STATE_POWER_DOWN  = 2 
} cs_pm_state_t;

/* CPU supporting frequencies */
typedef enum {
	CS_CPU_FREQUENCY_400	= 0,
	CS_CPU_FREQUENCY_600	= 1,
	CS_CPU_FREQUENCY_700	= 2,
	CS_CPU_FREQUENCY_750	= 3,
	CS_CPU_FREQUENCY_800	= 4,
	CS_CPU_FREQUENCY_850	= 5,
	CS_CPU_FREQUENCY_900	= 6
} cs_cpu_freq_t;


/*  CPU running configure information */
#define CS_CPU_NUM	2
typedef struct {
cs_cpu_freq_t freq; /* CPU frequency */
cs_boolean_t cpu_run[CS_CPU_NUM]; /* 1 ¡V power up, 0 ¡V power down */ 
} cs_pm_cpu_info_t; 

/* 
 * Configure USB working state setting of given USB ID. 
 * dev_id	CS device ID
 * usb_id	USB ID.
 * state	working state defined incs_pm_state_t.
 *  
 */
cs_status_t cs_pm_usb_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  usb_id,
CS_IN cs_pm_state_t  state);


/* middle */
/*
 * Retrieve USB running mode (run, suspend or power down) setting 
 * dev_id	CS device ID
 * usb_id	USB ID.
 * state	working state defined incs_pm_state_t.
 * 
 */
cs_status_t cs_pm_usb_state_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  usb_id,
CS_OUT cs_pm_state_t  *state);

/*
 * Configure PCIE running mode (run, suspend or power down) 
 * dev_id	CS device ID
 * pcie_id	PCIE ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_pcie_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pcie_id,
CS_IN cs_pm_state_t  state);

/*
 * Retrieve PCIE running mode (run, suspend or power down) setting 
 * dev_id	CS device ID
 * pcie_id	PCIE ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_pcie_state_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pcie_id,
CS_OUT cs_pm_state_t  *state);

/*
 * Configure SATA running mode (run, suspend or power down)  
 * dev_id	CS device ID
 * sata_id	SATA ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_sata_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  sata_id,
CS_IN cs_pm_state_t  state);

/*
 * Retrieve SATA running mode (run, suspend or power down) setting 
 * dev_id	CS device ID
 * sata_id	SATA ID.
 * state	working state defined incs_pm_state_t.
 *
 */
cs_status_t cs_pm_sata_state_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  sata_id,
CS_OUT cs_pm_state_t  *state);

/*
 * Configure PE running mode (run or suspend) 
 * dev_id	CS device ID
 * pe_id	PE ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_pe_suspend_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pe_id,
CS_IN cs_boolean_t  suspend);

/*
 * Retrieve PE running mode (run or suspend) setting 
 * ddev_id	CS device ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_pe_suspend_get (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  pe_id,
CS_OUT cs_boolean_t  *suspend);

/* Dev-team should do this */

/*
 * Configure NE running mode (run or suspend) 
 * ddev_id	CS device ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_ne_suspend_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN cs_boolean_t  suspend);

/*
 * Retrieve NE running mode (run or suspend) setting
 * dev_id	CS device ID
 * pe_id	PE ID
 * suspend	True ¡V suspend, False ¡V normal
 *
 */
cs_status_t cs_pm_ne_suspend_get (
CS_IN cs_dev_id_t  dev_id,
CS_OUT cs_boolean_t  *suspend);


/*
 * Configure CPU to power down 
 * dev_id	CS device ID
 * cpu_id	CPU Core ID
 *
 */
cs_status_t cs_pm_cpu_power_down (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  cpu_id);

/*
 * Configure CPU to power up 
 * dev_id	CS device ID
 * cpu_id	CPU Core ID
 *
 */
cs_status_t cs_pm_cpu_power_up (
CS_IN cs_dev_id_t  dev_id,
CS_IN  cs_uint32_t  cpu_id);

/*
 * Configure CPU running frequency 
 * dev_id	CS device ID
 * freq	CPU frequency, defined in cs_cpu_freq_t structure
 *
 */
cs_status_t cs_pm_cpu_frequency_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN cs_cpu_freq_t  freq);

/*
 * Retrieve CPU power and frequency configuraion 
 * dev_id	CS device ID
 * cpu_info	CPU information, such as frequency, each core power down or up, refer cs_pm_cpu_info_t
 *
 */
cs_status_t cs_pm_cpu_info_get (
CS_IN cs_dev_id_t  dev_id,
CS_OUT cs_pm_cpu_info_t 	*cpu_info);

/*
* Put system in standby.
* DDR will switch to self refresh mode eventually
* dev_id CS device ID
*/
cs_status_t cs_pm_system_suspend_enter (
CS_IN cs_dev_id_t   dev_id);

/*
 * Configure CPU power state 
 * dev_id	CS device ID
 * cpu_id	CPU Core ID
 * Cpu_state = CS_PM_STATE_SUSPEND  - enter WFI
 * Cpu_state = CS_PM_STATE_NORMAL  - enter active-online state     
 *
 */
cs_status_t cs_pm_cpu_power_state_set (
CS_IN cs_dev_id_t  dev_id,
CS_IN cs_uint32_t  cpu_id,
CS_IN cs_pm_state_t    cpu_state);

#endif /* __CS_PWRMGT_H__ */
