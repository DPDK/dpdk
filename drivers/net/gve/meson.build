# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2022 Intel Corporation

if not is_linux
    build = false
    reason = 'only supported on Linux'
    subdir_done()
endif

sources = files(
        'base/gve_adminq.c',
        'gve_rx.c',
        'gve_tx.c',
        'gve_rx_dqo.c',
        'gve_tx_dqo.c',
        'gve_ethdev.c',
        'gve_version.c',
)
includes += include_directories('base')
