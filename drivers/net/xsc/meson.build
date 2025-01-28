# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Yunsilicon Technology Co., Ltd.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64bit Linux'
endif

sources = files(
        'xsc_ethdev.c',
        'xsc_dev.c',
)
