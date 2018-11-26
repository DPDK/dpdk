# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

project('DPDK', 'C',
	version: '18.11.0',
	license: 'BSD',
	default_options: ['buildtype=release', 'default_library=static'],
	meson_version: '>= 0.41'
)

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
dpdk_conf = configuration_data()
dpdk_libraries = []
dpdk_static_libraries = []
dpdk_drivers = []
dpdk_extra_ldflags = []
dpdk_app_link_libraries = []

# set the major version, which might be used by drivers and libraries
# depending on the configuration options
pver = meson.project_version().split('.')
major_version = '@0@.@1@'.format(pver.get(0), pver.get(1))

pmd_subdir_opt = get_option('drivers_install_subdir')
if pmd_subdir_opt.contains('<VERSION>')
	pmd_subdir_opt = major_version.join(pmd_subdir_opt.split('<VERSION>'))
endif
driver_install_path = join_paths(get_option('libdir'), pmd_subdir_opt)
eal_pmd_path = join_paths(get_option('prefix'), driver_install_path)

# configure the build, and make sure configs here and in config folder are
# able to be included in any file. We also store a global array of include dirs
# for passing to pmdinfogen scripts
global_inc = include_directories('.', 'config')
subdir('config')

# build libs and drivers
subdir('lib')
subdir('buildtools')
subdir('drivers')

# build binaries and installable tools
subdir('usertools')
subdir('app')
subdir('test')

# build docs
subdir('doc')

# build any examples explicitly requested - useful for developers
if get_option('examples') != ''
	subdir('examples')
endif

# build kernel modules if enabled
if get_option('enable_kmods')
	subdir('kernel')
endif

# write the build config
build_cfg = 'rte_build_config.h'
configure_file(output: build_cfg,
		configuration: dpdk_conf,
		install_dir: join_paths(get_option('includedir'),
				get_option('include_subdir_arch')))

# for static builds, include the drivers as libs and we need to "whole-archive"
# them.
dpdk_drivers = ['-Wl,--whole-archive'] + dpdk_drivers + ['-Wl,--no-whole-archive']

# driver .so files often depend upon the bus drivers for their connect bus,
# e.g. ixgbe depends on librte_bus_pci. This means that the bus drivers need
# to be in the library path, so symlink the drivers from the main lib directory.
meson.add_install_script('buildtools/symlink-drivers-solibs.sh',
		driver_install_path,
		get_option('libdir'))

pkg = import('pkgconfig')
pkg.generate(name: meson.project_name(),
	filebase: 'lib' + meson.project_name().to_lower(),
	version: meson.project_version(),
	libraries: dpdk_libraries,
	libraries_private: dpdk_drivers + dpdk_libraries +
			['-Wl,-Bdynamic'] + dpdk_extra_ldflags,
	description: 'The Data Plane Development Kit (DPDK)',
	subdirs: [get_option('include_subdir_arch'), '.'],
	extra_cflags: ['-include', 'rte_config.h'] + machine_args
)

# final output, list all the libs and drivers to be built
# this does not affect any part of the build, for information only.
output_message = '\n=================\nLibraries Enabled\n=================\n'
output_message += '\nlibs:\n\t'
output_count = 0
foreach lib:enabled_libs
	output_message += lib + ', '
	output_count += 1
	if output_count == 8
		output_message += '\n\t'
		output_count = 0
	endif
endforeach
message(output_message + '\n')


# prior to 0.47 set_variable didn't work with arrays, so we can't
# track driver lists easily
if meson.version().version_compare('>=0.47')
	output_message = '\n===============\nDrivers Enabled\n===============\n'
	foreach class:driver_classes
		class_drivers = get_variable(class + '_drivers')
		output_message += '\n' + class + ':\n\t'
		output_count = 0
		foreach drv:class_drivers
			output_message += drv + ', '
			output_count += 1
			if output_count == 8
				output_message += '\n\t'
				output_count = 0
			endif
		endforeach
	endforeach
	message(output_message + '\n')
endif
