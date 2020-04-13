# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 NXP

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_dpaa.a

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa/include/
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa
CFLAGS += -I$(RTE_SDK)/lib/librte_mempool

# versioning export map
EXPORT_MAP := rte_mempool_dpaa_version.map

# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_DPAA_MEMPOOL) += dpaa_mempool.c

LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_eal -lrte_mempool -lrte_ring
LDLIBS += -lrte_common_dpaax

include $(RTE_SDK)/mk/rte.lib.mk
