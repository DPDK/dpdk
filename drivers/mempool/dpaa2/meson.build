# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif

deps += ['bus_fslmc']
sources = files('dpaa2_hw_mempool.c')

install_headers('rte_dpaa2_mempool.h')
