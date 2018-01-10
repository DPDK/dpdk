# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
# Copyright 2007 Nuova Systems, Inc.  All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_enic.a

EXPORT_MAP := rte_pmd_enic_version.map

LIBABIVER := 1

CFLAGS += -I$(SRCDIR)/base/
CFLAGS += -I$(SRCDIR)
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-strict-aliasing
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci

VPATH += $(SRCDIR)/src

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_main.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_clsf.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_res.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_cq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_wq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_rq.c
SRCS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += base/vnic_rss.c

include $(RTE_SDK)/mk/rte.lib.mk
