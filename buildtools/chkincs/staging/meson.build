# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Red Hat, Inc.

stage_headers_cmd = py3 + files('stage-headers.py')

cleanup_target = custom_target('cleanup_staging',
        output: 'cleanup_staging.stamp',
        depend_files: dpdk_arch_headers + dpdk_arch_indirect_headers + dpdk_generic_headers +
            dpdk_headers + dpdk_indirect_headers + dpdk_drivers_headers,
        command: [stage_headers_cmd, '--cleanup', meson.current_build_dir(), '@OUTPUT@'],
        install: false,
)

subdir('drivers')

includes += include_directories('.')
deps += declare_dependency(sources:
    custom_target('arch_headers_staging',
            output: 'arch_headers_staging.stamp',
            depends: cleanup_target,
            command: [stage_headers_cmd, meson.current_build_dir(), '@OUTPUT@',
                dpdk_arch_headers + dpdk_arch_indirect_headers],
            install: false,
    )
)
deps += declare_dependency(sources:
    custom_target('headers_staging',
            output: 'headers_staging.stamp',
            depends: cleanup_target,
            command: [stage_headers_cmd, meson.current_build_dir(), '@OUTPUT@',
                dpdk_headers + dpdk_indirect_headers],
            install: false,
    )
)
deps += declare_dependency(sources:
    custom_target('generic_headers_staging',
            output: 'generic_headers_staging.stamp',
            depends: cleanup_target,
            command: [stage_headers_cmd, join_paths(meson.current_build_dir(), 'generic'),
                '@OUTPUT@', dpdk_generic_headers],
            install: false,
    )
)
