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
 * cs_hw_accel_qos_ingress.c
 *
 * $Id$
 *
 * This file contains the implementation for CS QoS implementation
 * for ingress traffic.
 */
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 1)
# include <linux/export.h>
#endif

#include <mach/cs75xx_qos.h>
#include "cs_fe.h"
//#include <linux/cs_ne_ioctl.h>
//#include "cs_hw_accel_qos.h"
//#include "cs_hw_accel_qos_data.h"
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_core_hmu.h"
//#include "cs75xx_tm.h"

#define CS_OFFSET_BASED_QOS_PORT_SIZE       6   /* GE_PORT0/GE_PORT1/GE_PORT2/CPU_PORT/PE0_PORT/PE1_PORT */
#define CS_OFFSET_BASED_QOS_DSCP_SIZE       32
#define CS_OFFSET_BASED_QOS_DOT1P_SIZE      8
static u8 offset_based_qos_port_enabled_mask = 0;  /* Bit 0: GE_PORT0
                                                      Bit 1: GE_PORT1
                                                      Bit 2: GE_PORT2
                                                      Bit 3: CPU_PORT
                                                      Bit 4: PE0_PORT
                                                      Bit 5: PE1_PORT
                                                      Bit 6~7: n/a
                                                    */
static u8 offset_based_qos_port_bit_mask[CS_OFFSET_BASED_QOS_PORT_SIZE] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};

static struct rx_task_info
{
	u16 dscp_dot1p[CS_OFFSET_BASED_QOS_DSCP_SIZE][CS_OFFSET_BASED_QOS_DOT1P_SIZE];
	u16 dscp[CS_OFFSET_BASED_QOS_DSCP_SIZE];
	u16 dot1p[CS_OFFSET_BASED_QOS_DOT1P_SIZE];
} offset_based_qos_hash_table[CS_OFFSET_BASED_QOS_PORT_SIZE];

static unsigned int offset_based_qos_hm_idx = ~(0x0);
static unsigned int qosrslt_idx[8];



// Mapping Table
#define CS_QOS_DSCP_NO                  64
#define CS_QOS_DOT1P_NO                 8
#define CS_QOS_VOQP_NO                  8
extern cs_qos_mode_t cs_qos_current_mode[CS_QOS_INGRESS_PORT_MAX_];
extern cs_uint8_t cs_qos_dot1p_map_tbl[CS_QOS_INGRESS_PORT_MAX_][CS_QOS_DOT1P_NO];
extern cs_uint8_t cs_qos_dscp_map_tbl[CS_QOS_INGRESS_PORT_MAX_][CS_QOS_DSCP_NO];
static void cs_offset_based_qos_init_config_default(void)
{
    int i, j;

    cs_qos_current_mode[CS_QOS_INGRESS_PORT_GMAC0] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_GMAC1] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_GMAC2] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_CPU]   = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_WLAN0] = CS_QOS_MODE_DSCP_TC;
    cs_qos_current_mode[CS_QOS_INGRESS_PORT_WLAN1] = CS_QOS_MODE_DSCP_TC;

    //Set default DOT1P default priority
    // DOT1P     Priority
    // ==================
    //   0          0
    //   1          1
    //   2          2
    //   3          3
    //   4          4
    //   5          5
    //   6          6
    //   7          7
    for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++) {
        for (j=0; j<CS_QOS_DOT1P_NO; j++) {
            cs_qos_dot1p_map_tbl[i][j] = j;
        }
    }
    //Set default DSCP default priority
    //  DSCP      Priority
    // ==================
    // 000xxx        0
    // 001xxx        1
    // 010xxx        2
    // 011xxx        3
    // 100xxx        4
    // 101xxx        5
    // 110xxx        6
    // 111xxx        7
    for (i=0; i<CS_QOS_INGRESS_PORT_MAX_; i++) {
        for (j=0; j<CS_QOS_DSCP_NO; j++) {
            cs_qos_dscp_map_tbl[i][j] = (j >> 3) & 0x07;
        }
    }

    return;
}/* cs_offset_based_qos_init_config_default() */


static u8 cs_offset_based_qos_get_qosrslt_idx(u8 port_id, u8 dscp, u8 dot1p)
{
    u8  qosrslt_idx = 7;
    cs_qos_mode_t qos_mode;
    cs_uint8_t qos_priority;

    if (cs_qos_map_mode_get(0, port_id, &qos_mode) != CS_E_OK) {
	    /* Should not be happened */
//	    printk("%s:%d:: ERROR !!! cs_qos_map_mode_get fail.\n", __func__, __LINE__);
	    return qosrslt_idx;
	}

    if (qos_mode == CS_QOS_MODE_DSCP_TC) {
        /* key: DOT1P*/
        if (dscp == 0xFF) {
            return qosrslt_idx;
        }

        if (cs_qos_dscp_map_get(0, port_id, dscp, &qos_priority) == CS_E_OK) {
            qosrslt_idx = 7 - qos_priority;
        }
    } else {
        /* key: DSCP */
        if (dot1p == 0xFF) {
            return qosrslt_idx;
        }

        if (cs_qos_dot1p_map_get(0, port_id, dot1p, &qos_priority) == CS_E_OK) {
            qosrslt_idx = 7 - qos_priority;
        }
    } /* if (qos_mode == CS_QOS_MODE_DSCP_TC) */

    return qosrslt_idx;
} /* cs_offset_based_qos_get_qosrslt_idx() */



static int cs_offset_based_qos_delete_qos_hash(u16 hash_idx)
{
	u32 crc32;
	u16 crc16, rslt_idx;
	u8 mask_ptr;
	int ret;

	ret = cs_fe_hash_get_hash(hash_idx, &crc32, &crc16, &mask_ptr,
			&rslt_idx);
	if (ret != 0)
		return ret;

	/* del hash at first to avoid junk pkt */
	ret = cs_fe_hash_del_hash(hash_idx);

	cs_fe_table_del_entry_by_idx(FE_TABLE_QOSRSLT, (unsigned int)rslt_idx,
			false);

	return ret;
} /* cs_offset_based_qos_delete_qos_hash */


static void cs_offset_based_qos_clear_hashmask(unsigned int hm_idx)
{
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_BCAST, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L2_MCAST, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L3_MCAST_V4, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L3_MCAST_V6, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L2_FLOW, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L3_FLOW, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L3_FLOW_V6, hm_idx, true);
#ifdef CS75XX_HW_ACCEL_TUNNEL
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L3_TUNNEL, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L3_TUNNEL_V6, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE0_TUNNEL, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE0_TUNNEL_V6, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE1_TUNNEL, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE1_TUNNEL_V6, hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_L2_IPSEC, hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_WFO
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE0, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE1, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE0_WFO_L3, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE1_WFO_L3, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE0_WFO_L3_V6, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_RE1_WFO_L3_V6, hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_CPU_L3, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_CPU, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_CPU_L3_V6, hm_idx, true);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS_V6, hm_idx, true);
	cs_core_vtable_del_hashmask_by_idx(
			CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS_V6, hm_idx, true);
#endif

    return;
} /* cs_offset_based_qos_clear_hashmask() */


int cs_offset_based_qos_port_add(u8 port_id)
{
	fe_sw_hash_t key;
	unsigned int idx;
 	u32 crc32;
	u16 crc16;
	int ret, i, j;


	if ( offset_based_qos_hm_idx == ~0x0) {
	    /* Should not be happened */
	    printk("%s:%d:: ERROR !!! QoS default hashmask didn't init.\n", __func__, __LINE__);
	    return -1;
	}

	/* delete all the hashes created */
	if (offset_based_qos_port_bit_mask[port_id] & offset_based_qos_port_enabled_mask) {
	    offset_based_qos_port_enabled_mask &= ~offset_based_qos_port_bit_mask[port_id];
		for (i=0; i<CS_OFFSET_BASED_QOS_DSCP_SIZE; i++) {
		    for (j=0; j<CS_OFFSET_BASED_QOS_DOT1P_SIZE; j++) {
		        /* DSCP + DOT1P */
			    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dscp_dot1p[i][j]);
			}
	        /* DSCP */
		    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dscp[i]);
		}
	    for (j=0; j<CS_OFFSET_BASED_QOS_DOT1P_SIZE; j++) {
	        /* DOT1P */
		    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dot1p[j]);
		}
	}


	/* now.. create the hashes!! */
	/* DSCP + DOT1P */
	for (i=0; i<CS_OFFSET_BASED_QOS_DSCP_SIZE; i++) {
	    for (j=0; j<CS_OFFSET_BASED_QOS_DOT1P_SIZE; j++) {
        	memset(&key, 0x0, sizeof(fe_sw_hash_t));
        	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
	        key.dscp = (i<<1);
	        key.ip_valid = 1;
	        /* encoded TPID =
			 * 0:VLAN invalid
			 * 4:0x8100
			 * 5:0x9100
			 * 6:0x88a8
			 * 7:0x9200
			 */
			key.tpid_enc_1 = 4;
	        key._8021p_1 = j;
	        key.lspid = port_id;

    		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
    		if (ret != 0) {
    		    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
    			return ret;
    		}

            idx = cs_offset_based_qos_get_qosrslt_idx(port_id, (u8)(i<<1), (u8)j);

    		ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
    				qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dscp_dot1p[i][j]);
//printk("%s:%d:: (DSCP + DOT1P) (%2.2d, %2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
//        __func__, __LINE__, port_id, (i<<1), j, idx, offset_based_qos_hash_table[port_id].dscp_dot1p[i][j]);
    		if (ret != 0) {
    		    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
    			return ret;
    		}

    		if (likely((i+j) > 0)) {
    			cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
    					qosrslt_idx[idx]);
    		}
	    } /* for (j=0; j<8; j++) */
	} /* for (i=0; i<32; i++) */

	/* DSCP */
	for (i=0; i<CS_OFFSET_BASED_QOS_DSCP_SIZE; i++) {
    	memset(&key, 0x0, sizeof(fe_sw_hash_t));
    	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
        key.dscp = (i<<1);
        key.ip_valid = 1;
        /* encoded TPID =
		 * 0:VLAN invalid
		 * 4:0x8100
		 * 5:0x9100
		 * 6:0x88a8
		 * 7:0x9200
		 */
		key.tpid_enc_1 = 0;
        key.lspid = port_id;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
			return ret;
		}

        idx = cs_offset_based_qos_get_qosrslt_idx(port_id, (u8)(i<<1), 0xFF);

		ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
				qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dscp[i]);
//printk("%s:%d:: (DSCP) (%2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
//        __func__, __LINE__, port_id, (i<<1), idx, offset_based_qos_hash_table[port_id].dscp_dot1p[i][j]);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
			return ret;
		}

		if (likely(i > 0)) {
			cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
					qosrslt_idx[idx]);
		}
	} /* for (i=0; i<32; i++) */

	/* DOT1P */
    for (j=0; j<CS_OFFSET_BASED_QOS_DOT1P_SIZE; j++) {
    	memset(&key, 0x0, sizeof(fe_sw_hash_t));
    	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
        /* encoded TPID =
		 * 0:VLAN invalid
		 * 4:0x8100
		 * 5:0x9100
		 * 6:0x88a8
		 * 7:0x9200
		 */
		key.tpid_enc_1 = 4;
        key._8021p_1 = j;
        key.lspid = port_id;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
			return ret;
		}

        idx = cs_offset_based_qos_get_qosrslt_idx(port_id, 0xFF, (u8)j);

		ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
				qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dot1p[j]);
//printk("%s:%d:: (DOT1P) (%2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
//        __func__, __LINE__, port_id, j, idx, offset_based_qos_hash_table[port_id].dscp_dot1p[i][j]);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
			return ret;
		}

		if (likely((i+j) > 0)) {
			cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
					qosrslt_idx[idx]);
		}
    } /* for (j=0; j<8; j++) */

    offset_based_qos_port_enabled_mask |= offset_based_qos_port_bit_mask[port_id];

    return 0;
} /* cs_offset_based_qos_port_add */
EXPORT_SYMBOL(cs_offset_based_qos_port_add);


int cs_offset_based_qos_mode_update(u8 port_id)
{
    return cs_offset_based_qos_port_add(port_id);
} /* cs_offset_based_qos_mode_update */
EXPORT_SYMBOL(cs_offset_based_qos_mode_update);


int cs_offset_based_qos_dscp_update(u8 port_id, u8 dscp)
{
	fe_sw_hash_t key;
	unsigned int idx;
 	u32 crc32;
	u16 crc16;
	int ret, i;
    u8 offset_based_qos_dscp = dscp >> 1;

//    printk("%s:%d:: port_id %d, offset_based_qos_dscp %d\n", __func__, __LINE__, port_id, offset_based_qos_dscp);

	if ( offset_based_qos_hm_idx == ~0x0) {
	    /* Should not be happened */
	    printk("%s:%d:: ERROR !!! QoS default hashmask didn't init.\n", __func__, __LINE__);
	    return -1;
	}

	/* delete the DSCP hashes created */
	if (offset_based_qos_port_bit_mask[port_id] & offset_based_qos_port_enabled_mask) {
		for (i=0; i<CS_OFFSET_BASED_QOS_DOT1P_SIZE; i++) {
	        /* DSCP + DOT1P */
		    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dscp_dot1p[offset_based_qos_dscp][i]);
		}
        /* DSCP */
	    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dscp[offset_based_qos_dscp]);
	}

	/* now.. create the hashes!! */
	/* DSCP + DOT1P */
    for (i=0; i<CS_OFFSET_BASED_QOS_DOT1P_SIZE; i++) {
    	memset(&key, 0x0, sizeof(fe_sw_hash_t));
    	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
        key.dscp = dscp;
        key.ip_valid = 1;
        /* encoded TPID =
		 * 0:VLAN invalid
		 * 4:0x8100
		 * 5:0x9100
		 * 6:0x88a8
		 * 7:0x9200
		 */
		key.tpid_enc_1 = 4;
        key._8021p_1 = i;
        key.lspid = port_id;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
			return ret;
		}

        idx = cs_offset_based_qos_get_qosrslt_idx(port_id, (u8)dscp, (u8)i);

		ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
				qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dscp_dot1p[offset_based_qos_dscp][i]);
//printk("%s:%d:: (DSCP + DOT1P) (%2.2d, %2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
//        __func__, __LINE__, port_id, dscp, i, idx, offset_based_qos_hash_table[port_id].dscp_dot1p[offset_based_qos_dscp][i]);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
			return ret;
		}

		if (likely(i > 0)) {
			cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
					qosrslt_idx[idx]);
		}
    } /* for (i=0; i<CS_OFFSET_BASED_QOS_DOT1P_SIZE; i++) */

	/* DSCP */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
    key.dscp = dscp;
    key.ip_valid = 1;
    /* encoded TPID =
	 * 0:VLAN invalid
	 * 4:0x8100
	 * 5:0x9100
	 * 6:0x88a8
	 * 7:0x9200
	 */
	key.tpid_enc_1 = 0;
    key.lspid = port_id;

	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
	    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
		return ret;
	}

    idx = cs_offset_based_qos_get_qosrslt_idx(port_id, (u8)dscp, 0xFF);

	ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
			qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dscp[offset_based_qos_dscp]);
//printk("%s:%d:: (DSCP) (%2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
//        __func__, __LINE__, port_id, dscp, idx, offset_based_qos_hash_table[port_id].dscp_dot1p[offset_based_qos_dscp][i]);
	if (ret != 0) {
	    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
		return ret;
	}

	if (likely(i > 0)) {
		cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
				qosrslt_idx[idx]);
	}

    return 0;
} /* cs_offset_based_qos_dscp_update */
EXPORT_SYMBOL(cs_offset_based_qos_dscp_update);


int cs_offset_based_qos_dot1p_update(u8 port_id, u8 dot1p)
{
	fe_sw_hash_t key;
	unsigned int idx;
 	u32 crc32;
	u16 crc16;
	int ret, i;

    printk("%s:%d:: port_id %d, dot1p %d\n", __func__, __LINE__, port_id, dot1p);

	if ( offset_based_qos_hm_idx == ~0x0) {
	    /* Should not be happened */
	    printk("%s:%d:: ERROR !!! QoS default hashmask didn't init.\n", __func__, __LINE__);
	    return -1;
	}

	/* delete all the hashes created */
	if (offset_based_qos_port_bit_mask[port_id] & offset_based_qos_port_enabled_mask) {
		for (i=0; i<CS_OFFSET_BASED_QOS_DSCP_SIZE; i++) {
	        /* DSCP + DOT1P */
		    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dscp_dot1p[i][dot1p]);
		}
        /* DOT1P */
	    ret = cs_offset_based_qos_delete_qos_hash(offset_based_qos_hash_table[port_id].dot1p[dot1p]);
	}


	/* now.. create the hashes!! */
	/* DSCP + DOT1P */
	for (i=0; i<CS_OFFSET_BASED_QOS_DSCP_SIZE; i++) {
    	memset(&key, 0x0, sizeof(fe_sw_hash_t));
    	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
        key.dscp = (i<<1);
        key.ip_valid = 1;
        /* encoded TPID =
		 * 0:VLAN invalid
		 * 4:0x8100
		 * 5:0x9100
		 * 6:0x88a8
		 * 7:0x9200
		 */
		key.tpid_enc_1 = 4;
        key._8021p_1 = dot1p;
        key.lspid = port_id;

		ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
			return ret;
		}

        idx = cs_offset_based_qos_get_qosrslt_idx(port_id, (u8)(i<<1), (u8)dot1p);

		ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
				qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dscp_dot1p[i][dot1p]);
printk("%s:%d:: (DSCP + DOT1P) (%2.2d, %2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
        __func__, __LINE__, port_id, (i<<1), dot1p, idx, offset_based_qos_hash_table[port_id].dscp_dot1p[i][dot1p]);
		if (ret != 0) {
		    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
			return ret;
		}

		if (likely((i+dot1p) > 0)) {
			cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
					qosrslt_idx[idx]);
		}
	} /* for (i=0; i<32; i++) */

	/* DOT1P */
	memset(&key, 0x0, sizeof(fe_sw_hash_t));
	key.mask_ptr_0_7 = offset_based_qos_hm_idx;
    /* encoded TPID =
	 * 0:VLAN invalid
	 * 4:0x8100
	 * 5:0x9100
	 * 6:0x88a8
	 * 7:0x9200
	 */
	key.tpid_enc_1 = 4;
    key._8021p_1 = dot1p;
    key.lspid = port_id;

	ret = cs_fe_hash_calc_crc(&key, &crc32, &crc16, CRC16_CCITT);
	if (ret != 0) {
	    printk("%s:%d:: ERROR!!! cs_fe_hash_calc_crc\n", __func__, __LINE__);
		return ret;
	}

    idx = cs_offset_based_qos_get_qosrslt_idx(port_id, 0xFF, (u8)dot1p);

	ret = cs_fe_hash_add_hash(crc32, crc16, offset_based_qos_hm_idx,
			qosrslt_idx[idx], &offset_based_qos_hash_table[port_id].dot1p[dot1p]);
printk("%s:%d:: (DOT1P) (%2.2d, %2.2d) idx %d, offset_based_qos_hash_table 0x%4.4x\n",
        __func__, __LINE__, port_id, dot1p, idx, offset_based_qos_hash_table[port_id].dscp_dot1p[i][dot1p]);
	if (ret != 0) {
	    printk("%s:%d:: ERROR!!! cs_fe_hash_add_hash\n", __func__, __LINE__);
		return ret;
	}

	if (likely((i+dot1p) > 0)) {
		cs_fe_table_inc_entry_refcnt(FE_TABLE_QOSRSLT,
				qosrslt_idx[idx]);
	}

    return 0;
} /* cs_offset_based_qos_dot1p_update */
EXPORT_SYMBOL(cs_offset_based_qos_dot1p_update);


int cs_offset_based_qos_init(void)
{
	unsigned int bc_vtable_hm_idx = ~0x0, mc_vtable_hm_idx = ~0x0;
	unsigned int l2_vtable_hm_idx = ~0x0, l3_vtable_hm_idx = ~0x0;
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	unsigned int cpu_l3_vtable_hm_idx = ~0x0, cpu_vtable_hm_idx = ~0x0;
#endif
#ifdef CONFIG_CS75XX_WFO
	unsigned int pe0_l3_vtable_hm_idx = ~0x0, pe1_l3_vtable_hm_idx = ~0x0;
	unsigned int pe0_vtable_hm_idx = ~0x0, pe1_vtable_hm_idx = ~0x0;
#endif
 	fe_hash_mask_entry_t hash_mask;
	fe_qos_result_entry_t qos_rslt;
	int ret = 0;
	int i;

    printk("\nInitial QoS Default Hashes.\n");
    cs_offset_based_qos_init_config_default();

	memset(offset_based_qos_hash_table, 0x0, sizeof(offset_based_qos_hash_table));

    /* Clear hashmask for all vtable */
	if (offset_based_qos_hm_idx != ~0x0) {
	    cs_offset_based_qos_clear_hashmask(offset_based_qos_hm_idx);
		offset_based_qos_hm_idx = ~0x0;
	}

	/* allocate the HashMask into the vtable that we care */
	memset(&hash_mask, 0xff, sizeof(fe_hash_mask_entry_t));
	hash_mask.keygen_poly_sel = 0;
	hash_mask.ip_sa_mask = 0;
	hash_mask.ip_da_mask = 0;

	hash_mask.dscp_mask = 0;
    hash_mask.ip_vld_mask = 0;
	hash_mask.tpid_enc_1_msb_mask = 0;
	hash_mask._8021p_1_mask = 0;
	hash_mask.lspid_mask = 0;

	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_BCAST, &hash_mask,
			2, true, &bc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_MCAST, &hash_mask,
			2, true, &mc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_MCAST_V4, &hash_mask,
			2, true, &mc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_MCAST_V6, &hash_mask,
			2, true, &mc_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_FLOW, &hash_mask,
			2, true, &l2_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_FLOW, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_FLOW_V6, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#ifdef CS75XX_HW_ACCEL_TUNNEL
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_TUNNEL, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L3_TUNNEL_V6, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE0_TUNNEL, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE0_TUNNEL_V6, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE1_TUNNEL, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE1_TUNNEL_V6, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_IPSEC_PASS
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_L2_IPSEC, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_WFO
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE0, &hash_mask,
			2, true, &pe0_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE1, &hash_mask,
			2, true, &pe1_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE0_WFO_L3, &hash_mask,
			2, true, &pe0_l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE1_WFO_L3, &hash_mask,
			2, true, &pe1_l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE0_WFO_L3_V6, &hash_mask,
			2, true, &pe0_l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_RE1_WFO_L3_V6, &hash_mask,
			2, true, &pe1_l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_CPU_L3, &hash_mask,
			2, true, &cpu_l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_CPU, &hash_mask,
			2, true, &cpu_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_CPU_L3_V6, &hash_mask,
			2, true, &cpu_l3_vtable_hm_idx);
#endif
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC_NATT
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_IPSEC_NATT_INGRESS_V6, &hash_mask,
			2, true, &l3_vtable_hm_idx);
	cs_core_vtable_add_hashmask(CORE_VTABLE_TYPE_IPSEC_NATT_EGRESS_V6, &hash_mask,
			2, true, &l3_vtable_hm_idx);
#endif

	/* supposedly all the hash mask indice should be the same */
	if ( (bc_vtable_hm_idx != mc_vtable_hm_idx)
			&& (l2_vtable_hm_idx != l3_vtable_hm_idx)
			&& (l2_vtable_hm_idx != bc_vtable_hm_idx)
#ifdef CONFIG_CS75XX_HW_ACCEL_WIRELESS
			&& (cpu_l3_vtable_hm_idx != l3_vtable_hm_idx)
			&& (cpu_vtable_hm_idx != bc_vtable_hm_idx)
#endif
			) {
	    cs_offset_based_qos_clear_hashmask(offset_based_qos_hm_idx);
		return -1;
    }
	offset_based_qos_hm_idx = bc_vtable_hm_idx;

	/* create all the hashes */
 	/* first.. get the default qosrslt_idx */
 	for (i=0; i<8; i++) {
    	memset(&qos_rslt, 0x0, sizeof(fe_qos_result_entry_t));
    	qos_rslt.voq_cos = i;
    	ret = cs_fe_table_add_entry(FE_TABLE_QOSRSLT, &qos_rslt, &qosrslt_idx[i]);
//	    printk("%s:%d:: qosrslt_idx[%d] %d\n", __func__, __LINE__, i, qosrslt_idx[i]);
    	if (ret != 0) {
	    cs_offset_based_qos_clear_hashmask(offset_based_qos_hm_idx);
    		offset_based_qos_hm_idx = ~0x0;
    		return -2;
    	} /* if (ret != 0) */
    } /* for (i=0; i<32; i++) */


    printk("%s:: offset_based_qos_hm_idx %d.\n", __func__, offset_based_qos_hm_idx);
    ret  = cs_offset_based_qos_port_add(GE_PORT0);
    ret |= cs_offset_based_qos_port_add(GE_PORT1);
    ret |= cs_offset_based_qos_port_add(GE_PORT2);
    ret |= cs_offset_based_qos_port_add(CPU_PORT);
    ret |= cs_offset_based_qos_port_add(PE0_PORT);
    ret |= cs_offset_based_qos_port_add(PE1_PORT);
    printk("%s:: ret %d\n", __func__, ret);


	return ret;
} /* cs_offset_based_qos_init */

