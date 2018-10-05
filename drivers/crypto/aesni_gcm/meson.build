# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

lib = cc.find_library('IPSec_MB', required: false)
if not lib.found()
	build = false
else
	ext_deps += lib
endif

sources = files('aesni_gcm_pmd.c', 'aesni_gcm_pmd_ops.c')
deps += ['bus_vdev']
