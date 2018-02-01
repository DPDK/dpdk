# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

sources = files('evt_main.c',
		'evt_options.c',
		'evt_test.c',
		'parser.c',
		'test_order_common.c',
		'test_order_atq.c',
		'test_order_queue.c',
		'test_perf_common.c',
		'test_perf_atq.c',
		'test_perf_queue.c')

dep_objs = [get_variable(get_option('default_library') + '_rte_eventdev')]
dep_objs += cc.find_library('execinfo', required: false) # BSD only

link_libs = []
if get_option('default_library') == 'static'
	link_libs = dpdk_drivers
endif

executable('dpdk-test-eventdev',
	sources,
	c_args: [machine_args, '-DALLOW_EXPERIMENTAL_API'],
	link_whole: link_libs,
	dependencies: dep_objs,
	install_rpath: join_paths(get_option('prefix'), driver_install_path),
	install: true)
