# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Huawei Technologies Co., Ltd

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_hinic.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

ifeq ($(CONFIG_RTE_ARCH_ARM64),y)
CFLAGS += -D__ARM64_NEON__
else ifeq ($(CONFIG_RTE_ARCH_X86_64),y)
CFLAGS += -D__X86_64_SSE__
endif

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_hash
LDLIBS += -lrte_bus_pci
LDLIBS += -lpthread

EXPORT_MAP := rte_pmd_hinic_version.map

#
# CFLAGS for 32-bits platforms
#
ifneq ($(CONFIG_RTE_ARCH_64),y)
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
#
# CFLAGS for icc
#
CFLAGS += -diag-disable 2259
else
#
# CFLAGS for gcc
#
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
endif
endif

VPATH += $(SRCDIR)/base

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_api_cmd.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_cfg.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_cmdq.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_eqs.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_hwdev.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_hwif.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_mgmt.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_niccfg.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_nicio.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_wq.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_mbox.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_flow.c

SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_HINIC_PMD) += hinic_pmd_tx.c

include $(RTE_SDK)/mk/rte.lib.mk
