/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <wen.hsu@cortina-systems.com>
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
/*
 * cs_hw_accel_ipsec.h
 *
 * $Id: cs_hw_accel_ipsec.h,v 1.18 2012/09/18 09:54:48 gliang Exp $
 *
 * This header file defines the data structures and APIs for CS IPsec Offload.
 */

#ifndef __CS_HW_ACCEL_IPSEC_H__
#define __CS_HW_ACCEL_IPSEC_H__

#include <linux/skbuff.h>
#include <net/xfrm.h>
#include <linux/rbtree.h>
#include <mach/cs_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_core_logic.h>

//#define CS_IPSEC_OUTBOUND_BYPASS_LINUX


#define CS_IPC_ENABLED	1
#ifdef CS_IPC_ENABLED
#include <mach/g2cpu_ipc.h>
#endif

#define CS_IPSEC_TUN_NUM	16
#define CS_IPSEC_OP_DEC		0
#define CS_IPSEC_OP_ENC		1
#define CS_IPSEC_INBOUND	0
#define CS_IPSEC_OUTBOUND	1
#define CS_IPSEC_IPV4	0
#define CS_IPSEC_IPV6	1

#define CS_IPSEC_RE_SKB_SIZE_MIN	0
#define CS_IPSEC_RE_SKB_SIZE_MAX	1514

#define CS_IPSEC_HASH_TIME_OUT		120

/* guid is a 64-bit ID that's used for hash handling */
#define CS_IPSEC_GID(re_id, sa_id)	(((u64)CS_SWID64_MOD_ID_IPSEC << 48) \
		| ((u64)re_id << 9) | (sa_id))
#define CS_IPSEC_GET_TAG_ID_FROM_GID(guid)	((u16)CS_SWID64_TO_MOD_ID(guid))
#define CS_IPSEC_GET_SA_ID_FROM_GID(guid)	((u16)(guid & 0x01ff))
#define CS_IPSEC_GET_RE_ID_FROM_GID(guid)	((u8)((guid >> 9) & 0x01))
#define CS_IPSEC_SPGID(re_id, sa_id, sp_id) (((u64)CS_SWID64_MOD_ID_IPSEC << 48) \
		| ((u64)re_id << 17) | ((u64)sp_id << 10) | (1 << 25) | (sa_id))
#define CS_IPSEC_SPGID_MASK			0x0003fc00
#define CS_IPSEC_GET_SP_ID_FROM_SP_GID(guid)	((u16)((guid >> 10) & 0x07f))
#define CS_IPSEC_GET_RE_ID_FROM_SP_GID(guid)	((u8)((guid >> 17) & 0x01))
#define CS_IPSEC_GET_SA_ID_FROM_SP_GID(guid)	((u16)(guid & 0x01ff))

#define CS_IPSEC_GET_GUID_TYPE_FROM_GID(guid)	((u8)((guid >> 25) & 0x01))

#define CS_IPSEC_SW_ACTION_ID(func_id, recir_id, sa_id)		\
	(((u32)(func_id << 16)) | ((u32)(recir_id << 9)) | (sa_id))
#define CS_IPSEC_GET_RE_ID_FROM_FUNC_ID(func_id)			\
	((func_id >> 1) ^ 0x1)


enum {
	SADB_INIT,			/* 00 */
	SADB_PE_STOPPED,	/* 01 */
	SADB_ACCELERATED,	/* 10 */
	SADB_WAIT_PE_STOP	/* 11 */
};


/* VOQ related definition */
/*
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH
 *	Send outbound IPSec packets to VoQ#32
 *	Send inbound IPSec packets to VoQ#24
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0
 *	Send outbound IPSec packets to VoQ#25
 *	Send inbound IPSec packets to VoQ#24
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1
 *	Send outbound IPSec packets to VoQ#33
 *	Send inbound IPSec packets to VoQ#32
 */
//#define CS_IPSEC_RE_VOQ_ID(re_id)	((re_id << 3) + ENCRYPTION_VOQ_BASE)
#define CS_IPSEC_RE_VOQ_ID_SET(voq_id, re_id)	do { \
	switch (cs_hw_ipsec_offload_mode) { \
	case IPSEC_OFFLOAD_MODE_BOTH: \
		voq_id = (re_id << 3) + ENCRYPTION_VOQ_BASE; \
		break; \
	case IPSEC_OFFLOAD_MODE_PE0: \
		voq_id = re_id + ENCRYPTION_VOQ_BASE; \
		break; \
	case IPSEC_OFFLOAD_MODE_PE1: \
		voq_id = re_id + ENCAPSULATION_VOQ_BASE; \
		break; \
	default: \
		voq_id = GE_PORT1_VOQ_BASE; \
		printk("%s:%d: unknown IPSec offload mode %d\n", __func__, \
			__LINE__, cs_hw_ipsec_offload_mode); \
	} \
} while (0)

/*
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH
 *	Send outbound IPSec packets to VoQ#32
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0
 *	Send outbound IPSec packets to VoQ#25
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1
 *	Send outbound IPSec packets to VoQ#33
 */
#define CS_IPSEC_RE_VOQ_ID_FROM_CPU(voq_id)	do { \
		switch (cs_hw_ipsec_offload_mode) { \
		case IPSEC_OFFLOAD_MODE_BOTH: \
			voq_id = ENCAPSULATION_VOQ_BASE; \
			break; \
		case IPSEC_OFFLOAD_MODE_PE0: \
			voq_id = ENCRYPTION_VOQ_BASE + 1; \
			break; \
		case IPSEC_OFFLOAD_MODE_PE1: \
			voq_id = ENCAPSULATION_VOQ_BASE + 1; \
			break; \
		default: \
			voq_id = GE_PORT1_VOQ_BASE; \
			printk("%s:%d: unknown IPSec offload mode %d\n", __func__, \
				__LINE__, cs_hw_ipsec_offload_mode); \
		} \
	} while (0)

/*
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH
 *	Send IPSec packets to VoQ#96
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0
 *	Send IPSec packets to VoQ#96
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1
 *	Send IPSec packets to VoQ#97
 */
#define CS_IPSEC_CPU_VOQ_ID_FOR_RE(re_id) \
	((cs_hw_ipsec_offload_mode==IPSEC_OFFLOAD_MODE_PE1) ? \
	(CPU_PORT6_VOQ_BASE + 1) : CPU_PORT6_VOQ_BASE)

/*
 * Internal hash for outbound IPSec
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH
 *	Send IPSec packets from ENCAPSULATION_PORT
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0
 *	Send IPSec packets from ENCRYPTION_PORT
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1
 *	Send IPSec packets from ENCAPSULATION_PORT
 */
#define CS_IPSEC_OUT_LSPID_FROM_RE \
	((cs_hw_ipsec_offload_mode==IPSEC_OFFLOAD_MODE_PE0) ? \
	ENCRYPTION_PORT : ENCAPSULATION_PORT)

/*
 * hash for inbound IPSec
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_BOTH
 *	Send IPSec packets from ENCRYPTION_PORT
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE0
 *	Send IPSec packets from ENCRYPTION_PORT
 * cs_hw_ipsec_offload_mode == IPSEC_OFFLOAD_MODE_PE1
 *	Send IPSec packets from ENCAPSULATION_PORT
 */
#define CS_IPSEC_IN_LSPID_FROM_RE \
	((cs_hw_ipsec_offload_mode==IPSEC_OFFLOAD_MODE_PE1) ? \
	ENCAPSULATION_PORT : ENCRYPTION_PORT)


typedef union cs_ipaddr_s {
	u32 addr[4];		/* IP address */
} cs_ipaddr_t;

typedef struct cs_ipsec_skb_queue_s {
	struct cs_ipsec_skb_queue_s *next;
	struct sk_buff *skb;
} cs_ipsec_skb_queue_t;

/* maximum queue size to handle recovery queue packet */
#define CS_IPSEC_CB_QUEUE_MAX	30

typedef struct cs_ipsec_cb_queue_s {
	struct cs_ipsec_cb_queue_s *next;
	u8 idx;
	struct sk_buff skb;
	unsigned long timeout;
} cs_ipsec_cb_queue_t;

#define MAX_ENC_KEY_LEN		32
#define MAX_AUTH_KEY_LEN	32

/* special index handling */
#define CS_IPSEC_SP_IDX_START	112
#define CS_IPSEC_SP_IDX_NUM	16

typedef struct cs_ipsec_sadb_s {
	u16 sa_idx;
	u8 replay_window;
	u32 spi;
	u32 seq_num;
	u8 ekey[MAX_ENC_KEY_LEN];
	u8 akey[MAX_AUTH_KEY_LEN];
	u32	used:1,	/* 0=No, 1=Yes */
		ip_ver:1,		/* 0=IPv4, 1=IPv6 */
		proto:1,		/* 0=ESP; 1=AH */
		mode:1,		/* 0=Tunnel; 1=Transport */
		sa_dir:1,		/* 0=Inbound; 1=Outbound */
		state:2,
		/* encryption mode based on elliptic */
		ealg:2, ealg_mode:3, enc_keylen:4,	/* (n * 4) bytes */
		iv_len:4,		/* (n * 4) bytes for CBC */
		/* authentication mode based on elliptic */
		aalg:3, auth_keylen:4,	/* (n * 4) bytes */
		icv_trunclen:4,	/* (n * 4) bytes */
#ifdef CONFIG_CS752X_HW_ACCEL_ETHERIP
	 etherip:1;
#else
	 rsvd:1;
#endif

	/* if tunnel mode the outer IP header template is for reconstructing */
	cs_ipaddr_t tunnel_saddr;
	cs_ipaddr_t tunnel_daddr;
	u32 lifetime_bytes;
	u32 bytes_count;
	u32 lifetime_packets;
	u32 packets_count;
	u16 checksum;
	u8 eth_addr[14];
#ifdef CONFIG_CS752X_HW_ACCEL_ETHERIP
	cs_ipaddr_t	eip_tunnel_saddr;
	cs_ipaddr_t	eip_tunnel_daddr;
	u8		eip_ttl;
	u8		eip_eth_addr[14];
	u16 eip_checksum;
#endif
	u8 ekey_context[64];
	u8 akey_context[64];
	struct xfrm_state *x_state;
} __attribute__ ((__packed__)) cs_ipsec_sadb_t;

typedef struct cs_ipsec_rb_s {
	struct rb_node flow_node;
	u32 crc32;
} __attribute__ ((__packed__)) cs_ipsec_rb_t;

typedef struct cs_ipsec_cp_skb_queue_s {
	u8 used;
	struct sk_buff skb;
	unsigned long timeout;
	u16 hash_idx;
	u16 fwdrslt_idx;
} cs_ipsec_cp_skb_queue_t;

typedef struct cs_ipsec_db_s {
	u16 p_re0_idx;
	u16 p_re1_idx;
	spinlock_t re0_lock;
	spinlock_t re1_lock;
	cs_ipsec_skb_queue_t *re0_skb_q[CS_IPSEC_TUN_NUM];
	cs_ipsec_skb_queue_t *re1_skb_q[CS_IPSEC_TUN_NUM];
	cs_ipsec_sadb_t re0_sadb_q[CS_IPSEC_TUN_NUM];
	cs_ipsec_sadb_t re1_sadb_q[CS_IPSEC_TUN_NUM];
	struct net_device * re1_dest_voq[CS_IPSEC_TUN_NUM];
	cs_ipsec_cp_skb_queue_t skb_per_spidx[CS_IPSEC_TUN_NUM * CS_IPSEC_SP_IDX_NUM];
} __attribute__ ((__packed__)) cs_ipsec_db_t;


/* CS IPsec Offload supported cipher algorithm */
#define CS_IPSEC_CIPHER_NULL	0
#define CS_IPSEC_DES		1
#define CS_IPSEC_3DES		1
#define CS_IPSEC_AES		2

#define CS_IPSEC_CIPHER_ECB		0
#define CS_IPSEC_CIPHER_CBC		1
#define CS_IPSEC_CIPHER_CTR		2
#define CS_IPSEC_CIPHER_CCM		3
#define CS_IPSEC_CIPHER_GCM		5
#define CS_IPSEC_CIPHER_OFB		7
#define CS_IPSEC_CIPHER_CFB		8

/* CS IPsec Offload supported authentication algorithm */
#define CS_IPSEC_AUTH_NULL	0
#define CS_IPSEC_MD5		1
#define CS_IPSEC_SHA1		2
#define CS_IPSEC_SHA224		3
#define CS_IPSEC_SHA256		4

#ifdef CS_IPC_ENABLED
#define CS_IPSEC_IPC_TIMER_PERIOD	5	/* second */

/* IPC CPU ID for Kernel, RE0, and RE1 */
#define CS_IPSEC_IPC_ARM_CPU_ID		0x00
#define CS_IPSEC_IPC_RE0_CPU_ID		0x01
#define CS_IPSEC_IPC_RE1_CPU_ID		0x02

/* client ID for RE0 and RE1 */
#define CS_IPSEC_IPC_RE0_CLNT_ID	0x3
#define CS_IPSEC_IPC_RE1_CLNT_ID	0x4

/* msg type */
#define CS_IPSEC_IPC_RE_INIT			0x01
#define CS_IPSEC_IPC_RE_STOP			0x02
#define CS_IPSEC_IPC_RE_UPDATE			0x03
#define CS_IPSEC_IPC_RE_DUMPSTATE               0x04
#define CS_IPSEC_IPC_RE_INIT_COMPLETE	0x0a
#define CS_IPSEC_IPC_RE_STOP_COMPLETE	0x0b
#define CS_IPSEC_IPC_RE_UPDATE_COMPLETE	0x0c
#define CS_IPSEC_IPC_RE_STOP_BY_RE		0x0d

#define CS_IPSEC_IPC_RE_DEAD			0x00
#define CS_IPSEC_IPC_RE_ACTIVE			0x01
#define CS_IPSEC_IPC_RE_ACTIVE_CHECK1	0x02
#define CS_IPSEC_IPC_RE_ACTIVE_CHECK2	0x02

typedef struct cs_ipsec_ipc_msg_s {
	u8 op_id;
	u16 sa_id;
	u32 data[2];
} __attribute__ ((__packed__)) cs_ipsec_ipc_msg_t;
#endif

int cs_ipsec_handler(struct sk_buff *skb, struct xfrm_state *x, u8 ip_ver, u8 dir);
void cs_ipsec_done_handler(struct sk_buff *skb, u8 ip_ver, u8 dir);

int cs_hw_accel_ipsec_handle_rx(struct sk_buff *skb,
				      u8 src_port, u16 in_voq_id,
				      u32 sw_action);

void cs_hw_accel_ipsec_init(void);
void cs_hw_accel_ipsec_exit(void);

void ipsec_ipc_timer_func(unsigned long data);

cs_ipsec_sadb_t *ipsec_sadb_get(unsigned char table_id, unsigned short sa_id);

int ipsec_sadb_update(unsigned char table_id, unsigned short sa_id,
			    unsigned char dir, struct xfrm_state *p_x_state,
			    unsigned char ip_ver);

int ipsec_sadb_check_skb_queue_empty(unsigned char table_id, unsigned short sa_id);
void ipsec_sadb_update_dest_dev(unsigned char table_id, unsigned short sa_id,
				struct net_device * dev);


void ipsec_dequeue_skb_and_send_to_re(unsigned char table_id,
				      unsigned short sa_id);
void cs_hw_accel_ipsec_print_sadb_status(void);
void cs_hw_accel_ipsec_clean_sadb_status(void);
void cs_hw_accel_ipsec_print_pe_sadb_status(void);
void cs_hw_accel_ipsec_clean_pe_sadb_status(void);

#ifdef CS_IPC_ENABLED
int cs_hw_accel_ipsec_ipc_status(unsigned char table_id);
#endif

#endif				/* __CS_HW_ACCEL_IPSEC_H__ */
