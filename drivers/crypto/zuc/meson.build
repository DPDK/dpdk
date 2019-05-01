# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

lib = cc.find_library('sso_zuc', required: false)
if not lib.found() or not cc.has_header('sso_zuc.h')
	build = false
	subdir_done()
endif

allow_experimental_apis = true
ext_deps += lib
sources = files('rte_zuc_pmd.c', 'rte_zuc_pmd_ops.c')
deps += ['bus_vdev']
