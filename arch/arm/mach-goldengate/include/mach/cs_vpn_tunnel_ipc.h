#ifndef __CS_VPN_TUNNEL_IPC_H__
#define __CS_VPN_TUNNEL_IPC_H__

#include "cs_types.h"
#include "cs_network_types.h"
#include "cs_ip_translate.h"

#define G2_RE_TUNNEL_CLIENT_RCPU0			0x7
#define G2_RE_TUNNEL_CLIENT_RCPU1			0x8

#define IPSEC_TUNNELS_MAX 				24
#define L2TPv2_SERVER_IPSEC_TUNNELS_MAX 		24
#define PPTP_SERVER_TUNNELS_ECN_ONLY_MAX 		11
#define PPTP_SERVER_TUNNELS_MAX 			32
#define L2TPv2_SERVER_TUNNELS_MAX 			32
#define L2TPv3_CLIENT_TUNNELS_MAX 			32
#define RTP_TUNNELS_MAX 				160
#define IP_TRANSLATE_TUNNELS_MAX 		(512 + IPSEC_TUNNELS_MAX)
#define MAX_ENC_KEY_LEN 32
#define MAX_ATH_KEY_LEN 32

#define CS_RTP_PE0_STATS_BASE	0xF6A3E000
#define CS_RTP_PE1_STATS_BASE	0xF6A3F000

typedef enum {
	CS_FUNC_ID_FLOW_IPSEC = CS_IPSEC_FLOW_BASED,
	CS_FUNC_ID_L2TP = CS_L2TP,
	CS_FUNC_ID_GRE = CS_GRE,
	CS_FUNC_ID_GRE_IPSEC = CS_GRE_IPSEC,
	CS_FUNC_ID_IPSEC = CS_IPSEC,
	CS_FUNC_ID_L2TP_IPSEC = CS_L2TP_IPSEC,
	CS_FUNC_ID_PPTP = CS_PPTP,
	CS_FUNC_ID_RTP = CS_RTP,
	CS_FUNC_ID_L2TPV3 = CS_L2TPV3,
	CS_FUNC_ID_L2TPV3_IPSEC = CS_L2TPV3_IPSEC,
	CS_FUNC_ID_MAPE = CS_IP_TRANSLATE,
	CS_FUNC_ID_L2TP_IPSEC_MAPE = 13,
	CS_FUNC_ID_MAX = 12,
} cs_vpn_ofld_func_id;

typedef enum {
	/*===== Generic  =====*/
	CS_IPC_PE_DUMP_TBL = 0,
	CS_IPC_PE_MIB_EN,
	CS_IPC_PE_ENTRY_EN, /*fun_id, entry id*/
	CS_IPC_PE_MTU_SET,
	CS_IPC_PE_MTU_GET,

	/*===== PPTP  =====*/
	CS_PPTP_IPC_PE_SET_ENTRY,
	CS_PPTP_IPC_PE_DEL_ENTRY,
	CS_PPTP_IPC_PE_INFORM_LOST_PKT,
	CS_PPTP_IPC_PE_SET_GENERIC_SA_IDX,
	CS_PPTP_IPC_PE_KEY_CHANGE_EVENT,

	/*===== L2TP  =====*/
	CS_L2TP_IPC_PE_SET_ENTRY,
	CS_L2TP_IPC_PE_DEL_ENTRY,

	/*===== IPSec  =====*/
	CS_IPSEC_IPC_PE_SET_SADB,
	CS_IPSEC_IPC_PE_DEL_SADB,
	CS_IPSEC_IPC_PE_SET_SADB_KEY,
	CS_IPSEC_IPC_PE_SET_SADB_SELECTOR,

	/*===== RTP proxy  =====*/
	CS_RTP_IPC_PE_SET_ENTRY,
	CS_RTP_IPC_PE_DEL_ENTRY,

	/*===== ipv4/ipv6 translation =====*/
	CS_IP_TS_IPC_PE_SET_ENTRY,
	CS_IP_TS_IPC_PE_DEL_ENTRY,
	CS_IPC_PE_IP_TS_PORTSET_SET,

	/*===== setup nexthop said =====*/
	CS_NEXT_HOP_IPC_PE_SET_ENTRY,
	CS_NEXT_HOP_IPC_PE_DEL_ENTRY,
} cs_tunnel_ipc_msg_cmd;

/* Function ID*/

/* PE send to CPU */
#define CS_IPC_PE_DUMP_TBL_ACK			CS_IPC_PE_DUMP_TBL
#define CS_IPC_PE_MIB_EN_ACK			CS_IPC_PE_ENTRY_EN
#define CS_IPC_PE_MTU_GET_ACK			CS_IPC_PE_MTU_GET
#define CS_PPTP_IPC_PE_DEL_ENTRY_ACK		CS_PPTP_IPC_PE_DEL_ENTRY
#define CS_PPTP_IPC_PE_INFORM_LOST_PKT_ACK	CS_PPTP_IPC_PE_INFORM_LOST_PKT
#define CS_L2TP_IPC_PE_SET_ENTRY_ACK		CS_L2TP_IPC_PE_SET_ENTRY
#define CS_L2TP_IPC_PE_DEL_ENTRY_ACK		CS_L2TP_IPC_PE_DEL_ENTRY
#define CS_RTP_IPC_PE_SET_ENTRY_ACK		CS_RTP_IPC_PE_SET_ENTRY
#define CS_RTP_IPC_PE_DEL_ENTRY_ACK		CS_RTP_IPC_PE_DEL_ENTRY
#define CS_IPSEC_IPC_PE_SET_SADB_ACK		CS_IPSEC_IPC_PE_SET_SADB
#define CS_IPSEC_IPC_PE_DEL_SADB_ACK		CS_IPSEC_IPC_PE_DEL_SADB
#define CS_IPSEC_IPC_PE_SET_SADB_KEY_ACK	CS_IPSEC_IPC_PE_SET_SADB_KEY
#define CS_IP_TS_IPC_PE_SET_ENTRY_ACK	CS_IP_TS_IPC_PE_SET_ENTRY
#define CS_IP_TS_IPC_PE_DEL_ENTRY_ACK	CS_IP_TS_IPC_PE_DEL_ENTRY

/* ---------------------------------------------------------------
 * CS_IPC_PE_DUMP_TBL
 */
typedef struct {
	cs_uint8_t 	fun_id;
	cs_uint8_t 	entry_id;
}__attribute__((__packed__)) g2_ipc_pe_dump_tbl_t;


/* ---------------------------------------------------------------
 * CS_IPC_PE_MIB_EN
 */
typedef struct {
	cs_uint8_t	 	enabled;
}__attribute__((__packed__)) g2_ipc_pe_mib_en_t;

/* ---------------------------------------------------------------
 * CS_IPC_PE_MTU_SET
 */
typedef struct {
	cs_uint8_t 	fun_id;
	cs_uint16_t	sa_id;
	cs_uint16_t	mtu;	 /*not include CRC*/
}__attribute__((__packed__)) g2_ipc_pe_mtu_set_t;

/* ---------------------------------------------------------------
 * CS_IPC_PE_MTU_GET
 */
typedef struct {
	cs_uint8_t 	fun_id;
	cs_uint16_t	sa_id;
}__attribute__((__packed__)) g2_ipc_pe_mtu_get_t;

cs_status_t cs_tunnel_mtu_get_ack(
	struct ipc_addr peer,
	unsigned short msg_no,
	const void *msg_data,
	unsigned short msg_size,
	struct ipc_context *context);

/* ---------------------------------------------------------------
 * CS_IPC_PE_MTU_GET_ACK
 */
typedef struct {
	cs_uint8_t 	fun_id;
	cs_uint16_t	sa_id;
	cs_uint16_t	mtu;	 /*not include CRC*/
}__attribute__((__packed__)) g2_ipc_pe_mtu_get_ack_t;

/* ---------------------------------------------------------------
 * CS_NEXT_HOP_IPC_PE_SET_ENTRY
 */
typedef struct {
	cs_uint8_t 	fun_id;
	cs_uint16_t	sa_id;
	cs_uint8_t 	next_fun_id;
	cs_uint16_t	next_sa_id;
} __attribute__((__packed__)) g2_ipc_pe_next_hop_set_entry_t;

/* ---------------------------------------------------------------
 * CS_NEXT_HOP_IPC_PE_DEL_ENTRY
 */
typedef struct {
	cs_uint8_t 	fun_id;
	cs_uint16_t	sa_id;
	cs_uint8_t 	next_fun_id;
	cs_uint16_t	next_sa_id;
} __attribute__((__packed__)) g2_ipc_pe_next_hop_del_entry_t;


/* ----------------------------------------------------------------
 * CS_PPTP_IPC_PE_SET_ENTRY
 */
typedef struct {
	/*refer to cs_pptp_sa_t */
	cs_uint16_t	op_type;  /* 0: decrypt , 1: encrypt*/
	cs_uint16_t	enc_sa_idx;
	cs_uint16_t	dec_sa_idx;
	cs_uint16_t	src_call_id;	/* local call ID */
	cs_uint16_t	dest_call_id;	/* remote call ID */
	cs_ip_address_t src_addr;	/* local PPP addr */
	cs_ip_address_t dest_addr;	/* remote PPP addr */
	cs_uint8_t state;	/* MPPE stateless/stateful mode */
	cs_uint8_t crypto_type;
	cs_uint8_t key[16]; /* send/recv key */
	cs_uint8_t key_len;
	cs_uint32_t	host_seq_num;
	cs_uint32_t	host_ack_seq_num;
} __attribute__((__packed__)) g2_ipc_pe_pptp_set_entry_t;

/* ----------------------------------------------------------------
 * CS_PPTP_IPC_PE_DEL_ENTRY
 */
typedef struct {
	cs_uint16_t	op_type;  /* 0: decrypt , 1: encrypt*/
	cs_uint16_t	sa_idx;
} __attribute__((__packed__)) g2_ipc_pe_pptp_delete_entry_t;


typedef struct {
	cs_uint16_t	op_type;  /* 0: decrypt , 1: encrypt*/
	cs_uint16_t	sa_idx;
	cs_uint16_t	current_seq;
} __attribute__((__packed__)) g2_ipc_pe_pptp_delete_entry_ack_t;


/* ----------------------------------------------------------------
 * CS_PPTP_IPC_PE_INFORM_LOST_PKT
 */
typedef struct {
	cs_uint16_t	sa_idx;		/* sa idx */
	cs_uint16_t	prev_ccount;
	cs_uint16_t	current_ccount;
} __attribute__((__packed__)) g2_ipc_pe_pptp_inform_lost_pkt_t;

/* ----------------------------------------------------------------
 * CS_PPTP_IPC_PE_SET_GENERIC_SA_IDX
 */
typedef struct {
	cs_uint16_t	op_type;  /* 0: decrypt , 1: encrypt */
	cs_uint16_t	sa_idx;
} __attribute__((__packed__)) g2_ipc_pe_pptp_set_generic_sa_idx_t;

/* ----------------------------------------------------------------
 * CS_PPTP_IPC_PE_KEY_CHANGE_EVENT
 */
typedef struct {
	cs_uint16_t	enc_sa_idx;
} __attribute__((__packed__)) g2_ipc_pe_pptp_key_change_t;

typedef struct {
	/* refer to cs_l2tp_tunnel_cfg_t */
	cs_uint16_t	ver;		/* Version and flags */
	cs_uint16_t	len;		/* Optional Length */
	cs_uint16_t	tid;		/* Tunnel ID */
	cs_uint32_t	session_id;	/* Session ID */
	cs_uint16_t	dest_port;	/* UDP port of L2TP hdr */
	cs_uint16_t	src_port;	/* UDP port of L2TP hdr */

	cs_uint16_t	encap_type;	/* 1: ip, 0: udp */
	cs_uint16_t	l2specific_len;	/* default is 4 */
	cs_uint16_t	l2specific_type;/* 0: NONE, 1: DEFAULT */
	cs_uint8_t	send_seq;	/* 0: Not send sequence */
	cs_uint8_t	calc_udp_csum;  /* For IPv6 outer IP, calc UDP checksum of L2TP header. 0: UDP checksum is zero 1: calculate UDP checksum */
	cs_uint32_t	ns;		/* If send_seq, set the start of Session NR state */

	cs_uint16_t	cookie_len;
	cs_uint8_t	cookie[8];
	cs_uint16_t	offset;		/* offset from the end of L2TP header to beginning of data*/
	cs_uint8_t	l2tp_src_mac[6];
	cs_uint8_t	peer_l2tp_src_mac[6];

	/*refer to cs_ipsec_policy_t */
	cs_uint16_t	sa_idx; 	/* if have a associated ipsec_said, the range will be in 0~23 else, it would be > 23  */
	cs_uint16_t 	ip_ver;
	cs_uint16_t 	ip_checksum;
	cs_l3_ip_addr	tunnel_saddr;
	cs_l3_ip_addr	tunnel_daddr;

} __attribute__((__packed__)) g2_ipc_pe_l2tp_set_entry_t;

/* ---------------------------------------------------------------
 * CS_L2TP_IPC_PE_SET_ENTRY_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;  /* L2TP sa IDX */
} __attribute__((__packed__)) g2_ipc_pe_l2tp_set_entry_ack_t;

/* -------------------------------------------------------------
 * CS_L2TP_IPC_PE_DEL_ENTRY
 *
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__((__packed__)) g2_ipc_pe_l2tp_del_entry_t;


typedef struct {
	cs_uint16_t	sa_idx;
	cs_uint16_t	ofld_seq;
} __attribute__((__packed__)) g2_ipc_pe_l2tp_del_entry_ack_t;


/* ---------------------------------------------------------------
 * CS_IPSEC_IPC_PE_SET_SADB
 */
typedef struct {
	cs_uint16_t		sa_idx;

	/*refer to cs_ipsec_sa_t exclude key information*/
	cs_uint16_t	replay_window;
	cs_uint32_t	spi;
	cs_uint32_t	seq_num;
	cs_uint32_t	ip_ver:1;   /* 0=IPv4, 1=IPv6 */
	cs_uint32_t	proto:1;    /* 0=ESP; 1=AH */
	cs_uint32_t	tunnel:1;   /* 0=Tunnel; 1=Transport */
	cs_uint32_t	sa_dir:1;   /* 0=outbound; 1=inbound */

	/* encryption mode based on elliptic */
	cs_uint32_t	ealg:2; /* cs_ipsec_ciph_alg_t */
	cs_uint32_t	ealg_mode:3; /* cs_ipsec_ciph_mode_t */
	cs_uint32_t	reserved0:4;   /* (n * 4) bytes */
	cs_uint32_t	iv_len:4;   /* (n * 4) bytes for CBC */
	/* authentication mode based on elliptic */
	cs_uint32_t	aalg:3; /* cs_ipsec_hash_alg_t */
	cs_uint32_t	reserved1:4;
	cs_uint32_t	icv_trunclen:4; /* (n * 4) bytes */
	cs_uint32_t	etherIP:1;
	cs_uint32_t	copy_ip_id:1;
	cs_uint32_t	copy_tos:1;
	cs_uint32_t	reserved2:1;
	/* If tunnel mode the outer IP header template is sent for reconstructing */
	cs_l3_ip_addr	tunnel_saddr;
	cs_l3_ip_addr	tunnel_daddr;
	cs_uint32_t	lifetime_bytes;
	cs_uint32_t	bytes_count;
	cs_uint32_t	lifetime_packets;
	cs_uint32_t	packets_count;

	cs_uint16_t	ip_checksum;

	cs_uint8_t	is_natt; /* NAT-Traversal */
	cs_uint8_t default_tos;	/* Set the default dscp value */
} __attribute__((__packed__)) g2_ipc_pe_ipsec_set_entry_t;

/* ---------------------------------------------------------------
 * CS_IPSEC_IPC_PE_SET_SADB_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;  /* IPsec sa IDX */
} __attribute__((__packed__)) g2_ipc_pe_ipsec_set_sadb_ack_t;

/* ---------------------------------------------------------------
 * CS_IPSEC_IPC_PE_SET_SADB_KEY_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;  /* IPsec sa IDX */
} __attribute__((__packed__)) g2_ipc_pe_ipsec_set_sadb_key_ack_t;

/* ---------------------------------------------------------------
 * CS_IPSEC_IPC_PE_DEL_SADB
 */
typedef struct {
	cs_uint16_t	sa_idx;  /* IPsec sa IDX */
} __attribute__((__packed__)) g2_ipc_pe_ipsec_del_sadb_t;

typedef struct {
	cs_uint16_t	sa_idx;  /* IPsec sa ID */
	cs_uint16_t	ofld_seq;
} __attribute__((__packed__)) g2_ipc_pe_ipsec_del_sadb_ack_t;


/* -----------------------------------------------------------------
 * CS_IPSEC_IPC_PE_SET_SADB_KEY
 */
typedef struct {
	cs_uint16_t	sa_idx;
	cs_uint8_t	auth_keylen;
	cs_uint8_t	enc_keylen;
	cs_uint8_t 	ekey[MAX_ENC_KEY_LEN];
	cs_uint8_t 	akey[MAX_ATH_KEY_LEN];
} __attribute__((__packed__)) g2_ipc_pe_ipsec_set_sadb_key_t;

/* -----------------------------------------------------------------
 * CS_IPSEC_IPC_PE_SET_SADB_SELECTOR
 */
typedef struct {
	cs_uint16_t	sa_idx;
	/*refer to cs_ipsec_policy_t */
	cs_uint16_t		selector_idx;
	cs_ipsec_selector_t	selector;
} __attribute__((__packed__)) g2_ipc_pe_ipsec_set_sadb_selector_t;


/* ---------------------------------------------------------------
 * CS_RTP_IPC_PE_SET_ENTRY
 */
typedef struct {
	/*refer to cs_rtp_tunnel_cfg_t*/
	cs_uint16_t	sa_idx;
	cs_uint16_t	seq_num_diff;
	cs_uint32_t	timestamp_diff;
	cs_uint32_t	input_ssrc;
	cs_uint32_t	output_ssrc;
	cs_uint8_t	egress_sa_mac[6];
} __attribute__ ((__packed__)) g2_ipc_pe_rtp_set_entry_t;

/* ---------------------------------------------------------------
 * CS_RTP_IPC_PE_SET_ENTRY_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__ ((__packed__)) g2_ipc_pe_rtp_set_entry_ack_t;

/* ---------------------------------------------------------------
 * CS_RTP_IPC_PE_DEL_ENTRY
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__ ((__packed__)) g2_ipc_pe_rtp_del_entry_t;


/* ---------------------------------------------------------------
 * CS_RTP_IPC_PE_DEL_ENTRY_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__ ((__packed__)) g2_ipc_pe_rtp_del_entry_ack_t;



/* ---------------------------------------------------------------
 * CS_IP_TS_IPC_PE_SET_ENTRY
 */
typedef struct {
	cs_uint16_t sa_idx;
	cs_ip_translate_type_t	translate_type;

	cs_l3_ip_addr  src_ipv6;
	cs_l3_ip_addr  dst_ipv6;
	cs_uint8_t	copy_tos;
	cs_uint8_t	default_tos;

	cs_uint16 port_set_id;
	cs_uint16 port_set_id_length;
	cs_uint16 port_set_id_offset;
	cs_uint8_t	egress_da_mac[6];
	cs_uint8_t	egress_sa_mac[6];
} __attribute__ ((__packed__)) g2_ipc_pe_ip_translate_set_entry_t;

/* ---------------------------------------------------------------
 * CS_IP_TS_IPC_PE_SET_ENTRY_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__ ((__packed__)) g2_ipc_pe_ip_translate_set_entry_ack_t;

/* ---------------------------------------------------------------
 * CS_IP_TS_IPC_PE_DEL_ENTRY
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__ ((__packed__)) g2_ipc_pe_ip_translate_del_entry_t;


/* ---------------------------------------------------------------
 * CS_IP_TS_IPC_PE_DEL_ENTRY_ACK
 */
typedef struct {
	cs_uint16_t	sa_idx;
} __attribute__ ((__packed__)) g2_ipc_pe_ip_translate_del_entry_ack_t;

/* ---------------------------------------------------------------
 * CS_IPC_PE_IP_TS_PORTSET_SET
 */
typedef struct {
	cs_uint16 port_set_id;
	cs_uint16 port_set_id_length;
	cs_uint16 port_set_id_offset;
} __attribute__ ((__packed__)) g2_ipc_pe_ip_translate_portset_set_t;
#endif
