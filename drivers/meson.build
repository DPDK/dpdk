# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

if is_windows
	subdir_done()
endif

# Defines the order in which the drivers are buit.
dpdk_driver_classes = ['common',
	       'bus',
	       'mempool', # depends on common and bus.
	       'net',     # depends on common, bus, mempool
	       'raw',     # depends on common, bus and net.
	       'crypto',  # depends on common, bus and mempool (net in future).
	       'compress', # depends on common, bus, mempool.
	       'vdpa',    # depends on common, bus and mempool.
	       'event',   # depends on common, bus, mempool and net.
	       'baseband'] # depends on common and bus.

disabled_drivers = get_option('disable_drivers').split(',')

default_cflags = machine_args
if cc.has_argument('-Wno-format-truncation')
	default_cflags += '-Wno-format-truncation'
endif

foreach class:dpdk_driver_classes
	drivers = []
	std_deps = []
	config_flag_fmt = '' # format string used to set the value in dpdk_conf
	driver_name_fmt = '' # format string for driver name, used to name
	                     # the library, the dependency and to find the
	                     # version file for linking

	subdir(class)
	class_drivers = []

	foreach drv:drivers
		drv_path = join_paths(class, drv)

		# set up empty variables used for build
		build = true # set to false to disable, e.g. missing deps
		reason = '<unknown reason>' # set if build == false to explain
		name = drv
		fmt_name = ''
		allow_experimental_apis = false
		sources = []
		objs = []
		cflags = default_cflags
		includes = [include_directories(drv_path)]
		# set up internal deps. Drivers can append/override as necessary
		deps = std_deps
		# ext_deps: Stores external library dependency got
		# using dependency() (preferred) or find_library().
		# For the find_library() case (but not with dependency()) we also
		# need to specify the "-l" flags in pkgconfig_extra_libs variable
		# too, so that it can be reflected in the pkgconfig output for
		# static builds.
		ext_deps = []
		pkgconfig_extra_libs = []

		# pull in driver directory which should assign to each of the above
		subdir(drv_path)

		# skip disabled drivers. For meson 0.49 change this to use
		# "in" keyword
		foreach disable_path: disabled_drivers
			if drv_path == disable_path
				build = false
				reason = 'Explicitly disabled via build config'
			endif
		endforeach
		if build
			# get dependency objs from strings
			shared_deps = ext_deps
			static_deps = ext_deps
			foreach d:deps
				if not is_variable('shared_rte_' + d)
					build = false
					reason = 'Missing internal dependency, "@0@"'.format(d)
					message('Disabling @1@ [@2@]: missing internal dependency "@0@"'
							.format(d, name, 'drivers/' + drv_path))
				else
					shared_deps += [get_variable('shared_rte_' + d)]
					static_deps += [get_variable('static_rte_' + d)]
				endif
			endforeach
		endif

		if not build
			# some driver directories are placeholders which
			# are never built, so we allow suppression of the
			# component disable printout in those cases
			if reason != ''
				dpdk_drvs_disabled += drv_path
				set_variable(drv_path.underscorify() +
						'_disable_reason', reason)
			endif
		else
			class_drivers += name

			if fmt_name == ''
				fmt_name = name
			endif
			dpdk_conf.set(config_flag_fmt.format(fmt_name.to_upper()),1)
			lib_name = driver_name_fmt.format(fmt_name)

			if allow_experimental_apis
				cflags += '-DALLOW_EXPERIMENTAL_API'
			endif

			dpdk_extra_ldflags += pkgconfig_extra_libs

			# generate pmdinfo sources by building a temporary
			# lib and then running pmdinfogen on the contents of
			# that lib. The final lib reuses the object files and
			# adds in the new source file.
			out_filename = lib_name + '.pmd.c'
			tmp_lib = static_library('tmp_' + lib_name,
					sources,
					include_directories: includes,
					dependencies: static_deps,
					c_args: cflags)
			objs += tmp_lib.extract_all_objects()
			sources = custom_target(out_filename,
					command: [pmdinfo, tmp_lib.full_path(),
						'@OUTPUT@', pmdinfogen],
					output: out_filename,
					depends: [pmdinfogen, tmp_lib])

			version_map = '@0@/@1@/@2@_version.map'.format(
					meson.current_source_dir(),
					drv_path, lib_name)

			is_experimental = run_command(is_experimental_cmd,
				files(version_map)).returncode()

			if is_experimental != 0
				lib_version = experimental_abi_version
				so_version = experimental_so_version
			else
				lib_version = abi_version
				so_version = stable_so_version
			endif

			# now build the static driver
			static_lib = static_library(lib_name,
				sources,
				objects: objs,
				include_directories: includes,
				dependencies: static_deps,
				c_args: cflags,
				install: true)

			# now build the shared driver
			version_map = '@0@/@1@/@2@_version.map'.format(
					meson.current_source_dir(),
					drv_path, lib_name)
			implib = dir_name + '.dll.a'

			def_file = custom_target(lib_name + '_def',
				command: [map_to_def_cmd, '@INPUT@', '@OUTPUT@'],
				input: version_map,
				output: '@0@_exports.def'.format(lib_name))
			lk_deps = [version_map, def_file]
			if is_windows
				lk_args = ['-Wl,/def:' + def_file.full_path(),
					'-Wl,/implib:lib\\' + implib]
			else
				lk_args = ['-Wl,--version-script=' + version_map]
				# on unix systems check the output of the
				# experimental syms script, using it as a
				# dependency of the .so build
				lk_deps += custom_target(lib_name + '.exp_chk',
					command: [check_experimental_syms,
						version_map, '@INPUT@'],
					capture: true,
					input: static_lib,
					output: lib_name + '.exp_chk')
			endif

			shared_lib = shared_library(lib_name,
				sources,
				objects: objs,
				include_directories: includes,
				dependencies: shared_deps,
				c_args: cflags,
				link_args: lk_args,
				link_depends: lk_deps,
				version: lib_version,
				soversion: so_version,
				install: true,
				install_dir: driver_install_path)

			# create a dependency object and add it to the global dictionary so
			# testpmd or other built-in apps can find it if necessary
			shared_dep = declare_dependency(link_with: shared_lib,
					include_directories: includes,
					dependencies: shared_deps)
			static_dep = declare_dependency(link_with: static_lib,
					include_directories: includes,
					dependencies: static_deps)

			dpdk_drivers += static_lib

			set_variable('shared_@0@'.format(lib_name), shared_dep)
			set_variable('static_@0@'.format(lib_name), static_dep)
			dependency_name = ''.join(lib_name.split('rte_'))
			message('drivers/@0@: Defining dependency "@1@"'.format(
					drv_path, dependency_name))
		endif # build
	endforeach

	set_variable(class + '_drivers', class_drivers)
endforeach
