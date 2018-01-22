# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 NXP

include $(RTE_SDK)/mk/rte.vars.mk
RTE_BUS_DPAA=$(RTE_SDK)/drivers/bus/dpaa

#
# library name
#
LIB = librte_bus_dpaa.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS := -I$(SRCDIR) $(CFLAGS)
CFLAGS += -O3 $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -Wno-cast-qual
CFLAGS += -D _GNU_SOURCE
CFLAGS += -I$(RTE_BUS_DPAA)/
CFLAGS += -I$(RTE_BUS_DPAA)/include
CFLAGS += -I$(RTE_BUS_DPAA)/base/qbman
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include

# versioning export map
EXPORT_MAP := rte_bus_dpaa_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += \
	dpaa_bus.c

SRCS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += \
	base/fman/fman.c \
	base/fman/fman_hw.c \
	base/fman/of.c \
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

include $(RTE_SDK)/mk/rte.lib.mk
