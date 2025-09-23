# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Red Hat, Inc.

includes_drivers += include_directories('.')
deps_drivers += declare_dependency(sources:
    custom_target('drivers_headers_staging',
            output: 'drivers_headers_staging.stamp',
            depends: cleanup_target,
            command: [stage_headers_cmd, meson.current_build_dir(), '@OUTPUT@',
                dpdk_drivers_headers],
            install: false,
    )
)
