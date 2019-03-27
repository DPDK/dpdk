# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += qw
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += qwctl

include $(RTE_SDK)/mk/rte.extsubdir.mk
