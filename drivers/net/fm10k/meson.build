# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

subdir('base')
objs = [base_objs]

sources = files(
	'fm10k_ethdev.c',
	'fm10k_rxtx.c',
)
if arch_subdir == 'x86'
	dpdk_conf.set('RTE_LIBRTE_FM10K_INC_VECTOR', 1)
	sources += files('fm10k_rxtx_vec.c')
endif

includes += include_directories('base')
