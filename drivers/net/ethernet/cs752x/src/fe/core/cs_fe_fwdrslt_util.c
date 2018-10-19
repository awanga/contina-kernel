#include "cs_fe.h"
#include "cs_fe_util_api.h"

#if 0
int cs_fe_add_fwdrslt_from_hash_rslt(fe_hash_rslt_s *result)
{
	/* FIXME!! implement */
	return 0;
} /* cs_fe_add_fwdrslt */
#endif

#if defined(CONFIG_CS75XX_DOUBLE_CHECK)
extern int cs_core_double_chk_dsbl(u16 fwdrslt_index);
extern u32 cs_fe_double_chk;
#endif

int cs_fe_fwdrslt_del_by_idx(unsigned int idx)
{
	fe_fwd_result_entry_t fwdrslt_entry;
	int ret, last_ret = 0;
	unsigned char l2_type;

	ret = cs_fe_table_get_entry(FE_TABLE_FWDRSLT, idx, &fwdrslt_entry);
	if (ret != FE_TABLE_OK)
		return ret;

	/* delete used MAC_SA and/or MAC_DA L2 index */
	if ((fwdrslt_entry.l2.mac_sa_replace_en == 1) ||
			(fwdrslt_entry.l2.mac_da_replace_en == 1)) {
		if ((fwdrslt_entry.l2.mac_sa_replace_en == 1) &&
				(fwdrslt_entry.l2.mac_da_replace_en == 1))
			l2_type = L2_LOOKUP_TYPE_PAIR;
		else if (fwdrslt_entry.l2.mac_sa_replace_en == 1)
			l2_type = L2_LOOKUP_TYPE_SA;
		else /* if (fwdrslt_entry.l2.mac_da_replace_en == 1) */
			l2_type = L2_LOOKUP_TYPE_DA;
		ret = cs_fe_l2_result_dealloc(fwdrslt_entry.l2.l2_index,
				l2_type);
		if (ret != 0) {
			printk("%s:unable to dealloc L2 idx %d, type %d\n",
					__func__, fwdrslt_entry.l2.l2_index,
					l2_type);
			// FIXME! any debugging?
		}
		last_ret |= ret;
	}

	/* delete used IP_SA and/or IP_DA L3 index */
	if (fwdrslt_entry.l3.ip_sa_replace_en == 1) {
		ret = cs_fe_l3_result_dealloc(fwdrslt_entry.l3.ip_sa_index);
		if (ret != 0) {
			printk("%s:unable to dealloc L3 IP SA idx %d\n",
					__func__, fwdrslt_entry.l3.ip_sa_index);
			// FIXME! any debugging?
		}
		last_ret |= ret;
	}

	if (fwdrslt_entry.l3.ip_da_replace_en == 1) {
		ret = cs_fe_l3_result_dealloc(fwdrslt_entry.l3.ip_da_index);
		if (ret != 0) {
			printk("%s:unable to dealloc L3 IP DA idx %d\n",
					__func__, fwdrslt_entry.l3.ip_da_index);
			// FIXME! any debugging?
		}
		last_ret |= ret;
	}

	/* delete used voqpol index */
	if (fwdrslt_entry.dest.voq_pol_table_index != 0) {
		ret = cs_fe_table_del_entry_by_idx(FE_TABLE_VOQ_POLICER,
				fwdrslt_entry.dest.voq_pol_table_index, false);
		if (ret != 0) {
			printk("%s:unable to delete voqpol idx %d\n", __func__,
					fwdrslt_entry.dest.voq_pol_table_index);
			// FIXME! any debugging?
		}
		last_ret |= ret;
	}

	/* delete flow VLAN if it is used */
	if (fwdrslt_entry.l2.flow_vlan_op_en == 1) {
		ret = cs_fe_table_del_entry_by_idx(FE_TABLE_FVLAN,
				fwdrslt_entry.l2.flow_vlan_index, false);
		if (ret != 0) {
			printk("%s:unable to L2 Flow VLAN idx %d\n", __func__,
					fwdrslt_entry.l2.flow_vlan_index);
			// FIXME! any debugging?
		}
		last_ret |= ret;
	}

#if defined(CONFIG_CS752X_PROC) && defined(CONFIG_CS75XX_DOUBLE_CHECK)
	if (cs_fe_double_chk > 0)
		cs_core_double_chk_dsbl(idx);
#endif

	/* delete the fwdrslt index */
	ret = cs_fe_table_del_entry_by_idx(FE_TABLE_FWDRSLT, idx, false);
	if (ret != 0)
		printk("%s:unable to Forwarding result idx %d\n", __func__,
				idx);
	last_ret |= ret;

	return last_ret;
} /* cs_fe_fwdrslt_del_by_idx */

