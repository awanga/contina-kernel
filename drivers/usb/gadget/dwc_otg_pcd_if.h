/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/dwc_otg_pcd_if.h $
 * $Revision: 1.1 $
 * $Date: 2011/03/08 09:08:38 $
 * $Change: 1146996 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */
#ifndef DWC_HOST_ONLY

#if !defined(__DWC_PCD_IF_H__)
#define __DWC_PCD_IF_H__

//#include <linux/usb/dwc_os.h>
#include "dwc_otg_pcd.h"
//#include "../otg/dwc_otg_driver.h"
//#include <linux/usb/dwc_otg.h>

/** @file 
 * This file defines DWC_OTG PCD Core API.
 */


struct dwc_otg_pcd;
typedef struct dwc_otg_pcd dwc_otg_pcd_t;

/** Maxpacket size for EP0 */
#define MAX_EP0_SIZE	64
/** Maxpacket size for any EP */
#define MAX_PACKET_SIZE 1024


/** @name Function Driver Callbacks */
/** @{ */

/** This function will be called whenever a previously queued request has
 * completed.  The status value will be set to -DWC_E_SHUTDOWN to indicated a
 * failed or aborted transfer, or -DWC_E_RESTART to indicate the device was reset,
 * or -DWC_E_TIMEOUT to indicate it timed out, or -DWC_E_INVALID to indicate invalid
 * parameters. */
typedef int (*dwc_completion_cb_t) (dwc_otg_pcd_t * pcd, void *ep_handle,
				    void *req_handle, int32_t status,
				    uint32_t actual);
/**
 * This function will be called whenever a previousle queued ISOC request has
 * completed. Count of ISOC packets could be read using dwc_otg_pcd_get_iso_packet_count
 * function.
 * The status of each ISOC packet could be read using dwc_otg_pcd_get_iso_packet_*
 * functions.
 */
typedef int (*dwc_isoc_completion_cb_t) (dwc_otg_pcd_t * pcd, void *ep_handle,
					 void *req_handle, int proc_buf_num);
/** This function should handle any SETUP request that cannot be handled by the
 * PCD Core.  This includes most GET_DESCRIPTORs, SET_CONFIGS, Any
 * class-specific requests, etc.  The function must non-blocking.
 *
 * Returns 0 on success.
 * Returns -DWC_E_NOT_SUPPORTED if the request is not supported.
 * Returns -DWC_E_INVALID if the setup request had invalid parameters or bytes.
 * Returns -DWC_E_SHUTDOWN on any other error. */
typedef int (*dwc_setup_cb_t) (dwc_otg_pcd_t * pcd, uint8_t * bytes);
/** This is called whenever the device has been disconnected.  The function
 * driver should take appropriate action to clean up all pending requests in the
 * PCD Core, remove all endpoints (except ep0), and initialize back to reset
 * state. */
typedef int (*dwc_disconnect_cb_t) (dwc_otg_pcd_t * pcd);
/** This function is called when device has been connected. */
typedef int (*dwc_connect_cb_t) (dwc_otg_pcd_t * pcd, int speed);
/** This function is called when device has been suspended */
typedef int (*dwc_suspend_cb_t) (dwc_otg_pcd_t * pcd);
/** This function is called when device has received LPM tokens, i.e.
 * device has been sent to sleep state. */
typedef int (*dwc_sleep_cb_t) (dwc_otg_pcd_t * pcd);
/** This function is called when device has been resumed
 * from suspend(L2) or L1 sleep state. */
typedef int (*dwc_resume_cb_t) (dwc_otg_pcd_t * pcd);
/** This function is called whenever hnp params has been changed.
 * User can call get_b_hnp_enable, get_a_hnp_support, get_a_alt_hnp_support functions
 * to get hnp parameters. */
typedef int (*dwc_hnp_params_changed_cb_t) (dwc_otg_pcd_t * pcd);
/** This function is called whenever USB RESET is detected. */
typedef int (*dwc_reset_cb_t) (dwc_otg_pcd_t * pcd);

/** Function Driver Ops Data Structure */
struct dwc_otg_pcd_function_ops {
	dwc_connect_cb_t connect;
	dwc_disconnect_cb_t disconnect;
	dwc_setup_cb_t setup;
	dwc_completion_cb_t complete;
	dwc_isoc_completion_cb_t isoc_complete;
	dwc_suspend_cb_t suspend;
	dwc_sleep_cb_t sleep;
	dwc_resume_cb_t resume;
	dwc_reset_cb_t reset;
	dwc_hnp_params_changed_cb_t hnp_changed;
};
/** @} */

/** Stop ISOC transfers on endpoint referenced by ep_handle.
 *
 * @param pcd The PCD
 * @param ep_handle The handle of the endpoint
 * @param req_handle Handle of ISOC request
 *
 * Returns -DWC_E_INVALID if incorrect arguments are passed to the function
 * Returns 0 on success
 */
int dwc_otg_pcd_iso_ep_stop(dwc_otg_pcd_t * pcd, void *ep_handle,
			    void *req_handle);

 
#endif				/* __DWC_PCD_IF_H__ */

#endif				/* DWC_HOST_ONLY */
