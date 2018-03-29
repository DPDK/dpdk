# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 3
sources = files(
	'rte_port_ethdev.c',
	'rte_port_fd.c',
	'rte_port_frag.c',
	'rte_port_ras.c',
	'rte_port_ring.c',
	'rte_port_sched.c',
	'rte_port_source_sink.c')
headers = files(
	'rte_port_ethdev.h',
	'rte_port_fd.h',
	'rte_port_frag.h',
	'rte_port_ras.h',
	'rte_port.h',
	'rte_port_ring.h',
	'rte_port_sched.h',
	'rte_port_source_sink.h')
deps += ['ethdev', 'sched', 'ip_frag']

if dpdk_conf.has('RTE_LIBRTE_KNI')
	sources += files('rte_port_kni.c')
	headers += files('rte_port_kni.h')
	deps += 'kni'
endif
