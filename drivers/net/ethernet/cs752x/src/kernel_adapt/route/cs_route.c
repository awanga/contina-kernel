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
#include <cs_core_vtable.h>

#include <mach/cs_lpm_api.h>
#include <mach/cs_route_api.h>

#ifdef CONFIG_CS752X_PROC
#include <cs752x_proc.h>
extern u32 cs_adapt_debug;
#endif

typedef struct cs_l3_nexthop_internal_s {
	cs_int32_t 		used;
	cs_int32_t		ref_cnt;
	cs_uint32_t 		fwd_result_index;
	cs_uint16_t		sa_id; 			/* used for ipsec rekey */
	cs_l3_nexthop_t         nexthop;
} cs_l3_nexthop_internal_t;

typedef struct cs_l3_route_internal_s {
	cs_int32_t 		used;
	cs_ip_address_t         prefix;
	cs_l3_nexthop_internal_t         *p_internal_nexthop;
} cs_l3_route_internal_t;

static cs_l3_nexthop_internal_t g_l3_nexthop[CS_IPV4_ROUTE_MAX];
static cs_l3_route_internal_t g_l3_route[CS_IPV4_ROUTE_MAX];
static cs_uint8_t g_l3_nexthop_count = 0;
static cs_uint8_t g_l3_route_count = 0;
static cs_uint8_t g_route_inited = 0;
static cs_uint32_t g_nat_subnet_intf_id = 0;
static cs_ip_address_t g_nat_subnet = {0};

static spinlock_t g_route_lock;

static void cs_l3_ip_address_htonl(cs_ip_address_t *p_dest, cs_ip_address_t *p_src);
static cs_uint32_t cs_l3_route_gen_ip_mask_v4(cs_uint8_t mask);
static cs_boolean_t cs_l3_is_zero_ip_addr(cs_ip_address_t *ip_addr);

cs_status_t  (*g_fp_callback_cs_l3_nexthop_attr_set)(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop) = NULL;
cs_status_t  (*g_fp_callback_cs_l3_nexthop_attr_get)(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop) = NULL;

/* The following two function should be provided by system vendors */
cs_status_t system_vendor_cs_l3_nexthop_attr_set(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop)
{
	return CS_E_OK;
}

cs_status_t system_vendor_cs_l3_nexthop_attr_get(cs_dev_id_t device_id, cs_l3_nexthop_t	*nexthop)
{
	return CS_E_OK;
}

/* callback routine for IPSEC/PPTP tunnels */
cs_status_t cs_l2tp_ipsec_set_src_mac(cs_l3_nexthop_t *nexthop, char *src_mac, cs_uint16_t *sa_id);
cs_status_t cs_pptp_set_src_mac(cs_l3_nexthop_t *nexthop, char *src_mac);
cs_status_t cs_ip_translate_set_src_mac(cs_l3_nexthop_t *nexthop, char *src_mac);


/* IP selectors check for IPSEC tunnel */
cs_status_t cs_ipsec_sel_ip_check(cs_uint32_t tunnel_id, cs_ip_address_t *ip_prefix, cs_boolean_t *is_valid);

cs_status_t cs_l3_route_init_0(cs_dev_id_t device_id)
{
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: device_id=%d\n", __func__, device_id);
	}
#endif
	if (g_route_inited == 0)
	{
		spin_lock_init(&g_route_lock);

		memset(&(g_l3_nexthop[0]), 0, sizeof(g_l3_nexthop));
		memset(&(g_l3_route[0]), 0, sizeof(g_l3_route));
		g_l3_nexthop_count = 0;
		g_l3_route_count = 0;
		g_nat_subnet_intf_id = 0;
		memset(&g_nat_subnet, 0, sizeof(g_nat_subnet));

		g_fp_callback_cs_l3_nexthop_attr_set = system_vendor_cs_l3_nexthop_attr_set;
		g_fp_callback_cs_l3_nexthop_attr_get = system_vendor_cs_l3_nexthop_attr_get;
		cs_lpm_fwd_result_init(device_id);
		g_route_inited = 1;
	}

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_route_init_0);

cs_status_t cs_l3_route_shut(cs_dev_id_t device_id)
{
	unsigned long flags;
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: device_id=%d\n", __func__, device_id);
	}
#endif

	if (g_route_inited == 1)
	{
		spin_lock_irqsave(&g_route_lock, flags);

		cs_lpm_fwd_result_shut(device_id);

		memset(&(g_l3_nexthop[0]), 0, sizeof(g_l3_nexthop));
		memset(&(g_l3_route[0]), 0, sizeof(g_l3_route));
		g_l3_nexthop_count = 0;
		g_l3_route_count = 0;
		g_nat_subnet_intf_id = 0;
		memset(&g_nat_subnet, 0, sizeof(g_nat_subnet));
		g_route_inited = 0;

		spin_unlock_irqrestore(&g_route_lock, flags);
	}

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_route_shut);

static void cs_l3_nexthop_dump(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop)
{
#ifdef CONFIG_CS752X_PROC
	int i;
	char *p_src, *p_dest;

	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: device_id=%d, nexthop=0x%x\n", __func__, device_id, (unsigned int)nexthop);

		printk("%s: nexthop->nhid.nhop_type=%d\n", __func__, nexthop->nhid.nhop_type);

		printk("%s: nexthop->nhid.addr.afi=%d, nexthop->nhid.addr.ip_addr.ipv4_addr=0x%x, nexthop->nhid.addr.addr_len=%d\n",
			__func__, nexthop->nhid.addr.afi, nexthop->nhid.addr.ip_addr.ipv4_addr, nexthop->nhid.addr.addr_len);

		if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_DIRECT || nexthop->nhid.nhop_type == CS_L3_NEXTHOP_INTF || nexthop->nhid.nhop_type == CS_L3_NEXTHOP_UNKNOWN) {

			printk("%s: nexthop->id.port_id=%d\n", __func__, nexthop->id.port_id);
			printk("%s: da_mac[]=", __func__);
			for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
				printk("[0x%x]-", (unsigned int)(nexthop->da_mac[i]));
			}

			printk("%s: nexthop->encap.port_encap.type=%d\n", __func__, nexthop->encap.port_encap.type);

			if (nexthop->encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
				p_src = &(nexthop->encap.port_encap.port_encap.pppoe.src_mac[0]);
				p_dest = &(nexthop->encap.port_encap.port_encap.pppoe.dest_mac[0]);
			}
			else {
				p_src = &(nexthop->encap.port_encap.port_encap.eth.src_mac[0]);
			}
			printk("%s: src_mac[]=", __func__);
			for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
				printk("[0x%x]-", (unsigned int)(p_src[i]));
			}

			if (nexthop->encap.port_encap.type == CS_PORT_ENCAP_PPPOE_E) {
				printk("%s: dest_mac[]=", __func__);
				for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
					printk("[0x%x]-", (unsigned int)(p_dest[i]));
				}
				printk("%s: VLAN tag[0]=0x%x, VLAN tag[1]=0x%x\n", __func__,
					 nexthop->encap.port_encap.port_encap.pppoe.tag[0], nexthop->encap.port_encap.port_encap.pppoe.tag[1]);
			}
		}
		else {
			printk("%s: nexthop->id.ipsec_said=%d\n", __func__, nexthop->id.ipsec_said);
			printk("%s: nexthop->encap.tunnel_cfg.type=%d\n", __func__, nexthop->encap.tunnel_cfg.type);

			printk("%s: nexthop->encap.tunnel_cfg.dest_addr.afi=%d\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.afi);
			printk("%s: nexthop->encap.tunnel_cfg.dest_addr.addr_len=%d\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.addr_len);
			if (nexthop->encap.tunnel_cfg.dest_addr.afi == CS_IPV6) {
				printk("%s: nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[0]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[0]);
				printk("%s: nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[1]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[1]);
				printk("%s: nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[2]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[2]);
				printk("%s: nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[3]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv6_addr[3]);
			}
			else {
				printk("%s: nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv4_addr=0x%x\n", __func__, nexthop->encap.tunnel_cfg.dest_addr.ip_addr.ipv4_addr);
			}
			printk("%s: nexthop->encap.tunnel_cfg.src_addr.afi=%d\n", __func__, nexthop->encap.tunnel_cfg.src_addr.afi);
			printk("%s: nexthop->encap.tunnel_cfg.src_addr.addr_len=%d\n", __func__, nexthop->encap.tunnel_cfg.src_addr.addr_len);
			if (nexthop->encap.tunnel_cfg.src_addr.afi == CS_IPV6) {
				printk("%s: nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[0]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[0]);
				printk("%s: nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[1]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[1]);
				printk("%s: nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[2]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[2]);
				printk("%s: nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[3]=0x%x\n", __func__, nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv6_addr[3]);
			}
			else {
				printk("%s: nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv4_addr=0x%x\n", __func__, nexthop->encap.tunnel_cfg.src_addr.ip_addr.ipv4_addr);
			}
			printk("%s: nexthop->encap.tunnel_cfg.tx_port=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tx_port);
			printk("%s: nexthop->encap.tunnel_cfg.tunnel.l2tp.ver=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tunnel.l2tp.ver);
			printk("%s: nexthop->encap.tunnel_cfg.tunnel.l2tp.len=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tunnel.l2tp.len);
			printk("%s: nexthop->encap.tunnel_cfg.tunnel.l2tp.tid=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tunnel.l2tp.tid);
			printk("%s: nexthop->encap.tunnel_cfg.tunnel.l2tp.ipv4_id=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tunnel.l2tp.ipv4_id);
			printk("%s: nexthop->encap.tunnel_cfg.tunnel.l2tp.dest_port=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tunnel.l2tp.dest_port);
			printk("%s: nexthop->encap.tunnel_cfg.tunnel.l2tp.src_port=0x%x\n", __func__, nexthop->encap.tunnel_cfg.tunnel.l2tp.src_port);




		}
	}
#endif

}

static void cs_l3_mac_addr_ntohl(char *p_dest, char *p_src)
{
	int i;

	for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
		p_dest[i] = p_src[CS_ETH_ADDR_LEN - 1 - i];
	}
}

cs_status_t cs_l3_nexthop_add(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop, cs_uint32_t *index)
{
	cs_fwd_result_t fwd_result, *p_fwd_result = &fwd_result;
	cs_uint32_t fwd_result_index;
	int i, ret;
	cs_uint16_t sa_id = 0;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	cs_l3_nexthop_dump(device_id, nexthop);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_DIRECT) {

		//printk("%s: nexthop type is CS_L3_NEXTHOP_DIRECT\n", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			//printk("%s: nexthop->da_mac[%d]=0x%x\n", __func__, i, (unsigned int)nexthop->da_mac[i]);
			if (nexthop->da_mac[i] != 0)
				break;
		}
		//printk("%s: i =%d\n", __func__, i);
		if (i == CS_ETH_ADDR_LEN) {
			printk("%s: invalid DA_MAC with type CS_L3_NEXTHOP_DIRECT!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_ERROR;
		}
	}
	memset(p_fwd_result, 0, sizeof(cs_fwd_result_t));
	p_fwd_result->app_type = CS_APP_L3_ROUTE;
	p_fwd_result->port_id = nexthop->id.port_id;

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: nexthop->nhid.nhop_type = %d\n", __func__, nexthop->nhid.nhop_type);
	}
#endif

	/* The voq_base should be assigned according to TUNNEL type, L2TP/IPSEC/PPTP all passed to PE1 */
	if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_IPSEC || nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_L2TP_IPSEC
		|| nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_PPTP || nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_L2TP
		|| nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE) {
		p_fwd_result->voq_base = ENCAPSULATION_VOQ_BASE;
		//p_fwd_result->voq_base = p_fwd_result->port_id * 8;
		p_fwd_result->enc_type = CS_PORT_ENCAP_ETH_E;
		if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_PPTP) {
#ifdef CONFIG_CS75XX_HW_ACCEL_PPTP
			cs_pptp_set_src_mac(nexthop, &(p_fwd_result->src_mac[0]));
#endif
		} else	if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_IP_TRANSLATE) {
#ifdef CONFIG_CS75XX_HW_ACCEL_IP_TRANSLATE
			cs_ip_translate_set_src_mac(nexthop, &(p_fwd_result->src_mac[0]));
#endif
		}
		else {
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
			cs_l2tp_ipsec_set_src_mac(nexthop, &(p_fwd_result->src_mac[0]), &sa_id);
			printk("%s: sa_id=%d\n", __func__, sa_id);
#endif
		}
		printk("%s: src_mac[0-5]=0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n", __func__,
			(unsigned int)(p_fwd_result->src_mac[0]), (unsigned int)(p_fwd_result->src_mac[1]),
			(unsigned int)(p_fwd_result->src_mac[2]), (unsigned int)(p_fwd_result->src_mac[3]),
			(unsigned int)(p_fwd_result->src_mac[4]), (unsigned int)(p_fwd_result->src_mac[5]));
	}
	else {

		p_fwd_result->voq_base = p_fwd_result->port_id * 8;

		p_fwd_result->enc_type = nexthop->encap.port_encap.type;

		if (p_fwd_result->enc_type == CS_PORT_ENCAP_PPPOE_E) {
			/* user input is network order should change to be host order */
			//memcpy(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.src_mac[0]), CS_ETH_ADDR_LEN);
			//memcpy(&(p_fwd_result->dest_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.dest_mac[0]), CS_ETH_ADDR_LEN);
			cs_l3_mac_addr_ntohl(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.src_mac[0]));
			cs_l3_mac_addr_ntohl(&(p_fwd_result->dest_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.dest_mac[0]));

			memcpy(&(p_fwd_result->tag[0]), &(nexthop->encap.port_encap.port_encap.pppoe.tag[0]), sizeof(p_fwd_result->tag));
			p_fwd_result->pppoe_session_id = nexthop->encap.port_encap.port_encap.pppoe.pppoe_session_id;
		}

		if (p_fwd_result->enc_type == CS_PORT_ENCAP_ETH_E || p_fwd_result->enc_type == CS_PORT_ENCAP_ETH_1Q_E || p_fwd_result->enc_type == CS_PORT_ENCAP_ETH_QinQ_E) {
			/* user input is network order should change to be host order */
			//memcpy(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.eth.src_mac[0]), CS_ETH_ADDR_LEN);
			cs_l3_mac_addr_ntohl(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.eth.src_mac[0]));
			cs_l3_mac_addr_ntohl(&(p_fwd_result->dest_mac[0]), &(nexthop->da_mac[0]));

			if (p_fwd_result->enc_type == CS_PORT_ENCAP_ETH_1Q_E || p_fwd_result->enc_type == CS_PORT_ENCAP_ETH_QinQ_E)
				memcpy(&(p_fwd_result->tag[0]), &(nexthop->encap.port_encap.port_encap.eth.tag[0]), sizeof(p_fwd_result->tag));
		}
	}

	ret = cs_fwd_result_add(device_id, p_fwd_result, &fwd_result_index);
	if (ret != CS_E_OK)
	{
		printk("%s: cs_fwd_result_add() return failed!!, ret=%d\n", __func__, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	/* find a free table entry */
	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
		if (g_l3_nexthop[i].used == 0) {
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
			printk("%s: free entry is found at %d in g_l3_nexthop[]\n", __func__, i);
	}
#endif
			break;
		}
	}
	if (i == CS_IPV4_ROUTE_MAX) {
		printk("%s: No free entry is found in g_l3_nexthop[]!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}
	*index = i;

	g_l3_nexthop[i].used = 1;
	g_l3_nexthop[i].nexthop = *nexthop;
	g_l3_nexthop[i].fwd_result_index = fwd_result_index;
	g_l3_nexthop[i].sa_id = sa_id;

	g_l3_nexthop_count++;
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: g_l3_nexthop[%d].fwd_result_index=%d\n", __func__,
			g_l3_nexthop_count,  g_l3_nexthop[g_l3_nexthop_count].fwd_result_index);
	}
#endif

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_nexthop_add);

cs_status_t cs_l3_nexthop_get(cs_dev_id_t device_id, cs_uint32_t index, cs_l3_nexthop_t *nexthop)
{
	unsigned long flags;
	spin_lock_irqsave(&g_route_lock, flags);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}
	if (index >= CS_IPV4_ROUTE_MAX) {
		printk("%s: index=%d exceed the max number allowed!!\n", __func__, index);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_NEXTHOP_NOT_FOUND;
	}

	*nexthop = g_l3_nexthop[index].nexthop;
	cs_l3_nexthop_dump(device_id, nexthop);

	spin_unlock_irqrestore(&g_route_lock, flags);
	return CS_OK;
}

EXPORT_SYMBOL(cs_l3_nexthop_get);

/* interanl use for IPSEC tunnel */
extern cs_status_t cs_fwd_result_update_l2_mac_entry(cs_uint32_t fwd_result_index, char *source_mac);
cs_status_t cs_l3_nexthop_update_by_sa_id(cs_uint32_t old_sa_id, cs_uint32_t new_sa_id, cs_uint8_t *source_mac)
{
	int i;
	cs_uint32_t fwd_result_index;

	printk("%s: old_sa_id=%d, new_sa_id=%d\n", __func__, old_sa_id, new_sa_id);
	printk("%s: source_mac[0-5]=0x%x-0x%x-0x%x-0x%x-0x%x-0x%x\n", __func__,
		source_mac[0], source_mac[1], source_mac[2], source_mac[3], source_mac[4], source_mac[5]);
	/* find a free table entry */
	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
		if (g_l3_nexthop[i].used != 0) {
			if (g_l3_nexthop[i].sa_id == old_sa_id) {
				break;
			}
		}
	}
	if (i == CS_IPV4_ROUTE_MAX) {
		printk("%s: No match sa_id=%d is found!!\n", __func__, old_sa_id);
		return CS_E_ERROR;
	}
	g_l3_nexthop[i].sa_id = new_sa_id;
	fwd_result_index = g_l3_nexthop[i].fwd_result_index;
	return cs_fwd_result_update_l2_mac_entry(fwd_result_index, source_mac);
}

cs_status_t cs_l3_nexthop_update(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop)
{
	cs_fwd_result_t fwd_result, *p_fwd_result = &fwd_result;
	int ret;
	int i;
	cs_uint32_t index;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);
	cs_l3_nexthop_dump(device_id, nexthop);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
		/* skip the free ones */
		if (g_l3_nexthop[i].used == 0) {
			continue;
		}
		if (memcmp(&(nexthop->nhid), &(g_l3_nexthop[i].nexthop.nhid), sizeof(cs_l3_nexthop_id_t)) == 0) {
			printk("%s: nexthop found at index=%d\n", __func__, i);
			break;
		}
	}
	if (i == CS_IPV4_ROUTE_MAX) {
		printk("%s: can not find the nexthop!!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_NEXTHOP_NOT_FOUND;
	}

	ret = cs_fwd_result_get_by_index(device_id, g_l3_nexthop[i].fwd_result_index, p_fwd_result);
	if (ret != CS_E_OK) {
		printk("%s: cs_fwd_result_get_by_index(index=%d)\n", __func__, g_l3_nexthop[i].fwd_result_index);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}
	p_fwd_result->app_type = CS_APP_L3_ROUTE;
	p_fwd_result->port_id = nexthop->id.port_id;
	p_fwd_result->voq_base = (p_fwd_result->port_id - 1) * 8;
	p_fwd_result->enc_type = nexthop->encap.port_encap.type;
	if (p_fwd_result->enc_type == CS_PORT_ENCAP_PPPOE_E) {
		/* user input is network order should change to be host order */
		//memcpy(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.src_mac[0]), CS_ETH_ADDR_LEN);
		//memcpy(&(p_fwd_result->dest_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.dest_mac[0]), CS_ETH_ADDR_LEN);
		cs_l3_mac_addr_ntohl(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.src_mac[0]));
		cs_l3_mac_addr_ntohl(&(p_fwd_result->dest_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.dest_mac[0]));

		memcpy(&(p_fwd_result->tag[0]), &(nexthop->encap.port_encap.port_encap.pppoe.tag[0]), sizeof(p_fwd_result->tag));
		p_fwd_result->pppoe_session_id = nexthop->encap.port_encap.port_encap.pppoe.pppoe_session_id;
	}
	else if (p_fwd_result->enc_type == CS_PORT_ENCAP_ETH_E) {
		/* user input is network order should change to be host order */
		//memcpy(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.pppoe.src_mac[0]), CS_ETH_ADDR_LEN);
		cs_l3_mac_addr_ntohl(&(p_fwd_result->src_mac[0]), &(nexthop->encap.port_encap.port_encap.eth.src_mac[0]));

		memcpy(&(p_fwd_result->tag[0]), &(nexthop->encap.port_encap.port_encap.pppoe.tag[0]), sizeof(p_fwd_result->tag));
	}
	index = g_l3_nexthop[i].fwd_result_index;
	ret = cs_fwd_result_update(device_id, index, p_fwd_result);
	if (ret != CS_E_OK) {
		printk("%s: cs_fwd_result_update() return failed!!, ret=%d\n", __func__, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}
	g_l3_nexthop[i].nexthop = *nexthop;
	g_l3_nexthop[i].ref_cnt = 0;

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: g_l3_nexthop[%d].fwd_result_index=%d\n", __func__, i, g_l3_nexthop[i].fwd_result_index);
		printk("%s: p_fwd_result->app_type=%d, p_fwd_result->port_id=%d, p_fwd_result->voq_base=%d\n", __func__,
			p_fwd_result->app_type, p_fwd_result->port_id, p_fwd_result->voq_base);
		printk("%s: p_fwd_result->enc_type=%d\n", __func__, p_fwd_result->enc_type);
		printk("%s: src_mac[]=", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			printk("[0x%x]-", (unsigned int)(p_fwd_result->src_mac[i]));
		}
		printk("%s: dest_mac[]=", __func__);
		for (i = 0; i < CS_ETH_ADDR_LEN; i++) {
			printk("[0x%x]-", (unsigned int)(p_fwd_result->dest_mac[i]));
		}
		printk("%s: VLAN tag[0]=0x%x, VLAN tag[1]=0x%x\n", __func__, p_fwd_result->tag[0], p_fwd_result->tag[1]);
		printk("%s: p_fwd_result->voq_pol_rslt_idx=%d, p_fwd_result->fwd_rslt_idx=%d, p_fwd_result->fvlan_idx=%d, p_fwd_result->mac_idx=%d\n",
			__func__, p_fwd_result->voq_pol_rslt_idx, p_fwd_result->fwd_rslt_idx, p_fwd_result->fvlan_idx, p_fwd_result->mac_idx);
	}
#endif
	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_nexthop_update);

cs_status_t cs_l3_nexthop_delete(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop)
{
	int ret;
	int i;
	cs_uint32_t index;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	cs_l3_nexthop_dump(device_id, nexthop);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
		/* skip the free ones */
		if (g_l3_nexthop[i].used == 0) {
			continue;
		}
		if (memcmp(nexthop, &g_l3_nexthop[i].nexthop, sizeof(cs_l3_nexthop_t)) == 0) {
			//printk("%s: nexthop is found at index=%d\n", __func__, i);
			break;
		}
	}
	if (i == CS_IPV4_ROUTE_MAX) {
		printk("%s: can not find the nexthop!!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: g_l3_nexthop[i].ref_cnt=%d\n", __func__, g_l3_nexthop[i].ref_cnt);
	}
#endif
	if (g_l3_nexthop[i].ref_cnt > 0) {
		printk("%s: reference count is not zero!!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	g_l3_nexthop[i].used = 0;
	index = g_l3_nexthop[i].fwd_result_index;
	ret = cs_fwd_result_delete_entry_by_index(device_id, index);
	if (ret != CS_E_OK) {
		printk("%s: cs_fwd_result_delete_by_index index=(%d) return failed!!, ret=%d\n", __func__, index, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	g_l3_nexthop_count--;

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_nexthop_delete);

static cs_boolean_t cs_l3_is_ip_equal(cs_ip_address_t *ip_addr1, cs_ip_address_t *ip_addr2)
{
	cs_uint32_t ip1, ip2;
	cs_uint32_t  mask1, mask2;
	cs_ip_address_t addr1, addr2;

	cs_l3_ip_address_htonl(&addr1, ip_addr1);
	cs_l3_ip_address_htonl(&addr2, ip_addr2);

	if (addr1.afi == CS_IPV6) {
		mask1 = addr1.addr_len;
		mask2 = addr2.addr_len;

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
			printk("%s: mask1=0x%x, mask2=0x%x\n", __func__, mask1, mask2);
			printk("%s: ipv6_addr[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
			addr1.ip_addr.ipv6_addr[0], addr1.ip_addr.ipv6_addr[1], addr1.ip_addr.ipv6_addr[2], addr1.ip_addr.ipv6_addr[3]);
		}
#endif
		if (mask1 == mask2) {

			if (mask1 > 96) {        /* Just compare addr32[0~3] */
				mask1 = cs_l3_route_gen_ip_mask_v4(mask1 - 96);
				if ((addr1.ip_addr.ipv6_addr[0] == addr2.ip_addr.ipv6_addr[0]) &&
				    (addr1.ip_addr.ipv6_addr[1] == addr2.ip_addr.ipv6_addr[1]) &&
				    (addr1.ip_addr.ipv6_addr[2] == addr2.ip_addr.ipv6_addr[2]) &&
				    ((addr1.ip_addr.ipv6_addr[3] & mask1) == (addr2.ip_addr.ipv6_addr[3] & mask1)))
					return CS_TRUE;
			} else if (mask1 > 64) { /* Compare addr32[1~3] */
			       mask1 = cs_l3_route_gen_ip_mask_v4(mask1 - 64);
			       if ((addr1.ip_addr.ipv6_addr[0] == addr2.ip_addr.ipv6_addr[0]) &&
				   (addr1.ip_addr.ipv6_addr[1] == addr2.ip_addr.ipv6_addr[1]) &&
				   ((addr1.ip_addr.ipv6_addr[2] & mask1) == (addr2.ip_addr.ipv6_addr[2] & mask1)))
					return CS_TRUE;
			} else if (mask1 > 32) { /* Compare addr32[2~3] */
			       mask1 = cs_l3_route_gen_ip_mask_v4(mask1 - 32);
			       if ((addr1.ip_addr.ipv6_addr[0] == addr2.ip_addr.ipv6_addr[0]) &&
				   ((addr1.ip_addr.ipv6_addr[1] & mask1) == (addr2.ip_addr.ipv6_addr[1] & mask1)))
					return CS_TRUE;
			} else if (mask1 > 0) {  /* Compare addr32[3] */
			       mask1 = cs_l3_route_gen_ip_mask_v4(mask1);
			       if ((addr1.ip_addr.ipv6_addr[0] & mask1) == (addr2.ip_addr.ipv6_addr[0] & mask1))
					return CS_TRUE;
			}
	    }
	}
	else {
		ip1 = addr1.ip_addr.ipv4_addr;
		ip2 = addr2.ip_addr.ipv4_addr;
		mask1 = cs_l3_route_gen_ip_mask_v4(addr1.addr_len);
		mask2 = cs_l3_route_gen_ip_mask_v4(addr2.addr_len);

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
			printk("%s: ip1=0x%x, ip2=0x%x, mask1=0x%x, mask2=0x%x\n", __func__, ip1, ip2, mask1, mask2);
		}
#endif
		if ((mask1 == mask2) && ((ip1 & mask1) == (ip2 & mask2))) {
			return CS_TRUE;
		}
	}
	return CS_FALSE;
}

static cs_boolean_t cs_l3_is_addr1_higher(cs_ip_address_t *ip_addr1, cs_ip_address_t *ip_addr2)
{
	int j, found;
	cs_uint32_t ip1, ip2;
	cs_uint32_t mask1, mask2;
	cs_ip_address_t addr1, addr2;

	//printk("%s: ipv6_addr2[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
	//           ip_addr2->ip_addr.ipv6_addr[0], ip_addr2->ip_addr.ipv6_addr[1], ip_addr2->ip_addr.ipv6_addr[2], ip_addr2->ip_addr.ipv6_addr[3]);

	 if (cs_l3_is_zero_ip_addr(ip_addr2) == CS_TRUE)
			return CS_TRUE;

	cs_l3_ip_address_htonl(&addr1, ip_addr1);
	cs_l3_ip_address_htonl(&addr2, ip_addr2);

	if (addr1.afi == CS_IPV6) {

		mask1 = addr1.addr_len;
		mask2 = addr2.addr_len;

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
			printk("%s: mask1=0x%x, mask2=0x%x\n", __func__, mask1, mask2);
			printk("%s: addr1[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
				addr1.ip_addr.ipv6_addr[0], addr1.ip_addr.ipv6_addr[1], addr1.ip_addr.ipv6_addr[2], addr1.ip_addr.ipv6_addr[3]);
			printk("%s: addr2[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
				addr2.ip_addr.ipv6_addr[0], addr2.ip_addr.ipv6_addr[1], addr2.ip_addr.ipv6_addr[2], addr2.ip_addr.ipv6_addr[3]);
		}
#endif
		j = 0;
		found = 0;
		while (mask1 > 0 && mask2 > 0 && j >= 0) {

			if (mask1 > 32) {
				mask1 -= 32;
				ip1 = addr1.ip_addr.ipv6_addr[j];
			}
			else {
				mask1 = 0;
				ip1 = addr1.ip_addr.ipv6_addr[j] & cs_l3_route_gen_ip_mask_v4(mask1);
			}
			if (mask2 > 32) {
				mask2 -= 32;
				ip2 = addr2.ip_addr.ipv6_addr[j];
			}
			else {
				mask2 = 0;
				ip2 = addr2.ip_addr.ipv6_addr[j] & cs_l3_route_gen_ip_mask_v4(mask2);
			}
			/* find the ip/prefix is larger than the input one */
#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
				printk("%s: ip1=0x%x, ip2=0x%x, mask1=0x%x, mask2=0x%x\n", __func__, ip1, ip2, mask1, mask2);
			}
#endif
			if (ip1 != ip2) {
				if (ip1 > ip2)
					return CS_TRUE;
				break;
			}
			j++;
		}
	}
	else {
		ip1 = addr1.ip_addr.ipv4_addr;
		ip2 = addr2.ip_addr.ipv4_addr;
		mask1 = cs_l3_route_gen_ip_mask_v4(addr1.addr_len);
		mask2 = cs_l3_route_gen_ip_mask_v4(addr2.addr_len);

#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
			printk("%s: ip1=0x%x, ip2=0x%x, mask1=0x%x, mask2=0x%x\n", __func__, ip1, ip2, mask1, mask2);
		}
#endif
		if ((ip1 & mask1) > (ip2 & mask2)) {
			return CS_TRUE;
		}

	}
	return CS_FALSE;
}

cs_status_t cs_l3_nexthop_show(cs_dev_id_t device_id, cs_l3_nexthop_t *nexthop, cs_boolean_t next)
{
	int i, index, index1;
	cs_ip_address_t next_ip_addr, addr1, addr2;
	cs_uint8_t addr_len;

	cs_uint32_t save_adapt_debug;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	cs_l3_nexthop_dump(device_id, nexthop);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	if (next != CS_TRUE) {

		for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
			/* skip the free ones */
			if (g_l3_nexthop[i].used == 0) {
				continue;
			}
			if (memcmp(&(nexthop->nhid.addr), &(g_l3_nexthop[i].nexthop.nhid.addr), sizeof(cs_ip_address_t)) == 0) {
				printk("%s: nexthop found at index=%d\n", __func__, i);
				*nexthop = g_l3_nexthop[i].nexthop;
				break;
			}
		}
	}
	else {

		next_ip_addr = nexthop->nhid.addr;

		if (nexthop->nhid.addr.afi == CS_IPV6)
			addr_len = 128;
		else
			addr_len = 32;
		for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {

			if (g_l3_nexthop[i].used == 0) {
				continue;
			}

			if (g_l3_nexthop[i].nexthop.nhid.addr.afi != nexthop->nhid.addr.afi) {
				continue;
			}

			if (cs_l3_is_zero_ip_addr(&(nexthop->nhid.addr)) == CS_TRUE) {
				printk("%s: nexthop found at index=%d\n", __func__, i);
				next_ip_addr = g_l3_nexthop[i].nexthop.nhid.addr;
				break;
			}
			else {

				addr1 = g_l3_nexthop[i].nexthop.nhid.addr;
				addr2 = nexthop->nhid.addr;
				addr1.addr_len = addr_len;
				addr2.addr_len = addr_len;

				if (cs_l3_is_addr1_higher(&addr1, &addr2) == CS_TRUE) {

					printk("%s: nexthop found at index=%d\n", __func__, i);
					next_ip_addr = g_l3_nexthop[i].nexthop.nhid.addr;
					break;
				}
			}
		}
		if (i == CS_IPV4_ROUTE_MAX) {
			printk("%s: can not find the nexthop!!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_NEXTHOP_NOT_FOUND;
		}
		index1 = index = i;

		for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
			if (g_l3_nexthop[i].used == 0) {
				continue;
			}

#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {

				printk("%s: index=%d, i=%d, g_l3_nexthop[i].nexthop.nhid.addr.afi=%d, next_ip_addr.afi=%d\n",
					__func__, index, i, g_l3_nexthop[i].nexthop.nhid.addr.afi, next_ip_addr.afi);
			}
#endif

			if (g_l3_nexthop[i].nexthop.nhid.addr.afi != next_ip_addr.afi) {
				continue;
			}

			/* skip the nexthop which is the first higher one */
			if (index == i) {
				continue;
			}

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: next_ip_addr.addr[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
		       next_ip_addr.ip_addr.ipv6_addr[0], next_ip_addr.ip_addr.ipv6_addr[1],
		       next_ip_addr.ip_addr.ipv6_addr[2], next_ip_addr.ip_addr.ipv6_addr[3]);
	}
#endif

			addr1 = g_l3_nexthop[i].nexthop.nhid.addr;
			addr2 = nexthop->nhid.addr;

			addr1.addr_len = addr_len;
			addr2.addr_len = addr_len;

			/* try to find the numerically higher than input and lower than others */
			if (cs_l3_is_addr1_higher(&addr1, &addr2) == CS_TRUE) {
				addr1 = next_ip_addr;
				addr2 = g_l3_nexthop[i].nexthop.nhid.addr;
				if (cs_l3_is_addr1_higher(&addr1, &addr2) == CS_TRUE) {
					next_ip_addr = g_l3_nexthop[i].nexthop.nhid.addr;
					index1 = i;
				}
			}
		}
		if (index1 == CS_IPV4_ROUTE_MAX) {
			printk("%s: can not find the next nexthop!!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_NEXTHOP_NOT_FOUND;
		}

		printk("%s: the next nexthop is found at index=%d\n", __func__, index1);
		*nexthop = g_l3_nexthop[index1].nexthop;
	}

	/* display the nexthop structure */
	//save_adapt_debug = cs_adapt_debug;
	//cs_adapt_debug |= CS752X_ADAPT_ROUTE;
	cs_l3_nexthop_dump(device_id, nexthop);
	//cs_adapt_debug = save_adapt_debug;

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l3_nexthop_show);

static void cs_l3_route_dump(cs_dev_id_t device_id, cs_l3_route_t *route)
{
#ifdef CONFIG_CS752X_PROC
	cs_l3_nexthop_t *nexthop = &(route->nexthop);

	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: device_id=%d, route=0x%x\n", __func__, device_id, (unsigned int)route);
		printk("%s: route->prefix.afi=%d, route->prefix.ip_addr.ipv4_addr=0x%x, route->prefix.addr_len=%d\n",
			__func__, route->prefix.afi, route->prefix.ip_addr.ipv4_addr, route->prefix.addr_len);

		printk("%s: nexthop->nhid.nhop_type=%d\n", __func__, nexthop->nhid.nhop_type);
		printk("%s: nexthop->nhid.addr.afi=%d, nexthop->nhid.addr.ip_addr.ipv4_addr=0x%x, nexthop->nhid.addr.addr_len=%d\n",
			__func__, nexthop->nhid.addr.afi, nexthop->nhid.addr.ip_addr.ipv4_addr, nexthop->nhid.addr.addr_len);
		printk("%s: nexthop->id.port_id=%d\n", __func__, nexthop->id.port_id);
	}
#endif
}

static void cs_l3_ip_address_htonl(cs_ip_address_t *p_dest, cs_ip_address_t *p_src)
{
	/* user input is network order should be changed to host order */
	p_dest->afi = p_src->afi;
	p_dest->addr_len = p_src->addr_len;
	if (p_src->afi == CS_IPV6) {
		p_dest->ip_addr.ipv6_addr[0] = htonl(p_src->ip_addr.ipv6_addr[0]);
		p_dest->ip_addr.ipv6_addr[1] = htonl(p_src->ip_addr.ipv6_addr[1]);
		p_dest->ip_addr.ipv6_addr[2] = htonl(p_src->ip_addr.ipv6_addr[2]);
		p_dest->ip_addr.ipv6_addr[3] = htonl(p_src->ip_addr.ipv6_addr[3]);
	}
	else {
		p_dest->ip_addr.ipv4_addr = htonl(p_src->ip_addr.ipv4_addr);
	}
}

static void cs_l3_ip_address_ntohl(cs_ip_address_t *p_dest, cs_ip_address_t *p_src)
{
	/* user input is network order should be changed to host order */
	p_dest->afi = p_src->afi;
	p_dest->addr_len = p_src->addr_len;
	if (p_src->afi == CS_IPV6) {
		p_dest->ip_addr.ipv6_addr[0] = ntohl(p_src->ip_addr.ipv6_addr[0]);
		p_dest->ip_addr.ipv6_addr[1] = ntohl(p_src->ip_addr.ipv6_addr[1]);
		p_dest->ip_addr.ipv6_addr[2] = ntohl(p_src->ip_addr.ipv6_addr[2]);
		p_dest->ip_addr.ipv6_addr[3] = ntohl(p_src->ip_addr.ipv6_addr[3]);
	}
	else {
		p_dest->ip_addr.ipv4_addr = ntohl(p_src->ip_addr.ipv4_addr);
	}
}

cs_status_t cs_l3_route_add(cs_dev_id_t device_id, cs_l3_route_t *route)
{
	cs_l3_nexthop_t *nexthop = &(route->nexthop);
	int i, j, ret;
	cs_uint32_t index;
	cs_lpm_t lpm, *p_lpm = &lpm;
	cs_fwd_result_t fwd_result, *p_fwd_result = &fwd_result;
	cs_boolean_t is_valid;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	cs_l3_route_dump(device_id, route);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	 /* check duplicate entry */
	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
		if (g_l3_route[i].used == 0) {
			continue;
		}
		if (cs_l3_is_ip_equal(&(g_l3_route[i].prefix), &(route->prefix)) == CS_TRUE) {
			printk("%s: route prefix already exists!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_ERROR;
		}
	}

	/* find the used entry from g_l3_nexthop */
	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
		if (g_l3_nexthop[i].used == 0) {
			continue;
		}
		if (memcmp(nexthop, &g_l3_nexthop[i].nexthop, sizeof(cs_l3_nexthop_t)) == 0)
			break;
	}
	if (i == CS_IPV4_ROUTE_MAX) {
		printk("%s: can not find the nexthop!!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_NEXTHOP_NOT_FOUND;
	}

	/* if nexthop type is IPSEC tunnel, then should check the selector */
	if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_TUNNEL_IPSEC)
#ifdef CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC
	{
		if ((cs_ipsec_sel_ip_check(nexthop->id.tunnel_id, &(route->prefix), &is_valid) != CS_OK) || (is_valid != CS_TRUE)) {
			printk("%s: The nexthop is not valid in IPSEC selector!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_ERROR;
		}
	}
#else
	{
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_NOT_SUPPORT;
	}
#endif

	if (g_nat_subnet_intf_id != 0) {
		if (nexthop->nhid.nhop_type == CS_L3_NEXTHOP_UNKNOWN ||
		    nexthop->nhid.nhop_type == CS_L3_NEXTHOP_DIRECT ||
		    nexthop->nhid.nhop_type == CS_L3_NEXTHOP_INTF) {
			printk("%s: g_nat_subnet_intf_id is set only accept TUNNEL type nexthop!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_ERROR;
		}
	}
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: g_l3_nexthop[%d].fwd_result_index=%d\n", __func__, i, g_l3_nexthop[i].fwd_result_index);
	}
#endif
	memset(p_lpm, 0, sizeof(cs_lpm_t));
	p_lpm->app_type = CS_APP_L3_ROUTE;

	/* user input is network order should be changed to host order */
	//p_lpm->prefix = route->prefix;
	cs_l3_ip_address_ntohl(&(p_lpm->prefix), &(route->prefix));

	index = g_l3_nexthop[i].fwd_result_index;

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: index=%d\n", __func__, index);
	}
#endif
	ret = cs_fwd_result_get_by_index(device_id, index, p_fwd_result);
	if (ret != CS_E_OK) {
		printk("%s: cs_fwd_result_get_by_index(index=%d) return failed, ret=%d\n", __func__, index, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	p_lpm->fwd_rslt_idx = p_fwd_result->fwd_rslt_idx;

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: p_lpm->fwd_rslt_idx=%d\n", __func__, p_lpm->fwd_rslt_idx);
	}
#endif
	ret = cs_lpm_add(device_id, p_lpm);
	if (ret != CS_E_OK) {
		printk("%s: cs_lpm_add() return failed!!, ret=%d\n", __func__, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: cs_lpm_add() OK\n", __func__);
	}
#endif

	/* find the used entry from g_l3_route */
	for (j = 0; j < CS_IPV4_ROUTE_MAX; j++) {
		if (g_l3_route[j].used == 0) {
#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
				printk("%s: a free entry found at %d of g2_l3_route[]\n", __func__, j);
			}
#endif
			break;
		}
	}
	if (j == CS_IPV4_ROUTE_MAX) {
		printk("%s: can not find a free g_l3_route[] entry!!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ROUTE_MAX_LIMIT;
	}

	g_l3_route[j].used = 1;
	g_l3_route[j].prefix = route->prefix;
	g_l3_route[j].p_internal_nexthop = &g_l3_nexthop[i];
	g_l3_nexthop[i].ref_cnt++;

	g_l3_route_count++;

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_route_add);

cs_status_t cs_l3_route_get(cs_dev_id_t device_id, cs_l3_route_t *route)
{
	int ret;
	cs_uint32_t lpm_index;
	cs_lpm_t lpm, *p_lpm = &lpm;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	cs_l3_route_dump(device_id, route);
	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	if (route == NULL) {
		printk("%s: route == NULL!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	p_lpm->app_type = CS_APP_L3_ROUTE;

	/* user input is network order should be changed to host order */
	// p_lpm->prefix = route->prefix;
	cs_l3_ip_address_ntohl(&(p_lpm->prefix), &(route->prefix));

	ret = cs_lpm_get(device_id, p_lpm, &lpm_index);
	if (ret != CS_E_OK) {
		printk("%s: cs_lpm_get() return failed!!, ret=%d\n", __func__, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_route_get);

cs_status_t cs_l3_route_delete(cs_dev_id_t device_id, cs_l3_route_t *route)
{
	int i, ret;
	cs_uint32_t index;
	cs_lpm_t lpm, *p_lpm = &lpm;
	cs_fwd_result_t fwd_result, *p_fwd_result = &fwd_result;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	cs_l3_route_dump(device_id, route);

	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {

		if (g_l3_route[i].used == 0) {
			continue;
		}
		if (memcmp(&(route->prefix), &(g_l3_route[i].prefix), sizeof(cs_ip_address_t)) == 0) {

#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
				printk("%s: prefix is match!!\n", __func__);
			}
#endif
			if (memcmp(&(route->nexthop), &(g_l3_route[i].p_internal_nexthop->nexthop), sizeof(cs_l3_nexthop_t)) == 0) {
				printk("%s: the route is found at index=%d\n", __func__, i);
				break;
			}
		}
	}

	if (i == CS_IPV4_ROUTE_MAX) {
		printk("%s: can not find the route!!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ROUTE_NOT_FOUND;
	}

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: g_l3_route[%d].p_internal_nexthop->fwd_result_index=%d\n", __func__, i, g_l3_route[i].p_internal_nexthop->fwd_result_index);
	}
#endif
	memset(p_lpm, 0, sizeof(cs_lpm_t));
	p_lpm->app_type = CS_APP_L3_ROUTE;

	/* user input is network order should be changed to host order */
	//p_lpm->prefix = route->prefix;
	cs_l3_ip_address_ntohl(&(p_lpm->prefix), &(route->prefix));

	index = g_l3_route[i].p_internal_nexthop->fwd_result_index;
	ret = cs_fwd_result_get_by_index(device_id, index, p_fwd_result);
	if (ret != CS_E_OK) {
		printk("%s: cs_fwd_result_get_by_index(index=%d) return failed, ret=%d\n", __func__, index, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}

	p_lpm->fwd_rslt_idx = p_fwd_result->fwd_rslt_idx;
	ret = cs_lpm_delete(device_id, p_lpm);
	if (ret != CS_E_OK) {
		printk("%s: cs_lpm_delete() return failed!!, ret=%d\n", __func__, ret);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_ERROR;
	}
#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: g_l3_route[i].p_internal_nexthop->ref_cnt=%d\n", __func__,  g_l3_route[i].p_internal_nexthop->ref_cnt);
	}
#endif
	g_l3_route[i].p_internal_nexthop->ref_cnt--;
	if (g_l3_route[i].p_internal_nexthop->ref_cnt <= 0) {
		g_l3_route[i].p_internal_nexthop->ref_cnt = 0;
		printk("%s: index %d of g_l3_route[] nexthop index reach to 0!!\n", __func__, i);
	}


	g_l3_route[i].used = 0;

	g_l3_route_count--;

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_route_delete);

cs_status_t cs_l3_nhintf_all_route_delete(cs_dev_id_t device_id, cs_uint32_t intf_id)
{
	int i, ret;
	cs_uint32_t index;
	cs_l3_route_internal_t *p_route;
	cs_lpm_t lpm, *p_lpm = &lpm;
	cs_fwd_result_t fwd_result, *p_fwd_result = &fwd_result;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {

		if (g_l3_route[i].used == 0) {
			continue;
		}
		p_route = &g_l3_route[i];
		if (p_route->p_internal_nexthop->nexthop.nhid.intf_id != intf_id)
		{
			continue;
		}
		memset(p_lpm, 0, sizeof(cs_lpm_t));
		p_lpm->app_type = CS_APP_L3_ROUTE;

		/* user input is network order should be changed to host order */
		//p_lpm->prefix = p_route->prefix;
		cs_l3_ip_address_ntohl(&(p_lpm->prefix), &(p_route->prefix));

		index = g_l3_route[i].p_internal_nexthop->fwd_result_index;
		ret = cs_fwd_result_get_by_index(device_id, index, p_fwd_result);
		if (ret != CS_E_OK) {
			printk("%s: cs_fwd_result_get_by_index(index=%d) return failed, ret=%d\n", __func__, index, ret);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_ERROR;
		}

		p_lpm->fwd_rslt_idx = p_fwd_result->fwd_rslt_idx;

		ret = cs_lpm_delete(device_id, p_lpm);
		if (ret != CS_E_OK) {
			printk("%s: cs_lpm_delete() return failed!!, ret=%d\n", __func__, ret);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_ERROR;
		}
#ifdef CONFIG_CS752X_PROC
		if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
			printk("%s:  p_route->p_internal_nexthop->ref_cnt=%d\n", __func__,  p_route->p_internal_nexthop->ref_cnt);
			printk("%s:  index=%d is deleted\n", __func__, i);
		}
#endif
		p_route->p_internal_nexthop->ref_cnt--;
		if (p_route->p_internal_nexthop->ref_cnt <= 0) {
			p_route->p_internal_nexthop->ref_cnt = 0;
			printk("%s: p_route->p_internal_nexthop=0x%x reference count reach to 0!!\n", __func__,(unsigned int)( p_route->p_internal_nexthop));
		}

		g_l3_route[i].used = 0;

		g_l3_route_count--;

		i--; /* back one index */
	}

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_l3_nhintf_all_route_delete);

static cs_uint32_t cs_l3_route_gen_ip_mask_v4(cs_uint8_t mask)
{
	u8 i;
	u32 mask_pattern=0;

	for (i = 0; i < 32; i++, mask--) {
		if (mask == 0)
			break;
		mask_pattern |= 1 << (31 - i);
	}

	return mask_pattern;
}

static cs_boolean_t cs_l3_is_zero_ip_addr(cs_ip_address_t *ip_addr)
{
	if (ip_addr->afi == CS_IPV6) {
		 if (ip_addr->addr_len == 0 && ip_addr->ip_addr.addr[0] == 0 &&  ip_addr->ip_addr.addr[1] == 0
		     && ip_addr->ip_addr.addr[2] == 0 &&  ip_addr->ip_addr.addr[3] == 0)
			return CS_TRUE;
	}
	else {
		if (ip_addr->addr_len == 0 && ip_addr->ip_addr.addr[0] == 0)
			return CS_TRUE;
	}

	return CS_FALSE;
}

cs_status_t cs_l3_route_show(cs_dev_id_t device_id, cs_l3_route_t *route, cs_boolean_t next)
{
	int i, index, index1;
	cs_uint32_t save_adapt_debug;
	cs_ip_address_t next_ip_addr, addr1, addr2;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: device_id=%d, route=0x%x\n", __func__, device_id, (unsigned int)route);
		printk("%s: route->prefix.afi=%d, route->prefix.ip_addr.ipv4_addr=0x%x, route->prefix.addr_len=%d\n",
			__func__, route->prefix.afi, route->prefix.ip_addr.ipv4_addr, route->prefix.addr_len);

	}
#endif
	if (g_route_inited == 0) {
		printk("%s: route module has not been initialized!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	if (next != CS_TRUE) {
		for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
			if (g_l3_route[i].used == 0) {
				continue;
			}
			if (cs_l3_is_ip_equal(&(g_l3_route[i].prefix), &(route->prefix)) == CS_TRUE) {
				printk("%s: the next route is found at index=%d\n", __func__, i);
				route->prefix = g_l3_route[i].prefix;
				route->nexthop = g_l3_route[i].p_internal_nexthop->nexthop;
				break;
			}
		}
	}
	else {
		next_ip_addr = route->prefix;

		for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {

			if (g_l3_route[i].used == 0) {
				continue;
			}

			if (g_l3_route[i].prefix.afi != route->prefix.afi) {
				continue;
			}

			if (cs_l3_is_zero_ip_addr(&(route->prefix)) == CS_TRUE) {
				printk("%s: route found at index=%d\n", __func__, i);
				next_ip_addr = g_l3_route[i].prefix;
				break;
			}
			else {

				addr1 = g_l3_route[i].prefix;
				addr2 = route->prefix;

				if (cs_l3_is_addr1_higher(&addr1, &addr2) == CS_TRUE) {

					printk("%s: route found at index=%d\n", __func__, i);
					next_ip_addr = g_l3_route[i].prefix;
					break;
				}
			}
		}
		if (i == CS_IPV4_ROUTE_MAX) {
			printk("%s: can not find the route!!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_NEXTHOP_NOT_FOUND;
		}
		index1 = index = i;

		for (i = 0; i < CS_IPV4_ROUTE_MAX; i++) {
			if (g_l3_route[i].used == 0) {
				continue;
			}

#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {

				printk("%s: index=%d, i=%d, g_l3_route[i].prefix.afi=%d, next_ip_addr.afi=%d\n",
					__func__, index, i, g_l3_route[i].prefix.afi, next_ip_addr.afi);
			}
#endif

			if (g_l3_route[i].prefix.afi != next_ip_addr.afi) {
				continue;
			}

			/* skip the nexthop which is the first higher one */
			if (index == i) {
				continue;
			}

#ifdef CONFIG_CS752X_PROC
			if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
				printk("%s: next_ip_addr.addr[0-3]=0x%x-0x%x-0x%x-0x%x\n", __func__,
					next_ip_addr.ip_addr.ipv6_addr[0], next_ip_addr.ip_addr.ipv6_addr[1],
					next_ip_addr.ip_addr.ipv6_addr[2], next_ip_addr.ip_addr.ipv6_addr[3]);
			}
#endif

			addr1 = g_l3_route[i].prefix;
			addr2 = route->prefix;

			/* try to find the numerically higher than input and lower than others */
			if (cs_l3_is_addr1_higher(&addr1, &addr2) == CS_TRUE) {
				addr1 = next_ip_addr;
				addr2 = g_l3_route[i].prefix;
				if (cs_l3_is_addr1_higher(&addr1, &addr2) == CS_TRUE) {
					next_ip_addr = g_l3_route[i].prefix;
					index1 = i;
				}
			}
		}
		if (index1 == CS_IPV4_ROUTE_MAX) {
			printk("%s: can not find the next route!!!\n", __func__);
			spin_unlock_irqrestore(&g_route_lock, flags);
			return CS_E_NEXTHOP_NOT_FOUND;
		}

		printk("%s: the next route is found at index=%d\n", __func__, index1);
		route->prefix = g_l3_route[index1].prefix;
		route->nexthop = g_l3_route[index1].p_internal_nexthop->nexthop;
	}

	/* display the nexthop structure */
	//save_adapt_debug = cs_adapt_debug;
	//cs_adapt_debug |= CS752X_ADAPT_ROUTE;
	cs_l3_route_dump(device_id, route);
	//cs_adapt_debug = save_adapt_debug;

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_NONE;
}
EXPORT_SYMBOL(cs_l3_route_show);

cs_status_t cs_nat_subnet_add(cs_dev_id_t device_id, cs_uint32_t intf_id, cs_ip_address_t *nat_subnet)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: intf_id=%d, nat_subnet->afi=%d\n", __func__, intf_id, nat_subnet->afi);
		for(i = 0; i < 4; i++) {
			printk("%s: nat_subnet->ip_addr.ipv6_addr[%d]=0x%x\n", __func__, i, nat_subnet->ip_addr.ipv6_addr[i]);
		}
		printk("%s: nat_subnet->addr_len=%d\n", __func__, nat_subnet->addr_len);
	}
#endif
	if (g_route_inited == 0) {
		printk("%s: the route module has not been initializeed!!\n", __func__);
		spin_unlock_irqrestore(&g_route_lock, flags);
		return CS_E_INIT;
	}

	g_nat_subnet_intf_id = intf_id;
	g_nat_subnet = *nat_subnet;

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_nat_subnet_add);

cs_status_t cs_nat_subnet_delete(cs_dev_id_t device_id, cs_uint32_t intf_id, cs_ip_address_t *nat_subnet)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&g_route_lock, flags);

#ifdef CONFIG_CS752X_PROC
	if (cs_adapt_debug & CS752X_ADAPT_ROUTE) {
		printk("%s: intf_id=%d, nat_subnet->afi=%d\n", __func__, intf_id, nat_subnet->afi);
		 for(i = 0; i < 4; i++) {
			printk("%s: nat_subnet->ip_addr.ipv6_addr[%d]=0x%x\n", __func__, i, nat_subnet->ip_addr.ipv6_addr[i]);
		}
		printk("%s: nat_subnet->addr_len=%d\n", __func__, nat_subnet->addr_len);
	}
#endif
	if (g_route_inited == 0) {
		printk("%s: the route module has not been initializeed!!\n", __func__);
		return CS_E_INIT;
	}

	g_nat_subnet_intf_id = 0;
	memset(&g_nat_subnet, 0, sizeof(g_nat_subnet));

	spin_unlock_irqrestore(&g_route_lock, flags);

	return CS_E_OK;
}
EXPORT_SYMBOL(cs_nat_subnet_delete);

