# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

if is_windows
	subdir_done()
endif

# Defines the order in which the drivers are buit.
dpdk_driver_classes = ['common',
	       'bus',
	       'mempool', # depends on common and bus.
	       'net',     # depends on common, bus and mempool.
	       'crypto',  # depends on common, bus and mempool (net in future).
	       'compress', # depends on common, bus, mempool.
	       'event',   # depends on common, bus, mempool and net.
	       'baseband', # depends on common and bus.
	       'raw']     # depends on common, bus, mempool, net and event.

default_cflags = machine_args
if cc.has_argument('-Wno-format-truncation')
	default_cflags += '-Wno-format-truncation'
endif

# specify -D_GNU_SOURCE unconditionally
default_cflags += '-D_GNU_SOURCE'

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
		name = drv
		version = 1
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

		if build
			class_drivers += name

			dpdk_conf.set(config_flag_fmt.format(name.to_upper()),1)
			lib_name = driver_name_fmt.format(name)

			if allow_experimental_apis
				cflags += '-DALLOW_EXPERIMENTAL_API'
			endif

			# get dependency objs from strings
			shared_objs = []
			static_objs = []
			foreach d:deps
				if not is_variable('shared_rte_' + d)
					error('Missing dependency ' + d +
						' for driver ' + lib_name)
				endif
				shared_objs += [get_variable('shared_rte_' + d)]
				static_objs += [get_variable('static_rte_' + d)]
			endforeach
			shared_objs += ext_deps
			static_objs += ext_deps
			dpdk_extra_ldflags += pkgconfig_extra_libs

			# generate pmdinfo sources by building a temporary
			# lib and then running pmdinfogen on the contents of
			# that lib. The final lib reuses the object files and
			# adds in the new source file.
			out_filename = lib_name + '.pmd.c'
			tmp_lib = static_library('tmp_' + lib_name,
					sources,
					include_directories: includes,
					dependencies: static_objs,
					c_args: cflags)
			objs += tmp_lib.extract_all_objects()
			sources = custom_target(out_filename,
					command: [pmdinfo, tmp_lib.full_path(),
						'@OUTPUT@', pmdinfogen],
					output: out_filename,
					depends: [pmdinfogen, tmp_lib])

			if get_option('per_library_versions')
				lib_version = '@0@.1'.format(version)
				so_version = '@0@'.format(version)
			else
				lib_version = major_version
				so_version = major_version
			endif

			# now build the static driver
			static_lib = static_library(lib_name,
				sources,
				objects: objs,
				include_directories: includes,
				dependencies: static_objs,
				c_args: cflags,
				install: true)

			# now build the shared driver
			version_map = '@0@/@1@/@2@_version.map'.format(
					meson.current_source_dir(),
					drv_path, lib_name)
			shared_lib = shared_library(lib_name,
				sources,
				objects: objs,
				include_directories: includes,
				dependencies: shared_objs,
				c_args: cflags,
				link_args: '-Wl,--version-script=' + version_map,
				link_depends: version_map,
				version: lib_version,
				soversion: so_version,
				install: true,
				install_dir: driver_install_path)

			# create a dependency object and add it to the global dictionary so
			# testpmd or other built-in apps can find it if necessary
			shared_dep = declare_dependency(link_with: shared_lib,
					include_directories: includes,
					dependencies: shared_objs)
			static_dep = declare_dependency(link_with: static_lib,
					include_directories: includes,
					dependencies: static_objs)

			dpdk_drivers += static_lib

			set_variable('shared_@0@'.format(lib_name), shared_dep)
			set_variable('static_@0@'.format(lib_name), static_dep)
		endif # build
	endforeach

	set_variable(class + '_drivers', class_drivers)
endforeach
