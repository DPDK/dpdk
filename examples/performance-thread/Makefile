# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(filter y,$(CONFIG_RTE_ARCH_X86_64) $(CONFIG_RTE_ARCH_ARM64)),)
$(error This application is only supported for x86_64 and arm64 targets)
endif

DIRS-y += l3fwd-thread
DIRS-y += pthread_shim


include $(RTE_SDK)/mk/rte.extsubdir.mk
