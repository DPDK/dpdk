# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if host_machine.system() != 'linux'
        build = false
endif

deps += ['mempool_dpaa2']
sources = files('base/dpaa2_hw_dpni.c',
		'dpaa2_ethdev.c',
		'dpaa2_rxtx.c',
		'mc/dpkg.c',
		'mc/dpni.c')

includes += include_directories('base', 'mc')

# depends on fslmc bus which uses experimental API
allow_experimental_apis = true
