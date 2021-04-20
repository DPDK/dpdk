# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

dep = dependency('libsze2', required: false, method: 'pkg-config')
build = dep.found()
reason = 'missing dependency, "libsze2"'
ext_deps += dep
sources = files('rte_eth_szedata2.c')
