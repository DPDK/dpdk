# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

drivers = ['af_packet',
	'ark',
	'atlantic',
	'avf',
	'avp',
	'axgbe', 'bonding',
	'bnx2x',
	'bnxt',
	'cxgbe',
	'dpaa', 'dpaa2',
	'e1000',
	'ena',
	'enetc',
	'enic',
	'failsafe',
	'fm10k', 'i40e',
	'ifc',
	'ixgbe',
	'kni',
	'liquidio',
	'mlx4',
	'mlx5',
	'mvneta',
	'mvpp2',
	'netvsc',
	'nfp',
	'null', 'octeontx', 'pcap', 'qede', 'ring',
	'sfc',
	'softnic',
	'szedata2',
	'tap',
	'thunderx',
	'vdev_netvsc',
	'vhost',
	'virtio',
	'vmxnet3',
]
std_deps = ['ethdev', 'kvargs'] # 'ethdev' also pulls in mbuf, net, eal etc
std_deps += ['bus_pci']         # very many PMDs depend on PCI, so make std
std_deps += ['bus_vdev']        # same with vdev bus
config_flag_fmt = 'RTE_LIBRTE_@0@_PMD'
driver_name_fmt = 'rte_pmd_@0@'
