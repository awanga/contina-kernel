#ifndef __CS_NE_IOCTL_H__
#define __CS_NE_IOCTL_H__


#define SIOCDNEPRIVATE	SIOCDEVPRIVATE + 0x6	/* 0x89F0 + 0x6 */
#define	REGREAD			0
#define REGWRITE		1
#define NE_NI_IOCTL		2
#define	NE_FE_IOCTL		3
#define	NE_QM_IOCTL		4
#define	NE_SCH_IOCTL		5
#define	GMIIREG			6
#define	SMIIREG			7
#define	NE_INGRESS_QOS_IOCTL	8 /*[ingress qos]add by ethan for ingress qos*/
#define NE_VOQ_COUNTER_IOCTL	9
/* ne_cfg command for IPLIP TUNNEL control plane */
#define NE_TUNNEL_IOCTL		10

/* Table id */
typedef enum {
	CS_IOTCL_TBL_CLASSIFIER,    /* 0 */
	CS_IOCTL_TBL_SDB,
	CS_IOCTL_TBL_HASH_MASK,
	CS_IOCTL_TBL_LPM,
	CS_IOCTL_TBL_HASH_MATCH,

	CS_IOCTL_TBL_FWDRSLT,       /* 5 */
	CS_IOCTL_TBL_QOSRSLT,
	CS_IOCTL_TBL_L3_IP,
	CS_IOCTL_TBL_L2_MAC,
	CS_IOCTL_TBL_VOQ_POLICER,

	CS_IOCTL_TBL_LPB,           /* 10 */
	CS_IOCTL_TBL_AN_BNG_MAC,
	CS_IOCTL_TBL_PORT_RANGE,
	CS_IOCTL_TBL_VLAN,
	CS_IOCTL_TBL_ACL_RULE,

	CS_IOCTL_TBL_ACL_ACTION,    /* 15 */
	CS_IOCTL_TBL_PE_VOQ_DROP,
	CS_IOCTL_TBL_ETYPE,
	CS_IOCTL_TBL_LLC_HDR,
	CS_IOCTL_TBL_FVLAN,
	CS_IOCTL_TBL_HASH_HASH,     /* 20 */
	CS_IOCTL_TBL_HASH_CHECK,
	CS_IOCTL_TBL_PKTLEN,
	/*[begin][ingress qos]add by ethan for ingress qos*/
	CS_IOCTL_FE_TBL_MAX,       /*21*/	
	
	CS_IOCTL_TBL_INGRESS_QOS_TABLE, /* 22 */
	CS_IOCTL_TBL_INGRESS_QOS_SHAPER_VOQ_TABLE,
	CS_IOCTL_TBL_INGRESS_QOS_SHAPER_PORT_TABLE,
	CS_IOCTL_TBL_INGRESS_QOS_API,
	/*[end][ingress qos]add by ethan for ingress qos*/
	CS_IOCTL_TBL_INGRESS_QOS_MAX,

	CS_IOCTL_TBL_VOQ_COUNTER_API, /* 27 */
	/* ne_cfg command for IPLIP TUNNEL control plane */
	CS_IOCTL_TBL_TUNNEL_IPLIP_API, /* 28 */
} cs_ioctl_table_id_e;


/* Table Command  */
typedef enum
{
	CMD_ADD,
	CMD_DELETE,
	CMD_FLUSH,
	CMD_GET,
	CMD_REPLACE,
	CMD_INIT,
	/*[begin][ingress qos]add by ethan for ingress qos*/
	CS_IOCTL_CMD_MAX,	/* 6 */
	
	CMD_INGRESS_QOS_SET,				/* 7 */
	CMD_INGRESS_QOS_RESET,				/* 8 */
	/*[end][ingress qos]add by ethan for ingress qos*/
	CMD_INGRESS_QOS_MAX,
	
	CMD_VOQ_COUNTER_SET,				/* 10 */
	CMD_VOQ_COUNTER_RESET,				/* 11 */
	CMD_VOQ_COUNTER_MAX,
}COMMAND_DEF;


typedef struct {
	unsigned short		cmd;	/* command ID */
	unsigned short		len;	/* data length, excluding this header */
} NECMD_HDR_T;


/* REGREAD */
typedef struct {
	unsigned int		location;
	unsigned int		length;
	unsigned int		size;
} REGREAD_T;

/* REGWRITE */
typedef struct {
	unsigned int		location;
	unsigned int		data;
	unsigned int		size;
} REGWRITE_T;

/* GMIIREG */
typedef	struct{
	unsigned short		phy_addr;
	unsigned short		phy_reg;
	unsigned short		phy_len;
} GMIIREG_T;

/* SMIIREG */
typedef	struct{
	unsigned short		phy_addr;
	unsigned short		phy_reg;
	unsigned int		phy_data;
} SMIIREG_T;

typedef union
{
	REGREAD_T reg_read;
	REGWRITE_T	reg_write;
	GMIIREG_T get_mii_reg;
	SMIIREG_T set_mii_reg;
} NE_REQ_E;

typedef struct {
    unsigned char       Module;         // reference MODULE_DEF
    unsigned char       table_id;       // reference xx_TABLE_DEF
    unsigned char       cmd;            // reference COMMAND_DEF
    int                 Bypass;         // 0: Disable, 1: Enable
    unsigned short		idx_start;      // defined for command GET
    unsigned short		idx_end;        // defined for command GET
} NEFE_CMD_HDR_T;

typedef enum
{
	MODULE_NI,
	MODULE_FE,
	MODULE_TM,
	MODULE_SCH,
	MODULE_QM,
	MODULE_Ingress_QOS, //[ingress qos]add by ethan for ingress qos
	MODULE_VOQ_COUNTER,
	/* ne_cfg command for IPLIP TUNNEL control plane */
	MODULE_TUNNEL,
	MODULE_NULL,
} MODULE_DEF;

//[begin][ingress qos]add by ethan for ingress qos
typedef enum
{
	PRIORITY_SP,
	PRIORITY_DRR,
	PRIORITY_NULL,
}QOS_PRIORITY_DEF;
//[end][ingress qos]

/* Ingress QoS sub commands */
typedef enum {
	CS_QOS_INGRESS_SET_MODE,			/*  0 */
	CS_QOS_INGRESS_GET_MODE,
	CS_QOS_INGRESS_PRINT_MODE,

	CS_QOS_INGRESS_SET_PORT_PARAM,
	CS_QOS_INGRESS_GET_PORT_PARAM,
	CS_QOS_INGRESS_PRINT_PORT_PARAM,		/*  5 */

	CS_QOS_INGRESS_SET_QUEUE_SCHEDULER,
	CS_QOS_INGRESS_GET_QUEUE_SCHEDULER,
	CS_QOS_INGRESS_PRINT_QUEUE_SCHEDULER,
	CS_QOS_INGRESS_PRINT_QUEUE_SCHEDULER_OF_PORT,

	CS_QOS_INGRESS_SET_QUEUE_SIZE,			/* 10 */
	CS_QOS_INGRESS_GET_QUEUE_SIZE,
	CS_QOS_INGRESS_PRINT_QUEUE_SIZE,
	CS_QOS_INGRESS_PRINT_QUEUE_SIZE_OF_PORT,

	CS_QOS_INGRESS_SET_VALUE_QUEUE_MAPPING,
	CS_QOS_INGRESS_GET_VALUE_QUEUE_MAPPING,		/* 15 */
	CS_QOS_INGRESS_PRINT_VALUE_QUEUE_MAPPING,

	CS_QOS_INGRESS_SET_ARP_POLICER,
	CS_QOS_INGRESS_RESET_ARP_POLICER,
	CS_QOS_INGRESS_GET_ARP_POLICER,
	CS_QOS_INGRESS_PRINT_ARP_POLICER,		/* 20 */

	CS_QOS_INGRESS_SET_PKT_TYPE_POL,
	CS_QOS_INGRESS_RESET_PKT_TYPE_POL,
	CS_QOS_INGRESS_GET_PKT_TYPE_POL,
	CS_QOS_INGRESS_PRINT_PKT_TYPE_POL,
	CS_QOS_INGRESS_PRINT_PKT_TYPE_POL_PORT,		/* 25 */
	CS_QOS_INGRESS_PRINT_PKT_TYPE_POL_ALL,

	CS_QOS_INGRESS_MAX,

} cs_qos_ingress_ioctl_sub_cmd_e;

typedef struct {
	unsigned int sub_cmd; /* refer to cs_qos_ingress_ioctl_sub_cmd_e */
	/* parameters for commands */
	union {
		/* cs_qos_ingress_set_mode(u8 mode) */
		/* cs_qos_ingress_get_mode(u8 *mode) */
		/* cs_qos_ingress_print_mode(void) */
		struct cs_qos_ingress_mode {
			unsigned char mode;
		} mode;

		/* cs_qos_ingress_set_port_param(u8 port_id, u16 burst_size, u32 rate) */
		/* cs_qos_ingress_get_port_param(u8 port_id, u16 *burst_size, u32 *rate) */
		/* cs_qos_ingress_print_port_param(u8 port_id) */
		struct cs_qos_ingress_port_param {
			unsigned char port_id;
			unsigned short burst_size;
			unsigned int rate;
		} port_param;

		/* cs_qos_ingress_set_queue_scheduler(u8 port_id, u8 queue_id, u8 priority, u32 weight, u32 rate) */
		/* cs_qos_ingress_get_queue_scheduler(u8 port_id, u8 queue_id, u8 *priority, u32 *weight, u32 *rate) */
		/* cs_qos_ingress_print_queue_scheduler(u8 port_id, u8 queue_id) */
		/* cs_qos_ingress_print_queue_scheduler_of_port(u8 port_id) */
		struct cs_qos_ingress_queue_scheduler {
			unsigned char port_id;
			unsigned char queue_id;
			unsigned char priority;
			unsigned int weight;
			unsigned int rate;
		} queue_scheduler;
	
		/* cs_qos_ingress_set_queue_size(u8 port_id, u8 queue_id, u32 rsrv_size, u32 max_size) */
		/* cs_qos_ingress_get_queue_size(u8 port_id, u8 queue_id, u32 *rsrv_size, u32 *max_size) */
		/* cs_qos_ingress_print_queue_size(u8 port_id, u8 queue_id) */
		/* cs_qos_ingress_print_queue_size_of_port(u8 port_id) */
		struct cs_qos_ingress_queue_size {
			unsigned char port_id;
			unsigned char queue_id;
			unsigned int rsrv_size;
			unsigned int max_size;
		} queue_size;
		
		/* cs_qos_ingress_set_value_queue_mapping(u8 value, u8 queue_id) */
		/* cs_qos_ingress_get_value_queue_mapping(u8 value, u8 *queue_id) */
		/* cs_qos_ingress_print_value_queue_mapping(void) */
		struct cs_qos_ingress_queue_mapping {
			unsigned char value;
			unsigned char queue_id;
		} queue_mapping;

		struct cs_qos_ingress_arp_policer {
			unsigned int cir;
			unsigned int cbs;
			unsigned int pir;
			unsigned int pbs;
		} arp_policer;

		struct {
			unsigned char port_id;
			unsigned char pkt_type;
			unsigned int cir;
			unsigned int cbs;
			unsigned int pir;
			unsigned int pbs;
		} pkt_type_policer;
	};
		
	/* return value from ingress QoS APIs */
	int ret;
} cs_qos_ingress_api_entry_t;

/* VOQ Counter sub commands */
typedef enum {
	CS_VOQ_COUNTER_ADD,			/*  0 */
	CS_VOQ_COUNTER_DELETE,
	CS_VOQ_COUNTER_GET,
	CS_VOQ_COUNTER_PRINT,
	CS_VOQ_COUNTER_SET_READ_MODE,
	CS_VOQ_COUNTER_GET_READ_MODE,
	CS_VOQ_COUNTER_PRINT_READ_MODE,

	CS_VOQ_COUNTER_MAX,
} cs_voq_counter_ioctl_sub_cmd_e;

/* ne_cfg command for IPLIP TUNNEL control plane */
typedef enum {
	CS_IPLIP_PPPOE_PORT_ADD,		/*  0 */
	CS_IPLIP_PPPOE_PORT_DELETE,
	CS_IPLIP_PPPOE_PORT_ENCAP_SET,
	CS_IPLIP_PPPOE_PORT_ENCAP_GET,
	CS_IPLIP_PPPOE_PORT_SET,
	CS_IPLIP_PPPOE_PORT_GET,
	CS_IPLIP_TUNNEL_ADD,
	CS_IPLIP_TUNNEL_DELETE,
	CS_IPLIP_TUNNEL_DELETE_BY_IDX,
	CS_IPLIP_TUNNEL_GET,
	CS_IPLIP_L2TP_SESSION_ADD,
	CS_IPLIP_L2TP_SESSION_DELETE,
	CS_IPLIP_L2TP_SESSION_GET,
	CS_IPLIP_IPV6_OVER_L2TP_ADD,
	CS_IPLIP_IPV6_OVER_L2TP_DELETE,
	CS_IPLIP_IPV6_OVER_L2TP_GETNEXT,
	CS_IPLIP_MAX,
} cs_tunnel_iplip_ioctl_sub_cmd_e;

/* this one should match with the TM_PM read mode definition */
typedef enum {
	CS_VOQ_COUNTER_READ_MODE_NO_CLEAR = 0,
	CS_VOQ_COUNTER_READ_MODE_CLEAR_ALL = 1,
	CS_VOQ_COUNTER_READ_MODE_CLEAR_MSB = 2,
	CS_VOQ_COUNTER_READ_MODE_MAX,
} cs_voq_counter_read_mode_e;

typedef struct {
	unsigned int sub_cmd; /* refer to cs_voq_counter_ioctl_sub_cmd_e */

	/* parameters for commands */
	union {
		struct {
			unsigned char voq_id;
			unsigned int pkts;
			unsigned int pkts_mark;
			unsigned int pkts_drop;
			unsigned long long bytes;
			unsigned long long bytes_mark;
			unsigned long long bytes_drop;
		} param;
		unsigned char read_mode;
	};
		
	/* return value from VOQ counter APIs */
	int ret;
} cs_voq_counter_api_entry_t;

/* You can put all IOCTL structure here 
   for Ex. REGREAD, REGWRITE, GMIIREG... */

#endif//__CS_NE_IOCTL_H__
