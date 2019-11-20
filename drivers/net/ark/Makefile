# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2015-2018 Atomic Rules LLC

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_ark.a

CFLAGS += -O3 -I./
CFLAGS += $(WERROR_FLAGS) -Werror

EXPORT_MAP := rte_pmd_ark_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_ddm.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_ethdev_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_ethdev_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_mpu.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_pktchkr.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_pktdir.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_pktgen.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_rqp.c
SRCS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark_udm.c

# this lib depends upon:
LDLIBS += -lpthread
ifdef CONFIG_RTE_EXEC_ENV_LINUX
LDLIBS += -ldl
endif
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

include $(RTE_SDK)/mk/rte.lib.mk
