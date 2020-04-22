# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_eventdev.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
CFLAGS += -DLINUX
else
CFLAGS += -DBSD
endif
LDLIBS += -lrte_eal -lrte_ring -lrte_ethdev -lrte_hash -lrte_mempool -lrte_timer
LDLIBS += -lrte_mbuf -lrte_cryptodev -lpthread

# library source files
SRCS-y += rte_eventdev.c
SRCS-y += rte_event_ring.c
SRCS-y += eventdev_trace_points.c
SRCS-y += rte_event_eth_rx_adapter.c
SRCS-y += rte_event_timer_adapter.c
SRCS-y += rte_event_crypto_adapter.c
SRCS-y += rte_event_eth_tx_adapter.c

# export include files
SYMLINK-y-include += rte_eventdev.h
SYMLINK-y-include += rte_eventdev_pmd.h
SYMLINK-y-include += rte_eventdev_pmd_pci.h
SYMLINK-y-include += rte_eventdev_pmd_vdev.h
SYMLINK-y-include += rte_eventdev_trace.h
SYMLINK-y-include += rte_eventdev_trace_fp.h
SYMLINK-y-include += rte_event_ring.h
SYMLINK-y-include += rte_event_eth_rx_adapter.h
SYMLINK-y-include += rte_event_timer_adapter.h
SYMLINK-y-include += rte_event_timer_adapter_pmd.h
SYMLINK-y-include += rte_event_crypto_adapter.h
SYMLINK-y-include += rte_event_eth_tx_adapter.h

# versioning export map
EXPORT_MAP := rte_eventdev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
