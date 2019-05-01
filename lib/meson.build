# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation


# process all libraries equally, as far as possible
# "core" libs first, then others alphebetically as far as possible
# NOTE: for speed of meson runs, the dependencies in the subdirectories
# sometimes skip deps that would be implied by others, e.g. if mempool is
# given as a dep, no need to mention ring. This is especially true for the
# core libs which are widely reused, so their deps are kept to a minimum.
libraries = [
	'kvargs', # eal depends on kvargs
	'eal', # everything depends on eal
	'cmdline', # ethdev depends on cmdline for parsing functions
	'ring', 'mempool', 'mbuf', 'net', 'meter', 'ethdev', 'pci', # core
	'metrics', # bitrate/latency stats depends on this
	'hash',    # efd depends on this
	'timer',   # eventdev depends on this
	'acl', 'bbdev', 'bitratestats', 'cfgfile',
	'compressdev', 'cryptodev',
	'distributor', 'efd', 'eventdev',
	'gro', 'gso', 'ip_frag', 'jobstats',
	'kni', 'latencystats', 'lpm', 'member',
	'power', 'pdump', 'rawdev',
	'rcu', 'reorder', 'sched', 'security', 'stack', 'vhost',
	#ipsec lib depends on crypto and security
	'ipsec',
	# add pkt framework libs which use other libs from above
	'port', 'table', 'pipeline',
	# flow_classify lib depends on pkt framework table lib
	'flow_classify', 'bpf', 'telemetry']

if is_windows
	libraries = ['kvargs','eal'] # only supported libraries for windows
endif

default_cflags = machine_args
if cc.has_argument('-Wno-format-truncation')
	default_cflags += '-Wno-format-truncation'
endif

enabled_libs = [] # used to print summary at the end

# -D_GNU_SOURCE unconditionally
default_cflags += '-D_GNU_SOURCE'

foreach l:libraries
	build = true
	name = l
	version = 1
	allow_experimental_apis = false
	sources = []
	headers = []
	includes = []
	cflags = default_cflags
	objs = [] # other object files to link against, used e.g. for
	          # instruction-set optimized versions of code

	# use "deps" for internal DPDK dependencies, and "ext_deps" for
	# external package/library requirements
	ext_deps = []
	deps = []
	# eal is standard dependency once built
	if dpdk_conf.has('RTE_LIBRTE_EAL')
		deps += ['eal']
	endif

	dir_name = 'librte_' + l
	subdir(dir_name)

	if build
		enabled_libs += name
		dpdk_conf.set('RTE_LIBRTE_' + name.to_upper(), 1)
		install_headers(headers)

		libname = 'rte_' + name
		includes += include_directories(dir_name)

		if sources.length() == 0
			# if no C files, just set a dependency on header path
			shared_dep = declare_dependency(include_directories: includes)
			static_dep = shared_dep
		else
			shared_deps = ext_deps
			static_deps = ext_deps
			foreach d:deps
				if not is_variable('shared_rte_' + d)
					error('Missing dependency ' + d +
						' for library ' + libname)
				endif
				shared_deps += [get_variable('shared_rte_' + d)]
				static_deps += [get_variable('static_rte_' + d)]
			endforeach

			if allow_experimental_apis
				cflags += '-DALLOW_EXPERIMENTAL_API'
			endif

			if get_option('per_library_versions')
				lib_version = '@0@.1'.format(version)
				so_version = '@0@'.format(version)
			else
				lib_version = major_version
				so_version = major_version
			endif

			# first build static lib
			static_lib = static_library(libname,
					sources,
					objects: objs,
					c_args: cflags,
					dependencies: static_deps,
					include_directories: includes,
					install: true)
			static_dep = declare_dependency(link_with: static_lib,
					include_directories: includes,
					dependencies: static_deps)

			# then use pre-build objects to build shared lib
			sources = []
			objs += static_lib.extract_all_objects(recursive: false)
			version_map = '@0@/@1@/rte_@2@_version.map'.format(
					meson.current_source_dir(), dir_name, name)
			exports = []
			implib = dir_name + '.dll.a'
			if is_windows
				exports = '@0@/@1@/rte_@2@_exports.def'.format(
					meson.current_source_dir(), dir_name, name)
				lk_args = ['-Wl,/def:' + exports, '-Wl,/implib:lib\\' + implib]
			else
				lk_args = ['-Wl,--version-script=' + version_map]
			endif
			shared_lib = shared_library(libname,
					sources,
					objects: objs,
					c_args: cflags,
					dependencies: shared_deps,
					include_directories: includes,
					link_args: lk_args,
					link_depends: [version_map, exports],
					version: lib_version,
					soversion: so_version,
					install: true)
			shared_dep = declare_dependency(link_with: shared_lib,
					include_directories: includes,
					dependencies: shared_deps)

			dpdk_libraries = [shared_lib] + dpdk_libraries
			dpdk_static_libraries = [static_lib] + dpdk_static_libraries
		endif # sources.length() > 0

		set_variable('shared_' + libname, shared_dep)
		set_variable('static_' + libname, static_dep)
	endif # if build
endforeach
