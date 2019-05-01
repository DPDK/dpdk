# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

lib = cc.find_library('sso_snow3g', required: false)
if not lib.found() or not cc.has_header('sso_snow3g.h')
	build = false
	subdir_done()
endif

allow_experimental_apis = true
ext_deps += lib
sources = files('rte_snow3g_pmd.c', 'rte_snow3g_pmd_ops.c')
deps += ['bus_vdev', 'cryptodev']
