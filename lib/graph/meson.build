# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2020 Marvell International Ltd.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'node.c',
        'graph.c',
        'graph_ops.c',
        'graph_debug.c',
        'graph_stats.c',
        'graph_populate.c',
        'graph_pcap.c',
        'rte_graph_worker.c',
        'rte_graph_model_mcore_dispatch.c',
)
headers = files('rte_graph.h', 'rte_graph_worker.h')
indirect_headers += files(
        'rte_graph_model_mcore_dispatch.h',
        'rte_graph_model_rtc.h',
        'rte_graph_worker_common.h',
)

deps += ['eal', 'pcapng', 'mempool', 'ring']
