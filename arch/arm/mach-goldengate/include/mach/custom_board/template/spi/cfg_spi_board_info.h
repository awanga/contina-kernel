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

/*  SPI Device Tree Table for this board  */

/* No easy assumptions can be able about what kind of SPI devices are on custom boards.
 * Therefore, we can not turn on any good defaults for all boards. 
 * If you have SPI devices attached to the Cortina SPI master controller, then
 * this table needs to be turned on only after it has been filled out with real info.
 */

#if 0 /* template start */

/* The following is a list of spi_board_info structures 
 * see https://www.kernel.org/doc/htmldocs/device-drivers/API-struct-spi-board-info.html
 * for the format of this structure

        {
         .modalias = "my_spi_device_driver_name",
         .platform_data = PASS_THIS_VALUE_TO_SPI_DRIVER,
         .irq = MY_INTERRUPT,
         .max_speed_hz = 8000000,
         .bus_num = 0,
         .chip_select = 1,
         .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
         },
        {
         .modalias = "another_spi_driver_name",
         .platform_data = HERE_IS_A_DIFFERENT_VALUE_TO_BASE,
         .irq = GPIO_SLIC_INT,
         .max_speed_hz = 8000000,
         .bus_num = 0,
         .chip_select = 2,
         .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
         },
        {
         .modalias = "and_another_spi_driver_name",
         .platform_data = 0, /* pass nothing */
         .irq = INTERRUPT_FOR_THIS_3RD_DEVICE,
         .max_speed_hz = 8000000,
         .bus_num = 1,
         .chip_select = 1,
         .mode = (CS75XX_SPI_MODE_ISAM)|SPI_MODE_3
         }


#endif /* template end */
