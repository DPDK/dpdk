# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_virtio_crypto.a

#
# include virtio_crypto.h
#
CFLAGS += -I$(RTE_SDK)/lib/librte_vhost
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

EXPORT_MAP := rte_pmd_virtio_crypto_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_VIRTIO_CRYPTO) += virtqueue.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_VIRTIO_CRYPTO) += virtio_pci.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_VIRTIO_CRYPTO) += virtio_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_VIRTIO_CRYPTO) += virtio_cryptodev.c

# this lib depends upon:
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool
LDLIBS += -lrte_cryptodev
LDLIBS += -lrte_pci -lrte_bus_pci

include $(RTE_SDK)/mk/rte.lib.mk
