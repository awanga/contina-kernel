/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_fe_hw_table.c
 *
 * $Id: cs_fe_hw_table.c,v 1.8 2011/05/14 02:49:36 whsu Exp $
 *
 * It contains internal FE Hardware Table Management APIs implementation.
 */

#include <linux/module.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/types.h>
#include "cs_fe_head_table.h"
#include "cs_fe_hw_table_api.h"
#include "cs_fe.h"
#include "cs_mut.h"

#define GET_MASK(len) (0xFFFFFFFF >> (32-(len)))
#define GET_LO_MASK(start_pos)	(0xFFFFFFFF << (start_pos))
#define GET_UP_MASK(end_pos)	(GET_MASK(end_pos))

int __cs_fe_hw_table_set_field_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int field, __u32 *p_value);

int cs_fe_hw_table_init(void)
{
	return FE_TABLE_OK;
} /* cs_fe_hw_table_init */


/*
 * There are 1024 Row of 16 bytes entry in this L3 table.
 * Each row can serve for one IPv6 IP address or four IPv4 IP addresses.
 * Therefore, the index given here is 0~4095; however, for IPv6 address,
 * the index given should be divisble by 4.  (If not, it will round off.)
 */

/* It writes the IPv4 IP address & parity to the L3 Result Table Entry with
 * given index. */
int cs_fe_hw_table_set_rslt_l3_ipv4(unsigned int idx, __u32 ip_addr,
		bool parity)
{
	__u32 ip_addr_read[4];
	int status;

	status = cs_fe_hw_table_get_rslt_l3_ipv6(idx, ip_addr_read, NULL);
	if (FE_TABLE_OK != status)
		return status;
	ip_addr_read[(idx & 0x03)] = ip_addr;
	status = cs_fe_hw_table_set_rslt_l3_ipv6(idx, ip_addr_read, parity);

	return status;
} /* cs_fe_hw_table_set_rslt_l3_ipv4 */

/*
 * It writes the IPv6 IP address & parity to the L3 Result Table Entry with
 * given index. If p_ip_addr is NULL, it will clear off the value written
 * in the entry.
 */
int cs_fe_hw_table_set_rslt_l3_ipv6(unsigned int idx, __u32 *p_ip_addr,
		bool parity)
{
	unsigned int try_loop = 0;
	FETOP_FE_L3TBL_DATA0_t tbl_data0;
	FETOP_FE_L3TBL_DATA1_t tbl_data1;
	FETOP_FE_L3TBL_DATA2_t tbl_data2;
	FETOP_FE_L3TBL_DATA3_t tbl_data3;
	FETOP_FE_L3TBL_DATA4_t tbl_data4;
	FETOP_FE_L3TBL_ACCESS_t tbl_access;

	tbl_data0.wrd = tbl_data1.wrd = tbl_data2.wrd = 0;
	tbl_data3.wrd = tbl_data4.wrd = tbl_access.wrd = 0;

	tbl_access.bf.ADDR = (idx >> 2) & 0x03ff;
	tbl_access.bf.rbw = 1;
	tbl_access.bf.access = 1;

	if (p_ip_addr != NULL) {
		tbl_data0.bf.FWD_L3_ADDR = p_ip_addr[0];
		tbl_data1.bf.FWD_L3_ADDR = p_ip_addr[1];
		tbl_data2.bf.FWD_L3_ADDR = p_ip_addr[2];
		tbl_data3.bf.FWD_L3_ADDR = p_ip_addr[3];
	}
	tbl_data4.bf.FWD_L3_MEM_PARITY = parity ? 0x01 : 0;

	writel(tbl_data0.wrd, FETOP_FE_L3TBL_DATA0);
	writel(tbl_data1.wrd, FETOP_FE_L3TBL_DATA1);
	writel(tbl_data2.wrd, FETOP_FE_L3TBL_DATA2);
	writel(tbl_data3.wrd, FETOP_FE_L3TBL_DATA3);
	writel(tbl_data4.wrd, FETOP_FE_L3TBL_DATA4);
	writel(tbl_access.wrd, FETOP_FE_L3TBL_ACCESS);

	/* wait write complete */
	try_loop = 0;
	do {
		tbl_access.wrd = readl(FETOP_FE_L3TBL_ACCESS);
	} while ((1 == tbl_access.bf.access) &&
			(++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT)
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */

	return FE_TABLE_OK;
} /* cs_fe_hw_table_set_rslt_l3_ipv6 */

/*
 * It retrievs the IPv4 IP address & parity from the L3 Result Table Entry with
 * given index.
 */
int cs_fe_hw_table_get_rslt_l3_ipv4(unsigned int idx, __u32 *p_ip_addr,
		bool *p_parity)
{
	__u32 ip_addr_read[4];
	int status;

	status = cs_fe_hw_table_get_rslt_l3_ipv6(idx, ip_addr_read, p_parity);
	if (FE_TABLE_OK != status)
		return status;

	if (NULL != p_ip_addr)
		p_ip_addr[0] = ip_addr_read[(idx & 0x03)];

	return FE_TABLE_OK;
} /* cs_fe_hw_table_get_rslt_l3_ipv4 */

/* It retrieves the IPv6 IP address & parity to the L3 Result Table Entry with
 * given index. */
int cs_fe_hw_table_get_rslt_l3_ipv6(unsigned int idx, __u32 *p_ip_addr,
		bool *p_parity)
{
	unsigned int try_loop = 0;
	FETOP_FE_L3TBL_DATA0_t tbl_data0;
	FETOP_FE_L3TBL_DATA1_t tbl_data1;
	FETOP_FE_L3TBL_DATA2_t tbl_data2;
	FETOP_FE_L3TBL_DATA3_t tbl_data3;
	FETOP_FE_L3TBL_DATA4_t tbl_data4;
	FETOP_FE_L3TBL_ACCESS_t tbl_access;

	/* read the value from HW */
	tbl_access.bf.ADDR = (idx >> 2) & 0x03ff;
	tbl_access.bf.rbw = 0;
	tbl_access.bf.access = 1;
	writel(tbl_access.wrd, FETOP_FE_L3TBL_ACCESS);


	/* wait read complete */
	do {
		tbl_access.wrd = readl(FETOP_FE_L3TBL_ACCESS);
	} while ((1 == tbl_access.bf.access) &&
			(++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT)
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */

	tbl_data0.wrd = readl(FETOP_FE_L3TBL_DATA0);
	tbl_data1.wrd = readl(FETOP_FE_L3TBL_DATA1);
	tbl_data2.wrd = readl(FETOP_FE_L3TBL_DATA2);
	tbl_data3.wrd = readl(FETOP_FE_L3TBL_DATA3);
	tbl_data4.wrd = readl(FETOP_FE_L3TBL_DATA4);

	if (p_ip_addr != NULL) {
		p_ip_addr[0] = tbl_data0.bf.FWD_L3_ADDR;
		p_ip_addr[1] = tbl_data1.bf.FWD_L3_ADDR;
		p_ip_addr[2] = tbl_data2.bf.FWD_L3_ADDR;
		p_ip_addr[3] = tbl_data3.bf.FWD_L3_ADDR;
	}

	if (p_parity != NULL)
		(*p_parity) =
			(tbl_data4.bf.FWD_L3_MEM_PARITY == 1) ? true : false;

	return FE_TABLE_OK;
} /* cs_fe_hw_table_get_rslt_l3_ipv6 */

int cs_fe_hw_table_write_reg_off(unsigned long reg_loc,
		unsigned int offset, unsigned int width, __u32 value)
{
	unsigned long data;

	data = readl(reg_loc);
	data &= ~(GET_MASK(width) << offset);
	data |= (value & (GET_MASK(width) << offset));
	writel(data, reg_loc);

	return FE_TABLE_OK;
} /* cs_fe_hw_table_write_reg_off */

int cs_fe_hw_table_write_data_reg_loop(unsigned long start_reg,
		unsigned int start_pos, unsigned int total_width,
		__u32 *p_value)
{
	unsigned long value, val_remain = 0, tmp_val = 0;
	unsigned int index, width, pos, end_pos, remain_width, bit_width = 32;
	unsigned short offset = start_pos;

	if (total_width <= 8) {
		tmp_val = (*p_value & 0xff);
		bit_width = 8;
	} else if (total_width <= 16) {
		tmp_val = (*p_value & 0xffff);
		bit_width = 16;
	}
	pos = start_pos;
	width = min_t(u16, (32 - pos), total_width);
	remain_width = min_t(u16, 32, total_width) - width;

	end_pos = pos + width;
	index = 0;

	do {
		if (bit_width != 32) {
			value = ((tmp_val << (offset)) | val_remain)
				& (GET_LO_MASK(pos)) & GET_UP_MASK(end_pos);
			val_remain = tmp_val >> (width);
			//val_remain = tmp_val >> (bit_width - remain_width);
		} else {
			value = ((p_value[index] << (offset)) | val_remain)
				& (GET_LO_MASK(pos)) & GET_UP_MASK(end_pos);
			val_remain = p_value[index] >>
				(bit_width - remain_width);
		}

		cs_fe_hw_table_write_reg_off(start_reg, pos, width, value);
		total_width -= width;
		start_reg -= 4;
		pos = 0;
		index++;
		width = min_t(u16, (32 - pos), total_width);
		if ((remain_width + width) < 32)
			remain_width = 0;
		else
			remain_width = min_t(u16, 32, total_width) -
				(width-remain_width);
		end_pos = width;
		if (bit_width != 32)
			tmp_val = 0;
	} while (total_width > 32);

	if (total_width != 0) {
		value = val_remain;
		if (bit_width != 32) {
			cs_fe_hw_table_write_reg_off(start_reg, pos, width,
					value & GET_UP_MASK(end_pos));
		} else if (remain_width != width) {
			value |= (p_value[index] << offset);

			cs_fe_hw_table_write_reg_off(start_reg, pos, width,
				value & GET_LO_MASK(pos) & GET_UP_MASK(end_pos)
				);
		} else {
			cs_fe_hw_table_write_reg_off(start_reg, pos, width,
				value & GET_LO_MASK(pos) & GET_UP_MASK(end_pos)
				);
		}
	}
	return FE_TABLE_OK;
}

int cs_fe_hw_table_read_data_reg_loop(unsigned long start_reg,
		unsigned int start_pos, unsigned int total_width, void *p_value)
{
	unsigned int val_width, val_remain, reg_offset = start_pos;
	unsigned long value;
	unsigned int index = 0, input_width = 32;
	unsigned short *p8_val = NULL;
	unsigned int *p16_val = NULL;
	unsigned long *p32_val = NULL;

	memset(p_value, 0, (total_width + 7) >> 3);

	if (total_width <= 8) {
		input_width = 8;
		p8_val = (unsigned short*)p_value;
	} else if (total_width <= 16) {
		input_width = 16;
		p16_val = (unsigned int*)p_value;
	} else {
		p32_val = (unsigned long*)p_value;
	}
	val_remain = input_width;

	val_width = min_t(u16, 32 - reg_offset, total_width);
	value = readl(start_reg) &
		GET_UP_MASK(min_t(u16, 32, reg_offset + val_width));

	do {
		if (input_width == 8) {
			p8_val[index] |= value >> reg_offset;
		} else if (input_width == 16) {
			p16_val[index] |= value >> reg_offset;
		} else {
			p32_val[index] |= value >> reg_offset;
		}

		val_remain -= val_width;
		total_width -= val_width;

		if (total_width == 0)
			break;
		start_reg -= 4;
		val_width = min_t(u16, val_remain, total_width);
		value = readl(start_reg) &
			GET_UP_MASK(min_t(u16, 32, reg_offset+val_width));

		if (input_width == 8)
			p8_val[index] |= (value << (input_width - val_remain));
		else if (input_width == 16)
			p16_val[index] |= (value << (input_width - val_remain));
		else
			p32_val[index] |= (value << (32 - val_remain));

		total_width -= val_width;

		if (total_width == 0)
			break;

		reg_offset = val_remain;
		val_remain -= val_width;

		if (val_remain == 0) {
			index++;
			val_remain = input_width;
		}
		val_width = min_t(u16, 32 - reg_offset, total_width);
		value = readl(start_reg) &
			GET_UP_MASK(min_t(u16, 32, reg_offset + val_width));
	} while (total_width > 0);

	return FE_STATUS_OK;
}

/* This API writes the value to the field of the entry with given idx in the
 * table with given table type. Return error if p_value = NULL*/
int cs_fe_hw_table_set_field_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int field, __u32 *p_value)
{
	int status;
#ifdef CONFIG_TEST_READ_AFTER_WRITE
	const cs_reg_access_field_s *table_head;
	unsigned short *value;
	unsigned long total_width;
	int byte, bit_width;
	unsigned short mask = 0xff;
	unsigned short *_value = (unsigned short*)p_value;
#endif

	status = __cs_fe_hw_table_set_field_value(table_type, idx, field,
			p_value);
#ifdef CONFIG_TEST_READ_AFTER_WRITE
	table_head = cs_fe_hw_table_info[table_type].table_head;
	total_width = table_head[field].total_width;
	value = ca_malloc((total_width / 32) * 4 + 4, GFP_ATOMIC);
	if (!value)
		return status;

	memset(value, 0, (total_width / 8 + 1));
	status = cs_fe_hw_table_get_field_value(table_type, idx, field, value);

	if (status != FE_TABLE_OK)
		return status;

	/* memory compare now */
	bit_width = total_width % 8;

	for (byte = 0; byte <= (total_width - 1) / 8; byte++) {
		if (byte == (total_width - 1) / 8) {
			mask >>= (8 - (total_width % 8));
		}
		if (((*(value + byte)) & mask) != ((*(_value + byte)) & mask)) {
			printk("%s::write value %x, read %x, mask %x\n",
					__func__, *(_value + byte),
					*(value + byte), mask);
			printk("\ttable %d, field %d, idx %d, pval %x, "
					"byte %d, total_width %d\n",
					table_type, field, idx, 
					*(p_value + total_width / 4), byte,
					total_width);
			return -1;
		}
		mask = 0xff;
	}
#endif
	return status;
}

/* Set the value to a specific field of an table entry with given index
 * and table */
int __cs_fe_hw_table_set_field_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int field, __u32 *p_value)
{
	unsigned int try_loop, table_size;
	unsigned long reg = 0, access_reg_loc, addr_mask;
	const cs_reg_access_field_s *table_head;
	int status;

	if (p_value == NULL)
		return FE_TABLE_ENULLPTR;
	if (table_type >= FE_TABLE_MAX)
		return FE_TABLE_ETBLNOTEXIST;

	table_head = cs_fe_hw_table_info[table_type].table_head;
	table_size = cs_fe_hw_table_info[table_type].table_size;
	access_reg_loc = cs_fe_hw_table_info[table_type].access_reg;
	addr_mask = GET_MASK(cs_fe_hw_table_info[table_type].addr_bit_size);

	if (idx >= table_size)
		return FE_TABLE_EOUTRANGE;

	/* read the value from HW */
	reg |= (idx & addr_mask);
	if (cs_fe_hw_table_info[table_type].f_ecc_bypass != 0)
		reg |= 0 << 30;
	reg |= 0 << 30;		/* set rbw to read */
	reg |= 1 << 31;		/* set access to start */
	writel(reg, access_reg_loc);

	/* wait read complete */
	try_loop = 0;
	do {
		reg = readl(access_reg_loc);
	} while (((1 << 31) & reg) && (++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT)
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */

	/* start writing the value to the specific data register(s) */
	status = cs_fe_hw_table_write_data_reg_loop(table_head[field].start_reg,
			table_head[field].start_pos,
			table_head[field].total_width, p_value);
	if (FE_TABLE_OK != status) return status;

	/* start the write access (prepare access register) */
	reg |= (idx & addr_mask);
	if (cs_fe_hw_table_info[table_type].f_ecc_bypass != 0)
		reg |= 0 << 30;
	reg |= 1 << 30;		/* set rbw to write */
	reg |= 1 << 31;		/* set access to start */
	writel(reg, access_reg_loc);

	/* wait write complete */
	try_loop = 0;
	do {
		reg = readl(access_reg_loc);
	} while (((1 << 31) & reg) && (++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT)
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */

	return FE_TABLE_OK;
} /* cs_fe_hw_table_set_field_value */

int cs_fe_hw_table_set_field_value_to_sw_data(cs_fe_hw_table_e table_type,
		unsigned int field, __u32 *p_value, __u32 *p_data_array,
		unsigned int array_size)
{
	unsigned int loc, pos, end_pos, width, remain_width, total_width;
	unsigned int array_idx, bit_width;
	unsigned long access_reg_loc, value, val_remain = 0, tmp_val = 0;
	unsigned short offset, data_reg_cnt;
	const cs_reg_access_field_s *table_head;

	if ((p_value == NULL) || (p_data_array == NULL))
		return FE_TABLE_ENULLPTR;
	if (table_type >= FE_TABLE_MAX)
		return FE_TABLE_ETBLNOTEXIST;

	table_head = cs_fe_hw_table_info[table_type].table_head;
	access_reg_loc = cs_fe_hw_table_info[table_type].access_reg;
	data_reg_cnt = cs_fe_hw_table_info[table_type].data_reg_num;

	/* write the given value to the proper place in data array */
	loc = data_reg_cnt -
		((table_head[field].start_reg - access_reg_loc) >> 2);
	pos = table_head[field].start_pos;
	total_width = table_head[field].total_width;

	/* error when this field will write over the array with given size */
	if ((((pos + total_width) >> 5) + loc) >= array_size)
		return FE_TABLE_ENOMEM;

	if (total_width < 32) {
		tmp_val = p_value[0];
		bit_width = total_width;
	} else {
		bit_width = 32;
	}
	bit_width = (total_width < 32) ? total_width : 32;

	width = min_t(u16, (32 - pos), total_width);
	remain_width = min_t(u16, 32, total_width) - width;
	end_pos = pos + width;
	offset = pos;
	array_idx = 0;

	do {
		if (bit_width != 32) {
			value = ((tmp_val << (offset)) | val_remain) &
				(GET_LO_MASK(pos)) & GET_UP_MASK(end_pos);
			val_remain = tmp_val >> (width);
		} else {
			value = ((p_value[array_idx] << (offset)) | val_remain) &
					(GET_LO_MASK(pos)) & GET_UP_MASK(end_pos);
			val_remain = p_value[array_idx] >>
				(bit_width - remain_width);
		}

		p_data_array[loc] &= ~(GET_MASK(width) << pos);
		p_data_array[loc] |= (value & (GET_MASK(width) << pos));
		total_width -= width;
		loc++;
		pos = 0;
		array_idx++;
		width = min_t(u16, (32 - pos), total_width);
		if ((remain_width + width) < 32)
			remain_width = 0;
		else
			remain_width = min_t(u16, 32, total_width) -
				(width - remain_width);
		end_pos = width;
		if (bit_width != 32) tmp_val = 0;
	} while (total_width > 32);

	if (total_width != 0) {
		value = val_remain;
		if ((remain_width != width) && (bit_width == 32))
			value |= p_value[array_idx] << offset;
		if (bit_width == 32)
			value &= GET_LO_MASK(pos);
		value &= GET_UP_MASK(end_pos);
		p_data_array[loc] &= ~(GET_MASK(width) << pos);
		p_data_array[loc] |= (value & (GET_MASK(width) << pos));
	}
	return FE_TABLE_OK;
} /* cs_fe_hw_table_set_field_value_to_sw_data */

/* Clear the entry with given index and table */
int cs_fe_hw_table_clear_entry(cs_fe_hw_table_e table_type, unsigned int idx)
{
	unsigned int try_loop, table_size, data_reg_num, i;
	unsigned long reg = 0, access_reg_loc, addr_mask;
	const cs_reg_access_field_s *table_head;

	if (FE_TABLE_MAX <= table_type)
		return FE_TABLE_ETBLNOTEXIST;

	table_head = cs_fe_hw_table_info[table_type].table_head;
	table_size = cs_fe_hw_table_info[table_type].table_size;
	access_reg_loc = cs_fe_hw_table_info[table_type].access_reg;
	addr_mask = GET_MASK(cs_fe_hw_table_info[table_type].addr_bit_size);
	data_reg_num = cs_fe_hw_table_info[table_type].data_reg_num;

	if (idx >= table_size)
		return FE_TABLE_EOUTRANGE;

	for (i = 0; i < data_reg_num; i++)
		writel(0, table_head[0].start_reg - (i << 2));

	/* start the write access (prepare write register) */
	reg |= (idx & addr_mask);
	if (0 != cs_fe_hw_table_info[table_type].f_ecc_bypass)
		reg |= 0 << 30;
	reg |= 1 << 30;		/* set rbw to write */
	reg |= 1 << 31;		/* set access to start */
	writel(reg, access_reg_loc);

	/* wait write complete */
	try_loop = 0;
	do {
		reg = readl(access_reg_loc);
	} while (((1 << 31) & reg) && (++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT)
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */

	return FE_TABLE_OK;
} /* cs_fe_hw_table_clear_entry */

/* It retrieves the value of a specific field with given index of a
 * given table. */
int cs_fe_hw_table_get_field_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int field, __u32 *p_value)
{
	unsigned int try_loop, table_size;
	unsigned long reg = 0, access_reg_loc, addr_mask;
	const cs_reg_access_field_s *table_head;
	int status;
	unsigned short bit_width;

	if (p_value == NULL) {
		printk("%s:: p_Value is NULL!\n", __func__);
		return FE_TABLE_ENULLPTR;
	}

	if (FE_TABLE_MAX <= table_type) {
		printk("%s:: table %d, max %d\n",
			__func__, table_type, FE_TABLE_MAX);
		return FE_TABLE_ETBLNOTEXIST;
	}

	table_head = cs_fe_hw_table_info[table_type].table_head;
	table_size = cs_fe_hw_table_info[table_type].table_size;
	access_reg_loc = cs_fe_hw_table_info[table_type].access_reg;
	addr_mask = GET_MASK(cs_fe_hw_table_info[table_type].addr_bit_size);

	if (idx >= table_size) {
		printk("%s::index %d, table size %d\n",
			__func__, idx, table_size);
		return FE_TABLE_EOUTRANGE;
	}

	/* read the value from HW */
	reg |= (idx & addr_mask);
	if (0 != cs_fe_hw_table_info[table_type].f_ecc_bypass)
		reg |= 0 << 30;
	reg |= 0 << 30;		/* set rbw to read */
	reg |= 1 << 31;		/* set access to start */
	writel(reg, access_reg_loc);

	/* wait read complete */
	try_loop = 0;
	do {
		reg = readl(access_reg_loc);
	} while (((1 << 31) & reg) && (++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT) {
		printk("%s::time out error!\n", __func__);
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */
	}

	/* Init buffer
	 * U32:         total_width <= 32
	 * char MAC[6]: total_width == 48
	 * U64:         total_width == 64
	 * U32 IP[4]:   total_width == 128
	 */
	bit_width = table_head[field].total_width;
	if (bit_width <= 32) {
		*p_value = 0;
	} else if (bit_width == 48) {
		memset((__u8 *)p_value, 0x0, 6);
	} else if (bit_width == 64) {
		__u64 *tmp_val;
		tmp_val = (__u64 *)p_value;
		*tmp_val = 0;
	} else if (bit_width == 128) {
		memset((__u8 *)p_value, 0x0, 16);
	} else {
		printk("%s::unknown bit length (%d)!\n", __func__, bit_width);
	}

	status = cs_fe_hw_table_read_data_reg_loop(table_head[field].start_reg,
			table_head[field].start_pos,
			table_head[field].total_width, p_value);
	if (FE_TABLE_OK != status)
		return status;

	return FE_TABLE_OK;
} /* cs_fe_hw_table_get_field_value */

/* It retrieves the value of a table entry with given index and table type. */
int cs_fe_hw_table_get_entry_value(cs_fe_hw_table_e table_type,
		unsigned int idx, __u32 *p_data_array, unsigned int *p_data_cnt)
{
	unsigned int try_loop, table_size, i;
	unsigned long reg = 0, access_reg_loc, addr_mask;
	const cs_reg_access_field_s *table_head;
	int status;

	if ((p_data_array == NULL) || (p_data_cnt == NULL))
		return FE_TABLE_ENULLPTR;
	if (FE_TABLE_MAX <= table_type)
		return FE_TABLE_ETBLNOTEXIST;

	table_head = cs_fe_hw_table_info[table_type].table_head;
	table_size = cs_fe_hw_table_info[table_type].table_size;
	access_reg_loc = cs_fe_hw_table_info[table_type].access_reg;
	addr_mask = GET_MASK(cs_fe_hw_table_info[table_type].addr_bit_size);

	if (idx >= table_size) return FE_TABLE_EOUTRANGE;

	/* read the value from HW */
	reg |= (idx & addr_mask);
	if (cs_fe_hw_table_info[table_type].f_ecc_bypass != 0)
		reg |= 0 << 30;
	reg |= 0 << 30;		/* set rbw to read */
	reg |= 1 << 31;		/* set access to start */
	writel(reg, access_reg_loc);

	/* wait read complete */
	try_loop = 0;
	do {
		reg = readl(access_reg_loc);
	} while (((1 << 31) & reg) && (++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT) {
		printk("%s::time out error!\n", __func__);
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */
	}

	/* getting the value from data register */
	*p_data_cnt = cs_fe_hw_table_info[table_type].data_reg_num;

	for (i = 0; i < *p_data_cnt; i++) {
		status = cs_fe_hw_table_read_data_reg_loop(
				access_reg_loc + ((*p_data_cnt - i) << 2),
				0, 32, &p_data_array[i]);
		if (status != FE_TABLE_OK) return status;
	}

	return FE_TABLE_OK;
} /* cs_fe_hw_table_get_entry_value */

/* It sets the value to a table entry with given index and table type */
int cs_fe_hw_table_set_entry_value(cs_fe_hw_table_e table_type,
		unsigned int idx, unsigned int data_cnt, __u32 *p_data_array)
{
	unsigned int try_loop, table_size, i;
	unsigned short data_reg_cnt;
	unsigned long reg = 0, access_reg_loc, addr_mask;
	const cs_reg_access_field_s *table_head;

	if (p_data_array == NULL) return FE_TABLE_ENULLPTR;
	if (table_type >= FE_TABLE_MAX) return FE_TABLE_ETBLNOTEXIST;

	//printk("%s:%d:table_type = %d, idx = %d, data_cnt = %d\n", __func__,
	//		__LINE__, table_type, idx, data_cnt);

	table_head = cs_fe_hw_table_info[table_type].table_head;
	table_size = cs_fe_hw_table_info[table_type].table_size;
	access_reg_loc = cs_fe_hw_table_info[table_type].access_reg;
	addr_mask = GET_MASK(cs_fe_hw_table_info[table_type].addr_bit_size);
	data_reg_cnt = cs_fe_hw_table_info[table_type].data_reg_num;

	if (idx >= table_size) return FE_TABLE_EOUTRANGE;

	/* write the value to the specific data registers */
	for (i = 0; i < data_cnt; i++)
		writel(p_data_array[i], access_reg_loc +
				((data_reg_cnt - i) << 2));

	/* start the write access (prepare access register) */
	reg |= (idx & addr_mask);
	if (cs_fe_hw_table_info[table_type].f_ecc_bypass != 0)
		reg |= 0 << 30;
	reg |= 1 << 30;		/* set rbw to write */
	reg |= 1 << 31;		/* set access to start */
	writel(reg, access_reg_loc);

	/* wait write complete */
	try_loop = 0;
	do {
		reg = readl(access_reg_loc);
	} while (((1 << 31) & reg) && (++try_loop < TABLE_TRY_TIMEOUT));
	if (try_loop >= TABLE_TRY_TIMEOUT)
		return FE_TABLE_EACCESSINCOMP;	/* ACCESS can't complete */

	return FE_TABLE_OK;
} /* cs_fe_hw_table_set_entry_value */

int cs_fe_hw_table_flush_table(cs_fe_hw_table_e table_type)
{
	unsigned int table_size, idx;
	int status;

	if (FE_TABLE_MAX <= table_type)
		return FE_TABLE_ETBLNOTEXIST;

	table_size = cs_fe_hw_table_info[table_type].table_size;

	for (idx = 0; idx < table_size; idx++) {
		status = cs_fe_hw_table_clear_entry(table_type, idx);
		if (status != FE_TABLE_OK)
			return status;
	}

	return FE_TABLE_OK;
} /* cs_fe_hw_table_flush_table */

