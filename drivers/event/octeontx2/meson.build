# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

sources = files('otx2_evdev.c',
		'otx2_evdev_irq.c',
		)

allow_experimental_apis = true

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

deps += ['bus_pci', 'common_octeontx2', 'mempool_octeontx2']
