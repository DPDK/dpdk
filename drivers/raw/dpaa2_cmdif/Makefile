# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_dpaa2_cmdif.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include

LDLIBS += -lrte_bus_fslmc
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_eal
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_mempool_dpaa2
LDLIBS += -lrte_rawdev

EXPORT_MAP := rte_pmd_dpaa2_cmdif_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV) += dpaa2_cmdif.c

SYMLINK-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV)-include += rte_pmd_dpaa2_cmdif.h

include $(RTE_SDK)/mk/rte.lib.mk
