/*
 *  linux/arch/arm/mach-realview/clock.c
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include <asm/hardware/icst.h>

#include "clock.h"

// Suresh We don't use clock framework we introduce dummy functions
// will revisit if needed

int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

#ifdef FIXME 
long clk_round_rate(struct clk *clk, unsigned long rate)
{
	struct icst_vco vco;
	vco = icst_khz_to_vco(clk->params, rate / 1000);
	return icst_khz(clk->params, vco) * 1000;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EIO;

	if (clk->setvco) {
		struct icst307_vco vco;

		vco = icst_khz_to_vco(clk->params, rate / 1000);
		clk->rate = icst_khz(clk->params, vco) * 1000;
		clk->setvco(clk, vco);
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(clk_set_rate);
#else
long clk_round_rate(struct clk *clk, unsigned long rate)
{
#if 0
  long ret = -EIO;

  if (clk->ops && clk->ops->round)
    ret = clk->ops->round(clk, rate);
  return ret;
#endif
  return 0;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
#if 0
  int ret = -EIO;
  if (clk->ops && clk->ops->set)
    ret = clk->ops->set(clk, rate);
  return ret;
#endif
  return 0;
}
EXPORT_SYMBOL(clk_set_rate);
#endif
