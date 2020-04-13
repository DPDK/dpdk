# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_octeontx.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx/

EXPORT_MAP := rte_mempool_octeontx_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL) += octeontx_fpavf.c
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL) += rte_mempool_octeontx.c

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_rte_mempool_octeontx.o += -fno-prefetch-loop-arrays

ifeq ($(shell test $(GCC_VERSION) -ge 46 && echo 1), 1)
CFLAGS_rte_mempool_octeontx.o += -Ofast
else
CFLAGS_rte_mempool_octeontx.o += -O3 -ffast-math
endif

else
CFLAGS_rte_mempool_octeontx.o += -Ofast
endif

LDLIBS += -lrte_eal -lrte_mempool -lrte_ring -lrte_mbuf
LDLIBS += -lrte_bus_pci -lrte_common_octeontx

include $(RTE_SDK)/mk/rte.lib.mk
