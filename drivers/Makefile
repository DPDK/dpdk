# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += common
DIRS-y += bus
DIRS-y += mempool
DEPDIRS-mempool := common bus
DIRS-y += net
DEPDIRS-net := common bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_BBDEV) += baseband
DEPDIRS-baseband := common bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += crypto
DEPDIRS-crypto := common bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_COMPRESSDEV) += compress
DEPDIRS-compress := bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_EVENTDEV) += event
DEPDIRS-event := common bus mempool net
DIRS-$(CONFIG_RTE_LIBRTE_RAWDEV) += raw
DEPDIRS-raw := common bus mempool net event

include $(RTE_SDK)/mk/rte.subdir.mk
