################################################################################
#
# Cortina CS752X Linux driver
# Copyright (c) 2010 by Cortina Systems Incorporated.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#
# The full GNU General Public License is included in this distribution in
# the file called "COPYING".
#
# Contact Information:
# Mailing List <ch.hsu@cortina-systems.com>
# Cortina Systems, Inc., 1376 Bordeaux Drive Sunnyvale, CA 94089
# http://www.cortina-systems.com/
#
################################################################################
#
# Makefile for the Cortina CS752x HW acceleration
#
EXTRA_CFLAGS += -I$(CURDIR)/drivers/net/ethernet/cs752x/src/include -I$(CURDIR)/drivers/net/ethernet/cs752x/src/kernel_adapt/include

obj-$(CONFIG_CS752X_ACCEL_KERNEL) += 8021q/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += bridge/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += multicast/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += netfilter/
#obj-$(CONFIG_CS752X_HW_ACCELERATION) += qos/
obj-$(CONFIG_CS752X_HW_ACCELERATION_IPSEC) += ipsec/
obj-$(CONFIG_CS752X_HW_ACCEL_ETHERIP) += etherip/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += pppoe/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += arp/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += mtu/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += dscp/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += lpm/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += route/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += hash/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += flow/
obj-$(CONFIG_CS75XX_WFO) += wfo/
#obj-$(CONFIG_CS75XX_HW_ACCEL_IPLIP) += tunnel/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += common/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += tunnel/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += rtp/
obj-$(CONFIG_CS752X_ACCEL_KERNEL) += ip_translate/
obj-$(CONFIG_CS75XX_HW_ACCEL_WIRELESS) += wireless/
obj-$(CONFIG_CS752X) += cs_accel_kernel.o
cs_accel_kernel-objs := cs_hw_accel_core.o cs_hw_accel_manager.o
