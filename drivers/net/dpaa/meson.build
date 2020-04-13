# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif
deps += ['mempool_dpaa']

sources = files('dpaa_ethdev.c',
		'dpaa_rxtx.c')

if cc.has_argument('-Wno-pointer-arith')
	cflags += '-Wno-pointer-arith'
endif

install_headers('rte_pmd_dpaa.h')
