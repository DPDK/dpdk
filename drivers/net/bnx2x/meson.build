# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

dep = dependency('zlib', required: false, method: 'pkg-config')
build = dep.found()
reason = 'missing dependency, "zlib"'
ext_deps += dep
cflags += '-DZLIB_CONST'
sources = files(
        'bnx2x.c',
        'bnx2x_ethdev.c',
        'bnx2x_rxtx.c',
        'bnx2x_stats.c',
        'bnx2x_vfpf.c',
        'ecore_sp.c',
        'elink.c',
)

annotate_locks = false
