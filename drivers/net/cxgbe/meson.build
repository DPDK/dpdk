# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'cxgbe_ethdev.c',
        'cxgbe_main.c',
        'cxgbevf_ethdev.c',
        'cxgbevf_main.c',
        'sge.c',
        'cxgbe_filter.c',
        'cxgbe_flow.c',
        'clip_tbl.c',
        'mps_tcam.c',
        'l2t.c',
        'smt.c',
        'base/t4_hw.c',
        'base/t4vf_hw.c',
)
includes += include_directories('base')
