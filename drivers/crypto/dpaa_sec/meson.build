# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif

deps += ['bus_dpaa', 'mempool_dpaa', 'security']
sources = files('dpaa_sec.c')

includes += include_directories('../../bus/dpaa/include')
includes += include_directories('../../common/dpaax')
includes += include_directories('../../common/dpaax/caamflib/')
