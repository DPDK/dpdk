#   BSD LICENSE
#
#   Copyright(c) 2016 Intel Corporation. All rights reserved.
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
LIB = librte_pmd_openssl.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_openssl_version.map

# external library dependencies
LDLIBS += -lcrypto

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += rte_openssl_pmd.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += rte_openssl_pmd_ops.c

# library dependencies
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += lib/librte_eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += lib/librte_mempool
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += lib/librte_ring
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += lib/librte_cryptodev

include $(RTE_SDK)/mk/rte.lib.mk
