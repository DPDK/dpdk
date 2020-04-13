# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

sources = files(
	'opdl_evdev.c',
	'opdl_evdev_init.c',
	'opdl_evdev_xstats.c',
	'opdl_ring.c',
	'opdl_test.c',
)
deps += ['bus_vdev']

# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang' and dpdk_conf.get('RTE_ARCH_64') == false
	ext_deps += cc.find_library('atomic')
endif
