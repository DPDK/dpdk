# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_pfe.a

CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax

EXPORT_MAP := rte_pmd_pfe_version.map
LIBABIVER := 1

# Interfaces with DPDK
SRCS-$(CONFIG_RTE_LIBRTE_PFE_PMD) += pfe_ethdev.c

LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_common_dpaax
LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev -lrte_kvargs

include $(RTE_SDK)/mk/rte.lib.mk
