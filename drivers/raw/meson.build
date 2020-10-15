# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 NXP

if is_windows
	subdir_done()
endif

drivers = ['dpaa2_cmdif', 'dpaa2_qdma',
	'ifpga', 'ioat', 'ntb',
	'octeontx2_dma',
	'octeontx2_ep',
	'skeleton']
std_deps = ['rawdev']
config_flag_fmt = 'RTE_LIBRTE_PMD_@0@_RAWDEV'
