# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'ena_ethdev.c',
        'ena_rss.c',
        'base/ena_com.c',
        'base/ena_eth_com.c',
)

deps += ['timer']

includes += include_directories('base', 'base/ena_defs')
