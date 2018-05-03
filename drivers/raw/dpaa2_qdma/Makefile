# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_dpaa2_qdma.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include

LDLIBS += -lrte_bus_fslmc
LDLIBS += -lrte_eal
LDLIBS += -lrte_mempool
LDLIBS += -lrte_mempool_dpaa2
LDLIBS += -lrte_rawdev
LDLIBS += -lrte_ring

EXPORT_MAP := rte_pmd_dpaa2_qdma_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV) += dpaa2_qdma.c

SYMLINK-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV)-include += rte_pmd_dpaa2_qdma.h

include $(RTE_SDK)/mk/rte.lib.mk
