# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Marvell International Ltd.
# Copyright(c) 2017 Semihalf.
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),config)
ifeq ($(LIBMUSDK_PATH),)
$(error "Please define LIBMUSDK_PATH environment variable")
endif
endif
endif

# library name
LIB = librte_pmd_mvsam_crypto.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/mvep
CFLAGS += -I$(LIBMUSDK_PATH)/include
CFLAGS += -DMVCONF_TYPES_PUBLIC
CFLAGS += -DMVCONF_DMA_PHYS_ADDR_T_PUBLIC

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_mvsam_version.map

# external library dependencies
LDLIBS += -L$(LIBMUSDK_PATH)/lib -lmusdk
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_kvargs
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev -lrte_common_mvep

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_MVSAM_CRYPTO) += rte_mrvl_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_MVSAM_CRYPTO) += rte_mrvl_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
