# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017,2019 NXP
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
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
LDLIBS += -lrte_eal -lrte_eventdev
LDLIBS += -lrte_common_dpaax
LDLIBS += -lrte_bus_fslmc -lrte_mempool_dpaa2 -lrte_pmd_dpaa2
LDLIBS += -lrte_bus_vdev -lrte_mempool -lrte_mbuf -lrte_ethdev
CFLAGS += -I$(RTE_SDK)/drivers/net/dpaa2
CFLAGS += -I$(RTE_SDK)/drivers/net/dpaa2/mc

LDLIBS += -lrte_pmd_dpaa2_sec
CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec

# versioning export map
EXPORT_MAP := rte_pmd_dpaa2_event_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += dpaa2_hw_dpcon.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += dpaa2_eventdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += dpaa2_eventdev_selftest.c

include $(RTE_SDK)/mk/rte.lib.mk
