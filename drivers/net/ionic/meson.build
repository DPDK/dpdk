# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022 Advanced Micro Devices, Inc.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'ionic_dev.c',
        'ionic_dev_pci.c',
        'ionic_ethdev.c',
        'ionic_lif.c',
        'ionic_mac_api.c',
        'ionic_main.c',
        'ionic_rx_filter.c',
        'ionic_rxtx.c',
        'ionic_rxtx_simple.c',
        'ionic_rxtx_sg.c',
)
