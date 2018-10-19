
    /*******************************************************************\
    *                                                                   *
    *   Library         : lib_crc                                       *
    *   File            : lib_crc.c                                     *
    *   Author          : Lammert Bies  1999-2008                       *
    *   E-mail          : info@lammertbies.nl                           *
    *   Language        : ANSI C                                        *
    *                                                                   *
    *                                                                   *
    *   Description                                                     *
    *   ===========                                                     *
    *                                                                   *
    *   The file lib_crc.c contains the private  and  public  func-     *
    *   tions  used  for  the  calculation of CRC-16, CRC-CCITT and     *
    *   CRC-32 cyclic redundancy values.                                *
    *                                                                   *
    *                                                                   *
    *   Dependencies                                                    *
    *   ============                                                    *
    *                                                                   *
    *   lib_crc.h       CRC definitions and prototypes                  *
    *                                                                   *
    *                                                                   *
    *   Modification history                                            *
    *   ====================                                            *
    *                                                                   *
    *   Date        Version Comment                                     *
    *                                                                   *
    *   2008-04-20  1.16    Added CRC-CCITT calculation for Kermit      *
    *                                                                   *
    *   2007-04-01  1.15    Added CRC16 calculation for Modbus          *
    *                                                                   *
    *   2007-03-28  1.14    Added CRC16 routine for Sick devices        *
    *                                                                   *
    *   2005-12-17  1.13    Added CRC-CCITT with initial 0x1D0F         *
    *                                                                   *
    *   2005-05-14  1.12    Added CRC-CCITT with start value 0          *
    *                                                                   *
    *   2005-02-05  1.11    Fixed bug in CRC-DNP routine                *
    *                                                                   *
    *   2005-02-04  1.10    Added CRC-DNP routines                      *
    *                                                                   *
    *   1999-02-21  1.01    Added FALSE and TRUE mnemonics              *
    *                                                                   *
    *   1999-01-22  1.00    Initial source                              *
    *                                                                   *
    \*******************************************************************/

#include "crc.h"


    /*******************************************************************\
    *                                                                   *
    *   static int crc_tab...init                                       *
    *   static unsigned ... crc_tab...[]                                *
    *                                                                   *
    *   The algorithms use tables with precalculated  values.  This     *
    *   speeds  up  the calculation dramaticaly. The first time the     *
    *   CRC function is called, the table for that specific  calcu-     *
    *   lation  is set up. The ...init variables are used to deter-     *
    *   mine if the initialization has taken place. The  calculated     *
    *   values are stored in the crc_tab... arrays.                     *
    *                                                                   *
    *   The variables are declared static. This makes them  invisi-     *
    *   ble for other modules of the program.                           *
    *                                                                   *
    \*******************************************************************/

static int              crc_tab32_init         = FALSE;
static int              crc_tabccitt_init      = FALSE;
static int              crc_tab14_1_init       = FALSE;
static int              crc_tab14_2_init       = FALSE;
static int              crc_tab14_3_init       = FALSE;

static unsigned long    crc_tab32[256];
static unsigned short   crc_tabccitt[256];
static unsigned short   crc_tab14_1[256];
static unsigned short   crc_tab14_2[256];
static unsigned short   crc_tab14_3[256];

    /*******************************************************************\
    *                                                                   *
    *   unsigned short update_crc_ccitt( unsigned long crc, char c );   *
    *                                                                   *
    *   The function update_crc_ccitt calculates  a  new  CRC-CCITT     *
    *   value  based  on the previous value of the CRC and the next     *
    *   byte of the data to be checked.                                 *
    *                                                                   *
    \*******************************************************************/

inline unsigned short update_crc_ccitt( unsigned short crc, char c ) {

    unsigned short tmp, short_c;

    short_c  = 0x00ff & (unsigned short) c;

    if ( ! crc_tabccitt_init ) init_crcccitt_tab();

    tmp = (crc >> 8) ^ short_c;
    crc = (crc << 8) ^ crc_tabccitt[tmp];

    return crc;

}  /* update_crc_ccitt */

    /*******************************************************************\
    *                                                                   *
    *   unsigned short update_crc_14_1( unsigned long crc, char c );   *
    *                                                                   *
    *   The function update_crc_14_1 calculates  a  new  CRC		*
    *   value  based  on the previous value of the CRC and the next     *
    *   byte of the data to be checked.                                 *
    *                                                                   *
    \*******************************************************************/

inline unsigned short update_crc_14_1( unsigned short crc, char c ) {

    unsigned short tmp, short_c;

    short_c  = 0x00ff & (unsigned short) c;

    if ( ! crc_tab14_1_init ) init_crc14_1_tab();

    tmp = (crc >> 8) ^ short_c;
    crc = (crc << 8) ^ crc_tab14_1[tmp];

    return crc;

}  /* update_crc_ccitt */

    /*******************************************************************\
    *                                                                   *
    *   unsigned short update_crc_14_2( unsigned long crc, char c );   *
    *                                                                   *
    *   The function update_crc_14_2 calculates  a  new  CRC            *
    *   value  based  on the previous value of the CRC and the next     *
    *   byte of the data to be checked.                                 *
    *                                                                   *
    \*******************************************************************/

inline unsigned short update_crc_14_2( unsigned short crc, char c ) {

    unsigned short tmp, short_c;

    short_c  = 0x00ff & (unsigned short) c;

    if ( ! crc_tab14_2_init ) init_crc14_2_tab();

    tmp = (crc >> 8) ^ short_c;
    crc = (crc << 8) ^ crc_tab14_2[tmp];

    return crc;

}  /* update_crc_ccitt */

    /*******************************************************************\
    *                                                                   *
    *   unsigned short update_crc_14_3( unsigned long crc, char c );   *
    *                                                                   *
    *   The function update_crc_14_3 calculates  a  new  CRC            *
    *   value  based  on the previous value of the CRC and the next     *
    *   byte of the data to be checked.                                 *
    *                                                                   *
    \*******************************************************************/

inline unsigned short update_crc_14_3( unsigned short crc, char c ) {

    unsigned short tmp, short_c;

    short_c  = 0x00ff & (unsigned short) c;

    if ( ! crc_tab14_3_init ) init_crc14_3_tab();

    tmp = (crc >> 8) ^ short_c;
    crc = (crc << 8) ^ crc_tab14_3[tmp];

    return crc;

}  /* update_crc_ccitt */


    /*******************************************************************\
    *                                                                   *
    *   unsigned long update_crc_32( unsigned long crc, char c );       *
    *                                                                   *
    *   The function update_crc_32 calculates a  new  CRC-32  value     *
    *   based  on  the  previous value of the CRC and the next byte     *
    *   of the data to be checked.                                      *
    *                                                                   *
    \*******************************************************************/

inline unsigned long update_crc_32( unsigned long crc, char c ) {

    unsigned long tmp, long_c;

    long_c = 0x000000ffL & (unsigned long) c;

    if ( ! crc_tab32_init ) init_crc32_tab();

    tmp = crc ^ long_c;
    crc = (crc >> 8) ^ crc_tab32[ tmp & 0xff ];

    return crc;

}  /* update_crc_32 */


    /*******************************************************************\
    *                                                                   *
    *   static void init_crc32_tab( void );                             *
    *                                                                   *
    *   The function init_crc32_tab() is used  to  fill  the  array     *
    *   for calculation of the CRC-32 with values.                      *
    *                                                                   *
    \*******************************************************************/

void init_crc32_tab( void ) {

    int i, j;
    unsigned long crc;

    for (i=0; i<256; i++) {

        crc = (unsigned long) i;

        for (j=0; j<8; j++) {

            if ( crc & 0x00000001L ) crc = ( crc >> 1 ) ^ P_32;
            else                     crc =   crc >> 1;
        }

        crc_tab32[i] = crc;
    }

    crc_tab32_init = TRUE;

}  /* init_crc32_tab */



    /*******************************************************************\
    *                                                                   *
    *   static void init_crcccitt_tab( void );                          *
    *                                                                   *
    *   The function init_crcccitt_tab() is used to fill the  array     *
    *   for calculation of the CRC-CCITT with values.                   *
    *                                                                   *
    \*******************************************************************/

void init_crcccitt_tab( void ) {

    int i, j;
    unsigned short crc, c;

    for (i=0; i<256; i++) {

        crc = 0;
        c   = ((unsigned short) i) << 8;

        for (j=0; j<8; j++) {

            if ( (crc ^ c) & 0x8000 ) crc = ( crc << 1 ) ^ P_CCITT;
            else                      crc =   crc << 1;

            c = c << 1;
        }

        crc_tabccitt[i] = crc;
    }

    crc_tabccitt_init = TRUE;

}  /* init_crcccitt_tab */


    /*******************************************************************\
    *                                                                   *
    *   static void init_crc14_1_tab( void );                          *
    *                                                                   *
    *   The function init_crc14_1_tab() is used to fill the  array     *
    *   for calculation of the CRC with values.                   *
    *                                                                   *
    \*******************************************************************/

void init_crc14_1_tab( void ) {

    int i, j;
    unsigned short crc, c;

    for (i=0; i<256; i++) {

        crc = 0;
        c   = ((unsigned short) i) << 8;

        for (j=0; j<8; j++) {

            if ( (crc ^ c) & 0x8000 ) crc = ( crc << 1 ) ^ P_14_1;
            else                      crc =   crc << 1;

            c = c << 1;
        }

        crc_tab14_1[i] = crc;
    }

    crc_tab14_1_init = TRUE;

}  /* init_crcccitt_tab */


    /*******************************************************************\
    *                                                                   *
    *   static void init_crc14_2_tab( void );                          *
    *                                                                   *
    *   The function init_crc14_2_tab() is used to fill the  array     *
    *   for calculation of the CRC with values.                   *
    *                                                                   *
    \*******************************************************************/

void init_crc14_2_tab( void ) {

    int i, j;
    unsigned short crc, c;

    for (i=0; i<256; i++) {

        crc = 0;
        c   = ((unsigned short) i) << 8;

        for (j=0; j<8; j++) {

            if ( (crc ^ c) & 0x8000 ) crc = ( crc << 1 ) ^ P_14_2;
            else                      crc =   crc << 1;

            c = c << 1;
        }

        crc_tab14_2[i] = crc;
    }

    crc_tab14_2_init = TRUE;

}  /* init_crcccitt_tab */


    /*******************************************************************\
    *                                                                   *
    *   static void init_crc14_3_tab( void );                          *
    *                                                                   *
    *   The function init_crc14_3_tab() is used to fill the  array     *
    *   for calculation of the CRC with values.                   *
    *                                                                   *
    \*******************************************************************/

void init_crc14_3_tab( void ) {

    int i, j;
    unsigned short crc, c;

    for (i=0; i<256; i++) {

        crc = 0;
        c   = ((unsigned short) i) << 8;

        for (j=0; j<8; j++) {

            if ( (crc ^ c) & 0x8000 ) crc = ( crc << 1 ) ^ P_14_3;
            else                      crc =   crc << 1;

            c = c << 1;
        }

        crc_tab14_3[i] = crc;
    }

    crc_tab14_3_init = TRUE;

}  /* init_crcccitt_tab */


