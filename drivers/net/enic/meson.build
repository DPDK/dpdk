# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cisco Systems, Inc.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    subdir_done()
endif

sources = files(
        'base/vnic_cq.c',
        'base/vnic_dev.c',
        'base/vnic_intr.c',
        'base/vnic_rq.c',
        'base/vnic_wq.c',
        'enic_ethdev.c',
        'enic_flow.c',
        'enic_fm_flow.c',
        'enic_main.c',
        'enic_res.c',
        'enic_rxtx.c',
        'enic_sriov.c',
        'enic_vf_representor.c',
)
deps += ['hash']
includes += include_directories('base')

cflags += no_shadow_cflag

if dpdk_conf.has('RTE_ARCH_X86_64')
    cflags += '-DENIC_RXTX_VEC'
    sources_avx2 += files('enic_rxtx_vec_avx2.c')
endif

annotate_locks = false
