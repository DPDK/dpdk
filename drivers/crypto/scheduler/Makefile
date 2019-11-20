# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_crypto_scheduler.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev -lrte_kvargs -lrte_reorder
LDLIBS += -lrte_bus_vdev

# versioning export map
EXPORT_MAP := rte_pmd_crypto_scheduler_version.map

#
# Export include files
#
SYMLINK-y-include += rte_cryptodev_scheduler_operations.h
SYMLINK-y-include += rte_cryptodev_scheduler.h

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler_pmd_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += rte_cryptodev_scheduler.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler_roundrobin.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler_pkt_size_distr.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler_failover.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler_multicore.c

include $(RTE_SDK)/mk/rte.lib.mk
