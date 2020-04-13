# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_openssl.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# versioning export map
EXPORT_MAP := rte_pmd_openssl_version.map

# external library dependencies
LDLIBS += -lcrypto
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += rte_openssl_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += rte_openssl_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
