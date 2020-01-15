# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_common_cpt.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/bus/pci
EXPORT_MAP := rte_common_cpt_version.map

#
# all source are stored in SRCS-y
#
SRCS-y += cpt_fpm_tables.c
SRCS-y += cpt_pmd_ops_helper.c

LDLIBS += -lrte_eal

include $(RTE_SDK)/mk/rte.lib.mk
