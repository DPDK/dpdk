# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
# Copyright(c) 2023 Broadcom

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

headers = files('rte_pmd_bnxt.h')
cflags_options = [
        '-DSUPPORT_CFA_HW_ALL=1',
]

foreach option:cflags_options
        if cc.has_argument(option)
                cflags += option
        endif
endforeach

headers = files('rte_pmd_bnxt.h')

deps += ['hash']

sources = files(
        'bnxt_cpr.c',
        'bnxt_ethdev.c',
        'bnxt_filter.c',
        'bnxt_flow.c',
        'bnxt_hwrm.c',
        'bnxt_irq.c',
        'bnxt_ring.c',
        'bnxt_rxq.c',
        'bnxt_rxr.c',
        'bnxt_stats.c',
        'bnxt_txq.c',
        'bnxt_txr.c',
        'bnxt_util.c',
        'bnxt_vnic.c',
        'bnxt_reps.c',
        'rte_pmd_bnxt.c',
)

#Add the subdirectories that need to be compiled
subdir('tf_ulp')
subdir('tf_core')
subdir('hcapi/cfa')

if arch_subdir == 'x86'
    sources += files('bnxt_rxtx_vec_sse.c')
    # compile AVX2 version if either:
    # a. we have AVX supported in minimum instruction set baseline
    # b. it's not minimum instruction set, but supported by compiler
    if cc.get_define('__AVX2__', args: machine_args) != ''
            cflags += ['-DCC_AVX2_SUPPORT']
            sources += files('bnxt_rxtx_vec_avx2.c')
    elif cc.has_argument('-mavx2')
            cflags += ['-DCC_AVX2_SUPPORT']
            bnxt_avx2_lib = static_library('bnxt_avx2_lib',
                            'bnxt_rxtx_vec_avx2.c',
                            dependencies: [static_rte_ethdev,
                                    static_rte_bus_pci,
                                    static_rte_kvargs, static_rte_hash],
                            include_directories: includes,
                            c_args: [cflags, '-mavx2'])
            objs += bnxt_avx2_lib.extract_objects('bnxt_rxtx_vec_avx2.c')
     endif
elif arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
    sources += files('bnxt_rxtx_vec_neon.c')
endif

annotate_locks = false
