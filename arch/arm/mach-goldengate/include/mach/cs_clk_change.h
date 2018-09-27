/*
 * Copyright (c) Cortina-Systems Limited 2014.  All rights reserved.
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

#ifndef __CS_CLK_CHANGE_H__
#define __CS_CLK_CHANGE_H__

#include "cs_types.h"

/* Peripheral frequencies in use */
typedef enum {
	CS_PERIPHERAL_FREQUENCY_100,
	CS_PERIPHERAL_FREQUENCY_150,
	CS_PERIPHERAL_FREQUENCY_170,
} cs_peripheral_freq_t;

/* AXI bus frequencies in use */
typedef enum {
	CS_AXI_FREQUENCY_133,
	CS_AXI_FREQUENCY_140,
	CS_AXI_FREQUENCY_142,
	CS_AXI_FREQUENCY_150,
	CS_AXI_FREQUENCY_160,
} cs_axi_freq_t;

/* Notification events sent by the framework  */
typedef enum {
	CS_PM_FREQ_PRECHANGE,
	CS_PM_FREQ_POSTCHANGE,
	CS_PM_FREQ_RESUMECHANGE,
} cs_pm_freq_notifier_event_t;

/* Callback notification data structure */
typedef struct {
	cs_pm_freq_notifier_event_t event;
	void *data;
	cs_peripheral_freq_t old_peripheral_clk;
	cs_peripheral_freq_t new_peripheral_clk;
	cs_axi_freq_t old_axi_clk;
	cs_axi_freq_t new_axi_clk;
} cs_pm_freq_notifier_data_t;

/* Notification callback function */
typedef cs_status_t (cs_pm_freq_notifier_callback_t)(cs_pm_freq_notifier_data_t *);

/* Callback registration data structure */
typedef struct {
	cs_pm_freq_notifier_callback_t *notifier;
	void *data;
} cs_pm_freq_notifier_t;

/*
 * Register Clock Change Notification Callback
 * n		Structure containing a callback function and callback data
 * priority	Callback priority
 */
cs_status_t cs_pm_freq_register_notifier(CS_IN cs_pm_freq_notifier_t *n,
					 CS_IN cs_uint32_t priority);

/*
 * Unregister Clock Change Notification Callback
 * n		Structure containing a callback function and callback data
 */
cs_status_t cs_pm_freq_unregister_notifier(CS_IN cs_pm_freq_notifier_t *n);

#endif /* __CS_CLK_CHANGE_H__ */
