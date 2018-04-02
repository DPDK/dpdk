# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
# Copyright 2016 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(CONFIG_RTE_LIBRTE_SECURITY),y)
$(error "RTE_LIBRTE_SECURITY is required to build RTE_LIBRTE_PMD_DPAA2_SEC")
endif
endif

#
# library name
#
LIB = librte_pmd_dpaa2_sec.a

# build flags
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -D _GNU_SOURCE

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -gt 70 && echo 1), 1)
CFLAGS += -Wno-implicit-fallthrough
endif
endif

CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec/
CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/portal
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa2/
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal

# versioning export map
EXPORT_MAP := rte_pmd_dpaa2_sec_version.map

# library version
LIBABIVER := 1

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += dpaa2_sec_dpseci.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += mc/dpseci.c

LDLIBS += -lrte_bus_fslmc
LDLIBS += -lrte_mempool_dpaa2
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev

include $(RTE_SDK)/mk/rte.lib.mk
