# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 NXP

if not is_linux
	build = false
	reason = 'only supported on Linux'
endif
deps += ['common_dpaax']

sources = files('pfe_ethdev.c',
		'pfe_hal.c',
		'pfe_hif_lib.c',
		'pfe_hif.c')

if cc.has_argument('-Wno-pointer-arith')
	cflags += '-Wno-pointer-arith'
endif

includes += include_directories('base')
