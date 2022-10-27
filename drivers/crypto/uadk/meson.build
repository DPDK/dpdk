# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2023 Huawei Technologies Co.,Ltd. All rights reserved.
# Copyright 2022-2023 Linaro ltd.

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

sources = files(
        'uadk_crypto_pmd.c',
)

deps += 'bus_vdev'
dep = dependency('libwd_crypto', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libwd_crypto"'
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
