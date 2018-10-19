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


/* If you have a VoIP Subscribe Line Interface Chip (SLIC) supported by the cs75xx_phone wrapper driver 
 * then pass board specific paramters to the driver here.
 * 
 * If you dont know what this is, leave it commented out 
 * This code is only enabled if you have defined CONFIG_PHONE_CS75XX_WRAPPER  
 *
 * A standard Linux array of resource structs is used to communicate board specific parameters to 
 * cs752x_phone VoIP SLIC wrapper. However, it is important to note that no real resource as being
 * reserved. Instead IORESOURCE_IRQ types are being used to communite integer based parameters to 
 * the cs752x_phone wrapper. 
 */

#if 0 /* template start */
        {
         .name = "ssp_index", 			/* Cortina Sound Port Index - aka SPORT */
         .start = 0,
         .end = 0,
         .flags = IORESOURCE_IRQ,
         },
        {
         .name = "sclk_sel",			/* Source Clock */
         .start = SPORT_INT_SYS_CLK_DIV, 	/* SPORT_INT_SYS_CLK_DIV or PORT_EXT_16384_CLK_DIV */
         .end = SPORT_INT_SYS_CLK_DIV,   	/* SPORT_INT_SYS_CLK_DIV or PORT_EXT_16384_CLK_DIV */
         .flags = IORESOURCE_IRQ,
         },
        {
         .name = "dev_num",
         .start = 1,
         .end = 1,
         .flags = IORESOURCE_IRQ,
         },
        {
         .name = "chan_num",
         .start = 2,
         .end = 2,
         .flags = IORESOURCE_IRQ,
         },
        {
         .name = "slic_reset",			/* Pin used to reset the SLIC chip */
         .start = GPIO_SLIC0_RESET,
         .end = GPIO_SLIC0_RESET,
         .flags = IORESOURCE_IRQ,
         },

#endif /* template end */
