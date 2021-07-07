# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources += files(
        'virtio.c',
        'virtio_ethdev.c',
        'virtio_pci_ethdev.c',
        'virtio_pci.c',
        'virtio_rxtx.c',
        'virtio_rxtx_simple.c',
        'virtqueue.c',
)
deps += ['kvargs', 'bus_pci']

if arch_subdir == 'x86'
    if not machine_args.contains('-mno-avx512f')
        if cc.has_argument('-mavx512f') and cc.has_argument('-mavx512vl') and cc.has_argument('-mavx512bw')
            cflags += ['-DCC_AVX512_SUPPORT']
            virtio_avx512_lib = static_library('virtio_avx512_lib',
                          'virtio_rxtx_packed.c',
                          dependencies: [static_rte_ethdev,
                        static_rte_kvargs, static_rte_bus_pci],
                          include_directories: includes,
                          c_args: [cflags, '-mavx512f', '-mavx512bw', '-mavx512vl'])
            objs += virtio_avx512_lib.extract_objects('virtio_rxtx_packed.c')
            if (toolchain == 'gcc' and cc.version().version_compare('>=8.3.0'))
                cflags += '-DVHOST_GCC_UNROLL_PRAGMA'
            elif (toolchain == 'clang' and cc.version().version_compare('>=3.7.0'))
                cflags += '-DVHOST_CLANG_UNROLL_PRAGMA'
            elif (toolchain == 'icc' and cc.version().version_compare('>=16.0.0'))
                cflags += '-DVHOST_ICC_UNROLL_PRAGMA'
            endif
        endif
    endif
    sources += files('virtio_rxtx_simple_sse.c')
elif arch_subdir == 'ppc'
    sources += files('virtio_rxtx_simple_altivec.c')
elif arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
    sources += files('virtio_rxtx_packed.c')
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
