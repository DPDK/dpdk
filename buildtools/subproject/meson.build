# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation

message('DPDK subproject linking: ' + get_option('default_library'))
subproject_cflags = ['-include', 'rte_config.h'] + machine_args
if is_freebsd
    subproject_cflags += ['-D__BSD_VISIBLE']
endif
if get_option('default_library') == 'static'
    dpdk_dep = declare_dependency(
            version: meson.project_version(),
            dependencies: dpdk_static_lib_deps,
            compile_args: subproject_cflags,
            # static library deps in DPDK build don't include "link_with" parameters,
            # so explicitly link-in both libs and drivers
            link_whole: dpdk_static_libraries + dpdk_drivers,
            link_args: dpdk_extra_ldflags)
else
    dpdk_dep = declare_dependency(
            version: meson.project_version(),
            compile_args: subproject_cflags,
            # shared library deps include all necessary linking parameters
            dependencies: dpdk_shared_lib_deps)
endif

libdpdk_dep = dpdk_dep
