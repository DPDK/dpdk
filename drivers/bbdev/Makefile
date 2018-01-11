# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

core-libs := librte_eal librte_mbuf librte_mempool librte_ring
core-libs += librte_bbdev librte_kvargs librte_cfgfile

DIRS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_NULL) += null
DEPDIRS-null = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += turbo_sw
DEPDIRS-turbo_sw = $(core-libs)

include $(RTE_SDK)/mk/rte.subdir.mk
