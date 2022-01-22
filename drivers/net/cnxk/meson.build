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
        'cn9k_rx_select.c',
        'cn9k_tx.c',
        'cn9k_tx_mseg.c',
        'cn9k_tx_vec.c',
        'cn9k_tx_vec_mseg.c',
)

sources += files(
        'rx/cn9k/rx_0_15.c',
        'rx/cn9k/rx_16_31.c',
        'rx/cn9k/rx_32_47.c',
        'rx/cn9k/rx_48_63.c',
        'rx/cn9k/rx_64_79.c',
        'rx/cn9k/rx_80_95.c',
        'rx/cn9k/rx_96_111.c',
        'rx/cn9k/rx_112_127.c',
        'rx/cn9k/rx_0_15_mseg.c',
        'rx/cn9k/rx_16_31_mseg.c',
        'rx/cn9k/rx_32_47_mseg.c',
        'rx/cn9k/rx_48_63_mseg.c',
        'rx/cn9k/rx_64_79_mseg.c',
        'rx/cn9k/rx_80_95_mseg.c',
        'rx/cn9k/rx_96_111_mseg.c',
        'rx/cn9k/rx_112_127_mseg.c',
        'rx/cn9k/rx_0_15_vec.c',
        'rx/cn9k/rx_16_31_vec.c',
        'rx/cn9k/rx_32_47_vec.c',
        'rx/cn9k/rx_48_63_vec.c',
        'rx/cn9k/rx_64_79_vec.c',
        'rx/cn9k/rx_80_95_vec.c',
        'rx/cn9k/rx_96_111_vec.c',
        'rx/cn9k/rx_112_127_vec.c',
        'rx/cn9k/rx_0_15_vec_mseg.c',
        'rx/cn9k/rx_16_31_vec_mseg.c',
        'rx/cn9k/rx_32_47_vec_mseg.c',
        'rx/cn9k/rx_48_63_vec_mseg.c',
        'rx/cn9k/rx_64_79_vec_mseg.c',
        'rx/cn9k/rx_80_95_vec_mseg.c',
        'rx/cn9k/rx_96_111_vec_mseg.c',
        'rx/cn9k/rx_112_127_vec_mseg.c',
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
