# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2021 Marvell.
#

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

sources = files(
        'cn9k_eventdev.c',
        'cn9k_worker.c',
        'cn10k_eventdev.c',
        'cn10k_worker.c',
        'cnxk_eventdev.c',
        'cnxk_eventdev_adptr.c',
        'cnxk_eventdev_selftest.c',
        'cnxk_eventdev_stats.c',
        'cnxk_tim_evdev.c',
        'cnxk_tim_worker.c',
)

deps += ['bus_pci', 'common_cnxk']
