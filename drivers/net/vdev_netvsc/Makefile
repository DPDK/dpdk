# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 6WIND S.A.
# Copyright 2017 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Properties of the generated library.
LIB = librte_pmd_vdev_netvsc.a
EXPORT_MAP := rte_pmd_vdev_netvsc_version.map

# Additional compilation flags.
CFLAGS += -O3
CFLAGS += -g
CFLAGS += -Wall -Wextra
CFLAGS += -D_XOPEN_SOURCE=600
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += $(WERROR_FLAGS)

# Dependencies.
LDLIBS += -lrte_bus_vdev
LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_net

# Source files.
SRCS-$(CONFIG_RTE_LIBRTE_VDEV_NETVSC_PMD) += vdev_netvsc.c

include $(RTE_SDK)/mk/rte.lib.mk
