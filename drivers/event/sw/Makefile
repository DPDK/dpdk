# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_sw_event.a

# build flags
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
# for older GCC versions, allow us to initialize an event using
# designated initializers.
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -le 50 && echo 1), 1)
CFLAGS += -Wno-missing-field-initializers
endif
endif
LDLIBS += -lrte_eal -lrte_eventdev -lrte_kvargs -lrte_ring
LDLIBS += -lrte_mempool -lrte_mbuf
LDLIBS += -lrte_bus_vdev

# library version
LIBABIVER := 1

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
