# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation

message('DPDK subproject linking: ' + get_option('default_library'))
if get_option('default_library') == 'static'
    dpdk_dep = declare_dependency(
            version: meson.project_version(),
            dependencies: dpdk_static_lib_deps,
            # static library deps in DPDK build don't include "link_with" parameters,
            # so explicitly link-in both libs and drivers
            link_with: dpdk_static_libraries,
            link_whole: dpdk_drivers,
            link_args: dpdk_extra_ldflags)
else
    dpdk_dep = declare_dependency(
            version: meson.project_version(),
            # shared library deps include all necessary linking parameters
            dependencies: dpdk_shared_lib_deps)
endif

libdpdk_dep = dpdk_dep
