# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

name = 'bond' #, james bond :-)
sources = files('rte_eth_bond_api.c', 'rte_eth_bond_pmd.c', 'rte_eth_bond_flow.c',
	'rte_eth_bond_args.c', 'rte_eth_bond_8023ad.c', 'rte_eth_bond_alb.c')

deps += 'sched' # needed for rte_bitmap.h
deps += ['ip_frag']

headers = files('rte_eth_bond.h', 'rte_eth_bond_8023ad.h')
