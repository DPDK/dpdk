# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

name = 'ethdev'
version = 9
allow_experimental_apis = true
sources = files('ethdev_profile.c',
	'rte_ethdev.c',
	'rte_flow.c',
	'rte_mtr.c',
	'rte_tm.c')

headers = files('rte_ethdev.h',
	'rte_ethdev_driver.h',
	'rte_ethdev_core.h',
	'rte_ethdev_pci.h',
	'rte_ethdev_vdev.h',
	'rte_eth_ctrl.h',
	'rte_dev_info.h',
	'rte_flow.h',
	'rte_flow_driver.h',
	'rte_mtr.h',
	'rte_mtr_driver.h',
	'rte_tm.h',
	'rte_tm_driver.h')

deps += ['net']
