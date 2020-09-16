# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

if is_windows
	build = false
	reason = 'not supported on Windows'
endif

sources = files('octeontx_fpavf.c',
		'rte_mempool_octeontx.c'
)

deps += ['mbuf', 'bus_pci', 'common_octeontx']
