/* ========================================================================== */
/*                                                                            */
/*   pcie.h                                                                   */
/*   Cortina System (c) 2010 Author Stone                                     */
/*                                                                            */
/*   Description                                                              */
/*   For PCIe registers  define                                               */
/* ========================================================================== */


#define BIT0                                       0x00000001
#define BIT1                                       0x00000002
#define BIT2                                       0x00000004
#define BIT3                                       0x00000008
#define BIT4                                       0x00000010
#define BIT5                                       0x00000020
#define BIT6                                       0x00000040
#define BIT7                                       0x00000080
#define BIT8                                       0x00000100
#define BIT9                                       0x00000200
//Global register for PCIe config.
#define PCIE_GLBL_INTERRUPT_0                      PCIE_SATA_PCIE_GLBL_INTERRUPT_0
#define MSI_CTR_INT                                BIT8
#define Xmlh_link_up                               BIT9
#define PCIE_GLBL_INTERRUPT_ENABLE_0               PCIE_SATA_PCIE_GLBL_INTERRUPT_ENABLE_0
#define MSI_CTR_INT_EN                             BIT8
#define INTD_DEASSERT_EN                           BIT7
#define INTD_ASSERT_EN                             BIT6
#define INTC_DEASSERT_EN                           BIT5
#define INTC_ASSERT_EN                             BIT4
#define INTB_DEASSERT_EN                           BIT3
#define INTB_ASSERT_EN                             BIT2
#define INTA_DEASSERT_EN                           BIT1
#define INTA_ASSERT_EN                             BIT0

#define PCIE_GLBL_INTERRUPT_1                      PCIE_SATA_PCIE_GLBL_INTERRUPT_1
#define INTD_DEASSERT                           BIT7
#define INTD_ASSERT                             BIT6
#define INTC_DEASSERT                           BIT5
#define INTC_ASSERT                             BIT4
#define INTB_DEASSERT                           BIT3
#define INTB_ASSERT                             BIT2
#define INTA_DEASSERT                           BIT1
#define INTA_ASSERT                             BIT0

#define PCIE_GLBL_INTERRUPT_ENABLE_1               PCIE_SATA_PCIE_GLBL_INTERRUPT_ENABLE_1
#define PCIE_GLBL_AXI_MASTER_RESP_MISC_INFO        PCIE_SATA_PCIE_GLBL_AXI_MASTER_RESP_MISC_INFO
#define PCIE_GLBL_AXI_SLAVE_RESP_ERR_MAP           PCIE_SATA_PCIE_GLBL_AXI_SLAVE_RESP_ERR_MAP
#define PCIE_GLBL_AXI_MSTR_SLV_RESP_ERR_LOW_PW_MAP PCIE_SATA_PCIE_GLBL_AXI_MSTR_SLV_RESP_ERR_LOW_PW_MAP
#define PCIE_GLBL_CORE_CONFIG_REG                  PCIE_SATA_PCIE_GLBL_CORE_CONFIG_REG //offset 0x007

//#define   PCIE_GLBL_CORE_CONFIG_REG_diag_fast_link_mode BIT11
//#define   PCIE_GLBL_CORE_CONFIG_REG_diag_ctrl_bus       10:9
//#define   PCIE_GLBL_CORE_CONFIG_REG_cfg_pw_mg_en        BIT8
//#define   PCIE_GLBL_CORE_CONFIG_REG_fast_sim_i          BIT7
//#define   PCIE_GLBL_CORE_CONFIG_REG_ln1_resetn_i        BIT6
//#define   PCIE_GLBL_CORE_CONFIG_REG_cmu_resetn_i        BIT5
#define   PCIE_GLBL_CORE_CONFIG_REG_cfg_pcie0_clken        0x00000200
#define   PCIE_GLBL_CORE_CONFIG_REG_cfg_pcie1_clken        0x00000800
#ifndef CONFIG_CORTINA_FPGA
#define   PCIE_GLBL_CORE_CONFIG_REG_cfg_pcie2_clken        0x00001000
#endif
//#define   PCIE_GLBL_CORE_CONFIG_REG_app_init_rst        BIT3
//#define   PCIE_GLBL_CORE_CONFIG_REG_non_sticky_rst_n    BIT2
//#define   PCIE_GLBL_CORE_CONFIG_REG_sticky_rst_n        BIT
//debug_Aaron on 11/26/2010 for "UR" do not cause bus error
#define PCIE_GLBLCORE_CONFIG_REG_axi_ur_err_mask	 BIT5
#define   PCIE_GLBL_CORE_CONFIG_REG_app_ltssm_enable     BIT0

#define PCIE_GLBL_RCVD_ERR_COUNT                   0x008
#define PCIE_GLBL_PM_INFO_RESET_VOLT_STATUS        0x009
#define PCIE_GLBL_RADM_INFO_AXI_LOW_POWER_STS      0x00a
#define PCIE_GLBL_RTLH_INFO                        0x00b
#define PCIE_GLBL_AXI_MASTER_WR_MISC_INFO          0x00c
#define PCIE_GLBL_AXI_MASTER_RD_MISC_INFO          0x00d
#define PCIE_GLBL_AXI_SLAVE_BRESP_MISC_INFO        0x00e
#define PCIE_GLBL_AXI_SLAVE_RD_RESP_MISC_INFO      0x00f
#define PCIE_GLBL_CORE_DEBUG_0                     0x010
#define PCIE_GLBL_CORE_DEBUG_1                     0x011
#define PCIE_GLBL_CORE_DEBUG_2                     0x012
#define PCIE_GLBL_CORE_DEBUG_3                     0x013
#define PCIE_GLBL_CORE_DEBUG_4                     0x014
#define PCIE_GLBL_CORE_DEBUG_5                     0x015
#define PCIE_GLBL_CORE_DEBUG_6                     0x016
#define PCIE_GLBL_CORE_DEBUG_7                     0x017
#define PCIE_GLBL_CORE_DEBUG_8                     0x018
#define PCIE_GLBL_CORE_DEBUG_9                     0x019
#define PCIE_GLBL_CORE_DEBUG_10                    0x01a
#define PCIE_GLBL_CORE_DEBUG_11                    0x01b
#define PCIE_GLBL_CORE_DEBUG_12                    0x01c
#define PCIE_GLBL_CORE_DEBUG_13                    0x01d
#define PCIE_GLBL_CXPL_DEBUG_INFO_0                0x01e
#define PCIE_GLBL_CXPL_DEBUG_INFO_1                0x01f
#define PCIE_GLBL_PHY_STATUS                       0x020 //offset 0x020 
#define PCIE_GLBL_PCIE_CONTR_CFG_START_ADDR        0x021 //offset 0x021
#define PCIE_GLBL_PCIE_CONTR_CFG_END_ADDR          0x022 //offset 0x022  
#define PCIE_GLBL_PCIE_CONTR_CFG_ADDR_OFFSET       0x023 //offset 0x023

#define PCIE_GLBL_offset                           0x400
#define PCIE_RD_Fail                               0xffffffff

//PCIE Configuration register for Root Complex

#define PCI_CapPtr                0x034                //PCI-compatible base register groups begin                     
#define	PCI_Campatible_Header	  PCI_CapPtr
#define PM_NEXT_PTR               PCI_Campatible_Header + 0x01
#define MSI_NEXT_PTR              CFG_PM_CAP + 0x01

#define CFG_MSI_CAP               0x50

#define MSI_IRQ_MAP               0x10
//#define MSI_VECTOR                0x01
#define MSI_VECTOR                0x00

#define MSI_CONTORL               CFG_MSI_CAP + 0x02
#define MSI_CONTROL_ENABLE                BIT0   
#define MSI_LOWER_ADDRESS         CFG_MSI_CAP + 0x04
//#define G2_PCIE0_MSI_ADDRESS            0xF9FFFFFc        //MSI Address
#define G2_PCIE0_MSI_ADDRESS            0xE7FF0000        //MSI Address
#define G2_PCIE1_MSI_ADDRESS            0xFBFFFFFc        //MSI Address
#define MSI_UPPER_ADDRESS         CFG_MSI_CAP + 0x08
 
#define CFG_PCIE_CAP              MSI_NEXT_PTR
//#define CFG_MSIX_CAP              PCIE_NEXT_PTR
//#define CFG_SLOT_CAP              MSIX_NEXT_PTR
//#define CFG_VPD_CAP               SLOT_NEXT_PTR
    
             
 
#define PCIE_PORT_VIEWPORT        0x700+0x200 //Port Logic Viewport Register, offset 0x200

#define INBOUND                   0x80000000 // Inbound
#define OUTBOUND                  0x00000000 // Outbound
#define REGION_INDEX_0            0x00000000 // index 0
#define REGION_INDEX_1            0x00000001 // index 1
#define REGION_INDEX_2            0x00000002 // index 2
#define REGION_INDEX_3            0x00000003 // index 3
#define REGION_INDEX_4            0x00000004 // index 4
#define REGION_INDEX_5            0x00000005 // index 5
#define REGION_INDEX_6            0x00000006 // index 6
#define REGION_INDEX_7            0x00000007 // index 7
#define REGION_INDEX_8            0x00000008 // index 8
#define REGION_INDEX_9            0x00000009 // index 9
#define REGION_INDEX_10           0x0000000A // index 10
#define REGION_INDEX_11           0x0000000B // index 11
#define REGION_INDEX_12           0x0000000C // index 12
#define REGION_INDEX_13           0x0000000D // index 13
#define REGION_INDEX_14           0x0000000E // index 14
#define REGION_INDEX_15           0x0000000F // index 15
 
#define PCIE_PORT_REGION_CONTROL1 0x700+0x204 //Port Logic Region Control 1 Register, offset 0x204

#define TYPE_MEMORY               0x00000000 // type : Memory
#define TYPE_IO                   0x00000002 // type : I/O
#define TYPE_CF_0                 0x00000004 // type : Type 0 Configuration
#define TYPE_CF_1                 0x00000005 // type : Type 1 Configuration  

#define PCIE_PORT_REGION_CONTROL2 0x700+0x208 //Port Logic Region Control 2 Register, offset 0x208

#define ENABLE_REGION             0x80000000 //Enable Region
#define DISABLE_REGION            0x00000000

#define PCIE_PORT_LOWER_BASE      0x700+0x20c //Port Logic Lower Base Address Register, offset 0x20c
//Stone add for CFG, I/O, Memory address translation 
#define Lower_Base_Address_CFG_Region1   GOLDENGATE_PCIE1_BASE+0x1000
#define Upper_Base_Address_CFG_Region1   0x00000000

#define Lower_Base_Address_CFG_Region0   GOLDENGATE_PCIE0_BASE+0x1000
#define Upper_Base_Address_CFG_Region0   0x00000000

//debug_Aaron on 10/18/2010 PCIE_port_offset is not equal to GOLDENGATE_PCI_MEM_SIZE
//#define PCIE_1_offset                    0x20000000
//#define PCIE_port_offset                GOLDENGATE_PCI_MEM_SIZE 

#define Lower_Base_Address_IO_Region2   GOLDENGATE_PCIE0_BASE+0x1000
#define Upper_Base_Address_IO_Region2   0x00000000

#define End_of_MEM                      GOLDENGATE_PCIE0_BASE+0x5000

#define Limit_IO_region3                 0xB007EFFF
#define Limit_IO_region2                 0xB007EFFF 
#define Limit_CFG_region1                0xB00010FF
#define Limit_CFG_region0                GOLDENGATE_PCIE0_BASE+0x1000+0x0ff

//#define Lower_Target_address
#define Upper_Target_address             0x00000000             

#define PCIE_PORT_UPPER_BASE      0x700+0x210 //Port Logic Upper Base Address Register, offset 0x210
#define PCIE_PORT_LIMIT_BASE      0x700+0x214 //Port Logic Limit Base Address Register, offset 0x214
#define PCIE_PORT_LOWER_TARGET    0x700+0x218 //Port Logic Lower Target Address Register, offset 0x218
#define PCIE_PORT_UPPER_TARGET    0x700+0x21c //Port Logic Upper Target Address Register, offset 0x21c


//MSI function in Port Logic register
#define PCIE_PORT_MSI_CONTROLLER_ADDRESS         0x700+0x120
#define PCIE_PORT_MSI_ENABLE_ALL                 0xFFFFFFFF
#define PCIE_PORT_MSI_CONTROLLER_INT0_ENABLE     0x700+0x128
#define PCIE_PORT_MSI_CONTROLLER_INT0_MASK       0x700+0x12c
#define PCIE_PORT_MSI_CONTROLLER_INT0_STATUS     0x700+0x130

#define PCIE_RC_COUNT                            2

//#define IRQ_G2_PCIE_0                            20
//#define IRQ_G2_PCIE_1                            21

#define INTX_A_Offset                           10
#define INTX_B_Offset                           11
#define INTX_C_Offset                           12
#define INTX_D_Offset                           13 

#define IRQ_PCIE_0_INTA                          IRQ_PCIE0 + INTX_A_Offset 
#define IRQ_PCIE_0_INTB                          IRQ_PCIE0 + INTX_B_Offset
#define IRQ_PCIE_0_INTC                          IRQ_PCIE0 + INTX_C_Offset
#define IRQ_PCIE_0_INTD                          IRQ_PCIE0 + INTX_D_Offset

#define IRQ_PCIE_1_INTA                          IRQ_PCIE1 + INTX_A_Offset
#define IRQ_PCIE_1_INTB                          IRQ_PCIE1 + INTX_B_Offset
#define IRQ_PCIE_1_INTC                          IRQ_PCIE1 + INTX_C_Offset
#define IRQ_PCIE_1_INTD                          IRQ_PCIE1 + INTX_D_Offset

