# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

if is_windows
	subdir_done()
endif

apps = [
	'pdump',
	'proc-info',
	'test-acl',
	'test-bbdev',
	'test-cmdline',
	'test-compress-perf',
	'test-crypto-perf',
	'test-eventdev',
	'test-fib',
	'test-flow-perf',
	'test-pipeline',
	'test-pmd',
	'test-regex',
	'test-sad']

# for BSD only
lib_execinfo = cc.find_library('execinfo', required: false)

default_cflags = machine_args + ['-DALLOW_EXPERIMENTAL_API']

foreach app:apps
	build = true
	name = app
	sources = []
	includes = []
	cflags = default_cflags
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
			link_libs = dpdk_static_libraries + dpdk_drivers
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

# special case the autotests
subdir('test')
