# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_cfgfile.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(SRCDIR)/../librte_eal/common/include

EXPORT_MAP := rte_cfgfile_version.map

LIBABIVER := 2

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_CFGFILE) += rte_cfgfile.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_CFGFILE)-include += rte_cfgfile.h

include $(RTE_SDK)/mk/rte.lib.mk
