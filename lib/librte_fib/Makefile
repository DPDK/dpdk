# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_fib.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_rib

EXPORT_MAP := rte_fib_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_FIB) := rte_fib.c rte_fib6.c dir24_8.c trie.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_FIB)-include := rte_fib.h rte_fib6.h

include $(RTE_SDK)/mk/rte.lib.mk
