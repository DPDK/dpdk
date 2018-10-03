# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if host_machine.system() != 'linux'
	build = false
endif

sources = files('enetc_ethdev.c',
		'enetc_rxtx.c')

includes += include_directories('base')
