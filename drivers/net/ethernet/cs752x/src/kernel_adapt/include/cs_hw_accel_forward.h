#ifndef CS_KERNEL_FORWARD_HW_ACC_SUPPORT
#define CS_KERNEL_FORWARD_HW_ACC_SUPPORT 1

#include <linux/skbuff.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_nat.h>
#include <linux/list.h>

#define MAX_PORT_LIST_SIZE		32

// Initialization
#define BYPASS_LIST_TUPE_UDP			(0)
#define BYPASS_LIST_TUPE_TCP			(1)
#define BYPASS_LIST_TUPE_EXPECTED_MASTER	(2)
#define PRIORITY_LIST_TUPE_UDP			(3)
#define PRIORITY_LIST_TUPE_TCP			(4)
#define ALLOW_LOCALIN_TUPE				(5)

int cs_forward_init(void);
int cs_forward_exit(void);
void cs_forward_port_list_dump(int list_type);
int cs_forward_port_list_add(int list_type, unsigned short port);
int cs_forward_port_list_del(int list_type, unsigned short port);
u16* cs_forward_port_list_get(int list_type);

int cs_localout_check_allow_localout_port(u16 hdr_source, u16 hdr_dest);

#endif

