/*
 * Copyright (c) Cortina-Systems Limited 2010.  All rights reserved.
 *                Wen Hsu <wen.hsu@cortina-systems.com>
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
/*
 * cs_hw_accel_l2tp_ipsec.c
 *
 * $Id$
 *
 * This file contains the implementation for CS L2TP/IPsec Offload Kernel Module.
 */

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/esp.h>
#include <net/ah.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/timer.h>
#include <linux/crypto.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <mach/cs75xx_fe_core_table.h>
#include <mach/cs_network_types.h>
#include <mach/cs_route_api.h>
#include <mach/cs_rule_hash_api.h>
#include "cs_core_logic.h"
#include "cs_core_vtable.h"
#include "cs_hw_accel_manager.h"
#include "cs_fe.h"
#include "cs_hw_accel_tunnel.h"
#include "cs_hw_accel_sa_id.h"
#include <mach/cs_flow_api.h>
#include "cs_core_hmu.h"
#include <linux/socket.h>
#include <linux/l2tp.h>
#include <linux/in.h>
#include <linux/ppp_defs.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include "l2tp_core.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"

extern u32 cs_adapt_debug;
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#else
#define DBG(x)	{ }
#endif /* CONFIG_CS752X_PROC */

#define SKIP(x) { }
#define ERR(x)	(x)

extern struct net_device *ni_get_device(unsigned char port_id);

static cs_uint32_t cs_ipsec_pseudo_policy_id;

int cs_ipsec_ctrl_enable(void)
{
	if (cs_accel_kernel_module_enable(CS752X_ADAPT_ENABLE_IPSEC_CTRL) > 0)
		return TRUE;
	else
		return FALSE;
}

static cs_status_t cs_ipsec_get_ealg(unsigned char *alg_name, cs_uint8_t *ealg)
{
	unsigned char name[10][20] = { "ecb(cipher_null)", "cipher_null",
		"cbc(des)", "des", "cbc(des3_ede)", "des3_ede",
		"aes", "cbc(aes)", "rfc4106(gcm(aes))", "rfc4309(ccm(aes))"
	};
	cs_uint8_t alg_num[10] = { CS_IPSEC_CIPHER_NULL, CS_IPSEC_CIPHER_NULL,
		CS_IPSEC_DES, CS_IPSEC_DES, CS_IPSEC_3DES, CS_IPSEC_3DES,
		CS_IPSEC_AES, CS_IPSEC_AES, CS_IPSEC_AES, CS_IPSEC_AES
	};
	cs_uint8_t iii;

	for (iii = 0; iii < 10; iii++) {
		if (strncmp(alg_name, &name[iii][0], 20) == 0) {
			*ealg = alg_num[iii];
			return CS_OK;
		}
	}

	printk("%s:%d: Unknown IPSec encryption algorithm: %s\n",
		__func__, __LINE__, alg_name);
	
	*ealg = CS_IPSEC_CIPHER_NULL;
	return CS_E_NOT_SUPPORT;
}

static cs_status_t cs_ipsec_get_ealg_mode(u8 alg_mode, cs_uint8_t *ealg_mode)
{
	switch (alg_mode) {
	case SADB_EALG_NULL:
		*ealg_mode = CS_IPSEC_CIPHER_ECB;
		return CS_OK;
	case SADB_EALG_DESCBC:
	case SADB_EALG_3DESCBC:
	case SADB_X_EALG_AESCBC:
		*ealg_mode = CS_IPSEC_CIPHER_CBC;
		return CS_OK;
	case SADB_X_EALG_AESCTR:
		*ealg_mode = CS_IPSEC_CIPHER_CTR;
		return CS_OK;
	case SADB_X_EALG_AES_CCM_ICV8:
	case SADB_X_EALG_AES_CCM_ICV12:
	case SADB_X_EALG_AES_CCM_ICV16:
		*ealg_mode = CS_IPSEC_CIPHER_CCM;
		return CS_OK;
	case SADB_X_EALG_AES_GCM_ICV8:
	case SADB_X_EALG_AES_GCM_ICV12:
	case SADB_X_EALG_AES_GCM_ICV16:
		*ealg_mode = CS_IPSEC_CIPHER_GCM;
		return CS_OK;
	default:
		printk("%s:%d: Unknown IPSec encryption algorithm mode: %d\n",
			__func__, __LINE__, alg_mode);
		*ealg_mode = CS_IPSEC_CIPHER_ECB;
		return CS_E_NOT_SUPPORT;
	}
}

static cs_status_t cs_ipsec_get_aalg(unsigned char *alg_name, cs_uint8_t *aalg)
{
	unsigned char name[5][16] = { "md5", "sha1", "sha224", "sha256",
		"digest_null"
	};
	unsigned char name_hmac[5][20] = { "hmac(md5)", "hmac(sha1)",
		"hmac(sha224)", "hmac(sha256)", "hmac(digest_null)"
	};
	cs_uint8_t alg_num[5] = { CS_IPSEC_MD5, CS_IPSEC_SHA1, CS_IPSEC_SHA224,
		CS_IPSEC_SHA256, CS_IPSEC_AUTH_NULL
	};
	cs_uint8_t iii;

	for (iii = 0; iii < 5; iii++) {
		if (strncmp(alg_name, &name[iii][0], 16) == 0) {
			*aalg = alg_num[iii];
			return CS_OK;
		}
		if (strncmp(alg_name, &name_hmac[iii][0], 20) == 0) {
			*aalg = alg_num[iii];
			return CS_OK;
		}
	}

	printk("%s:%d: Unknown IPSec authentication algorithm: %s\n",
		__func__, __LINE__, alg_name);
	
	*aalg = CS_IPSEC_AUTH_NULL;
	return CS_E_NOT_SUPPORT;
}

static void cs_ipsec_pseudo_policy_construct(cs_ipsec_policy_t *p)
{
	p->policy_id = ++cs_ipsec_pseudo_policy_id;
	p->policy_action = CS_IPSEC_POLICY_ACTION_IPSEC;
	p->selector_count = 1;
	memset(&p->selector_array[0], 0, sizeof(cs_ipsec_selector_t));
}

static cs_status_t cs_ipsec_sa_construct(struct xfrm_state *x, cs_kernel_accel_cb_t *cs_cb, cs_ipsec_sa_t *sa)
{
	struct esp_data *esp;
	struct crypto_aead *aead;
	struct xfrm_algo_desc *ealg_desc;
	struct xfrm_algo_desc *aalg_desc;
	cs_uint8_t tmp;
	cs_status_t ret;
	struct cb_network_field *cb_net;

	sa->replay_window = x->props.replay_window;

	/* sa->spi should be host order, 
	   and we expect cs_ipsec_sa_add() will swap it again */
	sa->spi = ntohl(x->id.spi);

	/* the seq_num is for the next packet */
	sa->seq_num = x->replay.oseq + 1;
	sa->ip_ver = cs_cb->common.vpn_ip_ver;
	
	/* IPPROTO_ESP = 50 => sa->proto = 0
	 * IPPOROT_AH = 51  => sa->proto = 1 */
	sa->proto = x->id.proto - IPPROTO_ESP;

	switch (x->props.mode) {
	case XFRM_MODE_TRANSPORT:
		sa->tunnel = 1; /* transport */
		break;
	case XFRM_MODE_TUNNEL:
		sa->tunnel = 0; /* tunnel */
		break;
	default:
		printk("%s:%d: Unsupported IPSec mode %d\n",
			__func__, __LINE__, x->props.mode);
	}
	sa->sa_dir = cs_cb->common.vpn_dir;

	sa->etherIP = 0;
	sa->lifetime_bytes = MIN(x->lft.hard_byte_limit, x->lft.soft_byte_limit);
	sa->bytes_count = x->curlft.bytes;
	sa->lifetime_packets = MIN(x->lft.hard_packet_limit,
					x->lft.soft_packet_limit);
	sa->packets_count = x->curlft.packets;

	/* check if it is NAT-T */
	if (cs_cb->common.vpn_dir == 0 /* outbound */)
		cb_net = &cs_cb->output;
	else /* inbound */
		cb_net = &cs_cb->input;

	DBG(printk("%s:%d: Dir %d, IP protocol %d, L4 sport %d, dport %d\n",
		__func__, __LINE__,
		cs_cb->common.vpn_dir, cb_net->l3_nh.iph.protocol,
		ntohs(cb_net->l4_h.uh.sport), ntohs(cb_net->l4_h.uh.dport)));
	
	/* no matter IPv4 or IPv6, the offset of protocol field is the same. */ 
	if (cb_net->l3_nh.iph.protocol == IPPROTO_UDP && 
		(cb_net->l4_h.uh.sport == htons(4500) ||
		cb_net->l4_h.uh.dport == htons(4500))) {
		sa->is_natt = 1;
		sa->natt_ingress_src_port = ntohs(cb_net->l4_h.uh.sport);
		sa->natt_egress_dest_port = ntohs(cb_net->l4_h.uh.dport);
	} else {
		sa->is_natt = 0;
		sa->natt_ingress_src_port = 0;
		sa->natt_egress_dest_port = 0;
	}

	if (x->id.proto != IPPROTO_ESP) {
		printk("%s:%d: Unsupported IPSec protocol %d\n",
			__func__, __LINE__, x->id.proto);
		return CS_E_NOT_SUPPORT;
	}
	
	/* IPSec ESP */
	esp = (struct esp_data *)x->data;
	aead = esp->aead;

	sa->iv_len = crypto_aead_ivsize(aead) >> 2;

	if (x->aead) {
		if ((ret = cs_ipsec_get_ealg(x->aead->alg_name, &tmp)) == CS_OK)
			sa->ealg = tmp;
		else
			return ret;
		if ((ret = cs_ipsec_get_ealg_mode(x->props.ealgo, &tmp)) == CS_OK)
			sa->ealg_mode = tmp;
		else
			return ret;
		sa->enc_keylen = (x->aead->alg_key_len + 7) >> 5;
		memcpy(sa->ekey, x->aead->alg_key, (sa->enc_keylen << 2));

		/* in AEAD mode, there is no authentication algorithm */
		sa->aalg = CS_IPSEC_AUTH_NULL;
		memset(sa->akey, 0x00, MAX_AUTH_KEY_LEN);
		sa->auth_keylen = 0;

		sa->icv_trunclen = crypto_aead_authsize(aead);
		DBG(printk("%s:%d:x->aead->alg_icv_len %d"
		     " vs aead_authsize %d\n",
		     __func__, __LINE__,
		     x->aead->alg_icv_len >> 5,
		     crypto_aead_authsize(aead)));
	} else {
		if ((ret = cs_ipsec_get_ealg(x->ealg->alg_name, &tmp)) == CS_OK)
			sa->ealg = tmp;
		else
			return ret;
		if ((ret = cs_ipsec_get_ealg_mode(x->props.ealgo, &tmp)) == CS_OK)
			sa->ealg_mode = tmp;
		else
			return ret;
		sa->enc_keylen = (x->ealg->alg_key_len + 7) >> 5;
		memcpy(sa->ekey, x->ealg->alg_key, sa->enc_keylen << 2);

		/* get IV length */
		ealg_desc = xfrm_ealg_get_byname(x->ealg->alg_name, 0);
		if (x->aalg) {
			if ((ret = cs_ipsec_get_aalg(x->aalg->alg_name, &tmp)) == CS_OK)
				sa->aalg = tmp;
			else
				return ret;
			sa->auth_keylen = (x->aalg->alg_key_len + 7) >> 5;
			memcpy(sa->akey, x->aalg->alg_key, sa->auth_keylen << 2);

			/* get ICV (integrity check value) truncated length */
			aalg_desc = xfrm_aalg_get_byname(x->aalg->alg_name, 0);
			if (NULL == aalg_desc) {
				printk("%s:%d: Can't get aalg name = %s\n",
					__func__, __LINE__, x->aalg->alg_name);
				return CS_E_NOT_FOUND;
			}

			sa->icv_trunclen = aalg_desc->uinfo.auth.icv_truncbits >> 5;
		} else {
			sa->aalg = CS_IPSEC_AUTH_NULL;
			memset(sa->akey, 0x00, MAX_AUTH_KEY_LEN);
			sa->auth_keylen = 0;
			sa->icv_trunclen = 0;
		}
	}
	DBG(printk("%s:%d:ealg %x, ealg_mode %x, enc_keylen %x, iv_len %x, ",
		__func__, __LINE__, sa->ealg, sa->ealg_mode,
		sa->enc_keylen, sa->iv_len));
	DBG(printk("aalg %x, auth_keylen %x, icv_trunclen %x\n", sa->aalg,
		sa->auth_keylen, sa->icv_trunclen));


	/* common parameters */
	/* IP address should be network order */
	if (sa->ip_ver == 0 /* IPv4 */) {
		sa->tunnel_saddr.addr[0] = x->props.saddr.a4;
		sa->tunnel_daddr.addr[0] = x->id.daddr.a4;

		DBG(printk("%s:%d:IP address sa %pI4 da %pI4\n",
			__func__, __LINE__,
			&sa->tunnel_saddr.addr[0], &sa->tunnel_daddr.addr[0]));

	} else { /* IPv6 */
		*(struct in6_addr *)&sa->tunnel_saddr.addr =
			*(struct in6_addr *)&x->props.saddr.a6;
		*(struct in6_addr *)&sa->tunnel_daddr.addr =
			*(struct in6_addr *)&x->id.daddr.a6;

		DBG(printk("%s:%d:IP address sa %pI6 da %pI6\n",
			__func__, __LINE__,
			&sa->tunnel_saddr.addr[0], &sa->tunnel_daddr.addr[0]));

	}


	return CS_OK;
}

static cs_status_t cs_ipsec_nexthop_construct(struct sk_buff *skb,
	cs_tunnel_dir_t dir, cs_l3_nexthop_t *n)
{
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);

	/* Only MAC address and IP address should be network order.
	   Other fields are all host order */
	n->nhid.nhop_type = CS_L3_NEXTHOP_DIRECT;

	if (dir == CS_TUNNEL_DIR_OUTBOUND) {
		/* WAN ifindex */
		n->nhid.intf_id = skb->dev->ifindex;
		
		/* L3 IP */
		if (cs_cb->output.l3_nh.iph.ver == 4) {
			n->nhid.addr.afi = CS_IPV4;
			n->nhid.addr.ip_addr.ipv4_addr = cs_cb->output.l3_nh.iph.dip;
			n->nhid.addr.addr_len = 32;
		} else if (cs_cb->output.l3_nh.iph.ver == 6) {
			n->nhid.addr.afi = CS_IPV6;
			n->nhid.addr.ip_addr.ipv6_addr[0] = cs_cb->output.l3_nh.ipv6h.dip[0];
			n->nhid.addr.ip_addr.ipv6_addr[1] = cs_cb->output.l3_nh.ipv6h.dip[1];
			n->nhid.addr.ip_addr.ipv6_addr[2] = cs_cb->output.l3_nh.ipv6h.dip[2];
			n->nhid.addr.ip_addr.ipv6_addr[3] = cs_cb->output.l3_nh.ipv6h.dip[3];
			n->nhid.addr.addr_len = 128;
		} else {
			printk("%s:%d: Unsupported IP ver %d\n",
				__func__, __LINE__, cs_cb->output.l3_nh.iph.ver);
			return CS_E_NOT_SUPPORT;
		}

		/* DA MAC */
		memcpy(n->da_mac, cs_cb->output.raw.da, CS_ETH_ADDR_LEN);

		/* port ID */
		n->id.port_id = cs_cb->common.egress_port_id;


		/* port encap */
		if (cs_cb->common.module_mask & CS_MOD_MASK_PPPOE) {
			n->encap.port_encap.type = CS_PORT_ENCAP_PPPOE_E;

			n->encap.port_encap.port_encap.pppoe.pppoe_session_id =
				ntohs(cs_cb->output.raw.pppoe_frame);

			n->encap.port_encap.port_encap.pppoe.tag[0] = 
				cs_cb->output.raw.vlan_tci & VLAN_VID_MASK;

			n->encap.port_encap.port_encap.pppoe.tag[1] = 
				cs_cb->output.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.pppoe.dest_mac,
				cs_cb->output.raw.da, CS_ETH_ADDR_LEN);
			memcpy(n->encap.port_encap.port_encap.pppoe.src_mac,
				cs_cb->output.raw.sa, CS_ETH_ADDR_LEN);
				
		} else {
			if (cs_cb->output.raw.vlan_tci_2 > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_QinQ_E;
			else if (cs_cb->output.raw.vlan_tci > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_1Q_E;
			else
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_E;
			
			n->encap.port_encap.port_encap.eth.tag[0] =
				cs_cb->output.raw.vlan_tci & VLAN_VID_MASK;
			n->encap.port_encap.port_encap.eth.tag[1] =
				cs_cb->output.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.eth.src_mac,
				cs_cb->output.raw.sa, CS_ETH_ADDR_LEN);
		}
	} else if (dir == CS_TUNNEL_DIR_INBOUND) {
		/* WAN ifindex */
		n->nhid.intf_id = cs_cb->common.in_ifindex;
		
		/* L3 IP */
		if (cs_cb->input.l3_nh.iph.ver == 4) {
			n->nhid.addr.afi = CS_IPV4;
			n->nhid.addr.ip_addr.ipv4_addr = cs_cb->input.l3_nh.iph.sip;
			n->nhid.addr.addr_len = 32;
		} else if (cs_cb->input.l3_nh.iph.ver == 6) {
			n->nhid.addr.afi = CS_IPV6;
			n->nhid.addr.ip_addr.ipv6_addr[0] = cs_cb->input.l3_nh.ipv6h.sip[0];
			n->nhid.addr.ip_addr.ipv6_addr[1] = cs_cb->input.l3_nh.ipv6h.sip[1];
			n->nhid.addr.ip_addr.ipv6_addr[2] = cs_cb->input.l3_nh.ipv6h.sip[2];
			n->nhid.addr.ip_addr.ipv6_addr[3] = cs_cb->input.l3_nh.ipv6h.sip[3];
			n->nhid.addr.addr_len = 128;
		} else {
			printk("%s:%d: Unsupported IP ver %d\n",
				__func__, __LINE__, cs_cb->input.l3_nh.iph.ver);
			return CS_E_NOT_SUPPORT;
		}

		/* DA MAC */
		memcpy(n->da_mac, cs_cb->input.raw.sa, CS_ETH_ADDR_LEN);

		/* port ID */
		n->id.port_id = cs_cb->common.ingress_port_id;


		/* port encap */
		if (cs_cb->common.module_mask & CS_MOD_MASK_PPPOE) {
			n->encap.port_encap.type = CS_PORT_ENCAP_PPPOE_E;

			n->encap.port_encap.port_encap.pppoe.pppoe_session_id =
				ntohs(cs_cb->input.raw.pppoe_frame);

			n->encap.port_encap.port_encap.pppoe.tag[0] = 
				cs_cb->input.raw.vlan_tci & VLAN_VID_MASK;

			n->encap.port_encap.port_encap.pppoe.tag[1] = 
				cs_cb->input.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.pppoe.dest_mac,
				cs_cb->input.raw.sa, CS_ETH_ADDR_LEN);
			memcpy(n->encap.port_encap.port_encap.pppoe.src_mac,
				cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
				
		} else {
			if (cs_cb->input.raw.vlan_tci_2 > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_QinQ_E;
			else if (cs_cb->input.raw.vlan_tci > 0)
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_1Q_E;
			else
				n->encap.port_encap.type = CS_PORT_ENCAP_ETH_E;
			
			n->encap.port_encap.port_encap.eth.tag[0] =
				cs_cb->input.raw.vlan_tci & VLAN_VID_MASK;
			n->encap.port_encap.port_encap.eth.tag[1] =
				cs_cb->input.raw.vlan_tci_2 & VLAN_VID_MASK;

			memcpy(n->encap.port_encap.port_encap.eth.src_mac,
				cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
		}
	} else {
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return CS_E_NOT_SUPPORT;
	}

	return CS_OK;
}

static cs_status_t cs_ipsec_t_cfg_construct(cs_kernel_accel_cb_t *cs_cb,
	cs_tunnel_dir_t dir, cs_uint32_t nexthop_id, cs_uint32_t policy_handle,
	cs_tunnel_cfg_t *t_cfg)
{
	struct cb_network_field *cb_net;
	
	t_cfg->type = CS_IPSEC;
	t_cfg->tx_port = 0;
	t_cfg->nexthop_id = nexthop_id;
	t_cfg->dir = dir;
	t_cfg->tunnel.ipsec.ipsec_policy = policy_handle;

	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		cb_net = &cs_cb->output;
		break;
	case CS_TUNNEL_DIR_INBOUND:
		cb_net = &cs_cb->input;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return CS_E_NOT_SUPPORT;
	}

	/* IP address should be network order */
	if (cb_net->l3_nh.iph.ver == 4) {
		t_cfg->dest_addr.afi = CS_IPV4;
		t_cfg->dest_addr.ip_addr.ipv4_addr = cb_net->l3_nh.iph.dip;
		t_cfg->dest_addr.addr_len = 32;
	
		t_cfg->src_addr.afi = CS_IPV4;
		t_cfg->src_addr.ip_addr.ipv4_addr = cb_net->l3_nh.iph.sip;
		t_cfg->src_addr.addr_len = 32;
	} else if (cb_net->l3_nh.iph.ver == 6) {
		t_cfg->dest_addr.afi = CS_IPV6;
		t_cfg->dest_addr.ip_addr.ipv6_addr[0] = cb_net->l3_nh.ipv6h.dip[0];
		t_cfg->dest_addr.ip_addr.ipv6_addr[1] = cb_net->l3_nh.ipv6h.dip[1];
		t_cfg->dest_addr.ip_addr.ipv6_addr[2] = cb_net->l3_nh.ipv6h.dip[2];
		t_cfg->dest_addr.ip_addr.ipv6_addr[3] = cb_net->l3_nh.ipv6h.dip[3];
		t_cfg->dest_addr.addr_len = 128;
	
		t_cfg->src_addr.afi = CS_IPV6;
		t_cfg->src_addr.ip_addr.ipv6_addr[0] = cb_net->l3_nh.ipv6h.sip[0];
		t_cfg->src_addr.ip_addr.ipv6_addr[1] = cb_net->l3_nh.ipv6h.sip[1];
		t_cfg->src_addr.ip_addr.ipv6_addr[2] = cb_net->l3_nh.ipv6h.sip[2];
		t_cfg->src_addr.ip_addr.ipv6_addr[3] = cb_net->l3_nh.ipv6h.sip[3];
		t_cfg->src_addr.addr_len = 128;
	} else {
		printk("%s:%d: Unsupported IP ver %d\n",
			__func__, __LINE__, cb_net->l3_nh.iph.ver);
		return CS_E_NOT_SUPPORT;
	}
	
	return CS_OK;
}

static void cs_ipsec_revert_ipip_on_skb(struct sk_buff *skb)
{
	struct iphdr *p_ip = ip_hdr(skb);
	struct ipv6hdr *p_ip6 = ipv6_hdr(skb);
	/* at this point ip_hdr will point to out_ip */

	/* if outer IP header is not IPIP, then we don't have to worry about
	 * reverting it back to an IP packet */
	if (p_ip->version == 4) {
		if (p_ip->protocol != IPPROTO_IPIP)
			return;
	} else {
		if(p_ip6->nexthdr != NEXTHDR_IPV6)
			return;
	}

	/* however skb->data still points to the inner IP, we will just have to
	 * reset the network header to the inner IP */
	skb_reset_network_header(skb);

	return;
}

static void cs_ipsec_insert_mac_hdr_on_skb(
	struct sk_buff					*skb,
	cs_tunnel_entry_t 				*t)
{
	struct ethhdr *p_eth;
	struct iphdr *p_ip = (struct iphdr *)skb->data;
	struct net_device *dev;

	/* Current skb is an IP packet without proper L2 info.
	 * To be able to send it to IPsec Offload Engine,
	 * we need to construct a L2 ETH header.
	 */
	skb_push(skb, ETH_HLEN);

	/* (new skb->data) DA(6):SA(6):Ethertype(2): (old skb->data) */
	p_eth = (struct ethhdr *)skb->data;

	if (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND)
		dev = ni_get_device(CS_PORT_GMAC1);
	else
		dev = ni_get_device(CS_PORT_GMAC0);
		
	if (dev) {
		DBG(printk("%s:%d: new DA MAC = %pM\n",
			__func__, __LINE__, dev->dev_addr));
		memcpy(p_eth->h_dest, dev->dev_addr, ETH_HLEN);
	}

	if (p_ip->version == 4)
		p_eth->h_proto = __constant_htons(ETH_P_IP);
	else if (p_ip->version == 6)
		p_eth->h_proto = __constant_htons(ETH_P_IPV6);

	/* move the MAC header to current skb->data */
	skb_reset_mac_header(skb);

}

cs_status_t cs_ipsec_insert_sec_path(
	struct sk_buff					*skb,
	cs_ipsec_policy_node_t				*p)
{
	struct xfrm_state *x;
	cs_uint32_t *daddr;
	cs_uint16_t family;
	struct sec_path *sp;

	daddr = p->sa.tunnel_daddr.addr;

	
	if (p->sa.ip_ver == 0 /* IPv4 */) {
		family = AF_INET;
	} else { /* IPv6 */
		family = AF_INET6;
	}

	x = (struct xfrm_state *) p->xfrm_state;
#if 0
	/* try to get xfrm state */
	x = xfrm_state_lookup(net, skb->mark, (const xfrm_address_t *) daddr,
			      p->sa.spi, IPPROTO_ESP, family);
	if (!x) {
		printk("%s:%d: Can't get xfrm_state!! IP %pI4, SPI 0x%x\n",
			__func__, __LINE__, daddr, p->sa.spi);
		return CS_E_NOT_FOUND;
	} else {
		DBG(printk("%s:%d: Get xfrm_state!! IP %pI4, SPI 0x%x\n",
			__func__, __LINE__, daddr, p->sa.spi));
		if (x->km.state != XFRM_STATE_VALID) {
			printk("%s:%d: Invalid xfrm state, SPI 0x%x\n",
				__func__, __LINE__, p->sa.spi);
			return CS_E_CONFLICT;
		}
	}
#endif

	if (!skb->sp || atomic_read(&skb->sp->refcnt) != 1) {

		sp = secpath_dup(skb->sp);
		if (!sp) {
			printk("%s:%d: Can't allocate sec_path , SPI 0x%x\n",
				__func__, __LINE__, p->sa.spi);
			return CS_E_MEM_ALLOC;
		}
		if (skb->sp)
			secpath_put(skb->sp);
		skb->sp = sp;
	}

	if (skb->sp->len == XFRM_MAX_DEPTH) {
		printk("%s:%d: No available sp->xvec , SPI 0x%x\n",
			__func__, __LINE__, p->sa.spi);
		secpath_put(skb->sp);
		return CS_E_RESOURCE;
	}

	xfrm_state_hold(x);
	skb->sp->xvec[skb->sp->len++] = x;

	return CS_E_OK;
}


static void cs_ipsec_tx_skb_to_pe(
	struct sk_buff					*skb,
	cs_tunnel_entry_t 				*t,
	cs_ipsec_policy_node_t				*p)
{
	int ret;
	int len;
	cs_tunnel_dir_t dir;
	int ipsec_mode;
	struct iphdr *iph;
	cs_uint8_t protocol;
	cs_uint16_t tmp;


	/* if this skb is hijacked from kernel, the new ip header has been
	 * inserted, so we will have to remove it! */

	if (skb == NULL || t == NULL || p == NULL)
		return;

	dir = t->tunnel_cfg.dir;
	ipsec_mode = p->sa.tunnel; /* 0:tunnel, 1: transport */
	
	if (t->tunnel_cfg.dir == CS_TUNNEL_DIR_INBOUND) {
		SKIP(printk("%s:%d: dir = %d, data = 0x%p, len = %d, network ptr = 0x%p,"
			" network hdr len = %d, mac ptr = 0x%p, mac_len = %d\n",
			__func__, __LINE__,
			t->tunnel_cfg.dir,
			skb->data, skb->len,
			skb_network_header(skb), skb_network_header_len(skb),
			skb_mac_header(skb), skb->mac_len));
		SKIP(cs_dump_data(skb_mac_header(skb),skb->len));
	}

	if (dir == CS_TUNNEL_DIR_OUTBOUND && ipsec_mode == 0 /* tunnel */) {
		
		/*
		 * For outbound IPSec tunnel mode,
		 * the new ip header has been inserted,
		 * but skb->data still points to the inner IP.
		 * We should construct a ethernet packet,
		 * reset skb->network_header,
		 * and reset skb->mac_header based on skb->data.
		 */
		
		cs_ipsec_revert_ipip_on_skb(skb);

		/* convert the packet to an ethernet packet and send it to PE */
		cs_ipsec_insert_mac_hdr_on_skb(skb, t);

	} else if (dir == CS_TUNNEL_DIR_OUTBOUND && ipsec_mode == 1 /* transport */) {
		/*
		 * For outbound IPSec transport mode,
		 * the skb->data still points to the UDP-L2TP-PPP-IP header,
		 * but the outer IP header exists in front of skb->data.
		 * There are 2 network header in front of skb->data.
		 * [network header][4 bytes][network header][skb->data]...
		 * skb->network_header points to the 1st network header.
		 * skb->mac_header points IP protocol of the 1st network header.
		 * We should construct a ethernet packet,
		 * reset skb->network_header,
		 * and reset skb->mac_header based on skb->data.
		 */
		SKIP(printk("%s:%d: outbound IPSec transport mode\n",
			__func__, __LINE__));
		SKIP(printk("%s:%d: dir = %d, data = 0x%p, len = %d, network ptr = 0x%p,"
			" network hdr len = %d, mac ptr = 0x%p, mac_len = %d\n",
			__func__, __LINE__,
			t->tunnel_cfg.dir,
			skb->data, skb->len,
			skb_network_header(skb), skb_network_header_len(skb),
			skb_mac_header(skb), skb->mac_len));
		SKIP(cs_dump_data(skb_network_header(skb), 80));
		len = skb_network_header_len(skb);
		skb_push(skb, len);
		
		skb_reset_network_header(skb);

		/* Due to PE doesn't update IP checksum after IPSec encryption,
		 * we need to pre-calculate the checksume for the new protocol.
		 */
		/*pre-calc ip_checksum for the ecrypted pkt if IPv4*/
		iph = (struct iphdr *) skb->network_header;
		if(iph->version == 4)
		{
			protocol = iph->protocol;
			tmp = iph->check;
			
			if (p->sa.is_natt != 0)
				iph->protocol = IPPROTO_UDP;
			else
				iph->protocol = p->sa.proto ? IPPROTO_AH : IPPROTO_ESP;

			ip_send_check(iph);
			SKIP(printk("%s:%d: \n\t old protocol %d, checksum 0x%x,"
				"\n\t new protocol %d, checksume 0x%x\n",
				__func__, __LINE__,
				protocol, ntohs(tmp),
				iph->protocol, ntohs(iph->check)));
			iph->protocol = protocol;
		}

		/* convert the packet to an ethernet packet and send it to PE */
		cs_ipsec_insert_mac_hdr_on_skb(skb, t);
	} else if (dir == CS_TUNNEL_DIR_INBOUND && ipsec_mode == 0 /* tunnel */) {
		/* 
		 * For inbound IPSec tunnel mode,
		 * skb->data already points to ESP header,
		 * but skb->mac_header is still correct.
		 * We should change skb->data and skb->len to correct values.
		 */
		len = (cs_uint32_t) (skb->data - skb_mac_header(skb));

		skb_push(skb, len);
	} else {
		/* dir == CS_TUNNEL_DIR_INBOUND && ipsec_mode == transport */
		/* 
		 * For inbound IPSec transport mode,
		 * skb->data already points to ESP header,
		 * but skb->mac_header is still correct.
		 * We should change skb->data and skb->len to correct values.
		 */
		 
		SKIP(printk("%s:%d: inbound IPSec transport mode\n",
			__func__, __LINE__));
		SKIP(printk("%s:%d: dir = %d, data = 0x%p, len = %d, network ptr = 0x%p,"
			" network hdr len = %d, mac ptr = 0x%p, mac_len = %d\n",
			__func__, __LINE__,
			t->tunnel_cfg.dir,
			skb->data, skb->len,
			skb_network_header(skb), skb_network_header_len(skb),
			skb_mac_header(skb), skb->mac_len));
		SKIP(cs_dump_data(skb_network_header(skb),80));

		len = (cs_uint32_t) (skb->data - skb_mac_header(skb));
		
		skb_push(skb, len);
	}
	
	len = skb->len;
	
	ret = cs_tunnel_tx_frame_to_pe(skb_mac_header(skb), len,
			t->tunnel_id, t->tunnel_cfg.type, t->tunnel_cfg.dir);

	if (ret != CS_OK) {
		ERR(printk("%s:%d: can't send pkt to PE\n", __func__, __LINE__));
	}

	return;
}

static void cs_ipsec_lan2pe_flow_hash_add(
	cs_kernel_accel_cb_t 				*cs_cb,
	cs_tunnel_entry_t 				*t)
{
	cs_flow_t lan2pe_flow;
	cs_pkt_info_t *in_pkt, *out_pkt;
	int crc32;
	int ret;

	/* 1. Redirect packets to PE#1.
	 * 2. Replace SA MAC with extra information.
	 * 3. Keep other fields, including VLAN
	 */
	/* Only MAC address and IP address should be network order.
	   Other fields are all host order */
	memset(&lan2pe_flow, 0, sizeof(cs_flow_t));
	in_pkt = &lan2pe_flow.ingress_pkt;
	out_pkt = &lan2pe_flow.egress_pkt;
	
	/* 1. construct lan2pe_flow */
	lan2pe_flow.flow_type = CS_FLOW_TYPE_L4;

	/* L2 */
	/* CS_HM_MAC_DA_MASK | CS_HM_MAC_SA_MASK | CS_HM_ETHERTYPE_MASK */
	memcpy(in_pkt->sa_mac, cs_cb->input.raw.sa, CS_ETH_ADDR_LEN);
	memcpy(in_pkt->da_mac, cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
	in_pkt->eth_type = ntohs(cs_cb->input.raw.eth_protocol);

	/* VLAN */
	/* CS_HM_VID_1_MASK | CS_HM_VID_2_MASK | 
	   CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK | 
	   CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK | 
	   CS_HM_8021P_1_MASK | CS_HM_8021P_2_MASK |
	   CS_HM_DEI_1_MASK | CS_HM_DEI_2_MASK
	 */
	switch (cs_cb->input.raw.vlan_tpid) {
	case ETH_P_8021Q:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[0].vlan_id = cs_cb->input.raw.vlan_tci & 0xFFF;

	switch (cs_cb->input.raw.vlan_tpid_2) {
	case ETH_P_8021Q:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[1].vlan_id = cs_cb->input.raw.vlan_tci_2 & 0xFFF;

	/* PPPoE */
	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	in_pkt->pppoe_session_id_valid = cs_cb->input.raw.pppoe_frame_vld;
	in_pkt->pppoe_session_id = ntohs(cs_cb->input.raw.pppoe_frame);

	/* IP */
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK | CS_HM_IP_PROT_MASK | 
	   CS_HM_IP_VER_MASK | CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |
	   CS_HM_DSCP_MASK  | CS_HM_IP_FRAGMENT_MASK | 
	   CS_HM_IPV6_RH_MASK | CS_HM_IPV6_DOH_MASK | CS_HM_IPV6_NDP_MASK | 
	   CS_HM_IPV6_HBH_MASK
	 */
	if (cs_cb->input.l3_nh.iph.ver == 6) {
		in_pkt->sa_ip.afi = CS_IPV6;
		in_pkt->sa_ip.ip_addr.addr[0] = cs_cb->input.l3_nh.ipv6h.sip[0];
		in_pkt->sa_ip.ip_addr.addr[1] = cs_cb->input.l3_nh.ipv6h.sip[1];
		in_pkt->sa_ip.ip_addr.addr[2] = cs_cb->input.l3_nh.ipv6h.sip[2];
		in_pkt->sa_ip.ip_addr.addr[3] = cs_cb->input.l3_nh.ipv6h.sip[3];

		in_pkt->da_ip.ip_addr.addr[0] = cs_cb->input.l3_nh.ipv6h.dip[0];
		in_pkt->da_ip.ip_addr.addr[1] = cs_cb->input.l3_nh.ipv6h.dip[1];
		in_pkt->da_ip.ip_addr.addr[2] = cs_cb->input.l3_nh.ipv6h.dip[2];
		in_pkt->da_ip.ip_addr.addr[3] = cs_cb->input.l3_nh.ipv6h.dip[3];
	} else {
		in_pkt->sa_ip.afi = CS_IPV4;
		in_pkt->sa_ip.ip_addr.ipv4_addr = cs_cb->input.l3_nh.iph.sip;
		in_pkt->da_ip.ip_addr.ipv4_addr = cs_cb->input.l3_nh.iph.dip;
	}
	
	in_pkt->protocol = cs_cb->input.l3_nh.iph.protocol;
	in_pkt->tos = cs_cb->input.l3_nh.iph.tos;
	
	/* L4 */
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK | 
	   CS_HM_TCP_CTRL_MASK |
	   CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK
	 */
	switch (in_pkt->protocol) {
	case IPPROTO_TCP:
		in_pkt->l4_header.tcp.dport = ntohs(cs_cb->input.l4_h.th.dport); 
		in_pkt->l4_header.tcp.sport = ntohs(cs_cb->input.l4_h.th.sport);
		break;
	case IPPROTO_UDP:
		in_pkt->l4_header.udp.dport = ntohs(cs_cb->input.l4_h.uh.dport);
		in_pkt->l4_header.udp.sport = ntohs(cs_cb->input.l4_h.uh.sport);
		break;
	default:
		DBG(printk("%s:%d: unexpected protocol = %d\n",
			__func__, __LINE__, in_pkt->protocol));
		return;
	}

	/* set egress packet contents */
	memcpy(out_pkt, in_pkt, sizeof(cs_pkt_info_t));

	/* physical port */
	in_pkt->phy_port = cs_cb->common.ingress_port_id;
	out_pkt->phy_port = CS_PORT_OFLD1;

	/* fill the source MAC with the host order */
	out_pkt->sa_mac[1] = t->tunnel_cfg.type & 0xFF;
	out_pkt->sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	out_pkt->sa_mac[3] = t->sa_id & 0xFF;
	out_pkt->sa_mac[4] = (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND) ? 0 : 1;
	out_pkt->sa_mac[5] = 0; /* reserved */


	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &out_pkt->sa_mac[1], 5));
	/* Store 8 bits of the crc in the src MAC */
	out_pkt->sa_mac[0] = crc32 & 0xFF;

	/* set sw_id */
	if (cs_cb->input.l3_nh.iph.ver == 6) {
		lan2pe_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD];
	} else {
		lan2pe_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD];
	}

	lan2pe_flow.swid_array[CS_FLOW_SWID_VPN] = t->sw_id;
	DBG(printk("%s:%d: type %d, dir %d, sa_id %d, sw_id = 0x%0llx\n",
		__func__, __LINE__,
		t->tunnel_cfg.type, t->tunnel_cfg.dir, t->sa_id,t->sw_id));
	
	
	/* 2. create flow hash */
	ret = cs_flow_add(t->device_id, &lan2pe_flow);
	if (ret != CS_OK) {
		DBG(printk("%s:%d: cs_flow_add ret = %d\n",
			__func__, __LINE__, ret));
	}
	
	return;
}

static void cs_ipsec_pe2lan_flow_hash_add(
	cs_kernel_accel_cb_t 				*cs_cb,
	cs_tunnel_entry_t 				*t)
{
	cs_flow_t pe2lan_flow;
	cs_pkt_info_t *in_pkt, *out_pkt;
	int crc32;
	int ret;

	/* 1. WAN2PE hash will remove PPPoE header if it exists,
	 *    and we don't take care PPPoE header here.
	 * 2. SRC MAC will be modified by WAN2PE hash, but PE will keep it.
	 *    Therefore, we need to set the modified SRC MAC as key.
	 * 3. Other L2 and VLAN header will not modified by WAN2PE or PE.
	 *    Therefore, we also need to handle it.
	 */
	/* Only MAC address and IP address should be network order.
	   Other fields are all host order */
	memset(&pe2lan_flow, 0, sizeof(cs_flow_t));
	in_pkt = &pe2lan_flow.ingress_pkt;
	out_pkt = &pe2lan_flow.egress_pkt;
	
	/* 1. construct pe2lan_flow */
	pe2lan_flow.flow_type = CS_FLOW_TYPE_L4;

	/* L2 */
	/* CS_HM_MAC_DA_MASK | CS_HM_MAC_SA_MASK | CS_HM_ETHERTYPE_MASK */

	/* fill the source MAC with the host order */
	in_pkt->sa_mac[1] = t->tunnel_cfg.type & 0xFF;
	in_pkt->sa_mac[2] = 0; /* pkt_type: 0 : LAN->PE,  1: CPU->PE */
	in_pkt->sa_mac[3] = t->sa_id & 0xFF;
	in_pkt->sa_mac[4] = (t->tunnel_cfg.dir == CS_TUNNEL_DIR_OUTBOUND) ? 0 : 1;
	in_pkt->sa_mac[5] = 0; /* reserved */

	/* Calculate CRC over the MAC SA */
	crc32 = ~(calc_crc(~0, &in_pkt->sa_mac[1], 5));
	/* Store 8 bits of the crc in the src MAC */
	in_pkt->sa_mac[0] = crc32 & 0xFF;

	memcpy(in_pkt->da_mac, cs_cb->input.raw.da, CS_ETH_ADDR_LEN);
	in_pkt->eth_type = ntohs(cs_cb->input.raw.eth_protocol);

	/* VLAN */
	/* CS_HM_VID_1_MASK | CS_HM_VID_2_MASK | 
	   CS_HM_TPID_ENC_1_MSB_MASK | CS_HM_TPID_ENC_1_LSB_MASK | 
	   CS_HM_TPID_ENC_2_MSB_MASK | CS_HM_TPID_ENC_2_LSB_MASK | 
	   CS_HM_8021P_1_MASK | CS_HM_8021P_2_MASK |
	   CS_HM_DEI_1_MASK | CS_HM_DEI_2_MASK
	 */
	switch (cs_cb->input.raw.vlan_tpid) {
	case ETH_P_8021Q:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[0].vlan_id = cs_cb->input.raw.vlan_tci & 0xFFF;

	switch (cs_cb->input.raw.vlan_tpid_2) {
	case ETH_P_8021Q:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		in_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	in_pkt->tag[1].vlan_id = cs_cb->input.raw.vlan_tci_2 & 0xFFF;

	/* PPPoE */
	/* CS_HM_PPPOE_SESSION_ID_VLD_MASK | CS_HM_PPPOE_SESSION_ID_MASK */
	/* in_pkt->pppoe_session_id_valid =  0; */
	/* in_pkt->pppoe_session_id = 0; */

	/* IP */
	/* CS_HM_IP_SA_MASK | CS_HM_IP_DA_MASK | CS_HM_IP_PROT_MASK | 
	   CS_HM_IP_VER_MASK | CS_HM_IP_VLD_MASK | CS_HM_L3_CHKSUM_ERR_MASK |
	   CS_HM_DSCP_MASK  | CS_HM_IP_FRAGMENT_MASK | 
	   CS_HM_IPV6_RH_MASK | CS_HM_IPV6_DOH_MASK | CS_HM_IPV6_NDP_MASK | 
	   CS_HM_IPV6_HBH_MASK
	 */
	if (cs_cb->output.l3_nh.iph.ver == 6) {
		in_pkt->sa_ip.afi = CS_IPV6;
		in_pkt->sa_ip.ip_addr.addr[0] = cs_cb->output.l3_nh.ipv6h.sip[0];
		in_pkt->sa_ip.ip_addr.addr[1] = cs_cb->output.l3_nh.ipv6h.sip[1];
		in_pkt->sa_ip.ip_addr.addr[2] = cs_cb->output.l3_nh.ipv6h.sip[2];
		in_pkt->sa_ip.ip_addr.addr[3] = cs_cb->output.l3_nh.ipv6h.sip[3];

		in_pkt->da_ip.ip_addr.addr[0] = cs_cb->output.l3_nh.ipv6h.dip[0];
		in_pkt->da_ip.ip_addr.addr[1] = cs_cb->output.l3_nh.ipv6h.dip[1];
		in_pkt->da_ip.ip_addr.addr[2] = cs_cb->output.l3_nh.ipv6h.dip[2];
		in_pkt->da_ip.ip_addr.addr[3] = cs_cb->output.l3_nh.ipv6h.dip[3];	
	} else {
		in_pkt->sa_ip.afi = CS_IPV4;
		in_pkt->sa_ip.ip_addr.ipv4_addr = cs_cb->output.l3_nh.iph.sip;
		in_pkt->da_ip.ip_addr.ipv4_addr = cs_cb->output.l3_nh.iph.dip;
	}
	
	in_pkt->protocol = cs_cb->output.l3_nh.iph.protocol;
	in_pkt->tos = cs_cb->output.l3_nh.iph.tos;
	
	/* L4 */
	/* CS_HM_L4_DP_MASK | CS_HM_L4_SP_MASK | CS_HM_L4_VLD_MASK | 
	   CS_HM_TCP_CTRL_MASK |
	   CS_HM_SPI_VLD_MASK | CS_HM_SPI_MASK
	 */

	switch (in_pkt->protocol) {
	case IPPROTO_TCP:
		in_pkt->l4_header.tcp.dport = ntohs(cs_cb->output.l4_h.th.dport); 
		in_pkt->l4_header.tcp.sport = ntohs(cs_cb->output.l4_h.th.sport);
		break;
	case IPPROTO_UDP:
		in_pkt->l4_header.udp.dport = ntohs(cs_cb->output.l4_h.uh.dport);
		in_pkt->l4_header.udp.sport = ntohs(cs_cb->output.l4_h.uh.sport);
		break;
	default:
		DBG(printk("%s:%d: unexpected protocol = %d\n",
			__func__, __LINE__, in_pkt->protocol));
		return;
	}

	/* set egress packet contents */
	memcpy(out_pkt, in_pkt, sizeof(cs_pkt_info_t));

	/* L2 */
	memcpy(out_pkt->sa_mac, cs_cb->output.raw.sa, CS_ETH_ADDR_LEN);
	memcpy(out_pkt->da_mac, cs_cb->output.raw.da, CS_ETH_ADDR_LEN);
	/* out_pkt->eth_type = cs_cb->output.raw.eth_protocol; */

	/* VLAN */
	switch (cs_cb->output.raw.vlan_tpid) {
	case ETH_P_8021Q:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		out_pkt->tag[0].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	out_pkt->tag[0].vlan_id = cs_cb->output.raw.vlan_tci & 0xFFF;

	switch (cs_cb->output.raw.vlan_tpid_2) {
	case ETH_P_8021Q:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_8100;
		break;
	case 0x9100:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9100;
		break;
	case 0x88a8:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_88A8;
		break;
	case 0x9200:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_9200;
		break;
	default:
		out_pkt->tag[1].tpid_encap_type = CS_VLAN_TPID_NONE;
	}
	out_pkt->tag[1].vlan_id = cs_cb->output.raw.vlan_tci_2 & 0xFFF;

	/* physical port */
	in_pkt->phy_port = CS_PORT_OFLD0;
	out_pkt->phy_port = cs_cb->common.egress_port_id;

	/* set sw_id */
	if (cs_cb->output.l3_nh.iph.ver == 6) {
		pe2lan_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV6_FORWARD];
	} else {
		pe2lan_flow.swid_array[CS_FLOW_SWID_FORWARD] =
			cs_cb->common.swid[CS_SWID64_MOD_ID_IPV4_FORWARD];
	}

	pe2lan_flow.swid_array[CS_FLOW_SWID_VPN] = t->sw_id;
	DBG(printk("%s:%d: type %d, dir %d, sa_id %d, sw_id = 0x%0llx\n",
		__func__, __LINE__,
		t->tunnel_cfg.type, t->tunnel_cfg.dir, t->sa_id,t->sw_id));

	/* 2. create flow hash */
	ret = cs_flow_add(t->device_id, &pe2lan_flow);
	if (ret != CS_OK) {
		DBG(printk("%s:%d: cs_flow_add ret = %d\n",
			__func__, __LINE__, ret));
	}
	
	return;
}


/************************ main handlers *********************/
/* this function returns 0 when it is ok for Kernel to continue
 * its original task.  CS_DONE means Kernel does not have to
 * handle anymore.
 * parameters:
 *	skb
 *	x
 *	ip_ver:		0: IPv4, 1: IPv6
 *	dir:		0: outbound, 1: inbound
 * return value:
 *	0		Linux original path
 *	1 (CS_DONE)	free skb
 */
int cs_ipsec_ctrl(struct sk_buff *skb, struct xfrm_state *x,
			  unsigned char ip_ver, unsigned char dir)
{
	const cs_dev_id_t dev_id = 1;
	int ret;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_ipsec_policy_node_t p_node;
	cs_tunnel_state_t state = CS_TUNNEL_INVALID;
	cs_uint16_t sa_id = G2_INVALID_SA_ID;
	cs_tunnel_entry_t t_node, *t;
	cs_uint32_t spd_handle;

	DBG(printk("%s:%d: skb 0x%p, len %d, dir %d, ip ver %d, skb->sp 0x%p,"
		" cs_cb 0x%p, mode %d, enable %d\n",
		__func__, __LINE__,
		skb, skb->len, dir, ip_ver, skb->sp, cs_cb,
		cs_cb ? cs_cb->common.sw_only : 0xFF,
		cs_ipsec_ctrl_enable()));

	if (cs_ipsec_ctrl_enable() == FALSE)
		return 0;

	/* Linux sometimes routes a inbound packet to outbound tunnel
	 * for unknown reason. We need to avoid creating wrong hashes.
	 */
	if ((dir == CS_TUNNEL_DIR_OUTBOUND) && cs_cb &&
			(cs_cb->common.ingress_port_id == GE_PORT0 ||
			cs_cb->common.ingress_port_id == PE0_PORT)) {
		SKIP(printk("%s:%d: port %d loops back to WAN port\n",
			__func__, __LINE__, cs_cb->common.ingress_port_id));
		return CS_DONE; /* free skb */
	}
		
	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));
	spd_handle = dir;
	t = &t_node;

	ret = cs_ipsec_policy_node_get_by_spi(dev_id, spd_handle, x->id.spi, &p_node);

	if (ret == CS_OK) {
		/* get tunnel */
		sa_id = CS_IPSEC_SA_HANDLE_TO_ID(p_node.sa_handle);
		ret = cs_tunnel_get_by_sa_id(sa_id,
				(cs_sa_id_direction_t) p_node.sa.sa_dir, t);
		if (ret == CS_OK) {
			/* get tunnel state */
			state = t->state;

		} else {
			printk("%s:%d: Can't get tunnel, sa_id %d\n",
				__func__, __LINE__, sa_id);
			/* drop packets */
			return CS_DONE; /* free skb */
		}
			
	}
	
	if (state == CS_TUNNEL_ACCELERATED /* HW tunnel exists */) {
		DBG(printk("%s:%d: dir %d, tunnel state = CS_TUNNEL_ACCELERATED, sa_id %d\n",
			__func__, __LINE__, dir, sa_id));
		if (cs_cb && (cs_cb->common.sw_only != CS_SWONLY_STATE) &&
					dir == CS_TUNNEL_DIR_OUTBOUND) {
			/* create flow hash */
			if (t->tunnel_cfg.type == CS_IPSEC) {
				/* do nothing !? */
			} else {
				/* L2TP (PPP) control packet for L2TP over IPSec
				   will be offloaded here */
				DBG(printk("%s:%d: offload packet for sa_id %d, dir %d, type %d\n",
					__func__, __LINE__,
					sa_id, dir, t->tunnel_cfg.type));
				DBG(printk("%s:%d: dir = %d, data = 0x%p, len = %d, network ptr = 0x%p,"
					" network hdr len = %d, mac ptr = 0x%p, mac_len = %d\n",
					__func__, __LINE__,
					dir,
					skb->data, skb->len,
					skb_network_header(skb), skb_network_header_len(skb),
					skb_mac_header(skb), skb->mac_len));
				SKIP(cs_dump_data(skb->data, skb->len));
			}
			
			/* create flow hash for an outbound packet */
			cs_ipsec_lan2pe_flow_hash_add(cs_cb, &t_node);
		} else if (dir == CS_TUNNEL_DIR_INBOUND) {
			DBG(printk("%s:%d: dir %d, sa_id %d, skb 0x%p, DROP!\n",
				__func__, __LINE__, dir, sa_id, skb));
			/* drop packets */
			return CS_DONE; /* free skb */
		}

		/* send to PE */
		cs_ipsec_tx_skb_to_pe(skb, &t_node, &p_node);
		return CS_DONE; /* free skb */
	} else if (state == CS_TUNNEL_INIT /* HW tunnel is under construction */) {
		DBG(printk("%s:%d: dir %d, tunnel state = CS_TUNNEL_INIT, sa_id %d\n",
			__func__, __LINE__, dir, sa_id));
		/* drop packets */
		return CS_DONE; /* free skb */
	} else if (cs_cb && (cs_cb->common.sw_only != CS_SWONLY_STATE)) {
		cs_cb->common.module_mask |= CS_MOD_MASK_IPSEC;

		/* record spi, protocol and dir in cs_cb */
		cs_cb->common.vpn_dir = dir;
		cs_cb->common.vpn_ip_ver = ip_ver;

		DBG(printk("%s:%d: dir %d, SPI 0x%x, module_mask = 0x%x\n",
			__func__, __LINE__,
			dir, x->id.spi, cs_cb->common.module_mask));

		if (x->id.proto != IPPROTO_ESP) {
			printk("%s:%d: unexpected protocol %d\n",
				__func__, __LINE__, x->id.proto);
		}
	}

	return 0;

}

EXPORT_SYMBOL(cs_ipsec_ctrl);


/*
 * Based on given skb and its control block info, create IPSec tunnel.
 * Now skb->data points to MAC header.
 */
int cs_ipsec_hw_accel_add(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);
	struct xfrm_state *x;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_uint32_t *daddr;
	cs_uint16_t family;
	cs_tunnel_dir_t dir;
	cs_ip_afi_t ip_ver;
	cs_uint32_t spi;
	cs_status_t ret;
	cs_dev_id_t dev_id;
	cs_uint32_t spd_handle, policy_handle, sa_handle;
	cs_ipsec_policy_t policy;
	cs_ipsec_sa_t sa;
	cs_l3_nexthop_t nexthop;
	cs_uint32_t nexthop_id;
	cs_tunnel_cfg_t	t_cfg;
	cs_tunnel_id_t tunnel_id;
	cs_uint16_t sa_id = G2_INVALID_SA_ID;
	cs_tunnel_entry_t *t, t_node;
	cs_ipsec_policy_node_t p_node;

	memset(&policy, 0, sizeof(cs_ipsec_policy_t));
	memset(&sa, 0, sizeof(cs_ipsec_policy_t));
	memset(&t_cfg, 0, sizeof(cs_tunnel_cfg_t));
	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));
	memset(&nexthop, 0, sizeof(cs_l3_nexthop_t));
	t = &t_node;

	dir = (cs_tunnel_dir_t) cs_cb->common.vpn_dir;
	ip_ver = (cs_ip_afi_t) cs_cb->common.vpn_ip_ver;

	DBG(printk("%s:%d: skb 0x%p, len %d, dir %d, ip ver %d, skb->sp 0x%p,"
		" cs_cb 0x%p, mode %d, module_mask = 0x%x\n",
		__func__, __LINE__,
		skb, skb->len, dir, ip_ver, skb->sp, cs_cb,
		cs_cb->common.sw_only,
		cs_cb->common.module_mask));

	if (dir == CS_TUNNEL_DIR_OUTBOUND) {
		/* For LAN->WAN packet,
		 * the cs_cb->output.l4_h.ah_esp.spi is correct.
		 * skb->transport_header is invalid for IPSec + PPPoE.
		 * skb->transport_header is valid for IPSec.
		 */
		spi = cs_cb->output.vpn_h.ah_esp.spi;
		if (ip_ver == CS_IPV4) {
			daddr = (cs_uint32_t *) &cs_cb->output.l3_nh.iph.dip;
			family = AF_INET;
		} else { /* ip_ver == CS_IPV6 */
			daddr = (cs_uint32_t *) &cs_cb->output.l3_nh.ipv6h.dip;
			family = AF_INET6;
		}
		
	} else {
		/* inbound */
		/* For WAN->LAN packet, the cs_cb->input.l4_h.ah_esp.spi is correct */
		/* We can get the real SPI from skb->sp->xvec[0]->id.spi */
		spi = cs_cb->input.vpn_h.ah_esp.spi;

		if (ip_ver == CS_IPV4) {
			daddr = (cs_uint32_t *) &cs_cb->input.l3_nh.iph.dip;
			family = AF_INET;
		} else { /* ip_ver == CS_IPV6 */
			daddr = (cs_uint32_t *) &cs_cb->input.l3_nh.ipv6h.dip;
			family = AF_INET6;
		}
	}
	


	/* try to get xfrm state */
	if (skb->sp && skb->sp->len == 1) {
		DBG(printk("%s:%d: Get xfrm_state!! IP %pI4, skb->sp->refcnt %d\n",
			__func__, __LINE__, daddr, skb->sp->refcnt.counter));
		x = skb->sp->xvec[0];
		xfrm_state_hold(x);
		spi = x->id.spi;
	} else {
		x = xfrm_state_lookup(net, skb->mark,
					(const xfrm_address_t *) daddr,
					spi, IPPROTO_ESP, family);
		if (!x) {
			DBG(printk("%s:%d: Can't get xfrm_state!! IP %pI4, SPI 0x%x\n",
				__func__, __LINE__, daddr, spi));
			return CS_ACCEL_HASH_DONT_CARE;
		} else {
			DBG(printk("%s:%d: Get xfrm_state!! IP %pI4, SPI 0x%x\n",
				__func__, __LINE__, daddr, spi));
		}
	}
	SKIP(printk("%s:%d: DIR %d, selector of IPSec xfrm state\n"
		"\t IPSec SPI 0x%x, dest IP %pI4, protocol %d\n"
		"\t L3 dest ip %pI4, len 0x%x\n"
		"\t L3 src  ip %pI4, len 0x%x\n"
		"\t L3 protocol %d\n"
		"\t L4 dport 0x%x, dport_mask 0x%x\n"
		"\t L4 sport 0x%x, sport_mask 0x%x\n"
		"\t ifindex %d, family %d, user %d\n",
		__func__,__LINE__, dir,
		x->id.spi, &x->id.daddr, x->id.proto,
		&x->sel.daddr.a4, x->sel.prefixlen_d,
		&x->sel.saddr.a4, x->sel.prefixlen_s,
		x->sel.proto,
		x->sel.dport, x->sel.dport_mask,
		x->sel.sport, x->sel.sport_mask,
		x->sel.ifindex, x->sel.family, x->sel.user));
	/* The selector macth outer IP of the IPSec tunnel.
	 * It cause PE->LAN packet to be dropped by
	 * ip_forward() since the decrypted packet doesn't match
	 * the selector.
	 * force to clean the selector */
	family = x->sel.family;
	memset(&x->sel, 0, sizeof(struct xfrm_selector));
	x->sel.family = family;

	/* check if it is tunnel mode */
	if (x->props.mode == XFRM_MODE_TRANSPORT)
		goto EXIT;

	/* avoid duplicate tunnel */
	dev_id = 1;
	spd_handle = dir; /* 0:outbound, 1:inbound */

	ret = cs_ipsec_policy_node_get_by_spi(dev_id, spd_handle, spi, &p_node);
	if (ret == CS_OK) {
		/* we already create IPSec tunnel for it */
		DBG(printk("%s:%d: we already create IPSec tunnel for it. SPI 0x%x\n",
			__func__, __LINE__, spi));

		/* try to create flow hash if possible. */
		sa_id = CS_IPSEC_SA_HANDLE_TO_ID(p_node.sa_handle);
		goto FLOW_HASH;
	}


	/******************** create tunnel hash ***********************/
	/* add IPSec policy */
	cs_ipsec_pseudo_policy_construct(&policy);

	ret = cs_ipsec_policy_add(dev_id, spd_handle, &policy, &policy_handle);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec policy, ret = %d\n",
			__func__, __LINE__, ret);
		goto EXIT;
	}
	DBG(printk("%s:%d: spd_handle = %d, policy_handle = %d\n",
		__func__, __LINE__,
		spd_handle, policy_handle));

	/* add IPSec SA */
	ret = cs_ipsec_sa_construct(x, cs_cb, &sa);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct IPSec SA, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_POLICY;
	}
	ret = cs_ipsec_sa_add(dev_id, spd_handle, policy_handle, 1, &sa, 0, &sa_handle);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec SA, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_POLICY;
	}

	ret = cs_ipsec_policy_xfrm_link(dev_id, spd_handle, policy_handle, x);
	if (ret != CS_OK) {
		printk("%s:%d: failed to link IPSec xfrm state, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_SA;
	}

	sa_id = CS_IPSEC_SA_HANDLE_TO_ID(sa_handle);

	DBG(printk("%s:%d: sa_handle = %d, sa_id = %d\n",
		__func__, __LINE__,
		policy_handle, sa_id));

	/* add nexthop of IPSec tunnel */
	ret = cs_ipsec_nexthop_construct(skb, dir, &nexthop);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct IPSec nexthop, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_SA;
	}

	/* nexthop debug print */
	DBG(printk("%s:%d: nexthop ID\n\t\t type = %d\n",
		__func__, __LINE__,
		nexthop.nhid.nhop_type));

	DBG(printk("\t\t IF ID = %d\n", nexthop.nhid.intf_id));

	if (nexthop.nhid.addr.afi == CS_IPV4) {
		DBG(printk("\t\t IP = %pI4\n", nexthop.nhid.addr.ip_addr.addr));
	} else {
		/* IPv6 */
		DBG(printk("\t\t IP = %pI6\n", nexthop.nhid.addr.ip_addr.addr));
	}
	DBG(printk("\t DA MAC = %pM\n", nexthop.da_mac));
	DBG(printk("\t ID = %d\n", nexthop.id.port_id));
	DBG(printk("\t encap\n\t\t type = %d\n", nexthop.encap.port_encap.type));
	switch (nexthop.encap.port_encap.type) {
	case CS_PORT_ENCAP_PPPOE_E:
		DBG(printk("\t\t SA MAC = %pM\n", nexthop.encap.port_encap.port_encap.pppoe.src_mac));
		DBG(printk("\t\t DA MAC = %pM\n", nexthop.encap.port_encap.port_encap.pppoe.dest_mac));
		DBG(printk("\t\t VID[0] = %d\n", nexthop.encap.port_encap.port_encap.pppoe.tag[0]));
		DBG(printk("\t\t VID[1] = %d\n", nexthop.encap.port_encap.port_encap.pppoe.tag[1]));
		DBG(printk("\t\t PPPoE session ID = %d\n", nexthop.encap.port_encap.port_encap.pppoe.pppoe_session_id));
		break;
	case CS_PORT_ENCAP_ETH_E:
	case CS_PORT_ENCAP_ETH_1Q_E:
	case CS_PORT_ENCAP_ETH_QinQ_E:
		DBG(printk("\t\t SA MAC = %pM\n", nexthop.encap.port_encap.port_encap.eth.src_mac));
		DBG(printk("\t\t VID[0] = %d\n", nexthop.encap.port_encap.port_encap.eth.tag[0]));
		DBG(printk("\t\t VID[1] = %d\n", nexthop.encap.port_encap.port_encap.eth.tag[1]));
		break;
	default:
		DBG(printk("\t\t SA MAC = %pM\n", nexthop.encap.port_encap.port_encap.eth.src_mac));
	}
	
	ret = cs_l3_nexthop_add(dev_id, &nexthop, &nexthop_id);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec nexthop, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_SA;
	}

	/* add IPSec tunnel */
	ret = cs_ipsec_t_cfg_construct(cs_cb, dir, nexthop_id, policy_handle, &t_cfg);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct IPSec tunnel, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_NEXTHOP;
	}

	/* tunnel debug print */
	DBG(printk("%s:%d: tunnel_cfg\n\t type = %d\n",
		__func__, __LINE__,
		t_cfg.type));

	if (t_cfg.dest_addr.afi == CS_IPV4) {
		DBG(printk("\t dest IP = %pI4\n", t_cfg.dest_addr.ip_addr.addr));
		DBG(printk("\t src IP = %pI4\n", t_cfg.src_addr.ip_addr.addr));
	} else {
		/* IPv6 */
		DBG(printk("\t dest IP = %pI6\n", t_cfg.dest_addr.ip_addr.addr));
		DBG(printk("\t src IP = %pI6\n", t_cfg.src_addr.ip_addr.addr));
	}
	DBG(printk("\t tx port = %d\n", t_cfg.tx_port));
	DBG(printk("\t nexthop_id = %d\n", t_cfg.nexthop_id));
	DBG(printk("\t dir = %d\n", t_cfg.dir));
	switch (t_cfg.type) {
	case CS_IPSEC:
		DBG(printk("\t IPSec tunnel\n"));
		break;
	case CS_L2TP:
	case CS_L2TPV3:
		DBG(printk("\t L2TP tunnel\n"));
		break;
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
		DBG(printk("\t L2TP over IPSec tunnel\n"));
		break;
	default:
		DBG(printk("\t Unexpected tunnel, type %d\n", t_cfg.type));
	}

	if (t_cfg.type == CS_IPSEC) {
		DBG(printk("\t\t policy ID = %d\n", t_cfg.tunnel.ipsec.ipsec_policy));
	} else {
		DBG(printk("\t\t ver = 0x%x\n", t_cfg.tunnel.l2tp.ver));
		DBG(printk("\t\t len = 0x%x\n", t_cfg.tunnel.l2tp.len));
		DBG(printk("\t\t tid = 0x%x\n", t_cfg.tunnel.l2tp.tid));
		DBG(printk("\t\t UDP dest port = 0x%x\n", t_cfg.tunnel.l2tp.dest_port));
		DBG(printk("\t\t UDP src port = 0x%x\n", t_cfg.tunnel.l2tp.src_port));
		DBG(printk("\t\t policy ID = %d\n", t_cfg.tunnel.l2tp.ipsec_policy));
	}

	ret = cs_tunnel_add(dev_id, &t_cfg, &tunnel_id);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec tunnel, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_NEXTHOP;
	}

	printk("VPN HW acceleration for type %d, dir %d, tunnel id %d,"
		" SPI 0x%x\n",
		t_cfg.type, dir, tunnel_id, spi);

FLOW_HASH:
	/******************** create flow hash ***********************/
	/* get runtime tunnel entry back */
	ret = cs_tunnel_get_by_sa_id(sa_id, (cs_sa_id_direction_t) dir, t);
	if (ret != CS_OK) {
		printk("%s:%d: failed to get tunnel entry, ret = %d\n",
			__func__, __LINE__, ret);
		goto EXIT;
	}

	if (t->state == CS_TUNNEL_ACCELERATED) {
		if (dir == CS_TUNNEL_DIR_OUTBOUND)
			cs_ipsec_lan2pe_flow_hash_add(cs_cb, t);
		else if (dir == CS_TUNNEL_DIR_INBOUND)
			cs_ipsec_pe2lan_flow_hash_add(cs_cb, t);
	}
	xfrm_state_put(x);

	return CS_ACCEL_HASH_SUCCESS;

FREE_NEXTHOP:
	cs_l3_nexthop_delete(dev_id, &nexthop);
FREE_SA:
	cs_ipsec_sa_delete(dev_id, spd_handle, policy_handle, sa_handle);
FREE_POLICY:
	cs_ipsec_policy_delete(dev_id, spd_handle, policy_handle);

EXIT:
	xfrm_state_put(x);

	return CS_ACCEL_HASH_DONT_CARE;
}

int cs_ipsec_hw_accel_delete(cs_tunnel_entry_t *t, cs_ipsec_policy_node_t *p)
{
	cs_dev_id_t dev_id;
	cs_tunnel_id_t tunnel_id;
	cs_uint32_t nexthop_id;
	cs_l3_nexthop_t nexthop;
	cs_tunnel_dir_t dir;
	cs_uint32_t spd_handle, policy_handle, sa_handle;
	cs_uint32_t session_id;
	

	/* backup information before tunnel is deleted */
	dev_id = t->device_id;
	tunnel_id = t->tunnel_id;
	nexthop_id = t->tunnel_cfg.nexthop_id;
	dir = t->tunnel_cfg.dir;
	spd_handle = dir; /* 0:outbound, 1:inbound */
	policy_handle = p->policy_handle;
	sa_handle = p->sa_handle;

	
	/* L2TPv2 over IPSec should delete session at first */
	if (t->tunnel_cfg.type == CS_L2TP_IPSEC) {
		if (cs_l2tp_session_id_get(t->tunnel_id, &session_id) == CS_OK)
			cs_l2tp_session_delete(dev_id, tunnel_id, session_id);
	} else if (t->tunnel_cfg.type == CS_L2TPV3_IPSEC) {
		/* We don't support to delette L2TPv3 over IPSec
		   from kernel event now. */
		return CS_OK;
	}
	
	cs_tunnel_delete_by_idx(dev_id, tunnel_id);
	
	if (cs_l3_nexthop_get(dev_id, nexthop_id, &nexthop) == CS_OK)
		cs_l3_nexthop_delete(dev_id, &nexthop);
	
	cs_ipsec_sa_delete(dev_id, spd_handle, policy_handle, sa_handle);
	cs_ipsec_policy_delete(dev_id, spd_handle, policy_handle);
	
	return CS_OK;
}

int cs_ipsec_flow_hash_delete_by_sw_id(cs_uint64 sw_id)
{
	cs_core_hmu_value_t hmu_value;

	memset(&hmu_value, 0, sizeof(cs_core_hmu_value_t));
	hmu_value.type = CS_CORE_HMU_WATCH_SWID64;
	hmu_value.value.swid64 = sw_id;
	hmu_value.mask = 0x08;

	return cs_core_hmu_delete_hash(CS_CORE_HMU_WATCH_SWID64, &hmu_value);

}

void cs_ipsec_proc_callback(unsigned long notify_event,
					   unsigned long value)
{
	DBG(printk("%s notify_event 0x%lx value 0x%lx\n",
	     __func__, notify_event, value));
	switch (notify_event) {
		case CS_HAM_ACTION_MODULE_DISABLE:
			DBG(printk("%s:%d: CS_HAM_ACTION_MODULE_DISABLE\n",
				__func__, __LINE__));
			break;
		case CS_HAM_ACTION_MODULE_ENABLE:
			DBG(printk("%s:%d: CS_HAM_ACTION_MODULE_ENABLE\n",
				__func__, __LINE__));
			break;
		case CS_HAM_ACTION_MODULE_REMOVE:
			break;
		case CS_HAM_ACTION_MODULE_INSERT:
			break;
		case CS_HAM_ACTION_CLEAN_HASH_ENTRY:
			break;
	}

}

/* If SPI is changed, delete the tunnel,
   hashes and the PE entry */
void cs_ipsec_tunnel_chk(cs_tunnel_entry_t *t, cs_uint32 spi)
{
	int ret;
	cs_ipsec_policy_node_t p_node;

	if (t == NULL)
		return;

	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));

	ret = cs_ipsec_policy_node_get(t->device_id,
			t->tunnel_cfg.dir,
			t->tunnel_cfg.tunnel.l2tp.ipsec_policy,
			&p_node);

	/* If the SPI is the same, we don't need to delete the tunnel.
	 * Otherwise, we need to delete the old tunnel.
	 * Then the control plane will create a new tunnel.
	 */
	if (ret == CS_OK && p_node.sa.spi == spi)
		return;

	DBG(printk("%s:%d: New SPI 0x%x, old SPI 0x%x\n"
		"\t Delete tunnel ID %d",
		__func__, __LINE__, spi, p_node.sa.spi, t->tunnel_id));
	cs_tunnel_state_set(t->tunnel_id, CS_TUNNEL_INVALID);

	/* delete the tunnel entry, tunnel hashes and the PE entry */
	cs_ipsec_hw_accel_delete(t, &p_node);
	/* delete all flow hashes by t->sw_id */
	cs_ipsec_flow_hash_delete_by_sw_id(t->sw_id);

	return;
}


void cs_ipsec_tunnel_del_by_x(struct xfrm_state *x)
{
	const cs_dev_id_t dev_id = 1;
	int ret;
	cs_ipsec_policy_node_t p_node;
	cs_uint16_t sa_id = G2_INVALID_SA_ID;
	cs_tunnel_entry_t *t, t_node;
	cs_tunnel_dir_t dir;
	int retry_cnt = 0;
	cs_uint64 sw_id;

	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));
	t = &t_node;

	/* try to find the policy node in both SPD */
	if (cs_ipsec_policy_node_get_by_spi(dev_id, 0, x->id.spi, &p_node)
							== CS_OK) {
		dir = CS_TUNNEL_DIR_OUTBOUND;
	} else if (cs_ipsec_policy_node_get_by_spi(dev_id, 1, x->id.spi,
							&p_node) == CS_OK) {
		dir = CS_TUNNEL_DIR_INBOUND;
	} else {
		DBG(printk("%s:%d: Can't find policy entry by SPI 0x%x\n",
			__func__, __LINE__, x->id.spi));
		return;
	}

	sa_id = CS_IPSEC_SA_HANDLE_TO_ID(p_node.sa_handle);

CHECK_TUNNEL_STATE:

	/* get tunnel */
	ret = cs_tunnel_get_by_sa_id(sa_id, (cs_sa_id_direction_t) dir, t);
	if (ret != CS_OK) {
		printk("%s:%d: Can't find tunnel entry by SA ID %d, DIR %d,"
			" SPI 0x%x\n",
			__func__, __LINE__,
			sa_id, dir, x->id.spi);
		return;
	}
	sw_id = t->sw_id;

	switch (t->state) {
	case CS_TUNNEL_ACCELERATED:
		cs_tunnel_state_set(t->tunnel_id, CS_TUNNEL_INIT);
		
		/* delete the tunnel entry, tunnel hashes and the PE entry */
		cs_ipsec_hw_accel_delete(t, &p_node);
		/* delete all flow hashes by t->sw_id */
		cs_ipsec_flow_hash_delete_by_sw_id(sw_id);
		break;
	case CS_TUNNEL_INIT:
		/* retry */
		if (retry_cnt < CS_IPSEC_RETRY_CNT_MAX) {
			retry_cnt++;
			goto CHECK_TUNNEL_STATE;
		}
		SKIP(printk("%s:%d: tunnel state is unstable in %d secs."
			" SA ID %d, DIR %d, SPI 0x%x\n",
			__func__, __LINE__,
			CS_IPSEC_RETRY_CNT_MAX,
			sa_id, dir, x->id.spi));

		/* delete the tunnel entry, tunnel hashes and the PE entry */
		cs_ipsec_hw_accel_delete(t, &p_node);
		/* delete all flow hashes by t->sw_id */
		cs_ipsec_flow_hash_delete_by_sw_id(sw_id);
		break;
	default:
		printk("%s:%d: unknown tunnel state %d,"
			" SA ID %d, DIR %d, SPI 0x%x\n",
			__func__, __LINE__,
			t->state, sa_id, dir, x->id.spi);
		return;
	}
	return;
}



static int cs_ipsec_xfrm_send_notify(struct xfrm_state *x,
					const struct km_event *c)
{
	switch (c->event) {
		case XFRM_MSG_EXPIRE:
			DBG(printk("%s: xfrm_state=0x%p XFRM_MSG_EXPIRE, "
				"state: %d\n", __func__, x, x->km.state));
			if (x->km.state == XFRM_STATE_DEAD)
				cs_ipsec_tunnel_del_by_x(x);
			break;
		case XFRM_MSG_DELSA:
			DBG(printk("%s: xfrm_state=0x%p XFRM_MSG_DELSA\n",
				__func__, x));
			cs_ipsec_tunnel_del_by_x(x);
			break;
		case XFRM_MSG_NEWSA:
			DBG(printk("%s: xfrm_state=0x%p XFRM_MSG_NEWSA\n",
				__func__, x));
			break;
		case XFRM_MSG_UPDSA:
			DBG(printk("%s: xfrm_state=0x%p XFRM_MSG_UPDSA\n",
				__func__, x));
			break;
		default:
			DBG(printk("%s:%d xfrm_state=0x%p status=%d \n",
				__func__, __LINE__, x, x->km.state));
			break;
	}
	return 0;
}

static struct xfrm_policy *cs_ipsec_xfrm_compile_policy(struct sock *sk,
					int opt, u8 *data, int len, int *dir)
{
	return NULL;

}

static int cs_ipsec_xfrm_send_acquire(struct xfrm_state *x, struct xfrm_tmpl *t,
					struct xfrm_policy *xp, int dir)
{
	return 0;
}


struct xfrm_mgr cs_ipsec_xfrm_mgr =
{
	.id		= "cs_ipsec_xfrm_mgr",
	.notify		= cs_ipsec_xfrm_send_notify,
	.acquire	= cs_ipsec_xfrm_send_acquire,
	.compile_policy	= cs_ipsec_xfrm_compile_policy,
	.new_mapping	= NULL,
	.notify_policy	= NULL,
	.migrate	= NULL,
};

#if defined(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) &&\
	defined(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL)

static cs_status_t cs_l2tp_ipsec_t_cfg_construct(
	cs_kernel_accel_cb_t				*cs_cb,
	cs_tunnel_type_t				type,
	cs_tunnel_dir_t					dir,
	cs_uint32_t					nexthop_id,
	cs_uint32_t					policy_handle,
	struct l2tp_session				*session,
	cs_tunnel_cfg_t					*t_cfg)
{
	struct cb_network_field *cb_net;
	struct l2tp_tunnel *tunnel = NULL;
	int i;

	t_cfg->type = type;
	t_cfg->tx_port = 0;
	t_cfg->nexthop_id = nexthop_id;
	t_cfg->dir = dir;

	switch (dir) {
	case CS_TUNNEL_DIR_OUTBOUND:
		cb_net = &cs_cb->output;
		break;
	case CS_TUNNEL_DIR_INBOUND:
		cb_net = &cs_cb->input;
		break;
	case CS_TUNNEL_DIR_TWO_WAY:
	default:
		printk("%s:%d unsupported direction %d\n",
			__func__, __LINE__, dir);
		return CS_E_NOT_SUPPORT;
	}
	
	
	if (type == CS_L2TP || type == CS_L2TP_IPSEC ||
		type == CS_L2TPV3 || type == CS_L2TPV3_IPSEC) {
		if (session == NULL) {
			printk("%s:%d session pointer can't be NULL\n",
				__func__, __LINE__);
			return CS_E_CONFLICT;
		}
		tunnel = session->tunnel;
		if (tunnel == NULL) {
			printk("%s:%d tunnel pointer can't be NULL\n",
				__func__, __LINE__);
			return CS_E_CONFLICT;
		}
		t_cfg->tunnel.l2tp.ipsec_policy = policy_handle;
		t_cfg->tunnel.l2tp.ver = tunnel->version | 0x4000;

		if (dir == CS_TUNNEL_DIR_OUTBOUND)
			t_cfg->tunnel.l2tp.tid = tunnel->peer_tunnel_id;
		else
			t_cfg->tunnel.l2tp.tid = tunnel->tunnel_id;
			
		/* L4 UDP ports */
		t_cfg->tunnel.l2tp.dest_port = ntohs(cb_net->l4_h.uh.dport);
		t_cfg->tunnel.l2tp.src_port = ntohs(cb_net->l4_h.uh.sport);

		DBG(printk("%s:%d dir %d, tid 0x%x\n",
			__func__, __LINE__, dir, t_cfg->tunnel.l2tp.tid));

		if (type == CS_L2TPV3 || type == CS_L2TPV3_IPSEC) {
			/* L2TPv3 */
			t_cfg->tunnel.l2tp.session_id = 
				(dir == CS_TUNNEL_DIR_OUTBOUND) ? 
					ntohs(session->peer_session_id) : 
					ntohs(session->session_id);
			t_cfg->tunnel.l2tp.encap_type = (cs_uint16_t) tunnel->encap;
			t_cfg->tunnel.l2tp.l2specific_len = session->l2specific_len;
			t_cfg->tunnel.l2tp.l2specific_type = session->l2specific_type;
			t_cfg->tunnel.l2tp.send_seq = session->send_seq;
			t_cfg->tunnel.l2tp.ns = session->ns;
			t_cfg->tunnel.l2tp.cookie_len = session->cookie_len;
			for (i = 0; i < 8; i++)
				t_cfg->tunnel.l2tp.cookie[i] = session->cookie[i];
			t_cfg->tunnel.l2tp.offset = session->offset;
			
			for (i = 0; i < ETH_ALEN; i++) {
				t_cfg->tunnel.l2tp.l2tp_src_mac[i] = cs_cb->common.vpn_sa[i];
				t_cfg->tunnel.l2tp.peer_l2tp_src_mac[i] = cs_cb->common.vpn_da[i];
			}
		}		
	} else if (type == CS_IPSEC) {
		t_cfg->tunnel.ipsec.ipsec_policy = policy_handle;
	}
	
	/* IP address should be network order */
	if (cb_net->l3_nh.iph.ver == 4) {
		t_cfg->dest_addr.afi = CS_IPV4;
		t_cfg->dest_addr.ip_addr.ipv4_addr = cb_net->l3_nh.iph.dip;
		t_cfg->dest_addr.addr_len = 32;
	
		t_cfg->src_addr.afi = CS_IPV4;
		t_cfg->src_addr.ip_addr.ipv4_addr = cb_net->l3_nh.iph.sip;
		t_cfg->src_addr.addr_len = 32;
	} else if (cb_net->l3_nh.iph.ver == 6) {
		t_cfg->dest_addr.afi = CS_IPV6;
		t_cfg->dest_addr.ip_addr.ipv6_addr[0] = cb_net->l3_nh.ipv6h.dip[0];
		t_cfg->dest_addr.ip_addr.ipv6_addr[1] = cb_net->l3_nh.ipv6h.dip[1];
		t_cfg->dest_addr.ip_addr.ipv6_addr[2] = cb_net->l3_nh.ipv6h.dip[2];
		t_cfg->dest_addr.ip_addr.ipv6_addr[3] = cb_net->l3_nh.ipv6h.dip[3];
		t_cfg->dest_addr.addr_len = 128;
	
		t_cfg->src_addr.afi = CS_IPV6;
		t_cfg->src_addr.ip_addr.ipv6_addr[0] = cb_net->l3_nh.ipv6h.sip[0];
		t_cfg->src_addr.ip_addr.ipv6_addr[1] = cb_net->l3_nh.ipv6h.sip[1];
		t_cfg->src_addr.ip_addr.ipv6_addr[2] = cb_net->l3_nh.ipv6h.sip[2];
		t_cfg->src_addr.ip_addr.ipv6_addr[3] = cb_net->l3_nh.ipv6h.sip[3];
		t_cfg->src_addr.addr_len = 128;
	} else {
		printk("%s:%d: Unsupported IP ver %d\n",
			__func__, __LINE__, cb_net->l3_nh.iph.ver);
		return CS_E_NOT_SUPPORT;
	}

	return CS_OK;
}


/*
 * Based on given skb and its control block info, create IPSec tunnel.
 * Now skb->data points to MAC header.
 */

int cs_l2tp_ipsec_hw_accel_add(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);
	struct xfrm_state *x = NULL;
	cs_kernel_accel_cb_t *cs_cb = CS_KERNEL_SKB_CB(skb);
	cs_uint32_t *daddr;
	cs_uint16_t family;
	cs_tunnel_dir_t dir;
	cs_ip_afi_t ip_ver;
	cs_uint32_t spi = 0;
	cs_status_t ret;
	cs_dev_id_t dev_id;
	cs_uint32_t spd_handle = 0, policy_handle = 0, sa_handle;
	cs_ipsec_policy_t policy;
	cs_ipsec_sa_t sa;
	cs_l3_nexthop_t nexthop;
	cs_uint32_t nexthop_id;
	cs_tunnel_cfg_t	t_cfg;
	cs_tunnel_id_t tunnel_id;
	cs_uint16_t sa_id = G2_INVALID_SA_ID;
	cs_tunnel_entry_t *t, t_node;
	cs_ipsec_policy_node_t p_node;
	struct l2tp_session *session = NULL;
	struct l2tp_tunnel *tunnel = NULL;
	struct sock *sk = NULL;
	struct inet_sock *inet = NULL;
	cs_tunnel_type_t type = CS_L2TP;
	cs_uint32_t tid = 0, sid = 0;

	memset(&policy, 0, sizeof(cs_ipsec_policy_t));
	memset(&sa, 0, sizeof(cs_ipsec_policy_t));
	memset(&t_cfg, 0, sizeof(cs_tunnel_cfg_t));
	memset(&p_node, 0, sizeof(cs_ipsec_policy_node_t));
	memset(&nexthop, 0, sizeof(cs_l3_nexthop_t));
	t = &t_node;

	dir = (cs_tunnel_dir_t) cs_cb->common.vpn_dir;
	ip_ver = (cs_ip_afi_t) cs_cb->common.vpn_ip_ver;
	dev_id = 1;

	DBG(printk("%s:%d: skb 0x%p, len %d, dir %d, ip ver %d, skb->sp 0x%p,"
		" cs_cb 0x%p, mode %d, module_mask = 0x%x\n",
		__func__, __LINE__,
		skb, skb->len, dir, ip_ver, skb->sp, cs_cb,
		cs_cb->common.sw_only,
		cs_cb->common.module_mask));

	if (cs_cb->common.module_mask & CS_MOD_MASK_L2TP) {
		/******************** lookup tunnel ***********************/
		tunnel = l2tp_tunnel_find(&init_net, cs_cb->common.tunnel_id);
		if (tunnel == NULL) {
			DBG(printk("%s:%d: Can't find tunnel by tunnel_id 0x%x\n",
					__func__, __LINE__,
					cs_cb->common.tunnel_id));
			return CS_ACCEL_HASH_DONT_CARE;
		} else {
			DBG(printk("%s:%d: Got tunnel by tunnel_id 0x%x\n",
					__func__, __LINE__,
					cs_cb->common.tunnel_id));
		}
		sk = tunnel->sock;
		if (sk == NULL) {
			DBG(printk("%s:%d: Invalid sock for tid 0x%x\n",
					__func__, __LINE__,
					cs_cb->common.tunnel_id));
			return CS_ACCEL_HASH_DONT_CARE;
		} 
			
		
		/******************** lookup session ***********************/
		session = l2tp_session_find(&init_net, tunnel, cs_cb->common.session_id);
		if (session == NULL) {
			DBG(printk("%s:%d: Can't find session by session_id 0x%x\n",
					__func__, __LINE__,
					cs_cb->common.session_id));
			return CS_ACCEL_HASH_DONT_CARE;
		} else {
			DBG(printk("%s:%d: Got session by session_id 0x%x\n",
					__func__, __LINE__,
					cs_cb->common.session_id));
		}
		
		if (session->tunnel != tunnel) {
			DBG(printk("%s:%d: tunnel mismatch for tunnel_id 0x%x"
					" and session_id 0x%x\n",
					__func__, __LINE__,
					cs_cb->common.tunnel_id,
					cs_cb->common.session_id));
			return CS_ACCEL_HASH_DONT_CARE;
		}
		
		switch (dir) {
		case CS_TUNNEL_DIR_OUTBOUND:
			tid = tunnel->peer_tunnel_id;
			sid = session->peer_session_id;
			break;
		case CS_TUNNEL_DIR_INBOUND:
			tid = tunnel->tunnel_id;
			sid = session->session_id;
			break;
		case CS_TUNNEL_DIR_TWO_WAY:
		default:
			DBG(printk("%s:%d unsupported direction %d\n",
				__func__, __LINE__, dir));
			return CS_ACCEL_HASH_DONT_CARE;
		}
		
		DBG(printk("%s:%d: Dir %d, tid 0x%x, sid 0x%x\n",
				__func__, __LINE__, dir, tid, sid));
		
		
		inet = inet_sk(sk);
		
		DBG(printk("%s:%d: SIP %pI4, DIP %pI4, sport %d, dport %d\n",
			__func__, __LINE__,
			&inet->inet_saddr, &inet->inet_daddr,
			ntohs(inet->inet_sport), ntohs(inet->inet_dport)));
		DBG(printk("%s:%d: cb OUTPUT SIP %pI4, DIP %pI4, sport %d, dport %d\n",
			__func__, __LINE__,
			&cs_cb->output.l3_nh.iph.sip, &cs_cb->output.l3_nh.iph.dip,
			ntohs(cs_cb->output.l4_h.uh.sport),
			ntohs(cs_cb->output.l4_h.uh.dport)));
		
		DBG(printk("%s:%d: cb INPUT SIP %pI4, DIP %pI4, sport %d, dport %d\n",
			__func__, __LINE__,
			&cs_cb->input.l3_nh.iph.sip, &cs_cb->input.l3_nh.iph.dip,
			ntohs(cs_cb->input.l4_h.uh.sport),
			ntohs(cs_cb->input.l4_h.uh.dport)));
		
	}

	if (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) {
		if (dir == CS_TUNNEL_DIR_OUTBOUND) {
			spi = cs_cb->output.vpn_h.ah_esp.spi;
			if (ip_ver == CS_IPV4) {
				daddr = (cs_uint32_t *) &cs_cb->output.l3_nh.iph.dip;
				family = AF_INET;
			} else { /* ip_ver == CS_IPV6 */
				daddr = (cs_uint32_t *) &cs_cb->output.l3_nh.ipv6h.dip;
				family = AF_INET6;
			}
			
		} else {
			/* inbound */
			/* For WAN->LAN packet, the cs_cb->input.l4_h.ah_esp.spi is correct */
			/* For PE->LAN packet, the SPI is invalid because 
			   it may be overwriten by L4 port.
			   Then we can get the real SPI from skb->sp->xvec[0]->id.spi */
			spi = cs_cb->input.vpn_h.ah_esp.spi;

			if (ip_ver == CS_IPV4) {
				daddr = (cs_uint32_t *) &cs_cb->input.l3_nh.iph.dip;
				family = AF_INET;
			} else { /* ip_ver == CS_IPV6 */
				daddr = (cs_uint32_t *) &cs_cb->input.l3_nh.ipv6h.dip;
				family = AF_INET6;
			}
		}
		


		/* try to get xfrm state */
		if (skb->sp && skb->sp->len == 1) {
			DBG(printk("%s:%d: Get xfrm_state!! IP %pI4, skb->sp->refcnt %d\n",
				__func__, __LINE__, daddr, skb->sp->refcnt.counter));
			x = skb->sp->xvec[0];
			xfrm_state_hold(x);
			spi = x->id.spi;
		} else {
			x = xfrm_state_lookup(net, skb->mark,
						(const xfrm_address_t *) daddr,
						spi, IPPROTO_ESP, family);
			if (!x) {
				DBG(printk("%s:%d: Can't get xfrm_state!! IP %pI4, SPI 0x%x\n",
					__func__, __LINE__, daddr, spi));
				return CS_ACCEL_HASH_DONT_CARE;
			} else {
				DBG(printk("%s:%d: Get xfrm_state!! IP %pI4, SPI 0x%x\n",
					__func__, __LINE__, daddr, spi));
			}
		}

		SKIP(printk("%s:%d: DIR %d, selector of IPSec xfrm state\n"
			"\t IPSec SPI 0x%x, dest IP %pI4, protocol %d\n"
			"\t L3 dest ip %pI4, len 0x%x\n"
			"\t L3 src  ip %pI4, len 0x%x\n"
			"\t L3 protocol %d\n"
			"\t L4 dport 0x%x, dport_mask 0x%x\n"
			"\t L4 sport 0x%x, sport_mask 0x%x\n"
			"\t ifindex %d, family %d, user %d\n",
			__func__,__LINE__, dir,
			x->id.spi, &x->id.daddr, x->id.proto,
			&x->sel.daddr.a4, x->sel.prefixlen_d,
			&x->sel.saddr.a4, x->sel.prefixlen_s,
			x->sel.proto,
			x->sel.dport, x->sel.dport_mask,
			x->sel.sport, x->sel.sport_mask,
			x->sel.ifindex, x->sel.family, x->sel.user));
		/* The selector macth outer IP of the L2TP over IPSec tunnel.
		 * It cause PE->LAN packet to be dropped by
		 * ip_forward() since the decrypted packet doesn't match
		 * the selector.
		 * force to clean the selector */
		family = x->sel.family;
		memset(&x->sel, 0, sizeof(struct xfrm_selector));
		x->sel.family = family;
	}

	/* avoid duplicate tunnel */
	/* go to proper start procedure */
	if ((cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) && 
		(cs_cb->common.module_mask & CS_MOD_MASK_L2TP)) {
		/* L2TP over IPSec */
		type = CS_L2TP_IPSEC;
		spd_handle = dir; /* 0:outbound, 1:inbound */
		ret = cs_l2tp_tunnel_get_by_tid(dir, tid, t);
		
		if (ret == CS_OK) {
			/* we already create L2TP tunnel for it */
			
			sa_id = t->sa_id;
			DBG(printk("%s:%d: we already create L2TP over IPSec tunnel for it. tid 0x%x\n",
				__func__, __LINE__, tid));
			goto FLOW_HASH;
		}
		DBG(printk("%s:%d: L2TP_IPSEC_TUNNEL, module mask 0x%x\n",
			__func__, __LINE__,
			cs_cb->common.module_mask));
		goto L2TP_IPSEC_TUNNEL;
		
	} else if (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC) {
		/* IPSec tunnel mode */
		type = CS_IPSEC;
		spd_handle = dir; /* 0:outbound, 1:inbound */

		ret = cs_ipsec_policy_node_get_by_spi(dev_id, spd_handle, spi, &p_node);
		if (ret == CS_OK) {
			/* we already create IPSec tunnel for it */
			DBG(printk("%s:%d: we already create IPSec tunnel for it. SPI 0x%x\n",
				__func__, __LINE__, spi));

			/* try to create flow hash if possible. */
			sa_id = CS_IPSEC_SA_HANDLE_TO_ID(p_node.sa_handle);

			ret = cs_tunnel_get_by_sa_id(sa_id, (cs_sa_id_direction_t) dir, t);
			if (ret != CS_OK) {
				printk("%s:%d: failed to get tunnel entry, ret = %d\n",
					__func__, __LINE__, ret);
				goto EXIT;
			}
			goto FLOW_HASH;
		}

		
		DBG(printk("%s:%d: IPSEC_TUNNEL, module mask 0x%x\n",
			__func__, __LINE__,
			cs_cb->common.module_mask));
		goto IPSEC_TUNNEL;
		
	} else if (cs_cb->common.module_mask & CS_MOD_MASK_L2TP) {
		/* L2TP tunnel mode */
		type = CS_L2TP;

		ret = cs_l2tp_tunnel_get_by_tid(dir, tid, t);
		
		if (ret == CS_OK) {
			/* we already create L2TP tunnel for it */
			
			sa_id = t->sa_id;
			DBG(printk("%s:%d: we already create L2TP tunnel for it. tid 0x%x\n",
				__func__, __LINE__, tid));
			goto FLOW_HASH;
		}
		DBG(printk("%s:%d: L2TP_TUNNEL, module mask 0x%x\n",
			__func__, __LINE__,
			cs_cb->common.module_mask));
		goto L2TP_TUNNEL;
	}
	
	/******************** create tunnel hash ***********************/
IPSEC_TUNNEL:
L2TP_IPSEC_TUNNEL:

	/* add IPSec policy */
	cs_ipsec_pseudo_policy_construct(&policy);

	ret = cs_ipsec_policy_add(dev_id, spd_handle, &policy, &policy_handle);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec policy, ret = %d\n",
			__func__, __LINE__, ret);
		goto EXIT;
	}

	DBG(printk("%s:%d: spd_handle = %d, policy_handle = %d\n",
		__func__, __LINE__,
		spd_handle, policy_handle));

	/* add IPSec SA */
	ret = cs_ipsec_sa_construct(x, cs_cb, &sa);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct IPSec SA, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_POLICY;
	}
	ret = cs_ipsec_sa_add(dev_id, spd_handle, policy_handle, 1, &sa, 0, &sa_handle);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec SA, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_POLICY;
	}
	
	ret = cs_ipsec_policy_xfrm_link(dev_id, spd_handle, policy_handle, x);
	if (ret != CS_OK) {
		printk("%s:%d: failed to link IPSec xfrm state, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_SA;
	}

	sa_id = CS_IPSEC_SA_HANDLE_TO_ID(sa_handle);

	DBG(printk("%s:%d: sa_handle = %d, sa_id = %d\n",
		__func__, __LINE__,
		sa_handle, sa_id));

L2TP_TUNNEL:

	/* add nexthop of tunnel */
	ret = cs_ipsec_nexthop_construct(skb, dir, &nexthop);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct IPSec nexthop, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_SA;
	}

	/* nexthop debug print */
	DBG(printk("%s:%d: nexthop ID\n\t\t type = %d\n",
		__func__, __LINE__,
		nexthop.nhid.nhop_type));

	DBG(printk("\t\t IF ID = %d\n", nexthop.nhid.intf_id));

	if (nexthop.nhid.addr.afi == CS_IPV4) {
		DBG(printk("\t\t IP = %pI4\n", nexthop.nhid.addr.ip_addr.addr));
	} else {
		/* IPv6 */
		DBG(printk("\t\t IP = %pI6\n", nexthop.nhid.addr.ip_addr.addr));
	}
	DBG(printk("\t DA MAC = %pM\n", nexthop.da_mac));
	DBG(printk("\t ID = %d\n", nexthop.id.port_id));
	DBG(printk("\t encap\n\t\t type = %d\n", nexthop.encap.port_encap.type));
	DBG(printk("\t\t SA MAC = %pM\n", nexthop.encap.port_encap.port_encap.eth.src_mac));
	DBG(printk("\t\t VID[0] = %d\n", nexthop.encap.port_encap.port_encap.eth.tag[0]));
	DBG(printk("\t\t VID[1] = %d\n", nexthop.encap.port_encap.port_encap.eth.tag[1]));

	ret = cs_l3_nexthop_add(dev_id, &nexthop, &nexthop_id);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add IPSec nexthop, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_SA;
	}

	/* add tunnel */
	ret = cs_l2tp_ipsec_t_cfg_construct(cs_cb, type, dir, nexthop_id,
					policy_handle, session, &t_cfg);
	if (ret != CS_OK) {
		printk("%s:%d: failed to construct tunnel, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_NEXTHOP;
	}

	/* tunnel debug print */
	DBG(printk("%s:%d: tunnel_cfg\n\t type = %d\n",
		__func__, __LINE__,
		t_cfg.type));

	if (t_cfg.dest_addr.afi == CS_IPV4) {
		DBG(printk("\t dest IP = %pI4\n", t_cfg.dest_addr.ip_addr.addr));
		DBG(printk("\t src IP = %pI4\n", t_cfg.src_addr.ip_addr.addr));
	} else {
		/* IPv6 */
		DBG(printk("\t dest IP = %pI6\n", t_cfg.dest_addr.ip_addr.addr));
		DBG(printk("\t src IP = %pI6\n", t_cfg.src_addr.ip_addr.addr));
	}
	DBG(printk("\t tx port = %d\n", t_cfg.tx_port));
	DBG(printk("\t nexthop_id = %d\n", t_cfg.nexthop_id));
	DBG(printk("\t dir = %d\n", t_cfg.dir));
	switch (t_cfg.type) {
	case CS_IPSEC:
		DBG(printk("\t IPSec tunnel\n"));
		break;
	case CS_L2TP:
	case CS_L2TPV3:
		DBG(printk("\t L2TP tunnel\n"));
		break;
	case CS_L2TP_IPSEC:
	case CS_L2TPV3_IPSEC:
		DBG(printk("\t L2TP over IPSec tunnel\n"));
		break;
	default:
		DBG(printk("\t Unexpected tunnel, type %d\n", t_cfg.type));
	}

	if (t_cfg.type == CS_IPSEC) {
		DBG(printk("\t\t policy ID = %d\n", t_cfg.tunnel.ipsec.ipsec_policy));
	} else {
		DBG(printk("\t\t ver = 0x%x\n", t_cfg.tunnel.l2tp.ver));
		DBG(printk("\t\t len = 0x%x\n", t_cfg.tunnel.l2tp.len));
		DBG(printk("\t\t tid = 0x%x\n", t_cfg.tunnel.l2tp.tid));
		DBG(printk("\t\t UDP dest port = 0x%x\n", t_cfg.tunnel.l2tp.dest_port));
		DBG(printk("\t\t UDP src port = 0x%x\n", t_cfg.tunnel.l2tp.src_port));
		DBG(printk("\t\t policy ID = %d\n", t_cfg.tunnel.l2tp.ipsec_policy));
	}

	ret = cs_tunnel_add(dev_id, &t_cfg, &tunnel_id);
	if (ret != CS_OK) {
		printk("%s:%d: failed to add tunnel, ret = %d\n",
			__func__, __LINE__, ret);
		goto FREE_NEXTHOP;
	}

	/* add L2TP session if it is L2TPv2 */
	if (type == CS_L2TP || type == CS_L2TP_IPSEC) {
		ret = cs_l2tp_session_add(dev_id, tunnel_id, sid);
		
		if (ret != CS_OK) {
			printk("%s:%d: failed to add session, ret = %d\n",
				__func__, __LINE__, ret);
			goto FREE_TUNNEL;
		}
	}

	DBG(printk("%s:%d: Create flow hash for type %d, dir %d, tid 0x%x,"
		" sid 0x%x, SPI 0x%x\n",
		__func__, __LINE__, type, dir, tid, sid, spi));

	/******************** create flow hash ***********************/
	/* get runtime tunnel entry back */
	if (type == CS_IPSEC) {
		ret = cs_tunnel_get_by_sa_id(sa_id, (cs_sa_id_direction_t) dir, t);
		if (ret != CS_OK) {
			printk("%s:%d: failed to get tunnel entry, ret = %d\n",
				__func__, __LINE__, ret);
			goto EXIT;
		}
	} else {
		/* CS_L2TP, CS_L2TP_IPSEC, CS_L2TPV3, CS_L2TPV3_IPSEC */
		ret = cs_l2tp_tunnel_get_by_tid((cs_tunnel_dir_t) dir, tid, t);

		if (ret == CS_OK) {
			sa_id = t->sa_id;
		} else {
			printk("%s:%d: failed to get tunnel entry, ret = %d\n",
				__func__, __LINE__, ret);
			if (tunnel->version == 2)
				goto FREE_SESSION;
			else
				goto FREE_TUNNEL;
		}
	}

	DBG(printk("%s:%d: Get tunnel entry, sw_id 0x%llx, sa_id %d, state %d\n",
		__func__, __LINE__, t->sw_id, sa_id, t->state));

	printk("VPN HW acceleration for type %d, dir %d, tunnel id %d,"
		" tid 0x%x, sid 0x%x, SPI 0x%x\n",
		type, dir, tunnel_id, tid, sid, spi);

FLOW_HASH:
	if (t->state == CS_TUNNEL_ACCELERATED) {
		if (dir == CS_TUNNEL_DIR_OUTBOUND)
			cs_ipsec_lan2pe_flow_hash_add(cs_cb, t);
		else if (dir == CS_TUNNEL_DIR_INBOUND)
			cs_ipsec_pe2lan_flow_hash_add(cs_cb, t);
	}
	xfrm_state_put(x);

	return CS_ACCEL_HASH_SUCCESS;

FREE_SESSION:
	cs_l2tp_session_delete(dev_id, tunnel_id, sid);
FREE_TUNNEL:
	cs_tunnel_delete(dev_id, &t_cfg);
FREE_NEXTHOP:
	cs_l3_nexthop_delete(dev_id, &nexthop);
	if (type == CS_L2TP || type == CS_L2TPV3)
		return CS_ACCEL_HASH_DONT_CARE;
FREE_SA:
	cs_ipsec_sa_delete(dev_id, spd_handle, policy_handle, sa_handle);
FREE_POLICY:
	cs_ipsec_policy_delete(dev_id, spd_handle, policy_handle);

EXIT:
	if (cs_cb->common.module_mask & CS_MOD_MASK_IPSEC)
		xfrm_state_put(x);

	return CS_ACCEL_HASH_DONT_CARE;
}

#endif /* CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL && 
	  CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL */


