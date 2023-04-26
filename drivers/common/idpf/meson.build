# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

includes += include_directories('../iavf')

deps += ['mbuf']

sources = files(
        'idpf_common_device.c',
        'idpf_common_rxtx.c',
        'idpf_common_virtchnl.c',
)

if arch_subdir == 'x86'
    idpf_avx512_cpu_support = (
        cc.get_define('__AVX512F__', args: machine_args) != '' and
        cc.get_define('__AVX512BW__', args: machine_args) != '' and
        cc.get_define('__AVX512DQ__', args: machine_args) != ''
    )

    idpf_avx512_cc_support = (
        not machine_args.contains('-mno-avx512f') and
        cc.has_argument('-mavx512f') and
        cc.has_argument('-mavx512bw') and
        cc.has_argument('-mavx512dq')
    )

    if idpf_avx512_cpu_support == true or idpf_avx512_cc_support == true
        cflags += ['-DCC_AVX512_SUPPORT']
        avx512_args = [cflags, '-mavx512f', '-mavx512bw', '-mavx512dq']
        if cc.has_argument('-march=skylake-avx512')
            avx512_args += '-march=skylake-avx512'
        endif
        idpf_common_avx512_lib = static_library('idpf_common_avx512_lib',
                'idpf_common_rxtx_avx512.c',
                dependencies: [static_rte_mbuf,],
                include_directories: includes,
                c_args: avx512_args)
        objs += idpf_common_avx512_lib.extract_objects('idpf_common_rxtx_avx512.c')
    endif
endif

subdir('base')
