# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_nitrox.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# versioning export map
EXPORT_MAP := rte_pmd_nitrox_version.map

# external library dependencies
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_pci -lrte_bus_pci
LDLIBS += -lrte_cryptodev

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_device.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_hal.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_logs.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_sym.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_sym_capabilities.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_sym_reqmgr.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_NITROX) += nitrox_qp.c

include $(RTE_SDK)/mk/rte.lib.mk
