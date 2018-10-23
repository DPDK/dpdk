# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

version = 1
allow_experimental_apis = true
headers = files('rte_ip.h',
	'rte_tcp.h',
	'rte_udp.h',
	'rte_esp.h',
	'rte_sctp.h',
	'rte_icmp.h',
	'rte_arp.h',
	'rte_ether.h',
	'rte_gre.h',
	'rte_net.h',
	'rte_net_crc.h',
	'rte_mpls.h')

sources = files('rte_arp.c', 'rte_net.c', 'rte_net_crc.c')
deps += ['mbuf']
