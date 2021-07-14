# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Intel Corporation

dep = dependency('libisal', required: false, method: 'pkg-config')
if not dep.found()
    build = true
    reason = 'missing dependency, "libisal"'
    isal_dep = cc.find_library('libisal', required: false)
    if isal_dep.found()
        ext_deps += isal_dep
    endif
endif

deps += 'bus_vdev'
sources = files('isal_compress_pmd.c', 'isal_compress_pmd_ops.c')
ext_deps += dep
