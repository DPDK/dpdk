# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# module name and path
#
MODULE = igb_uio
MODULE_PATH = drivers/net/igb_uio

#
# CFLAGS
#
MODULE_CFLAGS += -I$(SRCDIR) --param max-inline-insns-single=100
MODULE_CFLAGS += -I$(RTE_OUTPUT)/include
MODULE_CFLAGS += -Winline -Wall -Werror
MODULE_CFLAGS += -include $(RTE_OUTPUT)/include/rte_config.h

#
# all source are stored in SRCS-y
#
SRCS-y := igb_uio.c

include $(RTE_SDK)/mk/rte.module.mk
