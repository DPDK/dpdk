# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += include
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += linux
DEPDIRS-linux := include
DIRS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += freebsd
DEPDIRS-freebsd := include

include $(RTE_SDK)/mk/rte.subdir.mk
