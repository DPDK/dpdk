# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

apps = ['pdump',
	'proc-info',
	'test-bbdev',
	'test-crypto-perf',
	'test-eventdev',
	'test-pmd']

# for BSD only
lib_execinfo = cc.find_library('execinfo', required: false)

foreach app:apps
	build = true
	name = app
	allow_experimental_apis = false
	sources = []
	includes = []
	cflags = machine_args
	objs = [] # other object files to link against, used e.g. for
	          # instruction-set optimized versions of code

	# use "deps" for internal DPDK dependencies, and "ext_deps" for
	# external package/library requirements
	ext_deps = []
	deps = []

	subdir(name)

	if build
		dep_objs = []
		foreach d:deps
			dep_objs += get_variable(get_option('default_library')
				 + '_rte_' + d)
		endforeach
		dep_objs += lib_execinfo

		link_libs = []
		if get_option('default_library') == 'static'
			link_libs = dpdk_drivers
		endif

		if allow_experimental_apis
			cflags += '-DALLOW_EXPERIMENTAL_API'
		endif

		executable('dpdk-' + name,
				sources,
				c_args: cflags,
				link_whole: link_libs,
				dependencies: dep_objs,
				install_rpath: join_paths(get_option('prefix'),
						 driver_install_path),
				install: true)
	endif
endforeach
