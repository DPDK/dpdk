# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_sched.c', 'rte_red.c', 'rte_approx.c', 'rte_pie.c')
headers = files(
        'rte_approx.h',
        'rte_red.h',
        'rte_sched.h',
        'rte_sched_common.h',
        'rte_pie.h',
)
deps += ['mbuf', 'meter']
