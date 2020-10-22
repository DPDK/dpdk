# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Intel Corporation

build = dpdk_conf.has('RTE_ARCH_X86')
reason = 'only supported on x86'
sources = files(
	'idxd_pci.c',
	'idxd_vdev.c',
	'ioat_common.c',
	'ioat_rawdev.c',
	'ioat_rawdev_test.c')
deps += ['bus_pci',
	'bus_vdev',
	'mbuf',
	'rawdev']

headers = files('rte_ioat_rawdev.h',
		'rte_ioat_rawdev_fns.h')
