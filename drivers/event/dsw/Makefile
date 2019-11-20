# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Ericsson AB

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_pmd_dsw_event.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
ifneq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -Wno-format-nonliteral
endif

LDLIBS += -lrte_eal
LDLIBS += -lrte_mbuf
LDLIBS += -lrte_mempool
LDLIBS += -lrte_ring
LDLIBS += -lrte_eventdev
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_dsw_event_version.map

SRCS-$(CONFIG_RTE_LIBRTE_PMD_DSW_EVENTDEV) += \
	dsw_evdev.c dsw_event.c dsw_xstats.c

include $(RTE_SDK)/mk/rte.lib.mk
