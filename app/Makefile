# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_APP_TEST) += test
DIRS-$(CONFIG_RTE_TEST_PMD) += test-pmd
DIRS-$(CONFIG_RTE_PROC_INFO) += proc-info
DIRS-$(CONFIG_RTE_LIBRTE_PDUMP) += pdump
DIRS-$(CONFIG_RTE_LIBRTE_ACL) += test-acl
DIRS-$(CONFIG_RTE_LIBRTE_CMDLINE) += test-cmdline
DIRS-$(CONFIG_RTE_LIBRTE_FIB) += test-fib
DIRS-$(CONFIG_RTE_TEST_FLOW_PERF) += test-flow-perf
DIRS-$(CONFIG_RTE_LIBRTE_PIPELINE) += test-pipeline
DIRS-$(CONFIG_RTE_LIBRTE_IPSEC) += test-sad
DIRS-$(CONFIG_RTE_LIBRTE_REGEXDEV) += test-regex

ifeq ($(CONFIG_RTE_LIBRTE_BBDEV),y)
DIRS-$(CONFIG_RTE_TEST_BBDEV) += test-bbdev
endif

ifeq ($(CONFIG_RTE_LIBRTE_COMPRESSDEV),y)
DIRS-$(CONFIG_RTE_APP_COMPRESS_PERF) += test-compress-perf
endif

ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
DIRS-$(CONFIG_RTE_APP_CRYPTO_PERF) += test-crypto-perf
endif

ifeq ($(CONFIG_RTE_LIBRTE_EVENTDEV),y)
DIRS-$(CONFIG_RTE_APP_EVENTDEV) += test-eventdev
endif

include $(RTE_SDK)/mk/rte.subdir.mk
