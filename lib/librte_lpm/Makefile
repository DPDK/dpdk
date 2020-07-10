# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_lpm.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_hash -lrte_rcu

EXPORT_MAP := rte_lpm_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_LPM) := rte_lpm.c rte_lpm6.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_LPM)-include := rte_lpm.h rte_lpm6.h

ifneq ($(filter y,$(CONFIG_RTE_ARCH_ARM) $(CONFIG_RTE_ARCH_ARM64)),)
SYMLINK-$(CONFIG_RTE_LIBRTE_LPM)-include += rte_lpm_neon.h
else ifeq ($(CONFIG_RTE_ARCH_X86),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_LPM)-include += rte_lpm_sse.h
else ifeq ($(CONFIG_RTE_ARCH_PPC_64),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_LPM)-include += rte_lpm_altivec.h
endif

include $(RTE_SDK)/mk/rte.lib.mk
