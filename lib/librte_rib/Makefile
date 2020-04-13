# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_rib.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_mempool

EXPORT_MAP := rte_rib_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_RIB) := rte_rib.c rte_rib6.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_RIB)-include := rte_rib.h rte_rib6.h

include $(RTE_SDK)/mk/rte.lib.mk
