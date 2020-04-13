# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_thunderx_nicvf.a

CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lm
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_thunderx_version.map

OBJS_BASE_DRIVER=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(OBJS_BASE_DRIVER), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += nicvf_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += nicvf_hw.c
SRCS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += nicvf_mbox.c
SRCS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += nicvf_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += nicvf_bsvf.c
SRCS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += nicvf_svf.c

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_nicvf_rxtx.o += -fno-prefetch-loop-arrays
endif
CFLAGS_nicvf_rxtx.o += -Ofast

include $(RTE_SDK)/mk/rte.lib.mk
