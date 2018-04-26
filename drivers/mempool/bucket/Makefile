# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2017-2018 Solarflare Communications Inc.
# All rights reserved.
#
# This software was jointly developed between OKTET Labs (under contract
# for Solarflare) and Solarflare Communications, Inc.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_mempool_bucket.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lrte_eal -lrte_mempool -lrte_ring

EXPORT_MAP := rte_mempool_bucket_version.map

LIBABIVER := 1

SRCS-$(CONFIG_RTE_DRIVER_MEMPOOL_BUCKET) += rte_mempool_bucket.c

include $(RTE_SDK)/mk/rte.lib.mk
