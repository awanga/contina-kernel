/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 *
 */

#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <mach/cs75xx_fe_core_table.h>
#include "cs_fe_hash.h"
#include "cs752x_proc.h"
#include "cs_hmu.h"
#include "cs_core_hmu.h"
#include "cs_core_fastnet.h"
#include "cs_fe.h"
#include "cs_mut.h"

#define PFX     "CS_HMU"
#define PRINT(format, args...) printk(KERN_WARNING PFX \
	":%s:%d: " format, __func__, __LINE__ , ## args)

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_ne_core_logic_debug;
#define DBG(x) {if (cs_ne_core_logic_debug & CS752X_CORE_LOGIC_HMU_FW) x;}
#else
#define DBG(x) { }
#endif

typedef struct cs_hmu_src_s {
	/* pointer to original key */
	unsigned char	*key;

	/* the value of key */
	u32	hash_value;

	/* tree node in hmu_src_list */
	struct rb_node hmu_src_node;

	/* tree node in to_be_call_back_list */
	struct list_head to_be_call_back_node;

	/* keep the current reference count by hash entry */
	atomic_t	ref_cnt;

	/* pointer to table */
	cs_hmu_table_t	*hmu_table_ptr;

	/* the list of pointer to hash entry that relates to this HMU SRC */
	struct list_head	hash_list;

	/* pointer to real data, used for rule-based hash */
	//void *data;
} cs_hmu_src_t;

typedef struct cs_hmu_hash_s {
	u32	hash_idx;
	short  rslt_type;
	u32	rslt_idx; /*this could be qosrslt or fwdrslt*/
	u32	link_qoshash_idx;
	u8	state;	/* 0: invalid, 1: valid */

	unsigned long	last_use;
	unsigned long	life_time; /* 0: static, otherwise: limited life time */

	atomic_t	hmu_usr_cnt;

	/* the list of pointer to HMU SRC the watches this hash */
	struct list_head	hmu_src_list;

	/* list node for invalid hash list */
	struct list_head	invalid_hash_node;

	/* pointer to real data, used for rule-based hash and flow hash */
        void *data;
} cs_hmu_hash_t;

typedef struct cs_hmu_ptr_list_s {
	struct list_head	node;
	u32	data;
} cs_hmu_ptr_list_t;

/* global variables */
static cs_hmu_ptr_list_t hmu_tbl_head;
static cs_hmu_hash_t hash_list[CS_HMU_HASH_LIST_SIZE];
static u64 hash_used_mask[FE_HASH_STATUS_ENTRY_MAX];
static spinlock_t hmu_lock;
struct list_head invalid_hash_list;
struct list_head to_be_call_back_list; /* collect HMU SRC entries */

/* internal APIs */
static int __cs_hmu_src_free(cs_hmu_src_t *entry, bool cross_check,
			     bool del_hash);
static int __cs_hmu_del_hash_by_idx(u32 hash_idx, bool f_xcheck);
static void cs_hmu_callback_table_scan(void);
extern cs_status_t cs_flow_hash_delete_data(void *data);

static u32 hash_idx_to_hash_list_idx(u32 hash_idx)
{
	if (IS_FASTNET_ENTRY(hash_idx)) {
		return CS_HMU_HASH_LIST_SIZE_HW_ACCEL + (hash_idx >> 4) ;
	} else {
		if (IS_OVERFLOW_ENTRY(hash_idx))
			return ((HASH_INDEX_SW2HW(hash_idx) & 0x3f) +
					((FE_HASH_STATUS_ENTRY_MAX - 1) << 6));
		else
			return (((hash_idx >> 4) * 6) + (hash_idx & 0x07));
	}
}

static int cs_hmu_delete_accel_hash(u32 hash_idx)
{
	cs_hmu_hash_t *hash_entry;
	u32 idx;
	int ret;

	if (IS_FASTNET_ENTRY(hash_idx))
		return cs_core_fastnet_del_fwd_hash(hash_idx);
	else {
		/* delete fwd_rsult here */
		idx = hash_idx_to_hash_list_idx(hash_idx);
		hash_entry = &hash_list[idx];
		if (hash_entry->rslt_type == 0) { /*fwd_rsult*/
			/* del hash at first to avoid junk pkt */
			ret = cs_fe_hash_del_hash(hash_idx);
			cs_flow_hash_delete_data(hash_entry->data);
			hash_entry->data = NULL;
			
			if (hash_entry->link_qoshash_idx != 0) {
				/*delete QoS reulst and hash*/
				/* del hash at first to avoid junk pkt */
				cs_fe_hash_del_hash(hash_entry->link_qoshash_idx);
				cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT,
							hash_entry->link_qoshash_idx, false);

				/*remove Qos hash watch*/
				__cs_hmu_del_hash_by_idx(hash_entry->link_qoshash_idx, true);
				hash_entry->link_qoshash_idx = 0;
			}
			cs_fe_fwdrslt_del_by_idx(hash_entry->rslt_idx);
			hash_entry->rslt_type = -1;

			return ret;
		}
		return 0;

	}
}

int hmu_get_key_by_hash_index(cs_hmu_table_t *p_hmu_tbl, u32 hash_idx, char *p_key, void **data)
{
	int key_size;
        cs_hmu_src_t *src_entry;
        cs_hmu_hash_t *hash_entry;
	cs_hmu_ptr_list_t *src_node;
	int i;

	spin_lock_bh(&hmu_lock);

	key_size = p_hmu_tbl->key_size;

   	/* locate hash entry in hash table */
        hash_entry = &hash_list[hash_idx_to_hash_list_idx(hash_idx)];
	//printk("%s: hash_idx=%d, hash_entry=0x%x, key_size=%d, hash_entry->state=%d, hash_entry->hmu_src_list=0x%x\n", 
	//	__func__, hash_idx, (unsigned int)hash_entry, key_size, hash_entry->state, (unsigned int)(hash_entry->hmu_src_list));

        if (!hash_entry->state ||
                !list_empty_careful(&hash_entry->invalid_hash_node)) {
                printk("%s: Invalid hash_idx %u\n", __func__, hash_idx);
                spin_unlock_bh(&hmu_lock);
                return -EINVAL;
        } else {
                /* if the entry has been created, we need to check whether
                 * the linkage has been done before. */
		i = 0;
                list_for_each_entry(src_node, &hash_entry->hmu_src_list, node) {
			printk("%s: src_node=0x%x\n", __func__, (unsigned int)src_node);
			printk("%s: src_node->data=0x%x\n", __func__, src_node->data);
			src_entry = (cs_hmu_src_t *)(src_node->data);
			printk("%s: src_entry=0x%x, src_entry->key=0x%x\n", __func__, (unsigned int)src_entry, (unsigned int)(src_entry->key));
			break;
                }
        }
	//debug_Aaron
	//printk("%s: hash_idx=%d, key_size=%d, src_entry->data=0x%x\n", __func__, hash_idx, key_size, src_entry->data);
	memcpy(p_key, src_entry->key, key_size);
	//*data = src_entry->data;
        spin_unlock_bh(&hmu_lock);

	return CS_OK;
}
                                                  
static cs_hmu_src_t *hmu_src_search(cs_hmu_table_t *hmu_tbl, u32 hash_value, char * src_key)
{
	struct rb_node *node = hmu_tbl->hmu_src_list.rb_node;
	cs_hmu_src_t *src_entry;
	int i;
	int key_size;

	while (node) {
		src_entry = rb_entry(node, cs_hmu_src_t, hmu_src_node);

		if (hash_value < src_entry->hash_value)
			node = node->rb_left;
		else if (hash_value > src_entry->hash_value)
			node = node->rb_right;
		else {
			key_size = hmu_tbl->key_size;
			if (memcmp(src_key, src_entry->key, key_size) != 0) {
				PRINT("WARN: hash_value:%x is the same but key content is different \n"
							, hash_value);

				printk("\t src_key: ");
				for (i = 0; i < key_size; i++)
					printk("%02X ", src_key[i]);
				printk("\n");

				printk("\t src_entry->key: ");
				for (i = 0; i < key_size; i++)
					printk("%02X ", src_entry->key[i]);
				printk("\n");
			}

			return src_entry;
		}
	}
	return NULL;
}

static int hmu_src_insert(cs_hmu_table_t *hmu_tbl, cs_hmu_src_t *src_entry)
{
	struct rb_node **ppnode = &(hmu_tbl->hmu_src_list.rb_node);
	struct rb_node *parent = NULL;
	cs_hmu_src_t *this;

	/* Figure out where to put new node */
	while (*ppnode) {
		this = rb_entry(*ppnode, cs_hmu_src_t, hmu_src_node);

		parent = *ppnode;
		if (src_entry->hash_value < this->hash_value)
			ppnode = &((*ppnode)->rb_left);
		else if (src_entry->hash_value > this->hash_value)
			ppnode = &((*ppnode)->rb_right);
		else	/* previous matching entry has inserted */
			return -1;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&src_entry->hmu_src_node, parent, ppnode);
	rb_insert_color(&src_entry->hmu_src_node, &hmu_tbl->hmu_src_list);

	return 0;
}

static int to_be_call_back_insert(cs_hmu_src_t *src_entry)
{
	if (src_entry != NULL &&
		list_empty_careful(&src_entry->to_be_call_back_node)) {
		list_add_tail(&src_entry->to_be_call_back_node,
			&to_be_call_back_list);
	} else {
		DBG(PRINT("duplicate action, cs_hmu_src_t = 0x%p\n",
			src_entry));
	}

	return 0;
}
static int to_be_call_back_remove(cs_hmu_src_t *src_entry)
{
	if (src_entry != NULL &&
		!list_empty_careful(&src_entry->to_be_call_back_node)) {
		list_del_init(&src_entry->to_be_call_back_node);
		return 0;
	}

	return -EINVAL;
}

/* if a hash entry is timed out.. call the following function to
 * invalidate the hash, and it will place the hash to invalid_hash_list */
static int cs_hmu_hash_invalidate(cs_hmu_hash_t *hash_ptr)
{
	cs_hmu_delete_accel_hash(hash_ptr->hash_idx);

	if (hash_ptr != NULL &&
		list_empty_careful(&hash_ptr->invalid_hash_node)) {
		list_add_tail(&hash_ptr->invalid_hash_node, &invalid_hash_list);
	} else {
		DBG(PRINT("duplicate action, hash_ptr = 0x%p\n", hash_ptr));
	}
	return 0;
}


/*******************************************************/
int cs_hmu_init(void)
{
	int i;

	spin_lock_init(&hmu_lock);
	INIT_LIST_HEAD(&hmu_tbl_head.node);
	INIT_LIST_HEAD(&invalid_hash_list);
	INIT_LIST_HEAD(&to_be_call_back_list);

	for (i = 0; i < CS_HMU_HASH_LIST_SIZE; i++) {
		atomic_set(&hash_list[i].hmu_usr_cnt, 0);
		hash_list[i].rslt_idx = -1;
		INIT_LIST_HEAD(&hash_list[i].hmu_src_list);
		INIT_LIST_HEAD(&hash_list[i].invalid_hash_node);
	}

	memset(hash_used_mask, 0x0, FE_HASH_STATUS_ENTRY_MAX << 3);
	return 0;
}

cs_hmu_table_t *cs_hmu_table_alloc(int key_size,
		int (*callback)(cs_hmu_table_t * table, unsigned char *key,
			u32 status))
{
	cs_hmu_table_t *tbl;
	cs_hmu_ptr_list_t *tbl_node;

	//debug_Aaron
	printk(KERN_DEBUG "%s: key_size=%d\n", __func__, key_size);
	DBG(PRINT("Start Allocating a HMU table\n"));
	tbl = (cs_hmu_table_t*) cs_zalloc(sizeof(cs_hmu_table_t), GFP_ATOMIC);
	if (!tbl) {
		DBG(PRINT("No memory for HMU SRC table\n"));
		return NULL;
	}

	spin_lock_bh(&hmu_lock);

	tbl->key_size = key_size;
	atomic_set(&tbl->src_cnt, 0);
	tbl->hmu_src_list = RB_ROOT;
	tbl->callback = callback;

	/* add tbl to hmu_tbl_head */
	tbl_node = (cs_hmu_ptr_list_t *)
		cs_zalloc(sizeof(cs_hmu_ptr_list_t), GFP_ATOMIC);
	if (!tbl_node) {
		spin_unlock_bh(&hmu_lock);
		cs_free(tbl);
		DBG(PRINT("No memory for table node\n"));
		return NULL;
	}
	tbl_node->data = (u32)tbl;

	list_add_tail(&tbl_node->node, &hmu_tbl_head.node);

	spin_unlock_bh(&hmu_lock);
	DBG(PRINT("Complete Allocating a HMU table@0x%p\n", tbl));
	return tbl;
}

static int __cs_hmu_src_free(cs_hmu_src_t *src_entry, bool cross_check,
			     bool del_hash)
{
	cs_hmu_hash_t *hash_entry;
	cs_hmu_ptr_list_t *src_node, *hash_node, *m;

	if (!src_entry) {
		DBG(PRINT("Invalid entry 0x%p\n", src_entry));
		return -EINVAL;
	}

	/* remove link of hash_entry from src_entry */
	while (!list_empty(&src_entry->hash_list)) {
		hash_node = list_first_entry(&src_entry->hash_list,
					     cs_hmu_ptr_list_t, node);
		hash_entry = (cs_hmu_hash_t *)hash_node->data;
		list_del_init(&hash_node->node);
		cs_free(hash_node);

		/*
		 * don't check contents of the hash entry
		 * when it is called from cs_hmu_del_all_hash()
		 */
		if (cross_check == false)
			continue;

		/* remove link of src_entry from hash_entry */
		list_for_each_entry_safe(src_node, m, &hash_entry->hmu_src_list,
			node) {
			if (src_node->data == (u32)src_entry) {
				list_del_init(&src_node->node);
				cs_free(src_node);
				break;
			}
		}

		atomic_dec(&hash_entry->hmu_usr_cnt);

		/*
		 * Don't invalidate the hash entry when it is called from
		 * cs_hmu_src_free() (cs_hmu_callback_table_scan()) or
		 * cs_hmu_del_all_hash().
		 * It would be invalidated from cs_hmu_table_clean().
		 */
		if (del_hash == true)
			cs_hmu_hash_invalidate(hash_entry);
	}

	/*
	 * Don't call callback function here.
	 * The callback function should be called by caller if necessary.
	 */

	cs_free(src_entry->key);
	cs_free(src_entry);
	return 0;
}

static int cs_hmu_src_free(cs_hmu_src_t *entry)
{
	return __cs_hmu_src_free(entry, true, false);
}

static int __cs_hmu_table_clean(cs_hmu_table_t *table)
{
	struct rb_node *rbnode;
	cs_hmu_src_t *src_entry;
	int ret = 0;

	DBG(PRINT("Cleaning table 0x%p\n", table));
	if (!table) {
		DBG(PRINT("Invalid table 0x%p\n", table));
		return -EINVAL;
	}

	/* walk through the whole red-black tree */
	while ((rbnode = rb_first(&table->hmu_src_list))) {
		src_entry = rb_entry(rbnode, cs_hmu_src_t, hmu_src_node);
		rb_erase(rbnode, &table->hmu_src_list);

		/* remove src entry from to_be_call_back_list if any */
		to_be_call_back_remove(src_entry);

		/* call callback function for this src_entry */
		table->callback(table, src_entry->key,
				CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED);

		ret |= __cs_hmu_src_free(src_entry, true, true);
	}

	table->hmu_src_list = RB_ROOT;
	atomic_set(&table->src_cnt, 0);


	DBG(PRINT("Done Cleaning table!\n"));
	return 0;
}

int cs_hmu_table_clean(cs_hmu_table_t *table)
{
	int ret;

	spin_lock_bh(&hmu_lock);
	ret = __cs_hmu_table_clean(table);
	spin_unlock_bh(&hmu_lock);

	return ret;
}

int cs_hmu_table_free(cs_hmu_table_t *table)
{
	int ret;
	cs_hmu_ptr_list_t *tmp_tbl_ptr, *n;

	DBG(PRINT("Free table 0x%p\n", table));
	spin_lock_bh(&hmu_lock);
	if ((ret = __cs_hmu_table_clean(table)) != 0) {
		spin_unlock_bh(&hmu_lock);
		return ret;
	}

	/* remove table from hmu_tbl_head */
	list_for_each_entry_safe(tmp_tbl_ptr, n, &hmu_tbl_head.node, node) {
		if (tmp_tbl_ptr->data == (u32) table) {
			list_del_init(&tmp_tbl_ptr->node);
			cs_free(tmp_tbl_ptr);
			break;
		}
	}
	spin_unlock_bh(&hmu_lock);
	DBG(PRINT("Done Freeing table!\n"));
	cs_free(table);
	return 0;
}

static int __cs_hmu_del_hash(cs_hmu_hash_t *hash_entry)
{
	cs_hmu_ptr_list_t *src_node, *hash_node;
	cs_hmu_src_t *src_entry;
	cs_hmu_table_t *hmu_tbl;
	u32 idx;


	/* invalidate this hash entry */
	idx = hash_idx_to_hash_list_idx(hash_entry->hash_idx);
	hash_entry->state = 0;

	if (IS_FASTNET_ENTRY(hash_entry->hash_idx) == false){
		hash_used_mask[idx >> 6] &= ~(((u64)0x01) << (idx & 0x3F));
	}

	/* walk through all invloved HMU SRC entries */
	/* remove link of src_entry from hash_entry */
	while (!list_empty(&hash_entry->hmu_src_list)) {
		/* store HMU SRC and Table ptr in local variables */
		src_node = list_first_entry(&hash_entry->hmu_src_list,
				cs_hmu_ptr_list_t, node);
		list_del_init(&src_node->node);
		src_entry = (cs_hmu_src_t *)src_node->data;
		hmu_tbl = src_entry->hmu_table_ptr;
		cs_free(src_node);
		atomic_dec(&hash_entry->hmu_usr_cnt);

		/* remove link of hash_entry from src_entry */
		list_for_each_entry(hash_node, &src_entry->hash_list, node) {
			if (hash_node->data == (u32)hash_entry) {
				list_del_init(&hash_node->node);
				cs_free(hash_node);
				atomic_dec(&src_entry->ref_cnt);
				break;
			}
		}

		/* try to add src_entry into to_be_call_back_list and
		 * ignore the duplicate case.
		 * We will handle all callback functions from caller */
		to_be_call_back_insert(src_entry);
	}

	return 0;
}

int cs_hmu_add_hash(u32 hash_idx, unsigned long hash_life_time, void *data)
{
	cs_hmu_hash_t *hash_entry;
	u32 idx;

	idx = hash_idx_to_hash_list_idx(hash_idx);
	hash_entry = &hash_list[idx];

	spin_lock_bh(&hmu_lock);

	if (hash_entry->state) {
		/* check if we already schedule to delete it */
		if (!list_empty_careful(&hash_entry->invalid_hash_node)) {
			list_del_init(&hash_entry->invalid_hash_node);
			DBG(PRINT("delete original hash entry (index 0x%x, "
				  "offset 0x%x)\n", hash_idx, idx));

			/* delete the original one and reinitialize it */
			__cs_hmu_del_hash(hash_entry);
			hash_entry->state = 1;

		} else {
			spin_unlock_bh(&hmu_lock);
			DBG(PRINT("duplicate hash entry (index 0x%x, "
				  "offset 0x%x)!?\n", hash_idx, idx));
			return -EINVAL;
		}
	} else {
		hash_entry->state = 1;
	}
	hash_entry->hash_idx = hash_idx;
	hash_entry->life_time = hash_life_time;
	hash_entry->last_use = jiffies;
	hash_entry->data = data;

	if (IS_FASTNET_ENTRY(hash_idx) == false) {
		hash_used_mask[idx >> 6] |= (((u64)0x01) << (idx & 0x3F));
	}

	spin_unlock_bh(&hmu_lock);

	return 0;
}

int cs_hmu_change_lifetime(u32 hash_idx, unsigned long hash_life_time)
{
	cs_hmu_hash_t *hash_entry;
	u32 idx;

	idx = hash_idx_to_hash_list_idx(hash_idx);
	hash_entry = &hash_list[idx];

//	spin_lock_bh(&hmu_lock);

	if (hash_entry->state)
		hash_entry->life_time = hash_life_time;

//	spin_unlock_bh(&hmu_lock);

	return 0;
}

int cs_hmu_set_result_idx(u32 hash_idx, u32 result_idx, u8 result_type)
{
	cs_hmu_hash_t *hash_entry;
	hash_entry = &hash_list[hash_idx_to_hash_list_idx(hash_idx)];
	hash_entry->rslt_idx = result_idx;
	hash_entry->rslt_type = result_type;
	hash_entry->link_qoshash_idx = 0;
	return 0;
}

int cs_hmu_set_qoshash_idx(u32 hash_idx, u32 qoshash_idx)
{
	cs_hmu_hash_t *hash_entry;
	hash_entry = &hash_list[hash_idx_to_hash_list_idx(hash_idx)];
	hash_entry->link_qoshash_idx = qoshash_idx;
	return 0;
}


int cs_hmu_create_link_src_and_hash(cs_hmu_table_t *table,
		unsigned char *src_key, u32 hash_idx, void *data)
{
	int key_size, i;
	u32 hash_value;
	cs_hmu_src_t *src_entry;
	cs_hmu_hash_t *hash_entry;
	cs_hmu_ptr_list_t *src_node, *hash_node;
	
	DBG(PRINT("Linking src 0x%p to hash_idx 0x%x in table 0x%p\n", src_key,
				hash_idx, table));
	if (!table || !src_key) {
		DBG(PRINT("Invalid table 0x%p or key 0x%p\n", table, src_key));
		return -EINVAL;
	}

	spin_lock_bh(&hmu_lock);
	key_size = table->key_size;

	/* locate HMU SRC entry */
	if (key_size <= 4) {
		hash_value = 0;
		for (i = (key_size - 1); i >= 0; i--) {
			hash_value = hash_value << 8;
			hash_value |= src_key[i];
		}
	} else
		hash_value = cs_fe_hash_keygen_crc32(src_key, key_size << 3);

	src_entry = hmu_src_search(table, hash_value, src_key);

	/* locate hash entry in hash table */
	hash_entry = &hash_list[hash_idx_to_hash_list_idx(hash_idx)];
	
	if (!hash_entry->state ||
		!list_empty_careful(&hash_entry->invalid_hash_node)) {
		spin_unlock_bh(&hmu_lock);
		DBG(PRINT("Invalid hash_idx %u\n", hash_idx));
		return -EINVAL;
	} else if (src_entry != NULL) {
		/* if the entry has been created, we need to check whether
		 * the linkage has been done before. */
		list_for_each_entry(src_node, &hash_entry->hmu_src_list, node) {
			if (src_node->data == (u32)src_entry) {
				spin_unlock_bh(&hmu_lock);				
				return 0;
			}
		}
	}

	 /* prepare hash_node and src_node */
	hash_node = (cs_hmu_ptr_list_t *)
		cs_zalloc(sizeof(cs_hmu_ptr_list_t), GFP_ATOMIC);
	if (!hash_node) {
		printk("%s: allocate hash_node failed!!\n", __func__);
		goto ERR_0;
	}


	src_node = (cs_hmu_ptr_list_t *)
		cs_zalloc(sizeof(cs_hmu_ptr_list_t), GFP_ATOMIC);
	if (!src_node)
		goto ERR_1;

	if (src_entry == NULL) {
		/* create new src_entry */
		src_entry = (cs_hmu_src_t *)
			cs_zalloc(sizeof(cs_hmu_src_t), GFP_ATOMIC);
		if (!src_entry)
			goto ERR_2;
		src_entry->key = (u8 *) cs_malloc(key_size, GFP_ATOMIC);
		if (!src_entry->key)
			goto ERR_3;
		memcpy(src_entry->key, src_key, key_size);
		src_entry->hash_value = hash_value;
		INIT_LIST_HEAD(&src_entry->to_be_call_back_node);
		atomic_set(&src_entry->ref_cnt, 0);
		src_entry->hmu_table_ptr = table;
		INIT_LIST_HEAD(&src_entry->hash_list);

		/* add src_entry into rbtree hmu_src_list */
		hmu_src_insert(table, src_entry);

		/* update table */
		atomic_inc(&table->src_cnt);
	}

	/* update the private data pointer */
	/* move this assignment to hash_add */
	//if (data != 0xffffffff)
	//	hash_entry->data = data;  /* keep the real data of rule-based hash */

	/* update src_entry */
	hash_node->data = (u32)hash_entry;

	atomic_inc(&src_entry->ref_cnt);
	list_add_tail(&hash_node->node, &src_entry->hash_list);

	/* update existed hash_entry */
	src_node->data = (u32)src_entry;

	atomic_inc(&hash_entry->hmu_usr_cnt);
	list_add_tail(&src_node->node, &hash_entry->hmu_src_list);
	
	spin_unlock_bh(&hmu_lock);
	DBG(PRINT("Done linking Src and hash\n"));
	return 0;

ERR_3:
	cs_free(src_entry);
ERR_2:
	cs_free(src_node);
ERR_1:
	cs_free(hash_node);
ERR_0:
	spin_unlock_bh(&hmu_lock);
	return -ENOMEM;
}

int cs_hmu_get_last_use_by_src(cs_hmu_table_t *table,
		unsigned char *src_key, unsigned long *src_last_use)
{
	int key_size, i;
	u32 hash_value;
	cs_hmu_src_t *src_entry;
	unsigned long last_use = 0;
	cs_hmu_hash_t *hash_entry;
	cs_hmu_ptr_list_t *hash_node;
	bool f_valid = false;

	DBG(PRINT("Get last use of src 0x%p in table 0x%p\n", src_key, table));
	if (!table || !src_key || !src_last_use) {
		DBG(PRINT("Invalid table 0x%p or key 0x%p or last_use 0x%p\n",
			table, src_key, src_last_use));
		return -EINVAL;
	}

	spin_lock_bh(&hmu_lock);

	key_size = table->key_size;

	/* locate HMU SRC entry */
	if (key_size <= 4) {
		hash_value = 0;
		for (i = (key_size - 1); i >= 0; i--) {
			hash_value = hash_value << 8;
			hash_value |= src_key[i];
		}
	} else
		hash_value = cs_fe_hash_keygen_crc32(src_key, key_size << 3);

	src_entry = hmu_src_search(table, hash_value, src_key);
	if (!src_entry) {
		spin_unlock_bh(&hmu_lock);
		DBG(PRINT("Invalid src_key 0x%p (hash_value 0x%x)\n",
					src_key, hash_value));
		return -EPERM;
	}

	/* walk through involved hash_entries to find the lastest last_use */
	list_for_each_entry(hash_node, &src_entry->hash_list, node) {
		hash_entry = (cs_hmu_hash_t *) hash_node->data;

		if (hash_entry->state == 0)
			continue;
		if (f_valid == false) {
			f_valid = true;
			last_use = hash_entry->last_use;
		} else if (time_before(last_use, hash_entry->last_use)) {
			last_use = hash_entry->last_use;
		}
	}

	if (f_valid == false) {
		spin_unlock_bh(&hmu_lock);
		return -EPERM;
	}

	*src_last_use = last_use;

	spin_unlock_bh(&hmu_lock);
	DBG(PRINT("Got last use!\n"));

	return 0;
}

int cs_hmu_get_last_use_by_hash_idx(u32 hash_idx, unsigned long *hash_last_use)
{
	cs_hmu_hash_t *hash_entry;

	DBG(PRINT("Get last use for hash_idx 0x%x\n", hash_idx));
	if (!hash_last_use) {
		DBG(PRINT("Invalid last_use\n"));
		return -EINVAL;
	}

	spin_lock_bh(&hmu_lock);

	/* locate hash entry in hash table */
	hash_entry = &hash_list[hash_idx_to_hash_list_idx(hash_idx)];
	if (!hash_entry->state || hash_idx != hash_entry->hash_idx) {
		spin_unlock_bh(&hmu_lock);
		DBG(PRINT("Invalid hash_idx %u\n", hash_idx));
		return -EPERM;
	}

	*hash_last_use = hash_entry->last_use;

	spin_unlock_bh(&hmu_lock);
	DBG(PRINT("Got last use!\n"));
	return 0;

}

int cs_hmu_get_src_ref_cnt(cs_hmu_table_t *table, unsigned char *src_key)
{
	int key_size, i;
	u32 hash_value;
	cs_hmu_src_t *src_entry;
	int cnt;

	DBG(PRINT("Get Src Ref Cnt for src 0x%p in table 0x%p\n", src_key, table));
	if (!table || !src_key) {
		DBG(PRINT("Invalid table 0x%p or key 0x%p\n", table, src_key));
		return -EINVAL;
	}

	spin_lock_bh(&hmu_lock);

	key_size = table->key_size;

	/* locate HMU SRC entry */
	if (key_size <= 4) {
		hash_value = 0;
		for (i = (key_size - 1); i >= 0; i--) {
			hash_value = hash_value << 8;
			hash_value |= src_key[i];
		}
	} else
		hash_value = cs_fe_hash_keygen_crc32(src_key, key_size << 3);

	src_entry = hmu_src_search(table, hash_value, src_key);
	if (!src_entry) {
		spin_unlock_bh(&hmu_lock);
		DBG(PRINT("Invalid src_key 0x%p (hash_value 0x%x)\n",
					src_key, hash_value));
		return -EINVAL;
	}

	cnt = atomic_read(&src_entry->ref_cnt);

	spin_unlock_bh(&hmu_lock);
	DBG(PRINT("Got Src Ref Cnt!\n"));
	return cnt;
}

static void cs_hmu_callback_table_scan(void)
{
	cs_hmu_src_t *src_entry;
	cs_hmu_table_t *table;
	u32 cb_status;

	/* walk through the whole to_be_call_back_list tree */
	while (!list_empty(&to_be_call_back_list)) {
		src_entry = list_first_entry(&to_be_call_back_list,
				cs_hmu_src_t, to_be_call_back_node);
		list_del_init(&src_entry->to_be_call_back_node);

		table = src_entry->hmu_table_ptr;

		if (atomic_read(&src_entry->ref_cnt) == 0) {
			cb_status = CS_HMU_TBL_RET_STATUS_ALL_HASH_TIMEOUT;
		} else {
			cb_status = CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_TIMEOUT;
		}

		/* call callback function per src_entry */
		table->callback(table, src_entry->key, cb_status);


		if (atomic_read(&src_entry->ref_cnt) == 0) {
			/* free HMU SRC entry */
			if (!list_empty(&src_entry->hash_list))
				DBG(PRINT("BUG!!\n"));

			rb_erase(&src_entry->hmu_src_node,
				&table->hmu_src_list);
			atomic_dec(&table->src_cnt);

			cs_hmu_src_free(src_entry);
		}
	}

	return;
}

static int __cs_hmu_del_hash_by_idx(u32 hash_idx, bool f_xcheck)
{
	cs_hmu_ptr_list_t *src_node, *hash_node;
	cs_hmu_hash_t *hash_entry;
	cs_hmu_src_t *src_entry;
	cs_hmu_table_t *hmu_tbl;
	u32 idx;


	/* locate hash entry in hash table */
	idx = hash_idx_to_hash_list_idx(hash_idx);
	hash_entry = &hash_list[idx];

	if (!hash_entry->state || hash_idx != hash_entry->hash_idx) {
		DBG(PRINT("Invalid hash_idx 0x%x, offset 0x%x\n", hash_idx,
					idx));
		return -EINVAL;
	}

	/* invalidate this hash entry */
	hash_entry->state = 0;
	if (IS_FASTNET_ENTRY(hash_idx) == false) {
		hash_used_mask[idx >> 6] &= ~(((u64)0x01) << (idx & 0x3F));
	}

	/* walk through all invloved HMU SRC entries */
	/* remove link of src_entry from hash_entry */
	while (!list_empty(&hash_entry->hmu_src_list)) {
		/* store HMU SRC and Table ptr in local variables */
		src_node = list_first_entry(&hash_entry->hmu_src_list,
				cs_hmu_ptr_list_t, node);
		list_del_init(&src_node->node);
		src_entry = (cs_hmu_src_t *)src_node->data;
		hmu_tbl = src_entry->hmu_table_ptr;
		cs_free(src_node);
		atomic_dec(&hash_entry->hmu_usr_cnt);

		/* Don't cross-check involved src entries. We will handle it
		 * in the caller. */
		if (f_xcheck == false)
			continue;

		/* remove link of hash_entry from src_entry */
		list_for_each_entry(hash_node, &src_entry->hash_list, node) {
			if (hash_node->data == (u32)hash_entry) {
				list_del_init(&hash_node->node);
				cs_free(hash_node);
				atomic_dec(&src_entry->ref_cnt);
				break;
			}
		}

		/* try to add src_entry into to_be_call_back_list and
		 * ignore the duplicate case.
		 * We will handle all callback functions from caller */
		to_be_call_back_insert(src_entry);

	}

	return 0;
}

/* for get flow hash by hash index */
int cs_hmu_get_hash_by_idx(u32 hash_idx, void **data)
{
       // cs_hmu_ptr_list_t *src_node, *hash_node;
        cs_hmu_hash_t *hash_entry;
        //cs_hmu_src_t *src_entry;
        //cs_hmu_table_t *hmu_tbl;
        u32 idx;

	spin_lock_bh(&hmu_lock);

        /* locate hash entry in hash table */
        idx = hash_idx_to_hash_list_idx(hash_idx);
        hash_entry = &hash_list[idx];
	
	DBG(printk("%s: hash_entry->state=%d, hash_idx=%d, hash_entry->hash_idx=%d\n",
		__func__, hash_entry->state, hash_idx, hash_entry->hash_idx);)

        if (!hash_entry->state || hash_idx != hash_entry->hash_idx) {
                DBG(PRINT("Invalid hash_idx 0x%x, offset 0x%x\n", hash_idx,
                                        idx));
		spin_unlock_bh(&hmu_lock);
                return -EINVAL;
        }

	*data = hash_entry->data;

	spin_unlock_bh(&hmu_lock);

#if 0
        /* walk through all invloved HMU SRC entries */
        while (!list_empty(&hash_entry->hmu_src_list)) {
                /* store HMU SRC and Table ptr in local variables */
                src_node = list_first_entry(&hash_entry->hmu_src_list,
                                cs_hmu_ptr_list_t, node);
                src_entry = (cs_hmu_src_t *)src_node->data;
                hmu_tbl = src_entry->hmu_table_ptr;
		if (src_entry->data != NULL) {
			//*data = src_entry->data;
			*data = hash_entry;
			printk("%s: hash_idx=%d is found and private data=0x%x\n", __func__, hash_idx, src_entry->data);
			break;
		}
	}
#endif
	return 0;
}

int cs_hmu_del_hash_by_idx(u32 hash_idx)
{
	int ret;
	DBG(PRINT("Deleting hash with index 0x%x\n", hash_idx));

	spin_lock_bh(&hmu_lock);

	/* delete accel hash*/
	cs_hmu_delete_accel_hash(hash_idx);

	/* delete hmu hash and inform to watch*/
	ret = __cs_hmu_del_hash_by_idx(hash_idx, true);

	spin_unlock_bh(&hmu_lock);

	return ret;
}

int cs_hmu_del_hash_by_src(cs_hmu_table_t *table, unsigned char *src_key)
{
	int key_size, i;
	u32 hash_value;
	cs_hmu_src_t *src_entry;
	cs_hmu_hash_t *hash_entry;
	cs_hmu_ptr_list_t *src_node, *hash_node;
	int ret = 0;

	DBG(PRINT("deleting hash with src 0x%p in table 0x%p!\n", src_key, table));
	if (!table || !src_key) {
		DBG(PRINT("Invalid table 0x%p or key 0x%p\n", table, src_key));
		return -EINVAL;
	}

	spin_lock_bh(&hmu_lock);

	key_size = table->key_size;

	/* locate HMU SRC entry */
	if (key_size <= 4) {
		hash_value = 0;
		for (i = (key_size - 1); i >= 0; i--) {
			hash_value = hash_value << 8;
			hash_value |= src_key[i];
		}
	} else
		hash_value = cs_fe_hash_keygen_crc32(src_key, key_size << 3);

	src_entry = hmu_src_search(table, hash_value, src_key);

	if (!src_entry) {
		spin_unlock_bh(&hmu_lock);
		DBG(PRINT("Invalid src_key 0x%p (hash_value 0x%x)\n",
					src_key, hash_value));
		return -EINVAL;
	}

	/* remove link of hash_entry from src_entry */
	while (!list_empty(&src_entry->hash_list)) {
		hash_node = list_first_entry(&src_entry->hash_list,
			cs_hmu_ptr_list_t, node);
		hash_entry = (cs_hmu_hash_t *)hash_node->data;
		list_del_init(&hash_node->node);
		atomic_dec(&src_entry->ref_cnt);
		cs_free(hash_node);

		/* remove link of src_entry from hash_entry */
		list_for_each_entry(src_node, &hash_entry->hmu_src_list, node) {
			if (src_node->data == (u32)src_entry) {
				list_del_init(&src_node->node);
				cs_free(src_node);
				break;
			}
		}
		atomic_dec(&hash_entry->hmu_usr_cnt);

		/* add hash_entry to invalid_hash_list */
		cs_hmu_hash_invalidate(hash_entry);
	}

	/* try to add src_entry into to_be_call_back_list and
	 * ignore the duplicate case.
	 * We will handle all callback functions from caller */
	to_be_call_back_insert(src_entry);

	spin_unlock_bh(&hmu_lock);

	DBG(PRINT("Done!\n"));
	return ret;
}

int cs_hmu_del_all_hash(void)
{
	cs_hmu_ptr_list_t *tmp_tbl_ptr;
	cs_hmu_table_t *tbl;
	struct rb_node *rbnode;
	cs_hmu_src_t *src_entry;
	int i, j;
	int ret = 0;
	u32 array_idx, hash_idx;
	bool f_reset_mask = false;

	DBG(PRINT("Deleting all hashes!\n"));

	spin_lock_bh(&hmu_lock);

	/* walk through hash table */
	for (i = 0; i < FE_HASH_STATUS_ENTRY_MAX; i++) {
		if (hash_used_mask[i] == 0)
			continue;
		for (j = 0; j < 64; j++) {
			if (hash_used_mask[i] & ((u64)0x01 << j)) {
				array_idx = (i << 6) | j;
				if (hash_list[array_idx].state == 0) {
					/* should not come here */
					f_reset_mask = true;
					DBG(PRINT("Inconsistent state in "
						"hash_list[%d]\n", array_idx));
					continue;
				}
				hash_idx = hash_list[array_idx].hash_idx;
				ret |= cs_hmu_delete_accel_hash(hash_idx);
				ret |= __cs_hmu_del_hash_by_idx(hash_idx, false);
			}
		}
	}

	/*delete fastnet hash*/
	for (i = 0; i < CS_HMU_HASH_LIST_SIZE_FASTNET; i++) {
		hash_idx = HASH_INDEX_FASTNET2SW(i);
		ret |= cs_hmu_delete_accel_hash(hash_idx);
		ret |= __cs_hmu_del_hash_by_idx(hash_idx, false);
	}

	if (f_reset_mask == true)
		memset(hash_used_mask, 0x0, FE_HASH_STATUS_ENTRY_MAX << 3);

	/* walk through all HMU SRC tables in hmu_tbl_head */
	list_for_each_entry(tmp_tbl_ptr, &hmu_tbl_head.node, node) {
		tbl = (cs_hmu_table_t *)tmp_tbl_ptr->data;

		/* walk through the whole red-black tree of the table */
		while ((rbnode = rb_first(&tbl->hmu_src_list))) {
			src_entry = rb_entry(rbnode, cs_hmu_src_t,
					hmu_src_node);
			rb_erase(rbnode, &tbl->hmu_src_list);

			/* remove src entry from to_be_call_back_list if any */
			to_be_call_back_remove(src_entry);

			/* call callback function for this src_entry */
			tbl->callback(tbl, src_entry->key,
					CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED);

			/* remove all links in src_entry,
			 * but don't cross-check involved hash entries. */
			ret |= __cs_hmu_src_free(src_entry, false, false);
		}

		tbl->hmu_src_list = RB_ROOT;

		atomic_set(&tbl->src_cnt, 0);
	}
	spin_unlock_bh(&hmu_lock);

	DBG(PRINT("DONE!!\n"));
	return ret;
}

/*
 * is_used:1 change last_use as current jiffies
 * is_used:0 if current jiffies - last_use > life_time, then remove it.
 *	     otherwise, do nothing.
 */
int cs_hmu_hash_update_last_use(u32 hash_idx, bool is_used)
{
	cs_hmu_hash_t *hash_entry;
	int ret = 0;

	DBG(PRINT("hash_idx = 0x%x, is_use = %d\n", hash_idx, is_used));

	spin_lock_bh(&hmu_lock);

	/* locate hash entry in hash table */
	hash_entry = &hash_list[hash_idx_to_hash_list_idx(hash_idx)];
	if (!hash_entry->state || hash_idx != hash_entry->hash_idx) {
		spin_unlock_bh(&hmu_lock);
		DBG(PRINT("Invalid hash_idx 0x%x, hash_entry->hash_idx = 0x%x, "
			"state = %d\n",
			hash_idx, hash_entry->hash_idx, hash_entry->state));
		return -EINVAL;
	}
	if (is_used) {
		/* update timer last_use */
		hash_entry->last_use = jiffies;
	} else if ((hash_entry->life_time != 0) &&
			(!time_in_range(jiffies, hash_entry->last_use,
					hash_entry->last_use +
					hash_entry->life_time))) {
		DBG(PRINT("hash time out! index = 0x%x!\n", hash_idx));
		/* prepare to delete this hash entry */
		cs_hmu_hash_invalidate(hash_entry);
	}

	spin_unlock_bh(&hmu_lock);

	return ret;
}

int cs_hmu_hash_table_scan(void)
{
	cs_hmu_hash_t *hash_entry;
	int ret = 0;

	DBG(PRINT("Going to scan hmu_hash_table!\n"));

	spin_lock_bh(&hmu_lock);

	/*
	 * walk through the whole invalid_hash_list tree,
	 * and remove invalid hash entries
	 */
	while (!list_empty(&invalid_hash_list)) {
		hash_entry = list_first_entry(&invalid_hash_list,
				cs_hmu_hash_t, invalid_hash_node);
		list_del_init(&hash_entry->invalid_hash_node);

		if (hash_entry->state)
			ret |= __cs_hmu_del_hash_by_idx(hash_entry->hash_idx,
							true);
	}

	cs_hmu_callback_table_scan();

	spin_unlock_bh(&hmu_lock);

	DBG(PRINT("Done scanning table!\n"));
	return ret;
}

int *cs_hmu_get_callback(cs_hmu_table_t *table)
{
	if (!table) {
		DBG(PRINT("Invalid table 0x%p\n", table));
		return NULL;
	}
	return (int *)(table->callback);
}

void cs_dump_src_entry(cs_hmu_src_t *src_entry) {
	cs_hmu_hash_t *hash_entry;
	cs_hmu_src_t *src_entry1;
	cs_hmu_ptr_list_t *src_node, *hash_node;
	int i = 0;
	int j = 0;
	list_for_each_entry(hash_node, &src_entry->hash_list, node) {
		j++;
		printk("\t\t %d ", j);
		hash_entry = (cs_hmu_hash_t *) hash_node->data;
		if (hash_entry) {
			printk("%x state=%d \n",
				hash_entry->hash_idx, hash_entry->state);
			list_for_each_entry(src_node, &hash_entry->hmu_src_list, node) {
				src_entry1 = (cs_hmu_src_t *)(src_node->data);
				printk("\t\t\t ref=%d key=", atomic_read(&src_entry1->ref_cnt));
				for (i = (src_entry1->hmu_table_ptr->key_size -1); i>=0; i-- )
					printk("%02X ", src_entry1->key[i]);
				printk("\n");
			}
		} else {
			printk(" null pointer \n");
		}
	}
}

/******* Debug API ********************/
void cs_hmu_dump_table(cs_hmu_table_t *table, bool detail)
{
	struct rb_node *rbnode;
	cs_hmu_src_t *src_entry;
	int src_entry_cnt = 0;
	unsigned int key_size;
	int hash_entry_cnt = 0;
	int i, j;

	if (!table) {
		DBG(PRINT("Invalid table 0x%p\n", table));
		return;
	}

	spin_lock_bh(&hmu_lock);

	key_size = table->key_size;
	src_entry_cnt = atomic_read(&table->src_cnt);

	printk("HMU SRC table      : 0x%p\n", table);
	printk("Key size           : %d bytes\n", key_size);
	printk("Callback func      : 0x%p\n", table->callback);
	printk("Total SRC entries  : %d\n", src_entry_cnt);

	/* walk through the whole red-black tree */
	rbnode = rb_first(&table->hmu_src_list);

	while (rbnode) {
		src_entry = rb_entry(rbnode, cs_hmu_src_t, hmu_src_node);
		if (detail == true) {
			j = atomic_read(&src_entry->ref_cnt);
			printk("\t ref=%d key=", j);
			for (i = (key_size -1); i>=0; i-- )
				printk("%02X ", src_entry->key[i]);
			printk("\n");
			if (j < 10)
				cs_dump_src_entry(src_entry);
		}
		hash_entry_cnt += atomic_read(&src_entry->ref_cnt);

		rbnode = rb_next(rbnode);
	}

	spin_unlock_bh(&hmu_lock);

	printk("Total HASH entries : %d\n", hash_entry_cnt);
}

void cs_hmu_dump_hash_summary(void)
{
	int i, j;
	int hash_cnt = 0;

	spin_lock_bh(&hmu_lock);

	/* walk through hash table */
	for (i = 0; i < FE_HASH_STATUS_ENTRY_MAX; i++) {
		if (hash_used_mask[i] == 0)
			continue;
		for (j = 0; j < 64; j++)
			if (hash_used_mask[i] & ((u64)0x01 << j))
				hash_cnt++;
	}

	spin_unlock_bh(&hmu_lock);

	printk("Total HW HASH entries : %d\n", hash_cnt);

}
