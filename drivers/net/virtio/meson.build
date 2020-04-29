# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources += files('virtio_ethdev.c',
	'virtio_pci.c',
	'virtio_rxtx.c',
	'virtio_rxtx_simple.c',
	'virtqueue.c')
deps += ['kvargs', 'bus_pci']

if arch_subdir == 'x86'
	if not machine_args.contains('-mno-avx512f')
		if cc.has_argument('-mavx512f') and cc.has_argument('-mavx512vl') and cc.has_argument('-mavx512bw')
			cflags += ['-mavx512f', '-mavx512bw', '-mavx512vl']
			cflags += ['-DCC_AVX512_SUPPORT']
			if (toolchain == 'gcc' and cc.version().version_compare('>=8.3.0'))
				cflags += '-DVHOST_GCC_UNROLL_PRAGMA'
			elif (toolchain == 'clang' and cc.version().version_compare('>=3.7.0'))
				cflags += '-DVHOST_CLANG_UNROLL_PRAGMA'
			elif (toolchain == 'icc' and cc.version().version_compare('>=16.0.0'))
				cflags += '-DVHOST_ICC_UNROLL_PRAGMA'
			endif
			sources += files('virtio_rxtx_packed_avx.c')
		endif
	endif
	sources += files('virtio_rxtx_simple_sse.c')
elif arch_subdir == 'ppc'
	sources += files('virtio_rxtx_simple_altivec.c')
elif arch_subdir == 'arm' and host_machine.cpu_family().startswith('aarch64')
	sources += files('virtio_rxtx_simple_neon.c')
endif

if is_linux
	dpdk_conf.set('RTE_VIRTIO_USER', 1)

	sources += files('virtio_user_ethdev.c',
		'virtio_user/vhost_kernel.c',
		'virtio_user/vhost_kernel_tap.c',
		'virtio_user/vhost_user.c',
		'virtio_user/virtio_user_dev.c')
	deps += ['bus_vdev']
endif
