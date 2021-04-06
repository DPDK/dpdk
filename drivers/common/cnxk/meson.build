# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Marvell, Inc
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit Linux'
	subdir_done()
endif

config_flag_fmt = 'RTE_LIBRTE_@0@_COMMON'
deps = ['eal', 'pci', 'bus_pci', 'mbuf']
sources = files('roc_irq.c',
		'roc_mbox.c',
		'roc_model.c',
		'roc_platform.c',
		'roc_utils.c')
includes += include_directories('../../bus/pci')
