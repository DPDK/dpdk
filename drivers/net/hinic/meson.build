# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Huawei Technologies Co., Ltd

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

sources = files(
        'hinic_pmd_ethdev.c',
        'hinic_pmd_rx.c',
        'hinic_pmd_tx.c',
        'hinic_pmd_flow.c',
)

includes += include_directories('base')

if is_freebsd
    annotate_locks = false
endif
