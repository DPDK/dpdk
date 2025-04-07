# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

testpmd_sources = files('iavf_testpmd.c')

deps += ['security', 'cryptodev']

sources = files(
        'base/iavf_adminq.c',
        'base/iavf_common.c',
        'base/iavf_impl.c',

        'iavf_base_symbols.c',
        'iavf_ethdev.c',
        'iavf_rxtx.c',
        'iavf_vchnl.c',
        'iavf_generic_flow.c',
        'iavf_fdir.c',
        'iavf_hash.c',
        'iavf_tm.c',
        'iavf_ipsec_crypto.c',
        'iavf_fsub.c',
)

if arch_subdir == 'x86'
    sources += files('iavf_rxtx_vec_sse.c')
    sources_avx2 += files('iavf_rxtx_vec_avx2.c')
    sources_avx512 += files('iavf_rxtx_vec_avx512.c')
elif arch_subdir == 'arm'
    sources += files('iavf_rxtx_vec_neon.c')
endif

headers = files('rte_pmd_iavf.h')
