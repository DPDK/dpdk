# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

subdir('base')
objs = [base_objs]

sources = files('octeontx_rxtx.c',
		'octeontx_ethdev.c',
		'octeontx_ethdev_ops.c'
		)

deps += ['mempool_octeontx', 'eventdev']

includes += include_directories('base')
