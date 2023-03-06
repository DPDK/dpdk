# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

name = 'bond' #, james bond :-)
sources = files(
        'rte_eth_bond_8023ad.c',
        'rte_eth_bond_alb.c',
        'rte_eth_bond_api.c',
        'rte_eth_bond_args.c',
        'rte_eth_bond_flow.c',
        'rte_eth_bond_pmd.c',
)
testpmd_sources = files('bonding_testpmd.c')

deps += 'sched' # needed for rte_bitmap.h
deps += ['ip_frag']

headers = files('rte_eth_bond.h', 'rte_eth_bond_8023ad.h')

require_iova_in_mbuf = false
