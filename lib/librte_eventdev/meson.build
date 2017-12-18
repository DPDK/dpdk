# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

allow_experimental_apis = true
sources = files('rte_eventdev.c',
		'rte_event_ring.c',
		'rte_event_eth_rx_adapter.c')
headers = files('rte_eventdev.h',
		'rte_eventdev_pmd.h',
		'rte_eventdev_pmd_pci.h',
		'rte_eventdev_pmd_vdev.h',
		'rte_event_ring.h',
		'rte_event_eth_rx_adapter.h')
deps += ['ring', 'ethdev', 'hash']
