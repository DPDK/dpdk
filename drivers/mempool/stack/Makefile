# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_stack.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

# Headers
CFLAGS += -I$(RTE_SDK)/lib/librte_mempool
LDLIBS += -lrte_eal -lrte_mempool -lrte_stack

EXPORT_MAP := rte_mempool_stack_version.map

LIBABIVER := 1

SRCS-$(CONFIG_RTE_DRIVER_MEMPOOL_STACK) += rte_mempool_stack.c

include $(RTE_SDK)/mk/rte.lib.mk
