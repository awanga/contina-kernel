/*
 * FILE NAME cs75xx_ir.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX Consumer Infrared (CIR) Receiver.
 *
 *  Copyright 2012 Cortina , Corp.
 *
 *  Mostly copied from dm1105.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <media/rc-core.h>
#include <mach/cs75xx_ir.h>


#define cs75xx_ir_read_reg(ir, offset)		(readl(ir->base + offset))
#define cs75xx_ir_write_reg(ir, offset, val)	(writel(val, ir->base + offset))

/* Register Map */
#define	CS75XX_IR_ID			0x00
#define	CS75XX_IR_CTRL0			0x04
#define	CS75XX_IR_CTRL1			0x08
#define	CS75XX_IR_INT_ST		0x0C
#define	CS75XX_IR_INT_EN		0x10
#define	CS75XX_IR_DATA			0x14
#define	CS75XX_IR_DATAEXT		0x18
#define	CS75XX_IR_POWER			0x1C
#define	CS75XX_IR_POWEREXT		0x20
#define	CS75XX_PWR_CTRL1		0x28
#define CS75XX_PWR_INT_EN		0x30

#define CIR_CTRL0_NEC_MAX		16
#define	CIR_CTRL1_DATA_LEN_MIN		8
#define	CIR_CTRL1_DATA_LEN_MAX		48

#define	CS75XX_IR_INT_EN_REPEAT		BIT(0)
#define	CS75XX_IR_INT_EN_DATA		BIT(1)
#define	CS75XX_IR_INT_EN_POWER		BIT(2)

#define CS75XX_IR_DEV_ID(ir)		((cs75xx_ir_read_reg(ir, CS75XX_IR_ID) & 0x00FFFF00) >> 8)
#define CS75XX_IR_REV_ID(ir)		(cs75xx_ir_read_reg(ir, CS75XX_IR_ID) & 0x000000FF)
#define CS75XX_IR_DEV_ID_VAL		0x000104


struct cs75xx_ir_device {
	struct platform_device *pdev;
	struct rc_dev *rdev;

	void __iomem *base;
	int irq;

	char name[16];
	struct work_struct work;
	u32 support;
	u32 code[2];
	u8 cfg;
	u8 size;
	u8 in_use;
};

enum cs75xx_ir_hw_param {
	IR_DEMOD_EN,
	IR_POS,
	IR_PROTOCOL,
	IR_RC5_STOPBIT,
	IR_RC5_EXTEND,
	IR_HEAD_LO,
	IR_HEAD_HI,
	IR_BAUD_DIV,
	IR_DATA_LEN,
	IR_DATA_COMPARE,
	IR_PWR_CPU,
	IR_PWR_CODE1,
	IR_PWR_CODE2,
	NR_HW_PARAM
};

static int ir_default_cfg = DVB_PROTOCOL;
static unsigned int ir_hw_params[][NR_HW_PARAM] = {
	[VCR_PROTOCOL] = {	/* VCR-33 */
		[IR_DEMOD_EN] = 0,
		[IR_POS] = 0,
		[IR_PROTOCOL] = 1,
		[IR_RC5_STOPBIT] = 0,
		[IR_RC5_EXTEND] = 0,
		[IR_HEAD_LO] = VCR_L_ACT_PER,
		[IR_HEAD_HI] = VCR_H_ACT_PER,
#ifdef CONFIG_CORTINA_FPGA
		[IR_BAUD_DIV] = (VCR_BAUD*EXT_CLK_SRC/EXT_CLK_DIV),
#else
		[IR_BAUD_DIV] = (VCR_BAUD*EXT_CLK),
#endif
		[IR_DATA_LEN] = VCR_DATA_LEN,
		[IR_DATA_COMPARE] = 0,
		[IR_PWR_CPU] = 0,
		[IR_PWR_CODE1] = VCR_KEY_POWER,
		[IR_PWR_CODE2] = 0,
	},
	[TV1_PROTOCOL] = {	/* TV1-26 */
		[IR_DEMOD_EN] = 0,
		[IR_POS] = 0,
		[IR_PROTOCOL] = 1,
		[IR_RC5_STOPBIT] = 0,
		[IR_RC5_EXTEND] = 0,
		[IR_HEAD_LO] = TV1_L_ACT_PER,
		[IR_HEAD_HI] = TV1_H_ACT_PER,
#ifdef CONFIG_CORTINA_FPGA
		[IR_BAUD_DIV] = (TV1_BAUD*EXT_CLK_SRC/EXT_CLK_DIV),
#else
		[IR_BAUD_DIV] = (TV1_BAUD*EXT_CLK),
#endif
		[IR_DATA_LEN] = TV1_DATA_LEN,
		[IR_DATA_COMPARE] = 0,
		[IR_PWR_CPU] = 0,
		[IR_PWR_CODE1] = TV1_KEY_POWER,
		[IR_PWR_CODE2] = TV1_KEY_POWER_EXT,
	},
	[DVB_PROTOCOL] = {
		[IR_DEMOD_EN] = 0,
		[IR_POS] = 0,
		[IR_PROTOCOL] = 0,
		[IR_RC5_STOPBIT] = 0,
		[IR_RC5_EXTEND] = 1,
		[IR_HEAD_LO] = 0,
		[IR_HEAD_HI] = 0,
#ifdef CONFIG_CORTINA_FPGA
		[IR_BAUD_DIV] = (DVB_BAUD*EXT_CLK_SRC/EXT_CLK_DIV),
#else
		[IR_BAUD_DIV] = (DVB_BAUD*EXT_CLK),
#endif
		[IR_DATA_LEN] = VCR_DATA_LEN,
		[IR_DATA_COMPARE] = 0,
		[IR_PWR_CPU] = 0,
		[IR_PWR_CODE1] = DVB_KEY_POWER,
		[IR_PWR_CODE2] = 0,
	},
	[CUSTOM_RC] = {
		[IR_DEMOD_EN] = 0,
		[IR_POS] = 0,
		[IR_PROTOCOL] = 0,
		[IR_RC5_STOPBIT] = 0,
		[IR_RC5_EXTEND] = 1,
		[IR_HEAD_LO] = 0,
		[IR_HEAD_HI] = 0,
#ifdef CONFIG_CORTINA_FPGA
		[IR_BAUD_DIV] = (DVB_BAUD*EXT_CLK_SRC/EXT_CLK_DIV),
#else
		[IR_BAUD_DIV] = (DVB_BAUD*EXT_CLK),
#endif
		[IR_DATA_LEN] = 31,
		[IR_DATA_COMPARE] = 0,
		[IR_PWR_CPU] = 0,
		[IR_PWR_CODE1] = DVB_KEY_POWER,
		[IR_PWR_CODE2] = 0
	}
};

/* ---------------------------------------------------------------------- */

/* ir work handler */
static void cs75xx_emit_key(struct work_struct *work)
{
	struct cs75xx_ir_device *ir = container_of(work, struct cs75xx_ir_device, work);
	u32 ircom;

	ircom = ir->code[0];

	rc_keydown(ir->rdev, ircom, 0);
}

static void cs75xx_ir_enable_int(struct cs75xx_ir_device *ir)
{
	cs75xx_ir_write_reg(ir, CS75XX_IR_INT_EN, ir->support);
}

static void cs75xx_ir_disable_int(struct cs75xx_ir_device *ir)
{
	cs75xx_ir_write_reg(ir, CS75XX_IR_INT_EN, 0);
}

static irqreturn_t cs75xx_ir_interrupt(int irq, void *dev_instance)
{
	struct cs75xx_ir_device *ir = (struct cs75xx_ir_device *)dev_instance;
	CIR_PWRCTRL_CIR_INT_STATUS_t reg_int_st;
	CIR_PWRCTRL_CIR_INT_ENABLE_t reg_int_en;

	/* disable interrupt */
	reg_int_en.wrd = 0;
	cs75xx_ir_write_reg(ir, CS75XX_IR_INT_EN, reg_int_en.wrd);

	reg_int_st.wrd = cs75xx_ir_read_reg(ir, CS75XX_IR_INT_ST);
	if (reg_int_st.bf.repeat_sts ||	reg_int_st.bf.cir_dat_int ||
					reg_int_st.bf.pwrkey_int_sts) {
		ir->code[0] = cs75xx_ir_read_reg(ir, CS75XX_IR_DATA);
		if (ir->size > 32)
			ir->code[1] = cs75xx_ir_read_reg(ir, CS75XX_IR_DATAEXT);
	}

	/* clear interrupt */
	cs75xx_ir_write_reg(ir, CS75XX_IR_INT_ST, reg_int_st.wrd);

	/* ensable interrupt */
	reg_int_en.bf.pwrkey_int_en = 1;
	reg_int_en.bf.dat_int_en = 1;
	reg_int_en.bf.repeat_int_en = 1;
	cs75xx_ir_write_reg(ir, CS75XX_IR_INT_EN, reg_int_en.wrd);

	schedule_work(&ir->work);

	return IRQ_RETVAL(IRQ_HANDLED);
}

/* ---------------------------------------------------------------------- */

static int cs75xx_ir_open(struct rc_dev *rc)
{
	struct cs75xx_ir_device *ir = rc->priv;

	ir->in_use = 1;
	// enable interrupt

	return 0;
}

static void cs75xx_ir_close(struct rc_dev *rc)
{
	struct cs75xx_ir_device *ir = rc->priv;

	ir->in_use = 0;
	// disable interrupt
}

/* ---------------------------------------------------------------------- */

int cs75xx_ir_sw_init(struct cs75xx_ir_device *ir)
{
	int error = -ENOMEM;
	struct rc_dev *rdev;
	char *rc_map_cfg;

	/* allocate memory */
	rdev = rc_allocate_device();
	if (!rdev)
		goto err_out_free;

	rdev->driver_type = RC_DRIVER_SCANCODE;
	rdev->allowed_protos = RC_TYPE_RC5 | RC_TYPE_NEC;
	rdev->priv = ir;
	rdev->open = cs75xx_ir_open;
	rdev->close = cs75xx_ir_close;
	rdev->input_name = ir->name;

	ir->rdev = rdev;

	INIT_WORK(&ir->work, cs75xx_emit_key);

	/* all done */
	switch (ir->cfg) {
	case VCR_PROTOCOL:
		rc_map_cfg = RC_MAP_NEC_KOKA_VCR_33;
		break;
#if 0	/* TV1 code is 48 bits, but Linux IR only suuports it up to 32 bits */
	case TV1_PROTOCOL:
		rc_map_cfg = RC_MAP_NEC_KOKA_TV1_33;
		break;
#endif
	case DVB_PROTOCOL:
		rc_map_cfg = RC_MAP_RC5_DVB_T;
		break;
	case CUSTOM_RC:
	default:
		rc_map_cfg = RC_MAP_LIRC;
	}
	rdev->map_name = rc_map_cfg;
	error = rc_register_device(rdev);
	if (error) {
		dev_err(&ir->pdev->dev, "ir_input_register fail!\n");
		goto err_out_free;
	}

	return 0;
err_out_free:
	return error;
}

int cs75xx_ir_sw_fini(struct cs75xx_ir_device *ir)
{
	rc_unregister_device(ir->rdev);

	return 0;
}

static int cs75xx_ir_hw_init(struct cs75xx_ir_device *ir)
{
	CIR_PWRCTRL_CIR_RXCTRL0_t reg_ctrl0;
	CIR_PWRCTRL_CIR_RXCTRL1_t reg_ctrl1;
	CIR_PWRCTRL_CIR_PWRKEY_t reg_pwrkey;
	CIR_PWRCTRL_CIR_PWRKEY_EXT_t reg_pwrkey_ext;

	reg_ctrl0.wrd = cs75xx_ir_read_reg(ir, CS75XX_IR_CTRL0);
	reg_ctrl0.bf.demod_en	    = ir_hw_params[ir->cfg][IR_DEMOD_EN];
	reg_ctrl0.bf.pos	    = ir_hw_params[ir->cfg][IR_POS];
	reg_ctrl0.bf.cir_protocol   = ir_hw_params[ir->cfg][IR_PROTOCOL];
	reg_ctrl0.bf.rc5_stopBit_en = ir_hw_params[ir->cfg][IR_RC5_STOPBIT];
	reg_ctrl0.bf.rc5_extend     = ir_hw_params[ir->cfg][IR_RC5_EXTEND];
	reg_ctrl0.bf.head_lo_t	    = ir_hw_params[ir->cfg][IR_HEAD_LO];
	reg_ctrl0.bf.head_hi_t	    = ir_hw_params[ir->cfg][IR_HEAD_HI];
	reg_ctrl0.bf.baud_div	    = ir_hw_params[ir->cfg][IR_BAUD_DIV];
	cs75xx_ir_write_reg(ir, CS75XX_IR_CTRL0, reg_ctrl0.wrd);

	reg_ctrl1.bf.data_len_b     = ir_hw_params[ir->cfg][IR_DATA_LEN];
	reg_ctrl1.bf.data_compare   = ir_hw_params[ir->cfg][IR_DATA_COMPARE];
	reg_ctrl1.bf.pwrKeyIRQCpu   = ir_hw_params[ir->cfg][IR_PWR_CPU];
#ifndef CONFIG_CS75XX_PWC
	reg_ctrl1.bf.pwrKeyIRQCpu   = 1;
#endif
	cs75xx_ir_write_reg(ir, CS75XX_IR_CTRL1, reg_ctrl1.wrd);

	reg_pwrkey.bf.pwr_code1     = ir_hw_params[ir->cfg][IR_PWR_CODE1];
	cs75xx_ir_write_reg(ir, CS75XX_IR_POWER, reg_pwrkey.wrd);
	reg_pwrkey_ext.bf.pwr_code2 = ir_hw_params[ir->cfg][IR_PWR_CODE2];
	cs75xx_ir_write_reg(ir, CS75XX_IR_POWEREXT, reg_pwrkey_ext.wrd);

	/* This will turn off the system power. It should run on bootloader
	   phase, not kernel driver loading. */
	if (reg_ctrl0.bf.fst_por_ok == 0) {
		CIR_PWRCTRL_CIR_INT_STATUS_t reg_int_st;
		CIR_PWRCTRL_PWR_CTRL1_t reg_pwr_ctrl1;
		CIR_PWRCTRL_PWR_INT_ENABLE_t reg_pwr_inten;

		reg_ctrl0.bf.fst_por_ok = 1;
		cs75xx_ir_write_reg(ir, CS75XX_IR_CTRL0, reg_ctrl0.wrd);

		/* Clear CIR Interrupt */
		reg_int_st.wrd = cs75xx_ir_read_reg(ir, CS75XX_IR_INT_ST);
		reg_int_st.bf.pwrkey_int_sts = 1;
		reg_int_st.bf.cir_dat_int = 1;
		reg_int_st.bf.repeat_sts = 1;
		cs75xx_ir_write_reg(ir, CS75XX_IR_INT_ST, reg_int_st.wrd);

#ifndef CONFIG_CS75XX_PWC
		goto finish;
#endif
		/* Clear PWR Interrupt */
		cs75xx_ir_write_reg(ir, CS75XX_PWR_INT_EN, 0);

		reg_pwr_ctrl1.wrd = 0;
		reg_pwr_ctrl1.bf.pwr_int_clear = 1;
		cs75xx_ir_write_reg(ir, CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

		/* Turn-on Interrupt */
		reg_pwr_inten.wrd = cs75xx_ir_read_reg(ir, CS75XX_PWR_INT_EN);
		reg_pwr_inten.bf.cir_pwr_on_en = 1;
		reg_pwr_inten.bf.rtc_wake_en = 1;
		reg_pwr_inten.bf.push_btn_wake_en = 1;
		cs75xx_ir_write_reg(ir, CS75XX_PWR_INT_EN, reg_pwr_inten.wrd);

		/* To Shut-Down, PWR_CTRL1_INIT_FINISH and PWR_CTRL1_SHUT_DOWN
		   can't set at the same time */
		reg_pwr_ctrl1.wrd = 0;
		reg_pwr_ctrl1.bf.sysInitFinish = 1;
		cs75xx_ir_write_reg(ir, CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

		mdelay(2);	/* Not Remove */

		reg_pwr_ctrl1.wrd = 0;
		reg_pwr_ctrl1.bf.swShutdnEn = 1;
		cs75xx_ir_write_reg(ir, CS75XX_PWR_CTRL1, reg_pwr_ctrl1.wrd);

		dev_info(&ir->pdev->dev, "Please Press Power Button\n");

		while(1);	/* stop until the system power down */
	}
	else {	/* Clear INTERRUPT before request_irq */
		CIR_PWRCTRL_CIR_INT_STATUS_t reg_int_st;

		reg_int_st.bf.pwrkey_int_sts = 1;
		reg_int_st.bf.cir_dat_int = 1;
		reg_int_st.bf.repeat_sts = 1;
		cs75xx_ir_write_reg(ir, CS75XX_IR_INT_ST, reg_int_st.wrd);
	}

finish:
	if (ir->cfg != CUSTOM_RC)	/* for display */
		memcpy(ir_hw_params[CUSTOM_RC], ir_hw_params[ir->cfg], sizeof(ir_hw_params[CUSTOM_RC]));

	return 0;
}

static int __devinit cs75xx_ir_probe(struct platform_device *pdev)
{
	struct cs75xx_ir_device *ir = NULL;
	struct resource *res_mem;
	char tmp_str[16];
	int rc;

	ir = kzalloc(sizeof(struct cs75xx_ir_device), GFP_KERNEL);
	if (!ir) {
		dev_err(&pdev->dev, "Func: %s - can't allocate memory for %s device\n", __func__, "ir");
		rc = -ENOMEM;
		goto err_get_ir;
	}

	/* get the module base address and irq number */
	sprintf(tmp_str, "cir");
	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, tmp_str);
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, tmp_str);
		rc = ENXIO;
		goto err_get_io;
	}
	ir->base = ioremap(res_mem->start, resource_size(res_mem));
	if (!ir->base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory\n",
		        __func__, tmp_str, resource_size(res_mem));
		rc = -ENOMEM;
		goto err_get_io;
	}
	dev_dbg(&pdev->dev, "\tir_base = 0x%p, range = %d\n", ir->base,
	        resource_size(res_mem));

	sprintf(tmp_str, "irq_cir");
	ir->irq = platform_get_irq_byname(pdev, tmp_str);
	if (ir->irq == -ENXIO) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, tmp_str);
		rc = ENXIO;
		goto err_get_irq;
	}
	dev_dbg(&pdev->dev, "\tirq_ir = %d\n", ir->irq);

	cs75xx_ir_disable_int(ir);
	if ((rc = request_irq(ir->irq, cs75xx_ir_interrupt, 0, "cs75xx_ir", ir)) != 0) {
		dev_err(&pdev->dev, "Error: Register IRQ for CS75XX IR failed %d\n", rc);
		goto err_get_irq;
	}

	platform_set_drvdata(pdev, ir);
	ir->pdev = pdev;

	/* config */
	strlcpy(ir->name, "cs75xx-ir", sizeof(ir->name));
	ir->support = CS75XX_IR_INT_EN_REPEAT | CS75XX_IR_INT_EN_DATA |
	              CS75XX_IR_INT_EN_POWER;
	if (ir_default_cfg > CUSTOM_RC)
		goto err_init;
	else
		ir->cfg = ir_default_cfg;
	ir->size = 32;
	ir->in_use = 0;

	/* init hw */
	rc = cs75xx_ir_hw_init(ir);
	if (rc)
		goto err_init;

	/* init ir */
	rc = cs75xx_ir_sw_init(ir);
	if (rc)
		goto err_init;

	cs75xx_ir_enable_int(ir);

	dev_info(&pdev->dev, "Func: %s(%s) %s-%s init ... OK\n", __func__, pdev->name, __DATE__, __TIME__);
	return 0;

err_init:
	free_irq(ir->irq, ir);
err_get_irq:
	iounmap(ir->base);
err_get_io:
	kfree(ir);
err_get_ir:

	dev_info(&pdev->dev, "Func: %s(%s) %s-%s init ... FAIL(%d)\n", __func__, pdev->name, __DATE__, __TIME__, rc);
	return rc;
}

static int __devexit cs75xx_ir_remove(struct platform_device *pdev)
{
	struct cs75xx_ir_device *ir = platform_get_drvdata(pdev);

	/* disable interrupt */

	cs75xx_ir_sw_fini(ir);

	free_irq(ir->irq, ir);
	iounmap(ir->base);
	kfree(ir);

	return 0;
}

static struct platform_driver cs75xx_ir_platform_driver = {
	.probe	= cs75xx_ir_probe,
	.remove	= __devexit_p(cs75xx_ir_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name = CS75XX_IR_NAME
	},
};

static int __init cs75xx_ir_init(void)
{
	return platform_driver_register(&cs75xx_ir_platform_driver);
}
module_init(cs75xx_ir_init);

static void __exit cs75xx_ir_exit(void)
{
	platform_driver_unregister(&cs75xx_ir_platform_driver);
}
module_exit(cs75xx_ir_exit);

module_param_named(cfg, ir_default_cfg, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(cfg, "ir configuration:\n" \
	"\t0 - KOKA's KUC-100 remote control with VCR-33 option\n" \
	"\t1 - KOKA's KUC-100 remote control with TV1-26 option\n" \
	"\t2 - No brand DVB-T remote control" \
	"\t2 - Customize by user");
module_param_array_named(hw_params, ir_hw_params[CUSTOM_RC], uint, NULL, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(hw_params, "hw setting parameters\n" \
	"\tparam 0 - demodulation enable\n" \
	"\tparam 1 - the polarity of received signal\n" \
	"\tparam 2 - 0:RC5 protocol, 1:NEC protocol\n" \
	"\tparam 3 - decode received frame with stop bit, RC5 only\n" \
	"\tparam 4 - decode received frame with second start bit, RC5 only\n" \
	"\tparam 5 - low period of frame header, NEC only\n" \
	"\tparam 6 - high period of frame header, NEC only\n" \
	"\tparam 7 - baud rate dividend = T(Baud_rate)*clk_osc(24 Mhz)\n" \
	"\tparam 8 - the data length of received frame 32 ~ 48bits, NEC only\n" \
	"\tparam 9 - received frame's data and data bar comparing, NEC only\n" \
	"\tparam 10 - power key to CPU\n" \
	"\tparam 11 - power key code\n" \
	"\tparam 12 - power key code extend\n");

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Cortina CS75XX IR driver");

