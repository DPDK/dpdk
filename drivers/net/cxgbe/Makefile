# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014-2018 Chelsio Communications.
# All rights reserved.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_cxgbe.a

CFLAGS += -I$(SRCDIR)/base/
CFLAGS += -I$(SRCDIR)
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_cxgbe_version.map

LIBABIVER := 1

#
# CFLAGS for gcc/clang
#
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -ge 44 && echo 1), 1)
CFLAGS     += -Wno-deprecated
endif
endif

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

#
# Add extra flags for base driver files (also known as shared code)
# to disable warnings in them
#
BASE_DRIVER_OBJS=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(BASE_DRIVER_OBJS), $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

VPATH += $(SRCDIR)/base


#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += cxgbe_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += cxgbevf_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += cxgbe_main.c
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += cxgbevf_main.c
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += sge.c
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += t4_hw.c
SRCS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += t4vf_hw.c

include $(RTE_SDK)/mk/rte.lib.mk
