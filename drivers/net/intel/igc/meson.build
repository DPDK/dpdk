# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019-2020 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

subdir('base')
objs = [base_objs]

sources = files(
        'igc_ethdev.c',
        'igc_logs.c',
        'igc_filter.c',
        'igc_flow.c',
        'igc_txrx.c',
)

includes += include_directories('base')
