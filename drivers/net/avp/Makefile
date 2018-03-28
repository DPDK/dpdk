# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2013-2017, Wind River Systems, Inc.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_avp.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_avp_version.map

LIBABIVER := 1

# install public header files to enable compilation of the hypervisor level
# dpdk application
SYMLINK-$(CONFIG_RTE_LIBRTE_AVP_PMD)-include += rte_avp_common.h
SYMLINK-$(CONFIG_RTE_LIBRTE_AVP_PMD)-include += rte_avp_fifo.h

#
# all source files are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_AVP_PMD) += avp_ethdev.c

include $(RTE_SDK)/mk/rte.lib.mk
