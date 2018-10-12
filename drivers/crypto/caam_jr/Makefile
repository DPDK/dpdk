# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_caam_jr.a

# build flags
CFLAGS += -DALLOW_EXPERIMENTAL_API

CFLAGS += -D _GNU_SOURCE

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa/include
CFLAGS += -I$(RTE_SDK)/drivers/crypto/caam_jr
#sharing the hw flib headers from dpaa2_sec pmd
CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec/
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal

# versioning export map
EXPORT_MAP := rte_pmd_caam_jr_version.map

# library version
LIBABIVER := 1

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CAAM_JR) += caam_jr.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CAAM_JR) += caam_jr_capabilities.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CAAM_JR) += caam_jr_hw.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CAAM_JR) += caam_jr_uio.c
# library dependencies

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_bus_vdev

include $(RTE_SDK)/mk/rte.lib.mk
