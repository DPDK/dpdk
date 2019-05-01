# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Arm Limited

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_rcu.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_eal

EXPORT_MAP := rte_rcu_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_RCU) := rte_rcu_qsbr.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_RCU)-include := rte_rcu_qsbr.h

include $(RTE_SDK)/mk/rte.lib.mk
