# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_sw_event.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_eventdev -lrte_kvargs -lrte_ring
LDLIBS += -lrte_mempool -lrte_mbuf
LDLIBS += -lrte_bus_vdev

# versioning export map
EXPORT_MAP := rte_pmd_sw_event_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_scheduler.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_xstats.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_selftest.c

# export include files
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
