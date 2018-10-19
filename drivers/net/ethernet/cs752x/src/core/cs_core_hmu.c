/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_core_hmu.c
 *
 * $Id$
 *
 * It contains the core HMU implementation.
 */

#include <linux/list.h>
#include <linux/errno.h>
#include <linux/if_vlan.h>
#include <linux/sched.h>
#include "cs_hmu.h"
#include "cs_core_hmu.h"
#include "cs_fe.h"
#include "cs_mut.h"

#define PFX     "CS_CORE_HMU"
#define PRINT(format, args...) printk(KERN_WARNING PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ne_core_logic_debug;
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_CORE_HMU) x;}
#else
#define DBG(x) { }
#endif



/* internal data structure */
typedef struct cs_core_hmu_callback_list_s {
	struct list_head node;
	int (*callback) (u32 watch_bitmask, cs_core_hmu_value_t *value,
			u32 status);
} cs_core_hmu_callback_list_t;

typedef struct cs_core_hmu_int_s {
	struct cs_core_hmu_int_s *next, *prev;

	cs_hmu_table_t *hmu_table;

	u32 watch_bitmask;
	u8 *mask_in_str;
	u16 str_size;
	cs_core_hmu_value_t *value_mask;	/* If the value for an enabled
						   watch bitmask is not set, it
						   will just watch all the
						   values */
	unsigned int callback_func_cnt;
	struct list_head callback_func_list;
} cs_core_hmu_int_t;

static cs_core_hmu_int_t *core_hmu_list = NULL;

#define FWDRSLT_KEY_SIZE	2
static cs_core_hmu_value_t fwdrslt_value = {
	.next = NULL,
	.type = CS_CORE_HMU_WATCH_FWDRSLT,
	.value.index = 0,
	.mask = 0,
};

static cs_core_hmu_t fwdrslt_core_hmu = {
	.watch_bitmask = CS_CORE_HMU_WATCH_FWDRSLT,
	.value_mask = &fwdrslt_value,
	.callback = NULL,
};

static int fwdrslt_hmu_callback(cs_hmu_table_t *table, unsigned char *key,
		u32 status)
{
#if 0
	u16 fwdrslt_idx = *((u16 *)key);
	int ret;

	if (table == NULL)
		return -1;

	switch (status) {
	case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_TIMEOUT:
	case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_DELETED:
		/* don't know how many have been deleted, but at least
		* we need to subtract one */
		cs_fe_fwdrslt_del_by_idx(fwdrslt_idx);
		break;
	case CS_HMU_TBL_RET_STATUS_ALL_HASH_TIMEOUT:
	case CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED:
		/* force remove the fwdrslt, since no one should use it.
		 * should we watch out for the case that fwdrslt is one
		 * of those default fwdrslt? */
		do {
			ret = cs_fe_fwdrslt_del_by_idx(fwdrslt_idx);
		} while (ret == 0);
		break;
	default:
		/* do nothing */
		break;
	}
#endif
	return 0;
} /* fwdrslt_hmu_callback */

#define QOSRSLT_KEY_SIZE	2
static cs_core_hmu_value_t qosrslt_value = {
	.next = NULL,
	.type = CS_CORE_HMU_WATCH_QOSRSLT,
	.value.index = 0,
	.mask = 0,
};

static cs_core_hmu_t qosrslt_core_hmu = {
	.watch_bitmask = CS_CORE_HMU_WATCH_QOSRSLT,
	.value_mask = &qosrslt_value,
	.callback = NULL,
};

static int qosrslt_hmu_callback(cs_hmu_table_t *table, unsigned char *key,
		u32 status)
{
#if 0
	u16 qosrslt_idx = (u16) *key;
	//int ret;

	if (table == NULL)
		return -1;

	switch (status) {
	case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_TIMEOUT:
	case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_DELETED:
		/* don't know how many have been deleted, but at least
		 * we need to subtract one */
		cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT,
				(unsigned int)qosrslt_idx, false);
		break;
	case CS_HMU_TBL_RET_STATUS_ALL_HASH_TIMEOUT:
	case CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED:
		/* force remove the qosrslt, since no one should use it.
		 * should we watch out for the case that qosrslt is not
		 * linked with HMU? */
		cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT,
				(unsigned int)qosrslt_idx, true);
		break;
	default:
		/* do nothing */
		break;
	}
#endif
	return 0;
} /* qosrslt_hmu_callback */

#define QOSHASH_KEY_SIZE	4
static cs_core_hmu_value_t qoshash_value = {
	.next = NULL,
	.type = CS_CORE_HMU_WATCH_QOSHASH,
	.value.index = 0,
	.mask = 0,
};

static cs_core_hmu_t qoshash_core_hmu = {
	.watch_bitmask = CS_CORE_HMU_WATCH_QOSHASH,
	.value_mask = &qoshash_value,
	.callback = NULL,
};

static int qoshash_hmu_callback(cs_hmu_table_t *table, unsigned char *key,
		u32 status)
{
#if 0
	u32 qoshash_idx = *((u32 *)key);

	if (table == NULL)
		return -1;

	switch (status) {
	case CS_HMU_TBL_RET_STATUS_ALL_HASH_TIMEOUT:
	case CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED:
		/* remove the QOSHASH when all the FWDHASH related are
		 * deleted. Since there is a spinlock issue, the easy
		 * workaround is by marking the lifetime of a QoSHash to 1,
		 * so it will be deleted soon. */
		return cs_hmu_change_lifetime(qoshash_idx, 1);
	case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_TIMEOUT:
	case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_DELETED:
	default:
		/* do nothing */
		break;
	}
#endif
	return 0;
} /* qoshash_hmu_callback */

static int key_type_to_size(u32 type)
{
	int size;

	switch (type) {
	case CS_CORE_HMU_WATCH_IN_MAC_SA:
	case CS_CORE_HMU_WATCH_IN_MAC_DA:
	case CS_CORE_HMU_WATCH_OUT_MAC_SA:
	case CS_CORE_HMU_WATCH_OUT_MAC_DA:
		size = 6;
		break;
	case CS_CORE_HMU_WATCH_IN_IPV4_SA:
	case CS_CORE_HMU_WATCH_IN_IPV4_DA:
	case CS_CORE_HMU_WATCH_OUT_IPV4_SA:
	case CS_CORE_HMU_WATCH_OUT_IPV4_DA:
		size = 4;
		break;
	case CS_CORE_HMU_WATCH_IN_IPV6_SA:
	case CS_CORE_HMU_WATCH_IN_IPV6_DA:
	case CS_CORE_HMU_WATCH_OUT_IPV6_SA:
	case CS_CORE_HMU_WATCH_OUT_IPV6_DA:
		size = 16;
		break;
	case CS_CORE_HMU_WATCH_IN_VID1:
	case CS_CORE_HMU_WATCH_OUT_VID1:
	case CS_CORE_HMU_WATCH_IN_VID2:
	case CS_CORE_HMU_WATCH_OUT_VID2:
	case CS_CORE_HMU_WATCH_PPPOE_ID:
	case CS_CORE_HMU_WATCH_FWDRSLT:
	case CS_CORE_HMU_WATCH_QOSRSLT:
	case CS_CORE_HMU_WATCH_QOSHASH:
	case CS_CORE_HMU_WATCH_SRC_PHY_PORT:
	case CS_CORE_HMU_WATCH_DST_PHY_PORT:
	case CS_CORE_HMU_WATCH_IN_L4_SP:
	case CS_CORE_HMU_WATCH_IN_L4_DP:
	case CS_CORE_HMU_WATCH_OUT_L4_SP:
	case CS_CORE_HMU_WATCH_OUT_L4_DP:
		size = 2;
		break;
	case CS_CORE_HMU_WATCH_SWID64:
		size = CS_SWID64_MOD_MAX << 3;
		break;
	case CS_CORE_HMU_WATCH_IP_PROT:
		size = 1;
		break;
	default:
		size = 0;
	}
	return size;
}

static int watch_bitmask_to_size(u32 watch_bitmask)
{
	int size = 0;
	int i, type;

	for (i = 0; i < 32; i++) {
		type = 0x1 << i;
		if (watch_bitmask & type)
			size += key_type_to_size(type);
	}
	return size;
}

/* return length of this sub key */
static int masked_value_to_sub_key(u8 *ptr, cs_core_hmu_value_t *value,
		bool check_mask)
{
	u16 mask;
	u16 mask_mode = 0, mask_real = 0;
	u16 unmask_byte, unmask_int, unmask_bit, unmask_real;
	int i;
	u8 *p8 = (u8 *)ptr;
	u16 *p16 = (u16 *)ptr;
	u32 *p32 = (u32 *)ptr;
	u64 *p64 = (u64 *)ptr;
	u16 swid_mod_id;

	mask = value->mask;
	if (check_mask == false)
		mask = 0xFFFF;

	if (mask > 0) {
		mask_mode  = mask & 0x8000;
		mask_real  = mask & 0x7FFF;
	}

	switch (value->type) {
	case CS_CORE_HMU_WATCH_IN_MAC_SA:
	case CS_CORE_HMU_WATCH_IN_MAC_DA:
	case CS_CORE_HMU_WATCH_OUT_MAC_SA:
	case CS_CORE_HMU_WATCH_OUT_MAC_DA:
		if (mask_real == 0) {
			memset(ptr, 0, 6);
			return 6;
		}

		memcpy(ptr, value->value.mac_addr, 6);
		if (mask_real >= (8 * 6))
			return 6;

		unmask_real  = (8 * 6) - mask_real;
		unmask_byte = unmask_real >> 3;
		unmask_bit  = unmask_real & 0x0007;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_byte; i++)
				ptr[i] = 0;
			ptr[unmask_byte] &= (0xFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_byte; i++)
				ptr[5 - i] = 0;
			ptr[5 - unmask_byte] &= (0xFF << unmask_bit);
		}

		return 6;
	case CS_CORE_HMU_WATCH_IN_IPV4_SA:
	case CS_CORE_HMU_WATCH_IN_IPV4_DA:
	case CS_CORE_HMU_WATCH_OUT_IPV4_SA:
	case CS_CORE_HMU_WATCH_OUT_IPV4_DA:
		if (mask_real == 0) {
			memset(ptr, 0, 4);
			return 4;
		}

		*p32 = ntohl(value->value.ip_addr[0]);
		if (mask_real >= (8 * 4))
			return 4;

		unmask_real = (8 * 4) - mask_real;
		if (mask_mode > 0)
			*p32 &= (0xFFFFFFFF >> unmask_real);
		else
			*p32 &= (0xFFFFFFFF << unmask_real);

		return 4;
	case CS_CORE_HMU_WATCH_IN_IPV6_SA:
	case CS_CORE_HMU_WATCH_IN_IPV6_DA:
	case CS_CORE_HMU_WATCH_OUT_IPV6_SA:
	case CS_CORE_HMU_WATCH_OUT_IPV6_DA:
		if (mask_real == 0) {
			memset(ptr, 0, 16);
			return 16;
		}

		p32[0] = ntohl(value->value.ip_addr[0]);
		p32[1] = ntohl(value->value.ip_addr[1]);
		p32[2] = ntohl(value->value.ip_addr[2]);
		p32[3] = ntohl(value->value.ip_addr[3]);
		if (mask_real >= (8 * 16))
			return 16;

		unmask_real = (8 * 16) - mask_real;
		unmask_int = unmask_real >> 5;
		unmask_bit = unmask_real & 0x001F;

		if (mask_mode > 0) {
			for (i = 0; i < unmask_int; i++)
				p32[i] = 0;
			p32[unmask_int] &= (0xFFFFFFFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_int; i++)
				p32[3 - i] = 0;
			p32[3 - unmask_int] &= (0xFFFFFFFF << unmask_bit);
		}

		return 16;
	case CS_CORE_HMU_WATCH_IN_VID1:
	case CS_CORE_HMU_WATCH_OUT_VID1:
	case CS_CORE_HMU_WATCH_IN_VID2:
	case CS_CORE_HMU_WATCH_OUT_VID2:
		if (mask_real == 0) {
			memset(ptr, 0, 2);
			return 2;
		}

		*p16 = value->value.vid;
		if (mask_real >= (8 * 2))
			return 2;

		unmask_real = (8 * 2) - mask_real;
		if (mask_mode > 0)
			*p16 &= (0xFFFF >> unmask_real);
		else
			*p16 &= (0xFFFF << unmask_real);

		return 2;
	case CS_CORE_HMU_WATCH_PPPOE_ID:
		if (mask_real == 0) {
			memset(ptr, 0, 2);
			return 2;
		}

		*p16 = value->value.pppoe_session_id;
		if (mask_real >= (8 * 2))
			return 2;

		unmask_real = (8 * 2) - mask_real;
		if (mask_mode > 0)
			*p16 &= (0xFFFF >> unmask_real);
		else
			*p16 &= (0xFFFF << unmask_real);

		return 2;
	case CS_CORE_HMU_WATCH_IP_PROT:
		/* shouldn't have mask */
		p8 = (u8 *) ptr;
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("IP PROT mask value shouldn't have mask!\n"));

		if (check_mask == true)
			*p8 = 0;
		else
			*p8 = value->value.ip_prot;
		return 1;
	case CS_CORE_HMU_WATCH_SWID64:
		swid_mod_id = (u16)CS_SWID64_TO_MOD_ID(value->value.swid64);
		if ((mask_real == 0) || (swid_mod_id >= CS_SWID64_MOD_MAX)) {
			memset(ptr, 0xFF, CS_SWID64_MOD_MAX << 3);
			return (CS_SWID64_MOD_MAX << 3);
		}

		for (i = 0; i < CS_SWID64_MOD_MAX; i++) {
			if (i == swid_mod_id)
				p64[i] = value->value.swid64;
			else
				p64[i] = 0xFFFFFFFFFFFFFFFF;
		}

		if (mask_real >= (8 * 8))
			return (CS_SWID64_MOD_MAX << 3);

		unmask_real = (8 * 8) - mask_real;
		if (mask_mode > 0)
			p64[swid_mod_id] &= (0xFFFFFFFFFFFFFFFF >> unmask_real);
		else
			p64[swid_mod_id] &= (0xFFFFFFFFFFFFFFFF << unmask_real);

		return (CS_SWID64_MOD_MAX << 3);
	case CS_CORE_HMU_WATCH_FWDRSLT:
	case CS_CORE_HMU_WATCH_QOSRSLT:
	case CS_CORE_HMU_WATCH_QOSHASH:
	case CS_CORE_HMU_WATCH_SRC_PHY_PORT:
	case CS_CORE_HMU_WATCH_DST_PHY_PORT:
		/* shouldn't have mask */
		p16 = (u16 *)ptr;
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("Index value shouldn't have mask!!\n"));
		if (check_mask == true)
			*p16 = 0x0000;
		else
			*p16 = value->value.index;
		return 2;
	case CS_CORE_HMU_WATCH_IN_L4_SP:
	case CS_CORE_HMU_WATCH_IN_L4_DP:
	case CS_CORE_HMU_WATCH_OUT_L4_SP:
	case CS_CORE_HMU_WATCH_OUT_L4_DP:
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("Port value shouldn't have mask!!\n"));

		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = ntohs(value->value.l4_port);
		return 2;
	default:
		return 0;
	}
}

/* return pointer of key */
static u8 *encode_core_hmu_value_to_key(u32 watch_bitmask,
		cs_core_hmu_value_t *value, bool check_mask)
{
	int i, j, type, len;
	int sub_len, offset;
	u8 *key;
	cs_core_hmu_value_t *curr;
	bool f_match_type;

	len = watch_bitmask_to_size(watch_bitmask);
	if (len <= 0) {
		DBG(PRINT("invalid key length, watch_bitmask = 0x%08X\n",
			watch_bitmask));
		return NULL;
	}

	key = (u8 *) cs_zalloc(len, GFP_ATOMIC);
	if (key == NULL) {
		DBG(PRINT("No memory for key, len = %d\n", len));
		return NULL;
	}

	/* start from the lowest bit */
	for (i = 0, offset = 0; i < 32; i++) {
		type = 0x1 << i;
		if (watch_bitmask & type) {
			curr = value;
			f_match_type = false;
			while (curr != NULL) {
				if (curr->type == type) {
					f_match_type = true;
					offset += masked_value_to_sub_key(
						key + offset, curr, check_mask);
					break;
				}
				curr = curr->next;
			}

			if (f_match_type == false) {
				/* fill all sub key with 0x00 */
				sub_len = key_type_to_size(type);
				for (j = 0; j < sub_len; j++) {
					if (type == CS_CORE_HMU_WATCH_SWID64)
						key[offset++] = 0xFF;
					else
						key[offset++] = 0x00;
				}
			}
		}
	}

	return key;
}

static cs_core_hmu_value_t *get_value_from_type(
		cs_core_hmu_value_t *start_value, u32 type)
{
	cs_core_hmu_value_t *curr_value = start_value;

	while (curr_value != NULL) {
		if (curr_value->type == type)
			return curr_value;
		curr_value = curr_value->next;
	};
	return NULL;
}

static cs_core_hmu_value_t *copy_hmu_value(cs_core_hmu_t *hmu_entry)
{
	int i;
	cs_core_hmu_value_t *curr_value, *new_value = NULL, *last_value = NULL;

	for (i = 0; i < 32; i++) {
		if ((hmu_entry->watch_bitmask >> i) & 0x01) {
			curr_value = get_value_from_type(hmu_entry->value_mask,
					(0x1 << i));
			if (curr_value != NULL) {
				if (last_value == NULL) {
					last_value = new_value = cs_malloc(
						sizeof(cs_core_hmu_value_t),
						GFP_ATOMIC);
					if (last_value == NULL)
						goto EXIT_FREE_ALL;
				} else {
					last_value->next = cs_malloc(
						sizeof(cs_core_hmu_value_t),
						GFP_ATOMIC);
					if (last_value->next == NULL)
						goto EXIT_FREE_ALL;
					last_value = last_value->next;
				}
				memcpy(last_value, curr_value,
						sizeof(cs_core_hmu_value_t));
				last_value->next = NULL;
			}
		}
	}
	return new_value;
EXIT_FREE_ALL:
	if (new_value != NULL) {
		do {
			curr_value = new_value->next;
			cs_free(new_value);
			new_value = curr_value;
		} while (new_value != NULL);
	}
	return NULL;
}

/* this internal free function only free value_mask, mask_in_str, and this
 * only data structure, but it doesn't free callback function list */
static void free_core_hmu_int(cs_core_hmu_int_t *int_core_hmu)
{
	cs_core_hmu_value_t *v, *tmp;

	/* free internal data structure */
	v = int_core_hmu->value_mask;
	while (v != NULL) {
		tmp = v->next;
		cs_free(v);
		v = tmp;
	}

	if (int_core_hmu->mask_in_str != NULL)
		cs_free(int_core_hmu->mask_in_str);

	cs_free(int_core_hmu);
}

static int insert_new_core_hmu_to_list(cs_core_hmu_t *hmu_entry,
		cs_hmu_table_t *hmu_table)
{
	cs_core_hmu_int_t *curr, *last = NULL;
	cs_core_hmu_callback_list_t *cbk = NULL;

	if ((hmu_table == NULL) || (hmu_entry == NULL)) {
		DBG(PRINT("invalid parameters!\n"));
		return -EINVAL;
	}

	curr = core_hmu_list;
	while (curr) {
		last = curr;
		curr = curr->next;
	}

	/* create new internal HMU table node */
	curr = (cs_core_hmu_int_t *) cs_zalloc(sizeof(cs_core_hmu_int_t),
			GFP_ATOMIC);
	if (curr == NULL) {
		DBG(PRINT("internal core HMU table allocation failed\n"));
		return -ENOMEM;
	}

	curr->watch_bitmask = hmu_entry->watch_bitmask;
	curr->str_size = watch_bitmask_to_size(curr->watch_bitmask);
	curr->value_mask = copy_hmu_value(hmu_entry);
	if ((hmu_entry->value_mask != NULL) && (curr->value_mask == NULL)) {
		DBG(PRINT("Copy HMU Value failed!\n"));
		free_core_hmu_int(curr);
		return -ENOMEM;
	}
	curr->mask_in_str = encode_core_hmu_value_to_key(curr->watch_bitmask,
			curr->value_mask, true);
	if (curr->mask_in_str == NULL) {
		DBG(PRINT("Create unsigned char string failed!\n"));
		free_core_hmu_int(curr);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&curr->callback_func_list);
	curr->callback_func_cnt = 0;
	if (hmu_entry->callback) {
		cbk = (cs_core_hmu_callback_list_t *) cs_malloc(
				sizeof(cs_core_hmu_callback_list_t),
				GFP_ATOMIC);
		if (cbk == NULL) {
			DBG(PRINT("callback list allocation failed!\n"));
			free_core_hmu_int(curr);
			return -ENOMEM;
		}
		cbk->callback = hmu_entry->callback;
		list_add_tail(&cbk->node, &curr->callback_func_list);
		curr->callback_func_cnt++;
	}
	curr->hmu_table = hmu_table;

	if (last) {
		last->next = curr;
		curr->prev = last;
	} else {
		core_hmu_list = curr;
	}
	return 0;
} /* insert_new_core_hmu_to_list */

static int remove_core_hmu_from_list(cs_core_hmu_int_t *curr_int_core_hmu)
{
	if (curr_int_core_hmu == core_hmu_list) {
		core_hmu_list = core_hmu_list->next;
		core_hmu_list->prev = NULL;
	} else {
		curr_int_core_hmu->prev->next = curr_int_core_hmu->next;
		if (curr_int_core_hmu->next != NULL)
			curr_int_core_hmu->next->prev = curr_int_core_hmu->prev;
	}

	return 0;
} /* remove_core_hmu_from_list */

static cs_core_hmu_int_t *find_core_hmu_node(cs_core_hmu_t *hmu_entry)
{
	cs_core_hmu_int_t *curr;
	u8 *tmp_key;
	int tmp_key_size;

	curr = core_hmu_list;
	tmp_key_size = watch_bitmask_to_size(hmu_entry->watch_bitmask);
	tmp_key = encode_core_hmu_value_to_key(hmu_entry->watch_bitmask,
			hmu_entry->value_mask, true);
	if (tmp_key_size == 0 || tmp_key == NULL) {
		DBG(PRINT("encode hmu_entry->value_mask to key failed. "
			"key_size = %d, key = 0x%p\n", tmp_key_size, tmp_key));
		return NULL;
	}

	while (curr) {
		if ((curr->str_size == tmp_key_size) &&
			(curr->watch_bitmask == hmu_entry->watch_bitmask) &&
			(memcmp(curr->mask_in_str, tmp_key, tmp_key_size) == 0)) {
			cs_free(tmp_key);
			return curr;
		}
		curr = curr->next;
	}
	cs_free(tmp_key);
	return NULL;
} /* find_core_hmu_node */

static cs_core_hmu_int_t *find_int_data_from_table_ptr(cs_hmu_table_t *table)
{
	cs_core_hmu_int_t *curr;

	curr = core_hmu_list;
	while (curr != NULL) {
		if (curr->hmu_table == table)
			return curr;
		curr = curr->next;
	};
	return NULL;
}

static cs_core_hmu_value_t *find_mask_from_int_data(cs_core_hmu_int_t *int_data,
		u32 type)
{
	cs_core_hmu_value_t *curr_mask;

	curr_mask = int_data->value_mask;
	while (curr_mask != NULL) {
		if (curr_mask->type == type)
			return curr_mask;
		curr_mask = curr_mask->next;
	}
	return NULL;
}

static cs_core_hmu_value_t *construct_value_type(u32 type,
		cs_core_hmu_value_t *mask_val, unsigned char *key)
{
	cs_core_hmu_value_t *new_value;
	u16 swid_mod;

	new_value = cs_zalloc(sizeof(cs_core_hmu_value_t), GFP_ATOMIC);
	if (new_value == NULL)
		return NULL;
	switch (type) {
	case CS_CORE_HMU_WATCH_IN_IPV4_SA:
	case CS_CORE_HMU_WATCH_IN_IPV4_DA:
	case CS_CORE_HMU_WATCH_OUT_IPV4_SA:
	case CS_CORE_HMU_WATCH_OUT_IPV4_DA:
		new_value->type = type;
		if (mask_val != NULL)
			new_value->mask = mask_val->mask;
		memcpy(new_value->value.ip_addr, key, key_type_to_size(type));
		new_value->value.ip_addr[0] = htonl(new_value->value.ip_addr[0]);
		break;
	case CS_CORE_HMU_WATCH_IN_IPV6_SA:
	case CS_CORE_HMU_WATCH_IN_IPV6_DA:
	case CS_CORE_HMU_WATCH_OUT_IPV6_SA:
	case CS_CORE_HMU_WATCH_OUT_IPV6_DA:
		new_value->type = type;
		if (mask_val != NULL)
			new_value->mask = mask_val->mask;
		memcpy(new_value->value.ip_addr, key, key_type_to_size(type));
		new_value->value.ip_addr[0] = htonl(new_value->value.ip_addr[0]);
		new_value->value.ip_addr[1] = htonl(new_value->value.ip_addr[1]);
		new_value->value.ip_addr[2] = htonl(new_value->value.ip_addr[2]);
		new_value->value.ip_addr[3] = htonl(new_value->value.ip_addr[3]);
		break;

	case CS_CORE_HMU_WATCH_SWID64:
		if (mask_val == NULL) {
			cs_free(new_value);
			return NULL;
		}
		swid_mod = (u16)CS_SWID64_TO_MOD_ID(mask_val->value.swid64);
		if (swid_mod >= CS_SWID64_MOD_MAX) {
			cs_free(new_value);
			return NULL;
		}
		new_value->type = type;
		new_value->mask = mask_val->mask;
		new_value->value.swid64 = *(((u64 *)key) + swid_mod);
		break;
	default:
		new_value->type = type;
		if (mask_val != NULL)
			new_value->mask = mask_val->mask;
		memcpy(new_value->value.ip_addr, key, key_type_to_size(type));

	}

	return new_value;
}

static cs_core_hmu_value_t *construct_core_hmu_value_from_string(
		cs_core_hmu_int_t *int_data, unsigned char *key)
{
	int i;
	u32 type;
	cs_core_hmu_value_t *ret_value = NULL, *new_value;
	cs_core_hmu_value_t *last_value = NULL, *mask_value;
	unsigned char *curr_key = key;

	if ((int_data == NULL) || (key == NULL))
		return NULL;

	for (i = 0; i < 32; i++) {
		if ((int_data->watch_bitmask >> i) & 0x1) {
			type = 0x1 << i;
			mask_value = find_mask_from_int_data(int_data, type);
			new_value = construct_value_type(type, mask_value,
					curr_key);
			curr_key += key_type_to_size(type);
			if (new_value == NULL)
				goto EXIT_FREE_ALLOCATED;
			if (ret_value == NULL) {
				ret_value = new_value;
				last_value = ret_value;
			} else {
				last_value->next = new_value;
				last_value = new_value;
			}
		}
	}
	return ret_value;
EXIT_FREE_ALLOCATED:
	while (ret_value != NULL) {
		new_value = ret_value->next;
		cs_free(ret_value);
		ret_value = new_value;
	}

	return NULL;
}

static u32 convert_status_from_hmu_to_core(u32 status)
{
	switch (status) {
		case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_TIMEOUT:
			return CS_CORE_HMU_RET_STATUS_TIMEOUT_PARTIAL;
		case CS_HMU_TBL_RET_STATUS_ALL_HASH_TIMEOUT:
			return CS_CORE_HMU_RET_STATUS_TIMEOUT_ALL;
		case CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_DELETED:
			return CS_CORE_HMU_RET_STATUS_DELETED_PARTIAL;
		case CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED:
			return CS_CORE_HMU_RET_STATUS_DELETED_ALL;
		default:
			return 0xffffffff;
	}
}

static int cs_core_hmu_generic_callback(cs_hmu_table_t *table,
	unsigned char *key, u32 status)
{
	cs_core_hmu_value_t *ret_hmu_value, *next_hmu_value;
	cs_core_hmu_int_t *int_data;
	u32 core_hmu_status = convert_status_from_hmu_to_core(status);
	cs_core_hmu_callback_list_t *cb_node, *c;

	if ((table == NULL) || (key == NULL))
		return -1;

	int_data = find_int_data_from_table_ptr(table);
	if (int_data == NULL)
		return -1;

	ret_hmu_value = construct_core_hmu_value_from_string(int_data, key);
	if (ret_hmu_value == NULL)
		return -1;

	list_for_each_entry_safe(cb_node, c, &int_data->callback_func_list,
			node) {
		if (cb_node->callback != NULL)
			cb_node->callback(int_data->watch_bitmask,
					ret_hmu_value, core_hmu_status);
	}

	while (ret_hmu_value != NULL) {
		next_hmu_value = ret_hmu_value->next;
		cs_free(ret_hmu_value);
		ret_hmu_value = next_hmu_value;
	};

	return 0;
}

/* init and exit */
int cs_core_hmu_init(void)
{
	int ret;
	cs_hmu_table_t *hmu_table;

	cs_hmu_init();

	hmu_table = cs_hmu_table_alloc(FWDRSLT_KEY_SIZE, &fwdrslt_hmu_callback);
	if (hmu_table == NULL) {
		DBG(PRINT("FWDRSLT HMU table allocation failed!\n"));
		return -EPERM;
	}
	ret = insert_new_core_hmu_to_list(&fwdrslt_core_hmu, hmu_table);
	if (ret != 0) {
		DBG(PRINT("FWDRSLT Core HMU registration failed!!\n"));
	}

	hmu_table = cs_hmu_table_alloc(QOSRSLT_KEY_SIZE, &qosrslt_hmu_callback);
	if (hmu_table == NULL) {
		DBG(PRINT("QOSRSLT HMU table allocation failed!\n"));
		return -EPERM;
	}
	ret = insert_new_core_hmu_to_list(&qosrslt_core_hmu, hmu_table);
	if (ret != 0) {
		DBG(PRINT("QOSRSLT Core HMU registration failed!!\n"));
		return ret;
	}
	hmu_table = cs_hmu_table_alloc(QOSHASH_KEY_SIZE, &qoshash_hmu_callback);
	if (hmu_table == NULL) {
		DBG(PRINT("QOSHASH HMU table allocation failed!\n"));
		return -EPERM;
	}
	ret = insert_new_core_hmu_to_list(&qoshash_core_hmu, hmu_table);
	if (ret != 0) {
		DBG(PRINT("QOSHASH Core HMU registration failed!!\n"));
		return ret;
	}

	return 0;
} /* cs_core_hmu_init */

void cs_core_hmu_exit(void)
{
	// FIXME!! implement
	cs_hmu_del_all_hash();
} /* cs_core_hmu_exit */

/* for registering HMU */
int cs_core_hmu_register_watch(cs_core_hmu_t *hmu_entry)
{
	cs_core_hmu_int_t *watch = NULL;
	cs_core_hmu_callback_list_t *cbk = NULL;
	cs_hmu_table_t *tbl;

	if (hmu_entry == NULL) {
		DBG(PRINT("invalid HMU entry\n"));
		return -EINVAL;
	}

	watch = find_core_hmu_node(hmu_entry);
	if (watch != NULL) {
		if (hmu_entry->callback) {
			cbk = (cs_core_hmu_callback_list_t *) cs_malloc(
					sizeof(cs_core_hmu_callback_list_t),
					GFP_ATOMIC);
			if (cbk == NULL) {
				DBG(PRINT("callback list allocation fails!\n"));
				return -ENOMEM;
			}
			cbk->callback = hmu_entry->callback;
			list_add_tail(&cbk->node, &watch->callback_func_list);
			watch->callback_func_cnt++;
		}
		return 0;
	}

	tbl = cs_hmu_table_alloc(
			watch_bitmask_to_size(hmu_entry->watch_bitmask),
			&cs_core_hmu_generic_callback);
	if (tbl == NULL) {
		DBG(PRINT("HMU table allocation failed!\n"));
		return -EINVAL;
	}

	return insert_new_core_hmu_to_list(hmu_entry, tbl);
} /* cs_core_hmu_register_watch */

/* for unregistering HMU */
int cs_core_hmu_unregister_watch(cs_core_hmu_t *hmu_entry)
{
	cs_core_hmu_int_t *curr = NULL;
	cs_core_hmu_callback_list_t *cb_node, *c;

	if (hmu_entry == NULL) {
		DBG(PRINT("invalid HMU entry\n"));
		return -EINVAL;
	}

	curr = find_core_hmu_node(hmu_entry);
	if (curr == NULL) {
		DBG(PRINT("can't find HMU entry 0x%p\n", hmu_entry));
		return -EINVAL;
	}

	list_for_each_entry_safe(cb_node, c, &curr->callback_func_list, node) {
		if (cb_node->callback == hmu_entry->callback) {
			list_del(&cb_node->node);
			cs_free(cb_node);
			curr->callback_func_cnt--;
		}
	}

	if (curr->callback_func_cnt == 0) {
		/* don't need the HMU table anymore */
		cs_hmu_table_free(curr->hmu_table);

		remove_core_hmu_from_list(curr);
		free_core_hmu_int(curr);
	}

	return 0;
} /* cs_core_hmu_unregister_watch */

/* clean current SRC entries in HMU */
int cs_core_hmu_clean_watch(cs_core_hmu_t *hmu_entry)
{
	cs_core_hmu_int_t *curr = NULL;

	if (hmu_entry == NULL) {
		DBG(PRINT("invalid HMU entry\n"));
		return -EINVAL;
	}

	curr = find_core_hmu_node(hmu_entry);
	if (curr == NULL) {
		DBG(PRINT("can't find HMU entry 0x%p\n", hmu_entry));
		return -EINVAL;
	}

	/* delete all src entries in HMU table */

	return cs_hmu_table_clean(curr->hmu_table);
}
/* add the hash */
int cs_core_hmu_add_hash(u32 hash_idx, unsigned long life_time, void *data)
{
	return cs_hmu_add_hash(hash_idx, life_time, data);
} /* cs_core_hmu_add_hash */

/* delete hash based on the given value */
int cs_core_hmu_delete_hash(u32 watch_bitmask, cs_core_hmu_value_t *value)
{
	cs_core_hmu_t hmu_entry;
	cs_core_hmu_int_t *curr;
	u8 *key;
	int ret;

	hmu_entry.watch_bitmask = watch_bitmask;
	hmu_entry.value_mask = value;

	curr = find_core_hmu_node(&hmu_entry);
	if (curr == NULL) {
		DBG(PRINT("Can't find corresponding internal core hmu entry\n"
			"watch_bitmask = %u, value ptr = 0x%p\n",
			watch_bitmask, value));
		return -EPERM;
	}
	if (curr->hmu_table == NULL) {
		DBG(PRINT("How can an existing Core HMU without HMU table!\n"));
		return -EPERM;
	}

	key = encode_core_hmu_value_to_key(watch_bitmask, value, false);
	if (key == NULL) {
		DBG(PRINT("encode hmu_entry->value_mask to key failed.\n"));
		return -EPERM;
	}
	ret = cs_hmu_del_hash_by_src(curr->hmu_table, (unsigned char *)key);
	cs_free(key);

	return ret;
} /* cs_core_hmu_delete_hash */

/* delete the hash by hash_idx */
int cs_core_hmu_delete_hash_by_idx(u32 hash_idx)
{
	return cs_hmu_del_hash_by_idx(hash_idx);
} /* cs_core_hmu_delete_hash_by_idx */

/* obtain the last used jiffies based on the given value */
int cs_core_hmu_get_last_use(u32 watch_bitmask, cs_core_hmu_value_t *value,
		unsigned long *time_in_jiffies)
{
	cs_core_hmu_int_t *curr;
	cs_core_hmu_t hmu_entry;
	u8 *key;
	int ret;

	if ((value == NULL) || (time_in_jiffies == NULL))
		return -1;

	hmu_entry.watch_bitmask = watch_bitmask;
	hmu_entry.value_mask = value;
	curr = find_core_hmu_node(&hmu_entry);
	if (curr == NULL) {
		DBG(PRINT("Unable to find int HMU node\n"));
		return -EPERM;
	}

	if (curr->hmu_table == NULL) {
		DBG(PRINT("How can an existing Core HMU without HMU table!\n"));
		return -EPERM;
	}

	key = encode_core_hmu_value_to_key(watch_bitmask, value, false);
	if (key == NULL) {
		DBG(PRINT("encode hmu_entry->value_mask to key failed.\n"));
		return -EPERM;
	}
	ret = cs_hmu_get_last_use_by_src(curr->hmu_table,
			(unsigned char *)key, time_in_jiffies);
	cs_free(key);

	if (ret) {
		DBG(PRINT("Can't get the last used jiffies, ret = %d\n", ret));
		return -EPERM;
	}

	return 0;
} /* cs_core_hmu_get_last_use */

/* delete all the hashes */
int cs_core_hmu_delete_all_hash(void)
{
	return cs_hmu_del_all_hash();
} /* cs_core_hmu_delete_all_hash */

/* return length of this sub key */
static int masked_cb_to_sub_key(u8 *ptr, cs_kernel_accel_cb_t *cb, u32 type,
		cs_core_hmu_value_t *value_mask, bool check_mask)
{
	u16 mask, mask_mode = 0, mask_real = 0, swid_mod;
	u16 unmask_byte, unmask_int, unmask_bit, unmask_real;
	int i;
	u8 *p8 = (u8 *)ptr;
	u16 *p16 = (u16 *)ptr;
	u32 *p32 = (u32 *)ptr;
	u64 *p64 = (u64 *)ptr;
	u64 curr64;

	if (value_mask != NULL)
		mask = value_mask->mask;
	else
		mask = 0x0;

	if (check_mask == false)
		mask = 0xFFFF;

	if (mask > 0) {
		mask_mode  = mask & 0x8000;
		mask_real  = mask & 0x7FFF;
	}

	switch (type) {
	case CS_CORE_HMU_WATCH_FWDRSLT:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("Forward result shouldn't have mask!!\n"));

		if (cb->hw_rslt.has_fwdrslt == 0) {
			DBG(PRINT("control block doesn't have FWDRSLT IDX!\n"));
			return -1;
		}

		if (check_mask == true)
			*p16 = 0x0000;
		else
			*p16 = cb->hw_rslt.fwdrslt_idx;
		return 2;
	case CS_CORE_HMU_WATCH_QOSRSLT:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("QoS result shouldn't have mask!!\n"));

		if (cb->hw_rslt.has_qosrslt == 0)
			return -1;

	if (check_mask == true)
			*p16 = 0x0000;
		else
			*p16 = cb->hw_rslt.qosrslt_idx;
		return 2;
	case CS_CORE_HMU_WATCH_QOSHASH:
		/* QoS hash linkage should be called with another API */
		return -1;
	case CS_CORE_HMU_WATCH_IN_MAC_SA:
		if (mask_real == 0) {
			memset(ptr, 0, 6);
			return 6;
		}

		for (i = 0; i < 6; i++)
			ptr[i] = cb->input.raw.sa[5 - i];
		DBG(PRINT("create IN MAC SA with address: 0x%02x."
					"%02x.%02x.%02x.%02x.%02x\n", ptr[0],
					ptr[1], ptr[2], ptr[3], ptr[4],
					ptr[5]));

		if (mask_real >= (8 * 6))
			return 6;

		unmask_real  = (8 * 6) - mask_real;
		unmask_byte = unmask_real >> 3;
		unmask_bit  = unmask_real & 0x0007;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_byte; i++)
				ptr[i] = 0;
			ptr[unmask_byte] &= (0xFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_byte; i++)
				ptr[5 - i] = 0;
			ptr[5 - unmask_byte] &= (0xFF << unmask_bit);
		}
		return 6;
	case CS_CORE_HMU_WATCH_IN_MAC_DA:
		if (mask_real == 0) {
			memset(ptr, 0, 6);
			return 6;
		}

		for (i = 0; i < 6; i++)
			ptr[i] = cb->input.raw.da[5 - i];

		if (mask_real >= (8 * 6))
			return 6;

		unmask_real  = (8 * 6) - mask_real;
		unmask_byte = unmask_real >> 3;
		unmask_bit  = unmask_real & 0x0007;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_byte; i++)
				ptr[i] = 0;
			ptr[unmask_byte] &= (0xFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_byte; i++)
				ptr[5 - i] = 0;
			ptr[5 - unmask_byte] &= (0xFF << unmask_bit);
		}
		return 6;
	case CS_CORE_HMU_WATCH_OUT_MAC_SA:
		if (mask_real == 0) {
			memset(ptr, 0, 6);
			return 6;
		}

		for (i = 0; i < 6; i++)
			ptr[i] = cb->output.raw.sa[5 - i];

		if (mask_real >= (8 * 6))
			return 6;

		unmask_real  = (8 * 6) - mask_real;
		unmask_byte = unmask_real >> 3;
		unmask_bit  = unmask_real & 0x0007;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_byte; i++)
				ptr[i] = 0;
			ptr[unmask_byte] &= (0xFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_byte; i++)
				ptr[5 - i] = 0;
			ptr[5 - unmask_byte] &= (0xFF << unmask_bit);
		}
		return 6;
	case CS_CORE_HMU_WATCH_OUT_MAC_DA:
		if (mask_real == 0) {
			memset(ptr, 0, 6);
			return 6;
		}

		for (i = 0; i < 6; i++)
			ptr[i] = cb->output.raw.da[5 - i];

		DBG(PRINT("create OUT MAC DA with address: 0x%02x."
					"%02x.%02x.%02x.%02x.%02x\n", ptr[0],
					ptr[1], ptr[2], ptr[3], ptr[4],
					ptr[5]));
		if (mask_real >= (8 * 6))
			return 6;

		unmask_real  = (8 * 6) - mask_real;
		unmask_byte = unmask_real >> 3;
		unmask_bit  = unmask_real & 0x0007;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_byte; i++)
				ptr[i] = 0;
			ptr[unmask_byte] &= (0xFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_byte; i++)
				ptr[5 - i] = 0;
			ptr[5 - unmask_byte] &= (0xFF << unmask_bit);
		}
		return 6;
	case CS_CORE_HMU_WATCH_IN_VID1:
		if (cb->input.raw.vlan_tpid == 0)
			return -1;

		if (mask_real == 0) {
			memset(ptr, 0, 2);
			return 2;
		}

		*p16 = cb->input.raw.vlan_tci & VLAN_VID_MASK;
		if (mask_real >= (8 * 2))
			return 2;

		unmask_real = (8 * 2) - mask_real;
		if (mask_mode > 0)
			*p16 &= (0xFFFF >> unmask_real);
		else
			*p16 &= (0xFFFF << unmask_real);
		return 2;
	case CS_CORE_HMU_WATCH_OUT_VID1:
		if (cb->output.raw.vlan_tpid == 0)
			return -1;

		if (mask_real == 0) {
			memset(ptr, 0, 2);
			return 2;
		}

		*p16 = cb->output.raw.vlan_tci& VLAN_VID_MASK;
		if (mask_real >= (8 * 2)) {
			return 2;
		}

		unmask_real = (8 * 2) - mask_real;
		if (mask_mode > 0)
			*p16 &= (0xFFFF >> unmask_real);
		else
			*p16 &= (0xFFFF << unmask_real);
		return 2;
	case CS_CORE_HMU_WATCH_IN_VID2:
		// TODO! doesn't support double tagged VLAN yet
		return -1;
	case CS_CORE_HMU_WATCH_OUT_VID2:
		// TODO! doesn't support double tagged VLAN yet
		return -1;
	case CS_CORE_HMU_WATCH_PPPOE_ID:
		if (!(cb->common.module_mask & CS_MOD_MASK_PPPOE))
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 2);
			return 2;
		}

		if (cb->action.l2.pppoe_op_en == CS_PPPOE_OP_INSERT) {
			*p16 = ntohs(cb->output.raw.pppoe_frame);
		} else if (cb->action.l2.pppoe_op_en == CS_PPPOE_OP_REMOVE) {
			*p16 = ntohs(cb->input.raw.pppoe_frame);
		} else {
			memset(ptr, 0, 2);
			return 2;
		}

		if (mask_real >= (8 * 2)) {
			return 2;
		}

		unmask_real = (8 * 2) - mask_real;
		if (mask_mode > 0)
			*p16 &= (0xFFFF >> unmask_real);
		else
			*p16 &= (0xFFFF << unmask_real);
		return 2;
	case CS_CORE_HMU_WATCH_IN_IPV4_SA:
	      if (cb->input.l3_nh.iph.ver != 4)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 4);
			return 4;
		}

		*p32 = ntohl(cb->input.l3_nh.iph.sip);
		if (mask_real >= (8 * 4))
			return 4;

		unmask_real = (8 * 4) - mask_real;
		if (mask_mode > 0)
			*p32 &= (0xFFFFFFFF >> unmask_real);
		else
			*p32 &= (0xFFFFFFFF << unmask_real);
		return 4;
	case CS_CORE_HMU_WATCH_IN_IPV4_DA:
		if (cb->input.l3_nh.iph.ver != 4)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 4);
			return 4;
		}

		*p32 = ntohl(cb->input.l3_nh.iph.dip);
		if (mask_real >= (8 * 4))
			return 4;

		unmask_real = (8 * 4) - mask_real;
		if (mask_mode > 0)
			*p32 &= (0xFFFFFFFF >> unmask_real);
		else
			*p32 &= (0xFFFFFFFF << unmask_real);
		return 4;
	case CS_CORE_HMU_WATCH_OUT_IPV4_SA:
		if (cb->output.l3_nh.iph.ver != 4)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 4);
			return 4;
		}

		*p32 = ntohl(cb->output.l3_nh.iph.sip);
		if (mask_real >= (8 * 4))
			return 4;

		unmask_real = (8 * 4) - mask_real;
		if (mask_mode > 0)
			*p32 &= (0xFFFFFFFF >> unmask_real);
		else
			*p32 &= (0xFFFFFFFF << unmask_real);
		return 4;
	case CS_CORE_HMU_WATCH_OUT_IPV4_DA:
		if (cb->output.l3_nh.iph.ver != 4)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 4);
			return 4;
		}

		*p32 = ntohl(cb->output.l3_nh.iph.dip);
		if (mask_real >= (8 * 4))
			return 4;

		unmask_real = (8 * 4) - mask_real;
		if (mask_mode > 0)
			*p32 &= (0xFFFFFFFF >> unmask_real);
		else
			*p32 &= (0xFFFFFFFF << unmask_real);
		return 4;
	case CS_CORE_HMU_WATCH_IN_IPV6_SA:
		if (cb->input.l3_nh.ipv6h.ver != 6)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 16);
			return 16;
		}

		p32[0] = ntohl(cb->input.l3_nh.ipv6h.sip[0]);
		p32[1] = ntohl(cb->input.l3_nh.ipv6h.sip[1]);
		p32[2] = ntohl(cb->input.l3_nh.ipv6h.sip[2]);
		p32[3] = ntohl(cb->input.l3_nh.ipv6h.sip[3]);
		if (mask_real >= (8 * 16))
			return 16;

		unmask_real = (8 * 16) - mask_real;
		unmask_int = unmask_real >> 5;
		unmask_bit = unmask_real & 0x001F;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_int; i++)
				p32[i] = 0;
			p32[unmask_int] &= (0xFFFFFFFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_int; i++)
				p32[3 - i] = 0;
			p32[3 - unmask_int] &= (0xFFFFFFFF << unmask_bit);
		}
		return 16;
	case CS_CORE_HMU_WATCH_IN_IPV6_DA:
		if (cb->input.l3_nh.ipv6h.ver != 6)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 16);
			return 16;
		}

		p32[0] = ntohl(cb->input.l3_nh.ipv6h.dip[0]);
		p32[1] = ntohl(cb->input.l3_nh.ipv6h.dip[1]);
		p32[2] = ntohl(cb->input.l3_nh.ipv6h.dip[2]);
		p32[3] = ntohl(cb->input.l3_nh.ipv6h.dip[3]);
		if (mask_real >= (8 * 16))
			return 16;

		unmask_real = (8 * 16) - mask_real;
		unmask_int = unmask_real >> 5;
		unmask_bit = unmask_real & 0x001F;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_int; i++)
				p32[i] = 0;
			p32[unmask_int] &= (0xFFFFFFFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_int; i++)
				p32[3 - i] = 0;
			p32[3 - unmask_int] &= (0xFFFFFFFF << unmask_bit);
		}
		return 16;
	case CS_CORE_HMU_WATCH_OUT_IPV6_SA:
		if (cb->output.l3_nh.ipv6h.ver != 6)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 16);
			return 16;
		}

		p32[0] = ntohl(cb->output.l3_nh.ipv6h.sip[0]);
		p32[1] = ntohl(cb->output.l3_nh.ipv6h.sip[1]);
		p32[2] = ntohl(cb->output.l3_nh.ipv6h.sip[2]);
		p32[3] = ntohl(cb->output.l3_nh.ipv6h.sip[3]);
		if (mask_real >= (8 * 16))
			return 16;

		unmask_real = (8 * 16) - mask_real;
		unmask_int = unmask_real >> 5;
		unmask_bit = unmask_real & 0x001F;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_int; i++)
				p32[i] = 0;
			p32[unmask_int] &= (0xFFFFFFFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_int; i++)
				p32[3 - i] = 0;
			p32[3 - unmask_int] &= (0xFFFFFFFF << unmask_bit);
		}
		return 16;
	case CS_CORE_HMU_WATCH_OUT_IPV6_DA:
		if (cb->output.l3_nh.ipv6h.ver != 6)
			return -1;
		if (mask_real == 0) {
			memset(ptr, 0, 16);
			return 16;
		}

		p32[0] = ntohl(cb->output.l3_nh.ipv6h.dip[0]);
		p32[1] = ntohl(cb->output.l3_nh.ipv6h.dip[1]);
		p32[2] = ntohl(cb->output.l3_nh.ipv6h.dip[2]);
		p32[3] = ntohl(cb->output.l3_nh.ipv6h.dip[3]);

		if (mask_real >= (8 * 16))
			return 16;

		unmask_real = (8 * 16) - mask_real;
		unmask_int = unmask_real >> 5;
		unmask_bit = unmask_real & 0x001F;
		if (mask_mode > 0) {
			for (i = 0; i < unmask_int; i++)
				p32[i] = 0;
			p32[unmask_int] &= (0xFFFFFFFF >> unmask_bit);
		} else {
			for (i = 0; i < unmask_int; i++)
				p32[3 - i] = 0;
			p32[3 - unmask_int] &= (0xFFFFFFFF << unmask_bit);
		}
		return 16;
	case CS_CORE_HMU_WATCH_IP_PROT:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("IP PROT mask value shouldn't have mask!\n"));

		if (check_mask == true)
			*p8 = 0;
		else
			*p8 = (u8) cb->input.l3_nh.iph.protocol;
		DBG(PRINT("create IP_PROT with protocol: %u (0x%02x).", *p8, *p8));
		return 1;
	case CS_CORE_HMU_WATCH_SRC_PHY_PORT:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("Port value shouldn't have mask!!\n"));
		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = (u16) cb->common.ingress_port_id;
		return 2;
	case CS_CORE_HMU_WATCH_DST_PHY_PORT:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("Port value shouldn't have mask!!\n"));
		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = (u16) cb->common.egress_port_id;
		return 2;
	case CS_CORE_HMU_WATCH_SWID64:
		if (value_mask == NULL)
			return -1;

		memset(ptr, 0xFF, CS_SWID64_MOD_MAX << 3);
		if (mask_real == 0)
			return (CS_SWID64_MOD_MAX << 3);

		curr64 = value_mask->value.swid64;
		swid_mod = (u16)CS_SWID64_TO_MOD_ID(curr64);
		if (swid_mod >= CS_SWID64_MOD_MAX)
			return -1;

		p64[swid_mod] = cb->common.swid[swid_mod];

		if (mask_real >= (8 * 8))
			return (CS_SWID64_MOD_MAX << 3);

		unmask_real = (8 * 8) - mask_real;
		if (mask_mode > 0) {
			p64[swid_mod] &= (0xFFFFFFFFFFFFFFFF >> unmask_real);
		} else {
			p64[swid_mod] &= (0xFFFFFFFFFFFFFFFF << unmask_real);
		}
		return (CS_SWID64_MOD_MAX << 3);
	case CS_CORE_HMU_WATCH_IN_L4_SP:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("L4 PORT mask value shouldn't have mask!\n"));

		if ((cb->input.l3_nh.iph.protocol != SOL_TCP)
				&& (cb->input.l3_nh.iph.protocol != SOL_UDP))
			return -1;

		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = (u16) cb->input.l4_h.uh.sport;
		DBG(PRINT("create IN_L4_SP with port: %u (0x%02x).\n", ntohs(*p16), ntohs(*p16)));
		return 2;
	case CS_CORE_HMU_WATCH_IN_L4_DP:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("L4 PORT mask value shouldn't have mask!\n"));

		if ((cb->input.l3_nh.iph.protocol != SOL_TCP)
				&& (cb->input.l3_nh.iph.protocol != SOL_UDP))
			return -1;

		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = (u16) cb->input.l4_h.uh.dport;
		DBG(PRINT("create IN_L4_DP with port: %u (0x%02x).\n", ntohs(*p16), ntohs(*p16)));
		return 2;
	case CS_CORE_HMU_WATCH_OUT_L4_SP:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("L4 PORT mask value shouldn't have mask!\n"));

		if ((cb->output.l3_nh.iph.protocol != SOL_TCP)
				&& (cb->output.l3_nh.iph.protocol != SOL_UDP))
			return -1;

		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = (u16) cb->output.l4_h.uh.sport;
		DBG(PRINT("create OUT_L4_SP with port: %u (0x%02x).\n", ntohs(*p16), ntohs(*p16)));
		return 2;
	case CS_CORE_HMU_WATCH_OUT_L4_DP:
		/* shouldn't have mask */
		if ((mask != 0) && (mask != 0xFFFF))
			DBG(PRINT("L4 PORT mask value shouldn't have mask!\n"));

		if ((cb->output.l3_nh.iph.protocol != SOL_TCP)
				&& (cb->output.l3_nh.iph.protocol != SOL_UDP))
			return -1;

		if (check_mask == true)
			*p16 = 0;
		else
			*p16 = (u16) cb->output.l4_h.uh.dport;
		DBG(PRINT("create OUT_L4_DP with port: %u (0x%02x).\n", ntohs(*p16), ntohs(*p16)));
		return 2;
	default:
		return 0;
	}
}

/* return pointer of key,
 * value_mask is optional, if it's NULL, then it will generate unsigned char
 * string without applying mask. */
static u8 *encode_cb_to_key(cs_kernel_accel_cb_t *cb, u32 watch_bitmask,
		cs_core_hmu_value_t *value_mask, bool check_mask)
{
	int i, type, len;
	int offset, ret;
	u8 *key;
	cs_core_hmu_value_t *curr;

	if (cb == NULL) {
		DBG(PRINT("invalid cb ptr\n"));
		return NULL;
	}

	curr = value_mask;
	len = watch_bitmask_to_size(watch_bitmask);
	if (len <= 0) {
		DBG(PRINT("invalid key length, watch_bitmask = 0x%08X\n",
					watch_bitmask));
		return NULL;
	}

	key = (u8 *) cs_zalloc(len, GFP_ATOMIC);
	if (key == NULL) {
		DBG(PRINT("No memory for key, len = %d\n", len));
		return NULL;
	}

	/* start from the lowest bit */
	for (i = 0, offset = 0; i < 32; i++) {
		type = 0x1 << i;
		if (watch_bitmask & type) {
			if (value_mask == NULL) {
				ret = masked_cb_to_sub_key(key + offset,
						cb, type, NULL, check_mask);
				if (ret <= 0) {
					cs_free(key);
					return NULL;
				}
				offset += ret;
			} else {
				/*
				 * assume value_mask is retrived from
				 * cs_core_hmu_int_t, and it should be
				 * already sorted by type.
				 */
				if (curr != NULL && curr->type == type) {
					ret = masked_cb_to_sub_key(
							key + offset, cb, type,
							curr, check_mask);
					if (ret <= 0) {
						cs_free(key);
						return NULL;
					}
					offset += ret;
					curr = curr->next;
				} else {
					/* we know (curr->type > type) here */
					ret = masked_cb_to_sub_key(
							key + offset, cb, type,
							NULL, check_mask);
					if (ret <= 0) {
						cs_free(key);
						return NULL;
					}
					offset += ret;
				}
			}
		}
	}
	return key;
}

static bool check_cs_cb_vs_core_hmu(cs_kernel_accel_cb_t *cs_cb,
		cs_core_hmu_int_t *curr)
{
	u8 *cb_key;
	int key_size;
	/* we can simplify the checking by
	 * 1) construct unsigned char string from cs_cb and bitmask for
	 * core_hmu_int_t
	 * 2) then compare the unsigned char stringS
	 */
	key_size = watch_bitmask_to_size(curr->watch_bitmask);
	cb_key = encode_cb_to_key(cs_cb, curr->watch_bitmask, curr->value_mask,
			true);
	if (key_size == 0 || cb_key == NULL) {
		//DBG(PRINT("Can't encode key from cs_cb. bitmask = 0x%x key = "
		//		"0x%p, size = %d\n", curr->watch_bitmask,
		//		cb_key, key_size));
		if (cb_key)
			cs_free(cb_key);
		return false;
	}

	if (memcmp(curr->mask_in_str, cb_key, key_size) == 0) {
		cs_free(cb_key);
		return true;
	}

	cs_free(cb_key);
	return false;
}

int cs_core_hmu_set_result_idx(u32 hash_idx, u32 result_idx, u8 result_type)
{
	return cs_hmu_set_result_idx(hash_idx, result_idx, result_type);
}

/* link HMU SRC based on info given in cb to hash of hash_idx */
int cs_core_hmu_link_src_and_hash(cs_kernel_accel_cb_t *cb, u32 hash_idx, void *data)
{
	cs_core_hmu_int_t *curr;
	u8 *key;
	int ret;

	if (cb == NULL)
		return -1;

	curr = core_hmu_list;
	while (curr) {
		if ((curr->hmu_table != NULL) &&
				(check_cs_cb_vs_core_hmu(cb, curr) == true)) {
			key = encode_cb_to_key(cb, curr->watch_bitmask,
					curr->value_mask, false);
			if (key == NULL) {
				DBG(PRINT("Failed to generate key value for "
							"linkage creation!\n"));
			} else {
				ret = cs_hmu_create_link_src_and_hash(
						curr->hmu_table, key, hash_idx, data);
				if (ret != 0)
					DBG(PRINT("Failed to link SRC and "
								"hash\n"));
				cs_free(key);
			}
		}
		curr = curr->next;
	}

	return 0;
} /* cs_core_hmu_link_src_and_hash */

static void do_callback(cs_core_hmu_int_t *int_data, cs_kernel_accel_cb_t *cb,
		u32 status)
{
	u8 *key;
	cs_core_hmu_value_t *ret_hmu_value, *next_hmu_value;
	cs_core_hmu_callback_list_t *cb_node, *c;

	key = encode_cb_to_key(cb, int_data->watch_bitmask,
			int_data->value_mask, false);
	if (key == NULL)
		return;

	ret_hmu_value = construct_core_hmu_value_from_string(int_data, key);
	if (ret_hmu_value == NULL) {
		cs_free(key);
		return;
	}

	list_for_each_entry_safe(cb_node, c, &int_data->callback_func_list,
			node) {
		if (cb_node->callback != NULL)
			cb_node->callback(int_data->watch_bitmask,
					ret_hmu_value, status);
	}

	while (ret_hmu_value != NULL) {
		next_hmu_value = ret_hmu_value->next;
		cs_free(ret_hmu_value);
		ret_hmu_value = next_hmu_value;
	};

	cs_free(key);

	return;
}

/* callback for hash creation success */
int cs_core_hmu_callback_for_hash_creation_pass(cs_kernel_accel_cb_t *cb)
{
	cs_core_hmu_int_t *curr;

	if (cb == NULL)
		return -1;

	curr = core_hmu_list;
	while (curr) {
		if ((curr->hmu_table != NULL) &&
				(check_cs_cb_vs_core_hmu(cb, curr) == true))
			do_callback(curr, cb,
					CS_CORE_HMU_RET_STATUS_CREATE_SUCCEED);
		curr = curr->next;
	}

	return 0;
} /* cs_core_hmu_callback_for_hash_creation_pass */

/* callback for hash creation failure */
int cs_core_hmu_callback_for_hash_creation_fail(cs_kernel_accel_cb_t *cb)
{
	cs_core_hmu_int_t *curr;

	if (cb == NULL)
		return -1;

	curr = core_hmu_list;
	while (curr) {
		if ((curr->hmu_table != NULL) &&
				(check_cs_cb_vs_core_hmu(cb, curr) == true))
			do_callback(curr, cb,
					CS_CORE_HMU_RET_STATUS_CREATE_FAIL);
		curr = curr->next;
	}

	return 0;
} /* cs_core_hmu_callback_for_hash_creation_fail */

/* link qos_hash_idx and fwd_hash_idx */
int cs_core_hmu_link_fwd_and_qos_hash(u32 fwd_hash_idx, u32 qos_hash_idx, bool need_watch)
{
	cs_core_hmu_int_t *curr;
	u8 *key;
	u32 *p32;
	int ret;

	cs_hmu_set_qoshash_idx(fwd_hash_idx, qos_hash_idx);

	if (!need_watch) {
		DBG(printk("%s: No need to be watched!!\n", __func__);)
		return 0;
	}

	/* get the core_hmu_table for QOSHASH */
	curr = core_hmu_list;
	while (curr) {
		if ((curr->hmu_table != NULL) &&
				(curr->watch_bitmask ==
				 CS_CORE_HMU_WATCH_QOSHASH))
			break;
		curr = curr->next;
	}

	if (curr == NULL)
		return -1;

	/* link the key and fwd_hash_idx */
	ret = cs_hmu_create_link_src_and_hash(curr->hmu_table, (u8 *) &qos_hash_idx,
			fwd_hash_idx, (void *) 0xffffffff);
	if (ret != 0)
		DBG(PRINT("Fail to link QoS hash idx %x and FWD hash idx %x\n",
					qos_hash_idx, fwd_hash_idx));

	return 0;
} /* cs_core_hmu_link_fwd_and_qos_hash */

/*************** Debug API *************/
/**********under construction******************/
static char* get_type_name(u32 type, cs_core_hmu_value_t *value_mask)
{
	switch (type) {
	case CS_CORE_HMU_WATCH_FWDRSLT:
		return "FWDRSLT";
	case CS_CORE_HMU_WATCH_QOSRSLT:
		return "QOSRSLT";
	case CS_CORE_HMU_WATCH_QOSHASH:
		return "QOSHASH";
	case CS_CORE_HMU_WATCH_IN_MAC_SA:
		return "IN_MAC_SA";
	case CS_CORE_HMU_WATCH_IN_MAC_DA:
		return "IN_MAC_DA";
	case CS_CORE_HMU_WATCH_OUT_MAC_SA:
		return "OUT_MAC_SA";
	case CS_CORE_HMU_WATCH_OUT_MAC_DA:
		return "OUT_MAC_DA";
	case CS_CORE_HMU_WATCH_IN_VID1:
		return "IN_VID1";
	case CS_CORE_HMU_WATCH_OUT_VID1:
		return "OUT_VID1";
	case CS_CORE_HMU_WATCH_IN_VID2:
		return "IN_VID2";
	case CS_CORE_HMU_WATCH_OUT_VID2:
		return "OUT_VID2";
	case CS_CORE_HMU_WATCH_PPPOE_ID:
		return "PPPOE_ID";
	case CS_CORE_HMU_WATCH_IN_IPV4_SA:
		return "IN_IPV4_SA";
	case CS_CORE_HMU_WATCH_IN_IPV4_DA:
		return "IN_IPV4_DA";
	case CS_CORE_HMU_WATCH_OUT_IPV4_SA:
		return "OUT_IPV4_SA";
	case CS_CORE_HMU_WATCH_OUT_IPV4_DA:
		return "OUT_IPV4_DA";
	case CS_CORE_HMU_WATCH_IN_IPV6_SA:
		return "IN_IPV6_SA";
	case CS_CORE_HMU_WATCH_IN_IPV6_DA:
		return "IN_IPV6_DA";
	case CS_CORE_HMU_WATCH_OUT_IPV6_SA:
		return "OUT_IPV6_SA";
	case CS_CORE_HMU_WATCH_OUT_IPV6_DA:
		return "OUT_IPV6_DA";
	case CS_CORE_HMU_WATCH_IP_PROT:
		return "IP_PROT";
	case CS_CORE_HMU_WATCH_SRC_PHY_PORT:
		return "SRC_PHY_PORT";
	case CS_CORE_HMU_WATCH_DST_PHY_PORT:
		return "DST_PHY_PORT";
	case CS_CORE_HMU_WATCH_SWID64:
		while (value_mask) {
			if (value_mask->type != CS_CORE_HMU_WATCH_SWID64) {
				value_mask = value_mask->next;
				continue;
			}
			switch (CS_SWID64_TO_MOD_ID(value_mask->value.swid64))
			{
			case CS_SWID64_MOD_ID_IPSEC:
				return "SWID64_IPSEC";
			case CS_SWID64_MOD_ID_BRIDGE:
				return "SWID64_BRIDGE";
			case CS_SWID64_MOD_ID_IPV4_FORWARD:
				return "SWID64_IPV4_FORWARD";
			case CS_SWID64_MOD_ID_IPV6_FORWARD:
				return "SWID64_IPV6_FORWARD";
			case CS_SWID64_MOD_ID_FLOW_POLICER:
				return "SWID64_FLOW_POLICER";
			case CS_SWID64_MOD_ID_NF_DROP:
				return "SWID64_NF_DROP";
			default:
				return "SWID64_UNKNOWN";
			}
		}
		return "SWID64_UNKNOWN";
	case CS_CORE_HMU_WATCH_IN_L4_SP:
		return "IN_L4_SP";
	case CS_CORE_HMU_WATCH_IN_L4_DP:
		return "IN_L4_DP";
	case CS_CORE_HMU_WATCH_OUT_L4_SP:
		return "OUT_L4_SP";
	case CS_CORE_HMU_WATCH_OUT_L4_DP:
		return "OUT_L4_DP";
	default:
		return "UNKNOWN";
	}
}
void cs_core_hmu_dump_table(int num)
{
	cs_core_hmu_int_t *n = core_hmu_list;
	cs_core_hmu_value_t *v;
	cs_core_hmu_callback_list_t *cb_node, *c;
	u32 i = 0, j;
	u32 type, type_cnt;

	while (n) {
		i++;
		if (num > 0)
			if (i < num) {
				n = n->next;
				if (n)
					continue;
				else
					break;
			}
		cond_resched();
		printk("Table #%d ------------------------------------\n", i);

		/* identify table name, start from the lowest bit */
		printk("name = ");
		type_cnt = 0;
		for (j = 0; j < 32; j++) {
			type = 0x1 << j;
			if (n->watch_bitmask & type) {
				type_cnt++;
				if (type_cnt > 1)
					printk(" & ");
				printk("%s", get_type_name(type, n->value_mask));
			}
		}
		printk("\n");

		printk("watch_bitmask = 0x%08X\n", n->watch_bitmask);
		printk("mask_in_str = \t");
		for (j = 0; j < n->str_size; j++) {
			if (j > 0 && (j & 0xf) == 0)
				printk("\n\t\t");
			printk("%02X ", n->mask_in_str[j]);
		}
		printk("\n");
		for (j = 1, v = n->value_mask; v != NULL; j++, v = v->next) {
			printk("type #%d = 0x%08X\n", j, v->type);
			switch (v->type) {
			case CS_CORE_HMU_WATCH_IN_MAC_SA:
			case CS_CORE_HMU_WATCH_IN_MAC_DA:
			case CS_CORE_HMU_WATCH_OUT_MAC_SA:
			case CS_CORE_HMU_WATCH_OUT_MAC_DA:
#if 1
				printk("mac_addr = %pM\n", v->value.mac_addr);
#else
				printk("mac_addr = %02X:%02X:%02X:%02X:%02X:%02X\n",
					v->value.mac_addr[0],
					v->value.mac_addr[1],
					v->value.mac_addr[2],
					v->value.mac_addr[3],
					v->value.mac_addr[4],
					v->value.mac_addr[5]);
#endif
				break;
			case CS_CORE_HMU_WATCH_IN_IPV4_SA:
			case CS_CORE_HMU_WATCH_IN_IPV4_DA:
			case CS_CORE_HMU_WATCH_OUT_IPV4_SA:
			case CS_CORE_HMU_WATCH_OUT_IPV4_DA:
#if 1
				printk("IPv4 = %pI4\n", v->value.ip_addr);
#else
				printk("IPv4 = %u.%u.%u.%u\n",
					((unsigned char *)&v->value.ip_addr[0])[0],
					((unsigned char *)&v->value.ip_addr[0])[1],
					((unsigned char *)&v->value.ip_addr[0])[2],
					((unsigned char *)&v->value.ip_addr[0])[3]);
#endif
				break;
			case CS_CORE_HMU_WATCH_IN_IPV6_SA:
			case CS_CORE_HMU_WATCH_IN_IPV6_DA:
			case CS_CORE_HMU_WATCH_OUT_IPV6_SA:
			case CS_CORE_HMU_WATCH_OUT_IPV6_DA:
				printk("IPv6 = %pI6\n",	v->value.ip_addr);
				break;
			case CS_CORE_HMU_WATCH_IN_VID1:
			case CS_CORE_HMU_WATCH_OUT_VID1:
			case CS_CORE_HMU_WATCH_IN_VID2:
			case CS_CORE_HMU_WATCH_OUT_VID2:
				printk("VID = %u\n", v->value.vid);
				break;
			case CS_CORE_HMU_WATCH_PPPOE_ID:
				printk("PPPoE ID = %u\n", v->value.pppoe_session_id);
				break;
			case CS_CORE_HMU_WATCH_IP_PROT:
				printk("IP Prot = %u\n", v->value.ip_prot);
				break;
			case CS_CORE_HMU_WATCH_SWID64:
				printk("SW ID = 0x%016llx\n", v->value.swid64);
				break;

			case CS_CORE_HMU_WATCH_FWDRSLT:
			case CS_CORE_HMU_WATCH_QOSRSLT:
			case CS_CORE_HMU_WATCH_QOSHASH:
			case CS_CORE_HMU_WATCH_SRC_PHY_PORT:
			case CS_CORE_HMU_WATCH_DST_PHY_PORT:
				printk("Index = %u\n", v->value.index);
				break;
			case CS_CORE_HMU_WATCH_IN_L4_SP:
			case CS_CORE_HMU_WATCH_OUT_L4_SP:
				printk("L4 Src Port = %u\n", v->value.l4_port);
				break;
			case CS_CORE_HMU_WATCH_IN_L4_DP:
			case CS_CORE_HMU_WATCH_OUT_L4_DP:
				printk("L4 Dst Port = %u\n", v->value.l4_port);
				break;
			default:
				printk("Value is not supported yet\n");
			}
			printk("mask #%d = 0x%04X\n", j, v->mask);
		}
		printk("# of callback = %d\n", n->callback_func_cnt);
		j = 1;
		list_for_each_entry_safe(cb_node, c, &n->callback_func_list,
			node)
		printk("callback #%d = 0x%p\n", j++, cb_node->callback);
		cs_hmu_dump_table(n->hmu_table, false);
		n = n->next;
		if (num > 0)
			break;
	}
} /* cs_core_hmu_dump_table */

void cs_core_hmu_dump_all_table(void)
{
	cs_core_hmu_dump_table(0);
}

int cs_hmu_get_hash_by_idx(u32 hash_idx, void **data);
int cs_core_hmu_get_hash_by_idx(u32 hash_idx, void **data)
{
	return cs_hmu_get_hash_by_idx(hash_idx, data);
}

int cs_hmu_get_last_use_by_hash_idx(u32 hash_idx, unsigned long *hash_last_use);
int cs_core_hmu_get_last_use_by_hash_idx(u32 hash_idx, unsigned long *hash_last_use)
{
	return cs_hmu_get_last_use_by_hash_idx(hash_idx, hash_last_use);
}
