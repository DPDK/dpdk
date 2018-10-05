# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_nfp.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -DALLOW_EXPERIMENTAL_API

LDLIBS += -lm
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci

EXPORT_MAP := rte_pmd_nfp_version.map

LIBABIVER := 1

VPATH += $(SRCDIR)/nfpcore

SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_cppcore.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_cpp_pcie_ops.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_mutex.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_resource.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_crc.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_mip.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_nffw.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_hwinfo.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_rtsym.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_nsp.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_nsp_cmds.c
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_nsp_eth.c

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp_net.c

include $(RTE_SDK)/mk/rte.lib.mk
