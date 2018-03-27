# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_aesni_gcm.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_aesni_gcm_version.map

# external library dependencies
LDLIBS += -lIPSec_MB
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM) += aesni_gcm_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM) += aesni_gcm_pmd_ops.c

# export include files
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
