/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
//Bug#40475
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/export.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/types.h>	/* size_t */
#include <mach/cs75xx_mibs.h>


void cs_pe_mibs_desc_init(void)
{
    cs_pe_mibs_desc_t *pmibs_desc = CS_MIBS_DESC_PHY_ADDR;
    
    memset(pmibs_desc, 0, sizeof(cs_pe_mibs_desc_t));
    
    return;
} /* cs_pe_mibs_desc_init() */
EXPORT_SYMBOL(cs_pe_mibs_desc_init);



void *cs_get_pe_mibs_phy_addr(cs_mibs_pe_id pe_id)
{
    cs_pe_mibs_desc_t *pmibs_desc = (cs_pe_mibs_desc_t *)CS_MIBS_DESC_PHY_ADDR;
    void *pmibs = NULL;    

    /* check CS_MIBS_MAGIC */
    if (pmibs_desc->CSMIBsMagic == CS_MIBS_MAGIC) {
        switch (pe_id) {
            case CS_MIBs_ID_PE0:
                if ( (pmibs_desc->PE0MIBsPhyAddress != 0) &&
                     (pmibs_desc->PE0MIBsSize != 0 ) &&
                     ( (pmibs_desc->PE0MIBsPhyAddress + pmibs_desc->PE0MIBsSize) <=
                       CS_MIBS_PHY_ADDR_RRAM1_MAX ) ) {

                    pmibs = (void*)pmibs_desc->PE0MIBsPhyAddress;
                }
                break;
            case CS_MIBs_ID_PE1:
                if ( (pmibs_desc->PE1MIBsPhyAddress != 0) &&
                     (pmibs_desc->PE1MIBsSize != 0 ) &&
                     ( (pmibs_desc->PE1MIBsPhyAddress + pmibs_desc->PE1MIBsSize) <=
                       CS_MIBS_PHY_ADDR_RRAM1_MAX ) ) {

                    pmibs = (void*)pmibs_desc->PE1MIBsPhyAddress;
                }
                break;
        }
    }
    
    return pmibs;
} /* cs_get_pe_mibs_phy_addr() */
EXPORT_SYMBOL(cs_get_pe_mibs_phy_addr);

