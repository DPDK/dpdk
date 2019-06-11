# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

mkfile = custom_target('igb_uio_makefile',
	output: 'Makefile',
	command: ['touch', '@OUTPUT@'])

custom_target('igb_uio',
	input: ['igb_uio.c', 'Kbuild'],
	output: 'igb_uio.ko',
	command: ['make', '-C', kernel_dir + '/build',
		'M=' + meson.current_build_dir(),
		'src=' + meson.current_source_dir(),
		'EXTRA_CFLAGS=-I' + meson.current_source_dir() +
			'/../../../lib/librte_eal/common/include',
		'modules'],
	depends: mkfile,
	install: true,
	install_dir: kernel_dir + '/extra/dpdk',
	build_by_default: get_option('enable_kmods'))
