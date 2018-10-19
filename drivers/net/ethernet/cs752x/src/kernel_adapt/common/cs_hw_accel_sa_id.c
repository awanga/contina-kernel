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
 * cs_hw_accel_rtp_proxy.c
 *
 * $Id$
 *
 * This file contains the implementation for CS RTP proxy
 * Acceleration.
 */

#include "cs_hw_accel_sa_id.h"
#include <mach/cs_vpn_tunnel_ipc.h>
#include "cs_mut.h"

#ifdef CONFIG_CS752X_PROC
#include "cs752x_proc.h"
extern u32 cs_adapt_debug;
#endif /* CONFIG_CS752X_PROC */

/* TODO:  add a common cs_adapt_debug flag*/
#define DBG(x)	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL) (x)
#define ERR(x)	(x)

static cs_sa_id_t* cs_sa_id_tbl[2][G2_MAX_SA_IDS];
static cs_uint8_t curr_l2tp_plaintext_tunnel_count[2];
static cs_uint8_t curr_pptp_tunnel_count[2];
static cs_uint8_t curr_rtp_count[2];
static cs_uint8_t curr_l2tpv3_plaintext_tunnel_count[2];

cs_status_t
cs_sa_id_alloc(
	cs_sa_id_direction_t	direction,		/* UP_STREAM(0) or DOWN_STREAM(1)*/
	cs_boolean_t			is_encrypted_tunnel,	/* CS_TRUE:1	CS_FALSE:0(plaintext tunnel) */
	cs_boolean_t			is_used_for_rekey,	/* CS_TRUE:1	CS_FALSE:0 */
	cs_tunnel_type_t		tunnel_type,
	cs_uint16_t			*sa_id			/* CS_OUT */
	)
{
	/* to be implemented */
	cs_sa_id_t * p_sa_id;
	cs_int16_t index;
	cs_uint8_t sa_base;

	*sa_id = G2_INVALID_SA_ID;
	
	DBG(printk("%s:%d direction=%d\n", __func__, __LINE__, direction));
	DBG(printk("%s:%d is_encrypted_tunnel=%d\n", __func__, __LINE__, is_encrypted_tunnel));
	DBG(printk("%s:%d is_used_for_rekey=%d\n", __func__, __LINE__, is_used_for_rekey));
	DBG(printk("%s:%d tunnel_type=%d\n", __func__, __LINE__, tunnel_type));

	if(is_encrypted_tunnel)
	{
		if(tunnel_type == CS_PPTP)
		{
			if(curr_pptp_tunnel_count[direction] == PPTP_SERVER_TUNNELS_MAX){
				printk("%s:%d pptp tunnel count exceeds max\n", __func__, __LINE__);
				return CS_E_RESOURCE;
			}
			
			if(is_used_for_rekey)
			{
				/* PPTP rekey protocol Requirement: 
					sa_id needs to be 0~3 8~11 16~19 
					It is better if rekey uses different virtual spacc from pptp data's.
					so the sa id is searched backward.
				*/
				for(index = G2_MAX_ENCRYPTED_TUNNEL-1; index >= 0; index--)
				{
					if(cs_sa_id_tbl[direction][index] == NULL)
						if(index % 8 < 4)
							break;
				}

				/* no resource available*/
				if(index < 0)
				{
					printk("%s:%d cs_sa_id_alloc failed\n", __func__, __LINE__);
					return CS_E_RESOURCE;
				}
				else
					curr_pptp_tunnel_count[direction]++;
			}
			else /* not used for rekey */
			{
				/* PPTP data protocol Requirement: 
					sa_id needs to be 0~3 8~11 16~19 */
				for(index = 0; index < G2_MAX_ENCRYPTED_TUNNEL; index++)
				{
					if(cs_sa_id_tbl[direction][index] == NULL)
						if(index % 8 < 4)
							break;
				}
				/* no resource available */
				if(index == G2_MAX_ENCRYPTED_TUNNEL)
				{
					printk("%s:%d cs_sa_id_alloc failed\n", __func__, __LINE__);
					return CS_E_RESOURCE;
				}
				else
					curr_pptp_tunnel_count[direction]++;
			}
		}
		else
		{
			/* search 4~7 12~15 20~23 at first*/
			for(index = 0; index < G2_MAX_ENCRYPTED_TUNNEL; index++)
			{
				if(cs_sa_id_tbl[direction][index] == NULL)
					if(index % 8 > 3)
						break;
			}
			
			if(index == G2_MAX_ENCRYPTED_TUNNEL)
			{
				/* 4~7 12~15 20~23 no resource available. 
					use PPTP's sa id  0~3 8~11 16~19*/
				for(index = 0; index < G2_MAX_ENCRYPTED_TUNNEL; index++)
				{
					if(cs_sa_id_tbl[direction][index] == NULL)
						if(index % 8 < 4)
							break;
				}
			}
			/* no resource available */
			if(index == G2_MAX_ENCRYPTED_TUNNEL)
			{
				printk("%s:%d cs_sa_id_alloc failed\n", __func__, __LINE__);
				return CS_E_RESOURCE;
			}
		}
	}
	else
	{	/* plaintext tunnel */
		sa_base = PLAINTEXT_SA_ID_BASE;

		/*TODO: should use switch*/
		if(tunnel_type != CS_L2TP &&
			tunnel_type != CS_PPTP &&
			tunnel_type != CS_RTP &&
			tunnel_type != CS_L2TPV3)
		{
			printk("%s:%d unsupported type\n", __func__, __LINE__);
			return CS_E_NOT_SUPPORT;
		}

		if(is_used_for_rekey)
		{
			printk("%s:%d palintext tunnel type with re-key is not supported\n",
					__func__, __LINE__);
			return CS_E_NOT_SUPPORT;
		}

		for(index = sa_base; index < G2_MAX_SA_IDS; index++)
		{
			if(cs_sa_id_tbl[direction][index] == NULL)
				break;
		}
		
		if(index == G2_MAX_SA_IDS)
		{
			printk("%s:%d cs_sa_id_alloc failed\n", __func__, __LINE__);
			return CS_E_RESOURCE;
		}

		if(tunnel_type == CS_L2TP)
		{
			curr_l2tp_plaintext_tunnel_count[direction]++;
			if(curr_l2tp_plaintext_tunnel_count[direction] > L2TPv2_SERVER_TUNNELS_MAX)
			{
				printk("%s:%d l2tp tunnel count exceeds max\n", __func__, __LINE__);
				return CS_E_RESOURCE;	
			}
		}
		else if(tunnel_type == CS_PPTP)
		{
			curr_pptp_tunnel_count[direction]++;
			if(curr_pptp_tunnel_count[direction] > PPTP_SERVER_TUNNELS_MAX)
			{
				printk("%s:%d pptp tunnel count exceeds max\n", __func__, __LINE__);
				return CS_E_RESOURCE;	
			}
		}
		else if(tunnel_type == CS_RTP)
		{
			curr_rtp_count[direction]++;
			if(curr_rtp_count[direction] > RTP_TUNNELS_MAX)
			{
				printk("%s:%d rtp offload stream count exceeds max\n", __func__, __LINE__);
				return CS_E_RESOURCE;	
			}
		}
		else if(tunnel_type == CS_L2TPV3)
		{
			curr_l2tpv3_plaintext_tunnel_count[direction]++;
			if(curr_l2tpv3_plaintext_tunnel_count[direction] > L2TPv3_CLIENT_TUNNELS_MAX)
			{
				printk("%s:%d l2tpv3 tunnel count exceeds max\n", __func__, __LINE__);
				return CS_E_RESOURCE;	
			}
		}
	}
	
	*sa_id = index;

	p_sa_id = cs_zalloc(sizeof(cs_sa_id_t), GFP_KERNEL);
	if (p_sa_id == NULL) {
		ERR(printk("%s:%d out of memory\n",
			__func__, __LINE__));
		return CS_E_MEM_ALLOC;
	}
	p_sa_id->direction = direction;
	p_sa_id->is_encrypted_tunnel = is_encrypted_tunnel;
	p_sa_id->is_used_for_rekey = is_used_for_rekey;
	p_sa_id->tunnel_type = tunnel_type;
	
	DBG(printk("%s:%d sa_id=%d\n", __func__, __LINE__, *sa_id));
	
	cs_sa_id_tbl[direction][index] = p_sa_id;
	
	return CS_OK;
	
}
EXPORT_SYMBOL(cs_sa_id_alloc);

cs_status_t cs_sa_id_free(cs_sa_id_direction_t direction, cs_uint16_t sa_id)
{
	cs_sa_id_t * p_sa_id;
	if(cs_sa_id_tbl[direction][sa_id] != NULL)
	{
		p_sa_id = cs_sa_id_tbl[direction][sa_id];
		switch(p_sa_id->tunnel_type){
		case CS_RTP:
			curr_rtp_count[direction]--;
			break;
		case CS_L2TP:
			curr_l2tp_plaintext_tunnel_count[direction]--;
			break;
		case CS_PPTP:
			curr_pptp_tunnel_count[direction]--;
			break;
		case CS_L2TPV3:
			curr_l2tpv3_plaintext_tunnel_count[direction]--;
			break;
		default:
			break;
		}
		cs_free(cs_sa_id_tbl[direction][sa_id]);
		cs_sa_id_tbl[direction][sa_id] = NULL;
		DBG(printk("%s:%d free sa_id=%d successfully\n", __func__, __LINE__, sa_id));
		return CS_OK;
	}
	else
	{
		ERR(printk("%s:%d free sa_id=%d does not exist\n", __func__, __LINE__, sa_id));
		return CS_E_ERROR;
	}
}
EXPORT_SYMBOL(cs_sa_id_free);

/* internal APIs*/
cs_status_t cs_sa_id_dump(void)
{
	int i, j;
	cs_sa_id_t * sa_id;
	if (cs_adapt_debug & CS752X_ADAPT_TUNNEL)
	{
		printk("%s:%d \n", __func__, __LINE__);
		for (i = 0; i < 2; i++)
		{
			printk("######[%s]######\n", i ? "DOWN_STREAM" : "UP_STREAM");
			for ( j = 0; j < G2_MAX_SA_IDS; j++)
			{
				if(cs_sa_id_tbl[i][j] != NULL)
				{
					sa_id = cs_sa_id_tbl[i][j] ;
					printk(" ------------------------------\n");
					printk("  [sa_id = %d]\n", j);
					//printk("  Direction = %d\n", sa_id->direction);
					printk("  Encrypted tunnel = %s\n", sa_id->is_encrypted_tunnel ? "Yes" : "No");
					if(sa_id->is_used_for_rekey)
						printk("  Is used for rekey : %s\n", sa_id->is_used_for_rekey ? "Yes" : "No");
					printk("  tunnel_type = %d\n", sa_id->tunnel_type);
					printk(" ------------------------------\n");
				}
				else
					continue;
			}		
		}
	}
	return CS_OK;
}
EXPORT_SYMBOL(cs_sa_id_dump);

void cs_hw_accel_sa_id_init(void){
	memset(&cs_sa_id_tbl, 0, sizeof(cs_sa_id_tbl));
}

void cs_hw_accel_sa_id_exit(void)
{
}
