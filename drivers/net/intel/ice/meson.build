# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

subdir('base')
objs = [base_objs]

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
includes += include_directories('base')

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
    sources += files('ice_rxtx_vec_sse.c')

    if is_windows and cc.get_id() != 'clang'
        cflags += ['-fno-asynchronous-unwind-tables']
    endif

    ice_avx2_lib = static_library('ice_avx2_lib',
            'ice_rxtx_vec_avx2.c',
            dependencies: [static_rte_ethdev, static_rte_hash],
            include_directories: includes,
            c_args: [cflags, cc_avx2_flags])
    objs += ice_avx2_lib.extract_objects('ice_rxtx_vec_avx2.c')

    if cc_has_avx512
        cflags += ['-DCC_AVX512_SUPPORT']
        avx512_args = cflags + cc_avx512_flags
        if cc.has_argument('-march=skylake-avx512')
            avx512_args += '-march=skylake-avx512'
            if cc.has_argument('-Wno-overriding-option')
                avx512_args += '-Wno-overriding-option'
            endif
        endif
        ice_avx512_lib = static_library('ice_avx512_lib',
                'ice_rxtx_vec_avx512.c',
                dependencies: [static_rte_ethdev, static_rte_hash],
                include_directories: includes,
                c_args: avx512_args)
        objs += ice_avx512_lib.extract_objects('ice_rxtx_vec_avx512.c')
    endif
endif

sources += files(
        'ice_dcf.c',
        'ice_dcf_vf_representor.c',
        'ice_dcf_ethdev.c',
        'ice_dcf_parent.c',
        'ice_dcf_sched.c',
)

require_iova_in_mbuf = false
