/*
 * arch/arm/mach-mv78xx0/pcie.c
 *
 * PCIe functions for Marvell MV78xx0 SoCs
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/mbus.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <asm/mach/pci.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <mach/hardware.h>
#include <linux/export.h>

#include <linux/slab.h> //for linux 2.6.36
#include <linux/suspend.h>


//debug_Aaron
#include <linux/delay.h>
#include <linux/sched.h>
#include <mach/gpio_alloc.h>
#include <linux/if.h>

#ifdef CONFIG_CS752X_PROC
#include <linux/proc_fs.h>
extern u32 cs_pcie_debug;
#endif

#include "pcie.h"
//#include "msi.h"

#ifndef CONFIG_CORTINA_FPGA 
#include "sb_phy.c"
#endif

#include <mach/cs_pwrmgt.h>

extern int pm_phy_mode; /* variable defined in kernel/power/main.c */
extern cs_status_t cs_pm_pcie_state_set(CS_IN cs_dev_id_t  dev_id,
		CS_IN  cs_uint32_t  pcie_id, CS_IN cs_pm_state_t  state);

#define NUM_PCIE_DEVICE	32
struct pcie_device_identifier{
	unsigned char bus_number;
	unsigned int vendor_device;
	unsigned int subsystem_subvendor;
	int isMultiFuncDevice;
};

#define MAX_CFG_REGS	64
struct pci_cfg_regs {
	struct pci_bus 	*bus;
	u32 		devfn;
	int 		where;
	int 		size;
	u32		val;
	u32		valid;
};

struct pcie_port {
	unsigned char		rc_number;
	unsigned char		root_bus_nr;
	unsigned int		base_addr;				/* the remapped base address */
	unsigned int		cnf_base_addr;     		 	/* the remapped conf base address */
	int			link_up;
	struct pcie_device_identifier	devid[NUM_PCIE_DEVICE];	/* record the vendor ID and device ID of devices after RC */
	spinlock_t		conf_lock;
	struct pci_dev 		*pdev;
	char			io_space_name[32];
	char			mem_space_name[32];
	struct resource		res[3];
	struct pci_cfg_regs	dev_cfg[MAX_CFG_REGS];
	struct pci_cfg_regs	rc_cfg[MAX_CFG_REGS];
};

//debug_Aaron on 11/29/2010, select from make menuconfig
//#define PCIE_DEBUG 1
//#define NUM_PCIE_PORTS  2 
//#define NUM_PCIE_PORTS  1
#ifdef CONFIG_CORTINA_FPGA
#define CONFIG_PCIE_NUM_PORTS 2
static int num_port_sb_phy  = 2;
#else
#ifdef CONFIG_CORTINA_ENGINEERING
#define CONFIG_PCIE_NUM_PORTS 3 
static int num_port_sb_phy  = 3;
#else
#define CONFIG_PCIE_NUM_PORTS 3 
static int num_port_sb_phy  = 3;
#endif
#endif

#define NUM_PCIE_PORTS  CONFIG_PCIE_NUM_PORTS 
static struct pcie_port pcie_port[NUM_PCIE_PORTS];

#define PCIE_DEBUG_INIT	0x1
#define PCIE_DEBUG_CONF	0x1
#define PCIE_DEBUG_IRQ	0x4

//debug_Aaron IRQ resources array
#define NUM_PCIE_IRQS 24
struct pcie_irq_resource{
	int used; 
	unsigned char rc_bus_number;
	struct pci_dev *pdev;
	unsigned char interrupt_pin;
};
static struct pcie_irq_resource pcie_irqs[NUM_PCIE_IRQS];

#define MSI_BUF_OFFSET        8

//static struct resource pcie_io_space;
//static struct resource pcie_mem_space;
//static struct pci_dev *g2_pci_dev;

int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc);
//void g2_pcie_msi_handler(unsigned int irq, struct irq_desc *desc);
static struct pcie_port *bus_to_port(unsigned char bus);
static void __init add_pcie_port(int rc_number, unsigned int base_addr, unsigned int cnf_base_addr);
static int g2_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where, int size, u32 *val);

static int g2_pcie_save_dev_cfg(struct pcie_port *pp, struct pci_bus *bus, u32 devfn, int where, int size, u32 val);
static int g2_pcie_save_rc_cfg(struct pcie_port *pp, struct pci_bus *bus, u32 devfn, int where, int size, u32 val);
static int g2_pcie_restore_rc_cfg(int	id);
static int g2_pcie_restore_dev_cfg(int	id);

#ifdef CONFIG_CS752X_PROC
static void g2_pcie_read_err_counts(int port_number);
#endif

unsigned int GOLDENGATE_PCIE0_BASE_remap, GOLDENGATE_PCIE0_CF_BASE_remap, GOLDENGATE_PCIE0_MEM_BASE_remap;

//debug_Aaron for RC 1
unsigned int GOLDENGATE_PCIE1_BASE_remap, GOLDENGATE_PCIE1_CF_BASE_remap, GOLDENGATE_PCIE1_MEM_BASE_remap;
//unsigned int PCIE_port_offset = 0;

//debug_Aaron on 04/12/2011 for SoC, RC2
#ifndef CONFIG_CORTINA_FPGA
unsigned int GOLDENGATE_PCIE2_BASE_remap, GOLDENGATE_PCIE2_CF_BASE_remap, GOLDENGATE_PCIE2_MEM_BASE_remap;
#endif


//debug_Aaron for MSI buffer
unsigned char *msi_buf_addr;

#define PCIe_bridge_flag 1 /* our IP should always be bridge mode */
#define MSI_INTx_Flag 1    /* 0 = INT mode or 1 = MSI mode */
#define G2_PCIE0_MSI_OFFSET 0x2000

//debug_Aaron
static int in_scanning_bus;

//static DECLARE_BITMAP(msi_irq_in_use, IRQ_PCIE0);

static int  pcie_set_vendor_device(struct pcie_port *pp, unsigned int vendor_device, unsigned int subsystem_subvendor, 
		                   unsigned char bus_number, int isMultiFuncDevice)
{
	int i;

	for (i = 0; i < NUM_PCIE_DEVICE; i++)
	{
		if (pp->devid[i].vendor_device == 0xffffffff)
			break;
	}
	if (i == NUM_PCIE_DEVICE)
	{
		printk("%s: number of device of RC %d is exceeded!!!\r\n", __func__, pp->rc_number);
		return -1;
	}
	pp->devid[i].bus_number = bus_number;
	pp->devid[i].vendor_device = vendor_device;
	pp->devid[i].subsystem_subvendor = subsystem_subvendor;
	pp->devid[i].isMultiFuncDevice = isMultiFuncDevice;

#ifdef PCIE_DEBUG
        printk("%s: vendor_device=0x%x, subsystem_subvendor=0x%x bus=%d, rc_number=%d!!!\r\n", 
                __func__, vendor_device, subsystem_subvendor,bus_number, pp->rc_number);
#endif
	return 0;
}

static int  pcie_find_vendor_device(struct pcie_port *pp, unsigned int vendor_device, unsigned int subsystem_subvendor,
				    unsigned char bus_number, int isMultiFuncDevice)
{
    int i;

    for (i = 0; i < NUM_PCIE_DEVICE; i++)
    {
		if (subsystem_subvendor == 0xffffffff)  /* is a bridge does not have subsystem and subvendor IDs */	
		{
       		if ((pp->devid[i].vendor_device == vendor_device) && (pp->devid[i].bus_number == bus_number))
            		break;
		}
		else
		{
			if ((pp->devid[i].vendor_device == vendor_device) && (pp->devid[i].bus_number == bus_number)
		    	   && (pp->devid[i].subsystem_subvendor == subsystem_subvendor))
			{
				   	if (pp->devid[i].isMultiFuncDevice)
					{
#ifdef PCIE_DEBUG
       				 printk("%s: vendor_device=0x%x, subsystem_subvendor=0x%x is a multi function device, at RC %d!!!\r\n",
        			__func__, vendor_device, subsystem_subvendor, pp->rc_number);
#endif
						return -1;			
					}
                   break;
			}
		}
    }
    if (i == NUM_PCIE_DEVICE)
    {
#ifdef PCIE_DEBUG
        printk("%s: vendor_device=0x%x, subsystem_subvendor=0x%x not found, at RC %d!!!\r\n", 
		__func__, vendor_device, subsystem_subvendor, pp->rc_number);
#endif
        return 0;
    }
#ifdef PCI_DEBUG
	printk("%s: vendor_device=0x%x, subsystem_subvendor=0x%x is found, at RC %d!!!\r\n", 
		__func__, vendor_device, subsystem_subvendor, pp->rc_number);
#endif	
    return 1;
}

/* find the Root Complex where the device is connected */
static struct pcie_port *pcie_find_RC_by_bus(struct pci_bus *bus)
{
	struct pci_bus *tmp_bus;
	struct pcie_port *pp;

	tmp_bus = bus;
    for (;;)
    {

        tmp_bus = tmp_bus->parent;

        if (tmp_bus == NULL)
   		{
			return NULL;
    	}
        pp = bus_to_port(tmp_bus->number);
        if (pp != NULL)
			return pp;
    }
}

//debug_Aaron
static int pcie_find_free_irq(struct pci_dev *pdev, unsigned char rc_bus_number)
{
	int i;
	u32 val;

	for (i = 0; i < NUM_PCIE_IRQS; i++)
	{
		if (pcie_irqs[i].used == 0)
		{
#ifdef PCIE_DEBUG
			printk("%s: found a free irq index %d\r\n", __func__, i);
#endif
			break;
		}
	}
	if (i == NUM_PCIE_IRQS)
	{
		printk("%s: run out of irqs!!!\r\n", __func__);
		return -ENOSPC;
	}
	pcie_irqs[i].used = 1;
	pcie_irqs[i].rc_bus_number = rc_bus_number;
	pcie_irqs[i].pdev = pdev;

	//debug_Aaron on 11/10/2010 for multiple function PCI device
 	g2_pcie_rd_conf(pdev->bus, pdev->devfn, PCI_INTERRUPT_PIN, 1, &val);
//#ifdef PCIE_DEBUG
	printk("%s: interrupt pin of bus=%d, devfn=0x%x is %d\r\n", __func__, pdev->bus->number, pdev->devfn, val);	
//#endif
	//debug_Aaron for debugging
	pcie_irqs[i].interrupt_pin = (unsigned char)val;
	
	return PCIE_IRQ_BASE + i;
}

static int pcie_find_irq_by_pdev(struct pci_dev *pdev)
{
	int i;
    for (i = 0; i < NUM_PCIE_IRQS; i++)
    {   
        if ((pcie_irqs[i].used == 1) && (pcie_irqs[i].pdev == pdev) && 
			(pcie_irqs[i].pdev->bus->number == pdev->bus->number) && (pcie_irqs[i].pdev->devfn == pdev->devfn))
        {       
#ifdef PCIE_DEBUG
            printk("%s: found a irq index %d\r\n", __func__, i);
#endif
            break;      
        }       
    }   
    if (i == NUM_PCIE_IRQS)
    {   
        printk("%s: Not found!!!\r\n", __func__);
        return -ENOSPC;
    }
    return PCIE_IRQ_BASE + i; 
}

static int pcie_find_irq_by_rc_bus_number(unsigned char rc_bus_number)
{
    int i;
    for (i = 0; i < NUM_PCIE_IRQS; i++)
    {
        if ((pcie_irqs[i].used == 1) && (pcie_irqs[i].rc_bus_number == rc_bus_number))
        {
#ifdef PCIE_DEBUG
            printk("%s: found a irq index %d\r\n", __func__, i);
#endif
            break;
        }
    }
    if (i == NUM_PCIE_IRQS)
    {
        printk("%s: Not found!!!\r\n", __func__);
        return -ENOSPC;
    }
    return PCIE_IRQ_BASE + i;
}

static int pcie_find_irq_by_rc_bus_number_and_interrupt_pin(unsigned char rc_bus_number, unsigned char interrupt_pin)
{
    int i;
    for (i = 0; i < NUM_PCIE_IRQS; i++)
    {
#ifdef PCIE_DEBUG
	printk("%s: i=%d, pcie_irqs[i].used=%d, pcie_irqs[i].rc_bus_number=%d, pcie_irqs[i].interrupt_pin=%d, rc_bus_number=%d, interrupt_pin=%d\r\n", __func__, i,  pcie_irqs[i].used,  pcie_irqs[i].rc_bus_number, pcie_irqs[i].interrupt_pin, rc_bus_number, interrupt_pin);
#endif
        if ((pcie_irqs[i].used == 1) && (pcie_irqs[i].rc_bus_number == rc_bus_number) &&
                (pcie_irqs[i].interrupt_pin == interrupt_pin))
        {
#ifdef PCIE_DEBUG
            printk("%s: found a irq index %d\r\n", __func__, i);
#endif
            break;
        }
    }
    if (i == NUM_PCIE_IRQS)
    {
        printk("%s: Not found!!!\r\n", __func__);
        return -ENOSPC;
    }
    return PCIE_IRQ_BASE + i;
}


static int pcie_find_irq_by_device_and_interrupt_pin(struct pci_dev *pdev, unsigned char interrupt_pin)
{
    int i;
    for (i = 0; i < NUM_PCIE_IRQS; i++)
    {
        if ((pcie_irqs[i].used == 1) && (pcie_irqs[i].pdev == pdev) &&
		(pcie_irqs[i].interrupt_pin == interrupt_pin))
        {
#ifdef PCIE_DEBUG
            printk("%s: found a irq index %d\r\n", __func__, i);
#endif 
            break;
        }
    }
    if (i == NUM_PCIE_IRQS)
    {   
        printk("%s: Not found!!!\r\n", __func__);
        return -ENOSPC;
    }           
    return PCIE_IRQ_BASE + i;
}

void g2_pcie_msi_clear(int rc_number)
{
	unsigned long status;
	unsigned int reg_offset;
	struct pcie_port *pp=&pcie_port[rc_number];

	//Write "one" to MSI interrupt vector (which raised the interrupt) of 
	//MSI Controller Interrupt Status Register (Offset: 0x700 + 0x130)
	reg_offset = pp->base_addr + PCIE_PORT_MSI_CONTROLLER_INT0_STATUS;
	status = readl(reg_offset);
	writel(status, reg_offset);

	//Write "one" to msi_ctrl_int bit of PCIE_INTERRUPT_1/ PCIE_INTERRUPT_0 register (PCIE Global register)
	reg_offset = PCIE_GLBL_INTERRUPT_0 + rc_number * PCIE_GLBL_offset;
	writel(MSI_CTR_INT, reg_offset);

	//debug_Aaron	move to irq handler for both of MSI and INTx
	//Pass MSI message to PCIe device handler.
	//generic_handle_irq(PCIE_IRQ_BASE);
}

/*
 * Dynamic irq allocate and deallocation
 */
static int pcie_create_irq(struct pci_dev *pdev)
{
	int irq;
	//struct pcie_port *pp = bus_to_port(pdev->bus->number);

	//debug_Aaron, pdev is the pci device structure of device not RC
	irq = pcie_find_irq_by_pdev(pdev);
	if (irq < 0)
	{
		printk("%s: Can not found an irq for bus=%d\r\n", __func__, pdev->bus->number);
		return -ENOSPC;
	}
	dynamic_irq_init(irq);
	return irq;
}

static void pcie_destroy_irq(unsigned int irq)
{
 //debug_Aaron on 03/15/2011 no need to destory irq, because
        // the map_irq is only called when probe not arch_setup_msi_irq
#if 0
	int index = irq - PCIE_IRQ_BASE;

	dynamic_irq_cleanup(irq);
	pcie_irqs[index].used = 0;	
	pcie_irqs[index].rc_bus_number = 0;
	pcie_irqs[index].pdev = NULL;	
#endif
	dynamic_irq_cleanup(irq);
}

//debug_Aaron for INTx 
static void g2_intx_nop(struct irq_data *irqd)
{
        return;
}

static void mask_intx_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	struct pcie_irq_resource *irqs = &pcie_irqs[irq - PCIE_IRQ_BASE];	
	struct pcie_port *pp = bus_to_port(irqs->rc_bus_number); 

	//debug_Aaron on 04/20/2011, for xHCI USB 3.0 card, usb_add_hcd() will call unmask/mask_intx_irq()
	int temp;
	temp = readl(PCIE_GLBL_INTERRUPT_ENABLE_0 + (pp->rc_number * PCIE_GLBL_offset));

//#ifdef PCIE_DEBUG
        printk("%s: irq=%d, bus_number=%d\r\n", __func__, irq, irqs->rc_bus_number);
//#endif

	//debug_Aaron on 04/20/2011, for xHCI USB 3.0 card, usb_add_hcd() will call unmask/mask_intx_irq()
	 //Disable Pcie Sata Pcie Glbl Interrupt Enable 0 Register, 0xf00a0004 and 0xf00a0404
        //writel(0x0, PCIE_GLBL_INTERRUPT_ENABLE_0 + (pp->rc_number * PCIE_GLBL_offset));
	temp &= ~(INTD_ASSERT_EN | INTC_ASSERT_EN | INTB_ASSERT_EN | INTA_ASSERT_EN);
        writel(temp, PCIE_GLBL_INTERRUPT_ENABLE_0 + (pp->rc_number * PCIE_GLBL_offset));
}

static void unmask_intx_irq(struct irq_data *irqd)
{
	unsigned irq = irqd->irq;
	struct pcie_irq_resource *irqs = &pcie_irqs[irq - PCIE_IRQ_BASE];
        struct pcie_port *pp = bus_to_port(irqs->rc_bus_number);

	//debug_Aaron on 04/20/2011, for xHCI USB 3.0 card, usb_add_hcd() will call unmask/mask_intx_irq()
        int temp;
        temp = readl(PCIE_GLBL_INTERRUPT_ENABLE_0 + (pp->rc_number * PCIE_GLBL_offset));

//#ifdef PCIE_DEBUG
	printk("%s: irq=%d, bus_number=%d\r\n", __func__, irq, irqs->rc_bus_number);
//#endif	
	 //Enable Pcie Sata Pcie Glbl Interrupt Enable 0 Register, 0xf00a0004 and 0xf00a0404
	//debug_Aaron on 04/20/2011, for xHCI USB 3.0 card, usb_add_hcd() will call unmask/mask_intx_irq()
        //writel(MSI_CTR_INT_EN | INTD_ASSERT_EN | INTC_ASSERT_EN | INTB_ASSERT_EN | INTA_ASSERT_EN,
        //                PCIE_GLBL_INTERRUPT_ENABLE_0 + (pp->rc_number * PCIE_GLBL_offset));
	temp |= (INTD_ASSERT_EN | INTC_ASSERT_EN | INTB_ASSERT_EN | INTA_ASSERT_EN);
        writel(temp, PCIE_GLBL_INTERRUPT_ENABLE_0 + (pp->rc_number * PCIE_GLBL_offset));
}


static struct irq_chip g2_intx_chip = {
        .name = "G2-PCI-INTX",
        .irq_ack = g2_intx_nop,
        .irq_enable = unmask_intx_irq,
        .irq_disable = mask_intx_irq,
        .irq_mask = mask_intx_irq,
        .irq_unmask = unmask_intx_irq,
};


void arch_teardown_msi_irq(unsigned int irq)
{
	pcie_destroy_irq(irq);
}

static void g2_msi_nop(struct irq_data *irqd)
{
	return;
}

static struct irq_chip g2_msi_chip = {
	.name = "G2-PCI-MSI",
	.irq_ack = g2_msi_nop,
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int irq;
	struct msi_msg msg;
	struct pcie_port *pp;
	struct pci_sys_data *sys = pdev->sysdata;

//debug_Aaron
#ifdef PCIE_DEBUG
	printk("%s: pdev->bus->number=%d\r\n", __func__, pdev->bus->number);
    printk("%s: sys->busnr=%d\r\n", __func__, sys->busnr);
#endif

	pp = bus_to_port(sys->busnr);
	if (pp == NULL)
	{
		printk("%s: No RC found at bus=%d\r\n", __func__, sys->busnr);
		return -ENOSPC;
	}

	irq = pcie_create_irq(pdev);

#ifdef PCIE_DEBUG
	/* debug_Aaron on 2011/07/03, use the hardware reserved address */
        /* printk("%s: irq=%d, __pa(msi_buf_addr)=0x%X\r\n", __func__, irq, (unsigned int)__pa(msi_buf_addr)); */
#endif

	if (irq < 0 )
	{
		return -ENOSPC;
	}

	irq_set_msi_desc(irq, desc);

	/* debug_Aaron on 2011/07/03, use the hardware reserved address */
  	/* msg.address_hi = 0x0; */
	/* msg.address_lo = (unsigned `int)__pa(msi_buf_addr) + pp->rc_number * MSI_BUF_OFFSET;  */
	/* msg.address_hi = PCIE_SATA_PCIE_GLBL_MSI_TARGET_ADDRESS_63_32_MSB +  (pp->rc_number * PCIE_GLBL_offset); */
	msg.address_hi = 0x0;
	msg.address_lo = PCIE_SATA_PCIE_GLBL_MSI_TARGET_ADDRESS_31_0_LSB  + (pp->rc_number * PCIE_GLBL_offset);

  	msg.data = MSI_VECTOR;
	write_msi_msg(irq, &msg);

	irq_set_chip_and_handler(irq, &g2_msi_chip, handle_simple_irq);
	set_irq_flags(irq, IRQF_VALID);

	return 0;
}

static int g2_pcie_link_up(int rc_number)
{
 	int temp;
	int try=10000;

#ifdef PCIE_DEBUG
	printk("%s: check rc_number=%d, link status\r\n", __func__, rc_number);
#endif
	while(try--)
	{	
#ifdef PCIE_DEBUG
		printk(".");
#endif
 		//read link status for each pcie status.
		//need add some dummy read cycle to wait for link stable
        	temp = readl(PCIE_GLBL_INTERRUPT_0 + (rc_number * PCIE_GLBL_offset));
 		temp &= Xmlh_link_up;

		if (temp)
			break;

		udelay(100);
		schedule();
	}

#ifdef PCIE_DEBUG
	printk("\r\n");
#endif

	if (temp == 0)
	{	
//#ifdef PCIE_DEBUG
		printk("%s: rc number %d link down!!!\r\n", __func__, rc_number);
//#endif		
		return 0;   /* link down */
	}

//#ifdef PCIE_DEBUG
        printk("%s: rc number %d link up!!!\r\n", __func__, rc_number);
//#endif
	return 1;		/* link up */
}

void g2_pcie_addr_translation_init_CF(struct pcie_port *pp, struct pci_bus *bus, u32 devfn, int where)
{ 
	// Need Init Read/Write Config
 	u32 Lower_Traget_address;
	//struct pcie_port *pp = bus_to_port(bus->parent->number);
	unsigned int base_addr;
	int rc_number;

//printk("%s: bus->self->hdr_type=0x%x, bus->number=%d\r\n", __func__, bus->self->hdr_type, bus->number);
#if 0
	if (pp == NULL)
	{
		printk("%s: RC can not be found at bus=%d\r\n", __func__, bus->number);
		return;
	}
#endif

#ifdef PCIE_DEBUG
        printk("%s: pp=0x%x\r\n", __func__, pp);
#endif

	base_addr = pp->base_addr;
	rc_number = pp->rc_number;

#ifdef PCIE_DEBUG
        printk("%s: pp->rc_number = %d, base_addr=0x%x, header type=0x%x, bus->number=%d\r\n", 
		__func__, pp->rc_number, pp->base_addr, bus->self->hdr_type, bus->number);
#endif


 	//1. Setup the Viewport Register, offset 0x200, region 0 : inbound (read), region 1 : outbound (write) 
 	//   write 0x00000001 for outbound (0x00000000 for inbound) to PCIE_BASE_ADDR + 0x200
   	writel(REGION_INDEX_0|OUTBOUND, base_addr + PCIE_PORT_VIEWPORT);


	//2. Setup the Region Base and Limit Address Registers (H/W need give us the total region address)
 	//   write 0xd0000000 to Lower Base Address, offset 0x20c
   	writel(Lower_Base_Address_CFG_Region0 + (rc_number * GOLDENGATE_PCI_MEM_SIZE), base_addr + PCIE_PORT_LOWER_BASE);

 	//   write 0x80000000 to Upper Base Address, offset 0x210
   	writel(Upper_Base_Address_CFG_Region0, base_addr + PCIE_PORT_UPPER_BASE);  

 	//   write 0xd000ffff to Limit Address, offset 0x214
   	writel(Limit_CFG_region0 + (rc_number * GOLDENGATE_PCI_MEM_SIZE), base_addr + PCIE_PORT_LIMIT_BASE);

 	//3. Setup Target Address Registers 
 	//   write 0x00010000 to Lower Target Address, offset 0x218
 	//   Register Number (bits 7:2), Extend Register Number (bits 11:8), Function Number (bits 18:16),
 	//   Device Number (bits 23:19), Bus Number (bits 31:24)
 	//(PCI_SLOT(devfn) << 11) | (PCI_FUNC(devfn) << 8)
   	Lower_Traget_address = ((bus->number)<<24) | (PCI_FUNC(devfn))<<16 | PCI_SLOT(devfn) << 19;

 	//   Lower_Traget_address = 0xffff0000;
   	writel(Lower_Traget_address, base_addr + PCIE_PORT_LOWER_TARGET); 

 	//   write 0x00000000 to Upper Target Address, offset 0x21c
   	writel(Upper_Target_address, base_addr + PCIE_PORT_UPPER_TARGET);

 	//   Register Number (bits 7:2), Extend Register Number (bits 11:8), Function Number (bits 18:16),
 	//   Device Number (bits 23:19), Bus Number (bits 31:24)
 	//4. Configure the region via the Region Control 1 Register (0 : Memory, 2 : I/O, 4 : type 0 config, 5 : type 1 config)
 	//   write 0x00000004 to Region Control 1, offset 0x204
	pp = bus_to_port(bus->parent->number);
	if (pp != NULL)
   		writel(TYPE_CF_0, base_addr + PCIE_PORT_REGION_CONTROL1); 
	else
	{
		if (bus->number == 0)
		{
   			writel(TYPE_CF_0, base_addr + PCIE_PORT_REGION_CONTROL1); 
		}	
		else
		{
#ifdef PCIE_DEBUG
			printk("%s: bus->parent->number=%d, bus->parent->self->hdr_type=%d\r\n", __func__, 
				bus->parent->number,  bus->parent->self->hdr_type);
#endif
			if (bus->parent->self->hdr_type == 1)
   				writel(TYPE_CF_1, base_addr + PCIE_PORT_REGION_CONTROL1); 
			else
   				writel(TYPE_CF_0, base_addr + PCIE_PORT_REGION_CONTROL1); 
		}
	}

	
 	//5. Enable the region
 	//   write 0x80000000 to Region Control 2, offset 0x208
   	writel(ENABLE_REGION, base_addr + PCIE_PORT_REGION_CONTROL2);  
}


//Stone add.
void g2_pcie_phy_init(int rc_number)
{
 	unsigned int temp;
	unsigned int reg_offset;

 	//PHY clock need to update (SATA and PCIe used the same IP).
 	//1. app_ltssm_enable set 0
 	//1. cfg_pcie_sata set 1 (PIPE mode -> 1, SAPIS mode -> 0)
 	temp = readl(GLOBAL_GLOBAL_CONFIG);

#ifdef PCIE_DEBUG
    printk("%s: read GLOBAL_GLOBAL_CONFIG=0x%x\n", __func__, temp);
#endif

	if (rc_number == 0)
		temp |= PCIE_GLBL_CORE_CONFIG_REG_cfg_pcie0_clken; 
	else if (rc_number == 1)
	    temp |= PCIE_GLBL_CORE_CONFIG_REG_cfg_pcie1_clken;
#ifndef CONFIG_CORTINA_FPGA
 	else if (rc_number == 2)
            temp |= PCIE_GLBL_CORE_CONFIG_REG_cfg_pcie2_clken;
#endif
	else
	{
		printk("%s: rc_number %d out of range!!!\r\n", __func__, rc_number);
		return;
	}

#ifdef PCIE_DEBUG
    printk("%s: write GLOBAL_GLOBAL_CONFIG=0x%x\n", __func__, temp);
#endif
 	writel(temp, GLOBAL_GLOBAL_CONFIG);

 	//2. app_ltssm_enable set 1
	reg_offset = PCIE_SATA_PCIE_GLBL_CORE_CONFIG_REG + rc_number * PCIE_GLBL_offset;
 	temp = readl(reg_offset);

#ifdef PCIE_DEBUG
    printk("%s: read PCIE_SATA_PCIE_GLBL_CORE_CONFIG_REG=0x%x, value=0x%x\n", __func__, reg_offset, temp);
#endif

	 //debug_Aaron on 11/26/2010 for "UR" do not cause bus error
 	temp |= PCIE_GLBL_CORE_CONFIG_REG_app_ltssm_enable | PCIE_GLBLCORE_CONFIG_REG_axi_ur_err_mask;

#ifdef PCIE_DEBUG
    printk("%s: write PCIE_SATA_PCIE_GLBL_CORE_CONFIG_REG=0x%x, value=0x%d\n", __func__, reg_offset, temp);
#endif
 	writel(temp, reg_offset);
}

#define PCIE_ALIGN_SIZE	0x100000
#define PCIE_IO_SIZE	0x100000
static int __init g2_pcie_setup(int nr, struct pci_sys_data *sys)
{
   	struct pcie_port *pp;

//#ifdef PCIE_DEBUG
  	printk("%s: NUM_PCIE_PORTS=%d, num_port_sb_phy=%d, nr=%d, sys->busnr=%d\n", __func__, NUM_PCIE_PORTS, num_port_sb_phy, nr, sys->busnr);
//#endif

	pp = &pcie_port[nr];
	pp->root_bus_nr = sys->busnr;
	pp->rc_number = nr;
	memset(&(pp->devid[0]), 0xff, sizeof(pp->devid));
	memset(&(pp->dev_cfg[0]), 0x00, sizeof(pp->dev_cfg));

	//debug_Aaron for power saving
	pp->pdev = NULL;;

	/*
	 * Generic PCIe unit setup.
	 */
  
	/*
	 * IORESOURCE_IO
	 */
	memset(pp->io_space_name, 0, sizeof(pp->io_space_name));
	snprintf(pp->io_space_name, sizeof(pp->io_space_name),
		 "PCIe %d BASE address", pp->rc_number);
	//pp->io_space_name[sizeof(pp->io_space_name) - 1] = 0;

	pp->res[0].name = pp->io_space_name;
	pp->res[0].start = GOLDENGATE_PCIE0_BASE + pp->rc_number * GOLDENGATE_PCI_MEM_SIZE + PCIE_ALIGN_SIZE;
	pp->res[0].end = pp->res[0].start + PCIE_IO_SIZE - 1;
	pp->res[0].flags = IORESOURCE_IO;
	pci_add_resource(&sys->resources, &pp->res[0]);

	/*
	 * IORESOURCE_MEM
	 */
	memset(pp->mem_space_name, 0, sizeof(pp->mem_space_name));
	snprintf(pp->mem_space_name, sizeof(pp->mem_space_name),
		 "PCIe %d MEM", pp->rc_number);
	//pp->mem_space_name[sizeof(pp->mem_space_name) - 1] = 0;

	pp->res[1].name = pp->mem_space_name;
	pp->res[1].start = pp->res[0].end + PCIE_ALIGN_SIZE;
	pp->res[1].end = pp->res[0].start + GOLDENGATE_PCI_MEM_SIZE - PCIE_ALIGN_SIZE  - 1;
	pp->res[1].flags = IORESOURCE_MEM;
	pci_add_resource(&sys->resources, &pp->res[1]);

#ifdef PCIE_DEBUG
	printk("%s: MEM.name=%s, MEM.start=0x%x, MEM.end=0x%x, MEM.flags=0x%lx\r\n", 
           __func__, pp->res[1].name, pp->res[1].start, pp->res[1].end, pp->res[1].flags);
#endif

#ifdef PCIE_DEBUG
	printk("%s: IO.name=%s, IO.start=0x%x, IO.end=0x%x, IO.flags=0x%lx\r\n", 
           __func__, pp->res[0].name, pp->res[0].start, pp->res[0].end, pp->res[0].flags);
#endif

	return 1;
}

static struct pcie_port *bus_to_port(unsigned char bus)
{
	int i;

	/* use number of sb_phy in stead of a constant */
	/* for (i = NUM_PCIE_PORTS - 1; i >= 0; i--) { */
	for (i = num_port_sb_phy - 1; i >= 0; i--) {
		int rbus = pcie_port[i].root_bus_nr;

#ifdef PCIE_DEBUG
	printk("%s: rbus=%d\r\n", __func__, rbus);
#endif
		if (rbus != 0xff && rbus == bus)
			break;
	}

#ifdef PCIE_DEBUG
	printk("%s: bus=%d, i=%d\r\n", __func__, bus, i);
#endif
	return (i >= 0) ? &pcie_port[i] : NULL;
}

static int g2_pcie_valid_config(int bus, int dev)
{
	//debug_Aaron, check where the request come from, device or RC 
    struct pcie_port *pp = bus_to_port(bus);
    if (pp != NULL)  /* comes from device let it go */
	{
		if (dev == 0) /* root complex dev should be 0 */
			return 1;
		return 0;
	}

	//debug_Aaron the bus number should depend on ????
    if ((bus <= 255) && (dev <= 31)) /* pcie device */
    //if ((bus <= 2) && (dev <= 2)) /* pcie device */
		return 1;

    return 0; 		
}

static DEFINE_SPINLOCK(g2_pcie_lock);

static int g2_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where, int size, u32 *val)
{
	unsigned long flags;
	struct pcie_port *pp;
	unsigned int base_addr;
	unsigned int cnf_base_addr;


    //Check G2 pcie dev number 
	if (g2_pcie_valid_config(bus->number, PCI_SLOT(devfn)) == 0) {

#ifdef PCIE_DEBUG
        printk("%s: Device not found at devfn=%d, where=%d, bus->number=%d!!!\n", __func__, devfn, where, bus->number);
#endif

		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

#ifdef CONFIG_CS752X_PROC
	if (cs_pcie_debug & PCIE_DEBUG_CONF)
	{
		printk("%s: bus->number=%d, where=0x%x, size=%d\r\n", __func__, bus->number, where, size);
	}	
#endif

 //debug_Aaron, check where the request come from, device or RC ?
    pp = bus_to_port(bus->number);

	if (pp != NULL)
	{
       if (!pp->link_up)
        {
#ifdef PCIE_DEBUG
        	printk("%s: Device not found at devfn=%d, where=%d, bus->number=%d!!!\n", __func__, devfn, where, bus->number);
#endif
            *val = 0xffffffff;
            return PCIBIOS_DEVICE_NOT_FOUND;
        }
	}

	if (PCIe_bridge_flag == 1)
	{
   		//If read the register from Root Complex then read driectly
   		if(pp != NULL) //comes from RCs
   		{
			base_addr = pp->base_addr;
#ifdef PCIE_DEBUG
			printk("%s: base_addr=0x%x\r\n", __func__, base_addr);
#endif


			//debug_Aaron on 2011/08/03, fix BUG#29450: probing a pci device with udevadm segfaults
            //because our RC does not allow the access some of configuration space
            if (where >= 512)
			{
				*val = 0;
				return PCIBIOS_SUCCESSFUL;
			}

			spin_lock_irqsave(&g2_pcie_lock, flags);

    		switch (size)
      		{
       			case 1:
              		*val = readb(base_addr + where);
              		break;              
       			case 2: 
               		*val = readw(base_addr + where);
               		break;
       			case 4:
			//debug_Aaron, for new FPGA image it should return the correct value
			//fix BUG#23703
#if 0
               		if (where == 8)
                		*val = 0x06040000;       /* CLASS code=0x6, sub-CLASS code=0x4 */
                	else   
#endif
               			*val = readl(base_addr + where);
               		break;
       			default:
               		*val = 0;  
               		break;                       
      		}		
     		spin_unlock_irqrestore(&g2_pcie_lock, flags); 

#ifdef PCIE_DEBUG
	if (bus->number == 3)
    printk("%s: read from RC, val=0x%x, devfn=%d, where=%d, bus->number=%d\n", __func__, *val, devfn, where, bus->number);
#endif
     		return PCIBIOS_SUCCESSFUL; 
    	}
	}	
    

	//debug_Aaron, get the pp for device on RC
#ifdef PCIE_DEBUG
	printk("%s: bus->parent->number=%d\r\n", __func__, bus->parent->number);	
#endif

	pp = pcie_find_RC_by_bus(bus);	
	if (pp == NULL)
	{
#ifdef PCIE_DEBUG
        printk("%s: can not forn RC from bus->number=%d!!!\n", __func__, bus->number);
#endif     
		*val = 0xffffffff; 
        return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (!pp->link_up)
    {
#ifdef PCIE_DEBUG
        printk("%s: RC %d is link down!!!\n", __func__, pp->rc_number);
#endif
        *val = 0xffffffff;
        return PCIBIOS_DEVICE_NOT_FOUND;
    }

#ifdef PCIE_DEBUG
	printk("%s: bus->number=%d, bus->primary=%d, bus->self->device=%d\r\n", __func__, bus->number, bus->primary, bus->self->device);
	printk("%s: pp->rc_number=%d\r\n", __func__, pp->rc_number);
#endif

	spin_lock_irqsave(&g2_pcie_lock, flags);
	//Stone add. Used Internl Address Translation Unit (iATU) to read pcie device configuration
    g2_pcie_addr_translation_init_CF(pp, bus, devfn, where); 


	cnf_base_addr = pp->cnf_base_addr;
    switch (size)
    {
    	case 1:
        	*val = readb(cnf_base_addr + where);
            break;              
        case 2: 
            *val = readw(cnf_base_addr + where);
            break;
        case 4:
            *val = readl(cnf_base_addr + where);
            break;
        default:
            *val = 0;  
            break;                       
    }
    spin_unlock_irqrestore(&g2_pcie_lock, flags);


#ifdef PCIE_DEBUG
//	if (where == 0 || where == 0x10 || where == 0x14 || where == 0x18 || where == 0x20 || where == 0x24)
	if (where == 0x0c || where == 0x30 || where == 0x3c)	
	{
		printk("%s: read from RC, val=0x%x, devfn=%d, where=%d, bus->number=%d\n", __func__, *val, devfn, where, bus->number);
#ifdef CONFIG_CS752X_PROC
		if (cs_pcie_debug & PCIE_DEBUG_CONF)
           	g2_pcie_read_err_counts(pp->rc_number);
#endif

	}
#endif


	//debug_Aaron on 11/01/2010, to avoid return duplicate vendor ID and device ID to kernel, check whether the
    //                           same vendor ID/device ID pair has been reported or not
	if (in_scanning_bus == 1)
	{
		if (where == PCI_VENDOR_ID)
		{
			u32 val1;
			int isMultiFuncDevice = 0;
			int ret;	
			val1 = 0;

			if (*val == 0xffffffff)
   			{
#ifdef PCIE_DEBUG
        		printk("%s: VendorID/DeviceID is 0xffffffff!!!, bus->number=%d, devfn=0x%x\n", __func__, bus->number, devfn);
#endif
        		*val = 0xffffffff;
        		return PCIBIOS_DEVICE_NOT_FOUND;
    		}
	
			val1 = readb(cnf_base_addr + PCI_HEADER_TYPE);
#ifdef PCIE_DEBUG
            printk("%s: bsu number=%d, PCI Configuration space PCI_HEADER_TYPE=0x%x\r\n", __func__, bus->number, val1);
#endif
			if (val1 == 1)  /* a bridge */
				val1 = 0xffffffff;
			else if (val1 & 0x80)  /* a multi function device */
			{
				val1 = 0x0; 
				isMultiFuncDevice = 1;
			}
			else
				val1 = readl(cnf_base_addr + PCI_SUBSYSTEM_VENDOR_ID); /* read sub-System ID and sub-Vendor ID */

#ifdef PCIE_DEBUG
            printk("%s: bsu number=%d, devfn=0x%x, PCI_SUBSYSTEM_VENDOR_ID=0x%x\r\n", __func__, bus->number, devfn, val1);
#endif

			ret = pcie_find_vendor_device(pp, *val, val1, bus->number, isMultiFuncDevice);
			if (ret == 1)/* already report to kernel */
			{	
				*val = 0xffffffff;
        		return PCIBIOS_DEVICE_NOT_FOUND;			
			}
			if (ret == 0)
				pcie_set_vendor_device(pp, *val, val1, bus->number, isMultiFuncDevice);
		}
	}

	if (where == 0x0c || where == 0x30 || where == 0x3c)	
	{
#ifdef CONFIG_CS752X_PROC
		if (cs_pcie_debug & PCIE_DEBUG_CONF)
      		g2_pcie_read_err_counts(pp->rc_number);
#endif
	}

	return PCIBIOS_SUCCESSFUL;
}

static int g2_pcie_wr_conf(struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{
	unsigned long flags;
	struct pcie_port *pp;
	unsigned int base_addr;
	unsigned int cnf_base_addr;

#ifdef PCIE_DEBUG
 if (bus->number == 2)
    printk("%s: val=0x%x, devfn=%d, where=%d, bus->number=%d, size=%d\n", __func__, val, devfn, where, bus->number, size);
#endif

	if (g2_pcie_valid_config(bus->number, PCI_SLOT(devfn)) == 0)
	{
#ifdef PCI_DEBUG
        	printk("%s: Device not found at devfn=%d, where=%d, bus->number=%d!!!\n", __func__, devfn, where, bus->number);
#endif
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

#ifdef CONFIG_CS752X_PROC
        if (cs_pcie_debug & PCIE_DEBUG_CONF)
        {
                printk("%s: bus->number=%d, where=0x%x, size=%d\r\n", __func__, bus->number, where, size);
        }
#endif


    //debug_Aaron, check where the request come from, device or RC ?
    pp = bus_to_port(bus->number);

	if (PCIe_bridge_flag == 1)
	{
		/* if we can find from port array it is a RC */
		if (pp != NULL)
   		{
			g2_pcie_save_rc_cfg(pp, bus, devfn, where, size, val);
			base_addr = pp->base_addr;
#ifdef PCIE_DEBUG
	printk("%s: base_addr=0x%x\r\n", __func__, base_addr);
#endif
			spin_lock_irqsave(&g2_pcie_lock, flags);
    		switch (size)
      		{	
       			case 1:
              		writeb(val, base_addr + where);
              		break;              
       			case 2: 
               		writew(val, base_addr + where);
               		break;
       			case 4:
                 	writel(val, base_addr + where);
               	break;
       			default:
               		break;                       
      		}
     		spin_unlock_irqrestore(&g2_pcie_lock, flags);
#ifdef PCIE_DEBUG
   if (where == 0x18)
        {
     printk("%s: Written to device, RC=%d\n", __func__, pp->rc_number);
         printk("%s: val=0x%x, devfn=%d, where=%d, bus->number=%d\n", __func__, val, devfn, where, bus->number);
        }

#endif
     		return PCIBIOS_SUCCESSFUL; 
    	}
	}
 
	//debug_Aaron, get the pp for device on RC
	pp = pcie_find_RC_by_bus(bus);
    if (pp == NULL)
    {
#ifdef PCIE_DEBUG
        printk("%s: can not forn RC from bus->number=%d!!!\n", __func__, bus->number);
#endif
        return PCIBIOS_DEVICE_NOT_FOUND;
    }
            
	if (!pp->link_up)
    {
#ifdef PCIE_DEBUG
        printk("%s: RC %d is link down!!!\n", __func__, pp->rc_number);
#endif
        return PCIBIOS_DEVICE_NOT_FOUND;
    }

	g2_pcie_save_dev_cfg(pp, bus, devfn, where, size, val);
	
	cnf_base_addr = pp->cnf_base_addr;

	spin_lock_irqsave(&g2_pcie_lock, flags);
    //Stone add. Used Internl Address Translation Unit (iATU) to write pcie device configuration
	g2_pcie_addr_translation_init_CF(pp, bus, devfn, where);

	//debug_Aaron for PCIe to PCI USB bridge, turn on Bus Master, Memory Space and IO Space
	if (where == PCI_COMMAND)
	{
		unsigned char val1;	
		val1 = readb(cnf_base_addr + PCI_HEADER_TYPE);
#ifdef PCIE_DEBUG
                        printk("%s: bsu number=%d, PCI Configuration space PCI_HEADER_TYPE=0x%x\r\n", __func__, bus->number, val1);
#endif
		if ((val1 == 1) && (where == PCI_COMMAND))
		{
			val += (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
		}
	}

 
    switch (size)
    {
    	case 1:
        	writeb(val, cnf_base_addr + where);
            break;              
       	case 2: 
            writew(val, cnf_base_addr + where);
            break;
        case 4:		//   write value to Upper Base Address + where   
            writel(val, cnf_base_addr + where);
            break;
       	default:
            break;                       
     }
  
	 spin_unlock_irqrestore(&g2_pcie_lock, flags);


//#ifdef PCIE_DEBUG
	//if (where == 0x10 || where == 0x14 || where == 0x18 || where == 0x20 || where == 0x24)
 if (bus->number == 1 && where == 0x04)
	{
 //    printk("%s: Written to device, RC=%d\n", __func__, pp->rc_number);
	 printk("%s: val=0x%x, devfn=%d, where=%d, bus->number=%d\n", __func__, val, devfn, where, bus->number);
	}
//#endif      

  	 return PCIBIOS_SUCCESSFUL;
}

static int g2_pcie_save_dev_cfg(struct pcie_port *pp, struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{
	unsigned int	i;
	
	for (i=0; i<MAX_CFG_REGS; i++) {
		if (pp->dev_cfg[i].valid == 0) {
			pp->dev_cfg[i].bus = bus;
			pp->dev_cfg[i].devfn = devfn;
			pp->dev_cfg[i].where = where;
			pp->dev_cfg[i].size = size;
			pp->dev_cfg[i].val = val;
			pp->dev_cfg[i].valid = 1;
			break;
		} else {
			if ( pp->dev_cfg[i].where == where ) {
				pp->dev_cfg[i].val = val;
				break;
			}
		}
	}
	
	if (i >= MAX_CFG_REGS)
		return 1;
	else
		return 0;	
}

static int g2_pcie_restore_dev_cfg(int	id)
{
	unsigned int	i;
	
	for (i=0; i<MAX_CFG_REGS; i++) {
		if (pcie_port[id].dev_cfg[i].valid == 1) {
			g2_pcie_wr_conf(pcie_port[id].dev_cfg[i].bus, 
					pcie_port[id].dev_cfg[i].devfn,
					pcie_port[id].dev_cfg[i].where, 
					pcie_port[id].dev_cfg[i].size,
					pcie_port[id].dev_cfg[i].val);
		}
		else {
			break;
		}
	}
	return 0;	
}

static int g2_pcie_save_rc_cfg(struct pcie_port *pp, struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{
	unsigned int	i;
	
	for (i=0; i<MAX_CFG_REGS; i++) {
		if (pp->rc_cfg[i].valid == 0) {
			pp->rc_cfg[i].bus = bus;
			pp->rc_cfg[i].devfn = devfn;
			pp->rc_cfg[i].where = where;
			pp->rc_cfg[i].size = size;
			pp->rc_cfg[i].val = val;
			pp->rc_cfg[i].valid = 1;
			break;
		} else {
			if ( pp->rc_cfg[i].where == where ) {
				pp->rc_cfg[i].val = val;
				break;
			}
		}
	}
	
	if (i >= MAX_CFG_REGS)
		return 1;
	else
		return 0;	
}

static int g2_pcie_restore_rc_cfg(int	id)
{
	unsigned int	i;
	
	for (i=0; i<MAX_CFG_REGS; i++) {
		if (pcie_port[id].rc_cfg[i].valid == 1) {
			g2_pcie_wr_conf(pcie_port[id].rc_cfg[i].bus, 
					pcie_port[id].rc_cfg[i].devfn,
					pcie_port[id].rc_cfg[i].where, 
					pcie_port[id].rc_cfg[i].size,
					pcie_port[id].rc_cfg[i].val);
		}
		else {
			break;
		}
	}
	return 0;	
}

int g2_pcie_dump_dev_cfg(int id)
{
	unsigned int	i,val;
	
	printk("\n\n\n\n Dump RC CFG registers..........\n\n\n");
	for (i=0; i<MAX_CFG_REGS; i++) {
		g2_pcie_rd_conf(pcie_port[id].rc_cfg[0].bus, 
				0,
				i*4, 
				4,
				&val);
		printk("======> where = 0x%x value = 0x%x......\n",i*4, val);
	}

	printk("\n\n\n\n Dump DEVICE CFG registers..........\n\n\n");
	for (i=0; i<MAX_CFG_REGS; i++) {
		g2_pcie_rd_conf(pcie_port[id].dev_cfg[0].bus, 
				0,
				i*4, 
				4,
				&val);
		printk("======> where = 0x%x value = 0x%x......\n",i*4, val);
	}
	return 0;
}
EXPORT_SYMBOL(g2_pcie_dump_dev_cfg);

static struct pci_ops g2_pcie_ops = {
	.read = g2_pcie_rd_conf,
	.write = g2_pcie_wr_conf,
};



void g2_pcie_register_init(int rc_number)
{
	unsigned int	i;
	u32 value;

	//debug_Aaron
#ifdef PCIE_DEBUG
	printk("%s: rc_number=%d\r\n", __func__, rc_number);
	value = readl(PCIE_SATA_PCIE_GLBL_AXI_SLAVE_RESP_ERR_MAP);
	value |= 1;
	writel(value, PCIE_SATA_PCIE_GLBL_AXI_SLAVE_RESP_ERR_MAP);
	value = readl(PCIE_SATA_PCIE_GLBL_AXI_SLAVE_RESP_ERR_MAP);
	printk("%s: PCIE_SATA_PCIE_GLBL_AXI_SLAVE_RESP_ERR_MAP=0x%x\r\n", __func__, value);
#endif

	//Maybe not need init in driver ....  
	//1. Setup PCIe Configuration start address (write PCIE_SATA_PCIE_GLBL_PCIE_CONTR_CFG_START_ADDR)
	writel(GOLDENGATE_PCIE0_BASE + rc_number * GOLDENGATE_PCI_MEM_SIZE, PCIE_SATA_PCIE_GLBL_PCIE_CONTR_CFG_START_ADDR + rc_number * PCIE_GLBL_offset);

	//2. Setup PCIe Configuration end address (PCIE_SATA_PCIE_GLBL_PCIE_CONTR_CFG_END_ADDR)
	writel(GOLDENGATE_PCIE0_BASE+0x0FFF+rc_number * GOLDENGATE_PCI_MEM_SIZE, PCIE_SATA_PCIE_GLBL_PCIE_CONTR_CFG_END_ADDR + rc_number * PCIE_GLBL_offset);

	/* debug_Aaron on 2011/07/04 fix BUG#23943 about the racing condition between MPIF and AXI */
//	readl(PCIE_SATA_PCIE_GLBL_PCIE_CONTR_CFG_END_ADDR + rc_number * PCIE_GLBL_offset);
	for (i=0; i<256; i++) {
		value = readl(PCIE_SATA_PCIE_GLBL_PCIE_CONTR_CFG_END_ADDR + rc_number * PCIE_GLBL_offset);
		if (value == GOLDENGATE_PCIE0_BASE+0x0FFF+rc_number * GOLDENGATE_PCI_MEM_SIZE)
			break;
	}
	if (i==256) {
		printk("%s : PCIe write failed in %d accesses.\n",__func__,i);
	}

}


static void g2_pcie_enable_interrupt(int rc_number)
{
	//Enable Pcie Sata Pcie Glbl Interrupt Enable 0 Register, 0xf00a0004 and 0xf00a0404
    writel(MSI_CTR_INT_EN | INTD_ASSERT_EN | INTC_ASSERT_EN | INTB_ASSERT_EN | INTA_ASSERT_EN,
            PCIE_GLBL_INTERRUPT_ENABLE_0 + (rc_number * PCIE_GLBL_offset));

	 /* debug_Aaron on 2011/07/04 fix BUG#23943 about the racing condition between MPIF and AXI */
	readl(PCIE_GLBL_INTERRUPT_ENABLE_0 + (rc_number * PCIE_GLBL_offset));
}

static void g2_pcie_disable_interrupt(int rc_number)
{
    //Enable Pcie Sata Pcie Glbl Interrupt Enable 0 Register, 0xf00a0004 and 0xf00a0404
    writel(0, PCIE_GLBL_INTERRUPT_ENABLE_0 + (rc_number * PCIE_GLBL_offset));
}

void g2_pcie_enable(int rc_number)
{
	struct pcie_port *pp = &pcie_port[rc_number];
	unsigned int base_addr;
	
	if (pp == NULL)
	{
		printk("%s: RC %d is not ready !!!\r\n", __func__, rc_number);
		return;
	}

	base_addr = pp->base_addr;
	
	//debug_Aaron
#ifdef PCIE_DEBUG
	printk("%s: rc_number=%d, base_addr=0x%x\r\n", __func__, rc_number, base_addr);
#endif

	//(Offset: `CFG_MSI_CAP + 0x02), (CFG_MSI_CAP =8'h50) locates at PCI Standard Cap. Structures
	writel(MSI_CONTROL_ENABLE, base_addr + MSI_CONTORL);

#ifdef PCIE_DEBUG
	/* debug_Aaron on 2011/07/03, use hardware register in stead */
     /* printk("%s: __pa(msi_buf_addr)=0x%X\r\n", __func__, (unsigned int)__pa(msi_buf_addr)); */
#endif
	//Setup MSI Controller Address (Port Logic Offset: 0x700+0x120), (31:0)
	/* debug_Aaron on 2011/07/03, use hardware register in stead */
	/* writel((unsigned int)__pa(msi_buf_addr) + (rc_number * MSI_BUF_OFFSET), base_addr + PCIE_PORT_MSI_CONTROLLER_ADDRESS); */
	writel(PCIE_SATA_PCIE_GLBL_MSI_TARGET_ADDRESS_31_0_LSB + (rc_number * PCIE_GLBL_offset), base_addr + PCIE_PORT_MSI_CONTROLLER_ADDRESS); 

	//debug_Aaron, for bridge device should we enable more interrupts ??
	//Enable the MSI interrupt vector by programming MSI Controller Interrupt Enable Register. (Offset: 0x700 + 0x128) 
	writel(PCIE_PORT_MSI_ENABLE_ALL, base_addr + PCIE_PORT_MSI_CONTROLLER_INT0_ENABLE);

	//Enable Pcie Sata Pcie Glbl Interrupt Enable 0 Register, 0xf00a0004 and 0xf00a0404
	//writel(MSI_CTR_INT_EN | INTD_ASSERT_EN | INTC_ASSERT_EN | INTB_ASSERT_EN | INTA_ASSERT_EN,
	//		PCIE_GLBL_INTERRUPT_ENABLE_0 + (rc_number * PCIE_GLBL_offset));
	g2_pcie_enable_interrupt(rc_number);
}

#ifdef CONFIG_CS752X_PROC
static void g2_pcie_read_err_counts(int port_number)
{
        unsigned int err_count;
        unsigned int reg_off;

        err_count = readl(PCIE_SATA_PCIE_GLBL_PHY_RX_10B_8B_DEC_ERR_CNT + port_number * 0x400);
        printk("%s: PCIE_SATA_PCIE_GLBL_PHY_RX_10B_8B_DEC_ERR_CNT=%d\r\n", __func__, err_count);

        err_count = readl(PCIE_SATA_PCIE_GLBL_PHY_RX_DISPARITY_ERR_CNT  + port_number * 0x400);
        printk("%s: PCIE_SATA_PCIE_GLBL_PHY_RX_DISPARITY_ERR_CNT=%d\r\n", __func__, err_count);

        for (reg_off = PCIE_SATA_PCIE_GLBL_RX_RCVD_TS1_ORDER_SET_CNT;
                reg_off <= PCIE_SATA_PCIE_GLBL_RX_RCVD_REQ_ERR_CNT; reg_off += 4)
        {
                err_count = readl(PCIE_SATA_PCIE_GLBL_RX_RCVD_TS1_ORDER_SET_CNT  + port_number * 0x400);
                printk("%s: reg_off=0x%x, count=%d\r\n", __func__, reg_off, err_count);
        }

}
#endif



static irqreturn_t g2_pcie_irq(int irq, void *devid)
{
	unsigned int status;
	unsigned char interrupt_pin;
	int isRC=1;

	struct pci_dev *pdev = devid;
	struct pcie_port *pp = bus_to_port(pdev->bus->number);

	if (pp == NULL)
	{
		pp = &pcie_port[irq - IRQ_PCIE0];
		isRC = 0;
	}

  	//debug_Aaron, disable interrupts to avoid re-entry
	g2_pcie_disable_interrupt(pp->rc_number);

	//debug_Aaron, need to tell which RC own this irq ???
#ifdef PCIE_DEBUG
	printk("%s: irq=%d, pp->rc_number=%d, pdev->bus->number=%d\r\n", __func__, irq, pp->rc_number, pdev->bus->number);
#endif

	//1. Read Global interrupt register.
	status = readl(PCIE_GLBL_INTERRUPT_0 + (irq - IRQ_PCIE0) * PCIE_GLBL_offset);
	writel(status, PCIE_GLBL_INTERRUPT_0 + (irq - IRQ_PCIE0) * PCIE_GLBL_offset);

#ifdef PCIE_DEBUG
	printk("%s: status=0x%x, pdev->bus->number=%d, pdev->devfn=0x%x\r\n", __func__, status, pdev->bus->number, pdev->devfn);
#endif

	if (!(status & (MSI_CTR_INT_EN | INTD_ASSERT_EN | INTC_ASSERT_EN | INTB_ASSERT_EN | INTA_ASSERT_EN)))
	{
		g2_pcie_enable_interrupt(pp->rc_number);

		printk("%s: invlid interrupt status=0x%x!!\r\n", __func__, status);
	 	return IRQ_RETVAL(IRQ_HANDLED);
	}
	//if (!status)
	//	return IRQ_RETVAL(IRQ_HANDLED);


	//debug_Aaron, need to tell which RC comes from
	if (status & MSI_CTR_INT)
	{
		g2_pcie_msi_clear(pp->rc_number);
       	irq = pcie_find_irq_by_rc_bus_number(pp->root_bus_nr);
	}
	else
	{
		if (pp != NULL)
		{
			interrupt_pin=0;
			if (status & INTA_ASSERT_EN)
				interrupt_pin = 1;
			else if (status & INTB_ASSERT_EN)
				interrupt_pin = 2;
			else if (status & INTC_ASSERT_EN)
				interrupt_pin = 3;
			else if (status & INTD_ASSERT_EN)
				//debug_Aaron, for PCIe to PCI bridge USB414N
				//interrupt_pin = 4;
				interrupt_pin = 3;

#ifdef PCIE_DEBUG
			printk("%s: interrupt_pin=%d, pp->root_bus_nr=%d\r\n", __func__, interrupt_pin, pp->root_bus_nr);
#endif
       		irq = pcie_find_irq_by_rc_bus_number_and_interrupt_pin(pp->root_bus_nr, interrupt_pin);
		}
	}

#ifdef PCIE_DEBUG
	if (irq != 86)
        printk("%s: handle over to irq=%d, interrupt_pin=%d\r\n", __func__, irq, interrupt_pin);
#endif
#ifdef CONFIG_CS752X_PROC
	if (cs_pcie_debug & PCIE_DEBUG_IRQ)
	{
        	printk("%s: handle over to irq=%d, interrupt_pin=%d\r\n", __func__, irq, interrupt_pin);
		g2_pcie_read_err_counts(pp->rc_number);
	}
#endif

  	generic_handle_irq(irq);

	//debug_Aaron
        //printk("%s: after handle over to irq=%d, interrupt_pin=%d\r\n", __func__, irq, interrupt_pin);

	//debug_Aaron, enable again
	g2_pcie_enable_interrupt(pp->rc_number);

	return IRQ_RETVAL(IRQ_HANDLED);
}



static struct pci_bus __init *
g2_pcie_scan_bus(int nr, struct pci_sys_data *sys)
{
	struct pci_bus *bus;
	unsigned int temp;
	struct pcie_port *pp;	
	unsigned int base_addr;

	in_scanning_bus = 1;
	bus = pci_scan_root_bus(NULL, sys->busnr, &g2_pcie_ops, sys, &sys->resources);
	in_scanning_bus = 0;

#ifdef PCIE_DEBUG
	printk("%s: nr=%d, bus=%d\n", __func__, nr, bus->number);
#endif

	//debug_Aaron, check where the request come from, device or RC ?
    pp = bus_to_port(bus->number);
    if (pp == NULL)
	{
		printk("%s: bus=%d is not ready yet\r\n", __func__, bus->number);
		return bus;
	}
	base_addr = pp->base_addr;

#ifdef PCIE_DEBUG
    printk("%s: base_addr=0x%x\n", __func__, base_addr);
#endif
			
	//I/O, Memory Space, Bus Master Enable for Root Complex
    writew(0x0007, base_addr + 0x4);
    writel(0x070280, base_addr + 0x71c);
    
    //Add For Debug "remote max request size"
    temp = readb(base_addr + 0x818);
    writeb(0x2, base_addr + 0x818); 

//debug_Aaron on 10/26/2010, modify sky2.c, do not change here
#if 0  
    //For SKY2 bug for DMA size too large (120)
    writew(0x0400, pp->cnf_base_addr + 0xe8);
#endif
    
	return bus;
}

static int g2_pcie_enable_bus_master(int nr)
{
	unsigned int temp;
	
	struct pcie_port 	*pp = &pcie_port[nr];
	unsigned int		base_addr;


	base_addr = pp->base_addr;

#ifdef PCIE_DEBUG
	printk("%s: base_addr=0x%x\n", __func__, base_addr);
#endif
			
	//I/O, Memory Space, Bus Master Enable for Root Complex
	writew(0x0007, base_addr + 0x4);
	writel(0x070280, base_addr + 0x71c);
    
	//Add For Debug "remote max request size"
	temp = readb(base_addr + 0x818);
	writeb(0x2, base_addr + 0x818); 

	return 0;
}

//debug_Aaron, dev carry the pci_dev for device not for RC
static int __init g2_pcie_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	struct pcie_port *pp;
	int g2_pcie_map_no, ret;
	struct pci_sys_data *sys = dev->sysdata;
	int isRC=0;
	struct pcie_irq_resource *irqs;

#ifdef PCIE_DEBUG
	printk("%s: dev->bus->number=%d, slot=%d, pin=%d\n", __func__, dev->bus->number, slot, pin);
    printk("%s: dev->bus->number=%d\r\n", __func__, dev->bus->number);
    printk("%s: sys->busnr=%d\r\n", __func__, sys->busnr);
#endif

  //debug_Aaron, check where the request come from, device or RC ?
        pp = bus_to_port(dev->bus->number);
        if (pp != NULL)
                isRC = 1;
        else
        {
                pp = bus_to_port(sys->busnr); /* sys->busnr is the bus number of RC */
                if (pp == NULL)
                {
                        printk("%s: Unknown device is detected at sys->busnr=%d!!!\r\n", __func__, sys->busnr);
                        return -1;
                }
        }

        //debug_Aaron support both of MSI and INTx, should be init only once
#if 0
        for (i = 0; i < NUM_PCIE_PORTS; i++)
                g2_pcie_enable(i);
        //g2_pcie_INTx_enable(0);
#endif

#ifdef PCIE_DEBUG
        printk("%s: MSI_INTx_Flag=%d\n", __func__, MSI_INTx_Flag);
#endif

        if (isRC)
	{
        	//debug_Aaron there are only 1 irq for 1 RC
		/* debug_Aaron on 2013/03/25 for BUG#39185 */
		char *pname;

		if (pp->rc_number == 0)
			pname = "G2 PCIE0 int";
		else if (pp->rc_number == 1)
			pname = "G2 PCIE1 int";
		else
			pname = "G2 PCIE2 int";
        	ret = request_irq(IRQ_PCIE0 + pp->rc_number, &g2_pcie_irq, IRQF_SHARED, pname, dev);

        	if (ret)
        	{
                	printk(KERN_ERR "%s: PCI: unable to grab PCI 0 error interrupt: %d\n", __func__, ret);
                	return -1;
        	}	
		else
        	{
#ifdef PCIE_DEBUG
        	printk("%s: is RC, irq=%d\n", __func__, IRQ_PCIE0 + pp->rc_number);
#endif
		 //debug_Aaron for power saving
                pp->pdev = dev;

		//debug_Aaron on 2011/08/22, set the initial power state
#ifdef CONFIG_PM
		pci_set_power_state(pp->pdev, PCI_D0);
#endif
                return IRQ_PCIE0 + pp->rc_number;
        	}	
	}

        //Need check PCIE device MSI enable ???
        //check which slot of PCIe
        g2_pcie_map_no = pcie_find_free_irq(dev, sys->busnr);


		//debug_Aaron set irq chip and handler for INTx
		irqs = &pcie_irqs[g2_pcie_map_no - PCIE_IRQ_BASE];
		if (irqs->interrupt_pin >= 1 && irqs->interrupt_pin <= 4)
		{
			irq_set_chip_and_handler(g2_pcie_map_no, &g2_intx_chip, handle_simple_irq);
//#ifdef PCIE_DEBUG
			printk("%s: call set_irq_chip_and_handler(), interrupt_pin=%d\r\n", __func__, irqs->interrupt_pin);	
//#endif
			set_irq_flags(g2_pcie_map_no, IRQF_VALID);
		}

//#ifdef PCIE_DEBUG
                printk("%s: is a EP, irq=%d, sys->busnr=%d\n", __func__, g2_pcie_map_no, sys->busnr);
//#endif
        return g2_pcie_map_no;
}

#ifndef CONFIG_CORTINA_FPGA
static void g2_pcie_reset_device(unsigned int gpio)
{	
	unsigned int gpio_num;
	unsigned int gpio_pin;
	unsigned int gpio_mux;
	unsigned int gpio_out;
	unsigned int gpio_cfg;
	unsigned int gpio_value;
	unsigned int temp;

	gpio_num = gpio / 32;
	gpio_pin = gpio % 32;

	gpio_mux = GLOBAL_GPIO_MUX_0 + gpio_num * 4;
	gpio_out = PER_GPIO0_OUT + gpio_num * 0x20;
	gpio_cfg = PER_GPIO0_CFG + gpio_num * 0x20;
	
	gpio_value = 1 << gpio_pin;	

#ifdef PCIE_DEBUG
	printk("%s: gpio_num=%d, gpio_pin=%d, gpio_value=0x%x, gpio_mux=0x%x, gpio_out=0x%x, gpio_cfg=0x%x, not gpio_value=0x%x\r\n",
		__func__, gpio_num, gpio_pin, gpio_value, gpio_mux, gpio_out, gpio_cfg, ~gpio_value);
#endif

 	/* debug_Aaron on 2011/07/03 for ref. board IO MUX */
        temp = readl(gpio_mux);
        temp |= gpio_value;
        writel(temp, gpio_mux);

        /* per Alan Carr's suggestion write 0 first then wait then write 1 */
        temp = readl(gpio_out);
        temp &= ~gpio_value;
        writel(temp, gpio_out);

        temp = readl(gpio_cfg);
        temp &= ~gpio_value;
        writel(temp, gpio_cfg);

        udelay(100);

        temp = readl(gpio_out);
        temp |= gpio_value;
        writel(temp, gpio_out);

        msleep(10);
        return;
}

static void setup_io_mux(void)
{
	
#ifdef CONFIG_CORTINA_REFERENCE_Q
	g2_pcie_reset_device(GPIO_PCIE0_RESET);
	g2_pcie_reset_device(GPIO_PCIE1_RESET);
#endif	/* CONFIG_CORTINA_REFERENCE_Q */
	g2_pcie_reset_device(GPIO_PCIE_RESET);
	
	return;
}
#endif /* CONFIG_CORTINA_FPGA */

static void __init g2_pcie_preinit(void);
static struct hw_pci g2_pcie __initdata = {
         /* use a variable in staed of a constant value, should use a constant value */
         .nr_controllers = NUM_PCIE_PORTS,
         .preinit        = g2_pcie_preinit,
         .swizzle        = pci_std_swizzle,
        .setup          = g2_pcie_setup,
         .scan           = g2_pcie_scan_bus,
         .map_irq        = g2_pcie_map_irq,
};

static void __init g2_pcie_preinit(void)
{
	int i, ret;
	unsigned int temp;
	unsigned int reg_offset;

#ifdef PCIE_DEBUG
	printk("%s: Enter...\r\n", __func__);
#endif
	//debug_Aaron
	memset(pcie_irqs, 0, sizeof(pcie_irqs));

//debug_Aaron release POR
#ifdef CONFIG_CORTINA_FPGA
         GLOBAL_PHY_CONTROL_t phy_control;

        printk("%s: release power on reset\r\n", __func__);

        /* Release power on reset */
        /* Register: GLOBAL_PHY_CONTROL_phy_# */
        /* Fields to write:
                        por_n_i = 1'b1;
        */
        phy_control.wrd = readl(GLOBAL_PHY_CONTROL);
        phy_control.bf.phy_0_por_n_i = 1;
        phy_control.bf.phy_1_por_n_i = 1;
        writel(phy_control.wrd, GLOBAL_PHY_CONTROL);
        udelay(1000);
#else

     	if ((num_port_sb_phy = sb_phy_init()) < 0)
                return;

	setup_io_mux();
#endif

	/* debug_Aaron on 2012/02/03 update number od controolers by number of SB PHY */
	g2_pcie.nr_controllers = num_port_sb_phy;
 
	 //debug_Aaron support both of MSI and INTx
	/* use variable count in staed of constant */
    /* for (i = 0; i < NUM_PCIE_PORTS; i++) */
    for (i = 0; i < num_port_sb_phy; i++)
	{

#ifdef CONFIG_CS752X_PROC
		if (cs_pcie_debug & PCIE_DEBUG_INIT)
        	g2_pcie_read_err_counts(i);
#endif

  		//Stone add, Init PHY clock steup, should before register init
        g2_pcie_phy_init(i);

#if 0
	  //debug_Aaron, should be called after PHY init
                switch(i)
                {
                        case 0:
                        add_pcie_port(i, GOLDENGATE_PCIE0_BASE_remap, GOLDENGATE_PCIE0_CF_BASE_remap);
                                break;
                        case 1:
                        add_pcie_port(i, GOLDENGATE_PCIE1_BASE_remap, GOLDENGATE_PCIE1_CF_BASE_remap);
                }
#endif


		 //Init PCIe Configuration registers "start address", "end address" and "address offset".
         g2_pcie_register_init(i);

 //debug_Aaron, should be called after PHY init
                switch(i)
                {
                        case 0:
                        	add_pcie_port(i, GOLDENGATE_PCIE0_BASE_remap, GOLDENGATE_PCIE0_CF_BASE_remap);
                            break;
                        case 1:
                        	add_pcie_port(i, GOLDENGATE_PCIE1_BASE_remap, GOLDENGATE_PCIE1_CF_BASE_remap);
							break;
#ifndef CONFIG_CORTINA_FPGA
						case 2:
                        	add_pcie_port(i, GOLDENGATE_PCIE2_BASE_remap, GOLDENGATE_PCIE2_CF_BASE_remap);
							break;
#endif
                }

        g2_pcie_enable(i);

#ifdef CONFIG_CS752X_PROC
	 	if (cs_pcie_debug & PCIE_DEBUG_INIT)
			g2_pcie_read_err_counts(i);
#endif
	}
  
 
}

void g2_pcie_reinit(unsigned int phy_id)
{
	int i, ret;
	unsigned int temp;
	unsigned int reg_offset;

	unsigned int val;

#ifndef CONFIG_PCIE_EXTERNAL_CLOCK 
	sb_phy_program(1, phy_id);
#else
	sb_phy_program(0, phy_id);
#endif

#ifdef CONFIG_CORTINA_REFERENCE_Q
	val = readl(GLOBAL_STRAP);
	val = (val >> 11) & 0x00000003;

	if (val == 0x01) {
		printk("Reset PCIe device #%d.\n",phy_id);
		 	
		switch (phy_id) {
		case 0 :
			g2_pcie_reset_device(GPIO_PCIE0_RESET);
			break;
			
		case 1 :
			g2_pcie_reset_device(GPIO_PCIE1_RESET);
			break;
			
		case 2 :
			g2_pcie_reset_device(GPIO_PCIE_RESET);
			break;
		
		default:
			break;	
		}
	}
#else
	setup_io_mux();
#endif /* CONFIG_CORTINA_REFERENCE_Q */
 
	//Stone add, Init PHY clock steup, should before register init
        g2_pcie_phy_init(phy_id);

	pcie_port[phy_id].link_up = g2_pcie_link_up(phy_id);
	if (pcie_port[phy_id].link_up) {

		//Init PCIe Configuration registers "start address", "end address" and "address offset".
	        g2_pcie_register_init(phy_id);

		g2_pcie_enable(phy_id);
       
		g2_pcie_restore_rc_cfg(phy_id);

		g2_pcie_restore_dev_cfg(phy_id);

		g2_pcie_enable_bus_master(phy_id);
	}
	
	return;
}
EXPORT_SYMBOL(g2_pcie_reinit);

#if 0
u8 g2_pcie_swizzle(struct pci_dev *dev, u8 *pinp)
{
	pci_std_swizzle(dev, pinp);
}
#endif

static int __init g2_pcie_probe(struct platform_device *pdev)
{
	struct resource *res;

	//need update cortina-g2.c pcie resource array.
	//platform_get_resource() and request_resource() need add too.
#ifdef PCIE_DEBUG
	printk("%s: pdev=0x%x\n", __func__, (unsigned int)pdev);
#endif
 
	//Root Complex 0 get resource
	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!res) {
		printk("%s: request PCIE BASE fail!!!\n", __func__);
        return -ENXIO;
    }                         
        
#ifdef PCIE_DEBUG
        printk("%s: RC 0, res->start=0x%x, res->end=0x%x\r\n", __func__, res->start, res->end);
#endif
  
	GOLDENGATE_PCIE0_BASE_remap = (unsigned int)ioremap(res->start, res->end - res->start + 1);        
	res = platform_get_resource(pdev,IORESOURCE_IO,1);
	if (!res) {
        printk("%s: request PCIE IO fail!!!\n", __func__);
        return -ENXIO;
    }

	GOLDENGATE_PCIE0_CF_BASE_remap = (unsigned int)ioremap(res->start, res->end - res->start + 1);

	//Root Complex 1 get resource
    res = platform_get_resource(pdev,IORESOURCE_IO,2);
    if (!res) {
        printk("%s: request PCIE BASE fail!!!\n", __func__);
        return -ENXIO;
    }

#ifdef PCIE_DEBUG
	printk("%s: RC 1, res->start=0x%x, res->end=0x%x\r\n", __func__, res->start, res->end);
#endif


    GOLDENGATE_PCIE1_BASE_remap = (unsigned int)ioremap(res->start, res->end - res->start + 1);


    res = platform_get_resource(pdev,IORESOURCE_IO,3);
    if (!res) {
        printk("%s: request PCIE IO fail!!!\n", __func__);
        return -ENXIO;
    }

    GOLDENGATE_PCIE1_CF_BASE_remap = (unsigned int)ioremap(res->start, res->end - res->start + 1);

#ifndef CONFIG_CORTINA_FPGA
    //Root Complex 2 get resource
    res = platform_get_resource(pdev,IORESOURCE_IO,4);
    if (!res) {
        printk("%s: request PCIE BASE fail!!!\n", __func__);
        return -ENXIO;
    }

#ifdef PCIE_DEBUG
        printk("%s: RC2 BASE, res->start=0x%x, res->end=0x%x\r\n", __func__, res->start, res->end);
#endif

    GOLDENGATE_PCIE2_BASE_remap = (unsigned int)ioremap(res->start, res->end - res->start + 1);


    res = platform_get_resource(pdev,IORESOURCE_IO,5);
    if (!res) {
        printk("%s: request PCIE IO fail!!!\n", __func__);
        return -ENXIO;
    }

#ifdef PCIE_DEBUG
    printk("%s: RC2 CF_BASE, res->start=0x%x, res->end=0x%x\r\n", __func__, res->start, res->end);
#endif

    GOLDENGATE_PCIE2_CF_BASE_remap = (unsigned int)ioremap(res->start, res->end - res->start + 1);
#endif

	//debug_Aaron initialize irqs array
    memset(pcie_irqs, 0, sizeof(pcie_irqs));

	//Init PCIe bus then kernel will call scan, read/write PCIe devices configuration, irq map .... etc.
	pci_common_init(&g2_pcie);

	return 0;  
}

static int __devexit g2_pcie_remove(struct platform_device *pdev)
{
	//debug_Aaron
	int i;

	//need update relesae_resource(). 
	/* use a variable value in stead of constant */
	/* for (i = 0; i < NUM_PCIE_PORTS; i++) */
	for (i = 0; i < num_port_sb_phy; i++)
	{
		release_resource(&pdev->resource[i]);
	}
	return 0;
}

#ifdef CONFIG_PM
static int g2_pcie_suspend(struct device *pdev)
{
	int i;
	struct pcie_port *pp;

	for (i = 0; i < num_port_sb_phy; i++)
	{
		pp = &pcie_port[i];
		if (pp->pdev != NULL)
		{
			printk("%s: set power state to D2\r\n", __func__);
			pci_set_power_state(pp->pdev, PCI_D3cold);
		}
	}
	return 0;
}

static int g2_pcie_resume(struct device *pdev)
{
	int i;
	struct pcie_port *pp;

	for (i = 0; i < num_port_sb_phy; i++)
        {
                pp = &pcie_port[i];
                if (pp->pdev != NULL)
                {
                        printk("%s: set power state to D0\r\n", __func__);
                        pci_set_power_state(pp->pdev, PCI_D0);
                }
        }
	return 0;
}

int g2_pcie_suspend_noirq(struct device *dev)
{
	unsigned int	i;
	
	for (i = 0; i < num_port_sb_phy; i++) {
		if (pm_phy_mode & (1 << (i+4)))
			cs_pm_pcie_state_set(0,i,CS_PM_STATE_POWER_DOWN);
	}

	return 0;
}

int g2_pcie_resume_noirq(struct device *dev)
{
	unsigned int	i;

	for (i = 0; i < num_port_sb_phy; i++) {
		if (pm_phy_mode & (1 << (i+4)))
			cs_pm_pcie_state_set(0,i,CS_PM_STATE_NORMAL);
	}

	return 0;
}

static const struct dev_pm_ops g2_pcie_pm_ops = {
	.suspend 	= g2_pcie_suspend,
	.suspend_noirq 	= g2_pcie_suspend_noirq,
	.resume_noirq 	= g2_pcie_resume_noirq,
	.resume 	= g2_pcie_resume,
};
#endif /* CONFIG_PM */

static struct platform_driver g2_pcie_driver __refdata = {
	.probe		= g2_pcie_probe,
	.remove		= __devexit_p(g2_pcie_remove),
	.driver = {
		.name		= "g2_pcie",
		.bus		= &platform_bus_type,
		.owner		= THIS_MODULE,
#ifdef CONFIG_PM
		.pm 		= &g2_pcie_pm_ops,
#endif /* CONFIG_PM */
	},
};


static void __init add_pcie_port(int rc_number, unsigned int base_addr, unsigned int cnf_base_addr)
{
	struct pcie_port *pp = &pcie_port[rc_number];
	int link_up;

	//debug_Aaron, record link status
	link_up = g2_pcie_link_up(rc_number);
#if 0	
	if (!g2_pcie_link_up(rc_number)) 
	{
		printk("%s: rc_number=%d, link down, ignored!!\n", __func__, rc_number);
		return;
	}
#endif

#ifdef PCEI_DEBUG
	printk("%s: rc_number=%d, link status=%d\n", __func__, rc_number, link_up);
#endif

	pp->rc_number = rc_number;
	pp->root_bus_nr = -1;
	pp->base_addr = base_addr;
	pp->cnf_base_addr = cnf_base_addr;
	pp->link_up = link_up;
	spin_lock_init(&pp->conf_lock);
	memset(pp->res, 0, sizeof(pp->res));
}

static int __init g2_pcie_init(int init_port0, int init_port1)
{
    int temp;

#ifdef PCEI_DEBUG
        printk("%s: init_port0=%d, init_port1=%d\n", __func__, init_port0, init_port1);
#endif

    /* debug_Aaron on 2011/07/03, use hardware registers in stead */
#if 0
    msi_buf_addr = kzalloc(NUM_PCIE_PORTS * MSI_BUF_OFFSET, GFP_KERNEL);
    if (msi_buf_addr == NULL)
    {
        printk("%s: MSI buffer allocation fail !\n", __func__);
    }
    else
    {
        printk("%s: MSI buffer address = 0x%x\n", __func__, (unsigned int)msi_buf_addr);
    }
#endif

	//debug_Aaron add_pcie_port() should be called after PHY init 	
    // add_pcie_port(0, GOLDENGATE_PCIE0_BASE);
    // add_pcie_port(1, GOLDENGATE_PCIE0_BASE + GOLDENGATE_PCI_MEM_SIZE);
 
    temp = platform_driver_register(&g2_pcie_driver);

    /* debug_Aaron on 2011/08/05, to fix BUG#29330: ping cause crash from eth0 to PCIe wifi */
    /* assign to CPU0 only */
    /* debug_Aaron on 2011/11/16, after fix PCIe configuration space 0x98.b=0x10 */
    /* it's no needs to assign to CPU0 only now */
    /* debug_Aaron on 2011/11/25, because RT5392AP driver still got TX/RX frozen, assign to CPU0 only */	
#ifdef CONFIG_SMP
    printk("%s, set the PCIe interrupt to CPU0 only\r\n", __func__);	
    irq_set_affinity(IRQ_PCIE0, cpumask_of(0x0));	
    irq_set_affinity(IRQ_PCIE1, cpumask_of(0x0));	
#ifndef CONFIG_CORTINA_FPGA
    irq_set_affinity(IRQ_PCIE2, cpumask_of(0x0));	
#endif
#endif

    return temp;
}

static void __exit g2_pcie_exit(void)
{

	platform_driver_unregister(&g2_pcie_driver);
	
}


module_init(g2_pcie_init);
module_exit(g2_pcie_exit);
