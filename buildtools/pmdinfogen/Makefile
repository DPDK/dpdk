# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Neil Horman <nhorman@tuxdriver.com>
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
HOSTAPP = dpdk-pmdinfogen

#
# all sources are stored in SRCS-y
#
SRCS-y += pmdinfogen.c

HOST_CFLAGS += $(HOST_WERROR_FLAGS) -g
HOST_CFLAGS += -I$(RTE_OUTPUT)/include

include $(RTE_SDK)/mk/rte.hostapp.mk
