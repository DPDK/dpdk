# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2019 Marvell International Ltd.

if host_machine.system() != 'linux'
        build = false
endif

deps += ['bus_pci']
deps += ['common_cpt']
deps += ['common_octeontx2']
name = 'octeontx2_crypto'

sources = files('otx2_cryptodev.c',
		'otx2_cryptodev_ops.c')

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
