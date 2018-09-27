
#ifndef __CS_HW_NF_DROP_H__
#define __CS_HW_NF_DROP_H__

void cs_hw_nf_drop_handler(struct sk_buff *skb);

void cs_hw_nf_drop_check_table_clean(void);
void cs_hw_nf_drop_check_table_dump(void);	

int cs_hw_nf_drop_init(void);
void cs_hw_nf_drop_exit(void);

#endif /* __CS_HW_NF_DROP_H__ */
