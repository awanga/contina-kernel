/***********************************************************************/
/* This file contains unpublished documentation and software           */
/* proprietary to Cortina Systems Incorporated. Any use or disclosure, */
/* in whole or in part, of the information in this file without a      */
/* written consent of an officer of Cortina Systems Incorporated is    */
/* strictly prohibited.                                                */
/* Copyright (c) 2010 by Cortina Systems Incorporated.                 */
/***********************************************************************/

#ifndef CS752X_CORE_FASTNET_H
#define CS752X_CORE_FASTNET_H

#define HASH_FLAG_FASTNET 0x9
#define IS_FASTNET_ENTRY(aaa) (((aaa) & 0xF) == HASH_FLAG_FASTNET)
#define HASH_INDEX_SW2FASTNET(aaa) ((aaa) >> 4)
#define HASH_INDEX_FASTNET2SW(aaa) (((aaa) << 4) | (HASH_FLAG_FASTNET))

#define CS_FASTNET_HASH_MAX 65536
#define CS_FASTNET_DST_DEV_MAX 4
#define CS_FASTNET_SKB_DBCHECK_MAX 32

int cs_core_fastnet_init(void);
int cs_core_fastnet_exit(void);
int cs_core_fastnet_add_fwd_hash(struct sk_buff *skb);
int cs_core_fastnet_fast_xmit(struct sk_buff *skb);
int cs_core_fastnet_del_fwd_hash(u32 hash_idx);
void cs_core_fastnet_print_table_used_count(void);

#endif /* CS752X_CORE_FASTNET_H */
