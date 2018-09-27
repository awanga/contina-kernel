
#ifndef __CS_HW_ARP_H__
#define __CS_HW_ARP_H__

void cs_neigh_delete(void *data);
void cs_neigh_update_used(void *data);

void cs_hw_arp_init(void);
void cs_hw_arp_exit(void);

#endif /* __CS_HW_ARP_H__ */
