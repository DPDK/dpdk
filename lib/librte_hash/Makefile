# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_hash.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
LDLIBS += -lrte_eal -lrte_ring

EXPORT_MAP := rte_hash_version.map

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_HASH) := rte_cuckoo_hash.c
SRCS-$(CONFIG_RTE_LIBRTE_HASH) += rte_fbk_hash.c

# install this header file
SYMLINK-$(CONFIG_RTE_LIBRTE_HASH)-include := rte_hash.h
SYMLINK-$(CONFIG_RTE_LIBRTE_HASH)-include += rte_hash_crc.h
ifeq ($(CONFIG_RTE_ARCH_ARM64),y)
ifneq ($(findstring RTE_MACHINE_CPUFLAG_CRC32,$(CFLAGS)),)
SYMLINK-$(CONFIG_RTE_LIBRTE_HASH)-include += rte_crc_arm64.h
endif
endif
SYMLINK-$(CONFIG_RTE_LIBRTE_HASH)-include += rte_jhash.h
SYMLINK-$(CONFIG_RTE_LIBRTE_HASH)-include += rte_thash.h
SYMLINK-$(CONFIG_RTE_LIBRTE_HASH)-include += rte_fbk_hash.h

include $(RTE_SDK)/mk/rte.lib.mk
