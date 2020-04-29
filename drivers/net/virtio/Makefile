# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_virtio.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs
LDLIBS += -lrte_bus_pci
ifeq ($(CONFIG_RTE_VIRTIO_USER),y)
LDLIBS += -lrte_bus_vdev
endif

EXPORT_MAP := rte_pmd_virtio_version.map

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
else ifeq ($(CONFIG_RTE_ARCH_PPC_64),y)
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx_simple_altivec.c
else ifneq ($(filter y,$(CONFIG_RTE_ARCH_ARM) $(CONFIG_RTE_ARCH_ARM64)),)
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx_simple_neon.c
endif

ifneq ($(FORCE_DISABLE_AVX512), y)
	CC_AVX512_SUPPORT=\
	$(shell $(CC) -march=native -dM -E - </dev/null 2>&1 | \
	sed '/./{H;$$!d} ; x ; /AVX512F/!d; /AVX512BW/!d; /AVX512VL/!d' | \
	grep -q AVX512 && echo 1)
endif

ifeq ($(CC_AVX512_SUPPORT), 1)
CFLAGS += -DCC_AVX512_SUPPORT
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_rxtx_packed_avx.c

ifeq ($(RTE_TOOLCHAIN), gcc)
ifeq ($(shell test $(GCC_VERSION) -ge 83 && echo 1), 1)
CFLAGS += -DVIRTIO_GCC_UNROLL_PRAGMA
endif
endif

ifeq ($(RTE_TOOLCHAIN), clang)
ifeq ($(shell test $(CLANG_MAJOR_VERSION)$(CLANG_MINOR_VERSION) -ge 37 && echo 1), 1)
CFLAGS += -DVIRTIO_CLANG_UNROLL_PRAGMA
endif
endif

ifeq ($(RTE_TOOLCHAIN), icc)
ifeq ($(shell test $(ICC_MAJOR_VERSION) -ge 16 && echo 1), 1)
CFLAGS += -DVIRTIO_ICC_UNROLL_PRAGMA
endif
endif

CFLAGS_virtio_rxtx_packed_avx.o += -mavx512f -mavx512bw -mavx512vl
ifeq ($(shell test $(GCC_VERSION) -ge 100 && echo 1), 1)
CFLAGS_virtio_rxtx_packed_avx.o += -Wno-zero-length-bounds
endif
endif

ifeq ($(CONFIG_RTE_VIRTIO_USER),y)
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/vhost_user.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/vhost_kernel.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/vhost_kernel_tap.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user/virtio_user_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio_user_ethdev.c
endif

include $(RTE_SDK)/mk/rte.lib.mk
