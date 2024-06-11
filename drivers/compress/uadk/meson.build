# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Huawei Technologies Co.,Ltd. All rights reserved.
# Copyright 2024-2025 Linaro ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

sources = files(
        'uadk_compress_pmd.c',
)

deps += 'bus_vdev'
dep = dependency('libwd_comp', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libwd_comp"'
else
    ext_deps += dep
endif

dep = dependency('libwd', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libwd"'
else
    ext_deps += dep
endif
