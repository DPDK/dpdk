# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Mellanox Technologies, Ltd

if is_windows
	subdir_done()
endif

drivers = ['ifc',
	   'mlx5',]
std_deps = ['bus_pci', 'kvargs']
std_deps += ['vhost']
config_flag_fmt = 'RTE_LIBRTE_@0@_PMD'
