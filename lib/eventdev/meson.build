# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'eventdev_private.c',
        'eventdev_trace_points.c',
        'rte_event_crypto_adapter.c',
        'rte_event_dma_adapter.c',
        'rte_event_eth_rx_adapter.c',
        'rte_event_eth_tx_adapter.c',
        'rte_event_ring.c',
        'rte_event_timer_adapter.c',
        'rte_eventdev.c',
)
headers = files(
        'rte_event_crypto_adapter.h',
        'rte_event_dma_adapter.h',
        'rte_event_eth_rx_adapter.h',
        'rte_event_eth_tx_adapter.h',
        'rte_event_ring.h',
        'rte_event_timer_adapter.h',
        'rte_eventdev.h',
        'rte_eventdev_trace_fp.h',
)
indirect_headers += files(
        'rte_eventdev_core.h',
)
driver_sdk_headers += files(
        'eventdev_pmd.h',
        'eventdev_pmd_pci.h',
        'eventdev_pmd_vdev.h',
        'event_timer_adapter_pmd.h',
)

deps += ['ring', 'ethdev', 'hash', 'mempool', 'mbuf', 'timer', 'cryptodev', 'dmadev']
deps += ['telemetry']
