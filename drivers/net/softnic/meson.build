# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
headers = files('rte_eth_softnic.h')
sources = files(
        'conn.c',
        'rte_eth_softnic.c',
        'rte_eth_softnic_cli.c',
        'rte_eth_softnic_mempool.c',
        'rte_eth_softnic_pipeline.c',
        'rte_eth_softnic_swq.c',
        'rte_eth_softnic_thread.c',
)
deps += ['pipeline', 'port', 'table']
