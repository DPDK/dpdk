# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

if meson.is_cross_build()
        soc_type = meson.get_external_property('platform', '')
else
        soc_type = platform
endif

if soc_type != 'cn9k' and soc_type != 'cn10k' and soc_type != 'cn20k'
        soc_type = 'all'
endif

sources = files(
        'cnxk_eventdev.c',
        'cnxk_worker.c',
        'cnxk_eventdev_adptr.c',
        'cnxk_eventdev_selftest.c',
        'cnxk_eventdev_stats.c',
        'cnxk_tim_evdev.c',
        'cnxk_tim_worker.c',
        'cnxk_vector_adptr.c',
)

disable_template = false
if meson.version().version_compare('>=1.1.0')
        if '-DCNXK_DIS_TMPLT_FUNC' in meson.build_options()
                disable_template = true
        endif
endif

if soc_type == 'cn9k' or soc_type == 'all'
sources += files(
        'cn9k_eventdev.c',
        'cn9k_worker.c',
)

if host_machine.cpu_family().startswith('aarch') and not disable_template
sources += files(
        'deq/cn9k/deq_0_15_burst.c',
        'deq/cn9k/deq_16_31_burst.c',
        'deq/cn9k/deq_32_47_burst.c',
        'deq/cn9k/deq_48_63_burst.c',
        'deq/cn9k/deq_64_79_burst.c',
        'deq/cn9k/deq_80_95_burst.c',
        'deq/cn9k/deq_96_111_burst.c',
        'deq/cn9k/deq_112_127_burst.c',
        'deq/cn9k/deq_0_15_seg_burst.c',
        'deq/cn9k/deq_16_31_seg_burst.c',
        'deq/cn9k/deq_32_47_seg_burst.c',
        'deq/cn9k/deq_48_63_seg_burst.c',
        'deq/cn9k/deq_64_79_seg_burst.c',
        'deq/cn9k/deq_80_95_seg_burst.c',
        'deq/cn9k/deq_96_111_seg_burst.c',
        'deq/cn9k/deq_112_127_seg_burst.c',
        'deq/cn9k/deq_0_15_tmo_burst.c',
        'deq/cn9k/deq_16_31_tmo_burst.c',
        'deq/cn9k/deq_32_47_tmo_burst.c',
        'deq/cn9k/deq_48_63_tmo_burst.c',
        'deq/cn9k/deq_64_79_tmo_burst.c',
        'deq/cn9k/deq_80_95_tmo_burst.c',
        'deq/cn9k/deq_96_111_tmo_burst.c',
        'deq/cn9k/deq_112_127_tmo_burst.c',
        'deq/cn9k/deq_0_15_tmo_seg_burst.c',
        'deq/cn9k/deq_16_31_tmo_seg_burst.c',
        'deq/cn9k/deq_32_47_tmo_seg_burst.c',
        'deq/cn9k/deq_48_63_tmo_seg_burst.c',
        'deq/cn9k/deq_64_79_tmo_seg_burst.c',
        'deq/cn9k/deq_80_95_tmo_seg_burst.c',
        'deq/cn9k/deq_96_111_tmo_seg_burst.c',
        'deq/cn9k/deq_112_127_tmo_seg_burst.c',
)

sources += files(
        'deq/cn9k/deq_0_15_dual_burst.c',
        'deq/cn9k/deq_16_31_dual_burst.c',
        'deq/cn9k/deq_32_47_dual_burst.c',
        'deq/cn9k/deq_48_63_dual_burst.c',
        'deq/cn9k/deq_64_79_dual_burst.c',
        'deq/cn9k/deq_80_95_dual_burst.c',
        'deq/cn9k/deq_96_111_dual_burst.c',
        'deq/cn9k/deq_112_127_dual_burst.c',
        'deq/cn9k/deq_0_15_dual_seg_burst.c',
        'deq/cn9k/deq_16_31_dual_seg_burst.c',
        'deq/cn9k/deq_32_47_dual_seg_burst.c',
        'deq/cn9k/deq_48_63_dual_seg_burst.c',
        'deq/cn9k/deq_64_79_dual_seg_burst.c',
        'deq/cn9k/deq_80_95_dual_seg_burst.c',
        'deq/cn9k/deq_96_111_dual_seg_burst.c',
        'deq/cn9k/deq_112_127_dual_seg_burst.c',
        'deq/cn9k/deq_0_15_dual_tmo_burst.c',
        'deq/cn9k/deq_16_31_dual_tmo_burst.c',
        'deq/cn9k/deq_32_47_dual_tmo_burst.c',
        'deq/cn9k/deq_48_63_dual_tmo_burst.c',
        'deq/cn9k/deq_64_79_dual_tmo_burst.c',
        'deq/cn9k/deq_80_95_dual_tmo_burst.c',
        'deq/cn9k/deq_96_111_dual_tmo_burst.c',
        'deq/cn9k/deq_112_127_dual_tmo_burst.c',
        'deq/cn9k/deq_0_15_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_16_31_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_32_47_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_48_63_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_64_79_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_80_95_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_96_111_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_112_127_dual_tmo_seg_burst.c',
        'deq/cn9k/deq_all_offload.c',
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
        'tx/cn9k/tx_0_15_seg.c',
        'tx/cn9k/tx_16_31_seg.c',
        'tx/cn9k/tx_32_47_seg.c',
        'tx/cn9k/tx_48_63_seg.c',
        'tx/cn9k/tx_64_79_seg.c',
        'tx/cn9k/tx_80_95_seg.c',
        'tx/cn9k/tx_96_111_seg.c',
        'tx/cn9k/tx_112_127_seg.c',
)

sources += files(
        'tx/cn9k/tx_0_15_dual.c',
        'tx/cn9k/tx_16_31_dual.c',
        'tx/cn9k/tx_32_47_dual.c',
        'tx/cn9k/tx_48_63_dual.c',
        'tx/cn9k/tx_64_79_dual.c',
        'tx/cn9k/tx_80_95_dual.c',
        'tx/cn9k/tx_96_111_dual.c',
        'tx/cn9k/tx_112_127_dual.c',
        'tx/cn9k/tx_0_15_dual_seg.c',
        'tx/cn9k/tx_16_31_dual_seg.c',
        'tx/cn9k/tx_32_47_dual_seg.c',
        'tx/cn9k/tx_48_63_dual_seg.c',
        'tx/cn9k/tx_64_79_dual_seg.c',
        'tx/cn9k/tx_80_95_dual_seg.c',
        'tx/cn9k/tx_96_111_dual_seg.c',
        'tx/cn9k/tx_112_127_dual_seg.c',
        'tx/cn9k/tx_all_offload.c',
)
else
sources += files(
        'deq/cn9k/deq_all_offload.c',
        'tx/cn9k/tx_all_offload.c',
)
endif
endif

if soc_type == 'cn10k' or soc_type == 'all'
sources += files(
        'cn10k_eventdev.c',
        'cn10k_worker.c',
)

if host_machine.cpu_family().startswith('aarch') and not disable_template
sources += files(
        'deq/cn10k/deq_0_15_burst.c',
        'deq/cn10k/deq_16_31_burst.c',
        'deq/cn10k/deq_32_47_burst.c',
        'deq/cn10k/deq_48_63_burst.c',
        'deq/cn10k/deq_64_79_burst.c',
        'deq/cn10k/deq_80_95_burst.c',
        'deq/cn10k/deq_96_111_burst.c',
        'deq/cn10k/deq_112_127_burst.c',
        'deq/cn10k/deq_0_15_seg_burst.c',
        'deq/cn10k/deq_16_31_seg_burst.c',
        'deq/cn10k/deq_32_47_seg_burst.c',
        'deq/cn10k/deq_48_63_seg_burst.c',
        'deq/cn10k/deq_64_79_seg_burst.c',
        'deq/cn10k/deq_80_95_seg_burst.c',
        'deq/cn10k/deq_96_111_seg_burst.c',
        'deq/cn10k/deq_112_127_seg_burst.c',
        'deq/cn10k/deq_0_15_tmo_burst.c',
        'deq/cn10k/deq_16_31_tmo_burst.c',
        'deq/cn10k/deq_32_47_tmo_burst.c',
        'deq/cn10k/deq_48_63_tmo_burst.c',
        'deq/cn10k/deq_64_79_tmo_burst.c',
        'deq/cn10k/deq_80_95_tmo_burst.c',
        'deq/cn10k/deq_96_111_tmo_burst.c',
        'deq/cn10k/deq_112_127_tmo_burst.c',
        'deq/cn10k/deq_0_15_tmo_seg_burst.c',
        'deq/cn10k/deq_16_31_tmo_seg_burst.c',
        'deq/cn10k/deq_32_47_tmo_seg_burst.c',
        'deq/cn10k/deq_48_63_tmo_seg_burst.c',
        'deq/cn10k/deq_64_79_tmo_seg_burst.c',
        'deq/cn10k/deq_80_95_tmo_seg_burst.c',
        'deq/cn10k/deq_96_111_tmo_seg_burst.c',
        'deq/cn10k/deq_112_127_tmo_seg_burst.c',
        'deq/cn10k/deq_all_offload.c',
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
        'tx/cn10k/tx_0_15_seg.c',
        'tx/cn10k/tx_16_31_seg.c',
        'tx/cn10k/tx_32_47_seg.c',
        'tx/cn10k/tx_48_63_seg.c',
        'tx/cn10k/tx_64_79_seg.c',
        'tx/cn10k/tx_80_95_seg.c',
        'tx/cn10k/tx_96_111_seg.c',
        'tx/cn10k/tx_112_127_seg.c',
        'tx/cn10k/tx_all_offload.c',
)
else
sources += files(
        'deq/cn10k/deq_all_offload.c',
        'tx/cn10k/tx_all_offload.c',
)
endif
endif

if soc_type == 'cn20k' or soc_type == 'all'
sources += files(
        'cn20k_eventdev.c',
        'cn20k_worker.c',
)

if host_machine.cpu_family().startswith('aarch') and not disable_template
sources += files(
        'deq/cn20k/deq_0_15_burst.c',
        'deq/cn20k/deq_16_31_burst.c',
        'deq/cn20k/deq_32_47_burst.c',
        'deq/cn20k/deq_48_63_burst.c',
        'deq/cn20k/deq_64_79_burst.c',
        'deq/cn20k/deq_80_95_burst.c',
        'deq/cn20k/deq_96_111_burst.c',
        'deq/cn20k/deq_112_127_burst.c',
        'deq/cn20k/deq_0_15_seg_burst.c',
        'deq/cn20k/deq_16_31_seg_burst.c',
        'deq/cn20k/deq_32_47_seg_burst.c',
        'deq/cn20k/deq_48_63_seg_burst.c',
        'deq/cn20k/deq_64_79_seg_burst.c',
        'deq/cn20k/deq_80_95_seg_burst.c',
        'deq/cn20k/deq_96_111_seg_burst.c',
        'deq/cn20k/deq_112_127_seg_burst.c',
        'deq/cn20k/deq_0_15_tmo_burst.c',
        'deq/cn20k/deq_16_31_tmo_burst.c',
        'deq/cn20k/deq_32_47_tmo_burst.c',
        'deq/cn20k/deq_48_63_tmo_burst.c',
        'deq/cn20k/deq_64_79_tmo_burst.c',
        'deq/cn20k/deq_80_95_tmo_burst.c',
        'deq/cn20k/deq_96_111_tmo_burst.c',
        'deq/cn20k/deq_112_127_tmo_burst.c',
        'deq/cn20k/deq_0_15_tmo_seg_burst.c',
        'deq/cn20k/deq_16_31_tmo_seg_burst.c',
        'deq/cn20k/deq_32_47_tmo_seg_burst.c',
        'deq/cn20k/deq_48_63_tmo_seg_burst.c',
        'deq/cn20k/deq_64_79_tmo_seg_burst.c',
        'deq/cn20k/deq_80_95_tmo_seg_burst.c',
        'deq/cn20k/deq_96_111_tmo_seg_burst.c',
        'deq/cn20k/deq_112_127_tmo_seg_burst.c',
        'deq/cn20k/deq_all_offload.c',
)

sources += files(
        'tx/cn20k/tx_0_15.c',
        'tx/cn20k/tx_16_31.c',
        'tx/cn20k/tx_32_47.c',
        'tx/cn20k/tx_48_63.c',
        'tx/cn20k/tx_64_79.c',
        'tx/cn20k/tx_80_95.c',
        'tx/cn20k/tx_96_111.c',
        'tx/cn20k/tx_112_127.c',
        'tx/cn20k/tx_0_15_seg.c',
        'tx/cn20k/tx_16_31_seg.c',
        'tx/cn20k/tx_32_47_seg.c',
        'tx/cn20k/tx_48_63_seg.c',
        'tx/cn20k/tx_64_79_seg.c',
        'tx/cn20k/tx_80_95_seg.c',
        'tx/cn20k/tx_96_111_seg.c',
        'tx/cn20k/tx_112_127_seg.c',
        'tx/cn20k/tx_all_offload.c',
)
else
sources += files(
        'deq/cn20k/deq_all_offload.c',
        'tx/cn20k/tx_all_offload.c',
)
endif
endif

cflags += no_wvla_cflag
cflags += no_shadow_cflag

extra_flags = ['-flax-vector-conversions', '-Wno-strict-aliasing']
if cc.get_id() == 'clang'
        extra_flags += ['-Wno-asm-operand-widths']
endif

foreach flag: extra_flags
    if cc.has_argument(flag)
        cflags += flag
    endif
endforeach

headers = files('rte_pmd_cnxk_eventdev.h')
deps += ['bus_pci', 'common_cnxk', 'net_cnxk', 'crypto_cnxk', 'dma_cnxk']

require_iova_in_mbuf = false

annotate_locks = false
