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

deps += ['hash', 'net_idpf']

sources = files(
        'cpfl_ethdev.c',
        'cpfl_rxtx.c',
        'cpfl_vchnl.c',
        'cpfl_representor.c',
        'cpfl_controlq.c',
        'cpfl_rules.c',
)

if arch_subdir == 'x86' and cc_has_avx512
    cflags += ['-DCC_AVX512_SUPPORT']
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
