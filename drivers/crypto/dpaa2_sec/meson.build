# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif

deps += ['security', 'mempool_dpaa2']
sources = files('dpaa2_sec_dpseci.c',
		'mc/dpseci.c')

# FIXME: temporary solution for Bugzilla 469
if (toolchain == 'gcc' and cc.version().version_compare('>=10.0.0'))
	cflags += '-fcommon'
endif

includes += include_directories('mc', '../../common/dpaax', '../../common/dpaax/caamflib')
