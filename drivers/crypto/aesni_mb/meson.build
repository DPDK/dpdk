# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

lib = cc.find_library('IPSec_MB', required: false)
if not lib.found()
	build = false
else
	ext_deps += lib
endif

sources = files('rte_aesni_mb_pmd_compat.c', 'rte_aesni_mb_pmd_ops_compat.c')
deps += ['bus_vdev']
