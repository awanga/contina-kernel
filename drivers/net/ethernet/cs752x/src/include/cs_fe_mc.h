#ifndef __CS_FE_MC_H__
#define __CS_FE_MC_H__

#include <mach/cs_types.h>
#include <linux/spinlock.h>

/* #define CS_FE_MC_DEBUG	1 */

/* 
 * MCGID [8:0] = Mode [8] + VTable ID [7:5] + real MCGID [4:0]
 * Allow adjustment to number of bits for MCGID/VTABLE
 */
#define MCGID_BITS 8
#define MCGID_VTABLE_BITS 3
#define MCGID_GROUP_BITS (MCGID_BITS - MCGID_VTABLE_BITS)

#define MAX_MCGID_VTABLE (1 << MCGID_VTABLE_BITS)
#define MAX_MCGID (1 << MCGID_GROUP_BITS)
#define MCG_INCREMENT (1 << MCGID_GROUP_BITS)
#define MCAL_TABLE_SIZE 256

#define MCGID(mode, vtable_id, group_id) ((mode << MCGID_BITS) | \
	(vtable_id << MCGID_GROUP_BITS) | group_id)
#define MCG_VTABLE_ID(mcgid) ((mcgid & 0xFF) >> MCGID_GROUP_BITS)
#define MCG_GROUP_ID(mcgid) (mcgid & (MAX_MCGID - 1))
#define MCG_MODE(mcgid) (mcgid >> MCGID_BITS)

/* port replication mode with no copy */
#define MCG_INIT_MCGID 0x100

#define IS_PORT_REPLICATION_MODE(mcgid) (mcgid > MCG_INIT_MCGID ? 1 : 0)
#define IS_ARBITRARY_REPLICATION_MODE(mcgid) (mcgid < MCG_INIT_MCGID ? 1 : 0)

/* Flags used in struct cs_fe_mcg_resource_info */
#define CS_MCGID_VTABLE_INVALID 0
#define CS_MCGID_VTABLE_VALID 1
#define CS_MCGID_GROUP_INVALID 0
#define CS_MCGID_GROUP_VALID 1

/*
 * When considering one VoQ per port, 
 * we define below bit-map for replication mode of MCGID
 *
 * When considering one VoQ per VLAN,
 * we ignore below definition and need more flexible mapping.
 */
#define CS_FE_MC_PORT_GE0	(1 << 0)
#define CS_FE_MC_PORT_GE1	(1 << 1)
#define CS_FE_MC_PORT_GE2	(1 << 2)
#define CS_FE_MC_PORT_CPU	(1 << 3)
#define CS_FE_MC_PORT_CCORE	(1 << 4)
#define CS_FE_MC_PORT_ECORE	(1 << 5)
#define CS_FE_MC_PORT_MCAL	(1 << 6)
#define CS_FE_MC_PORT_MIRROR	(1 << 7)
#define CS_FE_MC_PORT_MAX	0xFF
#define CS_FE_MC_ARBITRARY_MAX	0xFFFF

struct cs_fe_mcg_resource_info {
	unsigned int vtable_usage[MAX_MCGID_VTABLE];
	unsigned int group_usage[MAX_MCGID_VTABLE][MAX_MCGID];
	unsigned int mc_vec[MCAL_TABLE_SIZE];
	spinlock_t lock;
};

#define CS_MC_STA_ENTRY_SIZE  5

// EGRESS PORT ID DEFINITION, cs_mc_sta_mac_entry.egress_port_id
#define CS_EGRESS_PORT_GMAC_0   0x00
#define CS_EGRESS_PORT_GMAC_1   0x01
#define CS_EGRESS_PORT_GMAC_2   0x02
#define CS_EGRESS_PORT_STA1     0x03
#define CS_EGRESS_PORT_STA2     0x04
#define CS_EGRESS_PORT_STA3     0x05
#define CS_EGRESS_PORT_STA4     0x06
#define CS_EGRESS_PORT_STA5     0x07


typedef enum {
    CS_MC_MAC_ENTRY_INVALID,
    CS_MC_MAC_ENTRY_VALID,
} cs_mc_sta_mac_entry_state_e;


typedef enum {
    CS_MC_MAC_ENTRY_STATUS_SUCCESS,
    CS_MC_MAC_ENTRY_STATUS_FAIL,
    CS_MC_MAC_ENTRY_STATUS_EXIST,
    CS_MC_MAC_ENTRY_STATUS_FULL,
    CS_MC_MAC_ENTRY_STATUS_MAX
} cs_mc_sta_mac_entry_status_e;


typedef struct cs_mc_sta_mac_entry {
	unsigned char mac_da[6];	        // Station MAC DA
	cs_mc_sta_mac_entry_state_e status;	// Reference cs_mc_sta_mac_entry_state_e
	unsigned char egress_port_id;	    // Egress Port ID for core logic
} cs_mc_sta_mac_entry_s;


//void cs_wfo_mc_mac_table_init(void);
int cs_wfo_mc_mac_table_add_entry(unsigned char* mac_da, unsigned char* p_egress_port_id);
int cs_wfo_mc_mac_table_del_entry(unsigned char* mac_da);
void cs_wfo_mc_mac_table_del_all_entry(void);
cs_mc_sta_mac_entry_s* cs_wfo_mc_mac_table_get_entry(unsigned char* mac_da);



int cs_fe_mcg_init(void);

int cs_fe_allocate_mcg_vtable_id(int *vtable_id, int forced);
int cs_fe_free_mcg_vtable_id(int vtable_id);

int cs_fe_allocate_mcg_group_id(int vtable_id, int *group_id);
int cs_fe_free_mcg_group_id(int vtable_id, int group_id);

int cs_fe_set_mcg_mc_vec(int vtable_id, int group_id, int mc_vec);
int cs_fe_get_mcg_mc_vec(int vtable_id, int group_id, int *mc_vec);

int cs_fe_allocate_port_rep_mcgid(int *id, int port_map);

#endif
