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
#include <linux/spinlock.h>
#include <mach/cs_types.h>
#include <mach/cs75xx_fe_core_table.h>
#include <cs_fe_table_api.h>
#include <cs_fe_head_table.h>
#include <cs_fe_lpm_api.h>
#include <cs_core_logic_data.h>
#include <cs_core_vtable.h>
#include <cs_fe_util_api.h>

#include <mach/cs_lpm_api.h>

#ifdef CONFIG_CS752X_PROC
#include <cs752x_proc.h>
extern u32 cs_adapt_debug;
#endif

typedef struct cs_fwd_result_internal_s {
        cs_int32_t              used;
	cs_fwd_result_t		fwd_result;
} cs_fwd_result_internal_t;

typedef struct cs_lpm_internal_s {
        cs_int32_t              used;
	cs_uint32_t             lpm_idx;                      /* kept by this layer, upper layer no need to fill it */
	cs_lpm_t		lpm;
} cs_lpm_internal_t;

static cs_fwd_result_internal_t g_fwd_result[FE_LPM_ENTRY_MAX];
static cs_lpm_internal_t g_lpm[FE_LPM_ENTRY_MAX];
static cs_uint8_t g_fwd_result_count = 0;
static cs_uint8_t g_lpm_count = 0;

static cs_uint8_t g_lpm_init = 0;

static spinlock_t g_lpm_lock;

static cs_status_t cs_fwd_result_delete_by_index(cs_dev_id_t device_id, cs_uint32_t index);
cs_status_t cs_lpm_fwd_result_init(cs_dev_id_t device_id)
{
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		printk("%s: Enter..\n", __func__);
	}
#endif

	if (g_lpm_init == 0) {

		spin_lock_init(&g_lpm_lock);
		memset(&(g_fwd_result[0]), 0, sizeof(g_fwd_result));
		memset(&(g_lpm[0]), 0, sizeof(g_lpm));
		g_fwd_result_count = 0;
		g_lpm_count = 0;
		g_lpm_init = 1;
		cs_lpm_init(0);
	}
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_lpm_fwd_result_init);

cs_status_t cs_lpm_fwd_result_shut(cs_dev_id_t device_id)
{
	int i, ret;

	spin_lock(&g_lpm_lock);
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		printk("%s: g_fwd_result_count=%d, g_lpm_count=%d\n", __func__, g_fwd_result_count, g_lpm_count);
	}
#endif

        if (g_lpm_init == 1) {
                for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {

			if (g_fwd_result[i].used == 0) {
				continue;
			}
			spin_unlock(&g_lpm_lock);
			ret = cs_fwd_result_delete_by_index(device_id, i);
			spin_lock(&g_lpm_lock);
        		if (ret != CS_E_OK) {
                		printk("%s: cs_fwd_result_delete_by_index(%d), return failed!! ret=%d\n", 
					__func__,  g_fwd_result[i].fwd_result.fwd_rslt_idx, ret);
        		}
#ifdef CONFIG_CS752X_PROC
        		if (cs_adapt_debug & CS752X_ADAPT_LPM) {
				printk("%s: forward result index=%d is deleted\n", __func__, g_fwd_result[i].fwd_result.fwd_rslt_idx);
        		}
#endif
                }

		/* because cs_lpm_delete_index() will re-sort the LPM table after delete, need to delete from bottom to top */
		for (i = FE_LPM_ENTRY_MAX - 1; i >= 0; i--) {

			if (g_lpm[i].used == 0) {
				continue;
			}
			ret = cs_lpm_delete_index(g_lpm[i].lpm_idx);
        		if (ret != FE_TABLE_OK) {
                		printk("%s: cs_lpm_delete_idx(%d) return failed!! ret=%d\n", __func__, g_lpm[i].lpm_idx, ret);
        		}
#ifdef CONFIG_CS752X_PROC
                       	if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                               	printk("%s: lpm index=%d is deleted\n", __func__, g_lpm[i].lpm_idx);
                       	}
#endif
		}

                memset(&(g_fwd_result[0]), 0, sizeof(g_fwd_result));
                memset(&(g_lpm[0]), 0, sizeof(g_lpm));
                g_fwd_result_count = 0;
                g_lpm_count = 0;
                g_lpm_init = 0;
		cs_lpm_disable();
        }
	spin_unlock(&g_lpm_lock);
        return CS_E_OK;
}
EXPORT_SYMBOL(cs_lpm_fwd_result_shut);

static cs_status_t add_set_port_encap(cs_fwd_result_t *fwd_result, fe_fwd_result_entry_t *p_fwd_rslt_entry, cs_uint32_t *p_fvlan_idx, cs_uint32_t *p_mac_idx, cs_uint8_t action)
{
        cs_int8_t type = 0;
        cs_uint8_t tmp_buf[CS_ETH_ADDR_LEN * 2];
        cs_uint8_t *ptr;
        int i, ret;
	fe_flow_vlan_entry_t fvlan_entry, *p_fvlan_entry=&fvlan_entry;

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		if (action == 0) {
        		printk("%s: fvlan index=%d, mac_idx=%d\n", __func__, *p_fvlan_idx, *p_mac_idx);
		}
        }
#endif

	*p_mac_idx = 0xffffffff;
        *p_fvlan_idx = 0xffffffff;
	if (fwd_result->enc_type == CS_PORT_ENCAP_PPPOE_E || fwd_result->enc_type == CS_PORT_ENCAP_ETH_1Q_E || fwd_result->enc_type == CS_PORT_ENCAP_ETH_QinQ_E) {

       		if (fwd_result->enc_type == CS_PORT_ENCAP_PPPOE_E) {
                	p_fwd_rslt_entry->l2.pppoe_encap_en = 1;
		}

        	p_fwd_rslt_entry->l2.flow_vlan_op_en = 1;
        	memset(p_fvlan_entry, 0, sizeof(fe_flow_vlan_entry_t));
        	if (fwd_result->tag[0] != 0) {
        		p_fvlan_entry->first_vlan_cmd = CS_FE_VLAN_CMD_SWAP_B;
		}
		else {
                	p_fvlan_entry->first_vlan_cmd = CS_FE_VLAN_CMD_POP_A;
		}
        	p_fvlan_entry->first_vid = fwd_result->tag[0];
        	p_fvlan_entry->first_tpid_enc = 0;  /* ETH_P_8021Q */

 		if (action == 1)
			ret = cs_fe_table_add_entry(FE_TABLE_FVLAN, p_fvlan_entry, (unsigned int *)p_fvlan_idx);
        	else
                	ret = cs_fe_table_set_entry(FE_TABLE_FVLAN, *p_fvlan_idx, p_fvlan_entry);
        	if (ret != FE_TABLE_OK) {
			if (action == 1)
              			printk("%s: cs_fe_table_add_entry(FE_TABLE_FVLAN) return failed!! ret=%d\n", __func__,  ret);
			else
                       		printk("%s: cs_fe_table_set_entry(FE_TABLE_FVLAN, fvlan_idx=%d) return failed!! ret=%d\n", __func__, *p_fvlan_idx, ret);
                	return CS_E_ERROR;
        	}
        	p_fwd_rslt_entry->l2.flow_vlan_index = *p_fvlan_idx;
#ifdef CONFIG_CS752X_PROC
       		if (cs_adapt_debug & CS752X_ADAPT_LPM) {
              		cs_fe_table_print_entry(FE_TABLE_FVLAN, *p_fvlan_idx);
		}
#endif
	}

	for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
		if (fwd_result->src_mac[i] != 0)
			break;
	}
	if (i != CS_ETH_ADDR_LEN)
        	p_fwd_rslt_entry->l2.mac_sa_replace_en = 1;

	for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
                if (fwd_result->dest_mac[i] != 0)
                        break;
        }
        if (i != CS_ETH_ADDR_LEN)
                p_fwd_rslt_entry->l2.mac_da_replace_en = 1;

        if (p_fwd_rslt_entry->l2.mac_sa_replace_en == 1 && p_fwd_rslt_entry->l2.mac_da_replace_en == 1)
                type = L2_LOOKUP_TYPE_PAIR;
        else if (p_fwd_rslt_entry->l2.mac_sa_replace_en == 1 && p_fwd_rslt_entry->l2.mac_da_replace_en == 0)
                type = L2_LOOKUP_TYPE_SA;
        else if (p_fwd_rslt_entry->l2.mac_sa_replace_en == 0 && p_fwd_rslt_entry->l2.mac_da_replace_en == 1)
                type = L2_LOOKUP_TYPE_DA;
        else
                type = FE_TABLE_EOPNOTSUPP;
        if (type != FE_TABLE_EOPNOTSUPP) {
                if (type == L2_LOOKUP_TYPE_PAIR) {
                	memcpy(&tmp_buf[0], &fwd_result->dest_mac[0], CS_ETH_ADDR_LEN);
                        memcpy(&tmp_buf[CS_ETH_ADDR_LEN], &fwd_result->src_mac[0], CS_ETH_ADDR_LEN);
                        ptr = &tmp_buf[0];
                }
                else if (type == L2_LOOKUP_TYPE_SA)
                        ptr = &(fwd_result->src_mac[0]);
                else
                        ptr = &(fwd_result->dest_mac[0]);

                ret = cs_fe_l2_result_alloc(ptr, type, p_mac_idx);
                if (ret != FE_TABLE_OK) {
                        printk("%s: cs_fe_l2_result_alloc(type=%d) return failed!! ret=%d\n", __func__, type,  ret);
			if (*p_fvlan_idx != 0xffffffff) 
                        	cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN, *p_fvlan_idx, 0);
                        return CS_E_ERROR;
                }
                p_fwd_rslt_entry->l2.l2_index = *p_mac_idx;
                //printk("%s: mac_idx=%d\n", __func__, *p_mac_idx);
	}

	/* to implement routing function dec ttl by 1 */
        p_fwd_rslt_entry->l3.decr_ttl_hoplimit = 1;

	fwd_result->mac_type = type;
	fwd_result->mac_idx = *p_mac_idx;
	fwd_result->fvlan_idx = *p_fvlan_idx;
        return CS_E_OK;
}

static cs_status_t add_set_voq_pol_entry(cs_fwd_result_t *fwd_result, fe_voq_pol_entry_t *p_voq_pol_rslt_entry, cs_uint32_t *p_voq_pol_rslt_idx, cs_uint8_t action)
{
	cs_status_t ret;

	/* preapre fe_voq_pol_entry_t */
        memset(p_voq_pol_rslt_entry, 0 ,sizeof(fe_voq_pol_entry_t));
        p_voq_pol_rslt_entry->voq_base = fwd_result->voq_base;
        p_voq_pol_rslt_entry->pol_base = 0;
        p_voq_pol_rslt_entry->cpu_pid = 0;
        p_voq_pol_rslt_entry->ldpid = fwd_result->port_id;

        if (fwd_result->enc_type == CS_PORT_ENCAP_PPPOE_E)
                p_voq_pol_rslt_entry->pppoe_session_id = fwd_result->pppoe_session_id;
        else
                p_voq_pol_rslt_entry->pppoe_session_id = 0;
        p_voq_pol_rslt_entry->cos_nop = 0;
        p_voq_pol_rslt_entry->parity = 0;

	if (action == 1)
        	ret = cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER, p_voq_pol_rslt_entry, (unsigned int *)p_voq_pol_rslt_idx);
	else
        	ret = cs_fe_table_set_entry(FE_TABLE_VOQ_POLICER, *p_voq_pol_rslt_idx, p_voq_pol_rslt_entry);
	
        if (ret != FE_TABLE_OK) {
		if (action == 1)
                	printk("%s: cs_fe_table_add_entry(FE_TABLE_VOQ_POLICER) return failed!! ret=%d\n", __func__, ret);
		else
                	printk("%s: cs_fe_table_set_entry(FE_TABLE_VOQ_POLICER, voq_pol_rslt_idx=%d) return failed!! ret=%d\n", __func__, ret, *p_voq_pol_rslt_idx);
                return CS_E_ERROR;
        }
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                printk("%s: ret = %d, voq_pol_rslt_idx=%d\n", __func__, ret, (int)*p_voq_pol_rslt_idx);
        	cs_fe_table_print_entry(FE_TABLE_VOQ_POLICER, *p_voq_pol_rslt_idx);
        }
#endif
	return CS_E_OK;
}

cs_status_t cs_fwd_result_add(cs_dev_id_t device_id, cs_fwd_result_t *fwd_result, cs_uint32_t *index)
{
	fe_voq_pol_entry_t voq_pol_rslt_entry, *p_voq_pol_rslt_entry=&voq_pol_rslt_entry;
	fe_fwd_result_entry_t fwd_rslt_entry, *p_fwd_rslt_entry=&fwd_rslt_entry;
	cs_uint32_t fwd_rslt_idx, voq_pol_rslt_idx;
	cs_uint32_t fvlan_idx, mac_idx;
	cs_status_t i, ret;

	spin_lock(&g_lpm_lock);
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {	
		printk("%s: device_id=%d, fwd_result=0x%x\n", __func__, device_id,(unsigned int) fwd_result);

		printk("%s: fwd_result->app_type=%d\n", __func__, fwd_result->app_type);
		printk("%s: fwd_result->port_id=%d\n", __func__, fwd_result->port_id);
		printk("%s: fwd_result->voq_base=%d\n", __func__, fwd_result->voq_base);
		printk("%s: fwd_result->enc_type=%d\n", __func__, fwd_result->enc_type);
		printk("%s: fwd_result->pppoe_session_id=%d\n", __func__, fwd_result->pppoe_session_id);		
		printk("%s: fwd_result->src_mac[0-5]=0x%2x-0x%2x-0x%2x-0x%2x-0x%2x-0x%2x\n", __func__,
			(unsigned int)fwd_result->src_mac[0], (unsigned int)fwd_result->src_mac[1], (unsigned int)fwd_result->src_mac[2], 
			(unsigned int)fwd_result->src_mac[3], (unsigned int)fwd_result->src_mac[4], (unsigned int)fwd_result->src_mac[5]);
		printk("%s: fwd_result->dest_mac[0-5]=0x%2x-0x%2x-0x%2x-0x%2x-0x%2x-0x%2x\n", __func__,
                        (unsigned int)fwd_result->dest_mac[0], (unsigned int)fwd_result->dest_mac[1], (unsigned int)fwd_result->dest_mac[2],
                        (unsigned int)fwd_result->dest_mac[3], (unsigned int)fwd_result->dest_mac[4], (unsigned int)fwd_result->dest_mac[5]);
		printk("%s: fwd_result->tag[0-1]=0x%4x-0x%4x\n", __func__, (unsigned int)fwd_result->tag[0], (unsigned int)fwd_result->tag[1]);
	}
#endif
	if (g_lpm_init == 0) {
		printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
		return CS_E_ERROR;
	}

	for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {
        	if (g_fwd_result[i].used == 0) {
#ifdef CONFIG_CS752X_PROC
        		if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                        	printk("%s: a free g_fwd_result[] entry found at index %d\n", __func__, i);
			}
#endif		
			*index = i;
                        break;
                }
        }
        if (i == FE_LPM_ENTRY_MAX) {
                printk("%s: No free g_fwd_result[] entry is found!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
		return CS_E_ERROR;
        }


	/* preapre fe_voq_pol_entry_t */
	ret = add_set_voq_pol_entry(fwd_result, p_voq_pol_rslt_entry, &voq_pol_rslt_idx, 1);	
	if (ret != CS_E_OK) {
                printk("%s: add_set_fvlan_entry(%d) failed, ret=%d\n", __func__, 1, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	/* prepare fe_fwd_result_entry_t */
	memset(p_fwd_rslt_entry, 0, sizeof(fe_fwd_result_entry_t));
        p_fwd_rslt_entry->dest.voq_pol_table_index = voq_pol_rslt_idx; 

	ret = add_set_port_encap(fwd_result, p_fwd_rslt_entry, &fvlan_idx, &mac_idx, 1);
	if (ret != CS_E_OK) {
		printk("%s: add_set_port_encap(%d) failed, ret=%d\n", __func__, 1, ret);
		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, voq_pol_rslt_idx, 0);
		spin_unlock(&g_lpm_lock);	
		return CS_E_ERROR;
	}
	
	ret = cs_fe_table_add_entry(FE_TABLE_FWDRSLT, p_fwd_rslt_entry, &fwd_rslt_idx);
        if (ret != FE_TABLE_OK) {
                printk("%s: cs_fe_table_add_entry(FE_TABLE_FWDRSLT) return failed!! ret=%d\n", __func__, ret);

		cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER, voq_pol_rslt_idx, 0);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		cs_fe_table_print_entry(FE_TABLE_FWDRSLT, fwd_rslt_idx);
		printk("%s: fwd_rslt_idx=%d is added, g_fwd_result_count=%d\n", __func__, fwd_rslt_idx, g_fwd_result_count);
	}
#endif

	fwd_result->fwd_rslt_idx = fwd_rslt_idx;
	fwd_result->voq_pol_rslt_idx = voq_pol_rslt_idx;

	g_fwd_result[*index].fwd_result = *fwd_result;
	g_fwd_result[*index].used = 1;
	g_fwd_result_count++;
	
	spin_unlock(&g_lpm_lock);	
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_fwd_result_add);

cs_status_t cs_fwd_result_get_by_index(cs_dev_id_t device_id, cs_uint32_t index, cs_fwd_result_t *fwd_result)
{
	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                printk("%s: device_id=%d, index=%d, fwd_result=0x%x\n", __func__, device_id, index, (unsigned int)fwd_result);
        }
#endif
	 if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	if (g_fwd_result[index].used == 0) {
		printk("%s: inde x%d not a used entry!!\n", __func__, index);
                spin_unlock(&g_lpm_lock);
                return CS_E_ERROR;
	}

	*fwd_result = g_fwd_result[index].fwd_result;
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                printk("%s: device_id=%d, fwd_result=0x%x\n", __func__, device_id, (unsigned int)fwd_result);

                printk("%s: fwd_result->app_type=%d\n", __func__, fwd_result->app_type);
                printk("%s: fwd_result->port_id=%d\n", __func__, fwd_result->port_id);
                printk("%s: fwd_result->voq_base=%d\n", __func__, fwd_result->voq_base);
                printk("%s: fwd_result->voq_pol_rslt_idx=%d\n", __func__, fwd_result->voq_pol_rslt_idx);
                printk("%s: fwd_result->fwd_rslt_idx=%d\n", __func__, fwd_result->fwd_rslt_idx);
                printk("%s: fwd_result->mac_idx=%d, fwd_result->mac_type=%d\n", __func__, fwd_result->mac_idx, fwd_result->mac_type);
        }
#endif
	spin_unlock(&g_lpm_lock);	
        return CS_E_OK;
}
EXPORT_SYMBOL(cs_fwd_result_get_by_index);

cs_status_t cs_fwd_result_search(cs_dev_id_t device_id, cs_fwd_result_t *fwd_result, cs_uint32_t *index)
{
        cs_fwd_result_t *p_fwd_result;
        int i;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
        	printk("%s: device_id=%d, fwd_result=0x%x\n", __func__, device_id, (unsigned int)fwd_result);

        	printk("%s: fwd_result->app_type=%d\n", __func__, fwd_result->app_type);
        	printk("%s: fwd_result->port_id=%d\n", __func__, fwd_result->port_id);
        	printk("%s: fwd_result->voq_base=%d\n", __func__, fwd_result->voq_base);
        	printk("%s: fwd_result->voq_pol_rslt_idx=%d\n", __func__, fwd_result->voq_pol_rslt_idx);
        	printk("%s: fwd_result->fwd_rslt_idx=%d\n", __func__, fwd_result->fwd_rslt_idx);
	}
#endif

	if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

        for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {

		if (g_fwd_result[i].used == 0) {
			continue;
		}
                p_fwd_result = &g_fwd_result[i].fwd_result;

#ifdef CONFIG_CS752X_PROC
        	if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                	printk("%s: p_fwd_result->app_type=%d, p_fwd_result->port_id=%d, p_fwd_result->voq_base=%d, p_fwd_result->fwd_rslt_idx=%d, p_fwd_result->voq_pol_rslt_idx=%d\n", 
				__func__, p_fwd_result->app_type, p_fwd_result->port_id, p_fwd_result->voq_base, p_fwd_result->fwd_rslt_idx, p_fwd_result->voq_pol_rslt_idx);
		}
#endif
		if (memcmp(fwd_result, p_fwd_result, sizeof(cs_fwd_result_t)) == 0) {
                        printk("%s: the forward result is found, at index=%d!!\n", __func__, i);
                        break;
                }
        }
        if (i == FE_LPM_ENTRY_MAX) {
                printk("%s: the forward result is not found!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }
	*index = i;
	spin_unlock(&g_lpm_lock);	
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_fwd_result_search);

cs_status_t cs_fwd_result_update(cs_dev_id_t device_id, cs_uint32_t index, cs_fwd_result_t *fwd_result)
{
        fe_voq_pol_entry_t voq_pol_rslt_entry, *p_voq_pol_rslt_entry=&voq_pol_rslt_entry;
        fe_fwd_result_entry_t fwd_rslt_entry, *p_fwd_rslt_entry=&fwd_rslt_entry;
        cs_uint32_t fwd_rslt_idx, voq_pol_rslt_idx;
	cs_uint32_t fvlan_idx, mac_idx;
        cs_status_t ret;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                printk("%s: device_id=%d, index=%d, fwd_result=0x%x\n", __func__, device_id, index, (unsigned int) fwd_result);

                printk("%s: fwd_result->app_type=%d\n", __func__, fwd_result->app_type);
                printk("%s: fwd_result->port_id=%d\n", __func__, fwd_result->port_id);
                printk("%s: fwd_result->voq_base=%d\n", __func__, fwd_result->voq_base);
                printk("%s: fwd_result->enc_type=%d\n", __func__, fwd_result->enc_type);
                printk("%s: fwd_result->pppoe_session_id=%d\n", __func__, fwd_result->pppoe_session_id);
                printk("%s: fwd_result->src_mac[0-5]=0x%2x-0x%2x-0x%2x-0x%2x-0x%2x-0x%2x\n", __func__,
                        (unsigned int)fwd_result->src_mac[0], (unsigned int)fwd_result->src_mac[1], (unsigned int)fwd_result->src_mac[2],
                        (unsigned int)fwd_result->src_mac[3], (unsigned int)fwd_result->src_mac[4], (unsigned int)fwd_result->src_mac[5]);
                printk("%s: fwd_result->dest_mac[0-5]=0x%2x-0x%2x-0x%2x-0x%2x-0x%2x-0x%2x\n", __func__,
                        (unsigned int)fwd_result->dest_mac[0], (unsigned int)fwd_result->dest_mac[1], (unsigned int)fwd_result->dest_mac[2],
                        (unsigned int)fwd_result->dest_mac[3], (unsigned int)fwd_result->dest_mac[4], (unsigned int)fwd_result->dest_mac[5]);
                printk("%s: fwd_result->tag[0-1]=0x%4x-0x%4x\n", __func__, (unsigned int)fwd_result->tag[0], (unsigned int)fwd_result->tag[1]);
        }
#endif
	 if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	/* preapre fe_voq_pol_entry_t */
	voq_pol_rslt_idx = g_fwd_result[index].fwd_result.voq_pol_rslt_idx;
        ret = add_set_voq_pol_entry(fwd_result, p_voq_pol_rslt_entry, &voq_pol_rslt_idx, 0);
        if (ret != CS_E_OK) {
                printk("%s: add_set_fvlan_entry(%d) failed, ret=%d\n", __func__, 0, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

        /* prepare fe_fwd_result_entry_t */
	fvlan_idx = g_fwd_result[index].fwd_result.fvlan_idx;
        mac_idx = g_fwd_result[index].fwd_result.mac_idx;

	if (mac_idx != 0xffffffff) {
		ret = cs_fe_l2_result_dealloc(mac_idx, g_fwd_result[index].fwd_result.mac_type);
		if (ret != CS_E_OK) {
                	printk("%s: cs_fe_l2_result_dealloc(mac_idx=%d, mac_type=%d) failed, ret=%d\n", 
				__func__, mac_idx, g_fwd_result[index].fwd_result.mac_type, ret);
			spin_unlock(&g_lpm_lock);	
                	return CS_E_ERROR;
        	}
	}

	if (fvlan_idx != 0xffffffff){
                ret = cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN, fvlan_idx, 0);
		 if (ret != CS_E_OK) {
                        printk("%s: cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN, fvlan_idx=%d) failed, ret=%d\n",
                                __func__, fvlan_idx,ret);
                        spin_unlock(&g_lpm_lock);
                        return CS_E_ERROR;
                }
	}
	
	memset(p_fwd_rslt_entry, 0, sizeof(fe_fwd_result_entry_t));
        p_fwd_rslt_entry->dest.voq_pol_table_index = voq_pol_rslt_idx;

	ret = add_set_port_encap(fwd_result, p_fwd_rslt_entry, &fvlan_idx, &mac_idx, 0);
	if (ret != CS_E_OK) {
		printk("%s: add_set_port_encap(%d) failed, ret=%d\n", __func__, 0, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
	}

	fwd_rslt_idx = g_fwd_result[index].fwd_result.fwd_rslt_idx;
        ret = cs_fe_table_set_entry(FE_TABLE_FWDRSLT, fwd_rslt_idx, p_fwd_rslt_entry);
        if (ret != FE_TABLE_OK) {
                printk("%s: cs_fe_table_set_entry(FE_TABLE_FWDRSLT) return failed!! ret=%d\n", __func__, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
        	cs_fe_table_print_entry(FE_TABLE_FWDRSLT, fwd_rslt_idx);
	}
#endif

	fwd_result->fwd_rslt_idx = fwd_rslt_idx;
	fwd_result->voq_pol_rslt_idx = voq_pol_rslt_idx;
        g_fwd_result[index].fwd_result = *fwd_result;

	spin_unlock(&g_lpm_lock);	
        return CS_E_OK;
}
EXPORT_SYMBOL(cs_fwd_result_update);


cs_status_t cs_fwd_result_delete(cs_dev_id_t device_id, cs_fwd_result_t *fwd_result)
{
        int i;
        cs_fwd_result_t *p_fwd_result;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
	printk("%s: cs_adapt_debug=0x%x, CS752X_ADAPT_LPM=0x%x\n", __func__, cs_adapt_debug, CS752X_ADAPT_LPM);
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
        	printk("%s: device_id=%d, fwd_result=0x%x\n", __func__, device_id, (unsigned int)fwd_result);

        	printk("%s: fwd_result->app_type=%d\n", __func__, fwd_result->app_type);
        	printk("%s: fwd_result->port_id=%d\n", __func__, fwd_result->port_id);
        	printk("%s: fwd_result->voq_base=%d\n", __func__, fwd_result->voq_base);
        	printk("%s: fwd_result->voq_pol_rslt_idx=%d\n", __func__, fwd_result->voq_pol_rslt_idx);
        	printk("%s: fwd_result->fwd_rslt_idx=%d\n", __func__, fwd_result->fwd_rslt_idx);
	}
#endif
	if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

        for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {
		if (g_fwd_result[i].used == 0) {
			continue;
		}
                p_fwd_result = &g_fwd_result[i].fwd_result;
#ifdef CONFIG_CS752X_PROC
        	if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                	printk("%s: p_fwd_result->app_type=%d, p_fwd_result->port_id=%d, p_fwd_result->voq_base=%d, p_fwd_result->fwd_rslt_idx=%d, p_fwd_result->voq_pol_rslt_idx=%d\n", 
				__func__, p_fwd_result->app_type, p_fwd_result->port_id, p_fwd_result->voq_base, p_fwd_result->fwd_rslt_idx, p_fwd_result->voq_pol_rslt_idx);
		}
#endif
                if (!memcmp(fwd_result, p_fwd_result, sizeof(cs_fwd_result_t))) {
                        printk("%s: the forward result is found, at index=%d!!\n", __func__, i);
                        break;
                }
        }

        if (i == FE_LPM_ENTRY_MAX) {
                printk("%s: the forward result is not found!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	spin_unlock(&g_lpm_lock);	
	return cs_fwd_result_delete_entry_by_index(device_id, i);
}
EXPORT_SYMBOL(cs_fwd_result_delete);

static cs_status_t cs_fwd_result_delete_by_index(cs_dev_id_t device_id, cs_uint32_t index)
{
        int ret;
	cs_uint32_t fwd_rslt_idx;
        cs_uint32_t fvlan_idx;
        cs_uint32_t voq_pol_rslt_idx;
        cs_uint32_t mac_idx;
        cs_uint8_t    mac_type;
        cs_fwd_result_t *p_fwd_result;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        //printk("%s: cs_adapt_debug=0x%x, CS752X_ADAPT_LPM=0x%x\n", __func__, cs_adapt_debug, CS752X_ADAPT_LPM);
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                printk("%s: index=%d\n", __func__, index);
        }
#endif
        if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	if (g_fwd_result[index].used == 0) {  
		printk("%s: entry index %d of g_fwd_result[] is not used!!\n", __func__, index);
                spin_unlock(&g_lpm_lock);
                return CS_E_ERROR;
        }

        p_fwd_result = &g_fwd_result[index].fwd_result;
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                printk("%s: device_id=%d, p_fwd_result=0x%x\n", __func__, device_id, (unsigned int)p_fwd_result);

                printk("%s: p_fwd_result->app_type=%d\n", __func__, p_fwd_result->app_type);
                printk("%s: p_fwd_result->port_id=%d\n", __func__, p_fwd_result->port_id);
                printk("%s: p_fwd_result->voq_base=%d\n", __func__, p_fwd_result->voq_base);
                printk("%s: p_fwd_result->enc_type=%d\n", __func__, p_fwd_result->enc_type);
                printk("%s: p_fwd_result->pppoe_session_id=%d\n", __func__, p_fwd_result->pppoe_session_id);
		printk("%s: p_fwd_result->fvlan_idx=%d\n", __func__, p_fwd_result->fvlan_idx);
		printk("%s: p_fwd_result->voq_pol_rslt_idx=%d\n", __func__, p_fwd_result->voq_pol_rslt_idx);
		printk("%s: p_fwd_result->mac_idx=%d, p_fwd_result->mac_type=%d\n", __func__, p_fwd_result->mac_idx, p_fwd_result->mac_type);
		printk("%s: p_fwd_result->fwd_rslt_idx=%d\n", __func__, p_fwd_result->fwd_rslt_idx);
                printk("%s: p_fwd_result->src_mac[0-5]=0x%2x-0x%2x-0x%2x-0x%2x-0x%2x-0x%2x\n", __func__,
                        (unsigned int)p_fwd_result->src_mac[0], (unsigned int)p_fwd_result->src_mac[1], (unsigned int)p_fwd_result->src_mac[2],
                        (unsigned int)p_fwd_result->src_mac[3], (unsigned int)p_fwd_result->src_mac[4], (unsigned int)p_fwd_result->src_mac[5]);
                printk("%s: fwd_result->dest_mac[0-5]=0x%2x-0x%2x-0x%2x-0x%2x-0x%2x-0x%2x\n", __func__,
                        (unsigned int)p_fwd_result->dest_mac[0], (unsigned int)p_fwd_result->dest_mac[1], (unsigned int)p_fwd_result->dest_mac[2],
                        (unsigned int)p_fwd_result->dest_mac[3], (unsigned int)p_fwd_result->dest_mac[4], (unsigned int)p_fwd_result->dest_mac[5]);
                printk("%s: p_fwd_result->tag[0-1]=0x%4x-0x%4x\n", __func__, (unsigned int)p_fwd_result->tag[0], (unsigned int)p_fwd_result->tag[1]);
        }
#endif

        fwd_rslt_idx = g_fwd_result[index].fwd_result.fwd_rslt_idx;
	ret = cs_fe_fwdrslt_del_by_idx(fwd_rslt_idx);
	if (ret != FE_TABLE_OK) {
                     printk("%s: cs_fe_fwdrslt_del_by_idx(index=%d) return failed!! ret=%d\n", __func__, fwd_rslt_idx, ret);
        }

	g_fwd_result[index].used = 0;
	spin_unlock(&g_lpm_lock);	
        return CS_E_OK;
}

cs_status_t cs_fwd_result_delete_entry_by_index(cs_dev_id_t device_id, cs_uint32_t index)
{
	int ret;

	ret = cs_fwd_result_delete_by_index(device_id, index);

	spin_lock(&g_lpm_lock);	

        g_fwd_result_count--;
	spin_unlock(&g_lpm_lock);	
	return ret;
}
EXPORT_SYMBOL(cs_fwd_result_delete_entry_by_index);

/* update L2 MAC entry */
cs_status_t cs_fwd_result_update_l2_mac_entry(cs_uint32_t fwd_result_index, char *source_mac)
{
	int ret;
	cs_uint32_t new_mac_idx;
	cs_uint8_t tmp_buf[CS_ETH_ADDR_LEN * 2];
        cs_uint8_t *ptr;
	fe_fwd_result_entry_t fwd_rslt_entry, *p_fwd_rslt_entry=&fwd_rslt_entry;
	cs_fwd_result_t fwd_result, *p_fwd_result = &fwd_result;

	printk("%s: fwd_result_index=%d, source_mac=0x%x\n", __func__, fwd_result_index,(unsigned int) source_mac);
	printk("%s: source_mac[0-5]=0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n", __func__,
		source_mac[0], source_mac[1], source_mac[2], source_mac[3], source_mac[4], source_mac[5]);
	ret = cs_fwd_result_get_by_index(1, fwd_result_index, p_fwd_result);
 	if (ret != CS_E_OK) {
                printk("%s: cs_fwd_result_get_by_index(index=%d)\n", __func__, fwd_result_index);
                return CS_E_ERROR;
        }

	if (p_fwd_result->mac_type != L2_LOOKUP_TYPE_SA && p_fwd_result->mac_type != L2_LOOKUP_TYPE_PAIR) {
		printk("%s: Wrong mac_type = %d!!\n", __func__, p_fwd_result->mac_type);
		return CS_ERROR;
	}
        if (p_fwd_result->mac_type == L2_LOOKUP_TYPE_PAIR) {
        	memcpy(&tmp_buf[0], &(p_fwd_result->dest_mac[0]), CS_ETH_ADDR_LEN);
                memcpy(&tmp_buf[CS_ETH_ADDR_LEN], source_mac, CS_ETH_ADDR_LEN);
                ptr = &tmp_buf[0];
        }
        else	/* L2_LOOKUP_TYPE_SA */
                ptr = source_mac;

        ret = cs_fe_l2_result_alloc(ptr, p_fwd_result->mac_type, &new_mac_idx);
        if (ret != FE_TABLE_OK) {
       		printk("%s: cs_fe_l2_result_alloc(mac_type=%d) return failed!! ret=%d\n", __func__, p_fwd_result->mac_type, ret);
                return CS_E_ERROR;
	}
	printk("%s: new_mac_idx=%d\n", __func__, new_mac_idx);

	ret = cs_fe_table_get_entry(FE_TABLE_FWDRSLT, p_fwd_result->fwd_rslt_idx, p_fwd_rslt_entry);
	if (ret != FE_TABLE_OK) {
		cs_fe_l2_result_dealloc(new_mac_idx, p_fwd_result->mac_type);	
		printk("%s: cs_fe_table_get_entry(FE_TABLE_FWDRSLT, index=%d) failed!!, ret=%d\n", __func__, p_fwd_result->fwd_rslt_idx, ret);
		return FE_TABLE_OK;
	}
	p_fwd_rslt_entry->l2.l2_index = new_mac_idx;
	ret = cs_fe_table_set_entry(FE_TABLE_FWDRSLT, p_fwd_result->fwd_rslt_idx, p_fwd_rslt_entry);
	if (ret != FE_TABLE_OK) {
                cs_fe_l2_result_dealloc(new_mac_idx, p_fwd_result->mac_type);
                printk("%s: cs_fe_table_set_entry(FE_TABLE_FWDRSLT, index=%d) failed!!, ret=%d\n", __func__, p_fwd_result->fwd_rslt_idx, ret);
                return FE_TABLE_OK;
        }
	ret = cs_fe_l2_result_dealloc(p_fwd_result->mac_idx, p_fwd_result->mac_type);	
	if (ret != CS_E_OK) {
		printk("%s: cs_fe_l2_result_dealloc(mac_idx=%d, mac_type=%d) failed!!, ret=%d\n", 
			__func__, p_fwd_result->mac_idx, p_fwd_result->mac_type, ret); 	
	}
	p_fwd_result->mac_idx = new_mac_idx;
	g_fwd_result[fwd_result_index].fwd_result = fwd_result;
	return CS_E_OK;
}
EXPORT_SYMBOL(cs_fwd_result_update_l2_mac_entry);
                                                                        
cs_status_t cs_lpm_add(cs_dev_id_t device_id, cs_lpm_t *lpm)
{
	int i, ret;
	cs_uint32_t lpm_index;
	lpm_entity_info_t entity, *p_entity = &entity;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		printk("%s: device_id=%d, lpm=0x%x\n", __func__, device_id, (unsigned int)lpm);

		printk("%s: lpm->app_type=%d\n", __func__, lpm->app_type);	
		printk("%s: lpm->fwd_rslt_idx=%d\n", __func__, lpm->fwd_rslt_idx);	
		printk("%s: lpm->prefix.afi=%d, lpm->prefix.ip_addr.ipv4_addr=0x%x, lpm->prefix.addr_len=%d\n", 
			__func__, lpm->prefix.afi, lpm->prefix.ip_addr.ipv4_addr, lpm->prefix.addr_len);
	}
#endif
	if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {
		if (g_lpm[i].used == 0) {
#ifdef CONFIG_CS752X_PROC
        		if (cs_adapt_debug & CS752X_ADAPT_LPM) {
				printk("%s: found a free entry of g_lpm[] at index %d\n", __func__, i);
			}
#endif
			break; 
		}
	}
	if (i == FE_LPM_ENTRY_MAX) {
		printk("%s: No free entry of g_lpm[] found!!\n", __func__);
		spin_unlock(&g_lpm_lock);
                return CS_E_ERROR;
	}

	memset(p_entity, 0, sizeof(lpm_entity_info_t));	
	p_entity->is_empty = 0;	
	p_entity->is_v6 = lpm->prefix.afi;	
	p_entity->index = 0;
	p_entity->mask = lpm->prefix.addr_len;
	p_entity->priority = 0;
	p_entity->hash_index = lpm->fwd_rslt_idx;
        p_entity->hit_count = 0;

	/* IPv6 should convert addr[0] to addr[3], addr[1] to addr[2], addr[2] to addr[1], addr[3] to addr[0] */
	if (p_entity->is_v6 == CS_IPV6) {
		p_entity->ip_u.addr32[0] = lpm->prefix.ip_addr.ipv6_addr[3];
		p_entity->ip_u.addr32[1] = lpm->prefix.ip_addr.ipv6_addr[2];
		p_entity->ip_u.addr32[2] = lpm->prefix.ip_addr.ipv6_addr[1];
		p_entity->ip_u.addr32[3] = lpm->prefix.ip_addr.ipv6_addr[0];
	} 
	else {
		p_entity->ip_u.addr32[0] = lpm->prefix.ip_addr.ipv4_addr;
	}
	
	//if ((ret = cs_lpm_add_entry(p_entity)) != FE_TABLE_OK)
	if ((ret = cs_lpm_range_add_entry(p_entity->is_v6, 1, 64, p_entity)) != FE_TABLE_OK)
	{
		printk("%s: cs_lpm_range_add_entry() return failed!! ret=%d\n", __func__, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
	}
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		cs_lpm_print_entry(p_entity->index);
	}
#endif
	
	g_lpm[i].used = 1;
	g_lpm[i].lpm = *lpm;
	g_lpm[i].lpm_idx = p_entity->index;


	//Update all of the index because the LPM entries are re-sorted
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		printk("%s: g_lpm_count=%d\n", __func__, g_lpm_count);
	}
#endif

	for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {

		if (g_lpm[i].used == 0) {
			continue;
		}
		lpm = &g_lpm[i].lpm;

		spin_unlock(&g_lpm_lock);	
		ret = cs_lpm_get(device_id, lpm, &lpm_index);
		spin_lock(&g_lpm_lock);	
			
		if (ret == CS_E_ERROR)
			printk("%s: Get lpm index=%d failed!!\n", __func__, i);
		else {
#ifdef CONFIG_CS752X_PROC
        		if (cs_adapt_debug & CS752X_ADAPT_LPM) {
				printk("%s: g_lpm[%d].lpm_idx=%d\n", __func__, i, g_lpm[i].lpm_idx);
				printk("%s: lpm->app_type=%d\n", __func__, lpm->app_type);
       				printk("%s: lpm->fwd_rslt_idx=%d\n", __func__, lpm->fwd_rslt_idx);
       				printk("%s: lpm->prefix.afi=%d, lpm->prefix.ip_addr.ipv4_addr=0x%x, lpm->prefix.addr_len=%d\n",
               				__func__, lpm->prefix.afi, lpm->prefix.ip_addr.ipv4_addr, lpm->prefix.addr_len);
			}
#endif
		}
	}

        g_lpm_count++;
	spin_unlock(&g_lpm_lock);	
	
	/* write to hardware */
	cs_lpm_swap_active_table();

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_lpm_add);

cs_status_t cs_lpm_get(cs_dev_id_t device_id, cs_lpm_t *lpm, cs_uint32_t *lpm_index)
{
        int ret;
        lpm_entity_info_t entity, *p_entity = &entity;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
        	printk("%s: device_id=%d, lpm=0x%x\n", __func__, device_id, (unsigned int)lpm);

		printk("%s: lpm->app_type=%d\n", __func__, lpm->app_type);
        	printk("%s: lpm->fwd_rslt_idx=%d\n", __func__, lpm->fwd_rslt_idx);
        	printk("%s: lpm->prefix.afi=%d, lpm->prefix.ip_addr.ipv4_addr=0x%x, lpm->prefix.addr_len=%d\n",
                	__func__, lpm->prefix.afi, lpm->prefix.ip_addr.ipv4_addr, lpm->prefix.addr_len);
	}
#endif
	if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }


        if (lpm == NULL) {
                printk("%s: lpm == NULL!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	memset(p_entity, 0, sizeof(lpm_entity_info_t));	
        p_entity->is_empty = 0;
        p_entity->is_v6 = lpm->prefix.afi;
        memcpy(&(p_entity->ip_u.addr8[0]), &(lpm->prefix.ip_addr), sizeof(p_entity->ip_u.addr8));
        p_entity->index = 0;
        p_entity->mask = lpm->prefix.addr_len;
        p_entity->priority = 0;
        p_entity->hash_index = 0;
        p_entity->hit_count = 0;

	ret = cs_lpm_search_entry(p_entity);
	if (ret == FE_TABLE_EOUTRANGE) {
                printk("%s: cs_lpm_search_entry() return failed!! ret=%d\n", __func__, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		printk("%s: entry found at p_entity->index=%d, ret=%d\n", __func__, p_entity->index, ret);
        	cs_lpm_print_entry(p_entity->index);
	}
#endif
		
        *lpm_index = p_entity->index;

	spin_unlock(&g_lpm_lock);	

        return CS_E_OK;
}
EXPORT_SYMBOL(cs_lpm_get);

cs_status_t cs_lpm_get_by_index(cs_dev_id_t device_id, cs_uint32_t index, cs_lpm_t *lpm)
{
        int i, ret;
        lpm_entity_info_t entity, *p_entity = &entity;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
		printk("%s: index=%d, g_lpm_count=%d, lpm=0x%x\n", __func__, index, g_lpm_count, (unsigned int)lpm);
	}
#endif

	if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	if (index >= FE_LPM_ENTRY_MAX) {
		printk("%s: index=%d exceed FE_LPM_ENTRY_MAX\n", __func__, index);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
	}

        if (lpm == NULL) {
                printk("%s: lpm == NULL!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	if (g_lpm[index].used == 0) {
		printk("%s: entry index %d of g_lpm[] is not a used entry!!\n", __func__, index);
                spin_unlock(&g_lpm_lock);
                return CS_E_ERROR;
	}

	lpm = &g_lpm[index].lpm;
	spin_unlock(&g_lpm_lock);	
	
	ret = cs_lpm_get_entity_by_idx(g_lpm[index].lpm_idx, p_entity);
	if (ret != FE_TABLE_OK) {
                printk("%s: cs_lpm_get_entity_by_idx() return failed!! ret=%d\n", __func__, ret);
                return CS_E_ERROR;
        }

#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {

        	printk("%s: LPM entry found at p_entity->index=%d, ret=%d\n", __func__, p_entity->index, ret);
        	cs_lpm_print_entry(p_entity->index);

		spin_lock(&g_lpm_lock);	
		for (i = 0; i < FE_LPM_ENTRY_MAX; i++)
		{
			if (g_lpm[i].used == 0) {
				continue;
			}
			lpm = &g_lpm[i].lpm;
                	printk("%s: device_id=%d, lpm=0x%x\n", __func__, device_id, (unsigned int)lpm);
                	printk("%s: lpm->app_type=%d\n", __func__, lpm->app_type);
                	printk("%s: lpm->fwd_rslt_idx=%d\n", __func__, lpm->fwd_rslt_idx);
                	printk("%s: lpm->prefix.afi=%d, lpm->prefix.ip_addr.ipv4_addr=0x%x, lpm->prefix.addr_len=%d\n",
                        	__func__, lpm->prefix.afi, lpm->prefix.ip_addr.ipv4_addr, lpm->prefix.addr_len);
		}
		spin_unlock(&g_lpm_lock);	
        }
#endif
        return CS_E_OK;
}
EXPORT_SYMBOL(cs_lpm_get_by_index);

cs_status_t cs_lpm_delete(cs_dev_id_t device_id, cs_lpm_t *lpm)
{
        int i, j, ret;
	cs_uint32_t lpm_index;

	spin_lock(&g_lpm_lock);	
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
        	printk("%s: device_id=%d, lpm=0x%x\n", __func__, device_id, (unsigned int)lpm);

        	printk("%s: lpm->app_type=%d\n", __func__, lpm->app_type);
        	printk("%s: lpm->fwd_rslt_idx=%d\n", __func__, lpm->fwd_rslt_idx);
        	printk("%s: lpm->prefix.afi=%d, lpm->prefix.ip_addr.ipv4_addr=0x%x, lpm->prefix.addr_len=%d\n",
                	__func__, lpm->prefix.afi, lpm->prefix.ip_addr.ipv4_addr, lpm->prefix.addr_len);
	}
#endif

	if (g_lpm_init == 0) {
                printk("%s: LPM module has not been initialized!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {
		if (g_lpm[i].used == 0) {
			continue;
		}
#ifdef CONFIG_CS752X_PROC
        	if (cs_adapt_debug & CS752X_ADAPT_LPM) {
	 		printk("%s: i=%d\n", __func__, i);
		 	printk("%s: g_lpm[i].lpm.app_type=%d\n", __func__, g_lpm[i].lpm.app_type);
		 	printk("%s: g_lpm[i].lpm.fwd_rslt_idx=%d\n", __func__, g_lpm[i].lpm.fwd_rslt_idx);
        	 	printk("%s: g_lpm[i].lpm.prefix.afi=%d, g_lpm[i].lpm.prefix.ip_addr.ipv4_addr=0x%x, g_lpm[i].lpm.prefix.addr_len=%d\n",
                		__func__, g_lpm[i].lpm.prefix.afi, g_lpm[i].lpm.prefix.ip_addr.ipv4_addr, g_lpm[i].lpm.prefix.addr_len);
        	 	printk("%s: g_lpm[i].lpm_idx=%d\n", __func__, g_lpm[i].lpm_idx);
		}
#endif

		 if (memcmp(lpm, &g_lpm[i].lpm, sizeof(cs_lpm_t)) == 0) {
			printk("%s: the lpm entry is found at index=%d\n", __func__, i);
			break;
		}	
	}
	if (i == FE_LPM_ENTRY_MAX) {
		printk("%s: the lpm entry is not found!!\n", __func__);
		spin_unlock(&g_lpm_lock);	
		return CS_E_OK;
	}

	ret = cs_lpm_delete_index(g_lpm[i].lpm_idx);
        if (ret != FE_TABLE_OK) {
                printk("%s: cs_lpm_delete_idx(g_lpm[%d].lpm_idx=%d) return failed!! ret=%d\n", __func__, i, g_lpm[i].lpm_idx, ret);
		spin_unlock(&g_lpm_lock);	
                return CS_E_ERROR;
        }

	g_lpm[i].used = 0;
	g_lpm_count--;

        /* Update all of the index because the LPM entries are sorted */
#ifdef CONFIG_CS752X_PROC
        if (cs_adapt_debug & CS752X_ADAPT_LPM) {
        	printk("%s: g_lpm_count=%d\n", __func__, g_lpm_count);
	}
#endif
        for (i = 0; i < FE_LPM_ENTRY_MAX; i++) {

		if (g_lpm[i].used == 0) {
			continue;
		}

                lpm = &g_lpm[i].lpm;

		spin_unlock(&g_lpm_lock);	
                ret = cs_lpm_get(device_id, lpm, &lpm_index);
		spin_lock(&g_lpm_lock);	
	
		g_lpm[i].lpm_idx = lpm_index;	
			
                if (ret == CS_E_ERROR) {
                        printk("%s: Get lpm index=%d failed!!\n", __func__, i);
		}
                else {
//#ifdef CONFIG_CS752X_PROC
        		if (cs_adapt_debug & CS752X_ADAPT_LPM) {
                              	printk("%s: g_lpm[%d].lpm_idx=%d\n", __func__, i, g_lpm[i].lpm_idx);
			}
//#endif
		}
        }
	spin_unlock(&g_lpm_lock);	

	/* write to hardware */
        cs_lpm_swap_active_table();

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_lpm_delete);

