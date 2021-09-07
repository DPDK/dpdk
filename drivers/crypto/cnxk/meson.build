# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on 64-bit Linux'
	subdir_done()
endif

sources = files(
        'cn9k_cryptodev.c',
        'cn9k_cryptodev_ops.c',
        'cn9k_ipsec.c',
        'cn10k_cryptodev.c',
        'cn10k_cryptodev_ops.c',
        'cn10k_ipsec.c',
        'cnxk_cryptodev.c',
        'cnxk_cryptodev_capabilities.c',
        'cnxk_cryptodev_ops.c',
        'cnxk_cryptodev_sec.c',
)

deps += ['bus_pci', 'common_cnxk', 'security', 'eventdev']

includes += include_directories('../../../lib/net')
