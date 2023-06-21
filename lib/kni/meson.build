# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

if dpdk_conf.get('RTE_IOVA_IN_MBUF') == 0
    build = false
    reason = 'requires IOVA in mbuf (set enable_iova_as_pa option)'
endif

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
endif
sources = files('rte_kni.c')
headers = files('rte_kni.h', 'rte_kni_common.h')
deps += ['ethdev', 'pci']
