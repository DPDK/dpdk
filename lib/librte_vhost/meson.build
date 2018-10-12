# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

if host_machine.system() != 'linux'
	build = false
endif
if has_libnuma == 1
	dpdk_conf.set10('RTE_LIBRTE_VHOST_NUMA', true)
endif
dpdk_conf.set('RTE_LIBRTE_VHOST_POSTCOPY',
	      cc.has_header('linux/userfaultfd.h'))
version = 4
allow_experimental_apis = true
cflags += '-fno-strict-aliasing'
sources = files('fd_man.c', 'iotlb.c', 'socket.c', 'vdpa.c',
		'vhost.c', 'vhost_user.c',
		'virtio_net.c', 'vhost_crypto.c')
headers = files('rte_vhost.h', 'rte_vdpa.h', 'rte_vhost_crypto.h')
deps += ['ethdev', 'cryptodev', 'hash', 'pci']
