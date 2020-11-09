# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2019 Marvell International Ltd.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit Linux'
	subdir_done()
endif

deps += ['bus_pci']
deps += ['common_cpt']
deps += ['common_octeontx2']
deps += ['ethdev']
deps += ['eventdev']
deps += ['security']
fmt_name = 'octeontx2_crypto'

sources = files('otx2_cryptodev.c',
		'otx2_cryptodev_capabilities.c',
		'otx2_cryptodev_hw_access.c',
		'otx2_cryptodev_mbox.c',
		'otx2_cryptodev_ops.c',
		'otx2_cryptodev_sec.c')

includes += include_directories('../../common/cpt')
includes += include_directories('../../common/octeontx2')
includes += include_directories('../../crypto/octeontx2')
includes += include_directories('../../mempool/octeontx2')
includes += include_directories('../../net/octeontx2')
