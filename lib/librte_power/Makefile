# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_power.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3 -fno-strict-aliasing
LDLIBS += -lrte_eal

EXPORT_MAP := rte_power_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_POWER) := rte_power.c power_acpi_cpufreq.c
SRCS-$(CONFIG_RTE_LIBRTE_POWER) += power_kvm_vm.c guest_channel.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_POWER)-include := rte_power.h

include $(RTE_SDK)/mk/rte.lib.mk
