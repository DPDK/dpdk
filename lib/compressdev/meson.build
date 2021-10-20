# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_compressdev.c',
    'rte_compressdev_pmd.c',
    'rte_comp.c')
headers = files('rte_compressdev.h',
    'rte_compressdev_pmd.h',
    'rte_compressdev_internal.h',
    'rte_comp.h')
deps += ['kvargs', 'mbuf']
