# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

kernel_dir = get_option('kernel_dir')
if kernel_dir == ''
	kernel_version = run_command('uname', '-r').stdout().strip()
	kernel_dir = '/lib/modules/' + kernel_version + '/build'
endif

mkfile = custom_target('igb_uio_makefile',
	output: 'Makefile',
	command: ['touch', '@OUTPUT@'])

custom_target('igb_uio',
	input: ['igb_uio.c', 'Kbuild'],
	output: 'igb_uio.ko',
	command: ['make', '-C', kernel_dir,
		'M=' + meson.current_build_dir(),
		'src=' + meson.current_source_dir(),
		'EXTRA_CFLAGS=-I' + meson.current_source_dir() +
			'/../../common/include',
		'modules'],
	depends: mkfile,
	build_by_default: get_option('enable_kmods'))
