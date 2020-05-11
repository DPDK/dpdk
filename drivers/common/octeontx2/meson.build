# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

sources= files('otx2_dev.c',
		'otx2_irq.c',
		'otx2_mbox.c',
		'otx2_common.c',
		'otx2_sec_idev.c',
	       )

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

deps = ['eal', 'pci', 'ethdev', 'kvargs']
includes += include_directories('../../common/octeontx2',
		'../../mempool/octeontx2', '../../bus/pci')
