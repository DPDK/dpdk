# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += bus
DIRS-y += mempool
DEPDIRS-mempool := bus
DIRS-y += net
DEPDIRS-net := bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_BBDEV) += bbdev
DEPDIRS-bbdev := bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += crypto
DEPDIRS-crypto := bus mempool
DIRS-$(CONFIG_RTE_LIBRTE_EVENTDEV) += event
DEPDIRS-event := bus mempool net
DIRS-$(CONFIG_RTE_LIBRTE_RAWDEV) += raw
DEPDIRS-raw := bus mempool net event

include $(RTE_SDK)/mk/rte.subdir.mk
