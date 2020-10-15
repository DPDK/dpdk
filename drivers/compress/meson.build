# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
	subdir_done()
endif

drivers = ['isal', 'octeontx', 'zlib']

std_deps = ['compressdev'] # compressdev pulls in all other needed deps
config_flag_fmt = 'RTE_LIBRTE_PMD_@0@'
