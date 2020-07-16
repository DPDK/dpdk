# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2019 Marvell International Ltd.

if not is_linux
	build = false
	reason = 'only supported on Linux'
endif

deps += ['bus_pci']
deps += ['common_cpt']
deps += ['common_octeontx2']
deps += ['ethdev']
deps += ['security']
name = 'octeontx2_crypto'

sources = files('otx2_cryptodev.c',
		'otx2_cryptodev_capabilities.c',
		'otx2_cryptodev_hw_access.c',
		'otx2_cryptodev_mbox.c',
		'otx2_cryptodev_ops.c',
		'otx2_cryptodev_sec.c')

extra_flags = []
# This integrated controller runs only on a arm64 machine, remove 32bit warnings
if not dpdk_conf.get('RTE_ARCH_64')
	extra_flags += ['-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast']
endif

foreach flag: extra_flags
	if cc.has_argument(flag)
		cflags += flag
	endif
endforeach

includes += include_directories('../../common/cpt')
includes += include_directories('../../common/octeontx2')
includes += include_directories('../../crypto/octeontx2')
includes += include_directories('../../mempool/octeontx2')
includes += include_directories('../../net/octeontx2')
