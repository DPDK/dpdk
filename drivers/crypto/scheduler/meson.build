# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

deps += ['bus_vdev', 'reorder', 'security']
sources = files(
        'rte_cryptodev_scheduler.c',
        'scheduler_failover.c',
        'scheduler_multicore.c',
        'scheduler_pkt_size_distr.c',
        'scheduler_pmd.c',
        'scheduler_pmd_ops.c',
        'scheduler_roundrobin.c',
)

headers = files(
        'rte_cryptodev_scheduler.h',
        'rte_cryptodev_scheduler_operations.h',
)
