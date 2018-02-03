# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

drivers = ['af_packet', 'bonding',
	'e1000', 'fm10k', 'i40e', 'ixgbe',
	'null', 'octeontx', 'pcap', 'ring',
	'sfc', 'thunderx']
std_deps = ['ethdev', 'kvargs'] # 'ethdev' also pulls in mbuf, net, eal etc
std_deps += ['bus_pci']         # very many PMDs depend on PCI, so make std
std_deps += ['bus_vdev']        # same with vdev bus
config_flag_fmt = 'RTE_LIBRTE_@0@_PMD'
driver_name_fmt = 'rte_pmd_@0@'
