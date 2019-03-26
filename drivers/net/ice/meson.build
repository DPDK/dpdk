# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdir('base')
objs = [base_objs]

sources = files(
	'ice_ethdev.c',
	'ice_rxtx.c'
	)

deps += ['hash']
includes += include_directories('base')

if arch_subdir == 'x86'
	sources += files('ice_rxtx_vec_sse.c')
endif
