# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

kni_mkfile = custom_target('rte_kni_makefile',
	output: 'Makefile',
	command: ['touch', '@OUTPUT@'])

kni_sources = files(
	'kni_misc.c',
	'kni_net.c',
	'Kbuild')

custom_target('rte_kni',
	input: kni_sources,
	output: 'rte_kni.ko',
	command: ['make', '-j4', '-C', kernel_dir + '/build',
		'M=' + meson.current_build_dir(),
		'src=' + meson.current_source_dir(),
		'MODULE_CFLAGS=-include ' + meson.source_root() + '/config/rte_config.h' +
		' -I' + meson.source_root() + '/lib/librte_eal/include' +
		' -I' + meson.source_root() + '/lib/librte_eal/linux/include' +
		' -I' + meson.build_root() +
		' -I' + meson.current_source_dir(),
		'modules'],
	depends: kni_mkfile,
	install: true,
	install_dir: kernel_dir + '/extra/dpdk',
	build_by_default: get_option('enable_kmods'))
