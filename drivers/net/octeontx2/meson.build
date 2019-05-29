# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

sources = files(
		'otx2_rss.c',
		'otx2_mac.c',
		'otx2_link.c',
		'otx2_stats.c',
		'otx2_ethdev.c',
		'otx2_ethdev_irq.c',
		'otx2_ethdev_ops.c',
		'otx2_ethdev_debug.c',
		'otx2_ethdev_devargs.c'
		)

deps += ['bus_pci', 'common_octeontx2', 'mempool_octeontx2']

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
