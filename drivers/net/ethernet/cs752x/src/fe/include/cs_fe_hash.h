/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                CH Hsu <ch.hsu@cortina-systems.com>
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

#ifndef __CS_FE_HASH_H__
#define __CS_FE_HASH_H__

#include <linux/types.h>
#include <mach/cs75xx_fe_core_table.h>

#define CS_HASH_TIMER_PERIOD	10	/* seconds */
#define HASH_OVERFLOW_INDEX_MASK 0x0008
#define IS_OVERFLOW_ENTRY(aaa) ((aaa) & HASH_OVERFLOW_INDEX_MASK)
#define HASH_INDEX_SW2HW(aaa) ((((aaa) & 0x0006) << 11) | ((aaa) >> 4))

typedef enum {
	CRC16_CCITT,
	CRC16_14_1,
	CRC16_14_2,
	CRC16_14_3,
} crc16_polynomial_e;

int cs_fe_hash_add_hash(u32 crc32, u16 crc16, u8 mask_ptr,
		u16 rslt_idx, u16 *return_idx);
int cs_fe_hash_get_hash(unsigned int sw_idx, u32 *crc32, u16 *crc16,
		u8 *mask_ptr, u16 *rslt_idx);
int cs_fe_hash_get_hash_by_crc(u32 crc32, u16 crc16, u8 mask_ptr,
		u16* fwd_rslt_idx, unsigned int *sw_idx);
int cs_fe_hash_update_hash(unsigned int sw_idx, u32 crc32, u16 crc16,
		u8 mask_ptr, u16 rslt_idx);

int cs_fe_hash_del_hash(unsigned int sw_idx);
void cs_fe_hash_flush(void);
u16 cs_fe_hash_keygen_crc16(unsigned char polynomial, unsigned char *buff,
		u16 size);
u32 cs_fe_hash_keygen_crc32(unsigned char *buff,
		u16 bitNumber);
int cs_fe_hash_calc_crc(fe_sw_hash_t *swhash, u32 *pCrc32, u16 *pCrc16,
        unsigned char crc16_polynomial);

#endif /* __CS_FE_HASH_H__ */
