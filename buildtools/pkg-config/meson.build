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

# When calling pkg-config --static --libs, pkg-config will always output the
# regular libs first, and then the extra libs from Libs.private field,
# since the assumption is that those are additional dependencies for building
# statically that the .a files depend upon. The output order of .pc fields is:
#   Libs   Libs.private   Requires   Requires.private
# The fields Requires* are for package names.
# The flags of the DPDK libraries must be defined in Libs* fields.
# However, the DPDK drivers are linked only in static builds (Libs.private),
# and those need to come *before* the regular libraries (Libs field).
# This requirement is satisfied by moving the regular libs in a separate file
# included in the field Requires (after Libs.private).

pkg.generate(name: 'dpdk-libs',
	filebase: 'libdpdk-libs',
	description: '''Internal-only DPDK pkgconfig file. Not for direct use.
Use libdpdk.pc instead of this file to query DPDK compile/link arguments''',
	version: meson.project_version(),
	subdirs: [get_option('include_subdir_arch'), '.'],
	extra_cflags: pkg_extra_cflags,
	libraries: dpdk_libraries,
	libraries_private: dpdk_extra_ldflags)

pkg.generate(name: 'DPDK', # main DPDK pkgconfig file
	filebase: 'libdpdk',
	version: meson.project_version(),
	description: '''The Data Plane Development Kit (DPDK).
Note that CFLAGS might contain an -march flag higher than typical baseline.
This is required for a number of static inline functions in the public headers.''',
	requires: ['libdpdk-libs', libbsd], # may need libbsd for string funcs
	                  # if libbsd is not enabled, then this is blank
	libraries_private: dpdk_drivers + dpdk_static_libraries +
			['-Wl,-Bdynamic']
)
