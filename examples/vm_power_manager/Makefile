# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifneq ($(shell pkg-config --atleast-version=0.9.3 libvirt; echo $$?), 0)
$(error vm_power_manager requires libvirt >= 0.9.3)
else

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = vm_power_mgr

# all source are stored in SRCS-y
SRCS-y := main.c vm_power_cli.c power_manager.c channel_manager.c
SRCS-y += channel_monitor.c parse.c
ifeq ($(CONFIG_RTE_ARCH_X86_64),y)
SRCS-y += oob_monitor_x86.c
else
SRCS-y += oob_monitor_nop.c
endif

CFLAGS += -O3 -I$(RTE_SDK)/lib/librte_power/
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lvirt

JANSSON := $(shell pkg-config --exists jansson; echo $$?)
ifeq ($(JANSSON), 0)
LDLIBS += $(shell pkg-config --libs jansson)
CFLAGS += -DUSE_JANSSON
endif

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)

ifeq ($(CONFIG_RTE_LIBRTE_IXGBE_PMD),y)
LDLIBS += -lrte_pmd_ixgbe
endif

ifeq ($(CONFIG_RTE_LIBRTE_I40E_PMD),y)
LDLIBS += -lrte_pmd_i40e
endif

ifeq ($(CONFIG_RTE_LIBRTE_BNXT_PMD),y)
LDLIBS += -lrte_pmd_bnxt
endif

endif

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

include $(RTE_SDK)/mk/rte.extapp.mk

endif # libvirt check
