# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(LIBSSO_SNOW3G_PATH),)
$(error "Please define LIBSSO_SNOW3G_PATH environment variable")
endif
endif

# library name
LIB = librte_pmd_snow3g.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_snow3g_version.map

# external library dependencies
CFLAGS += -I$(LIBSSO_SNOW3G_PATH)
CFLAGS += -I$(LIBSSO_SNOW3G_PATH)/include
CFLAGS += -I$(LIBSSO_SNOW3G_PATH)/build
LDLIBS += -L$(LIBSSO_SNOW3G_PATH)/build -lsso_snow3g
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_bus_vdev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G) += rte_snow3g_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G) += rte_snow3g_pmd_ops.c

include $(RTE_SDK)/mk/rte.lib.mk
