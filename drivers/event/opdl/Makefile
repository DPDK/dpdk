# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_opdl_event.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lrte_eal -lrte_eventdev -lrte_kvargs
LDLIBS += -lrte_bus_vdev -lrte_mbuf -lrte_mempool

# versioning export map
EXPORT_MAP := rte_pmd_opdl_event_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_ring.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_evdev_init.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_evdev_xstats.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_test.c

# export include files
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
