#
#   BSD LICENSE
#
# Copyright (c) 2016-2017 Solarflare Communications Inc.
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

CFLAGS += -I$(SRCDIR)/base/
CFLAGS += -I$(SRCDIR)
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
# Strict-aliasing rules are violated by rte_eth_link to uint64_t casts
CFLAGS += -Wno-strict-aliasing

# Enable extra warnings
CFLAGS += -Wextra

# More warnings not enabled by above aggregators
CFLAGS += -Wdisabled-optimization

# Extra CFLAGS for base driver files
CFLAGS_BASE_DRIVER += -Wno-sign-compare
CFLAGS_BASE_DRIVER += -Wno-unused-parameter
CFLAGS_BASE_DRIVER += -Wno-unused-variable

# Compiler and version dependent flags
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS += -Waggregate-return
CFLAGS += -Wnested-externs
CFLAGS_BASE_DRIVER += -Wno-empty-body
CFLAGS_BASE_DRIVER += -Wno-unused-but-set-variable
else ifeq ($(CONFIG_RTE_TOOLCHAIN_CLANG),y)
CFLAGS += -Waggregate-return
CFLAGS += -Wbad-function-cast
CFLAGS_BASE_DRIVER += -Wno-empty-body
else ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_BASE_DRIVER += -Wno-unused-but-set-variable
endif

#
# List of base driver object files for which
# special CFLAGS above should be applied
#
BASE_DRIVER_OBJS=$(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c)))
$(foreach obj, $(BASE_DRIVER_OBJS), \
  $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

EXPORT_MAP := rte_pmd_sfc_efx_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_kvargs.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_mcdi.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ev.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_port.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_tso.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_dp.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ef10_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ef10_tx.c

VPATH += $(SRCDIR)/base

SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_bootcfg.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_crc32.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_ev.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_hash.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_lic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_mcdi.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_mon.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_nvram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_port.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_sram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_vpd.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += mcdi_mon.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_mcdi.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_nvram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_sram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += siena_vpd.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_ev.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_mcdi.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_nvram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_vpd.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += hunt_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += medford_nic.c

include $(RTE_SDK)/mk/rte.lib.mk
