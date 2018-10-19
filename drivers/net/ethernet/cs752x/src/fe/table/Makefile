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

EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/include
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/fe/include
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/cs752x/src/ni

obj-$(CONFIG_CS752X) += cs_fe_table.o
cs_fe_table-objs := cs_fe_hw_table.o cs_fe_table_util.o cs_fe_table_generic.o \
	cs_fe_table_acl.o cs_fe_table_an_bng_mac.o cs_fe_table_class.o \
	cs_fe_table_etype.o cs_fe_table_fvlan.o cs_fe_table_fwdrslt.o \
	cs_fe_table_hashcheck.o cs_fe_table_hashhash.o cs_fe_table_hashmask.o \
	cs_fe_table_hashoverflow.o cs_fe_table_l2mac.o cs_fe_table_l3ip.o \
	cs_fe_table_l4portrngs.o cs_fe_table_llchdr.o cs_fe_table_lpb.o \
	cs_fe_table_pktlen.o cs_fe_table_qosrslt.o cs_fe_table_sdb.o \
	cs_fe_table_vlan.o cs_fe_table_voqdrp.o cs_fe_table_voqpol.o \
	cs_fe_table_hashstatus.o cs_fe_table_core.o cs_fe_table_lpm.o


# FE unit test
obj-$(CONFIG_CS75XX_FE_TBL_MGMT_UT) += cs_fe_table_ut.o
