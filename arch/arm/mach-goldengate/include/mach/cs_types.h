/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2002 by Cortina Systems Incorporated.                 */
/***********************************************************************/
/*
 * cs_types.h
 *
 * Include file containing some basic and common data types and defines
 * used by driver.
 *
 * $Id$
*/
#ifndef __CS_TYPES_H__
#define __CS_TYPES_H__

/*
 * Basic data types
 */
typedef  unsigned long long     cs_uint64;
typedef  long long              cs_int64;
typedef  unsigned int           cs_uint32;
typedef  int                    cs_int32;
typedef  unsigned short         cs_uint16;
typedef  short                  cs_int16;
typedef  unsigned char          cs_uint8;
typedef  signed char            cs_int8;
typedef  cs_uint8               cs_boolean;
typedef  cs_int32               cs_status ;
typedef  cs_uint32              cs_addr ;


typedef  cs_int8                cs_int8_t;
typedef  cs_int16               cs_int16_t ;
typedef  cs_int32               cs_int32_t ;
typedef  cs_uint8               cs_uint8_t;
typedef  cs_uint16              cs_uint16_t ;
typedef  cs_uint32              cs_uint32_t ;

typedef  cs_uint16              cs_dev_id_t ;
typedef  cs_uint16              cs_port_id_t ;
typedef int                     cs_cos_t;
typedef int                     cs_queue_t;
typedef  unsigned int           cs_tunnel_id_t;
typedef  unsigned int           cs_rtp_id_t;
typedef  unsigned short         cs_session_id_t;
typedef  unsigned short         cs_ppp2_pro_t;
typedef  unsigned int           cs_ip_translate_id_t;



/* FIXME: re-visit CS_UNION and CS_DI - for now ported over from Cortina */
#ifndef  CS_UNION
#define  CS_UNION               union
#endif

#ifndef CS_DONT_USE_DESGNTD_INITLZR
#define  CS_DI(x) x
#else
#define  CS_DI(x)
#endif

#ifndef __LINE__
#define __LINE__ 0
#endif
#ifndef __FILE__
#define __FILE__ "unknown"
#endif

/* Conflict with Linux definition
#ifndef __FUNCTION__
#define __FUNCTION__ __FILE__
#endif
*/

/*
 * MAC address struct
 */
typedef struct {
  cs_uint8      byte5 ;
  cs_uint8      byte4 ;
  cs_uint8      byte3 ;
  cs_uint8      byte2 ;
  cs_uint8      byte1 ;
  cs_uint8      byte0 ;
} cs_mac_t ;

/*
 * Register data type
 */
typedef cs_uint32 cs_reg;

/*
 * Other typedef's
 */
typedef enum {
  CS_DISABLE   = 0,
  CS_ENABLE    = 1
} cs_ctl_t ;

typedef enum {
  CS_RESET_DEASSERT    = 0,
  CS_RESET_ASSERT      = 1,
  CS_RESET_TOGGLE      = 2
} cs_reset_action_t ;

typedef enum {
  CS_TX       = 0,
  CS_RX,
  CS_TX_AND_RX,
  CS_RX_AND_TX = CS_TX_AND_RX
} cs_dir_t ;

typedef enum {
  CS_OP_READ,
  CS_OP_WRITE
} cs_rw_t ;

typedef enum {
  CS_FALSE   = 0,
  CS_TRUE    = 1
} cs_boolean_t;

/*
 * Chip(Driver) Type
 */
typedef enum {
  CS_CHIP_UNKNOWN  = 0,
  CS_CHIP_MISANO,
  CS_CHIP_DAYTONA,
  CS_CHIP_ESTORIL,
  CS_CHIP_SUNI_TETRA
} cs_chip_type_t ;


#ifndef CS_IRQ_INFO_DEFINED
#define CS_IRQ_INFO_DEFINED
/*
 * Interrupt info data-structure - driver fills in this
 * structure before calling the user macro, CS_IRQ_USER_HANDLER()
 */
typedef struct {
  cs_chip_type_t   chip ;        /* Chip Name         */
  cs_uint32        mod_id ;      /* Module/Block Id   */
  cs_uint32        grp_id ;      /* Group Id          */
  const char     * grp_name ;    /* Group name string */
  cs_uint16        irq_id ;      /* IRQ Id            */
  const char     * irq_id_name ; /* IRQ Id name string */
  cs_uint16        flags ;       /* flags : contain info like status valid,
                                    param1 valid and param2 valid    */
  cs_uint16        status ;      /* Status value, if status is valid */
  cs_uint32        param1 ;      /* param1 value, if param1 is valid */
  cs_uint32        param2 ;      /* param2 value, if param2 is valid */
} cs_irq_info_t ;

#define CS_IRQ_INFO_STATUS_FLAG        (0x0001) /* status field valid */
#define CS_IRQ_INFO_PARAM1_FLAG        (0x0002) /* param1 field valid */
#define CS_IRQ_INFO_PARAM2_FLAG        (0x0004) /* param2 field valid */
#define CS_IRQ_INFO_SHRD_DEVICE_FLAG   (0x0008) /* shared block       */

#endif /* CS_IRQ_INFO_DEFINED */

/*
 * Other defines
 */
#define CS_DONE			1
#define CS_OK           0
#define CS_ERROR        -1
#define CS_ERR_ENTRY_NOT_FOUND	-2


typedef enum {
	CS_E_ERROR    = CS_ERROR,
	CS_E_OK       = CS_OK,
	CS_E_NONE     = CS_OK,
	CS_E_INIT,
	CS_E_DEV_ID,
	CS_E_PORT_ID,
	CS_E_RESOURCE,
	CS_E_PARAM,
	CS_E_DIR,
	CS_E_EXISTS,
	CS_E_NULL_PTR,
	CS_E_NOT_FOUND,
	CS_E_CONFLICT,
	CS_E_TIMEOUT,
	CS_E_INUSING,
	CS_E_NOT_SUPPORT,
	CS_E_MCAST_ADDR_EXISTS,
	CS_E_MCAST_ADDR_ADD_FAIL,
	CS_E_MCAST_ADDR_DELETE_FAIL,
	CS_E_MEM_ALLOC,
	CS_E_NEXTHOP_INVALID,
	CS_E_NEXTHOP_NOT_FOUND,
	CS_E_ROUTE_MAX_LIMIT,
	CS_E_ROUTE_NOT_FOUND,

} cs_status_t;


#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#ifndef NULL
#define NULL            0
#endif

#ifndef IN
#define IN
#define CS_IN

#endif

#ifndef OUT
#define OUT
#define CS_OUT
#endif

#ifndef CS_IN_OUT
#define CS_IN_OUT
#endif

#ifdef CS_DONT_USE_INLINE
#       define CS_INLINE        static
#else
#       define CS_INLINE        __inline__ static
#endif

#endif /* __CS_TYPES_H__ */

