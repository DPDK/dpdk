# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2022 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'base/gve_adminq.c',
        'gve_rx.c',
        'gve_tx.c',
        'gve_ethdev.c',
)
includes += include_directories('base')
