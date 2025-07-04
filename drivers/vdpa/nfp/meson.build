# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Corigine, Inc.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
endif

sources = files(
        'nfp_vdpa.c',
        'nfp_vdpa_core.c',
        'nfp_vdpa_log.c',
)

deps += ['vhost', 'common_nfp']
