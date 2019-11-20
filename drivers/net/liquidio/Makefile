# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_lio.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)/base -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_liquidio_version.map

VPATH += $(RTE_SDK)/drivers/net/liquidio/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_LIO_PMD) += lio_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_LIO_PMD) += lio_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_LIO_PMD) += lio_23xx_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_LIO_PMD) += lio_mbox.c

include $(RTE_SDK)/mk/rte.lib.mk
