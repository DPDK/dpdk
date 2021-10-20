# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'rte_ipv4_fragmentation.c',
        'rte_ipv6_fragmentation.c',
        'rte_ipv4_reassembly.c',
        'rte_ipv6_reassembly.c',
        'rte_ip_frag_common.c',
        'ip_frag_internal.c',
)
headers = files('rte_ip_frag.h')
deps += ['ethdev', 'hash']
