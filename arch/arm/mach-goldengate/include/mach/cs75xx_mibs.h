//Bug#40475
#ifndef _CS75XX_MIBS_H_
#define _CS75XX_MIBS_H_


/*
 *  Defined for MIB
 */
#define  CS_MIBS_DESC_PHY_ADDR          0xF6400000
#define  CS_MIBS_PHY_ADDR_RRAM1_MAX     0xF6400150

#define CS_MIBS_MAGIC                   0x4D494273  // "MIBs"

typedef enum {
    CS_MIBs_ID_RESERVED = 0,
    CS_MIBs_ID_PE0,
    CS_MIBs_ID_PE1,
    CS_MIBs_ID_MAX,
} cs_mibs_pe_id;

typedef struct cs_pe_mibs_desc_s {
    unsigned int    CSMIBsMagic;            // CS_MIBS_MAGIC
	unsigned int    PE0MIBsPhyAddress;
	unsigned short  PE0MIBsSize;
	unsigned int    PE1MIBsPhyAddress;
	unsigned short  PE1MIBsSize;
	unsigned char   reserved[16];
} __attribute__ ((__packed__)) cs_pe_mibs_desc_t;


void cs_pe_mibs_desc_init(void);
void *cs_get_pe_mibs_phy_addr(cs_mibs_pe_id pe_id);


#endif // _CS75XX_MIBS_H_

