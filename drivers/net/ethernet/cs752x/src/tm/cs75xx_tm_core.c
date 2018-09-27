/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#include <linux/types.h>
#include <mach/cs_types.h>
#include "cs75xx_tm.h"

int cs75xx_tm_init(void)
{
	int status;

	status = cs_tm_pol_init();
	if (status != 0)
		return status;

	status = cs_tm_bm_init();
	if (status != 0)
		return status;

	status = cs_tm_pm_init();
	if (status != 0)
		return status;

	status = cs_tm_tc_init();
	if (status != 0)
		return status;

	return 0;
}
