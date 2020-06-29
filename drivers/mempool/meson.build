# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
	subdir_done()
endif

drivers = ['bucket', 'dpaa', 'dpaa2', 'octeontx', 'octeontx2', 'ring', 'stack']
std_deps = ['mempool']
config_flag_fmt = 'RTE_LIBRTE_@0@_MEMPOOL'
driver_name_fmt = 'rte_mempool_@0@'
