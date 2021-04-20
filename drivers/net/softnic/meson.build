# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
headers = files('rte_eth_softnic.h')
sources = files(
        'conn.c',
        'parser.c',
        'rte_eth_softnic.c',
        'rte_eth_softnic_action.c',
        'rte_eth_softnic_cli.c',
        'rte_eth_softnic_cryptodev.c',
        'rte_eth_softnic_flow.c',
        'rte_eth_softnic_link.c',
        'rte_eth_softnic_mempool.c',
        'rte_eth_softnic_meter.c',
        'rte_eth_softnic_pipeline.c',
        'rte_eth_softnic_swq.c',
        'rte_eth_softnic_tap.c',
        'rte_eth_softnic_thread.c',
        'rte_eth_softnic_tm.c',
)
deps += ['pipeline', 'port', 'table', 'sched', 'cryptodev']
