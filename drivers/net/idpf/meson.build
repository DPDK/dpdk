# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['common_idpf']

sources = files(
        'idpf_ethdev.c',
        'idpf_rxtx.c',
        'idpf_vchnl.c',
)

if arch_subdir == 'x86'
    idpf_avx512_cpu_support = (
        cc.get_define('__AVX512F__', args: machine_args) != '' and
        cc.get_define('__AVX512BW__', args: machine_args) != ''
    )

    idpf_avx512_cc_support = (
        not machine_args.contains('-mno-avx512f') and
        cc.has_argument('-mavx512f') and
        cc.has_argument('-mavx512bw')
    )

    if idpf_avx512_cpu_support == true or idpf_avx512_cc_support == true
        cflags += ['-DCC_AVX512_SUPPORT']
        avx512_args = [cflags, '-mavx512f', '-mavx512bw']
        if cc.has_argument('-march=skylake-avx512')
            avx512_args += '-march=skylake-avx512'
        endif
        idpf_avx512_lib = static_library(
            'idpf_avx512_lib',
            'idpf_rxtx_vec_avx512.c',
            dependencies: [
                    static_rte_common_idpf,
                    static_rte_ethdev,
                    static_rte_bus_pci,
                    static_rte_kvargs,
                    static_rte_hash,
            ],
            include_directories: includes,
            c_args: avx512_args)
        objs += idpf_avx512_lib.extract_objects('idpf_rxtx_vec_avx512.c')
    endif
endif
