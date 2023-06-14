# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit'
    subdir_done()
endif

if meson.is_cross_build()
        soc_type = meson.get_cross_property('platform', '')
else
        soc_type = platform
endif

if soc_type != 'cn9k' and soc_type != 'cn10k'
        soc_type = 'all'
endif

sources = files(
        'cnxk_ethdev.c',
        'cnxk_ethdev_cman.c',
        'cnxk_ethdev_devargs.c',
        'cnxk_ethdev_mcs.c',
        'cnxk_ethdev_mtr.c',
        'cnxk_ethdev_ops.c',
        'cnxk_ethdev_sec.c',
        'cnxk_ethdev_telemetry.c',
        'cnxk_ethdev_sec_telemetry.c',
        'cnxk_link.c',
        'cnxk_lookup.c',
        'cnxk_ptp.c',
        'cnxk_flow.c',
        'cnxk_stats.c',
        'cnxk_tm.c',
)

if soc_type == 'cn9k' or soc_type == 'all'
# CN9K
sources += files(
        'cn9k_ethdev.c',
        'cn9k_ethdev_sec.c',
        'cn9k_flow.c',
        'cn9k_rx_select.c',
        'cn9k_tx_select.c',
)

if host_machine.cpu_family().startswith('aarch')
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

sources += files(
        'tx/cn9k/tx_0_15.c',
        'tx/cn9k/tx_16_31.c',
        'tx/cn9k/tx_32_47.c',
        'tx/cn9k/tx_48_63.c',
        'tx/cn9k/tx_64_79.c',
        'tx/cn9k/tx_80_95.c',
        'tx/cn9k/tx_96_111.c',
        'tx/cn9k/tx_112_127.c',
        'tx/cn9k/tx_0_15_mseg.c',
        'tx/cn9k/tx_16_31_mseg.c',
        'tx/cn9k/tx_32_47_mseg.c',
        'tx/cn9k/tx_48_63_mseg.c',
        'tx/cn9k/tx_64_79_mseg.c',
        'tx/cn9k/tx_80_95_mseg.c',
        'tx/cn9k/tx_96_111_mseg.c',
        'tx/cn9k/tx_112_127_mseg.c',
        'tx/cn9k/tx_0_15_vec.c',
        'tx/cn9k/tx_16_31_vec.c',
        'tx/cn9k/tx_32_47_vec.c',
        'tx/cn9k/tx_48_63_vec.c',
        'tx/cn9k/tx_64_79_vec.c',
        'tx/cn9k/tx_80_95_vec.c',
        'tx/cn9k/tx_96_111_vec.c',
        'tx/cn9k/tx_112_127_vec.c',
        'tx/cn9k/tx_0_15_vec_mseg.c',
        'tx/cn9k/tx_16_31_vec_mseg.c',
        'tx/cn9k/tx_32_47_vec_mseg.c',
        'tx/cn9k/tx_48_63_vec_mseg.c',
        'tx/cn9k/tx_64_79_vec_mseg.c',
        'tx/cn9k/tx_80_95_vec_mseg.c',
        'tx/cn9k/tx_96_111_vec_mseg.c',
        'tx/cn9k/tx_112_127_vec_mseg.c',
)
endif
endif

if soc_type == 'cn10k' or soc_type == 'all'
# CN10K
sources += files(
        'cn10k_ethdev.c',
        'cn10k_ethdev_sec.c',
        'cn10k_flow.c',
        'cn10k_rx_select.c',
        'cn10k_tx_select.c',
)

if host_machine.cpu_family().startswith('aarch')
sources += files(
        'rx/cn10k/rx_0_15.c',
        'rx/cn10k/rx_16_31.c',
        'rx/cn10k/rx_32_47.c',
        'rx/cn10k/rx_48_63.c',
        'rx/cn10k/rx_64_79.c',
        'rx/cn10k/rx_80_95.c',
        'rx/cn10k/rx_96_111.c',
        'rx/cn10k/rx_112_127.c',
        'rx/cn10k/rx_0_15_mseg.c',
        'rx/cn10k/rx_16_31_mseg.c',
        'rx/cn10k/rx_32_47_mseg.c',
        'rx/cn10k/rx_48_63_mseg.c',
        'rx/cn10k/rx_64_79_mseg.c',
        'rx/cn10k/rx_80_95_mseg.c',
        'rx/cn10k/rx_96_111_mseg.c',
        'rx/cn10k/rx_112_127_mseg.c',
        'rx/cn10k/rx_0_15_vec.c',
        'rx/cn10k/rx_16_31_vec.c',
        'rx/cn10k/rx_32_47_vec.c',
        'rx/cn10k/rx_48_63_vec.c',
        'rx/cn10k/rx_64_79_vec.c',
        'rx/cn10k/rx_80_95_vec.c',
        'rx/cn10k/rx_96_111_vec.c',
        'rx/cn10k/rx_112_127_vec.c',
        'rx/cn10k/rx_0_15_vec_mseg.c',
        'rx/cn10k/rx_16_31_vec_mseg.c',
        'rx/cn10k/rx_32_47_vec_mseg.c',
        'rx/cn10k/rx_48_63_vec_mseg.c',
        'rx/cn10k/rx_64_79_vec_mseg.c',
        'rx/cn10k/rx_80_95_vec_mseg.c',
        'rx/cn10k/rx_96_111_vec_mseg.c',
        'rx/cn10k/rx_112_127_vec_mseg.c',
)

sources += files(
        'tx/cn10k/tx_0_15.c',
        'tx/cn10k/tx_16_31.c',
        'tx/cn10k/tx_32_47.c',
        'tx/cn10k/tx_48_63.c',
        'tx/cn10k/tx_64_79.c',
        'tx/cn10k/tx_80_95.c',
        'tx/cn10k/tx_96_111.c',
        'tx/cn10k/tx_112_127.c',
        'tx/cn10k/tx_0_15_mseg.c',
        'tx/cn10k/tx_16_31_mseg.c',
        'tx/cn10k/tx_32_47_mseg.c',
        'tx/cn10k/tx_48_63_mseg.c',
        'tx/cn10k/tx_64_79_mseg.c',
        'tx/cn10k/tx_80_95_mseg.c',
        'tx/cn10k/tx_96_111_mseg.c',
        'tx/cn10k/tx_112_127_mseg.c',
        'tx/cn10k/tx_0_15_vec.c',
        'tx/cn10k/tx_16_31_vec.c',
        'tx/cn10k/tx_32_47_vec.c',
        'tx/cn10k/tx_48_63_vec.c',
        'tx/cn10k/tx_64_79_vec.c',
        'tx/cn10k/tx_80_95_vec.c',
        'tx/cn10k/tx_96_111_vec.c',
        'tx/cn10k/tx_112_127_vec.c',
        'tx/cn10k/tx_0_15_vec_mseg.c',
        'tx/cn10k/tx_16_31_vec_mseg.c',
        'tx/cn10k/tx_32_47_vec_mseg.c',
        'tx/cn10k/tx_48_63_vec_mseg.c',
        'tx/cn10k/tx_64_79_vec_mseg.c',
        'tx/cn10k/tx_80_95_vec_mseg.c',
        'tx/cn10k/tx_96_111_vec_mseg.c',
        'tx/cn10k/tx_112_127_vec_mseg.c',
)
endif
endif

deps += ['bus_pci', 'cryptodev', 'eventdev', 'security']
deps += ['common_cnxk', 'mempool_cnxk']

# Allow implicit vector conversions and strict aliasing warning
extra_flags = ['-flax-vector-conversions', '-Wno-strict-aliasing']
if cc.get_id() == 'clang'
        extra_flags += ['-Wno-asm-operand-widths']
endif
foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

headers = files('rte_pmd_cnxk.h')

require_iova_in_mbuf = false

annotate_locks = false
