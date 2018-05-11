# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017 NXP

include $(RTE_SDK)/mk/rte.vars.mk

# DIRS-$(<configuration>) += <directory>
DIRS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_RAWDEV) += skeleton_rawdev
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV) += dpaa2_cmdif
DIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV) += dpaa2_qdma
endif
DIRS-$(CONFIG_RTE_LIBRTE_PMD_IFPGA_RAWDEV) += ifpga_rawdev

include $(RTE_SDK)/mk/rte.subdir.mk
