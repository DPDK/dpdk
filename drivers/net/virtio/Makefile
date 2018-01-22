# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_virtio.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci
ifeq ($(CONFIG_RTE_VIRTIO_USER),y)
LDLIBS += -lrte_bus_vdev
endif

EXPORT_MAP := rte_pmd_virtio_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtqueue.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_pci.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx_simple.c

ifeq ($(CONFIG_RTE_ARCH_X86),y)
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx_simple_sse.c
else ifneq ($(filter y,$(CONFIG_RTE_ARCH_ARM) $(CONFIG_RTE_ARCH_ARM64)),)
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx_simple_neon.c
endif

ifeq ($(CONFIG_RTE_VIRTIO_USER),y)
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/vhost_user.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/vhost_kernel.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/vhost_kernel_tap.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/virtio_user_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user_ethdev.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
