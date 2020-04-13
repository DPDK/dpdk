# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 6WIND S.A.
# Copyright 2017 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name
LIB = librte_pmd_failsafe.a

EXPORT_MAP := rte_pmd_failsafe_version.map

# Sources are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_args.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_eal.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_ether.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE) += failsafe_intr.c
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
CFLAGS += -DLINUX
else
CFLAGS += -DBSD
endif

# No exported include files

# Basic CFLAGS:
CFLAGS += -std=gnu99 -Wextra
CFLAGS += -O3
CFLAGS += -I.
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_XOPEN_SOURCE=700
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-strict-prototypes
CFLAGS += -pedantic
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_vdev
LDLIBS += -lpthread

include $(RTE_SDK)/mk/rte.lib.mk
