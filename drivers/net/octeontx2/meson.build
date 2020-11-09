# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

if not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit'
	subdir_done()
endif

sources = files('otx2_rx.c',
		'otx2_tx.c',
		'otx2_tm.c',
		'otx2_rss.c',
		'otx2_mac.c',
		'otx2_ptp.c',
		'otx2_flow.c',
		'otx2_link.c',
		'otx2_vlan.c',
		'otx2_stats.c',
		'otx2_mcast.c',
		'otx2_lookup.c',
		'otx2_ethdev.c',
		'otx2_flow_ctrl.c',
		'otx2_flow_parse.c',
		'otx2_flow_utils.c',
		'otx2_ethdev_irq.c',
		'otx2_ethdev_ops.c',
		'otx2_ethdev_sec.c',
		'otx2_ethdev_debug.c',
		'otx2_ethdev_devargs.c'
		)

deps += ['bus_pci', 'cryptodev', 'eventdev', 'security']
deps += ['common_octeontx2', 'mempool_octeontx2']

extra_flags = ['-flax-vector-conversions']
foreach flag: extra_flags
	if cc.has_argument(flag)
		cflags += flag
	endif
endforeach

includes += include_directories('../../common/cpt')
includes += include_directories('../../crypto/octeontx2')
