# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'ark_ddm.c',
        'ark_ethdev.c',
        'ark_ethdev_rx.c',
        'ark_ethdev_tx.c',
        'ark_mpu.c',
        'ark_pktchkr.c',
        'ark_pktdir.c',
        'ark_pktgen.c',
        'ark_udm.c',
)
