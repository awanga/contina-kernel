#ifndef __DMA_H
#define __DMA_H

#define		G2_TS_MAX_CHANNEL		(6)	/* Maximum channel number of TS module. */
#define		G2_TS_MAX_PID_ENTRY		(256)	/* Total pid entries for each RX queue. */
#define		G2_TS_MAX_QUEUE_NUM		(12)	/* Total RX queue number for TS Module. */
#define 	G2_TS_PID_TABLE_MAX_ENTRY   	(32)    /* maximum PID filter entry of each channel */

#define		G2_TS_SW_RXQ_DEPTH		(6)	/* S/W rx queue depth for RX DMA. */
#define		G2_TS_FULL_THRESHOLD_DEPTH	(2)	/* Full threshold depth. */
#define		G2_TS_SW_RXQ_DESC_NUM		(1 << G2_TS_SW_RXQ_DEPTH)	/* RX DMA descriptor number. */

#define		G2_TS_RX_DATA_SIZE		(188*3) /* (32K-byte/188)*188=31584--> Need to be fine tune */
#define		G2_TS_RX_CONTROL_SIZE		(188)

#define 	G2_TS_DISABLE_ALL_INT		(0x00000000)
#define 	G2_TS_ENABLE_ALL_INT		(0xffffffff)

#define 	G2_TS_DMA_DESC_INT		(0x00000001)
#define 	G2_TS_DMA_RXQ_INT		(0x00001ffe)
#define 	G2_TS_DMA_AXI_INT		(0x0000e000)
#define 	G2_TS_DMA_RXPID_INT		(0x003f0000)

#define 	G2_TS_RXQ_EOF_FULL		(0x00000003)

#define		TS_GLOBAL_GPIO_MUX_1		(0x1FE00000)
#define		TS_GLOBAL_GPIO_MUX_2		(0x3FFFC000)

typedef enum {
	CS_TS_DATA   	= 0,
	CS_TS_CONTROL	= 1
} cs_ts_type_t ;


typedef union {
	unsigned int bits32;
	struct bit_202c
	{
		unsigned int buf_size		: 16;	/* bit 15:0 */
		unsigned int desc_cnt		:  6;	/* bit 21:16 */
		unsigned int rq_status		:  7;	/* bit 28:22 */
		unsigned int cache		:  1;	/* bit 29 */
		unsigned int share		:  1;	/* bit 30 */
		unsigned int own		:  1;	/* bit 31 */
	} bits;
} G2_TS_RX_DESC_0_T;


typedef struct {
	unsigned int buf_addr;
} G2_TS_RX_DESC_1_T;


typedef union {
	unsigned int bits32;
	struct bit_2034
	{
		unsigned int frame_size		:16;	/* bit 15:0 */
		unsigned int Rq_flag0		:16;	/* bit 31:16 */
	} bits;
} G2_TS_RX_DESC_2_T;

typedef struct {
	unsigned int	rq_flag1;
} G2_TS_RX_DESC_3_T;

typedef struct {
	G2_TS_RX_DESC_0_T	word0;
	G2_TS_RX_DESC_1_T	word1;
	G2_TS_RX_DESC_2_T	word2;
	G2_TS_RX_DESC_3_T	word3;
} G2_TS_RX_DESC_T;

typedef struct {
	unsigned short	pid[G2_TS_PID_TABLE_MAX_ENTRY];
	unsigned char	cur_idx;
} G2_TS_RXPID_TABLE_T;

typedef struct rxq_private{
	unsigned int	rxq_desc_base;		/* RX DMA descriptor virtual base address */
	dma_addr_t	rxq_desc_base_dma;	/* RX DMA descriptor physical base address */
} G2_TS_DESC_ADDR_T ;

typedef struct {
	unsigned char	qid;		/* 0 ~ 11 */
	unsigned char	channel;	/* 0 ~ 5 */
	cs_ts_type_t	type;		/* 0-data 1-control */
	unsigned char	depth;		/* queue depth */
	unsigned int	size;		/* descriptor buffer size */
	unsigned int	init_done;	/* 1:DMA buffer initial done 0:not done */
} G2_TS_RXQ_MAP_T;

typedef struct ts_private {
	/* dvb */
	struct dvb_adapter 	dvb_adapter;
	struct dvb_frontend 	*frontend[G2_TS_MAX_CHANNEL];
	struct dvb_demux 	demux[G2_TS_MAX_QUEUE_NUM];
	struct dmxdev 		dmxdev[G2_TS_MAX_QUEUE_NUM];
	struct dmx_frontend 	fe_hw;
	struct dmx_frontend 	fe_mem;
	unsigned int 		users;
	unsigned int 		full_ts_users;
	struct device		*dev;

	/* dma & irq */
	void __iomem		*base;
	int			irq;
	G2_TS_DESC_ADDR_T	dma_desc[G2_TS_MAX_QUEUE_NUM];
	G2_TS_RXQ_MAP_T		rxq_map[G2_TS_MAX_QUEUE_NUM];

	/* i2c */
	struct i2c_adapter 	*i2c_adap;

} G2_TS_INFO_T;

extern G2_TS_INFO_T tsdvb_private_data;



/***********************************************/
/*     TS RX Control and PID Table 0 ~ 5       */
/***********************************************/

#define	G2_TS_MAX_SYNC_BYTE_COUNT	3

typedef struct {
	cs_uint8	index;	/* 0~2 */
	cs_uint8	sync_byte;
	cs_ctl_t	sync_byte_en;
} G2_TS_RXPID_SYNC_T;

typedef struct {
	cs_uint16	pid;
	cs_uint8	qid0;
	cs_uint16	new_pid0;
	cs_boolean	action0;
	cs_boolean	valid0;
	cs_uint8	qid1;
	cs_uint16	new_pid1;
	cs_boolean	action1;
	cs_boolean	valid1;
} G2_TS_RXPID_ENTRY_T;


/***********************************************/
/*                TS DMA                       */
/***********************************************/
typedef struct {
	cs_ctl_t	rx_dma_enable;
	cs_ctl_t	rx_check_own;
	cs_uint8	rx_burst_len;
} G2_TS_RXDMA_CONTROL;

#endif
