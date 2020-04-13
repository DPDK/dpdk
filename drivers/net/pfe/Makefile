# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_pfe.a

CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -I$(RTE_SDK)/drivers/net/pfe/base/
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax

EXPORT_MAP := rte_pmd_pfe_version.map

# Interfaces with DPDK
SRCS-$(CONFIG_RTE_LIBRTE_PFE_PMD) += pfe_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PFE_PMD) += pfe_hal.c
SRCS-$(CONFIG_RTE_LIBRTE_PFE_PMD) += pfe_hif_lib.c
SRCS-$(CONFIG_RTE_LIBRTE_PFE_PMD) += pfe_hif.c

LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_common_dpaax
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_ethdev -lrte_kvargs

include $(RTE_SDK)/mk/rte.lib.mk
