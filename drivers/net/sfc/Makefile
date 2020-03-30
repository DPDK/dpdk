# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2019-2020 Xilinx, Inc.
# Copyright(c) 2016-2019 Solarflare Communications Inc.
#
# This software was jointly developed between OKTET Labs (under contract
# for Solarflare) and Solarflare Communications, Inc.

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
# Suppress ICC false positive warning on 'bulk' may be used before its
# value is set
CFLAGS_sfc_ef10_tx.o += -diag-disable 3656
endif
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci -lrte_pci

#
# List of base driver object files for which
# special CFLAGS above should be applied
#
BASE_DRIVER_OBJS=$(sort $(patsubst %.c,%.o,$(notdir $(wildcard $(SRCDIR)/base/*.c))))
$(foreach obj, $(BASE_DRIVER_OBJS), \
  $(eval CFLAGS_$(obj)+=$(CFLAGS_BASE_DRIVER)))

EXPORT_MAP := rte_pmd_sfc_version.map

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
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ef10_essb_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc_ef10_tx.c

VPATH += $(SRCDIR)/base

SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_bootcfg.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_crc32.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_ev.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_evb.c
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
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_proxy.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_sram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += efx_tunnel.c
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
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_evb.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_filter.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_intr.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_image.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_mcdi.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_nvram.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_phy.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_proxy.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_rx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_tx.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += ef10_vpd.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += hunt_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += medford_nic.c
SRCS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += medford2_nic.c

include $(RTE_SDK)/mk/rte.lib.mk
