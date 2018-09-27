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
#include <linux/phy.h>
#include <linux/workqueue.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/kmod.h>
#include "cs752x_eth.h"
#include "cs75xx_phy.h"

extern spinlock_t mdio_lock;

void cs_ni_phy_start(mac_info_t *tp)
{
	struct phy_device *phydev;

	if (!(tp->existed & CS_PHYFLG_IS_CONNECTED))
		return;

	phydev = tp->mdio_bus->phy_map[tp->phy_addr];

	phy_start(phydev);

	phy_start_aneg(phydev);
}

void cs_ni_phy_stop(mac_info_t *tp)
{
	//if (!(tp->phy_flags & CS_PHYFLG_IS_CONNECTED))
	//	return;

	phy_stop(tp->mdio_bus->phy_map[tp->phy_addr]);
}
 
static u16 cs_advert_flowctrl_1000T(u8 flow_ctrl)
{
	u16 miireg;

	if ((flow_ctrl & FLOW_CTRL_TX) && (flow_ctrl & FLOW_CTRL_RX))
		miireg = ADVERTISE_PAUSE_CAP;
	else if (flow_ctrl & FLOW_CTRL_TX)
		miireg = ADVERTISE_PAUSE_ASYM;
	else if (flow_ctrl & FLOW_CTRL_RX)
		miireg = ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
	else
		miireg = 0;

	return miireg;
}

static void cs_setup_flow_control(mac_info_t *tp, u32 lcladv, u32 rmtadv)
{
	u8 autoneg;
	u8 flowctrl = 0;

	autoneg = tp->link_config.autoneg;

	if (autoneg == AUTONEG_ENABLE)
		flowctrl = mii_resolve_flowctrl_fdx(lcladv, rmtadv);
	else
		flowctrl = tp->link_config.flowctrl;

	//tp->link_config.active_flowctrl = flowctrl;
	/* ask alan carr how to fill flow control */
	if (flowctrl & FLOW_CTRL_RX)
		cs_ni_flow_control(tp, NI_RX_FLOW_CTRL, CS_ENABLE);
	else
		cs_ni_flow_control(tp, NI_RX_FLOW_CTRL, CS_DISABLE);

	if (flowctrl & FLOW_CTRL_TX)
		cs_ni_flow_control(tp, NI_TX_FLOW_CTRL, CS_ENABLE);
	else
		cs_ni_flow_control(tp, NI_TX_FLOW_CTRL, CS_DISABLE);
	
	tp->link_config.flowctrl = flowctrl;
	return;
}

static void cs_ni_adjust_link(struct net_device *dev)
{
	//u8 oldflowctrl, linkmesg = 0;
	u32 lcl_adv, rmt_adv;
	mac_info_t *tp = netdev_priv(dev);
	struct phy_device *phydev = tp->mdio_bus->phy_map[tp->phy_addr];
	//unsigned long flags;
	int status_change = 0;
	
	//spin_lock_irqsave(&tp->link_lock, flags);
	
	//if (tp->link_config.speed == phydev->speed 
	//	&& tp->link_config.duplex == phydev->duplex) {
	//	return;
	//}
	
	if (phydev->link) {
		lcl_adv = 0;
		rmt_adv = 0;
		if (tp->link_config.speed != phydev->speed || 
	    		tp->link_config.duplex != phydev->duplex) {
		
			tp->link_config.speed = phydev->speed;
			tp->link_config.duplex = phydev->duplex;

			if (tp->phy_mode == NI_MAC_PHY_RGMII_1000) {
				if (phydev->speed == SPEED_100 || 
					phydev->speed == SPEED_10) {
					cs_ni_set_mac_speed_duplex(tp, 
							NI_MAC_PHY_RGMII_100);
				} else {
					cs_ni_set_mac_speed_duplex(tp, 
							NI_MAC_PHY_RGMII_1000);
				}
			} else {
				cs_ni_set_mac_speed_duplex(tp, tp->phy_mode);
			}
			status_change = 1;	
		}
		if (phydev->duplex == DUPLEX_FULL) {
			lcl_adv = cs_advert_flowctrl_1000T(
				  tp->link_config.flowctrl);

			if (phydev->pause)
				rmt_adv = LPA_PAUSE_CAP;
			if (phydev->asym_pause)
				rmt_adv |= LPA_PAUSE_ASYM;
		}
		tp->link_config.autoneg = phydev->autoneg;
		
		// FIXME - ACARR experiment on where CRC errors are coming from
		// Experiment was successful - 9:18am July-14th-2011
		// commented out to get working RX
		// Bug#37140: set Tx/Rx pause frame if the setting change
		if ((tp->link_config.lcl_adv != lcl_adv) ||
		    (tp->link_config.rmt_adv != rmt_adv)) {
		    tp->link_config.lcl_adv = lcl_adv;
		    tp->link_config.rmt_adv = rmt_adv;
		    cs_setup_flow_control(tp, lcl_adv, rmt_adv);
		}
	} 
	
	if (phydev->link != tp->link_config.link) {
		if (phydev->link) {
			/* delayed promiscuous enabling */
			if (dev->flags & IFF_PROMISC)
				cs_ni_set_rx_mode(dev);
		} else {
			tp->link_config.speed = -1;
			tp->link_config.duplex = -1;
			//phy_print_status(phydev);
		}
		tp->link_config.link = phydev->link;
		if (tp->link_config.link == 1)
			cs_ni_set_short_term_shaper(tp);
		status_change = 1;
	}

	//printk(KERN_INFO "%s: link up, speed %u Mb/s, %s duplex\n",
	//      dev->name, tp->link_config.speed, 
	//       tp->link_config.duplex ? "full" : "half");
	
	//spin_unlock_irqrestore(&tp->link_lock, flags);
	
	if (status_change && netif_msg_link(tp)) {
		phy_print_status(phydev);
		pr_debug("%s: MII BMCR %04x BMSR %04x LPA %04x\n",
			 dev->name,
			 phy_read(phydev, MII_BMCR),
			 phy_read(phydev, MII_BMSR),
			 phy_read(phydev, MII_LPA));

		/* If eth0 links up, run the command "/sbin/network_wan restart".
		 * If eth0 is static IP originally, the script forces to re-assign same IP.
		 * If eth0 is dynamic IP originally, the script restarts udhcpc to request IP.
		 */
		if ((phydev->link) && (strncmp(dev->name, "eth0", 4) == 0)) {
			char cmd_path[] = "/sbin/network_wan";
			char *cmd_argv[] = {cmd_path, "restart", NULL};
			char *env[] = {"HOME=/", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", NULL};
			call_usermodehelper(cmd_path, cmd_argv, env, 0);
		}
	}

}

static void set_phy_led_register(unsigned int port_id, unsigned int phy_addr)
{
		
	spin_lock(&mdio_lock);
	/* REG31 write 0x0007, set to extension page 7*/
	ni_mdio_write(phy_addr, 31, 0x0007);

	/* Write MII Phy 2 Reg 19 Data = 0xc03c */
	ni_mdio_write(phy_addr, 19, 0xc03c);

	if (port_id == GE_PORT0) {
		/* Write MII Phy 2 Reg 17 Data = 0x0b */
		ni_mdio_write(phy_addr, 17, 0x000b);
	} else if (port_id == GE_PORT1) {
		/* Write MII Phy 2 Reg 17 Data = 0xb0 */
		ni_mdio_write(phy_addr, 17, 0x00b0);
	}
	
	/* REG31 write 0X0000, back to page0 */
	ni_mdio_write(phy_addr, 31, 0x0000);
	spin_unlock(&mdio_lock);

}

static void set_phy_rmii_mode_register(unsigned int phy_addr)
{
		
	spin_lock(&mdio_lock);
	/* REG31 write 0x0007, set to extension page 7*/
	ni_mdio_write(phy_addr, 31, 0x0007);

	/* REG16 write 0x1FFB, set an external clock input to CKXTAL2 */
	ni_mdio_write(phy_addr, 16, 0x7FFB);

	/* REG31 write 0X0000, back to page0 */
	ni_mdio_write(phy_addr, 31, 0x0000);
	spin_unlock(&mdio_lock);

}

int cs_phy_init(mac_info_t *tp)
{
	struct phy_device *phydev;

	if (tp->existed & CS_PHYFLG_IS_CONNECTED)
		return 0;

	phydev = tp->mdio_bus->phy_map[tp->phy_addr];

#if 0	
	/* find the first phy */
	for (phy_addr = 0; phy_addr < PHY_MAX_ADDR; phy_addr++) {
		if (tp->mdio_bus->phy_map[phy_addr]) {
			if (phydev) {
				printk(KERN_ERR "%s: multiple PHYs found\n",
				       dev->name);
				return -EINVAL;
			}
			phydev = tp->mdio_bus->phy_map[phy_addr];
			break;
		}
	}
#endif
	
	if (!phydev) {
		printk(KERN_ERR "%s: no PHY found\n", tp->dev->name);
		return -ENODEV;
	}

#if !defined(CONFIG_CORTINA_REFERENCE_Q)
	/* reset PHY */
	spin_lock(&mdio_lock);
	ni_mdio_write(tp->phy_addr, 0, (1 << 15));
	spin_unlock(&mdio_lock);
	mdelay(50);
#endif
	
	/* for FPGA GE-0 is RMII, GE-1 and GE-2 is RGMII */
	switch (phydev->phy_id) {
	case PHY_ID_RTL8201E:
	case PHY_ID_RTL8201F:
		if (tp->phy_mode == NI_MAC_PHY_RMII)
			phydev->interface = PHY_INTERFACE_MODE_RMII;
		else
			phydev->interface = NI_MAC_PHY_MII;
		
		set_phy_led_register(tp->port_id, tp->phy_addr);
			
		if (tp->rmii_clk_src == 1)
			set_phy_rmii_mode_register(tp->phy_addr);
		
		printk("%s: PHY %s mode %d at addr %d\n",
			tp->dev->name, "RTL8201", tp->phy_mode, tp->phy_addr);
		break;
	case PHY_ID_RTL8211C:
	case PHY_ID_RTL8211E:
		phydev->interface = PHY_INTERFACE_MODE_RGMII;
		printk("%s: PHY %s mode %d at addr %d\n",
			tp->dev->name, "RTL8211", tp->phy_mode, tp->phy_addr);
		break;
	case PHY_ID_ATH0001:
		phydev->interface = PHY_INTERFACE_MODE_RMII;
		break;	
	case PHY_ID_ATH0002:
		phydev->interface = PHY_INTERFACE_MODE_RGMII;
		break;		
	case PHY_ID_ATH0003:
		phydev->interface = PHY_INTERFACE_MODE_RGMII;
		printk("%s: PHY %s mode %d at addr %d\n",
			tp->dev->name, "QCA8337", tp->phy_mode, tp->phy_addr);
		break;		
	default:
		phydev->interface = PHY_INTERFACE_MODE_RGMII;
		tp->phy_mode = NI_MAC_PHY_RGMII_1000;
		printk("%s: PHY %s mode NI_MAC_PHY_RGMII_1000 at addr %d\n",
			tp->dev->name, "Default", tp->phy_addr);
		break;
	}	

	/* Attach the MAC to the PHY. */
	phydev = phy_connect(tp->dev, dev_name(&phydev->dev), &cs_ni_adjust_link,
			     phydev->dev_flags, phydev->interface);
	if (IS_ERR(phydev)) {
		dev_err(&tp->dev->dev, "Could not attach to PHY\n");
		return PTR_ERR(phydev);
	}

	printk(KERN_INFO "GE-%d: attached PHY driver [%s] "
		"(mdio_bus:phy_addr=%s, id=0x%08x)\n",
		tp->port_id, phydev->drv->name, dev_name(&phydev->dev),
		phydev->phy_id);
	
	/* Mask with MAC supported features. */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_RGMII:
		phydev->supported |= (PHY_GBIT_FEATURES |
				      SUPPORTED_Pause |
				      SUPPORTED_Asym_Pause);
		break;
	case PHY_INTERFACE_MODE_RMII:	
	case PHY_INTERFACE_MODE_MII:
		phydev->supported |= (PHY_BASIC_FEATURES |
				      SUPPORTED_Pause |
				      SUPPORTED_Asym_Pause);
		break;
	default:
		phy_disconnect(tp->mdio_bus->phy_map[tp->phy_addr]);
		return -EINVAL;
	}

	tp->existed |= CS_PHYFLG_IS_CONNECTED;

	phydev->advertising = phydev->supported;
	
	//tp->link_config.link = 0;
	//tp->link_config.speed = -1;
	//tp->link_config.duplex = -1;
	/* flow control autonegotiation is default behavior */
	//tp->link_config.autoneg = AUTONEG_ENABLE;
	//tp->link_config.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX;
	tp->phydev = phydev;
	cs_ni_set_eth_cfg(tp, NI_CONFIG_1);
	
	cs_ni_set_mac_speed_duplex(tp, tp->phy_mode);
	
	return 0;
}

