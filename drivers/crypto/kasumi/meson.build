# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

lib = cc.find_library('libsso_kasumi', required: false)
if not lib.found()
	build = false
else
	ext_deps += lib
endif

sources = files('rte_kasumi_pmd.c', 'rte_kasumi_pmd_ops.c')
deps += ['bus_vdev']
