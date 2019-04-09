# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

driver_libs = []
if get_option('default_library') == 'static'
	driver_libs = dpdk_drivers
endif

execinfo = cc.find_library('execinfo', required: false)

allow_skips = true # don't flag an error if we can't build an app

if get_option('examples').to_lower() == 'all'
	dirs = run_command('sh', '-c',
		'cd $MESON_SOURCE_ROOT/$MESON_SUBDIR && for d in * ; do if [ -d $d ] ; then echo $d ; fi ; done')
	examples = dirs.stdout().split()
else
	examples = get_option('examples').split(',')
	allow_skips = false # error out if we can't build a requested app
endif
default_cflags = machine_args
if cc.has_argument('-Wno-format-truncation')
	default_cflags += '-Wno-format-truncation'
endif

# specify -D_GNU_SOURCE unconditionally
default_cflags += '-D_GNU_SOURCE'

foreach example: examples
	name = example
	build = true
	sources = []
	allow_experimental_apis = false
	cflags = default_cflags

	ext_deps = [execinfo]
	includes = [include_directories(example)]
	deps = ['eal', 'mempool', 'net', 'mbuf', 'ethdev', 'cmdline']
	if is_windows
		deps = ['eal'] # only supported lib on Windows currently
	endif
	subdir(example)

	if build
		dep_objs = ext_deps
		foreach d:deps
			var_name = get_option('default_library') + '_rte_' + d
			if not is_variable(var_name)
				error('Missing dependency "@0@" for example "@1@"'.format(d, name))
			endif
			dep_objs += [get_variable(var_name)]
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
	elif not allow_skips
		error('Cannot build requested example "' + name + '"')
	else
		message('Skipping example "' + name + '"')
	endif
endforeach
