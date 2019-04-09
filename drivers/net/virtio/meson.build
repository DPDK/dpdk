# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

allow_experimental_apis = true
sources += files('virtio_ethdev.c',
	'virtio_pci.c',
	'virtio_rxtx.c',
	'virtio_rxtx_simple.c',
	'virtqueue.c')
deps += ['kvargs', 'bus_pci']

if arch_subdir == 'x86'
	sources += files('virtio_rxtx_simple_sse.c')
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
