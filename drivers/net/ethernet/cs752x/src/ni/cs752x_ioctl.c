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

#include <linux/netdevice.h>
#include <mach/platform.h>
#include "cs752x_ioctl.h"
#include "cs752x_eth.h"
#include "cs_core_vtable.h"
#include "cs_core_logic_data.h"
#include "cs_core_hmu.h"
#include "cs752x_sch.h"
#include "cs752x_voq_cntr.h"

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
#include <mach/cs_types.h>
#include <mach/cs_tunnel.h>
#include <mach/cs_tunnel_iplip_api.h>
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */

#ifdef CONFIG_CS752X_HW_ACCELERATION
#include "cs_hw_accel_qos.h"
#endif /* CONFIG_CS752X_HW_ACCELERATION */

#ifdef CONFIG_CS752X_PROC
#define DBG(x) {if (cs_ni_debug == 7) x;}
#else
#define DBG(x) {}
#endif

#define MAX_IN_QOS_VOQ_WAN	2
#define MAX_IN_QOS_VOQ_LAN	5

#define IN_QOS_MASK_BIT_DIP	0x01
#define IN_QOS_MASK_BIT_SIP	0x02
#define IN_QOS_MASK_BIT_DSCP	0x04
#define IN_QOS_MASK_BIT_8021P	0x08
#define IN_QOS_MASK_BIT_VID	0x10
#define IN_QOS_MASK_BIT_LSPID	0x20
#define IN_QOS_MASK_BIT_DEL	0x40

unsigned short qos_enabled_lan = 0;	/* bit8: enable or not
				 * bit0~7: means how many SP VOQ we have requested
				 */

unsigned short qos_enabled_wan = 0;	/* bit8: enable or not
				 * bit0~7: means how many SP VOQ we have requested
				 */

extern cs_vtable_t* vtable_list[CORE_VTABLE_TYPE_MAX];
#define TUPLE_QOS_LAN	2
unsigned int mask_idx_lan;
unsigned int voqpol_idxs_lan[MAX_IN_QOS_VOQ_LAN];
unsigned int voqpol_lan[MAX_IN_QOS_VOQ_LAN];
unsigned short result_lan[MAX_IN_QOS_VOQ_LAN];
unsigned int hw_index_lan[MAX_IN_QOS_VOQ_LAN];
fe_voq_pol_entry_t voqpol_entries_lan[MAX_IN_QOS_VOQ_LAN];
cs_fwd_hash_t hash_entry_lan[MAX_IN_QOS_VOQ_LAN];

#define TUPLE_QOS_WAN	3
unsigned int mask_idx_wan;
unsigned int voqpol_idxs_wan[MAX_IN_QOS_VOQ_WAN];
unsigned int voqpol_wan[MAX_IN_QOS_VOQ_WAN];
unsigned short result_wan[MAX_IN_QOS_VOQ_WAN];
unsigned int hw_index_wan[MAX_IN_QOS_VOQ_WAN];
fe_voq_pol_entry_t voqpol_entries_wan[MAX_IN_QOS_VOQ_WAN];
cs_fwd_hash_t hash_entry_wan[MAX_IN_QOS_VOQ_WAN];


int qos_add_chain_lan(fe_ingress_qos_table_entry_t *p_rslt, int alloc_mask_ram)
{
	int ret;
	unsigned int voqpol_idx, rslt_idx, crc32;
	u16 hw_index;
	unsigned short crc16;
	//unsigned char mask_ptr;
	fe_sw_hash_t key;
	fe_fwd_result_entry_t action;
	fe_hash_mask_entry_t hm_entry;
	fe_voq_pol_entry_t *voqpol_entry = &voqpol_entries_lan[qos_enabled_lan&0xff];
	fe_sdb_entry_t sdb_entry;
	cs_vtable_t *p_vtable = vtable_list[CORE_VTABLE_TYPE_L3_FLOW];
	cs_fwd_hash_t *phash_entry = &hash_entry_lan[qos_enabled_lan&0xFF];

	memset(&key, 0, sizeof(fe_sw_hash_t));
	memset(&action, 0, sizeof(fe_fwd_result_entry_t));
	memset(voqpol_entry, 0, sizeof(voqpol_entry));
	memset(phash_entry, 0x0, sizeof(cs_fwd_hash_t));

	phash_entry->lifetime = 0;

	/* Create mask ram */
	if ( alloc_mask_ram == 1){
		//init_hashmask_entry(&hm_entry);
		memset(&hm_entry, 0xff, sizeof(fe_hash_mask_entry_t));
		hm_entry.keygen_poly_sel = 0;
		hm_entry.ip_sa_mask = 0;
		hm_entry.ip_da_mask = 0;

		if ( p_rslt->mask & IN_QOS_MASK_BIT_VID ){
			if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid==0)		/* WAN */
				hm_entry.vid_1_mask = 0;
			else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid==1)		/* LAN */
				hm_entry.vid_1_mask = 0;
			else
				hm_entry.vid_1_mask = 0;
		}
		if ( p_rslt->mask & IN_QOS_MASK_BIT_8021P){
			if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid==0)		/* WAN */
				hm_entry._8021p_1_mask = 0;
			else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid==1)		/* LAN */
				hm_entry._8021p_1_mask = 0;
			else
				hm_entry._8021p_2_mask = 0;

		}
		if ( p_rslt->mask & IN_QOS_MASK_BIT_DSCP)
			hm_entry.dscp_mask = 0;

		if ( p_rslt->mask & IN_QOS_MASK_BIT_DIP)
			hm_entry.ip_da_mask = 0x20;
		if ( p_rslt->mask & IN_QOS_MASK_BIT_SIP)
			hm_entry.ip_sa_mask = 0x20;

		hm_entry.lspid_mask = 0;
		cs_fe_table_add_entry(FE_TABLE_HASH_MASK, &hm_entry, &mask_idx_lan);
		DBG(printk("Mask RAM IDX:%d\n", mask_idx_lan));
	}

	/* Create voqpol */
	voqpol_entry->voq_base = p_rslt->voq;
	voqpol_lan[qos_enabled_lan&0xFF] = p_rslt->voq;
	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, voqpol_entry,
				&voqpol_idx);
	voqpol_idxs_lan[qos_enabled_lan&0xFF] = voqpol_idx;

	DBG(printk("VOQPOL IDX:%d\n",voqpol_idx));

	/* Create result */
	action.dest.voq_pol_table_index = voqpol_idx;
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &action,
				&rslt_idx);
	result_lan[qos_enabled_lan&0xFF] = rslt_idx;
	DBG(printk("Rslt IDX:%d\n",rslt_idx));

	/* Create hash */
	if ( p_rslt->mask & IN_QOS_MASK_BIT_VID ){
		if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
			p_rslt->lspid==0)		/* WAN */
			key.vid_1 = (p_rslt->vid);
		else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
			p_rslt->lspid==1)		/* LAN */
			key.vid_1 = (p_rslt->vid);
		else
			key.vid_1 = (p_rslt->vid);
	}
	if ( p_rslt->mask & IN_QOS_MASK_BIT_8021P){
		if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
			p_rslt->lspid==0)		/* WAN */
			key._8021p_1 = p_rslt->_8021p;
		else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
			p_rslt->lspid==1)		/* LAN */
			key._8021p_1 = p_rslt->_8021p;
		else
			key._8021p_2 = p_rslt->_8021p;
	}
	if ( p_rslt->mask & IN_QOS_MASK_BIT_DSCP)
		key.dscp = p_rslt->dscp;
	key.mask_ptr_0_7 = mask_idx_lan;
	key.lspid = p_rslt->lspid;

	if ( p_rslt->mask & IN_QOS_MASK_BIT_DIP)
		key.da[0] = (p_rslt->dip[0] << 24) | (p_rslt->dip[1] << 16)| \
				(p_rslt->dip[2] << 8) |(p_rslt->dip[3]);
	if ( p_rslt->mask & IN_QOS_MASK_BIT_SIP)
        	key.sa[0] = (p_rslt->sip[0] << 24) | (p_rslt->sip[1] << 16)| \
				(p_rslt->sip[2] << 8) |(p_rslt->sip[3]);

	cs_fe_hash_calc_crc(&key, &crc32,
				&crc16, CRC16_CCITT);
	DBG(printk("CRC32:%x   CRC16:%x\n",crc32, crc16));
	ret = cs_fe_hash_add_hash(crc32,
				crc16, mask_idx_lan, rslt_idx,
				&hw_index);
	if (ret != 0)
		printk("Add hash fail\n");

	hw_index_lan[qos_enabled_lan & 0xFF] = (unsigned int)hw_index;
//	cs_core_hmu_add_hash(hw_index, phash_entry->lifetime);

	DBG(printk("Class IDX:%d\n",p_vtable->class_index));
	DBG(printk("SDB IDX:%d\n",p_vtable->sdb_index));
	/* update tuple */
	cs_fe_table_get_entry(FE_TABLE_SDB, p_vtable->sdb_index, &sdb_entry);
	sdb_entry.sdb_tuple[TUPLE_QOS_LAN].enable = 1;
	sdb_entry.sdb_tuple[TUPLE_QOS_LAN].priority = 2;
	if ( alloc_mask_ram == 1)// Only assign mask pinter if allocate mask ram
		sdb_entry.sdb_tuple[TUPLE_QOS_LAN].mask_ptr = mask_idx_lan;
	cs_fe_table_set_entry(FE_TABLE_SDB, p_vtable->sdb_index, &sdb_entry);

	return 0;
}

int qos_reset_entry_chain(void)
{
	int i;

	/* LAN */
	qos_enabled_lan &= ~0x0100;
	for (i = 0; i < qos_enabled_lan; i++){

		/* del hash at first to avoid junk pkt */
		cs_fe_hash_del_hash(hw_index_lan[i]);

		/* Delete VOQPOL */
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voqpol_idxs_lan[i], 0);

		/* Delete Result table */
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				result_lan[i], 0);

		cs752x_sch_reset_queue(voqpol_entries_lan[i].voq_base/SCH_MAX_PORT_QUEUE,
					voqpol_entries_lan[i].voq_base%SCH_MAX_PORT_QUEUE);
	}

	/* Delete Mask RAM table */
	cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK, mask_idx_lan, 1);

	qos_enabled_lan = 0;
	mask_idx_lan = 0;
	memset(voqpol_idxs_lan, 0, sizeof(int)*MAX_IN_QOS_VOQ_LAN);
	memset(voqpol_lan, -1, sizeof(int)*MAX_IN_QOS_VOQ_LAN);

	/* WAN */
	qos_enabled_wan &= ~0x0100;
	for (i = 0; i < qos_enabled_wan; i++){

		/* Delete VOQPOL */
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
						voqpol_idxs_wan[i], 0);

		/* Delete Result table */
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				result_wan[i], 0);

		cs_fe_hash_del_hash(hw_index_wan[i]);
		cs752x_sch_reset_queue(voqpol_entries_wan[i].voq_base/SCH_MAX_PORT_QUEUE,
					voqpol_entries_wan[i].voq_base%SCH_MAX_PORT_QUEUE);
	}

	/* Delete Mask RAM table */
	cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK, mask_idx_wan, 1);

	qos_enabled_wan = 0;
	mask_idx_wan = 0;
	memset(voqpol_idxs_wan, 0, sizeof(int)*MAX_IN_QOS_VOQ_WAN);
	memset(voqpol_wan, -1, sizeof(int)*MAX_IN_QOS_VOQ_WAN);

	return 0;
}

int qos_add_chain_wan(fe_ingress_qos_table_entry_t *p_rslt, int alloc_mask_ram)
{
	int ret;
	unsigned int voqpol_idx, rslt_idx, crc32;
	u16 hw_index;
	unsigned short crc16;
	//unsigned char mask_ptr;
	fe_sw_hash_t key;
	fe_fwd_result_entry_t action;
	fe_hash_mask_entry_t hm_entry;
	fe_voq_pol_entry_t *voqpol_entry = &voqpol_entries_wan[qos_enabled_wan&0xff];
	fe_sdb_entry_t sdb_entry;
	cs_vtable_t *p_vtable = vtable_list[CORE_VTABLE_TYPE_L3_FLOW];
	cs_fwd_hash_t *phash_entry = &hash_entry_wan[qos_enabled_wan&0xFF];

	memset(&key, 0, sizeof(fe_sw_hash_t));
	memset(&action, 0, sizeof(fe_fwd_result_entry_t));
	memset(voqpol_entry, 0, sizeof(voqpol_entry));
	memset(phash_entry, 0x0, sizeof(cs_fwd_hash_t));

	phash_entry->lifetime = 0;

	/* Create mask ram */
	if ( alloc_mask_ram == 1){
		memset(&hm_entry, 0xff, sizeof(fe_hash_mask_entry_t));
		hm_entry.keygen_poly_sel = 0;
		hm_entry.ip_sa_mask = 0;
		hm_entry.ip_da_mask = 0;

		if ( p_rslt->mask & IN_QOS_MASK_BIT_VID ){
			if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid == 0)		/* WAN */
				hm_entry.vid_1_mask = 0;
			else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				(p_rslt->lspid == 1))		/* LAN */
				hm_entry.vid_2_mask = 0;
			else
				hm_entry.vid_1_mask = 0;
		}
		if ( p_rslt->mask & IN_QOS_MASK_BIT_8021P){
			if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				(p_rslt->lspid == 0))		/* WAN */
				hm_entry._8021p_1_mask = 0;
			else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				(p_rslt->lspid == 1))		/* LAN */
				hm_entry._8021p_2_mask = 0;
			else
				hm_entry._8021p_2_mask = 0;
		}
		if ( p_rslt->mask & IN_QOS_MASK_BIT_DSCP)
			hm_entry.dscp_mask = 0;

		if ( p_rslt->mask & IN_QOS_MASK_BIT_DIP)
			hm_entry.ip_da_mask = 0x20;
		if ( p_rslt->mask & IN_QOS_MASK_BIT_SIP)
			hm_entry.ip_sa_mask = 0x20;

		hm_entry.lspid_mask = 0;
		cs_fe_table_add_entry(FE_TABLE_HASH_MASK, &hm_entry, &mask_idx_wan);
		DBG(printk("Mask RAM IDX:%d\n", mask_idx_wan));
	}

	/* Create voqpol */
	voqpol_entry->voq_base = p_rslt->voq;
	voqpol_wan[qos_enabled_wan&0xFF] = p_rslt->voq;
	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, voqpol_entry,
				&voqpol_idx);
	voqpol_idxs_wan[qos_enabled_wan&0xFF] = voqpol_idx;

	DBG(printk("VOQPOL IDX:%d\n",voqpol_idx));

	/* Create result */
	action.dest.voq_pol_table_index = voqpol_idx;
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, &action,
				&rslt_idx);
	result_wan[qos_enabled_wan&0xFF] = rslt_idx;
	DBG(printk("Rslt IDX:%d\n",rslt_idx));

	/* Create hash */
	if ( p_rslt->mask & IN_QOS_MASK_BIT_VID ){
		if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
			p_rslt->lspid==0)		/* WAN */
			key.vid_1 = (p_rslt->vid);
		else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
			p_rslt->lspid==1)		/* LAN */
			key.vid_2 = (p_rslt->vid);
		else
			key.vid_1 = (p_rslt->vid);
	}
	if ( p_rslt->mask & IN_QOS_MASK_BIT_8021P){
			if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid==0)		/* WAN */
				key._8021p_1 = p_rslt->_8021p;
			else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
				p_rslt->lspid==1)		/* LAN */
				key._8021p_2 = p_rslt->_8021p;
			else
				key._8021p_2 = p_rslt->_8021p;
	}
	if ( p_rslt->mask & IN_QOS_MASK_BIT_DSCP)
		key.dscp = p_rslt->dscp;
	key.mask_ptr_0_7 = mask_idx_wan;
	key.lspid = p_rslt->lspid;

	if ( p_rslt->mask & IN_QOS_MASK_BIT_DIP)
		key.da[0] = (p_rslt->dip[0] << 24) | (p_rslt->dip[1] << 16)| \
				(p_rslt->dip[2] << 8) |(p_rslt->dip[3]);
	if ( p_rslt->mask & IN_QOS_MASK_BIT_SIP)
        	key.sa[0] = (p_rslt->sip[0] << 24) | (p_rslt->sip[1] << 16)| \
				(p_rslt->sip[2] << 8) |(p_rslt->sip[3]);

	cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	DBG(printk("CRC32:%x   CRC16:%x\n",crc32, crc16));
	ret = cs_fe_hash_add_hash(crc32, crc16, mask_idx_wan, rslt_idx,
				&hw_index);
	if (ret != 0)
		printk("Add hash fail\n");
	hw_index_wan[qos_enabled_wan & 0xFF] = (unsigned int)hw_index;
//	cs_core_hmu_add_hash(hw_index, phash_entry->lifetime);

	DBG(printk("Class IDX:%d\n",p_vtable->class_index));
	DBG(printk("SDB IDX:%d\n",p_vtable->sdb_index));
	/* update tuple */
	cs_fe_table_get_entry(FE_TABLE_SDB, p_vtable->sdb_index, &sdb_entry);
	sdb_entry.sdb_tuple[TUPLE_QOS_WAN].enable = 1;
	sdb_entry.sdb_tuple[TUPLE_QOS_WAN].priority = 3;
	if ( alloc_mask_ram == 1)// Only assign mask pinter if allocate mask ram
		sdb_entry.sdb_tuple[TUPLE_QOS_WAN].mask_ptr = mask_idx_wan;
	cs_fe_table_set_entry(FE_TABLE_SDB, p_vtable->sdb_index, &sdb_entry);

	return 0;
}

int qos_del_entry(fe_ingress_qos_table_entry_t *p_rslt)
{
	int i;

	//if ((p_rslt->mask & 0x10) == 0)	/* No VID, delete LAN QoS setting */
	if ((p_rslt->mask & IN_QOS_MASK_BIT_LSPID) &&
		p_rslt->lspid == 1)
		goto Delete_LAN;


	for(i = 0; i < (qos_enabled_wan&0xff); i++){
		if (p_rslt->voq != voqpol_wan[i])
			continue;

		/* del hash at first to avoid junk pkt */
		cs_fe_hash_del_hash(hw_index_lan[i]);

		voqpol_wan[i] = -1;
		/* Delete VOQPOL */
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
					voqpol_idxs_wan[i], 0);
		/* Delete Result table */
		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
				result_lan[i], 0);


		/*  We can't reset voq attribution here!! SP & DRR can't interleave.
		 *  And reset will let VOQ become DRR. Just leave it as SP. packet will not
	 	*  go through this queue since hash has been removed.
	 	*/
		//cs752x_sch_reset_queue(voqpol_entries_wan[i].voq_base/SCH_MAX_PORT_QUEUE,
		//				voqpol_entries_wan[i].voq_base%SCH_MAX_PORT_QUEUE);

//		memset(&p_vhash_fwd_entries_wan[i], 0x0, sizeof(cs_vtable_hash_entry_t));
		qos_enabled_wan--;

	}

	if ((qos_enabled_wan & 0xff) == 0){
		cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK, mask_idx_wan, 1);
		qos_enabled_wan = 0;
		mask_idx_wan = 0;
		memset(voqpol_idxs_wan, 0, sizeof(int)*MAX_IN_QOS_VOQ_WAN);
	}

	return 0;

Delete_LAN:
	for (i = 0; i < (qos_enabled_lan & 0xff); i++) {
		if ( p_rslt->voq != voqpol_lan[i])
			continue;

		voqpol_lan[i] = -1;
		/* Delete VOQPOL */
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
					voqpol_idxs_lan[i], 0);
		/* Delete Result table */
//		cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT,
//				p_vhash_fwd_entries_lan[i].result_index, 0);

//		cs_fe_hash_del_hash(p_vhash_fwd_entries_lan[i].hw_index);

		/*  We can't reset voq attribution here!! SP & DRR can't interleaving.
		 *  And reset will let VOQ become DRR. Just leave it as SP. packet will not
	 	*  go through this queue since hash has been removed.
	 	*/
		//cs752x_sch_reset_queue(voqpol_entries_lan[i].voq_base/SCH_MAX_PORT_QUEUE,
		//				voqpol_entries_lan[i].voq_base%SCH_MAX_PORT_QUEUE);

//		memset(&p_vhash_fwd_entries_lan[i], 0x0, sizeof(cs_vtable_hash_entry_t));
		qos_enabled_lan--;

	}

	if ((qos_enabled_lan & 0xff) == 0) {
		cs_fe_table_del_entry_by_idx(FE_TABLE_HASH_MASK, mask_idx_lan, 1);
		qos_enabled_lan = 0;
		mask_idx_lan = 0;
		memset(voqpol_idxs_lan, 0, sizeof(int)*MAX_IN_QOS_VOQ_LAN);
	}

	return 0;
}

int cs_ne_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	int i, err = 0;
	NECMD_HDR_T ne_hdr;
	NEFE_CMD_HDR_T fe_cmd_hdr;
	NE_REQ_E ctrl;
	u8 *req_datap;
	u8 tbl_entry[FE_MAX_ENTRY_SIZE];
	u32 phy_data_s = -1,location_r = -1,length_r = -1,location_w = -1,data_w = -1;
	u16 phy_addr = -1,phy_reg = -1,phy_len = -1, phy_addr_s = 0, phy_reg_s = 0;
	u32 size_r,size_w;
	//etan:fix me
	fe_ingress_qos_table_entry_t *p_rslt;
	fe_ingress_qos_shaper_voq_entry_t *p1_rslt;
	fe_ingress_qos_shaper_port_entry_t *p2_rslt;
	
#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
	/* ne_cfg command for IPLIP TUNNEL control plane */
	cs_tunnel_iplip_api_entry_t 	*tunnel_iplip_entry;
	cs_dev_id_t		device_id;
	cs_port_id_t		port_id;
	cs_port_id_t		pppoe_port_id;
	cs_tunnel_id_t		tunnel_id;
	cs_session_id_t		session_id;
	cs_pppoe_port_cfg_t	*p_cfg;
	cs_pppoe_port_cfg_t	pppoe_cfg;
	cs_tunnel_cfg_t		*p_tunnel_cfg;
	cs_tunnel_cfg_t		tunnel_cfg;
	cs_ip_address_t		*p_ipv6_prefix;
	cs_ip_address_t		ipv6_prefix;
	unsigned char		is_present;	
#endif

	if (copy_from_user((void *)&ne_hdr, rq->ifr_data, sizeof(ne_hdr)))
		return -EFAULT;

	req_datap = (u8 *)rq->ifr_data + sizeof(ne_hdr);

	switch (ne_hdr.cmd) {
	case REGREAD:
		if (ne_hdr.len != sizeof(REGREAD_T))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.reg_read, req_datap, sizeof(ctrl.reg_read)))
			return -EFAULT;
		location_r = ctrl.reg_read.location;
		length_r = ctrl.reg_read.length;
		size_r = ctrl.reg_read.size;

		//if (size_r == 1 && ((MIN_READ <= location_r) && (location_r <= MAX_READ)))
		if (size_r == 1)
			size_r = 4;
		if (size_r == 1)
			ni_dm_byte(location_r, length_r);
		if (size_r == 2)
			ni_dm_short(location_r, length_r);
		if (size_r == 4)
			ni_dm_long(location_r, length_r);
		break;
	case REGWRITE:
		if (ne_hdr.len != sizeof(REGWRITE_T))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.reg_write, req_datap, sizeof(ctrl.reg_write)))
			return -EFAULT;
		location_w = ctrl.reg_write.location;
		data_w = ctrl.reg_write.data;
		size_w = ctrl.reg_write.size;
		if (size_w == 1) {
			if (data_w > 0xff)
				err = 1;
			} else {
				writeb(data_w,location_w);
				printk("Write Data 0x%X to Location 0x%X\n",(u32)data_w, location_w);
			}
		if (size_w == 2) {
			if (data_w > 0xffff)
				err = 1;
			} else {
				writew(data_w, location_w);
				printk("Write Data 0x%X to Location 0x%X\n",(u32)data_w, location_w);
			}
		if (size_w == 4) {
			if (data_w > 0xffffffff)
				err = 1;
			} else {
				writel(data_w, location_w);
				printk("Write Data 0x%X to Location 0x%X\n",(u32)data_w, location_w);
			}
		if (err == 1) {
			printk("Syntax:	ne write mem [-b <location>] [-d <data>] [-1|2|4]\n");
			printk("Options:\n");
			printk("\t-b  Register Address\n");
			printk("\t-d  Data Vaule\n");
			if (size_w == 1)
				printk("\t-1  Data 0x%X < 0xFF\n",data_w);
			if (size_w == 2)
				printk("\t-2  Data 0x%X < 0xFFFF\n",data_w);
			if (size_w == 4)
				printk("\t-4  Data 0x%X < 0xFFFFFFFF\n",data_w);
		}
		break;
	case GMIIREG:
		if (ne_hdr.len != sizeof(GMIIREG_T))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.get_mii_reg, req_datap, sizeof(ctrl.get_mii_reg)))
			return -EFAULT; /* Invalid argument */
		phy_addr = ctrl.get_mii_reg.phy_addr;
		phy_reg = ctrl.get_mii_reg.phy_reg;
		phy_len = ctrl.get_mii_reg.phy_len;
		if (phy_addr < 32) {
			for (i = 0; i < phy_len ; i++)	{
				unsigned int data;
				data = ni_mdio_read((int)phy_addr, (int)phy_reg);
				printk("MII Phy %d Reg %d Data = 0x%x\n", phy_addr, phy_reg++, data);
			}
		} else {
			err = 1;
		}

		if (err == 1) {
			printk("Syntax error!\n");
			printk("Syntax: MII read [-a phy addr] [-r phy reg] [-l length]\n");
			printk("Options:\n");
			printk("\t-a  Phy address\n");
			printk("\t-r  Phy registers\n");
			printk("\t-l  Display total registers\n");
			printk("MII Phy address -a %d error !! Phy address must be smaller than 32.\n", phy_addr);
		}
		break;
	case SMIIREG:
		if (ne_hdr.len != sizeof(SMIIREG_T))
			return -EPERM;
		if (copy_from_user((void *)&ctrl.set_mii_reg, req_datap, sizeof(ctrl.set_mii_reg)))
			return -EFAULT;

		phy_addr_s = ctrl.set_mii_reg.phy_addr;
		phy_reg_s = ctrl.set_mii_reg.phy_reg;
		phy_data_s = ctrl.set_mii_reg.phy_data;
		if (phy_addr_s < 32)	{
				ni_mdio_write((int)phy_addr_s, (int)phy_reg_s, (int)phy_data_s);
				printk("Write MII Phy %d Reg %d Data = 0x%x\n", phy_addr_s, phy_reg_s, phy_data_s);
		} else {
			err = 1;
		}
		if (err == 1) {
			printk("Syntax error!\n");
			printk("Syntax: MII write [-a phy addr] [-r phy reg] [-d data]\n");
			printk("Options:\n");
			printk("\t-a  Phy address\n");
			printk("\t-r  Phy registers\n");
			printk("\t-d  date\n");
			printk("MII Phy address -a %d error !! Phy address must be smaller than 32.\n", phy_addr_s);
		}
		break;
	/* ne_cfg command for IPLIP TUNNEL control plane */
	case NE_TUNNEL_IOCTL:
		if (copy_from_user((void *)&fe_cmd_hdr, req_datap, sizeof(NEFE_CMD_HDR_T)))
                        return -EFAULT;
		req_datap += sizeof(NEFE_CMD_HDR_T);
		switch (fe_cmd_hdr.table_id) {

#ifdef CONFIG_CS75XX_HW_ACCEL_IPLIP
		case CS_IOCTL_TBL_TUNNEL_IPLIP_API:
			if (copy_from_user((void *)&tbl_entry, req_datap,
                        	sizeof(cs_tunnel_iplip_api_entry_t)))
                        	return -EFAULT;
                        tunnel_iplip_entry = (cs_tunnel_iplip_api_entry_t *)tbl_entry;
			switch (tunnel_iplip_entry->sub_cmd) {
				case CS_IPLIP_PPPOE_PORT_ADD:
					/* cs_pppoe_port_add(); */
					device_id = tunnel_iplip_entry->pppoe_port_add_delete_param.device_id;
					port_id = tunnel_iplip_entry->pppoe_port_add_delete_param.port_id;
					pppoe_port_id = tunnel_iplip_entry->pppoe_port_add_delete_param.pppoe_port_id;
					cs_pppoe_port_add(device_id, port_id, pppoe_port_id);
					break;
				case CS_IPLIP_PPPOE_PORT_DELETE:
					/* cs_pppoe_port_delete(); */
					device_id = tunnel_iplip_entry->pppoe_port_add_delete_param.device_id;
					pppoe_port_id = tunnel_iplip_entry->pppoe_port_add_delete_param.pppoe_port_id;
					cs_pppoe_port_delete(device_id, pppoe_port_id);
					break;
				case CS_IPLIP_PPPOE_PORT_ENCAP_SET:
					/* cs_pppoe_port_encap_set(); */
					device_id = tunnel_iplip_entry->pppoe_port_encap_param.device_id;
					pppoe_port_id = tunnel_iplip_entry->pppoe_port_encap_param.pppoe_port_id;
					p_cfg = &(tunnel_iplip_entry->pppoe_port_encap_param.pppoe_port_cfg);
					cs_pppoe_port_encap_set(device_id, pppoe_port_id, p_cfg);
					break;
				case CS_IPLIP_PPPOE_PORT_ENCAP_GET:
					/* cs_pppoe_port_config_get(); */
					device_id = tunnel_iplip_entry->pppoe_port_encap_param.device_id;
					pppoe_port_id = tunnel_iplip_entry->pppoe_port_encap_param.pppoe_port_id;
					p_cfg = &pppoe_cfg;
					memset(p_cfg, 0x0, sizeof(cs_pppoe_port_cfg_t));
					cs_pppoe_port_config_get(device_id, pppoe_port_id, p_cfg);
					/* TODO: 1.format the printk 2. cs_status=cs_pppoe_port_config_get()*/
					printk("cs_pppoe_port_config_get() device id=%d\n", device_id);
					printk("pppoe_port_id = 0x%x\n", pppoe_port_id);
					printk("p_cfg->src_mac: \n");
					printk("%02x:%02x:", p_cfg->src_mac[0], p_cfg->src_mac[1]);
					printk("%02x:%02x:", p_cfg->src_mac[2], p_cfg->src_mac[3]);
					printk("%02x:%02x\n", p_cfg->src_mac[4], p_cfg->src_mac[5]);
					printk("p_cfg->dest_mac: \n");
					printk("%02x:%02x:", p_cfg->dest_mac[0], p_cfg->dest_mac[1]);
					printk("%02x:%02x:", p_cfg->dest_mac[2], p_cfg->dest_mac[3]);
					printk("%02x:%02x\n", p_cfg->dest_mac[4], p_cfg->dest_mac[5]);					
					printk("pppoe_session_id = 0x%x\n", p_cfg->pppoe_session_id);
					printk("tx_phy_port = %d\n", p_cfg->tx_phy_port);
					printk("vlan_tag = %d\n", p_cfg->vlan_tag);
					break;
				case CS_IPLIP_TUNNEL_ADD:
					/* cs_tunnel_add(); */
					device_id = tunnel_iplip_entry->tunnel_param.device_id;
					p_tunnel_cfg = &(tunnel_iplip_entry->tunnel_param.tunnel_cfg);
					cs_tunnel_add(device_id, p_tunnel_cfg, &tunnel_id);
					printk("cs_tunnel_add() src_addr.ipv4_addr=0x%u\n", p_tunnel_cfg->src_addr.ip_addr.ipv4_addr);
					printk("cs_tunnel_add() tunnel_id=0x%x\n", tunnel_id);
					break;
				case CS_IPLIP_TUNNEL_DELETE:
					/* cs_tunnel_delete(); */
					device_id = tunnel_iplip_entry->tunnel_param.device_id;
					p_tunnel_cfg = &(tunnel_iplip_entry->tunnel_param.tunnel_cfg);
					cs_tunnel_delete(device_id, p_tunnel_cfg);
					break;
				case CS_IPLIP_TUNNEL_DELETE_BY_IDX:
					/* cs_tunnel_delete_by_idx(); */
					device_id = tunnel_iplip_entry->tunnel_param.device_id;
					tunnel_id = tunnel_iplip_entry->tunnel_param.tunnel_id;
					cs_tunnel_delete_by_idx(device_id, tunnel_id);
					break;
				case CS_IPLIP_TUNNEL_GET:
					/* cs_tunnel_get(); */
					device_id = tunnel_iplip_entry->tunnel_param.device_id;
					tunnel_id = tunnel_iplip_entry->tunnel_param.tunnel_id;
					p_tunnel_cfg = &tunnel_cfg;
					memset(p_tunnel_cfg, 0x0, sizeof(cs_tunnel_cfg_t));
					cs_tunnel_get(device_id, tunnel_id, p_tunnel_cfg);
					/*TODO: 1.format the printk 2.cs_status=cs_tunnel_get()*/
					printk("cs_tunnel_get() device id=%d\n", device_id);
					printk("tunnel_id = 0x%x\n", tunnel_id);
					printk("tunnel_type = 0x%x\n", p_tunnel_cfg->type);
					printk("tx_port = 0x%x\n", p_tunnel_cfg->tx_port);
					//p_tunnel_cfg->src_addr
					printk("p_tunnel_cfg->src_addr info:\n afi  addr_len\n");
					printk("0x%x 0x%x\n", p_tunnel_cfg->src_addr.afi, p_tunnel_cfg->src_addr.addr_len);
					/*FIXME: should be a print function*/
					if(p_tunnel_cfg->src_addr.afi == CS_IPV4){
						printk("IPV4: ");
						printk("0x%x\n", p_tunnel_cfg->src_addr.ip_addr.ipv4_addr);
					}else{ /* CS_IPV6 */
						printk("IPV6: ");
						printk("%x:%x:", p_tunnel_cfg->src_addr.ip_addr.ipv6_addr[0], p_tunnel_cfg->src_addr.ip_addr.ipv6_addr[1]);
						printk("%x:%x", p_tunnel_cfg->src_addr.ip_addr.ipv6_addr[2], p_tunnel_cfg->src_addr.ip_addr.ipv6_addr[3]);
					}
					//p_tunnel_cfg->dest_addr
					printk("p_tunnel_cfg->dest_addr info:\n afi  addr_len\n");
					printk("0x%x 0x%x\n", p_tunnel_cfg->dest_addr.afi, p_tunnel_cfg->dest_addr.addr_len);
					if(p_tunnel_cfg->dest_addr.afi == CS_IPV4){
						printk("IPV4: ");
						printk("0x%x\n", p_tunnel_cfg->dest_addr.ip_addr.ipv4_addr);
					}else{ /* CS_IPV6 */
						printk("IPV6: ");
						printk("%x:%x:", p_tunnel_cfg->dest_addr.ip_addr.ipv6_addr[0], p_tunnel_cfg->dest_addr.ip_addr.ipv6_addr[1]);
						printk("%x:%x\n", p_tunnel_cfg->dest_addr.ip_addr.ipv6_addr[2], p_tunnel_cfg->dest_addr.ip_addr.ipv6_addr[3]);
					}
					/* p_tunnel_cfg->l2tp */
					printk("p_tunnel_cfg->tunnel.l2tp info:\n Version and friends, Optional Length\n");
					printk("0x%x 0x%x\n", p_tunnel_cfg->tunnel.l2tp.ver, p_tunnel_cfg->tunnel.l2tp.len);
					printk("Tunnel ID,   ID of IPv4 hdr\n");
					printk("0x%x 0x%x\n", p_tunnel_cfg->tunnel.l2tp.tid, p_tunnel_cfg->tunnel.l2tp.ipv4_id);
					printk("dest_port,   src_port (UDP port of L2TP)\n");
					printk("0x%x 0x%x\n", p_tunnel_cfg->tunnel.l2tp.dest_port, p_tunnel_cfg->tunnel.l2tp.src_port);
					break;
				case CS_IPLIP_L2TP_SESSION_ADD:
					/* cs_l2tp_session_add(); */
					device_id = tunnel_iplip_entry->l2tp_session_param.device_id;
					tunnel_id = tunnel_iplip_entry->l2tp_session_param.tunnel_id;
					session_id = tunnel_iplip_entry->l2tp_session_param.session_id;
					cs_l2tp_session_add(device_id, tunnel_id, session_id);
					break;
				case CS_IPLIP_L2TP_SESSION_DELETE:
					/* cs_l2tp_session_delete(); */
					device_id = tunnel_iplip_entry->l2tp_session_param.device_id;
					tunnel_id = tunnel_iplip_entry->l2tp_session_param.tunnel_id;
					session_id = tunnel_iplip_entry->l2tp_session_param.session_id;
					cs_l2tp_session_delete(device_id, tunnel_id, session_id);
					break;
				case CS_IPLIP_L2TP_SESSION_GET:
					/* cs_l2tp_session_get(); */
					is_present = 0;
					device_id = tunnel_iplip_entry->l2tp_session_param.device_id;
					tunnel_id = tunnel_iplip_entry->l2tp_session_param.tunnel_id;
					session_id = tunnel_iplip_entry->l2tp_session_param.session_id;
					cs_l2tp_session_get(device_id, tunnel_id, session_id, &is_present);
					/*TODO: cs_status=cs_l2tp_session_get()*/
					printk("cs_l2tp_session_get() is_present=%d\n", is_present);
					break;
				case CS_IPLIP_IPV6_OVER_L2TP_ADD:
					/* cs_ipv6_over_l2tp_add(); */
					device_id = tunnel_iplip_entry->ipv6_over_l2tp_param.device_id;
					tunnel_id = tunnel_iplip_entry->ipv6_over_l2tp_param.tunnel_id;
					session_id = tunnel_iplip_entry->ipv6_over_l2tp_param.session_id;
					p_ipv6_prefix = &(tunnel_iplip_entry->ipv6_over_l2tp_param.ipv6_prefix);
					ni_dm_byte((unsigned int)p_ipv6_prefix->ip_addr.ipv6_addr, 16);
					cs_ipv6_over_l2tp_add(device_id, tunnel_id, session_id, p_ipv6_prefix);
					break;
				case CS_IPLIP_IPV6_OVER_L2TP_DELETE:
					/* cs_ipv6_over_l2tp_delete(); */
					device_id = tunnel_iplip_entry->ipv6_over_l2tp_param.device_id;
					tunnel_id = tunnel_iplip_entry->ipv6_over_l2tp_param.tunnel_id;
					session_id = tunnel_iplip_entry->ipv6_over_l2tp_param.session_id;
					p_ipv6_prefix = &(tunnel_iplip_entry->ipv6_over_l2tp_param.ipv6_prefix);
					cs_ipv6_over_l2tp_delete(device_id, tunnel_id, session_id, p_ipv6_prefix);
					break;
				case CS_IPLIP_IPV6_OVER_L2TP_GETNEXT:
					/* cs_ipv6_over_l2tp_getnext(); */
					device_id = tunnel_iplip_entry->ipv6_over_l2tp_param.device_id;
					tunnel_id = tunnel_iplip_entry->ipv6_over_l2tp_param.tunnel_id;
					session_id = tunnel_iplip_entry->ipv6_over_l2tp_param.session_id;
					p_ipv6_prefix = &(tunnel_iplip_entry->ipv6_over_l2tp_param.ipv6_prefix);
					if(p_ipv6_prefix->ip_addr.ipv6_addr[0]==0 &&
						p_ipv6_prefix->ip_addr.ipv6_addr[1]==0 &&
						p_ipv6_prefix->ip_addr.ipv6_addr[2]==0 &&
						p_ipv6_prefix->ip_addr.ipv6_addr[3]==0 ){ /* First time: all 0 */
						memset(&ipv6_prefix, 0x0, sizeof(cs_ip_address_t));
						p_ipv6_prefix = &ipv6_prefix;
					}
					cs_ipv6_over_l2tp_getnext(device_id, tunnel_id, session_id, p_ipv6_prefix);
					/*TODO: format the printk cs_status=cs_ipv6_over_l2tp_getnext()*/
					/*FIXME: should be a print function*/
					printk("ipv6_prefix info:\n afi  addr_len\n");
					printk("0x%x 0x%x\n", p_ipv6_prefix->afi, p_ipv6_prefix->addr_len);
					if(p_ipv6_prefix->afi == CS_IPV4){
						printk("IPV4: ");
						printk("0x%x\n", p_ipv6_prefix->ip_addr.ipv4_addr);
					}else{ /* CS_IPV6 */
						printk("IPV6: ");
						printk("%x:%x:", p_ipv6_prefix->ip_addr.ipv6_addr[0], p_ipv6_prefix->ip_addr.ipv6_addr[1]);
						printk("%x:%x\n", p_ipv6_prefix->ip_addr.ipv6_addr[2], p_ipv6_prefix->ip_addr.ipv6_addr[3]);
					}
					break;
				default:
                        		break;
			}
			break;
#endif /* CONFIG_CS75XX_HW_ACCEL_IPLIP */
		default:
			break;
		}
		break;
	case NE_NI_IOCTL:
		printk("NE_NI_IOCTL\n");
		break;
#ifndef CONFIG_CS75XX_OFFSET_BASED_QOS
	 /* [begin][ingress qos]--add by ethan for ingress qos */
	case NE_INGRESS_QOS_IOCTL:
		if (copy_from_user((void *)&fe_cmd_hdr, req_datap, sizeof(NEFE_CMD_HDR_T)))
			return -EFAULT;
		//[no need]
		//ni_set_ne_enabled((fe_cmd_hdr.Bypass==1)?0:1);
		req_datap += sizeof(NEFE_CMD_HDR_T);
		switch(fe_cmd_hdr.table_id){
		case CS_IOCTL_TBL_INGRESS_QOS_TABLE:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_ingress_qos_table_entry_t)))
				return -EFAULT;
			//[to be implemented]
			//cs_inqos_ioctl_qos_table(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			/*debug message*/
			p_rslt = (fe_ingress_qos_table_entry_t *)tbl_entry;
			printk("\n\n==========================qos table===========================================\n");
			printk("mask[6:delete][5:lspid][4:vid][3:8021p][2:dscp][1:sip][0:dip]---------%d\n",p_rslt->mask);
			printk("vid------------------------------------------------%d\n",p_rslt->vid);
			printk("8021p----------------------------------------------%d\n",p_rslt->_8021p);
			printk("dscp-----------------------------------------------%d\n",p_rslt->dscp);
			printk("sip------------------------------------------------%d.%d.%d.%d\n",p_rslt->sip[0],p_rslt->sip[1],p_rslt->sip[2],p_rslt->sip[3]);
			printk("dip------------------------------------------------%d.%d.%d.%d\n",p_rslt->dip[0],p_rslt->dip[1],p_rslt->dip[2],p_rslt->dip[3]);
			printk("voq------------------------------------------------%d\n",p_rslt->voq);
			printk("lspid----------------------------------------------%d\n",p_rslt->lspid);
			printk("delete---------------------------------------------%d\n",p_rslt->del);
			printk("==============================================================================\n\n");

			if (fe_cmd_hdr.cmd == CMD_INGRESS_QOS_RESET){
				qos_reset_entry_chain();
			}
			else if(p_rslt->mask & IN_QOS_MASK_BIT_DEL){
				qos_del_entry(p_rslt);		/* Delete entry */
			}
			else{
				if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
					(p_rslt->lspid==1) && 		/* LAN */
					((qos_enabled_lan & 0xFF) >= MAX_IN_QOS_VOQ_LAN)){
					printk("Over Max QoS Hash(Lan-5) !!\n");
					return -EPERM;
				}
				else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
					(p_rslt->lspid==0) &&		/* WAN */
					(qos_enabled_wan & 0xFF) >= MAX_IN_QOS_VOQ_WAN){
					printk("Over Max QoS Hash(Wan-2) !!\n");
					return -EPERM;
				}

				if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
					p_rslt->lspid==1){		/* LAN */
					if (qos_enabled_lan == 0)
						qos_add_chain_lan(p_rslt, 1);
					else
						qos_add_chain_lan(p_rslt, 0);
					qos_enabled_lan++ ;
					qos_enabled_lan |= 0x100;
				}
				else if((p_rslt->mask&IN_QOS_MASK_BIT_LSPID)&&
					p_rslt->lspid==0){		/* WAN */
					if (qos_enabled_wan == 0)
						qos_add_chain_wan(p_rslt, 1);
					else
						qos_add_chain_wan(p_rslt, 0);
					qos_enabled_wan++ ;
					qos_enabled_wan |= 0x100;
				}
			}
			break;
		case CS_IOCTL_TBL_INGRESS_QOS_SHAPER_VOQ_TABLE:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_ingress_qos_shaper_voq_entry_t)))
				return -EFAULT;
			//[to be implemented]
			//cs_inqos_ioctl_shaper_voq_table(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			//********priority field please refer to QOS_PRIORITY_DEF in cs_ne_ioctl.h
			//debug message
			p1_rslt = (fe_ingress_qos_shaper_voq_entry_t *)tbl_entry;
			printk("\n\n==========================shaper_voq table=====================================\n");
			printk("voq_id--------------------------------------------------------%d\n",p1_rslt->voq_id);
			printk("priority--[0:SP] [1:DRR] [255:user didn't select this field]--%d\n",p1_rslt->sp_drr);
			printk("rate [0:user didn't select this field]------------------------%d\n",p1_rslt->rate);
			printk("===============================================================================\n\n");

			if (fe_cmd_hdr.cmd == CMD_INGRESS_QOS_RESET){
				cs752x_sch_reset_queue(p1_rslt->voq_id/SCH_MAX_PORT_QUEUE,
					p1_rslt->voq_id%SCH_MAX_PORT_QUEUE);
					return 0;
			}

			cs752x_sch_set_queue_sp(p1_rslt->voq_id / SCH_MAX_PORT_QUEUE,
						p1_rslt->voq_id % SCH_MAX_PORT_QUEUE,
						p1_rslt->rate*1000*8);
			break;
		case CS_IOCTL_TBL_INGRESS_QOS_SHAPER_PORT_TABLE:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_ingress_qos_shaper_port_entry_t)))
				return -EFAULT;
			//[to be implemented]
			//cs_inqos_ioctl_shaper_port_table(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			//debug message
			p2_rslt = (fe_ingress_qos_shaper_port_entry_t *)tbl_entry;
			printk("\n\n==========================shaper_port table========================\n");
			printk("port_vid---------%d\n",p2_rslt->port_id);
			printk("st_rate----------%d\n",p2_rslt->st_rate);
			printk("lt_rate----------%d\n",p2_rslt->lt_rate);
			printk("burst_size-------%d\n",p2_rslt->burst_size);
			printk("===================================================================\n\n");

			if (fe_cmd_hdr.cmd == CMD_INGRESS_QOS_RESET){
				cs752x_sch_set_port_rate_lt(p2_rslt->port_id, 0);
				cs752x_sch_set_port_rate_st(p2_rslt->port_id, 0);
				cs752x_sch_set_port_burst_size(p2_rslt->port_id, 64);
				return 0;
			}
			cs752x_sch_set_port_rate_lt(p2_rslt->port_id, p2_rslt->lt_rate*1000*8);
			cs752x_sch_set_port_rate_st(p2_rslt->port_id, p2_rslt->st_rate*1000*8);
			cs752x_sch_set_port_burst_size(p2_rslt->port_id, p2_rslt->burst_size);
			break;
#ifdef CONFIG_CS752X_HW_ACCELERATION
		case CS_IOCTL_TBL_INGRESS_QOS_API:
			if (copy_from_user((void *)&tbl_entry, req_datap,
					sizeof(cs_qos_ingress_api_entry_t)))
				return -EFAULT;

			cs_qos_ingress_ioctl(dev, &tbl_entry, (void*)&fe_cmd_hdr);

			if (copy_to_user(req_datap, (void *)&tbl_entry,
					sizeof(cs_qos_ingress_api_entry_t)))
				return -EFAULT;
			break;
#endif
		/* END of Eric Wang */
		default :
			break;
		}
		break;
   /* [end][ingress qos]--add by ethan for ingress qos */
#endif //CS75XX_VOQ_REASSIGN
	case NE_VOQ_COUNTER_IOCTL:
		if (copy_from_user((void *)&fe_cmd_hdr, req_datap,
				sizeof(NEFE_CMD_HDR_T)))
			return -EFAULT;

		req_datap += sizeof(NEFE_CMD_HDR_T);

		switch (fe_cmd_hdr.table_id) {
		case CS_IOCTL_TBL_VOQ_COUNTER_API:
			if (copy_from_user((void *)&tbl_entry, req_datap,
					sizeof(cs_voq_counter_api_entry_t)))
				return -EFAULT;
			cs_diag_voq_cntr_ioctl(dev, &tbl_entry,
					(void *)&fe_cmd_hdr);

			if (copy_to_user(req_datap, (void *)&tbl_entry,
					sizeof(cs_voq_counter_api_entry_t)))
				return -EFAULT;
			break;
		default:
			break;
		}
		break;
	case NE_FE_IOCTL:
		if (copy_from_user((void *)&fe_cmd_hdr, req_datap, sizeof(NEFE_CMD_HDR_T)))
			return -EFAULT;

		req_datap += sizeof(NEFE_CMD_HDR_T);
		switch(fe_cmd_hdr.table_id){
		case CS_IOTCL_TBL_CLASSIFIER:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_class_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_class(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_SDB:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_sdb_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_sdb(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_HASH_MASK:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_hash_mask_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_hashmask(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_LPM:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_lpm_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_lpm(dev, (void*)&tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_HASH_MATCH:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_hash_hash_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_hashhash(dev, (void*)&tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_HASH_CHECK:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_hash_check_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_hashcheck(dev, (void*)&tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_FWDRSLT:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_fwd_result_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_fwdrslt(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_QOSRSLT:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_qos_result_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_qosrslt(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_L2_MAC:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_l2_addr_pair_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_l2mac(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_L3_IP:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_l3_addr_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_l3ip(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_VOQ_POLICER:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_voq_pol_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_voqpol(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_LPB:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_lpb_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_lpb(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_AN_BNG_MAC:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_an_bng_mac_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_an_bng_mac(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_PORT_RANGE:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_port_range_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_l4portrngs(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_VLAN:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_vlan_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_vlan(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_ACL_RULE:
		case CS_IOCTL_TBL_ACL_ACTION:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_acl_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_acl(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_PE_VOQ_DROP:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_pe_voq_drp_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_voqdrp(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_ETYPE:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_eth_type_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_etype(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_LLC_HDR:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_llc_hdr_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_llchdr(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_FVLAN:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_flow_vlan_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_fvlan(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;
		case CS_IOCTL_TBL_PKTLEN:
			if (copy_from_user((void *)&tbl_entry, req_datap,
						sizeof(fe_pktlen_rngs_entry_t)))
				return -EFAULT;
			cs_fe_ioctl_pktlen(dev, &tbl_entry, (void*)&fe_cmd_hdr);
			break;

		/* Some other tables that dont have ioctl definted in user:
		 * hashcheck and hashoverflow. */
		default :
			break;
		}
		break;
	case NE_QM_IOCTL:
		printk("NE_QM_IOCTL\n");
		break;
	case NE_SCH_IOCTL:
		printk("NE_SCH_IOCTL\n");
		break;
	default:
		return -EPERM;
	}

	return 0;
}

