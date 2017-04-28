#   BSD LICENSE
#
#   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_table.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_table_version.map

LIBABIVER := 2

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
