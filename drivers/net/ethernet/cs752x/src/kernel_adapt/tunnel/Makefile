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

EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/kernel_adapt/include
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/ni
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/qm
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/diagnostic
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/fe/include
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/include
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/sch
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/tm
EXTRA_CFLAGS += -I$(srctree)/net/l2tp

obj-$(CONFIG_CS752X_ACCEL_KERNEL) += cs_tunnel.o
obj-$(CONFIG_CS75XX_HW_ACCEL_PPTP) += cs_tunnel_pptp.o
obj-$(CONFIG_CS75XX_HW_ACCEL_L2TP_IPSEC) += cs_tunnel_l2tp_ipsec.o
obj-$(CONFIG_CS75XX_HW_ACCEL_IPLIP) += cs_tunnel_iplip.o
obj-$(CONFIG_CS75XX_HW_ACCEL_IPSEC_CTRL) += cs_tunnel_ipsec_ctrl.o
obj-$(CONFIG_CS75XX_HW_ACCEL_L2TP_CTRL) += cs_tunnel_l2tp_ctrl.o
cs_tunnel-objs := cs_hw_accel_tunnel.o
cs_tunnel_pptp-objs := cs_hw_accel_pptp.o
cs_tunnel_l2tp_ipsec-objs := cs_hw_accel_l2tp_ipsec.o cs_hw_accel_ipsec_api.o
cs_tunnel_iplip-objs := cs_hw_accel_iplip.o
cs_tunnel_ipsec_ctrl-objs := cs_hw_accel_ipsec_ctrl.o
cs_tunnel_l2tp_ctrl-objs := cs_hw_accel_l2tp_ctrl.o
