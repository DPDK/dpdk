# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_e1000.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_e1000_version.map

ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
#
# CFLAGS for icc
#
CFLAGS_BASE_DRIVER  = -diag-disable 177 -diag-disable 181
CFLAGS_BASE_DRIVER += -diag-disable 869 -diag-disable 2259
else
#
# CFLAGS for gcc/clang
#
CFLAGS_BASE_DRIVER = -Wno-uninitialized -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-unused-variable
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -ge 60 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-misleading-indentation
ifeq ($(shell test $(GCC_VERSION) -ge 70 && echo 1), 1)
CFLAGS_BASE_DRIVER += -Wno-implicit-fallthrough
endif
endif
endif
endif

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
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_base.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_80003es2lan.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82540.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82541.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82542.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82543.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82571.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_82575.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_i210.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_api.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_ich8lan.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_logs.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_manage.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_mbx.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_nvm.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_osdep.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_pf.c
SRCS-$(CONFIG_RTE_LIBRTE_IGB_PMD) += igb_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_EM_PMD) += em_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_EM_PMD) += em_rxtx.c

include $(RTE_SDK)/mk/rte.lib.mk
