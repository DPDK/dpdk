# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_common_dpaax.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# versioning export map
EXPORT_MAP := rte_common_dpaax_version.map

# library version
LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-y += dpaax_iova_table.c

LDLIBS += -lrte_eal

SYMLINK-y-include += dpaax_iova_table.h

include $(RTE_SDK)/mk/rte.lib.mk