# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_dpaa2_event.a

CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/portal
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa2
CFLAGS += -I$(RTE_SDK)/drivers/event/dpaa2
LDLIBS += -lrte_eal -lrte_eventdev
LDLIBS += -lrte_bus_fslmc -lrte_mempool_dpaa2 -lrte_pmd_dpaa2
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_common_dpaax
CFLAGS += -I$(RTE_SDK)/drivers/net/dpaa2
CFLAGS += -I$(RTE_SDK)/drivers/net/dpaa2/mc

ifeq ($(CONFIG_RTE_LIBRTE_SECURITY),y)
LDLIBS += -lrte_pmd_dpaa2_sec
CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec
endif

# versioning export map
EXPORT_MAP := rte_pmd_dpaa2_event_version.map

LIBABIVER := 2

# depends on fslmc bus which uses experimental API
CFLAGS += -DALLOW_EXPERIMENTAL_API

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += dpaa2_hw_dpcon.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += dpaa2_eventdev.c

include $(RTE_SDK)/mk/rte.lib.mk
