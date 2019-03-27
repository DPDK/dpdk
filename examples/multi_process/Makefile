# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += client_server_mp
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += simple_mp
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += symmetric_mp
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += hotplug_mp

include $(RTE_SDK)/mk/rte.extsubdir.mk
