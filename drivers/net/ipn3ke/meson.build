# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

#
# Add the experimenatal APIs called from this PMD
#  rte_eth_switch_domain_alloc()
#  rte_eth_dev_create()
#  rte_eth_dev_destroy()
#  rte_eth_switch_domain_free()
#
allow_experimental_apis = true

sources += files('ipn3ke_ethdev.c',
	'ipn3ke_representor.c',
	'ipn3ke_tm.c',
	'ipn3ke_flow.c')
deps += ['bus_ifpga', 'sched']
