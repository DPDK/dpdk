# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Yunsilicon Technology Co., Ltd.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64bit Linux'
endif

sources = files(
        'xsc_ethdev.c',
        'xsc_dev.c',
        'xsc_vfio_mbox.c',
        'xsc_vfio.c',
        'xsc_np.c',
        'xsc_rx.c',
        'xsc_tx.c',
)

cflags += no_wvla_cflag
