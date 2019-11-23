# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

project('DPDK', 'C',
	# Get version number from file.
	# Fallback to "more" for Windows compatibility.
	version: run_command(find_program('cat', 'more'),
		files('VERSION')).stdout().strip(),
	license: 'BSD',
	default_options: ['buildtype=release', 'default_library=static'],
	meson_version: '>= 0.47.1'
)

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
dpdk_conf = configuration_data()
dpdk_libraries = []
dpdk_static_libraries = []
dpdk_driver_classes = []
dpdk_drivers = []
dpdk_extra_ldflags = []
dpdk_app_link_libraries = []
dpdk_libs_disabled = []
dpdk_drvs_disabled = []
abi_version_file = files('ABI_VERSION')

# configure the build, and make sure configs here and in config folder are
# able to be included in any file. We also store a global array of include dirs
# for passing to pmdinfogen scripts
global_inc = include_directories('.', 'config',
	'lib/librte_eal/common/include',
	'lib/librte_eal/@0@/eal/include'.format(host_machine.system()),
)
subdir('config')

# build libs and drivers
subdir('buildtools')
subdir('lib')
subdir('drivers')

# build binaries and installable tools
subdir('usertools')
subdir('app')

# build docs
subdir('doc')

# build any examples explicitly requested - useful for developers - and
# install any example code into the appropriate install path
subdir('examples')

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

pkg = import('pkgconfig')
pkg_extra_cflags = ['-include', 'rte_config.h'] + machine_args
if is_freebsd
	pkg_extra_cflags += ['-D__BSD_VISIBLE']
endif
pkg.generate(name: meson.project_name(),
	filebase: 'lib' + meson.project_name().to_lower(),
	version: meson.project_version(),
	libraries: dpdk_libraries,
	libraries_private: dpdk_drivers + dpdk_static_libraries +
			['-Wl,-Bdynamic'] + dpdk_extra_ldflags,
	requires: libbsd, # apps using rte_string_fns.h may need this if enabled
	                  # if libbsd is not enabled, then this is blank
	description: '''The Data Plane Development Kit (DPDK).
Note that CFLAGS might contain an -march flag higher than typical baseline.
This is required for a number of static inline functions in the public headers.''',
	subdirs: [get_option('include_subdir_arch'), '.'],
	extra_cflags: pkg_extra_cflags
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

output_message = '\n===============\nDrivers Enabled\n===============\n'
foreach class:dpdk_driver_classes
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

output_message = '\n=================\nContent Skipped\n=================\n'
output_message += '\nlibs:\n\t'
foreach lib:dpdk_libs_disabled
	reason = get_variable(lib.underscorify() + '_disable_reason')
	output_message += lib + ':\t' + reason + '\n\t'
endforeach
output_message += '\ndrivers:\n\t'
foreach drv:dpdk_drvs_disabled
	reason = get_variable(drv.underscorify() + '_disable_reason')
	output_message += drv + ':\t' + reason + '\n\t'
endforeach
message(output_message + '\n')
