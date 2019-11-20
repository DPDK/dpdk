# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_table.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_port
LDLIBS += -lrte_lpm -lrte_hash
ifeq ($(CONFIG_RTE_LIBRTE_ACL),y)
LDLIBS += -lrte_acl
endif

EXPORT_MAP := rte_table_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_lpm.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_lpm_ipv6.c
ifeq ($(CONFIG_RTE_LIBRTE_ACL),y)
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_acl.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_hash_cuckoo.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_hash_key8.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_hash_key16.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_hash_key32.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_hash_ext.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_hash_lru.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_array.c
SRCS-$(CONFIG_RTE_LIBRTE_TABLE) += rte_table_stub.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_lpm.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_lpm_ipv6.h
ifeq ($(CONFIG_RTE_LIBRTE_ACL),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_acl.h
endif
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_hash.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_hash_cuckoo.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_hash_func.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_hash_func_arm64.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_lru.h
ifeq ($(CONFIG_RTE_ARCH_X86),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_lru_x86.h
endif
ifeq ($(CONFIG_RTE_ARCH_ARM64),y)
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_lru_arm64.h
endif
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_array.h
SYMLINK-$(CONFIG_RTE_LIBRTE_TABLE)-include += rte_table_stub.h

include $(RTE_SDK)/mk/rte.lib.mk
