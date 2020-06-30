# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

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
