# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if not is_linux
	build = false
	reason = 'only supported on linux'
endif

deps += ['mempool_dpaa2']
sources = files('base/dpaa2_hw_dpni.c',
		'dpaa2_mux.c',
		'dpaa2_ethdev.c',
		'dpaa2_flow.c',
		'dpaa2_rxtx.c',
		'dpaa2_sparser.c',
		'mc/dpkg.c',
		'mc/dpdmux.c',
		'mc/dpni.c')

if dpdk_conf.has('RTE_LIBRTE_IEEE1588')
	sources += files('mc/dprtc.c')
	sources += files('dpaa2_ptp.c')
endif

includes += include_directories('base', 'mc')

install_headers('rte_pmd_dpaa2.h')
