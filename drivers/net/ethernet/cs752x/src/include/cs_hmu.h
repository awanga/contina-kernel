#ifndef __CS_HMU_H__
#define __CS_HMU_H__

#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_core_fastnet.h>

/* in the relationshp table, we use Red-Black tree to main the list.
 * Red-Black tree has good look up time, it is similar to AVL tree which
 * tries to balance tree at insertion/deletiong. */

#define CS_HMU_HASH_LIST_SIZE_HW_ACCEL	(FE_HASH_STATUS_ENTRY_MAX << 6)

#ifdef CONFIG_CS752X_FASTNET
#define CS_HMU_HASH_LIST_SIZE_FASTNET	CS_FASTNET_HASH_MAX
#else
#define CS_HMU_HASH_LIST_SIZE_FASTNET 0
#endif

#define CS_HMU_HASH_LIST_SIZE	(CS_HMU_HASH_LIST_SIZE_FASTNET + CS_HMU_HASH_LIST_SIZE_HW_ACCEL)


typedef enum {
	CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_TIMEOUT,
	CS_HMU_TBL_RET_STATUS_ALL_HASH_TIMEOUT,
	CS_HMU_TBL_RET_STATUS_PARTIAL_HASH_DELETED,
	CS_HMU_TBL_RET_STATUS_ALL_HASH_DELETED,
} cs_hmu_table_ret_status_e;

typedef struct cs_hmu_table_s {
	/* the size of the key */
	unsigned int	key_size;	/* byte size */

	/* Note: We have a thought to add key_mask which behaves like IP address
	 * subnet mask, but we decide to put it that feature later or never. */

	/* number of HMU SRC entry in the table */
	atomic_t	src_cnt;

	/* the list of HMU SRC entry */
	struct rb_root	hmu_src_list;

	int (*callback)(struct cs_hmu_table_s *table, unsigned char *key,
			u32 status);

	spinlock_t	lock;
} cs_hmu_table_t;


/******** APIs for Main Control Unit ********/
/* software init */
int cs_hmu_init(void);

/* to allocate a HMU table with given key_size and callback function */
cs_hmu_table_t *cs_hmu_table_alloc(int key_size,
		int (*callback)(cs_hmu_table_t *table, unsigned char *key,
			u32 status));

/* free HMU table, but do not free the hash entries related to the HMU entry
 * in the table. */
int cs_hmu_table_free(cs_hmu_table_t *table);

/* delete all src entries in the HMU table */
int cs_hmu_table_clean(cs_hmu_table_t *table);

/* enabling a hash with given hash life time */
int cs_hmu_add_hash(u32 hash_idx, unsigned long hash_life_time, void *data);

/* changing the life time of a given hash */
int cs_hmu_change_lifetime(u32 hash_idx, unsigned long hash_life_time);

/* link the key value with the given pointer to hash entry.  It must have
 * a valid hw_idx.  It is called after HW hash entry has been created.
 * Hash life time is in jiffies. */
int cs_hmu_create_link_src_and_hash(cs_hmu_table_t *table,
		unsigned char *src_key, u32 hash_idx, void *data);

int cs_hmu_set_result_idx(u32 hash_idx, u32 result_idx,u8 result_type);
int cs_hmu_set_qoshash_idx(u32 hash_idx, u32 qoshash_idx);


/* 1) use hw_idx to locate hash entry in hash table.
 * 	a) If it is found, use it.
 * 	b) If not, return error.
 * 2) If the key size is less than or equal to 4 byte, we use src_key
 *    for hash value.  Otherwise, we compute hash based on the src_key.
 * 3) After we obtain the hash value, we use it to search through the HMU
 *    entries in the rbtree table
 * 	a) if nothing is found, create a new one.
 * 	b) else use the matched one.
 * 4) Once we have both HMU SRC and HMU Hash, add the pointer to the other
 *    entity into their respective lists.
 */


/* get the last use of the hash that this specific key watches.
 * Return: 0 if succeeds, else otherwise.
 */
int cs_hmu_get_last_use_by_src(cs_hmu_table_t *table,
		unsigned char *src_key, unsigned long *src_last_use);

/* get the last use of the hash with the given hash_idx
 * Return: 0 if succeeds, else otherwise.
 */
int cs_hmu_get_last_use_by_hash_idx(u32 hash_idx,
				    unsigned long *hash_last_use);

/* return the ref_cnt of the HMU SRC entry with key value */
int cs_hmu_get_src_ref_cnt(cs_hmu_table_t *table, unsigned char *src_key);

/* delete hash by hash index */
int cs_hmu_del_hash_by_idx(u32 hash_idx);

/* delete all the hash entries that are watched by the src_key */
int cs_hmu_del_hash_by_src(cs_hmu_table_t *table, unsigned char *src_key);
/* 1) find HMU entry with the key
 * 2) go through HMU's hash list
 * 	if hash has been invalidated -> skip.
 * 	else invalidate the hash by doing:
 * 		a) unlink the HMU SRC and the HASH (both directions)
 * 		b) put it into invalid_hash_list
 * 		c) delete HMU SRC from HMU table? */

/* delete/nuke all the hashes! */
int cs_hmu_del_all_hash(void);
/* 1) walk through all the valid hash entries, mark them (remove hardware)
 * 	invalid.
 * 2) walk through all the valid hash entries, locate the HMU src entries
 * 	they relate too.  Callback and remove the pointer from the list */

/******** APIs for hash status table scan routine ********/

/* update the last use of the hash entry with hash_idx.
 * If it is used, update the hash's last use jiffies and all the HMU SRC that
 * watch this hash entry.
 * If not, check the life time vs (the current time - the last use time),
 * it might need to invalidate the hash entry and add it to invalid_hash_list */
int cs_hmu_hash_update_last_use(u32 hash_idx, bool is_used);

/* going through the invalid_hash_list:
 * for every hash entry in the invalid_hash_list queue
 * 	for every HMU under this hash entry
 * 		call its callback function */
int cs_hmu_hash_table_scan(void);

int *cs_hmu_get_callback(cs_hmu_table_t *table);

int cs_hmu_get_hash_by_idx(u32 hash_idx, void **data);

/******* Debug API ********************/
void cs_hmu_dump_table(cs_hmu_table_t *table, bool detail);
void cs_hmu_dump_hash_summary(void);

#endif /* __CS_HMU_H__ */
