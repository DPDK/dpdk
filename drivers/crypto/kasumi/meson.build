# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

lib = cc.find_library('sso_kasumi', required: false)
if not lib.found() or not cc.has_header('sso_kasumi.h')
	build = false
	subdir_done()
endif

allow_experimental_apis = true
ext_deps += lib
sources = files('rte_kasumi_pmd.c', 'rte_kasumi_pmd_ops.c')
deps += ['bus_vdev']
