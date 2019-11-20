# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_rawdev_octeontx2_dma.a

CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2/
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2/
CFLAGS += -I$(RTE_SDK)/drivers/raw/octeontx2_dma/
LDLIBS += -lrte_eal -lrte_rawdev -lrte_bus_pci -lrte_mbuf
LDLIBS += -lrte_common_octeontx2 -lrte_mempool

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

EXPORT_MAP := rte_rawdev_octeontx2_dma_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_DMA_RAWDEV) += otx2_dpi_rawdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_DMA_RAWDEV) += otx2_dpi_msg.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_DMA_RAWDEV) += otx2_dpi_test.c

include $(RTE_SDK)/mk/rte.lib.mk
