# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_security.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool

# library source files
SRCS-y += rte_security.c

# export include files
SYMLINK-y-include += rte_security.h
SYMLINK-y-include += rte_security_driver.h

# versioning export map
EXPORT_MAP := rte_security_version.map

include $(RTE_SDK)/mk/rte.lib.mk
