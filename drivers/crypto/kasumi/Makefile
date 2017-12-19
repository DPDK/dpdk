# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(LIBSSO_KASUMI_PATH),)
$(error "Please define LIBSSO_KASUMI_PATH environment variable")
endif
endif

# library name
LIB = librte_pmd_kasumi.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_kasumi_version.map

# external library dependencies
CFLAGS += -I$(LIBSSO_KASUMI_PATH)
CFLAGS += -I$(LIBSSO_KASUMI_PATH)/include
CFLAGS += -I$(LIBSSO_KASUMI_PATH)/build
LDLIBS += -L$(LIBSSO_KASUMI_PATH)/build -lsso_kasumi
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI) += rte_kasumi_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI) += rte_kasumi_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
