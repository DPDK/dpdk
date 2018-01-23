# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_opdl_event.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
# for older GCC versions, allow us to initialize an event using
# designated initializers.
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -le 50 && echo 1), 1)
CFLAGS += -Wno-missing-field-initializers
endif
endif

LDLIBS += -lrte_eal -lrte_eventdev -lrte_kvargs
LDLIBS += -lrte_bus_vdev -lrte_mbuf -lrte_mempool

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_evdev_opdl_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_ring.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_evdev_init.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_evdev_xstats.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += opdl_test.c

# export include files
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
