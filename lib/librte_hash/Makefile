#   BSD LICENSE
#
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

# library name
LIB = librte_hash.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)

EXPORT_MAP := rte_hash_version.map

LIBABIVER := 2

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
