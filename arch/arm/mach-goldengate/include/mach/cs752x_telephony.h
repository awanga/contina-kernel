/**********************************************************************
 *           For CORTINA SLIC verification
 *   
 ***********************************************************************/


#ifndef CS752X_TELEPHONY_H
#define CS752X_TELEPHONY_H


//***********************************************************************
//*             Cortina G2 -- SSP
//***********************************************************************/
typedef struct {
        int addr;
        int data;
        int reg_type; // 0: SSP Control, 1: Slic Direct, 2: Slic Indirect Register
}Ssp_reg;

typedef struct {
        int chan;
        unsigned int cmd;
        unsigned int data;
} phone_diag_cmd_t;

int hssp;
unsigned int start_dma;

#define SSP_GET_HOOK_STATUS             _IOR  ('q', 0xC0, int)
#define SSP_GET_LINEFEED                _IOR  ('q', 0xC1, int)
#define SSP_SET_LINEFEED                _IOW  ('q', 0xC2, int)
#define SSP_GET_REG                     _IOWR ('q', 0xC3, struct Ssp_reg *)
#define SSP_SET_REG                     _IOWR ('q', 0xC4, struct Ssp_reg *)
#define SSP_GEN_OFFHOOK_TONE            _IO   ('q', 0xC5)
#define SSP_GEN_BUSY_TONE               _IO   ('q', 0xC6)
#define SSP_GEN_RINGBACK_TONE           _IO   ('q', 0xC7)
#define SSP_GEN_CONGESTION_TONE         _IO   ('q', 0xC8)
#define SSP_GEN_REORDER_TONE            _IO   ('q', 0xC9)
#define SSP_DISABLE_DIALTONE            _IO   ('q', 0xCA)
#define SSP_PHONE_RING_START            _IO   ('q', 0xCB)
#define SSP_PHONE_RING_STOP             _IO   ('q', 0xCC)
#define SSP_PHONE_RINGING               _IO   ('q', 0xCD)
#define SSP_GET_PHONE_STATE             _IOR  ('q', 0xCE, int)
#define SSP_SET_PHONE_STATE             _IOW  ('q', 0xCF, int)
#define SSP_SLIC_GOACTIVE               _IO   ('q', 0xD0)
#define SSP_SLIC_GROUNDSHORT            _IO   ('q', 0xD1)
#define SSP_SLIC_POWERLEAKTEST          _IO   ('q', 0xD2)
#define SSP_SLIC_POWERUP                _IO   ('q', 0xD3)
#define SSP_SLIC_EXCEPTION              _IOW  ('q', 0xD4, int)
#define SSP_SLIC_CLEARALARMBITS         _IO   ('q', 0xD5)
#define SSP_SLIC_DTMFACTION             _IO   ('q', 0xD6)
#define SSP_SLIC_CLEAN_DTMF             _IO   ('q', 0xD7)
#define SSP_SLIC_DTMFACTION_TEST        _IO   ('q', 0xD8)
#define SSP_SLIC_DMA_TEST               _IO   ('q', 0xD9)
#define SSP_SLIC_STOP_DMA               _IO   ('q', 0xDA)
#define SSP_SLIC_GET_LINKSTATE          _IOR  ('q', 0xDB, int)
#define SSP_SLIC_SET_LINKSTATE          _IOW  ('q', 0xDC, int)
#define SSP_SLIC_GET_RDOK               _IOR  ('q', 0xDD, int)
#define SSP_SLIC_GET_WTOK               _IOR  ('q', 0xDE, int)
#define SSP_SLIC_SET_TXRATE             _IOW  ('q', 0xDF, int)
#define SSP_SLIC_SET_RINGTYPE           _IOW  ('q', 0xE0, int)
#define SSP_SLIC_GET_DTMF               _IOR  ('q', 0xE1, char[20])
#define SSP_SLIC_GET_DTMF_CNT           _IOR  ('q', 0xE2, int)
#define SSP_SLIC_SET_MODE               _IOW  ('q', 0xE3, int)
#define SSP_DIAG_LOOPBACK               _IOW  ('q', 0xE4, int)
#define SSP_SLIC_LOOPBACK               _IOW  ('q', 0xE5, int)
#define SSP_DIAG_FUNC_TEST              _IOW  ('q', 0xE6, phone_diag_cmd_t)
//#define SSP_SLIC_                     _IO   ('q', 0xDF)


#endif








