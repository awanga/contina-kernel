/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <wen.hsu@cortina-systems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CS_CORE_VTABLE_H__
#define __CS_CORE_VTABLE_H__

#include "cs_vtable.h"

#if defined(CONFIG_CS75XX_HW_ACCEL_IPLIP) || \
	defined(CONFIG_CS75XX_HW_ACCEL_PPTP) || \
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC)
#define CS75XX_HW_ACCEL_TUNNEL
#endif

/* Vtable (classification) type definition */
typedef enum {
	CORE_VTABLE_TYPE_NONE,
	CORE_VTABLE_TYPE_BCAST,
	CORE_VTABLE_TYPE_L2_MCAST,
	CORE_VTABLE_TYPE_L3_MCAST_V4,
	CORE_VTABLE_TYPE_L3_MCAST_V4_MCGID128,
	CORE_VTABLE_TYPE_L3_MCAST_V6,
	CORE_VTABLE_TYPE_L2_FLOW,
#ifdef CS75XX_HW_ACCEL_TUNNEL
	CORE_VTABLE_TYPE_L3_TUNNEL,
#endif
	CORE_VTABLE_TYPE_L3_FLOW,
#ifdef CS75XX_HW_ACCEL_TUNNEL
	CORE_VTABLE_TYPE_RE0_TUNNEL,
#endif
	CORE_VTABLE_TYPE_RE0,
#ifdef CS75XX_HW_ACCEL_TUNNEL
	CORE_VTABLE_TYPE_RE1_TUNNEL,
#endif
	CORE_VTABLE_TYPE_RE1,
	CORE_VTABLE_TYPE_CPU,
	CORE_VTABLE_TYPE_ARP,
	CORE_VTABLE_TYPE_ICMPV6,
#ifdef CONFIG_CS75XX_WFO
	CORE_VTABLE_TYPE_RE0_WFO_L3,
	CORE_VTABLE_TYPE_RE1_WFO_L3,
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	CORE_VTABLE_TYPE_CPU_L3,
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS
	CORE_VTABLE_TYPE_L2_IPSEC,
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_PASS
	CORE_VTABLE_TYPE_L2_L2TP,
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS,
	CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS,
#endif
	/*ADD CHECKSUM hashmask for v6*/
#ifdef CS75XX_HW_ACCEL_TUNNEL
	CORE_VTABLE_TYPE_L3_TUNNEL_V6,
	CORE_VTABLE_TYPE_RE0_TUNNEL_V6,
	CORE_VTABLE_TYPE_RE1_TUNNEL_V6,
#endif
	CORE_VTABLE_TYPE_L3_FLOW_V6,
#ifdef CONFIG_CS75XX_WFO
	CORE_VTABLE_TYPE_RE0_WFO_L3_V6,
	CORE_VTABLE_TYPE_RE1_WFO_L3_V6,
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	CORE_VTABLE_TYPE_CPU_L3_V6,
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS_V6,
	CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS_V6,
#endif
	CORE_VTABLE_TYPE_MAX,
} cs_vtable_type_e;

/*Vtables for packets from GMAC#*/
#define BCAST_DEF_VTABLE_PRIORITY			41
#define L2_MCAST_DEF_VTABLE_PRIORITY 		38
#define L3_MCAST_DEF_VTABLE_PRIORITY 		40
#define L2_FLOW_DEF_VTABLE_PRIORITY			10
#define L3_FLOW_DEF_VTABLE_PRIORITY			20
#define L3_FLOW_V6_DEF_VTABLE_PRIORITY		(L3_FLOW_DEF_VTABLE_PRIORITY + 1)
#define L3_TUNNEL_DEF_VTABLE_PRIORITY		(L3_FLOW_V6_DEF_VTABLE_PRIORITY + 1)
#define L3_TUNNEL_V6_DEF_VTABLE_PRIORITY	(L3_TUNNEL_DEF_VTABLE_PRIORITY + 1)
#define ICMPV6_DEF_VTABLE_PRIORITY			43
#define ARP_DEF_VTABLE_PRIORITY				45

/*Vtables for packets from Packet engine*/
#define RE_SPECIFIC_DEF_VTABLE_PRIORITY			50
#define RE_SPECIFIC_L3_DEF_VTABLE_PRIORITY		(RE_SPECIFIC_DEF_VTABLE_PRIORITY + 1)
#define RE_SPECIFIC_L3_V6_DEF_VTABLE_PRIORITY 	(RE_SPECIFIC_L3_DEF_VTABLE_PRIORITY + 1)
#define RE_TUNNEL_L3_DEF_VTABLE_PRIORITY		(RE_SPECIFIC_L3_V6_DEF_VTABLE_PRIORITY + 1)
#define RE_TUNNEL_L3_V6_DEF_VTABLE_PRIORITY		(RE_TUNNEL_L3_DEF_VTABLE_PRIORITY + 1)

/* NATT priority must be higher than CORE_VTABLE_TYPE_L3_TUNNEL,
 * CORE_VTABLE_TYPE_RE0_TUNNEL, CORE_VTABLE_TYPE_RE1_TUNNEL
 */
#define RE_NATT_EGRESS_L3_DEF_VTABLE_PRIORITY	(RE_TUNNEL_L3_V6_DEF_VTABLE_PRIORITY + 1)
#define RE_NATT_EGRESS_L3_V6_DEF_VTABLE_PRIORITY	(RE_NATT_EGRESS_L3_DEF_VTABLE_PRIORITY + 1)
#define RE_NATT_INGRESS_L3_DEF_VTABLE_PRIORITY	(RE_NATT_EGRESS_L3_V6_DEF_VTABLE_PRIORITY + 1)
#define RE_NATT_INGRESS_L3_V6_DEF_VTABLE_PRIORITY	(RE_NATT_INGRESS_L3_DEF_VTABLE_PRIORITY + 1)

typedef enum {
	CORE_VTABLE_TYPE_L2_RULE = CORE_VTABLE_TYPE_MAX,
	CORE_VTABLE_TYPE_L3_RULE_PREROUTING,
	CORE_VTABLE_TYPE_L3_RULE_FORWARD,
	CORE_VTABLE_TYPE_L3_RULE_POSTROUTING,
	CORE_VTABLE_RULE_TYPE_MAX
} cs_core_rule_vtable_type_e;

/* Application Type definition */
typedef enum {
	CORE_FWD_APP_TYPE_NONE,
	CORE_FWD_APP_TYPE_L2_MCAST,
	CORE_FWD_APP_TYPE_L3_MCAST,
	CORE_FWD_APP_TYPE_SA_CHECK,
	CORE_FWD_APP_TYPE_L2_FLOW,
	CORE_FWD_APP_TYPE_L3_GENERIC,
	CORE_FWD_APP_TYPE_L3_IPSEC,
	CORE_FWD_APP_TYPE_IPSEC_FROM_RE,
	CORE_FWD_APP_TYPE_SEPARATE_LOGICAL_PORT,
	CORE_FWD_APP_TYPE_L4_SPORT,
	CORE_FWD_APP_TYPE_L4_DPORT,
	CORE_FWD_APP_TYPE_PE_RECIDX,
	CORE_FWD_APP_TYPE_IP_PROT,
#ifdef CONFIG_CS75XX_MTU_CHECK
	/* NEC MTU CHECK Requirement */
	CORE_FWD_APP_TYPE_L3_MTU_IPOE,
	CORE_FWD_APP_TYPE_L3_MTU_PPPOE,
	CORE_FWD_APP_TYPE_L3_MTU_IPLIP,
	CORE_FWD_APP_TYPE_L3_MTU_IPSEC,
#endif
	CORE_FWD_APP_TYPE_MCAST_TO_DEST,
	CORE_FWD_APP_TYPE_MCAST_WITHOUT_SRC,
	CORE_FWD_APP_TYPE_MCAST_WITH_SRC,
	CORE_FWD_APP_TYPE_TUNNEL,
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	CORE_FWD_APP_TYPE_TUNNEL_L4_L7,
#endif
	CORE_FWD_APP_TYPE_L7_GENERIC,
	CORE_FWD_APP_TYPE_IPLIP_LAN,
	CORE_FWD_APP_TYPE_MCAST_CTRL_IPTV,
	CORE_FWD_APP_TYPE_MCAST_CTRL_IP_SA,
	CORE_FWD_APP_TYPE_MCAST_L7_FILTER,
#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS) || defined(CONFIG_CS75XX_HW_ACCEL_L2TP_PASS)
	CORE_FWD_APP_TYPE_L2_PASS,
#endif
	CORE_FWD_APP_TYPE_L3_GENERIC_WITH_CHKSUM,
#ifdef CONFIG_CS75XX_MTU_CHECK
	CORE_FWD_APP_TYPE_L3_MTU_IPOE_WITH_CHKSUM,
	CORE_FWD_APP_TYPE_L3_MTU_PPPOE_WITH_CHKSUM,
	CORE_FWD_APP_TYPE_L3_MTU_IPLIP_WITH_CHKSUM,
	CORE_FWD_APP_TYPE_L3_MTU_IPSEC_WITH_CHKSUM,
#endif
	CORE_FWD_APP_TYPE_MAX,
} cs_core_fwd_app_type_e;

typedef enum {
	CORE_QOS_APP_TYPE_NONE = CORE_FWD_APP_TYPE_MAX,
	CORE_QOS_APP_TYPE_L2_QOS_1,
	CORE_QOS_APP_TYPE_L2_QOS_2,
	CORE_QOS_APP_TYPE_L3_QOS_GENERIC,
	CORE_QOS_APP_TYPE_L3_QOS_MULTICAST,
	CORE_QOS_APP_TYPE_L4_QOS_NAT,
	CORE_QOS_APP_TYPE_L7_QOS_GENERIC,
	CORE_QOS_APP_TYPE_L3_QOS_GENERIC_WITH_CHKSUM,
	CORE_QOS_APP_TYPE_MCAST_CTRL_IPTV,
	CORE_QOS_APP_TYPE_1P_DSCP_MAP,
	CORE_QOS_APP_TYPE_MCAST_L7_FILTER_IPTV,
	CORE_QOS_APP_TYPE_MAX,
} cs_core_qos_app_type_e;

#define CORE_APP_TYPE_MAX	CORE_QOS_APP_TYPE_MAX

/* Hash Mask Defintion */
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_VLAN	(CS_HM_VID_1_MASK | CS_HM_VID_2_MASK |\
		CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK |\
		CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK |\
		CS_HM_DEI_1_MASK | CS_HM_DEI_2_MASK)
#define CS_HASHMASK_VLAN_DOT1P	(CS_HM_VID_1_MASK | CS_HM_VID_2_MASK |\
		CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK |\
		CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK |\
		CS_HM_8021P_1_MASK | CS_HM_8021P_2_MASK | CS_HM_DEI_1_MASK|\
		CS_HM_DEI_2_MASK)
#else
#define CS_HASHMASK_VLAN	(CS_HM_VID_1_MASK | CS_HM_VID_2_MASK |\
		CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK |\
		CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK |\
		CS_HM_8021P_1_MASK | CS_HM_8021P_2_MASK | CS_HM_DEI_1_MASK|\
		CS_HM_DEI_2_MASK)
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
#ifdef SA_CHECK_ENABLE
#define CS_HASHMASK_MAC		(CS_HM_MAC_DA_MASK | CS_HM_ETHERTYPE_MASK)
#else
#define CS_HASHMASK_MAC		(CS_HM_MAC_DA_MASK | CS_HM_MAC_SA_MASK |\
		CS_HM_ETHERTYPE_MASK)
#endif
#define CS_HASHMASK_PPPOE	(CS_HM_PPPOE_SESSION_ID_VLD_MASK |\
		CS_HM_PPPOE_SESSION_ID_MASK)
#define CS_HASHMASK_L2		(CS_HASHMASK_VLAN | CS_HASHMASK_MAC |\
		CS_HASHMASK_PPPOE)
#define CS_HASHMASK_L2_DSCP (CS_HASHMASK_L2 | CS_HM_DSCP_MASK | CS_HM_IP_VLD_MASK) //Bug#40322
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L2_DOT1P    (CS_HASHMASK_VLAN_DOT1P | CS_HASHMASK_MAC |\
		CS_HASHMASK_PPPOE)
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#define CS_HASHMASK_L3_NO_DSCP	(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
		CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK)

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L3		(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
		CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK )
#define CS_HASHMASK_L3_DSCP	(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
		CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
		CS_HM_DSCP_MASK)
#else
#define CS_HASHMASK_L3		(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
		CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
		CS_HM_DSCP_MASK)
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#define CS_HASHMASK_IPV6	(CS_HM_IPV6_RH_MASK | CS_HM_IPV6_DOH_MASK |\
		CS_HM_IPV6_NDP_MASK | CS_HM_IPV6_HBH_MASK)
#define CS_HASHMASK_L4		(CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK |\
		CS_HM_L4_VLD_MASK)
#define CS_HASHMASK_L4_TCP	(CS_HASHMASK_L4 | CS_HM_TCP_CTRL_MASK)
#define CS_HASHMASK_IPSEC	(CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK)

#define CS_HASHMASK_L3_FLOW	(CS_HASHMASK_L2 |\
		CS_HASHMASK_L3 | CS_HASHMASK_L4_TCP |\
		CS_HASHMASK_IPSEC | CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_IPV6)

#define CS_HASHMASK_L3_FLOW_WITH_L4_CHKSUM (CS_HASHMASK_L3_FLOW | CS_HM_L4_CHKSUM_ERR_MASK)


#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L3_FLOW_DSCP	(CS_HASHMASK_L2 |\
		CS_HASHMASK_L3_DSCP | CS_HASHMASK_L4_TCP |\
		CS_HASHMASK_IPSEC | CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_IPV6)
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#define CS_HASHMASK_L7_FLOW	(CS_HASHMASK_L2 |\
		CS_HASHMASK_L3 | CS_HASHMASK_L4_TCP |\
		CS_HASHMASK_IPSEC | CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_IPV6 |\
		CS_HM_L7_FIELD_MASK | CS_HM_L7_FIELD_VLD_MASK | CS_HM_L7_FIELD_SEL_TCP_UDP|\
		CS_HM_L4_CHKSUM_ERR_MASK)

#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L7_FLOW_DSCP	(CS_HASHMASK_L2 |\
		CS_HASHMASK_L3_DSCP | CS_HASHMASK_L4_TCP |\
		CS_HASHMASK_IPSEC | CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_IPV6 |\
		CS_HM_L7_FIELD_MASK | CS_HM_L7_FIELD_VLD_MASK | CS_HM_L7_FIELD_SEL_TCP_UDP |\
		CS_HM_L4_CHKSUM_ERR_MASK)
#endif

#define CS_HASHMASK_L3_IPSEC_FLOW	(CS_HASHMASK_L2 |\
		CS_HASHMASK_L3_NO_DSCP | CS_HASHMASK_L4 |\
		CS_HASHMASK_IPSEC | CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_IPV6)


#ifdef SA_CHECK_ENABLE
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L2_MCAST	(CS_HM_MAC_DA_MASK | \
        CS_HASHMASK_VLAN | CS_HM_IP_VLD_MASK )
#else
#define CS_HASHMASK_L2_MCAST	(CS_HM_MAC_DA_MASK | \
        CS_HASHMASK_VLAN | CS_HM_IP_VLD_MASK | CS_HM_DSCP_MASK) //Bug#40322
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L3_MCAST	(CS_HASHMASK_L3_FLOW | \
	    CS_HM_MCIDX_MASK | CS_HM_LSPID_MASK | CS_HM_L4_CHKSUM_ERR_MASK)
#else
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L2_MCAST	(CS_HM_MAC_SA_MASK | CS_HM_MAC_DA_MASK | \
        CS_HASHMASK_VLAN | CS_HM_IP_VLD_MASK )
#else
#define CS_HASHMASK_L2_MCAST	(CS_HM_MAC_SA_MASK | CS_HM_MAC_DA_MASK | \
        CS_HASHMASK_VLAN | CS_HM_IP_VLD_MASK | CS_HM_DSCP_MASK) //Bug#40322
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L3_MCAST	(CS_HASHMASK_L3_FLOW | \
	    CS_HM_MCIDX_MASK | CS_HM_LSPID_MASK | CS_HM_L4_CHKSUM_ERR_MASK)
#endif
#ifdef CONFIG_CS75XX_OFFSET_BASED_QOS
#define CS_HASHMASK_L3_MCAST_DSCP	(CS_HASHMASK_L3_FLOW_DSCP | \
	    CS_HM_MCIDX_MASK | CS_HM_LSPID_MASK | CS_HM_L4_CHKSUM_ERR_MASK)
#endif //CONFIG_CS75XX_OFFSET_BASED_QOS

#define CS_HASHMASK_RE		(CS_HM_MAC_SA_MASK | CS_HM_IP_SA_MASK |\
		CS_HM_IP_DA_MASK | CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HASHMASK_L4_TCP | CS_HM_LSPID_MASK |\
		CS_HASHMASK_IPSEC | CS_HM_RECIRC_IDX_MASK)

#define CS_HASHMASK_IPSEC_FROM_CPU	(CS_HM_MAC_SA_MASK |\
		CS_HM_RECIRC_IDX_MASK | CS_HASHMASK_L3 | CS_HASHMASK_L4|\
		CS_HASHMASK_IPSEC)

#define CS_HASHMASK_SUPER_HASH CS_HASHMASK_L3_FLOW
//#define CS_HASHMASK_SUPER_HASH CS_HASHMASK_RE


#define CS_HASHMASK_LOGICAL_PORT CS_HM_LSPID_MASK

#define CS_HASHMASK_L4_SPORT	(CS_HM_L4_SP_MASK | CS_HM_IP_PROT_MASK)
#define CS_HASHMASK_L4_DPORT	(CS_HM_L4_DP_MASK | CS_HM_IP_PROT_MASK)

#define CS_HASHMASK_L4_SPORT_BY_LOGICAL_PORT	(CS_HASHMASK_L4_SPORT | CS_HASHMASK_LOGICAL_PORT)
#define CS_HASHMASK_L4_DPORT_BY_LOGICAL_PORT	(CS_HASHMASK_L4_DPORT | CS_HASHMASK_LOGICAL_PORT)

#define CS_HASHMASK_PE_RECIDX (CS_HM_LSPID_MASK | CS_HM_RECIRC_IDX_MASK)

#define CS_HASHMASK_IP_PROT   (CS_HM_IP_VLD_MASK | CS_HM_IP_PROT_MASK | CS_HM_LSPID_MASK)

#define CS_HASHMASK_MCAST_WITHOUT_SRC	(CS_HM_MAC_DA_MASK | CS_HM_IP_DA_MASK | CS_HM_IP_VER_MASK | CS_HM_IP_VLD_MASK | CS_HM_LSPID_MASK)
#define CS_HASHMASK_MCAST_WITH_SRC	(CS_HASHMASK_MCAST_WITHOUT_SRC | CS_HM_IP_SA_MASK | CS_HM_LSPID_MASK)

#define CS_HASHMASK_MCAST_TO_DEST	(CS_HM_MCIDX_MASK | CS_HM_LSPID_MASK | CS_HM_IP_VLD_MASK)

#define CS_HASHMASK_IPLIP_WAN	(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
		CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
		CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_PPPOE |\
		CS_HASHMASK_L4 | CS_HM_LSPID_MASK |\
		CS_HM_VID_1_MASK)

#define CS_HASHMASK_TUNNEL	(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
			CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
			CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
			CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_PPPOE |\
			CS_HASHMASK_L4 | CS_HM_LSPID_MASK |\
			CS_HASHMASK_IPSEC | CS_HM_L4_CHKSUM_ERR_MASK)

#define CS_HASHMASK_TUNNEL_L4_L7	(CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
			CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
			CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
			CS_HM_IP_FRAGMENT_MASK | CS_HASHMASK_PPPOE |\
			CS_HASHMASK_L4 | CS_HM_LSPID_MASK | CS_HM_L4_CHKSUM_ERR_MASK |\
			CS_HM_L7_FIELD_MASK | CS_HM_L7_FIELD_VLD_MASK | CS_HM_L7_FIELD_SEL_TCP_UDP)

#ifdef CONFIG_CS75XX_MTU_CHECK
#define CS_HASHMASK_IPLIP_LAN	(CS_HM_IP_SA_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
		CS_HM_IP_FRAGMENT_MASK | CS_HM_LSPID_MASK |\
		CS_HM_PKTLEN_RNG_MATCH_VECTOR_B2_MASK)
#else
#define CS_HASHMASK_IPLIP_LAN	(CS_HM_IP_SA_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
		CS_HM_IP_FRAGMENT_MASK | CS_HM_LSPID_MASK)
#endif

#define CS_HASHMASK_MCAST_CTRL (CS_HM_MAC_DA_MASK | \
		CS_HM_IP_VLD_MASK | CS_HM_IP_FRAGMENT_MASK | \
		CS_HM_L3_CHKSUM_ERR_MASK | CS_HM_IP_VER_MASK | \
		CS_HM_IP_PROT_MASK | CS_HM_IP_DA_MASK | \
		CS_HM_L4_VLD_MASK | CS_HM_MCIDX_MASK)
		
#define CS_HASHMASK_MCAST_L7_FILTER (CS_HM_MAC_DA_MASK | \
		CS_HM_IP_VLD_MASK | CS_HM_IP_FRAGMENT_MASK | \
		CS_HM_L3_CHKSUM_ERR_MASK | CS_HM_IP_VER_MASK | \
		CS_HM_IP_PROT_MASK | CS_HM_IP_DA_MASK | CS_HM_L4_DP_MASK | \
		CS_HM_L4_VLD_MASK | CS_HM_MCIDX_MASK | \
		CS_HM_L7_FIELD_MASK | CS_HM_L7_FIELD_VLD_MASK | CS_HM_L7_FIELD_SEL_TCP_UDP)

#define CS_HASHMASK_MCAST_CTRL_IP_SA (CS_HASHMASK_MCAST_CTRL | CS_HM_IP_SA_MASK)
#define CS_HASHMASK_MCAST_CTRL_IPTV (CS_HASHMASK_MCAST_CTRL	| CS_HM_L4_DP_MASK)
#define CS_HASHMASK_MCAST_L7_FILTER_IPTV (CS_HASHMASK_MCAST_L7_FILTER | CS_HM_L4_DP_MASK)
#define CS_HASHMASK_1P_DSCP_MAP (CS_HM_LSPID_MASK| CS_HM_TPID_ENC_1_MSB_MASK|\
		CS_HM_8021P_1_MASK)
	
#ifdef CONFIG_CS75XX_MTU_CHECK
#define CS_HASHMASK_L3_MTU_IPOE (CS_HASHMASK_L3_FLOW | CS_HM_PKTLEN_RNG_MATCH_VECTOR_B0_MASK)
#define CS_HASHMASK_L3_MTU_PPPOE (CS_HASHMASK_L3_FLOW | CS_HM_PKTLEN_RNG_MATCH_VECTOR_B1_MASK)
#define CS_HASHMASK_L3_MTU_IPLIP (CS_HASHMASK_L3_FLOW | CS_HM_PKTLEN_RNG_MATCH_VECTOR_B2_MASK)
#define CS_HASHMASK_L3_MTU_IPSEC (CS_HASHMASK_L3_IPSEC_FLOW| CS_HM_PKTLEN_RNG_MATCH_VECTOR_B3_MASK)
#define CS_HASHMASK_L3_MTU_IPOE_WITH_L4_CHKSUM  (CS_HASHMASK_L3_MTU_IPOE  | CS_HM_L4_CHKSUM_ERR_MASK)
#define CS_HASHMASK_L3_MTU_PPPOE_WITH_L4_CHKSUM (CS_HASHMASK_L3_MTU_PPPOE | CS_HM_L4_CHKSUM_ERR_MASK)
#define CS_HASHMASK_L3_MTU_IPLIP_WITH_L4_CHKSUM (CS_HASHMASK_L3_MTU_IPLIP | CS_HM_L4_CHKSUM_ERR_MASK)
#define CS_HASHMASK_L3_MTU_IPSEC_WITH_L4_CHKSUM (CS_HASHMASK_L3_MTU_IPSEC | CS_HM_L4_CHKSUM_ERR_MASK)

#endif

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS) || defined(CONFIG_CS75XX_HW_ACCEL_L2TP_PASS)
#define CS_HASHMASK_L2_PASS (CS_HM_LSPID_MASK |\
		CS_HM_MAC_DA_MASK | CS_HM_MAC_SA_MASK |\
		CS_HM_ETHERTYPE_MASK | CS_HASHMASK_PPPOE |\
		CS_HM_VID_1_MASK |CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK |\
		CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK |\
		CS_HM_IP_PROT_MASK | CS_HM_IP_VER_MASK |\
		CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |\
		CS_HM_IP_FRAGMENT_MASK |\
		CS_HASHMASK_IPSEC | CS_HASHMASK_L4)
#endif

#define MAX_FWD_HASH_TUPLE	6
#define MAX_QOS_HASH_TUPLE	2

typedef struct cs_core_vtable_def_hashmask_info_s {
	u16 lpm_enable;
	u16 fwdtuple_count;
	u16 qostuple_count;
	u16 mask_apptype[MAX_FWD_HASH_TUPLE + MAX_QOS_HASH_TUPLE];
} cs_core_vtable_def_hashmask_info_t;

/* API */
/* initialize the default vtables.
 * Seven default Vtables: broadcast, multicast, L2, L3, RE x2, and ARP. */
int cs_core_vtable_init(void);

/* exit the core vtable */
void cs_core_vtable_exit(void);

/* allocate a vtable with vtbl_type. Pointer to the table is returned when it
 * succeeds, or it will return NULL.  It performs allocation based on the
 * following list of case:
 * 	1) Vtbl_type is a flow vtable
 * 		a) No existing vtable in the chain => new vtable is the head
 * 		of chain.
 * 		b) Chain is not empty => new vtable will be inserted between
 * 		the last existed flow vtable and the first existed rule vtable.
 * 	2) Vtbl_type is rule vtable
 * 		a) Chain of its respective flow vtable is empty => error!!
 * 		b) Else => the logic will locate where to insert the newly
 * 		allocated vtable in its respective chain. */
cs_vtable_t *cs_core_vtable_alloc(unsigned int vtbl_type);

/* release a vtable.  This function will release a given vtable. If there is
 * vtable connected to it, it will also take care the re-chaining the vtables.
 * If the table given is the first one of the chain, then it will wipe the
 * whole chain. */
int cs_core_vtable_release(cs_vtable_t *table);

/* free vtable.  This API doesn't care about re-link of any previous and/or
 * next vtable in the same chain.  It just forcefully frees the vtable and all
 * the resources allocated. */
int cs_core_vtable_free(cs_vtable_t *table);

/* release vtable by type
 * this API releases all the vtables associated with the given vtbl_type,
 * and it takes care of the chaining too.  However, if given vtbl_type
 * is flow vtable type, then it will wipe the whole chain! */
int cs_core_vtable_release_by_type(unsigned int vtbl_type);

/* get the first occurrence of the vtable with given vtbl_type */
cs_vtable_t *cs_core_vtable_get(unsigned int vtbl_type);

/* get the head of the chain where the vtable type belongs to */
cs_vtable_t *cs_core_vtable_get_head(unsigned int vtbl_type);

/* Adding hashmask to SDB of the vtable. If succeeds, the allocated
 * hash mask index will return through the pointer given by hm_idx */
int cs_core_vtable_add_hashmask(unsigned int vtbl_type,
		fe_hash_mask_entry_t *hash_mask, unsigned int priority,
		bool is_qos, unsigned int *hm_idx);

/* Deleting hashmask from SDB of the vtable given with vtbl_type,
 * it will use the hash_mask entry info given to find the index */
int cs_core_vtable_del_hashmask(unsigned int vtbl_type,
		fe_hash_mask_entry_t *hash_mask, bool is_qos);

/* deleting hashmask from SDB of the vtable given with vtbl_type,
 * it will find the matching hm_idx from SDB and remove it */
int cs_core_vtable_del_hashmask_by_idx(unsigned int vtbl_type,
		unsigned int hm_idx, bool is_qos);

/* get the hashmask flag with given fwd application type flow */
int cs_core_vtable_get_hashmask_flag_from_apptype(unsigned int app_type,
		u64 *app_hm_flag);

/* get the hashmask index with given fwd application type flow */
int cs_core_vtable_get_hashmask_index_from_apptype(unsigned int app_type,
		u8 *app_hm_idx);

/* dump the current allocated vtable */
void cs_core_vtable_dump(void);

/* convert hashmask flag to data content */
void convert_hashmask_flag_to_data(u64 flag, fe_hash_mask_entry_t *hm_entry);

/* change rule priority for a vtbl_type */
int cs_core_vtable_set_entry_valid(unsigned int vtbl_type, u8 valid);

typedef struct cs_cls_entry_s {
	/*rule*/
	__u16 proto;
	__u16 src_port_lo;
	__u16 src_port_hi;
	__u16 dst_port_lo;
	__u16 dst_port_hi;
	__u32 ip_da[4];
	__u16 ip_ver;
	/*action*/
	__u16 action_flow_id;
	__u16 action_dst_port;
	__u16 action_dst_voq;
} cs_acl_entry_t;


int cs_acl_add(cs_acl_entry_t * flow);
int cs_acl_del(int idx);

/* change acl enable for a vtbl_type */
int cs_core_vtable_set_acl_enable(unsigned int vtbl_type, u8 enable);

#endif /* __CS_CORE_VTABLE_H__ */
