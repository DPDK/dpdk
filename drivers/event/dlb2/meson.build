# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on x86_64 Linux'
        subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

sources = files(
        'dlb2.c',
        'dlb2_iface.c',
        'dlb2_xstats.c',
        'pf/dlb2_main.c',
        'pf/dlb2_pf.c',
        'pf/base/dlb2_resource.c',
        'rte_pmd_dlb2.c',
        'dlb2_selftest.c',
)

if target_has_avx512
    cflags += '-DCC_AVX512_SUPPORT'
    sources += files('dlb2_avx512.c')

elif cc_has_avx512
    cflags += '-DCC_AVX512_SUPPORT'
    avx512_tmplib = static_library('avx512_tmp',
                               'dlb2_avx512.c',
                               dependencies: [static_rte_eal, static_rte_eventdev],
                               c_args: cflags + cc_avx512_flags)
    objs += avx512_tmplib.extract_objects('dlb2_avx512.c')
else
    sources += files('dlb2_sse.c')
endif

headers = files('rte_pmd_dlb2.h')

deps += ['mbuf', 'mempool', 'ring', 'pci', 'bus_pci']
