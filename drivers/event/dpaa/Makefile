#   SPDX-License-Identifier:        BSD-3-Clause
#   Copyright 2017 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk
RTE_SDK_DPAA=$(RTE_SDK)/drivers/net/dpaa

#
# library name
#
LIB = librte_pmd_dpaa_event.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -I$(RTE_SDK_DPAA)/
CFLAGS += -I$(RTE_SDK_DPAA)/include
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa
CFLAGS += -I$(RTE_SDK)/drivers/bus/dpaa/include/
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal/include

EXPORT_MAP := rte_pmd_dpaa_event_version.map

LIBABIVER := 1

# Interfaces with DPDK
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA_EVENTDEV) += dpaa_eventdev.c

LDLIBS += -lrte_bus_dpaa
LDLIBS += -lrte_mempool_dpaa
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_eventdev -lrte_pmd_dpaa -lrte_bus_vdev
LDLIBS += -lrte_common_dpaax

include $(RTE_SDK)/mk/rte.lib.mk
