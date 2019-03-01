# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_octeontx_crypto.a

# library version
LIBABIVER := 1

# build flags
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_common_cpt

VPATH += $(RTE_SDK)/drivers/crypto/octeontx

CFLAGS += -O3
CFLAGS += -I$(RTE_SDK)/drivers/common/cpt
CFLAGS += -DALLOW_EXPERIMENTAL_API

# PMD code
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO) += otx_cryptodev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO) += otx_cryptodev_capabilities.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO) += otx_cryptodev_hw_access.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO) += otx_cryptodev_mbox.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO) += otx_cryptodev_ops.c

# export include files
SYMLINK-y-include +=

# versioning export map
EXPORT_MAP := rte_pmd_octeontx_crypto_version.map

include $(RTE_SDK)/mk/rte.lib.mk
