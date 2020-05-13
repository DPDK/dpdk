# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_common_dpaax.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-pointer-arith
CFLAGS += -Wno-cast-qual

CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax
CFLAGS += -I$(RTE_SDK)/drivers/common/dpaax/caamflib

# versioning export map
EXPORT_MAP := rte_common_dpaax_version.map

#
# all source are stored in SRCS-y
#
SRCS-y += dpaax_iova_table.c dpaa_of.c caamflib.c

LDLIBS += -lrte_eal

include $(RTE_SDK)/mk/rte.lib.mk
