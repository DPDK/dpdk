# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

sources += files(
        'virtio.c',
        'virtio_cvq.c',
        'virtio_ethdev.c',
        'virtio_pci_ethdev.c',
        'virtio_pci.c',
        'virtio_rxtx.c',
        'virtio_rxtx_simple.c',
        'virtqueue.c',
)
deps += ['kvargs', 'bus_pci']

cflags += no_wvla_cflag

if arch_subdir == 'x86'
    if cc_has_avx512
        cflags += ['-DVIRTIO_RXTX_PACKED_VEC']
        sources_avx512 += files('virtio_rxtx_packed.c')
        if toolchain == 'gcc'
            cflags += '-DVIRTIO_GCC_UNROLL_PRAGMA'
        elif toolchain == 'clang'
            cflags += '-DVIRTIO_CLANG_UNROLL_PRAGMA'
        endif
    endif
    cflags += ['-DVIRTIO_RXTX_VEC']
    sources += files('virtio_rxtx_simple_sse.c')
elif arch_subdir == 'ppc'
    cflags += ['-DVIRTIO_RXTX_VEC']
    sources += files('virtio_rxtx_simple_altivec.c')
elif arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
    cflags += ['-DVIRTIO_RXTX_PACKED_VEC']
    sources += files('virtio_rxtx_packed.c')
    cflags += ['-DVIRTIO_RXTX_VEC']
    sources += files('virtio_rxtx_simple_neon.c')
endif

if is_linux
    sources += files('virtio_user_ethdev.c',
        'virtio_user/vhost_kernel.c',
        'virtio_user/vhost_kernel_tap.c',
        'virtio_user/vhost_user.c',
        'virtio_user/vhost_vdpa.c',
        'virtio_user/virtio_user_dev.c')
    deps += ['bus_vdev']
endif
