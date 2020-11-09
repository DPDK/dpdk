# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

if is_windows
	build = false
	reason = 'not supported on Windows'
	subdir_done()
endif
if not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit'
	subdir_done()
endif

sources = files('otx2_mempool_ops.c',
		'otx2_mempool.c',
		'otx2_mempool_irq.c',
		'otx2_mempool_debug.c'
		)

deps += ['eal', 'mbuf', 'kvargs', 'bus_pci', 'common_octeontx2', 'mempool']
