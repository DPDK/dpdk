# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_ring.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool -lrte_ring

EXPORT_MAP := rte_mempool_ring_version.map

SRCS-$(CONFIG_RTE_DRIVER_MEMPOOL_RING) += rte_mempool_ring.c

include $(RTE_SDK)/mk/rte.lib.mk
