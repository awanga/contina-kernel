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
#ifndef __CS75XX_PHY__
#define __CS75XX_PHY__

#include <linux/skbuff.h>
#include <linux/phy.h>

/* Reltek PHY related */
#define PHY_ID_RTL8211C			0x001cc910
#define PHY_ID_RTL8201E			0x001CC815
#define PHY_ID_RTL8201F			0x001CC816
#define PHY_ID_RTL8211E			0x001CC915
#define PHY_ID_ATH0001			0x004DD023 /* check correct atheoth phy name */
#define PHY_ID_ATH0002			0x004DD04E
#define PHY_ID_ATH0003			0x004DD035 /* internal PHY of QCA8337N */

#define MAC_PHYCFG2_EMODE_MASK_RT8211	0x00000000
#define MAC_PHYCFG2_EMODE_MASK_RT8201	0x000001c0
#define MAC_PHYCFG2_EMODE_COMP_RT8211	0x00000800
#define MAC_PHYCFG2_EMODE_COMP_RT8201	0x00000000
#define MAC_PHYCFG2_FMODE_MASK_RT8211	0x00000000
#define MAC_PHYCFG2_FMODE_MASK_RT8201	0x00007000
#define MAC_PHYCFG2_GMODE_MASK_RT8211	0x00000000
#define MAC_PHYCFG2_GMODE_MASK_RT8201	0x001c0000
#define MAC_PHYCFG2_GMODE_COMP_RT8211	0x00200000
#define MAC_PHYCFG2_GMODE_COMP_RT8201	0x00000000
#define MAC_PHYCFG2_ACT_MASK_RT8211	0x03000000
#define MAC_PHYCFG2_ACT_MASK_RT8201	0x01000000
#define MAC_PHYCFG2_ACT_COMP_RT8211	0x00000000
#define MAC_PHYCFG2_ACT_COMP_RT8201	0x08000000
#define MAC_PHYCFG2_QUAL_MASK_RT8211	0x30000000
#define MAC_PHYCFG2_QUAL_MASK_RT8201	0x30000000
#define MAC_PHYCFG2_QUAL_COMP_RT8211	0x00000000
#define MAC_PHYCFG2_QUAL_COMP_RT8201	0x00000000
#define MAC_PHYCFG2_FMODE_COMP_RT8211	0x00038000
#define MAC_PHYCFG2_FMODE_COMP_RT8201	0x00000000

#define MAC_PHYCFG2_RTL8211C_LED_MODES \
	(MAC_PHYCFG2_EMODE_MASK_RT8211 | \
	 MAC_PHYCFG2_EMODE_COMP_RT8211 | \
	 MAC_PHYCFG2_FMODE_MASK_RT8211 | \
	 MAC_PHYCFG2_FMODE_COMP_RT8211 | \
	 MAC_PHYCFG2_GMODE_MASK_RT8211 | \
	 MAC_PHYCFG2_GMODE_COMP_RT8211 | \
	 MAC_PHYCFG2_ACT_MASK_RT8211 | \
	 MAC_PHYCFG2_ACT_COMP_RT8211 | \
	 MAC_PHYCFG2_QUAL_MASK_RT8211 | \
	 MAC_PHYCFG2_QUAL_COMP_RT8211)
#define MAC_PHYCFG2_RTL8201E_LED_MODES \
	(MAC_PHYCFG2_EMODE_MASK_RT8201 | \
	 MAC_PHYCFG2_EMODE_COMP_RT8201 | \
	 MAC_PHYCFG2_FMODE_MASK_RT8201 | \
	 MAC_PHYCFG2_FMODE_COMP_RT8201 | \
	 MAC_PHYCFG2_GMODE_MASK_RT8201 | \
	 MAC_PHYCFG2_GMODE_COMP_RT8201 | \
	 MAC_PHYCFG2_ACT_MASK_RT8201 | \
	 MAC_PHYCFG2_ACT_COMP_RT8201 | \
	 MAC_PHYCFG2_QUAL_MASK_RT8201 | \
	 MAC_PHYCFG2_QUAL_COMP_RT8201)


int cs_phy_probe(mac_info_t *tp);
int cs_phy_init(mac_info_t *tp);
void cs_ni_phy_start(mac_info_t *tp);
void cs_ni_phy_stop(mac_info_t *tp);
#endif
