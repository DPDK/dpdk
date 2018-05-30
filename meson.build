# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

project('DPDK', 'C',
	version: '18.05.0',
	license: 'BSD',
	default_options: ['buildtype=release', 'default_library=static'],
	meson_version: '>= 0.41'
)

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
dpdk_conf = configuration_data()
dpdk_libraries = []
dpdk_drivers = []
dpdk_extra_ldflags = []

driver_install_path = join_paths(get_option('libdir'), 'dpdk/drivers')
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
