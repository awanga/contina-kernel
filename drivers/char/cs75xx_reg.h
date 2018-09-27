/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs75xx_reg.h
 *
 * $Id: 2060-g2-regrw.patch,v 1.1.2.1 2013/02/21 05:31:25 jflee Exp $
 *
 * Support to read/write register in command line for
 * Cortex-A9 CPU on Cortina-Systems Baseboard.
 *
 */
#ifndef _CS75XX_IOCTL_H_
#define _CS75XX_IOCTL_H_    1

#define MII_DEBUG 1
/*----------------------------------------------------------------------
* Command set
*----------------------------------------------------------------------*/
#define SIOCDEVCS75XX   SIOCDEVPRIVATE  // 0x89F0
#define REGREAD         26
#define REGWRITE        27
#define	GMIIREG		28
#define	SMIIREG		29
#define	NIGETPORTCAL	30
#define	NISETPORTCAL	31

/*----------------------------------------------------------------------
* Command Structure
*----------------------------------------------------------------------*/
// Common Header
typedef struct {
    unsigned short      cmd;    // command ID
    unsigned short      len;    // data length, excluding this header
} CS_REGCMD_HDR_T;

//REGREAD
typedef struct {
    unsigned int        location;
    unsigned int        length;
    unsigned int        size;
}CS_REGREAD;

//REGWRITE
typedef struct {
    unsigned int        location;
    unsigned int        data;
    unsigned int        size;
}CS_REGWRITE;

/* GMIIREG */
typedef	struct{
	unsigned short phy_addr;
	unsigned short phy_reg;
	unsigned short phy_len;
} GMIIREG_T_1;

/* SMIIREG */
typedef	struct{
	unsigned short phy_addr;
	unsigned short phy_reg;
	unsigned int phy_data;
} SMIIREG_T_1;

/* NIGETPORTCAL */
typedef	struct{
	unsigned short get_port_cal;
} NIGETPORTCAL_T;

/* NISETPORTCAL */
typedef	struct{
	unsigned short table_address;
	unsigned short pspid_ts;
} NISETPORTCAL_T;

typedef union
{
    CS_REGREAD reg_read;
    CS_REGWRITE reg_write;
    GMIIREG_T_1 get_mii_reg;
    SMIIREG_T_1 set_mii_reg;
    NIGETPORTCAL_T get_ni_cal;
    NISETPORTCAL_T set_ni_cal;
} CS_REG_REQ_E;

extern int ni_mdio_write(int phy_addr, int reg_addr, u16 value);
extern int ni_mdio_read(int phy_addr, int reg_addr);
int cs_ni_set_port_calendar(u16 table_address, u8 pspid_ts);
int ni_get_port_calendar(u16 table_address);
#endif // _CS75XX_IOCTL_H_



