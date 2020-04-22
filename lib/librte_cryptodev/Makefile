# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_cryptodev.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool -lrte_ring -lrte_mbuf
LDLIBS += -lrte_kvargs

# library source files
SRCS-y += rte_cryptodev.c rte_cryptodev_pmd.c cryptodev_trace_points.c

# export include files
SYMLINK-y-include += rte_crypto.h
SYMLINK-y-include += rte_crypto_asym.h
SYMLINK-y-include += rte_crypto_sym.h
SYMLINK-y-include += rte_cryptodev.h
SYMLINK-y-include += rte_cryptodev_pmd.h
SYMLINK-y-include += rte_cryptodev_trace.h
SYMLINK-y-include += rte_cryptodev_trace_fp.h

# versioning export map
EXPORT_MAP := rte_cryptodev_version.map

include $(RTE_SDK)/mk/rte.lib.mk
