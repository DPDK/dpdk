# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += client_server_mp
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += simple_mp
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += symmetric_mp

include $(RTE_SDK)/mk/rte.extsubdir.mk
