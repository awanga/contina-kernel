#ifndef CS_9580_WFO_MEM_DEF
#define CS_9580_WFO_MEM_DEF

#define CS_A9580_A9_TXS_VADDR              0xEF0FB000  //0xEF0FB000 (non-cacheale) 0x81EFB000 (cacheable)
#define CS_A9580_A9_TXS_PADDR              0x01EFB000  //512 * 36
#define CS_A9580_WIFI_TXS_PADDR            0xf6A30000  //512 * 36
#define CS_A9580_A9_PCIE_PADDR             0xf6403400  //HAL_TXFIFO_DEPTH(8) * HAL_NUM_TX_QUEUES (10) * sizeof (uint32) = 320 bytes

typedef enum {
	CS_WFO_IPC_MSG_CMD_RESET_TX = CS_WFO_IPC_MSG_CMD_CUSTOMERIZED,
	CS_WFO_IPC_MSG_CMD_RESET_RX,
} cs_wfo_ar9580_ipc_msg_cmd;

#define NI_FIELD_KEY		1
#define NI_FIELD_TXPWR		2
#define NI_FIELD_NI_INFO	4
#define NI_FIELD_GROUP_KEY	8

typedef struct cs_ar9580_wfo_ipc_update_msg_s {
	cs_wfo_ipc_msg_hdr_t ipc_msg_hdr;
	u32 field_mask;

	u8 mac_address[6];	/* ni->ni_macaddr*/
	/*update by NI_FIELD_NI_INFO*/
	u8 ni_bssid[6]; /*ni->ni_bssid*/
	u32 ni_flags;	/*ni->ni_flags for bit:
					  Addqos
					  IEEE80211_NODE_HT (support aggregation)
					 */

	u8 power;		/* tx_desc_11, 21:16 == ni->ni_txpower */
	u8 rsvd[2];

	/*update by NI_FIELD_TXPWR*/
	u8	pad_delim;	    /* txdesc_17, 25:18 will calculated by framelen*/

	u32 txctl_flags;
	u32	tx_tries;	    /* txdesc_13 */
	u32	tx_rates;	    /* txdesc_14 */
	u32	rts_cts_0_1;	/* txdesc_15 */

	u32	rts_cts_2_3;	/* txdese_16 */
	u32	chain_sel;	    /* txdesc_18 */
	u32	ness_0; 	    /* txdesc_19 */
	u32	ness_1; 	    /* txdesc_20 */

	u32	ness_2; 	    /* txdesc_21 */
	u32	ness_3; 	    /* txdesc_22 */

	/*update by NI_FIELD_KEY*/
	u16 dest_idx;		/* txdesc_12, 19:13 == txctl->key_ix  */
	u8	encrypt_type;	/* txdesc_17, 28:26 == txctl->keytype */
	u8	frame_type; 	/* txdesc_12, 23:20 == txctl->atype */

	/* rate control */
	u16     rc_series_flags[4];
	
	/*for crypto*/
	u32 key_ptr;
} __attribute__ ((__packed__)) cs_ar9580_wfo_ipc_update_msg_t;


#define WFO_IEEE80211_TID_SIZE 17

struct cs_ar9580_wfo_ieee80211_key {
    u8	wk_keylen;	/* key length in bytes */

    u8	wk_flags;
#define	IEEE80211_KEY_XMIT	0x01	/* key used for xmit */
#define	IEEE80211_KEY_RECV	0x02	/* key used for recv */
#define	IEEE80211_KEY_GROUP	0x04	/* key used for WPA group operation */
#define IEEE80211_KEY_MFP   0x08    /* key also used for management frames */
#define	IEEE80211_KEY_SWCRYPT	0x10	/* host-based encrypt/decrypt */
#define	IEEE80211_KEY_SWMIC	0x20	/* host-based enmic/demic */
#define IEEE80211_KEY_PERSISTENT 0x40   /* do not remove unless OS commands us to do so */
#define IEEE80211_KEY_PERSTA    0x80    /* per STA default key */

	u16 wk_keyix;	/* key index */
    int wk_valid;
 //   u_int8_t	wk_key[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
//#define	wk_txmic	wk_key+IEEE80211_KEYBUF_SIZE+0	/* XXX can't () right */
//#define	wk_rxmic	wk_key+IEEE80211_KEYBUF_SIZE+8	/* XXX can't () right */
/*support for WAPI*/
#if ATH_SUPPORT_WAPI
    u8    wk_recviv[16];          /* key receive IV for WAPI*/
    u32   wk_txiv[4];          /* key transmit IV for WAPI*/
#endif

    u64	wk_keyrsc[WFO_IEEE80211_TID_SIZE];	/* key receive sequence counter */
    u64 wk_keyrsc_suspect[WFO_IEEE80211_TID_SIZE]; /* Key receive sequence counter suspected */
    u64 wk_keyglobal; /* key receive global sequnce counter */
    u64	wk_keytsc;	/* key transmit sequence counter */
    void *  wk_cipher;
	u32 ic_cipher;		/* IEEE80211_CIPHER_* */

    //void *  wk_private;	/* private cipher state */
    //u64 cipher_private; // for tkip, it is rx_rsc;
    //uint16  wk_clearkeyix; /* index of clear key entry, needed for MFP */
}__attribute__ ((__packed__));

#endif
