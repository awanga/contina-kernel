#ifndef __GPIO_ALLOC_H__
#define __GPIO_ALLOC_H__

#include <mach/gpio.h>


#if defined(CONFIG_CORTINA_CUSTOM_BOARD)

/* custom board patch start HERE: */

/* Review contents of included template below. After review, apply patch to replace 
 * everything between the start HERE: comment above to the and end HERE: comment below
 * including the start HERE: and end HERE: lines themselves. 
 *
 * This patch should also remove the warning below and also change inclusion path to be a location 
 * within YOUR own custom_board/my_board_name tree which will not be overwritten by
 * future Cortina releases.   
 *
 * WARNING: Do NOT remove or change the CONFIG_CORTINA_CUSTOM_BOARD pre-processor definition name above.
 * Cortina will only support custom board builds which use the CONFIG_CORTINA_CUSTOM_BOARD definition.
 */

#warning CUSTOM_BOARD_REVIEW_ME
#include <mach/custom_board/template/gpio/cfg_gpio_alloc.h>

/* custom board patch end HERE: */

#elif defined(CONFIG_CORTINA_ENGINEERING)

#define GPIO_PFLASH_RESETN_CNTL GPIO2_BIT18
#define GPIO_FLASH_WP_CNTRL     GPIO2_BIT21
#define GPIO_SLIC0_RESET        GPIO2_BIT22
#define GPIO_SLIC1_RESET        GPIO2_BIT23
#define GPIO_SLIC_INT           GPIO2_BIT26
#define GPIO_HDMI_INT           GPIO2_BIT27	/* GPIO INTR from HDMI and Touch panel */
#define GPIO_PCIE_RESET         GPIO2_BIT28	/* #define GPIO2_BIT28  32*2+28 */
#define GPIO_USB_VBUS_POWER_0   GPIO2_BIT24
#define GPIO_USB_VBUS_POWER_1   GPIO2_BIT25
/*#define GPIO_USB_STORAGE_LED_0  GPIO1_BIT12*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO1_BIT12*/

#elif defined(CONFIG_CORTINA_REFERENCE)

#define GPIO_FLASH_WP_CNTRL     GPIO2_BIT19
#define GPIO_VOIP_LED           GPIO2_BIT20
#define GPIO_SLIC0_RESET        GPIO2_BIT22
#define GPIO_SLIC_INT           GPIO2_BIT26
#define GPIO_PCIE_RESET         GPIO2_BIT28	/* #define GPIO2_BIT28  32*2+28 */
#define GPIO_USB_VBUS_POWER_0   GPIO2_BIT24
#define GPIO_USB_VBUS_POWER_1   GPIO2_BIT25
/*#define GPIO_USB_STORAGE_LED_0  GPIO1_BIT12*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO1_BIT12*/

#elif defined(CONFIG_CORTINA_REFERENCE_B)

#define GPIO_FLASH_WP_CNTRL     GPIO2_BIT19
#define GPIO_VOIP_LED           GPIO2_BIT20
#define GPIO_SLIC0_RESET        GPIO2_BIT22
#define GPIO_SLIC_INT           GPIO2_BIT26
#define GPIO_PCIE_RESET         GPIO2_BIT28	/* #define GPIO2_BIT28  32*2+28 */
#define GPIO_USB_VBUS_POWER_0   GPIO2_BIT24
#define GPIO_USB_VBUS_POWER_1   GPIO2_BIT25
/*#define GPIO_USB_STORAGE_LED_0  GPIO1_BIT19*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO1_BIT19*/
#define GPIO_USB_LED            GPIO1_BIT19
#define GPIO_FACTORY_DEFAULT    GPIO1_BIT5
#define GPIO_WPS_SW             GPIO1_BIT16
#define GPIO_SYS_RDY_LED        GPIO1_BIT18
#define GPIO_WIFI_DISABLE       GPIO2_BIT8
#define GPIO_WIFI_SW            GPIO2_BIT10
#define GPIO_WPS_SYS_LED        GPIO2_BIT11

#define FACTORY_DEFAULT_TIME            5
#define GPIO_BTNS_POLLING_INTERVAL	1000

#elif defined(CONFIG_CORTINA_ENGINEERING_S)

#define GPIO_VOIP_LED           GPIO1_BIT3
#define GPIO_SLIC0_RESET        GPIO1_BIT5
#define GPIO_PCIE_RESET         GPIO2_BIT11
/*#define GPIO_USB_VBUS_POWER_0   GPIO0_BIT26*/
#define GPIO_USB_VBUS_POWER_1   GPIO0_BIT26
/*#define GPIO_USB_STORAGE_LED_0  GPIO2_BIT12*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO2_BIT12*/
#define GPIO_USB_LED            GPIO2_BIT12

#elif defined(CONFIG_CORTINA_REFERENCE_S)

#define GPIO_FLASH_WP_CNTRL     GPIO2_BIT19
#define GPIO_VOIP_LED           GPIO1_BIT3
#define GPIO_SLIC0_RESET        GPIO1_BIT5
#define GPIO_PCIE_RESET         GPIO2_BIT11	/* #define GPIO2_BIT28  32*2+28 */
/*#define GPIO_USB_VBUS_POWER_0  GPIO0_BIT26*/
#define GPIO_USB_VBUS_POWER_1   GPIO0_BIT26
/*#define GPIO_USB_STORAGE_LED_0  GPIO2_BIT12*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO2_BIT12*/
#define GPIO_USB_LED            GPIO2_BIT12
#define GPIO_FACTORY_DEFAULT    GPIO1_BIT10
#define GPIO_SYS_RDY_LED        GPIO2_BIT13
#define GPIO_WIFI_SW            GPIO3_BIT13
#define GPIO_WPS_SW             GPIO3_BIT14
#define GPIO_WIFI_DISABLE       GPIO3_BIT21
#define GPIO_WPS_SYS_LED0       GPIO3_BIT22
#define GPIO_WPS_SYS_LED1       GPIO3_BIT23

#define FACTORY_DEFAULT_TIME            5
#define GPIO_BTNS_POLLING_INTERVAL	1000

#elif defined(CONFIG_CORTINA_FPGA)

#define GPIO_SLIC0_RESET        GPIO2_BIT7
#define GPIO_SLIC_INT           GPIO1_BIT4

#elif defined(CONFIG_CORTINA_PON)

#define GPIO_VOIP_LED           GPIO2_BIT20
#define GPIO_SLIC0_RESET        GPIO2_BIT22
#define GPIO_SLIC_INT           GPIO2_BIT26
#define GPIO_HDMI_INT           GPIO2_BIT27
#define GPIO_PCIE_RESET         GPIO2_BIT28
#define GPIO_USB_VBUS_POWER_0   GPIO2_BIT24
/*#define GPIO_USB_VBUS_POWER_1   GPIO2_BIT25*/
/*#define GPIO_USB_STORAGE_LED_0  GPIO1_BIT19*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO1_BIT19*/
#define GPIO_USB_LED            GPIO1_BIT19
#define GPIO_FACTORY_DEFAULT    GPIO1_BIT5
#define GPIO_SYS_RDY_LED        GPIO1_BIT18
#define GPIO_WIFI_SW            GPIO2_BIT10
#define GPIO_WIFI_DISABLE       GPIO2_BIT8
#define GPIO_WPS_SW             GPIO1_BIT16
#define GPIO_WPS_SYS_LED0       GPIO2_BIT11
#define GPIO_WPS_SYS_LED1       GPIO1_BIT17
#define GPIO_EPON_INT           GPIO2_BIT19
#define GPIO_WAN_INT            GPIO2_BIT29

#define FACTORY_DEFAULT_TIME            5
#define GPIO_BTNS_POLLING_INTERVAL	1000

#elif defined(CONFIG_CORTINA_WAN) || defined(CONFIG_CORTINA_REFERENCE_Q)

#define GPIO_VOIP_LED           GPIO2_BIT20
#define GPIO_SLIC0_RESET        GPIO2_BIT22
#define GPIO_SLIC_INT           GPIO2_BIT26
#define GPIO_HDMI_INT           GPIO2_BIT27
#define GPIO_PCIE_RESET         GPIO2_BIT28
#define GPIO_PCIE0_RESET        GPIO2_BIT18
#define GPIO_PCIE1_RESET        GPIO2_BIT19
#define GPIO_USB_VBUS_POWER_0   GPIO2_BIT24
#define GPIO_USB_VBUS_POWER_1   GPIO2_BIT25	/* new HW support */
/*#define GPIO_USB_STORAGE_LED_0  GPIO1_BIT19*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO1_BIT19*/
#define GPIO_USB_LED            GPIO1_BIT19
#define GPIO_FACTORY_DEFAULT    GPIO1_BIT5
#define GPIO_SYS_RDY_LED        GPIO1_BIT18
#define GPIO_WIFI_SW            GPIO2_BIT10
#define GPIO_WIFI_DISABLE       GPIO2_BIT8
#define GPIO_WPS_SW             GPIO1_BIT16
#define GPIO_WPS_SYS_LED0       GPIO2_BIT11
#define GPIO_WPS_SYS_LED1       GPIO1_BIT17
#define GPIO_EPON_INT           GPIO2_BIT19
#define GPIO_WAN_INT            GPIO2_BIT29

#define FACTORY_DEFAULT_TIME            5
#define GPIO_BTNS_POLLING_INTERVAL	1000

#elif defined(CONFIG_CORTINA_BHR)

#define GPIO_PCIE_RESET         GPIO2_BIT28
#define GPIO_USB_VBUS_POWER_0   GPIO2_BIT24
#define GPIO_USB_VBUS_POWER_1   GPIO2_BIT25
/*#define GPIO_USB_STORAGE_LED_0  GPIO1_BIT19*/
/*#define GPIO_USB_STORAGE_LED_1  GPIO1_BIT19*/
#define GPIO_USB_LED0           GPIO1_BIT19
#define GPIO_USB_LED1           GPIO2_BIT20
#define GPIO_FACTORY_DEFAULT    GPIO1_BIT5
#define GPIO_SYS_RDY_LED        GPIO1_BIT18
#define GPIO_WIFI_SW            GPIO2_BIT10
#define GPIO_WIFI_DISABLE       GPIO2_BIT8
#define GPIO_WPS_SW             GPIO1_BIT16
#define GPIO_WPS_SYS_LED0       GPIO2_BIT11
#define GPIO_WPS_SYS_LED1       GPIO1_BIT17
#define GPIO_EPON_INT           GPIO2_BIT19
#define GPIO_WAN_INT            GPIO2_BIT29

#define FACTORY_DEFAULT_TIME            5
#define GPIO_BTNS_POLLING_INTERVAL	1000
#endif

#endif/* __GPIO_ALLOC_H__ */

