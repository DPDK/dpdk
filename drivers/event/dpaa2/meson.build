# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif
deps += ['bus_vdev', 'pmd_dpaa2', 'pmd_dpaa2_sec']
sources = files('dpaa2_hw_dpcon.c',
		'dpaa2_eventdev.c',
		'dpaa2_eventdev_selftest.c')

includes += include_directories('../../crypto/dpaa2_sec/')
