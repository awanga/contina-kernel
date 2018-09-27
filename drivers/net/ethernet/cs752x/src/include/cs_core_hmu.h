
#ifndef __CS_CORE_HMU_H__
#define __CS_CORE_HMU_H__

#include "cs_core_logic.h"

/* for watch_mask in cs_core_hmu_t and type in cs_core_hmu_value_t */
#define CS_CORE_HMU_WATCH_FWDRSLT	0x00000001
#define CS_CORE_HMU_WATCH_QOSRSLT	0x00000002
#define CS_CORE_HMU_WATCH_QOSHASH	0x00000004
#define CS_CORE_HMU_WATCH_IN_MAC_SA	0x00000008
#define CS_CORE_HMU_WATCH_IN_MAC_DA	0x00000010
#define CS_CORE_HMU_WATCH_OUT_MAC_SA	0x00000020
#define CS_CORE_HMU_WATCH_OUT_MAC_DA	0x00000040
#define CS_CORE_HMU_WATCH_IN_VID1	0x00000080
#define CS_CORE_HMU_WATCH_OUT_VID1	0x00000100
#define CS_CORE_HMU_WATCH_IN_VID2	0x00000200
#define CS_CORE_HMU_WATCH_OUT_VID2	0x00000400
#define CS_CORE_HMU_WATCH_PPPOE_ID	0x00000800
#define CS_CORE_HMU_WATCH_IN_IPV4_SA	0x00001000
#define CS_CORE_HMU_WATCH_IN_IPV4_DA	0x00002000
#define CS_CORE_HMU_WATCH_OUT_IPV4_SA	0x00004000
#define CS_CORE_HMU_WATCH_OUT_IPV4_DA	0x00008000
#define CS_CORE_HMU_WATCH_IN_IPV6_SA	0x00010000
#define CS_CORE_HMU_WATCH_IN_IPV6_DA	0x00020000
#define CS_CORE_HMU_WATCH_OUT_IPV6_SA	0x00040000
#define CS_CORE_HMU_WATCH_OUT_IPV6_DA	0x00080000
#define CS_CORE_HMU_WATCH_IP_PROT	0x00100000
#define CS_CORE_HMU_WATCH_SRC_PHY_PORT	0x00200000
#define CS_CORE_HMU_WATCH_DST_PHY_PORT	0x00400000
#define CS_CORE_HMU_WATCH_SWID64	0x00800000
#define CS_CORE_HMU_WATCH_IN_L4_SP	0x01000000
#define CS_CORE_HMU_WATCH_IN_L4_DP	0x02000000
#define CS_CORE_HMU_WATCH_OUT_L4_SP	0x04000000
#define CS_CORE_HMU_WATCH_OUT_L4_DP	0x08000000

/* return status from callback function */
typedef enum {
	CS_CORE_HMU_RET_STATUS_CREATE_SUCCEED,
	CS_CORE_HMU_RET_STATUS_CREATE_FAIL,
	CS_CORE_HMU_RET_STATUS_TIMEOUT_PARTIAL,
	CS_CORE_HMU_RET_STATUS_TIMEOUT_ALL,
	CS_CORE_HMU_RET_STATUS_DELETED_PARTIAL,
	CS_CORE_HMU_RET_STATUS_DELETED_ALL,
	// FIXME!! more to be done.. follow cs_hmu.h
} cs_core_hmu_ret_status_e;

typedef struct cs_core_hmu_value_s {
	struct cs_core_hmu_value_s *next;

	u32 type;
	/* all the network header field values should be in host order */
	union {
		u16 index;
		u8 mac_addr[6];
		u16 vid;
		u16 pppoe_session_id;
		u32 ip_addr[4];		/* if IPv4, use ip_addr[0]. addr is
					   			work ordered. */
		u16 ip_prot;
		u16 l4_port;
		u64 swid64;
	} value;
	u16 mask;	/* when MSB is cleared, the lower 8 bits are used as
			   standard IP subnet mask (= decimal value) where it
			   checks the first # of bits that matches with the
			   value.  When MSB is set, it's the other way
			   around.  It checks the last # of bits that matches
			   with the value. */
} cs_core_hmu_value_t;

typedef struct cs_core_hmu_s {
	u32 watch_bitmask;
	cs_core_hmu_value_t *value_mask;	/* If the value for an enabled
						   watch bitmask is not set, it
						   will just watch all the
						   values */
	/* callback function should be handled fast */
	int (*callback) (u32 watch_bitmask, cs_core_hmu_value_t *value,
			u32 status);
} cs_core_hmu_t;

/* init and exit */
int cs_core_hmu_init(void);
void cs_core_hmu_exit(void);

/* for registering HMU */
int cs_core_hmu_register_watch(cs_core_hmu_t *hmu_entry);
/* 1) create HMU table.. define callback.
 * 	callback(hmu_table, value, stats) {
 * 		1) based on hmu_table_ptr, find the store core_hmu info from the
 * 		stored list.
 * 		2) stored list -> register callback functions (pointer)
 * 		3) convert the value back to cs_core_hmu_value_t
 *		4) go through the stored list. call all the callback functions
 *		that registered with this core HMU stored list
 * 	}
 */

/* for unregistering HMU */
int cs_core_hmu_unregister_watch(cs_core_hmu_t *hmu_entry);

/* clean current SRC entries in HMU */
int cs_core_hmu_clean_watch(cs_core_hmu_t *hmu_entry);

/* add the hash */
int cs_core_hmu_add_hash(u32 hash_idx, unsigned long life_time, void *data);

/* delete hash based on the given value */
int cs_core_hmu_delete_hash(u32 watch_bitmask, cs_core_hmu_value_t *value);

/* delete the hash based on index */
int cs_core_hmu_delete_hash_by_idx(u32 hash_idx);

/* obtain the last used jiffies based on the given value */
int cs_core_hmu_get_last_use(u32 watch_bitmask, cs_core_hmu_value_t *value,
		unsigned long *time_in_jiffies);

/* delete all the hashes */
int cs_core_hmu_delete_all_hash(void);

/* The following APIs are for Main Control Unit */
/* link HMU SRC based on info given in cs_cb to hash of hash_idx */
int cs_core_hmu_link_src_and_hash(cs_kernel_accel_cb_t *cs_cb, u32 hash_idx, void *data);

/* callback for hash creation success */
int cs_core_hmu_callback_for_hash_creation_pass(cs_kernel_accel_cb_t *cs_cb);

/* callback for hash creation failure */
int cs_core_hmu_callback_for_hash_creation_fail(cs_kernel_accel_cb_t *cs_cb);

/* link qos_hash_idx and fwd_hash_idx */
int cs_core_hmu_link_fwd_and_qos_hash(u32 fwd_hash_idx, u32 qos_hash_idx, bool need_watch);

int cs_core_hmu_set_result_idx(u32 hash_idx, u32 result_idx,u8 result_type);

int cs_core_hmu_get_hash_by_idx(u32 hash_idx, void **data);

int cs_core_hmu_get_last_use_by_hash_idx(u32 hash_idx, unsigned long *hash_last_use);

/*************** Debug API *************/
void cs_core_hmu_dump_table(int num);
void cs_core_hmu_dump_all_table(void);

#endif /* __CS_CORE_HMU_H__ */
