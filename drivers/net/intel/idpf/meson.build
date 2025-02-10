# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

includes += include_directories('../iavf/base')

sources = files(
        'idpf_common_device.c',
        'idpf_common_rxtx.c',
        'idpf_common_virtchnl.c',

        'idpf_ethdev.c',
        'idpf_rxtx.c',
)

if arch_subdir == 'x86' and dpdk_conf.get('RTE_IOVA_IN_MBUF') == 1
    idpf_avx2_lib = static_library('idpf_avx2_lib',
        'idpf_common_rxtx_avx2.c',
        dependencies: [static_rte_ethdev, static_rte_hash],
        include_directories: includes,
        c_args: [cflags, '-mavx2'])
    objs += idpf_avx2_lib.extract_objects('idpf_common_rxtx_avx2.c')

    if cc_has_avx512
        cflags += ['-DCC_AVX512_SUPPORT']
        avx512_args = cflags + cc_avx512_flags
        if cc.has_argument('-march=skylake-avx512')
            avx512_args += '-march=skylake-avx512'
            if cc.has_argument('-Wno-overriding-option')
                avx512_args += '-Wno-overriding-option'
            endif
        endif
        idpf_common_avx512_lib = static_library('idpf_common_avx512_lib',
                'idpf_common_rxtx_avx512.c',
                dependencies: static_rte_mbuf,
                include_directories: includes,
                c_args: avx512_args)
        objs += idpf_common_avx512_lib.extract_objects('idpf_common_rxtx_avx512.c')
    endif
endif

subdir('base')
