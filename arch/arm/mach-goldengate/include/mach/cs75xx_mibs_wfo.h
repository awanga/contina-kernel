//Bug#40328
#ifndef _CS75XX_MIBS_WFO_H_
#define _CS75XX_MIBS_WFO_H_


/*
 *  Defined for MIB
 */
typedef enum {
    CS_WFO_MIBS_ALL,
    CS_WFO_MIBS_PCIE,
    CS_WFO_MIBS_NI,
    CS_WFO_MIBS_IPC,
    CS_WFO_MIBS_MAX,
} cs_wfo_mibs_e;


/* Aligned at 16 bytes */
typedef struct mib_pcie_s {
	unsigned int   PCIeRxFrameCount;	// WiFi RX frame count
	unsigned int   PCIeTxFrameCount;	// WiFi TX frame count

	unsigned int   PCIeRxByteCount;	// WiFi RX byte count
	unsigned int   PCIeTxByteCount;	// WiFi TX byte count

	unsigned int   PCIeTxErrorCount;
	unsigned int   PCIeTxPEFrameCount;	// PE to PE TX frame count

	unsigned int   PCIeDropFrameCount;
	unsigned int   PCIeRxEOLCount;

	unsigned int   PCIeRxORNCount;
	unsigned int   PCIeRxsErrorCount;

	unsigned int   PCIeTxsErrorCount;
	unsigned int   PCIeRxBcastFrameCount;

	unsigned int   PCIeTxBcastFrameCount;
	unsigned int   PCIeRxMcastFrameCount;

	unsigned int   PCIeTxMcastFrameCount;
	unsigned int   rsvd;
} __attribute__ ((__packed__)) cs_mib_pcie_t;

typedef struct mib_ni_s {
	unsigned int   NIRxGEFrameCount;	// Frame count from GE
	unsigned int   NIRxA9FrameCount;	// Frame count from A9 through PNI

	unsigned int   NITxGEFrameCount;	// Frame count to GE
	unsigned int   NITxA9FrameCount;	// Frame count to A9 through PNI

	unsigned int   NIRxGEByteCount;	// Byte count of frames from GE
	unsigned int   NIRxA9ByteCount;	// Byte count of frames from A9 through PNI

	unsigned int   NITxGEByteCount;	// Byte count of frames to GE
	unsigned int   NITxA9ByteCount;	// Byte count of frames to A9 through PNI

	unsigned int   NIRXGEDropFrameCount;
	unsigned int   NIRXA9DropFrameCount;

	unsigned int   NIRxBcastFrameCount;
	unsigned int   NITxBcastFrameCount;

	unsigned int   NIRxMcastFrameCount;
	unsigned int   NITxMcastFrameCount;

	unsigned int	rsvd[2];
} __attribute__ ((__packed__)) cs_mib_ni_t;

typedef struct mib_ipc_s {
	unsigned int    IpcRcvCnt;		// IPC message count from A9 to PE
	unsigned int    IpcRspCnt;		// IPC message count from PE to A9

	unsigned int    PEFiFoStatus;		// HW FIFO depth status (RECPU_RX_CRYPTO_DST_FF_STS)
	unsigned int	PENIXferCnt;		// HW FIFO depth status (RECPU_CRYPTO_RX_RETURN_FF_STS)

	unsigned int	BusyAccum;
	unsigned int	CurCcount;
} __attribute__ ((__packed__)) cs_mib_ipc_t;

typedef struct mib_pe_s {
	/* PE MIB */
	cs_mib_ipc_t  pe_ipc;
	cs_mib_pcie_t pe_pci;
	cs_mib_ni_t   pe_ni;
} __attribute__ ((__packed__)) cs_mib_pe_s;




/*
 *  APIs
 */
void cs_wfo_ipc_pe_get_mibs( cs_mib_pe_s *pmib, unsigned char pe_id );
void cs_wfo_ipc_pe_set_mibs( cs_mib_pe_s *pmib, unsigned char pe_id );


#endif // _CS75XX_MIBS_WFO_H_

