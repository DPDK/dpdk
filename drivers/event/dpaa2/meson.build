# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if host_machine.system() != 'linux'
	build = false
endif
deps += ['bus_vdev', 'pmd_dpaa2']
sources = files('dpaa2_hw_dpcon.c',
		'dpaa2_eventdev.c')

allow_experimental_apis = true
