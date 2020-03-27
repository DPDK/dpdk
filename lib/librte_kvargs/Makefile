# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014 6WIND S.A.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_kvargs.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/include

EXPORT_MAP := rte_kvargs_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_KVARGS) := rte_kvargs.c

# install includes
INCS := rte_kvargs.h
SYMLINK-$(CONFIG_RTE_LIBRTE_KVARGS)-include := $(INCS)

include $(RTE_SDK)/mk/rte.lib.mk
