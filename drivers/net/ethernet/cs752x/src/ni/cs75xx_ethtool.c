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
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/types.h>
#include <linux/phy.h>
#include <linux/kernel.h>
#include "cs752x_eth.h"
#include "cs75xx_ethtool.h"

static int cs_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	mac_info_t *tp = netdev_priv(dev);
	if (tp->phydev)
		return phy_ethtool_gset(tp->phydev, cmd);
	else {
		cmd->advertising = tp->link_config.advertising;

		ethtool_cmd_speed_set(cmd, tp->link_config.speed);
		cmd->duplex = tp->link_config.duplex;
		cmd->phy_address = tp->mac_addr;
		cmd->autoneg = tp->link_config.autoneg;
	}
	return 0;

}

static int cs_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	mac_info_t *tp = netdev_priv(dev);
	int mac_interface;
	if (tp->phydev)
		return phy_ethtool_sset(tp->phydev, cmd);
	else {
		tp->link_config.advertising = cmd->advertising;
		tp->link_config.speed = ethtool_cmd_speed(cmd);
		mac_interface = (tp->link_config.speed == SPEED_1000) ? NI_MAC_PHY_RGMII_1000 : NI_MAC_PHY_RGMII_100;
		tp->link_config.duplex = cmd->duplex;
		tp->mac_addr = cmd->phy_address;
		tp->link_config.autoneg = cmd->autoneg;
		cs_ni_set_mac_speed_duplex(tp, mac_interface);
	}
	return 0;
}

static void cs_get_drvinfo(struct net_device *dev, 
		struct ethtool_drvinfo *drvinfo)
{
	char firmware_version[32];
	
	sprintf(firmware_version, "N/A");
	strncpy(drvinfo->fw_version, firmware_version, 32);
	strcpy(drvinfo->driver, DRV_NAME);
	strcpy(drvinfo->version, DRV_VERSION);
	strcpy(drvinfo->bus_info, "internal");
}

static int cs_get_regs_len(struct net_device *dev)
{
	return CS_REGDUMP_LEN;
}

static void cs_get_regs(struct net_device *dev, struct ethtool_regs *regs, 
		void *p)
{
	regs->version = 1;

	memset(p, 0, CS_REGDUMP_LEN);
	/* DUMP NI related register */
	memcpy_fromio(p, (void *)NI_TOP_NI_INTF_RST_CONFIG, CS_REGDUMP_LEN);
}

static void cs_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	printk("%s:: not implement !\n", __func__);
}

static int cs_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	printk("%s:: not implement !\n", __func__);
	return 0;
}

static u32 cs_get_msglevel(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	return tp->msg_enable;
}

static void cs_set_msglevel(struct net_device *dev, u32 data)
{
	mac_info_t *tp = netdev_priv(dev);
	tp->msg_enable = data;
}

static int cs_nway_reset(struct net_device *dev)
{
	mac_info_t *tp = netdev_priv(dev);
	return phy_start_aneg(tp->phydev);
}

static int cs_set_coalesce(struct net_device *dev, 
		struct ethtool_coalesce *cmd)
{
	printk("%s:: not implement !\n", __func__);
	return 0;
}

static int cs_get_coalesce(struct net_device *dev, 
		struct ethtool_coalesce *cmd)
{
	printk("%s:: not implement !\n", __func__);
	return 0;
}

static void cs_get_pauseparam(struct net_device *dev, 
		struct ethtool_pauseparam *pause)
{
	mac_info_t *tp = netdev_priv(dev);
	
	pause->autoneg = tp->link_config.autoneg;

	if (tp->link_config.flowctrl & FLOW_CTRL_RX)
		pause->rx_pause = 1;
	else
		pause->rx_pause = 0;

	if (tp->link_config.flowctrl & FLOW_CTRL_TX)
		pause->tx_pause = 1;
	else
		pause->tx_pause = 0;
}

static int cs_set_pauseparam(struct net_device *dev, 
		struct ethtool_pauseparam *pause)
{
	mac_info_t *tp = netdev_priv(dev);
	struct ethtool_pauseparam old;
	int err = 0;
	
	cs_get_pauseparam(dev, &old);
	
	tp->link_config.autoneg = pause->autoneg;
	
	if (pause->autoneg != old.autoneg) {
		tp->link_config.flowctrl = pause->autoneg ?
					0 : (FLOW_CTRL_TX | FLOW_CTRL_RX);
	} else {
		if (pause->rx_pause && pause->tx_pause)
			tp->link_config.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX;
		else if (pause->rx_pause && !pause->tx_pause)
			tp->link_config.flowctrl = FLOW_CTRL_RX;
		else if (!pause->rx_pause && pause->tx_pause)
			tp->link_config.flowctrl = FLOW_CTRL_TX;
		else if (!pause->rx_pause && !pause->tx_pause)
			tp->link_config.flowctrl = 0;
	}
		
	if (netif_running(dev)) {
		cs_ni_close(dev);
		err = cs_ni_open(dev);
		if (err) {
			dev_close(dev);
			return err;
		}
	}
	
	return 0;
}

static void cs_diag_test(struct net_device *dev, 
		struct ethtool_test *eth_test, u64 *data)
{
	printk("%s:: not implement !\n", __func__);
}

static void cs_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	switch(stringset) {
	case ETH_SS_STATS:
		memcpy(data, &ethtool_stats_keys, sizeof(ethtool_stats_keys));
		break;
	//case ETH_SS_TEST:
	//	printk("%s:: not finished\n", __func__);
		//memcpy(buf, &ethtool_test_keys, sizeof(ethtool_test_keys));
	//	break;	
	}
}

static void cs_get_ethtool_stats(struct net_device *dev, 
		struct ethtool_stats *stats, u64 *data)
{
	mac_info_t *tp = netdev_priv(dev);
	memcpy(data, cs_ni_update_stats(tp), sizeof(tp->stats));
}

static int cs_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return CS_NUM_STATS;
	//case ETH_SS_TEST:
	//	printk("%s:: not finished !\n", __func__);
	//	return 0;	
	default:
		return -EOPNOTSUPP;
	}
}

/* Ethtool operations. We may fill more later */
static const struct ethtool_ops cs_ethtool_ops = {
	.get_settings	= cs_get_settings,
	.set_settings	= cs_set_settings,
	.get_drvinfo	= cs_get_drvinfo,
	.get_regs_len	= cs_get_regs_len,
	.get_regs	= cs_get_regs,
	.get_wol	= cs_get_wol,
	.set_wol	= cs_set_wol,
	.get_msglevel	= cs_get_msglevel,
	.set_msglevel	= cs_set_msglevel,
	.nway_reset	= cs_nway_reset,
	.get_link	= ethtool_op_get_link,
	.get_coalesce	= cs_get_coalesce,
	.set_coalesce	= cs_set_coalesce,
	.get_pauseparam	= cs_get_pauseparam,
	.set_pauseparam	= cs_set_pauseparam,
	.self_test	= cs_diag_test,
	.get_strings	= cs_get_strings,
	.get_ethtool_stats	= cs_get_ethtool_stats,
	.get_sset_count	= cs_get_sset_count,
}; 

void cs_ni_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &cs_ethtool_ops;
}
