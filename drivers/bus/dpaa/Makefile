# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 NXP

include $(RTE_SDK)/mk/rte.vars.mk
RTE_BUS_DPAA=$(RTE_SDK)/drivers/bus/dpaa

#
# library name
#
LIB = librte_bus_dpaa.a

CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -Wno-cast-qual
CFLAGS += -I$(RTE_BUS_DPAA)/
CFLAGS += -I$(RTE_BUS_DPAA)/include
CFLAGS += -I$(RTE_BUS_DPAA)/base/qbman
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/include

# versioning export map
EXPORT_MAP := rte_bus_dpaa_version.map

# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += \
	dpaa_bus.c

SRCS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += \
	base/fman/fman.c \
	base/fman/fman_hw.c \
	base/fman/netcfg_layer.c \
	base/qbman/process.c \
	base/qbman/bman.c \
	base/qbman/bman_driver.c \
	base/qbman/qman.c \
	base/qbman/qman_driver.c \
	base/qbman/dpaa_alloc.c \
	base/qbman/dpaa_sys.c

# Link Pthread
LDLIBS += -lpthread
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev
LDLIBS += -lrte_common_dpaax

include $(RTE_SDK)/mk/rte.lib.mk
