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

EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/kernel_adapt/include -I$(srctree)/drivers/net/ethernet/cs752x/src/include -I$(srctree)/drivers/net/ethernet/cs752x/src/sch -I$(srctree)/drivers/net/ethernet/cs752x/src/tm -I$(srctree)/drivers/net/ethernet/cs752x/src/fe/include/

obj-$(CONFIG_CS752X) += cs_qos.o
cs_qos-objs := cs_hw_accel_qos_core.o cs_hw_accel_qos_multiq.o \
	cs_hw_accel_qos_ingress.o cs_hw_accel_qos_pkt_type_pol.o \
	cs_hw_accel_offset_based_qos.o
