# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif
deps += ['pmd_dpaa']
sources = files('dpaa_eventdev.c')

allow_experimental_apis = true
