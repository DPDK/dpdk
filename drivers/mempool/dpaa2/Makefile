# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_dpaa2.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include

# versioning export map
EXPORT_MAP := rte_mempool_dpaa2_version.map

# Lbrary version
LIBABIVER := 2

# depends on fslmc bus which uses experimental API
CFLAGS += -DALLOW_EXPERIMENTAL_API

# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_DPAA2_MEMPOOL) += dpaa2_hw_mempool.c

LDLIBS += -lrte_bus_fslmc
LDLIBS += -lrte_eal -lrte_mempool -lrte_ring
LDLIBS += -lrte_common_dpaax

SYMLINK-$(CONFIG_RTE_LIBRTE_DPAA2_MEMPOOL)-include := rte_dpaa2_mempool.h

include $(RTE_SDK)/mk/rte.lib.mk
