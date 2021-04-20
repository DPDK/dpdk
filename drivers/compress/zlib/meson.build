# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium Networks

dep = dependency('zlib', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "zlib"'
endif

deps += 'bus_vdev'
sources = files('zlib_pmd.c', 'zlib_pmd_ops.c')
ext_deps += dep
