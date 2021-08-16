# SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
# Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ionic.a

# Required to use rte_eth_dev_create and rte_eth_dev_destroy
CFLAGS += -DALLOW_EXPERIMENTAL_API

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/ionic

EXPORT_MAP := rte_pmd_ionic_version.map

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_pci -lrte_bus_vdev
LDLIBS += -lrte_common_ionic

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_mac_api.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_rx_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_rxtx_simple.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_rxtx_sg.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_lif.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_main.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_dev_pci.c
SRCS-$(CONFIG_RTE_LIBRTE_IONIC_PMD) += ionic_dev_vdev.c

ifeq ($(CONFIG_RTE_LIBRTE_IONIC_PMD_EMBEDDED),y)
CFLAGS += -DIONIC_EMBEDDED
endif

include $(RTE_SDK)/mk/rte.lib.mk
