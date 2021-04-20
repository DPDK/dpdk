# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'base/lio_23xx_vf.c',
        'base/lio_mbox.c',
        'lio_ethdev.c',
        'lio_rxtx.c',
)
includes += include_directories('base')
