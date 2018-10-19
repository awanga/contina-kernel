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
#ifndef __CS_NE_IRQ_H__
#define __CS_NE_IRQ_H__

#include <mach/cs_types.h>

#ifndef CONFIG_INTR_COALESCING
#define CONFIG_INTR_COALESCING		1
#endif

/* Global NE Interrupt Status And Enable Masks */

#ifndef CONFIG_INTR_COALESCING
#define GLOBAL_NE_INT_NI_XRAM_RX_STATUS	(1 << 0)
#else
#define GLOBAL_NE_INT_NI_XRAM_RX_STATUS (0)
#endif

#define GLOBAL_NE_INT_NI_XRAM_TX_STATUS	(1 << 1)
#define GLOBAL_NE_INT_NI_STATUS		(1 << 2)
#define GLOBAL_NE_INT_FE_STATUS		(1 << 3)
#define GLOBAL_NE_INT_TM_STATUS		(1 << 4)
#define GLOBAL_NE_INT_QM_STATUS		(1 << 5)
#define GLOBAL_NE_INT_SCH_STATUS	(1 << 6)

#ifndef CONFIG_INTR_COALESCING
#define GLOBAL_NE_INT_NI_XRAM_RX_ENABLE (1 << 0)
#else
#define GLOBAL_NE_INT_NI_XRAM_RX_ENABLE (0)
#endif

#define GLOBAL_NE_INT_NI_XRAM_TX_ENABLE (1 << 1)
#define GLOBAL_NE_INT_NI_ENABLE		(1 << 2)
#define GLOBAL_NE_INT_FE_ENABLE		(1 << 3)
#define GLOBAL_NE_INT_TM_ENABLE		(1 << 4)
#define GLOBAL_NE_INT_QM_ENABLE 	(1 << 5)
#define GLOBAL_NE_INT_SCH_ENABLE	(1 << 6)

/* FE Interrupt Status And Enable Masks */
#define FETOP_FWD_INT_STATUS		(1 << 0)
#define FETOP_LPM_INT_STATUS		(1 << 1)
#define FETOP_HASH_INT_STATUS		(1 << 2)
#define FETOP_PE_INT_STATUS		(1 << 3)
#define FETOP_DBG_INT_STATUS		(1 << 31)

#define FETOP_FWD_INT_ENABLE		(1 << 0)
#define FETOP_LPM_INT_ENABLE		(1 << 1)
#define FETOP_HASH_INT_ENABLE		(1 << 2)
#define FETOP_PE_INT_ENABLE		(1 << 3)
#define FETOP_DBG_INT_ENABLE		(1 << 31)

/* FE FWD Interrupt Status And Enable Masks */
#define FEFWD_DROP_INT_STATUS			(1 << 0)
#define FEFWD_CLASS0_PORT_CHK_FAIL_STATUS	(1 << 1)
#define FEFWD_CLASS1_PORT_CHK_FAIL_STATUS	(1 << 2)
#define FEFWD_CLASS2_PORT_CHK_FAIL_STATUS	(1 << 3)
#define FEFWD_CLASS3_PORT_CHK_FAIL_STATUS	(1 << 4)
#define FEFWD_ACLRULE0_PORT_CHK_FAIL_STATUS	(1 << 5)
#define FEFWD_ACLRULE1_PORT_CHK_FAIL_STATUS	(1 << 6)
#define FEFWD_ACLRULE2_PORT_CHK_FAIL_STATUS	(1 << 7)
#define FEFWD_ACLRULE3_PORT_CHK_FAIL_STATUS	(1 << 8)
#define FEFWD_ACLRULE4_PORT_CHK_FAIL_STATUS	(1 << 9)
#define FEFWD_ACLRULE5_PORT_CHK_FAIL_STATUS	(1 << 10)
#define FEFWD_ACLACT0_PORT_CHK_FAIL_STATUS	(1 << 11)
#define FEFWD_ACLACT1_PORT_CHK_FAIL_STATUS	(1 << 12)
#define FEFWD_ACLACT2_PORT_CHK_FAIL_STATUS	(1 << 13)
#define FEFWD_ACLACT3_PORT_CHK_FAIL_STATUS	(1 << 14)
#define FEFWD_ACLACT4_PORT_CHK_FAIL_STATUS	(1 << 15)
#define FEFWD_ACLACT5_PORT_CHK_FAIL_STATUS	(1 << 16)
#define FEFWD_FWDRSLT_PORT_CHK_FAIL_STATUS	(1 << 17)
#define FEFWD_QOSRSLT_PORT_CHK_FAIL_STATUS	(1 << 18)
#define FEFWD_L2TABLE_PORT_CHK_FAIL_STATUS	(1 << 19)
#define FEFWD_L3TABLE_PORT_CHK_FAIL_STATUS	(1 << 20)
#define FEFWD_VOQPOL_PORT_CHK_FAIL_STATUS	(1 << 21)
#define FEFWD_FLOWVLAN_PORT_CHK_FAIL_STATUS	(1 << 22)
#define FEFWD_VLANTBL_PORT_CHK_FAIL_STATUS	(1 << 23)

#define FEFWD_DROP_INT_ENABLE			(1 << 0)
#define FEFWD_CLASS0_PORT_CHK_FAIL_ENABLE	(1 << 1)
#define FEFWD_CLASS1_PORT_CHK_FAIL_ENABLE	(1 << 2)
#define FEFWD_CLASS2_PORT_CHK_FAIL_ENABLE	(1 << 3)
#define FEFWD_CLASS3_PORT_CHK_FAIL_ENABLE	(1 << 4)
#define FEFWD_ACLRULE0_PORT_CHK_FAIL_ENABLE	(1 << 5)
#define FEFWD_ACLRULE1_PORT_CHK_FAIL_ENABLE	(1 << 6)
#define FEFWD_ACLRULE2_PORT_CHK_FAIL_ENABLE	(1 << 7)
#define FEFWD_ACLRULE3_PORT_CHK_FAIL_ENABLE	(1 << 8)
#define FEFWD_ACLRULE4_PORT_CHK_FAIL_ENABLE	(1 << 9)
#define FEFWD_ACLRULE5_PORT_CHK_FAIL_ENABLE	(1 << 10)
#define FEFWD_ACLACT0_PORT_CHK_FAIL_ENABLE	(1 << 11)
#define FEFWD_ACLACT1_PORT_CHK_FAIL_ENABLE	(1 << 12)
#define FEFWD_ACLACT2_PORT_CHK_FAIL_ENABLE	(1 << 13)
#define FEFWD_ACLACT3_PORT_CHK_FAIL_ENABLE	(1 << 14)
#define FEFWD_ACLACT4_PORT_CHK_FAIL_ENABLE	(1 << 15)
#define FEFWD_ACLACT5_PORT_CHK_FAIL_ENABLE	(1 << 16)
#define FEFWD_FWDRSLT_PORT_CHK_FAIL_ENABLE	(1 << 17)
#define FEFWD_QOSRSLT_PORT_CHK_FAIL_ENABLE	(1 << 18)
#define FEFWD_L2TABLE_PORT_CHK_FAIL_ENABLE	(1 << 19)
#define FEFWD_L3TABLE_PORT_CHK_FAIL_ENABLE	(1 << 20)
#define FEFWD_VOQPOL_PORT_CHK_FAIL_ENABLE	(1 << 21)
#define FEFWD_FLOWVLAN_PORT_CHK_FAIL_ENABLE	(1 << 22)
#define FEFWD_VLANTBL_PORT_CHK_FAIL_ENABLE	(1 << 23)

/* FE PE Interrupt Status And Enable Masks */
#define FEPE_DATA_FIFO_OVERFLOW_INT_STATUS	(1 << 0)
#define FEPE_CMD_FIFO_OVERFLOW_INT_STATUS	(1 << 1)
#define FEPE_FE_PKT_CNT_MSB_SETL_STATUS		(1 << 2)
#define FEPE_FE_PKT_DROP_MSB_SETL_STATUS	(1 << 3)
#define FEPE_HDRA_PKT_DROP_MSB_SETL_STATUS	(1 << 4)
#define FEPE_RUNT_PKT_DETL_STATUS		(1 << 5)
#define FEPE_IPV6_UDPCSUM_0_DETL_STATUS		(1 << 6)

#define FEPE_DATA_FIFO_OVERFLOW_INT_ENABLE	(1 << 0)
#define FEPE_CMD_FIFO_OVERFLOW_INT_ENABLE	(1 << 1)
#define FEPE_FE_PKT_CNT_MSB_SETL_ENABLE		(1 << 2)
#define FEPE_FE_PKT_DROP_MSB_SETL_ENABLE	(1 << 3)
#define FEPE_HDRA_PKT_DROP_MSB_SETL_ENABLE	(1 << 4)
#define FEPE_RUNT_PKT_DETL_ENABLE		(1 << 5)
#define FEPE_IPV6_UDPCSUM_0_DETL_ENABLE		(1 << 6)

/* FE LPM Interrupt Status And Enable Masks */
#define FELPM_TABLE0_UPPER_PAR_ERR_INT_STATUS		(1 << 0)
#define FELPM_TABLE0_LOWER_PAR_ERR_INT_STATUS		(1 << 1)
#define FELPM_TABLE1_UPPER_PAR_ERR_INT_STATUS		(1 << 2)
#define FELPM_TABLE1_LOWER_PAR_ERR_INT_STATUS		(1 << 3)
#define FELPM_HC_TABLE0_UPPER_PAR_ERR_INT_STATUS	(1 << 4)
#define FELPM_HC_TABLE0_LOWER_PAR_ERR_INT_STATUS	(1 << 5)
#define FELPM_HC_TABLE1_UPPER_PAR_ERR_INT_STATUS	(1 << 6)
#define FELPM_HC_TABLE1_LOWER_PAR_ERR_INT_STATUS	(1 << 7)
#define FELPM_HC0_HALF_ROLL_INT_STATUS			(1 << 8)
#define FELPM_HC0_FULL_ROLL_INT_STATUS			(1 << 9)
#define FELPM_HC1_HALF_ROLL_INT_STATUS			(1 << 10)
#define FELPM_HC1_FULL_ROLL_INT_STATUS			(1 << 11)

#define FELPM_TABLE0_UPPER_PAR_ERR_INT_ENABLE		(1 << 0)
#define FELPM_TABLE0_LOWER_PAR_ERR_INT_ENABLE		(1 << 1)
#define FELPM_TABLE1_UPPER_PAR_ERR_INT_ENABLE		(1 << 2)
#define FELPM_TABLE1_LOWER_PAR_ERR_INT_ENABLE		(1 << 3)
#define FELPM_HC_TABLE0_UPPER_PAR_ERR_INT_ENABLE	(1 << 4)
#define FELPM_HC_TABLE0_LOWER_PAR_ERR_INT_ENABLE	(1 << 5)
#define FELPM_HC_TABLE1_UPPER_PAR_ERR_INT_ENABLE	(1 << 6)
#define FELPM_HC_TABLE1_LOWER_PAR_ERR_INT_ENABLE	(1 << 7)
#define FELPM_HC0_HALF_ROLL_INT_ENABLE			(1 << 8)
#define FELPM_HC0_FULL_ROLL_INT_ENABLE			(1 << 9)
#define FELPM_HC1_HALF_ROLL_INT_ENABLE			(1 << 10)
#define FELPM_HC1_FULL_ROLL_INT_ENABLE			(1 << 11)

/* FE HASH Interrupt Status And Enable Masks */
#define FEHASH_HASHMEM_CORR_ECC_ERR_STATUS	(1 << 0)
#define FEHASH_HASHMEM_UNCORR_ECC_ERR_STATUS	(1 << 1)
#define FEHASH_MASKMEM_CORR_ECC_ERR_STATUS	(1 << 2)
#define FEHASH_MASKMEM_UNCORR_ECC_ERR_STATUS	(1 << 3)
#define FEHASH_STATUSMEM_CORR_ECC_ERR_STATUS	(1 << 4)
#define FEHASH_STATUSMEM_UNCORR_ECC_ERR_STATUS	(1 << 5)
#define FEHASH_HASHMEM_MULT_HIT_STATUS		(1 << 6)
#define FEHASH_HASH_OVERFLOW_MULT_HIT_STATUS	(1 << 7)
#define FEHASH_CHECKMEM_PARITY_FAIL_STATUS	(1 << 8)

#define FEHASH_HASHMEM_CORR_ECC_ERR_ENABLE	(1 << 0)
#define FEHASH_HASHMEM_UNCORR_ECC_ERR_ENABLE	(1 << 1)
#define FEHASH_MASKMEM_CORR_ECC_ERR_ENABLE	(1 << 2)
#define FEHASH_MASKMEM_UNCORR_ECC_ERR_ENABLE	(1 << 3)
#define FEHASH_STATUSMEM_CORR_ECC_ERR_ENABLE	(1 << 4)
#define FEHASH_STATUSMEM_UNCORR_ECC_ERR_ENABLE	(1 << 5)
#define FEHASH_HASHMEM_MULT_HIT_ENABLE		(1 << 6)
#define FEHASH_HASH_OVERFLOW_MULT_HIT_ENABLE	(1 << 7)
#define FEHASH_CHECKMEM_PARITY_FAIL_ENABLE	(1 << 8)

/* FE DBG Interrupt Status And Masks */
#define FEDBG_HDR_D_BUF_OVF_INT_STATUS		(1 << 0)
#define FEDBG_CLASS_RSLT_BUF_INT_OVF_INT_STATUS	(1 << 1)
#define FEDBG_CLASS_RSLT_BUF_OVF_INT_STATUS	(1 << 2)
#define FEDBG_HLKP_RSLT_BUF_INT_OVF_INT_STATUS	(1 << 3)
#define FEDBG_HLKP_RSLT_BUF_OVF_INT_STATUS	(1 << 4)
#define FEDBG_LPM_RSLT_BUF_INT_OVF_INT_STATUS	(1 << 5)
#define FEDBG_LPM_RSLT_BUF_OVF_INT_STATUS	(1 << 6)
#define FEDBG_FWD_RSLT_BUF_INT_OVF_INT_STATUS	(1 << 7)
#define FEDBG_FWD_RSLT_BUF_OVF_INT_STATUS	(1 << 8)
#define FEDBG_QOS_RSLT_BUF_INT_OVF_INT_STATUS	(1 << 9)
#define FEDBG_QOS_RSLT_BUF_OVF_INT_STATUS	(1 << 10)
#define FEDBG_INBUF_OVF_INT_STATUS		(1 << 11)
#define FEDBG_TM_BUF_OVF_INT_STATUS		(1 << 12)
#define FEDBG_CLASS_HIT_INT_STATUS		(1 << 31)

#define FEDBG_HDR_D_BUF_OVF_INT_ENABLE		(1 << 0)
#define FEDBG_CLASS_RSLT_BUF_INT_OVF_INT_ENABLE	(1 << 1)
#define FEDBG_CLASS_RSLT_BUF_OVF_INT_ENABLE	(1 << 2)
#define FEDBG_HLKP_RSLT_BUF_INT_OVF_INT_ENABLE	(1 << 3)
#define FEDBG_HLKP_RSLT_BUF_OVF_INT_ENABLE	(1 << 4)
#define FEDBG_LPM_RSLT_BUF_INT_OVF_INT_ENABLE	(1 << 5)
#define FEDBG_LPM_RSLT_BUF_OVF_INT_ENABLE	(1 << 6)
#define FEDBG_FWD_RSLT_BUF_INT_OVF_INT_ENABLE	(1 << 7)
#define FEDBG_FWD_RSLT_BUF_OVF_INT_ENABLE	(1 << 8)
#define FEDBG_QOS_RSLT_BUF_INT_OVF_INT_ENABLE	(1 << 9)
#define FEDBG_QOS_RSLT_BUF_OVF_INT_ENABLE	(1 << 10)
#define FEDBG_INBUF_OVF_INT_ENABLE		(1 << 11)
#define FEDBG_TM_BUF_OVF_INT_ENABLE		(1 << 12)
#define FEDBG_CLASS_HIT_INT_ENABLE		(1 << 31)

/* TM (TOP) TC Interrupt and Masks */
#define TM_TC_BM_STATUS				(1 << 0)
#define TM_TC_POL_STATUS			(1 << 1)
#define TM_TC_PM_STATUS				(1 << 2)

#define TM_TC_BM_ENABLE				(1 << 0)
#define TM_TC_POL_ENABLE			(1 << 1)
#define TM_TC_PM_ENABLE				(1 << 2)

/* TM BM Interrupt And Masks */
#define TM_BM_ERR_VOQ_MEM_STATUS			(1 << 0)
#define TM_BM_ERR_CORRECT_VOQ_MEM_STATUS		(1 << 1)
#define TM_BM_ERR_VOQ_PROFILE_MEM_STATUS		(1 << 2)
#define TM_BM_ERR_CORRECT_VOQ_PROFILE_MEM_STATUS	(1 << 3)
#define TM_BM_ERR_VOQ_STATUS_MEM_STATUS			(1 << 4)
#define TM_BM_ERR_CORRECT_VOQ_STATUS_MEM_STATUS		(1 << 5)
#define TM_BM_ERR_DEST_PORT_MEM_STATUS			(1 << 6)
#define TM_BM_ERR_CORRECT_DEST_PORT_MEM_STATUS		(1 << 7)
#define TM_BM_ERR_DEST_PORT_STATUS_MEM_STATUS		(1 << 8)
#define TM_BM_ERR_CORRECT_DEST_PORT_STATUS_MEM_STATUS	(1 << 9)
#define TM_BM_ERR_WRED_PROFILE_MEM_STATUS		(1 << 10)
#define TM_BM_ERR_CORRECT_WRED_PROFILE_MEM_STATUS	(1 << 11)
#define TM_BM_ERR_GLOBAL_BUFFERS_USED_STATUS		(1 << 12)
#define TM_BM_ERR_GLOBAL_CPU_BUFFERS_USED_STATUS	(1 << 13)
#define TM_BM_ERR_GLOBAL_LINUX0_BUFFERS_USED_STATUS	(1 << 14)
#define TM_BM_ERR_GLOBAL_LINUX1_BUFFERS_USED_STATUS	(1 << 15)
#define TM_BM_ERR_GLOBAL_LINUX2_BUFFERS_USED_STATUS	(1 << 16)
#define TM_BM_ERR_GLOBAL_LINUX3_BUFFERS_USED_STATUS	(1 << 17)
#define TM_BM_ERR_GLOBAL_LINUX4_BUFFERS_USED_STATUS	(1 << 18)
#define TM_BM_ERR_GLOBAL_LINUX5_BUFFERS_USED_STATUS	(1 << 19)
#define TM_BM_ERR_GLOBAL_LINUX6_BUFFERS_USED_STATUS	(1 << 20)
#define TM_BM_ERR_GLOBAL_LINUX7_BUFFERS_USED_STATUS	(1 << 21)

#define TM_BM_ERR_VOQ_MEM_ENABLE			(1 << 0)
#define TM_BM_ERR_CORRECT_VOQ_MEM_ENABLE		(1 << 1)
#define TM_BM_ERR_VOQ_PROFILE_MEM_ENABLE		(1 << 2)
#define TM_BM_ERR_CORRECT_VOQ_PROFILE_MEM_ENABLE	(1 << 3)
#define TM_BM_ERR_VOQ_STATUS_MEM_ENABLE			(1 << 4)
#define TM_BM_ERR_CORRECT_VOQ_STATUS_MEM_ENABLE		(1 << 5)
#define TM_BM_ERR_DEST_PORT_MEM_ENABLE			(1 << 6)
#define TM_BM_ERR_CORRECT_DEST_PORT_MEM_ENABLE		(1 << 7)
#define TM_BM_ERR_DEST_PORT_STATUS_MEM_ENABLE		(1 << 8)
#define TM_BM_ERR_CORRECT_DEST_PORT_STATUS_MEM_ENABLE	(1 << 9)
#define TM_BM_ERR_WRED_PROFILE_MEM_ENABLE		(1 << 10)
#define TM_BM_ERR_CORRECT_WRED_PROFILE_MEM_ENABLE	(1 << 11)
#define TM_BM_ERR_GLOBAL_BUFFERS_USED_ENABLE		(1 << 12)
#define TM_BM_ERR_GLOBAL_CPU_BUFFERS_USED_ENABLE	(1 << 13)
#define TM_BM_ERR_GLOBAL_LINUX0_BUFFERS_USED_ENABLE	(1 << 14)
#define TM_BM_ERR_GLOBAL_LINUX1_BUFFERS_USED_ENABLE	(1 << 15)
#define TM_BM_ERR_GLOBAL_LINUX2_BUFFERS_USED_ENABLE	(1 << 16)
#define TM_BM_ERR_GLOBAL_LINUX3_BUFFERS_USED_ENABLE	(1 << 17)
#define TM_BM_ERR_GLOBAL_LINUX4_BUFFERS_USED_ENABLE	(1 << 18)
#define TM_BM_ERR_GLOBAL_LINUX5_BUFFERS_USED_ENABLE	(1 << 19)
#define TM_BM_ERR_GLOBAL_LINUX6_BUFFERS_USED_ENABLE	(1 << 20)
#define TM_BM_ERR_GLOBAL_LINUX7_BUFFERS_USED_ENABLE	(1 << 21)

/* TM POL Interrupts And Masks */
#define TM_POL_ERR_FLOW_PROFILE_MEM_STATUS		(1 << 2)
#define TM_POL_ERR_CORRECT_FLOW_PROFILE_MEM_STATUS	(1 << 3)
#define TM_POL_ERR_FLOW_STATUS_MEM_STATUS		(1 << 4)
#define TM_POL_ERR_CORRECT_FLOW_STATUS_MEM_STATUS	(1 << 5)
#define TM_POL_ERR_SPID_PROFILE_MEM_STATUS		(1 << 6)
#define TM_POL_ERR_CORRECT_SPID_PROFILE_MEM_STATUS	(1 << 7)
#define TM_POL_ERR_SPID_STATUS_MEM_STATUS		(1 << 8)
#define TM_POL_ERR_CORRECT_SPID_STATUS_MEM_STATUS	(1 << 9)
#define TM_POL_ERR_CPU_PROFILE_MEM_STATUS		(1 << 10)
#define TM_POL_ERR_CORRECT_CPU_PROFILE_MEM_STATUS	(1 << 11)
#define TM_POL_ERR_CPU_STATUS_MEM_STATUS		(1 << 12)
#define TM_POL_ERR_CORRECT_CPU_STATUS_MEM_STATUS	(1 << 13)
#define TM_POL_ERR_PKT_TYPE_PROFILE_MEM_STATUS		(1 << 14)
#define TM_POL_ERR_CORRECT_PKT_TYPE_PROFILE_MEM_STATUS	(1 << 15)
#define TM_POL_ERR_PKT_TYPE_STATUS_MEM_STATUS		(1 << 15)
#define TM_POL_ERR_CORRECT_PKT_TYPE_STATUS_MEM_STATUS	(1 << 15)

#define TM_POL_ERR_FLOW_PROFILE_MEM_ENABLE		(1 << 2)
#define TM_POL_ERR_CORRECT_FLOW_PROFILE_MEM_ENABLE	(1 << 3)
#define TM_POL_ERR_FLOW_STATUS_MEM_ENABLE		(1 << 4)
#define TM_POL_ERR_CORRECT_FLOW_STATUS_MEM_ENABLE	(1 << 5)
#define TM_POL_ERR_SPID_PROFILE_MEM_ENABLE		(1 << 6)
#define TM_POL_ERR_CORRECT_SPID_PROFILE_MEM_ENABLE	(1 << 7)
#define TM_POL_ERR_SPID_STATUS_MEM_ENABLE		(1 << 8)
#define TM_POL_ERR_CORRECT_SPID_STATUS_MEM_ENABLE	(1 << 9)
#define TM_POL_ERR_CPU_PROFILE_MEM_ENABLE		(1 << 10)
#define TM_POL_ERR_CORRECT_CPU_PROFILE_MEM_ENABLE	(1 << 11)
#define TM_POL_ERR_CPU_STATUS_MEM_ENABLE		(1 << 12)
#define TM_POL_ERR_CORRECT_CPU_STATUS_MEM_ENABLE	(1 << 13)
#define TM_POL_ERR_PKT_TYPE_PROFILE_MEM_ENABLE		(1 << 14)
#define TM_POL_ERR_CORRECT_PKT_TYPE_PROFILE_MEM_ENABLE	(1 << 15)
#define TM_POL_ERR_PKT_TYPE_STATUS_MEM_ENABLE		(1 << 15)
#define TM_POL_ERR_CORRECT_PKT_TYPE_STATUS_MEM_ENABLE	(1 << 15)

/* TM PM Interrupt And Masks */
#define TM_PM_FLOW_CNTR_MSB0_STATUS			(1 << 0)
#define TM_PM_FLOW_CNTR_MSB1_STATUS			(1 << 1)
#define TM_PM_FLOW_CNTR_MSB2_STATUS			(1 << 2)
#define TM_PM_FLOW_CNTR_MSB3_STATUS			(1 << 3)
#define TM_PM_FLOW_CNTR_MSB4_STATUS			(1 << 4)
#define TM_PM_FLOW_CNTR_MSB5_STATUS			(1 << 5)
#define TM_PM_FLOW_CNTR_MSB6_STATUS			(1 << 6)
#define TM_PM_FLOW_CNTR_MSB7_STATUS			(1 << 7)
#define TM_PM_SPID_CNTR_MSB0_STATUS			(1 << 8)
#define TM_PM_VOQ_CNTR_MSB0_STATUS			(1 << 9)
#define TM_PM_VOQ_CNTR_MSB1_STATUS			(1 << 10)
#define TM_PM_CPU_CNTR_MSB0_STATUS			(1 << 11)
#define TM_PM_PKT_TYPE_CNTR_MSB0_STATUS			(1 << 12)
#define TM_PM_PKT_TYPE_CNTR_MSB1_STATUS			(1 << 13)
#define TM_PM_CPU_COPY_CNTR_MSB0_STATUS			(1 << 14)
#define TM_PM_CPU_COPY_CNTR_MSB1_STATUS			(1 << 15)
#define TM_PM_CPU_COPY_CNTR_MSB2_STATUS			(1 << 16)
#define TM_PM_CPU_COPY_CNTR_MSB3_STATUS			(1 << 17)
#define TM_PM_GLB_CNTR_MSB0_STATUS			(1 << 18)
#define TM_PM_ERR_CNTR_MEM_STATUS			(1 << 19)
#define TM_PM_ERR_GLB_CNTR_MEM_STATUS			(1 << 20)

#define TM_PM_FLOW_CNTR_MSB0_ENABLE			(1 << 0)
#define TM_PM_FLOW_CNTR_MSB1_ENABLE			(1 << 1)
#define TM_PM_FLOW_CNTR_MSB2_ENABLE			(1 << 2)
#define TM_PM_FLOW_CNTR_MSB3_ENABLE			(1 << 3)
#define TM_PM_FLOW_CNTR_MSB4_ENABLE			(1 << 4)
#define TM_PM_FLOW_CNTR_MSB5_ENABLE			(1 << 5)
#define TM_PM_FLOW_CNTR_MSB6_ENABLE			(1 << 6)
#define TM_PM_FLOW_CNTR_MSB7_ENABLE			(1 << 7)
#define TM_PM_SPID_CNTR_MSB0_ENABLE			(1 << 8)
#define TM_PM_VOQ_CNTR_MSB0_ENABLE			(1 << 9)
#define TM_PM_VOQ_CNTR_MSB1_ENABLE			(1 << 10)
#define TM_PM_CPU_CNTR_MSB0_ENABLE			(1 << 11)
#define TM_PM_PKT_TYPE_CNTR_MSB0_ENABLE			(1 << 12)
#define TM_PM_PKT_TYPE_CNTR_MSB1_ENABLE			(1 << 13)
#define TM_PM_CPU_COPY_CNTR_MSB0_ENABLE			(1 << 14)
#define TM_PM_CPU_COPY_CNTR_MSB1_ENABLE			(1 << 15)
#define TM_PM_CPU_COPY_CNTR_MSB2_ENABLE			(1 << 16)
#define TM_PM_CPU_COPY_CNTR_MSB3_ENABLE			(1 << 17)
#define TM_PM_GLB_CNTR_MSB0_ENABLE			(1 << 18)
#define TM_PM_ERR_CNTR_MEM_ENABLE			(1 << 19)
#define TM_PM_ERR_GLB_CNTR_MEM_ENABLE			(1 << 20)

/* QM Interrupts and Masks */
#define QM_ERR_BUF_LIST_MEM_STATUS			(1 << 0)
#define QM_ERR_CORRECT_BUF_LIST_MEM_STATUS		(1 << 1)
#define QM_ERR_CPU_BUF_LIST_MEM_STATUS			(1 << 2)
#define QM_ERR_CORRECT_CPU_BUF_LIST_MEM_STATUS		(1 << 3)
#define QM_ERR_PROFILE_MEM_STATUS			(1 << 4)
#define QM_ERR_CORRECT_PROFILE_MEM_STATUS		(1 << 5)
#define QM_ERR_STATUS_MEM_STATUS			(1 << 6)
#define QM_ERR_CORRECT_STATUS_MEM_STATUS		(1 << 7)
#define QM_ERR_STATUS_SDRAM_ADDR_MEM_STATUS		(1 << 8)
#define QM_ERR_CORRECT_STATUS_SDRAM_ADDR_MEM_STATUS	(1 << 9)
#define QM_ERR_INT_BUF_LIST_MEM_STATUS			(1 << 10)
#define QM_ERR_CORRECT_INT_BUF_LIST_MEM_STATUS		(1 << 11)

#define QM_FLUSH_COMPLETE_STATUS			(1 << 13)
#define QM_ERR_AXI_INTMEM_READ_STATUS			(1 << 14)
#define QM_ERR_AXI_INTMEM_WRITE_STATUS			(1 << 15)
#define QM_ERR_BUF_UNDERRUN_STATUS			(1 << 16)
#define QM_VOQ_DISABLE_STATUS				(1 << 17)
#define QM_ERR_PKT_ENQUEUE_STATUS			(1 << 18)

#define QM_ERR_CPU_BUF_UNDERRUN_STATUS			(1 << 20)
#define QM_CPU_VOQ_DISABLE_STATUS			(1 << 21)
#define QM_ERR_CPU_PKT_ENQUEUE_STATUS			(1 << 22)

#define QM_PKT_AGE_OLD_STATUS				(1 << 24)
#define QM_QUE_AGE_OLD_STATUS				(1 << 25)
#define QM_ERR_PKT_HEADER_STATUS			(1 << 26)

#define QM_ERR_AXI_QM_READ_STATUS			(1 << 28)
#define QM_ERR_AXI_QM_WRITE_STATUS			(1 << 29)
#define QM_ERR_AXI_QMCPU_READ_STATUS			(1 << 30)
#define QM_ERR_AXI_QMCPU_WRITE_STATUS			(1 << 31)

#define QM_ERR_BUF_LIST_MEM_ENABLE			(1 << 0)
#define QM_ERR_CORRECT_BUF_LIST_MEM_ENABLE		(1 << 1)
#define QM_ERR_CPU_BUF_LIST_MEM_ENABLE			(1 << 2)
#define QM_ERR_CORRECT_CPU_BUF_LIST_MEM_ENABLE		(1 << 3)
#define QM_ERR_PROFILE_MEM_ENABLE			(1 << 4)
#define QM_ERR_CORRECT_PROFILE_MEM_ENABLE		(1 << 5)
#define QM_ERR_STATUS_MEM_ENABLE			(1 << 6)
#define QM_ERR_CORRECT_STATUS_MEM_ENABLE		(1 << 7)
#define QM_ERR_STATUS_SDRAM_ADDR_MEM_ENABLE		(1 << 8)
#define QM_ERR_CORRECT_STATUS_SDRAM_ADDR_MEM_ENABLE	(1 << 9)
#define QM_ERR_INT_BUF_LIST_MEM_ENABLE			(1 << 10)
#define QM_ERR_CORRECT_INT_BUF_LIST_MEM_ENABLE		(1 << 11)

#define QM_FLUSH_COMPLETE_ENABLE			(1 << 13)
#define QM_ERR_AXI_INTMEM_READ_ENABLE			(1 << 14)
#define QM_ERR_AXI_INTMEM_WRITE_ENABLE			(1 << 15)
#define QM_ERR_BUF_UNDERRUN_ENABLE			(1 << 16)
#define QM_VOQ_DISABLE_ENABLE				(1 << 17)
#define QM_ERR_PKT_ENQUEUE_ENABLE			(1 << 18)

#define QM_ERR_CPU_BUF_UNDERRUN_ENABLE			(1 << 20)
#define QM_CPU_VOQ_DISABLE_ENABLE			(1 << 21)
#define QM_ERR_CPU_PKT_ENQUEUE_ENABLE			(1 << 22)

#define QM_PKT_AGE_OLD_ENABLE				(1 << 24)
#define QM_QUE_AGE_OLD_ENABLE				(1 << 25)
#define QM_ERR_PKT_HEADER_ENABLE			(1 << 26)

#define QM_ERR_AXI_QM_READ_ENABLE			(1 << 28)
#define QM_ERR_AXI_QM_WRITE_ENABLE			(1 << 29)
#define QM_ERR_AXI_QMCPU_READ_ENABLE			(1 << 30)
#define QM_ERR_AXI_QMCPU_WRITE_ENABLE			(1 << 31)

/* Scheduler Interrupt And Masks */
#define SCHTOP_CPU_CMD_EXEC_STATUS			(1 << 0)
#define SCHTOP_SHP_PAR_ERR_0_INT_STATUS			(1 << 1)
#define SCHTOP_SHP_PAR_ERR_1_INT_STATUS			(1 << 2)
#define SCHTOP_SHP_PAR_ERR_2_INT_STATUS			(1 << 3)
#define SCHTOP_SHP_PAR_ERR_3_INT_STATUS			(1 << 4)
#define SCHTOP_EXPRESS_MODE_ON_STATUS			(1 << 5)
#define SCHTOP_EXPRESS_MODE_OFF_STATUS			(1 << 6)
#define SCHTOP_SCH_EMYVOQ_REQGVN_STATUS			(1 << 7)

#define SCHTOP_CPU_CMD_EXEC_ENABLE			(1 << 0)
#define SCHTOP_SHP_PAR_ERR_0_INT_ENABLE			(1 << 1)
#define SCHTOP_SHP_PAR_ERR_1_INT_ENABLE			(1 << 2)
#define SCHTOP_SHP_PAR_ERR_2_INT_ENABLE			(1 << 3)
#define SCHTOP_SHP_PAR_ERR_3_INT_ENABLE			(1 << 4)
#define SCHTOP_EXPRESS_MODE_ON_ENABLE			(1 << 5)
#define SCHTOP_EXPRESS_MODE_OFF_ENABLE			(1 << 6)
#define SCHTOP_SCH_EMYVOQ_REQGVN_ENABLE			(1 << 7)

#define SCH_CNFG_PARERR_VOQ_STATUS(qid)			(1 << ((qid)&0x1f))
#define SCH_CNFG_PARERR_VOQ_ENABLE(qid)			(1 << ((qid)&0x1f))

/* ===== NI Interrupts ===== */

/* NI port interrupt */
#define NI_PORT_LINK_STAT_CHANGE			(1 << 0)
#define NI_PORT_TXFIFO_UNDERRUN				(1 << 1)
#define NI_PORT_TXFIFO_OVERRUN				(1 << 2)
#define NI_PORT_RXCNTRL_OVERRUN				(1 << 3)
#define NI_PORT_RXCNTRL_USAGE_EXCEED			(1 << 4)
#define NI_PORT_RXMIB_CNTMSB_SET			(1 << 5)
#define NI_PORT_TXMIB_CNTMSB_SET			(1 << 6)
#define NI_PORT_TXEM_CRCERR_CNTMSB_SET			(1 << 7)
#define NI_PORT_RXCNTRL_RD_IDLE				(1 << 8)

#define NI_PORT_LINK_STAT_CHANGE_ENABLE			(1 << 0)
#define NI_PORT_TXFIFO_UNDERRUN_ENABLE			(1 << 1)
#define NI_PORT_TXFIFO_OVERRUN_ENABLE			(1 << 2)
#define NI_PORT_RXCNTRL_OVERRUN_ENABLE			(1 << 3)
#define NI_PORT_RXCNTRL_USAGE_EXCEED_ENABLE		(1 << 4)
#define NI_PORT_RXMIB_CNTMSB_SET_ENABLE			(1 << 5)
#define NI_PORT_TXMIB_CNTMSB_SET_ENABLE			(1 << 6)
#define NI_PORT_TXEM_CRCERR_CNTMSB_SET_ENABLE		(1 << 7)
#define NI_PORT_RXCNTRL_RD_IDLE_ENABLE			(1 << 8)

/* NI rxfifo interrupt */
#define NI_RXFIFO_FULL_STATUS				(1 << 0)
#define NI_RXFIFO_OVERRUN_STATUS			(1 << 1)
#define NI_RXFIFO_VOQ_FULL_STATUS			(1 << 2)
#define NI_RXFIFO_CONG_STATUS				(1 << 3)
#define NI_RXFIFO_NOEOP_STATUS				(1 << 4)
#define NI_RXFIFO_NOSOP_STATUS				(1 << 5)
#define NI_RXFIFO_NOEOP_AF_FL_STATUS			(1 << 6)
#define NI_RXFIFO_EOP_BF_FL_STATUS			(1 << 7)
#define NI_RXFIFO_LL_ECC_ERR_STATUS			(1 << 8)
#define NI_RXFIFO_LL_ECC_CORR_ERR_STATUS		(1 << 9)
#define NI_RXFIFO_MAL_DROP_PKT_CNT_MSB_SET_STATUS	(1 << 10)
#define NI_RXFIFO_MCAL_PKT_DROP_STATUS			(1 << 11)
#define NI_RXFIFO_VOQ_ECC_ERR_STATUS			(1 << 12)
#define NI_RXFIFO_VOQ_ECC_CORR_ERR_STATUS		(1 << 13)

#define NI_RXFIFO_FULL_ENABLE				(1 << 0)
#define NI_RXFIFO_OVERRUN_ENABLE			(1 << 1)
#define NI_RXFIFO_VOQ_FULL_ENABLE			(1 << 2)
#define NI_RXFIFO_CONG_ENABLE				(1 << 3)
#define NI_RXFIFO_NOEOP_ENABLE				(1 << 4)
#define NI_RXFIFO_NOSOP_ENABLE				(1 << 5)
#define NI_RXFIFO_NOEOP_AF_FL_ENABLE			(1 << 6)
#define NI_RXFIFO_EOP_BF_FL_ENABLE			(1 << 7)
#define NI_RXFIFO_LL_ECC_ERR_ENABLE			(1 << 8)
#define NI_RXFIFO_LL_ECC_CORR_ERR_ENABLE		(1 << 9)
#define NI_RXFIFO_MAL_DROP_PKT_CNT_MSB_SET_ENABLE	(1 << 10)
#define NI_RXFIFO_MCAL_PKT_DROP_ENABLE			(1 << 11)
#define NI_RXFIFO_VOQ_ECC_ERR_ENABLE			(1 << 12)
#define NI_RXFIFO_VOQ_ECC_CORR_ERR_ENABLE		(1 << 13)

/* NI TX EM ingterrupt */
#define NI_TXEM_IFIFO_OVF_STATUS			(1 << 0)
#define NI_TXEM_CRC_ERR_STATUS				(1 << 1)
#define NI_TXEM_CNTOVF_STATUS				(1 << 2)
#define NI_TXEM_TXMIB_FIFO_OVF_STATUS			(1 << 3)
#define NI_TXEM_RXMIB_FIFO_OVF_STATUS			(1 << 4)
#define NI_TXEM_PTP_VOQCHG_ERR_STATUS			(1 << 5)
#define NI_TXEM_PTP_CACHEVOQ_ERR_STATUS			(1 << 6)
#define NI_TXEM_VOQ_LKUP_MEM_PERR_STATUS		(1 << 7)
#define NI_TXEM_MC_INDX_LKUP_MEM_PERR_STATUS		(1 << 8)
#define NI_TXEM_PTP_V6CSUM0_ERR_STATUS			(1 << 9)

#define NI_TXEM_IFIFO_OVF_ENABLE			(1 << 0)
#define NI_TXEM_CRC_ERR_ENABLE				(1 << 1)
#define NI_TXEM_CNTOVF_ENABLE				(1 << 2)
#define NI_TXEM_TXMIB_FIFO_OVF_ENABLE			(1 << 3)
#define NI_TXEM_RXMIB_FIFO_OVF_ENABLE			(1 << 4)
#define NI_TXEM_PTP_VOQCHG_ERR_ENABLE			(1 << 5)
#define NI_TXEM_PTP_CACHEVOQ_ERR_ENABLE			(1 << 6)
#define NI_TXEM_VOQ_LKUP_MEM_PERR_ENABLE		(1 << 7)
#define NI_TXEM_MC_INDX_LKUP_MEM_PERR_ENABLE		(1 << 8)
#define NI_TXEM_PTP_V6CSUM0_ERR_ENABLE			(1 << 9)

/* NI PC interrupt */
#define NI_PC0_DCHK_OUTOFSYNC_STATUS			(1 << 0)
#define NI_PC0_DST_ADDR_MISMATCH_STATUS			(1 << 1)
#define NI_PC0_SRC_ADDR_MISMATCH_STATUS			(1 << 2)
#define NI_PC0_VLAN1_FIELD_MISMATCH_STATUS		(1 << 3)
#define NI_PC0_TYPE_FIELD_MISMATCH_STATUS		(1 << 4)
#define NI_PC0_VLAN2_FIELD_MISMATCH_STATUS		(1 << 5)
#define NI_PC0_SEQNUM_MISMATCH_STATUS			(1 << 6)
#define NI_PC0_DATA_MISMATCH_STATUS			(1 << 7)
#define NI_PC0_FRAME_LEN_MISMATCH_STATUS		(1 << 8)

#define NI_PC0_DCHK_OUTOFSYNC_ENABLE			(1 << 0)
#define NI_PC0_DST_ADDR_MISMATCH_ENABLE			(1 << 1)
#define NI_PC0_SRC_ADDR_MISMATCH_ENABLE			(1 << 2)
#define NI_PC0_VLAN1_FIELD_MISMATCH_ENABLE		(1 << 3)
#define NI_PC0_TYPE_FIELD_MISMATCH_ENABLE		(1 << 4)
#define NI_PC0_VLAN2_FIELD_MISMATCH_ENABLE		(1 << 5)
#define NI_PC0_SEQNUM_MISMATCH_ENABLE			(1 << 6)
#define NI_PC0_DATA_MISMATCH_ENABLE			(1 << 7)
#define NI_PC0_FRAME_LEN_MISMATCH_ENABLE		(1 << 8)

/* NI CPU XRAM CNTR interrupt */
#define NI_CPUXRAM_PKT_DROP_ERR0_SET			(1 << 0)
#define NI_CPUXRAM_PKT_DROP_ERR1_SET			(1 << 1)
#define NI_CPUXRAM_PKT_DROP_ERR2_SET			(1 << 2)
#define NI_CPUXRAM_PKT_DROP_ERR3_SET			(1 << 3)
#define NI_CPUXRAM_PKT_DROP_ERR4_SET			(1 << 4)
#define NI_CPUXRAM_PKT_DROP_ERR5_SET			(1 << 5)
#define NI_CPUXRAM_PKT_DROP_ERR6_SET			(1 << 6)
#define NI_CPUXRAM_PKT_DROP_ERR7_SET			(1 << 7)
#define NI_CPUXRAM_PKT_DROP_ERR8_SET			(1 << 8)
#define NI_CPUXRAM_PKT_TO_XRAM0_SET			(1 << 10)
#define NI_CPUXRAM_PKT_TO_XRAM1_SET			(1 << 11)
#define NI_CPUXRAM_PKT_TO_XRAM2_SET			(1 << 12)
#define NI_CPUXRAM_PKT_TO_XRAM3_SET			(1 << 13)
#define NI_CPUXRAM_PKT_TO_XRAM4_SET			(1 << 14)
#define NI_CPUXRAM_PKT_TO_XRAM5_SET			(1 << 15)
#define NI_CPUXRAM_PKT_TO_XRAM6_SET			(1 << 16)
#define NI_CPUXRAM_PKT_TO_XRAM7_SET			(1 << 17)
#define NI_CPUXRAM_PKT_TO_XRAM8_SET			(1 << 18)
#define NI_CPUXRAM_PKT_DROP_OVERRUN_SET			(1 << 19)
#define NI_CPUXRAM_BYTE_TO_XRAM0_SET			(1 << 20)
#define NI_CPUXRAM_BYTE_TO_XRAM1_SET			(1 << 21)
#define NI_CPUXRAM_BYTE_TO_XRAM2_SET			(1 << 22)
#define NI_CPUXRAM_BYTE_TO_XRAM3_SET			(1 << 23)
#define NI_CPUXRAM_BYTE_TO_XRAM4_SET			(1 << 24)
#define NI_CPUXRAM_BYTE_TO_XRAM5_SET			(1 << 25)
#define NI_CPUXRAM_BYTE_TO_XRAM6_SET			(1 << 26)
#define NI_CPUXRAM_BYTE_TO_XRAM7_SET			(1 << 27)
#define NI_CPUXRAM_BYTE_TO_XRAM8_SET			(1 << 28)
#define NI_CPUXRAM_DMA_PKT_TO_CPU_SET			(1 << 29)
#define NI_CPUXRAM_DMA_BYTE_TO_CPU_SET			(1 << 30)
#define NI_CPUXRAM_PKT_DROP_OVERRUN_MGMT		(1 << 31)

#define NI_CPUXRAM_PKT_DROP_ERR0_SET_ENABLE		(1 << 0)
#define NI_CPUXRAM_PKT_DROP_ERR1_SET_ENABLE		(1 << 1)
#define NI_CPUXRAM_PKT_DROP_ERR2_SET_ENABLE		(1 << 2)
#define NI_CPUXRAM_PKT_DROP_ERR3_SET_ENABLE		(1 << 3)
#define NI_CPUXRAM_PKT_DROP_ERR4_SET_ENABLE		(1 << 4)
#define NI_CPUXRAM_PKT_DROP_ERR5_SET_ENABLE		(1 << 5)
#define NI_CPUXRAM_PKT_DROP_ERR6_SET_ENABLE		(1 << 6)
#define NI_CPUXRAM_PKT_DROP_ERR7_SET_ENABLE		(1 << 7)
#define NI_CPUXRAM_PKT_DROP_ERR8_SET_ENABLE		(1 << 8)
#define NI_CPUXRAM_PKT_TO_XRAM0_SET_ENABLE		(1 << 10)
#define NI_CPUXRAM_PKT_TO_XRAM1_SET_ENABLE		(1 << 11)
#define NI_CPUXRAM_PKT_TO_XRAM2_SET_ENABLE		(1 << 12)
#define NI_CPUXRAM_PKT_TO_XRAM3_SET_ENABLE		(1 << 13)
#define NI_CPUXRAM_PKT_TO_XRAM4_SET_ENABLE		(1 << 14)
#define NI_CPUXRAM_PKT_TO_XRAM5_SET_ENABLE		(1 << 15)
#define NI_CPUXRAM_PKT_TO_XRAM6_SET_ENABLE		(1 << 16)
#define NI_CPUXRAM_PKT_TO_XRAM7_SET_ENABLE		(1 << 17)
#define NI_CPUXRAM_PKT_TO_XRAM8_SET_ENABLE		(1 << 18)
#define NI_CPUXRAM_PKT_DROP_OVERRUN_SET_ENABLE		(1 << 19)
#define NI_CPUXRAM_BYTE_TO_XRAM0_SET_ENABLE		(1 << 20)
#define NI_CPUXRAM_BYTE_TO_XRAM1_SET_ENABLE		(1 << 21)
#define NI_CPUXRAM_BYTE_TO_XRAM2_SET_ENABLE		(1 << 22)
#define NI_CPUXRAM_BYTE_TO_XRAM3_SET_ENABLE		(1 << 23)
#define NI_CPUXRAM_BYTE_TO_XRAM4_SET_ENABLE		(1 << 24)
#define NI_CPUXRAM_BYTE_TO_XRAM5_SET_ENABLE		(1 << 25)
#define NI_CPUXRAM_BYTE_TO_XRAM6_SET_ENABLE		(1 << 26)
#define NI_CPUXRAM_BYTE_TO_XRAM7_SET_ENABLE		(1 << 27)
#define NI_CPUXRAM_BYTE_TO_XRAM8_SET_ENABLE		(1 << 28)
#define NI_CPUXRAM_DMA_PKT_TO_CPU_SET_ENABLE		(1 << 29)
#define NI_CPUXRAM_DMA_BYTE_TO_CPU_SET_ENABLE		(1 << 30)
#define NI_CPUXRAM_PKT_DROP_OVERRUN_MGMT_ENABLE		(1 << 31)

/* NI CPUXRAM err interrupt */
#define NI_CPUXRAM_RX0_PTRBKUP				(1 << 0)
#define NI_CPUXRAM_RX1_PTRBKUP				(1 << 1)
#define NI_CPUXRAM_RX2_PTRBKUP				(1 << 2)
#define NI_CPUXRAM_RX3_PTRBKUP				(1 << 3)
#define NI_CPUXRAM_RX4_PTRBKUP				(1 << 4)
#define NI_CPUXRAM_RX5_PTRBKUP				(1 << 5)
#define NI_CPUXRAM_RX6_PTRBKUP				(1 << 6)
#define NI_CPUXRAM_RX7_PTRBKUP				(1 << 7)
#define NI_CPUXRAM_RX8_PTRBKUP				(1 << 8)
#define NI_CPUXRAM_RX0_DIS_PKT				(1 << 10)
#define NI_CPUXRAM_RX1_DIS_PKT				(1 << 11)
#define NI_CPUXRAM_RX2_DIS_PKT				(1 << 12)
#define NI_CPUXRAM_RX3_DIS_PKT				(1 << 13)
#define NI_CPUXRAM_RX4_DIS_PKT				(1 << 14)
#define NI_CPUXRAM_RX5_DIS_PKT				(1 << 15)
#define NI_CPUXRAM_RX6_DIS_PKT				(1 << 16)
#define NI_CPUXRAM_RX7_DIS_PKT				(1 << 17)
#define NI_CPUXRAM_RX8_DIS_PKT				(1 << 18)
#define NI_CPUXRAM_RX_QMFIFO_OVERRUN			(1 << 20)
#define NI_CPUXRAM_RX_PTPFIFO_OVERRUN			(1 << 21)
#define NI_CPUXRAM_RX_MGMTFIFO_OVERRUN			(1 << 22)
#define NI_CPUXRAM_DMA_FIFO_OVERRUN			(1 << 23)

#define NI_CPUXRAM_RX0_PTRBKUP_ENABLE			(1 << 0)
#define NI_CPUXRAM_RX1_PTRBKUP_ENABLE			(1 << 1)
#define NI_CPUXRAM_RX2_PTRBKUP_ENABLE			(1 << 2)
#define NI_CPUXRAM_RX3_PTRBKUP_ENABLE			(1 << 3)
#define NI_CPUXRAM_RX4_PTRBKUP_ENABLE			(1 << 4)
#define NI_CPUXRAM_RX5_PTRBKUP_ENABLE			(1 << 5)
#define NI_CPUXRAM_RX6_PTRBKUP_ENABLE			(1 << 6)
#define NI_CPUXRAM_RX7_PTRBKUP_ENABLE			(1 << 7)
#define NI_CPUXRAM_RX8_PTRBKUP_ENABLE			(1 << 8)
#define NI_CPUXRAM_RX0_DIS_PKT_ENABLE			(1 << 10)
#define NI_CPUXRAM_RX1_DIS_PKT_ENABLE			(1 << 11)
#define NI_CPUXRAM_RX2_DIS_PKT_ENABLE			(1 << 12)
#define NI_CPUXRAM_RX3_DIS_PKT_ENABLE			(1 << 13)
#define NI_CPUXRAM_RX4_DIS_PKT_ENABLE			(1 << 14)
#define NI_CPUXRAM_RX5_DIS_PKT_ENABLE			(1 << 15)
#define NI_CPUXRAM_RX6_DIS_PKT_ENABLE			(1 << 16)
#define NI_CPUXRAM_RX7_DIS_PKT_ENABLE			(1 << 17)
#define NI_CPUXRAM_RX8_DIS_PKT_ENABLE			(1 << 18)
#define NI_CPUXRAM_RX_QMFIFO_OVERRUN_ENABLE		(1 << 20)
#define NI_CPUXRAM_RX_PTPFIFO_OVERRUN_ENABLE		(1 << 21)
#define NI_CPUXRAM_RX_MGMTFIFO_OVERRUN_ENABLE		(1 << 22)
#define NI_CPUXRAM_DMA_FIFO_OVERRUN_ENABLE		(1 << 23)

/* NI TOP interrupt */
#define NI_TOP_INT_STAT_PSPID_0				(1 << 0)
#define NI_TOP_INT_STAT_PSPID_1				(1 << 1)
#define NI_TOP_INT_STAT_PSPID_2				(1 << 2)
#define NI_TOP_INT_STAT_PSPID_3				(1 << 3)
#define NI_TOP_INT_STAT_PSPID_4				(1 << 4)
#define NI_TOP_INT_STAT_PSPID_5				(1 << 5)
#define NI_TOP_INT_STAT_PSPID_6				(1 << 6)
#define NI_TOP_INT_STAT_PSPID_7				(1 << 7)
#define NI_TOP_INT_STAT_RXFIFO				(1 << 8)
#define NI_TOP_INT_STAT_TXEM				(1 << 9)
#define NI_TOP_INT_STAT_PC0				(1 << 10)
#define NI_TOP_INT_STAT_PC1				(1 << 11)
#define NI_TOP_INT_STAT_PC2				(1 << 12)
#define NI_TOP_CPUXRAM_STAT_CNTR			(1 << 13)
#define NI_TOP_CPUXRAM_STAT_ERR				(1 << 14)

#define NI_TOP_INT_STAT_PSPID_0_ENABLE			(1 << 0)
#define NI_TOP_INT_STAT_PSPID_1_ENABLE			(1 << 1)
#define NI_TOP_INT_STAT_PSPID_2_ENABLE			(1 << 2)
#define NI_TOP_INT_STAT_PSPID_3_ENABLE			(1 << 3)
#define NI_TOP_INT_STAT_PSPID_4_ENABLE			(1 << 4)
#define NI_TOP_INT_STAT_PSPID_5_ENABLE			(1 << 5)
#define NI_TOP_INT_STAT_PSPID_6_ENABLE			(1 << 6)
#define NI_TOP_INT_STAT_PSPID_7_ENABLE			(1 << 7)
#define NI_TOP_INT_STAT_RXFIFO_ENABLE			(1 << 8)
#define NI_TOP_INT_STAT_TXEM_ENABLE			(1 << 9)
#define NI_TOP_INT_STAT_PC0_ENABLE			(1 << 10)
#define NI_TOP_INT_STAT_PC1_ENABLE			(1 << 11)
#define NI_TOP_INT_STAT_PC2_ENABLE			(1 << 12)
#define NI_TOP_CPUXRAM_STAT_CNTR_ENABLE			(1 << 13)
#define NI_TOP_CPUXRAM_STAT_ERR_ENABLE			(1 << 14)

/* NI CPUXRAM rx pkt interrupt*/
#define NI_TOP_XRAM_RX_0_DONE				(1 << 0)
#define NI_TOP_XRAM_RX_1_DONE				(1 << 1)
#define NI_TOP_XRAM_RX_2_DONE				(1 << 2)
#define NI_TOP_XRAM_RX_3_DONE				(1 << 3)
#define NI_TOP_XRAM_RX_4_DONE				(1 << 4)
#define NI_TOP_XRAM_RX_5_DONE				(1 << 5)
#define NI_TOP_XRAM_RX_6_DONE				(1 << 6)
#define NI_TOP_XRAM_RX_7_DONE				(1 << 7)
#define NI_TOP_XRAM_RX_8_DONE				(1 << 8)

#define NI_TOP_XRAM_RX_0_DONE_ENABLE			(1 << 0)
#define NI_TOP_XRAM_RX_1_DONE_ENABLE			(1 << 1)
#define NI_TOP_XRAM_RX_2_DONE_ENABLE			(1 << 2)
#define NI_TOP_XRAM_RX_3_DONE_ENABLE			(1 << 3)
#define NI_TOP_XRAM_RX_4_DONE_ENABLE			(1 << 4)
#define NI_TOP_XRAM_RX_5_DONE_ENABLE			(1 << 5)
#define NI_TOP_XRAM_RX_6_DONE_ENABLE			(1 << 6)
#define NI_TOP_XRAM_RX_7_DONE_ENABLE			(1 << 7)
#define NI_TOP_XRAM_RX_8_DONE_ENABLE			(1 << 8)

/* NI CPUXRAM tx pkt interrupt */
#define NI_TOP_XRAM_TX_0_DONE				(1 << 0)
#define NI_TOP_XRAM_TX_1_DONE				(1 << 1)

#define NI_TOP_XRAM_TX_0_DONE_ENABLE			(1 << 0)
#define NI_TOP_XRAM_TX_1_DONE_ENABLE			(1 << 1)

/* NI WOL stat interrupt */
#define NI_WOL_STAT_0_INT				(1 << 0)

/* Default Interrupt Coalescing Profile */
#define NI_INTR_COALESCING_FIRST_EN		1
#define NI_INTR_COALESCING_PKT			4
#define NI_INTR_COALESCING_DELAY		100

struct cs_ne_irq_info;
/* module specific intr handler. i.e. cpuxram rx pkt */
//typedef int (*cs_ne_irq_hndlr) (u32 dev_id, u32 intr_status);
typedef int (*cs_ne_irq_hndlr) (u32 dev_id,
				const struct cs_ne_irq_info * irq_module,
				u32 intr_status);

struct cs_ne_irq_info {
	char *module_name;
	u32 intr_status;
	u32 intr_enable;
	u32 intr_e_reg;
	cs_ne_irq_hndlr intr_handle;
	const struct cs_ne_irq_info *sub_module[32];
	u32 sub_intr_reg[32];
};

int cs_ne_fedbg_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_fehash_intr_handle(u32 dev_id,
			     const struct cs_ne_irq_info *irq_module,
			     u32 intr_status);
int cs_ne_felpm_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_fepe_intr_handle(u32 dev_id,
			   const struct cs_ne_irq_info *irq_module,
			   u32 intr_status);
int cs_ne_fefwd_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_fetop_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);

int cs_ne_tmpol_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_tmbm_intr_handle(u32 dev_id,
			   const struct cs_ne_irq_info *irq_module,
			   u32 intr_status);
int cs_ne_tmpm_intr_handle(u32 dev_id,
			   const struct cs_ne_irq_info *irq_module,
			   u32 intr_status);
int cs_ne_tmtop_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);

int cs_ne_qmtop_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);

int cs_ne_schcnfg_intr_handle(u32 dev_id,
			      const struct cs_ne_irq_info *irq_module,
			      u32 intr_status);
int cs_ne_schtop_intr_handle(u32 dev_id,
			     const struct cs_ne_irq_info *irq_module,
			     u32 intr_status);

int cs_ne_ni_xram_rx_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_ni_xram_tx_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_ni_port_handle(u32 dev_id,
			 const struct cs_ne_irq_info *irq_module,
			 u32 intr_status);
int cs_ne_ni_rxfifo_intr_handle(u32 dev_id,
				const struct cs_ne_irq_info *irq_module,
				u32 intr_status);
int cs_ne_ni_txem_intr_handle(u32 dev_id,
			      const struct cs_ne_irq_info *irq_module,
			      u32 intr_status);
int cs_ne_ni_pc_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_ni_xram_cntr_intr_handle(u32 dev_id, const struct cs_ne_irq_info
				   *irq_module, u32 intr_status);
int cs_ne_ni_xram_err_intr_handle(u32 dev_id,
				  const struct cs_ne_irq_info *irq_module,
				  u32 intr_status);
int cs_ne_nitop_intr_handle(u32 dev_id,
			    const struct cs_ne_irq_info *irq_module,
			    u32 intr_status);
int cs_ne_global_intr_handle(u32 dev_id,
			     const struct cs_ne_irq_info *irq_module,
			     u32 intr_status);

#endif				/* __CS_NE_IRQ_H__ */
