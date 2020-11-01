# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

if not is_linux or not dpdk_conf.has('RTE_ARCH_X86_64')
        build = false
        reason = 'only supported on ARCH_X86_64 Linux'
        subdir_done()
endif

sources = files('dlb2.c',
		'dlb2_iface.c',
		'pf/dlb2_main.c',
		'pf/dlb2_pf.c'
)

headers = files()

deps += ['mbuf', 'mempool', 'ring', 'pci', 'bus_pci']
