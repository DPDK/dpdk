# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

# Library name
LIB = librte_rawdev_octeontx2_ep.a

# Build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2/
CFLAGS += -I$(RTE_SDK)/drivers/raw/octeontx2_ep/

LDLIBS += -lrte_eal
LDLIBS += -lrte_rawdev
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_mempool
LDLIBS += -lrte_common_octeontx2

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

EXPORT_MAP := rte_rawdev_octeontx2_ep_version.map

#
# All source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV) += otx2_ep_rawdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV) += otx2_ep_enqdeq.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV) += otx2_ep_test.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV) += otx2_ep_vf.c


include $(RTE_SDK)/mk/rte.lib.mk
