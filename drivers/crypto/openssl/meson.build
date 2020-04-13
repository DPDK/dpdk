# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

dep = dependency('libcrypto', required: false)
if not dep.found()
	build = false
	reason = 'missing dependency, "libcrypto"'
endif
deps += 'bus_vdev'
sources = files('rte_openssl_pmd.c', 'rte_openssl_pmd_ops.c')
ext_deps += dep
