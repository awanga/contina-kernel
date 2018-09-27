
#include "dwc_otg_pcd_if.h"
/** @name Function Driver Functions */
/** @{ */

/** Call this function to get pointer on dwc_otg_pcd_t,
 * this pointer will be used for all PCD API functions.
 *
 * @param core_if The DWC_OTG Core
 */
 
extern dwc_otg_pcd_t *dwc_otg_pcd_init(dwc_otg_core_if_t * core_if);

/** Frees PCD allocated by dwc_otg_pcd_init
 *
 * @param pcd The PCD
 */
extern void dwc_otg_pcd_remove(dwc_otg_pcd_t * pcd);

/** Call this to bind the function driver to the PCD Core.
 *
 * @param pcd Pointer on dwc_otg_pcd_t returned by dwc_otg_pcd_init function.
 * @param fops The Function Driver Ops data structure containing pointers to all callbacks.
 */
extern void dwc_otg_pcd_start(dwc_otg_pcd_t * pcd,
			      const struct dwc_otg_pcd_function_ops *fops);

/** Enables an endpoint for use.  This function enables an endpoint in
 * the PCD.  The endpoint is described by the ep_desc which has the
 * same format as a USB ep descriptor.  The ep_handle parameter is used to refer
 * to the endpoint from other API functions and in callbacks.  Normally this
 * should be called after a SET_CONFIGURATION/SET_INTERFACE to configure the
 * core for that interface.
 *
 * Returns -DWC_E_INVALID if invalid parameters were passed.
 * Returns -DWC_E_SHUTDOWN if any other error ocurred.
 * Returns 0 on success.
 *
 * @param pcd The PCD
 * @param ep_desc Endpoint descriptor
 * @param ep_handle Handle on endpoint, that will be used to identify endpoint.
 */
extern int dwc_otg_pcd_ep_enable(dwc_otg_pcd_t * pcd,
				 const uint8_t * ep_desc, void *ep_handle);

/** Disable the endpoint referenced by ep_handle.
 *
 * Returns -DWC_E_INVALID if invalid parameters were passed.
 * Returns -DWC_E_SHUTDOWN if any other error ocurred.
 * Returns 0 on success. */
extern int dwc_otg_pcd_ep_disable(dwc_otg_pcd_t * pcd, void *ep_handle);

/** Queue a data transfer request on the endpoint referenced by ep_handle.
 * After the transfer is completes, the complete callback will be called with
 * the request status.
 *
 * @param pcd The PCD
 * @param ep_handle The handle of the endpoint
 * @param buf The buffer for the data
 * @param dma_buf The DMA buffer for the data
 * @param buflen The length of the data transfer
 * @param zero Specifies whether to send zero length last packet.
 * @param req_handle Set this handle to any value to use to reference this
 * request in the ep_dequeue function or from the complete callback
 * @param atomic_alloc If driver need to perform atomic allocations
 * for internal data structures.
 *
 * Returns -DWC_E_INVALID if invalid parameters were passed.
 * Returns -DWC_E_SHUTDOWN if any other error ocurred.
 * Returns 0 on success. */
extern int dwc_otg_pcd_ep_queue(dwc_otg_pcd_t * pcd, void *ep_handle,
				uint8_t * buf, dwc_dma_t dma_buf,
				uint32_t buflen, int zero, void *req_handle,
				int atomic_alloc);

/** De-queue the specified data transfer that has not yet completed.
 *
 * Returns -DWC_E_INVALID if invalid parameters were passed.
 * Returns -DWC_E_SHUTDOWN if any other error ocurred.
 * Returns 0 on success. */
extern int dwc_otg_pcd_ep_dequeue(dwc_otg_pcd_t * pcd, void *ep_handle,
				  void *req_handle);

/** Halt (STALL) an endpoint or clear it.
 *
 * Returns -DWC_E_INVALID if invalid parameters were passed.
 * Returns -DWC_E_SHUTDOWN if any other error ocurred.
 * Returns -DWC_E_AGAIN if the STALL cannot be sent and must be tried again later
 * Returns 0 on success. */
extern int dwc_otg_pcd_ep_halt(dwc_otg_pcd_t * pcd, void *ep_handle, int value);

/** This function should be called on every hardware interrupt */
extern int32_t dwc_otg_pcd_handle_intr(dwc_otg_pcd_t * pcd);

/** This function returns current frame number */
extern int dwc_otg_pcd_get_frame_number(dwc_otg_pcd_t * pcd);

/**
 * Start isochronous transfers on the endpoint referenced by ep_handle.
 * For isochronous transfers duble buffering is used.
 * After processing each of buffers comlete callback will be called with
 * status for each transaction.
 *
 * @param pcd The PCD
 * @param ep_handle The handle of the endpoint
 * @param buf0 The virtual address of first data buffer
 * @param buf1 The virtual address of second data buffer
 * @param dma0 The DMA address of first data buffer
 * @param dma1 The DMA address of second data buffer
 * @param sync_frame Data pattern frame number
 * @param dp_frame Data size for pattern frame
 * @param data_per_frame Data size for regular frame
 * @param start_frame Frame number to start transfers, if -1 then start transfers ASAP.
 * @param buf_proc_intrvl Interval of ISOC Buffer processing
 * @param req_handle Handle of ISOC request
 * @param atomic_alloc Specefies whether to perform atomic allocation for
 * 			internal data structures.
 *
 * Returns -DWC_E_NO_MEMORY if there is no enough memory.
 * Returns -DWC_E_INVALID if incorrect arguments are passed to the function.
 * Returns -DW_E_SHUTDOWN for any other error.
 * Returns 0 on success
 */
extern int dwc_otg_pcd_iso_ep_start(dwc_otg_pcd_t * pcd, void *ep_handle,
				    uint8_t * buf0, uint8_t * buf1,
				    dwc_dma_t dma0, dwc_dma_t dma1,
				    int sync_frame, int dp_frame,
				    int data_per_frame, int start_frame,
				    int buf_proc_intrvl, void *req_handle,
				    int atomic_alloc);

/** Get ISOC packet status.
 *
 * @param pcd The PCD
 * @param ep_handle The handle of the endpoint
 * @param iso_req_handle Isochronoush request handle
 * @param packet Number of packet
 * @param status Out parameter for returning status
 * @param actual Out parameter for returning actual length
 * @param offset Out parameter for returning offset
 *
 */
 
extern void dwc_otg_pcd_get_iso_packet_params(dwc_otg_pcd_t * pcd,
					      void *ep_handle,
					      void *iso_req_handle, int packet,
					      int *status, int *actual,
					      int *offset);

/** Get ISOC packet count.
 *
 * @param pcd The PCD
 * @param ep_handle The handle of the endpoint
 * @param iso_req_handle
 */
extern int dwc_otg_pcd_get_iso_packet_count(dwc_otg_pcd_t * pcd,
					    void *ep_handle,
					    void *iso_req_handle);

/** This function starts the SRP Protocol if no session is in progress. If
 * a session is already in progress, but the device is suspended,
 * remote wakeup signaling is started.
 */
extern int dwc_otg_pcd_wakeup(dwc_otg_pcd_t * pcd);

/** This function returns 1 if LPM support is enabled, and 0 otherwise. */
extern int dwc_otg_pcd_is_lpm_enabled(dwc_otg_pcd_t * pcd);

/** This function returns 1 if remote wakeup is allowed and 0, otherwise. */
extern int dwc_otg_pcd_get_rmwkup_enable(dwc_otg_pcd_t * pcd);

/** Initiate SRP */
extern void dwc_otg_pcd_initiate_srp(dwc_otg_pcd_t * pcd);

/** Starts remote wakeup signaling. */
extern void dwc_otg_pcd_remote_wakeup(dwc_otg_pcd_t * pcd, int set);

/** This function returns whether device is dualspeed.*/
extern uint32_t dwc_otg_pcd_is_dualspeed(dwc_otg_pcd_t * pcd);

/** This function returns whether device is otg. */
extern uint32_t dwc_otg_pcd_is_otg(dwc_otg_pcd_t * pcd);

/** This functions allow to get hnp parameters */
//extern uint32_t get_b_hnp_enable(dwc_otg_pcd_t * pcd);
extern uint32_t get_a_hnp_support(dwc_otg_pcd_t * pcd);
extern uint32_t get_a_alt_hnp_support(dwc_otg_pcd_t * pcd);

/** @} */