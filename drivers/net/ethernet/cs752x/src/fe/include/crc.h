

#define FALSE           0
#define TRUE            1

unsigned long update_crc_32(unsigned long  crc, char c);
unsigned short update_crc_ccitt(unsigned short crc, char c);
unsigned short update_crc_14_1(unsigned short crc, char c);
unsigned short update_crc_14_2(unsigned short crc, char c);
unsigned short update_crc_14_3(unsigned short crc, char c);


    /*******************************************************************\
    *                                                                   *
    *   #define P_xxxx                                                  *
    *                                                                   *
    *   The CRC's are computed using polynomials. The  coefficients     *
    *   for the algorithms are defined by the following constants.      *
    *                                                                   *
    \*******************************************************************/

#define P_32	0xEDB88320L
#define P_CCITT	0x1021	/* x^16 + x^12 + x^5 + 1 */
#define P_14_1	0x4011	/* x^14 + x^4 + 1 */
#define P_14_2	0x4131	/* x^14 + x^8 + x^5 + x^4 + 1  */
#define P_14_3	0x4B31	/* x^14 + x^11 + x^9 + x^8 + x^5 + x^4 + 1 */

void init_crc32_tab(void);
void init_crcccitt_tab(void);
void init_crc14_1_tab(void);
void init_crc14_2_tab(void);
void init_crc14_3_tab(void);
