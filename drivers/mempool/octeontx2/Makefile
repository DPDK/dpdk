# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_octeontx2.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/bus/pci
CFLAGS += -O3

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

EXPORT_MAP := rte_mempool_octeontx2_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX2_MEMPOOL) += \
	otx2_mempool_ops.c	\
	otx2_mempool.c 		\
	otx2_mempool_irq.c	\
	otx2_mempool_debug.c

LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf
LDLIBS += -lrte_common_octeontx2 -lrte_kvargs -lrte_bus_pci

include $(RTE_SDK)/mk/rte.lib.mk
