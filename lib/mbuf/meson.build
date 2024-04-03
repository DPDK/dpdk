# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_ms_compiler
    build = false
    reason = 'not supported building with Visual Studio Toolset'
    subdir_done()
endif

sources = files(
        'rte_mbuf.c',
        'rte_mbuf_ptype.c',
        'rte_mbuf_pool_ops.c',
        'rte_mbuf_dyn.c',
)
headers = files(
        'rte_mbuf.h',
        'rte_mbuf_core.h',
        'rte_mbuf_ptype.h',
        'rte_mbuf_pool_ops.h',
        'rte_mbuf_dyn.h',
)
deps += ['mempool']
