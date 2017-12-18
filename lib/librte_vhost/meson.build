# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if host_machine.system() != 'linux'
	build = false
endif
if has_libnuma == 1
	dpdk_conf.set10('RTE_LIBRTE_VHOST_NUMA', true)
endif
version = 4
allow_experimental_apis = true
sources = files('fd_man.c', 'iotlb.c', 'socket.c', 'vhost.c', 'vhost_user.c',
		'virtio_net.c')
headers = files('rte_vhost.h')
deps += ['ethdev']
