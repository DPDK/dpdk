# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif

deps += ['common_dpaax', 'eventdev', 'kvargs']
sources = files('fslmc_bus.c',
		'fslmc_vfio.c',
		'mc/dpbp.c',
		'mc/dpci.c',
		'mc/dpcon.c',
		'mc/dpdmai.c',
		'mc/dpio.c',
		'mc/dpmng.c',
		'mc/mc_sys.c',
		'portal/dpaa2_hw_dpbp.c',
		'portal/dpaa2_hw_dpci.c',
		'portal/dpaa2_hw_dpio.c',
		'qbman/qbman_portal.c',
		'qbman/qbman_debug.c')

includes += include_directories('mc', 'qbman/include', 'portal')
