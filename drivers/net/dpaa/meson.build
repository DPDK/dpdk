# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if host_machine.system() != 'linux'
	build = false
endif
deps += ['mempool_dpaa']

sources = files('dpaa_ethdev.c',
		'dpaa_rxtx.c')

allow_experimental_apis = true

install_headers('rte_pmd_dpaa.h')
