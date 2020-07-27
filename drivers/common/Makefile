# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

CPT-y := $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_CRYPTO)
CPT-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO)
ifneq (,$(findstring y,$(CPT-y)))
DIRS-y += cpt
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF)$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL),yy)
DIRS-y += octeontx
endif
OCTEONTX2-y := $(CONFIG_RTE_LIBRTE_OCTEONTX2_MEMPOOL)
OCTEONTX2-y += $(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_CRYPTO)
ifeq ($(findstring y,$(OCTEONTX2-y)),y)
DIRS-y += octeontx2
endif

MVEP-y := $(CONFIG_RTE_LIBRTE_MVPP2_PMD)
MVEP-y += $(CONFIG_RTE_LIBRTE_MVNETA_PMD)
MVEP-y += $(CONFIG_RTE_LIBRTE_PMD_MVSAM_CRYPTO)
ifneq (,$(findstring y,$(MVEP-y)))
DIRS-y += mvep
endif

ifeq ($(CONFIG_RTE_LIBRTE_COMMON_DPAAX),y)
DIRS-y += dpaax
endif

IAVF-y := $(CONFIG_RTE_LIBRTE_IAVF_PMD)
IAVF-y += $(CONFIG_RTE_LIBRTE_ICE_PMD)
ifneq (,$(findstring y,$(IAVF-y)))
DIRS-y += iavf
endif

include $(RTE_SDK)/mk/rte.subdir.mk
