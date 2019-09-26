# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2019 Hisilicon Limited.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_hns3.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

LDLIBS += -lrte_eal
LDLIBS += -lrte_ethdev -lrte_net -lrte_hash
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_hns3_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_ethdev_vf.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_cmd.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_mbx.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_rss.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_fdir.c
SRCS-$(CONFIG_RTE_LIBRTE_HNS3_PMD) += hns3_dcb.c

include $(RTE_SDK)/mk/rte.lib.mk
