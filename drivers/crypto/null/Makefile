# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk


# library name
LIB = librte_pmd_null_crypto.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_null_crypto_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO) += null_crypto_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO) += null_crypto_pmd_ops.c

# export include files
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
