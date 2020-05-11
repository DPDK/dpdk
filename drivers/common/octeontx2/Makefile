# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_common_octeontx2.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/bus/pci

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

EXPORT_MAP := rte_common_octeontx2_version.map

#
# all source are stored in SRCS-y
#
SRCS-y += otx2_dev.c
SRCS-y += otx2_irq.c
SRCS-y += otx2_mbox.c
SRCS-y += otx2_common.c
SRCS-y += otx2_sec_idev.c

LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev -lrte_kvargs

include $(RTE_SDK)/mk/rte.lib.mk
