# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_dpaa_sec.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa/include
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa/base/qbman
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa_sec/
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax/caamflib/
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/include
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev

# versioning export map
EXPORT_MAP := rte_pmd_dpaa_sec_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA_SEC) += dpaa_sec.c

# library dependencies

LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_mempool_dpaa
LDLIBS += -lrte_common_dpaax

include $(RTE_SDK)/mk/rte.lib.mk
