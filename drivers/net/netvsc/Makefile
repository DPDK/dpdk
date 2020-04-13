# SPDX-License-Identifier: BSD-3-Clause

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_pmd_netvsc.a

CFLAGS += -O3 $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_netvsc_version.map

SRCS-$(CONFIG_RTE_LIBRTE_NETVSC_PMD) += hn_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_NETVSC_PMD) += hn_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_NETVSC_PMD) += hn_rndis.c
SRCS-$(CONFIG_RTE_LIBRTE_NETVSC_PMD) += hn_nvs.c
SRCS-$(CONFIG_RTE_LIBRTE_NETVSC_PMD) += hn_vf.c

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_vmbus

include $(RTE_SDK)/mk/rte.lib.mk
