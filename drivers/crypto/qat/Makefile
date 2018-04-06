# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_qat.a

# library version
LIBABIVER := 1

# build flags
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -O3

# external library include paths
CFLAGS += -I$(SRCDIR)/qat_adf
LDLIBS += -lcrypto
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_pci -lrte_bus_pci

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_QAT) += qat_crypto.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_QAT) += qat_qp.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_QAT) += qat_adf/qat_algs_build_desc.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_QAT) += rte_qat_cryptodev.c

# export include files
SYMLINK-y-include +=

# versioning export map
EXPORT_MAP := rte_pmd_qat_version.map

include $(RTE_SDK)/mk/rte.lib.mk
