#include "cs_crc.h"

__u8 cs_getBit(__u8 *pBuff, __u16 index)
{
	__u8 byte_idx = index / 8;
	__u8 bit_idx = index % 8;
	__u8 data = (pBuff[byte_idx] >> bit_idx) & 0x01;

	return data;
} /* end cs_getBit() */


/* 
 * CRC32 Polynomial 
 * x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 +
 * x^4 + x^2 + x + 1
 */
__u32 cs_update_crc_32(__u32 crc, __u8 c)
{
	dword_def_s crc32_in;
	dword_def_s crc32_out;
	byte_def_s  data_in;

	data_in.BYTE = c;
	crc32_in.DWORD = crc;

	crc32_out.b0 = crc32_in.b31 ^ data_in.b0;
	crc32_out.b1 = crc32_in.b0 ^ crc32_out.b0;
	crc32_out.b2 = crc32_in.b1 ^ crc32_out.b0;
	crc32_out.b3 = crc32_in.b2;
	crc32_out.b4 = crc32_in.b3 ^ crc32_out.b0;
	crc32_out.b5 = crc32_in.b4 ^ crc32_out.b0;
	crc32_out.b6 = crc32_in.b5;
	crc32_out.b7 = crc32_in.b6 ^ crc32_out.b0;
	crc32_out.b8 = crc32_in.b7 ^ crc32_out.b0;
	crc32_out.b9 = crc32_in.b8;
	crc32_out.b10 = crc32_in.b9 ^ crc32_out.b0;
	crc32_out.b11 = crc32_in.b10 ^ crc32_out.b0;
	crc32_out.b12 = crc32_in.b11 ^ crc32_out.b0;
	crc32_out.b13 = crc32_in.b12;
	crc32_out.b14 = crc32_in.b13;
	crc32_out.b15 = crc32_in.b14;
	crc32_out.b16 = crc32_in.b15 ^ crc32_out.b0;
	crc32_out.b17 = crc32_in.b16;
	crc32_out.b18 = crc32_in.b17;
	crc32_out.b19 = crc32_in.b18;
	crc32_out.b20 = crc32_in.b19;
	crc32_out.b21 = crc32_in.b20;
	crc32_out.b22 = crc32_in.b21 ^ crc32_out.b0;
	crc32_out.b23 = crc32_in.b22 ^ crc32_out.b0;
	crc32_out.b24 = crc32_in.b23;
	crc32_out.b25 = crc32_in.b24;
	crc32_out.b26 = crc32_in.b25 ^ crc32_out.b0;
	crc32_out.b27 = crc32_in.b26;
	crc32_out.b28 = crc32_in.b27;
	crc32_out.b29 = crc32_in.b28;
	crc32_out.b30 = crc32_in.b29;
	crc32_out.b31 = crc32_in.b30;

	return crc32_out.DWORD;
} /* end cs_update_crc_32() */


/* 
 * CRC16 Polynomial: CCITT
 * x^16 + x^12 + x^5 + 1
 */
__u16 cs_update_crc_ccitt(__u16 crc, __u8 c)
{
	word_def_s crc16_in;
	word_def_s crc16_out;
	byte_def_s data_in;

	data_in.BYTE = c;
	crc16_in.WORD = crc;

	crc16_out.b0 = crc16_in.b15 ^ data_in.b0;
	crc16_out.b1 = crc16_in.b0;
	crc16_out.b2 = crc16_in.b1;
	crc16_out.b3 = crc16_in.b2;
	crc16_out.b4 = crc16_in.b3;
	crc16_out.b5 = crc16_in.b4 ^ crc16_out.b0;
	crc16_out.b6 = crc16_in.b5;
	crc16_out.b7 = crc16_in.b6;
	crc16_out.b8 = crc16_in.b7;
	crc16_out.b9 = crc16_in.b8;
	crc16_out.b10 = crc16_in.b9;
	crc16_out.b11 = crc16_in.b10;
	crc16_out.b12 = crc16_in.b11 ^ crc16_out.b0;
	crc16_out.b13 = crc16_in.b12;
	crc16_out.b14 = crc16_in.b13;
	crc16_out.b15 = crc16_in.b14;

	return crc16_out.WORD;
} /* end cs_update_crc_ccitt() */


/* 
 * CRC16 Polynomial_1
 * x^14 + x^4 + 1
 */
__u16 cs_update_crc_14_1(__u16 crc, __u8 c)
{
	word_def_s crc16_in;
	word_def_s crc16_out;
	byte_def_s data_in;

	data_in.BYTE = c;
	crc16_in.WORD = crc;

	crc16_out.b0 = crc16_in.b13 ^ data_in.b0;
	crc16_out.b1 = crc16_in.b0;
	crc16_out.b2 = crc16_in.b1;
	crc16_out.b3 = crc16_in.b2;
	crc16_out.b4 = crc16_in.b3 ^ crc16_out.b0;
	crc16_out.b5 = crc16_in.b4;
	crc16_out.b6 = crc16_in.b5;
	crc16_out.b7 = crc16_in.b6;
	crc16_out.b8 = crc16_in.b7;
	crc16_out.b9 = crc16_in.b8;
	crc16_out.b10 = crc16_in.b9;
	crc16_out.b11 = crc16_in.b10;
	crc16_out.b12 = crc16_in.b11;
	crc16_out.b13 = crc16_in.b12;

	return crc16_out.WORD;
} /* end cs_update_crc_14_1() */

/* 
 * CRC16 Polynomial_2
 * x^14 + x^8 + x^5 + x^4 + 1
 */
__u16 cs_update_crc_14_2(__u16 crc, __u8 c)
{
	word_def_s crc16_in;
	word_def_s crc16_out;
	byte_def_s data_in;

	data_in.BYTE = c;
	crc16_in.WORD = crc;

	crc16_out.b0 = crc16_in.b13 ^ data_in.b0;
	crc16_out.b1 = crc16_in.b0;
	crc16_out.b2 = crc16_in.b1;
	crc16_out.b3 = crc16_in.b2;
	crc16_out.b4 = crc16_in.b3 ^ crc16_out.b0;
	crc16_out.b5 = crc16_in.b4 ^ crc16_out.b0;
	crc16_out.b6 = crc16_in.b5;
	crc16_out.b7 = crc16_in.b6;
	crc16_out.b8 = crc16_in.b7^ crc16_out.b0;
	crc16_out.b9 = crc16_in.b8;
	crc16_out.b10 = crc16_in.b9;
	crc16_out.b11 = crc16_in.b10;
	crc16_out.b12 = crc16_in.b11;
	crc16_out.b13 = crc16_in.b12;

	return crc16_out.WORD;
} /* end cs_update_crc_14_2() */

/* 
 * CRC16 Polynomial_3
 * x^14 + x^11 + x^9 + x^8 + x^5 + x^4 + 1
 */
__u16 cs_update_crc_14_3(__u16 crc, __u8 c)
{
	word_def_s crc16_in;
	word_def_s crc16_out;
	byte_def_s data_in;

	data_in.BYTE = c;
	crc16_in.WORD = crc;

	crc16_out.b0 = crc16_in.b13 ^ data_in.b0;
	crc16_out.b1 = crc16_in.b0;
	crc16_out.b2 = crc16_in.b1;
	crc16_out.b3 = crc16_in.b2;
	crc16_out.b4 = crc16_in.b3 ^ crc16_out.b0;
	crc16_out.b5 = crc16_in.b4 ^ crc16_out.b0;
	crc16_out.b6 = crc16_in.b5;
	crc16_out.b7 = crc16_in.b6;
	crc16_out.b8 = crc16_in.b7 ^ crc16_out.b0;
	crc16_out.b9 = crc16_in.b8 ^ crc16_out.b0;
	crc16_out.b10 = crc16_in.b9;
	crc16_out.b11 = crc16_in.b10 ^ crc16_out.b0;
	crc16_out.b12 = crc16_in.b11;
	crc16_out.b13 = crc16_in.b12;

	return crc16_out.WORD;
} /* end cs_update_crc_14_3() */

