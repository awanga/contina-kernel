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
#include <mach/cs_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_fe_table_api.h>
#include <cs_fe_head_table.h>
#include <cs_core_logic_data.h>
#include <cs_core_vtable.h>
#include <cs_fe_util_api.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include "cs_fe.h"
#include "cs_core_rule_hmu.h"
#include "cs_core_hmu.h"
#include <mach/cs_rule_hash_api.h>
#include "cs_mut.h"

#include <cs752x_proc.h>
extern u32 cs_adapt_debug;

#define CS_RULE_HASH_TYPE	1	
typedef struct cs_rule_hash_internal_s {
	cs_uint32_t	hash_type;
        cs_rule_hash_t  rule_hash;
} cs_rule_hash_internal_t;

void cs_rule_hash_dump(fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry, fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt)
{
#ifdef CONFIG_CS752X_PROC
        int i;

        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("=== fe_sw_hash_t ===\n");	
		printk("%s: mac_da[]=", __func__);
                for (i = 0; i < 6; i++) {
                        printk("[0x%x]-", (unsigned int)(p_key->mac_da[i]));
                }
		printk("\n");

		printk("%s: mac_sa[]=", __func__);
                for (i = 0; i < 6; i++) {
                        printk("[0x%x]-", (unsigned int)(p_key->mac_sa[i]));
                }
		printk("\n");

		printk("%s: eth_type=0x%x\n", __func__, p_key->eth_type);
		printk("%s: llc_type_enc=%d\n", __func__, p_key->llc_type_enc);
		printk("%s: ip_frag=%d\n", __func__, p_key->ip_frag);
		printk("%s: revd_115=%d\n", __func__, p_key->revd_115);
		printk("%s: tpid_enc_1=%d\n", __func__, p_key->tpid_enc_1);
		printk("%s: _8021p_1=%d\n", __func__, p_key->_8021p_1);
		printk("%s: dei_1=%d\n", __func__, p_key->dei_1);
		printk("%s: vid_1=%d\n", __func__, p_key->vid_1);
		printk("%s: revd_135=%d\n", __func__, p_key->revd_135);
		printk("%s: tpid_enc_2=%d\n", __func__, p_key->tpid_enc_2);
                printk("%s: _8021p_2=%d\n", __func__, p_key->_8021p_2);
                printk("%s: dei_2=%d\n", __func__, p_key->dei_2);
                printk("%s: vid_2=%d\n", __func__, p_key->vid_2);

		printk("%s: da[0-3]=", __func__);
                for (i = 0; i < 4; i++) {
			if (p_key->ip_version == CS_IPV4) {
                        	printk("[0x%x]-", (unsigned int)(htonl(p_key->da[i])));
			}
			else {
                        	printk("[0x%x]-", (unsigned int)(htonl(p_key->da[3-i])));
			}
                }
		printk("\n");

                printk("%s: sa[0-3]=", __func__);
                for (i = 0; i < 4; i++) {
			if (p_key->ip_version == CS_IPV4) {
                        	printk("[0x%x]-", (unsigned int)(htonl(p_key->sa[i])));
			}
			else {
                        	printk("[0x%x]-", (unsigned int)(htonl(p_key->sa[3-i])));
			}
                }
		printk("\n");
	
                printk("%s: ip_prot=%d\n", __func__, p_key->ip_prot);
                printk("%s: dscp=%d\n", __func__, p_key->dscp);
                printk("%s: ecn=%d\n", __func__, p_key->ecn);
                printk("%s: pktlen_rng_match_vector=%d\n", __func__, p_key->pktlen_rng_match_vector);
                printk("%s: ipv6_flow_label=0x%x\n", __func__, p_key->ipv6_flow_label);
                printk("%s: ip_version=%d\n", __func__, p_key->ip_version);
                printk("%s: ip_valid=%d\n", __func__, p_key->ip_valid);
                printk("%s: l4_dp=%d\n", __func__, p_key->l4_dp);
                printk("%s: l4_sp=%d\n", __func__, p_key->l4_sp);
                printk("%s: tcp_ctrl_flags=0x%x\n", __func__, (cs_uint32_t)p_key->tcp_ctrl_flags);
                printk("%s: tcp_ecn_flags=0x%x\n", __func__, (cs_uint32_t)p_key->tcp_ecn_flags);
                printk("%s: l4_valid=%d\n", __func__, (cs_uint32_t)p_key->l4_valid);
                printk("%s: sdbid=%d\n", __func__, (cs_uint32_t)p_key->sdbid);
                printk("%s: lspid=%d\n", __func__, (cs_uint32_t)p_key->lspid);
                printk("%s: fwdtype=%d\n", __func__, (cs_uint32_t)p_key->fwdtype);
                printk("%s: pppoe_session_id_valid=%d\n", __func__, (cs_uint32_t)p_key->pppoe_session_id_valid);
                printk("%s: pppoe_session_id=%d\n", __func__, (cs_uint32_t)p_key->pppoe_session_id);
                printk("%s: mask_ptr_0_7=%d\n", __func__, (cs_uint32_t)p_key->mask_ptr_0_7);
                printk("%s: mcgid=%d\n", __func__, (cs_uint32_t)p_key->mcgid);
                printk("%s: mc_idx=%d\n", __func__, (cs_uint32_t)p_key->mc_idx);
                printk("%s: da_an_mac_sel=%d\n", __func__, (cs_uint32_t)p_key->da_an_mac_sel);
                printk("%s: da_an_mac_hit=%d\n", __func__, (cs_uint32_t)p_key->da_an_mac_hit);
                printk("%s: sa_bng_mac_sel=%d\n", __func__, (cs_uint32_t)p_key->sa_bng_mac_sel);
                printk("%s: sa_bng_mac_hit=%d\n", __func__, (cs_uint32_t)p_key->sa_bng_mac_hit);
                printk("%s: orig_lspid=%d\n", __func__, (cs_uint32_t)p_key->orig_lspid);
                printk("%s: recirc_idx=%d\n", __func__, (cs_uint32_t)p_key->recirc_idx);
                printk("%s: l7_field=0x%x\n", __func__, (cs_uint32_t)p_key->l7_field);
                printk("%s: l7_field_valid=%d\n", __func__, (cs_uint32_t)p_key->l7_field_valid);
                printk("%s: hdr_a_flags_crcerr=%d\n", __func__, (cs_uint32_t)p_key->hdr_a_flags_crcerr);
                printk("%s: l3_csum_err=%d\n", __func__, (cs_uint32_t)p_key->l3_csum_err);
                printk("%s: l4_csum_err=%d\n", __func__, (cs_uint32_t)p_key->l4_csum_err);
                printk("%s: not_hdr_a_flags_stsvld=%d\n", __func__, (cs_uint32_t)p_key->not_hdr_a_flags_stsvld);
                printk("%s: hash_fid=%d\n", __func__, (cs_uint32_t)p_key->hash_fid);
                printk("%s: mc_da=%d\n", __func__, (cs_uint32_t)p_key->mc_da);
                printk("%s: bc_da=%d\n", __func__, (cs_uint32_t)p_key->bc_da);
                printk("%s: spi_vld=%d\n", __func__, (cs_uint32_t)p_key->spi_vld);
                printk("%s: spi_idx=%d\n", __func__, (cs_uint32_t)p_key->spi_idx);
		printk("\n\n");

		printk("=== fe_flow_vlan_entry_t ===\n");
                printk("%s: first_vlan_cmd=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->first_vlan_cmd);
                printk("%s: first_vid=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->first_vid);
                printk("%s: first_tpid_enc=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->first_tpid_enc);
                printk("%s: second_vlan_cmd=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->second_vlan_cmd);
                printk("%s: second_vid=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->second_vid);
                printk("%s: second_tpid_enc=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->second_tpid_enc);
                printk("%s: parity=%d\n", __func__, (cs_uint32_t)p_fvlan_entry->parity);
		printk("\n\n");
	
		printk("=== fe_voq_pol_entry_t ===\n");
                printk("%s: voq_base=%d\n", __func__, (cs_uint32_t)p_voq_pol->voq_base);
                printk("%s: pol_base=%d\n", __func__, (cs_uint32_t)p_voq_pol->pol_base);
                printk("%s: cpu_pid=%d\n", __func__, (cs_uint32_t)p_voq_pol->cpu_pid);
                printk("%s: ldpid=%d\n", __func__, (cs_uint32_t)p_voq_pol->ldpid);
                printk("%s: pppoe_session_id=%d\n", __func__, (cs_uint32_t)p_voq_pol->pppoe_session_id);
                printk("%s: cos_nop=%d\n", __func__, (cs_uint32_t)p_voq_pol->cos_nop);
                printk("%s: parity=%d\n", __func__, (cs_uint32_t)p_voq_pol->parity);
		printk("\n\n");

		printk("=== fe_fwd_result_entry_t ===\n");
                printk("%s: mac_sa_replace_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.mac_sa_replace_en);
                printk("%s: mac_da_replace_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.mac_da_replace_en);
                printk("%s: l2_index=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.l2_index);
                printk("%s: mcgid=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.mcgid);
                printk("%s: mcgid_valid=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.mcgid_valid);
                printk("%s: flow_vlan_op_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.flow_vlan_op_en);
                printk("%s: flow_vlan_index=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.flow_vlan_index);
                printk("%s: pppoe_encap_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.pppoe_encap_en);
                printk("%s: pppoe_decap_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l2.pppoe_decap_en);
		printk("\n");
                printk("%s: ip_sa_replace_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l3.ip_sa_replace_en);
                printk("%s: ip_da_replace_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l3.ip_da_replace_en);
                printk("%s: ip_sa_index=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l3.ip_sa_index);
                printk("%s: ip_da_index=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l3.ip_da_index);
                printk("%s: decr_ttl_hoplimit=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l3.decr_ttl_hoplimit);
		printk("\n");
                printk("%s: sp_replace_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l4.sp_replace_en);
                printk("%s: dp_replace_en=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l4.dp_replace_en);
                printk("%s: sp=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l4.sp);
                printk("%s: dp=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->l4.dp);
		printk("\n");
                printk("%s: pol_policy=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->dest.pol_policy);
                printk("%s: voq_policy=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->dest.voq_policy);
                printk("%s: voq_pol_table_index=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->dest.voq_pol_table_index);
		printk("\n");
                printk("%s: fwd_type_valid=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->act.fwd_type_valid);
                printk("%s: fwd_type=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->act.fwd_type);
                printk("%s: drop=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->act.drop);
		printk("\n");
                printk("%s: acl_dsbl=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->acl_dsbl);
                printk("%s: parity=%d\n", __func__, (cs_uint32_t)p_fwd_rslt->parity);
		printk("================");
       }
#endif

}

static void cs_l3_mac_addr_ntohl(char *p_dest, char *p_src)
{
        int i;

        for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
                p_dest[i] = p_src[CS_ETH_ADDR_LEN - 1 - i];
        }
}


void cs_rule_hash_convert_to_fe(cs_rule_hash_t *p_rule_hash, fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry, fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt)
{
        /* prepare the fe_sw_hash_t */
        memset(p_key, 0, sizeof(fe_sw_hash_t));

        /* chnage the network order to host order */
        //memcpy(&(key.mac_da[0]), &(p_rule_hash->key.mac_da[0]), CS_ETH_ADDR_LEN);
        //memcpy(&(key.mac_sa[0]), &(p_rule_hash->key.mac_sa[0]), CS_ETH_ADDR_LEN);
        p_key->mac_da[0] = p_rule_hash->key.mac_da[5];
        p_key->mac_da[1] = p_rule_hash->key.mac_da[4];
        p_key->mac_da[2] = p_rule_hash->key.mac_da[3];
        p_key->mac_da[3] = p_rule_hash->key.mac_da[2];
        p_key->mac_da[4] = p_rule_hash->key.mac_da[1];
        p_key->mac_da[5] = p_rule_hash->key.mac_da[0];

        p_key->mac_sa[0] = p_rule_hash->key.mac_sa[5];
        p_key->mac_sa[1] = p_rule_hash->key.mac_sa[4];
        p_key->mac_sa[2] = p_rule_hash->key.mac_sa[3];
        p_key->mac_sa[3] = p_rule_hash->key.mac_sa[2];
        p_key->mac_sa[4] = p_rule_hash->key.mac_sa[1];
        p_key->mac_sa[5] = p_rule_hash->key.mac_sa[0];

        p_key->eth_type   = p_rule_hash->key.eth_type;
        p_key->tpid_enc_1 = p_rule_hash->key.tpid_enc_1;
	p_key->_8021p_1   = p_rule_hash->key._8021p_1;
        p_key->vid_1      = p_rule_hash->key.vid_1;
        p_key->tpid_enc_2 = p_rule_hash->key.tpid_enc_2;
	p_key->_8021p_2   = p_rule_hash->key._8021p_2;
        p_key->vid_2      = p_rule_hash->key.vid_2;

        /* chnage the network order to host order */
        //memcpy(&(key.da[0]), &(p_rule_hash->key.da[0]), 4);
        //memcpy(&(key.sa[0]), &(p_rule_hash->key.sa[0]), 4);
        if (p_rule_hash->key.ip_version == CS_IPV4) {
                p_key->da[0] = ntohl(p_rule_hash->key.da[0]);
                p_key->sa[0] = ntohl(p_rule_hash->key.sa[0]);
        }
        else {
                p_key->da[0] = ntohl(p_rule_hash->key.da[3]);
                p_key->da[1] = ntohl(p_rule_hash->key.da[2]);
                p_key->da[2] = ntohl(p_rule_hash->key.da[1]);
                p_key->da[3] = ntohl(p_rule_hash->key.da[0]);
		
                p_key->sa[0] = ntohl(p_rule_hash->key.sa[3]);
                p_key->sa[1] = ntohl(p_rule_hash->key.sa[2]);
                p_key->sa[2] = ntohl(p_rule_hash->key.sa[1]);
                p_key->sa[3] = ntohl(p_rule_hash->key.sa[0]);
        }
        p_key->ip_prot    = p_rule_hash->key.ip_prot;
        p_key->dscp       = p_rule_hash->key.dscp;
        p_key->ip_version = p_rule_hash->key.ip_version;
        p_key->ip_valid   = p_rule_hash->key.ip_valid;
        p_key->l4_dp      = p_rule_hash->key.l4_dp;
        p_key->l4_sp      = p_rule_hash->key.l4_sp;
        p_key->l4_valid   = p_rule_hash->key.l4_valid;
        p_key->lspid      = p_rule_hash->key.lspid;
        p_key->fwdtype    = p_rule_hash->key.fwdtype;
        p_key->pppoe_session_id_valid    = p_rule_hash->key.pppoe_session_id_valid;
        p_key->pppoe_session_id          = p_rule_hash->key.pppoe_session_id;
        p_key->mask_ptr_0_7 = p_rule_hash->key.mask_ptr_0_7;
        p_key->mcgid      = p_rule_hash->key.mcgid;
        p_key->mc_idx     = p_rule_hash->key.mc_idx;
        p_key->orig_lspid = p_rule_hash->key.orig_lspid;
        p_key->recirc_idx = p_rule_hash->key.recirc_idx;
	p_key->l7_field = p_rule_hash->key.l7_field;
        p_key->l7_field_valid = p_rule_hash->key.l7_field_valid;
        p_key->ppp_protocol_vld = p_rule_hash->key.ppp_protocol_vld;
        p_key->ppp_protocol     = p_rule_hash->key.ppp_protocol;
        p_key->spi_vld    = p_rule_hash->key.spi_vld;
        p_key->spi_idx    = ntohl(p_rule_hash->key.spi_idx);

        /* prepare fe_flow_vlan_entry_t */
        memset(p_fvlan_entry, 0, sizeof(fe_flow_vlan_entry_t));
        p_fvlan_entry->first_vlan_cmd = p_rule_hash->fvlan.first_vlan_cmd;
        p_fvlan_entry->first_vid      = p_rule_hash->fvlan.first_vid;
        p_fvlan_entry->first_tpid_enc = p_rule_hash->fvlan.first_tpid_enc;
        p_fvlan_entry->second_vlan_cmd = p_rule_hash->fvlan.second_vlan_cmd;
        p_fvlan_entry->second_vid      = p_rule_hash->fvlan.second_vid;
        p_fvlan_entry->second_tpid_enc = p_rule_hash->fvlan.second_tpid_enc;
        p_fvlan_entry->parity          = p_rule_hash->fvlan.parity;

        /* set VoQ policer */
        memset(p_voq_pol, 0, sizeof(fe_voq_pol_entry_t));
        p_voq_pol->voq_base    = p_rule_hash->voq_pol.voq_base;
        p_voq_pol->pol_base    = p_rule_hash->voq_pol.pol_base;
        p_voq_pol->cpu_pid     = p_rule_hash->voq_pol.cpu_pid;
        p_voq_pol->ldpid       = p_rule_hash->voq_pol.ldpid;
        p_voq_pol->pppoe_session_id = p_rule_hash->voq_pol.pppoe_session_id;
        p_voq_pol->cos_nop     = p_rule_hash->voq_pol.cos_nop;
        p_voq_pol->parity      = p_rule_hash->voq_pol.parity;

        /* set fwd result */
        memset(p_fwd_rslt, 0x0, sizeof(fe_fwd_result_entry_t));
        p_fwd_rslt->l2.mac_sa_replace_en = p_rule_hash->fwd_result.l2.mac_sa_replace_en;
        p_fwd_rslt->l2.mac_da_replace_en = p_rule_hash->fwd_result.l2.mac_da_replace_en;
        p_fwd_rslt->l2.l2_index          = p_rule_hash->fwd_result.l2.l2_index;
        p_fwd_rslt->l2.mcgid             = p_rule_hash->fwd_result.l2.mcgid;
        p_fwd_rslt->l2.mcgid_valid       = p_rule_hash->fwd_result.l2.mcgid_valid;
	p_fwd_rslt->l2.flow_vlan_op_en   = p_rule_hash->fwd_result.l2.flow_vlan_op_en;
        p_fwd_rslt->l2.flow_vlan_index   = p_rule_hash->fwd_result.l2.flow_vlan_index;
        p_fwd_rslt->l2.pppoe_encap_en    = p_rule_hash->fwd_result.l2.pppoe_encap_en;
        p_fwd_rslt->l2.pppoe_decap_en    = p_rule_hash->fwd_result.l2.pppoe_decap_en;

        p_fwd_rslt->l3.ip_sa_replace_en  = p_rule_hash->fwd_result.l3.ip_sa_replace_en;
        p_fwd_rslt->l3.ip_da_replace_en  = p_rule_hash->fwd_result.l3.ip_da_replace_en;
        p_fwd_rslt->l3.ip_sa_index       = p_rule_hash->fwd_result.l3.ip_sa_index;
        p_fwd_rslt->l3.ip_da_index       = p_rule_hash->fwd_result.l3.ip_da_index;
        p_fwd_rslt->l3.decr_ttl_hoplimit = p_rule_hash->fwd_result.l3.decr_ttl_hoplimit;

        p_fwd_rslt->l4.sp_replace_en     = p_rule_hash->fwd_result.l4.sp_replace_en;
        p_fwd_rslt->l4.dp_replace_en     = p_rule_hash->fwd_result.l4.dp_replace_en;
        p_fwd_rslt->l4.sp                = p_rule_hash->fwd_result.l4.sp;
        p_fwd_rslt->l4.dp                = p_rule_hash->fwd_result.l4.dp;

        p_fwd_rslt->dest.pol_policy      = p_rule_hash->fwd_result.dest.pol_policy;
        p_fwd_rslt->dest.voq_policy      = p_rule_hash->fwd_result.dest.voq_policy;
        p_fwd_rslt->dest.voq_pol_table_index      = p_rule_hash->fwd_result.dest.voq_pol_table_index;

        p_fwd_rslt->act.fwd_type_valid   = p_rule_hash->fwd_result.act.fwd_type_valid;
        p_fwd_rslt->act.fwd_type         = p_rule_hash->fwd_result.act.fwd_type;
        p_fwd_rslt->act.drop             = p_rule_hash->fwd_result.act.drop;

        p_fwd_rslt->acl_dsbl             = p_rule_hash->fwd_result.acl_dsbl;
        p_fwd_rslt->parity               = p_rule_hash->fwd_result.parity;

        cs_rule_hash_dump(p_key, p_fvlan_entry, p_voq_pol, p_fwd_rslt);

}

int cs_rule_hash_add_1(cs_rule_hash_t *p_rule_hash, fe_sw_hash_t *p_key, fe_flow_vlan_entry_t *p_fvlan_entry, fe_voq_pol_entry_t *p_voq_pol, fe_fwd_result_entry_t *p_fwd_rslt)
{
	unsigned long long fwd_hm_flag;
	unsigned int crc32;
	unsigned short crc16;
	int ret = 0;
	int type = 0;
	cs_uint8_t tmp_buf[CS_ETH_ADDR_LEN * 2];
        cs_uint8_t *ptr;
	cs_uint16_t *p_hash_index = &(p_rule_hash->hash_index);
	cs_uint32_t *p_voq_pol_idx = &(p_rule_hash->voq_pol_idx);
	cs_uint32_t *p_fwd_rslt_idx = &(p_rule_hash->fwd_rslt_idx);
	cs_uint32_t *p_vlan_rslt_idx = &(p_rule_hash->vlan_rslt_idx);
	cs_uint32_t tmp_ip_da[4], tmp_ip_sa[4];
	cs_uint32_t value;

	cs_rule_hash_convert_to_fe(p_rule_hash, p_key, p_fvlan_entry, p_voq_pol, p_fwd_rslt);

	/* Please refer to ./drivers/net/cs752x/src/include/cs_core_vtable.h for apptype definition */
	ret = cs_core_vtable_get_hashmask_flag_from_apptype(p_rule_hash->apptype, &fwd_hm_flag);
	if (ret != 0) {
		printk("%s:%d Can't get hashmask flag, ret = 0x%x\n", __func__, __LINE__, ret);
		return CS_ERROR;
	}

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: p_rule_hash->apptype=%d\n", __func__, p_rule_hash->apptype);
	}
#endif

	ret = cs_core_vtable_get_hashmask_index_from_apptype(p_rule_hash->apptype, &(p_key->mask_ptr_0_7));
	if (ret != 0) {
		printk("%s:%d Can't get hashmask index, ret = 0x%x\n", __func__, __LINE__, ret);
		return CS_ERROR;
	}
	p_rule_hash->key.mask_ptr_0_7 = p_key->mask_ptr_0_7;

	/* set VoQ policer */
	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, p_voq_pol, p_voq_pol_idx);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d Can't add VoQ policer\n", __func__, __LINE__);
		return CS_ERROR;
	}

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: p_rule_hash->key.mask_ptr_0_7=%d\n", __func__, p_rule_hash->key.mask_ptr_0_7);
		printk("%s: *p_voq_pol_idx=%d\n", __func__, *p_voq_pol_idx);
	}
#endif

	/* set FVLAN command */
	if (p_fwd_rslt->l2.flow_vlan_op_en == 1) {
		ret = cs_fe_table_add_entry(FE_TABLE_FVLAN, p_fvlan_entry, p_vlan_rslt_idx);
 		if (ret != FE_TABLE_OK) {
			printk("%s:%d Can't add VLAN result\n", __func__, __LINE__);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);
			return CS_ERROR;
		}

#ifdef CONFIG_CS752X_PROC
        	if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: *p_vlan_rslt_idx=%d\n", __func__, *p_vlan_rslt_idx);
		}
#endif
	}

	/* set L2 for fwd result */
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: p_fwd_rslt->l2.mac_sa_replace_en=%d, p_fwd_rslt->l2.mac_da_replace_en=%d\n", __func__, p_fwd_rslt->l2.mac_sa_replace_en, p_fwd_rslt->l2.mac_da_replace_en);
	}
#endif

	if (p_fwd_rslt->l2.mac_sa_replace_en == 1 && p_fwd_rslt->l2.mac_da_replace_en == 1)
                type = L2_LOOKUP_TYPE_PAIR;
        else if (p_fwd_rslt->l2.mac_sa_replace_en == 1 && p_fwd_rslt->l2.mac_da_replace_en == 0)
                type = L2_LOOKUP_TYPE_SA;
        else if (p_fwd_rslt->l2.mac_sa_replace_en == 0 && p_fwd_rslt->l2.mac_da_replace_en == 1)
                type = L2_LOOKUP_TYPE_DA;
        else
                type = FE_TABLE_EOPNOTSUPP;
        if (type != FE_TABLE_EOPNOTSUPP) {
                if (type == L2_LOOKUP_TYPE_PAIR) {
			cs_l3_mac_addr_ntohl(&tmp_buf[0], &(p_rule_hash->fwd_result.l2.mac_da[0]));
			cs_l3_mac_addr_ntohl(&tmp_buf[CS_ETH_ADDR_LEN], &(p_rule_hash->fwd_result.l2.mac_sa[0]));
                }
                else if (type == L2_LOOKUP_TYPE_SA)
			cs_l3_mac_addr_ntohl(&tmp_buf[0], &(p_rule_hash->fwd_result.l2.mac_sa[0]));
                else
			cs_l3_mac_addr_ntohl(&tmp_buf[0], &(p_rule_hash->fwd_result.l2.mac_da[0]));
                ptr = &tmp_buf[0];

                ret = cs_fe_l2_result_alloc(ptr, type, &value);
                if (ret != FE_TABLE_OK) {
                        printk("%s: cs_fe_l2_result_alloc(type=%d) return failed!! ret=%d\n", __func__, type,  ret);
				cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);
                                return CS_E_ERROR;
                }
		p_rule_hash->mac_type = type;
		p_fwd_rslt->l2.l2_index = (cs_uint16_t)value;
		p_rule_hash->fwd_result.l2.l2_index = (cs_uint16_t)value;

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
                	printk("%s: mac_type=%d, mac_idx=%d\n", __func__, type, value);
        	}
#endif

	}

	/* set L3 for fwd result */
	if (p_rule_hash->fwd_result.l3.ip_da_replace_en) {
		if (p_rule_hash->key.ip_version == CS_IPV4) {
			tmp_ip_da[0] = ntohl(p_rule_hash->fwd_result.l3.ip_da[0]);
		}
		else {
			tmp_ip_da[0] = ntohl(p_rule_hash->fwd_result.l3.ip_da[3]);	
			tmp_ip_da[1] = ntohl(p_rule_hash->fwd_result.l3.ip_da[2]);	
			tmp_ip_da[2] = ntohl(p_rule_hash->fwd_result.l3.ip_da[1]);	
			tmp_ip_da[3] = ntohl(p_rule_hash->fwd_result.l3.ip_da[0]);	
		}
		ret = cs_fe_l3_result_alloc(&(tmp_ip_da[0]), p_rule_hash->key.ip_version, &value);
		if (ret != CS_OK) {
			printk("%s: cs_fe_l3_result_alloc(DA) failed!! ret=%d\n", __func__, ret);
			cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);
			if (p_rule_hash->mac_type != FE_TABLE_EOPNOTSUPP)
                        	cs_fe_l2_result_dealloc(p_fwd_rslt->l2.l2_index, p_rule_hash->mac_type);
			return CS_E_ERROR;
		}
		p_fwd_rslt->l3.ip_da_index = (cs_uint16_t)value;
		p_rule_hash->fwd_result.l3.ip_da_index = (cs_uint16_t)value;
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
			printk("%s: ip_da_index=%d\n", __func__, p_fwd_rslt->l3.ip_da_index);
		}
#endif
	}

	if (p_rule_hash->fwd_result.l3.ip_sa_replace_en) {

		if (p_rule_hash->key.ip_version == CS_IPV4) {
                        tmp_ip_sa[0] = ntohl(p_rule_hash->fwd_result.l3.ip_sa[0]);
                }
                else {
                        tmp_ip_sa[0] = ntohl(p_rule_hash->fwd_result.l3.ip_sa[3]);
                        tmp_ip_sa[1] = ntohl(p_rule_hash->fwd_result.l3.ip_sa[2]);
                        tmp_ip_sa[2] = ntohl(p_rule_hash->fwd_result.l3.ip_sa[1]);
                        tmp_ip_sa[3] = ntohl(p_rule_hash->fwd_result.l3.ip_sa[0]);
                }
                ret = cs_fe_l3_result_alloc(&(tmp_ip_sa[0]), p_rule_hash->key.ip_version, &value);
                if (ret != CS_OK) {
                        printk("%s: cs_fe_l3_result_alloc(SA) failed!! ret=%d\n", __func__, ret);
                        cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);
                        if (p_rule_hash->mac_type != FE_TABLE_EOPNOTSUPP)
                                cs_fe_l2_result_dealloc(p_fwd_rslt->l2.l2_index, p_rule_hash->mac_type);
			if (p_rule_hash->fwd_result.l3.ip_da_replace_en)
				cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_da_index);
                        return CS_E_ERROR;
                }
		p_fwd_rslt->l3.ip_sa_index = (cs_uint16_t)value;
		p_rule_hash->fwd_result.l3.ip_sa_index = (cs_uint16_t)value;

#ifdef CONFIG_CS752X_PROC
       		if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
                	printk("%s: ip_sa_index=%d\n", __func__, p_fwd_rslt->l3.ip_sa_index);
       		} 
#endif
        }

	/* set fwd result */
	p_fwd_rslt->dest.voq_pol_table_index = *p_voq_pol_idx;
	p_fwd_rslt->l2.flow_vlan_index =  *p_vlan_rslt_idx;
	/* remove outer VLAN if any */
	
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, p_fwd_rslt, p_fwd_rslt_idx);
	if (ret != FE_TABLE_OK) {
		printk("%s:%d Can't add forwarding result\n", __func__, __LINE__);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN, *p_vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);

		if (p_rule_hash->mac_type != FE_TABLE_EOPNOTSUPP)
			cs_fe_l2_result_dealloc(p_fwd_rslt->l2.l2_index, p_rule_hash->mac_type);

		if (p_fwd_rslt->l3.ip_da_replace_en)
                        cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_da_index);

		if (p_fwd_rslt->l3.ip_sa_replace_en)
                        cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_sa_index);

		return CS_ERROR;
	}

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: *p_fwd_rslt_idx=%d\n", __func__, *p_fwd_rslt_idx);
	}
#endif

	/* create fwd hash */
	ret = cs_fe_hash_calc_crc(p_key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
		printk("%s:%d Can't get crc32, ret = 0x%x\n", __func__, __LINE__, ret);

		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, *p_fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN, *p_vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);

		if (p_rule_hash->mac_type != FE_TABLE_EOPNOTSUPP)
                        cs_fe_l2_result_dealloc(p_fwd_rslt->l2.l2_index, p_rule_hash->mac_type);

		if (p_fwd_rslt->l3.ip_da_replace_en)
                        cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_da_index);

                if (p_fwd_rslt->l3.ip_sa_replace_en)
                        cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_sa_index);
		return CS_ERROR;
	}

	ret = cs_fe_hash_add_hash(crc32, crc16, p_key->mask_ptr_0_7, *p_fwd_rslt_idx, p_hash_index);
	if (ret != 0) {
		//printk("%s:%d Can't add forwarding hash, ret = %d\n", __func__, __LINE__, ret);

		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, *p_fwd_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN, *p_vlan_rslt_idx, false);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, *p_voq_pol_idx, false);

		if (p_rule_hash->mac_type != FE_TABLE_EOPNOTSUPP)
                	cs_fe_l2_result_dealloc(p_fwd_rslt->l2.l2_index, p_rule_hash->mac_type);
		if (p_fwd_rslt->l3.ip_da_replace_en)
                        cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_da_index);

                if (p_fwd_rslt->l3.ip_sa_replace_en)
                        cs_fe_l3_result_dealloc(p_fwd_rslt->l3.ip_sa_index);
		return CS_ERROR;

	}

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_RULE_HASH) {
		printk("%s: *p_hash_index=%d\n", __func__, *p_hash_index);
	}
#endif

	//printk("%s: hash = %d, VoQ policer = %d, fwd_rslt = %d, VLAN result index = %d\n", __func__, 
	//	(cs_uint32_t)*p_hash_index,(cs_uint32_t)*p_voq_pol_idx, (cs_uint32_t)*p_fwd_rslt_idx, (cs_uint32_t)*p_vlan_rslt_idx);

	return CS_OK;
}


cs_status_t cs_rule_hash_add(cs_dev_id_t device_id, cs_rule_hash_t *p_rule_hash)
{
	fe_flow_vlan_entry_t fvlan_entry;
  	fe_voq_pol_entry_t voq_pol;
  	fe_fwd_result_entry_t fwd_rslt;
  	fe_sw_hash_t key;
	cs_rule_hash_internal_t *p_rule_hash_internal;

	int ret;

  	ret = cs_rule_hash_add_1(p_rule_hash, &key, &fvlan_entry, &voq_pol, &fwd_rslt);
	
	if (ret != CS_OK) {
		printk("%s: cs_rule_hash_add_1() failed!!\n", __func__);
		return CS_ERROR;
	}

	p_rule_hash_internal = cs_malloc(sizeof(cs_rule_hash_internal_t), GFP_ATOMIC);
        if (p_rule_hash_internal == NULL) {
                printk("%s: malloc sizeof(cs_rule_hash_internal_t) failed!!\n", __func__);
                return CS_ERROR;
        }

	p_rule_hash_internal->hash_type = CS_RULE_HASH_TYPE;
	p_rule_hash_internal->rule_hash = *p_rule_hash;

	ret = cs_core_rule_hmu_add_hash(p_rule_hash->hash_index, p_rule_hash_internal);
        if (ret != CS_OK) {
                printk("%s: cs_core_rule_hmu_add_hash(hash_index=%d) failed!!\n", __func__, p_rule_hash->hash_index);
		cs_free(p_rule_hash_internal);
                return CS_ERROR;
        }
        cs_core_rule_hmu_set_result_idx(p_rule_hash->hash_index, p_rule_hash->fwd_rslt_idx, 0);

	ret = cs_core_rule_hmu_link_src_and_hash(p_rule_hash->hash_index, p_rule_hash_internal);
	if (ret != CS_OK) {
		printk("%s: cs_core_rule_hmu_link_src_and_hash(hash_index)=%d\n", __func__, p_rule_hash->hash_index);
		kfree(p_rule_hash_internal);
		return ret;
	}
		
	return CS_OK;
}
EXPORT_SYMBOL(cs_rule_hash_add);


cs_status_t cs_rule_hash_get_by_hash_index(cs_dev_id_t device_id, cs_uint16_t hash_index, cs_rule_hash_t *p_rule_hash)
{
        int ret;
	cs_uint32_t save_adapt_debug;
	cs_core_rule_hmu_data_t rule_hmu_data, *p_rule_hmu_data=&rule_hmu_data;
	fe_flow_vlan_entry_t *p_fvlan_entry = &(p_rule_hmu_data->fvlan_entry);
        fe_voq_pol_entry_t *p_voq_pol = &(p_rule_hmu_data->voq_pol);
        fe_fwd_result_entry_t *p_fwd_rslt = &(p_rule_hmu_data->fwd_rslt);
        fe_sw_hash_t *p_key = &(p_rule_hmu_data->key);
	cs_rule_hash_internal_t	*p_rule_hash_internal;

	ret = cs_core_rule_hmu_get_data_by_hash_index(hash_index, (unsigned char *)&hash_index, &p_rule_hash_internal);

	if (ret != CS_OK) {
		printk("%s: get hash_index=%d failed!!\n", __func__, hash_index);
		return ret;
	}

	if (p_rule_hash_internal == NULL) {
		printk("%s: this rule hash does not created by rule hash add!!\n", __func__);
		return CS_ERROR;
	}

	if (p_rule_hash_internal->hash_type != CS_RULE_HASH_TYPE) {
		printk("%s: hash type=%d is not a rule hash!!\n", __func__, p_rule_hash_internal->hash_type);
		return CS_ERROR;
	}

	*p_rule_hash = p_rule_hash_internal->rule_hash;
	cs_rule_hash_convert_to_fe(p_rule_hash, p_key, p_fvlan_entry, p_voq_pol, p_fwd_rslt);

	//printk("%s: hash_indx=%d, vo1_pol_idx=%d, fwd_rslt_idx=%d, vlan_rslt_idx=%d\n", 
	//	__func__, p_rule_hash->hash_index, p_rule_hash->voq_pol_idx, p_rule_hash->fwd_rslt_idx, p_rule_hash->vlan_rslt_idx);

	cs_rule_hash_dump(p_key, p_fvlan_entry, p_voq_pol, p_fwd_rslt);
	
	return ret;
}
EXPORT_SYMBOL(cs_rule_hash_get_by_hash_index);

cs_status_t cs_rule_hash_delete_by_hash_index(cs_dev_id_t device_id, cs_uint16_t hash_index)
{
	int ret;
	cs_rule_hash_t rule_hash, *p_rule_hash=&rule_hash;

	ret = cs_rule_hash_get_by_hash_index(device_id, hash_index, p_rule_hash);
	if (ret != CS_OK) {
		printk("%s: cs_rule_hash_get_by_hash_index(hash_index=%d) failed !!, ret=%d\n", __func__, hash_index, ret);
		return CS_ERROR;
	}

	//printk("%s: hash_index=%d, fwd_rslt_idx=%d, voq_pol_idx=%d, vlan_rslt_idx=%d\n", 
	//	__func__, p_rule_hash->hash_index, p_rule_hash->fwd_rslt_idx, p_rule_hash->voq_pol_idx, p_rule_hash->vlan_rslt_idx);
	//printk("%s: mac_type=%d, ip_da_replace_en=%d, ip_sa_replace_en=%d, l2_index=%d, ip_da_index=%d, ip_sa_index=%d\n", 
	//	__func__, p_rule_hash->mac_type, p_rule_hash->fwd_result.l3.ip_da_replace_en, p_rule_hash->fwd_result.l3.ip_sa_replace_en, 
	//	p_rule_hash->fwd_result.l2.l2_index, p_rule_hash->fwd_result.l3.ip_da_index, p_rule_hash->fwd_result.l3.ip_sa_index);
	
	/* del hash at first to avoid junk pkt */
        ret = cs_fe_hash_del_hash(p_rule_hash->hash_index);
	if (ret != 0) {
		printk("%s: cs_fe_hash_del_hash(hash_index=%d) failed!!\n", __func__, p_rule_hash->hash_index);
		return CS_ERROR;
	}

	ret = cs_core_rule_hmu_delete_hash_by_idx(p_rule_hash->hash_index);
	if (ret != 0) {
		printk("%s: cs_core_rule_hmu_delete_hash_by_idx(hash_index=%d) failed\n", __func__, p_rule_hash->hash_index);
		return CS_ERROR;
	}

	return CS_OK;
}
EXPORT_SYMBOL(cs_rule_hash_delete_by_hash_index);

