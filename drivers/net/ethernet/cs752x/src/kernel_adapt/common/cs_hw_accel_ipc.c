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
/*
 * cs_hw_accel_rtp_proxy.c
 *
 * $Id$
 *
 * This file contains the implementation for CS RTP proxy
 * Acceleration.
 */
#include "cs_hw_accel_ipc.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_hw_accel_pptp.h"
#include "cs_hw_accel_rtp_proxy.h"
#include "cs_hw_accel_sa_id.h"
#include <mach/cs_vpn_tunnel_ipc.h>
#include "cs_hw_accel_ip_translate.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#endif /* CONFIG_CS752X_PROC */

/* TODO:  add a common cs_adapt_debug flag*/
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#define ERR(x)	(x)

#ifdef CS_IPC_ENABLED
static struct ipc_context *cs_ipc_pe_ctxt0;
static struct ipc_context *cs_ipc_pe_ctxt1;

struct g2_ipc_msg cs_pe_ipc_procs[] = {
	{CS_IPC_PE_MTU_GET_ACK, (unsigned long) cs_tunnel_mtu_get_ack},
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	{CS_L2TP_IPC_PE_SET_ENTRY_ACK, (unsigned long) cs_l2tp_ipc_rcv_set_entry_ack},
	{CS_L2TP_IPC_PE_DEL_ENTRY_ACK, (unsigned long) cs_l2tp_ipc_rcv_del_entry_ack},
	{CS_IPSEC_IPC_PE_SET_SADB_ACK, (unsigned long) cs_ipsec_ipc_rcv_set_sadb_ack},
	{CS_IPSEC_IPC_PE_DEL_SADB_ACK, (unsigned long) cs_ipsec_ipc_rcv_del_sadb_ack},
	{CS_IPSEC_IPC_PE_SET_SADB_KEY_ACK, (unsigned long) cs_ipsec_ipc_rcv_set_sadb_key_ack},
#endif /* CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC */
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
	{CS_PPTP_IPC_PE_DEL_ENTRY_ACK, (unsigned long) cs_pptp_ipc_del_entry_ack},
	{CS_PPTP_IPC_PE_INFORM_LOST_PKT_ACK, (unsigned long) cs_pptp_ipc_inform_lost_pkt_ack},
#endif /* CONFIG_CS75XX_HW_ACCEL_PPTP */
#ifdef CONFIG_CS75XX_HW_ACCEL_RTP_PROXY
	{CS_RTP_IPC_PE_SET_ENTRY_ACK, (unsigned long) cs_rtp_ipc_rcv_set_entry_ack},
	{CS_RTP_IPC_PE_DEL_ENTRY_ACK, (unsigned long) cs_rtp_ipc_rcv_del_entry_ack},
#endif /* CONFIG_CS75XX_HW_ACCEL_RTP_PROXY */
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
	{CS_IP_TS_IPC_PE_SET_ENTRY_ACK, (unsigned long) cs_ip_translate_ipc_rcv_set_entry_ack},
	{CS_IP_TS_IPC_PE_DEL_ENTRY_ACK, (unsigned long) cs_ip_translate_ipc_rcv_del_entry_ack},
#endif
	{0, (unsigned long) 0}
};

/* IPC related */
cs_status_t cs_pe_ipc_send(
		cs_sa_id_direction_t direction,
		cs_uint16_t msg_type,
		const void *msg_data,
		cs_uint16_t msg_size)
{
	int ret;
	cs_uint8_t ipc_clnt_id;
	cs_uint8_t cpu_id;
	static struct ipc_context * cs_ipc_pe_ctxt;

	if (msg_data == NULL) {
		ERR(printk("%s:%d NULL pointer\n",
			__func__, __LINE__));
		return CS_E_NULL_PTR;
	}

	/* Note: Only deal with Dual PE case.
	    Needs to modify for single case in the furture*/

	/*
		PE0: decrypt/decap(downstream) ,
		PE1: encrypt/encap(upnstream)
	*/
	ipc_clnt_id = direction ? G2_RE_TUNNEL_CLIENT_RCPU0 : G2_RE_TUNNEL_CLIENT_RCPU1;
	cpu_id = direction ? CPU_RCPU0 : CPU_RCPU1;
	cs_ipc_pe_ctxt = direction ? cs_ipc_pe_ctxt0 : cs_ipc_pe_ctxt1;

	ret = g2_ipc_send(cs_ipc_pe_ctxt,
			 cpu_id,
			 ipc_clnt_id,
			 G2_IPC_HPRIO,
			 msg_type,
			 msg_data,
			 msg_size);

	if (G2_IPC_OK != ret)
		return CS_E_ERROR;

	return CS_OK;
}
EXPORT_SYMBOL(cs_pe_ipc_send);

/*CS_IPC_PE_DUMP_TBL*/
cs_status_t
cs_pe_ipc_send_dump(
		cs_sa_id_direction_t direction,
		cs_uint8_t fun_id,
		cs_uint8_t entry_id
		)
{
	g2_ipc_pe_dump_tbl_t msg;

	DBG(printk("%s:%d\n", __func__, __LINE__));
	memset(&msg, 0, sizeof(g2_ipc_pe_dump_tbl_t));
	msg.fun_id = fun_id;
	msg.entry_id = entry_id;

	return cs_pe_ipc_send(direction, CS_IPC_PE_DUMP_TBL, &msg,
				sizeof(g2_ipc_pe_dump_tbl_t));

}
EXPORT_SYMBOL(cs_pe_ipc_send_dump);

/*CS_IPC_PE_MIB_EN*/
cs_status_t
cs_pe_ipc_send_mib_en(
		cs_sa_id_direction_t direction,
		cs_uint8_t	enabled
		)
{
	g2_ipc_pe_mib_en_t msg;

	DBG(printk("%s:%d enbl = %d\n", __func__, __LINE__, enabled));
	memset(&msg, 0, sizeof(g2_ipc_pe_mib_en_t));
	msg.enabled = enabled;

	return cs_pe_ipc_send(direction, CS_IPC_PE_MIB_EN, &msg,
				sizeof(g2_ipc_pe_mib_en_t));
}
EXPORT_SYMBOL(cs_pe_ipc_send_mib_en);

/*CS_IPC_PE_ENTRY_EN*/
cs_status_t
cs_pe_ipc_send_pe_entry_en(
		cs_sa_id_direction_t direction
		)
{
	/*TODO: TBD*/
	DBG(printk("%s:%d direction=%d\n", __func__, __LINE__, direction));

	return cs_pe_ipc_send(direction, CS_IPC_PE_ENTRY_EN, NULL, 0);
}
EXPORT_SYMBOL(cs_pe_ipc_send_pe_entry_en);

cs_status_t cs_pe_ipc_register(void)
{
	short status;
	cs_uint16 msg_count;

	msg_count = sizeof(cs_pe_ipc_procs) / sizeof(cs_pe_ipc_procs[0]);
	status = g2_ipc_register(G2_RE_TUNNEL_CLIENT_RCPU0, cs_pe_ipc_procs,
				msg_count, 0, NULL, &cs_ipc_pe_ctxt0);
	if (status != G2_IPC_OK) {
		printk("%s::Failed to register IPC for CS HW acceleration\n",
			__func__);
		return CS_E_ERROR;
	} else
		printk("%s::successfully register IPC for CS HW acceleration msg_count=%d\n",
			__func__, msg_count);

	status = g2_ipc_register(G2_RE_TUNNEL_CLIENT_RCPU1, cs_pe_ipc_procs,
				 msg_count, 0, NULL, &cs_ipc_pe_ctxt1);
	if (status != G2_IPC_OK) {
		printk("%s::Failed to register IPC for CS HW acceleration\n",
			__func__);
		return CS_E_ERROR;
	} else
		printk("%s::successfully register IPC for CS HW acceleration msg_count=%d\n",
			__func__, msg_count);

	return CS_OK;
}

cs_status_t cs_pe_ipc_deregister(void)
{
	g2_ipc_deregister(cs_ipc_pe_ctxt0);
	g2_ipc_deregister(cs_ipc_pe_ctxt1);
	printk("%s::Done deregister IPC for CS HW acceleration\n", __func__);
	return CS_OK;
}
#endif /* CS_IPC_ENABLED */
