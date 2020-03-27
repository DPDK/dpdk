# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include := \
	$(sort $(notdir \
	$(wildcard $(RTE_SDK)/lib/librte_eal/include/*.h)))

SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include/generic := \
	$(sort $(addprefix generic/, $(notdir \
	$(wildcard $(RTE_SDK)/lib/librte_eal/include/generic/*.h))))

ARCH_DIR ?= $(RTE_ARCH)
SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include += \
	$(sort $(addprefix ../$(ARCH_DIR)/include/, $(notdir \
	$(wildcard $(RTE_SDK)/lib/librte_eal/$(ARCH_DIR)/include/*.h))))

include $(RTE_SDK)/mk/rte.install.mk
