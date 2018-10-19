#include <mach/cs_cpu.h>

static void sb_phy_program_24mhz(unsigned int baddr)
{
	volatile void __iomem *base_addr = (volatile void __iomem *)baddr;
	unsigned int value;

	writel(0x00090006, base_addr);
	writel(0x000e0960, base_addr + 4);

   	writel(0x68a00000, base_addr + 32);     /* Samsung's suggestion */
   	/* writel(0x680a0000, base_addr + 32); */
	printk("%s: base_addr + 32 = 0x%x\r\n", __func__, readl(base_addr + 32));

	writel(0x50040000, base_addr + 44);
	writel(0x40250270, base_addr + 48);
	writel(0x00004001, base_addr + 52);

	/* debug_Aaron on 2012/04/05 for A1 chip checking */
	if (cs_soc_is_cs7522a0() || cs_soc_is_cs7542a0())
		value = 0x5e082e00;
	else if (cs_soc_is_cs7522a1() || cs_soc_is_cs7542a1())
		value = 0x5e080600;
	else
		printk("%s: Wrong JTAG ID:%p (FPGA ?)\n", __func__, IO_ADDRESS(GLOBAL_JTAG_ID));
	writel(value, base_addr + 96);	 /* Samsung's suggestion */

        /* writel(0x5e002e00, base_addr + 96); */
	printk("%s: base_addr + 96 = 0x%x\r\n", __func__, readl(base_addr + 96));

        writel(0xF0914200, base_addr + 100);	 /* Samsung's suggestion */
        /* writel(0x10914200, base_addr + 100); */
	printk("%s: base_addr + 100 = 0x%x\r\n", __func__, readl(base_addr + 100));

	writel(0x4c0c9048, base_addr + 104);
	writel(0x00000373, base_addr + 108);  //REF clock selection
	writel(0x04841000, base_addr + 124);

        writel(0x000000e0, base_addr + 128);    /* Samsung's suggestion */
        /* writel(0x0000000e, base_addr + 128); */
	printk("%s: base_addr + 128 = 0x%x\r\n", __func__, readl(base_addr + 128));

	writel(0x04000023, base_addr + 132);
	writel(0x68001038, base_addr + 136);
	writel(0x0d181ea2, base_addr + 140);
	writel(0x0000000c, base_addr + 144);
	writel(0x0f600000, base_addr + 196);
	writel(0x400290c0, base_addr + 200);
	writel(0x0000003c, base_addr + 204);
	writel(0xc68b8300, base_addr + 208);
	writel(0x98280301, base_addr + 212);
	writel(0xe1782819, base_addr + 216);
	writel(0x00f410f0, base_addr + 220);
	writel(0xa0a0a000, base_addr + 232);
	writel(0xa0a0a0a0, base_addr + 236);
	writel(0x9fc00068, base_addr + 240);
	writel(0x00000001, base_addr + 244);
	writel(0x00000000, base_addr + 248);
	writel(0xd07e4130, base_addr + 252);
	writel(0x935285cc, base_addr + 256);
	writel(0xb0dd49e0, base_addr + 260);
	writel(0x0000020b, base_addr + 264);
	writel(0xd8000000, base_addr + 300);
	writel(0x0001ff1a, base_addr + 304);
	writel(0xf0000000, base_addr + 308);
	writel(0xffffffff, base_addr + 312);
	writel(0x3fc3c21c, base_addr + 316);
	writel(0x0000000a, base_addr + 320);
	writel(0x00f80000, base_addr + 324);

	//release AHB reset
	writel(0x00090007, base_addr);
}

static void sb_phy_program_100mhz(unsigned int baddr)
{
    volatile void __iomem *base_addr = (volatile void __iomem *)baddr;
    unsigned int value;

    writel(0x00010006, base_addr);   	//rate prog
    writel(0x64a00000, base_addr + 32);
    writel(0x50040000, base_addr + 44);
    writel(0x40250270, base_addr + 48);
    writel(0x00004001, base_addr + 52);


	/* debug_Aaron on 2012/04/05 for A1 chip checking */
    if (cs_soc_is_cs7522a0() || cs_soc_is_cs7542a0())
	    value = 0x5e002e00;
    else if (cs_soc_is_cs7522a1() || cs_soc_is_cs7542a1())
	    value = 0x5e000600;
    else
	    printk("%s: Wrong JTAG ID:%p (FPGA ?)\n", __func__, IO_ADDRESS(GLOBAL_JTAG_ID));
    writel(value, base_addr + 96);   /* Samsung's suggestion */

    writel(0x90914200, base_addr + 100);	 /* Samsung's suggestion */
    /* writel(0x10914200, base_addr + 100); */
    printk("%s: base_addr + 100 = 0x%x\r\n", __func__, readl(base_addr + 100));	

    writel(0xce449048, base_addr + 104);
    writel(0x0000000b, base_addr + 108);  //REF clock selection
    writel(0x04841000, base_addr + 124);
    writel(0x000000e0, base_addr + 128);
    writel(0x04000023, base_addr + 132);
    writel(0x68000438, base_addr + 136);
    writel(0x0d181ea2, base_addr + 140);
    writel(0x0000000d, base_addr + 144);
    writel(0x0f600000, base_addr + 196);
    writel(0x400290c0, base_addr + 200);  //8b/10 enc. enable, DW-bits
    writel(0x0000003c, base_addr + 204);
    writel(0xc6496300, base_addr + 208);
    writel(0x98280301, base_addr + 212);
    writel(0xe1782819, base_addr + 216);
    writel(0x00f410f0, base_addr + 220);
    writel(0xa0a0a000, base_addr + 232);
    writel(0xa0a0a0a0, base_addr + 236);
    writel(0x9fc00064, base_addr + 240);
    writel(0x00000001, base_addr + 244);
    writel(0xd07e4130, base_addr + 252);
    writel(0x935285cc, base_addr + 256);
    writel(0xb0dd49e0, base_addr + 260);
    writel(0x0000020b, base_addr + 264);
    writel(0xd8000000, base_addr + 300);
    writel(0x0001ff1a, base_addr + 304);
	writel(0xf0000000, base_addr + 308);
    writel(0xffffffff, base_addr + 312);
	writel(0x3fc3c21c, base_addr + 316);
    writel(0x0000000a, base_addr + 320);
    writel(0x00f80000, base_addr + 324);

    //release AHB reset
    writel(0x00010007, base_addr);
}

static void sb_phy_program(int is_24mhz, int phy_number)
{
	int i;
	GLOBAL_PHY_CONTROL_t phy_control;
    PCIE_SATA_PCIE_GLBL_CMU_OK_CORE_DEBUG_13_t cmu_ok;
    PCIE_SATA_SNOW_PHY_COM_LANE_REG3_REG2_REG1_REG0_t com_lane;
	unsigned int reg_offset;
	GLOBAL_BLOCK_RESET_t block_reset;

	printk("%s: is_24mhz=%d, phy_number=%d\r\n", __func__, is_24mhz, phy_number);


	/* Before do the configuration for Snow Bush PHY, power down the PHY first */
    /* to avoid system panic */
	 /* Release the power on reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
            por_n_i = 1'b1
            pd = 0'b0
       If reference clock is 24MHz, then program refclksel = 2'b10
       If reference clock is 100MHz, then program refclksel = 2'b00
    */
    phy_control.wrd = readl((volatile void __iomem *)GLOBAL_PHY_CONTROL);
    switch (phy_number)
    {
        case 0:
            phy_control.bf.phy_0_por_n_i = 0;
            phy_control.bf.phy_0_pd = 1;
            phy_control.bf.phy_0_refclksel = 2;  /* if 24MHz */
	    	phy_control.bf.phy_0_ln0_resetn_i = 0;
	    	phy_control.bf.phy_0_cmu_resetn_i = 0;
            break;
        case 1:
            phy_control.bf.phy_1_por_n_i = 0;
            phy_control.bf.phy_1_pd = 1;
            phy_control.bf.phy_1_refclksel = 2;  /* if 24MHz */
	    	phy_control.bf.phy_1_ln0_resetn_i = 0;
	    	phy_control.bf.phy_1_cmu_resetn_i = 0;
            break;
        case 2:
            phy_control.bf.phy_2_por_n_i = 0;
            phy_control.bf.phy_2_pd = 1;
            phy_control.bf.phy_2_refclksel = 2;  /* if 24MHz */
	    	phy_control.bf.phy_2_ln0_resetn_i = 0;
	    	phy_control.bf.phy_2_cmu_resetn_i = 0;
            break;
            break;
    }
    writel(phy_control.wrd, (volatile void __iomem *)GLOBAL_PHY_CONTROL);
	mdelay(100);

	/* Release the power on reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
            por_n_i = 1'b1
            pd = 1'b0
       If reference clock is 24MHz, then program refclksel = 2'b10
       If reference clock is 100MHz, then program refclksel = 2'b00
    */
    phy_control.wrd = readl((volatile void __iomem *)GLOBAL_PHY_CONTROL);
	switch (phy_number)
	{
		case 0:
        	phy_control.bf.phy_0_por_n_i = 1;
        	phy_control.bf.phy_0_pd = 0;
        	phy_control.bf.phy_0_refclksel = 2;  /* if 24MHz */
			break;
		case 1:
        	phy_control.bf.phy_1_por_n_i = 1;
        	phy_control.bf.phy_1_pd = 0;
        	phy_control.bf.phy_1_refclksel = 2;  /* if 24MHz */
			break;
		case 2:
        	phy_control.bf.phy_2_por_n_i = 1;
        	phy_control.bf.phy_2_pd = 0;
        	phy_control.bf.phy_2_refclksel = 2;  /* if 24MHz */
			break;
    }
    writel(phy_control.wrd, (volatile void __iomem *)GLOBAL_PHY_CONTROL);
	udelay(10);

	switch (phy_number)
    {
        case 0:
			if (is_24mhz)
				sb_phy_program_24mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0);
			else
				sb_phy_program_100mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0);
			break;
		case 1:
			if (is_24mhz)
        		sb_phy_program_24mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x4000);
			else
        		sb_phy_program_100mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x4000);
			break;
		case 2:
			if (is_24mhz)
        		sb_phy_program_24mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x8000);
			else
        		sb_phy_program_100mhz(PCIE_SATA_SNOW_PHY_CMU_REG3_REG2_REG1_REG0 + 0x8000);
			break;
    }
	/* Release the cmu reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
            cmu_resetn_i = 1'b1
    */
    phy_control.wrd = readl((volatile void __iomem *)GLOBAL_PHY_CONTROL);
	switch (phy_number)
    {
        case 0:
        	phy_control.bf.phy_0_cmu_resetn_i = 1;
			break;
		case 1:
        	phy_control.bf.phy_1_cmu_resetn_i = 1;
			break;
		case 2:
        	phy_control.bf.phy_2_cmu_resetn_i = 1;
			break;
    }
    writel(phy_control.wrd, (volatile void __iomem *)GLOBAL_PHY_CONTROL);

    /* Wait for CMU OK */
    for (i = 0; i < 1000; i++)
    {
        cmu_ok.wrd = readl((volatile void __iomem *)
		(PCIE_SATA_PCIE_GLBL_CMU_OK_CORE_DEBUG_13 + phy_number * 0x400));
        if (cmu_ok.bf.phy_cmu_ok == 1)
            break;

        udelay(100);
    }


    /* Release Lane0 master reset */
    /* Register: Common Lane Register 0 Base address +'d200 = 32'h400290c2 */
    com_lane.wrd = 0x400290c2;
    writel(com_lane.wrd, (volatile void __iomem *)
		(PCIE_SATA_SNOW_PHY_COM_LANE_REG3_REG2_REG1_REG0 + phy_number * 0x4000));


    /* Release the lane reset */
    /* Register: GLOBAL_PHY_CONTROL_phy_# */
    /* Fields to write:
                ln0_resetn_i = 1'b1
    */
    phy_control.wrd = readl((volatile void __iomem *)GLOBAL_PHY_CONTROL);

	switch (phy_number)
    {
		case 0:
        	phy_control.bf.phy_0_ln0_resetn_i = 1;
			break;
		case 1:
        	phy_control.bf.phy_1_ln0_resetn_i = 1;
			break;
		case 2:
        	phy_control.bf.phy_2_ln0_resetn_i = 1;
			break;
    }
    writel(phy_control.wrd, (volatile void __iomem *)GLOBAL_PHY_CONTROL);
    mdelay(100);

    reg_offset = GLOBAL_BLOCK_RESET;
    block_reset.wrd = readl((volatile void __iomem *)reg_offset);
    if (phy_number == 0)
    	block_reset.bf.reset_pcie0 = 1;
    else if (phy_number == 1)
        block_reset.bf.reset_pcie1 = 1;
    else if (phy_number == 2)
        block_reset.bf.reset_pcie2 = 1;
    writel(block_reset.wrd, (volatile void __iomem *)GLOBAL_BLOCK_RESET);
    msleep(10);
	if (phy_number == 0)
    	block_reset.bf.reset_pcie0 = 0;
    else if (phy_number == 1)
    	block_reset.bf.reset_pcie1 = 0;
    else if (phy_number == 2)
        block_reset.bf.reset_pcie2 = 0;
    writel(block_reset.wrd, (volatile void __iomem *)GLOBAL_BLOCK_RESET);
    msleep(10);
}

static int sb_phy_init(void)
{
	//parse command line, SB_PHY=PPSS
	int i;
	char *ptr;
	/*unsigned int phymap = 0;
	int port_number = 0;*/
	int num_port;

/* 	printk("%s: command line=%s\r\n", __func__, saved_command_line); */
	ptr = strstr(saved_command_line, "SB_PHY");
	if (ptr == NULL)
	{
		printk("%s: no SB_PHY found !!!\r\n", __func__);
		return -1;
	}
/* 	printk("%s: %s\r\n", __func__, ptr); */
	ptr += strlen("SB_PHY") + 1;

	num_port = 0;
	for (i = 0; i < 4; i++, ptr)
	{
		if (ptr[i] == 'P')
		{
#ifndef CONFIG_PCIE_EXTERNAL_CLOCK
			sb_phy_program(1, i);
#else
			sb_phy_program(0, i);
#endif
			num_port++;
		}
	}

	if (num_port == 0)
	{
		printk("%s: no 'P' found in SB_PHY=\r\n", __func__);
		return -1;
	}
	return num_port;
}
