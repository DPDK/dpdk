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
    endif
endif
