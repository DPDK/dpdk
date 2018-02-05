# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

subdir('base')
objs = [base_objs]

sources = files('octeontx_rxtx.c',
		'octeontx_ethdev.c'
		)

allow_experimental_apis = true

deps += ['mempool_octeontx', 'eventdev']

includes += include_directories('base')
