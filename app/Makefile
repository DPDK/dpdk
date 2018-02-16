# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_TEST_PMD) += test-pmd
DIRS-$(CONFIG_RTE_PROC_INFO) += proc-info
DIRS-$(CONFIG_RTE_LIBRTE_PDUMP) += pdump

ifeq ($(CONFIG_RTE_LIBRTE_BBDEV),y)
DIRS-$(CONFIG_RTE_TEST_BBDEV) += test-bbdev
endif

ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
DIRS-$(CONFIG_RTE_APP_CRYPTO_PERF) += test-crypto-perf
endif

ifeq ($(CONFIG_RTE_LIBRTE_EVENTDEV),y)
DIRS-$(CONFIG_RTE_APP_EVENTDEV) += test-eventdev
endif

include $(RTE_SDK)/mk/rte.subdir.mk
