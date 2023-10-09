# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

deps += ['hash', 'common_idpf']

sources = files(
        'cpfl_ethdev.c',
        'cpfl_rxtx.c',
        'cpfl_vchnl.c',
        'cpfl_representor.c',
        'cpfl_controlq.c',
        'cpfl_rules.c',
)

if arch_subdir == 'x86'
    cpfl_avx512_cpu_support = (
        cc.get_define('__AVX512F__', args: machine_args) != '' and
        cc.get_define('__AVX512BW__', args: machine_args) != '' and
        cc.get_define('__AVX512DQ__', args: machine_args) != ''
    )

    cpfl_avx512_cc_support = (
        not machine_args.contains('-mno-avx512f') and
        cc.has_argument('-mavx512f') and
        cc.has_argument('-mavx512bw') and
        cc.has_argument('-mavx512dq')
    )

    if cpfl_avx512_cpu_support == true or cpfl_avx512_cc_support == true
        cflags += ['-DCC_AVX512_SUPPORT']
    endif
endif

if dpdk_conf.has('RTE_HAS_JANSSON')
    sources += files(
            'cpfl_flow.c',
            'cpfl_flow_engine_fxp.c',
            'cpfl_flow_parser.c',
            'cpfl_fxp_rule.c',
    )
    ext_deps += jansson_dep
endif
