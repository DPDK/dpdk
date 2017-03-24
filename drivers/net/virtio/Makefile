#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_virtio.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

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
