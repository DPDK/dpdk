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

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
endif

EXPORT_MAP := rte_common_octeontx2_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-y += otx2_mbox.c
SRCS-y += otx2_common.c

LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev

include $(RTE_SDK)/mk/rte.lib.mk
