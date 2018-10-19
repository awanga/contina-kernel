#ifndef __CS_FE_UTIL_H__
#define __CS_FE_UTIL_H__

#include <linux/types.h>

/* L2 MAC result table assistence APIs */

/*
 * notes: when inserting both MAC_DA and MAC_SA at the same time,
 * MAC_DA must be the first 6 bytes passed in with *mac_addr.
 */
typedef enum {
	L2_LOOKUP_TYPE_DA,
	L2_LOOKUP_TYPE_SA,
	L2_LOOKUP_TYPE_PAIR,
	L2_LOOKUP_TYPE_MAX
} fe_l2_rslt_lookup_type_e;

int cs_fe_l2_result_alloc(unsigned char *mac_addr, unsigned char type,
		unsigned int *return_idx);
int cs_fe_l2_result_dealloc(unsigned int l2_idx, unsigned char type);

/* L3 IP resource table assistence APIs */

int cs_fe_l3_result_alloc(__u32 *ip_addr, bool is_v6, unsigned int *return_idx);
int cs_fe_l3_result_dealloc(__u16 l3_idx);

/* FWDRSLT resource table assistence API */
int cs_fe_fwdrslt_del_by_idx(unsigned int idx);

/* helper function of AN BNG MAC table */
int cs_fe_an_bng_mac_get_port_by_mac(unsigned char *mac_addr, __u8 *pspid);

/* helper function of LPB table */
int cs_fe_lpb_get_lspid_by_pspid(__u8 pspid, __u8 *lspid);

#endif /* __CS_FE_UTIL_H__ */
