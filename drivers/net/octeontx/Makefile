# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_octeontx.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx/
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx/

EXPORT_MAP := rte_pmd_octeontx_version.map

OBJS_BASE_DRIVER=$(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c)))
$(foreach obj, $(OBJS_BASE_DRIVER), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += octeontx_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += octeontx_pkovf.c
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += octeontx_pkivf.c
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += octeontx_bgx.c
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += octeontx_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += octeontx_ethdev_ops.c

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_octeontx_rxtx.o += -fno-prefetch-loop-arrays

ifeq ($(shell test $(GCC_VERSION) -ge 46 && echo 1), 1)
CFLAGS_octeontx_rxtx.o += -O3 -Ofast
else
CFLAGS_octeontx_rxtx.o += -O3 -ffast-math
endif

else
CFLAGS_octeontx_rxtx.o += -O3 -Ofast
endif

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_common_octeontx
LDLIBS += -lrte_mempool_octeontx
LDLIBS += -lrte_eventdev
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_bus_vdev

include $(RTE_SDK)/mk/rte.lib.mk
