#ifndef _CS_CRC_H_
#define _CS_CRC_H_

#include <linux/types.h>

typedef struct _dword_def_ {
	union {
		unsigned long DWORD;
		struct {
			unsigned long b0:1;
			unsigned long b1:1;
			unsigned long b2:1;
			unsigned long b3:1;
			unsigned long b4:1;
			unsigned long b5:1;
			unsigned long b6:1;
			unsigned long b7:1;
			unsigned long b8:1;
			unsigned long b9:1;
			unsigned long b10:1;
			unsigned long b11:1;
			unsigned long b12:1;
			unsigned long b13:1;
			unsigned long b14:1;
			unsigned long b15:1;
			unsigned long b16:1;
			unsigned long b17:1;
			unsigned long b18:1;
			unsigned long b19:1;
			unsigned long b20:1;
			unsigned long b21:1;
			unsigned long b22:1;
			unsigned long b23:1;
			unsigned long b24:1;
			unsigned long b25:1;
			unsigned long b26:1;
			unsigned long b27:1;
			unsigned long b28:1;
			unsigned long b29:1;
			unsigned long b30:1;
			unsigned long b31:1;
		};
	};
} dword_def_s;

typedef struct _word_def_ {
	union {
		unsigned short WORD;

		struct {
			unsigned short b0:1;
			unsigned short b1:1;
			unsigned short b2:1;
			unsigned short b3:1;
			unsigned short b4:1;
			unsigned short b5:1;
			unsigned short b6:1;
			unsigned short b7:1;
			unsigned short b8:1;
			unsigned short b9:1;
			unsigned short b10:1;
			unsigned short b11:1;
			unsigned short b12:1;
			unsigned short b13:1;
			unsigned short b14:1;
			unsigned short b15:1;
		};
	};
} word_def_s;

typedef struct _byte_def_{
	union {
		unsigned char BYTE;

		struct {
			unsigned char b0:1;
			unsigned char b1:1;
			unsigned char b2:1;
			unsigned char b3:1;
			unsigned char b4:1;
			unsigned char b5:1;
			unsigned char b6:1;
			unsigned char b7:1;
		};
	};
} byte_def_s;

__u8 cs_getBit(__u8 *pBuff, __u16 index);
__u32 cs_update_crc_32(__u32 crc, __u8 c);
__u16 cs_update_crc_ccitt(__u16 crc, __u8 c);
__u16 cs_update_crc_14_1(__u16 crc, __u8 c);
__u16 cs_update_crc_14_2(__u16 crc, __u8 c);
__u16 cs_update_crc_14_3(__u16 crc, __u8 c);

#endif /* _CS_CRC_H_ */
