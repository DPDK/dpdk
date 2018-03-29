# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if host_machine.system() != 'linux'
	build = false
endif
deps += ['pmd_dpaa']
sources = files('dpaa_eventdev.c')

allow_experimental_apis = true
