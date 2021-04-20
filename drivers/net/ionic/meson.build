# SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
# Copyright(c) 2019 Pensando

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'ionic_dev.c',
        'ionic_ethdev.c',
        'ionic_lif.c',
        'ionic_mac_api.c',
        'ionic_main.c',
        'ionic_rx_filter.c',
        'ionic_rxtx.c',
)
