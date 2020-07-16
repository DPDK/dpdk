# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_octeontx2_crypto.a

# build flags
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lrte_eal -lrte_ethdev -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev -lrte_security
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_common_cpt -lrte_common_octeontx2

VPATH += $(RTE_SDK)/drivers/crypto/octeontx2

CFLAGS += -O3
CFLAGS += -I$(RTE_SDK)/drivers/common/cpt
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/crypto/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/net/octeontx2

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

# PMD code
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += otx2_cryptodev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += otx2_cryptodev_capabilities.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += otx2_cryptodev_hw_access.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += otx2_cryptodev_mbox.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += otx2_cryptodev_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO) += otx2_cryptodev_sec.c

# export include files
SYMLINK-y-include +=

# versioning export map
EXPORT_MAP := rte_pmd_octeontx2_crypto_version.map

include $(RTE_SDK)/mk/rte.lib.mk
