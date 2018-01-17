# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
# Copyright 2016 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_dpaa2.a

ifeq ($(CONFIG_RTE_LIBRTE_DPAA2_DEBUG_INIT),y)
CFLAGS += -O0 -g
CFLAGS += "-Wno-error"
else
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
endif

CFLAGS += -I$(RTE_SDK)/drivers/net/dpaa2
CFLAGS += -I$(RTE_SDK)/drivers/net/dpaa2/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/portal
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa2
CFLAGS += -I$(RTE_SDK)/drivers/event/dpaa2
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal

# versioning export map
EXPORT_MAP := rte_pmd_dpaa2_version.map

# library version
LIBABIVER := 1

SRCS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD) += base/dpaa2_hw_dpni.c
SRCS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD) += dpaa2_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD) += dpaa2_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD) += mc/dpni.c
SRCS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD) += mc/dpkg.c

LDLIBS += -lrte_bus_fslmc
LDLIBS += -lrte_mempool_dpaa2
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs

include $(RTE_SDK)/mk/rte.lib.mk
