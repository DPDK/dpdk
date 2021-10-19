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
        'cnxk_ethdev_mtr.c',
        'cnxk_ethdev_ops.c',
        'cnxk_ethdev_sec.c',
        'cnxk_ethdev_telemetry.c',
        'cnxk_link.c',
        'cnxk_lookup.c',
        'cnxk_ptp.c',
        'cnxk_rte_flow.c',
        'cnxk_stats.c',
        'cnxk_tm.c',
)

# CN9K
sources += files(
        'cn9k_ethdev.c',
        'cn9k_ethdev_sec.c',
        'cn9k_rte_flow.c',
        'cn9k_rx.c',
        'cn9k_rx_mseg.c',
        'cn9k_rx_vec.c',
        'cn9k_rx_vec_mseg.c',
        'cn9k_tx.c',
        'cn9k_tx_mseg.c',
        'cn9k_tx_vec.c',
        'cn9k_tx_vec_mseg.c',
)
# CN10K
sources += files(
        'cn10k_ethdev.c',
        'cn10k_ethdev_sec.c',
        'cn10k_rte_flow.c',
        'cn10k_rx.c',
        'cn10k_rx_mseg.c',
        'cn10k_rx_vec.c',
        'cn10k_rx_vec_mseg.c',
        'cn10k_tx.c',
        'cn10k_tx_mseg.c',
        'cn10k_tx_vec.c',
        'cn10k_tx_vec_mseg.c',
)

deps += ['bus_pci', 'cryptodev', 'eventdev', 'security']
deps += ['common_cnxk', 'mempool_cnxk']

# Allow implicit vector conversions and strict aliasing warning
extra_flags = ['-flax-vector-conversions', '-Wno-strict-aliasing']
foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach
