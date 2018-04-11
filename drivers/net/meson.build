# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

drivers = ['af_packet', 'axgbe', 'bonding', 'dpaa', 'dpaa2',
	'e1000', 'enic', 'fm10k', 'i40e', 'ixgbe',
	'mvpp2', 'null', 'octeontx', 'pcap', 'ring',
	'sfc', 'thunderx', 'virtio']
std_deps = ['ethdev', 'kvargs'] # 'ethdev' also pulls in mbuf, net, eal etc
std_deps += ['bus_pci']         # very many PMDs depend on PCI, so make std
std_deps += ['bus_vdev']        # same with vdev bus
config_flag_fmt = 'RTE_LIBRTE_@0@_PMD'
driver_name_fmt = 'rte_pmd_@0@'
