# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_softnic.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_pipeline -lrte_port -lrte_table
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_sched
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

EXPORT_MAP := rte_pmd_softnic_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_mempool.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_swq.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_link.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_tm.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_tap.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_action.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_pipeline.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_thread.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_cli.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_meter.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += rte_eth_softnic_cryptodev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += parser.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC) += conn.c

#
# Export include files
#
SYMLINK-y-include += rte_eth_softnic.h

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
$(info Softnic PMD can only operate in a linux environment, \
please change the definition of the RTE_TARGET environment variable)
all:
clean:
else

include $(RTE_SDK)/mk/rte.lib.mk

endif
