# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 NebulaMatrix Technology Co., Ltd.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
endif

includes += include_directories('nbl_include')
includes += include_directories('nbl_hw')

sources = files(
        'nbl_ethdev.c',
        'nbl_core.c',
        'nbl_dispatch.c',
        'nbl_common/nbl_common.c',
        'nbl_hw/nbl_channel.c',
        'nbl_hw/nbl_resource.c',
        'nbl_hw/nbl_txrx.c',
        'nbl_hw/nbl_hw_leonis/nbl_hw_leonis_snic.c',
        'nbl_hw/nbl_hw_leonis/nbl_res_leonis.c',
)
