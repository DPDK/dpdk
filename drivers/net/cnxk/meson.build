# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit'
    subdir_done()
endif

sources = files(
        'cnxk_ethdev.c',
        'cnxk_ethdev_devargs.c',
        'cnxk_ethdev_ops.c',
        'cnxk_link.c',
        'cnxk_lookup.c',
)

# CN9K
sources += files(
        'cn9k_ethdev.c',
        'cn9k_rx.c',
        'cn9k_rx_mseg.c',
)
# CN10K
sources += files(
        'cn10k_ethdev.c',
)

deps += ['bus_pci', 'cryptodev', 'eventdev', 'security']
deps += ['common_cnxk', 'mempool_cnxk']
