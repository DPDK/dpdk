# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdir('base')

sources = files(
        'ice_acl_filter.c',
        'ice_diagnose.c',
        'ice_ethdev.c',
        'ice_fdir_filter.c',
        'ice_generic_flow.c',
        'ice_hash.c',
        'ice_rxtx.c',
        'ice_switch_filter.c',
        'ice_tm.c',
)

testpmd_sources = files('ice_testpmd.c')

deps += ['hash', 'net']

if dpdk_conf.has('RTE_NET_IAVF')
    deps += 'net_iavf'
else
    includes += include_directories('../iavf/base')
    sources += files(
            '../iavf/base/iavf_adminq.c',
            '../iavf/base/iavf_common.c',
            '../iavf/base/iavf_impl.c',
    )
endif

if arch_subdir == 'x86'
    sources_avx2 += files('ice_rxtx_vec_avx2.c')
    sources_avx512 += files('ice_rxtx_vec_avx512.c')
endif

sources += files(
        'ice_dcf.c',
        'ice_dcf_vf_representor.c',
        'ice_dcf_ethdev.c',
        'ice_dcf_parent.c',
        'ice_dcf_sched.c',
)

require_iova_in_mbuf = false
