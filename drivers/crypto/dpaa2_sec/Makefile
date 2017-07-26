#   BSD LICENSE
#
#   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
#   Copyright 2016 NXP.
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
#     * Neither the name of Freescale Semiconductor, Inc nor the names of its
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
LIB = librte_pmd_dpaa2_sec.a

# build flags
ifeq ($(CONFIG_RTE_LIBRTE_DPAA2_SEC_DEBUG_INIT),y)
CFLAGS += -O0 -g
CFLAGS += "-Wno-error"
else
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
endif
CFLAGS += -D _GNU_SOURCE

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -gt 70 && echo 1), 1)
CFLAGS += -Wno-implicit-fallthrough
endif
endif

CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec/
CFLAGS += -I$(RTE_SDK)/drivers/crypto/dpaa2_sec/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/qbman/include
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/mc
CFLAGS += -I$(RTE_SDK)/drivers/bus/fslmc/portal
CFLAGS += -I$(RTE_SDK)/drivers/mempool/dpaa2/
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/linuxapp/eal

# versioning export map
EXPORT_MAP := rte_pmd_dpaa2_sec_version.map

# library version
LIBABIVER := 1

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += dpaa2_sec_dpseci.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += mc/dpseci.c

# library dependencies
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += lib/librte_eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += lib/librte_mempool lib/librte_mbuf
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += lib/librte_cryptodev
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += drivers/bus/fslmc
DEPDIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += drivers/mempool/dpaa2

LDLIBS += -lrte_bus_fslmc
LDLIBS += -lrte_mempool_dpaa2

include $(RTE_SDK)/mk/rte.lib.mk
