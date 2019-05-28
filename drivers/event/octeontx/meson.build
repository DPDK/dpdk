# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

sources = files('ssovf_worker.c',
		'ssovf_evdev.c',
		'ssovf_evdev_selftest.c',
		'ssovf_probe.c',
		'timvf_worker.c',
		'timvf_evdev.c',
		'timvf_probe.c'
)

deps += ['common_octeontx', 'mempool_octeontx', 'bus_vdev', 'pmd_octeontx']

# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang' and dpdk_conf.get('RTE_ARCH_64') == false
	ext_deps += cc.find_library('atomic')
endif
