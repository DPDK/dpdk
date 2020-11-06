# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on x86_64 Linux'
        subdir_done()
endif

sources = files('dlb.c',
		'dlb_iface.c',
		'dlb_xstats.c',
		'pf/dlb_main.c',
		'pf/dlb_pf.c',
		'pf/base/dlb_resource.c',
		'rte_pmd_dlb.c',
		'dlb_selftest.c'
)

headers = files('rte_pmd_dlb.h')

deps += ['mbuf', 'mempool', 'ring', 'pci', 'bus_pci']
