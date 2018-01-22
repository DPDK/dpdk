# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_eventdev.a

# library version
LIBABIVER := 3

# build flags
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_ring -lrte_ethdev -lrte_hash

# library source files
SRCS-y += rte_eventdev.c
SRCS-y += rte_event_ring.c
SRCS-y += rte_event_eth_rx_adapter.c

# export include files
SYMLINK-y-include += rte_eventdev.h
SYMLINK-y-include += rte_eventdev_pmd.h
SYMLINK-y-include += rte_eventdev_pmd_pci.h
SYMLINK-y-include += rte_eventdev_pmd_vdev.h
SYMLINK-y-include += rte_event_ring.h
SYMLINK-y-include += rte_event_eth_rx_adapter.h

# versioning export map
EXPORT_MAP := rte_eventdev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
