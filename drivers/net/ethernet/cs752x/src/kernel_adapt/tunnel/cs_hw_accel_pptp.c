/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Raymond Tseng <raymond.tseng@cortina-systems.com>
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

/* Flow:
 *   LAN  <---  PE0  <---  WAN
 *   LAN  --->  PE1  --->  WAN
 *
 * PE0: Decryption, down-stream
 * PE1: Encryption, up-stream
 */


#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/if_vlan.h>
#include <linux/if_ether.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/net_namespace.h>
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include <mach/cs75xx_fe_core_table.h>
#include "cs_fe.h"
#include <mach/cs_rule_hash_api.h>
#include <mach/cs_route_api.h>
#include <mach/cs_vpn_tunnel_ipc.h>
#include "cs_hw_accel_tunnel.h"
#include "cs_hw_accel_pptp.h"
#include "cs_hw_accel_sa_id.h"

#ifdef CS_IPC_ENABLED
#include <mach/g2cpu_ipc.h>
#endif

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#endif	/* CONFIG_CS752X_PROC */

static cs_tunnel_id_t pptp_tunnel_id = TID_PPTP_BASE;

struct pptp_db up_db[MAX_PPTP_SESSION];
struct pptp_db down_db[MAX_PPTP_SESSION];

cs_uint16_t up_rekey_sa_id = ~0;	/* SA ID for upstream re-key */
cs_uint16_t down_rekey_sa_id = ~0;	/* SA ID for downstream re-key */
int rekey_sa_id_set = 0;		/* flag: setting rekey SA ID to PE or not */

#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)

static void cs_pptp_dump_db(void);

/*************************************************************************/
#ifdef CS_IPC_ENABLED
/* CPU requests PE to reset PPTP entry table */
cs_status_t cs_pptp_ipc_send_reset(cs_sa_id_direction_t dir)
{
	return CS_E_OK;
}

/* CPU requests PE to stop all acceleration */
cs_status_t cs_pptp_ipc_send_stop(cs_sa_id_direction_t dir)
{
	return CS_E_OK;
}

cs_status_t cs_pptp_ipc_inform_lost_pkt(cs_sa_id_direction_t dir)
{
	g2_ipc_pe_pptp_inform_lost_pkt_t msg;

/* TODO
	msg.sa_idx;
	msg.lost_seq;
	msg.current_seq;
*/

	return cs_tunnel_ipc_send(dir, CS_PPTP_IPC_PE_INFORM_LOST_PKT, &msg, sizeof(msg));
}

/* Set generic SA IDX which is for re-key in each PE */
cs_status_t cs_pptp_ipc_set_generic_sa_idx(cs_sa_id_direction_t dir)
{
	g2_ipc_pe_pptp_set_generic_sa_idx_t msg;

	if ((up_rekey_sa_id == ~0) || (down_rekey_sa_id == ~0)) {
		/* maybe just init the SA ID here? */
		return CS_E_INIT;
	}

	/* PE0: decryption, PE1: encryption */
	msg.op_type = (dir == UP_STREAM) ? 1 : 0;
	msg.sa_idx = (dir == UP_STREAM) ? up_rekey_sa_id : down_rekey_sa_id;

	return cs_tunnel_ipc_send(dir, CS_PPTP_IPC_PE_SET_GENERIC_SA_IDX, &msg, sizeof(msg));
}

/* sa_idx must be upstream SA ID */
cs_status_t cs_pptp_ipc_key_change(cs_uint16_t sa_idx)
{
	g2_ipc_pe_pptp_key_change_t msg;

	msg.enc_sa_idx = sa_idx;

	return cs_tunnel_ipc_send(UP_STREAM, CS_PPTP_IPC_PE_KEY_CHANGE_EVENT, &msg, sizeof(msg));
}

/* CPU sets a PPTP entry in PE */
cs_status_t cs_pptp_ipc_send_set_entry(cs_sa_id_direction_t dir, cs_tunnel_cfg_t *p_tunnel_cfg)
{
	g2_ipc_pe_pptp_set_entry_t msg;
	int i, found;
	int up_idx = -1, down_idx = -1;

	/* PE1 to WAN, ingress */
	found = 0;
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (up_db[i].valid == 0) continue;

		if (up_db[i].session.sa_id == p_tunnel_cfg->tunnel.gre.overlay_tunnel_ingress.pptp_sa_id) {
			up_idx = i;
			msg.enc_sa_idx = up_db[i].session.sa_id;
			msg.dest_call_id = up_db[i].session.call_id;
			memcpy(&msg.src_addr, &up_db[i].session.local_addr, sizeof(msg.src_addr));
			msg.state = up_db[i].session.state;
			msg.crypto_type = up_db[i].session.crypto_type;
			msg.host_seq_num = up_db[i].session.seq_num;
			break;
		}
	}
	if (up_idx < 0) {
		printk(KERN_ERR "%s: Can not found entry for upstream SA ID %u.\n",
			__func__, p_tunnel_cfg->tunnel.gre.overlay_tunnel_ingress.pptp_sa_id);
		return CS_E_NOT_FOUND;
	}

	/* WAN to PE0, egress */
	found = 0;
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (down_db[i].valid == 0) continue;

		if (down_db[i].session.sa_id == p_tunnel_cfg->tunnel.gre.overlay_tunnel_egress.pptp_sa_id) {
			down_idx = i;
			msg.dec_sa_idx = down_db[i].session.sa_id;
			msg.src_call_id = down_db[i].session.call_id;
			memcpy(&msg.dest_addr, &up_db[i].session.remote_addr, sizeof(msg.dest_addr));
			msg.state = down_db[i].session.state;
			msg.crypto_type = down_db[i].session.crypto_type;
			msg.host_ack_seq_num = down_db[i].session.seq_num;
			break;
		}
	}
	if (down_idx < 0) {
		printk(KERN_ERR "%s: Can not found entry for downstream SA ID %u.\n",
			__func__, p_tunnel_cfg->tunnel.gre.overlay_tunnel_egress.pptp_sa_id);
		return CS_E_NOT_FOUND;
	}

	if (dir == UP_STREAM) {
		msg.op_type = 1;
		memcpy(&msg.key, up_db[up_idx].session.key, 16);
		msg.key_len = up_db[up_idx].session.keylen;
	} else {
		msg.op_type = 0;
		memcpy(&msg.key, down_db[down_idx].session.key, 16);
		msg.key_len = down_db[down_idx].session.keylen;
	}

	return cs_tunnel_ipc_send(dir, CS_PPTP_IPC_PE_SET_ENTRY, &msg, sizeof(msg));
}

/* CPU deletes a PPTP entry in PE */
cs_status_t cs_pptp_ipc_send_del_entry(cs_sa_id_direction_t dir, cs_uint16_t sa_idx)
{
	g2_ipc_pe_pptp_delete_entry_t msg;

	msg.op_type = (dir == UP_STREAM) ? 1 : 0;
	msg.sa_idx = sa_idx;

	return cs_tunnel_ipc_send(dir, CS_PPTP_IPC_PE_DEL_ENTRY, &msg, sizeof(msg));
}

/* CPU requests PE to dump PPTP table if DEBUG_PRINT is defined in Makefile. */
cs_status_t cs_pptp_ipc_send_dump(cs_sa_id_direction_t dir, cs_uint8_t sa_id)
{
	g2_ipc_pe_dump_tbl_t msg;

	msg.fun_id  = CS_PPTP;
	msg.entry_id = sa_id;

	return cs_tunnel_ipc_send(dir, CS_IPC_PE_DUMP_TBL, &msg, sizeof(msg));
}

/* CPU sends echo IPC to PE for testing if the connection is normally */
cs_status_t cs_pptp_ipc_send_echo(cs_sa_id_direction_t dir)
{
	return CS_E_OK;
}

/* CPU request PE to enable MIB counters */
cs_status_t cs_pptp_ipc_send_mib_en(cs_sa_id_direction_t dir, cs_uint8_t enable)
{
	g2_ipc_pe_mib_en_t msg;

	memset(&msg, 0, sizeof(g2_ipc_pe_mib_en_t));
	msg.enabled = enable;

	return cs_tunnel_ipc_send(dir, CS_IPC_PE_MIB_EN, &msg, sizeof(g2_ipc_pe_mib_en_t));
}

cs_status_t cs_pptp_ipc_del_entry_ack(struct ipc_addr peer,
				unsigned short msg_no, const void *msg_data,
				unsigned short msg_size,
				struct ipc_context *context)
{
	g2_ipc_pe_pptp_delete_entry_ack_t *msg;

	if (msg_data == NULL) {
		printk(KERN_ERR "%s: ERROR! Null pointer.\n", __func__);
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_pptp_delete_entry_ack_t *) msg_data;

	printk(KERN_INFO "%s: op_type=%u\n", __func__, msg->op_type);
	printk(KERN_INFO "%s: sa_idx=%u\n", __func__, msg->sa_idx);
	printk(KERN_INFO "%s: current_seq=%u\n", __func__, msg->current_seq);

	return CS_E_OK;
}

cs_status_t cs_pptp_ipc_inform_lost_pkt_ack(struct ipc_addr peer,
				unsigned short msg_no, const void *msg_data,
				unsigned short msg_size,
				struct ipc_context *context)
{
	g2_ipc_pe_pptp_inform_lost_pkt_t *msg;

	if (msg_data == NULL) {
		printk(KERN_ERR "%s: ERROR! Null pointer.\n", __func__);
		return CS_E_NULL_PTR;
	}

	msg = (g2_ipc_pe_pptp_inform_lost_pkt_t *) msg_data;

	printk(KERN_INFO "%s: sa_idx=%u\n", __func__, msg->sa_idx);
	printk(KERN_INFO "%s: prev_ccount=%u\n", __func__, msg->prev_ccount);
	printk(KERN_INFO "%s: current_ccount=%u\n", __func__, msg->current_ccount);

	return CS_E_OK;
}
#endif /* CS_IPC_ENABLED */
/*************************************************************************/

/* callback function for LAN->PE1 LPM which requires the modified SRC MAC */
cs_status_t cs_pptp_set_src_mac(cs_l3_nexthop_t *nexthop, char *src_mac)
{
	int crc32;
	int i;

	if (nexthop == NULL || src_mac == NULL) {
		printk(KERN_ERR "%s: ERROR NULL pointer.\n", __func__);
		return CS_E_NULL_PTR;
	}

	printk(KERN_DEBUG "%s: tunnel_id=%u\n", __func__, nexthop->id.tunnel_id);

	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (up_db[i].valid == 0) continue;

		if (up_db[i].tunnel_id == nexthop->id.tunnel_id) {
			src_mac[0] = 0; /* reserved */
			src_mac[1] = 0x00; /* 0: enc, 1: dec */
			src_mac[2] = (cs_uint8_t) 0xff & up_db[i].session.sa_id; /* SA ID */
			src_mac[3] = 0x00; /* 0: LAN->PE or WAN->PE, 1: CPU->PE */
			src_mac[4] = CS_PPTP; /* cs_tunnel_type_t */
			crc32 = ~(calc_crc(~0, (u8 const *) &src_mac[0], 5));
			src_mac[5] = crc32 & 0xff;

			return CS_OK;
		}
	}

	return CS_E_NOT_FOUND;
}

cs_status_t cs_pptp_tunnel_id_to_sa_id(cs_tunnel_id_t tunnel_id, cs_tunnel_dir_t dir, cs_uint16_t *sa_id)
{
	struct pptp_db *db;
	int i;

	/* Verify tunnel ID range */
	if ((tunnel_id < TID_PPTP_BASE) || (tunnel_id >= TID_PPTP_MAX))
		return CS_E_PARAM;

	if (dir == CS_TUNNEL_DIR_OUTBOUND)
		db = up_db;
	else if (dir == CS_TUNNEL_DIR_INBOUND)
		db = down_db;
	else
		return CS_E_PARAM;

	/* Search DB for SA ID */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (db[i].valid == 0) continue;

		if (db[i].tunnel_id == tunnel_id) {
			*sa_id = db[i].session.sa_id;
			return CS_E_OK;
		}
	}

	return CS_E_NOT_FOUND;
}

/* Call this while necessary PPTP data tunnel info is collected */
cs_status_t cs_pptp_tunnel_add(cs_tunnel_cfg_t *p_tunnel_cfg)
{
	int i;
	int up_idx = -1, down_idx = -1;
	cs_l3_nexthop_t nexthop;
	int ret;
	cs_uint8_t ip_version;
	cs_uint16_t pppoe_session_id;
	cs_uint16_t hash_index;
	cs_uint16_t vlan_id = 0;
	cs_uint8 *p_sa_mac, *p_da_mac;
	cs_l3_ip_addr *p_da_ip, *p_sa_ip;

	cs_rule_hash_t wan2pe_rule_hash;
	cs_rule_hash_t pe2wan_rule_hash;

	int found = 0;

	if (p_tunnel_cfg->type != CS_PPTP)
		return CS_E_NOT_SUPPORT;

	/* get nexthop */

	if (cs_l3_nexthop_get(0, p_tunnel_cfg->nexthop_id, &nexthop) != CS_E_OK) {
		printk(KERN_ERR "%s: Fail to get nexthop, index=%u\n", __func__, p_tunnel_cfg->nexthop_id);
		return CS_E_ERROR;
	}

	if (nexthop.nhid.nhop_type != CS_L3_NEXTHOP_DIRECT) {
		printk(KERN_ERR "%s: Unexpected nexthop type %d.\n", __func__, nexthop.nhid.nhop_type);
		return CS_E_CONFLICT;
	}

	/***** WAN to PE0 *****/

	memset(&wan2pe_rule_hash, 0, sizeof(wan2pe_rule_hash));

	/* get IP SA and DA from DB */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (down_db[i].valid == 0) continue;

		if (down_db[i].session.sa_id == p_tunnel_cfg->tunnel.gre.overlay_tunnel_egress.pptp_sa_id) {
			down_idx = i;
			down_db[i].tunnel_id = pptp_tunnel_id; // store tunnel ID
			memcpy(&wan2pe_rule_hash.key.sa[0], &down_db[i].session.remote_addr.ip_addr, sizeof(down_db[i].session.remote_addr.ip_addr));
			memcpy(&wan2pe_rule_hash.key.da[0], &down_db[i].session.local_addr.ip_addr, sizeof(down_db[i].session.local_addr.ip_addr));
			break;
		}
	}
	if (down_idx < 0) {
		printk(KERN_ERR "%s: ERROR! Can not find downstream SA ID %u.\n", __func__, p_tunnel_cfg->tunnel.gre.overlay_tunnel_egress.pptp_sa_id);
		return CS_E_ERROR;
	}

	ip_version = (down_db[i].session.local_addr.afi == CS_IPV4) ? 0 : 1;
	pppoe_session_id = 0;
	if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
		pppoe_session_id = nexthop.encap.port_encap.port_encap.pppoe.pppoe_session_id;
	}
	p_sa_ip = &(down_db[i].session.remote_addr.ip_addr);
	p_da_ip = &(down_db[i].session.local_addr.ip_addr);
	ret = cs_tunnel_wan2pe_rule_hash_add(p_tunnel_cfg, 0, p_da_ip, p_sa_ip, ip_version, pppoe_session_id, 0, 0, 0, &wan2pe_rule_hash);
	if (ret != CS_E_OK) {
		printk(KERN_ERR "%s: cs_tunnel_wan2pe_rule_hash_add(type=%d) failed !!, ret=%d\n", __func__, p_tunnel_cfg->type, ret);
	}

	down_db[down_idx].hash_idx = wan2pe_rule_hash.hash_index;

	/***** PE1 to WAN, PE0 to WAN *****/

	memset(&pe2wan_rule_hash, 0, sizeof(pe2wan_rule_hash));

	/* get IP SA and DA from DB */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (up_db[i].valid == 0) continue;

		if (up_db[i].session.sa_id == p_tunnel_cfg->tunnel.gre.overlay_tunnel_ingress.pptp_sa_id) {
			up_idx = i;
			up_db[i].tunnel_id = pptp_tunnel_id; // store tunnel ID
			memcpy(&pe2wan_rule_hash.key.sa[0], &up_db[i].session.local_addr.ip_addr, sizeof(up_db[i].session.local_addr.ip_addr));
			memcpy(&pe2wan_rule_hash.key.da[0], &up_db[i].session.remote_addr.ip_addr, sizeof(up_db[i].session.remote_addr.ip_addr));
			break;
		}
	}
	if (up_idx < 0) {
		printk(KERN_ERR "%s: ERROR! Can not find downstream SA ID %u.\n", __func__, p_tunnel_cfg->tunnel.gre.overlay_tunnel_ingress.pptp_sa_id);
		return CS_E_ERROR;
	}

	ip_version = (up_db[i].session.local_addr.afi == CS_IPV4) ? 0 : 1;

	pppoe_session_id = 0;
	if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
		vlan_id = VLAN_VID_MASK & nexthop.encap.port_encap.port_encap.pppoe.tag[0];
		pppoe_session_id = nexthop.encap.port_encap.port_encap.pppoe.pppoe_session_id;
	} else if (nexthop.encap.port_encap.type == CS_PORT_ENCAP_ETH_1Q_E || nexthop.encap.port_encap.type == CS_PORT_ENCAP_ETH_QinQ_E) {
		vlan_id = VLAN_VID_MASK & nexthop.encap.port_encap.port_encap.eth.tag[0];
	}

	p_sa_mac = &(nexthop.encap.port_encap.port_encap.eth.src_mac[0]);
	p_da_mac = &(nexthop.da_mac[0]);
	p_sa_ip = &(down_db[i].session.local_addr.ip_addr);
	p_da_ip = &(down_db[i].session.remote_addr.ip_addr);

	ret = cs_tunnel_pe2wan_rule_hash_add(p_tunnel_cfg, 0, p_da_ip, p_sa_ip, ip_version, pppoe_session_id, vlan_id, p_da_mac, p_sa_mac, 0, 0, &pe2wan_rule_hash, &hash_index);

	if (ret != CS_E_OK) {
		printk(KERN_ERR "%s: cs_tunnel_wan2pe_rule_hash_add(type=%d) failed !!, ret=%d\n", __func__, p_tunnel_cfg->type, ret);
	}

	up_db[up_idx].hash_idx = pe2wan_rule_hash.hash_index;
	up_db[up_idx].hash_idx2 = hash_index;

	/* send necessary info to PE */
#ifdef CS_IPC_ENABLED
	if (cs_pptp_ipc_send_set_entry(UP_STREAM, p_tunnel_cfg) != CS_E_OK) {
		printk(KERN_ERR "%s: ERROR! Fail to set entry for upstream.\n", __func__);
		return CS_E_ERROR;
	}

	if (cs_pptp_ipc_send_set_entry(DOWN_STREAM, p_tunnel_cfg) != CS_E_OK) {
		printk(KERN_ERR "%s: ERROR! Fail to set entry for downstream.\n", __func__);
		return CS_E_ERROR;
	}
#endif /* CS_IPC_ENABLED */
	/* for returning tunnel ID */
	p_tunnel_cfg->tunnel.gre.tunnel_id = pptp_tunnel_id;

	/* Search next unused tunnel ID.
	 * PPTP tunnel ID range is larger than MAX_PPTP_SESSION,
	 * so it is impossible to use all PPTP tunnel ID nor
	 * the following do-while becomes infinite loop.
	 */
	do {
		pptp_tunnel_id++;
		/* circular use tunnel ID */
		if (pptp_tunnel_id >= TID_PPTP_MAX)
			pptp_tunnel_id = TID_PPTP_BASE;

		found = 1;

		/* check if new pptp_tunnel_id is used already */
		for (i = 0; i < MAX_PPTP_SESSION; i++) {
			if (up_db[i].valid == 0) continue;

			if (up_db[i].tunnel_id == pptp_tunnel_id) {
				found = 0;
				break;
			}
		}
	} while (found == 0);

	cs_pptp_dump_db();

	return CS_E_OK;
}

/* Call this while PPTP data tunnel is asked to delete */
cs_status_t cs_pptp_tunnel_delete(cs_tunnel_cfg_t *p_tunnel_cfg)
{
	int i;
	int up_idx = -1, down_idx = -1;

	if (p_tunnel_cfg->type != CS_PPTP)
		return CS_E_NOT_SUPPORT;

	/***** WAN to PE0 *****/

	/* Delete rule hash that allows encrypted PPTP packets from WAN to PE0 */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (down_db[i].valid == 0) continue;

		if (down_db[i].session.sa_id == p_tunnel_cfg->tunnel.gre.overlay_tunnel_egress.pptp_sa_id) {
			down_idx = i;

			if (cs_rule_hash_delete_by_hash_index(0, down_db[i].hash_idx) != CS_E_OK) {
				printk(KERN_ERR "%s: ERROR! Fail to delete hash for WAN to PE\n", __func__);
				return CS_E_ERROR;
			}
			break;
		}
	}

	/***** PE1 to WAN *****/

	/* Delete rule hash that allows encrypted PPTP packets from PE1 to WAN */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (up_db[i].valid == 0) continue;

		if (up_db[i].session.sa_id == p_tunnel_cfg->tunnel.gre.overlay_tunnel_ingress.pptp_sa_id) {
			up_idx = i;

			if (cs_rule_hash_delete_by_hash_index(0, up_db[i].hash_idx) != CS_E_OK) {
				printk(KERN_ERR "%s: ERROR! Fail to delete hash for PE1 to WAN\n", __func__);
				return CS_E_ERROR;
			}
			if (cs_rule_hash_delete_by_hash_index(0, up_db[i].hash_idx2) != CS_E_OK) {
				printk(KERN_ERR "%s: ERROR! Fail to delete hash for PE0 to WAN\n", __func__);
				return CS_E_ERROR;
			}
			break;
		}
	}
#ifdef CS_IPC_ENABLED
	/***** Delete session info from PE *****/
	if (up_idx >= 0) {
		if (cs_pptp_ipc_send_del_entry(UP_STREAM, up_db[up_idx].session.sa_id) != CS_E_OK) {
			printk(KERN_ERR "%s: ERROR! Fail to delete entry for upstream, SA ID=%u.\n", __func__, up_db[up_idx].session.sa_id);
			return CS_E_ERROR;
		}
	}

	if (down_idx >= 0) {
		if (cs_pptp_ipc_send_del_entry(DOWN_STREAM, down_db[down_idx].session.sa_id) != CS_E_OK) {
			printk(KERN_ERR "%s: ERROR! Fail to delete entry for downstream, SA ID=%u.\n", __func__, down_db[down_idx].session.sa_id);
			return CS_E_ERROR;
		}
	}
#endif /* CS_IPC_ENABLED */
	return CS_E_OK;
}

/* Call this while PPTP data tunnel is asked to delete */
cs_status_t cs_pptp_tunnel_delete_by_idx(cs_tunnel_id_t tunnel_id)
{
	int i;
	int up_idx = -1, down_idx = -1;

	/***** WAN to PE0 *****/

	/* Delete rule hash that allows encrypted PPTP packets from WAN to PE0 */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (down_db[i].valid == 0) continue;

		if (down_db[i].tunnel_id == tunnel_id) {
			down_idx = i;

			if (cs_rule_hash_delete_by_hash_index(0, down_db[i].hash_idx) != CS_E_OK) {
				printk(KERN_ERR "%s: ERROR! Fail to delete hash for WAN to PE\n", __func__);
				return CS_E_ERROR;
			}
			break;
		}
	}

	/***** PE1 to WAN *****/

	/* Delete rule hash that allows encrypted PPTP packets from PE1 to WAN */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (up_db[i].valid == 0) continue;

		if (up_db[i].tunnel_id == tunnel_id) {
			up_idx = i;

			if (cs_rule_hash_delete_by_hash_index(0, up_db[i].hash_idx) != CS_E_OK) {
				printk(KERN_ERR "%s: ERROR! Fail to delete hash for PE1 to WAN\n", __func__);
				return CS_E_ERROR;
			}
			if (cs_rule_hash_delete_by_hash_index(0, up_db[i].hash_idx2) != CS_E_OK) {
				printk(KERN_ERR "%s: ERROR! Fail to delete hash for PE0 to WAN\n", __func__);
				return CS_E_ERROR;
			}
			break;
		}
	}
#ifdef CS_IPC_ENABLED
	/***** Delete session info from PE *****/
	if (up_idx >= 0) {
		if (cs_pptp_ipc_send_del_entry(UP_STREAM, up_db[up_idx].session.sa_id) != CS_E_OK) {
			printk(KERN_ERR "%s: ERROR! Fail to delete entry for upstream, SA ID=%u.\n", __func__, up_db[up_idx].session.sa_id);
			return CS_E_ERROR;
		}
	}

	if (down_idx >= 0) {
		if (cs_pptp_ipc_send_del_entry(DOWN_STREAM, down_db[down_idx].session.sa_id) != CS_E_OK) {
			printk(KERN_ERR "%s: ERROR! Fail to delete entry for downstream, SA ID=%u.\n", __func__, down_db[down_idx].session.sa_id);
			return CS_E_ERROR;
		}
	}
#endif /* CS_IPC_ENABLED */
	return CS_E_OK;
}

/*************************************************************************/

static cs_status_t cs_pptp_rekey_said_alloc(void)
{
	cs_status_t ret;

	if (up_rekey_sa_id == (cs_uint16_t) ~0) {
		if ((ret = cs_sa_id_alloc(UP_STREAM, 1, 1, CS_PPTP, &up_rekey_sa_id)) != CS_E_OK) {
			printk(KERN_ERR "%s: Fail to alloc a SA ID for upstream re-key. (%d)\n", __func__, ret);
			return ret;
		}
		printk(KERN_DEBUG "%s: Upstream rekey SA ID = %u.\n", __func__, up_rekey_sa_id);
	}

	if (down_rekey_sa_id == (cs_uint16_t) ~0) {
		if ((ret = cs_sa_id_alloc(DOWN_STREAM, 1, 1, CS_PPTP, &down_rekey_sa_id)) != CS_E_OK) {
			printk(KERN_ERR "%s: Fail to alloc a SA ID for downstream re-key. (%d)\n", __func__, ret);
			return ret;
		}
		printk(KERN_DEBUG "%s: Downstream rekey SA ID = %u.\n", __func__, down_rekey_sa_id);
	}

	/* set SA ID to PE */
	if (rekey_sa_id_set == 0) {
#ifdef CS_IPC_ENABLED
		if ((ret = cs_pptp_ipc_set_generic_sa_idx(DOWN_STREAM)) != CS_E_OK) {
			printk(KERN_ERR "%s: Fail to set downstream rekey SA ID %u to PE0. (%d)", __func__, down_rekey_sa_id, ret);
			return CS_E_ERROR;
		}

		if ((ret = cs_pptp_ipc_set_generic_sa_idx(UP_STREAM)) != CS_E_OK) {
			printk(KERN_ERR "%s: Fail to set rekey upstream SA ID %u to PE1. (%d)", __func__, up_rekey_sa_id, ret);
			return CS_E_ERROR;
		}
#endif /* CS_IPC_ENABLED */
		rekey_sa_id_set = 1;
	}

	return CS_E_OK;
}

static cs_status_t cs_pptp_rekey_said_free(void)
{
	cs_status_t ret;

	if (up_rekey_sa_id != ~0) {
		if ((ret = cs_sa_id_free(UP_STREAM, up_rekey_sa_id)) != CS_E_OK) {
			printk(KERN_ERR "%s: Fail to free the SA ID for upstream re-key.\n", __func__);
			return ret;
		}
	}
	up_rekey_sa_id = ~0;

	if (down_rekey_sa_id != ~0) {
		if ((ret = cs_sa_id_free(DOWN_STREAM, down_rekey_sa_id)) != CS_E_OK) {
			printk(KERN_ERR "%s: Fail to free the SA ID for downstream re-key.\n", __func__);
			return ret;
		}
	}
	down_rekey_sa_id = ~0;

	return CS_E_OK;
}

static void _cs_pptp_dump_db(struct pptp_db *db)
{
	int i, j;

	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (db[i].valid == 0) continue;

		printk(KERN_DEBUG "Entry %d:\n", i);
		printk(KERN_DEBUG "  valid=%d\n", db[i].valid);
		printk(KERN_DEBUG "  tunnel_id=%d\n", db[i].tunnel_id);
		printk(KERN_DEBUG "  sa_id=%u\n", db[i].session.sa_id);
		printk(KERN_DEBUG "  call_id=0x%04x\n", db[i].session.call_id);

		if (db[i].session.local_addr.afi == CS_IPV4)
			printk(KERN_DEBUG "  local_addr=%pI4\n", &db[i].session.local_addr.ip_addr.ipv4_addr);
		else
			printk(KERN_DEBUG "  local_addr=%pI6\n", &db[i].session.local_addr.ip_addr.ipv6_addr);

		if (db[i].session.remote_addr.afi == CS_IPV4)
			printk(KERN_DEBUG "  remote_addr=%pI4\n", &db[i].session.remote_addr.ip_addr.ipv4_addr);
		else
			printk(KERN_DEBUG "  remote_addr=%pI6\n", &db[i].session.remote_addr.ip_addr.ipv6_addr);

		if (db[i].session.state == CS_PPTP_STATELESS)
			printk(KERN_DEBUG "  state=STATELESS\n");
		else
			printk(KERN_DEBUG "  state=STATEFUL\n");

		switch (db[i].session.crypto_type) {
		case CS_CRYPTO_INVALID:
			printk(KERN_DEBUG "  crypto_type=INVALID\n");
			break;
		case CS_CRYPTO_NONE:
			printk(KERN_DEBUG "  crypto_type=NONE\n");
			break;
		case CS_CRYPTO_MPPE40:
			printk(KERN_DEBUG "  crypto_type=MPPE40\n");
			break;
		case CS_CRYPTO_MPPE128:
			printk(KERN_DEBUG "  crypto_type=MPPE128\n");
			break;
		}

		printk(KERN_DEBUG "  keylen=%d\n", db[i].session.keylen);
		printk(KERN_DEBUG "  key=");
		for (j = 0; j < db[i].session.keylen; j++)
			printk("%02x ", db[i].session.key[j]);
		printk("\n");

		printk(KERN_DEBUG "  seq_num=%d\n", db[i].session.seq_num);
	}
}

static void cs_pptp_dump_db(void)
{
	printk(KERN_DEBUG "*** PPTP Upstream DB ***\n");
	_cs_pptp_dump_db(&up_db);

	printk(KERN_DEBUG "*** PPTP Downstream DB ***\n");
	_cs_pptp_dump_db(&down_db);
}

/*************************************************************************/

/* Add a PPTP forwarding rule to HW according to the session content */
cs_status_t cs_pptp_session_add(cs_dev_id_t dev_id, cs_pptp_sa_t *session)
{
	cs_status_t ret;
	cs_uint8_t dir;
	cs_uint8_t is_crypto;
	int i, valid_db = -1;
	struct pptp_db *db;

	printk(KERN_DEBUG "%s: call_id=%u, crypto_type=%d, state=%d\n",
		__func__, session->call_id, session->crypto_type, session->state);

	if (session->local_addr.afi == CS_IPV4)
		printk(KERN_DEBUG "%s: src_addr=%pI4, dest_addr=%pI4\n",
			__func__, &session->local_addr.ip_addr.ipv4_addr, &session->remote_addr.ip_addr.ipv4_addr);
	else
		printk(KERN_DEBUG "%s: src_addr=%pI6, dest_addr=%pI6\n",
			__func__, &session->local_addr.ip_addr.ipv6_addr, &session->remote_addr.ip_addr.ipv6_addr);

	/* make sure SA ID for rekey is allocated */
	cs_pptp_rekey_said_alloc();

	/* check IP type; currently only support IPv4 encap */
	if ((session->local_addr.afi != CS_IPV4) || (session->remote_addr.afi != CS_IPV4)) {
		printk(KERN_ERR "%s: ERROR! Only support IPv4 encap.\n", __func__);
		return CS_E_ERROR;
	}

	/* check MPPE state */
	if ((session->state != CS_PPTP_STATELESS) && (session->state != CS_PPTP_STATEFUL)) {
		printk(KERN_ERR "%s: ERROR! Wrong MPPE state %d.\n", __func__, session->state);
		return CS_E_ERROR;
	}

	/* ignore user's keylen for fool-proofing */
	switch (session->crypto_type) {
	case CS_CRYPTO_NONE:
		session->keylen = 0;
		break;

	case CS_CRYPTO_MPPE40:
		session->keylen = 8;
		break;

	case CS_CRYPTO_MPPE128:
		session->keylen = 16;
		break;

	case CS_CRYPTO_INVALID:
	default:
		printk(KERN_ERR "%s: ERROR! Unexpected crypto type %d.\n", __func__, session->crypto_type);
		return CS_E_ERROR;
	}

	is_crypto = (session->crypto_type == CS_CRYPTO_NONE) ? 0 : 1;
	db = (session->dir == CS_PPTP_DIR_UPSTREAM) ? up_db : down_db;
	dir = (session->dir == CS_PPTP_DIR_UPSTREAM) ? UP_STREAM : DOWN_STREAM;

	/* allocate a new sa_id from cs_virtual_spacc_alloc */
	ret = cs_sa_id_alloc(dir, is_crypto, 0, CS_PPTP, &session->sa_id);
	if (ret != CS_E_OK) {
		printk(KERN_ERR "%s: Fail to allocate SA ID.\n", __func__);
		return CS_E_RESOURCE;
	}

	/* search DB for free */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (db[i].valid == 0) {
			valid_db = i;
			break;
		}
	}
	if (valid_db < 0) {
		printk(KERN_INFO "%s: Max PPTP session nubmer %d is reached.\n", __func__, MAX_PPTP_SESSION);
		cs_sa_id_free(dir, session->sa_id);
		return CS_E_RESOURCE;
	}

	/* store the session info */
	memcpy(&db[valid_db].session, session, sizeof(db[valid_db].session));

	/* pass the varification, set the valid flag */
	db[valid_db].valid = 1;

	cs_pptp_dump_db();

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pptp_session_add);

/* Remove a PPTP forwarding rule from HW according to the session content */
cs_status_t cs_pptp_session_delete(cs_dev_id_t dev_id, cs_uint16_t sa_id, cs_uint8_t direction)
{
	cs_status_t ret = CS_E_ERROR;
	struct pptp_db *db;
	int i;
	cs_uint8_t dir;

	printk(KERN_DEBUG "%s: sa_id=%u\n", __func__, sa_id);

	db = (direction == CS_PPTP_DIR_UPSTREAM) ? up_db : down_db;
	dir = (direction == CS_PPTP_DIR_UPSTREAM) ? UP_STREAM : DOWN_STREAM;

	/* free sa_id by cs_virtual_spacc_free */
	if (cs_sa_id_free(dir, sa_id) != CS_E_OK) {
		printk(KERN_ERR "%s: Fail to free SA_ID %u.\n", __func__, sa_id);
		return CS_E_ERROR;
	}

	/* search db for sa_id and free it */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (db[i].valid == 0) continue;

		if (db[i].session.sa_id == sa_id) {
			db[i].valid = 0;
			ret = CS_E_OK;
		}
	}

	cs_pptp_dump_db();

	return ret;
}
EXPORT_SYMBOL(cs_pptp_session_delete);

/* Get the PPTP session content */
cs_status_t cs_pptp_session_get(cs_dev_id_t dev_id, cs_pptp_sa_t *session)
{
	struct pptp_db *db;
	cs_status_t ret = CS_E_NOT_FOUND;
	int i;

	db = up_db;
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (db[i].valid == 0) continue;

		if (db[i].session.sa_id == session->sa_id) {
			memcpy(session, &db[i].session, sizeof(db[i].session));
			ret = CS_E_OK;
			goto _got;
		}
	}

	db = down_db;
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (db[i].valid == 0) continue;

		if (db[i].session.sa_id == session->sa_id) {
			memcpy(session, &db[i].session, sizeof(db[i].session));
			ret = CS_E_OK;
			goto _got;
		}
	}

_got:
	return ret;
}
EXPORT_SYMBOL(cs_pptp_session_get);

/* Delete all PPTP HW forwarding rules */
cs_status_t cs_pptp_session_clear_all(cs_dev_id_t dev_id)
{
	int i;

	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		// TODO: need to delete hash?
		up_db[i].valid = 0;
		down_db[i].valid = 0;
	}

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_pptp_session_clear_all);

/* Notify PE to change enc_key for the entry 'sa_id'; sa_id must be upstream SA ID */
cs_status cs_pptp_key_change(cs_dev_id_t dev_id, cs_uint16_t sa_id)
{
	cs_status_t ret;
	int i;

	/* check if sa_id is valid in up_db */
	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		if (up_db[i].valid == 0) continue;

		if (up_db[i].session.sa_id == sa_id) {
#ifdef CS_IPC_ENABLED
			if ((ret = cs_pptp_ipc_key_change(sa_id)) != CS_E_OK) {
				printk(KERN_ERR "%s: Fail to send key change event for SA ID %u. (%d)", __func__, sa_id, ret);
				return CS_E_ERROR;
			}
#endif /* CS_IPC_ENABLED */
			return CS_E_OK;
		}
	}

	return CS_E_NOT_FOUND;
}
EXPORT_SYMBOL(cs_pptp_key_change);

cs_status_t cs_hw_accel_pptp_init(void)
{
	int i;

	for (i = 0; i < MAX_PPTP_SESSION; i++) {
		up_db[i].valid = 0;
		down_db[i].valid = 0;
	}

	/* any static rule hash could be created here? */

	return CS_E_OK;
}

cs_status_t cs_hw_accel_pptp_exit(void)
{
	cs_status_t ret;

	/* make sure SA ID for rekey is allocated */
	ret = cs_pptp_rekey_said_free();
	if (ret != CS_E_OK) {
		printk(KERN_ERR "%s: Fail to free SA ID for re-key.\n", __func__);
		return CS_E_ERROR;
	}

	return CS_E_OK;
}

