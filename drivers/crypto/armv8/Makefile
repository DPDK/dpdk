# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),config)
ifeq ($(ARMV8_CRYPTO_LIB_PATH),)
$(error "Please define ARMV8_CRYPTO_LIB_PATH environment variable")
endif
endif
endif

# library name
LIB = librte_pmd_armv8.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# versioning export map
EXPORT_MAP := rte_pmd_armv8_version.map

# external library dependencies
CFLAGS += -I$(ARMV8_CRYPTO_LIB_PATH)
LDLIBS += -L$(ARMV8_CRYPTO_LIB_PATH) -lAArch64crypto
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO) += rte_armv8_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO) += rte_armv8_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
