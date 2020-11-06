# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020 NXP

if not is_linux
	build = false
	reason = 'only supported on Linux'
endif
deps += ['mempool_dpaa']

sources = files('dpaa_ethdev.c',
		'fmlib/fm_lib.c',
		'fmlib/fm_vsp.c',
		'dpaa_flow.c',
		'dpaa_rxtx.c',
		'dpaa_fmc.c')

if cc.has_argument('-Wno-pointer-arith')
	cflags += '-Wno-pointer-arith'
endif

headers = files('rte_pmd_dpaa.h')
