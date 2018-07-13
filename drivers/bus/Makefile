# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016 NXP

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_LIBRTE_DPAA_BUS) += dpaa
ifeq ($(CONFIG_RTE_EAL_VFIO),y)
DIRS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS) += fslmc
endif
DIRS-$(CONFIG_RTE_LIBRTE_IFPGA_BUS) += ifpga
DIRS-$(CONFIG_RTE_LIBRTE_PCI_BUS) += pci
DIRS-$(CONFIG_RTE_LIBRTE_VDEV_BUS) += vdev
DIRS-$(CONFIG_RTE_LIBRTE_VMBUS) += vmbus

include $(RTE_SDK)/mk/rte.subdir.mk
