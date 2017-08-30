#   BSD LICENSE
#
#   Copyright(c) 2017 Intel Corporation.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

driver_classes = ['bus', 'crypto', 'mempool', 'net']

foreach class:driver_classes
	drivers = []
	std_deps = []
	config_flag_fmt = '' # format string used to set the value in dpdk_conf
	driver_name_fmt = '' # format string for driver name, used to name
	                     # the library, the dependency and to find the
	                     # version file for linking

	subdir(class)

	foreach drv:drivers
		drv_path = join_paths(class, drv)

		# set up empty variables used for build
		build = true # set to false to disable, e.g. missing deps
		name = drv
		version = 1
		allow_experimental_apis = false
		sources = []
		objs = []
		cflags = []
		includes = [include_directories(drv_path)]
		# set up internal deps. Drivers can append/override as necessary
		deps = std_deps
		# ext_deps: Stores external library dependency got
		# using dependency() or cc.find_library(). For most cases, we
		# probably also need to specify the "-l" flags in
		# pkgconfig_extra_libs variable too, so that it can be reflected
		# in the pkgconfig output for static builds
		ext_deps = []
		pkgconfig_extra_libs = []

		# pull in driver directory which should assign to each of the above
		subdir(drv_path)

		if build
			dpdk_conf.set(config_flag_fmt.format(name.to_upper()),1)

			if allow_experimental_apis
				cflags += '-DALLOW_EXPERIMENTAL_API'
			endif

			# get dependency objs from strings
			dep_objs = []
			foreach d:deps
				dep_objs += [get_variable('dep_rte_' + d)]
			endforeach
			dep_objs += ext_deps
			dpdk_extra_ldflags += pkgconfig_extra_libs

			# generate pmdinfo sources
			pmdinfogen_srcs = run_command('grep', '--files-with-matches',
				'RTE_PMD_REGISTER_.*(.*)', sources).stdout().strip().split()
			foreach src: pmdinfogen_srcs
				out_filename = '@0@.pmd.c'.format(src.split('/')[-1])
				tmp_lib = static_library('tmp_@0@'.format(src.underscorify()),
					src, include_directories: includes,
					dependencies: dep_objs,
					c_args: cflags)
				sources += custom_target(out_filename,
						command: [pmdinfo, tmp_lib.full_path(),
							'@OUTPUT@', pmdinfogen],
						output: out_filename,
						depends: [pmdinfogen, tmp_lib])
			endforeach

			# now build the driver itself, and add to the drivers list
			lib_name = driver_name_fmt.format(name)
			version_map = '@0@/@1@/@2@_version.map'.format(
					meson.current_source_dir(),
					drv_path, lib_name)
			lib = library(lib_name,
				sources,
				objects: objs,
				include_directories: includes,
				dependencies: dep_objs,
				c_args: cflags,
				link_args: '-Wl,--version-script=' + version_map,
				link_depends: version_map,
				version: '@0@.1'.format(version),
				install: true,
				install_dir: driver_install_path)

			dpdk_drivers += lib

			# create a dependency object and add it to the global dictionary so
			# testpmd or other built-in apps can find it if necessary
			set_variable('dep_@0@'.format(lib_name),
					declare_dependency(link_with: lib,
					include_directories: includes,
					dependencies: dep_objs))
		endif # build
	endforeach
endforeach
