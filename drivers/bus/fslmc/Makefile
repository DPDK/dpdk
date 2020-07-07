# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_bus_fslmc.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common
LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev
LDLIBS += -lrte_common_dpaax

# versioning export map
EXPORT_MAP := rte_bus_fslmc_version.map

SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += \
        qbman/qbman_portal.c \
        qbman/qbman_debug.c

SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += \
	mc/dpmng.c \
	mc/dpbp.c \
	mc/dpio.c \
	mc/mc_sys.c \
	mc/dpcon.c \
	mc/dpci.c \
	mc/dpdmai.c

SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += portal/dpaa2_hw_dpio.c
SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += portal/dpaa2_hw_dpbp.c
SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += portal/dpaa2_hw_dpci.c
SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += fslmc_vfio.c
SRCS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += fslmc_bus.c

include $(RTE_SDK)/mk/rte.lib.mk
