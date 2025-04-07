# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')

includes += include_directories('../iavf/base')

sources += files(
        'idpf_common_device.c',
        'idpf_common_rxtx.c',
        'idpf_common_virtchnl.c',

        'idpf_ethdev.c',
        'idpf_rxtx.c',
)

if arch_subdir == 'x86' and dpdk_conf.get('RTE_IOVA_IN_MBUF') == 1
    sources_avx2 += files('idpf_common_rxtx_avx2.c')
    sources_avx512 += files('idpf_common_rxtx_avx512.c')
endif
