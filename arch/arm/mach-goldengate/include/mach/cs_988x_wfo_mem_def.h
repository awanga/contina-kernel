#ifndef CS_988X_WFO_MEM_DEF
#define CS_988X_WFO_MEM_DEF


#define CS_A988X_CE_HTT_RX_ID               1
#define CS_A988X_CE_HTT_TX_ID               4
#define CS_A988X_CE_HTT_RX_DST_NENTRIES     256
#define CS_A988X_HTT_RX_RING_SIZE           512
#define CS_A988X_PKT_RX_MAX_SIZE            1920 // eqal to HTT_RX_BUF_SIZE
#define CS_A988X_CE_HTT_RX_MAX_SIZE         512  // CE1 HTC msg rx buffer size

//RRAM1 stuff
//#define CS_A988X_HTT_RX_DSC_PADDR           0xF6402400 //RX PKT DESC
#define CS_A988X_HTT_RX_PADDR               0xF6400150 //alloc_idx.paddr
#define CS_A988X_HTT_RX_VADDR               0xF6400150 //(virtual address)
#define CS_A988X_CE1_RX_DESC_BASE           0xF6400400 //CE1 desc starting addr (2K)
#define CS_A988X_HTT_RX_DSC_PADDR           (CS_A988X_CE1_RX_DESC_BASE + 0x800) //RX PKT DESC (2K)

#define CS_A988X_HTT_RX_RING_FILL_LEVEL     CS_A988X_HTT_RX_RING_SIZE - 1


//PKTBUF stuff
#define CS_A988X_HTC_RX_BUFFER_BASE         0xF6A00000 // HTC buffer starting addr
//#define CS_A988X_HTT_RX_BUFFER_PADDR        0xF6A10000 // (Internal buffer)


//DDR stuff
#define CS_A988X_HTT_RX_BUFFER_PADDR        0x01C00000 // (Internal buffer)

#define CS_A988X_PDEV_DOWNLOAD_LEN 			88
#define CS_A988X_PDEV_TX_HTC_ENDPOINT		1

//++BUG#39672: 1. Separate STA in same SSID
typedef enum {
    CS_WFO_IPC_MSG_CMD_AR988X_STA2STA = CS_WFO_IPC_MSG_CMD_CUSTOMERIZED,
    CS_WFO_IPC_MSG_CMD_AR988X_MULTI_BSSID,  //BUG#40246: PE0 usage is high for downlink
} cs_wfo_ar988x_ipc_msg_cmd;

typedef enum {
    WFO_AR988X_SAME_SSID_STA2STA_DISABLE,
    WFO_AR988X_SAME_SSID_STA2STA_ENABLE,
} cs_wfo_ar988x_same_ssid_sta2sta_type_e;

typedef struct cs_ar988X_wfo_ipc_cmd_sta2sta_s {
	cs_wfo_ipc_msg_hdr_t ipc_msg_hdr;
	u8  mode;       //reference cs_wfo_ar988x_sta2sta_type_e
} __attribute__ ((__packed__)) cs_ar988X_wfo_ipc_cmd_sta2sta_t;
//--BUG#39672

//++BUG#40246: PE0 usage is high for downlink
typedef enum {
    WFO_AR988X_MULTI_BSSID_DISABLE,
    WFO_AR988X_MULTI_BSSID_ENABLE,
} cs_wfo_ar988x_multi_bssid_type_e;

typedef struct cs_ar988X_wfo_ipc_cmd_multi_bssid_s {
	cs_wfo_ipc_msg_hdr_t ipc_msg_hdr;
	cs_wfo_ar988x_multi_bssid_type_e  mode;
} __attribute__ ((__packed__)) cs_ar988X_wfo_ipc_cmd_multi_bssid_t;
//--BUG#40246: PE0 usage is high for downlink

#endif
