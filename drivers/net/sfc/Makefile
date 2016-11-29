#
# Copyright (c) 2016 Solarflare Communications Inc.
# All rights reserved.
#
# This software was jointly developed between OKTET Labs (under contract
# for Solarflare) and Solarflare Communications, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_sfc_efx.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

# Enable extra warnings
CFLAGS += -Wextra

# More warnings not enabled by above aggregators
CFLAGS += -Wdisabled-optimization

# Compiler and version dependent flags
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS += -Waggregate-return
CFLAGS += -Wbad-function-cast
else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
CFLAGS += -Waggregate-return
CFLAGS += -Wbad-function-cast
else ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
endif

EXPORT_MAP := rte_pmd_sfc_efx_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_kvargs.c


# this lib depends upon:
DEPDIRS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += lib/librte_eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += lib/librte_kvargs
DEPDIRS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += lib/librte_mempool
DEPDIRS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += lib/librte_mbuf

include $(RTE_SDK)/mk/rte.lib.mk
