/***********************************************************************/  
/* This file contains unpublished documentation and software           */ 
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */ 
/* in whole or in part, of the information in this file without a      */ 
/* written consent of an officer of Cortina Systems Incorporated is    */ 
/* strictly prohibited.                                                */ 
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */ 
/***********************************************************************/ 

#ifndef __CS_FE_TABLE_UT_H__
#define __CS_FE_TABLE_UT_H__

#ifndef NULL
#define NULL    0
#endif

#if defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE)
#define FE_NUM_TABLES  FE_TABLE_MAX
#define FE_NUM_TEST_PLAN_PER_TABLE    3
#define FE_NUM_TEST_PLAN    (FE_NUM_TABLES * FE_NUM_TEST_PLAN_PER_TABLE)

#endif	/* defined(CONFIG_CS75XX_FE_TBL_MGMT_UT) || \
	 * defined(CONFIG_CS75XX_FE_TBL_MGMT_UT_MODULE) */

#endif	/* __CS_FE_TABLE_UT_H__ */
