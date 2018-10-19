#include "cs_fe.h"
//#include "cs_fe_util_api.h"

#if 0
int cs_fe_set_qos_mapping_table(int8_t i_pri, int8_t i_dscp,
		int8_t e_pri, int8_t e_dscp)
{
	// FIXME implement later!
#if 0
		unsigned int index;
		if (i_pri<0 && i_dscp<0) {
				return CS_OK;
		}
		if (i_pri<0)
				i_pri = 0;
		if (i_dscp<0)
				i_dscp = 0;

		index = ((i_pri & (_8021P_MASK)) << 6) | (i_dscp & DSCP_MASK);
		spin_lock(&qos_mapping_table_lock);
		if (e_dscp >= 0)
				qos_mapping_table[index].dscp = (e_dscp & DSCP_MASK) | QOS_ENTRY_VALI
D_MASK;
		else
				qos_mapping_table[index].dscp = 0;
		if (e_pri >= 0)
				qos_mapping_table[index]._8021_p = (e_pri & _8021P_MASK) | QOS_ENTRY_
VALID_MASK;
		else
				qos_mapping_table[index]._8021_p = 0;
		spin_unlock(&qos_mapping_table_lock);

		// Add tuple7 result, hash entry and hash mask here?
		cs_add_qos_tuple7_entries(i_pri, i_dscp, e_pri, e_dscp);
#endif
	return 0;
} /* cs_fe_set_qos_mapping_table */

EXPORT_SYMBOL(cs_fe_set_qos_mapping_table);

int cs_fe_get_qos_mapping_result(int8_t i_pri, int8_t i_dscp,
		int8_t *e_pri, int8_t *e_dscp)
{
	// FIXME!! copy from original code later
#if 0
		unsigned int index = ((i_pri & (_8021P_MASK)) << 6) | (i_dscp & DSCP_MASK);

		spin_lock(&qos_mapping_table_lock);
		*e_pri = qos_mapping_table[index]._8021_p;
		*e_dscp= qos_mapping_table[index].dscp;
		spin_unlock(&qos_mapping_table_lock);
#endif
		return 0;
}
EXPORT_SYMBOL(cs_fe_get_qos_mapping_result);

int cs_fe_print_qos_mapping_table(void)
{
	// FIXME!! copy from original code later
#if 0
	unsigned int index;
	qos_egress_result_s *entry = qos_mapping_table;
	printk("*** QOS Mapping Table ***\n");
	for (index = 0; index<QOS_MAPPING_TABLE_SIZE; index++) {
		if (entry->_8021_p & QOS_ENTRY_VALID_MASK) {
			printk("entry %04d, 8021p %04x, DSCP %06x\n",
					index, entry->_8021_p & (~QOS_ENTRY_VALID_MASK), entry->dscp);
		}
		entry++;
	}
#endif
	return 0;
}
EXPORT_SYMBOL(cs_fe_print_qos_mapping_table);
#endif

