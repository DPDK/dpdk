# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 NebulaMatrix Technology Co., Ltd.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
endif

sources = files(
        'nbl_ethdev.c',
)
