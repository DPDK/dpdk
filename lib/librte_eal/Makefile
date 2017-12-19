# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += common
DIRS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += linuxapp
DEPDIRS-linuxapp := common
DIRS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += bsdapp
DEPDIRS-bsdapp := common

include $(RTE_SDK)/mk/rte.subdir.mk
