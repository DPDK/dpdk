# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Intel Corporation

dep = dependency('libisal', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libisal"'
endif

deps += 'bus_vdev'
sources = files('isal_compress_pmd.c', 'isal_compress_pmd_ops.c')
ext_deps += dep
