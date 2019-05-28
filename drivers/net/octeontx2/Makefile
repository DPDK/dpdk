# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_octeontx2.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/net/octeontx2
CFLAGS += -O3

EXPORT_MAP := rte_pmd_octeontx2_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX2_PMD) += \
	otx2_ethdev.c

LDLIBS += -lrte_common_octeontx2 -lrte_mempool_octeontx2

include $(RTE_SDK)/mk/rte.lib.mk
