/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_misc_util.c
 *
 * $Id: cs_fe_misc_util.c,v 1.2 2011/05/14 02:49:36 whsu Exp $
 *
 * It contains the assistance API various FE tables.
 */
#include "cs_fe.h"

int cs_fe_an_bng_mac_get_port_by_mac(unsigned char *mac_addr, __u8 *pspid)
{
	fe_an_bng_mac_entry_t abm_entry;
	int status, i;

	if ((mac_addr == NULL) || (pspid == NULL))
		return FE_TABLE_ENULLPTR;

	for (i = 0; i < FE_AN_BNG_MAC_ENTRY_MAX; i++) {
		status = cs_fe_table_get_entry(FE_TABLE_AN_BNG_MAC, i,
				&abm_entry);
		if (status == FE_TABLE_OK) {
			if (memcmp(abm_entry.mac, mac_addr, 6) == 0) {
				*pspid = abm_entry.pspid;
				return FE_TABLE_OK;
			}
		}
	}
	return FE_TABLE_ENTRYNOTFOUND;
} /* cs_fe_an_bng_mac_get_port_by_mac */

int cs_fe_lpb_get_lspid_by_pspid(__u8 pspid, __u8 *lspid)
{
	fe_lpb_entry_t lpb_entry;
	int status;

	status = cs_fe_table_get_entry(FE_TABLE_LPB, pspid, &lpb_entry);
	if (status != FE_TABLE_OK)
		return status;

	*lspid = lpb_entry.lspid;
	return status;
} /* cs_fe_lpb_get_lspid_by_pspid */

