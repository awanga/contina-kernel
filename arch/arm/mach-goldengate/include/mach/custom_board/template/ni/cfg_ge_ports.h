/* 2012 (c) Copyright Cortina Systems Inc.
 * Author: Alex Nemirovsky <alex.nemirovsky@cortina-systems.com>
 *
 * This file is licensed under the terms of the GNU General Public License version 2. 
 * This file is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * WARNING!!: DO NOT MODIFY THIS TEMPLATE FILE 
 * 
 * Future Cortina releases updates will overwrite this location. 
 *
 * Instead, copy out this template into your own custom_board/my_board_name tree 
 * and create a patch against the Cortina source code which included this template file
 * from this location. When your code is fully functional, your patch should also 
 * remove the #warning message from the code which directed you
 * to this template file for inspection and customization.
 */ 

/* Configure Cortina NI Gigabit Ethernet Ports */

/* these are likely good defaults for most set ups, lets try them out */
	[GE_PORT0_CFG] = {
#if (GMAC0_PHY_MODE == NI_MAC_PHY_RGMII_1000)
		.speed = SPEED_1000,
#else
		.speed = SPEED_100,
#endif
		.auto_nego = AUTONEG_DISABLE,
		.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.irq = IRQ_NI_RX_XRAM0,
		.phy_mode = GMAC0_PHY_MODE,
		.full_duplex = DUPLEX_FULL,
		.port_id = GE_PORT0,
		.phy_addr = GE_PORT0_PHY_ADDR,
		.mac_addr = (&(eth_mac[0][0])),
#if defined(CONFIG_CS75XX_GMAC0_RMII) && \
	defined(CONFIG_CS75XX_INT_CLK_SRC_RMII_GMAC0)
		.rmii_clk_src = 1,
#else
		.rmii_clk_src = 0,
#endif
	},

	[GE_PORT1_CFG] = {
#if (GMAC1_PHY_MODE == NI_MAC_PHY_RGMII_1000)
		.speed = SPEED_1000,
#else
		.speed = SPEED_100,
#endif
		.auto_nego = AUTONEG_DISABLE,
		.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.phy_mode = GMAC1_PHY_MODE,
		.irq = IRQ_NI_RX_XRAM1,
		.full_duplex = DUPLEX_FULL,
		.port_id = GE_PORT1,
		.phy_addr = GE_PORT1_PHY_ADDR,
		.mac_addr = (&(eth_mac[1][0])),
#if defined(CONFIG_CS75XX_GMAC1_RMII) && \
	defined(CONFIG_CS75XX_INT_CLK_SRC_RMII_GMAC1)
		.rmii_clk_src = 1,
#else
		.rmii_clk_src = 0,
#endif
	},
	[GE_PORT2_CFG] = {
		.auto_nego = AUTONEG_DISABLE,
		.speed = SPEED_1000,
		.irq = IRQ_NI_RX_XRAM2,
		.phy_mode = NI_MAC_PHY_RGMII_1000,
		.flowctrl = FLOW_CTRL_TX | FLOW_CTRL_RX,
		.phy_mode = GMAC2_PHY_MODE,
		.irq = IRQ_NI_RX_XRAM2,
		.full_duplex = DUPLEX_FULL,
		.port_id = GE_PORT2,
		.phy_addr = GE_PORT2_PHY_ADDR,
		.mac_addr = (&(eth_mac[2][0])),
#if defined(CONFIG_CS75XX_GMAC2_RMII) && \
	defined(CONFIG_CS75XX_INT_CLK_SRC_RMII_GMAC2)
		.rmii_clk_src = 1,
#else
		.rmii_clk_src = 0,
#endif
	}

