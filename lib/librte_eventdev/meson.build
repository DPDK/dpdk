# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 3
allow_experimental_apis = true
sources = files('rte_eventdev.c',
		'rte_event_ring.c',
		'rte_event_eth_rx_adapter.c',
		'rte_event_timer_adapter.c')
headers = files('rte_eventdev.h',
		'rte_eventdev_pmd.h',
		'rte_eventdev_pmd_pci.h',
		'rte_eventdev_pmd_vdev.h',
		'rte_event_ring.h',
		'rte_event_eth_rx_adapter.h',
		'rte_event_timer_adapter.h',
		'rte_event_timer_adapter_pmd.h')
deps += ['ring', 'ethdev', 'hash']
