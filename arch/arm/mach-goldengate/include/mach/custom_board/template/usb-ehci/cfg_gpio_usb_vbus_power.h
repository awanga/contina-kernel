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


/* lets try these default, they should be good for most designs based on Cortina reference boards */
#if 1 /* template start */

	/* USB VBUS 0 power enable ? */
#ifdef GPIO_USB_VBUS_POWER_0
        if (gpio_request(GPIO_USB_VBUS_POWER_0, "GPIO_USB_VBUS_POWER_0")) {
                printk("can't request GPIO_USB_VBUS_POWER_0 %d\n", GPIO_USB_VBUS_POWER_0);
                return -ENODEV;
        }


	/* Initialize GPIO pin for USB_VBUS_POWER_0 direction as an output */
	gpio_direction_output(GPIO_USB_VBUS_POWER_0, 0);

#if 1
	/* set output active HIGH to turn on */
	gpio_set_value(GPIO_USB_VBUS_POWER_0, 1);
#else
	/* set output active LOW to turn on */
	gpio_set_value(GPIO_USB_VBUS_POWER_0, 0);
#endif

#endif /* GPIO_USB_VBUS_POWER_0 end */


        /* USB VBUS 1 power enable ? */
#ifdef GPIO_USB_VBUS_POWER_1
        if (gpio_request(GPIO_USB_VBUS_POWER_1, "GPIO_USB_VBUS_POWER_1")) {
                printk("can't request GPIO_USB_VBUS_POWER_1 %d\n", GPIO_USB_VBUS_POWER_1);
                return -ENODEV;
        }


        /* Initialize GPIO pin for USB_VBUS_POWER_1 direction as an output */
        gpio_direction_output(GPIO_USB_VBUS_POWER_1, 0);

#if 1
        /* set output active HIGH to turn on */
        gpio_set_value(GPIO_USB_VBUS_POWER_1, 1);
#else
        /* set output active LOW to turn on */
        gpio_set_value(GPIO_USB_VBUS_POWER_1, 0);
#endif

#endif /* GPIO_USB_VBUS_POWER_1 end */


#endif /* template end */
