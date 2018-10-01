# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_port.a
ifeq ($(CONFIG_RTE_PORT_PCAP),y)
LDLIBS += -lpcap
endif
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_ethdev
LDLIBS += -lrte_ip_frag -lrte_sched -lrte_cryptodev
ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
LDLIBS += -lrte_kni
endif

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_port_version.map

LIBABIVER := 3

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_ring.c
ifeq ($(CONFIG_RTE_LIBRTE_IP_FRAG),y)
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_frag.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_ras.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_sched.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_fd.c
ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_kni.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_source_sink.c
SRCS-$(CONFIG_RTE_LIBRTE_PORT) += rte_port_sym_crypto.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_ethdev.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_ring.h
ifeq ($(CONFIG_RTE_LIBRTE_IP_FRAG),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_frag.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_ras.h
endif
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_sched.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_fd.h
ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_kni.h
endif
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_source_sink.h
SYMLINK-$(CONFIG_RTE_LIBRTE_PORT)-include += rte_port_sym_crypto.h

include $(RTE_SDK)/mk/rte.lib.mk
