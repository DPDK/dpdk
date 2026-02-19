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

cflags += no_wvla_cflag
cflags += no_shadow_cflag
cflags += no_comma_cflag

headers = files('rte_pmd_bnxt.h')
cflags_options = [
        '-DSUPPORT_CFA_HW_P70=1',
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
        'bnxt_mpc.c',
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
subdir('tf_core/v3')
subdir('hcapi/cfa')
subdir('hcapi/cfa_v3')

if arch_subdir == 'x86'
    sources += files('bnxt_rxtx_vec_sse.c')
    sources_avx2 += files('bnxt_rxtx_vec_avx2.c')
elif arch_subdir == 'arm' and dpdk_conf.get('RTE_ARCH_64')
    sources += files('bnxt_rxtx_vec_neon.c')
endif

annotate_locks = false
