# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += common
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += linux
DEPDIRS-linux := common
DIRS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += freebsd
DEPDIRS-freebsd := common

include $(RTE_SDK)/mk/rte.subdir.mk
