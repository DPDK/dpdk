# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal
DEPDIRS-kni := eal

CFLAGS += -DALLOW_EXPERIMENTAL_API

include $(RTE_SDK)/mk/rte.subdir.mk
