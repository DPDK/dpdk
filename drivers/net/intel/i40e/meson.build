# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if arch_subdir == 'loongarch'
    build = false
    reason = 'not supported on LoongArch'
    subdir_done()
endif

if arch_subdir == 'riscv'
    build = false
    reason = 'not supported on RISC-V'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

subdir('base')

sources += files(
        'i40e_ethdev.c',
        'i40e_rxtx.c',
        'i40e_pf.c',
        'i40e_fdir.c',
        'i40e_flow.c',
        'i40e_tm.c',
        'i40e_hash.c',
        'i40e_vf_representor.c',
        'i40e_recycle_mbufs_vec_common.c',
        'rte_pmd_i40e.c',
)

testpmd_sources = files('i40e_testpmd.c')

deps += ['hash']

if arch_subdir == 'x86'
    sources += files('i40e_rxtx_vec_sse.c')
    sources_avx2 += files('i40e_rxtx_vec_avx2.c')
    sources_avx512 += files('i40e_rxtx_vec_avx512.c')
elif arch_subdir == 'ppc'
       sources += files('i40e_rxtx_vec_altivec.c')
elif arch_subdir == 'arm'
       sources += files('i40e_rxtx_vec_neon.c')
endif

headers = files('rte_pmd_i40e.h')
