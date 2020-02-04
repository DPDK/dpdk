# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_octeontx2.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/cpt
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/crypto/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/net/octeontx2
CFLAGS += -O3
ifneq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -flax-vector-conversions
endif

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

EXPORT_MAP := rte_pmd_octeontx2_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_OCTEONTX2_PMD) += \
	otx2_rx.c 	\
	otx2_tx.c 	\
	otx2_tm.c	\
	otx2_rss.c	\
	otx2_mac.c	\
	otx2_ptp.c	\
	otx2_flow.c	\
	otx2_link.c	\
	otx2_vlan.c	\
	otx2_stats.c	\
	otx2_mcast.c	\
	otx2_lookup.c	\
	otx2_ethdev.c	\
	otx2_flow_ctrl.c \
	otx2_flow_parse.c \
	otx2_flow_utils.c \
	otx2_ethdev_irq.c \
	otx2_ethdev_ops.c \
	otx2_ethdev_sec.c \
	otx2_ethdev_debug.c \
	otx2_ethdev_devargs.c

LDLIBS += -lrte_common_octeontx2 -lrte_mempool_octeontx2 -lrte_eal -lrte_net
LDLIBS += -lrte_ethdev -lrte_bus_pci -lrte_kvargs -lrte_mbuf  -lrte_mempool -lm
LDLIBS += -lrte_cryptodev -lrte_eventdev -lrte_security

include $(RTE_SDK)/mk/rte.lib.mk
