# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

dep = dependency('libcrypto', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libcrypto"'
endif
deps += 'bus_vdev'
sources = files('rte_openssl_pmd.c', 'rte_openssl_pmd_ops.c')
ext_deps += dep
require_iova_in_mbuf = false
