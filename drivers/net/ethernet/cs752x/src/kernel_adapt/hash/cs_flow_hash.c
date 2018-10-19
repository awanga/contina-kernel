/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
//#include <linux/export.h>
#include <linux/in.h>
#include <linux/if_vlan.h>
#include <mach/cs_types.h>
#include <mach/cs_network_types.h>
#include "cs_core_rule_hmu.h"
#include "cs_core_hmu.h"
#include "cs_core_vtable.h"
#include <mach/cs_rule_hash_api.h>
#include <mach/cs_flow_api.h>
#include "cs_fe.h"
#include "cs_mut.h"

#include <cs752x_proc.h>
extern u32 cs_adapt_debug;

#define CS_FLOW_HASH_TYPE       2

typedef struct cs_flow_hash_internal_s {
	cs_uint32_t	hash_type;
	cs_rule_hash_t	rule_hash;
	cs_flow_t	flow;
} cs_flow_hash_internal_t;

int cs_rule_hash_add_1(cs_rule_hash_t *p_rule_hash, fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry, fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt);
void cs_rule_hash_convert_to_fe(cs_rule_hash_t *p_rule_hash, fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry, fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt);
void cs_rule_hash_dump(fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry, fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt);
cs_status_t cs_flow_hash_delete(cs_uint16_t hash_index);

#define HW_MCGID_OFFSET 128
#define HW_MCIDX_OFFSET 1
extern cs_int16_t cs_mc_ctrl_manager_get_group_idx(cs_ip_address_t group_ip, cs_uint32_t l7_4_bytes);
extern cs_int16_t cs_mc_ctrl_manager_get_client_idx(cs_uint8_t * client_mac);
extern cs_int16_t cs_mc_ctrl_manager_del_by_group_idx(cs_int8_t group_idx);
extern cs_int16_t cs_mc_ctrl_manager_del_client_by_idx(cs_int8_t client_idx);
extern u16 cs_ni_get_mc_table(u8 mc_index);
extern void cs_ni_set_mc_table(u8 mc_index, u16 mc_vec);

static void cs_flow_hash_dump_cs_cb(cs_kernel_accel_cb_t *cs_cb)
{
#ifdef CONFIG_CS752X_PROC
	int i;

	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {

		printk("=== cs_kernel_accel_cb_t ===\n");
		printk("%s: fwdrslt_idx=%d\n", __func__, cs_cb->hw_rslt.fwdrslt_idx);
		printk("%s: qosrslt_idx=%d\n", __func__, cs_cb->hw_rslt.qosrslt_idx);

		printk("%s: input raw sa[]=", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			printk("[0x%x]-", (unsigned int)(cs_cb->input.raw.sa[i]));
		}
		printk("\n");

		printk("%s: input raw da[]=", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			printk("[0x%x]-", (unsigned int)(cs_cb->input.raw.da[i]));
		}
		printk("\n");

		printk("%s: output raw sa[]=", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			printk("[0x%x]-", (unsigned int)(cs_cb->output.raw.sa[i]));
		}
		printk("\n");

		printk("%s: output raw da[]=", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			printk("[0x%x]-", (unsigned int)(cs_cb->output.raw.da[i]));
		}
		printk("\n");

		printk("%s: input.raw.vlan_tpid=0x%x\n", __func__, cs_cb->input.raw.vlan_tpid);
		printk("%s: input.raw.vlan_tci=0x%x\n", __func__, cs_cb->input.raw.vlan_tci);
		printk("%s: input.raw.eth_protocol=0x%x\n", __func__, cs_cb->input.raw.eth_protocol);

		printk("%s: output.raw.vlan_tpid=0x%x\n", __func__, cs_cb->output.raw.vlan_tpid);
		printk("%s: output.raw.vlan_tci=0x%x\n", __func__, cs_cb->output.raw.vlan_tci);
		printk("%s: output.raw.eth_protocol=0x%x\n", __func__, cs_cb->output.raw.eth_protocol);

		printk("%s: action.l2.pppoe_op_en=%d\n", __func__, cs_cb->action.l2.pppoe_op_en);
		printk("%s: output.raw.pppoe_frame=0x%x\n", __func__, cs_cb->output.raw.pppoe_frame);

		printk("%s: input.l3_nh.iph.ver=%d\n", __func__, cs_cb->input.l3_nh.iph.ver);
		printk("%s: input.l3_nh.iph.protocol=%d\n", __func__, cs_cb->input.l3_nh.iph.protocol);
		printk("%s: input.l3_nh.iph.tos=0x%x\n", __func__, cs_cb->input.l3_nh.iph.tos);

		printk("%s: input.l3_nh.iph.sip=0x%x\n", __func__, cs_cb->input.l3_nh.iph.sip);
		printk("%s: input.l3_nh.iph.dip=0x%x\n", __func__, cs_cb->input.l3_nh.iph.dip);
		printk("%s: input.l3_nh.ipv6h.sip[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
			cs_cb->input.l3_nh.ipv6h.sip[0], cs_cb->input.l3_nh.ipv6h.sip[1], cs_cb->input.l3_nh.ipv6h.sip[2], cs_cb->input.l3_nh.ipv6h.sip[3]);
		printk("%s: input.l3_nh.ipv6h.dip[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
			cs_cb->input.l3_nh.ipv6h.dip[0], cs_cb->input.l3_nh.ipv6h.dip[1], cs_cb->input.l3_nh.ipv6h.dip[2], cs_cb->input.l3_nh.ipv6h.dip[3]);

		printk("%s: output.l3_nh.iph.ver=%d\n", __func__, cs_cb->output.l3_nh.iph.ver);
		printk("%s: output.l3_nh.iph.protocol=%d\n", __func__, cs_cb->output.l3_nh.iph.protocol);
		printk("%s: output.l3_nh.iph.tos=0x%x\n", __func__, cs_cb->output.l3_nh.iph.tos);

		printk("%s: output.l3_nh.iph.sip=0x%x\n", __func__, cs_cb->output.l3_nh.iph.sip);
		printk("%s: output.l3_nh.iph.dip=0x%x\n", __func__, cs_cb->output.l3_nh.iph.dip);
		printk("%s: output.l3_nh.ipv6h.sip[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
			cs_cb->output.l3_nh.ipv6h.sip[0], cs_cb->output.l3_nh.ipv6h.sip[1], cs_cb->output.l3_nh.ipv6h.sip[2], cs_cb->output.l3_nh.ipv6h.sip[3]);
		printk("%s: output.l3_nh.ipv6h.dip[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
			cs_cb->output.l3_nh.ipv6h.dip[0], cs_cb->output.l3_nh.ipv6h.dip[1], cs_cb->output.l3_nh.ipv6h.dip[2], cs_cb->output.l3_nh.ipv6h.dip[3]);

		printk("%s: common.ingress_port_id=%d\n", __func__, cs_cb->common.ingress_port_id);
		printk("%s: common.egress_port_id=%d\n", __func__, cs_cb->common.egress_port_id);

		printk("%s: input.l4_h.uh.dport=%d\n", __func__, cs_cb->input.l4_h.uh.dport);
		printk("%s: input.l4_h.uh.sport=%d\n", __func__, cs_cb->input.l4_h.uh.sport);
	}
#endif
}

static int cs_rep_hash_add(cs_rule_hash_t *p_rule_hash,cs_flow_t *p_flow,
	fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry,
	fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt)
{
	cs_rule_hash_t tmp_rule_hash;
	fe_flow_vlan_entry_t tmp_fvlan_entry;
	fe_sw_hash_t tmp_key;
	fe_voq_pol_entry_t tmp_voq_pol;
	fe_fwd_result_entry_t tmp_fwd_rslt;
	u32 crc32;
	u16 crc16, rslt_idx;
	int ret;
	u16 curr_mcidx;
	unsigned int sw_idx;

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s:%d: igress port = %d, egress port = %d\n", __func__, __LINE__, p_flow->ingress_pkt.phy_port, p_flow->egress_pkt.phy_port);
	}
#endif

	switch (p_flow->egress_pkt.phy_port) {
	case CS_PORT_GMAC0:
		if (p_flow->flow_type == CS_FLOW_TYPE_MC_MEMBER)
			curr_mcidx = 1 << 4;
		else
			curr_mcidx = 1 << (p_flow->egress_pkt.phy_port);
		break;
	case CS_PORT_GMAC1:
	case CS_PORT_GMAC2:
	case CS_PORT_OFLD0:
	case CS_PORT_OFLD1:
		curr_mcidx = 1 << (p_flow->egress_pkt.phy_port);
		break;
	case CS_PORT_CUSTOM0:
		curr_mcidx = 1 << 5;
		break;
	case CS_PORT_CUSTOM1:
		curr_mcidx = 1 << 6;
		break;
	case CS_PORT_CUSTOM2:
		curr_mcidx = 1 << 4;
		break;
	default:
		printk("%s: ERROR! Multicast can't support egress phy_port %d.\n", __func__, p_flow->egress_pkt.phy_port);
		return CS_E_ERROR;
		break;
	}

	/* check if replication hash exist */
	memcpy(&tmp_key, p_key, sizeof(fe_sw_hash_t));
	tmp_key.lspid = p_rule_hash->key.lspid;
	tmp_key.mc_idx = 0;

	ret = cs_fe_hash_calc_crc(&tmp_key, &crc32, &crc16,
			CRC16_CCITT);
	if (ret != 0){
		printk("%s calculate crc err fail err=%d\n", __func__, ret);
		return 0;
	}

	ret = cs_fe_hash_get_hash_by_crc(crc32, crc16,
			tmp_key.mask_ptr_0_7, &rslt_idx, &sw_idx);

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s:%d: crc32 = 0x%x, crc16 = 0x%x, ret = %d, rslt_idx = %d, sw_idx = 0x%x\n", __func__, __LINE__, crc32, crc16, ret, rslt_idx, sw_idx);
	}
#endif

	if (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER) {
		curr_mcidx = cs_ni_get_mc_table(p_rule_hash->mcgid) | (1 << p_rule_hash->mcidx);
		cs_ni_set_mc_table(p_rule_hash->mcgid, curr_mcidx);
		printk("%s set entry[%d] value 0x%x \n", __func__, p_rule_hash->mcgid, curr_mcidx);
	}

	if (ret == FE_TABLE_ENTRYNOTFOUND){
		/* create new replication hash */
		memcpy(&tmp_rule_hash, p_rule_hash, sizeof(cs_rule_hash_t));
		memset(&tmp_rule_hash.fwd_result, 0, sizeof(cs_rule_hash_fwd_result_t));
		switch (p_flow->egress_pkt.phy_port) {
		case CS_PORT_GMAC0:
		case CS_PORT_GMAC1:
		case CS_PORT_GMAC2:
		case CS_PORT_OFLD0:
		case CS_PORT_OFLD1:
		case CS_PORT_CPU:
			tmp_rule_hash.voq_pol.voq_base = ROOT_PORT_VOQ_BASE + p_flow->voq_offset;
			break;
		case CS_PORT_CUSTOM0:
		case CS_PORT_CUSTOM1:
		case CS_PORT_CUSTOM2:
			tmp_rule_hash.voq_pol.voq_base = ROOT_PORT_VOQ_BASE + (p_flow->voq_offset % 8); // any better solution?
			break;
		}
		if (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER)
			tmp_rule_hash.fwd_result.l2.mcgid = p_rule_hash->mcgid;
		else
			tmp_rule_hash.fwd_result.l2.mcgid = curr_mcidx;
		tmp_rule_hash.fwd_result.l2.mcgid_valid = CS_RESULT_ACTION_ENABLE;

		ret = cs_rule_hash_add_1(&tmp_rule_hash, &tmp_key, &tmp_fvlan_entry, &tmp_voq_pol, &tmp_fwd_rslt);

		if (ret != CS_OK) {
			printk("%s: cs_rule_hash_add_1() failed!!\n", __func__);
			return CS_ERROR;
		}

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s:%d: create new replication hash index = 0x%x\n", __func__, __LINE__, tmp_rule_hash.hash_index);
		}
#endif
	} else {
		/* update the fwdrslt of the replication hash */
		ret = cs_fe_table_get_entry(FE_TABLE_FWDRSLT,
					rslt_idx, &tmp_fwd_rslt);

		if (ret != FE_TABLE_OK){
			printk("%s get tmp_fwd_reslt fail err=%d\n",__func__, ret);
			return 0;
		}

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s:%d: update the fwdrslt of the replication hash, mcgid = 0x%x\n", __func__, __LINE__, tmp_fwd_rslt.l2.mcgid);
		}
#endif
		if ((p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
			(tmp_fwd_rslt.l2.mcgid & curr_mcidx) == 0) {
			/* make sure it is a new member */
			tmp_fwd_rslt.l2.mcgid |= curr_mcidx;
			ret = cs_fe_table_set_entry(FE_TABLE_FWDRSLT,
					rslt_idx, &tmp_fwd_rslt);

			if (ret != FE_TABLE_OK) {
				/* if update fwdrslt fail, should delete the
				 * whole hash tree, but will this happen?*/
				printk("%s update tmp_fwd_reslt mcgid fail err=%d\n",__func__, ret);
				return 0;
			}

#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s:%d: new mcgid = 0x%x\n", __func__, __LINE__, tmp_fwd_rslt.l2.mcgid);
			}
#endif
		}
	}

	return 0;
} /*cs_rep_hash_add*/

static int cs_rep_hash_del(cs_rule_hash_t *p_rule_hash,cs_flow_t *p_flow,
	fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry,
	fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt)
{
	cs_rule_hash_t tmp_rule_hash;
	fe_flow_vlan_entry_t tmp_fvlan_entry;
	fe_sw_hash_t tmp_key;
	fe_voq_pol_entry_t tmp_voq_pol;
	fe_fwd_result_entry_t tmp_fwd_rslt;
	u32 crc32;
	u16 crc16, rslt_idx, voq_pol_idx;
	int ret;
	u16 curr_mcidx;
	u16 mc_vec;
	unsigned int sw_idx;

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s:%d: igress port = %d, egress port = %d\n", __func__, __LINE__, p_flow->ingress_pkt.phy_port, p_flow->egress_pkt.phy_port);
	}
#endif

	memcpy(&tmp_rule_hash, p_rule_hash, sizeof(cs_rule_hash_t));

	switch (p_flow->egress_pkt.phy_port) {
	case CS_PORT_GMAC0:
		if (p_flow->flow_type == CS_FLOW_TYPE_MC_MEMBER)
			curr_mcidx = 1 << 4;
		else
			curr_mcidx = 1 << (p_flow->egress_pkt.phy_port);
		break;
	case CS_PORT_GMAC1:
	case CS_PORT_GMAC2:
	case CS_PORT_OFLD0:
	case CS_PORT_OFLD1:
		curr_mcidx = 1 << (p_flow->egress_pkt.phy_port);
		break;
	case CS_PORT_CUSTOM0:
		curr_mcidx = 1 << 5;
		break;
	case CS_PORT_CUSTOM1:
		curr_mcidx = 1 << 6;
		break;
	case CS_PORT_CUSTOM2:
		curr_mcidx = 1 << 4;
		break;
	default:
		printk("%s: ERROR! Multicast can't support egress phy_port %d.\n", __func__, p_flow->egress_pkt.phy_port);
		return CS_E_ERROR;
		break;
	}

	cs_rule_hash_convert_to_fe(&tmp_rule_hash, &tmp_key, &tmp_fvlan_entry, &tmp_voq_pol, &tmp_fwd_rslt);

	tmp_key.mc_idx = 0;

	ret = cs_fe_hash_calc_crc(&tmp_key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0){
		printk("%s calculate crc err fail err=%d\n", __func__, ret);
		return CS_OK;
	}

	ret = cs_fe_hash_get_hash_by_crc(crc32, crc16,
			tmp_key.mask_ptr_0_7, &rslt_idx, &sw_idx);

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s:%d: crc32 = 0x%x, crc16 = 0x%x, ret = %d, rslt_idx = %d, sw_idx = 0x%x\n", __func__, __LINE__, crc32, crc16, ret, rslt_idx, sw_idx);
	}
#endif

	if (ret == FE_TABLE_ENTRYNOTFOUND){
		//printk("%s: STRANGE! There is no replication hash to root port.\n", __func__);
		return CS_OK;
	} else {
		ret = cs_fe_table_get_entry(FE_TABLE_FWDRSLT,
					rslt_idx, &tmp_fwd_rslt);

		if (ret != FE_TABLE_OK){
			printk("%s get tmp_fwd_rslt fail err=%d\n",__func__, ret);
			return CS_OK;
		}

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s:%d: update the fwdrslt of the replication hash, mcgid = 0x%x\n", __func__, __LINE__, tmp_fwd_rslt.l2.mcgid);
		}
#endif

		if ((p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
			(tmp_fwd_rslt.l2.mcgid & curr_mcidx)) {
			tmp_fwd_rslt.l2.mcgid &= ~curr_mcidx;

			if (tmp_fwd_rslt.l2.mcgid > 0) {
				ret = cs_fe_table_set_entry(FE_TABLE_FWDRSLT,
						rslt_idx, &tmp_fwd_rslt);

				if (ret != FE_TABLE_OK) {
					printk("%s update tmp_fwd_reslt mcgid fail err=%d\n",__func__, ret);
					return CS_OK;
				}

#ifdef CONFIG_CS752X_PROC
				if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
					printk("%s:%d: new mcgid = 0x%x\n", __func__, __LINE__, tmp_fwd_rslt.l2.mcgid);
				}
#endif

			}
		}

		if (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER) {
			curr_mcidx = 1 << p_rule_hash->mcidx;
			curr_mcidx = cs_ni_get_mc_table(p_rule_hash->mcgid) & ~curr_mcidx;
			cs_ni_set_mc_table(p_rule_hash->mcgid, curr_mcidx);
			printk("%s set entry[%d] value 0x%x \n", __func__, p_rule_hash->mcgid, curr_mcidx);
		}

		if (((p_flow->flow_type != CS_FLOW_TYPE_MC_L7_FILTER) &&
			 (tmp_fwd_rslt.l2.mcgid == 0)) ||
			((p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER) &&
			 (curr_mcidx == 0))) {
				voq_pol_idx = tmp_fwd_rslt.dest.voq_pol_table_index;

#ifdef CONFIG_CS752X_PROC
				if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
					printk("%s:%d: del sw_idx = 0x%x, rslt_idx = 0x%x, voq_pol_idx = 0x%x\n", __func__, __LINE__, sw_idx, rslt_idx, voq_pol_idx);
				}
#endif

				/* Delete hash at first to avoid junk pkt */
				ret = cs_fe_hash_del_hash(sw_idx);
				if (ret != CS_OK) {
					printk("%s del replication hash index %u failed, ret = %d\n",__func__, sw_idx, ret);
				}
				/* Delete Result table */
				ret = cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
							rslt_idx, false);
				if (ret != CS_OK) {
					printk("%s del fwdrslt index %u failed, ret = %d\n",__func__, rslt_idx, ret);
				}

				/* Delete VOQPOL */
				cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
							voq_pol_idx, false);
				if (ret != CS_OK) {
					printk("%s del fwdrslt index %u failed, ret = %d\n",__func__, voq_pol_idx, ret);
				}

		}
	}

	return CS_OK;
} /*cs_rep_hash_del*/

cs_status_t cs_flow_hash_add(cs_rule_hash_t *p_rule_hash, cs_flow_t *p_flow)
{
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	fe_sw_hash_t key;
	cs_kernel_accel_cb_t  cb, *cs_cb = &cb;
	cs_uint8_t tpid_encap_type;
	cs_flow_hash_internal_t *p_flow_hash;
	cs_uint8_t tmp_mc_idx;
	cs_int8_t mc_entry_idx = -1;

	int ret;

	if ((p_flow->flow_type == CS_FLOW_TYPE_L3_MC) || (p_flow->flow_type == CS_FLOW_TYPE_MC_MEMBER)) {
		switch (p_flow->egress_pkt.phy_port) {
		case CS_PORT_GMAC0:
			if (p_flow->flow_type == CS_FLOW_TYPE_MC_MEMBER)
				p_rule_hash->key.mc_idx = 4;
			else
				p_rule_hash->key.mc_idx = p_flow->egress_pkt.phy_port;
			break;
		case CS_PORT_GMAC1:
		case CS_PORT_GMAC2:
		case CS_PORT_OFLD0:
		case CS_PORT_OFLD1:
			p_rule_hash->key.mc_idx = p_flow->egress_pkt.phy_port;
			break;
		case CS_PORT_CUSTOM0:
			p_rule_hash->key.mc_idx = 5;
			break;
		case CS_PORT_CUSTOM1:
			p_rule_hash->key.mc_idx = 6;
			break;
		case CS_PORT_CUSTOM2:
			p_rule_hash->key.mc_idx = 4;
			break;
		case CS_PORT_CPU:
		default:
			printk("%s: ERROR! flow type %d of multicast does not support egress port ID %d.\n",
				__func__, p_flow->flow_type, p_flow->egress_pkt.phy_port);
			return CS_ERROR;
			break;
		}
	}

	if (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER) {
		mc_entry_idx = cs_mc_ctrl_manager_get_group_idx(p_flow->ingress_pkt.da_ip,
			p_flow->ingress_pkt.natt_4_bytes);
		if (mc_entry_idx == -1) {
			printk("%s: ERROR! flow type %d cannot allocate mcgid entry\n",
				__func__, p_flow->flow_type);
			return CS_ERROR;
		}
		p_rule_hash->mcgid = mc_entry_idx + HW_MCGID_OFFSET;
		ret = cs_mc_ctrl_manager_get_client_idx(&p_flow->egress_pkt.da_mac[0]);
		if (ret == -1) {
			printk("%s: ERROR! flow type %d cannot allocate mc idx for entry %d \n",
				__func__, p_flow->flow_type, mc_entry_idx);
			cs_mc_ctrl_manager_del_by_group_idx(mc_entry_idx);
			return CS_ERROR;
		}
		p_rule_hash->mcidx = ret + HW_MCIDX_OFFSET;
		printk("%s assign mc entry[%d]: mc idx[%d]\n", __func__, p_rule_hash->mcgid, p_rule_hash->mcidx);
		p_rule_hash->key.mc_idx = p_rule_hash->mcidx;
	}

	if (p_flow->flow_type == CS_FLOW_TYPE_L3_MC) {
		/* the hash mask of CS_FLOW_TYPE_MC_MEMBER does not watch lspid */
		p_rule_hash->key.lspid = MCAST_PORT;
	}

	ret = cs_rule_hash_add_1(p_rule_hash, &key, &fvlan_entry, &voq_pol, &fwd_rslt);

	if (ret != CS_OK) {
		//printk("%s: cs_rule_hash_add_1() failed!!\n", __func__);
		if (p_rule_hash->fwd_result.act.drop == 0) {
			if (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER) {
				cs_mc_ctrl_manager_del_client_by_idx(p_rule_hash->mcidx - HW_MCIDX_OFFSET);
				cs_mc_ctrl_manager_del_by_group_idx(p_rule_hash->mcgid - HW_MCGID_OFFSET);
			}
		}
		return CS_ERROR;
	}

	p_flow_hash = cs_malloc(sizeof(cs_flow_hash_internal_t), GFP_ATOMIC);
	if (p_flow_hash == NULL) {
		printk("%s: malloc sizeof(cs_flow_hash_internal_t) failed!!\n", __func__);
		return CS_ERROR;
	}
	p_flow_hash->hash_type = CS_FLOW_HASH_TYPE;
	p_flow_hash->rule_hash = *p_rule_hash;
	p_flow_hash->flow = *p_flow;

	ret = cs_core_hmu_add_hash(p_rule_hash->hash_index, p_flow->life_time, p_flow_hash);
	if (ret != CS_OK) {
		printk("%s: cs_core_hmu_add_hash(hash_index=%d) failed!!\n", __func__, p_rule_hash->hash_index);
		cs_free(p_flow_hash);
		return CS_ERROR;
	}
	cs_core_hmu_set_result_idx(p_rule_hash->hash_index, p_rule_hash->fwd_rslt_idx, 0);

	memset(cs_cb, 0, sizeof(cs_kernel_accel_cb_t));

	cs_cb->hw_rslt.has_fwdrslt = 1;
	cs_cb->hw_rslt.fwdrslt_idx = p_rule_hash->fwd_rslt_idx;
	cs_cb->hw_rslt.has_qosrslt = 0;
	cs_cb->hw_rslt.qosrslt_idx = 0;
	memcpy(&(cs_cb->input.raw.sa[0]), &(p_flow->ingress_pkt.sa_mac[0]), CS_ETH_ADDR_LEN);
	memcpy(&(cs_cb->input.raw.da[0]), &(p_flow->ingress_pkt.da_mac[0]), CS_ETH_ADDR_LEN);
	memcpy(&(cs_cb->output.raw.sa[0]), &(p_flow->egress_pkt.sa_mac[0]), CS_ETH_ADDR_LEN);
	memcpy(&(cs_cb->output.raw.da[0]), &(p_flow->egress_pkt.da_mac[0]), CS_ETH_ADDR_LEN);

	tpid_encap_type = p_flow->ingress_pkt.tag[0].tpid_encap_type;
	if (tpid_encap_type == CS_VLAN_TPID_8100 || tpid_encap_type == CS_VLAN_TPID_9100 ||
	    tpid_encap_type == CS_VLAN_TPID_88A8 || tpid_encap_type == CS_VLAN_TPID_9200) {

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: ingress tpid_encap_type=0x%x\n", __func__, tpid_encap_type);
	}
#endif

		switch (tpid_encap_type) {
			case CS_VLAN_TPID_8100:
				cs_cb->input.raw.vlan_tpid = 0x8100;
				break;
			case CS_VLAN_TPID_9100:
				cs_cb->input.raw.vlan_tpid = 0x9100;
				break;
			case CS_VLAN_TPID_88A8:
				cs_cb->input.raw.vlan_tpid = 0x88a8;
				break;
			case CS_VLAN_TPID_9200:
				cs_cb->input.raw.vlan_tpid = 0x9200;
				break;
		}
		/* TODO: Need include priority */
		cs_cb->input.raw.vlan_tci = p_flow->ingress_pkt.tag[0].vlan_id;
	}
	cs_cb->input.raw.eth_protocol = p_flow->ingress_pkt.eth_type;

	tpid_encap_type = p_flow->egress_pkt.tag[0].tpid_encap_type;
	if (tpid_encap_type == CS_VLAN_TPID_8100 || tpid_encap_type == CS_VLAN_TPID_9100 ||
	    tpid_encap_type == CS_VLAN_TPID_88A8 || tpid_encap_type == CS_VLAN_TPID_9200) {

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: egress tpid_encap_type=0x%x\n", __func__, tpid_encap_type);
		}
#endif

		switch (tpid_encap_type) {
			case CS_VLAN_TPID_8100:
				cs_cb->output.raw.vlan_tpid = 0x8100;
				break;
			case CS_VLAN_TPID_9100:
				cs_cb->output.raw.vlan_tpid = 0x9100;
				break;
			case CS_VLAN_TPID_88A8:
				cs_cb->output.raw.vlan_tpid = 0x88a8;
				break;
			case CS_VLAN_TPID_9200:
				cs_cb->output.raw.vlan_tpid = 0x9200;
				break;
		}
		/* TODO: Need include priority */
		cs_cb->output.raw.vlan_tci = p_flow->egress_pkt.tag[0].vlan_id;
		cs_cb->output.raw.eth_protocol = p_flow->egress_pkt.eth_type;
	}

	/* set pppoe output parameter to cs_cb */
	if (p_flow->ingress_pkt.pppoe_session_id_valid && !p_flow->egress_pkt.pppoe_session_id_valid) {
		cs_cb->output_mask |= CS_HM_PPPOE_SESSION_ID_MASK;
		cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_REMOVE;
		cs_cb->output.raw.pppoe_frame = 0;
	}

	if (!p_flow->ingress_pkt.pppoe_session_id_valid && p_flow->egress_pkt.pppoe_session_id_valid) {
		cs_cb->output_mask |= CS_HM_PPPOE_SESSION_ID_MASK;
		cs_cb->action.l2.pppoe_op_en = CS_PPPOE_OP_INSERT;
		cs_cb->output.raw.pppoe_frame = p_flow->ingress_pkt.pppoe_session_id;
	}

	if (p_flow->ingress_pkt.sa_ip.afi == CS_IPV6) {
		cs_cb->input.l3_nh.iph.ver = 6;
		memcpy(cs_cb->input.l3_nh.ipv6h.sip, &(p_flow->ingress_pkt.sa_ip.ip_addr.addr[0]), sizeof(cs_cb->input.l3_nh.ipv6h.sip));
		memcpy(cs_cb->input.l3_nh.ipv6h.dip, &(p_flow->ingress_pkt.da_ip.ip_addr.addr[0]), sizeof(cs_cb->input.l3_nh.ipv6h.dip));
	}
	else {
		cs_cb->input.l3_nh.iph.ver = 4;
		cs_cb->input.l3_nh.iph.sip = p_flow->ingress_pkt.sa_ip.ip_addr.ipv4_addr;
		cs_cb->input.l3_nh.iph.dip = p_flow->ingress_pkt.da_ip.ip_addr.ipv4_addr;
	}


	cs_cb->input.l3_nh.iph.protocol = p_flow->ingress_pkt.protocol;;
	cs_cb->input.l3_nh.iph.tos = p_flow->ingress_pkt.tos;

	if (p_flow->egress_pkt.sa_ip.afi == CS_IPV6) {
		cs_cb->output.l3_nh.iph.ver = 6;
		memcpy(cs_cb->output.l3_nh.ipv6h.sip, &(p_flow->egress_pkt.sa_ip.ip_addr.addr[0]), sizeof(cs_cb->output.l3_nh.ipv6h.sip));
		memcpy(cs_cb->output.l3_nh.ipv6h.dip, &(p_flow->egress_pkt.da_ip.ip_addr.addr[0]), sizeof(cs_cb->output.l3_nh.ipv6h.dip));
	}
	else {
		cs_cb->output.l3_nh.iph.ver = 4;
		cs_cb->output.l3_nh.iph.sip = p_flow->egress_pkt.sa_ip.ip_addr.ipv4_addr;
		cs_cb->output.l3_nh.iph.dip = p_flow->egress_pkt.da_ip.ip_addr.ipv4_addr;
	}

	cs_cb->output.l3_nh.iph.protocol = p_flow->egress_pkt.protocol;;

	cs_cb->common.ingress_port_id = p_flow->ingress_pkt.phy_port;
	cs_cb->common.egress_port_id = p_flow->egress_pkt.phy_port;

	switch (p_flow->ingress_pkt.protocol) {
		case IPPROTO_TCP:
			cs_cb->input.l4_h.uh.dport = p_flow->ingress_pkt.l4_header.tcp.dport;
			cs_cb->input.l4_h.uh.sport = p_flow->ingress_pkt.l4_header.tcp.sport;
			break;
		case IPPROTO_UDP:
			cs_cb->input.l4_h.uh.dport = p_flow->ingress_pkt.l4_header.udp.dport;
			cs_cb->input.l4_h.uh.sport = p_flow->ingress_pkt.l4_header.udp.sport;
			break;
		case IPPROTO_ESP:
			break;
	}

	switch (p_flow->egress_pkt.protocol) {
		case IPPROTO_TCP:
			cs_cb->output.l4_h.uh.dport = p_flow->egress_pkt.l4_header.tcp.dport;
			cs_cb->output.l4_h.uh.sport = p_flow->egress_pkt.l4_header.tcp.sport;
			break;
		case IPPROTO_UDP:
			cs_cb->output.l4_h.uh.dport = p_flow->egress_pkt.l4_header.udp.dport;
			cs_cb->output.l4_h.uh.sport = p_flow->egress_pkt.l4_header.udp.sport;
			break;
		case IPPROTO_ESP:
			break;
	}

	cs_cb->common.swid_cnt = 0;
	if (p_flow->swid_array[CS_FLOW_SWID_BRIDGE] != 0) {
		cs_cb->common.swid_cnt++;
		cs_cb->common.swid[CS_SWID64_MOD_ID_BRIDGE] = CS_SWID64_MASK(CS_SWID64_MOD_ID_BRIDGE) | p_flow->swid_array[CS_FLOW_SWID_BRIDGE];
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: cs_cb->common.swid[CS_SWID64_MOD_ID_BRIDGE]=0x%llx\n", __func__, cs_cb->common.swid[CS_SWID64_MOD_ID_BRIDGE]);
		}
#endif
	}
	if (p_flow->swid_array[CS_FLOW_SWID_FORWARD] != 0) {
		cs_cb->common.swid_cnt++;
		if (p_flow->ingress_pkt.sa_ip.afi == CS_IPV6) {
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD] = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV6_FORWARD) | p_flow->swid_array[CS_FLOW_SWID_FORWARD];
#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD]=0x%llx\n", __func__, cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD]);
			}
#endif
		}
		else
		{
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD] = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPV4_FORWARD) | p_flow->swid_array[CS_FLOW_SWID_FORWARD];
#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
				printk("%s: cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD]=0x%llx\n", __func__, cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD]);
			}
#endif
		}
	}
	if (p_flow->swid_array[CS_FLOW_SWID_VPN] != 0) {
		cs_cb->common.swid_cnt++;
		cs_cb->common.swid[CS_SWID64_MOD_ID_IPSEC] = CS_SWID64_MASK(CS_SWID64_MOD_ID_IPSEC) | p_flow->swid_array[CS_FLOW_SWID_VPN];
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: cs_cb->common.swid[CS_SWID64_MOD_ID_IPSEC]=0x%llx\n", __func__, cs_cb->common.swid[CS_SWID64_MOD_ID_IPSEC]);
		}
#endif
	}

	cs_flow_hash_dump_cs_cb(cs_cb);

	p_flow_hash->hash_type = CS_FLOW_HASH_TYPE;
	p_flow_hash->flow = *p_flow;
	p_flow_hash->rule_hash = *p_rule_hash;

	/* check whether this flow need to be watched or not */
	if ((p_flow->flag & FLOW_FLAG_SKIP_HMU) == 0) {
		ret = cs_core_hmu_link_src_and_hash(cs_cb, p_rule_hash->hash_index, p_flow_hash);
		if (ret != CS_OK) {
			kfree(p_flow_hash);
			printk("%s: cs_core_hmu_link_src_and_hash(hash_index)=%d\n", __func__, p_rule_hash->hash_index);
			return ret;
		}
	}
	else {
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: No need to watch this flow hash!!, p_rule_hash->hash_index=%d\n", __func__, p_rule_hash->hash_index);
		}
#endif
	}

	/* FORWARD callback function will update connection track and ignore
	 * TCP window checking.
	 */
	cs_core_hmu_callback_for_hash_creation_pass(cs_cb);

	if (p_flow->flow_type == CS_FLOW_TYPE_L3_MC) {
		/* the hash mask of CS_FLOW_TYPE_MC_MEMBER does not watch lspid */
		p_rule_hash->key.lspid = p_flow->ingress_pkt.phy_port;
	}

	if ((p_rule_hash->fwd_result.act.drop == 0) &&
		((p_flow->flow_type == CS_FLOW_TYPE_L3_MC) || (p_flow->flow_type == CS_FLOW_TYPE_MC_MEMBER)
		|| (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER))) {
		tmp_mc_idx = p_rule_hash->key.mc_idx;
		p_rule_hash->key.mc_idx = 0;
		ret = cs_rep_hash_add(p_rule_hash, p_flow, &key, &fvlan_entry, &voq_pol, &fwd_rslt);

		/* Write back original value for QoS hash */
		p_rule_hash->key.mc_idx = tmp_mc_idx;

		if (ret != CS_OK) {
			//cs_free(p_flow_hash);
			printk("%s: cs_rep_hash_add() failed!\n", __func__);
			cs_flow_hash_delete(p_flow_hash->rule_hash.hash_index);
			return ret;
		}
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_hash_add);

cs_status_t cs_flow_qos_hash_add(cs_rule_hash_t *p_rule_hash, cs_flow_t *p_flow)
{
	int ret;
	cs_qos_hash_t qos_hash, *p_qos_hash=&qos_hash;
	cs_uint16_t qos_hash_index;
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	fe_sw_hash_t key;
	unsigned long long  fwd_hm_flag;

	memset(p_qos_hash, 0, sizeof(cs_qos_hash_t));

	if (p_flow->ingress_pkt.tag[0].priority != p_flow->egress_pkt.tag[0].priority) {
		p_qos_hash->action.change_8021p_1_en = 1;
		p_qos_hash->action.top_802_1p = p_flow->egress_pkt.tag[0].priority & 0x7;
	}
	if (p_flow->ingress_pkt.tag[1].priority != p_flow->egress_pkt.tag[1].priority) {
		p_qos_hash->action.change_8021p_2_en = 1;
		p_qos_hash->action.inner_802_1p = p_flow->egress_pkt.tag[1].priority & 0x7;
	}
	if (p_flow->ingress_pkt.tos != p_flow->egress_pkt.tos) {
		p_qos_hash->action.change_dscp_en = 1;
		p_qos_hash->action.dscp = (p_flow->egress_pkt.tos >> 2) & 0x3f;
	}

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: change_8021p_1_en=%d, change_8021p_2_en=%d, top_802_1p=0x%x, inner_802_1p=0x%x\n", __func__,
			p_qos_hash->action.change_8021p_1_en, p_qos_hash->action.change_8021p_2_en, p_qos_hash->action.top_802_1p, p_qos_hash->action.inner_802_1p);
		printk("%s: change_dscp_en=%d, dscp=%d\n", __func__, p_qos_hash->action.change_dscp_en, p_qos_hash->action.dscp);
	}
#endif

	if (p_qos_hash->action.change_8021p_1_en == 1 || p_qos_hash->action.change_8021p_2_en == 1 || p_qos_hash->action.change_dscp_en == 1) {

		cs_rule_hash_convert_to_fe(p_rule_hash, &key, &fvlan_entry, &voq_pol, &fwd_rslt);

		if (p_flow->flow_type == CS_FLOW_TYPE_L2) {
			p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L2_QOS_1;
		}
		else if (p_flow->flow_type == CS_FLOW_TYPE_L3_MC) {
			p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L3_QOS_MULTICAST;
		}
		else if (p_rule_hash->apptype == CORE_FWD_APP_TYPE_MCAST_CTRL_IPTV) {
			p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_MCAST_CTRL_IPTV;
		}
		else if (p_rule_hash->apptype == CORE_FWD_APP_TYPE_MCAST_CTRL_IP_SA) {
			p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_1P_DSCP_MAP;
		}
		else if (p_rule_hash->apptype == CORE_FWD_APP_TYPE_MCAST_L7_FILTER) {
			p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_MCAST_L7_FILTER_IPTV;
		}
		else if (p_flow->flow_type == CS_FLOW_TYPE_L4_NATT) {
			p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L7_QOS_GENERIC;
		}
		else {
			if (p_flow->ingress_pkt.eth_type == ETH_P_IPV6)
				p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L3_QOS_GENERIC_WITH_CHKSUM;
			else
				p_qos_hash->qos_app_type = CORE_QOS_APP_TYPE_L3_QOS_GENERIC;
		}

		ret = cs_core_vtable_get_hashmask_flag_from_apptype(p_qos_hash->qos_app_type, &fwd_hm_flag);
		if (ret != 0) {
			printk("%s:%d Can't get hashmask flag, ret = 0x%x\n", __func__, __LINE__, ret);
			return ret;
		}

		ret = cs_core_vtable_get_hashmask_index_from_apptype(p_qos_hash->qos_app_type, &(key.mask_ptr_0_7));
		if (ret != 0) {
			printk("%s:%d Can't get hashmask index, ret = 0x%x\n", __func__, __LINE__, ret);
			return CS_ERROR;
		}
		p_qos_hash->key = key;

		cs_rule_hash_dump(&key, &fvlan_entry, &voq_pol, &fwd_rslt);

		ret = cs_core_add_qos_hash(p_qos_hash, &qos_hash_index);
		if (ret != 0) {
			printk("%s: cs_core_add_qos_hash() failed!!, ret=%d\n", __func__, ret);
			return ret;
		}

		/* check whether flag bit 0 has been set */
		if (!(p_flow->flag & 1)) {
			ret = cs_core_hmu_link_fwd_and_qos_hash(p_flow->flow_id, qos_hash_index, 1);
		}
		else {
			ret = cs_core_hmu_link_fwd_and_qos_hash(p_flow->flow_id, qos_hash_index, 0);
		}

		if (ret != 0) {
			printk("%s: cs_core_hmu_link_fwd_and_qos_hash(hash_index=%d, qos_hash_index=%d)\n",
				__func__, p_flow->flow_id, qos_hash_index);
			cs_core_hmu_delete_hash_by_idx(qos_hash_index);
			return ret;
		}
		printk("%s: qos_hash_index=%d, qos rslt_idx=%d\n", __func__, qos_hash_index, p_qos_hash->rslt_idx);
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_qos_hash_add);

cs_status_t cs_flow_hash_get(cs_uint16_t hash_index, cs_flow_t *p_flow)
{
	int ret;
	cs_flow_hash_internal_t *p_flow_hash;
	cs_rule_hash_t *p_rule_hash;
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	fe_sw_hash_t key;

	ret = cs_core_hmu_get_hash_by_idx(hash_index, (void **)&p_flow_hash);

	if (ret != 0) {
		printk("%s: cs_core_hmu_get_hash_by_idx(hash_index=%d) failed, ret=%d\n", __func__, hash_index, ret);
		return CS_ERROR;
	}

	/* check whether the flow hash was created by traffic */
	if (p_flow_hash == NULL) {
		printk("%s: this hash (hash_index=%d) was created by traffic in stead of cs_flow_add!!\n", __func__, hash_index);
		return CS_ERROR;
	}

	*p_flow = p_flow_hash->flow;

	if (p_flow_hash->hash_type != CS_FLOW_HASH_TYPE) {
		printk("%s: hash type is %d, not FLOW HASH!!\n", __func__, p_flow_hash->hash_type);
		return CS_ERROR;
	}

	p_rule_hash = &(p_flow_hash->rule_hash);

	cs_rule_hash_convert_to_fe(p_rule_hash, &key, &fvlan_entry, &voq_pol, &fwd_rslt);

	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_hash_get);

cs_status_t cs_flow_hash_delete_data(void *data)
{
	cs_flow_hash_internal_t *p_flow_hash;
	cs_flow_t *p_flow;
	cs_rule_hash_t *p_rule_hash;
	fe_flow_vlan_entry_t fvlan_entry;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	fe_sw_hash_t key;
	int ret;

	p_flow_hash = (cs_flow_hash_internal_t *) data;
	if (p_flow_hash != NULL) {
		if (p_flow_hash->hash_type != CS_FLOW_HASH_TYPE) {
			cs_free(p_flow_hash);
			//printk("%s: hash type is %d, not FLOW HASH!!\n", __func__, p_flow_hash->hash_type);
			return CS_ERROR;
		}
		p_flow = &p_flow_hash->flow;
		p_rule_hash = &p_flow_hash->rule_hash;

		if (p_flow->flow_type == CS_FLOW_TYPE_L3_MC) {
			p_rule_hash->key.lspid = p_flow->ingress_pkt.phy_port;
		}

		if ((p_flow->egress_pkt.phy_port != 0xff) &&
			((p_flow->flow_type == CS_FLOW_TYPE_L3_MC)
			|| (p_flow->flow_type == CS_FLOW_TYPE_MC_MEMBER)
			|| (p_flow->flow_type == CS_FLOW_TYPE_MC_L7_FILTER))) {
			p_rule_hash->key.mc_idx = 0;
			ret = cs_rep_hash_del(p_rule_hash, p_flow, &key, &fvlan_entry, &voq_pol, &fwd_rslt);
			if (ret != CS_OK) {
				printk("%s: cs_rep_hash_del() failed!\n", __func__);
			}
		}
		cs_free(p_flow_hash);
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_hash_delete_data);

cs_status_t cs_flow_hash_delete(cs_uint16_t hash_index)
{
	int ret;
	cs_flow_hash_internal_t *p_flow_hash;
	cs_flow_t *p_flow;
	cs_rule_hash_t *p_rule_hash;
	fe_voq_pol_entry_t voq_pol;
	fe_fwd_result_entry_t fwd_rslt;
	fe_sw_hash_t key;
	fe_flow_vlan_entry_t fvlan_entry;

	ret = cs_core_hmu_get_hash_by_idx(hash_index, (void **)&p_flow_hash);

	if (ret != 0) {
		printk("%s: cs_core_hmu_get_hash_by_idx(hash_index=%d) failed, ret=%d\n", __func__, hash_index, ret);
		return CS_ERROR;
	}

	if (p_flow_hash->flow.flow_type == CS_FLOW_TYPE_MC_L7_FILTER) {

		p_flow = &p_flow_hash->flow;
		p_rule_hash = &p_flow_hash->rule_hash;

		if (p_flow->egress_pkt.phy_port != 0xff) {
			p_rule_hash->key.mc_idx = 0;
			cs_rep_hash_del(p_rule_hash, p_flow, &key, &fvlan_entry, &voq_pol, &fwd_rslt);

			cs_mc_ctrl_manager_del_client_by_idx(p_flow_hash->rule_hash.mcidx - HW_MCIDX_OFFSET);
			cs_mc_ctrl_manager_del_by_group_idx(p_flow_hash->rule_hash.mcgid - HW_MCGID_OFFSET);
			printk("%s: remove client idx[%d] at entry[%d]\n", __func__,
				p_flow_hash->rule_hash.mcidx, p_flow_hash->rule_hash.mcgid);
		}
	}

	ret = cs_core_hmu_delete_hash_by_idx(hash_index);

	if (ret != 0) {
		printk("%s: cs_core_hmu_delete_hash_by_idx(hash_index=%d) failed, ret=%d\n", __func__, hash_index, ret);
		return CS_ERROR;
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_hash_delete);

cs_status_t cs_flow_hash_get_lastuse_tickcount(cs_uint16_t flow_id, cs_uint32_t *lastuse_tickcount)
{
	int ret;

	ret = cs_core_hmu_get_last_use_by_hash_idx(flow_id, (unsigned long *)lastuse_tickcount);

	if (ret != 0) {
		printk("%s: cs_core_hmu_get_last_use_by_hash_idx(hash_index=%d) failed, ret=%d\n", __func__, flow_id, ret);
		return CS_ERROR;
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_flow_hash_get_lastuse_tickcount);
