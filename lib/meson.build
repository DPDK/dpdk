# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation


# process all libraries equally, as far as possible
# "core" libs first, then others alphebetically as far as possible
# NOTE: for speed of meson runs, the dependencies in the subdirectories
# sometimes skip deps that would be implied by others, e.g. if mempool is
# given as a dep, no need to mention ring. This is especially true for the
# core libs which are widely reused, so their deps are kept to a minimum.
libraries = [ 'compat', # just a header, used for versioning
	'eal', 'ring', 'mempool', 'mbuf', 'net', 'kvargs', 'ethdev', 'pci', # core
	'metrics', # bitrate/latency stats depends on this
	'hash',    # efd depends on this
	'timer',   # eventdev depends on this
	'acl', 'bbdev', 'bitratestats', 'cfgfile',
	'cmdline', 'compressdev', 'cryptodev',
	'distributor', 'efd', 'eventdev',
	'gro', 'gso', 'ip_frag', 'jobstats',
	'kni', 'latencystats', 'lpm', 'member',
	'meter', 'power', 'pdump', 'rawdev',
	'reorder', 'sched', 'security', 'vhost',
	# add pkt framework libs which use other libs from above
	'port', 'table', 'pipeline',
	# flow_classify lib depends on pkt framework table lib
	'flow_classify', 'bpf']

foreach l:libraries
	build = true
	name = l
	version = 1
	allow_experimental_apis = false
	sources = []
	headers = []
	includes = []
	cflags = machine_args
	objs = [] # other object files to link against, used e.g. for
	          # instruction-set optimized versions of code

	# use "deps" for internal DPDK dependencies, and "ext_deps" for
	# external package/library requirements
	ext_deps = []
	deps = ['eal']   # eal is standard dependency except for itself
	if l == 'eal'
		deps = []
	endif

	dir_name = 'librte_' + l
	subdir(dir_name)

	if build
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
				prj_ver = meson.project_version().split('.')
				lib_version = '@0@.@1@'.format(
						prj_ver.get(0), prj_ver.get(1))
				so_version = lib_version
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
			shared_lib = shared_library(libname,
					sources,
					objects: objs,
					c_args: cflags,
					dependencies: shared_deps,
					include_directories: includes,
					link_args: '-Wl,--version-script=' + version_map,
					link_depends: version_map,
					version: lib_version,
					soversion: so_version,
					install: true)
			shared_dep = declare_dependency(link_with: shared_lib,
					include_directories: includes,
					dependencies: shared_deps)

			dpdk_libraries = [shared_lib] + dpdk_libraries
		endif # sources.length() > 0

		set_variable('shared_' + libname, shared_dep)
		set_variable('static_' + libname, static_dep)
	endif # if build
endforeach
