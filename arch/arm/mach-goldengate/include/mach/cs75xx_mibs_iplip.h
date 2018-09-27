//Bug#40475
#ifndef _CS75XX_MIBS_IPLIP_H_
#define _CS75XX_MIBS_IPLIP_H_


/*
 *  Defined for MIB
 */
typedef enum {
    CS_IPLIP_MIBS_ALL,
    CS_IPLIP_MIBS_NI,
    CS_IPLIP_MIBS_IPC,
    CS_IPLIP_MIBS_MAX,
} cs_iplip_mibs_e;


/* Aligned at 16 bytes */
typedef struct cs_iplip_mib_ni_s {
	unsigned int    NIRxGEFrameCount;   // Frame count from GE
	unsigned int    NIRxA9FrameCount;   // Frame count from A9 through PNI

	unsigned int    NITxGEFrameCount;   // Frame count to GE
	unsigned int    NITxA9FrameCount;   // Frame count to A9 through PNI

	unsigned int    NIRxGEByteCount;    // Byte count of frames from GE
	unsigned int    NIRxA9ByteCount;    // Byte count of frames from A9 through PNI

	unsigned int    NITxGEByteCount;    // Byte count of frames to GE
	unsigned int    NITxA9ByteCount;    // Byte count of frames to A9 through PNI

	unsigned int    NIRXGEDropFrameCount;
	unsigned int    NIRXA9DropFrameCount;
} __attribute__ ((__packed__)) cs_iplip_mib_ni_t;

typedef struct cs_iplip_mib_ipc_s {
	unsigned int    IpcRcvCnt;          // IPC message count from A9 to PE
	unsigned int    IpcRspCnt;          // IPC message count from PE to A9

	unsigned int    PEFifoStatus;       // HW FIFO depth status (RECPU_RX_CRYPTO_DST_FF_STS)
	unsigned int    PENIXferCnt;        // HW FIFO depth status (RECPU_CRYPTO_RX_RETURN_FF_STS)

	unsigned int    BusyAccum;
	unsigned int    CurCcount;
} __attribute__ ((__packed__)) cs_iplip_mib_ipc_t;

typedef struct iplip_mib_pe_s {
	/* PE 1 MIB: IPLIP */
	cs_iplip_mib_ipc_t  iplip_ipc_mib;
	cs_iplip_mib_ni_t   iplip_ni_mib;
} __attribute__ ((__packed__)) cs_iplip_mib_pe_t;


void cs_iplip_proc_init_module(void);
void cs_iplip_proc_exit_module(void);

/*
 *  IPC APIs
 */
void cs_iplip_get_mibs(cs_iplip_mib_pe_t *pmib);
void cs_iplip_clear_mibs(cs_iplip_mib_pe_t *pmib, cs_iplip_mibs_e type);


#endif // _CS75XX_MIBS_IPLIP_H_

