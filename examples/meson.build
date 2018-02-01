# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

driver_libs = []
if get_option('default_library') == 'static'
	driver_libs = dpdk_drivers
endif

execinfo = cc.find_library('execinfo', required: false)
foreach example: get_option('examples').split(',')
	name = example
	sources = []
	allow_experimental_apis = false
	cflags = machine_args
	ext_deps = [execinfo]
	includes = [include_directories(example)]
	deps = ['eal', 'mempool', 'net', 'mbuf', 'ethdev', 'cmdline']
	subdir(example)

	dep_objs = ext_deps
	foreach d:deps
		dep_objs += [get_variable(get_option('default_library') + '_rte_' + d)]
	endforeach
	if allow_experimental_apis
		cflags += '-DALLOW_EXPERIMENTAL_API'
	endif
	executable('dpdk-' + name, sources,
		include_directories: includes,
		link_whole: driver_libs,
		link_args: dpdk_extra_ldflags,
		c_args: cflags,
		dependencies: dep_objs)
endforeach
