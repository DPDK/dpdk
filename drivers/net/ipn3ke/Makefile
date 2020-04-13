# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ipn3ke.a

#
# Add the experimenatal APIs called from this PMD
#  rte_eth_switch_domain_alloc()
#  rte_eth_dev_create()
#  rte_eth_dev_destroy()
#  rte_eth_switch_domain_free()
#
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/bus/ifpga
CFLAGS += -I$(RTE_SDK)/drivers/raw/ifpga
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_ifpga
LDLIBS += -lrte_bus_vdev
LDLIBS += -lpthread

EXPORT_MAP := rte_pmd_ipn3ke_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_IPN3KE_PMD) += ipn3ke_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IPN3KE_PMD) += ipn3ke_representor.c
SRCS-$(CONFIG_RTE_LIBRTE_IPN3KE_PMD) += ipn3ke_tm.c
SRCS-$(CONFIG_RTE_LIBRTE_IPN3KE_PMD) += ipn3ke_flow.c

include $(RTE_SDK)/mk/rte.lib.mk
