#ifndef _CS75XX_IPC_WFO_H_
#define _CS75XX_IPC_WFO_H_
#include <linux/types.h>

typedef enum {
    WFO_MAC_ENTRY_INVALID,
    WFO_MAC_ENTRY_TRY,
    WFO_MAC_ENTRY_VALID,
    WFO_MAC_ENTRY_TXWI,
    WFO_MAC_ENTRY_EXPIRE,
    WFO_MAC_ENTRY_NEWPE,
    WFO_MAC_ENTRY_STATUS_MAX
} wfo_mac_entry_status_e;

typedef enum {
    WFO_DP_TYPE_A9,
    WFO_DP_TYPE_GE,
    WFO_DP_TYPE_WIFI,
    WFO_DP_TYPE_MAX
} wfo_dp_type_e;

typedef enum {
    WFO_FWD_ENTRY_TYPE_8023,
    WFO_FWD_ENTRY_TYPE_80211,
} wfo_entry_type_e;

#define WFO_MAX_IPC_RETRY_COUNT	3
#define WFO_MAX_MAC_AGING_COUNT 300

/*
 * MAC Table Entry
 * This table is maintained by Linux. Whenever there is update, an IPC message
 * will be propagated to PE.
 *
 */
#define WFO_MAC_ENTRY_FLAG_BA_DISABLE   0x1

struct wfo_mac_table_entry {
    /* MAC DA as entry key for lookup */
    unsigned char mac_da[6];

    /* Destination egress port. Merge WiFi and Eth ports together? */
    unsigned char	egress_port;

    /* IPC message send count, before status changes to ACTIVE.
     * If MAX_CNT, stop sending IPC */
    unsigned char	to_pe0;
    unsigned char	to_pe1;
    unsigned char	retry_cnt;

    wfo_mac_entry_status_e	status;

    /* Aging timeout */
    unsigned long	timeout;

    unsigned char t1_txwi[16]; //ralink specific type 1 data frame
    unsigned char t2_txwi[16]; //ralink specific type 2 data frame
    unsigned char t4_txwi[16]; //ralink specific type 3 data frame


    unsigned char ba_count;    //BA send count
    unsigned char ba_created;  //BA session created

    /* List of MAC table entries */
    // struct list_head	list;
};

#define INC_8023_ENTRY_RETRY(a) mac_table_802_3[a].retry_cnt++;
#define INC_80211_ENTRY_RETRY(a) mac_table_802_11[a].retry_cnt++;

typedef enum wfo_mac_type {
    WFO_MAC_TYPE_NONE,
    WFO_MAC_TYPE_802_3,
    WFO_MAC_TYPE_802_11,
} wfo_mac_type_e;

/*
 * Struct used to send MAC entry info to PE.
 */
typedef struct wfo_mac_entry {
	__u8 *mac_da;
	__u8 egress_port;
	__u8 pe_id;
	__u16 len;
	__u8 priority;
	__u8 frame_type;
	wfo_mac_type_e da_type;
	__u8 *p802_11_hdr;
	__u8 *txwi;
	__u8 orig_lspid;
} wfo_mac_entry_s;

//A9 WFO local mac table used for sending fwd entry add IPC to PE
void cs_wfo_mac_table_init(void);
wfo_mac_entry_status_e cs_wfo_set_mac_entry(wfo_mac_entry_s *mac_entry);
wfo_mac_entry_status_e cs_ath_wfo_set_mac_entry(wfo_mac_entry_s *mac_entry);
bool cs_wfo_del_mac_entry(unsigned char* mac_da, char da_type, char pe_id);
void cs_wfo_dump_frame(__u8 *p_comment, __u8 p_frame[], __u16 size);
void cs_wfo_dump_mac(__u8 mac[]);
void cs_wfo_dump_mac_with_comemnts(__u8 *p_comment, __u8 mac[]);
unsigned char cs_wfo_mac_entry_ba_inc(unsigned char* mac_da, unsigned char pe_id);
unsigned char cs_wfo_mac_entry_get_ba_status(unsigned char* mac_da, unsigned char pe_id);
void cs_wfo_mac_entry_set_ba_status(unsigned char* mac_da, unsigned char pe_id, unsigned char ba_created);
bool cs_wfo_del_all_entry(void);
void cs_wfo_dump_mac_entry(void);


typedef enum {
    CS_BA_SESSION_ADD,
    CS_BA_SESSION_DEL,
} cs_wfo_ba_session_status;



/* client ID for PE0 and PE1 */
typedef enum {
    CS_WFO_IPC_ARM_CLNT_ID = 0,
    CS_WFO_IPC_PE0_CLNT_ID,
    CS_WFO_IPC_PE1_CLNT_ID,
    CS_WFO_IPC_CLNT_ID_MAX,
} cs_wfo_ipc_client_id;

/* IPC CPU ID for Kernel, PE0, and PE1 */
typedef enum {
    CS_WFO_IPC_ARM_CPU_ID = 0,
    CS_WFO_IPC_PE0_CPU_ID,
    CS_WFO_IPC_PE1_CPU_ID,
    CS_WFO_IPC_CPU_ID_MAX,
} cs_wfo_ipc_cpu_id;


/* msg type */
// Message send from Host to PE
#define CS_WFO_IPC_PE_START			    0x01
#define CS_WFO_IPC_PE_STOP			    0x02
#define CS_WFO_IPC_PE_MESSAGE			0x03
#define CS_WFO_IPC_PE_MAX_RX		    CS_WFO_IPC_PE_MESSAGE

// Message send from PE to Host
#define CS_WFO_IPC_PE_START_COMPLETE    0x04
#define CS_WFO_IPC_PE_STOP_COMPLETE     0x05
#define CS_WFO_IPC_PE_MESSAGE_COMPLETE  0x06
#define CS_WFO_RPC_MESSAGE              0x08

#define CS_WFO_DEL_HWFWD                0xFE

/* PE Init state */
#define CS_WFO_IPC_PE_DEAD			    0x00
#define CS_WFO_IPC_PE_ACTIVE			0x01



/* TXWI structure */
typedef struct rt3593_txwi_s {
    /* Word  0 */
    /* ex: 00 03 00 40 means txop = 3, PHYMODE = 1 */
    __u32         FRAG:1;     /* 1 to inform TKIP engine this is a fragment. */
    __u32         MIMOps:1;   /* the remote peer is in dynamic MIMO-PS mode */
    __u32         CFACK:1;
    __u32         TS:1;

    __u32         AMPDU:1;
    __u32         MpduDensity:3;
    __u32         txop:2;  /*FOR "THIS" frame. 0:HT TXOP rule ,
    					  1:PIFS TX ,2:Backoff, 3:sifs only when
    					  previous frame exchange is successful. */
    __u32         NDPSndRate:2; /* 0 : MCS0, 1: MCS8, 2: MCS16, 3: reserved */
    __u32         NDPSndBW:1; /* NDP sounding BW */
    __u32         Autofallback:1; /* TX rate auto fallback disable */
    __u32         TXRPT:1;
    __u32         rsv:1;

    __u32         MCS:7;
    __u32         BW:1; /*channel bandwidth 20MHz or 40 MHz */
    __u32         ShortGI:1;
    __u32         STBC:2;  /* 1: STBC support MCS =0-7,   2,3 : RESERVE */
    __u32         eTxBF:1; /* eTxBF enable */
    __u32         Sounding:1; /* Sounding enable */
    __u32         iTxBF:1; /* iTxBF enable */
    __u32         PHYMODE:2;
    /* Word1 */
    /* ex:  1c ff 38 00 means ACK=0, BAWinSize=7, MPDUtotalByteCount = 0x38 */
    __u32         ACK:1;
    __u32         NSEQ:1;
    __u32         BAWinSize:6;
    __u32         WirelessCliID:8;
    __u32         MPDUtotalByteCount:12;
    __u32         PacketId:4;
    /*Word2 */
    __u32         IV;
    /*Word3 */
    __u32         EIV;
} __attribute__((__packed__)) rt3593_txwi_t;


// Command Add/Remove 802.3 lookup - used for WIFI->GE forwarding
// PE needs to extra MAC SA and frame length to build 802.3 header
#define CS_WFO_EGRESS_PORT_GMAC_0     0x00
#define CS_WFO_EGRESS_PORT_GMAC_1     0x01
#define CS_WFO_EGRESS_PORT_GMAC_2     0x02
#define CS_WFO_EGRESS_PORT_PE_0 	  0x03
#define CS_WFO_EGRESS_PORT_PE_1	      0x04


typedef struct cs_wfo_ipc_cmd_802_3_s {
	__u8          mac_da_address[6];
	__u8          egress_port_id;;
	__u8			  rsvd;
} __attribute__ ((__packed__)) cs_wfo_ipc_cmd_802_3_t;

typedef enum {
	DP_TYPE_A9,
	DP_TYPE_GE,
	DP_TYPE_WIFI
}cs_wfo_dp_type_e;

// Command Add/Remove 802.11 lookup - used for GE->WIFI forwarding
// PE needs to adjust the sequence control based on priority
typedef struct cs_wfo_ipc_cmd_802_11_s {
	__u8          mac_da_address[6];

	__u8          header_802_11[36];
	__u8          header_802_11_len;
	__s8          prio;
	__u8          len_hdr_802_11_and_vendor_data;
	__u8          chip_model;          // reference cs_wfo_wifi_chip
	__u16          starting_seq_control;
   cs_wfo_dp_type_e dp_type;
	union {
		rt3593_txwi_t   txwi;
		__u8			reserved[32];
	} vendor_spec;
} __attribute__ ((__packed__)) cs_wfo_ipc_cmd_802_11_t;
/* IPC Message Command definitions */
typedef enum {
    CS_WFO_CHIP_RSVD = 0,
    CS_WFO_CHIP_RT3593,
    CS_WFO_CHIP_AR988X,
    CS_WFO_CHIP_AR9580,
    CS_WFO_CHIP_BC4360,
} cs_wfo_wifi_chip;

// Command Updated RT3593 TXWI
typedef struct cs_wfo_ipc_cmd_updated_txwi_s {
    __u8              mac_da_address[6];
	__s8              prio;
	__u8					rsvd;
    rt3593_txwi_t     txwi;
} __attribute__ ((__packed__)) cs_wfo_ipc_cmd_updated_txwi_t;


#define CS_WFO_CLR_MIB_PCIE     0x01
#define CS_WFO_CLR_MI_NI        0x02
#define CS_WFO_CLR_MIB_WLAN     0x04
typedef struct cs_wfo_ipc_cmd_clear_mib_s {
    __u8              mib_type;
} __attribute__ ((__packed__)) cs_wfo_ipc_cmd_clear_mib_t;

// Command Send PCIe access physical address
typedef struct cs_wfo_ipc_cmd_send_pcie_phy_addr_s {
    __u32              valid_addr_mask;          /* Bit_0~Bit_5: addr0~addr5 */
                                                /* Bit_6~Bit_31: reserved */
    __u32             pcie_phy_addr_start[6];   /* Upto 6 bar address can be there */
    __u32             pcie_phy_addr_end[6];     /* Upto 6 bar address can be there */
} __attribute__ ((__packed__)) cs_wfo_ipc_cmd_send_pcie_phy_addr_t;


// Response 802.11 delete
#define MAX_WFO_COS 8
typedef struct cs_wfo_ipc_resp_802_11_delete_s {
	__u8          mac_da_address[6];
	__u16         seq_control[MAX_WFO_COS];
} __attribute__ ((__packed__)) cs_wfo_ipc_resp_802_11_delete_t;

// FOR CS_WFO_IPC_PE_MESSAGE
#define IPC_MSG_START_COMPLETE_SIZE     2
/* PE needs aligned structure elements */
typedef struct cs_wfo_ipc_msg_s {
	__u32              pe_id;
	union {
		struct {
	        __u8              wfo_cmd;        // reference cs_wfo_ipc_msg_cmd
	        __u8              wfo_cmd_seq;
		}pe_msg;
		struct {
	        __u8              wfo_ack_seq;
	        __u8              wfo_status;     // reference cs_wfo_status
		}pe_msg_complete;
		__u32 rsvd;
    }hdr; // union

    union {
        cs_wfo_ipc_cmd_802_3_t cmd_802_3;
        cs_wfo_ipc_cmd_802_11_t cmd_802_11;
        cs_wfo_ipc_cmd_updated_txwi_t cmd_update_txwi;
        cs_wfo_ipc_cmd_clear_mib_t cmd_clear_mib;
        cs_wfo_ipc_cmd_send_pcie_phy_addr_t cmd_pcie_phy_addr;

        cs_wfo_ipc_resp_802_11_delete_t resp_802_11_del;
    }paras;

} __attribute__ ((__packed__)) cs_wfo_ipc_msg_t;
#define IPC_MSG_SIZE                    sizeof(cs_wfo_ipc_msg_t)

typedef struct cs_wfo_ipc_msg_hdr_s {
	__u32              pe_id;
	union {
		struct {
	        __u8              wfo_cmd;        // reference cs_wfo_ipc_msg_cmd
	        __u8              wfo_cmd_seq;
		}pe_msg;
		struct {
	        __u8              wfo_ack_seq;
	        __u8              wfo_status;     // reference cs_wfo_status
		}pe_msg_complete;
		__u32 rsvd;
    }hdr; // union
} __attribute__ ((__packed__)) cs_wfo_ipc_msg_hdr_t;

/* IPC Message Command definitions */
typedef enum {
    CS_WFO_IPC_MSG_CMD_RSVD = 0,
    CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_3,
    CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_3,
    CS_WFO_IPC_MSG_CMD_ADD_LOOKUP_802_11,
    CS_WFO_IPC_MSG_CMD_DEL_LOOKUP_802_11,
    CS_WFO_IPC_MSG_CMD_UPDATED_TXWI,
    CS_WFO_IPC_MSG_CMD_CLR_MIB,
    CS_WFO_IPC_MSG_CMD_PE_CLR_ERR_LOG,
    CS_WFO_IPC_MSG_CMD_PE_START_WFO, //PE will only start WFO after recieving this message
    CS_WFO_IPC_MSG_CMD_SEND_PCIE_PHY_ADDR,
    CS_WFO_IPC_MSG_CMD_DUMP_FWTBL,
    CS_WFO_IPC_MSG_CMD_CUSTOMERIZED = 0x20,  // Defined for the vendor specified

    CS_WFO_GET_MIB_COUNTER = 0xF0,  // Defined for wfo-ipc-test
    CS_WFO_GET_CRITICAL_LOGS,
    CS_WFO_DUMP_A9_MAC_ENTRY,
} cs_wfo_ipc_msg_cmd;

/* Return status codes */
typedef enum {
	CS_WFO_OP_SUCCESS = 0,
	CS_WFO_OP_FAILURE,
	CS_WFO_INVALID_ARGUMENTS,
	CS_WFO_INVALID_SEQ,
	CS_WFO_ENTRY_NOT_FOUND,
	CS_WFO_ENTRY_EXISTS,
	CS_WFO_INTERNAL_ERROR,
	CS_WFO_PE_TBL_FULL,
} cs_wfo_status;


/* IPC mib_type */
typedef enum {
    CS_WFO_IPC_MIB_TYPE_RSVD = 0,
    CS_WFO_IPC_MIB_TYPE_PCIE,
    CS_WFO_IPC_MIB_TYPE_NI,
    CS_WFO_IPC_MIB_TYPE_WLAN,
} cs_wfo_mib_type;

/* IEEE 802.11 2-byte Frame control field */
typedef struct {
    __u16 ver:2;		/* Protocol version */
	__u16 type:2;		/* MSDU type */
	__u16 subType:4;	/* MSDU subtype */
	__u16 toDs:1;		/* To DS indication */
	__u16 frDs:1;		/* From DS indication */
	__u16 moreFrag:1;	/* More fragment bit */
	__u16 retry:1;	/* Retry status bit */
	__u16 pwrMgmt:1;	/* Power management bit */
	__u16 moreData:1;	/* More data bit */
	__u16 wep:1;		/* Wep data */
	__u16 order:1;    /* Strict order expected */
} __attribute__ ((__packed__)) frame_control, *pframe_control;


//this part is common (independent of ToDS/FromDS)
typedef struct {
    frame_control   fc;
    __u16             duration;
    __u8              addr1[6];
    __u8              addr2[6];
    __u8			  addr3[6];
    __u16			  frag:4;
    __u16			  sequence:12;
 } __attribute__ ((__packed__)) header_802_11, *pheader_802_11;



// Defined for IOCTL
#define CS_WFO_IPC_MAGIC   0x4353574F

// IOCTL Common Header
typedef struct cs_wfo_ipc_ioctl_s{
    __u8                cmd;        // command ID
    cs_wfo_ipc_msg_t    pmsg;       // IPC message payload
} __attribute__ ((__packed__)) cs_wfo_ipc_ioctl_t;

/*
 *  Defined for Dump Farwarding Table
 */
#define CS_WFO_FWTBL_DMESG_SIZE		0x1000
#define CS_WFO_FWTBL_DMESG_BASE0	0xf6405000

#define CS_WFO_FWTBL_DMESG_BASE1 	(CS_WFO_FWTBL_DMESG_BASE0 + \
												CS_WFO_FWTBL_DMESG_SIZE)

#define CS_WFO_FWTBL_MAGIC			0xf7b1c0de
#define CS_WFO_FWTBL_OWNER_PE		0x0
#define CS_WFO_FWTBL_OWNER_A9		0x1

typedef struct fwtbl_hdr_s{
	__u32 signature;
	__u16 owner;
	__u16 tbl_size;
}fwtbl_hdr_t;

typedef struct fwtbl_rec_s {
	__u8 entry_valid;
	__u8 wfo_lkup_type;
	__u8 mac_addr[6];
	__u8 out_port;
	cs_wfo_dp_type_e dp_type;
}fwtbl_rec_t;


/*
 *  Defined for MIB
 */
#define  CS_WFO_MIB_PHY_ADDR    0xF6400000
#define  CS_WFO_LOG_PHY_ADDR    0xF6404000
#define  CS_WFO_LOG_BUF_SIZE    0x8000
//#define  CS_WFO_LOG_BUF_SIZE    0x800
#define  CS_PRINT_MAGIC         0xbabeface


/*
 *  IPC APIs
 */
int cs_wfo_ipc_send_start(__u8 pe_id, __u8 *p_msg_payload);
int cs_wfo_ipc_send_stop(__u8 pe_id, __u8 *p_msg_payload);
int cs_wfo_ipc_add_lookup_802_3( __u8 pe_id,
                                 __u8 *pmac_da_address,
                                 __u8 egress_port_id );
int cs_wfo_ipc_del_lookup_802_3( __u8 pe_id,
                                 __u8 *pmac_da_address );
int cs_wfo_ipc_add_lookup_802_11( __u8 pe_id,
                                  __u8 *phdr_802_11,
                                  __u8 hdr_802_11_len,
                                  __u8 prio,
                                  __u8 *pvendor_spec,
                                  __u8 frame_type );
int cs_wfo_ipc_del_lookup_802_11( __u8 pe_id,
                                  __u8 *pmac_da_address,
                                  __u8 prio );
int cs_wfo_ipc_update_rt3593_txwi( __u8 pe_id,
                                   __u8 *pmac_da_address,
                                   __u8 prio,
                                   rt3593_txwi_t *txwi );
int cs_wfo_ipc_pe_clear_mib(__u8 pe_id, __u8 mib_type);
int cs_wfo_ipc_pe_clear_err_log(__u8 pe_id);
int cs_wfo_ipc_pe_start_wfo(__u8 pe_id);
int cs_wfo_ipc_send_pcie_phy_addr( __u8 pe_id,
                                   __u8 valid_addr_mask,
                                   __u32 addr_start[],
                                   __u32 addr_end[] );
/* Bug#40475 Move to cs75xx_mibs_who.h */
//void cs_wfo_ipc_pe_get_mibs( cs_mib_pe_s *pmib, __u8 pe_id );
//void cs_wfo_ipc_pe_set_mibs( cs_mib_pe_s *pmib, __u8 pe_id );
void cs_wfo_ipc_pe_get_logs( void *plog, __u8 pe_id );
void cs_wfo_del_hash_by_mac_da(__u8 *pmac);
int cs_wfo_ipc_pe_send_dump_fwtbl( __u8 pe_id );
void cs_wfo_ipc_pe_dump_fwtbl(__u8 pe_id);
void cs_wfo_ipc_set_wifi_adatper_addr( __u8 pe_id, void* pAd );
void cs_wfo_ipc_send_start_stop_command(__u8 cmd);

// Update 802.11 Sequence Control Callback function
typedef __u8 (*WFOUpdateSCCallback)(void *pWFOAd, __u8 *pAddr, __u16 *pTxSeq);
void register_updateSC_callback(__u8 (*WFOUpdateSCCallback));

#endif // _CS75XX_IPC_WFO_H_

